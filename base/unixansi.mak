# Copyright (C) 2001-2025 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#
# makefile for Unix/ANSI C/X11 configuration.

# ------------------------------- Options ------------------------------- #

####### The following are the only parts of the file you should need to edit.

# Define the directory for the final executable, and the
# source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

BINDIR=./$(BUILDDIRPREFIX)bin
GLSRCDIR=./base
DEVSRCDIR=./devices
GLGENDIR=./$(BUILDDIRPREFIX)obj
GLOBJDIR=./$(BUILDDIRPREFIX)obj
DEVGENDIR=./$(BUILDDIRPREFIX)obj
DEVOBJDIR=./$(BUILDDIRPREFIX)obj
AUXDIR=$(GLGENDIR)/aux
PSSRCDIR=./psi
PSLIBDIR=./lib
PSRESDIR=./Resource
PSGENDIR=./$(BUILDDIRPREFIX)obj
PSOBJDIR=./$(BUILDDIRPREFIX)obj

# Do not edit the next group of lines.

#include $(COMMONDIR)/ansidefs.mak
#include $(COMMONDIR)/unixdefs.mak
#include $(COMMONDIR)/generic.mak
include $(GLSRCDIR)/version.mak
DD=$(GLGENDIR)/
GLD=$(GLGENDIR)/
PSD=$(PSGENDIR)/

# ------ Generic options ------ #

# Define the installation commands and target directories for
# executables and files.  The commands are only relevant to `make install';
# the directories also define the default search path for the
# initialization files (gs_*.ps) and the fonts.

INSTALL = $(GLSRCDIR)/instcopy -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
scriptdir = $(bindir)
mandir = $(prefix)/man
man1ext = 1
datadir = $(prefix)/share
gsdir = $(datadir)/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)

docdir=$(gsdatadir)/doc
exdir=$(gsdatadir)/examples
GS_DOCDIR=$(docdir)

# Define the default directory/ies for the runtime initialization, resource and
# font files.  Separate multiple directories with a :.

GS_LIB_DEFAULT=$(gsdatadir)/Resource/Init:$(gsdatadir)/lib:$(gsdatadir)/Resource/Font:$(gsdir)/fonts

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems,  but may be convenient sometimes.

SEARCH_HERE_FIRST=0

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# -DDEBUG
#	includes debugging features (-Z switch) in the code.
#	  Code runs substantially slower even if no debugging switches
#	  are set.

GENOPT=

# Define the name of the executable file.

GS=gs

# Define the directories for debugging and profiling binaries, relative to
# the standard binaries.

DEBUGDIRPREFIX=debug
MEMENTODIRPREFIX=mem
PGDIRPREFIX=pg

# Define whether to compile in the FreeType library, and if so, where
# the source tree is location. Otherwise, what library name to use
# in linking to a shared implementation.

FT_BRIDGE=1
SHARE_FT=0
FTSRCDIR=freetype
FT_CFLAGS=-Ifreetype/include
FT_LIBS=

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=jpeg

# Note: if a shared library is used, it may not contain the
# D_MAX_BLOCKS_IN_MCU patch, and thus may not be able to read
# some older JPEG streams that violate the standard. If the JPEG
# library built from local sources, the patch will be applied.

SHARE_JPEG=0
JPEG_NAME=jpeg

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See png.mak for more information.

PNGSRCDIR=libpng

# Choose whether to use a shared version of the PNG library, and if so,
# what its name is.
# See gs.mak and Make.htm for more information.

SHARE_LIBPNG=0
LIBPNG_NAME=png

# Define whether to use a shared version of libtiff and where
# it is stored and what its name is.

SHARE_LIBTIFF=0
TIFFSRCDIR=tiff
TIFFCONFDIR=tiff
TIFFPLATFORM=unix
TIFFCONFIG_SUFFIX=.unix
LIBTIFF_NAME=tiff

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

ZSRCDIR=zlib

# Choose whether to use a shared version of the zlib library, and if so,
# what its name is (usually libz, but sometimes libgz).
# See gs.mak and Make.htm for more information.

SHARE_ZLIB=0
#ZLIB_NAME=gz
ZLIB_NAME=z

