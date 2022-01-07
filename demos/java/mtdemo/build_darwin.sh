#!/bin/bash

echo "Build mtdemo"

cd ../gsjava

bash build_darwin.sh

cd ../mtdemo

cp ../mtdemo/gsjava.jar gsjava.jar

echo "Compiling mtdemo Java source..."
javac -classpath "../gsjava/bin:." "Main.java" "Worker.java"
echo "Done."

echo "Copy gs_jni.dylib"
cp "../jni/gs_jni/gs_jni.dylib" "gs_jni.dylib"

echo "Create libgpdl.dylib link"
cp "../../../sobin/libgpdl.dylib" "libgpdl.dylib"

cd ../../../sobin

echo "Copy libgpdl.dylib target"
cp $(readlink "libgpdl.dylib") "../demos/java/mtdemo"

cd ../demos/java/mtdemo