# Copyright (C) 2001-2021 Artifex Software, Inc.
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
# Platform-independent makefile for the XPS interpreter

XPSSRC      = $(XPSSRCDIR)$(D)
XPSGEN      = $(XPSGENDIR)$(D)
XPSOBJ      = $(XPSOBJDIR)$(D)
XPSO_       = $(O_)$(XPSOBJ)
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
PLOBJ       = $(PLOBJDIR)$(D)

XPSCCC  = $(CC_) $(D_)XPS_INCLUDED$(_D) $(I_)$(XPSSRCDIR)$(_I) $(I_)$(XPSGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
XPS_MAK     = $(XPSSRC)xps.mak $(TOP_MAKEFILES)

xps.clean: xps.config-clean xps.clean-not-config-clean

xps.clean-not-config-clean:
	$(RM_) $(XPSOBJ)*.$(OBJ)

xps.config-clean: clean_gs
	$(RM_) $(XPSOBJ)*.dev
	$(RM_) $(XPSOBJ)devs.tr5

XPS_TOP_OBJ=$(XPSOBJDIR)$(D)xpstop.$(OBJ)
XPS_TOP_OBJS= $(XPS_TOP_OBJ) $(XPSOBJDIR)$(D)xpsimpl.$(OBJ)

XPSINCLUDES=$(XPSSRC)*.h $(XPSOBJ)arch.h $(XPSOBJ)jpeglib_.h

$(XPSOBJ)xpsmem.$(OBJ): $(XPSSRC)xpsmem.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsmem.c $(XPSO_)xpsmem.$(OBJ)

$(XPSOBJ)xpsutf.$(OBJ): $(XPSSRC)xpsutf.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsutf.c $(XPSO_)xpsutf.$(OBJ)

$(XPSOBJ)xpscrc.$(OBJ): $(XPSSRC)xpscrc.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpscrc.c $(XPSO_)xpscrc.$(OBJ)

$(XPSOBJ)xpshash.$(OBJ): $(XPSSRC)xpshash.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpshash.c $(XPSO_)xpshash.$(OBJ)

$(XPSOBJ)xpsjpeg.$(OBJ): $(XPSSRC)xpsjpeg.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsjpeg.c $(XPSO_)xpsjpeg.$(OBJ)

$(XPSOBJ)xpspng.$(OBJ): $(XPSSRC)xpspng.c $(XPSINCLUDES) $(png__h) \
                        $(PNGGENDIR)$(D)libpng.dev $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(I_)$(PNGSRCDIR)$(_I) $(XPSSRC)xpspng.c $(XPSO_)xpspng.$(OBJ)

$(XPSOBJ)xpstiff.$(OBJ): $(XPSSRC)xpstiff.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpstiff.c $(XPSO_)xpstiff.$(OBJ)

$(XPSOBJ)xpsjxr.$(OBJ): $(XPSSRC)xpsjxr.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsjxr.c $(XPSO_)xpsjxr.$(OBJ)

$(XPSOBJ)xpszip.$(OBJ): $(XPSSRC)xpszip.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpszip.c $(XPSO_)xpszip.$(OBJ)

$(XPSOBJ)xpsxml.$(OBJ): $(XPSSRC)xpsxml.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsxml.c $(XPSO_)xpsxml.$(OBJ)

$(XPSOBJ)xpsdoc.$(OBJ): $(XPSSRC)xpsdoc.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsdoc.c $(XPSO_)xpsdoc.$(OBJ)

$(XPSOBJ)xpspage.$(OBJ): $(XPSSRC)xpspage.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpspage.c $(XPSO_)xpspage.$(OBJ)

$(XPSOBJ)xpsresource.$(OBJ): $(XPSSRC)xpsresource.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsresource.c $(XPSO_)xpsresource.$(OBJ)

$(XPSOBJ)xpscommon.$(OBJ): $(XPSSRC)xpscommon.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpscommon.c $(XPSO_)xpscommon.$(OBJ)

$(XPSOBJ)xpsanalyze.$(OBJ): $(XPSSRC)xpsanalyze.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsanalyze.c $(XPSO_)xpsanalyze.$(OBJ)

$(XPSOBJ)xpscolor.$(OBJ): $(XPSSRC)xpscolor.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpscolor.c $(XPSO_)xpscolor.$(OBJ)

$(XPSOBJ)xpsopacity.$(OBJ): $(XPSSRC)xpsopacity.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsopacity.c $(XPSO_)xpsopacity.$(OBJ)

$(XPSOBJ)xpspath.$(OBJ): $(XPSSRC)xpspath.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpspath.c $(XPSO_)xpspath.$(OBJ)

$(XPSOBJ)xpstile.$(OBJ): $(XPSSRC)xpstile.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpstile.c $(XPSO_)xpstile.$(OBJ)

$(XPSOBJ)xpsvisual.$(OBJ): $(XPSSRC)xpsvisual.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsvisual.c $(XPSO_)xpsvisual.$(OBJ)

$(XPSOBJ)xpsimage.$(OBJ): $(XPSSRC)xpsimage.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsimage.c $(XPSO_)xpsimage.$(OBJ)

$(XPSOBJ)xpsgradient.$(OBJ): $(XPSSRC)xpsgradient.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsgradient.c $(XPSO_)xpsgradient.$(OBJ)

$(XPSOBJ)xpsglyphs.$(OBJ): $(XPSSRC)xpsglyphs.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsglyphs.c $(XPSO_)xpsglyphs.$(OBJ)

$(XPSOBJ)xpsfont.$(OBJ): $(XPSSRC)xpsfont.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsfont.c $(XPSO_)xpsfont.$(OBJ)

$(XPSOBJ)xpsttf.$(OBJ): $(XPSSRC)xpsttf.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsttf.c $(XPSO_)xpsttf.$(OBJ)

$(XPSOBJ)xpscff.$(OBJ): $(XPSSRC)xpscff.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpscff.c $(XPSO_)xpscff.$(OBJ)

$(XPSOBJ)xpsfapi.$(OBJ): $(XPSSRC)xpsfapi.c $(XPSINCLUDES) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpsfapi.c $(XPSO_)xpsfapi.$(OBJ)

$(XPSGEN)xpsimpl.c: $(PLSRC)plimpl.c $(XPS_MAK) $(MAKEDIRS)
	$(CP_) $(PLSRC)plimpl.c $(XPSGEN)xpsimpl.c

$(PLOBJ)xpsimpl.$(OBJ): $(XPSGEN)xpsimpl.c          \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)                  \
                        $(XPS_MAK)                  \
                        $(MAKEDIRS)
	$(XPSCCC) $(XPSGEN)xpsimpl.c $(XPSO_)xpsimpl.$(OBJ)

