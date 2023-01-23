The following projects show the use of the Ghostscript API
in a C# environment.  A WPF C# Windows viewer application is
contained in the windows folder.  A MONO C# Gtk viewer 
application is contained in the Linux folder.

The applications share the same API file to Ghostscript or GhostPDL
which is in api/ghostapi.cs.  In this file, lib_dll is set to the appropriate
dll or so file that is created by the compilation of Ghostscript. Note
that to build libgpdl.so on Linux you use "make so".  On windows gpdldll64.dll
and variants are built depending upon the VS solution configurations.

The applications each have another level of interface which is api/ghostnet.cs
for the Windows application and api/ghostmono.cs for the Linux application.
These files assemble the commands that are to be executed and create the working
threads that GhostPDL/Ghostscript will run on as well as handling the call backs
from GhostPDL/Ghostscript.  The Linux and Windows applications use different
threading methods.

The Windows application includes the ability to print the document via
the Windows XPS print pipeline.  The application will call into GhostPDL/Ghostscript
with the xpswrite device to create the XPS content.

Both applications will offer the user the chance to distill any non-PDF files
that are opened with the application.

Both applications should at some point be slightly reworked to provide improved
performance on rendering only the visible pages when PDF is the document source.
PCL and PS file formats are streamed and so cannot work in this manner without
a severe performance penalty.

The Linux application is built using the make.sh bash script.

The windows project includes the viewer application as well as a simple
console application that lets you type in gs command lines and provides
timing information after running the command syncrounsly.  This application
is configured to build only with x64 and ghostscript DLL.


