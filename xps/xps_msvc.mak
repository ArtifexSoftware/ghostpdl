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

# The build process will put all of its output in this directory:
# GENDIR is defined in the 'base' makefile, but we need its value immediately
!ifndef GENDIR
GENDIR=.\obj
!endif

# The sources are taken from these directories:
!ifndef MAINSRCDIR
MAINSRCDIR=..\main
!endif

!ifndef PSSRCDIR
PSSRCDIR=..\gs\src
!endif

!ifndef XPSSRCDIR
XPSSRCDIR=..\xps
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
TARGET_XE=$(GENDIR)\gxps
!endif

# Main file's name
# this is already in pcl6_gcc.mak
#XPS_TOP_OBJ=$(XPSOBJDIR)\xpstop.$(OBJ)
#TOP_OBJ+= $(XPS_TOP_OBJ)
#XCFLAGS=-DXPS_INCLUDED

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
# this is already in pcl6_gcc.mak
#include $(XPSSRCDIR)\xps.mak
!include $(PSSRCDIR)\expat.mak