$(XPS_TOP_OBJ): $(XPSSRC)xpstop.c $(plmain_h) $(pltop_h) $(gsparam_h) $(XPSINCLUDES) $(GLOBJ)gconfig.$(OBJ) \
                $(pconfig_h) $(XPS_MAK) $(MAKEDIRS)
	$(XPSCCC) $(XPSSRC)xpstop.c $(XPSO_)xpstop.$(OBJ)

XPS_OBJS=\
    $(XPSOBJ)xpsmem.$(OBJ) \
    $(XPSOBJ)xpsutf.$(OBJ) \
    $(XPSOBJ)xpscrc.$(OBJ) \
    $(XPSOBJ)xpshash.$(OBJ) \
    $(XPSOBJ)xpsjpeg.$(OBJ) \
    $(XPSOBJ)xpspng.$(OBJ) \
    $(XPSOBJ)xpstiff.$(OBJ) \
    $(XPSOBJ)xpsjxr.$(OBJ) \
    $(XPSOBJ)xpszip.$(OBJ) \
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
    $(XPSOBJ)xpsfapi.$(OBJ)



# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==XPSGEN is required.

$(XPSOBJ)xps.dev: $(ECHOGS_XE) $(XPS_OBJS)  $(XPSOBJDIR)$(D)expat.dev $(JPEGXR_GENDIR)$(D)jpegxr.dev \
                  $(XPSGEN)pl.dev $(XPSGEN)$(PL_SCALER).dev $(XPSGEN)pjl.dev $(XPS_MAK) $(MAKEDIRS)
	$(SETMOD) $(XPSOBJ)xps $(XPS_OBJS)
	$(ADDMOD) $(XPSOBJ)xps -include $(XPSGEN)$(PL_SCALER) $(XPSGEN)pjl.dev \
                                                                $(XPSOBJDIR)$(D)expat.dev $(JPEGXR_GENDIR)$(D)jpegxr.dev
