#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

# $Id$
# makefile for Unix/gcc/X11 configuration.

# ------------------------------- Options ------------------------------- #

####### The following are the only parts of the file you should need to edit.

# Define the source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

GLSRCDIR=.
GLGENDIR=./obj
GLOBJDIR=./obj
PSSRCDIR=.
PSGENDIR=./obj
PSOBJDIR=./obj

# Do not edit the next group of lines.

#include $(COMMONDIR)/gccdefs.mak
#include $(COMMONDIR)/unixdefs.mak
#include $(COMMONDIR)/generic.mak
GLSRC=$(GLSRCDIR)/
include $(GLSRC)version.mak
PSSRC=$(PSSRCDIR)/

# ------ Generic options ------ #

# Define the installation commands and target directories for
# executables and files.  The commands are only relevant to `make install';
# the directories also define the default search path for the
# initialization files (gs_*.ps) and the fonts.

# If your system has installbsd, change install to installbsd in the next line.
INSTALL = install -c
INSTALL_PROGRAM = $(INSTALL) -m 755
INSTALL_DATA = $(INSTALL) -m 644

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
scriptdir = $(bindir)
mandir = $(prefix)/man
man1ext = 1
man1dir = $(mandir)/man$(man1ext)
datadir = $(prefix)/share
gsdir = $(datadir)/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)

docdir=$(gsdatadir)/doc
exdir=$(gsdatadir)/examples
GS_DOCDIR=$(docdir)

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with a :.

GS_LIB_DEFAULT=$(gsdatadir):$(gsdir)/fonts

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems, but users insist on it.
# NOTE: this also affects searching for files named on the command line:
# see the "File searching" section of use.txt for full details.
# Because of this, setting SEARCH_HERE_FIRST to 0 is not recommended.

SEARCH_HERE_FIRST=1

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# -DDEBUG
#	includes debugging features (-Z switch) in the code.
#	  Code runs substantially slower even if no debugging switches
#	  are set.
# -DNOPRIVATE
#	makes private (static) procedures and variables public,
#	  so they are visible to the debugger and profiler.
#	  No execution time or space penalty.

#GENOPT=-DDEBUG
GENOPT=

# Define the name of the executable file.

GS=gs

# Define the directories for debugging and profiling binaries, relative to
# the standard binaries.

DEBUGRELDIR=../debugobj
PGRELDIR=../pgobj

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=jpeg
JVERSION=6

# Choose whether to use a shared version of the IJG JPEG library (-ljpeg).
# DON'T DO THIS. If you do, the resulting executable will not be able to
# read some PostScript files containing JPEG data, because Adobe chose to
# define PostScript's JPEG capabilities in a way that is slightly
# incompatible with the JPEG standard.  See make.txt for more details.

# DON'T SET THIS TO 1!  See the comment just above.
SHARE_JPEG=0
JPEG_NAME=jpeg

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See libpng.mak for more information.

PSRCDIR=libpng
PVERSION=96

# Choose whether to use a shared version of the PNG library, and if so,
# what its name is.
# See gs.mak and make.txt for more information.

SHARE_LIBPNG=0
LIBPNG_NAME=png

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

ZSRCDIR=zlib

# Choose whether to use a shared version of the zlib library, and if so,
# what its name is (usually libz, but sometimes libgz).
# See gs.mak and make.txt for more information.

SHARE_ZLIB=0
#ZLIB_NAME=gz
ZLIB_NAME=z

# Define how to build the library archives.  (These are not used in any
# standard configuration.)

AR=ar
ARFLAGS=qc
RANLIB=ranlib

# Define the configuration ID.  Read gs.mak carefully before changing this.

CONFIG=

# ------ Platform-specific options ------ #

# Define the name of the C compiler.

CC=gcc

# Define the name of the linker for the final link step.
# Normally this is the same as the C compiler.

CCLD=$(CC)

# Define the default gcc flags.
# Note that depending whether or not we are running a version of gcc with
# the 2.7.0-2.7.2 optimizer bug, either "-Dconst=" or
# "-Wcast-qual -Wwrite-strings" is automatically included.

GCFLAGS=-Wall -Wcast-align -Wstrict-prototypes -Wwrite-strings -fno-common

# Define the added flags for standard, debugging, and profiling builds.

CFLAGS_STANDARD=-O2
CFLAGS_DEBUG=-g -O
CFLAGS_PROFILE=-pg -O2

# Define the other compilation flags.  Add at most one of the following:
#	-DBSD4_2 for 4.2bsd systems.
#	-DSYSV for System V or DG/UX.
# 	-DSYSV -D__SVR3 for SCO ODT, ISC Unix 2.2 or before,
#	   or any System III Unix, or System V release 3-or-older Unix.
#	-DSVR4 -DSVR4_0 (not -DSYSV) for System V release 4.0.
#	-DSVR4 (not -DSYSV) for System V release 4.2 (or later) and Solaris 2.
# XCFLAGS can be set from the command line.
# We don't include -ansi, because this gets in the way of the platform-
#   specific stuff that <math.h> typically needs; nevertheless, we expect
#   gcc to accept ANSI-style function prototypes and function definitions.
XCFLAGS=

CFLAGS=$(CFLAGS_STANDARD) $(GCFLAGS) $(XCFLAGS)

