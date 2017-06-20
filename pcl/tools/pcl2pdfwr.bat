@echo off
@rem $RCSfile$ $Revision: 2063 $
rem Convert PCL or PXL to PDF without specifying CompatibilityLevel.

set PS2PDFPARAMS= -dNOPAUSE -dBATCH -sDEVICE#pdfwrite
set PS2PDFOPT=
set PS2PDFGS=gpcl6

if "%OS%"=="Windows_NT" goto nt

rem	Run pcl2pdf on any Microsoft OS.

:run
if "%1"=="" goto usage
if "%2"=="" goto usage
:opt
if "%3"=="" goto exec
set PS2PDFOPT=%PS2PDFOPT% %1
shift
goto opt

:exec
rem Watcom C deletes = signs, so use # instead.
%PS2PDFGS% %PS2PDFOPT% %PS2PDFPARAMS% -sOutputFile#%2 %1
goto end

:usage
echo "Usage: pcl2pdf [options...] input.pcl output.pdf"
goto end

rem	Run pcl2pdf on Windows NT.

:nt
if not CMDEXTVERSION 1 goto run
if "%1"=="" goto ntusage
if "%2"=="" goto nooutfile
if not "%3"=="" goto opt

rem Watcom C deletes = signs, so use # instead.
%PS2PDFGS% %PS2PDFOPT% %PS2PDFPARAMS% -sOutputFile#%2 %1
goto end

:ntusage
echo "Usage: pcl2pdf input.pcl [output.pdf]"
echo "   or: pcl2pdf [options...] input.pcl output.pdf"
goto end

:nooutfile
set PS2PDF=%1
set PS2PDF=%PS2PDF:.PS=.PDF%
%PS2PDFGS% %PS2PDFOPT% %PS2PDFPARAMS% -sOutputFile#%PS2PDF% %1

:end
rem	Clean up.
SET PS2PDFPARAMS=
SET PS2PDFGS=
SET PS2PDFOPT=
