#!/bin/bash
# Build for gsmono gtk sample viewer
# Note first do "make so" in ghostpdl
# root directory to create libgpdl.so

cp ../../../../sobin/libgpdl.so bin/libgpdl.so
mcs  -define:MONO -pkg:gtk-sharp-2.0 \
	src/DocPage.cs \
	src/gsOutput.cs \
	src/MainRender.cs \
	src/MainThumbRendering.cs \
	src/MainWindow.cs \
	src/MainZoom.cs \
	src/Program.cs \
	src/TempFile.cs \
	../../api/ghostapi.cs \
	../../api/ghostmono.cs \
	-out:bin/gsMonoGtkViewer.exe
