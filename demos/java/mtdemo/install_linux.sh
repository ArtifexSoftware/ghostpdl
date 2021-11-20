#!bin/bash

echo "Copy gs_jni.so"
cp "../jni/gs_jni/gs_jni.so" "gs_jni.so"

echo "Create libgpdl.so link"
cp "../../../sobin/libgpdl.so" "libgpdl.so"

cd ../../../sobin

echo "Copy libgpdl.so target"
cp $(readlink "libgpdl.so") "../demos/java/mtdemo"

cd ../demos/java/mtdemo
