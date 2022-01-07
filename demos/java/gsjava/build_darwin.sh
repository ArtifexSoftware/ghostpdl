#!/bin/bash

mkdir -p bin

cd "../jni/gs_jni"

bash build_darwin.sh

cd "../../gsjava"

echo "Compiling gsjava Java source..."
javac -sourcepath src/ -d bin/ \
	"src/com/artifex/gsjava/GSAPI.java" \
	"src/com/artifex/gsjava/GSInstance.java" \
	\
	"src/com/artifex/gsjava/util/AllocationError.java" \
	"src/com/artifex/gsjava/util/ByteArrayReference.java" \
	"src/com/artifex/gsjava/util/BytePointer.java" \
	"src/com/artifex/gsjava/util/NativeArray.java" \
	"src/com/artifex/gsjava/util/NativePointer.java" \
	"src/com/artifex/gsjava/util/Reference.java" \
	"src/com/artifex/gsjava/util/StringUtil.java" \
	\
	"src/com/artifex/gsjava/callbacks/DisplayCallback.java" \
	"src/com/artifex/gsjava/callbacks/ICalloutFunction.java" \
	"src/com/artifex/gsjava/callbacks/IPollFunction.java" \
	"src/com/artifex/gsjava/callbacks/IStdErrFunction.java" \
	"src/com/artifex/gsjava/callbacks/IStdInFunction.java" \
	"src/com/artifex/gsjava/callbacks/IStdOutFunction.java" \
	\
	"src/com/artifex/gsjava/devices/BMPDevice.java" \
	"src/com/artifex/gsjava/devices/Device.java" \
	"src/com/artifex/gsjava/devices/DeviceInUseException.java" \
	"src/com/artifex/gsjava/devices/DeviceNotSupportedException.java" \
	"src/com/artifex/gsjava/devices/DisplayDevice.java" \
	"src/com/artifex/gsjava/devices/EPSDevice.java" \
	"src/com/artifex/gsjava/devices/FAXDevice.java" \
	"src/com/artifex/gsjava/devices/FileDevice.java" \
	"src/com/artifex/gsjava/devices/HighLevelDevice.java" \
	"src/com/artifex/gsjava/devices/JPEGDevice.java" \
	"src/com/artifex/gsjava/devices/OCRDevice.java" \
	"src/com/artifex/gsjava/devices/PCXDevice.java" \
	"src/com/artifex/gsjava/devices/PDFDevice.java" \
	"src/com/artifex/gsjava/devices/PDFImageDevice.java" \
	"src/com/artifex/gsjava/devices/PDFPostscriptDeviceFamily.java" \
	"src/com/artifex/gsjava/devices/PNGDevice.java" \
	"src/com/artifex/gsjava/devices/PNMDevice.java" \
	"src/com/artifex/gsjava/devices/PostScriptDevice.java" \
	"src/com/artifex/gsjava/devices/PSDDevice.java" \
	"src/com/artifex/gsjava/devices/PXLDevice.java" \
	"src/com/artifex/gsjava/devices/TextDevice.java" \
	"src/com/artifex/gsjava/devices/TIFFDevice.java" \
	"src/com/artifex/gsjava/devices/XPSDevice.java"

cd bin

echo "Packing gsjava JAR file..."
jar cfm "../gsjava.jar" "../Manifest.md" "com/"

cd ..