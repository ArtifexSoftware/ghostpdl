# Copyright (C) 2006-2010 Artifex Software, Inc.
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
# Platform-independent makefile for the XPS interpreter

XPSSRC      = $(XPSSRCDIR)$(D)
XPSGEN      = $(XPSGENDIR)$(D)
XPSOBJ      = $(XPSOBJDIR)$(D)
XPSO_       = $(O_)$(XPSOBJ)
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
PLOBJ       = $(PLOBJDIR)$(D)

XPSCCC  = $(CC_) $(I_)$(XPSSRCDIR)$(_I) $(I_)$(XPSGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(EXPATINCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
XPS_MAK     = $(XPSSRC)xps.mak

xps.clean: xps.config-clean xps.clean-not-config-clean

xps.clean-not-config-clean:
	$(RM_) $(XPSOBJ)*.$(OBJ)

xps.config-clean: clean_gs
	$(RM_) $(XPSOBJ)*.dev
	$(RM_) $(XPSOBJ)devs.tr5


XPSINCLUDES=$(XPSSRC)*.h $(XPSOBJ)arch.h $(XPSOBJ)jpeglib_.h


$(XPSOBJ)xpsmem.$(OBJ): $(XPSSRC)xpsmem.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsmem.c $(XPSO_)xpsmem.$(OBJ)

$(XPSOBJ)xpsutf.$(OBJ): $(XPSSRC)xpsutf.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsutf.c $(XPSO_)xpsutf.$(OBJ)

$(XPSOBJ)xpscrc.$(OBJ): $(XPSSRC)xpscrc.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpscrc.c $(XPSO_)xpscrc.$(OBJ)

$(XPSOBJ)xpshash.$(OBJ): $(XPSSRC)xpshash.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpshash.c $(XPSO_)xpshash.$(OBJ)

$(XPSOBJ)xpsjpeg.$(OBJ): $(XPSSRC)xpsjpeg.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsjpeg.c $(XPSO_)xpsjpeg.$(OBJ)

$(XPSOBJ)xpspng.$(OBJ): $(XPSSRC)xpspng.c $(XPSINCLUDES) $(PNGSRCDIR)$(D)png.h
	$(XPSCCC) $(I_)$(PNGSRCDIR)$(_I) $(XPSSRC)xpspng.c $(XPSO_)xpspng.$(OBJ)

$(XPSOBJ)xpstiff.$(OBJ): $(XPSSRC)xpstiff.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpstiff.c $(XPSO_)xpstiff.$(OBJ)

$(XPSOBJ)xpshdp.$(OBJ): $(XPSSRC)xpshdp.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpshdp.c $(XPSO_)xpshdp.$(OBJ)

$(XPSOBJ)xpszip.$(OBJ): $(XPSSRC)xpszip.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpszip.c $(XPSO_)xpszip.$(OBJ)

$(XPSOBJ)xpszipseek.$(OBJ): $(XPSSRC)xpszipseek.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpszipseek.c $(XPSO_)xpszipseek.$(OBJ)

$(XPSOBJ)xpsxml.$(OBJ): $(XPSSRC)xpsxml.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsxml.c $(XPSO_)xpsxml.$(OBJ)

$(XPSOBJ)xpsdoc.$(OBJ): $(XPSSRC)xpsdoc.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsdoc.c $(XPSO_)xpsdoc.$(OBJ)

$(XPSOBJ)xpspage.$(OBJ): $(XPSSRC)xpspage.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpspage.c $(XPSO_)xpspage.$(OBJ)

$(XPSOBJ)xpsresource.$(OBJ): $(XPSSRC)xpsresource.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsresource.c $(XPSO_)xpsresource.$(OBJ)

$(XPSOBJ)xpscommon.$(OBJ): $(XPSSRC)xpscommon.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpscommon.c $(XPSO_)xpscommon.$(OBJ)

$(XPSOBJ)xpsanalyze.$(OBJ): $(XPSSRC)xpsanalyze.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsanalyze.c $(XPSO_)xpsanalyze.$(OBJ)

$(XPSOBJ)xpscolor.$(OBJ): $(XPSSRC)xpscolor.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpscolor.c $(XPSO_)xpscolor.$(OBJ)

$(XPSOBJ)xpsopacity.$(OBJ): $(XPSSRC)xpsopacity.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsopacity.c $(XPSO_)xpsopacity.$(OBJ)

$(XPSOBJ)xpspath.$(OBJ): $(XPSSRC)xpspath.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpspath.c $(XPSO_)xpspath.$(OBJ)

$(XPSOBJ)xpstile.$(OBJ): $(XPSSRC)xpstile.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpstile.c $(XPSO_)xpstile.$(OBJ)

$(XPSOBJ)xpsvisual.$(OBJ): $(XPSSRC)xpsvisual.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsvisual.c $(XPSO_)xpsvisual.$(OBJ)

$(XPSOBJ)xpsimage.$(OBJ): $(XPSSRC)xpsimage.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsimage.c $(XPSO_)xpsimage.$(OBJ)

$(XPSOBJ)xpsgradient.$(OBJ): $(XPSSRC)xpsgradient.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsgradient.c $(XPSO_)xpsgradient.$(OBJ)

$(XPSOBJ)xpsglyphs.$(OBJ): $(XPSSRC)xpsglyphs.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsglyphs.c $(XPSO_)xpsglyphs.$(OBJ)

$(XPSOBJ)xpsfont.$(OBJ): $(XPSSRC)xpsfont.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsfont.c $(XPSO_)xpsfont.$(OBJ)

$(XPSOBJ)xpsttf.$(OBJ): $(XPSSRC)xpsttf.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpsttf.c $(XPSO_)xpsttf.$(OBJ)

$(XPSOBJ)xpscff.$(OBJ): $(XPSSRC)xpscff.c $(XPSINCLUDES)
	$(XPSCCC) $(XPSSRC)xpscff.c $(XPSO_)xpscff.$(OBJ)


$(XPS_TOP_OBJ): $(XPSSRC)xpstop.c $(XPSGEN)pconf.h $(pltop_h) $(XPSINCLUDES)
	$(CP_) $(XPSGEN)pconf.h $(XPSGEN)pconfig.h
	$(XPSCCC) $(XPSSRC)xpstop.c $(XPSO_)xpstop.$(OBJ)

XPS_OBJS=\
    $(XPSOBJ)xpsmem.$(OBJ) \
    $(XPSOBJ)xpsutf.$(OBJ) \
    $(XPSOBJ)xpscrc.$(OBJ) \
    $(XPSOBJ)xpshash.$(OBJ) \
    $(XPSOBJ)xpsjpeg.$(OBJ) \
    $(XPSOBJ)xpspng.$(OBJ) \
    $(XPSOBJ)xpstiff.$(OBJ) \
    $(XPSOBJ)xpshdp.$(OBJ) \
    $(XPSOBJ)xpszip.$(OBJ) \
    $(XPSOBJ)xpszipseek.$(OBJ) \
    $(XPSOBJ)xpsxml.$(OBJ) \
    $(XPSOBJ)xpsdoc.$(OBJ) \
    $(XPSOBJ)xpspage.$(OBJ) \
    $(XPSOBJ)xpsresource.$(OBJ) \
    $(XPSOBJ)xpscommon.$(OBJ) \
    $(XPSOBJ)xpsanalyze.$(OBJ) \
    $(XPSOBJ)xpscolor.$(OBJ) \
    $(XPSOBJ)xpsopacity.$(OBJ) \
    $(XPSOBJ)xpspath.$(OBJ) \
    $(XPSOBJ)xpstile.$(OBJ) \
    $(XPSOBJ)xpsvisual.$(OBJ) \
    $(XPSOBJ)xpsimage.$(OBJ) \
    $(XPSOBJ)xpsgradient.$(OBJ) \
    $(XPSOBJ)xpsglyphs.$(OBJ) \
    $(XPSOBJ)xpsfont.$(OBJ) \
    $(XPSOBJ)xpsttf.$(OBJ) \
    $(XPSOBJ)xpscff.$(OBJ) \

# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==XPSGEN is required.

$(XPSOBJ)xps.dev: $(XPS_MAK) $(ECHOGS_XE) $(XPS_OBJS) $(XPSGEN)expat.dev \
		  $(XPSGEN)pl.dev $(XPSGEN)$(PL_SCALER).dev $(XPSGEN)pjl.dev
	$(SETMOD) $(XPSOBJ)xps $(XPS_OBJS)
	$(ADDMOD) $(XPSOBJ)xps -include $(XPSGEN)expat $(XPSGEN)pl $(XPSGEN)$(PL_SCALER) $(XPSGEN)pjl.dev

