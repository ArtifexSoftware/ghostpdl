@echo off
:next
if '%1'=='' goto exit
if '%1'=='-f' goto sh
if '%1'=='-r' goto sh
if exist %1 erase /Q %1
:sh
shift
goto next
:exit
