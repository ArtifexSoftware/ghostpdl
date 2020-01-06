# Copyright (C) 2001-2019 Artifex Software, Inc.
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
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
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

# Define which jbig2 library to use
!if !defined(JBIG2_LIB) && (!defined(NO_LURATECH) || "$(NO_LURATECH)" != "1")
!if exist("luratech\ldf_jb2")
JBIG2_LIB=luratech
!endif
!endif

!ifndef JBIG2_LIB
JBIG2_LIB=jbig2dec
!endif

!if "$(JBIG2_LIB)" == "luratech" || "$(JBIG2_LIB)" == "ldf_jb2"
# Set defaults for using the Luratech JB2 implementation
!ifndef JBIG2SRCDIR
# CSDK source code location
JBIG2SRCDIR=luratech\ldf_jb2
!endif
!ifndef JBIG2_CFLAGS
# required compiler flags
!ifdef WIN64
JBIG2_CFLAGS=-DUSE_LDF_JB2 -DWIN64
!else
JBIG2_CFLAGS=-DUSE_LDF_JB2 -DWIN32
!endif
!endif
!else
# Use jbig2dec by default. See jbig2.mak for more information.
!ifndef JBIG2SRCDIR
# location of included jbig2dec library source
JBIG2SRCDIR=jbig2dec
!endif
!endif

# Alternatively, you can build a separate DLL
# and define SHARE_JBIG2=1 in src/winlib.mak

# Define which jpeg2k library to use
!if !defined(JPX_LIB) && (!defined(NO_LURATECH) || "$(NO_LURATECH)" != "1")
!if exist("luratech\lwf_jp2")
JPX_LIB=luratech
!endif
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

# Define which major version of MSVC is being used
# (currently, 4, 5, 6, 7, and 8 are supported).
# Define the minor version of MSVC, currently only
# used for Microsoft Visual Studio .NET 2003 (7.1)

#MSVC_VERSION=6
#MSVC_MINOR_VERSION=0

# Make a guess at the version of MSVC in use
# This will not work if service packs change the version numbers.

!if defined(_NMAKE_VER) && !defined(MSVC_VERSION)
!if "$(_NMAKE_VER)" == "162"
MSVC_VERSION=5
!endif
!if "$(_NMAKE_VER)" == "6.00.8168.0"
# VC 6
MSVC_VERSION=6
!endif
!if "$(_NMAKE_VER)" == "7.00.9466"
MSVC_VERSION=7
!endif
!if "$(_NMAKE_VER)" == "7.00.9955"
MSVC_VERSION=7
!endif
!if "$(_NMAKE_VER)" == "7.10.3077"
MSVC_VERSION=7
MSVC_MINOR_VERSION=1
!endif
!if "$(_NMAKE_VER)" == "8.00.40607.16"
# VS2005
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.42"
# VS2005
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "8.00.50727.762"
# VS2005
MSVC_VERSION=8
!endif
!if "$(_NMAKE_VER)" == "9.00.21022.08"
# VS2008
MSVC_VERSION=9
!endif
!if "$(_NMAKE_VER)" == "9.00.30729.01"
# VS2008
MSVC_VERSION=9
!endif
!if "$(_NMAKE_VER)" == "10.00.30319.01"
# VS2010
MSVC_VERSION=10
!endif
!if "$(_NMAKE_VER)" == "11.00.50522.1"
# VS2012
MSVC_VERSION=11
!endif
!if "$(_NMAKE_VER)" == "11.00.50727.1"
# VS2012
MSVC_VERSION=11
!endif
!if "$(_NMAKE_VER)" == "11.00.60315.1"
# VS2012
MSVC_VERSION=11
!endif
!if "$(_NMAKE_VER)" == "11.00.60610.1"
# VS2012
MSVC_VERSION=11
!endif
!if "$(_NMAKE_VER)" == "12.00.21005.1"
# VS 2013
MSVC_VERSION=12
!endif
!if "$(_NMAKE_VER)" == "14.00.23506.0"
# VS2015
MSVC_VERSION=14
!endif
!if "$(_NMAKE_VER)" == "14.00.24210.0"
# VS2015
MSVC_VERSION=14
!endif
!if "$(_NMAKE_VER)" == "14.16.27034.0"
# VS2017 or VS2019 (Toolset v141)
MSVC_VERSION=15
!endif
!if "$(_NMAKE_VER)" == "14.24.28314.0"
# VS2019 (Toolset v142)
MSVC_VERSION=16
!endif
!endif

