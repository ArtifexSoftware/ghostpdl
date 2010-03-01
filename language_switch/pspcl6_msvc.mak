# The !ifndef-laden style of this makefile is designed to facilitate "deriving"
# your own make file from it by setting your own custom options, then !including
# this file. In its current form, this file will compile using default options
# and locations.
#
# This file only defines the portions of the MSVC makefile that are different
# between the present language switcher vs. the standard pcl6 makefile which
# is !included near the bottom. All other settings default to the base makefile.

# Define the name of this makefile.
MAKEFILE=$(MAKEFILE) ..\language_switch\pspcl6_msvc.mak

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
!ifndef PLPLATFORM
PLPLATFORM=ps
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
!ifndef APPSRCDIR
APPSRCDIR=.
!endif
!ifndef GLSRCDIR
GLSRCDIR=..\gs\base
!endif
!ifndef PSSRCDIR
PSSRCDIR=..\gs\psi
!endif
!ifndef PSISRCDIR
PSISRCDIR=..\psi
!endif
!ifndef MAINSRCDIR
MAINSRCDIR=..\main
!endif
!ifndef PSLIBDIR
PSLIBDIR=..\gs\lib
!endif
!ifndef ICCSRCDIR
ICCSRCDIR=..\gs\icclib
!endif

# Path for including gs/Resource into romfs (replaces the gs default) :
!ifndef PSRESDIR
PSRESDIR=..\gs\Resource
!endif

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif
!ifndef PCLOBJDIR
PCLOBJDIR=$(GENDIR)
!endif
!ifndef PXLOBJDIR
PXLOBJDIR=$(GENDIR)
!endif
!ifndef ICCGENDIR
ICCGENDIR=$(GENDIR)
!endif
!ifndef ICCOBJDIR
ICCOBJDIR=$(GENDIR)
!endif
!ifndef PSGENDIR
PSGENDIR=$(GENDIR)
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(GENDIR)
!endif
!ifndef GLGEN
GLGEN=$(GENDIR)
!endif
!ifndef GLOBJ
GLOBJ=$(GENDIR)
!endif
!ifndef PSIGENDIR
PSIGENDIR=$(GENDIR)
!endif
!ifndef PSIOBJDIR
PSIOBJDIR=$(GENDIR)
!endif

!ifndef DD
DD=$(GLGENDIR)
!endif


# Executable path\name w/o the .EXE extension
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\pspcl6
!endif
!ifndef PSI_TOP_OBJ
PSI_TOP_OBJ=$(PSIOBJDIR)\psitop.$(OBJ)
!endif

# Banding options
!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

!ifndef COMPILE_INITS
COMPILE_INITS=1
!endif

!ifndef FT_BRIDGE
FT_BRIDGE=0
!endif

!ifndef APP_CCC
APP_CCC=$(CC_) -I..\pl -I..\gs\base -I.\obj $(C_)
!endif

# Default major version of MSVC to use;
# this should generally be the latest version.
!ifndef MSVC_VERSION
MSVC_VERSION=9
!endif

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
# fts  - FreeType font system.
# afs  - Artifex font scaler.
# 3 mutually exclusive choices follow, pick one.

PL_SCALER=afs
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)

# defines for building PSI against UFST
!if "$(PL_SCALER)" == "ufst"

!if "$(COMPILE_INITS)" == "1"
UFSTFONTDIR=%rom%fontdata/
!else
UFSTFONTDIR=../fontdata/
!endif

!ifndef UFST_ROOT
UFST_ROOT="../ufst"
!endif
!ifndef FAPI_DEFS
FAPI_DEFS= -DUFST_BRIDGE=1 -DUFST_LIB_EXT=.lib -DGCCx86 -DUFST_ROOT=$(UFST_ROOT)
!endif
UFST_BRIDGE=1
UFST_LIB_EXT=.lib

# specify agfa library locations and includes.
UFST_LIB=$(UFST_ROOT)\rts\lib
UFST_INCLUDES=$(I_)$(UFST_ROOT)\rts\inc $(I_)$(UFST_ROOT)\sys\inc $(I_)$(UFST_ROOT)\rts\fco $(I_)$(UFST_ROOT)\rts\gray $(I_)$(UFST_ROOT)\rts\tt -DMSVC -DAGFA_FONT_TABLE
!endif

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)

!ifndef XCFLAGS
XCFLAGS=/DPSI_INCLUDED
!endif
!ifndef PSICFLAGS
PSICFLAGS=/DPSI_INCLUDED
!endif

# #define xxx_BIND is in std.h
# putting compile time bindings here will have the side effect of having different options
# based on application build.  PSI_INCLUDED is and example of this.
!ifndef EXPERIMENT_CFLAGS
EXPERIMENT_CFLAGS=
!endif

!ifndef BSCFILE
BSCFILE=$(GENDIR)\language_switch.bsc
!endif

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
!ifndef TARGET_DEVS
TARGET_DEVS=$(PXLOBJDIR)\pxl.dev $(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev
!endif

!ifndef FEATURE_DEVS    
FEATURE_CORE    = \
          $(DD)\psl3.dev		\
	  $(DD)\pdf.dev		\
	  $(DD)\dpsnext.dev	\
          $(DD)\htxlib.dev	\
          $(DD)\roplib.dev	\
	  $(DD)\ttfont.dev	\
          $(DD)\gsnogc.dev       \
	  $(DD)\pipe.dev

FEATURE_DEVS    = $(FEATURE_CORE) $(DD)\fapi.dev 
!endif

!if "$(COMPILE_INITS)" == "1"
!include $(PSSRCDIR)\psromfs.mak
!endif

!include $(MAINSRCDIR)\pcl6_msvc.mak

# Subsystems
!include $(PSISRCDIR)\psi.mak

