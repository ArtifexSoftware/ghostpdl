        Building GhostPDL/Ghostscript using Visual Studio
        =================================================

This directory contains a simple Visual Studio 2005 Solution file
with projects for language_switch, pcl, svg and xps. A project for
ghostscript exists in the gs directory, which this relies on. It is
intended to be useful for Windows based developers who are familiar with
the Visual Studio IDE and who want to limit their exposure to the
traditional makefile based approach that ghostscript building is based
on.

This solution is currently a work in progress - feedback (both of success
and failures) and suggested improvements are welcome.


How to build GhostPDL with VS2005
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Load VS2005.
  File => Open Project/Solution...
  Navigate to win32 and select GhostPDL.sln
  Select Release or Debug as appropriate.
  Build => Build Solution.


How to build just Ghostscript with VS2005
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

  Load VS2005.
  File => Open Project/Solution...
  Navigate to win32 and select GhostPDL.sln
  Select Release or Debug as appropriate.
  Build => Build ghostscript.

If you are sure that you will never want to build anything other than
Ghostscript, you can remove the other 4 projects from the solution and
remove any potential confusion between building the solution and building
the project.


How to debug
~~~~~~~~~~~~

Unfortunately, due to the fact that Visual Studio stores the debugging
settings in the user file rather than the project file, we cannot set
the project up to debug within the IDE by default. The changes required
to make this work are fairly simple though.

  Load VS2005.
  File => Open Project/Solution...
  Navigate to win32 and select GhostPDL.sln
  Click the right hand mouse button on 'ghostscript' in Solution Explorer.
  From the menu choose Properties.
  Choose Configuration: Debug in the top left corner of the window.
  In Configuration Properties -> Debugging, edit the options as follows:
  
      Command              ..\gs\debugbin\gswin32c.exe
      Command Arguments    -r72 gs\examples\tiger.eps
      Working Directory    ..
  
  Choose Apply.
  Now Choose Configuration: Release
  In Configuration Properties -> Debugging, edit the options as follows:
  
      Command              ..\gs\bin\gswin32c.exe
      Command Arguments    -r72 gs\examples\tiger.eps
      Working Directory    ..
  
  Select OK.
  
This will have set the debugger up for operating on ghostscript. Clearly
you can customise the arguments as required for your own work.

Once these modifications are done, just Debug => Start Debugging to debug
in the usual way. Breakpoints etc should behave as in any other solution.

Similar steps are required for each of the other projects. Example
settings are as follows:

  language_switch
    Debug
      Cmd     ..\language_switch\debugobj\pspcl6.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp gs\examples\tiger.eps
      Dir     ..
    Release
      Cmd     ..\language_switch\obj\pspcl6.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp gs\examples\tiger.eps
      Dir     ..
  pcl
    Debug
      Cmd     ..\main\debugobj\pcl6.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.px3
      Dir     ..
    Release
      Cmd     ..\main\obj\pcl6.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.px3
      Dir     ..
  svg
    Debug
      Cmd     ..\svg\debugobj\gsvg.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.svg
      Dir     ..
    Release
      Cmd     ..\svg\obj\gsvg.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.svg
      Dir     ..
  xps
    Debug
      Cmd     ..\xps\debugobj\gxps.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.xps
      Dir     ..
    Release
      Cmd     ..\xps\obj\gxps.exe
      Args    -r72 -sDEVICE=bmp16m -sOutputFile=out.bmp tools\tiger.xps
      Dir     ..

To start one of these debugging, either reset the active project, or
start the program by right hand clicking on the appropriate project and
choosing "Debug => Start New Instance".


Other projects
~~~~~~~~~~~~~~

By default this solution just builds the standard GhostPDL set of targets.
We anticipate offering more project files that can be included by
customers wanting to use some of the optional capabilities of the GhostPDL
suite. These projects are not included by default as they require extra
libraries that are not supplied as part of the standard distribution.

Each of these can be added to the solution by choosing File => Add =>
Existing Project... and navigating to the appropriate vcproj file.

The first of these is now available, gs\ghostscript-ufst.vcproj. This
project file allows customers to build a version of Ghostscript that uses
the UFST font library. By default the project expects the UFST to be
checked out and built in C:\ufst (using the project files in
C:\ufst\demo\bin).

This location can be changed if required by editing the value of UFST_ROOT
set in each of the nmake invocation lines in the debugging section of the
project properties for ghostscript-ufst.vcproj. If the path is changed the
debugging runtime arguments below will need to be adjusted accordingly.

Example debugging arguments for this project are as follows:

  ghostscript-ufst
    Debug
      Cmd     ..\gs\ufstdebugbin\gswin32c.exe
      Args    -dUFST_SSDir=(/ufst/fontdata/mtfonts/pclps/mt3) -dUFST_Plugin=(/ufst/fontdata/mtfonts/pclps2/mt3/pclp2_xj.fco) gs/examples/alphabet.ps
      Dir     ..


Things to know about this solution
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This solution is NOT a typical Visual Studio file, in that the solution/
project files call out to GhostPDLs/Ghostscripts standard makefile based
build system. This means that adding new files still requires some tweaking
within the makefiles. We hope that future versions of this solution may
lift that restriction.

Nonetheless, this solution should allow you to get most of the benefits of
a typical solution; familiar IDE, the ability to 'browse' the source files
as usual, interactive debugging etc. If anything doesn't work the way you
expect, please let us know.


How to use later versions of Visual Studio
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Later versions of Visual Studio should import the Solution/Project files
without any problems. When you first open the file, a wizard should allow
you to convert to the latest version. No manual editing should be required.

At the time of writing VS2010 has just been released; it appears to import
and build the project files quite happily (with a few warnings that can
safely be ignored). The one problem is with the 'Clean' build options; due
to a bug in the initial release of VS2010 the environment is not set up
correctly, so nmake cannot be found. Microsoft have acknowledged the bug
and stated that it will be fixed in the next release (presumably the next
service pack).

There are 3 possible workarounds:

 * Most people use 'Clean' just so they can then do 'Build' and get a
   clean build. Instead use 'Rebuild', which does exactly that but in
   one step.

 * Copy nmake.exe to somewhere on your PATH. It can be found in:
 
    C:\Program Files\Microsoft Visual Studio 10.0\VC\bin

   (or alternatively, set that directory to be on your PATH, though beware
   this hasn't been tested and may cause problems with other compilation
   environments).

 * Change the project 'Clean' command lines to include an explicit path
   for nmake.
