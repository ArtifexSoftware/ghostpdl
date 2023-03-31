# Copyright (C) 2001-2023 Artifex Software, Inc.
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
# makefile template for/from the autoconf build.
# Makefile.  Generated from Makefile.in by configure.

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

PCL5SRCDIR=./pcl/pcl
PCL5GENDIR=./$(BUILDDIRPREFIX)obj
PCL5OBJDIR=./$(BUILDDIRPREFIX)obj

PXLSRCDIR=./pcl/pxl
PXLGENDIR=./$(BUILDDIRPREFIX)obj
PXLOBJDIR=./$(BUILDDIRPREFIX)obj

PLSRCDIR=./pcl/pl
PLGENDIR=./$(BUILDDIRPREFIX)obj
PLOBJDIR=./$(BUILDDIRPREFIX)obj

XPSSRCDIR=./xps
XPSGENDIR=./$(BUILDDIRPREFIX)obj
XPSOBJDIR=./$(BUILDDIRPREFIX)obj

PDFSRCDIR=./pdf
PDFGENDIR=./$(BUILDDIRPREFIX)obj
PDFOBJDIR=./$(BUILDDIRPREFIX)obj

GPDLSRCDIR=./gpdl
GPDLGENDIR=./$(BUILDDIRPREFIX)obj
GPDLOBJDIR=./$(BUILDDIRPREFIX)obj


CONTRIBDIR=./contrib

# Do not edit the next group of lines.

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
INSTALL_SHARED =

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
scriptdir = $(bindir)
includedir = ${prefix}/include
libdir = ${exec_prefix}/lib
mandir = ${datarootdir}/man
man1ext = 1
man1dir = $(mandir)/man$(man1ext)
datadir = ${datarootdir}
datarootdir = ${prefix}/share

# The following must be substituted using ${datarootdir} and ${exec_prefix}/lib
# to avoid adding RPM generation paths (CUPS STR #1112)
gsdir = ${datarootdir}/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)
gssharedir = ${exec_prefix}/lib/ghostscript/$(GS_DOT_VERSION)
gsincludedir = ${prefix}/include/ghostscript/

docdir=$(gsdatadir)/doc
exdir=$(gsdatadir)/examples
GS_DOCDIR=$(docdir)

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

COMPILE_INITS=1

# Define the default directory/ies for the runtime
# initialization and font files.  Separate multiple directories with a :.

GS_LIB_DEFAULT=$(gsdatadir)/Resource/Init:$(gsdatadir)/lib:$(gsdatadir)/Resource/Font:$(gsdir)/fonts:${datarootdir}/fonts/default/ghostscript:${datarootdir}/fonts/default/Type1:${datarootdir}/fonts/default/TrueType:/usr/lib/DPS/outline/base:/usr/openwin/lib/X11/fonts/Type1:/usr/openwin/lib/X11/fonts/TrueType

# Define the default directory for cached data files
# this must be a single path.

GS_CACHE_DIR="~/.ghostscript/cache/"

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems,  but may be convenient sometimes.

SEARCH_HERE_FIRST=0

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

TARGET_ARCH_FILE=

# -DDEBUG
#	includes debugging features (-Z switch) in the code.
#	  Code runs substantially slower even if no debugging switches
#	  are set.

GENOPT=

# Choose capability options.

# -DHAVE_BSWAP32
#       use bswap32 intrinsic
# -DHAVE_BYTESWAP_H
#       use byteswap.h functions
#
# -DHAVE_MKSTEMP
#	uses mkstemp instead of mktemp
#		This uses the more secure temporary file creation call
#		Enable this if it is available on your platform.
# -DHAVE_FILE64
#	use marked versions of the stdio FILE calls, fopen64() et al.
#
# -DHAVE_MKSTEMP64
#	use non-standard function mkstemp64()
#
# -DHAVE_LIBIDN
#	use libidn to canonicalize Unicode passwords
#
# -DHAVE_SETLOCALE
#	call setlocale(LC_CTYPE) when running as a standalone app
# -DHAVE_SSE2
#       use sse2 intrinsics