# Define the directory where the brotli sources are stored.
# See brotli.mak for more information.

BROTLISRCDIR=zlib

# Choose whether to use a shared version of the brotli library, and if so,
# what its name is.
# See gs.mak and Make.htm for more information.

SHARE_BROTLI=0
BROTLI_NAME=brotli

# Choose shared or compiled in libjbig2dec and source location
SHARE_JBIG2=0
JBIG2_LIB=jbig2dec
JBIG2SRCDIR=jbig2dec

# Define the directory where the lcms source is stored.
# See lcms.mak for more information

SHARE_LCMS=0
LCMS2MTSRCDIR=lcms2mt

# Define the directory where the lcms2 source is stored.
# See lcms2.mak for more information

LCMS2SRCDIR=lcms2

# Which CMS are we using?
# Options are currently lcms2mt or lcms2

WHICH_CMS=lcms2mt

# Define the directory where the ijs source is stored,
# and the process forking method to use for the server.
# See ijs.mak for more information.

SHARE_IJS=0
IJS_NAME=
IJSSRCDIR=ijs
IJSEXECTYPE=unix

# Define how to build the library archives.  (These are not used in any
# standard configuration.)

AR=ar
ARFLAGS=qc
RANLIB=ranlib

# ------ Platform-specific options ------ #

# Define the name of the C compiler.  If the standard compiler for your
# platform is ANSI-compatible, leave this line commented out; if not,
# uncomment the line and insert the proper definition.

#CC=some_C_compiler

# Define the name of the linker for the final link step.
# Normally this is the same as the C compiler.

CCLD=$(CC)

# Define the added flags for standard, debugging, and profiling builds.

CFLAGS_STANDARD=-O -DNDEBUG
CFLAGS_DEBUG=-g
CFLAGS_PROFILE=-pg -O -DNDEBUG

# Define the other compilation flags.  Add at most one of the following:
#	-Aa -w -D_HPUX_SOURCE for the HP 400.
#	-DBSD4_2 for 4.2bsd systems.
#	-DSYSV for System V or DG/UX.
#	-DSVR4 -DSVR4_0 (not -DSYSV) for System V release 4.0.
#	-DSVR4 (not -DSYSV) for System V release 4.2 (or later) and Solaris 2.
# XCFLAGS can be set from the command line.
XCFLAGS=

CFLAGS=$(CFLAGS_STANDARD) $(XCFLAGS)

# Define platform flags for ld.
# SunOS and some others want -X; Ultrix wants -x.
# SunOS 4.n may need -Bstatic.
# Solaris 2.6 (and possibly some other versions) with any of the SHARE_
# parameters set to 1 may need
#	-R /usr/local/xxx/lib:/usr/local/lib
# giving the full path names of the shared library directories.
# Apollos running DomainOS don't support -X (and -x has no effect).
# XLDFLAGS can be set from the command line.
XLDFLAGS=

LDFLAGS=$(XLDFLAGS)

GS_LDFLAGS=$(LDFLAGS)
PCL_LDFLAGS=$(LDFLAGS)
XPS_LDFLAGS=$(LDFLAGS)

# Define any extra libraries to link into the executable.
# ISC Unix 2.2 wants -linet.
# SCO Unix needs -lsocket if you aren't including the X11 driver.
# SVR4 may need -lnsl.
# Solaris may need -lnsl -lsocket -lposix4.
# (Libraries required by individual drivers are handled automatically.)

EXTRALIBS=

# Define the standard libraries to search at the end of linking.
# Most platforms require -lpthread for the POSIX threads library;
# on FreeBSD, change -lpthread to -lc_r; BSDI and perhaps some others
# include pthreads in libc and don't require any additional library.
# All reasonable platforms require -lm, but Rhapsody and perhaps one or
# two others fold libm into libc and don't require any additional library.

#STDLIBS=-lpthread -lm

# Since the default build is for nosync, don't include pthread lib
STDLIBS=-lm

