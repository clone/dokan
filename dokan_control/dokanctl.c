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
#include <locale.h>

#include "dokan.h"
#include "dokanc.h"

int ShowMountList()
{
	DOKAN_CONTROL control;
	ULONG index = 0;
	ZeroMemory(&control, sizeof(DOKAN_CONTROL));

	control.Type = DOKAN_CONTROL_LIST;
	control.Index = 0;
	control.Status = DOKAN_CONTROL_SUCCESS;

	while(DokanMountControl(&control)) {
		if (control.Status == DOKAN_CONTROL_SUCCESS) {
			fwprintf(stderr, L"[% 2d] MountPoint: %s\n     DeviceName: %s\n",
				control.Index, control.MountPoint, control.DeviceName);
			control.Index++;
		} else {
			return 0;
		}
	}
	return 0;
}

int Unmount(LPCWSTR	DeviceName)
{
	DOKAN_CONTROL control;
	ZeroMemory(&control, sizeof(DOKAN_CONTROL));

	if (wcslen(DeviceName) == 1 && L'0' <= DeviceName[0] && DeviceName[0] <= L'9') {
		control.Type = DOKAN_CONTROL_LIST;
		control.Index = DeviceName[0] - L'0';
		DokanMountControl(&control);

		if (control.Status == DOKAN_CONTROL_SUCCESS) {
			return DokanUnmount(control.MountPoint);
		} else {
			fwprintf(stderr, L"Mount entry %d not found\n", control.Index);
			return -1;
		}
	} else {
		return DokanUnmount(DeviceName);
	}
}

#define GetOption(argc, argv, index) \
	(((argc) > (index) && \
		wcslen((argv)[(index)]) == 2 && \
		(argv)[(index)][0] == L'/')? \
		towlower((argv)[(index)][1]) : L'\0')

int __cdecl
wmain(int argc, PWCHAR argv[])
{
	ULONG	i;
	WCHAR	fileName[MAX_PATH];
	WCHAR	driverFullPath[MAX_PATH];
	WCHAR	mounterFullPath[MAX_PATH];
	WCHAR	type;

	//setlocale(LC_ALL, "");

	GetModuleFileName(NULL, fileName, MAX_PATH);

	// search the last "\"
	for(i = wcslen(fileName) -1; i > 0 && fileName[i] != L'\\'; --i)
		;
	fileName[i] = L'\0';

	ZeroMemory(mounterFullPath, sizeof(mounterFullPath));
	ZeroMemory(driverFullPath, sizeof(driverFullPath));
	wcscpy_s(mounterFullPath, MAX_PATH, fileName);
	mounterFullPath[i] = L'\\';
	wcscat_s(mounterFullPath, MAX_PATH, L"mounter.exe");

	GetSystemDirectory(driverFullPath, MAX_PATH);
	wcscat_s(driverFullPath, MAX_PATH, L"\\drivers\\dokan.sys");

	fwprintf(stderr, L"driver path %s\n", driverFullPath);
	fwprintf(stderr, L"mounter path %s\n", mounterFullPath);


	if (GetOption(argc, argv, 1) == L'v') {
		fprintf(stderr, "dokanctl : %s %s\n", __DATE__, __TIME__);
		fprintf(stderr, "Dokan version : %X\n", DokanVersion());
		fprintf(stderr, "Dokan driver version : %X\n", DokanDriverVersion());
		return 0;
	
	} else if (GetOption(argc, argv, 1) == L'm') {
		return ShowMountList();

	} else if (GetOption(argc, argv, 1) == L'u' && argc == 3) {
		return Unmount(argv[2]);

	} else if (argc < 3 || wcslen(argv[1]) != 2 || argv[1][0] != L'/' ) {
		fprintf(stderr, "dokanctrl /u DriveLetter\n");

		fprintf(stderr, "dokanctrl /i [d|s|a]\n");
		fprintf(stderr, "dokanctrl /r [d|s|a}\n");
		
		fprintf(stderr, "dokanctrl /v\n");

		fprintf(stderr, "\n");
		fprintf(stderr, "  /u\tUnmount drive\n");
		fprintf(stderr, "  /i s\tInstall mounter service\n");
		fprintf(stderr, "  /r d\tRemove driver\n");
		fprintf(stderr, "  /r a\tRemove driver and mounter service\n");
		fprintf(stderr, "  /v Print Dokan version\n");
		return -1;
	}

	type = towlower(argv[2][0]);

	switch(towlower(argv[1][1])) {
	case L'i':
		if (type ==  L'd') {
			if (DokanServiceInstall(DOKAN_DRIVER_SERVICE,
									SERVICE_FILE_SYSTEM_DRIVER,
									driverFullPath))
				fprintf(stderr, "driver install ok\n");
			else
				fprintf(stderr, "driver install failed\n");

		} else if (type == L's') {
			if (DokanServiceInstall(DOKAN_MOUNTER_SERVICE,
									SERVICE_WIN32_OWN_PROCESS,
									mounterFullPath))
				fprintf(stderr, "mounter install ok\n");
			else
				fprintf(stderr, "mounter install failed\n");
		
		} else if (type == L'a') {
			if (DokanServiceInstall(DOKAN_DRIVER_SERVICE,
									SERVICE_FILE_SYSTEM_DRIVER,
									driverFullPath))
				fprintf(stderr, "driver install ok\n");
			else
				fprintf(stderr, "driver install failed\n");

			if (DokanServiceInstall(DOKAN_MOUNTER_SERVICE,
									SERVICE_WIN32_OWN_PROCESS,
									mounterFullPath))
				fprintf(stderr, "mounter install ok\n");
			else
				fprintf(stderr, "mounter install failed\n");
		} else if (type == L'n') {
			if (DokanNetworkProviderInstall())
				fprintf(stderr, "network provider install ok\n");
			else
				fprintf(stderr, "network provider install failed\n");
		}
		break;

	case L'r':
		if (type == L'd') {
			if (DokanServiceDelete(DOKAN_DRIVER_SERVICE))
				fprintf(stderr, "driver remove ok\n");
			else
				fprintf(stderr, "driver remvoe failed\n");
		
		} else if (type == L's') {
			if (DokanServiceDelete(DOKAN_MOUNTER_SERVICE))
				fprintf(stderr, "mounter remove ok\n");
			else
				fprintf(stderr, "mounter remvoe failed\n");	
		
		} else if (type == L'a') {
			if (DokanServiceDelete(DOKAN_MOUNTER_SERVICE))
				fprintf(stderr, "mounter remove ok\n");
			else
				fprintf(stderr, "mounter remvoe failed\n");	

			if (DokanServiceDelete(DOKAN_DRIVER_SERVICE))
				fprintf(stderr, "driver remove ok\n");
			else
				fprintf(stderr, "driver remvoe failed\n");
		} else if (type == L'n') {
			if (DokanNetworkProviderUninstall())
				fprintf(stderr, "network provider remove ok\n");
			else
				fprintf(stderr, "network provider remove failed\n");
		}
		break;
	case L'd':
		if (L'0' <= type && type <= L'9') {
			ULONG mode = type - L'0';
			if (DokanSetDebugMode(mode)) {
				fprintf(stderr, "set debug mode ok\n");
			} else {
				fprintf(stderr, "set debug mode failed\n");
			}
		}
		break;
	default:
		fprintf(stderr, "unknown option\n");
	}
	

	return 0;
}

