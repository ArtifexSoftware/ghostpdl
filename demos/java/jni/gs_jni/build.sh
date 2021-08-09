#!/bin/bash

unset LD_LIBRARY_PATH

mkdir -p obin

echo "Compiling gs_jni C++ source..."

echo "Compile callbacks.cpp"
g++ -c -Wall -O3 \
	-std=c++14 \
	-I./include \
	-I./include/linux \
	-I./../../../../psi \
	-I./../../../../devices \
	"callbacks.cpp" \
	-o "obin/callbacks.o"

echo "Compile com_artifex_gsjava_GSAPI.cpp"
g++ -c -Wall -O3 \
	-std=c++14 \
	-I./include \
	-I./include/linux \
	-I./../../../../psi \
	-I./../../../../devices \
	"com_artifex_gsjava_GSAPI.cpp" \
	-o "obin/com_artifex_gsjava_GSAPI.o"

echo "Compile com_artifex_gsjava_util_NativePointer.cpp"
g++ -c -Wall -O3 \
	-std=c++14 \
	-I./include \
	-I./include/linux \
	-I./../../../../psi \
	-I./../../../../devices \
	"com_artifex_gsjava_util_NativePointer.cpp" \
	-o "obin/com_artifex_gsjava_util_NativePointer.o"

echo "Compile jni_util.cpp"
g++ -c -Wall -O3\
	-std=c++14 \
	-I./include \
	-I./include/linux \
	-I./../../../../psi \
	-I./../../../../devices \
	"jni_util.cpp" \
	-o "obin/jni_util.o"

echo "Link"
g++ -shared \
	-Wl,-soname,gs_jni.so \
	-o "gs_jni.so" \
	"obin/callbacks.o" \
	"obin/com_artifex_gsjava_GSAPI.o" \
	"obin/com_artifex_gsjava_util_NativePointer.o" \
	"obin/jni_util.o" \
	"../../../../sobin/libgpdl.so"
