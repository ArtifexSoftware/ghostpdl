@echo off 
@rem $Id$

if %1/==/ goto usage
if %2/==/ goto usage

call gssetgs.bat
set infile=%1
set outfile=%2

rem First we need to determine the bounding box. ps2epsi.ps below will pick
rem the result up from %outfile%
%GSC% -q -dNOPAUSE -dBATCH -P- -dSAFER -dDELAYSAFER -sDEVICE=bbox -sOutputFile=NUL %infile% 2> %outfile%

rem Ghostscript uses %outfile% to define the output file
%GSC% -q -dNOPAUSE -P- -dSAFER -dDELAYSAFER -sDEVICE=bit -sOutputFile=NUL ps2epsi.ps < %infile%

rem We bracket the actual file with a few commands to help encapsulation
echo %%%%Page: 1 1 >> %outfile%
echo %%%%BeginDocument: %outfile% >> %outfile%
echo /InitDictCount countdictstack def gsave save mark newpath >> %outfile%
echo userdict /setpagedevice /pop load put >> %outfile%

rem Append the original onto the preview header
rem cat.ps uses the %infile% and %outfile% environment variables for the filenames
%GSC% -q -dNOPAUSE -dBATCH -P- -dSAFER -dDELAYSAFER -sDEVICE=bit -sOutputFile=NUL cat.ps


echo %%%%EndDocument >> %outfile%
echo countdictstack InitDictCount sub { end } repeat >> %outfile%
echo cleartomark restore grestore >> %outfile%

goto end

:usage
echo "Usage: ps2epsi <infile.ps> <outfile.epi>"

:end
