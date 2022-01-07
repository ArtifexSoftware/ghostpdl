@echo off

echo Build mtdemo

cd "..\gsjava"

call build_win32

cd "..\mtdemo"

copy "..\gsjava\gsjava.jar" ".\gsjava.jar"

echo Compiling mtdemo Java source...
javac -cp ../gsjava/bin;. Main.java Worker.java
echo Done.