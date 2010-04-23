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

# The sources are taken from these directories:
MAINSRCDIR?=../main
GLSRCDIR?=../gs/base
PSSRCDIR?=../gs/psi
PSISRCDIR?=../psi
PSLIBDIR?=../gs/lib
ICCSRCDIR?=../gs/icclib
# Path for including gs/Resource into romfs (replaces the gs default) :
PSRESDIR?=../gs/Resource

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
PSD?=$(GENDIR)/

# Executable path\name w/o the .EXE extension
TARGET_XE?=$(GENDIR)/pspcl6

# Main file's name
PSI_TOP_OBJ?=$(PSIOBJDIR)/psitop.$(OBJ)
PCL_TOP_OBJ?=$(PCLOBJDIR)/pctop.$(OBJ)
PXL_TOP_OBJ?=$(PXLOBJDIR)/pxtop.$(OBJ)

TOP_OBJ=$(PSI_TOP_OBJ) $(PCL_TOP_OBJ) $(PXL_TOP_OBJ)

# Choose COMPILE_INITS=1 for init files and fonts in ROM (otherwise =0)
COMPILE_INITS?=1

PSICFLAGS?=-DPSI_INCLUDED
PDL_INCLUDE_FLAGS?=-DPCL_INCLUDED $(PSICFLAGS)

# Choose FT_BRIDGE=1 to use the freetype rasterizer
FT_BRIDGE?=1
SHARE_FT?=0
FTSRCDIR?=$(GLSRCDIR)/../freetype
FT_CFLAGS?=-I$(FTSRCDIR)/include
FT_LIBS?=

DD=$(GLGENDIR)/

FEATURE_DEVS    ?= \
          $(DD)psl3.dev		\
	  $(DD)pdf.dev		\
	  $(DD)dpsnext.dev	\
          $(DD)htxlib.dev	\
          $(DD)roplib.dev	\
	  $(DD)ttfont.dev	\
	  $(DD)pipe.dev         \
          $(DD)gsnogc.dev       \
	  $(DD)fapi.dev

# extra objects.
XOBJS?=$(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gconfig.o \
       $(GLOBJDIR)/gscdefs.o $(GLOBJDIR)/iconfig.o 

ifeq ($(COMPILE_INITS), 1)
include $(PSSRCDIR)/psromfs.mak
endif

# "Subclassed" makefile
include $(MAINSRCDIR)/pcl6_gcc.mak

# Subsystems
include $(PSISRCDIR)/psi.mak
