# The "?=" style of this makefile is designed to facilitate "deriving"
# your own make file from it by setting your own custom options, then include'ing
# this file. In its current form, this file will compile using default options
# and locations. It is recommended that you make any modifications to settings
# in this file by creating your own makefile which includes this one.
#

# Define the name of this makefile.
MAKEFILE+= ../main/pcl6_gcc.mak

# Frequently changed configuration options follow:

# Pick (uncomment) one font system technology
# ufst - Agfa universal font scaler.
# afs  - Artifex font scaler (gs native).
# PL_SCALER?=ufst
PL_SCALER?=afs

# if 1 this will build the x11 devices.
WANT_X11?=1

# Embed the fonts in the executable.
BUNDLE_FONTS?=1

# define if this is a cygwin system, we should get rid of this.
CYGWIN?=

# extra cflags
XCFLAGS?=

# The build process will put all of its output in this directory
GENDIR?=./obj
PGGENDIR?=./pgobj

# The sources are taken from these directories:
GLSRCDIR?=../gs/base
PCLSRCDIR?=../pcl
PLSRCDIR?=../pl
PXLSRCDIR?=../pxl
ICCSRCDIR?=../gs/icclib
COMMONDIR?=../common
MAINSRCDIR?=../main
PSSRCDIR?=../gs/psi

# Specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR?=../gs/zlib
ZGENDIR?=$(GENDIR)
ZOBJDIR?=$(GENDIR)
SHARE_ZLIB?=0

# Specify the location of libpng.
PNGSRCDIR?=../gs/libpng
# only relevant if not shared
PNGCCFLAGS?=-DPNG_USER_MEM_SUPPORTED
SHARE_LIBPNG?=0

# Specify the location of lcms
LCMSSRCDIR?=../gs/lcms
SHARE_LCMS?=0

# Specify the location of imdi
IMDISRCDIR?=../gs/imdi

# PCL_INCLUDED means PCL + PCL XL
PDL_INCLUDE_FLAGS?=-DPCL_INCLUDED

# This is constant in PCL, XPS and SVG, do not change it.  A ROM file
# system is always needed for the icc profiles.
COMPILE_INITS=1

# PLPLATFORM should be set to 'ps' for language switch builds and null
# otherwise.
PLPLATFORM?=

# specify the location and setup of the jpeg library.
JSRCDIR?=../gs/jpeg
JGENDIR?=$(GENDIR)
JOBJDIR?=$(GENDIR)
SHARE_JPEG?=0

# specify the location and setup of the tiff library.
SHARE_LIBTIFF?=0
TIFFSRCDIR?=../gs/tiff
TIFFPLATFORM?=unix

# ridiculousness, not worth documenting.
AK?=

# specify if banding should be memory or file based, and choose a
# compression method.
BAND_LIST_STORAGE=memory
BAND_LIST_COMPRESSOR=zlib

# file implementation for high level devices.
FILE_IMPLEMENTATION=stdio

# stdio
STDIO_IMPLEMENTATION=c

# taken from ugcclib.mak... hmmph.
gsdir?=/usr/local/share/ghostscript
gsdatadir?=$(gsdir)/$(GS_DOT_VERSION)
GS_DOCDIR?=$(gsdatadir)/doc
GS_LIB_DEFAULT?=$(gsdatadir)/lib:$(gsdatadir)/Resource/Font:$(gsdir)/fonts
SEARCH_HERE_FIRST?=1
GS_INIT?=gs_init.ps
GS?=
# end hmmph


# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR?=$(GENDIR)
GLOBJDIR?=$(GENDIR)
PLGENDIR?=$(GENDIR)
PLOBJDIR?=$(GENDIR)
PXLGENDIR?=$(GENDIR)
PXLOBJDIR?=$(GENDIR)
PCLGENDIR?=$(GENDIR)
PCLOBJDIR?=$(GENDIR)
XPSGENDIR?=$(GENDIR)
XPSOBJDIR?=$(GENDIR)
SVGGENDIR?=$(GENDIR)
SVGOBJDIR?=$(GENDIR)