CAPOPT= -DHAVE_MKSTEMP -DHAVE_FILE64 -DHAVE_FSEEKO -DHAVE_MKSTEMP64   -DHAVE_SETLOCALE -DHAVE_SSE2  -DHAVE_BSWAP32 -DHAVE_BYTESWAP_H -DHAVE_STRERROR -DHAVE_PREAD_PWRITE=1 -DGS_RECURSIVE_MUTEXATTR=PTHREAD_MUTEX_RECURSIVE

# Define the name of the executable file.

GS=gs
GS_SO_BASE=gs

PCL=gpcl6
XPS=gxps
PDF=gpdf
GPDL=gpdl

XE=
XEAUX=

PCL_XPS_PDL_TARGETS=

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
FTSRCDIR=./freetype
FT_CFLAGS=-I./freetype/include
FT_LIBS=

# Define whether to compile in UFST.
# FAPI/UFST depends on UFST_BRIDGE being undefined - hence the construct below.
# (i.e. use "UFST_BRIDGE=1" or *not to define UFST_BRIDGE to anything*)
# As UFST is not used for fonts embedded in input files, we should still have
# Freetype enabled, above.

UFST_ROOT=
UFST_LIB_EXT=

UFST_ROMFS_ARGS?=-b \
 -P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
 -P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
 -c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO

UFSTROMFONTDIR=\"%rom%fontdata/\"
UFSTDISCFONTDIR?=\"$(UFST_ROOT)/fontdata/\"


UFST_CFLAGS=

# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

JSRCDIR=./jpeg

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

SHARE_LIBPNG=0
PNGSRCDIR=./libpng
LIBPNG_NAME=png

# libtiff
SHARE_LIBTIFF=0
TIFFSRCDIR=
TIFFCONFDIR=
TIFFPLATFORM=unix
TIFFCONFIG_SUFFIX=
LIBTIFF_NAME=tiff

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

SHARE_ZLIB=0
ZSRCDIR=./zlib
#ZLIB_NAME=gz
ZLIB_NAME=z

# Choose shared or compiled in libjbig2dec and source location
# JBIG2_LIB=jbig2dec
JBIG2_LIB=jbig2dec
SHARE_JBIG2=0
JBIG2SRCDIR=./jbig2dec
JBIG2_CFLAGS=-DHAVE_STDINT_H=1

# Choose the library to use for (JPXDecode support)
# whether to link to an external build or compile in from source
# and source location and configuration flags for compiling in
JPX_LIB=openjpeg
SHARE_JPX=0
JPXSRCDIR=./openjpeg
JPX_CFLAGS=  -DUSE_JPIP -DUSE_OPENJPEG_JP2 -DOPJ_HAVE_STDINT_H=1 -DOPJ_HAVE_INTTYPES_H=1  -DOPJ_HAVE_FSEEKO=1

# Uncomment the following 4 lines to to compile in OpenJPEG codec
#JPX_LIB=openjpeg
#SHARE_JPX=0
#JPXSRCDIR=openjpeg
#JPX_CFLAGS=-DUSE_OPENJPEG_JP2 -DOPJ_STATIC

# options for lcms color management library
SHARE_LCMS=0
LCMS2SRCDIR=./lcms2mt
LCMS2_CFLAGS=-DSHARE_LCMS=$(SHARE_LCMS) -DCMS_USE_BIG_ENDIAN=0

LCMS2MTSRCDIR=./lcms2mt
LCMS2_CFLAGS=-DSHARE_LCMS=$(SHARE_LCMS) -DCMS_USE_BIG_ENDIAN=0

# Which CMS are we using?
# Options are currently lcms2mt or lcms2
WHICH_CMS=lcms2mt

EXPATSRCDIR=./expat
EXPAT_CFLAGS=-DHAVE_MEMMOVE
EXPATGENDIR=$(GLGENDIR)
EXPATOBJDIR=$(GLOBJDIR)
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
SHARE_EXPAT=0

