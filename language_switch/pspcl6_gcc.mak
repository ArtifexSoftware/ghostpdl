# Define the name of this makefile.
MAKEFILE=../language_switch/pspcl6_gcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
APPSRCDIR=.
GLSRCDIR=../gs/src
PSSRCDIR=../gs/src
PCLSRCDIR=../pcl
PLSRCDIR=../pl
PXLSRCDIR=../pxl
PSISRCDIR=../psi
COMMONDIR=../common
MAINSRCDIR=../main
PSLIBDIR=../gs/lib
ICCSRCDIR=../gs/icclib
PSRCDIR=../gs/libpng
PVERSION=10012

APP_CCC=$(CC_) -I../pl -I../gs/src -I./obj $(C_)


# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0

# specify the locate of the jpeg library.
JSRCDIR=../gs/jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)
JVERSION="6"

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
PSGENDIR=$(GENDIR)
PSOBJDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
PLGENDIR=$(GENDIR)
PLOBJDIR=$(GENDIR)
PXLGENDIR=$(GENDIR)
PCLGENDIR=$(GENDIR)
PSIGENDIR=$(GENDIR)
PXLOBJDIR=$(GENDIR)
PCLOBJDIR=$(GENDIR)
PSIOBJDIR=$(GENDIR)
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PXLOBJDIR)/pxl.dev $(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev

# Executable path\name w/o the .EXE extension
TARGET_XE=$(GENDIR)/pspcl6

PCL_TOP_OBJ=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)/pxtop.$(OBJ)
PSI_TOP_OBJ=$(PSIOBJDIR)/psitop.$(OBJ)

# Main file's name
MAIN_OBJ=$(PLOBJDIR)/plmain.$(OBJ) $(PLOBJDIR)/plimpl.$(OBJ)
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ) $(PSI_TOP_OBJ)

COMPILE_INITS=1

# note agfa gives it libraries incompatible names so they cannot be
# properly found by the linker.  Change the library names to reflect the
# following (i.e. the if library should be named libif.a
# NB - this should all be done automatically by choosing the device
# but it ain't.

# The user is responsible for building the agfa or freetype libs.  We
# don't overload the makefile with nonsense to build these libraries
# on the fly. If the artifex font scaler is chosen the makefiles will
# build the scaler automatically.

# Pick a font system technology.  PCL and XL do not need to use the
# same scaler, but it is necessary to tinker/hack the makefiles to get
# it to work properly.

# ufst - Agfa universal font scaler.
# fts - freetype font system.
# afs - artifex font scaler.
# 3 mutually exclusive choices follow, pick one.

PL_SCALER=fts
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

ifeq ($(PL_SCALER), ufst)
LDFLAGS=-Xlinker -L../pl/agfa/rts/lib/
# agfa does not use normalized library names (ie we expect libif.a not
# agfa's if_lib.a)
EXTRALIBS=-lif -lfco -ltt
AGFA_INCLUDES=-I../pl/agfa/rts/inc/ -I../pl/agfa/sys/inc/ -I../pl/agfa/rts/fco/ -I../pl/agfa/rts/gray/ -DAGFA_FONT_TABLE
endif

ifeq ($(PL_SCALER), fts)
LDFLAGS=
EXTRALIBS=-lfreetype
# the second include is to find ftbuild.h referenced in the FT
# tutorial.
FT_INCLUDES=-I/usr/include/freetype2 -I/usr/include/freetype2/freetype/config/
endif

ifeq ($(PL_SCALER), afs)
LDFLAGS=
EXTRALIBS=
endif

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)
PSICFLAGS=-DPSI_INCLUDED

# #define xxx_BIND is in std.h
# putting compile time bindings here will have the side effect of having different options
# based on application build.  PSI_INCLUDED is and example of this. 
EXPERIMENT_CFLAGS=

GCFLAGS=-Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings $(PSICFLAGS) $(EXPERIMENT_CFLAGS)
CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS) $(PSICFLAGS)

XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib -L/usr/X11R6/lib
XLIBDIR=
XLIBS=Xt SM ICE Xext X11

CCLD=gcc

DD='$(GLGENDIR)$(D)'

DEVICE_DEVS=$(DD)x11.dev $(DD)x11mono.dev $(DD)x11alpha.dev $(DD)x11cmyk.dev\
 $(DD)djet500.dev $(DD)ljet4.dev $(DD)cljet5pr.dev $(DD)cljet5c.dev\
 $(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcxcmyk.dev\
 $(DD)pxlmono.dev $(DD)pxlcolor.dev\
 $(DD)bmpmono.dev $(DD)bmpamono.dev $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)jpeg.dev

FEATURE_DEVS    = \
		  $(DD)psl3.dev		\
		  $(DD)pdf.dev		\
		  $(DD)dpsnext.dev	\
                  $(DD)htxlib.dev	\
                  $(DD)roplib.dev	\
		  $(DD)ttfont.dev	\
		  $(DD)pipe.dev

SYNC=posync
STDLIBS=-lm -lpthread


# Generic makefile
include $(COMMONDIR)/ugcc_top.mak
# Subsystems

include $(PLSRCDIR)/plps.mak
include $(PXLSRCDIR)/pxl.mak
include $(PCLSRCDIR)/pcl.mak
include $(PSISRCDIR)/psi.mak


# Main program.

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld.tr
	$(RMN_) $(PXLGEN)pconf.h $(PXLGEN)pconfig.h



#### Implementation stub
#$(PLOBJDIR)plimpl.$(OBJ): ./plimpl.c \
#                        $(memory__h)          \
#                        $(scommon_h)          \
#                        $(gxdevice_h)         \
#                        $(pltop_h)
#	$(APP_CCC) ./plimpl.c $(PLO_)plimpl.$(OBJ)