!ifndef MSVC_VERSION
!MESSAGE Could not determine MSVC_VERSION! Guessing at an ancient one.
MSVC_VERSION=6
!endif
!ifndef MSVC_MINOR_VERSION
MSVC_MINOR_VERSION=0
!endif

# Define the drive, directory, and compiler name for the Microsoft C files.
# COMPDIR contains the compiler and linker (normally \msdev\bin).
# MSINCDIR contains the include files (normally \msdev\include).
# LIBDIR contains the library files (normally \msdev\lib).
# COMP is the full C compiler path name (normally \msdev\bin\cl).
# COMPCPP is the full C++ compiler path name (normally \msdev\bin\cl).
# COMPAUX is the compiler name for DOS utilities (normally \msdev\bin\cl).
# RCOMP is the resource compiler name (normallly \msdev\bin\rc).
# LINK is the full linker path name (normally \msdev\bin\link).
# Note that when MSINCDIR and LIBDIR are used, they always get a '\' appended,
#   so if you want to use the current directory, use an explicit '.'.

!if $(MSVC_VERSION) == 4
! ifndef DEVSTUDIO
DEVSTUDIO=c:\msdev
! endif
COMPBASE=$(DEVSTUDIO)
SHAREDBASE=$(DEVSTUDIO)
!endif

!if $(MSVC_VERSION) == 5
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Devstudio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\SharedIDE
!endif
!endif

!if $(MSVC_VERSION) == 6
! ifndef DEVSTUDIO
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC98
SHAREDBASE=$(DEVSTUDIO)\Common\MSDev98
!endif
!endif

!if $(MSVC_VERSION) == 7
! ifndef DEVSTUDIO
!if $(MSVC_MINOR_VERSION) == 0
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio .NET 2003
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\Vc7
SHAREDBASE=$(DEVSTUDIO)\Vc7
!endif
!endif

!if $(MSVC_VERSION) == 8
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 8
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 8
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 9
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 9.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 9.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
RCDIR=$(MSSDK)\bin
! else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v6.0A\bin
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\AMD64"
!endif
!endif
!endif

!if $(MSVC_VERSION) == 10
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 10.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 10.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
!  ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!  else
RCDIR=$(MSSDK)\bin
!  endif
! else
!ifdef WIN64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.0\bin
!else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v7.0\bin
!endif
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
!if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\amd64
LINKLIBPATH=/LIBPATH:"$(MSSDK)\lib\x64" /LIBPATH:"$(COMPBASE)\lib\amd64"
!else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!endif
!endif
!endif
!endif

!if $(MSVC_VERSION) == 11
! ifndef DEVSTUDIO
!if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 11.0
!else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 11.0
!endif
! endif
!if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
!else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
! ifdef MSSDK
!  ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!  else
RCDIR=$(MSSDK)\bin
!  endif
! else
!if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Windows Kits\8.0\bin\x64
!else
RCDIR=C:\Program Files\Windows Kits\8.0\bin\x86
!endif
! endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!ifdef WIN64
!if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(MSSDK)\lib\x64" /LIBPATH:"$(COMPBASE)\lib\amd64"
!else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!endif
!endif
!endif
!endif

!if $(MSVC_VERSION) == 12
! ifndef DEVSTUDIO
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 12.0
!  else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 12.0
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
!  ifdef MSSDK
!   ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!   else
RCDIR=$(MSSDK)\bin
!   endif
!  else
!   if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Bin
!   else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v7.1A\Bin
!   endif
!  endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!  ifdef WIN64
!   if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64"
!   else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!   endif
!  endif
! endif
!endif