JPEGXR_SRCDIR=./jpegxr
SHARE_JPEGXR=0
JPEGXR_GENDIR=$(GLGENDIR)
JPEGXR_OBJDIR=$(GLOBJDIR)

# Define the directory where the ijs source is stored,
# and the process forking method to use for the server.
# See ijs.mak for more information.

SHARE_IJS=0
IJS_NAME=
IJSSRCDIR=./ijs
IJSEXECTYPE=unix

# Define install location for 'cups' device/filter support
CUPSLIBS=
CUPSLIBDIRS=
CUPSSERVERBIN=
CUPSSERVERROOT=
CUPSDATA=
CUPSPDFTORASTER=0

SHARE_LCUPS=1
LCUPS_NAME=cups
LCUPSSRCDIR=./cups
LIBCUPSSRCDIR=./cups
LCUPSBUILDTYPE=
CUPS_CC=$(CC)

SHARE_LCUPSI=1
LCUPSI_NAME=cupsimage
LCUPSISRCDIR=./cups
CUPS_CC=$(CC)

CUPSCFLAGS= -DSHARE_LCUPS=$(SHARE_LCUPS) -DSHARE_LCUPSI=$(SHARE_LCUPSI)

# Define how to build the library archives.  (These are not used in any
# standard configuration.)

AR=ar
ARFLAGS=qc
RANLIB=ranlib

# ------ Platform-specific options ------ #

# Define the name of the C compiler (target and host (AUX))

CC=gcc
CCAUX=gcc

# Define the name of the linker for the final link step.
# Normally this is the same as the C compiler.

CCLD=$(CC)
CCAUXLD=$(CCAUX)

# Define the default gcc flags.
GCFLAGS=  -Wall -Wstrict-prototypes -Wundef -Wmissing-declarations -Wmissing-prototypes -Wwrite-strings -Wno-strict-aliasing -Werror=declaration-after-statement -fno-builtin -fno-common -Werror=return-type -DHAVE_STDINT_H=1 -DHAVE_DIRENT_H=1 -DHAVE_SYS_DIR_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_TIMES_H=1 -DHAVE_INTTYPES_H=1 -DGX_COLOR_INDEX_TYPE="unsigned long int" -D__USE_UNIX98=1   -DNOCONTRIB

# Define the added flags for standard, debugging, profiling
# and shared object builds.

CFLAGS_STANDARD= -O2 -DNDEBUG
CFLAGS_DEBUG= -g -O0
CFLAGS_PROFILE=-pg  -O2 -DNDEBUG
CFLAGS_SO=-fPIC

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
# CFLAGS from autoconf
AC_CFLAGS=

# fontconfig flags, used by unix-aux.mak
FONTCONFIG_CFLAGS=
FONTCONFIG_LIBS=

# DBus flags, used by cups.mak
DBUS_CFLAGS=
DBUS_LIBS=

