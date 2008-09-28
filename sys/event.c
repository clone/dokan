/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2008 Hiroki Asakawa asakaw@gmail.com

  http://dokan-dev.net/en

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "dokan.h"


/*
 Control Flow

 loop {
	=> EventIRP 
	    When IRP for Event from user-mode dokan library comes,
		 put it in EventQueue                   # DokanRegisterPendingIrpForEvent
	<= STATUS_PENDING (EventIRP)

	=> QueryDirectoryIRP                        # from individual dispatch routine
		put it in IrpQueue                      # DokanRegisterPendingIrp
		by use one of EventQueue
		 inform it to user-mode that QueryDirectory is requested
		                                        # DokanEventNotification
		<= Complete EventIRP
	<= STATUS_PENDING (QueryDirectoryIRP)

	=> EventIRP with QueryDirectoryInfo         # DokanCompleteIrp
		When user-mode file system returns EventInfo for QueryDirectory
		  search corresponding IRP from IrpQueue
		  copy QueryDirectoryInfo to that IRP
		<= Complete QueryDirectoryIRP
	<= STATUS_SUCCESS (EventIRP with QueryDirectoryInfo)
 }
    

  * Caution *
	use lock while Queue operations
	register CancelRoutine
	needs to regiter CleanupRoutine

*/



VOID
DokanIrpCancelRoutine(
    __in PDEVICE_OBJECT   DeviceObject,
    __in PIRP             Irp
    )
{
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
    KIRQL               oldIrql;
    PIRP_ENTRY			irpEntry;
	ULONG				serialNumber;
	PIO_STACK_LOCATION	irpSp;

    DDbgPrint("==> DokanIrpCancelRoutine\n");

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

    // Release the cancel spinlock
    IoReleaseCancelSpinLock(Irp->CancelIrql);

    irpEntry = Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY];
    
	if (irpEntry != NULL) {
		PKSPIN_LOCK	lock = &irpEntry->IrpList->ListLock;

		// Acquire the queue spinlock
		ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
		KeAcquireSpinLock(lock, &oldIrql);

		irpSp = IoGetCurrentIrpStackLocation(Irp);
		ASSERT(irpSp != NULL);

		serialNumber = irpEntry->SerialNumber;

		RemoveEntryList(&irpEntry->ListEntry);

		// If Write is canceld before completion and buffer that saves writing
		// content is not freed, free it here
		if (irpSp->MajorFunction == IRP_MJ_WRITE) {
			PVOID eventContext = Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_EVENT];
			if (eventContext != NULL) {
				ExFreePool(eventContext);
			}
			Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_EVENT] = NULL;
		}


		if (IsListEmpty(&irpEntry->IrpList->ListHead)) {
			//DDbgPrint("    list is empty ClearEvent\n");
			KeClearEvent(&irpEntry->IrpList->NotEmpty);
		}

		irpEntry->PendingIrp = NULL;

		if (irpEntry->CancelRoutineFreeMemory == FALSE) {
			InitializeListHead(&irpEntry->ListEntry);
		} else {
			ExFreePool(irpEntry);
			irpEntry = NULL;
		}

		Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL; 

		KeReleaseSpinLock(lock, oldIrql);
	}

	DDbgPrint("   canceled IRP #%X\n", serialNumber);
    Irp->IoStatus.Status = STATUS_CANCELLED;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

	DDbgPrint("<== DokanIrpCancelRoutine\n");
    return;

}


