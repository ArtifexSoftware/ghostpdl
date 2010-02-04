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
# Platform-independent makefile for the SVG interpreter

SVGSRC      = $(SVGSRCDIR)$(D)
SVGGEN      = $(SVGGENDIR)$(D)
SVGOBJ      = $(SVGOBJDIR)$(D)
SVGO_       = $(O_)$(SVGOBJ)
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
PLOBJ       = $(PLOBJDIR)$(D)

SVGCCC  = $(CC_) $(I_)$(SVGSRCDIR)$(_I) $(I_)$(SVGGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(EXPATINCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) $(C_)

# Define the name of this makefile.
SVG_MAK     = $(SVGSRC)svg.mak

svg.clean: svg.config-clean svg.clean-not-config-clean

svg.clean-not-config-clean:
	$(RM_) $(SVGOBJ)*.$(OBJ)

svg.config-clean: clean_gs
	$(RM_) $(SVGOBJ)*.dev
	$(RM_) $(SVGOBJ)devs.tr5


ghostsvg_h=$(SVGSRC)ghostsvg.h $(memory__h) $(math__h) \
	$(gsgc_h) $(gstypes_h) $(gsstate_h) $(gsmatrix_h) $(gscoord_h) \
	$(gsmemory_h) $(gsparam_h) $(gsdevice_h) $(scommon_h) \
	$(gserror_h) $(gserrors_h) $(gspaint_h) $(gspath_h) $(gsimage_h) \
	$(gscspace_h) $(gsptype1_h) $(gscolor2_h) $(gscolor3_h) \
	$(gsutil_h) $(gsicc_h) $(gstrans_h) $(gxpath_h) $(gxfixed_h) \
	$(gxmatrix_h) $(gsshade_h) $(gsfunc_h) $(gsfunc3_h) \
	$(gxfont_h) $(gxchar_h) $(gxtype1_h) $(gxfont1_h) $(gxfont42_h) \
	$(gxfcache_h) $(gxistate_h) $(gzstate_h) $(gzpath_h) $(zlib_h)

SVGINCLUDES=$(ghostsvg_h)

$(SVGOBJ)svgtypes.$(OBJ): $(SVGSRC)svgtypes.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgtypes.c $(SVGO_)svgtypes.$(OBJ)

$(SVGOBJ)svgcolor.$(OBJ): $(SVGSRC)svgcolor.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgcolor.c $(SVGO_)svgcolor.$(OBJ)

$(SVGOBJ)svgtransform.$(OBJ): $(SVGSRC)svgtransform.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgtransform.c $(SVGO_)svgtransform.$(OBJ)

$(SVGOBJ)svgshapes.$(OBJ): $(SVGSRC)svgshapes.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgshapes.c $(SVGO_)svgshapes.$(OBJ)

$(SVGOBJ)svgdoc.$(OBJ): $(SVGSRC)svgdoc.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgdoc.c $(SVGO_)svgdoc.$(OBJ)

$(SVGOBJ)svgxml.$(OBJ): $(SVGSRC)svgxml.c $(SVGINCLUDES)
	$(SVGCCC) $(SVGSRC)svgxml.c $(SVGO_)svgxml.$(OBJ)


$(SVG_TOP_OBJ): $(SVGSRC)svgtop.c $(pltop_h) $(SVGGEN)pconf.h $(SVGINCLUDES)
	$(CP_) $(SVGGEN)pconf.h $(SVGGEN)pconfig.h
	$(SVGCCC) $(SVGSRC)svgtop.c $(SVGO_)svgtop.$(OBJ)

SVG_OBJS=\
    $(SVGOBJ)svgtypes.$(OBJ) \
    $(SVGOBJ)svgcolor.$(OBJ) \
    $(SVGOBJ)svgtransform.$(OBJ) \
    $(SVGOBJ)svgshapes.$(OBJ) \
    $(SVGOBJ)svgdoc.$(OBJ) \
    $(SVGOBJ)svgxml.$(OBJ) \

# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==SVGGEN is required.

$(SVGOBJ)svg.dev: $(SVG_MAK) $(ECHOGS_XE) $(SVG_OBJS) \
		$(SVGGEN)expat.dev \
		$(SVGGEN)pl.dev $(SVGGEN)$(PL_SCALER).dev $(SVGGEN)pjl.dev
	$(SETMOD) $(SVGOBJ)svg $(SVG_OBJS)
	$(ADDMOD) $(SVGOBJ)svg -include $(SVGGEN)expat $(SVGGEN)pl $(SVGGEN)$(PL_SCALER) $(SVGGEN)pjl.dev
#	$(ADDMOD) $(SVGOBJ)svg -lib profiler

