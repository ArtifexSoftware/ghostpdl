#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pxl_ugcc.mak
# Top-level makefile for PCL XL on Unix/gcc platforms.

# Define the name of this makefile.
MAKEFILE=../pxl/pxl_ugcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
GLSRCDIR=../gs/src
PLSRCDIR=../pl
PXLSRCDIR=../pxl
COMMONDIR=../common

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0


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
MAIN_OBJ=$(PLOBJDIR)/plmain.$(OBJ)  $(PXLOBJDIR)/pximpl.$(OBJ)
TOP_OBJ=$(PXLOBJDIR)/pxtop.$(OBJ)

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

DD='$(GLGENDIR)$(D)'
DEVICE_DEVS=$(DD)x11.dev $(DD)x11mono.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev\
 $(DD)djet500.dev $(DD)ljet4.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
 $(DD)pcxmono.dev $(DD)pcxgray.dev\
 $(DD)bmpmono.dev $(DD)bmpamono.dev $(DD)bmpa16m.dev\
 $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev

FEATURE_DEVS=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev $(DD)ttflib.dev  $(DD)cielib.dev $(DD)pipe.dev

# posync for most unix variants bsd needs fbsdsync.
SYNC=posync

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems
include $(PLSRCDIR)/pl.mak
include $(PXLSRCDIR)/pxl.mak

# Main program.
include $(PXLSRCDIR)/pxl_top.mak
