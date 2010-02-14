!define VERSION "1.0.0"

!include LogicLib.nsh
!include x64.nsh
!include WinVer.nsh

Name "DokanLibraryInstaller ${VERSION}"
OutFile "DokanInstall_${VERSION}.exe"

InstallDir $PROGRAMFILES32\Dokan\DokanLibrary
RequestExecutionLevel admin
LicenseData "licdata.rtf"
ShowUninstDetails show

Page license
Page components
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles


!macro X86Files os

  SetOutPath $PROGRAMFILES32\Dokan\DokanLibrary
 
    File ..\dokan\readme.txt
    File ..\dokan\readme.ja.txt
    File ..\dokan\dokan.h
    File ..\license.gpl.txt
    File ..\license.lgpl.txt
    File ..\license.mit.txt
    File ..\dokan\objchk_${os}_x86\i386\dokan.lib
    File ..\dokan_control\objchk_${os}_x86\i386\dokanctl.exe
    File ..\dokan_mount\objchk_${os}_x86\i386\mounter.exe

  SetOutPath $PROGRAMFILES32\Dokan\DokanLibrary\sample\mirror

    File ..\dokan_mirror\makefile
    File ..\dokan_mirror\mirror.c
    File ..\dokan_mirror\sources
    File ..\dokan_mirror\objchk_${os}_x86\i386\mirror.exe

  SetOutPath $SYSDIR

    File ..\dokan\objchk_${os}_x86\i386\dokan.dll

!macroend

/*
!macro X64Files os

  SetOutPath $PROGRAMFILES64\Dokan\DokanLibrary

    File ..\dokan\readme.txt
    File ..\dokan\readme.ja.txt
    File ..\dokan\dokan.h
    File ..\license.gpl.txt
    File ..\license.lgpl.txt
    File ..\license.mit.txt
    File ..\dokan\objchk_${os}_amd64\amd64\dokan.lib
    File ..\dokan_control\objchk_${os}_amd64\amd64\dokanctl.exe
    File ..\dokan_mount\objchk_${os}_amd64\amd64\mounter.exe

  SetOutPath $PROGRAMFILES64\Dokan\DokanLibrary\sample\mirror

    File ..\dokan_mirror\makefile
    File ..\dokan_mirror\mirror.c
    File ..\dokan_mirror\sources
    File ..\dokan_mirror\objchk_${os}_amd64\amd64\mirror.exe

  ${DisableX64FSRedirection}

  SetOutPath $SYSDIR

    File ..\dokan\objchk_${os}_amd64\amd64\dokan.dll

  ${EnableX64FSRedirection}

!macroend
*/

!macro DokanSetup
  ExecWait '"$PROGRAMFILES32\Dokan\DokanLibrary\dokanctrl.exe" /i a' $0
  DetailPrint "dokanctrl returned $0"
  WriteUninstaller $PROGRAMFILES32\Dokan\DokanLibrary\uninstaller.exe
!macroend

!macro X86Driver os
  SetOutPath $SYSDIR\drivers
    File ..\sys\objchk_${os}_x86\i386\dokan.sys
!macroend

!macro X64Driver os
  ${DisableX64FSRedirection}

  SetOutPath $SYSDIR\drivers

    File ..\sys\objchk_${os}_amd64\amd64\dokan.sys

  ${EnableX64FSRedirection}
!macroend


Section "Dokan Library x86" section_x86
  ${If} ${IsWin7}
    !insertmacro X86Files "win7"
  ${ElseIf} ${IsWinVista}
    !insertmacro X86Files "wlh"
  ${ElseIf} ${IsWin2003}
    !insertmacro X86Files "wnet"
  ${ElseIf} ${IsWinXp}
    !insertmacro X86Files "wxp"
  ${EndIf}
SectionEnd