# Define the include switch(es) for the X11 header files.
# This can be null if handled in some other way (e.g., the files are
# in /usr/include, or the directory is supplied by an environment variable);
# in particular, SCO Xenix, Unix, and ODT just want
#XINCLUDE=
# Note that x_.h expects to find the header files in $(XINCLUDE)/X11,
# not in $(XINCLUDE).

XINCLUDE=-I/usr/X11R6/include

# Define the directory/ies and library names for the X11 library files.
# XLIBDIRS is for ld and should include -L; XLIBDIR is for LD_RUN_PATH
# (dynamic libraries on SVR4) and should not include -L.
# Newer SVR4 systems can use -R in XLIBDIRS rather than setting XLIBDIR.
# Both can be null if these files are in the default linker search path;
# in particular, SCO Xenix, Unix, and ODT just want
#XLIBDIRS=
# Solaris and other SVR4 systems with dynamic linking probably want
#XLIBDIRS=-L/usr/openwin/lib -R/usr/openwin/lib
# X11R6 (on any platform) may need
#XLIBS=Xt SM ICE Xext X11

XLIBDIRS=-L/usr/X11R6/lib
XLIBDIR=
XLIBS=Xt Xext X11

# Define the .dev module that implements thread and synchronization
# primitives for this platform.  Don't change this unless you really know
# what you're doing.

# If POSIX sync primitives are used, also change the STDLIBS to include
# the pthread library.
#SYNC=posync

# Default is No sync primitives since some platforms don't have it (HP-UX)
SYNC=nosync

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)ttfont.dev $(PSD)epsf.dev $(GLD)pipe.dev $(PSD)fapi.dev

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

COMPILE_INITS=1

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

BAND_LIST_STORAGE=file

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.

BAND_LIST_COMPRESSOR=zlib

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

FILE_IMPLEMENTATION=stdio

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

