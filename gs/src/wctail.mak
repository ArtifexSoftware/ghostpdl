# Portions Copyright (C) 2001 artofcode LLC. 
#  Portions Copyright (C) 1996, 2001 Artifex Software Inc.
#  Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
#  This software is based in part on the work of the Independent JPEG Group.
#  All Rights Reserved.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/ or
#  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
#  San Rafael, CA  94903, (415)492-9861, for further information.

# $RCSfile$ $Revision$
# wctail.mak
# Last part of Watcom C/C++ makefile common to MS-DOS and MS Windows.

# Define the name of this makefile.
WCTAIL_MAK=$(GLSRCDIR)\wctail.mak

# Include the generic makefiles, except for devs.mak, contrib.mak,
# int.mak, and cfonts.mak.
#!include $(COMMONDIR)/watcdefs.mak
#!include $(COMMONDIR)/pcdefs.mak
#!include $(COMMONDIR)/generic.mak
!include $(GLSRCDIR)\version.mak
!include $(GLSRCDIR)\gs.mak
!include $(GLSRCDIR)\lib.mak
!include $(GLSRCDIR)\jpeg.mak
# zlib.mak must precede libpng.mak
!include $(GLSRCDIR)\zlib.mak
!include $(GLSRCDIR)\libpng.mak
!include $(GLSRCDIR)\jbig2.mak
!include $(GLSRCDIR)\icclib.mak
!include $(GLSRCDIR)\ijs.mak

# -------------------------- Auxiliary programs --------------------------- #

temp_tr=$(GLOBJ)_temp_.tr

$(ECHOGS_XE): $(AUXGEN)echogs.$(OBJ)
	echo OPTION STUB=$(STUB) >$(temp_tr)
	echo $(LIBPATHS) >>$(temp_tr)
	$(LINK) @$(temp_tr) FILE $(AUXGEN)echogs

$(AUXGEN)echogs.$(OBJ): $(GLSRC)echogs.c
	$(CCAUX) $(GLSRC)echogs.c $(O_)$(AUXGEN)echogs.$(OBJ)

$(GENARCH_XE): $(AUXGEN)genarch.$(OBJ)
	echo $(LIBPATHS) >$(temp_tr)
	$(LINK) @$(temp_tr) FILE $(AUXGEN)genarch

$(AUXGEN)genarch.$(OBJ): $(GLSRC)genarch.c $(stdpre_h)
	$(CCAUX) $(GLSRC)genarch.c $(O_)$(AUXGEN)genarch.$(OBJ)

$(GENCONF_XE): $(AUXGEN)genconf.$(OBJ)
	echo OPTION STUB=$(STUB) >$(temp_tr)
	echo OPTION STACK=8k >>$(temp_tr)
	echo $(LIBPATHS) >>$(temp_tr)
	$(LINK) @$(temp_tr) FILE $(AUXGEN)genconf

$(AUXGEN)genconf.$(OBJ): $(GLSRC)genconf.c $(stdpre_h)
	$(CCAUX) $(GLSRC)genconf.c $(O_)$(AUXGEN)genconf.$(OBJ)

$(GENDEV_XE): $(AUXGEN)gendev.$(OBJ)
	echo OPTION STUB=$(STUB) >$(temp_tr)
	echo OPTION STACK=8k >>$(temp_tr)
	echo $(LIBPATHS) >>$(temp_tr)
	$(LINK) @$(temp_tr) FILE $(AUXGEN)gendev

$(AUXGEN)gendev.$(OBJ): $(GLSRC)gendev.c $(stdpre_h)
	$(CCAUX) $(GLSRC)gendev.c $(O_)$(AUXGEN)gendev.$(OBJ)

$(GENINIT_XE): $(AUXGEN)geninit.$(OBJ)
	echo OPTION STUB=$(STUB) >$(temp_tr)
	echo OPTION STACK=8k >>$(temp_tr)
	echo $(LIBPATHS) >>$(temp_tr)
	$(LINK) @$(temp_tr) FILE $(AUXGEN)geninit

$(AUXGEN)geninit.$(OBJ): $(GLSRC)geninit.c $(stdpre_h)
	$(CCAUX) $(GLSRC)geninit.c $(O_)$(AUXGEN)geninit.$(OBJ)

# No special gconfig_.h is needed.
# Watcom `make' supports output redirection.
$(gconfig__h): $(WCTAIL_MAK)
	echo /* This file deliberately left blank. */ >$(gconfig__h)

$(gconfigv_h): $(WCTAIL_MAK) $(TOP_MAKEFILES) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfigv_h) -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)
	$(ECHOGS_XE) -a $(gconfigv_h) -x 23 define SYSTEM_CONSTANTS_ARE_WRITABLE 0$(SYSTEM_CONSTANTS_ARE_WRITABLE)
