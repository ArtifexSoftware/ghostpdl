#    Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

# wctail.mak
# Last part of Watcom C/C++ makefile common to MS-DOS and MS Windows.

# Define the name of this makefile.
WCTAIL_MAK=wctail.mak

# Include the generic makefiles, except for devs.mak and int.mak.
!include version.mak
!include gs.mak
!include lib.mak
!include jpeg.mak
!include libpng.mak
!include zlib.mak

# -------------------------- Auxiliary programs --------------------------- #

$(ECHOGS_XE): echogs.$(OBJ)
	echo OPTION STUB=$(STUB) >_temp_.tr
	echo $LIBPATHS >>_temp_.tr
	$(LINK) @_temp_.tr FILE echogs

echogs.$(OBJ): echogs.c
	$(CCAUX) echogs.c

$(GENARCH_XE): genarch.$(OBJ)
	echo $LIBPATHS >_temp_.tr
	$(LINK) @_temp_.tr FILE genarch

genarch.$(OBJ): genarch.c $(stdpre_h)
	$(CCAUX) genarch.c

$(GENCONF_XE): genconf.$(OBJ)
	echo OPTION STUB=$(STUB) >_temp_.tr
	echo OPTION STACK=8k >>_temp_.tr
	echo $LIBPATHS >>_temp_.tr
	$(LINK) @_temp_.tr FILE genconf

genconf.$(OBJ): genconf.c $(stdpre_h)
	$(CCAUX) genconf.c

$(GENINIT_XE): geninit.$(OBJ)
	echo OPTION STUB=$(STUB) >_temp_.tr
	echo OPTION STACK=8k >>_temp_.tr
	echo $LIBPATHS >>_temp_.tr
	$(LINK) @_temp_.tr FILE geninit

geninit.$(OBJ): geninit.c $(stdio__h) $(string__h)
	$(CCAUX) geninit.c

# No special gconfig_.h is needed.
# Watcom `make' supports output redirection.
gconfig_.h: $(WCTAIL_MAK)
	echo /* This file deliberately left blank. */ >gconfig_.h

gconfigv.h: $(WCTAIL_MAK) $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w gconfigv.h -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)
