@echo off

call "%~dp0gssetgs.bat"
%GSC% -P- -dSAFER -q -dNODISPLAY -dWRITESYSTEMDICT -- font2c.ps %1 %2 %3 %4 %5 %6 %7 %8 %9
