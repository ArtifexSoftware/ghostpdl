This directory contains files which demo multithreading in Ghostscript
using the Java Language Bindings. The application here simply reads in
the same EPS file and outputs it to multiple PDF files on different
threads.

What is here?

* Main.java - Class containing Java main method
* Worker.java - Class which handles concurrently running Ghostscript
* build_win32.bat - Builds the Java program for Windows
* runmtd_win32.bat - Starts the Java program for Windows


Building:

Windows:

* Ensure the gsjava project has been built (..\gsjava\build_win32.bat)
* Run build_win32.bat


Running:

Windows:

Needed in this directory to run:

* gpdldll64.dll (COMPILED FOR MULTITHREADING - use DGS_THREADSAFE in psi\msvc.mak)
* gs_jni.dll (COMPILED FOR MULTITHREADING - enable GSJNI_SUPPORT_MT in settings.h)
* gsjava.jar

The application can be started through the command line through the
batch file "runmtd_win32.bat". The batch file takes an optional command
line argument specifying the number of threads to be created. If
specified, it should be an integer greater than 0.