NTSTATUS
DokanRegisterPendingIrpMain(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP			Irp,
	__in ULONG			SerialNumber,
	__in PIRP_LIST		IrpList,
	__in ULONG			CheckMount
    )
{
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
 	PIRP_ENTRY			irpEntry;
    PIO_STACK_LOCATION	irpSp;
    KIRQL				oldIrql;
 
	DDbgPrint("==> DokanRegisterPendingIrp\n");

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	//ExAcquireResourceSharedLite(&deviceExtension->Resource, TRUE);
	if (CheckMount && !deviceExtension->Mounted) {
		DDbgPrint(" device is not mounted\n");
		//ExReleaseResourceLite(&deviceExtension->Resource);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//ExReleaseResourceLite(&deviceExtension->Resource);

    irpSp = IoGetCurrentIrpStackLocation(Irp);
 
    // Allocate a record and save all the event context.
    irpEntry = ExAllocatePool(sizeof(IRP_ENTRY));

    if (NULL == irpEntry) {
        return  STATUS_INSUFFICIENT_RESOURCES;
    }

	RtlZeroMemory(irpEntry, sizeof(IRP_ENTRY));

    InitializeListHead(&irpEntry->ListEntry);

	irpEntry->SerialNumber		= SerialNumber;
    irpEntry->FileObject		= irpSp->FileObject;
    irpEntry->DeviceExtension	= deviceExtension;
    irpEntry->PendingIrp		= Irp;
	irpEntry->IrpSp				= irpSp;
	irpEntry->IrpList			= IrpList;

	KeQueryTickCount(&irpEntry->TickCount);

	//DDbgPrint("  Lock IrpList.ListLock\n");
	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
    KeAcquireSpinLock(&IrpList->ListLock, &oldIrql);

    IoSetCancelRoutine(Irp, DokanIrpCancelRoutine);

    if (Irp->Cancel) {
        if (IoSetCancelRoutine(Irp, NULL) != NULL) {
			//DDbgPrint("  Release IrpList.ListLock %d\n", __LINE__);
            KeReleaseSpinLock(&IrpList->ListLock, oldIrql);

            ExFreePool(irpEntry);

            return STATUS_CANCELLED;
        }
	}

    IoMarkIrpPending(Irp);

    InsertTailList(&IrpList->ListHead, &irpEntry->ListEntry);

    irpEntry->CancelRoutineFreeMemory = FALSE;

	// save the pointer in order to be accessed by cancel routine
	Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] =  irpEntry;


	KeSetEvent(&IrpList->NotEmpty, IO_NO_INCREMENT, FALSE);

	//DDbgPrint("  Release IrpList.ListLock\n");
    KeReleaseSpinLock(&IrpList->ListLock, oldIrql);

	DDbgPrint("<== DokanRegisterPendingIrp\n");
    return STATUS_PENDING;;

}


NTSTATUS
DokanRegisterPendingIrp(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP			Irp,
	__in ULONG			SerialNumber
    )
{
	PDEVICE_EXTENSION deviceExtension = DokanGetDeviceExtension(DeviceObject);

	return DokanRegisterPendingIrpMain(
		DeviceObject,
		Irp,
		SerialNumber,
		&deviceExtension->IrpList,
		TRUE);
}



NTSTATUS
DokanRegisterPendingIrpForEvent(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP			Irp
    )
{
	PDEVICE_EXTENSION deviceExtension = DokanGetDeviceExtension(DeviceObject);

	DDbgPrint("DokanRegisterPendingIrpForEvent\n");

	return DokanRegisterPendingIrpMain(
		DeviceObject,
		Irp,
		0, // SerialNumber
		&deviceExtension->EventList,
		TRUE);
}



NTSTATUS
DokanRegisterPendingIrpForService(
	__in PDEVICE_OBJECT	DeviceObject,
	__in PIRP			Irp
	)
{
	PDEVICE_EXTENSION deviceExtension = DokanGetDeviceExtension(DeviceObject);

	DDbgPrint("DokanRegisterPendingIrpForService\n");

	return DokanRegisterPendingIrpMain(
		DeviceObject,
		Irp,
		0, // SerialNumber
		&deviceExtension->Global->ServiceList,
		FALSE);
}



NTSTATUS
DokanEventNotificationMain(
	__in PDEVICE_EXTENSION	DeviceExtension,
	__in PEVENT_CONTEXT		EventContext,
	__in PIRP_LIST			IrpList,
	__in ULONG				Timeout // in seconds
	)

