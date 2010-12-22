# pcl6_msvc.mak
# Top-level makefile for PCL5* + PCL-XL on Win32 platforms using MS Visual C 4.1 or later

# Define the name of this makefile.
MAKEFILE=$(MAKEFILE) ..\main\pcl6_msvc.mak

# Frequently changed configuration options follow:

# Pick (uncomment) one font system technology
# ufst - Agfa universal font scaler.
# afs  - Artifex font scaler (gs native).
!ifndef PL_SCALER
#PL_SCALER=ufst
PL_SCALER=afs
!endif

# Embed the fonts in the executable by default
!ifndef BUNDLE_FONTS
BUNDLE_FONTS=1
!endif

# extra cflags
!ifndef XCFLAGS
XCFLAGS=
!endif

# The build process will put all of its output in this directory:
!ifndef GENDIR
!if "$(DEBUG)"=="1"
GENDIR=.\debugobj
!else
GENDIR=.\obj
!endif
!endif

# The sources are taken from these directories:
!ifndef GLSRCDIR
GLSRCDIR=..\gs\base
!endif
!ifndef PCLSRCDIR
PCLSRCDIR=..\pcl
!endif
!ifndef PLSRCDIR
PLSRCDIR=..\pl
!endif
!ifndef PXLSRCDIR
PXLSRCDIR=..\pxl
!endif
!ifndef ICCSRCDIR
ICCSRCDIR=..\gs\icclib
!endif
!ifndef COMMONDIR
COMMONDIR=..\common
!endif

# Specify the location of zlib.  We use zlib for bandlist compression.
!ifndef ZSRCDIR
ZSRCDIR=..\gs\zlib
!endif
!ifndef ZGENDIR
ZGENDIR=$(GENDIR)
!endif
!ifndef ZOBJDIR
ZOBJDIR=$(GENDIR)
!endif
!ifndef SHARE_ZLIB
SHARE_ZLIB=0
!endif

# Specify the location of libpng.
!ifndef PNGSRCDIR
PNGSRCDIR=..\gs\libpng
!endif
!ifndef PNGCCFLAGS
PNGCCFLAGS=-DPNG_USER_MEM_SUPPORTED
!endif
!ifndef SHARE_LIBPNG
SHARE_LIBPNG=0
!endif

# Specify the location of lcms
!ifndef LCMSSRCDIR
LCMSSRCDIR=..\gs\lcms
!endif

# Specify the location of imdi
!ifndef IMDISRCDIR
IMDISRCDIR=..\gs\imdi
!endif

# PCL_INCLUDED means pcl + pcl xl
!ifndef PDL_INCLUDE_FLAGS
PDL_INCLUDE_FLAGS=/DPCL_INCLUDED
!endif

# This is constant in PCL, XPS and SVG, do not change it. A ROM file
# system us always needed for the icc profiles.
COMPILE_INITS=1

# PLPLATFORM should be set to 'ps' for language switch builds and null
# otherwise.
!ifndef PLPLATFORM
PLPLATFORM=
!endif

# specify the location and setup of the jpeg library.
!ifndef JSRCDIR
JSRCDIR=..\gs\jpeg
!endif
!ifndef JGENDIR
JGENDIR=$(GENDIR)
!endif
!ifndef JOBJDIR
JOBJDIR=$(GENDIR)
!endif

# specify the location and setup of the tiff library
!ifndef TIFFSRCDIR
TIFFSRCDIR=..\gs\tiff
TIFFCONFIG_SUFFIX=.vc
TIFFPLATFORM=win32
!endif

# specify if banding should be memory or file based, and choose a
# compression method
!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=memory
!endif
!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif


# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif

!ifndef GLOBJDIR
GLOBJDIR=$(GENDIR)
!endif

!ifndef PLGENDIR
PLGENDIR=$(GENDIR)
!endif

!ifndef PLOBJDIR
PLOBJDIR=$(GENDIR)
!endif

!ifndef PCLGENDIR
PCLGENDIR=$(GENDIR)
!endif

