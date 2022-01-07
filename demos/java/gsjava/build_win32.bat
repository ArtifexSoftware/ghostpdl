@echo off

if not exist bin mkdir bin

echo Compiling gsjava Java source...
javac -sourcepath src\ -d bin^
	src\com\artifex\gsjava\GSAPI.java^
	src\com\artifex\gsjava\GSInstance.java^
	^
	src\com\artifex\gsjava\util\AllocationError.java^
	src\com\artifex\gsjava\util\ByteArrayReference.java^
	src\com\artifex\gsjava\util\BytePointer.java^
	src\com\artifex\gsjava\util\NativeArray.java^
	src\com\artifex\gsjava\util\NativePointer.java^
	src\com\artifex\gsjava\util\Reference.java^
	src\com\artifex\gsjava\util\StringUtil.java^
	^
	src\com\artifex\gsjava\callbacks\DisplayCallback.java^
	src\com\artifex\gsjava\callbacks\ICalloutFunction.java^
	src\com\artifex\gsjava\callbacks\IPollFunction.java^
	src\com\artifex\gsjava\callbacks\IStdErrFunction.java^
	src\com\artifex\gsjava\callbacks\IStdInFunction.java^
	src\com\artifex\gsjava\callbacks\IStdOutFunction.java^
	^
	src\com\artifex\gsjava\devices\BMPDevice.java^
	src\com\artifex\gsjava\devices\Device.java^
	src\com\artifex\gsjava\devices\DeviceInUseException.java^
	src\com\artifex\gsjava\devices\DeviceNotSupportedException.java^
	src\com\artifex\gsjava\devices\DisplayDevice.java^
	src\com\artifex\gsjava\devices\EPSDevice.java^
	src\com\artifex\gsjava\devices\FAXDevice.java^
	src\com\artifex\gsjava\devices\FileDevice.java^
	src\com\artifex\gsjava\devices\HighLevelDevice.java^
	src\com\artifex\gsjava\devices\JPEGDevice.java^
	src\com\artifex\gsjava\devices\OCRDevice.java^
	src\com\artifex\gsjava\devices\PCXDevice.java^
	src\com\artifex\gsjava\devices\PDFDevice.java^
	src\com\artifex\gsjava\devices\PDFImageDevice.java^
	src\com\artifex\gsjava\devices\PDFPostscriptDeviceFamily.java^
	src\com\artifex\gsjava\devices\PNGDevice.java^
	src\com\artifex\gsjava\devices\PNMDevice.java^
	src\com\artifex\gsjava\devices\PostScriptDevice.java^
	src\com\artifex\gsjava\devices\PSDDevice.java^
	src\com\artifex\gsjava\devices\PXLDevice.java^
	src\com\artifex\gsjava\devices\TextDevice.java^
	src\com\artifex\gsjava\devices\TIFFDevice.java^
	src\com\artifex\gsjava\devices\XPSDevice.java

cd bin

echo Packing JAR file...
jar -cf ..\gsjava.jar^
	com\artifex\gsjava\GSAPI.class^
	com\artifex\gsjava\GSAPI$Revision.class^
	com\artifex\gsjava\GSInstance.class^
	com\artifex\gsjava\GSInstance$GSParam.class^
	com\artifex\gsjava\GSInstance$ParamIterator.class^
	^
	com\artifex\gsjava\util\AllocationError.class^
	com\artifex\gsjava\util\ByteArrayReference.class^
	com\artifex\gsjava\util\BytePointer.class^
	com\artifex\gsjava\util\NativeArray.class^
	com\artifex\gsjava\util\NativePointer.class^
	com\artifex\gsjava\util\Reference.class^
	com\artifex\gsjava\util\StringUtil.class^
	^
	com\artifex\gsjava\callbacks\DisplayCallback.class^
	com\artifex\gsjava\callbacks\ICalloutFunction.class^
	com\artifex\gsjava\callbacks\IPollFunction.class^
	com\artifex\gsjava\callbacks\IStdErrFunction.class^
	com\artifex\gsjava\callbacks\IStdInFunction.class^
	com\artifex\gsjava\callbacks\IStdOutFunction.class^
	^
	com\artifex\gsjava\devices\BMPDevice.class^
	com\artifex\gsjava\devices\Device.class^
	com\artifex\gsjava\devices\Device$StdIO.class^
	com\artifex\gsjava\devices\DeviceInUseException.class^
	com\artifex\gsjava\devices\DeviceNotSupportedException.class^
	com\artifex\gsjava\devices\DisplayDevice.class^
	com\artifex\gsjava\devices\EPSDevice.class^
	com\artifex\gsjava\devices\FAXDevice.class^
	com\artifex\gsjava\devices\FileDevice.class^
	com\artifex\gsjava\devices\HighLevelDevice.class^
	com\artifex\gsjava\devices\JPEGDevice.class^
	com\artifex\gsjava\devices\OCRDevice.class^
	com\artifex\gsjava\devices\PCXDevice.class^
	com\artifex\gsjava\devices\PDFDevice.class^
	com\artifex\gsjava\devices\PDFImageDevice.class^
	com\artifex\gsjava\devices\PDFPostscriptDeviceFamily.class^
	com\artifex\gsjava\devices\PNGDevice.class^
	com\artifex\gsjava\devices\PNMDevice.class^
	com\artifex\gsjava\devices\PostScriptDevice.class^
	com\artifex\gsjava\devices\PSDDevice.class^
	com\artifex\gsjava\devices\PXLDevice.class^
	com\artifex\gsjava\devices\TextDevice.class^
	com\artifex\gsjava\devices\TIFFDevice.class^
	com\artifex\gsjava\devices\XPSDevice.class

cd ..