/*++


--*/
{
	PIRP_ENTRY			eventEntry;
	PLIST_ENTRY			listEntry;
	PIRP				irp;
    KIRQL				oldIrql;
	PIO_STACK_LOCATION	irpSp;
	LARGE_INTEGER		timeout;
	NTSTATUS			status = STATUS_INSUFFICIENT_RESOURCES;
	NTSTATUS			eventStatus;
	ULONG				info	 = 0;
	ULONG				eventLen = 0;
	ULONG				bufferLen= 0;
	PVOID				buffer	 = NULL;


	//DDbgPrint("==> DokanEventNotification\n");

	ASSERT(DeviceExtension);

	//ExAcquireResourceSharedLite(&DeviceExtension->Resource, TRUE);
	if (!DeviceExtension->Mounted) {
		DDbgPrint("  device is not mounted\n");
		//ExReleaseResourceLite(&DeviceExtension->Resource);
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	//ExReleaseResourceLite(&DeviceExtension->Resource);

	RtlZeroMemory(&timeout, sizeof(LARGE_INTEGER));

	// in 100 nano seconds units, -10000 * milliseconds
	timeout.QuadPart = -10000 * 1000 * DOKAN_NOTIFICATION_TIMEOUT;

	while (1) {
		// wait until Event IRP come
		ASSERT(KeGetCurrentIrql() < DISPATCH_LEVEL);
		//DDbgPrint("   [#%X] wait for Event\n", EventContext->SerialNumber);

		//ExAcquireResourceSharedLite(&DeviceExtension->Resource, TRUE);
		if (!DeviceExtension->Mounted) {
			//ExReleaseResourceLite(&DeviceExtension->Resource);
			return STATUS_INSUFFICIENT_RESOURCES;
		}
		//ExReleaseResourceLite(&DeviceExtension->Resource);

		eventStatus = KeWaitForSingleObject(&IrpList->NotEmpty, Executive, KernelMode,
			FALSE, &timeout);

		// timeout
		if (!NT_SUCCESS(eventStatus) || eventStatus == STATUS_TIMEOUT) {
			DDbgPrint("  wait notification event timeout\n");
			return STATUS_INSUFFICIENT_RESOURCES;
		}

		//DDbgPrint("   [#%X] get Event\n", EventContext->SerialNumber);

		//DDbgPrint("      Lock EventList.ListLock\n");
		ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
		KeAcquireSpinLock(&IrpList->ListLock, &oldIrql);

		if (IsListEmpty(&IrpList->ListHead)) {
			//DDbgPrint("      Release EventList.ListLock\n");
			//DDbgPrint("      ### EventQueue is Emtpy ###\n");
			//DDbgPrint("      ClearEvent\n");
			KeClearEvent(&IrpList->NotEmpty);
			KeReleaseSpinLock(&IrpList->ListLock, oldIrql);
		} else {
			break;
		}
		DDbgPrint("   try again\n");
	}

	// EventQueue must not be emtpy
	ASSERT(!IsListEmpty(&IrpList->ListHead));
	listEntry = RemoveHeadList(&IrpList->ListHead);

	if (IsListEmpty(&IrpList->ListHead)) {
		//DDbgPrint("    list is empty ClearEvent\n");
		KeClearEvent(&IrpList->NotEmpty);
	}

	// eventEntry's memory must be freed in this function
	eventEntry = CONTAINING_RECORD(listEntry, IRP_ENTRY, ListEntry);

	irp = eventEntry->PendingIrp;
	
	if (irp == NULL) {
		// this IRP is already canceled
		// TODO: should do something
		ASSERT(eventEntry->CancelRoutineFreeMemory == FALSE);
		ExFreePool(eventEntry);
		eventEntry = NULL;

		KeReleaseSpinLock(&IrpList->ListLock, oldIrql);
		return status;
	}

	if (IoSetCancelRoutine(irp, NULL) == NULL) {
		// Cancel routine will run as soon as we release the lock
		InitializeListHead(&eventEntry->ListEntry);
		eventEntry->CancelRoutineFreeMemory = TRUE;
		KeReleaseSpinLock(&IrpList->ListLock, oldIrql);
		return status;
	}

	// this IRP is not canceled yet

	if(EventContext != NULL)
		eventLen = EventContext->Length;

	//irpSp = IoGetCurrentIrpStackLocation(irp);
	irpSp = eventEntry->IrpSp;
			
	// available size that is used for event notification
	bufferLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
			
	// buffer that is used to inform Event
	buffer	= irp->AssociatedIrp.SystemBuffer;

	irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;

	//DDbgPrint("  !!EventNotification!!\n");
			
	// buffer is not specified or short of length
	if (bufferLen == 0 || buffer == NULL || bufferLen < eventLen) {
			
		DDbgPrint("EventNotice : STATUS_INSUFFICIENT_RESOURCES\n");
		DDbgPrint("  bufferLen: %d, eventLen: %d\n", bufferLen, eventLen);
		info   = 0;
		status = STATUS_INSUFFICIENT_RESOURCES;
			
	} else {
			
		// let's copy EVENT_CONTEXT
		ASSERT(buffer != NULL);
		ASSERT(bufferLen != 0);
	
		//DDbgPrint("  buffer %X\n", buffer);
		//DDbgPrint("  bufferLen %d\n", bufferLen);
		//DDbgPrint("  eventLen %d\n", eventLen);
		//DDbgPrint("  copy EventContext\n");
		RtlCopyMemory(buffer, EventContext, eventLen);
		status = STATUS_SUCCESS;
		info = eventLen;
	}

	KeReleaseSpinLock(&IrpList->ListLock, oldIrql);
	irp->IoStatus.Status = status;
	irp->IoStatus.Information = info;
	IoCompleteRequest(irp, IO_NO_INCREMENT);

	ExFreePool(eventEntry);
	eventEntry = NULL;
	
	//DDbgPrint("<== DokanEventNotification\n");

    return status;
}



NTSTATUS
DokanEventNotification(
	__in PDEVICE_EXTENSION	DeviceExtension,
	__in PEVENT_CONTEXT		EventContext
	)
{
	return DokanEventNotificationMain(
		DeviceExtension,
		EventContext,
		&DeviceExtension->EventList,
		DOKAN_NOTIFICATION_TIMEOUT);
}



NTSTATUS
DokanUnmountNotification(
	__in PDEVICE_EXTENSION	DeviceExtension,
	__in PEVENT_CONTEXT		EventContext
	)
{
	return DokanEventNotificationMain(
		DeviceExtension,
		EventContext,
		&DeviceExtension->Global->ServiceList,
		DOKAN_UNMOUNT_NOTIFICATION_TIMEOUT); // timeout in seconds
}



// remove IRP that is enqueued at RegisterPendingIrp
NTSTATUS
DokanDequeueIrp(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp)
{
	KIRQL				oldIrql;
    PDEVICE_EXTENSION   deviceExtension;
	PIRP_ENTRY			irpEntry;

	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&deviceExtension->IrpList.ListLock, &oldIrql);

	irpEntry = (PIRP_ENTRY)Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY];
	Irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;

	RemoveEntryList(&irpEntry->ListEntry);

	if (IoSetCancelRoutine(Irp, NULL) != NULL) {
		ExFreePool(irpEntry);

	} else {
		// this IRP is already canceled?
		// this IRP should not be completed?
		InitializeListHead(&irpEntry->ListEntry);
		irpEntry->CancelRoutineFreeMemory = TRUE;
	}

	KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

	return STATUS_SUCCESS;
}



