# Copyright (C) 2001-2024 Artifex Software, Inc.
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
# makefile for Microsoft Visual C++ 4.1 or later, Windows NT or Windows 95 LIBRARY.
#
# All configurable options are surrounded by !ifndef/!endif to allow
# preconfiguration from within another makefile.

# If we are building MEMENTO=1, then adjust default debug flags
!if "$(MEMENTO)"=="1"
!ifndef DEBUG
DEBUG=1
!endif
!ifndef TDEBUG
TDEBUG=1
!endif
!ifndef DEBUGSYM
DEBUGSYM=1
!endif
!endif

# If we are building PROFILE=1, then adjust default debug flags
!if "$(PROFILE)"=="1"
!ifndef DEBUG
DEBUG=0
!endif
!ifndef TDEBUG
TDEBUG=0
!endif
!ifndef DEBUGSYM
DEBUGSYM=1
!endif
!endif

# Pick the target architecture file
!if "$(TARGET_ARCH_FILE)"==""
!ifdef WIN64
TARGET_ARCH_FILE=$(GLSRCDIR)\..\arch\windows-x64-msvc.h
!else
!ifdef ARM
TARGET_ARCH_FILE=$(GLSRCDIR)\..\arch\windows-arm-msvc.h
!else
TARGET_ARCH_FILE=$(GLSRCDIR)\..\arch\windows-x86-msvc.h
!endif
!endif
!endif

# ------------------------------- Options ------------------------------- #

###### This section is the only part of the file you should need to edit.

# ------ Generic options ------ #

# Define the root directory for Ghostscript installation.

!ifndef AROOTDIR
AROOTDIR=c:/gs
!endif
!ifndef GSROOTDIR
GSROOTDIR=$(AROOTDIR)/gs$(GS_DOT_VERSION)
!endif

# Define the directory that will hold documentation at runtime.

!ifndef GS_DOCDIR
GS_DOCDIR=$(GSROOTDIR)/doc
!endif

# Define the default directory/ies for the runtime initialization, resource and
# font files.  Separate multiple directories with ';'.
# Use / to indicate directories, not \.
# MSVC will not allow \'s here because it sees '\;' CPP-style as an
# illegal escape.

!ifndef GS_LIB_DEFAULT
GS_LIB_DEFAULT=$(GSROOTDIR)/Resource/Init;$(GSROOTDIR)/lib;$(GSROOTDIR)/Resource;$(AROOTDIR)/fonts
!endif

# Define whether or not searching for initialization files should always
# look in the current directory first.  This leads to well-known security
# and confusion problems,  but may be convenient sometimes.

!ifndef SEARCH_HERE_FIRST
SEARCH_HERE_FIRST=0
!endif

# Define the name of the interpreter initialization file.
# (There is no reason to change this.)

GS_INIT=gs_init.ps

# Choose generic configuration options.

# Setting DEBUG=1 includes debugging features (-Z switch) in the code.
# Code runs substantially slower even if no debugging switches are set,
# and also takes about another 25K of memory.

!ifndef DEBUG
DEBUG=0
!endif

# Setting TDEBUG=1 includes symbol table information for the debugger,
# and also enables stack checking.  Code is substantially slower and larger.

# NOTE: The MSVC++ 5.0 compiler produces incorrect output code with TDEBUG=0.
# Leave TDEBUG set to 1.

!ifndef TDEBUG
TDEBUG=1
!endif

# Setting DEBUGSYM=1 is only useful with TDEBUG=0.
# This option is for advanced developers. It includes symbol table
# information for the debugger in an optimized (release) build.
# NOTE: The debugging information generated for the optimized code may be
# significantly misleading. For general MSVC users we recommend TDEBUG=1.

!ifndef DEBUGSYM
DEBUGSYM=0
!endif


# We can compile for a 32-bit or 64-bit target
# WIN32 and WIN64 are mutually exclusive.  WIN32 is the default.
!if !defined(WIN32) && (!defined(Win64) || !defined(WIN64))
WIN32=0
!endif