# defines from autoconf; note that we don't use all of these at present.
ACDEFS=-DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DPACKAGE_URL=\"\" -DHAVE_DIRENT_H=1 -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_ERRNO_H=1 -DHAVE_FCNTL_H=1 -DHAVE_LIMITS_H=1 -DHAVE_MALLOC_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_STRINGS_H=1 -DHAVE_SYS_IOCTL_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_TIMES_H=1 -DHAVE_SYSLOG_H=1 -DHAVE_UNISTD_H=1 -DHAVE_DIRENT_H=1 -DHAVE_SYS_DIR_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STRUCT_STAT_ST_BLOCKS=1 -DHAVE_ST_BLOCKS=1 -DTIME_WITH_SYS_TIME=1 -DSIZEOF_UNSIGNED_LONG_INT=8 -DHAVE_LIBM=1 -DHAVE_PREAD=1 -DHAVE_PWRITE=1 -DHAVE_DECL_PWRITE=1 -DHAVE_DECL_PREAD=1 -DHAVE_LIBDL=1 -DHAVE_FSEEKO=1 -DHAVE_MEMALIGN=1 -DX_DISPLAY_MISSING=1 -DHAVE_MKSTEMP=1 -DHAVE_FOPEN64=1 -DHAVE_FSEEKO=1 -DHAVE_MKSTEMP64=1 -DHAVE_SETLOCALE=1 -DHAVE_STRERROR=1 -DHAVE_FORK=1 -DHAVE_VFORK=1 -DHAVE_WORKING_VFORK=1 -DHAVE_WORKING_FORK=1 -DHAVE_STDLIB_H=1 -DHAVE_MALLOC=1 -DRETSIGTYPE=void -DLSTAT_FOLLOWS_SLASHED_SYMLINK=1 -DHAVE_VPRINTF=1 -DHAVE_BZERO=1 -DHAVE_DUP2=1 -DHAVE_FLOOR=1 -DHAVE_GETTIMEOFDAY=1 -DHAVE_MEMCHR=1 -DHAVE_MEMMOVE=1 -DHAVE_MEMSET=1 -DHAVE_MKDIR=1 -DHAVE_MKFIFO=1 -DHAVE_MODF=1 -DHAVE_POW=1 -DHAVE_PUTENV=1 -DHAVE_RINT=1 -DHAVE_SETENV=1 -DHAVE_SQRT=1 -DHAVE_STRCHR=1 -DHAVE_STRRCHR=1 -DHAVE_STRSPN=1 -DHAVE_STRSTR=1 -DHAVE_STRNLEN=1

CFLAGS=$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)

# Define platform flags for ld.
# SunOS 4.n may need -Bstatic.
# Solaris 2.6 (and possibly some other versions) with any of the SHARE_
# parameters set to 1 may need
#	-R /usr/local/xxx/lib:/usr/local/lib
# giving the full path names of the shared library directories.
# XLDFLAGS can be set from the command line.
# AC_LDFLAGS from autoconf
AC_LDFLAGS=

LDFLAGS= $(AC_LDFLAGS) $(XLDFLAGS)

GS_LDFLAGS=$(LDFLAGS)
PCL_LDFLAGS=$(LDFLAGS)
XPS_LDFLAGS=$(LDFLAGS)
PDF_LDFLAGS=$(LDFLAGS)

GS_LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(GS_SONAME_MAJOR)
PCL_LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(PCL_SONAME_MAJOR)
XPS_LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(XPS_SONAME_MAJOR)
PDF_LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(PDF_SONAME_MAJOR)

# Define any extra libraries to link into the executable.
# ISC Unix 2.2 wants -linet.
# SCO Unix needs -lsocket if you aren't including the X11 driver.
# SVR4 may need -lnsl.
# Solaris may need -lnsl -lsocket -lposix4.
# (Libraries required by individual drivers are handled automatically.)

EXTRALIBS=$(XTRALIBS) -ldl -lm  -rdynamic -ldl
AUXEXTRALIBS=$(XTRALIBS) -ldl -lm  -rdynamic -ldl

# Define the standard libraries to search at the end of linking.
# Most platforms require -lpthread for the POSIX threads library;
# on FreeBSD, change -lpthread to -lc_r; BSDI and perhaps some others
# include pthreads in libc and don't require any additional library.
# All reasonable platforms require -lm, but Rhapsody and perhaps one or
# two others fold libm into libc and don't require any additional library.

# STDLIBS=-lpthread -lm
STDLIBS=-lm

# Define the include switch(es) for the X11 header files.
# This can be null if handled in some other way (e.g., the files are
# in /usr/include, or the directory is supplied by an environment variable)

XINCLUDE=

# Define the directory/ies and library names for the X11 library files.
# XLIBDIRS is for ld and should include -L; XLIBDIR is for LD_RUN_PATH
# (dynamic libraries on SVR4) and should not include -L.
# Newer SVR4 systems can use -R in XLIBDIRS rather than setting XLIBDIR.
# Both can be null if these files are in the default linker search path.

