/*

Copyright (c) 2007, 2008 Hiroki Asakawa asakaw@gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "dokan.h"
#include "fileinfo.h"

static WCHAR RootDirectory[MAX_PATH] = L"C:";

static void
GetFilePath(
	PWCHAR	filePath,
	LPCWSTR FileName)
{
	RtlZeroMemory(filePath, MAX_PATH);
	wcsncpy(filePath,
			RootDirectory,
			wcslen(RootDirectory));
	wcsncat(filePath,
			FileName,
			wcslen(FileName));
}


#define MirrorCheckFlag(val, flag) if(val&flag) fprintf(stderr, "\t" #flag "\n")

static int
MirrorCreateFile(
	LPCWSTR					FileName,
	DWORD					AccessMode,
	DWORD					ShareMode,
	DWORD					CreationDisposition,
	DWORD					FlagsAndAttributes,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	HANDLE handle;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"CreateFile : %s\n", filePath);

	
	if (CreationDisposition == CREATE_NEW)
		fprintf(stderr, "\tCREATE_NEW\n");
	if (CreationDisposition == OPEN_ALWAYS)
		fprintf(stderr, "\tOPEN_ALWAYS\n");
	if (CreationDisposition == CREATE_ALWAYS)
		fprintf(stderr, "\tCREATE_ALWAYS\n");
	if (CreationDisposition == OPEN_EXISTING)
		fprintf(stderr, "\tOPEN_EXISTING\n");
	if (CreationDisposition == TRUNCATE_EXISTING)
		fprintf(stderr, "\tTRUNCATE_EXISTING\n");

	/*
	if (ShareMode == 0 && AccessMode & FILE_WRITE_DATA)
		ShareMode = FILE_SHARE_WRITE;
	else if (ShareMode == 0)
		ShareMode = FILE_SHARE_READ;
	*/

	fprintf(stderr, "\tShareMode = 0x%x\n", ShareMode);

	MirrorCheckFlag(ShareMode, FILE_SHARE_READ);
	MirrorCheckFlag(ShareMode, FILE_SHARE_WRITE);
	MirrorCheckFlag(ShareMode, FILE_SHARE_DELETE);

	fprintf(stderr, "\tAccessMode = 0x%x\n", AccessMode);

	MirrorCheckFlag(AccessMode, GENERIC_READ);
	MirrorCheckFlag(AccessMode, GENERIC_WRITE);
	MirrorCheckFlag(AccessMode, GENERIC_EXECUTE);
	
	MirrorCheckFlag(AccessMode, DELETE);
	MirrorCheckFlag(AccessMode, FILE_READ_DATA);
	MirrorCheckFlag(AccessMode, FILE_READ_ATTRIBUTES);
	MirrorCheckFlag(AccessMode, FILE_READ_EA);
	MirrorCheckFlag(AccessMode, READ_CONTROL);
	MirrorCheckFlag(AccessMode, FILE_WRITE_DATA);
	MirrorCheckFlag(AccessMode, FILE_WRITE_ATTRIBUTES);
	MirrorCheckFlag(AccessMode, FILE_WRITE_EA);
	MirrorCheckFlag(AccessMode, FILE_APPEND_DATA);
	MirrorCheckFlag(AccessMode, WRITE_DAC);
	MirrorCheckFlag(AccessMode, WRITE_OWNER);
	MirrorCheckFlag(AccessMode, SYNCHRONIZE);
	MirrorCheckFlag(AccessMode, FILE_EXECUTE);
	MirrorCheckFlag(AccessMode, STANDARD_RIGHTS_READ);
	MirrorCheckFlag(AccessMode, STANDARD_RIGHTS_WRITE);
	MirrorCheckFlag(AccessMode, STANDARD_RIGHTS_EXECUTE);


	// when filePath is a directory, flags is changed to the file be opened
	if (GetFileAttributes(filePath) & FILE_ATTRIBUTE_DIRECTORY) {
		FlagsAndAttributes |= FILE_FLAG_BACKUP_SEMANTICS;
		//AccessMode = 0;
	}

	fprintf(stderr, "\tFlagsAndAttributes = 0x%x\n", FlagsAndAttributes);

	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ARCHIVE);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_ENCRYPTED);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_HIDDEN);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NORMAL);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_OFFLINE);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_READONLY);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_SYSTEM);
	MirrorCheckFlag(FlagsAndAttributes, FILE_ATTRIBUTE_TEMPORARY);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_WRITE_THROUGH);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OVERLAPPED);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_NO_BUFFERING);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_RANDOM_ACCESS);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_SEQUENTIAL_SCAN);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_DELETE_ON_CLOSE);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_BACKUP_SEMANTICS);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_POSIX_SEMANTICS);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_REPARSE_POINT);
	MirrorCheckFlag(FlagsAndAttributes, FILE_FLAG_OPEN_NO_RECALL);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_ANONYMOUS);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_IDENTIFICATION);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_IMPERSONATION);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_DELEGATION);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_CONTEXT_TRACKING);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_EFFECTIVE_ONLY);
	MirrorCheckFlag(FlagsAndAttributes, SECURITY_SQOS_PRESENT);

	handle = CreateFile(
		filePath,
		AccessMode,//GENERIC_READ|GENERIC_WRITE|GENERIC_EXECUTE,
		ShareMode,
		NULL, // security attribute
		CreationDisposition,
		FlagsAndAttributes,// |FILE_FLAG_NO_BUFFERING,
		NULL); // template file handle

	if (handle == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1; // error codes are negated value of Windows System Error codes
	}

	fprintf(stderr, "\n");

	// save the file handle in Context
	DokanFileInfo->Context = (ULONG64)handle;
	return 0;
}


