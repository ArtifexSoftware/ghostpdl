# The "?=" style of this makefile is designed to facilitate "deriving"
# your own make file from it by setting your own custom options, then include'ing
# this file. In its current form, this file will compile using default options
# and locations. It is recommended that you make any modifications to settings
# in this file by creating your own makefile which includes this one.
#
# This file only defines the portions of the makefile that are different
# between the present language switcher vs. the standard pcl6 makefile which
# is included near the bottom. All other settings default to the base makefile.

# Define the name of this makefile.
MAKEFILE+= ../language_switch/pspcl6_gcc.mak

# Pick (uncomment) one font system technology
# afs (Artifex/gs native) or ufst (AGFA UFST)
PL_SCALER?=afs
#PL_SCALER?=ufst

# The build process will put all of its output in this directory:
# GENDIR is defined in the 'base' makefile, but we need its value immediately
GENDIR?=./obj

# The sources are taken from these directories:
APPSRCDIR?=.
MAINSRCDIR?=../main
PSSRCDIR?=../gs/src
PSISRCDIR?=../psi
PSLIBDIR?=../gs/lib
ICCSRCDIR?=../gs/icclib
PSRCDIR?=../gs/libpng
# Path for including gs/Resource into romfs (replaces the gs default) :
PSRESDIR?=../gs/Resource


APP_CCC?=$(CC_) -I../pl -I../gs/src -I./obj $(C_)

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
PLPLATFORM?=ps

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
PSGENDIR?=$(GENDIR)
PSOBJDIR?=$(GENDIR)
PSIGENDIR?=$(GENDIR)
PSIOBJDIR?=$(GENDIR)
JGENDIR?=$(GENDIR)
JOBJDIR?=$(GENDIR)
ZGENDIR?=$(GENDIR)
ZOBJDIR?=$(GENDIR)

# Executable path\name w/o the .EXE extension
TARGET_XE?=$(GENDIR)/pspcl6

# Main file's name
PSI_TOP_OBJ?=$(PSIOBJDIR)/psitop.$(OBJ)
TOP_OBJ+= $(PSI_TOP_OBJ)

# Choose COMPILE_INITS=1 for init files and fonts in ROM (otherwise =0)
COMPILE_INITS?=1

# defines for building PSI against UFST
ifeq ($(PL_SCALER), ufst)

ifeq ($(COMPILE_INITS), 1)
UFSTFONTDIR=%rom%fontdata/
else
UFSTFONTDIR=/usr/local/fontdata5.0/
endif

UFST_ROOT?=../ufst
FAPI_DEFS?= -DUFST_BRIDGE=1 -DUFST_LIB_EXT=.a -DGCCx86 -DUFST_ROOT=$(UFST_ROOT)
UFST_BRIDGE=1
UFST_LIB_EXT=.a
endif

GX_COLOR_INDEX_DEFINE?=-DGX_COLOR_INDEX_TYPE="unsigned long long"
# Stupid autoconf coupling 
HAVE_STDINT_H_DEFINE?=-DHAVE_STDINT_H

# Assorted definitions.  Some of these should probably be factored out....
# We use -O0 for debugging, because optimization confuses gdb.
# Note that the omission of -Dconst= rules out the use of gcc versions
# between 2.7.0 and 2.7.2 inclusive.  (2.7.2.1 is OK.)
PSICFLAGS?=-DPSI_INCLUDED


# #define xxx_BIND is in std.h
# putting compile time bindings here will have the side effect of having different options
# based on application build.  PSI_INCLUDED is and example of this.
EXPERIMENT_CFLAGS?=

GCFLAGS?=-Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings -DNDEBUG $(PSICFLAGS) $(EXPERIMENT_CFLAGS) $(HAVE_STDINT_H_DEFINE) $(GX_COLOR_INDEX_DEFINE) $(FAPI_DEFS)
CFLAGS?=-g -O0 $(GCFLAGS) $(XCFLAGS) $(PSICFLAGS)

FEATURE_CORE    ?= \
          $(DD)psl3.dev		\
	  $(DD)pdf.dev		\
	  $(DD)dpsnext.dev	\
          $(DD)htxlib.dev	\
          $(DD)roplib.dev	\
	  $(DD)ttfont.dev	\
	  $(DD)pipe.dev

ifeq ($(PL_SCALER), ufst)
FEATURE_DEVS    ?= $(FEATURE_CORE) $(DD)fapi.dev
else
FEATURE_DEVS    ?= $(FEATURE_CORE)
endif

# "Subclassed" makefile
include $(MAINSRCDIR)/pcl6_gcc.mak

# Subsystems
include $(PSISRCDIR)/psi.mak
