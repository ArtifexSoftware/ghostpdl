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
DEBUG=0
TDEBUG=0
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

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
TARGET_DEVS=$(PXLOBJDIR)\pjl.dev $(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev

DEVICE_DEVS2=$(DD)\epson.dev $(DD)\eps9high.dev $(DD)\eps9mid.dev $(DD)\epsonc.dev $(DD)\ibmpro.dev
DEVICE_DEVS3=$(DD)\deskjet.dev $(DD)\djet500.dev $(DD)\laserjet.dev $(DD)\ljetplus.dev $(DD)\ljet2p.dev
DEVICE_DEVS4=$(DD)\cdeskjet.dev $(DD)\cdjcolor.dev $(DD)\cdjmono.dev $(DD)\cdj550.dev
DEVICE_DEVS5=$(DD)\djet500c.dev $(DD)\declj250.dev $(DD)\lj250.dev
DEVICE_DEVS6=$(DD)\st800.dev $(DD)\stcolor.dev $(DD)\bj10e.dev $(DD)\bj200.dev
DEVICE_DEVS7=$(DD)\t4693d2.dev $(DD)\t4693d4.dev $(DD)\t4693d8.dev $(DD)\tek4696.dev
DEVICE_DEVS8=$(DD)\pcxmono.dev $(DD)\pcxgray.dev $(DD)\pcx16.dev $(DD)\pcx256.dev $(DD)\pcx24b.dev $(DD)\pcxcmyk.dev
DEVICE_DEVS9=$(DD)\pbm.dev $(DD)\pbmraw.dev $(DD)\pgm.dev $(DD)\pgmraw.dev $(DD)\pgnm.dev $(DD)\pgnmraw.dev
DEVICE_DEVS10=$(DD)\tiffcrle.dev $(DD)\tiffg3.dev $(DD)\tiffg32d.dev $(DD)\tiffg4.dev $(DD)\tifflzw.dev $(DD)\tiffpack.dev
DEVICE_DEVS11=$(DD)\bmpmono.dev $(DD)\bmpgray.dev $(DD)\bmp16.dev $(DD)\bmp256.dev $(DD)\bmp16m.dev $(DD)\tiff12nc.dev $(DD)\tiff24nc.dev
DEVICE_DEVS12=$(DD)\psmono.dev $(DD)\bit.dev $(DD)\bitrgb.dev $(DD)\bitcmyk.dev
DEVICE_DEVS13=$(DD)\pngmono.dev $(DD)\pnggray.dev $(DD)\png16.dev $(DD)\png256.dev $(DD)\png16m.dev
DEVICE_DEVS14=$(DD)\jpeg.dev $(DD)\jpeggray.dev
DEVICE_DEVS15=$(DD)\epswrite.dev $(DD)\pxlmono.dev $(DD)\pxlcolor.dev
# Overflow for DEVS3,4,5,6,9
DEVICE_DEVS16=$(DD)\ljet3.dev $(DD)\ljet3d.dev $(DD)\ljet4.dev $(DD)\ljet4d.dev
DEVICE_DEVS17=$(DD)\pj.dev $(DD)\pjxl.dev $(DD)\pjxl300.dev
DEVICE_DEVS18=$(DD)\jetp3852.dev $(DD)\r4081.dev $(DD)\lbp8.dev $(DD)\uniprint.dev
DEVICE_DEVS19=$(DD)\m8510.dev $(DD)\necp6.dev $(DD)\bjc600.dev $(DD)\bjc800.dev
DEVICE_DEVS20=$(DD)\pnm.dev $(DD)\pnmraw.dev $(DD)\ppm.dev $(DD)\ppmraw.dev
DEVICE_DEVS=\
             $(DEVICE_DEVS2)\
             $(DEVICE_DEVS3)\
             $(DEVICE_DEVS4)\
             $(DEVICE_DEVS5)\
             $(DEVICE_DEVS6)\
             $(DEVICE_DEVS7)\
             $(DEVICE_DEVS8)\
             $(DEVICE_DEVS8)\
             $(DEVICE_DEVS9)\
             $(DEVICE_DEVS10)\
             $(DEVICE_DEVS11)\
             $(DEVICE_DEVS12)\
             $(DEVICE_DEVS13)\
             $(DEVICE_DEVS14)\
             $(DEVICE_DEVS15)\
             $(DEVICE_DEVS17)\
             $(DEVICE_DEVS18)\
             $(DEVICE_DEVS19)\
             $(DEVICE_DEVS20)
# GS options
# Even though FEATURE_DEVS is defined in pcl_top.mak, define identically here
# for msvc_top.mak because nmake defines macros eagerly (i.e. here & now).
FEATURE_DEVS    = $(DD)\dps2lib.dev   \
                  $(DD)\path1lib.dev  \
                  $(DD)\patlib.dev    \
                  $(DD)\rld.dev       \
                  $(DD)\roplib.dev    \
                  $(DD)\ttflib.dev    \
                  $(DD)\colimlib.dev  \
                  $(DD)\cielib.dev    \
                  $(DD)\htxlib.dev    \
                  $(DD)\devcmap.dev   \
                  $(DD)\gsnogc.dev    \
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
