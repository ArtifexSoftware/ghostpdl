# Define the name of this makefile.
MAKEFILE=../main/pcl6_gcc.mak

# The build process will put all of its output in this directory:
GENDIR=./obj

# The sources are taken from these directories:
GLSRCDIR=../gs/src
PCLSRCDIR=../pcl
PLSRCDIR=../pl
PXLSRCDIR=../pxl
COMMONDIR=../common
MAINSRCDIR=../main

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0

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
PXLOBJDIR=$(GENDIR)
PCLOBJDIR=$(GENDIR)

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PXLOBJDIR)/pxl.dev $(PCLOBJDIR)/pcl5c.dev $(PCLOBJDIR)/hpgl2c.dev
TARGET_XE=$(GENDIR)/pcl6
MAIN_OBJ=$(PLOBJDIR)/plmain.$(OBJ) $(PLOBJDIR)/plimpl.$(OBJ)
PCL_TOP_OBJ=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)/pxtop.$(OBJ)
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

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
 $(DD)pcxmono.dev $(DD)pcxgray.dev $(DD)pcxcmyk.dev $(DD)pswrite.dev\
 $(DD)pxlmono.dev $(DD)pxlcolor.dev\
 $(DD)bmpmono.dev $(DD)bmpamono.dev $(DD)bmpa16m.dev\
 $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)jpeg.dev

FEATURE_DEVS=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev \
	     $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev \
             $(DD)ttflib.dev  $(DD)cielib.dev $(DD)pipe.dev $(DD)htxlib.dev \
	     $(DD)devcmap.dev $(DD)gsnogc.dev $(DD)sdctd.dev

# Generic makefile
include $(COMMONDIR)/ugcc_top.mak

# Subsystems

include $(PLSRCDIR)/pl.mak
include $(PXLSRCDIR)/pxl.mak
include $(PCLSRCDIR)/pcl.mak

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
$(PLOBJDIR)plimpl.$(OBJ): $(PLSRCDIR)plimpl.c \
                        $(memory__h)          \
                        $(scommon_h)          \
                        $(gxdevice_h)         \
                        $(pltop_h)
