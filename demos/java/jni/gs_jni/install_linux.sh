#!bin/bash

echo "Copy libgpdl.so"
cp -L "../jni/gs_jni/gs_jni.so" "gs_jni.so"

echo "Create libpgdl.so link"
cp "../../../sobin/lbgpdl.so" "libgpdl.so"

cd ../../../sobin

echo "Copy libgpdl.so target"
cp $(readlink "libgpdl.so") "../demos/java/gsviewer"

cd ../demos/java.gsviewer
