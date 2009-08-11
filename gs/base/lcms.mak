#  Copyright (C) 2009 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id$

# makefile for the lcms library code.
# Users of this makefile must define the following:
#	SHARE_LCMS - whether to compile in or link to the library
#	LCMSSRCDIR - the library source directory
#
# gs.mak and friends define the following:
#	LCSMOBJDIR - the output obj directory
#	LCMSGENDIR - generated (.dev) file directory
#	LCMSI_ LCMSCF_ - include and cflags for compiling the lib

# We define the lcms.dev target and its dependencies
#
# This partial makefile compiles the lcms library for use in
# Ghostscript.

# Define the name of this makefile.
LCMS_MAK=$(GLSRC)lcms.mak

LCMSSRC=$(LCMSSRCDIR)$(D)src$(D)
LCMSGEN=$(LCMSGENDIR)$(D)
LCMSOBJ=$(LCMSOBJDIR)$(D)

# This makefile was originally written for lcms-1.18.
# Other versions may require adjustments to the OBJS list below

lcms_OBJS=\
	$(LCMSOBJ)cmscnvrt.$(OBJ) \
	$(LCMSOBJ)cmserr.$(OBJ) \
	$(LCMSOBJ)cmsgamma.$(OBJ) \
	$(LCMSOBJ)cmsgmt.$(OBJ) \
	$(LCMSOBJ)cmsintrp.$(OBJ) \
	$(LCMSOBJ)cmsio0.$(OBJ) \
	$(LCMSOBJ)cmsio1.$(OBJ) \
	$(LCMSOBJ)cmslut.$(OBJ) \
	$(LCMSOBJ)cmsmatsh.$(OBJ) \
	$(LCMSOBJ)cmsmtrx.$(OBJ) \
	$(LCMSOBJ)cmspack.$(OBJ) \
	$(LCMSOBJ)cmspcs.$(OBJ) \
	$(LCMSOBJ)cmswtpnt.$(OBJ) \
	$(LCMSOBJ)cmsxform.$(OBJ) \
	$(LCMSOBJ)cmssamp.$(OBJ) \
	$(LCMSOBJ)cmscam97.$(OBJ) \
	$(LCMSOBJ)cmsnamed.$(OBJ) \
	$(LCMSOBJ)cmsps2.$(OBJ) \
	$(LCMSOBJ)cmscam02.$(OBJ) \
	$(LCMSOBJ)cmsvirt.$(OBJ) \
	$(LCMSOBJ)cmscgats.$(OBJ)

lcms_HDRS=\
        $(LCMSSRCDIR)$(D)include$(D)lcms.h \
        $(LCMSSRCDIR)$(D)include$(D)icc34.h

lcms.clean : lcms.config-clean lcms.clean-not-config-clean

lcms.clean-not-config-clean :
	$(EXP)$(ECHOGS_XE) $(LCMSSRCDIR) $(LCMSOBJDIR)
	$(RM_) $(lcms_OBJS)

lcms.config-clean :
	$(RMN_) $(LCMSGEN)$(D)lcms*.dev

# NB: we can't use the normal $(CC_) here because msvccmd.mak
# adds /Za which conflicts with the lcms source.
LCMS_CC=$(CC) $(CFLAGS) $(I_)$(LCMSSRCDIR)$(D)include $(LCMSCF_)
LCMSO_=$(O_)$(LCMSOBJ)

# switch in the version of lcms.dev we're actually using
$(LCMSGEN)lcms.dev : $(TOP_MAKEFILES) $(LCMSGEN)lcms_$(SHARE_LCMS).dev
	$(CP_) $(LCMSGEN)lcms_$(SHARE_LCMS).dev $(LCMSGEN)lcms.dev

# dev file for shared (separately built) lcms library
$(LCMSGEN)lcms_1.dev : $(TOP_MAKEFILES) $(LCMS_MAK) $(ECHOGS_XE)
	$(SETMOD) $(LCMSGEN)lcms_1 -lib lcms

# dev file for compiling our own from source
$(LCMSGEN)lcms_0.dev : $(TOP_MAKEFILES) $(LCMS_MAK) $(ECHOGS_XE) $(lcms_OBJS)
	$(SETMOD) $(LCMSGEN)lcms_0 $(lcms_OBJS)

