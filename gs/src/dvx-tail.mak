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
# Partial makefile, common to all Desqview/X configurations.
# This is the last part of the makefile for Desqview/X configurations.

# The following prevents GNU make from constructing argument lists that
# include all environment variables, which can easily be longer than
# brain-damaged system V allows.

.NOEXPORT:

# -------------------------------- Library -------------------------------- #

## The Desqview/X platform

dvx__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_dvx.$(OBJ) $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_dosfs.$(OBJ)
$(GLGEN)dvx_.dev: $(dvx__) nosync.dev
	$(SETMOD) $(GLGEN)dvx_ $(dvx__) -include nosync

$(GLOBJ)gp_dvx.$(OBJ): $(GLSRC)gp_dvx.c $(AK) $(string__h) $(gx_h) $(gsexit_h) $(gp_h) \
  $(time__h) $(dos__h)
	$(CC_) -D__DVX__ -c $(GLSRC)gp_dvx.c -o $(GLOBJ)gp_dvx.$(OBJ)

# -------------------------- Auxiliary programs --------------------------- #

$(ANSI2KNR_XE): ansi2knr.c $(stdio__h) $(string__h) $(malloc__h)
	$(CC) -o ansi2knr $(CFLAGS) ansi2knr.c

$(ECHOGS_XE): echogs.c
	$(CC) -o echogs $(CFLAGS) echogs.c
	strip echogs
	coff2exe echogs
	del echogs

$(GENARCH_XE): genarch.c $(GENARCH_DEPS)
	$(CC) -o genarch genarch.c
	strip genarch
	coff2exe genarch
	del genarch

$(GENCONF_XE): genconf.c $(GENCONF_DEPS)
	$(CC) -o genconf genconf.c
	strip genconf
	coff2exe genconf
	del genconf

$(GENDEV_XE): gendev.c $(GENDEV_DEPS)
	$(CC) -o gendev gendev.c
	strip gendev
	coff2exe gendev
	del gendev

$(GENHT_XE): genht.c $(GENHT_DEPS)
	$(CC) -o genht $(GENHT_CFLAGS) genht.c
	strip genht
	coff2exe genht
	del genht

$(GENINIT_XE): geninit.c $(GENINIT_DEPS)
	$(CC) -o geninit geninit.c
	strip geninit
	coff2exe geninit
	del geninit

# Construct $(gconfig__h) to reflect the environment.
INCLUDE=/djgpp/include
$(gconfig__h): $(GLSRCDIR)/dvx-tail.mak $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfig__h) -x 2f2a -s This file was generated automatically. -s -x 2a2f
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIME_H
	$(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_DIRENT_H

# ----------------------------- Main program ------------------------------ #

# Interpreter main program

$(GS_XE): ld.tr gs.$(OBJ) $(INT_ALL) $(LIB_ALL) $(DEVS_ALL)
	$(CP_) ld.tr _temp_
	echo $(EXTRALIBS) $(STDLIBS) >>_temp_
	$(CC) $(LDFLAGS) -o $(GS) gs.$(OBJ) @_temp_
	strip $(GS)
	coff2exe $(GS)  
	del $(GS)  
