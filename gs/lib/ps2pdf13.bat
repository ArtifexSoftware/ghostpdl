@echo off
@rem $Id$

rem Convert PostScript to PDF 1.3 (Acrobat 4-and-later compatible).

rem Make sure an empty argument list stays empty.
if "%1"=="" goto bot

rem This clunky loop is the only way to ensure we pass the full
rem argument list!
set PS2PDFSW=-dCompatibilityLevel#1.3
goto bot
:top
set PS2PDFSW=%PS2PDFSW% %1
shift
:bot
if not "%1"=="" goto top
ps2pdfwr %PS2PDFSW%