Section "Dokan Driver x86" section_x86_driver
  ${If} ${IsWin7}
    !insertmacro X86Driver "win7"
  ${ElseIf} ${IsWinVista}
    !insertmacro X86Driver "wlh"
  ${ElseIf} ${IsWin2003}
    !insertmacro X86Driver "wnet"
  ${ElseIf} ${IsWinXp}
    !insertmacro X86Driver "wxp"
  ${EndIf}
  !insertmacro DokanSetup
SectionEnd

Section "Dokan Driver x64" section_x64_driver
  ${If} ${IsWin7}
    !insertmacro X64Driver "win7"
  ${ElseIf} ${IsWinVista}
    !insertmacro X64Driver "wlh"
  ${ElseIf} ${IsWin2003}
    !insertmacro X64Driver "wnet"
  ${EndIf}
  !insertmacro DokanSetup
SectionEnd

/*
Section "Dokan Library x64" section_x64
  ${If} ${IsWin7}
    !insertmacro X64Files "win7"
  ${ElseIf} ${IsWinVista}
    !insertmacro X64Files "wlh"
  ${ElseIf} ${IsWin2003}
    !insertmacro X64Files "wnet"
  ${EndIf}
SectionEnd
*/

Section "Uninstall"
  ExecWait '"$PROGRAMFILES32\Dokan\DokanLibrary\dokanctrl.exe" /r a' $0
  DetailPrint "dokanctrl.exe returned $0"

  RMDir /r $PROGRAMFILES32\Dokan\DokanLibrary
  RMDir $PROGRAMFILES32\Dokan
  Delete $SYSDIR\dokan.dll

  ${If} ${RunningX64}
    ${DisableX64FSRedirection}
      Delete $SYSDIR\drivers\dokan.sys
    ${EnableX64FSRedirection}
  ${Else}
    Delete $SYSDIR\drivers\dokan.sys
  ${EndIf}

  MessageBox MB_YESNO "A reboot is required to finish the uninstallation. Do you wish to reboot now?" IDNO noreboot
    Reboot
  noreboot:

SectionEnd

Function .onInit
  ${If} ${RunningX64}
    SectionSetFlags ${section_x86} ${SF_SELECTED}
    SectionSetFlags ${section_x86_driver} ${SF_RO}  ; disable
    SectionSetFlags ${section_x64_driver} ${SF_SELECTED}
  ${Else}
    SectionSetFlags ${section_x86} ${SF_SELECTED}
    SectionSetFlags ${section_x86_driver} ${SF_SELECTED}
    SectionSetFlags ${section_x64_driver} ${SF_RO}  ; disable
  ${EndIf}

  ; Windows Version check

  ${If} ${RunningX64}
    ${If} ${IsWin2003}
    ${ElseIf} ${IsWinVista}
    ${ElseIf} ${IsWin7}
    ${Else}
      MessageBox MB_OK "Your OS is not supported. Dokan library supports Windows 2003, Vista and 7 for x64."
      Abort
    ${EndIf}
  ${Else}
    ${If} ${IsWinXP}
    ${ElseIf} ${IsWin2003}
    ${ElseIf} ${IsWinVista}
    ${ElseIf} ${IsWin7}
    ${Else}
      MessageBox MB_OK "Your OS is not supported. Dokan library supports Windows XP, 2003, Vista and 7 for x86."
      Abort
    ${EndIf}
  ${EndIf}

  ; Previous version
  ${If} ${RunningX64}
    ${DisableX64FSRedirection}
      IfFileExists $SYSDIR\drivers\dokan.sys HasPreviousVersion NoPreviousVersion
    ${EnableX64FSRedirection}
  ${Else}
    IfFileExists $SYSDIR\drivers\dokan.sys HasPreviousVersion NoPreviousVersion
  ${EndIf}

  HasPreviousVersion:
    MessageBox MB_OK "Please unstall the previous version and restart your computer before running this installer."
    Abort
  NoPreviousVersion:

FunctionEnd

Function .onInstSuccess
  ExecShell "open" "$PROGRAMFILES32\Dokan\DokanLibrary"
FunctionEnd