# Define platform flags for ld.
# SunOS 4.n may need -Bstatic.
# Solaris 2.6 (and possibly some other versions) with any of the SHARE_
# parameters set to 1 may need
#	-R /usr/local/xxx/lib:/usr/local/lib
# giving the full path names of the shared library directories.
# XLDFLAGS can be set from the command line.
XLDFLAGS=

LDFLAGS=$(XLDFLAGS) -fno-common

# Define any extra libraries to link into the executable.
# ISC Unix 2.2 wants -linet.
# SCO Unix needs -lsocket if you aren't including the X11 driver.
# SVR4 may need -lnsl.
# (Libraries required by individual drivers are handled automatically.)

EXTRALIBS=

# Define the include switch(es) for the X11 header files.
# This can be null if handled in some other way (e.g., the files are
# in /usr/include, or the directory is supplied by an environment variable);
# in particular, SCO Xenix, Unix, and ODT just want
#XINCLUDE=
# Note that x_.h expects to find the header files in $(XINCLUDE)/X11,
# not in $(XINCLUDE).

XINCLUDE=-I/usr/local/X/include

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

#XLIBDIRS=-L/usr/local/X/lib
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt Xext X11

# Define whether this platform has floating point hardware:
#	FPU_TYPE=2 means floating point is faster than fixed point.
# (This is the case on some RISCs with multiple instruction dispatch.)
#	FPU_TYPE=1 means floating point is at worst only slightly slower
# than fixed point.
#	FPU_TYPE=0 means that floating point may be considerably slower.
#	FPU_TYPE=-1 means that floating point is always much slower than
# fixed point.

FPU_TYPE=1

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

#FEATURE_DEVS=psl3.dev pdf.dev dpsnext.dev epsf.dev pipe.dev rasterop.dev
FEATURE_DEVS=psl3.dev pdf.dev dpsnext.dev pipe.dev

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

COMPILE_INITS=0

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

BAND_LIST_STORAGE=file

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.  lzw is not recommended, because the
# LZW-compatible code in Ghostscript doesn't actually compress its input.

BAND_LIST_COMPRESSOR=zlib

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

FILE_IMPLEMENTATION=stdio

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.

DEVICE_DEVS=x11.dev x11alpha.dev x11cmyk.dev x11gray2.dev x11mono.dev
#DEVICE_DEVS1=bmpmono.dev bmpamono.dev posync.dev
DEVICE_DEVS1=
DEVICE_DEVS2=
DEVICE_DEVS3=deskjet.dev djet500.dev laserjet.dev ljetplus.dev ljet2p.dev ljet3.dev ljet4.dev
DEVICE_DEVS4=cdeskjet.dev cdjcolor.dev cdjmono.dev cdj550.dev pj.dev pjxl.dev pjxl300.dev
DEVICE_DEVS5=uniprint.dev
DEVICE_DEVS6=bj10e.dev bj200.dev bjc600.dev bjc800.dev
DEVICE_DEVS7=faxg3.dev faxg32d.dev faxg4.dev
DEVICE_DEVS8=pcxmono.dev pcxgray.dev pcx16.dev pcx256.dev pcx24b.dev pcxcmyk.dev
DEVICE_DEVS9=pbm.dev pbmraw.dev pgm.dev pgmraw.dev pgnm.dev pgnmraw.dev pnm.dev pnmraw.dev ppm.dev ppmraw.dev pkm.dev pkmraw.dev
DEVICE_DEVS10=tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev tifflzw.dev tiffpack.dev
DEVICE_DEVS11=tiff12nc.dev tiff24nc.dev
DEVICE_DEVS12=psmono.dev psgray.dev psrgb.dev bit.dev bitrgb.dev bitcmyk.dev
DEVICE_DEVS13=pngmono.dev pnggray.dev png16.dev png256.dev png16m.dev
DEVICE_DEVS14=jpeg.dev jpeggray.dev
DEVICE_DEVS15=pdfwrite.dev pswrite.dev epswrite.dev pxlmono.dev pxlcolor.dev

# ---------------------------- End of options --------------------------- #

# Define the name of the partial makefile that specifies options --
# used in dependencies.

MAKEFILE=$(GLSRC)unix-gcc.mak

# Define the ANSI-to-K&R dependency.  There isn't one, but we do have to
# detect whether we're running a version of gcc with the const optimization
# bug.

AK=$(GLGENDIR)/cc.tr

# Define the compilation rules and flags.

CCFLAGS=$(GENOPT) $(CFLAGS)
CC_=$(CC) `cat $(AK)` $(CCFLAGS)
CCAUX=$(CC)
#We can't use -fomit-frame-pointer with -pg....
#CC_LEAF=$(CC_)
CC_LEAF=$(CC_) -fomit-frame-pointer

# ---------------- End of platform-specific section ---------------- #

include $(GLSRC)unixhead.mak
include $(GLSRC)gs.mak
include $(GLSRC)lib.mak
include $(PSSRC)int.mak
include $(GLSRC)jpeg.mak
# zlib.mak must precede libpng.mak
include $(GLSRC)zlib.mak
include $(GLSRC)libpng.mak
include $(GLSRC)devs.mak
include $(GLSRC)contrib.mak
include $(GLSRC)unixtail.mak
include $(GLSRC)unix-end.mak
include $(GLSRC)unixinst.mak

# This has to come last so it won't be taken as the default target.
$(AK):
	if ( gcc --version | grep "2.7.[01]" >/dev/null || test `gcc --version` = 2.7.2 ); then echo -Dconst= >$(AK); else echo -Wcast-qual -Wwrite-strings >$(AK); fi
