#!/bin/bash

# build gsjava
cd ../gsjava

bash build_linux.sh

cd ../gsviewer

cp ../gsjava/gsjava.jar gsjava.jar

mkdir -p bin

echo "Compiling gsviewer Java source..."
javac -sourcepath src/ -d bin/ \
	-classpath "../gsjava/bin" \
	"src/com/artifex/gsviewer/DefaultUnhandledExceptionHandler.java" \
	"src/com/artifex/gsviewer/Page.java" \
	"src/com/artifex/gsviewer/Settings.java" \
	"src/com/artifex/gsviewer/Document.java" \
	"src/com/artifex/gsviewer/ImageUtil.java" \
	"src/com/artifex/gsviewer/PageUpdateCallback.java" \
	"src/com/artifex/gsviewer/StdIO.java" \
	"src/com/artifex/gsviewer/GSFileFilter.java" \
	"src/com/artifex/gsviewer/Main.java" \
	"src/com/artifex/gsviewer/PDFFileFilter.java" \
	"src/com/artifex/gsviewer/ViewerController.java" \
	\
	"src/com/artifex/gsviewer/gui/ScrollMap.java" \
	"src/com/artifex/gsviewer/gui/SettingsDialog.java" \
	"src/com/artifex/gsviewer/gui/ViewerGUIListener.java" \
	"src/com/artifex/gsviewer/gui/ViewerWindow.java"

cd bin

echo "Packing gsviewer JAR file..."
jar cfm "../gsviewer.jar" "../Manifest.md" "com/"


echo "Copy gs_jni.so"
cp "../jni/gs_jni/gs_jni.so" "gs_jni.so"

echo "Create libgpdl.so link"
cp "../../../sobin/libgpdl.so" "libgpdl.so"

cd ../../../sobin

echo "Copy libgpdl.so target"
cp $(readlink "libgpdl.so")  "../demos/java/gsviewer"

cd ../demos/java/gsviewer