// When user-mode file system application returns EventInformation,
// search corresponding pending IRP and complete it
NTSTATUS
DokanCompleteIrp(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
	)
{
	KIRQL				oldIrql;
    PLIST_ENTRY			thisEntry, nextEntry, listHead;
	PIRP_ENTRY			irpEntry;
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
	PEVENT_INFORMATION	eventInfo;

	eventInfo		= (PEVENT_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	ASSERT(eventInfo != NULL);
	
	//DDbgPrint("==> DokanCompleteIrp [EventInfo #%X]\n", eventInfo->SerialNumber);

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	//DDbgPrint("      Lock IrpList.ListLock\n");
	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&deviceExtension->IrpList.ListLock, &oldIrql);

	// search corresponding IRP through pending IRP list
	listHead = &deviceExtension->IrpList.ListHead;

    for (thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry) {

		PIRP				irp;
		PIO_STACK_LOCATION	irpSp;

        nextEntry = thisEntry->Flink;

        irpEntry = CONTAINING_RECORD(thisEntry, IRP_ENTRY, ListEntry);

		// check whether this is corresponding IRP

        //DDbgPrint("SerialNumber irpEntry %X eventInfo %X\n", irpEntry->SerialNumber, eventInfo->SerialNumber);

		// this irpEntry must be freed in this if statement
		if (irpEntry->SerialNumber != eventInfo->SerialNumber)  {
			continue;
		}
			
		RemoveEntryList(thisEntry);

		irp = irpEntry->PendingIrp;
	
		if (irp == NULL) {
			// this IRP is already canceled
			ASSERT(irpEntry->CancelRoutineFreeMemory == FALSE);
			ExFreePool(irpEntry);
			irpEntry = NULL;
			break;
		}

		if (IoSetCancelRoutine(irp, NULL) == NULL) {
			// Cancel routine will run as soon as we release the lock
			InitializeListHead(&irpEntry->ListEntry);
			irpEntry->CancelRoutineFreeMemory = TRUE;
			break;
		}

		// IRP is not canceled yet
		irpSp = irpEntry->IrpSp;	
		
		ASSERT(irpSp != NULL);
					
		// IrpEntry is saved here for CancelRoutine
		// Clear it to prevent to be completed by CancelRoutine twice
		irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;
		KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

		switch (irpSp->MajorFunction) {
		case IRP_MJ_DIRECTORY_CONTROL:
			DokanCompleteDirectoryControl(irpEntry, eventInfo);
			break;
		case IRP_MJ_READ:
			DokanCompleteRead(irpEntry, eventInfo);
			break;
		case IRP_MJ_WRITE:
			DokanCompleteWrite(irpEntry, eventInfo);
			break;
		case IRP_MJ_QUERY_INFORMATION:
			DokanCompleteQueryInformation(irpEntry, eventInfo);
			break;
		case IRP_MJ_QUERY_VOLUME_INFORMATION:
			DokanCompleteQueryVolumeInformation(irpEntry, eventInfo);
			break;
		case IRP_MJ_CREATE:
			DokanCompleteCreate(irpEntry, eventInfo);
			break;
		case IRP_MJ_CLEANUP:
			DokanCompleteCleanup(irpEntry, eventInfo);
			break;
		case IRP_MJ_CLOSE:
			DokanCompleteClose(irpEntry, eventInfo);
			break;
		case IRP_MJ_LOCK_CONTROL:
			DokanCompleteLock(irpEntry, eventInfo);
			break;
		case IRP_MJ_SET_INFORMATION:
			DokanCompleteSetInformation(irpEntry, eventInfo);
			break;
		case IRP_MJ_FLUSH_BUFFERS:
			DokanCompleteFlush(irpEntry, eventInfo);
			break;
		default:
			DDbgPrint("Unknown IRP %d\n", irpSp->MajorFunction);
			// TODO: in this case, should complete this IRP
			break;
		}		

		ExFreePool(irpEntry);
		irpEntry = NULL;

		return STATUS_SUCCESS;
	}

	KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

    //DDbgPrint("<== AACompleteIrp [EventInfo #%X]\n", eventInfo->SerialNumber);

	// TODO: should return error
    return STATUS_SUCCESS;
}