!if $(MSVC_VERSION) == 14
! ifndef DEVSTUDIO
!  if $(BUILD_SYSTEM) == 64
DEVSTUDIO=C:\Program Files (x86)\Microsoft Visual Studio 14.0
!  else
DEVSTUDIO=C:\Program Files\Microsoft Visual Studio 14.0
!  endif
! endif
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
# There are at least 4 different values:
# "v6.0"=Vista, "v6.0A"=Visual Studio 2008,
# "v6.1"=Windows Server 2008, "v7.0"=Windows 7
!  ifdef MSSDK
!   ifdef WIN64
RCDIR=$(MSSDK)\bin\x64
!   else
RCDIR=$(MSSDK)\bin
!   endif
!  else
!   if $(BUILD_SYSTEM) == 64
RCDIR=C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\Bin
!   else
RCDIR=C:\Program Files\Microsoft SDKs\Windows\v10.0A\Bin
!   endif
!  endif
COMPBASE=$(DEVSTUDIO)\VC
SHAREDBASE=$(DEVSTUDIO)\VC
!  ifdef WIN64
!   if $(BUILD_SYSTEM) == 64
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64"
!   else
COMPDIR64=$(COMPBASE)\bin\x86_amd64
LINKLIBPATH=/LIBPATH:"$(COMPBASE)\lib\amd64" /LIBPATH:"$(COMPBASE)\PlatformSDK\Lib\x64"
!   endif
!  endif
! endif
!endif

!if $(MSVC_VERSION) == 15
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
!MESSAGE Compilation is unlikely to work like this. Build from VS solution for now.
! endif
!endif

!if $(MSVC_VERSION) == 16
! if "$(DEVSTUDIO)"==""
COMPBASE=
SHAREDBASE=
! else
!MESSAGE Compilation is unlikely to work like this. Build from VS solution for now.
! endif
!endif

!if "$(ARM)"=="1"
VCINSTDIR=$(VS110COMNTOOLS)..\..\VC\

!ifndef WINSDKVER
WINSDKVER=8.0
!endif

!ifndef WINSDKDIR
WINSDKDIR=$(VS110COMNTOOLS)..\..\..\Windows Kits\$(WINSDKVER)\
!endif

COMPAUX__="$(VCINSTDIR)\bin\cl.exe"
COMPAUXCFLAGS=/I"$(VCINSTDIR)\INCLUDE" /I"$(VCINSTDIR)\ATLMFC\INCLUDE" \
/I"$(WINSDKDIR)\include\shared" /I"$(WINSDKDIR)\include\um" \
/I"$(WINDSKDIR)include\winrt"

COMPAUXLDFLAGS=/LIBPATH:"$(VCINSTDIR)\LIB" \
/LIBPATH:"$(VCINSTDIR)\ATLMFC\LIB" \
/LIBPATH:"$(WINSDKDIR)\lib\win8\um\x86"

COMPAUX=$(COMPAUX__) $(COMPAUXCFLAGS)

!else

COMPAUXLDFLAGS=""

!endif

# Some environments don't want to specify the path names for the tools at all.
# Typical definitions for such an environment would be:
#   MSINCDIR= LIBDIR= COMP=cl COMPAUX=cl RCOMP=rc LINK=link
# COMPDIR, LINKDIR, and RCDIR are irrelevant, since they are only used to
# define COMP, LINK, and RCOMP respectively, but we allow them to be
# overridden anyway for completeness.
!ifndef COMPDIR
!if "$(COMPBASE)"==""
COMPDIR=
!else
!ifdef WIN64
COMPDIR=$(COMPDIR64)
!else
COMPDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef LINKDIR
!if "$(COMPBASE)"==""
LINKDIR=
!else
!ifdef WIN64
LINKDIR=$(COMPDIR64)
!else
LINKDIR=$(COMPBASE)\bin
!endif
!endif
!endif

!ifndef RCDIR
!if "$(SHAREDBASE)"==""
RCDIR=
!else
RCDIR=$(SHAREDBASE)\bin
!endif
!endif

!ifndef MSINCDIR
!if "$(COMPBASE)"==""
MSINCDIR=
!else
MSINCDIR=$(COMPBASE)\include
!endif
!endif

!ifndef LIBDIR
!if "$(COMPBASE)"==""
LIBDIR=
!else
!ifdef WIN64
LIBDIR=$(COMPBASE)\lib\amd64
!else
LIBDIR=$(COMPBASE)\lib
!endif
!endif
!endif

!ifndef COMP
!if "$(COMPDIR)"==""
COMP=cl
!else
COMP="$(COMPDIR)\cl"
!endif
!endif
!ifndef COMPCPP
COMPCPP=$(COMP)
!endif
!ifndef COMPAUX
!ifdef WIN64
COMPAUX=$(COMP)
!else
COMPAUX=$(COMP)
!endif
!endif

