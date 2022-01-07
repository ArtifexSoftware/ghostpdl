This directory contains files which demo multithreading in Ghostscript
using the Java Language Bindings. The application here simply reads in
the same EPS file and outputs it to multiple PDF files on different
threads.

What is here?

* Main.java - Class containing Java main method
* Worker.java - Class which handles concurrently running Ghostscript
* build_darwin.sh - Builds the Java program on Darwin systems
* build_linux.sh - Builds the Java program on Linux systems
* build_win32.bat - Builds the Java program on Windows
* runmtd_darwin.sh - Runs the Java program on Darwin systems
* runmtd_linux.sh - Runs the Java program on Linux systems
* runmtd_win32.bat - Starts the Java program for Windows

Build/run instructions:

-= WINDOWS =-

1. Ensure the following libraries have been built and are in
   this directory:
	* gpdldll64.dll
	* gs_jni.dll

2. Run build_win32.bat to build.

3. Run runmtd_win32.bat to start the application.


-= LINUX =-

1. Ensure the following libraries have been built and are in
   this directory:
	* libgpdl.so (this would have been built as a link to another file, so
	  it should be copied into this directory and renamed to libgpdl.so)
	  gs_jni.so

2. If using OpenJDK 8, the property "assistive_technologies" may
   need to be modified for the Java code to build. It can be modified by
   editing the "accessibility.properties" file. This is located at:

   /etc/java-8-openjdk/accessibility.properties

3. Run build_linux.sh to build and copy libraries.

5. Run runmtd_linux.sh to start the application.


-= DARWIN =-

Same as Linux, except with .dylib extensions on all shared objects and
"_darwin.sh" suffixes instead of "_linux.sh".