#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_msvc.mak
# Top-level makefile for PCL5* on Win32 platforms using MS Visual C 4.1 or later

# Define the name of this makefile.
MAKEFILE=..\pcl\pcl_msvc.mak

# The build process will put all of its output in this directory:
GENDIR=.\obj

# The sources are taken from these directories:
GLSRCDIR=..\gs
PLSRCDIR=..\pl
PCLSRCDIR=..\pcl
COMMONDIR=..\common

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
PLGENDIR=$(GENDIR)
PLOBJDIR=$(GENDIR)
PCLGENDIR=$(GENDIR)
PCLOBJDIR=$(GENDIR)

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=pcl5
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
DEVICE_DEVS=djet500.dev ljet4.dev pcxmono.dev pcxgray.dev
!endif

# Generic platform-specific makefile
!include $(PCLSRCDIR)\pcl_conf.mak
!include $(COMMONDIR)\msvc_top.mak

# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak

# Main program.
!include $(PCLSRCDIR)\pcl_top.mak

