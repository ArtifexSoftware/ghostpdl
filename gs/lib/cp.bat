@echo off
if "%2"=="." goto ne
erase _.tmp
rem >_.tmp
copy /B %1+_.tmp %2
erase _.tmp
goto x
:ne
copy /B %1 %2
:x