# Solaris and other SVR4 systems with dynamic linking probably want
#XLIBDIRS=-L/usr/openwin/lib -R/usr/openwin/lib
# X11R6 (on any platform) may need
#XLIBS=Xt SM ICE Xext X11

# We use the autoconf macro AC_PATH_XTRA which defines X_LIBS with
# the -L (or whatever). It also defines X_PRE_LIBS and X_EXTRA_LIBS
# all three of which are stripped and slotted into XLIBS below.
# Usually however, all but X_LIBS are empty on modern platforms.
XLIBDIRS=
XLIBDIR=
# XLIBS= Xt Xext X11
XLIBS=

# Define the .dev module that implements thread and synchronization
# primitives for this platform.

# If POSIX sync primitives are used, also change the STDLIBS to include
# the pthread library. Otherwise use SYNC=nosync
#SYNC=posync
#SYNC=nosync
SYNC=nosync

# programs we use
RM=rm -f

# ------ Dynamic loader options ------- #
SOC_CFLAGS	=
SOC_LIBS	=
SOC_LOADER	=	dxmainc.c

# on virtually every Unix-a-like system, this is "so",
# but Apple just had to be different, so it's now set
# by configure
SO_LIB_EXT=.so
DLL_EXT=
SO_LIB_VERSION_SEPARATOR=.

#CAIRO_CFLAGS	=	@CAIRO_CFLAGS@
#CAIRO_LIBS	=	@CAIRO_LIBS@

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

# if it's included, $(PSD)gs_pdfwr.dev should always be one of the last in the list
PSI_FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)epsf.dev $(PSD)ttfont.dev \
                  $(PSD)fapi_ps.dev $(PSD)jpx.dev $(PSD)jbig2.dev $(PSD)gs_pdfwr.dev


PCL_FEATURE_DEVS=$(PLOBJDIR)/pl.dev $(PLOBJDIR)/pjl.dev $(PXLOBJDIR)/pxl.dev $(PCL5OBJDIR)/pcl5c.dev \
             $(PCL5OBJDIR)/hpgl2c.dev

XPS_FEATURE_DEVS=$(XPSOBJDIR)/pl.dev $(XPSOBJDIR)/xps.dev

PDF_FEATURE_DEVS=$(PDFOBJDIR)/pl.dev $(PDFOBJDIR)/gpdf.dev

FEATURE_DEVS=$(GLD)pipe.dev $(GLD)gsnogc.dev $(GLD)htxlib.dev $(GLD)psl3lib.dev $(GLD)psl2lib.dev \
             $(GLD)dps2lib.dev $(GLD)path1lib.dev $(GLD)patlib.dev $(GLD)psl2cs.dev $(GLD)rld.dev $(GLD)gxfapiu$(UFST_BRIDGE).dev\
             $(GLD)ttflib.dev  $(GLD)cielib.dev $(GLD)pipe.dev $(GLD)htxlib.dev $(GLD)sdct.dev $(GLD)libpng.dev\
	     $(GLD)seprlib.dev $(GLD)translib.dev $(GLD)cidlib.dev $(GLD)psf0lib.dev $(GLD)psf1lib.dev\
             $(GLD)psf2lib.dev $(GLD)lzwd.dev $(GLD)sicclib.dev \
             $(GLD)sjbig2.dev $(GLD)sjpx.dev $(GLD)ramfs.dev \
             $(GLD)pwgd.dev $(GLD)urfd.dev




#FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev
#FEATURE_DEVS=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)ttfont.dev $(GLD)pipe.dev
# The following is strictly for testing.
FEATURE_DEVS_ALL=$(PSD)psl3.dev $(PSD)pdf.dev $(PSD)ttfont.dev $(PSD)double.dev $(PSD)trapping.dev $(PSD)stocht.dev $(GLD)pipe.dev $(GLD)gsnogc.dev $(GLD)htxlib.dev $(PSD)jbig2.dev $(PSD)jpx.dev  $(GLD)ramfs.dev
#FEATURE_DEVS=$(FEATURE_DEVS_ALL)

