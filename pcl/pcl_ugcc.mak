#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_ugcc.mak
# Top-level makefile for PCL5* on Unix/gcc platforms.

# Define the name of this makefile.
MAKEFILE=../pcl/pcl_ugcc.mak

# Directories
GSDIR=../gs
GSSRCDIR=../gs
PLSRCDIR=../pl
PLGENDIR=../pl
PLOBJDIR=../pl
PCLSRCDIR=../pcl
PCLGENDIR=../pcl
PCLOBJDIR=../pcl
COMMONDIR=../common

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
CONFIG=5
TARGET_DEVS=$(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev
TARGET_XE=pcl5
MAIN_OBJ=$(PCLOBJDIR)/pcmain.$(OBJ)

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
#GCFLAGS=-Wall -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wwrite-strings
GCFLAGS=-Dconst= -Wall -Wpointer-arith -Wstrict-prototypes
CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS)
LDFLAGS=$(XLDFLAGS)
EXTRALIBS=
XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt SM ICE Xext X11

CCLD=gcc

DEVICE_DEVS=x11.dev x11mono.dev x11alpha.dev x11cmyk.dev\
 djet500.dev ljet4.dev\
 pcxmono.dev pcxgray.dev\
 pbmraw.dev pgmraw.dev ppmraw.dev

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems
include $(PLSRCDIR)/pl.mak
include $(PCLSRCDIR)/pcl.mak

# Main program.
include $(PCLSRCDIR)/pcl_top.mak
