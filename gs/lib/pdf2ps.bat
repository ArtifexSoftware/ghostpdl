@echo off 
@rem Convert PDF to PostScript.

if "%1"=="" goto usage
if "%2"=="" goto usage
echo -dNODISPLAY -dNOPAUSE >_.at
:cp
if "%3"=="" goto doit
echo %1 >>_.at
shift
goto cp

:doit
rem Watcom C deletes = signs, so use # instead.
gs -q -sPSFile#%2 @_.at %1 -c quit
goto end

:usage
echo "Usage: pdf2ps [-dPSBinaryOK] [-dPSLevel1] [-dPSNoProcSet] input.pdf output.ps"

:end