// release all pending IRP(for Event notificaiton)
NTSTATUS
DokanReleaseEventIrp(
    __in PDEVICE_OBJECT DeviceObject)
{
	KIRQL				oldIrql;
    PLIST_ENTRY			thisEntry, nextEntry, listHead;
	PIRP_ENTRY			eventEntry;
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
	PEVENT_INFORMATION	eventInfo;


	DDbgPrint("==> DokanReleaseEventIRP\n");


	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&deviceExtension->EventList.ListLock, &oldIrql);

	// if EventQueue is empty, nothing to do
	if (IsListEmpty(&deviceExtension->EventList.ListHead)) {
		KeReleaseSpinLock(&deviceExtension->EventList.ListLock, oldIrql);
		return STATUS_SUCCESS;
	}

	// release all IRP through pending IRP list
	listHead = &deviceExtension->EventList.ListHead;

    for (thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry) {

		PIRP	irp;
        nextEntry = thisEntry->Flink;

        eventEntry = CONTAINING_RECORD(thisEntry, IRP_ENTRY, ListEntry);

		RemoveEntryList(thisEntry);

		irp = eventEntry->PendingIrp;
	
		if (irp != NULL) {

			// this IRP isn't canceled yet
			if (IoSetCancelRoutine(irp, NULL) != NULL) {
				
				PIO_STACK_LOCATION	irpSp;
				ULONG				bufferLen;
				PVOID				buffer;
				ULONG				info = 0;
				NTSTATUS			status = STATUS_SUCCESS;

				irpSp = eventEntry->IrpSp;	
			
				ASSERT(irpSp != NULL);
					
				// IrpEntry is saved here for CancelRoutine
				// Clear it to prevent to be completed by CancelRoutine twice
				irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;
		
				// available size that is used for event notification
				bufferLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
			
				// buffer that is used to inform event
				buffer	= irp->AssociatedIrp.SystemBuffer;

			
				// buffer is not specified or short of length
				if (bufferLen == 0 || buffer == NULL || bufferLen < sizeof(EVENT_CONTEXT)) {
			
					DDbgPrint("EventNotice : STATUS_INSUFFICIENT_RESOURCES\n");
					DDbgPrint("  bufferLen: %d, eventLen: %d\n", bufferLen, sizeof(EVENT_CONTEXT));
					info   = 0;
					status = STATUS_INSUFFICIENT_RESOURCES;
			
				} else {
			
					// set EVNET_CONTEXT
					PEVENT_CONTEXT eventContext = (PEVENT_CONTEXT)buffer;

					RtlZeroMemory(buffer, sizeof(EVENT_CONTEXT));

					eventContext->SerialNumber	= 0;
					eventContext->MajorFunction = IRP_MJ_SHUTDOWN;
					eventContext->MinorFunction = 0;
					eventContext->Flags			= 0;
					info = sizeof(EVENT_CONTEXT);
				}

				//
				// must release all SpinLock before complete IRP
				// 
				KeReleaseSpinLock(&deviceExtension->EventList.ListLock, oldIrql);

				irp->IoStatus.Status = status;
				irp->IoStatus.Information = info;
				IoCompleteRequest(irp, IO_NO_INCREMENT);

				KeAcquireSpinLock(&deviceExtension->EventList.ListLock, &oldIrql);
				
				ExFreePool(eventEntry);
				eventEntry = NULL;

			} else {
				InitializeListHead(&eventEntry->ListEntry);
				eventEntry->CancelRoutineFreeMemory = TRUE;
			}
		} else {
			ASSERT(eventEntry->CancelRoutineFreeMemory == FALSE);
			ExFreePool(eventEntry);
			eventEntry = NULL;
		}
	}
	
	KeReleaseSpinLock(&deviceExtension->EventList.ListLock, oldIrql);

	DDbgPrint("<== DokanReleaseEventIRP\n");

	return STATUS_SUCCESS;
}


