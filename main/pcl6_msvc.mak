# pcl6_msvc.mak
# Top-level makefile for PCL5* + PCL-XL on Win32 platforms using MS Visual C 4.1 or later

# Define the name of this makefile.
MAKEFILE=..\main\pcl6_msvc.mak

# The build process will put all of its output in this directory:
GENDIR=.\obj

# The sources are taken from these directories:
GLSRCDIR=..\gs\src
PLSRCDIR=..\pl
PCLSRCDIR=..\pcl
PXLSRCDIR=..\pxl
COMMONDIR=..\common
JSRCDIR=..\gs\jpeg
JVERSION=6
PSRCDIR=..\gs\libpng
PVERSION=10005
ZSRCDIR=..\gs\zlib

SHARE_ZLIB=0

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)

GLOBJDIR=$(GENDIR)

PLGENDIR=$(GENDIR)

PLOBJDIR=$(GENDIR)

PCLGENDIR=$(GENDIR)

PXLGENDIR=$(GENDIR)

PCLOBJDIR=$(GENDIR)

PXLOBJDIR=$(GENDIR)

JGENDIR=$(GENDIR)

JOBJDIR=$(GENDIR)

ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)

DD=$(GLGENDIR)

# Executable path\name w/o the .EXE extension
TARGET_XE=$(GENDIR)\pcl6

# Debugging options
DEBUG=1
TDEBUG=1
NOPRIVATE=0

# Banding options
BAND_LIST_STORAGE=memory
BAND_LIST_COMPRESSOR=zlib

# Target options
CPU_TYPE=586
FPU_TYPE=0

BAND_LIST_STORAGE=file
BAND_LIST_COMPRESSOR=zlib

# Define which major version of MSVC is being used (currently, 4, 5, & 6 supported)
#       default to the latest version
MSVC_VERSION=6

D=\\

# Main file's name
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ) $(PLOBJDIR)\plimpl.$(OBJ)
PCL_TOP_OBJ=$(PCLOBJDIR)\pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)\pxtop.$(OBJ)
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

PCL_FONT_SCALER=afs
PXL_FONT_SCALER=afs

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PXLOBJDIR)\pjl.dev $(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev

DEVICE_DEVS=$(DD)\ljet4.dev\
 $(DD)\bmpmono.dev $(DD)\bmp16m.dev $(DD)\bmp32b.dev\
 $(DD)\bitcmyk.dev $(DD)\bitrgb.dev $(DD)\bit.dev\
 $(DD)\pkmraw.dev $(DD)\ppmraw.dev $(DD)\pgmraw.dev $(DD)\pbmraw.dev\
 $(DD)\pcx16.dev $(DD)\pcx256.dev $(DD)\pcx24b.dev\
 $(DD)\cljet5.dev\
 $(DD)\pcxmono.dev $(DD)\pcxcmyk.dev $(DD)\pcxgray.dev\
 $(DD)\pbmraw.dev $(DD)\pgmraw.dev $(DD)\ppmraw.dev $(DD)\pkmraw.dev\
 $(DD)\pxlmono.dev $(DD)\pxlcolor.dev\
 $(DD)\tiffcrle.dev $(DD)\tiffg3.dev $(DD)\tiffg32d.dev $(DD)\tiffg4.dev\
 $(DD)\tifflzw.dev $(DD)\tiffpack.dev\
 $(DD)\tiff12nc.dev $(DD)\tiff24nc.dev \
 $(DD)\pswrite.dev $(DD)\pdfwrite2.dev

# GS options
# Even though FEATURE_DEVS is defined in pcl_top.mak, define identically here
# for msvc_top.mak because nmake defines macros eagerly (i.e. here & now).
FEATURE_DEVS    = $(DD)\dps2lib.dev   \
                  $(DD)\path1lib.dev  \
                  $(DD)\patlib.dev    \
                  $(DD)\rld.dev       \
                  $(DD)\psl2cs.dev    \
                  $(DD)\roplib.dev    \
                  $(DD)\ttflib.dev    \
                  $(DD)\colimlib.dev  \
                  $(DD)\cielib.dev    \
                  $(DD)\htxlib.dev    \
                  $(DD)\devcmap.dev   \
                  $(DD)\psl3lib.dev   \
                  $(DD)\seprlib.dev   \
                  $(DD)\translib.dev  \
                  $(DD)\cidlib.dev    \
                  $(DD)\psf1lib.dev   \
                  $(DD)\sdctd.dev



default: $(TARGET_XE).exe
        echo Done.

clean: config-clean

config-clean:
        $(RMN_) $(PXLGEN)\pconf.h $(PXLGEN)\pconfig.h

#### Implementation stub
$(PLOBJDIR)\plimpl.$(OBJ): $(PLSRCDIR)\plimpl.c \
                            $(memory__h) \
                            $(scommon_h) \
                            $(gxdevice_h) \
                            $(pltop_h)

!include $(COMMONDIR)\msvc_top.mak

# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak
!include $(PXLSRCDIR)\pxl.mak
