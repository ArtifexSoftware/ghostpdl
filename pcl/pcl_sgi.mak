#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_sgi.mak
# Top-level makefile for PCL5* on Silicon Graphics Irix 6.x platforms.

# Define the name of this makefile.
MAKEFILE    = ../pcl/pcl_sgi.mak

# Directories
GSDIR       = ../gs
GSSRCDIR    = ../gs
PLSRCDIR    = ../pl
PLGENDIR    = ../pl
PLOBJDIR    = ../pl
PCLSRCDIR   = ../pcl
PCLGENDIR   = ../pcl
PCLOBJDIR   = ../pcl
COMMONDIR   = ../common

TARGET_XE   = pcl5

# Assorted definitions.  Some of these should probably be factored out....
#
# -woff turns off specific warnings:
#
#     1174 - unused function parameter
#
#     1209 - constant value for control expression
#
#     1506 - loss of precision on implicit rounding
#
CDEFS       = -DSYSV
SGICFLAGS   = -mips4 -n32 -fullwarn             \
              -woff 1506 -woff 1174 -woff 1209  \
              $(CDEFS) $(XCFLAGS)
CFLAGS      = -g $(SGICFLAGS) $(XCFLAGS)
LDFLAGS     = -n32 -mips4 $(XLDFLAGS)
EXTRALIBS   =
XINCLUDE    = -I/usr/include
XLIBDIRS    = -L/usr/lib32
XLIBDIR     =
XLIBS       = Xt SM ICE Xext X11

CCLD        = cc

DEVICE_DEVS = x11mono.dev   \
              x11.dev       \
              x11alpha.dev  \
              x11cmyk.dev


#              djet500.dev   \
#              ljet4.dev     \
#              pcxmono.dev   \
#              pcxgray.dev   \
#              pbmraw.dev    \
#              pgmraw.dev    \
#              ppmraw.dev    \
#              pxlmono.dev   \
#              pxlcolor.dev

# Generic makefile
include $(PCLSRCDIR)/pcl_conf.mak
include $(COMMONDIR)/sgi_top.mak

# Subsystems
include $(PLSRCDIR)/pl.mak
include $(PCLSRCDIR)/pcl.mak

# Main program.
include $(PCLSRCDIR)/pcl_top.mak
