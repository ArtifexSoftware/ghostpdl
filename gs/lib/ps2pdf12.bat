@echo off
@rem $Id$

rem Convert PostScript to PDF 1.2 (Acrobat 3-and-later compatible).

echo -dCompatibilityLevel#1.2 >%TEMP%_.at
goto bot

rem Pass arguments through a file to avoid overflowing the command line.
:top
echo %1 >>%TEMP%_.at
shift
:bot
if not %3/==/ goto top
call %~dp0ps2pdfxx %1 %2
