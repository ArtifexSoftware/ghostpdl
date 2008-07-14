if '%1'=='' goto usage
if '%2'=='' goto p1
if '%3'=='' goto p2
if '%4'=='' goto p3
goto usage
:p3
pclxl -sDEVICE#ljet4 -dMaxBitmap#200000 -dBufferSpace#200000 -sOutputFile#PRN -dNOPAUSE %1 -dFirstPage#%2 -dLastPage#%3
goto x
:p2
pclxl -sDEVICE#ljet4 -dMaxBitmap#200000 -dBufferSpace#200000 -sOutputFile#PRN -dNOPAUSE %1 -dFirstPage#%2
goto x
:p1
pclxl -sDEVICE#ljet4 -dMaxBitmap#200000 -dBufferSpace#200000 -sOutputFile#PRN -dNOPAUSE %1
goto x
:usage
echo Usage: pxprint file [first-page [last-page]]
:x
