@echo off 
@rem $Id$
@rem Linearized PDF hint formatting utility.

if %1/==/ goto usage
call %~dp0gssetgs.bat
echo -q -dNODISPLAY -P- -dSAFER -dDELAYSAFER >%TEMP%_.at
:cp
if %2/==/ goto doit
echo %2 >>%TEMP%_.at
shift
goto cp

:doit
%GSC% -q @%TEMP%_.at -- dumphint.ps %1
if exist %TEMP%_.at erase %TEMP%_.at
goto end

:usage
echo Usage: dumphint input.pdf

:end
