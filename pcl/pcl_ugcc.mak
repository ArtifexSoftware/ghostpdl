#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_ugcc.mak
# Top-level makefile for PCL5* on Unix/gcc platforms.

# Define the name of this makefile.
MAKEFILE=../pcl/pcl_ugcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
GLSRCDIR=../gs/src
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
MAIN_OBJ=$(PLOBJDIR)/plmain.$(OBJ) $(PCLOBJDIR)/pcimpl.$(OBJ)
TOP_OBJ=$(PCLOBJDIR)/pctop.$(OBJ)

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0

JSRCDIR=../gs/jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)

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

DD='$(GLGENDIR)$(D)'
DEVICE_DEVS=$(DD)x11mono.dev $(DD)x11.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev\
 $(DD)djet500.dev $(DD)ljet4.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
 $(DD)pcx16.dev $(DD)pcx256.dev\
 $(DD)pcxmono.dev $(DD)pcxcmyk.dev $(DD)pcxgray.dev\
 $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)pkmraw.dev\
 $(DD)pxlmono.dev $(DD)pxlcolor.dev\
 $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev\
 $(DD)tifflzw.dev $(DD)tiffpack.dev\
 $(DD)tiff12nc.dev $(DD)tiff24nc.dev\
 $(DD)bit.dev $(DD)bitrgb.dev $(DD)bitcmyk.dev \
 $(DD)jpeg.dev \
 $(DD)bmpmono.dev $(DD)bmpamono.dev $(DD)bmpa16m.dev $(DD)bmpa32b.dev

FEATURE_DEVS    = $(DD)dps2lib.dev   \
                  $(DD)path1lib.dev  \
                  $(DD)patlib.dev    \
                  $(DD)rld.dev       \
                  $(DD)roplib.dev    \
                  $(DD)ttflib.dev    \
                  $(DD)colimlib.dev  \
                  $(DD)cielib.dev    \
                  $(DD)htxlib.dev    \
	          $(DD)pipe.dev      \
                  $(DD)devcmap.dev   \
		  $(DD)gsnogc.dev

# posync for most unix variants bsd needs fbsdsync.
SYNC=posync

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems
include $(PLSRCDIR)/pl.mak
include $(PCLSRCDIR)/pcl.mak

# Main program.
include $(PCLSRCDIR)/pcl_top.mak
