; De-RAS - Windows installer (NSIS 3, Modern UI 2)
; Build: makensis -DVERSION=0.1.0 -DLABEL=ALPHA -DSTAGE=<dist folder> -DOUTDIR=<out> installer/De-RAS.nsi
; Rules (from BASE - PROMPT):
;  - installs only when the user runs it; nothing is installed silently by De-RAS itself
;  - an older De-RAS installation is detected and removed only after the user agrees
;  - never touches HEC-RAS or any other software; no file associations are changed
;    (.prj is also used by HEC-RAS and other programs)
Unicode true
SetCompressor /SOLID lzma

!ifndef VERSION
  !define VERSION "0.1.0"
!endif
!ifndef LABEL
  !define LABEL "ALPHA"
!endif
!ifndef STAGE
  !error "STAGE (folder with De-RAS.exe and its files) is required"
!endif
!ifndef OUTDIR
  !define OUTDIR "."
!endif

!define APPNAME "De-RAS"
!define UNINSTKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\De-RAS"

Name "${APPNAME} ${VERSION} (${LABEL})"
OutFile "${OUTDIR}\De-RAS-${VERSION}-setup-x64.exe"
InstallDir "$PROGRAMFILES64\De-RAS"
InstallDirRegKey HKLM "Software\De-RAS" "InstallDir"
RequestExecutionLevel admin
ShowInstDetails show
ShowUninstDetails show
BrandingText "De-RAS ${VERSION}"

VIProductVersion "${VERSION}.0"
VIAddVersionKey /LANG=1033 "ProductName" "De-RAS"
VIAddVersionKey /LANG=1033 "FileDescription" "De-RAS installer"
VIAddVersionKey /LANG=1033 "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=1033 "ProductVersion" "${VERSION} ${LABEL}"
VIAddVersionKey /LANG=1033 "LegalCopyright" "De-RAS project"
VIAddVersionKey /LANG=1033 "Comments" "Not affiliated with HEC / USACE."

!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "FileFunc.nsh"

!define MUI_ICON "${STAGE}\De-RAS.ico"
!define MUI_UNICON "${STAGE}\De-RAS.ico"
!define MUI_ABORTWARNING
!define MUI_LANGDLL_ALLLANGUAGES
!define MUI_FINISHPAGE_RUN "$INSTDIR\De-RAS.exe"
!define MUI_FINISHPAGE_RUN_NOTCHECKED

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}\NOTICE.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

; English first = default language of the installer (since 0.2.1, review R7)
!insertmacro MUI_LANGUAGE "English"
!insertmacro MUI_LANGUAGE "Indonesian"