# We can build either 32-bit or 64-bit target on a 64-bit platform
# but the location of the binaries differs. Would be nice if the
# detection of the platform could be automatic.
!ifndef BUILD_SYSTEM
!if "$(PROCESSOR_ARCHITEW6432)"=="AMD64" || "$(PROCESSOR_ARCHITECTURE)"=="AMD64"
BUILD_SYSTEM=64
PGMFILES=$(SYSTEMDRIVE)\Program Files
PGMFILESx86=$(SYSTEMDRIVE)\Program Files (x86)
!else
BUILD_SYSTEM=32
PGMFILES=$(SYSTEMDRIVE)\Program Files
PGMFILESx86=$(SYSTEMDRIVE)\Program Files
!endif
!endif

!ifndef MSWINSDKPATH
!if exist ("$(PGMFILESx86)\Microsoft SDKs\Windows")
!if exist ("$(PGMFILESx86)\Microsoft SDKs\Windows\v7.1A")
MSWINSDKPATH=$(PGMFILESx86)\Microsoft SDKs\Windows\v7.1A
!else
!if exist ("$(PGMFILESx86)\Microsoft SDKs\Windows\v7.1")
MSWINSDKPATH=$(PGMFILESx86)\Microsoft SDKs\Windows\v7.1
!else
!if exist ("$(PGMFILESx86)\Microsoft SDKs\Windows\v7.0A")
MSWINSDKPATH=$(PGMFILESx86)\Microsoft SDKs\Windows\v7.0A
!else
!if exist ("$(PGMFILESx86)\Microsoft SDKs\Windows\v7.0")
MSWINSDKPATH=$(PGMFILESx86)\Microsoft SDKs\Windows\v7.0
!endif
!endif
!endif
!endif
!else
!if exist ("$(PGMFILES)\Microsoft SDKs\Windows")
!if exist ("$(PGMFILES)\Microsoft SDKs\Windows\v7.1A")
MSWINSDKPATH=$(PGMFILES)\Microsoft SDKs\Windows\v7.1A
!else
!if exist ("$(PGMFILES)\Microsoft SDKs\Windows\v7.1")
MSWINSDKPATH=$(PGMFILES)\Microsoft SDKs\Windows\v7.1
!else
!if exist ("$(PGMFILES)\Microsoft SDKs\Windows\v7.0A")
MSWINSDKPATH=$(PGMFILES)\Microsoft SDKs\Windows\v7.0A
!else
!if exist ("$(PGMFILES)\Microsoft SDKs\Windows\v7.0")
MSWINSDKPATH=$(PGMFILES)\Microsoft SDKs\Windows\v7.0
!endif
!endif
!endif
!endif
!endif
!endif
!endif

XPSPRINTCFLAGS=
XPSPRINT=0

!ifdef MSWINSDKPATH
!if exist ("$(MSWINSDKPATH)\Include\XpsPrint.h")
XPSPRINTCFLAGS=/DXPSPRINT=1 /I"$(MSWINSDKPATH)\Include"
XPSPRINT=1
!endif
!endif

!if "$(MEMENTO)"=="1"
CFLAGS=$(CFLAGS) -DMEMENTO
!endif

!if "$(USE_LARGE_COLOR_INDEX)"=="1"
# Definitions to force gx_color_index to 64 bits
LARGEST_UINTEGER_TYPE=unsigned __int64
GX_COLOR_INDEX_TYPE=$(LARGEST_UINTEGER_TYPE)

CFLAGS=$(CFLAGS) /DGX_COLOR_INDEX_TYPE="$(GX_COLOR_INDEX_TYPE)"
!endif

# -W3 generates too much noise.
!ifndef WARNOPT
WARNOPT=-W2
!endif

# Define the name of the executable file.

!ifndef GS
GS=gslib
!endif

# Define the directory for the final executable, and the
# source, generated intermediate file, and object directories
# for the graphics library (GL) and the PostScript/PDF interpreter (PS).

!if "$(MEMENTO)"=="1"
DEFAULT_OBJ_DIR=.\$(PRODUCT_PREFIX)memobj
!else
!if "$(PROFILE)"=="1"
DEFAULT_OBJ_DIR=.\$(PRODUCT_PREFIX)profobj
!else
!if "$(DEBUG)"=="1"
DEFAULT_OBJ_DIR=.\$(PRODUCT_PREFIX)debugobj
!else
DEFAULT_OBJ_DIR=.\$(PRODUCT_PREFIX)obj
!endif
!endif
!endif
!ifdef METRO
DEFAULT_OBJ_DIR=$(DEFAULT_OBJ_DIR)rt
!endif
!ifdef WIN64
DEFAULT_OBJ_DIR=$(DEFAULT_OBJ_DIR)64
!endif

