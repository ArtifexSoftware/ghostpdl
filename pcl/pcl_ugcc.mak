#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_ugcc.mak
# Top-level makefile for PCL5* on Unix/gcc platforms.

# Define the name of this makefile.
MAKEFILE=../pcl/pcl_ugcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
GLSRCDIR=../gs
PLSRCDIR=../pl
PCLSRCDIR=../pcl
COMMONDIR=../common

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
PLGENDIR=$(GENDIR)
PLOBJDIR=$(GENDIR)
PCLGENDIR=$(GENDIR)
PCLOBJDIR=$(GENDIR)

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
CONFIG=5
TARGET_DEVS=$(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev
TARGET_XE=$(PCLOBJDIR)/pcl5
MAIN_OBJ=$(PCLOBJDIR)/pcmain.$(OBJ)

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
#GCFLAGS=-Wall -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wwrite-strings
GCFLAGS=-Wall -Wpointer-arith -Wstrict-prototypes -Wcast-align
CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS)
LDFLAGS=$(XLDFLAGS)
EXTRALIBS=
XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt SM ICE Xext X11

CCLD=gcc

DEVICE_DEVS=x11mono.dev x11.dev x11alpha.dev x11cmyk.dev\
 djet500.dev ljet4.dev\
 pcx16.dev pcx256.dev\
 pcxmono.dev pcxcmyk.dev pcxgray.dev\
 pbmraw.dev pgmraw.dev ppmraw.dev pkmraw.dev\
 pxlmono.dev pxlcolor.dev cljet.dev\
 paintjet.dev

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems
include $(PLSRCDIR)/pl.mak
include $(PCLSRCDIR)/pcl.mak

# Main program.
include $(PCLSRCDIR)/pcl_top.mak
