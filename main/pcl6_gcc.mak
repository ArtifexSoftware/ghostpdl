# The "?=" style of this makefile is designed to facilitate "deriving"
# your own make file from it by setting your own custom options, then include'ing
# this file. In its current form, this file will compile using default options
# and locations. It is recommended that you make any modifications to settings
# in this file by creating your own makefile which includes this one.
#

# Define the name of this makefile.
MAKEFILE+= ../main/pcl6_gcc.mak

# Pick (uncomment) one font system technology ufst or afs (gs native)
#PL_SCALER?=ufst
PL_SCALER?=afs

# define if this is a cygwin system.
CYGWIN?=

# extra cflags
XCFLAGS?=

# The build process will put all of its output in this directory:
GENDIR?=./obj
PGGENDIR?=./pgobj
# The sources are taken from these directories:
GLSRCDIR?=../gs/base
PCLSRCDIR?=../pcl
PLSRCDIR?=../pl
PXLSRCDIR?=../pxl
XPSSRCDIR?=../xps
SVGSRCDIR?=../svg
ICCSRCDIR?=../gs/icclib
COMMONDIR?=../common
MAINSRCDIR?=../main
PSSRCDIR?=../gs/psi

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR?=../gs/zlib
ZGENDIR?=$(GENDIR)
ZOBJDIR?=$(GENDIR)
SHARE_ZLIB?=0
SHARE_LIBPNG?=0

PNGSRCDIR?=../gs/libpng
# only relevant if not shared
PNGCCFLAGS?=-DPNG_USER_MEM_SUPPORTED

LCMSSRCDIR?=../gs/lcms
SHARE_LCMS?=0

IMDISRCDIR?=../gs/imdi

# PCL_INCLUDED means pcl + pcl xl
PDL_INCLUDE_FLAGS?=-DPCL_INCLUDED

# Choose COMPILE_INITS=1 for init files and fonts in ROM (otherwise =0)
COMPILE_INITS?=1

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
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
PCLGENDIR?=$(GENDIR)
XPSGENDIR?=$(GENDIR)
SVGGENDIR?=$(GENDIR)
PXLOBJDIR?=$(GENDIR)
PCLOBJDIR?=$(GENDIR)
XPSOBJDIR?=$(GENDIR)
SVGOBJDIR?=$(GENDIR)