# explicit rules for building the source files. 

$(LCMSOBJ)cmscnvrt.$(OBJ) : $(LCMSSRC)cmscnvrt.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmscnvrt.$(OBJ) $(C_) $(LCMSSRC)cmscnvrt.c

$(LCMSOBJ)cmserr.$(OBJ) : $(LCMSSRC)cmserr.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmserr.$(OBJ) $(C_) $(LCMSSRC)cmserr.c

$(LCMSOBJ)cmsgamma.$(OBJ) : $(LCMSSRC)cmsgamma.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsgamma.$(OBJ) $(C_) $(LCMSSRC)cmsgamma.c

$(LCMSOBJ)cmsgmt.$(OBJ) : $(LCMSSRC)cmsgmt.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsgmt.$(OBJ) $(C_) $(LCMSSRC)cmsgmt.c

$(LCMSOBJ)cmsintrp.$(OBJ) : $(LCMSSRC)cmsintrp.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsintrp.$(OBJ) $(C_) $(LCMSSRC)cmsintrp.c

$(LCMSOBJ)cmsio0.$(OBJ) : $(LCMSSRC)cmsio0.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsio0.$(OBJ) $(C_) $(LCMSSRC)cmsio0.c

$(LCMSOBJ)cmsio1.$(OBJ) : $(LCMSSRC)cmsio1.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsio1.$(OBJ) $(C_) $(LCMSSRC)cmsio1.c

$(LCMSOBJ)cmslut.$(OBJ) : $(LCMSSRC)cmslut.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmslut.$(OBJ) $(C_) $(LCMSSRC)cmslut.c

$(LCMSOBJ)cmsmatsh.$(OBJ) : $(LCMSSRC)cmsmatsh.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsmatsh.$(OBJ) $(C_) $(LCMSSRC)cmsmatsh.c

$(LCMSOBJ)cmsmtrx.$(OBJ) : $(LCMSSRC)cmsmtrx.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsmtrx.$(OBJ) $(C_) $(LCMSSRC)cmsmtrx.c

$(LCMSOBJ)cmspack.$(OBJ) : $(LCMSSRC)cmspack.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmspack.$(OBJ) $(C_) $(LCMSSRC)cmspack.c

$(LCMSOBJ)cmspcs.$(OBJ) : $(LCMSSRC)cmspcs.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmspcs.$(OBJ) $(C_) $(LCMSSRC)cmspcs.c

$(LCMSOBJ)cmswtpnt.$(OBJ) : $(LCMSSRC)cmswtpnt.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmswtpnt.$(OBJ) $(C_) $(LCMSSRC)cmswtpnt.c

$(LCMSOBJ)cmsxform.$(OBJ) : $(LCMSSRC)cmsxform.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsxform.$(OBJ) $(C_) $(LCMSSRC)cmsxform.c

$(LCMSOBJ)cmssamp.$(OBJ) : $(LCMSSRC)cmssamp.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmssamp.$(OBJ) $(C_) $(LCMSSRC)cmssamp.c

$(LCMSOBJ)cmscam97.$(OBJ) : $(LCMSSRC)cmscam97.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmscam97.$(OBJ) $(C_) $(LCMSSRC)cmscam97.c

$(LCMSOBJ)cmsnamed.$(OBJ) : $(LCMSSRC)cmsnamed.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsnamed.$(OBJ) $(C_) $(LCMSSRC)cmsnamed.c

$(LCMSOBJ)cmsps2.$(OBJ) : $(LCMSSRC)cmsps2.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsps2.$(OBJ) $(C_) $(LCMSSRC)cmsps2.c

$(LCMSOBJ)cmscam02.$(OBJ) : $(LCMSSRC)cmscam02.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmscam02.$(OBJ) $(C_) $(LCMSSRC)cmscam02.c

$(LCMSOBJ)cmsvirt.$(OBJ) : $(LCMSSRC)cmsvirt.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmsvirt.$(OBJ) $(C_) $(LCMSSRC)cmsvirt.c

$(LCMSOBJ)cmscgats.$(OBJ) : $(LCMSSRC)cmscgats.c $(lcms_HDRS)
	$(LCMS_CC) $(LCMSO_)cmscgats.$(OBJ) $(C_) $(LCMSSRC)cmscgats.c