// release all pending IRP
NTSTATUS
DokanReleasePendingIrp(
    __in PDEVICE_OBJECT DeviceObject)
{
	KIRQL				oldIrql;
    PLIST_ENTRY			thisEntry, nextEntry, listHead;
	PIRP_ENTRY			irpEntry;
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;

	DDbgPrint("==> DokanReleasePendingIRP\n");

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&deviceExtension->IrpList.ListLock, &oldIrql);

	// if EventQueue is empty, nothing to do
	if (IsListEmpty(&deviceExtension->IrpList.ListHead)) {
		KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);
		return STATUS_SUCCESS;
	}

	
	// release all IRP through pending IRP list
	listHead = &deviceExtension->IrpList.ListHead;

    for (thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry) {

		PIRP	irp;
        nextEntry = thisEntry->Flink;

        irpEntry = CONTAINING_RECORD(thisEntry, IRP_ENTRY, ListEntry);

		RemoveEntryList(thisEntry);

		irp = irpEntry->PendingIrp;
	
		if (irp != NULL) {

			// this IRP isn't canceled yet
			if (IoSetCancelRoutine(irp, NULL) != NULL) {
			
				// IrpEntry is saved here for CancelRoutine
				// Clear it to prevent to be completed by CancelRoutine twice
				irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;
		
				//
				// must release all SpinLock before complete IRP
				// 
				KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				irp->IoStatus.Information = 0;
				IoCompleteRequest(irp, IO_NO_INCREMENT);

				KeAcquireSpinLock(&deviceExtension->IrpList.ListLock, &oldIrql);
				
				ExFreePool(irpEntry);
				irpEntry = NULL;

			} else {
				InitializeListHead(&irpEntry->ListEntry);
				irpEntry->CancelRoutineFreeMemory = TRUE;
			}
		} else {
			ASSERT(irpEntry->CancelRoutineFreeMemory == FALSE);
			ExFreePool(irpEntry);
			irpEntry = NULL;
		}
	}
	
	KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

	DDbgPrint("<== DokanReleasePendingIRP\n");

	return STATUS_SUCCESS;
}


 
// start event dispatching
NTSTATUS
DokanEventStart(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
   )
{
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
	ULONG				bufferLen;
	ULONG				inBufferLen;
	PVOID				buffer;
	PIO_STACK_LOCATION	irpSp;
	PEVENT_START		eventStart;
	WCHAR				driveLetter;

	DDbgPrint("==> DokanEventStart\n");

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	irpSp		= IoGetCurrentIrpStackLocation(Irp);

	bufferLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;		
	inBufferLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
	eventStart = (PEVENT_START)Irp->AssociatedIrp.SystemBuffer;

	if (bufferLen < sizeof(EVENT_START))
		return STATUS_INSUFFICIENT_RESOURCES;
	if (inBufferLen < sizeof(WCHAR))
		return STATUS_INSUFFICIENT_RESOURCES;

	
	driveLetter = *(WCHAR*)Irp->AssociatedIrp.SystemBuffer;

	eventStart->Version = DOKAN_VERSION;
	eventStart->DeviceNumber = deviceExtension->Number;

	KeEnterCriticalRegion();
	ExAcquireResourceExclusiveLite(&deviceExtension->Resource, TRUE);

	if (deviceExtension->Mounted) {
		DDbgPrint("  DOKAN_USED\n");
		eventStart->Status = DOKAN_USED;
	} else {
		DDbgPrint("  DOKAN_MOUNTED\n");
		eventStart->Status = DOKAN_MOUNTED;
		deviceExtension->Mounted = driveLetter;
		KeQueryTickCount(&deviceExtension->TickCount);
		InterlockedIncrement(&deviceExtension->MountId);

		deviceExtension->UseAltStream = 0;
		deviceExtension->UseKeepAlive = 0;
		DokanStartCheckThread(deviceExtension);
	}

	ExReleaseResourceLite(&deviceExtension->Resource);
	KeLeaveCriticalRegion();

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = sizeof(EVENT_START);

	DDbgPrint("<== DokanEventStart\n");

	return Irp->IoStatus.Status;
}