TARGET_DEVS?=$(PXLOBJDIR)/pxl.dev $(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev 
TARGET_XE?=$(GENDIR)/pcl6
TARGET_LIB?=$(GENDIR)/pcl6.a
MAIN_OBJ?=$(PLOBJDIR)/plmain.$(OBJ) $(PLOBJDIR)/plimpl.$(OBJ)
PCL_TOP_OBJ?=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ?=$(PXLOBJDIR)/pxtop.$(OBJ)
TOP_OBJ?=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

# note agfa gives its libraries incompatible names so they cannot be
# properly found by the linker.  Change the library names to reflect the
# following (i.e. the if library should be named libif.a
# NB - this should all be done automatically by choosing the device
# but it ain't.

# The user is responsible for building the agfa or freetype libs.  We
# don't overload the makefile with nonsense to build these libraries
# on the fly. If the artifex font scaler is chosen the makefiles will
# build the scaler automatically.

# PCL and XL do not need to use the same scaler, but it is necessary to
# tinker/hack the makefiles to get it to work properly.

PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

# to compile in the fonts uncomment the following (this option only
# works with the artifex scaler on a unix platform)

# flags for UFST scaler.
ifeq ($(PL_SCALER), ufst)
UFST_ROOT?=../ufst
UFST_BRIDGE?=1
UFST_LIB_EXT?=.a
UFST_LIB?=$(UFST_ROOT)/rts/lib/
UFST_CFLAGS?=-DGCCx86 -DUFST_ROOT=$(UFST_ROOT)
UFST_INCLUDES?=-I$(UFST_ROOT)/rts/inc/ -I$(UFST_ROOT)/sys/inc/ -I$(UFST_ROOT)/rts/fco/ -I$(UFST_ROOT)/rts/gray/ -I$(UFST_ROOT)/rts/tt/ -DAGFA_FONT_TABLE
# fco's are binary (-b), the following is only used if COMPILE_INITS=1
UFST_ROMFS_ARGS?=-b \
-P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
-P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
-c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO

EXTRALIBS?= $(UFST_LIB)if_lib.a $(UFST_LIB)fco_lib.a $(UFST_LIB)tt_lib.a  $(UFST_LIB)if_lib.a
ifeq ($(COMPILE_INITS), 0)
UFSTFONTDIR?=/usr/local/fontdata5.0/
else
UFSTFONTDIR?=%rom%fontdata/
endif
endif # PL_SCALER = ufst

# flags for artifex scaler
ifeq ($(PL_SCALER), afs)
UFST_BRIDGE?=
# The mkromfs arguments for including the PCL fonts if COMPILE_INITS=1
PCLXL_ROMFS_ARGS?= -c -P ../urwfonts -d ttfonts /*.ttf
XLDFLAGS=
EXTRALIBS=
UFST_OBJ=
endif # PL_SCALER = afs

# a 64 bit type is needed for devicen color space/model support but
# carries a performance burden.  Change unsigned long to unsigned long
# long to enable large color indices.
GX_COLOR_INDEX_DEFINE?=-DGX_COLOR_INDEX_TYPE="unsigned long"

HAVE_STDINT_H_DEFINE?=-DHAVE_STDINT_H
HAVE_MKSTEMP_DEFINE?=-DHAVE_MKSTEMP
HAVE_HYPOT_DEFINE?=-DHAVE_HYPOT
HAVE_NO_STRICT_ALIASING_WARNING?=-Wno-strict-aliasing

GCFLAGS?=-Wall -Wundef -Wstrict-prototypes -Wmissing-declarations \
         -Wmissing-prototypes -Wpointer-arith \
         -Wwrite-strings $(HAVE_NO_STRICT_ALIASING_WARNING) \
         -fno-builtin -fno-common \
          $(HAVE_STDINT_H_DEFINE) $(HAVE_MKSTEMP_DEFINE) $(HAVE_HYPOT_DEFINE) \
          $(GX_COLOR_INDEX_DEFINE) $(PSICFLAGS) $(PDL_INCLUDE_FLAGS)

CFLAGS?= $(GCFLAGS) $(XCFLAGS)

XINCLUDE?=-I/usr/X11R6/include
XLIBDIRS?=-L/usr/X11R6/lib
XLIBDIR?=
XLIBS?=Xt SM ICE Xext X11

CCLD?=gcc

DD?=$(GLGENDIR)/


DEVICES_DEVS?=$(DD)ljet4.dev $(DD)djet500.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
   $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bitrgbtags.dev \
   $(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcxcmyk.dev $(DD)pswrite.dev $(DD)pdfwrite.dev\
   $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev\
   $(DD)tifflzw.dev $(DD)tiffpack.dev $(DD)tiffgray.dev $(DD)tiffscaled.dev\
   $(DD)tiff12nc.dev $(DD)tiff24nc.dev\
   $(DD)pxlmono.dev $(DD)pxlcolor.dev\
   $(DD)bmpmono.dev $(DD)bmp16m.dev $(DD)bmpsep8.dev \
   $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev \
   $(DD)png16m.dev $(DD)pngmono.dev $(DD)jpeg.dev\
   $(DD)wtscmyk.dev $(DD)wtsimdi.dev $(DD)imdi.dev\
   $(DD)romfs$(COMPILE_INITS).dev

FEATURE_DEVS?=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev\
	     $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev\
	     $(DD)gxfapiu$(UFST_BRIDGE).dev\
             $(DD)ttflib.dev  $(DD)cielib.dev $(DD)pipe.dev $(DD)htxlib.dev\
	     $(DD)gsnogc.dev $(DD)sdctd.dev $(DD)libpng_$(SHARE_LIBPNG).dev\
	     $(DD)psl3lib.dev $(DD)seprlib.dev $(DD)translib.dev $(DD)psl2lib.dev\
	     $(DD)cidlib.dev $(DD)psf0lib.dev $(DD)psf1lib.dev $(DD)psf2lib.dev\
	     $(DD)lzwd.dev

# cygwin does not have threads at this time, so we don't include the
# thread library
ifeq ($(CYGWIN), TRUE)
SYNC=nosync
CFLAGS+=-DHAVE_STDINT_H
STDLIBS=-lm
DEVICE_DEVS?=$(DEVICES_DEVS)

else

SYNC=posync
# some systems may need -ldl as well as pthread
STDLIBS=-lm -lpthread -ldl
DEVICE_DEVS?=$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11mono.dev $(DD)x11cmyk.dev $(DEVICES_DEVS)
endif

#miscellaneous
XOBJS?=$(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

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
