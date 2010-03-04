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
#include "mount.h"


BOOL
DokanControlMount(
	ULONG	DeviceNumber,
	WCHAR	DriveLetter)
{
	WCHAR   volumeName[] = L"\\\\.\\ :";
	WCHAR	driveLetterAndSlash[] = L"C:\\";
	HANDLE  device;
	WCHAR	deviceName[MAX_PATH];
	
	wsprintf(deviceName, DOKAN_RAW_DEVICE_NAME, DeviceNumber);

	volumeName[4] = DriveLetter;
	driveLetterAndSlash[0] = DriveLetter;

	DbgPrintW(L"DeviceNumber %d DriveLetter %c\n", DeviceNumber, DriveLetter);
	DbgPrintW(L"DeviceName %s\n",deviceName);

	device = CreateFile(
		volumeName,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_NO_BUFFERING,
		NULL
		);

    if (device != INVALID_HANDLE_VALUE) {
		DbgPrintW(L"DokanControl Mount failed: %wc: is alredy used\n", DriveLetter);
		CloseHandle(device);
        return FALSE;
    }

    if (!DefineDosDevice(DDD_RAW_TARGET_PATH, &volumeName[4], deviceName)) {
		DbgPrintW(L"DokanControl DefineDosDevice failed: %d\n", GetLastError());
        return FALSE;
    }

	/* NOTE: IOCTL_MOUNTDEV_QUERY_DEVICE_NAME in sys/device.cc handles
	   GetVolumeNameForVolumeMountPoint. But it returns error even if driver
	   return success.

	wsprintf(deviceName, L"\\\\?\\Volume{dca0e0a5-d2ca-4f0f-8416-a6414657a77a}\\");
	DbgPrintW(L"DeviceName %s\n",deviceName);

	if (!GetVolumeNameForVolumeMountPoint(
			driveLetterAndSlash, 
			uniqueVolumeName,
			MAX_PATH)) {

		DbgPrint("Error: GetVolumeNameForVolumeMountPoint failed : %d\n", GetLastError());
	} else {
	
		DbgPrintW(L"UniqueVolumeName %s\n", uniqueVolumeName);
		DefineDosDevice(DDD_REMOVE_DEFINITION,
						&volumeName[4],
				        NULL);

		if (!SetVolumeMountPoint(driveLetterAndSlash, deviceName)) {
			DbgPrint("Error: SetVolumeMountPoint failed : %d\n", GetLastError());
			return FALSE;
		}
	}*/

    device = CreateFile(
        volumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (device == INVALID_HANDLE_VALUE) {
		DbgPrintW(L"DokanControl Mount %ws failed:%d\n", volumeName, GetLastError());
        DefineDosDevice(DDD_REMOVE_DEFINITION, &volumeName[4], NULL);
        return FALSE;
    }

	CloseHandle(device);

    return TRUE;
}


BOOL
DokanControlUnmount(
	WCHAR DriveLetter)
{
    WCHAR   volumeName[] = L"\\\\.\\ :";
    HANDLE  device;

    volumeName[4] = DriveLetter;
/*
    device = CreateFile(
        volumeName,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING,
        NULL
        );

    if (device == INVALID_HANDLE_VALUE) {
		DbgPrintW(L"DriveLetter %wc\n", DriveLetter);
        DbgPrintW(L"DokanControl Unmount failed\n");
        return FALSE;
    }

    CloseHandle(device);
*/
    if (!DefineDosDevice(DDD_REMOVE_DEFINITION, &volumeName[4], NULL)) {
		DbgPrintW(L"DriveLetter %wc\n", DriveLetter);
        DbgPrintW(L"DokanControl DefineDosDevice failed\n");
        return FALSE;
	} else {
		DbgPrintW(L"DokanControl DD_REMOVE_DEFINITION success\n");
	}

	return TRUE;
}
