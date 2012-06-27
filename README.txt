== GhostPDL ==

This is an implemenation of several page description languages on top
of the Ghostscript graphics library.  Currently the languages PCL,
PCLXL, and XPS are production quality implementations and SVG is under
development and experimental.

Ghostscript PCL5e, PCL5c and PXL documentation is in doc/ghostpcl.*
License is in LICENSE and COPYING. For information on support and
commercial distribution please contact http://artifex.com/

=== BUILD INSTRUCTIONS ===

Assuming the GNU toolchain (make, gcc, autoconf etc.) is installed the
the languages can be built with:

./configure
make

or if you have checked the code out from the source code repository
and are not using an official release:

./autogen.sh
./configure
make

in the top level source directory. This will create four different
executables: main/obj/pcl6, a build of GhostPCL with support for
PCL5e, PCL5c, and PXL, language_switch/obj/pspcl6 a build of GhostPCL
with additional support for PostScript and PDF, xps/obj/gxps a build
with support for XPS and finally svg/obj/gsvg the SVG implementation.

===

The same executables can be built with Microsoft Visual C by running
the appropriate *_mvsc.mak file through nmake. For example:

cd xps
nmake -f xps_msvc.mak

or

cd main
nmake -f pcl6_msvc.mak

The preferred method to build on Windows is to use the project files
in the win32 subdirectory.  See the ReadMe.txt file in that directory
for further instructions.

===

To build just the xps interpreter with GNU make and gcc, type:

make xps

The executable will be xps/obj/gxps.

To build a shared language build with PCL/PXL and PostScript/PDF

make ls-product

The executable with be in language_switch/obj/pspcl6

Other targets are provided see the top level Makefile.

===

Note for cygwin users

make CYGWIN=TRUE
make install

The CYGWIN variable will not include devices that depend on threads or
X11, neither are supported by the default cygwin system.

To build with a different font scaler use:

make PL_SCALER=afs ... # build using the Artifex Font scaler also the default.
make PL_SCALER=ufst ...# build with the AGFA Font Scaler.

We recommend doing a clean make prior to the changing the font scaler.

The default location for the fonts is /windows/fonts, but the current
working directory is also searched. If the fonts are placed in another
location the environment variable PCLFONTSOURCE must be set accordingly.
See the documentation for more details.

=== REQUIRED FONTS ===

The PCL interpreter requires access to the base 80 font set for proper
operation. Unfortunately this set isn't available under a free software
license, and may only be used and distributed under the AFPL license,
which forbids commercial use, or under a commercial agreement with
Artifex.

If your copy of GhostPDL includes these fonts, they should be available
in the 'urwfonts' directory. If not, they can be downloaded separately
from http://artifex.com/downloads/ just look for the newest set of 80
TrueType fonts.

