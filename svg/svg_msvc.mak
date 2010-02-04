# Copyright (C) 2008 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied, modified
# or distributed except as expressly authorized under the terms of that
# license.  Refer to licensing information at http://www.artifex.com/
# or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
# 
# 'nmake' build file for the SVG interpreter

# Define the name of this makefile.
MAKEFILE= $(MAKEFILE) ..\svg\svg_msvc.mak

# Include SVG support
!ifndef SVG_INCLUDED
SVG_INCLUDED=TRUE
!endif SVG_INCLUDED

# Font scaler
!ifndef PL_SCALER
PL_SCALER=afs
!endif

# The build process will put all of its output in this directory:
# GENDIR is defined in the 'base' makefile, but we need its value immediately
!ifndef GENDIR
!if "$(DEBUG)"=="1"
GENDIR=.\debugobj
!else
GENDIR=.\obj
!endif
!endif

# The sources are taken from these directories:
!ifndef MAINSRCDIR
MAINSRCDIR=..\main
!endif

!ifndef PSSRCDIR
PSSRCDIR=..\gs\psi
!endif

!ifndef SVGSRCDIR
SVGSRCDIR=..\svg
!endif

!ifndef PSLIBDIR
PSLIBDIR=..\gs\lib
!endif

!ifndef ICCSRCDIR
ICCSRCDIR=..\gs\icclib
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

!ifndef PSRCDIR
PSRCDIR=..\gs\libpng
!endif

!ifndef TIFFSRCDIR
TIFFSRCDIR=..\gs\tiff
TIFFCONFIG_SUFFIX=.vc
TIFFPLATFORM=win32
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

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\gsvg
!endif

!ifndef BSCFILE
BSCFILE=$(GENDIR)\svg.bsc
!endif

# Main file's name
# this is already in pcl6_gcc.mak
SVG_TOP_OBJ=$(SVGOBJDIR)\svgtop.$(OBJ)
TOP_OBJ= $(SVG_TOP_OBJ)

# Target SVG
TARGET_DEVS=$(SVGOBJDIR)\svg.dev
PDL_INCLUDE_FLAGS=/DSVG_INCLUDED

# Choose COMPILE_INITS=1 for init files and fonts in ROM (otherwise =0)
!ifndef COMPILE_INITS
COMPILE_INITS=0
!endif

# configuration assumptions
!ifndef GX_COLOR_INDEX_DEFINE
GX_COLOR_INDEX_DEFINE=-DGX_COLOR_INDEX_TYPE="unsigned long long"
!endif

# "Subclassed" makefile
!include $(MAINSRCDIR)\pcl6_msvc.mak

# Subsystems
!include $(SVGSRCDIR)\svg.mak
!include $(GLSRCDIR)\expat.mak
