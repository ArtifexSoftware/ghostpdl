# Top-level makefile for PCL6 (PCL-5 + PCL-XL) on MS-DOS/Watcom platforms.

# Define the name of this makefile.
MAKEFILE=$(MAINSRCDIR)\PCL6_watc.mak

# The build process will put all of its output in this directory:
GENDIR=obj

# The sources are taken from these directories:
GLSRCDIR=..\gs\src
PCLSRCDIR=..\pcl
PLSRCDIR=..\pl
PXLSRCDIR=..\pxl
COMMONDIR=..\common
MAINSRCDIR=..\main

# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=..\gs\zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0

# specify the locate of the jpeg library.
JSRCDIR=..\gs\jpeg
JVERSION=6
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)

# specify the location of the PNG library
PSRCDIR=..\gs\libpng
PVERSION=10002

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
TARGET_DEVS=$(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev \
	$(PLOBJDIR)\hpgl2c.dev
TARGET_XE=$(GENDIR)\pcl6
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ)
IMPL_OBJ=$(PLOBJDIR)\plimpl.$(OBJ)
PCL_TOP_OBJ=$(PCLOBJDIR)\pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)\pxtop.$(OBJ)
TOP_OBJ=$(IMPL_OBJ) $(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

# Debugging options
# These use the conditional to allow command line definition.
# Watcom wmake doesn't act like other makes and would override
# values set on the command line without these conditionals.
!ifndef DEBUG
DEBUG=0
!endif
!ifndef TDEBUG
TDEBUG=0
!endif
!ifndef NOPRIVATE
NOPRIVATE=0
!endif

# Target options
!ifndef CPU_TYPE
CPU_TYPE=586
!endif
!ifndef FPU_TYPE
FPU_TYPE=0
!endif

# Assorted definitions.  Some of these should probably be factored out....
!ifndef WCVERSION
WCVERSION=10.695
!endif

# For building on 16 bit platforms (which may not work anymore) use 'wmakel'
MAKE=wmake
# Generic makefile

# make sure the target directories exist - use special Watcom .BEFORE
.BEFORE
	if not exist $(GLGENDIR) mkdir $(GLGENDIR)
	if not exist $(GLOBJDIR) mkdir $(GLOBJDIR)

CCLD=wcl386

D=\
DD=$(GENDIR)$(D) 

DEVICE_DEVS=$(DD)ljet4.dev\
 $(DD)bmpmono.dev $(DD)bmp16m.dev $(DD)bmp32b.dev\
 $(DD)bitcmyk.dev $(DD)bitrgb.dev $(DD)bit.dev\
 $(DD)pkmraw.dev $(DD)ppmraw.dev $(DD)pgmraw.dev $(DD)pbmraw.dev\
 $(DD)pcx16.dev $(DD)pcx256.dev $(DD)pcx24b.dev\
 $(DD)cljet5.dev\
 $(DD)pcxmono.dev $(DD)pcxcmyk.dev $(DD)pcxgray.dev\
 $(DD)pbmraw.dev $(DD)pgmraw.dev $(DD)ppmraw.dev $(DD)pkmraw.dev\
 $(DD)pxlmono.dev $(DD)pxlcolor.dev\
 $(DD)tiffcrle.dev $(DD)tiffg3.dev $(DD)tiffg32d.dev $(DD)tiffg4.dev\
 $(DD)tifflzw.dev $(DD)tiffpack.dev\
 $(DD)tiff12nc.dev $(DD)tiff24nc.dev \
 $(DD)pdfwrite.dev $(DD)pswrite.dev

FEATURE_DEVS=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev \
	     $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev \
             $(DD)ttflib.dev  $(DD)cielib.dev $(DD)htxlib.dev \
	     $(DD)devcmap.dev $(DD)gsnogc.dev

# Generic makefile
!include $(COMMONDIR)\watc_top.mak

# Subsystems

!include $(PLSRCDIR)\pl.mak
!include $(PXLSRCDIR)\pxl.mak
!include $(PCLSRCDIR)\pcl.mak

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
$(PLOBJDIR)\plimpl.$(OBJ): $(PLSRCDIR)\plimpl.c \
                        $(memory__h)          \
                        $(scommon_h)          \
                        $(gxdevice_h)         \
                        $(pltop_h)
