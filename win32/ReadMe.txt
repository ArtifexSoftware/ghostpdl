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
