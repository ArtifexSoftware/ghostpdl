@echo off 
@rem $RCSfile$ $Revision$
@rem Convert PDF to DSC

if %1/==/ goto usage
if %2/==/ goto usage
call gssetgs.bat

rem Watcom C deletes = signs, so use # instead.
%GSC% -q -dNODISPLAY -dSAFER -dDELAYSAFER -sPDFname#%1 -sDSCname#%2 -sOutputFile#%2 pdf2dsc.ps
goto end

:usage
echo "Usage: pdf2dsc input.pdf output.dsc"

:end


