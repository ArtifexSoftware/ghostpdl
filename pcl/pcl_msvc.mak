#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_msvc.mak
# Top-level makefile for PCL5* on Win32 platforms using MS Visual C 4.1 or later

# Define the name of this makefile.
MAKEFILE=..\pcl\pcl_msvc.mak

# The build process will put all of its output in this directory:
!ifndef GENDIR
GENDIR=.\obj
!endif

# The sources are taken from these directories:
!ifndef GLSRCDIR
GLSRCDIR=..\gs
!endif
!ifndef PLSRCDIR
PLSRCDIR=..\pl
!endif
!ifndef PCLSRCDIR
PCLSRCDIR=..\pcl
!endif
!ifndef COMMONDIR
COMMONDIR=..\common
!endif
!ifndef JSRCDIR
JSRCDIR=$(GLSRCDIR)\jpeg
JVERSION=6
!endif
!ifndef PSRCDIR
PSRCDIR=$(GLSRCDIR)\libpng
PVERSION=96
!endif
!ifndef ZSRCDIR
ZSRCDIR=$(GLSRCDIR)\zlib
!endif


# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif
!ifndef GLOBJDIR
GLOBJDIR=$(GENDIR)
!endif
!ifndef PLGENDIR
PLGENDIR=$(GENDIR)
!endif
!ifndef PLOBJDIR
PLOBJDIR=$(GENDIR)
!endif
!ifndef PCLGENDIR
PCLGENDIR=$(GENDIR)
!endif
!ifndef PCLOBJDIR
PCLOBJDIR=$(GENDIR)
!endif

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
CONFIG=5
TARGET_DEVS=$(PCLOBJDIR)$(D)pcl5c.dev $(PCLOBJDIR)$(D)hpgl2c.dev

# Main file's name
MAIN_OBJ=$(PCLOBJDIR)$(D)pcmain.$(OBJ)

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(PCLOBJDIR)\pcl5
!endif

# Debugging options
!ifndef DEBUG
DEBUG=1
!endif
!ifndef TDEBUG
TDEBUG=1
!endif
!ifndef NOPRIVATE
NOPRIVATE=0
!endif

# Target options
!ifndef CPU_TYPE
CPU_TYPE=486
!endif
!ifndef FPU_TYPE
FPU_TYPE=0
!endif

# Assorted definitions.  Some of these should probably be factored out....
!ifndef GS
GS=gs386
!endif

# Define which major version of MSVC is being used (currently, 4 & 5 supported)
!ifndef MSVC_VERSION
MSVC_VERSION=5
!endif

!ifndef DEVICE_DEVS
DEVICE_DEVS=djet500.dev ljet4.dev cljet5c.dev\
 pcx16.dev pcx256.dev\
 pcxmono.dev pcxcmyk.dev pcxgray.dev\
 pbmraw.dev pgmraw.dev ppmraw.dev pkmraw.dev\
 pxlmono.dev pxlcolor.dev\
 tiffcrle.dev tiffg3.dev tiffg32d.dev tiffg4.dev\
 tifflzw.dev tiffpack.dev\
 tiff12nc.dev tiff24nc.dev
!endif

# GS options
#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS    = dps2lib.dev   \
                  path1lib.dev  \
                  patlib.dev    \
                  rld.dev       \
                  roplib.dev    \
                  ttflib.dev    \
                  colimlib.dev  \
                  cielib.dev    \
                  htxlib.dev    \
                  devcmap.dev

!include $(COMMONDIR)\msvc_top.mak

# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak

# Main program.
!include $(PCLSRCDIR)\pcl_top.mak

