#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_watc.mak
# Top-level makefile for PCL5* on MS-DOS/Watcom platforms.

# The build process will put all of its output in this directory:
!ifndef GENDIR
GENDIR=..\pcl\obj
!endif

# The sources are taken from these directories:
!ifndef GLSRCDIR
GLSRCDIR=..\gs\src
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
JSRCDIR=..\gs\jpeg
JVERSION=6
!endif
!ifndef PSRCDIR
PSRCDIR=..\gs\libpng
PVERSION=96
!endif
!ifndef ZSRCDIR
ZSRCDIR=..\gs\zlib
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
!ifndef DD
DD=$(GLGENDIR)$(D)
!endif

# Define the name of this makefile.
MAKEFILE=$(PCLSRCDIR)\pcl_watc.mak

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PCLOBJDIR)$(D)pcl5c.dev $(PCLOBJDIR)$(D)hpgl2c.dev

# Main file's name
MAIN_OBJ=FILE $(PLOBJDIR)$(D)plmain.$(OBJ) FILE $(PCLOBJDIR)$(D)pcimpl.$(OBJ)
TOP_OBJ=FILE $(PCLOBJDIR)$(D)pctop.$(OBJ)

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(PCLOBJDIR)\pcl5
!endif

# Debugging options
!ifndef DEBUG
DEBUG=0
!endif
!ifndef TDEBUG
TDEBUG=0
!endif
!ifndef NOPRIVATE
NOPRIVATE=0
!endif

# Target options
!ifndef CPU_TYPE
CPU_TYPE=586
!endif
!ifndef FPU_TYPE
FPU_TYPE=0
!endif

# Assorted definitions.  Some of these should probably be factored out....
WCVERSION=10.695

# For building on 16 bit platforms (which may not work anymore) use 'wmakel'
MAKE=wmake

!ifndef DEVICE_DEVS
DEVICE_DEVS=$(DD)ljet4.dev\
 $(DD)pkmraw.dev $(DD)ppmraw.dev $(DD)pgmraw.dev $(DD)pbmraw.dev\
 $(DD)pcx16.dev $(DD)pcx256.dev $(DD)bitcmyk.dev\
 $(DD)cljet5.dev $(DD)ljet4.dev\
 $(DD)pcxmono.dev $(DD)pcxcmyk.dev $(DD)pcxgray.dev\
 $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)pkmraw.dev\
 $(DD)pxlmono.dev $(DD)pxlcolor.dev\
 $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev\
 $(DD)tifflzw.dev $(DD)tiffpack.dev\
 $(DD)tiff12nc.dev $(DD)tiff24nc.dev \
 $(DD)bmpamono.dev $(DD)bmpa16m.dev $(DD)bmpmono.dev $(DD)bmp16m.dev
# $(DD)bmpmono.dev $(DD)bmp16m.dev
!endif
# Generic makefile

# GS options
#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS    = $(DD)dps2lib.dev   \
                  $(DD)path1lib.dev  \
                  $(DD)patlib.dev    \
                  $(DD)rld.dev       \
                  $(DD)roplib.dev    \
                  $(DD)ttflib.dev    \
                  $(DD)colimlib.dev  \
                  $(DD)cielib.dev    \
                  $(DD)htxlib.dev    \
                  $(DD)devcmap.dev   \
		  $(DD)gsnogc.dev

# make sure the target directories exist - use special Watcom .BEFORE
.BEFORE
	if not exist $(GLGENDIR) mkdir $(GLGENDIR)
	if not exist $(GLOBJDIR) mkdir $(GLOBJDIR)
	if not exist $(PSGENDIR) mkdir $(PSGENDIR)
	if not exist $(PSOBJDIR) mkdir $(PSOBJDIR)

!include $(COMMONDIR)\watc_top.mak

# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak

# Main program.
!include $(PCLSRCDIR)\pcl_top.mak
