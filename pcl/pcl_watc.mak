#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_watc.mak
# Top-level makefile for PCL5* on MS-DOS/Watcom platforms.

# Define the name of this makefile.
MAKEFILE=..\pcl\pcl_watc.mak

# Directories
GSDIR=\gs
GSSRCDIR=\gs
PLSRCDIR=..\pl
PLGENDIR=..\pl
PLOBJDIR=..\pl
PCLSRCDIR=..\pcl
PCLGENDIR=..\pcl
PCLOBJDIR=..\pcl
COMMONDIR=..\common

TARGET_XE=pcl5

# Debugging options
DEBUG=1
TDEBUG=1
NOPRIVATE=0

# Target options
CPU_TYPE=386
FPU_TYPE=0

# Assorted definitions.  Some of these should probably be factored out....
GS=gs386
WCVERSION=10.0

DEVICE_DEVS=vga.dev djet500.dev ljet4.dev pcxmono.dev pcxgray.dev

# Generic makefile

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

!include $(COMMONDIR)\watc_top.mak

# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak

# Main program.
!include $(PCLSRCDIR)\pcl_top.mak