!ifndef AUXDIR
AUXDIR=$(DEFAULT_OBJ_DIR)\aux_
!endif

!ifndef BINDIR
BINDIR=.\bin
!endif
!ifndef GLSRCDIR
GLSRCDIR=.\base
!ifndef DEVSRCDIR
DEVSRCDIR=.\devices
!endif
!ifndef PSRESDIR
PSRESDIR=.\Resource
!endif
!ifndef PSGENDIR
PSGENDIR=$(GLGENDIR)
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(GLOBJDIR)
!endif
!endif
!ifndef GLGENDIR
GLGENDIR=$(DEFAULT_OBJ_DIR)
!endif
!ifndef GLOBJDIR
GLOBJDIR=$(GLGENDIR)
!endif
!ifndef DEVGENDIR
DEVGENDIR=$(GLGENDIR)
!endif
!ifndef DEVOBJDIR
DEVOBJDIR=$(DEVGENDIR)
!endif

!ifndef EXPATGENDIR
EXPATGENDIR=$(GLGENDIR)
!endif

!ifndef EXPATOBJDIR
EXPATOBJDIR=$(GLOBJDIR)
!endif

!ifndef JPEGXR_GENDIR
JPEGXR_GENDIR=$(GLGENDIR)
!endif

!ifndef JPEGXR_OBJDIR
JPEGXR_OBJDIR=$(GLOBJDIR)
!endif

!ifndef AUXDIR
AUXDIR=$(DEFAULT_OBJ_DIR)\aux_
!endif

!ifndef CONTRIBDIR
CONTRIBDIR=.\contrib
!endif

# Do not edit the next group of lines.
NUL=
DD=$(GLGENDIR)\$(NUL)
GLD=$(GLGENDIR)\$(NUL)

# Should we build in the cups device....
!ifdef WITH_CUPS
!if "$(WITH_CUPS)"!="0"
WITH_CUPS=1
!else
WITH_CUPS=0
!endif
!else
WITH_CUPS=0
!endif

# We can't build cups libraries in a Metro friendly way,
# so if building for Metro, disable cups regardless of the
# request
!ifdef METRO
WITH_CUPS=0
!endif

# Define the directory where the FreeType2 library sources are stored.
# See freetype.mak for more information.

!ifdef UFST_BRIDGE
!if "$(UFST_BRIDGE)"=="1"
FT_BRIDGE=0
!endif
!endif

!ifndef FT_BRIDGE
FT_BRIDGE=1
!endif

!ifndef FTSRCDIR
FTSRCDIR=freetype
!endif
!ifndef FT_CFLAGS
FT_CFLAGS=-I$(FTSRCDIR)\include
!endif

!ifdef BITSTREAM_BRIDGE
FT_BRIDGE=0
!endif


# Define the directory where the IJG JPEG library sources are stored,
# and the major version of the library that is stored there.
# You may need to change this if the IJG library version changes.
# See jpeg.mak for more information.

!ifndef JSRCDIR
JSRCDIR=jpeg
!endif

# Define the directory where the PNG library sources are stored,
# and the version of the library that is stored there.
# You may need to change this if the libpng version changes.
# See png.mak for more information.

!ifndef PNGSRCDIR
PNGSRCDIR=libpng
!endif

# Define the directory where the zlib sources are stored.
# See zlib.mak for more information.

!ifndef ZSRCDIR
ZSRCDIR=zlib
!endif

!ifndef TIFFSRCDIR
TIFFSRCDIR=tiff$(D)
TIFFCONFDIR=$(TIFFSRCDIR)
TIFFCONFIG_SUFFIX=.vc
TIFFPLATFORM=win32
TIFF_CFLAGS=-DJPEG_SUPPORT -DOJPEG_SUPPORT -DJPEG_LIB_MK1_OR_12BIT=0
!endif

!ifndef JBIG2_LIB
JBIG2_LIB=jbig2dec
!endif