static int
MirrorCreateDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"CreateDirectory : %s\n", filePath);
	if (!CreateDirectory(filePath, NULL)) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1; // error codes are negated value of Windows System Error codes
	}
	return 0;
};


static int
MirrorOpenDirectory(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	HANDLE handle;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"OpenDirectory : %s\n", filePath);

	handle = CreateFile(
		filePath,
		0,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS,
		NULL);

	if (handle == INVALID_HANDLE_VALUE) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}

	fprintf(stderr, "\n");

	DokanFileInfo->Context = (ULONG64)handle;

	return 0;
}


static int
MirrorCloseFile(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, FileName);

	if (DokanFileInfo->Context) {
		fwprintf(stderr, L"CloseFile: %s\n", filePath);
		fprintf(stderr, "\terror : not cleanuped file\n\n");
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;
	} else {
		//fwprintf(stderr, L"Close: %s\n\tinvalid handle\n\n", filePath);
		fwprintf(stderr, L"Close: %s\n\n", filePath);
		return 0;
	}

	//fprintf(stderr, "\n");
	return 0;
}


static int
MirrorCleanup(
	LPCWSTR					FileName,
	PDOKAN_FILE_INFO		DokanFileInfo)
{
	WCHAR filePath[MAX_PATH];
	GetFilePath(filePath, FileName);

	if (DokanFileInfo->Context) {
		fwprintf(stderr, L"Cleanup: %s\n\n", filePath);
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;
	
	} else {
		fwprintf(stderr, L"Cleanup: %s\n\tinvalid handle\n\n", filePath);
		return -1;
	}

	return 0;
}


static int
MirrorReadFile(
	LPCWSTR				FileName,
	LPVOID				Buffer,
	DWORD				BufferLength,
	LPDWORD				ReadLength,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	ULONG	offset = (ULONG)Offset;
	BOOL	opened = FALSE;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"ReadFile : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle, cleanuped?\n");
		handle = CreateFile(
			filePath,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "\tCreateFile error : %d\n\n", GetLastError());
			return -1;
		}
		opened = TRUE;
	}
	
	if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
		fprintf(stderr, "\tseek error, offset = %d\n\n", offset);
		if (opened)
			CloseHandle(handle);
		return -1;
	}

		
	if (!ReadFile(handle, Buffer, BufferLength, ReadLength,NULL)) {
		fprintf(stderr, "\tread error = %u, buffer length = %d, read length = %d\n\n",
			GetLastError(), BufferLength, *ReadLength);
		if (opened)
			CloseHandle(handle);
		return -1;

	} else {
		fprintf(stderr, "\tread %d, offset %d\n\n", *ReadLength, offset);
	}

	if (opened)
		CloseHandle(handle);

	return 0;
}


