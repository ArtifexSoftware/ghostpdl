#!bin/bash

echo "Copy libgpdl.so -> /usr/lib/libgpdl.so"
sudo cp -L "../../../../sobin/libgpdl.so" "/usr/lib/libgpdl.so"

echo "Copy gs_jni.so -> /usr/lib/gs_jni.so"
sudo cp "gs_jni.so" "/usr/lib/gs_jni.so"