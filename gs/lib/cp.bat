@echo off
if "%2"=="." goto ne
if exist _.tmp erase _.tmp > nul:
rem >_.tmp
copy /B %1+_.tmp %2 > nul:
erase _.tmp > nul:
goto x
:ne
copy /B %1 %2 > nul:
:x