# The list of resources to be included in the %rom% file system.
# This is in the top makefile since the file descriptors are platform specific
RESOURCE_LIST=Resource/CMap/ Resource/ColorSpace/ Resource/Decoding/ Resource/Font/ Resource/ProcSet/ Resource/IdiomSet/ Resource/CIDFont/

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

BAND_LIST_STORAGE=file

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.

BAND_LIST_COMPRESSOR=zlib

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfxfd.c for more details.

FILE_IMPLEMENTATION=stdio

# List of default devices, in order of priority. They need not be
# present in the actual build.
GS_DEV_DEFAULT="x11alpha bbox"

# Fallback default device.  This is set to 'display' by
# unix-dll.mak when building a shared object.
DISPLAY_DEV=$(DD)bbox.dev

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and dcontrib.mak for the list of available devices.
# DEVICE_DEVS=$(DISPLAY_DEV) $(DD)x11.dev $(DD)x11_.dev $(DD)x11alpha.dev $(DD)x11alt_.dev $(DD)x11cmyk.dev $(DD)x11cmyk2.dev $(DD)x11cmyk4.dev $(DD)x11cmyk8.dev $(DD)x11gray2.dev $(DD)x11gray4.dev $(DD)x11mono.dev $(DD)x11rg16x.dev $(DD)x11rg32x.dev
DEVICE_DEVS=$(DISPLAY_DEV)
DEVICE_DEVS1=$(DD)bit.dev $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bitrgbtags.dev $(DD)bmp16.dev $(DD)bmp16m.dev $(DD)bmp256.dev $(DD)bmp32b.dev $(DD)bmpgray.dev $(DD)bmpmono.dev $(DD)bmpsep1.dev $(DD)bmpsep8.dev $(DD)ccr.dev $(DD)cif.dev $(DD)devicen.dev $(DD)eps2write.dev $(DD)fpng.dev $(DD)inferno.dev $(DD)ink_cov.dev $(DD)inkcov.dev $(DD)jpeg.dev $(DD)jpegcmyk.dev $(DD)jpeggray.dev $(DD)mgr4.dev $(DD)mgr8.dev $(DD)mgrgray2.dev $(DD)mgrgray4.dev $(DD)mgrgray8.dev $(DD)mgrmono.dev $(DD)miff24.dev $(DD)pam.dev $(DD)pamcmyk32.dev $(DD)pamcmyk4.dev $(DD)pbm.dev $(DD)pbmraw.dev $(DD)pcx16.dev $(DD)pcx24b.dev $(DD)pcx256.dev $(DD)pcxcmyk.dev $(DD)pcxgray.dev $(DD)pcxmono.dev $(DD)pdfwrite.dev $(DD)pgm.dev $(DD)pgmraw.dev $(DD)pgnm.dev $(DD)pgnmraw.dev $(DD)pkm.dev $(DD)pkmraw.dev $(DD)pksm.dev $(DD)pksmraw.dev $(DD)plan.dev $(DD)plan9bm.dev $(DD)planc.dev $(DD)plang.dev $(DD)plank.dev $(DD)planm.dev $(DD)plank.dev $(DD)plib.dev $(DD)plibc.dev $(DD)plibg.dev $(DD)plibk.dev $(DD)plibm.dev $(DD)pnm.dev $(DD)pnmraw.dev $(DD)ppm.dev $(DD)ppmraw.dev $(DD)ps2write.dev $(DD)psdcmyk.dev $(DD)psdcmykog.dev $(DD)psdf.dev $(DD)psdrgb.dev $(DD)spotcmyk.dev $(DD)txtwrite.dev $(DD)xcf.dev $(DD)psdcmyk16.dev $(DD)psdrgb16.dev $(DD)psdcmyktags.dev $(DD)psdcmyktags16.dev
DEVICE_DEVS2=$(DD)ap3250.dev $(DD)atx23.dev $(DD)atx24.dev $(DD)atx38.dev $(DD)bj10e.dev $(DD)bj200.dev $(DD)bjc600.dev $(DD)bjc800.dev $(DD)cdeskjet.dev $(DD)cdj500.dev $(DD)cdj550.dev $(DD)cdjcolor.dev $(DD)cdjmono.dev $(DD)cljet5.dev $(DD)cljet5c.dev $(DD)cljet5pr.dev $(DD)coslw2p.dev $(DD)coslwxl.dev $(DD)declj250.dev $(DD)deskjet.dev $(DD)dj505j.dev $(DD)djet500.dev $(DD)djet500c.dev $(DD)dnj650c.dev $(DD)eps9high.dev $(DD)eps9mid.dev $(DD)epson.dev $(DD)epsonc.dev $(DD)escp.dev $(DD)fs600.dev $(DD)hl7x0.dev $(DD)ibmpro.dev $(DD)imagen.dev $(DD)itk24i.dev $(DD)itk38.dev $(DD)jetp3852.dev $(DD)laserjet.dev $(DD)lbp8.dev $(DD)lips3.dev $(DD)lj250.dev $(DD)lj3100sw.dev $(DD)lj4dith.dev $(DD)lj4dithp.dev $(DD)lj5gray.dev $(DD)lj5mono.dev $(DD)ljet2p.dev $(DD)ljet3.dev $(DD)ljet3d.dev $(DD)ljet4.dev $(DD)ljet4d.dev $(DD)ljet4pjl.dev $(DD)ljetplus.dev $(DD)lp2563.dev $(DD)lp8000.dev $(DD)lq850.dev $(DD)lxm5700m.dev $(DD)m8510.dev $(DD)necp6.dev $(DD)oce9050.dev $(DD)oki182.dev $(DD)okiibm.dev $(DD)paintjet.dev $(DD)photoex.dev $(DD)picty180.dev $(DD)pj.dev $(DD)pjetxl.dev $(DD)pjxl.dev $(DD)pjxl300.dev $(DD)pxlcolor.dev $(DD)pxlmono.dev $(DD)r4081.dev $(DD)rinkj.dev $(DD)sj48.dev $(DD)st800.dev $(DD)stcolor.dev $(DD)t4693d2.dev $(DD)t4693d4.dev $(DD)t4693d8.dev $(DD)tek4696.dev $(DD)uniprint.dev
DEVICE_DEVS3=
DEVICE_DEVS4=$(DD)ijs.dev
DEVICE_DEVS5=
DEVICE_DEVS6=$(DD)png16.dev $(DD)png16m.dev $(DD)png256.dev $(DD)png48.dev $(DD)pngalpha.dev $(DD)png16malpha.dev $(DD)pnggray.dev $(DD)pngmono.dev
DEVICE_DEVS7=
DEVICE_DEVS8=
DEVICE_DEVS9=
DEVICE_DEVS10=
DEVICE_DEVS11=
DEVICE_DEVS12=
DEVICE_DEVS13=
DEVICE_DEVS14=
DEVICE_DEVS15=
DEVICE_DEVS16=
DEVICE_DEVS17=
DEVICE_DEVS18=
DEVICE_DEVS19=
DEVICE_DEVS20=
DEVICE_DEVS21=


