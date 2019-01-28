# Copyright (C) 2001-2019 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
#

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

# If we are building MEMENTO=1, then adjust default debug flags
!if "$(MEMENTO)"=="1"
!ifndef DEBUG
DEBUG=1
!endif
!ifndef TDEBUG
TDEBUG=1
!endif
!ifndef DEBUGSYM
DEBUGSYM=1
!endif
!endif

# If we are building PROFILE=1, then adjust default debug flags
!if "$(PROFILE)"=="1"
!ifndef DEBUG
DEBUG=0
!endif
!ifndef TDEBUG
TDEBUG=0
!endif
!ifndef DEBUGSYM
DEBUGSYM=1
!endif
!endif

# The build process will put all of its output in this directory:
!ifndef GENDIR
!if "$(MEMENTO)"=="1"
GENDIR=.\memobj
!else
!if "$(PROFILE)"=="1"
GENDIR=.\profobj
!else
!if "$(DEBUG)"=="1"
GENDIR=.\debugobj
!else
GENDIR=.\obj
!endif
!endif
!endif
!ifdef WIN64
GENDIR=$(GENDIR)64
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

!ifndef FTSRCDIR
FTSRCDIR=..\gs\freetype
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
!ifndef PSGENDIR
PSGENDIR=$(GENDIR)
!endif
!ifndef PSOBJDIR
PSOBJDIR=$(GENDIR)
!endif
!ifndef GLGENDIR
GLGENDIR=$(GENDIR)
!endif
!ifndef GLOBJDIR
GLOBJDIR=$(GENDIR)
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
!if "$(UNSUPPORTED)"=="1"
!ifndef TARGET_XE
TARGET_XE=$(GENDIR)\pspcl6
!endif
!else
all:
	@echo ******* NOTE: the language_switch build is a proof of concept and is, therefore, unsupported.
	@echo ******* If you wish to try it, add UNSUPPORTED=1 to your nmake command line.
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
FT_BRIDGE=1
!endif

SHARE_FT=0

!ifndef FTSRCDIR
FTSRCDIR=$(GLSRCDIR)/../freetype
!endif

!ifndef FT_CFLAGS
FT_CFLAGS=-I$(FTSRCDIR)/include
!endif

FT_LIBS=""

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

# Define whether to compile in UFST. Note that freetype will/must be disabled.
# FAPI/UFST depends on UFST_BRIDGE being undefined - hence the construct below.
# (i.e. use "UFST_BRIDGE=1" or *not to define UFST_BRIDGE to anything*)
!ifndef UFST_BRIDGE
UFST_BRIDGE=
!endif

!ifndef UFST_ROOT
UFST_ROOT=$(GLSRCDIR)/../ufst
!endif

UFST_LIB_EXT=.a

UFST_ROMFS_ARGS=-b \
 -P $(UFST_ROOT)/fontdata/mtfonts/pcl45/mt3/ -d fontdata/mtfonts/pcl45/mt3/ pcl___xj.fco plug__xi.fco wd____xh.fco \
 -P $(UFST_ROOT)/fontdata/mtfonts/pclps2/mt3/ -d fontdata/mtfonts/pclps2/mt3/ pclp2_xj.fco \
 -c -P $(PSSRCDIR)/../lib/ -d Resource/Init/ FAPIconfig-FCO

UFSTROMFONTDIR=\"%rom%fontdata/\"
UFSTDISCFONTDIR=\"$(UFST_ROOT)/fontdata/\"


UFST_CFLAGS=

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
          $(DD)\htxlib.dev	\
	  $(DD)\ttfont.dev	\
          $(DD)\gsnogc.dev	\
          $(DD)\msprinter.dev

!ifndef METRO
FEATURE_CORE = $(FEATURE_CORE) $(DD)\pipe.dev
!endif

FEATURE_DEVS    = $(FEATURE_CORE)
!endif

!if "$(COMPILE_INITS)" == "1"
!include $(PSSRCDIR)\psromfs.mak
!endif

!include $(MAINSRCDIR)\pcl6_msvc.mak

# Subsystems
!include $(PSISRCDIR)\psi.mak
