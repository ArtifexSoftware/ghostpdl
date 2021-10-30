This directory contains files which demo multithreading in Ghostscript
using the Java Language Bindings. The application here simply reads in
the same EPS file and outputs it to multiple PDF files on different
threads.

What is here?

* Main.java - Class containing Java main method
* Worker.java - Class which handles concurrently running Ghostscript
* build.bat - Builds the Java program
* runmtd.bat - Starts the Java program

Needed in this directory to run:

* gpdldll64.dll (COMPILED FOR MULTITHREADING - use DGS_THREADSAFE)
* gs_jni.dll
* gsjava.jar

How to run:

Windows:

The application can be started through the command line through the
batch file "runmtd.bat". The batch file takes an optional command
line argument specifying the number of threads to be created. If
specified, it should be an integer grater than 0.