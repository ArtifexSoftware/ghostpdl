#!bin/bash

echo "Copy gs_jni.dylib"
cp "../jni/gs_jni/gs_jni.dylib" "gs_jni.dylib"

cd ../../../sobin

echo "Copy libgpdl.dylib"
cp $(readlink "libgpdl.dylib") "../demos/java/gsviewer"

cd ../demos/java/gsviewer