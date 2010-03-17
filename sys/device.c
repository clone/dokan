/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2008 Hiroki Asakawa info@dokan-dev.net

  http://dokan-dev.net/en

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free
Software Foundation; either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program. If not, see <http://www.gnu.org/licenses/>.
*/


#include "dokan.h"

#include <mountdev.h>
#include <mountmgr.h>

NTSTATUS
DokanDispatchDeviceControl(
	__in PDEVICE_OBJECT DeviceObject,
	__in PIRP Irp
	)

/*++

Routine Description:

	This device control dispatcher handles IOCTLs.

Arguments:

	DeviceObject - Context for the activity.
	Irp 		 - The device control argument block.

Return Value:

	NTSTATUS

--*/

{
	PDokanVCB			vcb;
	PDokanDCB			dcb;
	PIO_STACK_LOCATION	irpSp;
	NTSTATUS			status = STATUS_NOT_IMPLEMENTED;
	ULONG				controlCode;
	// {DCA0E0A5-D2CA-4f0f-8416-A6414657A77A}
	GUID dokanGUID = 
		{ 0xdca0e0a5, 0xd2ca, 0x4f0f, { 0x84, 0x16, 0xa6, 0x41, 0x46, 0x57, 0xa7, 0x7a } };


	__try {
		FsRtlEnterFileSystem();

		Irp->IoStatus.Information = 0;

		irpSp = IoGetCurrentIrpStackLocation(Irp);

		controlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
	
		if (controlCode != IOCTL_EVENT_WAIT &&
			controlCode != IOCTL_EVENT_INFO &&
			controlCode != IOCTL_KEEPALIVE) {
		
			DDbgPrint("==> DokanDispatchIoControl\n");
			DDbgPrint("  ProcessId %lu\n", IoGetRequestorProcessId(Irp));
		}

		vcb = DeviceObject->DeviceExtension;
		if (GetIdentifierType(vcb) == DGL) {
			switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
			case IOCTL_EVENT_START:
				DDbgPrint("  IOCTL_EVENT_START\n");
				status = DokanEventStart(DeviceObject, Irp);
				break;
			case IOCTL_SERVICE_WAIT:
				status = DokanRegisterPendingIrpForService(DeviceObject, Irp);
				break;
			case IOCTL_TEST:
				if (irpSp->Parameters.DeviceIoControl.OutputBufferLength >= sizeof(ULONG)) {
					*(ULONG*)Irp->AssociatedIrp.SystemBuffer = DOKAN_VERSION;
					Irp->IoStatus.Information = sizeof(ULONG);
					status = STATUS_SUCCESS;
					break;
				}
			default:
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			__leave;
		}


		if (GetIdentifierType(vcb) != VCB) {
			status = STATUS_INVALID_PARAMETER;
			__leave;
		}
		dcb = vcb->Dcb;

		switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
		//case IOCTL_QUERY_DEVICE_NAME:
		//	DDbgPrint("  IOCTL_QUERY_DEVICE_NAME\n");
			//status = STATUS_SUCCESS;
		//	break;
				
		case IOCTL_EVENT_WAIT:
			//DDbgPrint("  IOCTL_EVENT_WAIT\n");
			status = DokanRegisterPendingIrpForEvent(DeviceObject, Irp);
			break;

		case IOCTL_EVENT_INFO:
			//DDbgPrint("  IOCTL_EVENT_INFO\n");
			status = DokanCompleteIrp(DeviceObject, Irp);
			break;

		case IOCTL_EVENT_RELEASE:
			DDbgPrint("  IOCTL_EVENT_RELEASE\n");
			status = DokanEventRelease(DeviceObject);
			break;

		case IOCTL_EVENT_WRITE:
			DDbgPrint("  IOCTL_EVENT_WRITE\n");
			status = DokanEventWrite(DeviceObject, Irp);
			break;

		case IOCTL_KEEPALIVE:
			if (dcb->Mounted) {
				KeEnterCriticalRegion();
				ExAcquireResourceExclusiveLite(&dcb->Resource, TRUE);
				DokanUpdateTimeout(&dcb->TickCount, DOKAN_KEEPALIVE_TIMEOUT);
				ExReleaseResourceLite(&dcb->Resource);
				KeLeaveCriticalRegion();
				status = STATUS_SUCCESS;
			} else {
				DDbgPrint(" device is not mounted\n");
				status = STATUS_INSUFFICIENT_RESOURCES;
			}
			break;

		case IOCTL_RESET_TIMEOUT:
			status = DokanResetPendingIrpTimeout(DeviceObject, Irp);
			break;

		case IOCTL_DISK_CHECK_VERIFY:
			DDbgPrint("  IOCTL_DISK_CHECK_VERIFY\n");
			break;

		case IOCTL_STORAGE_CHECK_VERIFY:
			DDbgPrint("  IOCTL_STORAGE_CHECK_VERIFY\n");
			break;

		case IOCTL_STORAGE_CHECK_VERIFY2:
			DDbgPrint("  IOCTL_STORAGE_CHECK_VERIFY2\n");
			break;

		case IOCTL_DISK_GET_DRIVE_GEOMETRY:
			{
				PDISK_GEOMETRY	diskGeometry;
				ULONG		    length;

				DDbgPrint("  IOCTL_DISK_GET_DRIVE_GEOMETRY\n");
				if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
					sizeof(DISK_GEOMETRY)) {
					status = STATUS_BUFFER_TOO_SMALL;
					Irp->IoStatus.Information = 0;
					break;
				}

				diskGeometry = (PDISK_GEOMETRY)Irp->AssociatedIrp.SystemBuffer;
				ASSERT(diskGeometry != NULL);

				length = 1024*1024*1024;
				diskGeometry->Cylinders.QuadPart = length / DOKAN_SECTOR_SIZE / 32 / 2;
				diskGeometry->MediaType = FixedMedia;
				diskGeometry->TracksPerCylinder = 2;
				diskGeometry->SectorsPerTrack = 32;
				diskGeometry->BytesPerSector = DOKAN_SECTOR_SIZE;

				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = sizeof(DISK_GEOMETRY);
			}
			break;

		case IOCTL_DISK_GET_LENGTH_INFO:
			{
				PGET_LENGTH_INFORMATION getLengthInfo;

				DDbgPrint("  IOCTL_DISK_GET_LENGTH_INFO\n");
	            
				if (irpSp->Parameters.DeviceIoControl.OutputBufferLength <
					sizeof(GET_LENGTH_INFORMATION)) {
					status = STATUS_BUFFER_TOO_SMALL;
					Irp->IoStatus.Information = 0;
					break;
				}

				getLengthInfo = (PGET_LENGTH_INFORMATION) Irp->AssociatedIrp.SystemBuffer;
				ASSERT(getLengthInfo != NULL);

				getLengthInfo->Length.QuadPart = 1024*1024*500;
				status = STATUS_SUCCESS;
				Irp->IoStatus.Information = sizeof(GET_LENGTH_INFORMATION);
			}
			break;

		case IOCTL_DISK_GET_PARTITION_INFO:
			DDbgPrint("  IOCTL_DISK_GET_PARTITION_INFO\n");
			break;

		case IOCTL_DISK_GET_PARTITION_INFO_EX:
			DDbgPrint("  IOCTL_DISK_GET_PARTITION_INFO_EX\n");
			break;

		case IOCTL_DISK_IS_WRITABLE:
			DDbgPrint("  IOCTL_DISK_IS_WRITABLE\n");
			status = STATUS_SUCCESS;
			break;

		case IOCTL_DISK_MEDIA_REMOVAL:
			DDbgPrint("  IOCTL_DISK_MEDIA_REMOVAL\n");
			status = STATUS_SUCCESS;
			break;

		case IOCTL_STORAGE_MEDIA_REMOVAL:
			DDbgPrint("  IOCTL_STORAGE_MEDIA_REMOVAL\n");
			status = STATUS_SUCCESS;
			break;

		case IOCTL_DISK_SET_PARTITION_INFO:
			DDbgPrint("  IOCTL_DISK_SET_PARTITION_INFO\n");
			break;

		case IOCTL_DISK_VERIFY:
			DDbgPrint("  IOCTL_DISK_VERIFY\n");
			break;

		case IOCTL_MOUNTDEV_QUERY_DEVICE_NAME:
			{
				PMOUNTDEV_NAME	mountdevName;
				WCHAR			deviceName[MAXIMUM_FILENAME_LENGTH];
				ULONG			bufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
				WCHAR			deviceName1[] = UNIQUE_VOLUME_NAME;
				
				DDbgPrint("   IOCTL_MOUNTDEV_QUERY_DEVICE_NAME\n");

				if (bufferLength < sizeof(MOUNTDEV_NAME)) {
					status = STATUS_BUFFER_TOO_SMALL;
					Irp->IoStatus.Information = 0;
					break;
				}

				if (!dcb->Mounted) {
					status = STATUS_INVALID_PARAMETER;
					break;
				}

				mountdevName = (PMOUNTDEV_NAME)Irp->AssociatedIrp.SystemBuffer;
				ASSERT(mountdevName != NULL);
				/* NOTE: When Windows API GetVolumeNameForVolumeMountPoint is called, this IO control is called.
				   Even if status = STATUS_SUCCESS, GetVolumeNameForVolumeMountPoint returns error.
				   Something is wrong..
				*/
				swprintf(deviceName, NTDEVICE_NAME_STRING L"%u", dcb->MountId);
				mountdevName->NameLength = (wcslen(deviceName) + 1) * sizeof(WCHAR); // includes null char

				if (sizeof(USHORT) + mountdevName->NameLength < bufferLength) {
					RtlCopyMemory((PCHAR)mountdevName->Name, deviceName, mountdevName->NameLength);
					Irp->IoStatus.Information = FIELD_OFFSET(MOUNTDEV_NAME, Name[0]) + mountdevName->NameLength;
					status = STATUS_SUCCESS;
					DDbgPrint("  NameLength %d\n", mountdevName->NameLength);
					DDbgPrint("  info %d\n", Irp->IoStatus.Information);
					DDbgPrint("  DeviceName %ws\n", mountdevName->Name);
				} else {
					Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
					status = STATUS_BUFFER_OVERFLOW;
				}

				/* NOTE: Creates symbolic link UNIQUE_VOLUME_NAME (this should be unique name per volume,
				   now the same name is used for experiment) and return symbolic name here.
				mountdevName->NameLength = sizeof(deviceName1);
				if (FIELD_OFFSET(MOUNTDEV_NAME, Name[0]) + sizeof(deviceName1) < bufferLength) {
					RtlCopyMemory((PCHAR)mountdevName->Name, deviceName1, sizeof(deviceName1));
					Irp->IoStatus.Information = FIELD_OFFSET(MOUNTDEV_NAME, Name[0]) + mountdevName->NameLength;
					DDbgPrint("  NameLength %d\n", mountdevName->NameLength);
					DDbgPrint("  info %d\n", Irp->IoStatus.Information);
					DDbgPrint("  DeviceName %ws\n", mountdevName->Name);
					status = STATUS_SUCCESS;

				} else {
					Irp->IoStatus.Information = sizeof(MOUNTDEV_NAME);
					status = STATUS_BUFFER_OVERFLOW;
				}
				*/
			}
			break;
		case IOCTL_MOUNTDEV_QUERY_UNIQUE_ID:
			{
				// This IO control code is not called?
				PMOUNTDEV_UNIQUE_ID uniqueId;
				WCHAR				uniqueName[] = L"\\??\\Volume{dca0e0a5-d2ca-4f0f-8416-a6414657a77a}\\";
				ULONG				bufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

				DDbgPrint("   IOCTL_MOUNTDEV_QUERY_UNIQUE_ID\n");
				if (bufferLength < sizeof(MOUNTDEV_UNIQUE_ID)) {
					status = STATUS_BUFFER_TOO_SMALL;
					Irp->IoStatus.Information = 0;
					break;
				}

				uniqueId = (PMOUNTDEV_UNIQUE_ID)Irp->AssociatedIrp.SystemBuffer;
				ASSERT(uniqueId != NULL);
				uniqueId->UniqueIdLength = sizeof(uniqueName);
				if (FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId[0]) + sizeof(uniqueName) < bufferLength) {
					RtlCopyMemory((PCHAR)uniqueId->UniqueId, uniqueName, sizeof(uniqueName));
					Irp->IoStatus.Information = FIELD_OFFSET(MOUNTDEV_UNIQUE_ID, UniqueId[0]) + sizeof(uniqueName);
					status = STATUS_SUCCESS;
					break;
				} else {
					Irp->IoStatus.Information = sizeof(MOUNTDEV_UNIQUE_ID);
					status = STATUS_BUFFER_OVERFLOW;
				}
			}
		case IOCTL_STORAGE_EJECT_MEDIA:
			{
				DDbgPrint("   IOCTL_STORAGE_EJECT_MEDIA\n");
				DokanUnmount(dcb);				
				status = STATUS_SUCCESS;
				break;
			}
		default:
			DDbgPrint("   Unknown Code 0x%x\n", irpSp->Parameters.DeviceIoControl.IoControlCode);
			status = STATUS_NOT_IMPLEMENTED;
			break;
		} // switch IoControlCode
	
	} __finally {

		if (status != STATUS_PENDING) {
			//
			// complete the Irp
			//
			Irp->IoStatus.Status = status;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
		}

		if (controlCode != IOCTL_EVENT_WAIT &&
			controlCode != IOCTL_EVENT_INFO &&
			controlCode != IOCTL_KEEPALIVE) {
		
			DokanPrintNTStatus(status);
			DDbgPrint("<== DokanDispatchIoControl\n");
		}

		FsRtlExitFileSystem();
	}

	return status;
}
