# Define the name of this makefile.
MAKEFILE=..\language_switch\pspcl6_msvc.mak

# The build process will put all of its output in this directory:
GENDIR=.\obj

# The sources are taken from these directories:
APPSRCDIR=.
GLSRCDIR=..\gs\src
PSSRCDIR=..\gs\src
PCLSRCDIR=..\pcl
PLSRCDIR=..\pl
PXLSRCDIR=..\pxl
PSISRCDIR=..\psi
COMMONDIR=..\common
MAINSRCDIR=..\main
PSLIBDIR=..\gs\lib
ICCSRCDIR=..\gs\icclib
PSRCDIR=..\gs\libpng
PVERSION=10012

APP_CCC=$(CC_) -I..\pl -I..\gs\src -I.\obj $(C_)


# specify the location of zlib.  We use zlib for bandlist compression.
ZSRCDIR=..\gs\zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)
SHARE_ZLIB=0

# specify the locate of the jpeg library.
JSRCDIR=..\gs\jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)
JVERSION=6

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR=$(GENDIR)
ICCGENDIR=$(GENDIR)
ICCOBJDIR=$(GENDIR)
PSGENDIR=$(GENDIR)
PSOBJDIR=$(GENDIR)
GLOBJDIR=$(GENDIR)
GLOBJ=$(GENDIR)
GLGEN=$(GENDIR)
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
TARGET_DEVS=$(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev

# Executable path\name w/o the .EXE extension
TARGET_XE=$(GENDIR)\pspcl6
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ) $(PLOBJDIR)\plimpl.$(OBJ)
PCL_TOP_OBJ=$(PCLOBJDIR)\pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)\pxtop.$(OBJ)
PSI_TOP_OBJ=$(PSIOBJDIR)\psitop.$(OBJ)

# Main file's name
MAIN_OBJ=$(PLOBJDIR)\plmain.$(OBJ) $(PLOBJDIR)\plimpl.$(OBJ)
TOP_OBJ=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ) $(PSI_TOP_OBJ)

# Debugging options
DEBUG=1
TDEBUG=1
NOPRIVATE=0

# Banding options
BAND_LIST_STORAGE=file
BAND_LIST_COMPRESSOR=zlib

# Target options
CPU_TYPE=586
FPU_TYPE=0

COMPILE_INITS=1

# Define which major version of MSVC is being used (currently, 4, 5, & 6 supported)
#       default to the latest version
MSVC_VERSION=6

D=\\
DD=$(GLGENDIR)
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

PL_SCALER=afs
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

# specify agfa library locations and includes.  This is ignored
# if the current scaler is not the AGFA ufst.  Note we assume the agfa
# directory is under the shared pcl pxl library ..\pl
AGFA_ROOT=\cygwin\home\Administrator\ufst
UFST_LIBDIR=$(AGFA_ROOT)\rts\lib
AGFA_INCLUDES=$(I_)$(AGFA_ROOT)\rts\inc $(I_)$(AGFA_ROOT)\sys\inc $(I_)$(AGFA_ROOT)\rts\fco $(I_)$(AGFA_ROOT)\rts\gray -DMSVC


# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)

XCFLAGS=/DPSI_INCLUDED
PSICFLAGS=/DPSI_INCLUDED

# #define xxx_BIND is in std.h
# putting compile time bindings here will have the side effect of having different options
# based on application build.  PSI_INCLUDED is and example of this. 
EXPERIMENT_CFLAGS=

D=\\

DEVICE_DEVS=$(DD)\ljet4.dev $(DD)\cljet5pr.dev $(DD)\cljet5c.dev\
 $(DD)\pcxmono.dev $(DD)\pcxgray.dev $(DD)\pcxcmyk.dev\
 $(DD)\pxlmono.dev $(DD)\pxlcolor.dev\
 $(DD)\tiffcrle.dev $(DD)\tiffg3.dev $(DD)\tiffg32d.dev $(DD)\tiffg4.dev\
 $(DD)\tifflzw.dev $(DD)\tiffpack.dev\
 $(DD)\tiff12nc.dev $(DD)\tiff24nc.dev\
 $(DD)\bmpmono.dev $(DD)\bmpamono.dev $(DD)\pbmraw.dev $(DD)\pgmraw.dev\
 $(DD)\ppmraw.dev $(DD)\jpeg.dev

FEATURE_DEVS    = \
		  $(DD)\psl3.dev		\
		  $(DD)\pdf.dev		\
		  $(DD)\dpsnext.dev	\
                  $(DD)\htxlib.dev	\
                  $(DD)\roplib.dev	\
		  $(DD)\ttfont.dev	\
		  $(DD)\pipe.dev


# Main program.

default: $(TARGET_XE).exe
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld.tr
	$(RMN_) $(PXLGEN)pconf.h $(PXLGEN)pconfig.h


!include $(COMMONDIR)\msvc_top.mak

# Subsystems
!include $(PLSRCDIR)\plps.mak
!include $(PCLSRCDIR)\pcl.mak
!include $(PXLSRCDIR)\pxl.mak
!include $(PSISRCDIR)\psi.mak

