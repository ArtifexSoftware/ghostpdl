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

# Define the name of this makefile.
GPDL_MAK=$(GPDLSRCDIR)$(D)gpdl.mak

GPDLSRC=$(GPDLSRCDIR)$(D)
GPDLPSISRC=$(GPDLSRCDIR)$(D)psi$(D)
GPDLURFSRC=$(URFSRCDIR)$(D)

GPDLOBJ=$(GPDLOBJDIR)$(D)
GPDLGEN=$(GPDLGENDIR)$(D)
GPDLO_=$(O_)$(GPDLOBJ)
GLGEN=$(GLGENDIR)$(D)

GPDL_PSI_TOP_OBJ_FILE=psitop.$(OBJ)
GPDL_PSI_TOP_OBJ=$(GPDLOBJ)$(GPDL_PSI_TOP_OBJ_FILE)

GPDL_URF_TOP_OBJ_FILE=urftop.$(OBJ)

GPDL_JPG_TOP_OBJ_FILE=jpgtop.$(OBJ)
GPDL_JPG_TOP_OBJ=$(GPDLOBJ)$(GPDL_JPG_TOP_OBJ_FILE)

GPDL_PWG_TOP_OBJ_FILE=pwgtop.$(OBJ)
GPDL_PWG_TOP_OBJ=$(GPDLOBJ)$(GPDL_PWG_TOP_OBJ_FILE)

GPDL_TIFF_TOP_OBJ_FILE=tifftop.$(OBJ)
GPDL_TIFF_TOP_OBJ=$(GPDLOBJ)$(GPDL_TIFF_TOP_OBJ_FILE)

GPDL_JBIG2_TOP_OBJ_FILE=jbig2top.$(OBJ)
GPDL_JBIG2_TOP_OBJ=$(GPDLOBJ)$(GPDL_JBIG2_TOP_OBJ_FILE)

GPDL_JP2K_TOP_OBJ_FILE=jp2ktop.$(OBJ)
GPDL_JP2K_TOP_OBJ=$(GPDLOBJ)$(GPDL_JP2K_TOP_OBJ_FILE)

GPDL_PNG_TOP_OBJ_FILE=pngtop.$(OBJ)
GPDL_PNG_TOP_OBJ=$(GPDLOBJ)$(GPDL_PNG_TOP_OBJ_FILE)

GPDL_PSI_TOP_OBJS=\
	$(GPDL_PNG_TOP_OBJ)\
	$(GPDL_JP2K_TOP_OBJ)\
	$(GPDL_JBIG2_TOP_OBJ)\
	$(GPDL_TIFF_TOP_OBJ)\
	$(GPDL_PWG_TOP_OBJ)\
	$(GPDL_JPG_TOP_OBJ)\
	$(GPDL_URF_TOP_OBJ)\
	$(GPDL_PSI_TOP_OBJ)\
	$(GPDLOBJ)gpdlimpl.$(OBJ)

LANG_CFLAGS=\
	$(D_)PCL_INCLUDED$(_D)\
	$(D_)PSI_INCLUDED$(_D)\
	$(D_)XPS_INCLUDED$(_D)\
	$(ENABLE_URF)\
	$(D_)JPG_INCLUDED$(_D)\
	$(D_)PWG_INCLUDED$(_D)\
	$(ENABLE_TIFF)\
	$(D_)JBIG2_INCLUDED$(_D)\
	$(D_)JP2K_INCLUDED$(_D)\
	$(D_)PNG_INCLUDED$(_D)\