!ifndef PCLOBJDIR
PCLOBJDIR=$(GENDIR)
!endif
        
!ifndef PXLGENDIR
PXLGENDIR=$(GENDIR)
!endif

!ifndef PXLOBJDIR
PXLOBJDIR=$(GENDIR)
!endif

!ifndef DD
DD=$(GLGENDIR)
!endif

!ifndef XPSGENDIR
XPSGENDIR=$(GENDIR)
!endif

!ifndef XPSOBJDIR
XPSOBJDIR=$(GENDIR)
!endif

!ifndef SVGGENDIR
SVGGENDIR=$(GENDIR)
!endif

!ifndef SVGOBJDIR
SVGOBJDIR=$(GENDIR)
!endif

!ifndef SBRDIR
SBRDIR=$(GENDIR)
!endif

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
!ifndef TARGET_DEVS
TARGET_DEVS=$(PXLOBJDIR)\pjl.dev $(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev
!endif

# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\pcl6
!endif

# Main file's name
!ifndef MAIN_OBJ
!ifndef ALLOW_VD_TRACE
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ) $(PLOBJDIR)\plimpl.$(OBJ)
!else
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ) $(PLOBJDIR)\plimpl.$(OBJ) $(PLOBJDIR)\dwtrace.$(OBJ)\
 $(PLOBJDIR)\dwimg.$(OBJ) $(PLOBJDIR)\dwreg.$(OBJ)
!endif
!endif
!ifndef REALMAIN_OBJ
REALMAIN_OBJ=$(PLOBJDIR)\realmain.$(OBJ)
!endif
!ifndef PCL_TOP_OBJ
PCL_TOP_OBJ=$(PCLOBJDIR)\pctop.$(OBJ)
!endif
!ifndef PXL_TOP_OBJ
PXL_TOP_OBJ=$(PXLOBJDIR)\pxtop.$(OBJ)
!endif
!ifndef PSI_TOP_OBJ
PSI_TOP_OBJ=
!endif
!ifndef TOP_OBJ
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) $(SVG_TOP_OBJ)
!endif

# In theory XL and PCL could be build with different font scalers so
# we provide two different font scaler variables
!ifndef PCL_FONT_SCALER
PCL_FONT_SCALER=$(PL_SCALER)
!endif
!ifndef PXL_FONT_SCALER
PXL_FONT_SCALER=$(PL_SCALER)
!endif

# flags for UFST scaler.
!if "$(PL_SCALER)" == "ufst"

!ifndef UFST_ROOT
UFST_ROOT=..\ufst
!endif

UFST_BRIDGE=1

!ifndef UFST_LIB_EXT
UFST_LIB_EXT=.lib
!endif

!ifndef UFST_LIB
UFST_LIB=$(UFST_ROOT)\rts\lib\
!endif

!ifndef UFST_CFLAGS
UFST_CFLAGS= -DUFST_BRIDGE=$(UFST_BRIDGE) -DUFST_LIB_EXT=$(UFST_LIB_EXT) -DMSVC -DUFST_ROOT=$(UFST_ROOT)
!endif

!ifndef UFST_INCLUDES
UFST_INCLUDES=$(I_)$(UFST_ROOT)\rts\inc $(I_)$(UFST_ROOT)\sys\inc $(I_)$(UFST_ROOT)\rts\fco $(I_)$(UFST_ROOT)\rts\gray $(I_)$(UFST_ROOT)\rts\tt -DAGFA_FONT_TABLE
!endif

!if "$(BUNDLE_FONTS)" == "1"

!ifndef UFST_ROMFS_ARGS
UFST_ROMFS_ARGS=-b \
-P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
-P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
-c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO
!endif
!ifndef UFSTFONTDIR
UFSTFONTDIR=%rom%fontdata/
!endif

!else

!ifndef UFSTFONTDIR
UFSTFONTDIR=/usr/local/fontdata5.0/
!endif

!endif

