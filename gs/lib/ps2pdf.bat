@echo off
@rem $Id$

rem Convert PostScript to PDF 1.2 (Acrobat 3-and-later compatible).
rem The PDF compatibility level may change in the future:
rem use ps2pdf12 or ps2pdf13 if you want a specific level.

rem Make sure an empty argument list stays empty.
if "%1"=="" goto bot

rem This clunky loop is the only way to ensure we pass the full
rem argument list!
set PS2PDFSW=
goto bot
:top
set PS2PDFSW=%PS2PDFSW% %1
shift
:bot
if not "%1"=="" goto top
ps2pdf12 %PS2PDFSW%