// user assinged bigger buffer that is enough to return WriteEventContext
NTSTATUS
DokanEventWrite(
    __in PDEVICE_OBJECT DeviceObject,
    __in PIRP Irp
	)
{
	KIRQL				oldIrql;
    PLIST_ENTRY			thisEntry, nextEntry, listHead;
	PIRP_ENTRY			irpEntry;
	PDokanVCB			vcb;
    PDEVICE_EXTENSION   deviceExtension;
	PEVENT_INFORMATION	eventInfo;

	eventInfo		= (PEVENT_INFORMATION)Irp->AssociatedIrp.SystemBuffer;
	ASSERT(eventInfo != NULL);
	
	DDbgPrint("==> DokanEventWrite [EventInfo #%X]\n", eventInfo->SerialNumber);

	vcb = DokanGetVcb(DeviceObject);
	deviceExtension = DokanGetDeviceExtension(DeviceObject);

	//DDbgPrint("      Lock IrpList.ListLock\n");
	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&deviceExtension->IrpList.ListLock, &oldIrql);

	// search corresponding write IRP through pending IRP list
	listHead = &deviceExtension->IrpList.ListHead;

    for (thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry) {

		nextEntry = thisEntry->Flink;

        irpEntry = CONTAINING_RECORD(thisEntry, IRP_ENTRY, ListEntry);

		// check whehter this is corresponding IRP

        //DDbgPrint("SerialNumber irpEntry %X eventInfo %X\n", irpEntry->SerialNumber, eventInfo->SerialNumber);

		// do NOT free irpEntry here
		if (irpEntry->SerialNumber == eventInfo->SerialNumber)  {
			
			PIRP	writeIrp = irpEntry->PendingIrp;
	
			if (writeIrp != NULL) {

				// write IRP is not canceled yet
				// TODO: is it readly true?
				if (IoSetCancelRoutine(writeIrp, DokanIrpCancelRoutine) != NULL) {
				//if (IoSetCancelRoutine(writeIrp, NULL) != NULL) {

					PIO_STACK_LOCATION writeIrpSp, eventIrpSp;
					PEVENT_CONTEXT	eventContext;
					ULONG			info = 0;
					NTSTATUS		status;

					writeIrpSp = irpEntry->IrpSp;
					eventIrpSp = IoGetCurrentIrpStackLocation(Irp);
			
					ASSERT(writeIrpSp != NULL);
					ASSERT(eventIrpSp != NULL);

					eventContext = (PEVENT_CONTEXT)writeIrp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_EVENT];
					ASSERT(eventContext != NULL);
				
					// short of buffer length
					if (eventIrpSp->Parameters.DeviceIoControl.OutputBufferLength
							< eventContext->Length) {
						
						DDbgPrint("  EventWrite: STATUS_INSUFFICIENT_RESOURCE\n");
						status =  STATUS_INSUFFICIENT_RESOURCES;
					} else {
						PVOID buffer;

						DDbgPrint("  EventWrite CopyMemory\n");
						DDbgPrint("  EventLength %d, BufLength %d\n", eventContext->Length,
							eventIrpSp->Parameters.DeviceIoControl.OutputBufferLength);

						if (Irp->MdlAddress)
							buffer = MmGetSystemAddressForMdlSafe(Irp->MdlAddress, NormalPagePriority);
						else
							buffer = Irp->AssociatedIrp.SystemBuffer;
					
						ASSERT(buffer != NULL);
						RtlCopyMemory(buffer, eventContext, eventContext->Length);
						
						info = eventContext->Length;
						status = STATUS_SUCCESS;
					
					}
					ExFreePool(eventContext);
					writeIrp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_EVENT] = 0;

					KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

					Irp->IoStatus.Status = status;
					Irp->IoStatus.Information = info;

					// this IRP will be completed by caller function
					return Irp->IoStatus.Status;


				} else {
					InitializeListHead(&irpEntry->ListEntry);
					irpEntry->CancelRoutineFreeMemory = TRUE;
				}
			} else {
				ASSERT(irpEntry->CancelRoutineFreeMemory == FALSE);
				ExFreePool(irpEntry);
				irpEntry = NULL;
			}
			break;
		}
	}

	//DDbgPrint("      Release IrpList.ListLock\n");
	KeReleaseSpinLock(&deviceExtension->IrpList.ListLock, oldIrql);

    //DDbgPrint("<== AACompleteIrp [EventInfo #%X]\n", eventInfo->SerialNumber);

    return STATUS_SUCCESS;
}




