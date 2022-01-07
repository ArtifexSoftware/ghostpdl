#!/bin/bash

echo "Build mtdemo"

cd ../gsjava

bash build_linux.sh

cd ../mtdemo

cp ../gsjava/gsjava.jar gsjava.jar

echo "Compiling mtdemo Java source..."
javac -classpath "../gsjava/bin:." "Main.java" "Worker.java"
echo "Done."

echo "Copy gs_jni.so"
cp "../jni/gs_jni/gs_jni.so" "gs_jni.so"

echo "Create libgpdl.so link"
cp "../../../sobin/libgpdl.so" "libgpdl.so"

cd ../../../sobin

echo "Copy libgpdl.so target"
cp $(readlink "libgpdl.so") "../demos/java/mtdemo"

cd ../demos/java/mtdemo