!ifndef EXTRALIBS
EXTRALIBS= $(UFST_LIB)if_lib.lib $(UFST_LIB)fco_lib.lib $(UFST_LIB)tt_lib.lib $(UFST_LIB)if_lib.lib
!endif

!endif
# end PL_SCALER == ufst

# flags for artifex scaler
!if "$(PL_SCALER)" == "afs"
# The mkromfs arguments for including the PCL fonts if COMPILE_INITS=1
!ifndef PCLXL_ROMFS_ARGS
PCLXL_ROMFS_ARGS= -c -P ../urwfonts -d ttfonts /*.ttf
!endif

!endif
# end PL_SCALER == afs

# a 64 bit type is needed for devicen color space/model support but
# carries a performance burden.  Change unsigned long to unsigned long
# long to enable large color indices.
!ifndef GX_COLOR_INDEX_DEFINE
GX_COLOR_INDEX_DEFINE=-DGX_COLOR_INDEX_TYPE="unsigned long"
!endif

# Debugging options
!ifndef DEBUG
DEBUG=0
!endif
!ifndef TDEBUG
TDEBUG=0
!endif
!ifndef DEBUGSYM
DEBUGSYM=0
!endif

!ifndef NOPRIVATE
NOPRIVATE=0
!endif

# Target options
!ifndef CPU_TYPE
CPU_TYPE=586
!endif

# Default major version of MSVC to use;
# this should generally be the latest version.
!ifndef MSVC_VERSION
MSVC_VERSION=9
!endif

!ifndef D
D=\\
!endif

!ifdef XPS_INCLUDED
!ifndef XPS_TOP_OBJ
XPS_TOP_OBJ=$(XPSOBJDIR)/xpstop.$(OBJ)
XCFLAGS=$(XCFLAGS) /DXPS_INCLUDED
!endif
!endif

!ifdef SVG_INCLUDED
!ifndef SVG_TOP_OBJ
SVG_TOP_OBJ=$(SVGOBJDIR)/svgtop.$(OBJ)
XCFLAGS=$(XCFLAGS) /DSVG_INCLUDED
!endif
!endif

!ifdef WIN64
XCFLAGS=$(XCFLAGS) /DWIN64
!endif

XCFLAGS=$(XCFLAGS) $(PDL_INCLUDE_FLAGS)

!ifdef SBR
SBRFLAGS="/FR$(SBRDIR)\ "
!endif

!ifdef XPS_INCLUDED
TARGET_DEVS= $(TARGET_DEVS) $(XPSOBJDIR)/xps.dev
!endif

!ifndef DEVICE_DEVS
DEVICE_DEVS=$(DD)\ljet4.dev\
 $(DD)\wtsimdi.dev\
 $(DD)\bmpmono.dev $(DD)\bmp16m.dev $(DD)\bmp32b.dev\
 $(DD)\bitcmyk.dev $(DD)\bitrgb.dev $(DD)\bit.dev\
 $(DD)\pkmraw.dev $(DD)\ppmraw.dev $(DD)\pgmraw.dev $(DD)\pbmraw.dev\
 $(DD)\pcx16.dev $(DD)\pcx256.dev $(DD)\pcx24b.dev\
 $(DD)\cljet5.dev\
 $(DD)\pcxmono.dev $(DD)\pcxcmyk.dev $(DD)\pcxgray.dev\
 $(DD)\pbmraw.dev $(DD)\pgmraw.dev $(DD)\ppmraw.dev $(DD)\pkmraw.dev\
 $(DD)\pxlmono.dev $(DD)\pxlcolor.dev\
 $(DD)\tiffcrle.dev $(DD)\tiffg3.dev $(DD)\tiffg32d.dev $(DD)\tiffg4.dev\
 $(DD)\tifflzw.dev $(DD)\tiffpack.dev $(DD)\tiffgray.dev $(DD)\tiffscaled.dev \
 $(DD)\tiff12nc.dev $(DD)\tiff24nc.dev $(DD)\tiffscaled.dev \
 $(DD)\png16m.dev $(DD)\pngmono.dev $(DD)\jpeg.dev \
 $(DD)\pswrite.dev $(DD)\pdfwrite.dev \
 $(DD)\wtscmyk.dev $(DD)\wtsimdi.dev
!endif

# GS options
# Even though FEATURE_DEVS is defined in pcl_top.mak, define identically here
# for msvc_top.mak because nmake defines macros eagerly (i.e. here & now).
!ifndef FEATURE_DEVS    
FEATURE_DEVS    = $(DD)\dps2lib.dev   \
                  $(DD)\path1lib.dev  \
                  $(DD)\patlib.dev    \
		  $(DD)\gxfapiu$(UFST_BRIDGE).dev\
                  $(DD)\rld.dev       \
                  $(DD)\psl2cs.dev    \
                  $(DD)\roplib.dev    \
                  $(DD)\ttflib.dev    \
                  $(DD)\colimlib.dev  \
                  $(DD)\cielib.dev    \
                  $(DD)\htxlib.dev    \
                  $(DD)\psl3lib.dev   \
                  $(DD)\psl2lib.dev   \
		  $(DD)\libpng_$(SHARE_LIBPNG).dev \
                  $(DD)\seprlib.dev   \
                  $(DD)\translib.dev  \
                  $(DD)\cidlib.dev    \
                  $(DD)\psf1lib.dev   \
		  $(DD)\psf0lib.dev   \
                  $(DD)\sdctd.dev     \
                  $(DD)\psf2lib.dev   \
	          $(DD)\lzwd.dev      \
	          $(DD)\sicclib.dev
!endif


default: $(TARGET_XE).exe
        echo Done.

clean: config-clean clean-not-config-clean

#### Implementation stub
$(PLOBJDIR)\plimpl.$(OBJ): $(PLSRCDIR)\plimpl.c \
                            $(memory__h) \
                            $(scommon_h) \
                            $(gxdevice_h) \
                            $(pltop_h)

!include $(COMMONDIR)\msvc_top.mak

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld.tr
	$(RMN_) $(PXLGENDIR)\pconf.h $(PXLGENDIR)\pconfig.h


# Subsystems
!include $(PLSRCDIR)\pl.mak
!include $(PCLSRCDIR)\pcl.mak
!include $(PXLSRCDIR)\pxl.mak


#### Rules for visual tracer window (copied from gs\winint.mak).
#### Note PLCCC brings /Za, which can't compile Windows headers, so we define and use PLCCC_W instead. :

CC_W=$(CC_WX) $(COMPILE_FULL_OPTIMIZED) $(ZM)
PLCCC_W=$(CC_W) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

$(PLOBJDIR)\dwtrace.$(OBJ): $(GLSRC)dwtrace.c $(AK)\
 $(dwimg_h) $(dwtrace_h)\
 $(gscdefs_h) $(stdpre_h) $(gsdll_h) $(vdtrace_h)
	$(PLCCC_W) $(GLO_)dwtrace.$(OBJ) $(C_) $(GLSRC)dwtrace.c

$(PLOBJDIR)\dwimg.obj: $(GLSRC)dwimg.c $(AK)\
 $(dwmain_h) $(dwdll_h) $(dwtext_h) $(dwimg_h) $(gdevdsp_h) $(stdio__h) \
 $(gscdefs_h) $(iapi_h) $(dwreg_h)
	$(PLCCC_W)  $(GLO_)dwimg.obj $(C_) $(GLSRC)dwimg.c

$(PLOBJDIR)\dwreg.obj: $(GLSRC)dwreg.c $(AK) $(dwreg_h)
	$(PLCCC_W) $(GLO_)dwreg.obj $(C_) $(GLSRC)dwreg.c


!ifndef BSCFILE
BSCFILE=$(GENDIR)\pcl.bsc
!endif

bsc:
	bscmake /o $(BSCFILE) /v $(GENDIR)\*.sbr