static int
MirrorWriteFile(
	LPCWSTR		FileName,
	LPCVOID		Buffer,
	DWORD		NumberOfBytesToWrite,
	LPDWORD		NumberOfBytesWritten,
	LONGLONG			Offset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	ULONG	offset = (ULONG)Offset;
	BOOL	opened = FALSE;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"WriteFile : %s, offset %I64d, length %d\n", filePath, Offset, NumberOfBytesToWrite);
	//fprintf(stderr, "----\n%s\n----\n\n", Buffer);

	//fprintf(stderr, "press any key?");
	//getchar();


	// reopen the file
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle, cleanuped?\n");
		handle = CreateFile(
			filePath,
			GENERIC_WRITE,
			FILE_SHARE_WRITE,
			NULL,
			OPEN_EXISTING,
			0,
			NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			fprintf(stderr, "\tCreateFile error : %d\n\n", GetLastError());
			return -1;
		}
		opened = TRUE;
		
		return -1;
	}

	if (SetFilePointer(handle, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
		fprintf(stderr, "\tseek error, offset = %d, error = %d\n", offset, GetLastError());
		return -1;
	}

		
	if (!WriteFile(handle, Buffer, NumberOfBytesToWrite, NumberOfBytesWritten, NULL)) {
		fprintf(stderr, "\twrite error = %u, buffer length = %d, write length = %d\n",
			GetLastError(), NumberOfBytesToWrite, *NumberOfBytesWritten);
		return -1;

	} else {
		fprintf(stderr, "\twrite %d, offset %d\n\n", *NumberOfBytesWritten, offset);
	}

	// close the file when it is reopened
	if (opened)
		CloseHandle(handle);

	return 0;
}


static int
MirrorFlushFileBuffers(
	LPCWSTR		FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"FlushFileBuffers : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");
		return 0;
	}

	if (FlushFileBuffers(handle)) {
		return 0;
	} else {
		fprintf(stderr, "\tflush error code = %d\n", GetLastError());
		return -1;
	}

}


static int
MirrorGetFileInformation(
	LPCWSTR							FileName,
	LPBY_HANDLE_FILE_INFORMATION	HandleFileInformation,
	PDOKAN_FILE_INFO				DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;
	BOOL	opened = FALSE;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"GetFileInfo : %s\n", filePath);

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");

		// If CreateDirectory returned FILE_ALREADY_EXISTS and 
		// it is called with FILE_OPEN_IF, that handle must be opened.
		handle = CreateFile(filePath, 0, FILE_SHARE_READ, NULL, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (handle == INVALID_HANDLE_VALUE)
			return -1;
		opened = TRUE;
	}

	if (!GetFileInformationByHandle(handle,HandleFileInformation)) {
		fprintf(stderr, "\terror code = %d\n", GetLastError());

		// FileName is a root directory
		// in this case, FindFirstFile can't get directory information
		if (wcslen(FileName) == 1) {
			HandleFileInformation->dwFileAttributes = GetFileAttributes(filePath);

		} else {
			WIN32_FIND_DATAW find;
			ZeroMemory(&find, sizeof(WIN32_FIND_DATAW));
			handle = FindFirstFile(filePath, &find);
			if (handle == INVALID_HANDLE_VALUE) {
				fprintf(stderr, "\tFindFirstFile error code = %d\n\n", GetLastError());
				return -1;
			}
			HandleFileInformation->dwFileAttributes = find.dwFileAttributes;
			HandleFileInformation->ftCreationTime = find.ftCreationTime;
			HandleFileInformation->ftLastAccessTime = find.ftLastAccessTime;
			HandleFileInformation->ftLastWriteTime = find.ftLastWriteTime;
			HandleFileInformation->nFileSizeHigh = find.nFileSizeHigh;
			HandleFileInformation->nFileSizeLow = find.nFileSizeLow;
			fprintf(stderr, "\tFindFiles OK\n");
			CloseHandle(handle);
		}
	}

	fprintf(stderr, "\n");

	if (opened)
		CloseHandle(handle);

	return 0;
}