TARGET_DEVS?=$(PXLOBJDIR)/pjl.dev $(PXLOBJDIR)/pxl.dev $(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev 
TARGET_XE?=$(GENDIR)/pcl6
TARGET_LIB?=$(GENDIR)/pcl6.a
MAIN_OBJ?=$(PLOBJDIR)/plmain.$(OBJ) $(PLOBJDIR)/plimpl.$(OBJ)
REALMAIN_OBJ?=$(PLOBJDIR)/realmain.$(OBJ)
REALMAIN_SRC?=realmain
PCL_TOP_OBJ?=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ?=$(PXLOBJDIR)/pxtop.$(OBJ)
TOP_OBJ?=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

# In theory XL and PCL could be built with different font scalers so
# we provide 2 font scaler variables.
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

# flags for UFST scaler.
ifeq ($(PL_SCALER), ufst)
  LONG_BITS := $(shell getconf LONG_BIT)
  ifeq ($(LONG_BITS), 64)
       GCCCONF=-DGCCx86_64
  else
       GCCCONF=-DGCCx86
  endif
  UFST_ROOT?=../ufst
  UFST_BRIDGE?=1
  UFST_LIB_EXT?=.a
  UFST_LIB?=$(UFST_ROOT)/rts/lib/
  UFST_CFLAGS?=-DGCCx86 -DUFST_ROOT=$(UFST_ROOT)
  UFST_INCLUDES?=-I$(UFST_ROOT)/rts/inc/ -I$(UFST_ROOT)/sys/inc/ -I$(UFST_ROOT)/rts/fco/ \
                 -I$(UFST_ROOT)/rts/gray/ -I$(UFST_ROOT)/rts/tt/ -DAGFA_FONT_TABLE
  # fco's are always bundled in the executable
  ifeq ($(BUNDLE_FONTS), 1)
    UFST_ROMFS_ARGS?=-b \
                     -P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
                     -P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
                     -c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO
    UFSTFONTDIR?=%rom%fontdata/
  else
    UFSTFONTDIR?=/usr/local/fontdata5.0/
  endif
  EXTRALIBS?= $(UFST_LIB)if_lib.a $(UFST_LIB)fco_lib.a $(UFST_LIB)tt_lib.a  $(UFST_LIB)if_lib.a
endif # PL_SCALER

# Flags for artifex scaler
ifeq ($(PL_SCALER), afs)
  # The mkromfs arguments for including the PCL fonts if BUNDLE_FONTS=1
  ifeq ($(BUNDLE_FONTS), 1)
    PCLXL_ROMFS_ARGS?= -c -P ../urwfonts -d ttfonts /*.ttf
  endif # BUNDLE_FONTS

  XLDFLAGS=
  EXTRALIBS=
  UFST_OBJ=
endif # PL_SCALER = afs

# a 64 bit type is needed for devicen color space/model support but
# carries a performance burden.  Change unsigned long to unsigned long
# long to enable large color indices.
GX_COLOR_INDEX_DEFINE?=-DGX_COLOR_INDEX_TYPE="unsigned long"

include ../config.mak

HAVE_STDINT_H_DEFINE?=-DHAVE_STDINT_H
HAVE_NO_STRICT_ALIASING_WARNING?=-Wno-strict-aliasing

GCFLAGS?=-Wall -Wundef -Wstrict-prototypes -Wmissing-declarations \
         -Wmissing-prototypes -Wpointer-arith \
         -Wwrite-strings $(HAVE_NO_STRICT_ALIASING_WARNING) \
         -fno-builtin -fno-common $(CONFDEFS) \
          $(HAVE_STDINT_H_DEFINE) $(GX_COLOR_INDEX_DEFINE) \
          $(PSICFLAGS) $(PDL_INCLUDE_FLAGS)

CFLAGS?= $(GCFLAGS) $(XCFLAGS)

XINCLUDE?=-I/usr/X11R6/include
XLIBDIRS?=-L/usr/X11R6/lib
XLIBDIR?=
XLIBS?=Xt SM ICE Xext X11

CCLD?=gcc

DD?=$(GLGENDIR)/


DEVICES_DEVS?=$(DD)ljet4.dev $(DD)djet500.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
   $(DD)bit.dev $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bitrgbtags.dev \
   $(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcxcmyk.dev $(DD)pdfwrite.dev $(DD)pswrite.dev $(DD)ps2write.dev\
   $(DD)pamcmyk32.dev $(DD)pamcmyk4.dev\
   $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev\
   $(DD)tifflzw.dev $(DD)tiffpack.dev $(DD)tiffgray.dev $(DD)tiffscaled.dev\
   $(DD)tiff12nc.dev $(DD)tiff24nc.dev\
   $(DD)pxlmono.dev $(DD)pxlcolor.dev\
   $(DD)bmpmono.dev $(DD)bmp16m.dev $(DD)bmpsep8.dev \
   $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev \
   $(DD)png16m.dev $(DD)pngmono.dev $(DD)jpeg.dev\
   $(DD)wtscmyk.dev $(DD)wtsimdi.dev\
   $(DD)romfs$(COMPILE_INITS).dev

FEATURE_DEVS?=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev\
	     $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev\
	     $(DD)gxfapiu$(UFST_BRIDGE).dev\
             $(DD)ttflib.dev  $(DD)cielib.dev $(DD)pipe.dev $(DD)htxlib.dev\
	     $(DD)sdctd.dev $(DD)libpng_$(SHARE_LIBPNG).dev\
	     $(DD)psl3lib.dev $(DD)seprlib.dev $(DD)translib.dev $(DD)psl2lib.dev\
	     $(DD)cidlib.dev $(DD)psf0lib.dev $(DD)psf1lib.dev $(DD)psf2lib.dev\
	     $(DD)lzwd.dev $(DD)sicclib.dev

# cygwin does not have threads at this time, so we don't include the
# thread library
ifeq ($(CYGWIN), TRUE)
  SYNC=nosync
  CFLAGS+=-DHAVE_STDINT_H
  STDLIBS=-lm
else
  SYNC=posync
  # some systems may need -ldl as well as pthread
  STDLIBS=-lm -lpthread -ldl
endif

ifeq ($(WANT_X11), 1)
  DEVICE_DEVS?=$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11mono.dev $(DD)x11cmyk.dev $(DEVICES_DEVS)
else
  DEVICE_DEVS?=$(DEVICES_DEVS)
endif

#miscellaneous
XOBJS?=$(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Windows eneds a different set of C flags, so pl.mak uses PLATCCC
# gcc doesn't need this, so use the same set.
PLATCCC=$(PLCCC)

# Subsystems

include $(PLSRCDIR)/pl.mak
include $(PCLSRCDIR)/pcl.mak
include $(PXLSRCDIR)/pxl.mak

# Main program.

pdl-default: $(TARGET_XE)$(XE)
	echo Done.

lib: $(TARGET_LIB)
	echo Done lib.

#### Implementation stub
$(PLOBJDIR)plimpl.$(OBJ): $(PLSRCDIR)plimpl.c \
                        $(memory__h)          \
                        $(scommon_h)          \
                        $(gxdevice_h)         \
                        $(pltop_h)