DEVICE_DEVS=$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev $(DD)x11gray2.dev $(DD)x11gray4.dev $(DD)x11mono.dev
DEVICE_DEVS1=
DEVICE_DEVS2=
DEVICE_DEVS3=$(DD)deskjet.dev $(DD)djet500.dev $(DD)laserjet.dev $(DD)ljetplus.dev $(DD)ljet2p.dev $(DD)ljet3.dev $(DD)ljet3d.dev $(DD)ljet4.dev $(DD)ljet4d.dev
DEVICE_DEVS4=$(DD)cdeskjet.dev $(DD)cdjcolor.dev $(DD)cdjmono.dev $(DD)cdj550.dev $(DD)pj.dev $(DD)pjxl.dev $(DD)pjxl300.dev
DEVICE_DEVS5=$(DD)uniprint.dev
DEVICE_DEVS6=$(DD)bj10e.dev $(DD)bj200.dev $(DD)bjc600.dev $(DD)bjc800.dev
DEVICE_DEVS7=$(DD)faxg3.dev $(DD)faxg32d.dev $(DD)faxg4.dev
DEVICE_DEVS8=$(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcx16.dev $(DD)pcx256.dev $(DD)pcx24b.dev $(DD)pcxcmyk.dev
DEVICE_DEVS9=$(DD)pbm.dev $(DD)pbmraw.dev $(DD)pgm.dev $(DD)pgmraw.dev $(DD)pgnm.dev $(DD)pgnmraw.dev
DEVICE_DEVS10=$(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev $(DD)tifflzw.dev $(DD)tiffpack.dev
DEVICE_DEVS11=$(DD)tiff12nc.dev $(DD)tiff24nc.dev $(DD)tiffgray.dev $(DD)tiff32nc.dev $(DD)tiffsep.dev $(DD)tiffsep1.dev $(DD)tiffscaled.dev $(DD)tiffscaled8.dev $(DD)tiffscaled24.dev $(DD)tiffscaled32.dev $(DD)tiffscaled4.dev
DEVICE_DEVS12=$(DD)bit.dev $(DD)bitrgb.dev $(DD)bitcmyk.dev
DEVICE_DEVS13=$(DD)pngmono.dev $(DD)pngmonod.dev $(DD)pnggray.dev $(DD)png16.dev $(DD)png256.dev $(DD)png16m.dev $(DD)pngalpha.dev $(DD)png16malpha.dev
DEVICE_DEVS14=$(DD)jpeg.dev $(DD)jpeggray.dev $(DD)jpegcmyk.dev
DEVICE_DEVS15=$(DD)pdfwrite.dev $(DD)ps2write.dev $(DD)eps2write.dev $(DD)txtwrite.dev $(DD)pxlmono.dev $(DD)pxlcolor.dev
DEVICE_DEVS16=$(DD)bbox.dev $(DD)inkcov.dev $(DD)ink_cov.dev $(DD)pdfimage8.dev $(DD)pdfimage24.dev $(DD)pdfimage32.dev $(DD)PCLm.dev $(DD)PCLm8.dev
# Overflow from DEVS9
DEVICE_DEVS17=$(DD)pnm.dev $(DD)pnmraw.dev $(DD)ppm.dev $(DD)ppmraw.dev $(DD)pkm.dev $(DD)pkmraw.dev $(DD)pksm.dev $(DD)pksmraw.dev $(DD)pamcmyk32.dev
DEVICE_DEVS18=
DEVICE_DEVS19=
DEVICE_DEVS20=
DEVICE_DEVS21=$(DD)spotcmyk.dev $(DD)devicen.dev $(DD)xcf.dev $(DD)bmpsep1.dev $(DD)bmpsep8.dev $(DD)bmp16m.dev $(DD)bmp32b.dev $(DD)psdcmyk.dev $(DD)psdrgb.dev $(DD)pamcmyk32.dev $(DD)psdcmykog.dev $(DD)fpng.dev  $(DD)psdcmyk16.dev $(DD)psdrgb16.dev $(DD)psdcmyktags16.dev $(DD)psdcmyktags.dev

# ---------------------------- End of options --------------------------- #

# Define the name of the partial makefile that specifies options --
# used in dependencies.

MAKEFILE=$(GLSRCDIR)/unixansi.mak
TOP_MAKEFILES=$(MAKEFILE) $(GLSRCDIR)/unixhead.mak

# Define the auxilary program dependency.

AK=

# Define the compilation rules and flags.

# If you system has a 64 bit type you should pass it through
# CCFLAGS to improve support for multiple colorants. e.g.:
#     -DGX_COLOR_INDEX_TYPE='unsigned long long'
# or use the autoconf build, which sets this automatically.
# If you do not define a 64 bit type, there may be some warnings
# about oversize shifts. It's a bug if these are not harmless.

CCFLAGS=$(GENOPT) $(CFLAGS)
CC_=$(CC) $(CCFLAGS)
CCAUX=$(CC)
CC_NO_WARN=$(CC_)
CC_SHARED=$(CC_)

# ---------------- End of platform-specific section ---------------- #

include $(GLSRCDIR)/unixhead.mak
include $(GLSRCDIR)/gs.mak
# psromfs.mak must precede lib.mak
include $(PSSRCDIR)/psromfs.mak
include $(GLSRCDIR)/lib.mak
include $(PSSRCDIR)/int.mak
include $(GLSRCDIR)/freetype.mak
include $(GLSRCDIR)/jpeg.mak
include $(GLSRCDIR)/brotli.mak
# zlib.mak must precede png.mak
include $(GLSRCDIR)/zlib.mak
include $(GLSRCDIR)/png.mak
include $(GLSRCDIR)/jbig2.mak
include $(GLSRCDIR)/tiff.mak
include $(GLSRCDIR)/lcms.mak
include $(GLSRCDIR)/ijs.mak
include $(GLSRCDIR)/devs.mak
include $(GLSRCDIR)/contrib.mak
include $(GLSRCDIR)/unix-aux.mak
include $(GLSRCDIR)/unixlink.mak
include $(GLSRCDIR)/unix-end.mak
include $(GLSRCDIR)/unixinst.mak

# platform-specific clean-up
# this makefile is intended to be hand edited so we don't distribute
# the (presumedly modified) version in the top level directory
distclean : clean config-clean
	-$(RM) Makefile
	@-rmdir $(BINDIR) $(GLOBJDIR) $(PSOBJDIR)

maintainer-clean : distclean
	# nothing special to do
