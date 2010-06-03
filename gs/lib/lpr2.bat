@echo off
@rem $Id$

call gssetgs.bat
%GSC% -sDEVICE#djet500 -P- -dSAFER -dNOPAUSE -sPROGNAME=lpr2 -- gslp.ps -2r %1 %2 %3 %4 %5 %6 %7 %8 %9