GPDLCC=$(CC_) $(LANG_CFLAGS) $(I_)$(PSSRCDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(DEVSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

GPDLJB2CC=$(CC) $(LANG_CFLAGS) $(I_)$(LDF_JB2I_) $(JBIG2_CFLAGS) $(II)$(JB2I_)$(_I) $(I_)$(PSSRCDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) \
$(I_)$(GLSRCDIR)$(_I) $(I_)$(DEVSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(CCFLAGS) $(C_)

$(GPDLGEN)gpdlimpl.c: $(PLSRC)plimpl.c $(MAKEDIRS)
	$(CP_) $(PLSRC)plimpl.c $(GPDLGEN)gpdlimpl.c

$(GPDLOBJ)gpdlimpl.$(OBJ): $(GPDLGEN)gpdlimpl.c          \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(GPDLCC) $(GPDLGEN)gpdlimpl.c $(GPDLO_)gpdlimpl.$(OBJ)


$(GPDL_PSI_TOP_OBJ): $(GPDLSRC)psitop.c $(AK) $(stdio__h)\
 $(string__h) $(gdebug_h) $(gp_h) $(gsdevice_h) $(gserrors_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gspaint_h) $(gstypes_h) $(gxalloc_h) $(gxstate_h)\
 $(gsnogc_h) $(pltop_h) $(psitop_h) $(plparse_h) $(gsicc_manage_h)\
 $(plfont_h) $(uconfig_h)
	$(GPDLCC) $(GPDLSRC)psitop.c $(GPDLO_)$(GPDL_PSI_TOP_OBJ_FILE)

# Note that we don't use $(GPDL_URF_TOP_OBJ) as the target of the
# next make rule, as this expands to "" in builds that don't use
# URF.
$(GPDLOBJ)/$(GPDL_URF_TOP_OBJ_FILE): $(GPDLURFSRC)urftop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(surfx_h) $(strimpl_h)\
 $(gscoord_h) $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLCC) $(GPDLURFSRC)urftop.c $(GPDLO_)$(GPDL_URF_TOP_OBJ_FILE)

$(GPDL_JPG_TOP_OBJ): $(GPDLSRC)jpgtop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(jpeglib_h) $(setjmp__h) $(sjpeg_h) $(pltop_h) $(gsicc_manage_h)\
 $(gspaint_h) $(plmain_h) $(jpeglib__h)
	$(GPDLCC) $(I_)$(GLI_) $(II)$(JI_)$(_I) $(JCF_) $(GLF_) $(GPDLSRC)jpgtop.c $(GPDLO_)$(GPDL_JPG_TOP_OBJ_FILE)

$(GPDL_PWG_TOP_OBJ): $(GPDLSRC)pwgtop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(spwgx_h) $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLCC) $(GPDLSRC)pwgtop.c $(GPDLO_)$(GPDL_PWG_TOP_OBJ_FILE)

# TIFF reading can only work with both libtiff and libjpeg using share libs
# *or* both using local - we can't mix.
$(GPDLOBJ)tifftop_0.$(OBJ): $(GPDLSRC)tifftop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h) $(jmemcust_h)
	$(GPDLCC) $(D_)SHARE_LIBTIFF=0$(_D) $(D_)SHARE_JPEG=0$(_D) $(II)$(TI_)$(_I) $(II)$(JI_)$(_I) $(GPDLSRC)tifftop.c $(GPDLO_)tifftop_0.$(OBJ)

$(GPDLOBJ)tifftop_1.$(OBJ): $(GPDLSRC)tifftop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLCC) $(D_)SHARE_LIBTIFF=1$(_D) $(D_)SHARE_JPEG=1$(_D) $(II)$(TI_)$(_I) $(II)$(JI_)$(_I) $(GPDLSRC)tifftop.c $(GPDLO_)tifftop_1.$(OBJ)

$(GPDL_TIFF_TOP_OBJ): $(GPDLOBJ)tifftop_$(SHARE_LIBTIFF).$(OBJ)
	$(CP_) $(GPDLOBJ)tifftop_$(SHARE_LIBTIFF).$(OBJ) $(GPDL_TIFF_TOP_OBJ)

$(GPDL_JBIG2_TOP_OBJ): $(GPDLSRC)jbig2top.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLJB2CC) $(GPDLSRC)jbig2top.c $(GPDLO_)$(GPDL_JBIG2_TOP_OBJ_FILE)

$(GPDL_JP2K_TOP_OBJ): $(GPDLSRC)jp2ktop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLCC) $(I_)$(JPX_OPENJPEG_I_)$(D).. $(I_)$(JPX_OPENJPEG_I_) $(II)$(GLI_)$(_I) $(JPXCF_) $(I_)$(LWF_JPXI_) $(GPDLSRC)jp2ktop.c $(GPDLO_)$(GPDL_JP2K_TOP_OBJ_FILE)

$(GPDL_PNG_TOP_OBJ): $(GPDLSRC)pngtop.c $(AK)\
 $(gxdevice_h) $(gserrors_h) $(gsstate_h) $(strimpl_h) $(gscoord_h)\
 $(png__h) $(pltop_h) $(gsicc_manage_h) $(gspaint_h) $(plmain_h)
	$(GPDLCC) $(II)$(PI_)$(_I) $(PCF_) $(GPDLSRC)pngtop.c $(GPDLO_)$(GPDL_PNG_TOP_OBJ_FILE)
