@echo off 
@rem $Id$
@rem Convert PDF to "optimized" form.

if %1/==/ goto usage
if %2/==/ goto usage
call %~dp0gssetgs.bat
echo -q -dNODISPLAY -P- -dSAFER -dDELAYSAFER >%TEMP%_.at
:cp
if %3/==/ goto doit
echo %1 >>%TEMP%_.at
shift
goto cp

:doit
%GSC% -q @%TEMP%_.at -- pdfopt.ps %1 %2
if exist %TEMP%_.at erase %TEMP%_.at
goto end

:usage
echo "Usage: pdfopt input.pdf output.pdf"

:end
