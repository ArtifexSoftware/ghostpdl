#!bin/bash

echo "Copy gs_jni.dylib"
cp "../jni/gs_jni/gs_jni.dylib" "gs_jni.dylib"

echo "Create libgpdl.dylib link"
cp "../../../sobin/libgpdl.dylib" "libgpdl.dylib"

cd ../../../sobin

echo "Copy libgpdl.dylib target"
cp $(readlink "libgpdl.dylib") "../demos/java/mtdemo"

cd ../demos/java/mtdemo