NTSTATUS
DokanReleaseTimeoutPendingIrp(
   PDEVICE_EXTENSION	DeviceExtension
   )
{
	KIRQL				oldIrql;
    PLIST_ENTRY			thisEntry, nextEntry, listHead;
	PIRP_ENTRY			irpEntry;
	LARGE_INTEGER		tickCount;

	DDbgPrint("==> DokanReleaseTimeoutPendingIRP\n");

	ASSERT(KeGetCurrentIrql() <= DISPATCH_LEVEL);
	KeAcquireSpinLock(&DeviceExtension->IrpList.ListLock, &oldIrql);

	// when IRP queue is empty, there is nothing to do
	if (IsListEmpty(&DeviceExtension->IrpList.ListHead)) {
		KeReleaseSpinLock(&DeviceExtension->IrpList.ListLock, oldIrql);
		DDbgPrint("  IrpQueue is Empty\n");
		return STATUS_SUCCESS;
	}

	
	KeQueryTickCount(&tickCount);

	// search timeout IRP through pending IRP list
	listHead = &DeviceExtension->IrpList.ListHead;

    for (thisEntry = listHead->Flink;
		thisEntry != listHead;
		thisEntry = nextEntry) {

		PIRP	irp;
        nextEntry = thisEntry->Flink;

        irpEntry = CONTAINING_RECORD(thisEntry, IRP_ENTRY, ListEntry);

		// this IRP is NOT timeout yet
		if ( (tickCount.QuadPart - irpEntry->TickCount.QuadPart) * KeQueryTimeIncrement()
			< DOKAN_IPR_PENDING_TIMEOUT * 10000 * 1000) {
			continue;
		}
		
		RemoveEntryList(thisEntry);

		DDbgPrint(" timeout Irp #%X\n", irpEntry->SerialNumber);


		irp = irpEntry->PendingIrp;
	
		if (irp != NULL) {

			// this IRP is not canceled yet
			if (IoSetCancelRoutine(irp, NULL) != NULL) {

				// IrpEntry is saved here for CancelRoutine
				// Clear it to prevent to be completed by CancelRoutine twice
				irp->Tail.Overlay.DriverContext[DRIVER_CONTEXT_IRP_ENTRY] = NULL;
		
				//
				// must release all SpinLock before complete IRP
				// 
				KeReleaseSpinLock(&DeviceExtension->IrpList.ListLock, oldIrql);

				irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
				irp->IoStatus.Information = 0;
				IoCompleteRequest(irp, IO_NO_INCREMENT);

				KeAcquireSpinLock(&DeviceExtension->IrpList.ListLock, &oldIrql);
				
				ExFreePool(irpEntry);
				irpEntry = NULL;

			} else {
				InitializeListHead(&irpEntry->ListEntry);
				irpEntry->CancelRoutineFreeMemory = TRUE;
			}
		} else {
			ASSERT(irpEntry->CancelRoutineFreeMemory == FALSE);
			ExFreePool(irpEntry);
			irpEntry = NULL;
		}
	}
	
	KeReleaseSpinLock(&DeviceExtension->IrpList.ListLock, oldIrql);

	DDbgPrint("<== DokanReleaseTimeoutPendingIRP\n");

	return STATUS_SUCCESS;
}

