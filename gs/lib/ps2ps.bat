@echo off 
@rem $Id$
@rem "Distill" PostScript.

if "%1"=="" goto usage
if "%2"=="" goto usage
echo -dNODISPLAY -dNOPAUSE -dSAFER -dBATCH >_.at
:cp
if "%3"=="" goto doit
echo %1 >>_.at
shift
goto cp

:doit
rem Watcom C deletes = signs, so use # instead.
gs -q -sDEVICE#pswrite -sOutputFile#%2 @_.at %1
goto end

:usage
echo "Usage: ps2ps ...switches... input.ps output.ps"

:end
