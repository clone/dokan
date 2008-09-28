/*
  Dokan : user-mode file system library for Windows

  Copyright (C) 2008 Hiroki Asakawa asakaw@gmail.com

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

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "dokani.h"
#include "fileinfo.h"


int
DokanSetAllocationInformation(
	 PEVENT_CONTEXT		EventContext,
	 PDOKAN_FILE_INFO	FileInfo,
	 PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_ALLOCATION_INFORMATION allocInfo =
		(PFILE_ALLOCATION_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);

	// A file's allocation size and end-of-file position are independent of each other,
	// with the following exception: The end-of-file position must always be less than
	// or equal to the allocation size. If the allocation size is set to a value that
	// is less than the end-of-file position, the end-of-file position is automatically
	// adjusted to match the allocation size.

	// How can we check the current end-of-file position?
	if (allocInfo->AllocationSize.QuadPart == 0) {
		return DokanOperations->SetEndOfFile(
			EventContext->SetFile.FileName,
			allocInfo->AllocationSize.QuadPart,
			FileInfo);
	} else {
		DbgPrint("  SetAllocationInformation %I64d, can't handle this parameter.\n",
				allocInfo->AllocationSize.QuadPart);
	}

	return 0;
}


int
DokanSetBasicInformation(
	 PEVENT_CONTEXT		EventContext,
	 PDOKAN_FILE_INFO	FileInfo,
	 PDOKAN_OPERATIONS	DokanOperations)
{
	FILETIME creation, lastAccess, lastWrite;
	int status = -1;

	PFILE_BASIC_INFORMATION basicInfo =
		(PFILE_BASIC_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);

	if (!DokanOperations->SetFileAttributes)
		return -1;
	
	if (!DokanOperations->SetFileTime)
		return -1;

	status = DokanOperations->SetFileAttributes(
		EventContext->SetFile.FileName,
		basicInfo->FileAttributes,
		FileInfo);

	if (status < 0)
		return status;

	creation.dwLowDateTime = basicInfo->CreationTime.LowPart;
	creation.dwHighDateTime = basicInfo->CreationTime.HighPart;
	lastAccess.dwLowDateTime = basicInfo->LastAccessTime.LowPart;
	lastAccess.dwHighDateTime = basicInfo->LastAccessTime.HighPart;
	lastWrite.dwLowDateTime = basicInfo->LastWriteTime.LowPart;
	lastWrite.dwHighDateTime = basicInfo->LastWriteTime.HighPart;


	return DokanOperations->SetFileTime(
		EventContext->SetFile.FileName,
		&creation,
		&lastAccess,
		&lastWrite,
		FileInfo);
}


int
DokanSetDispositionInformation(
	 PEVENT_CONTEXT		EventContext,
	 PDOKAN_FILE_INFO	FileInfo,
	 PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_DISPOSITION_INFORMATION dispositionInfo =
		(PFILE_DISPOSITION_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);


	if (!DokanOperations->DeleteFile)
		return -1;

	// when dipoInfo->DeleteFile is true, delete file when closed
	// 1. mark CCB with delete
	//    when file is closed, call DokanOpe->DeleteFile and DokanOpe->Close
	// 2. add DokanOpe->DeleteFile parameter and programer takes care of deletion

	// WinAPI DeleteFile can't delete file after closing all existing HANDLE
	// scan NextCCB of FCB and search opening files
	// and if other program don't open it, delete it?

	//if (!dispositionInfo->DeleteFile) {

	if (FileInfo->IsDirectory) {
		if (!DokanOperations->DeleteDirectory)
			return -1;
		else
			return DokanOperations->DeleteDirectory(
					EventContext->SetFile.FileName,
					FileInfo);
	} else {
		return DokanOperations->DeleteFile(
			EventContext->SetFile.FileName,
			FileInfo);
	}
	//}

	//return 0;
}


int
DokanSetEndOfFileInformation(
	 PEVENT_CONTEXT		EventContext,
	 PDOKAN_FILE_INFO	FileInfo,
	 PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_END_OF_FILE_INFORMATION endInfo =
		(PFILE_END_OF_FILE_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);

	if (!DokanOperations->SetEndOfFile)
		return -1;

	return DokanOperations->SetEndOfFile(
		EventContext->SetFile.FileName,
		endInfo->EndOfFile.QuadPart,
		FileInfo);
}


int
DokanSetLinkInformation(
	PEVENT_CONTEXT		EventContext,
	PDOKAN_FILE_INFO	FileInfo,
	PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_LINK_INFORMATION linkInfo =
		(PFILE_LINK_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);
	return -1;
}



int
DokanSetRenameInformation(
PEVENT_CONTEXT		EventContext,
	 PDOKAN_FILE_INFO	FileInfo,
	 PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_RENAME_INFORMATION renameInfo =
		(PFILE_RENAME_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);
		//(PFILE_RENAME_INFORMATION)((PCHAR)&EventContext->SetFile + EventContext->SetFile.BufferOffset);

	WCHAR newName[MAX_PATH];
	ZeroMemory(newName, sizeof(newName));

	if (renameInfo->FileName[0] != L'\\') {
		ULONG pos;
		for (pos = EventContext->SetFile.FileNameLength/sizeof(WCHAR);
			pos != 0; --pos) {
			if (EventContext->SetFile.FileName[pos] == '\\')
				break;
		}
		RtlCopyMemory(newName, EventContext->SetFile.FileName, (pos+1)*sizeof(WCHAR));
		RtlCopyMemory((PCHAR)newName + (pos+1)*sizeof(WCHAR), renameInfo->FileName, renameInfo->FileNameLength);
	} else {
		RtlCopyMemory(newName, renameInfo->FileName, renameInfo->FileNameLength);
	}

	if (!DokanOperations->MoveFile)
		return -1;

	return DokanOperations->MoveFile(
		EventContext->SetFile.FileName,
		newName,
		renameInfo->ReplaceIfExists,
		FileInfo);
}


int
DokanSetValidDataLengthInformation(
	PEVENT_CONTEXT		EventContext,
	PDOKAN_FILE_INFO	FileInfo,
	PDOKAN_OPERATIONS	DokanOperations)
{
	PFILE_VALID_DATA_LENGTH_INFORMATION validInfo =
		(PFILE_VALID_DATA_LENGTH_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);

	if (!DokanOperations->SetEndOfFile)
		return -1;

	return DokanOperations->SetEndOfFile(
		EventContext->SetFile.FileName,
		validInfo->ValidDataLength.QuadPart,
		FileInfo);
}


VOID
DispatchSetInformation(
 	HANDLE				Handle,
	PEVENT_CONTEXT		EventContext,
	PDOKAN_OPERATIONS	DokanOperations)
{
	PEVENT_INFORMATION		eventInfo;
	PDOKAN_OPEN_INFO		openInfo;
	DOKAN_FILE_INFO			fileInfo;
	int						status;
	ULONG					sizeOfEventInfo = sizeof(EVENT_INFORMATION);


	if (EventContext->SetFile.FileInformationClass == FileRenameInformation) {
		PFILE_RENAME_INFORMATION renameInfo =
		(PFILE_RENAME_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);
		sizeOfEventInfo += renameInfo->FileNameLength;
	}

	CheckFileName(EventContext->SetFile.FileName);

	eventInfo = DispatchCommon(EventContext, sizeOfEventInfo, &fileInfo);
	
	openInfo = (PDOKAN_OPEN_INFO)EventContext->Context;

	DbgPrint("###SetFileInfo %04d\n", openInfo->EventId);

	switch (EventContext->SetFile.FileInformationClass) {
		case FileAllocationInformation:
			status = DokanSetAllocationInformation(EventContext, &fileInfo, DokanOperations);
			break;
		
		case FileBasicInformation:
			status = DokanSetBasicInformation(EventContext, &fileInfo, DokanOperations);
			break;
		
		case FileDispositionInformation:
			status = DokanSetDispositionInformation(EventContext, &fileInfo, DokanOperations);
			break;
		
		case FileEndOfFileInformation:
			status = DokanSetEndOfFileInformation(EventContext, &fileInfo, DokanOperations);
			break;
		
		case FileLinkInformation:
			status = DokanSetLinkInformation(EventContext, &fileInfo, DokanOperations);
			break;
		
		case FilePositionInformation:
			// this case is dealed with by driver
			status = -1;
			break;
		
		case FileRenameInformation:
			status = DokanSetRenameInformation(EventContext, &fileInfo, DokanOperations);
			break;

		case FileValidDataLengthInformation:
			status = DokanSetValidDataLengthInformation(EventContext, &fileInfo, DokanOperations);
			break;
	}

	openInfo->UserContext = fileInfo.Context;

	eventInfo->BufferLength = 0;

	if (status < 0) {
		int error = status * -1;

		eventInfo->Status = STATUS_NOT_IMPLEMENTED;

		switch (error) {
		case ERROR_DIR_NOT_EMPTY:
			eventInfo->Status = STATUS_DIRECTORY_NOT_EMPTY;
			break;
		}
	} else {
		eventInfo->Status = STATUS_SUCCESS;

		// notice new file name to driver
		if (EventContext->SetFile.FileInformationClass == FileRenameInformation) {
			PFILE_RENAME_INFORMATION renameInfo =
				(PFILE_RENAME_INFORMATION)((PCHAR)EventContext + EventContext->SetFile.BufferOffset);
			eventInfo->BufferLength = renameInfo->FileNameLength;
			CopyMemory(eventInfo->Buffer, renameInfo->FileName, renameInfo->FileNameLength);
		}
	}

	//DbgPrint("SetInfomation status = %d\n\n", status);

	SendEventInformation(Handle, eventInfo, sizeOfEventInfo);
	free(eventInfo);
	return;
}


