#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pxl_ugcc.mak
# Top-level makefile for PCL XL on Unix/gcc platforms.

# Define the name of this makefile.
MAKEFILE=../pxl/pxl_ugcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
GLSRCDIR=/home/henrys/gs5.30
PLSRCDIR=../pl
PXLSRCDIR=../pxl
COMMONDIR=../common

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
PLGENDIR=$(GENDIR)
PLOBJDIR=$(GENDIR)
PXLGENDIR=$(GENDIR)
PXLOBJDIR=$(GENDIR)

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
CONFIG=6
TARGET_DEVS=$(PXLOBJDIR)/pxl.dev
TARGET_XE=$(PXLOBJDIR)/pclxl
MAIN_OBJ=$(PXLOBJDIR)/pxmain.$(OBJ)

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)
#GCFLAGS=-Dconst= -Wall -Wpointer-arith -Wstrict-prototypes
GCFLAGS=-Wall -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wwrite-strings
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
include $(PXLSRCDIR)/pxl.mak

# Main program.
include $(PXLSRCDIR)/pxl_top.mak
