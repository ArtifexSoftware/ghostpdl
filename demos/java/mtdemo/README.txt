This directory contains files which demo multithreading in Ghostscript
using the Java Language Bindings. The application here simply reads in
the same EPS file and outputs it to multiple PDF files on different
threads.

What is here?

* Main.java - Class containing Java main method
* Worker.java - Class which handles concurrently running Ghostscript
* build.bat - Builds the Java program

Needed in this directory to run:

* gpdldll64.dll (COMPILED FOR MULTITHREADING - use DGS_THREADSAFE)
* gs_jni.dll
* gsjava.jar

How to run:

Windows:

The application can be started through the command line through the
command:

java -cp gsjava.jar;. Main

OR

java -cp gsjava.jar;. Main [thread count]

where [thread count] is the number of threads (and files) which will
be ran/written.