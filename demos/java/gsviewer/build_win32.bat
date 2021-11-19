@echo off

echo Build gsjava

cd "..\gsjava"

call build_win32

cd "..\gsviewer"

copy "..\gsjava\gsjava.jar" ".\gsjava.jar"

echo Build gsviewer

if not exist bin mkdir bin

echo Compiling Java source...
javac -sourcepath src\ -d bin\^
	-classpath "..\gsjava\bin"^
	"src\com\artifex\gsviewer\DefaultUnhandledExceptionHandler.java"^
	"src\com\artifex\gsviewer\Page.java" ^
	"src\com\artifex\gsviewer\Settings.java" ^
	"src\com\artifex\gsviewer\Document.java" ^
	"src\com\artifex\gsviewer\ImageUtil.java" ^
	"src\com\artifex\gsviewer\PageUpdateCallback.java" ^
	"src\com\artifex\gsviewer\StdIO.java" ^
	"src\com\artifex\gsviewer\GSFileFilter.java" ^
	"src\com\artifex\gsviewer\Main.java" ^
	"src\com\artifex\gsviewer\PDFFileFilter.java" ^
	"src\com\artifex\gsviewer\ViewerController.java" ^
	^
	"src\com\artifex\gsviewer\gui\ScrollMap.java" ^
	"src\com\artifex\gsviewer\gui\SettingsDialog.java" ^
	"src\com\artifex\gsviewer\gui\ViewerGUIListener.java" ^
	"src\com\artifex\gsviewer\gui\ViewerWindow.java"

cd bin

echo Packing JAR file...
jar cfm "..\gsviewer.jar" "..\Manifest.md" com\

cd..