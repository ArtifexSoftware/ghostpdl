# Define the name of this makefile.
MAKEFILE=../main/pcl6_gcc.mak

# define if this is a cygwin system.
CYGWIN=

# The build process will put all of its output in this directory:
GENDIR=./obj
PGGENDIR=./pgobj
# The sources are taken from these directories:
GLSRCDIR=../gs/src
PCLSRCDIR=../pcl
PLSRCDIR=../pl
PXLSRCDIR=../pxl
METSRCDIR=../met
COMMONDIR=../common
MAINSRCDIR=../main

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0
SHARE_LIBPNG=0

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
PLPLATFORM=

# specify the locate of the jpeg library.
JSRCDIR=../gs/jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)


# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
PLGENDIR=$(GENDIR)
PLOBJDIR=$(GENDIR)
PXLGENDIR=$(GENDIR)
PCLGENDIR=$(GENDIR)
METGENDIR=$(GENDIR)
PXLOBJDIR=$(GENDIR)
PCLOBJDIR=$(GENDIR)
METOBJDIR=$(GENDIR)

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PXLOBJDIR)/pxl.dev $(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev $(METOBJDIR)/met.dev
TARGET_XE=$(GENDIR)/pcl6
TARGET_LIB=$(GENDIR)/pcl6.a
MAIN_OBJ=$(PLOBJDIR)/plmain.$(OBJ) $(PLOBJDIR)/plimpl.$(OBJ)
PCL_TOP_OBJ=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)/pxtop.$(OBJ)
MET_TOP_OBJ=$(METOBJDIR)/mettop.$(OBJ)
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ) $(MET_TOP_OBJ)

# note agfa gives it libraries incompatible names so they cannot be
# properly found by the linker.  Change the library names to reflect the
# following (i.e. the if library should be named libif.a
# NB - this should all be done automatically by choosing the device
# but it ain't.

# The user is responsible for building the agfa or freetype libs.  We
# don't overload the makefile with nonsense to build these libraries
# on the fly. If the artifex font scaler is chosen the makefiles will
# build the scaler automatically.

# Pick (uncomment) one font system technology ufst or artifex.  PCL and
# XL do not need to use the same scaler, but it is necessary to
# tinker/hack the makefiles to get it to work properly.

# PL_SCALER=ufst
PL_SCALER=afs
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

# to compile in the fonts uncomment the following (this option only
# works with the artifex scaler on a unix platform)

# ROMFONTS=true

# flags for UFST scaler.
ifeq ($(PL_SCALER), ufst)
AGFA_LIB=../pl/agfa/rts/lib/
# hack!!! we append this onto TOP_OBJ to avoid creating another
# makefile variable.
EXTRALIBS= $(AGFA_LIB)if_lib.a $(AGFA_LIB)fco_lib.a $(AGFA_LIB)tt_lib.a  $(AGFA_LIB)if_lib.a
# agfa does not use normalized library names (ie we expect libif.a not
# agfa's if_lib.a)
AGFA_INCLUDES=-I../pl/agfa/rts/inc/ -I../pl/agfa/sys/inc/ -I../pl/agfa/rts/fco/ -I../pl/agfa/rts/gray/ -I../pl/agfa/rts/tt/ -DAGFA_FONT_TABLE -DGCCx86
endif

# flags for artifex scaler
ifeq ($(PL_SCALER), afs)

# substitue dingbats for missin wingdings font in PXL
WING_DING_SUB=-DWINGDING_DINGBATS_SUB_ON
XLDFLAGS=
EXTRALIBS=
AGFA_OBJ=
ifeq ($(ROMFONTS), true)
PL_SCALER=afsr
XLDFLAGS=-L../pl/ 
EXTRALIBS=-lttffont

endif

endif

# include the xml parser library
EXTRALIBS+=-lexpat

# a 64 bit type is needed for devicen color space/model support but
# carries a performance burden.  Use this definition (uncomment) for
# devicen support.

# GX_COLOR_INDEX_DEFINE=-DGX_COLOR_INDEX_TYPE="unsigned long long"

# and this definition if devicen support is not required

GX_COLOR_INDEX_DEFINE=

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)
# disable assert() with -DNDEBUG

GCFLAGS=-Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings -DNDEBUG $(GX_COLOR_INDEX_DEFINE)
# CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS)
CFLAGS= $(GCFLAGS) $(XCFLAGS)

XINCLUDE=-I/usr/X11R6/include
XLIBDIRS=-L/usr/X11R6/lib
XLIBDIR=
XLIBS=Xt SM ICE Xext X11

CCLD=gcc

DD='$(GLGENDIR)$(D)'


# NB: hack in png read capability under the png output device instead of a Feature dev.
# obviously this is temporary 

DEVICES_DEVS=$(DD)ljet4.dev $(DD)djet500.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
   $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bitrgbtags.dev $(DD)tr_rgb.dev\
   $(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcxcmyk.dev $(DD)pswrite.dev $(DD)pdfwrite.dev\
   $(DD)pxlmono.dev $(DD)pxlcolor.dev\
   $(DD)bmpmono.dev $(DD)bmpsep8.dev $(DD)png16.dev \
   $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)jpeg.dev

FEATURE_DEVS=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev\
	     $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev\
             $(DD)ttflib.dev  $(DD)cielib.dev $(DD)pipe.dev $(DD)htxlib.dev\
	     $(DD)gsnogc.dev $(DD)sdctd.dev\
	     $(DD)psl3lib.dev $(DD)seprlib.dev $(DD)translib.dev\
	     $(DD)cidlib.dev $(DD)psf1lib.dev $(DD)psf0lib.dev $(DD)lzwd.dev 

# cygwin does not have threads at this time, so we don't include the
# thread library or asyncronous devices.  Also cygwin does not include
# X by default so we do not include the X devices either.

ifeq ($(CYGWIN), TRUE)
SYNC=
STDLIBS=-lm -ldl
DEVICE_DEVS=$(DEVICES_DEVS)
else
SYNC=posync
STDLIBS=-lm -lpthread -ldl
DEVICE_DEVS=$(DD)x11.dev $(DD)x11alpha.dev $(DD)x11mono.dev $(DD)x11cmyk.dev $(DEVICES_DEVS) $(DD)bmpamono.dev $(DD)bmpa16m.dev
endif

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems

include $(PLSRCDIR)/pl.mak
include $(PXLSRCDIR)/pxl.mak
include $(PCLSRCDIR)/pcl.mak
include $(METSRCDIR)/met.mak

# Main program.

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld.tr
	$(RMN_) $(PXLGEN)pconf.h $(PXLGEN)pconfig.h

lib: $(TARGET_LIB)
	echo Done lib.

#### Implementation stub
$(PLOBJDIR)plimpl.$(OBJ): $(PLSRCDIR)plimpl.c \
                        $(memory__h)          \
                        $(scommon_h)          \
                        $(gxdevice_h)         \
                        $(pltop_h)