static int
MirrorFindFiles(
	LPCWSTR				FileName,
	PFillFindData		FillFindData, // function pointer
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR				filePath[MAX_PATH];
	HANDLE				hFind;
	WIN32_FIND_DATAW	findData;
	DWORD				error;
	PWCHAR				yenStar = L"\\*";
	int count = 0;

	GetFilePath(filePath, FileName);

	wcscat(filePath, yenStar);
	fwprintf(stderr, L"FindFiles :%s\n", filePath);

	hFind = FindFirstFile(filePath, &findData);

	if (hFind == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid file handle. Error is %u\n\n", GetLastError());
		return -1;
	}

	FillFindData(&findData, DokanFileInfo);
	count++;

	while (FindNextFile(hFind, &findData) != 0) {
 		FillFindData(&findData, DokanFileInfo);
		count++;
	}
	
	error = GetLastError();
	FindClose(hFind);

	if (error != ERROR_NO_MORE_FILES) {
		fprintf(stderr, "\tFindNextFile error. Error is %u\n\n", error);
		return -1;
	}

	fwprintf(stderr, L"\tFindFiles return %d entries in %s\n\n", count, filePath);

	return 0;
}


static int
MirrorDeleteFile(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	GetFilePath(filePath, FileName);

	// file handle must be closed before deleted
	if (handle)
		CloseHandle(handle);

	DokanFileInfo->Context = 0;

	fwprintf(stderr, L"DeleteFile %s\n", filePath);

	if (DeleteFile(filePath) == 0) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}
	
	fwprintf(stderr, L"\tsuccess\n\n");
	return 0;
}


static int
MirrorDeleteDirectory(
	LPCWSTR				FileName,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle = (HANDLE)DokanFileInfo->Context;

	GetFilePath(filePath, FileName);

	// file handle must be closed before deleted
	if (handle)
		CloseHandle(handle);

	DokanFileInfo->Context = 0;

	fwprintf(stderr, L"DeleteDirectory %s\n", filePath);

	if (!RemoveDirectory(filePath)) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}
	fwprintf(stderr, L"\tsuccess\n\n");
	return 0;
}


static int
MirrorMoveFile(
	LPCWSTR				FileName, // existing file name
	LPCWSTR				NewFileName,
	BOOL				ReplaceIfExisting,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR			filePath[MAX_PATH];
	WCHAR			newFilePath[MAX_PATH];
	BOOL			status;

	GetFilePath(filePath, FileName);
	GetFilePath(newFilePath, NewFileName);

	fwprintf(stderr, L"MoveFile %s -> %s\n\n", filePath, newFilePath);

	if (DokanFileInfo->Context) {
		// should close? or rename at closing?
		CloseHandle((HANDLE)DokanFileInfo->Context);
		DokanFileInfo->Context = 0;
	}

	if (ReplaceIfExisting)
		status = MoveFileEx(filePath, newFilePath, MOVEFILE_REPLACE_EXISTING);
	else
		status = MoveFile(filePath, newFilePath);

	if (status == 0)
		fprintf(stderr, "\tMoveFile failed status = %d, code = %d\n", status, GetLastError());

	return status == TRUE ? 0 : -1;
}


static int
MirrorLockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;
	LARGE_INTEGER offset;
	LARGE_INTEGER length;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"LockFile %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");
		return -1;
	}

	length.QuadPart = Length;
	offset.QuadPart = ByteOffset;

	if (LockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
		fprintf(stderr, "\tsuccess\n\n");
		return 0;
	} else {
		fprintf(stderr, "\tfail\n\n");
		return -1;
	}
}


static int
MirrorSetEndOfFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR			filePath[MAX_PATH];
	HANDLE			handle;
	LARGE_INTEGER	offset;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"SetEndOfFile %s, %I64d\n", filePath, ByteOffset);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");
		return -1;
	}

	offset.QuadPart = ByteOffset;
	if (!SetFilePointerEx(handle, offset, NULL, FILE_BEGIN)) {
		fprintf(stderr, "\tSetFilePointer error: %d, offset = %I64d\n\n", GetLastError(), ByteOffset);
		return GetLastError() * -1;
	}

	if (!SetEndOfFile(handle)) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}

	return 0;
}


static int
MirrorSetFileAttributes(
	LPCWSTR				FileName,
	DWORD				FileAttributes,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	
	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"SetFileAttributes %s\n", filePath);

	if (!SetFileAttributes(filePath, FileAttributes)) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}

	fprintf(stderr, "\n");
	return 0;
}