!ifndef RCOMP
!if "$(RCDIR)"==""
RCOMP=rc
!else
RCOMP="$(RCDIR)\rc"
!endif
!endif

!ifndef LINK
!if "$(LINKDIR)"==""
LINK=link
!else
LINK="$(LINKDIR)\link"
!endif
!endif

# nmake does not have a form of .BEFORE or .FIRST which can be used
# to specify actions before anything else is done.  If LIB and INCLUDE
# are not defined then we want to define them before we link or
# compile.  Here is a kludge which allows us to to do what we want.
# nmake does evaluate preprocessor directives when they are encountered.
# So the desired set statements are put into dummy preprocessor
# directives.
!ifndef INCLUDE
!if "$(MSINCDIR)"!=""
!if [set INCLUDE=$(MSINCDIR)]==0
!endif
!endif
!endif
!ifndef LIB
!if "$(LIBDIR)"!=""
!if [set LIB=$(LIBDIR)]==0
!endif
!endif
!endif

!ifndef LINKLIBPATH
LINKLIBPATH=
!endif

# Define the processor architecture. (i386, ppc, alpha)

!ifndef CPU_FAMILY
CPU_FAMILY=i386
#CPU_FAMILY=ppc
#CPU_FAMILY=alpha  # not supported yet - we need someone to tweak
!endif

# Define the processor (CPU) type. Allowable values depend on the family:
#   i386: 386, 486, 586
#   ppc: 601, 604, 620
#   alpha: not currently used.

!ifndef CPU_TYPE
CPU_TYPE=486
#CPU_TYPE=601
!endif

# Define special features of CPUs

# We'll assume that if you have an x86 machine, you've got a modern
# enough one to have SSE2 instructions. If you don't, then predefine
# DONT_HAVE_SSE2 when calling this makefile
!ifndef ARM
!if "$(CPU_FAMILY)" == "i386"
!ifndef DONT_HAVE_SSE2
!ifndef HAVE_SSE2
!message **************************************************************
!message * Assuming that target has SSE2 instructions available. If   *
!message * this is NOT the case, define DONT_HAVE_SSE2 when building. *
!message **************************************************************
!endif
HAVE_SSE2=1
CFLAGS=$(CFLAGS) /DHAVE_SSE2
# add "/D__SSE__" here, but causes crashes just now
JPX_SSE_CFLAGS=
!endif
!endif
!endif

# Define the .dev module that implements thread and synchronization
# primitives for this platform.  Don't change this unless you really know
# what you're doing.

!ifndef SYNC
SYNC=winsync
!endif

# Luratech jp2 flags depend on the compiler version
#
!if "$(JPX_LIB)" == "luratech" || "$(JPX_LIB)" == "lwf_jp2"
# Set defaults for using the Luratech JP2 implementation
!ifndef JPXSRCDIR
# CSDK source code location
JPXSRCDIR=luratech\lwf_jp2
!endif
!ifndef JPX_CFLAGS
# required compiler flags
!ifdef WIN64
JPX_CFLAGS=-DUSE_LWF_JP2 -DWIN64 -DNO_ASSEMBLY
!else
JPX_CFLAGS=-DUSE_LWF_JP2 -DWIN32 -DNO_ASSEMBLY
!endif
!endif
!endif

# OpenJPEG compiler flags
#
!if "$(JPX_LIB)" == "openjpeg"
!ifndef JPXSRCDIR
JPXSRCDIR=openjpeg
!endif
!ifndef JPX_CFLAGS
!ifdef WIN64
JPX_CFLAGS=-DMUTEX_pthread=0 -DUSE_OPENJPEG_JP2 -DUSE_JPIP $(JPX_SSE_CFLAGS) -DOPJ_STATIC -DWIN64
!else
JPX_CFLAGS=-DMUTEX_pthread=0 -DUSE_OPENJPEG_JP2 -DUSE_JPIP $(JPX_SSE_CFLAGS) -DOPJ_STATIC -DWIN32
!endif
!else
JPX_CFLAGS = $JPX_CFLAGS -DUSE_JPIP -DUSE_OPENJPEG_JP2 -DOPJ_STATIC
!endif
!endif

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