LangString OLD_FOUND ${LANG_INDONESIAN} "De-RAS versi $0 sudah terpasang di:$\r$\n$1$\r$\n$\r$\nHapus versi lama sebelum memasang versi ${VERSION}?$\r$\n(Proyek dan data Anda tidak dihapus. HEC-RAS dan program lain tidak disentuh.)$\r$\n$\r$\nYa = hapus versi lama lalu lanjut$\r$\nTidak = pasang tanpa menghapus$\r$\nBatal = keluar dari installer"
LangString OLD_FOUND ${LANG_ENGLISH} "De-RAS version $0 is already installed in:$\r$\n$1$\r$\n$\r$\nRemove the old version before installing version ${VERSION}?$\r$\n(Your projects and data are not deleted. HEC-RAS and other programs are not touched.)$\r$\n$\r$\nYes = remove the old version and continue$\r$\nNo = install without removing$\r$\nCancel = quit the installer"
LangString OLD_FAILED ${LANG_INDONESIAN} "Penghapusan versi lama tidak selesai (kode $0). Instalasi dilanjutkan."
LangString OLD_FAILED ${LANG_ENGLISH} "Removing the old version did not finish (code $0). Installation continues."
LangString NEED64 ${LANG_INDONESIAN} "De-RAS memerlukan Windows 10/11 64-bit."
LangString NEED64 ${LANG_ENGLISH} "De-RAS requires 64-bit Windows 10/11."
LangString SEC_APP ${LANG_INDONESIAN} "De-RAS (aplikasi dan mesin hitung)"
LangString SEC_APP ${LANG_ENGLISH} "De-RAS (application and compute engine)"
LangString SEC_START ${LANG_INDONESIAN} "Pintasan Start Menu"
LangString SEC_START ${LANG_ENGLISH} "Start Menu shortcut"
LangString SEC_DESK ${LANG_INDONESIAN} "Pintasan desktop"
LangString SEC_DESK ${LANG_ENGLISH} "Desktop shortcut"
LangString UN_LINK ${LANG_INDONESIAN} "Hapus De-RAS"
LangString UN_LINK ${LANG_ENGLISH} "Uninstall De-RAS"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_ICONSTOP $(NEED64)
    Abort
  ${EndIf}
  SetRegView 64
  !insertmacro MUI_LANGDLL_DISPLAY
  ; Detect an older De-RAS installation (only De-RAS's own uninstall key).
  ReadRegStr $0 HKLM "${UNINSTKEY}" "DisplayVersion"
  ReadRegStr $1 HKLM "${UNINSTKEY}" "InstallLocation"
  ReadRegStr $2 HKLM "${UNINSTKEY}" "UninstallString"
  ${If} $2 != ""
    MessageBox MB_YESNOCANCEL|MB_ICONQUESTION $(OLD_FOUND) IDYES remove IDNO keep
    Abort
    remove:
      ; run the old uninstaller in place, silently, and wait for it
      ExecWait '"$1\uninstall.exe" /S _?=$1' $0
      ${If} $0 != 0
        MessageBox MB_ICONEXCLAMATION $(OLD_FAILED)
      ${Else}
        Delete "$1\uninstall.exe"
        RMDir "$1"
      ${EndIf}
    keep:
  ${EndIf}
FunctionEnd

Section "!$(SEC_APP)" SecApp
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "${STAGE}\*.*"
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKLM "Software\De-RAS" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayName" "De-RAS ${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNINSTKEY}" "Publisher" "De-RAS"
  WriteRegStr HKLM "${UNINSTKEY}" "DisplayIcon" "$INSTDIR\De-RAS.exe"
  WriteRegStr HKLM "${UNINSTKEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKLM "${UNINSTKEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "${UNINSTKEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNINSTKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKLM "${UNINSTKEY}" "EstimatedSize" "$0"
SectionEnd

Section "$(SEC_START)" SecStart
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\De-RAS"
  CreateShortcut "$SMPROGRAMS\De-RAS\De-RAS.lnk" "$INSTDIR\De-RAS.exe" "" "$INSTDIR\De-RAS.exe" 0
  CreateShortcut "$SMPROGRAMS\De-RAS\$(UN_LINK).lnk" "$INSTDIR\uninstall.exe"
SectionEnd

Section /o "$(SEC_DESK)" SecDesk
  SetShellVarContext all
  CreateShortcut "$DESKTOP\De-RAS.lnk" "$INSTDIR\De-RAS.exe" "" "$INSTDIR\De-RAS.exe" 0
SectionEnd

Function un.onInit
  SetRegView 64
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd

Section "Uninstall"
  SetShellVarContext all
  Delete "$SMPROGRAMS\De-RAS\*.lnk"
  RMDir "$SMPROGRAMS\De-RAS"
  Delete "$DESKTOP\De-RAS.lnk"
  ; Remove only what the installer put there (user projects are never stored in the install folder).
  RMDir /r "$INSTDIR\platforms"
  RMDir /r "$INSTDIR\styles"
  RMDir /r "$INSTDIR\imageformats"
  RMDir /r "$INSTDIR\docs"
  RMDir /r "$INSTDIR\licenses"
  Delete "$INSTDIR\qt.conf"
  Delete "$INSTDIR\*.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\*.txt"
  Delete "$INSTDIR\*.ico"
  Delete "$INSTDIR\*.md"
  RMDir "$INSTDIR"
  DeleteRegKey HKLM "${UNINSTKEY}"
  DeleteRegKey HKLM "Software\De-RAS"
SectionEnd