# Shared library target to build.
GS_SHARED_OBJS=

# ---------------------------- End of options --------------------------- #

# Define the name of the partial makefile that specifies options --
# used in dependencies.

MAKEFILE=$(GLSRCDIR)/unix-gcc.mak
TOP_MAKEFILES=$(MAKEFILE) $(GLSRCDIR)/unixhead.mak

# for use in unix-dll.mak and unix-end.mak
# if you rename the Makefile, you *must* also set this to the new name
SUB_MAKE_OPTION=-f $(MAKEFILE)

# Define the auxiliary program dependency. We don't use this.

AK=

# Define the compilation rules and flags.

CCFLAGS=$(GENOPT) $(CAPOPT) $(CFLAGS)
CC_=$(CC) $(CCFLAGS)
CCAUX_=$(CCAUX) $(CFLAGS)
CC_LEAF=$(CC_)
# note gcc can't use -fomit-frame-pointer with -pg.
CC_LEAF_PG=$(CC_)
# These are the specific warnings we have to turn off to compile those
# specific few files that need this.  We may turn off others in the future.
CC_NO_WARN=$(CC_)
CCAUX_NO_WARN=$(CCAUX_)
CC_SHARED=$(CC_) -fPIC

LD_SET_DT_SONAME=-soname=

# MAKEDIRS = the dependency on ALL object files (must be the last one on
# the line. Requires GNU make to make it an 'order only' dependency
# MAKEDIRSTOP = the topmost dependency - set this if you can't set MAKEDIRS