# Use jbig2dec by default. See jbig2.mak for more information.
!ifndef JBIG2SRCDIR
# location of included jbig2dec library source
JBIG2SRCDIR=jbig2dec
!endif

# Alternatively, you can build a separate DLL
# and define SHARE_JPX=1 in src/winlib.mak
!ifndef JPX_LIB
JPX_LIB=openjpeg
!endif

!ifndef JPEGXR_SRCDIR
JPEGXR_SRCDIR=.\jpegxr
!endif

!ifndef SHARE_JPEGXR
SHARE_JPEGXR=0
!endif

!ifndef JPEGXR_CFLAGS
JPEGXR_CFLAGS=/TP /DNDEBUG
!endif

!ifndef EXPATSRCDIR
EXPATSRCDIR=.\expat
!endif

!ifndef SHARE_EXPAT
SHARE_EXPAT=0
!endif

!ifndef EXPAT_CFLAGS
EXPAT_CFLAGS=/DWIN32
!endif

# Define any other compilation flags.

!ifndef XCFLAGS
XCFLAGS=
!endif

!ifndef CFLAGS
CFLAGS=
!endif

!ifdef DEVSTUDIO
CFLAGS=$(CFLAGS) /FC
!endif

!ifdef CLUSTER
CFLAGS=$(CFLAGS) -DCLUSTER
!endif

!if "$(MEMENTO)"=="1"
CFLAGS=$(CFLAGS) -DMEMENTO
!endif

!ifdef METRO
# Ideally the TIF_PLATFORM_CONSOLE define should only be for libtiff,
# but we aren't set up to do that easily
CFLAGS=$(CFLAGS) -DMETRO -DWINAPI_FAMILY=WINAPI_PARTITION_APP -DTIF_PLATFORM_CONSOLE
# WinRT doesn't allow ExitProcess() so we have to suborn it here.
# it shouldn't matter since we actually rely on setjmp()/longjmp() for error handling in libtiff
PNG_CFLAGS=/DExitProcess=exit
!endif

CFLAGS=$(CFLAGS) $(XCFLAGS)

# ------ Platform-specific options ------ #

!include $(GLSRCDIR)\msvcver.mak

# Define the directory where the lcms2mt source is stored.
# See lcms2mt.mak for more information
SHARE_LCMS=0
!ifndef LCMS2MTSRCDIR
LCMS2MTSRCDIR=lcms2mt
!endif

# Define the directory where the lcms2 source is stored.
# See lcms2.mak for more information
!ifndef LCMS2SRCDIR
LCMS2SRCDIR=lcms2
!endif

# ------ Devices and features ------ #

# Choose the language feature(s) to include.  See gs.mak for details.

!ifndef FEATURE_DEVS
FEATURE_DEVS=$(GLD)pipe.dev $(GLD)gsnogc.dev $(GLD)htxlib.dev $(GLD)psl3lib.dev $(GLD)psl2lib.dev \
             $(GLD)dps2lib.dev $(GLD)path1lib.dev $(GLD)patlib.dev $(GLD)psl2cs.dev $(GLD)rld.dev $(GLD)gxfapiu$(UFST_BRIDGE).dev\
             $(GLD)ttflib.dev  $(GLD)cielib.dev $(GLD)pipe.dev $(GLD)htxlib.dev $(GLD)sdct.dev $(GLD)libpng.dev\
	     $(GLD)seprlib.dev $(GLD)translib.dev $(GLD)cidlib.dev $(GLD)psf0lib.dev $(GLD)psf1lib.dev\
             $(GLD)psf2lib.dev $(GLD)lzwd.dev $(GLD)sicclib.dev $(GLD)mshandle.dev \
             $(GLD)ramfs.dev $(GLD)sjpx.dev $(GLD)sjbig2.dev $(GLD)pwgd.dev
!endif

# Choose whether to compile the .ps initialization files into the executable.
# See gs.mak for details.

!ifndef COMPILE_INITS
COMPILE_INITS=1
!endif

# Choose whether to store band lists on files or in memory.
# The choices are 'file' or 'memory'.

!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

# Choose which compression method to use when storing band lists in memory.
# The choices are 'lzw' or 'zlib'.

