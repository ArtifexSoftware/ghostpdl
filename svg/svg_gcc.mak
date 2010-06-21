# Copyright (C) 2008 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied, modified
# or distributed except as expressly authorized under the terms of that
# license.  Refer to licensing information at http://www.artifex.com/
# or contact Artifex Software, Inc.,  7 Mt. Lassen  Drive - Suite A-134,
# San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# GNU makefile for the SVG interpreter

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
MAKEFILE+= ../svg/svg_gcc.mak

# Include SVG support
SVG_INCLUDED?=TRUE

# Font scaler
PL_SCALER?=afs

# The build process will put all of its output in this directory:
# GENDIR is defined in the 'base' makefile, but we need its value immediately
GENDIR?=./obj

# The sources are taken from these directories:
MAINSRCDIR?=../main
PSSRCDIR?=../gs/psi
SVGSRCDIR?=../svg
PSLIBDIR?=../gs/lib
ICCSRCDIR?=../gs/icclib
PNGSRCDIR?=../gs/libpng
EXPATSRCDIR?=../gs/expat

SHARE_EXPAT?=0
EXPAT_CFLAGS=-DHAVE_MEMMOVE

# PLPLATFORM indicates should be set to 'ps' for language switch
# builds and null otherwise.
PLPLATFORM?=

# If you want to build the individual packages in their own directories,
# you can define this here, although normally you won't need to do this:
GLGENDIR?=$(GENDIR)
GLOBJDIR?=$(GENDIR)
PSGENDIR?=$(GENDIR)
PSOBJDIR?=$(GENDIR)
JGENDIR?=$(GENDIR)
JOBJDIR?=$(GENDIR)
ZGENDIR?=$(GENDIR)
ZOBJDIR?=$(GENDIR)
EXPATGENDIR?=$(GENDIR)
EXPATOBJDIR?=$(GENDIR)

# Executable path\name w/o the .EXE extension
TARGET_DEVS=$(SVGOBJDIR)/svg.dev
TARGET_XE?=$(GENDIR)/gsvg

# Main file's name
# this is already in pcl6_gcc.mak
SVG_TOP_OBJ?=$(SVGOBJDIR)/svgtop.$(OBJ)
TOP_OBJ?=$(SVG_TOP_OBJ)

PDL_INCLUDE_FLAGS?=-DSVG_INCLUDED

# SVG only needs the ICC profiles from the %rom% file system.
COMPILE_INITS?=1

include $(MAINSRCDIR)/pcl6_gcc.mak

# Subsystems
# this is already in pcl6_gcc.mak
include $(SVGSRCDIR)/svg.mak
include $(GLSRCDIR)/expat.mak