MAKEDIRS=| directories
MAKEDIRSTOP=

# ---------------- End of platform-specific section ---------------- #

INSTALL_CONTRIB=
include $(GLSRCDIR)/unixhead.mak
include $(GLSRCDIR)/gs.mak
# *romfs.mak must precede lib.mak

# The following can be uncommented if the PCL/XPS/PDL sources
# are available
# include $(PLSRCDIR)$(D)plromfs.mak   # plromfs.mak
# include $(XPSSRCDIR)$(D)xpsromfs.mak # xpsromfs.mak
# include $(PDFSRCDIR)$(D)pdfromfs.mak # pdfromfs.mak

include $(PSSRCDIR)/psromfs.mak
include $(GLSRCDIR)/lib.mak
include $(PSSRCDIR)/int.mak

# The following can be uncommented if the PCL/XPS/PDL sources
# are available
# include $(PLSRCDIR)$(D)pl.mak # pl.mak
# include $(PCL5SRCDIR)$(D)pcl.mak # pcl.mak
# include $(PCL5SRCDIR)$(D)pcl_top.mak # pcl_top.mak
# include $(PXLSRCDIR)$(D)pxl.mak # pxl.mak

# include $(XPSSRCDIR)$(D)xps.mak # xps.mak

# include $(PDFSRCDIR)$(D)pdf.mak # pdf.mak

# include $(GPDLSRCDIR)$(D)gpdl.mak # gpdl.mak

include $(GLSRCDIR)/freetype.mak
include $(GLSRCDIR)$(D)stub.mak
include $(GLSRCDIR)/jpeg.mak
# zlib.mak must precede png.mak
include $(GLSRCDIR)/zlib.mak
include $(GLSRCDIR)/png.mak
include $(GLSRCDIR)/tiff.mak
include $(GLSRCDIR)/jbig2.mak
include $(GLSRCDIR)/openjpeg.mak
include $(GLSRCDIR)/cal.mak
include $(GLSRCDIR)/ocr.mak

include $(GLSRCDIR)/jpegxr.mak
include $(GLSRCDIR)/expat.mak

include $(GLSRCDIR)/$(WHICH_CMS).mak
include $(GLSRCDIR)/ijs.mak


include $(DEVSRCDIR)/devs.mak
include $(DEVSRCDIR)/dcontrib.mak
include $(GLSRCDIR)/unix-aux.mak
include $(GLSRCDIR)/unixlink.mak
include $(GLSRCDIR)/unix-dll.mak
include $(GLSRCDIR)/unix-end.mak
include $(GLSRCDIR)/unixinst.mak



# Clean up after the autotools scripts
distclean : clean config-clean soclean pgclean debugclean mementoclean
	-$(RM_) -r $(BINDIR) $(GLOBJDIR) $(PSOBJDIR) $(AUXDIR)
	-$(RM_) -r autom4te.cache
	-$(RM_) config.log config.status
	-$(RM_) -r $(TIFFCONFDIR)
	-$(RM_) Makefile

# a debug-clean target for consistency with the ghostpdl builds
debug-clean : debugclean

memento-clean : mementoclean

maintainer-clean : distclean
	-$(RM_) configure

check : default
	$(NO_OP)
