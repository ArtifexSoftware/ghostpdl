@echo off
@rem $RCSfile$ $Revision: 1412 $

rem Convert PCL or PXL to PDF 1.3 (Acrobat 4-and-later compatible).
rem The default PDF compatibility level may change in the future:

echo -dCompatibilityLevel#1.3 >_.at
goto bot

rem Pass arguments through a file to avoid overflowing the command line.
:top
echo %1 >>_.at
shift
:bot
if not "%3"=="" goto top
call pcl2pdfwr %1 %2
