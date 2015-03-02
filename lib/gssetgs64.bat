@echo off

rem Set default values for GS (gs with graphics window) and GSC
rem (console mode gs) if the user hasn't set them.

if %GS%/==/ set GS=gswin64
if %GSC%/==/ set GSC=gswin64c