!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif

# Choose the implementation of file I/O: 'stdio', 'fd', or 'both'.
# See gs.mak and sfilefd.c for more details.

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

# Choose the device(s) to include.  See devs.mak for details,
# devs.mak and contrib.mak for the list of available devices.
!ifndef DEVICE_DEVS
DEVICE_DEVS=$(DD)ppmraw.dev $(DD)bbox.dev
DEVICE_DEVS1=
DEVICE_DEVS2=
DEVICE_DEVS3=
DEVICE_DEVS4=
DEVICE_DEVS5=
DEVICE_DEVS6=
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
!endif

# ---------------------------- End of options ---------------------------- #

# Define the name of the makefile -- used in dependencies.

MAKEFILE=$(GLSRCDIR)\msvclib.mak
TOP_MAKEFILES=$(MAKEFILE) $(GLSRCDIR)\msvccmd.mak $(GLSRCDIR)\msvctail.mak $(GLSRCDIR)\winlib.mak

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES2=$(GLOBJDIR)\$(GS).ilk $(GLOBJDIR)\$(GS).pdb $(GLOBJDIR)\genarch.ilk $(GLOBJDIR)\genarch.pdb \
$(GLOBJDIR)\*.sbr $(GLOBJDIR)\cups\*.h $(AUXDIR)\*.sbr $(AUXDIR)\*.pdb

# Define these right away because they modify the behavior of
# msvccmd.mak, msvctail.mak & winlib.mak.

LIB_ONLY=$(GLOBJDIR)\gslib.obj $(GLOBJDIR)\gsnogc.obj $(GLOBJDIR)\gconfig.obj $(GLOBJDIR)\gscdefs.obj $(GLOBJDIR)\gsromfs$(COMPILE_INITS).obj
MAKEDLL=0
GSPLATFORM=mslib32_

!include $(GLSRCDIR)\version.mak
!include $(GLSRCDIR)\msvccmd.mak
!include $(GLSRCDIR)\winlib.mak
!include $(GLSRCDIR)\msvctail.mak

# -------------------------------- Library -------------------------------- #

# The Windows Win32 platform for library

# For some reason, C-file dependencies have to come before mslib32__.dev

$(GLOBJ)gp_mslib.$(OBJ): $(GLSRC)gp_mslib.c $(TOP_MAKEFILES) $(arch_h) $(AK)
	$(GLCCWIN) $(GLO_)gp_mslib.$(OBJ) $(C_) $(GLSRC)gp_mslib.c

mslib32__=$(GLOBJ)gp_mslib.$(OBJ)

$(GLGEN)mslib32_.dev: $(mslib32__) $(ECHOGS_XE) $(GLGEN)mswin32_.dev $(TOP_MAKEFILES)
	$(SETMOD) $(GLGEN)mslib32_ $(mslib32__)
	$(ADDMOD) $(GLGEN)mslib32_ -include $(GLGEN)mswin32_.dev

# ----------------------------- Main program ------------------------------ #

# The library tester EXE
$(GS_XE):  $(GS_ALL) $(DEVS_ALL) $(LIB_ONLY) $(LIBCTR) $(TOP_MAKEFILES)
	copy $(ld_tr) $(GLGENDIR)\gslib32.tr
	echo $(GLOBJDIR)\gsromfs$(COMPILE_INITS).$(OBJ) >> $(GLGENDIR)\gslib32.tr
	echo $(GLOBJ)gsnogc.obj >> $(GLGENDIR)\gslib32.tr
	echo $(GLOBJ)gconfig.obj >> $(GLGENDIR)\gslib32.tr
	echo $(GLOBJ)gscdefs.obj >> $(GLGENDIR)\gslib32.tr
	echo  /SUBSYSTEM:CONSOLE > $(GLGENDIR)\gslib32.rsp
	echo  /OUT:$(GS_XE) >> $(GLGENDIR)\gslib32.rsp
	$(LINK) $(LCT) @$(GLGENDIR)\gslib32.rsp $(GLOBJ)gslib @$(GLGENDIR)\gslib32.tr @$(LIBCTR)
	-del $(GLGENDIR)\gslib32.rsp
	-del $(GLGENDIR)\gslib32.tr
