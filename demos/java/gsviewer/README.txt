This project is a viewer designed to load PDF documents,
but will also view other file types (like PostScript, XPS or PCL)
by distilling them into PDF documents first.

When the document is first loaded, every page is rendered at
a low resolution. These will be displayed as thumbnails on the
left side. All loading is done in the com.artifex.gsjava.Document
class. As the user changes the position of the viewport, the
viewer will dynamically load high resolution images based on
what page the user is viewing. This is done in the
com.artifex.gsviewer.ViewerController.SmartLoader class. This
also handles rendering zoomed images if necessary. Only pages
which need to be loaded at a high resolution are loaded, and
zoomed pages are unloaded when no longer necessary to save memory.

Required libraries in this directory for the viewer to work:

-= WINDOWS =-

gpdldll64.dll
gs_jni.dll


-= LINUX =-

libgpdl.so (this would have been built as a link to another file, so
it should be copied into this directory and renamed to libgpdl.so)
gs_jni.so

On Linux, when using OpenJDK, the property "assistive_technologies" may
need to be modified for the Java code to build. It can be modified by
editing the "accessibility.properties" file. This is located at:

/etc/java-8-openjdk/accessibility.properties

Additionally, to start the application, start_linux.sh should be executed as it
sets up the shared library paths.


-= MAC =-

Same as Linux, except with .dylib extensions on all shared objects.

start_darwin.sh should be used to start the application.


Building:

-= WINDOWS =-

Ensure both gs_jni.dll and gpdldll64.dll. Then, run the build_win32.bat script.
This will automatically build and copy gsjava.jar to this directory. To run,
open gsviewer.jar either through File Explorer or in the command line through
the following command:

java -jar gsviewer.jar

-= LINUX =-

Ensure Ghostscript has been built.
Run the build_linux.sh script. This will automatically build
gs_jni.so, gsjava.jar, and copy the files to the needed
directories. gsviewer.jar will be outputted in this directory.

-= MAC =-

Ensure Ghostscript has been built.
Run the build_darwin.sh script. This will automatically build
gs_jni.dylib, gsjava.jar, and copy the files to the needed
directories. gsviewer.jar will be outputed in this directory.
