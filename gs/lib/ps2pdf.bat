@echo off
@rem $Id$

rem Convert PostScript to PDF 1.3 (Acrobat 4-and-later compatible).
rem The PDF compatibility level may change in the future:
rem use ps2pdf12 or ps2pdf13 if you want a specific level.

rem >_.at
goto bot

rem Pass arguments through a file to avoid overflowing the command line.
:top
echo %1 >>_.at
shift
:bot
if not "%3"=="" goto top
ps2pdfxx %1 %2
