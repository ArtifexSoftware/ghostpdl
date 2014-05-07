# Copyright (C) 2001-2012 Artifex Software, Inc.
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
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# 'nmake' build file for the XPS interpreter

# Define the name of this makefile.
MAKEFILE= $(MAKEFILE) ..\xps\xps_msvc.mak

# Include XPS support
!ifndef XPS_INCLUDED
XPS_INCLUDED=TRUE
!endif XPS_INCLUDED

# Font scaler
!ifndef PL_SCALER
PL_SCALER=afs
!endif

!ifndef FT_BRIDGE
FT_BRIDGE=1
!endif

SHARE_FT=0
FTSRCDIR=$(GLSRCDIR)/../freetype
FT_CFLAGS=-I$(GLSRCDIR)/../freetype/include
FT_LIBS=
FT_CONFIG_SYSTEM_ZLIB=


# Define whether to compile in UFST. Note that freetype will/must be disabled.
# FAPI/UFST depends on UFST_BRIDGE being undefined - hence the construct below.
# (i.e. use "UFST_BRIDGE=1" or *not to define UFST_BRIDGE to anything*)
!ifndef UFST_BRIDGE
UFST_BRIDGE=
!endif

!ifndef UFST_ROOT
UFST_ROOT=$(GLSRCDIR)/../ufst
!endif

UFST_LIB_EXT=.a

UFST_ROMFS_ARGS=-b \
 -P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
 -P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
 -c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO

UFSTROMFONTDIR=\"%rom%fontdata/\"
UFSTDISCFONTDIR=\"$(UFST_ROOT)/fontdata/\"


UFST_CFLAGS=-DGCCx86



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

# The build process will put all of its output in this directory:
# GENDIR is defined in the 'base' makefile, but we need its value immediately
!ifndef GENDIR
!if "$(MEMENTO)"=="1"
GENDIR=.\memobj
!else
!if "$(PROFILE)"=="1"
GENDIR=.\profobj
!else
!if "$(DEBUG)"=="1"
GENDIR=.\debugobj
!else
GENDIR=.\obj
!endif
!endif
!endif
!ifdef WIN64
GENDIR=$(GENDIR)64
!endif
!endif

# The sources are taken from these directories:
!ifndef MAINSRCDIR
MAINSRCDIR=..\main
!endif

!ifndef PSSRCDIR
PSSRCDIR=..\gs\psi
!endif

!ifndef XPSSRCDIR
XPSSRCDIR=..\xps
!endif

!ifndef PSLIBDIR
PSLIBDIR=..\gs\lib
!endif

# Define the directory where the lcms source is stored.
# See lcms.mak for more information
!ifndef LCMSSRCDIR
LCMSSRCDIR=..\gs\lcms
!endif

!ifndef PNGSRCDIR
PNGSRCDIR=..\gs\libpng
!endif

!ifndef TIFFSRCDIR
TIFFSRCDIR=..\gs\tiff
TIFFCONFDIR=$(TIFFSRCDIR)
TIFFCONFIG_SUFFIX=.vc
TIFFPLATFORM=win32
!endif

!ifndef EXPATSRCDIR
EXPATSRCDIR=..\gs\expat
!endif

!ifndef SHARE_EXPAT
SHARE_EXPAT=0
!endif

!ifndef EXPAT_CFLAGS
EXPAT_CFLAGS=/DHAVE_MEMMOVE
!endif

!ifndef JPEGXR_SRCDIR
JPEGXR_SRCDIR=..\gs\jpegxr
!endif

!ifndef SHARE_JPEGXR
SHARE_JPEGXR=0
!endif

!ifndef JPEGXR_CFLAGS
JPEGXR_CFLAGS=/TP /DNDEBUG
!endif

!ifndef TRIOSRCDIR
TRIOSRCDIR=..\gs\trio
!endif

!ifndef SHARE_TRIO
SHARE_TRIO=0
!endif

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
!ifndef PLPLATFORM
PLPLATFORM=
!endif

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif

!ifndef GLOBJDIR
GLOBJDIR=$(GENDIR)
!endif

!ifndef DEVGENDIR
DEVGENDIR=$(GENDIR)
!endif

!ifndef DEVOBJDIR
DEVOBJDIR=$(GENDIR)
!endif

!ifndef PSGENDIR
PSGENDIR=$(GENDIR)
!endif

!ifndef PSOBJDIR
PSOBJDIR=$(GENDIR)
!endif

!ifndef JGENDIR
JGENDIR=$(GENDIR)
!endif

!ifndef JOBJDIR
JOBJDIR=$(GENDIR)
!endif

!ifndef ZGENDIR
ZGENDIR=$(GENDIR)
!endif

!ifndef ZOBJDIR
ZOBJDIR=$(GENDIR)
!endif

!ifndef EXPATGENDIR
EXPATGENDIR=$(GENDIR)
!endif

!ifndef EXPATOBJDIR
EXPATOBJDIR=$(GENDIR)
!endif

!ifndef JPEGXR_GENDIR
JPEGXR_GENDIR=$(GENDIR)
!endif

!ifndef JPEGXR_OBJDIR
JPEGXR_OBJDIR=$(GENDIR)
!endif

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\gxps
!endif

!ifndef BSCFILE
BSCFILE=$(GENDIR)\xps.bsc
!endif

# Main file's name
XPS_TOP_OBJ=$(XPSOBJDIR)\xpstop.$(OBJ)
TOP_OBJ=$(XPS_TOP_OBJ)

# Target XPS
TARGET_DEVS=$(XPSOBJDIR)\xps.dev
PDL_INCLUDE_FLAGS=/DXPS_INCLUDED
# We don't need fonts included by pcl6_gcc.mak
PCLXL_ROMFS_ARGS=

# Choose COMPILE_INITS=1 for init files and fonts in ROM (otherwise =0)
!ifndef COMPILE_INITS
# XPS only needs the ICC profiles from the %rom% file system.
COMPILE_INITS=1
!endif

# "Subclassed" makefile
!include $(MAINSRCDIR)\pcl6_msvc.mak

# Subsystems
!include $(XPSSRCDIR)\xps.mak
!include $(GLSRCDIR)\expat.mak
!include $(GLSRCDIR)\jpegxr.mak
