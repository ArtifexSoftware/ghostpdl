@echo off

rem Set default values for GS (gs with graphics window) and GSC
rem (console mode gs) if the user hasn't set them.
rem if 64-bit version is available, prefer that.

if NOT %GS%/==/ goto :gsset
if EXIST %~dp0..\bin\gswin32.exe set GS=%~dp0..\bin\gswin32
if EXIST %~dp0..\bin\gswin64.exe set GS=%~dp0..\bin\gswin64
if %GS/==/ set GS=gswin32
:gsset

if NOT %GSC%/==/ goto :gscset
if EXIST %~dp0..\bin\gswin32c.exe set GSC=%~dp0..\bin\gswin32c
if EXIST %~dp0..\bin\gswin64c.exe set GSC=%~dp0..\bin\gswin64c
if %GSC%/==/ set GSC=gswin32c
:gscset