static int
MirrorSetFileTime(
	LPCWSTR				FileName,
	CONST FILETIME*		CreationTime,
	CONST FILETIME*		LastAccessTime,
	CONST FILETIME*		LastWriteTime,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"SetFileTime %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;

	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");
		return -1;
	}

	if (!SetFileTime(handle, CreationTime, LastAccessTime, LastWriteTime)) {
		DWORD error = GetLastError();
		fprintf(stderr, "\terror code = %d\n\n", error);
		return error * -1;
	}

	fprintf(stderr, "\n");
	return 0;
}



static int
MirrorUnlockFile(
	LPCWSTR				FileName,
	LONGLONG			ByteOffset,
	LONGLONG			Length,
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	WCHAR	filePath[MAX_PATH];
	HANDLE	handle;
	LARGE_INTEGER	length;
	LARGE_INTEGER	offset;

	GetFilePath(filePath, FileName);

	fwprintf(stderr, L"UnlockFile %s\n", filePath);

	handle = (HANDLE)DokanFileInfo->Context;
	if (!handle || handle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "\tinvalid handle\n\n");
		return -1;
	}

	length.QuadPart = Length;
	offset.QuadPart = ByteOffset;

	if (UnlockFile(handle, offset.HighPart, offset.LowPart, length.HighPart, length.LowPart)) {
		fprintf(stderr, "\tsuccess\n\n");
		return 0;
	} else {
		fprintf(stderr, "\tfail\n\n");
		return -1;
	}
}


static int
MirrorUnmount(
	PDOKAN_FILE_INFO	DokanFileInfo)
{
	fprintf(stderr, "Unmount\n");
	return 0;
}


static DOKAN_OPERATIONS
dokanOperations = {
	MirrorCreateFile,
	MirrorOpenDirectory,
	MirrorCreateDirectory,
	MirrorCleanup,
	MirrorCloseFile,
	MirrorReadFile,
	MirrorWriteFile,
	MirrorFlushFileBuffers,
	MirrorGetFileInformation,
	MirrorFindFiles,
	NULL, // FindFilesWithPattern
	MirrorSetFileAttributes,
	MirrorSetFileTime,
	MirrorDeleteFile,
	MirrorDeleteDirectory,
	MirrorMoveFile,
	MirrorSetEndOfFile,
	MirrorLockFile,
	MirrorUnlockFile,
	NULL, // GetDiskFreeSpace
	NULL, // GetVolumeInformation
	MirrorUnmount // Unmount
};



int __cdecl
main(ULONG argc, PCHAR argv[])
{
	int status;
	PDOKAN_OPTIONS dokanOptions = (PDOKAN_OPTIONS)malloc(sizeof(DOKAN_OPTIONS));

	if (argc < 3) {
		fprintf(stderr, "mirror.exe RootDirectory DriveLetter [ThreadCount]\n");
		return -1;
	}

	ZeroMemory(dokanOptions, sizeof(DOKAN_OPTIONS));

	mbstowcs(RootDirectory, argv[1], strlen(argv[1]));
	wprintf(L"RootDirectory: %ls\n", RootDirectory);

	dokanOptions->DriveLetter = argv[2][0];

	if (argc == 4)
		dokanOptions->ThreadCount = atoi(argv[3]);
	else
		dokanOptions->ThreadCount = 1;
	
	dokanOptions->DebugMode = 1;
	dokanOptions->UseKeepAlive = 1;

	status = DokanMain(dokanOptions, &dokanOperations);
	switch (status) {
		case DOKAN_SUCCESS:
			fprintf(stderr, "Success\n");
			break;
		case DOKAN_ERROR:
			fprintf(stderr, "Error\n");
			break;
		case DOKAN_DRIVE_LETTER_ERROR:
			fprintf(stderr, "Bad Drive letter\n");
			break;
		case DOKAN_DRIVER_INSTALL_ERROR:
			fprintf(stderr, "Can't install driver\n");
			break;
		case DOKAN_START_ERROR:
			fprintf(stderr, "Driver something wrong\n");
			break;
		case DOKAN_MOUNT_ERROR:
			fprintf(stderr, "Can't assign a drive letter\n");
			break;
		default:
			fprintf(stderr, "Unknown error: %d\n", status);
			break;
	}

	return 0;
}

