SetCompressor /SOLID /FINAL lzma
XPStyle on
CRCCheck on

!include "MUI.nsh"

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "LICENSE"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

Name "GPL Ghostscript"
OutFile "GS872w32.exe"
Icon   obj\gswin.ico
UninstallIcon   obj\gswin.ico
RequestExecutionLevel admin

!ifndef VERSION
!define VERSION 8.72
!endif

; Some default compiler settings (uncomment and change at will):
; SetCompress auto ; (can be off or force)
; SetDatablockOptimize on ; (can be off)
; CRCCheck on ; (can be off)
; AutoCloseWindow false ; (can be true for the window go away automatically at end)
; ShowInstDetails hide ; (can be show to have them shown, or nevershow to disable)
; SetDateSave off ; (can be on to have files restored to their orginal date)

LicenseText "You must agree to this license before installing."
LicenseData "LICENSE"


InstallDir "$PROGRAMFILES\gs\gs${VERSION}"
;InstallDirRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Artifex\GPL Ghostscript" ""
; DirShow show ; (make this hide to not let the user change it)
DirText "Select the directory to install GPL Ghostscript in:"

Section "" ; (default section)
SetOutPath "$INSTDIR"
CreateDirectory "$INSTDIR\bin"
; add files / whatever that need to be installed here.
File   /r /x contrib /x lcms /x expat /x jasper doc
File   /r /x zlib /x expat examples
File   /r /x contrib /x expat /x luratech /x lwf_jp2 /x lcms lib
File /oname=bin\gsdll32.dll .\bin\gsdll32.dll
File /oname=bin\gsdll32.lib .\bin\gsdll32.lib
File /oname=bin\gswin32.exe .\bin\gswin32.exe
File /oname=bin\gswin32c.exe .\bin\gswin32c.exe
WriteRegStr HKEY_LOCAL_MACHINE "Software\GPL Ghostscript\${VERSION}" "GS_DLL" "$INSTDIR\bin\gsdll32.dll"
WriteRegStr HKEY_LOCAL_MACHINE "Software\GPL Ghostscript\${VERSION}" "GS_LIB" "$INSTDIR\bin;$INSTDIR\..\fonts"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Artifex\GPL Ghostscript" "" "$INSTDIR"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "DisplayName" "GPL Ghostscript (remove only)"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "UninstallString" '"$INSTDIR\uninstgs.exe"'
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "Publisher" "Artifex Software Inc."
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "HelpLink" "http://www.ghostscript.com/"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "URLInfoAbout" "http://www.ghostscript.com/"
WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript" "DisplayVersion" "${VERSION}"
; write out uninstaller
WriteUninstaller "$INSTDIR\uninstgs.exe"
SectionEnd ; end of default section

Function .onInstSuccess
    SetShellVarContext all
    CreateDirectory "$SMPROGRAMS\Ghostscript"
    CreateShortCut "$SMPROGRAMS\Ghostscript\Ghostscript ${VERSION}.LNK" "$INSTDIR\bin\gswin32.exe" '"-I$INSTDIR\lib;$INSTDIR\..\fonts"'
    CreateShortCut "$SMPROGRAMS\Ghostscript\Ghostscript Readme ${VERSION}.LNK" "$INSTDIR\doc\Readme.htm"
    MessageBox MB_YESNO "Use Windows TrueType fonts for Chinese, Japanese and Korean?" /SD IDYES IDNO NoReadme
       ExecWait '"$INSTDIR\bin\gswin32c.exe" -q -dBATCH -sFONTDIR="$FONTS" -sCIDFMAP="$INSTDIR\lib\cidfmap" "$INSTDIR\lib\mkcidfm.ps"'
    NoReadme:
FunctionEnd

Function .onInit
  System::Call 'kernel32::CreateMutexA(i 0, i 0, t "GhostscriptInstaller") i .r1 ?e'
  Pop $R0
  StrCmp $R0 0 +3
    MessageBox MB_OK "The Ghostscript installer is already running."
    Abort
FunctionEnd

; begin uninstall settings/section
UninstallText "This will uninstall GPL Ghostscript from your system"

Section Uninstall
; add delete commands to delete whatever files/registry keys/etc you installed here.
    SetShellVarContext all
    Delete "$SMPROGRAMS\Ghostscript\Ghostscript ${VERSION}.LNK"
    Delete "$SMPROGRAMS\Ghostscript\Ghostscript Readme ${VERSION}.LNK"
    RMDir "$SMPROGRAMS\Ghostscript"
Delete "$INSTDIR\uninstgs.exe"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Artifex\GPL Ghostscript"
DeleteRegKey HKEY_LOCAL_MACHINE "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\GPL Ghostscript"
RMDir /r "$INSTDIR"
SectionEnd ; end of uninstall section

; eof
