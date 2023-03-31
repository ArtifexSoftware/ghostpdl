# Copyright (C) 2018-2023 Artifex Software, Inc.
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
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#
# Platform-independent makefile for the PDF interpreter

PDFSRC      = $(PDFSRCDIR)$(D)
PDFGEN      = $(PDFGENDIR)$(D)
PDFOBJ      = $(PDFOBJDIR)$(D)
PDFO_       = $(O_)$(PDFOBJ)
PLOBJ       = $(PLOBJDIR)$(D)

PDFCCC_WIN = $(CC) $(ZM) $(JPX_CFLAGS) $(D_)PDF_INCLUDED$(_D) $(I_)$(PDFSRCDIR)$(_I) $(I_)$(PDFGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) \
        $(I_)$(JPX_OPENJPEG_I_)$(_I) $(I_)$(JB2I_)$(_I) $(C_)

PDFCCC  = $(CC_) $(JPX_CFLAGS) $(D_)PDF_INCLUDED$(_D) $(I_)$(PDFSRCDIR)$(_I) $(I_)$(PDFGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) \
        $(I_)$(JPX_OPENJPEG_I_)$(_I) $(I_)$(JB2I_)$(_I) $(C_)

PDFJB2CC=$(CC_) $(JPX_CFLAGS) $(D_)PDF_INCLUDED$(_D) $(I_)$(PDFSRCDIR)$(_I) $(I_)$(PDFGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) \
	$(I_)$(JPX_OPENJPEG_I_)$(_I) $(I_)$(JB2I_)$(_I) $(JB2CF_) $(C_)

PDFLURCC=$(CC_) $(JPX_CFLAGS) $(D_)PDF_INCLUDED$(_D) $(I_)$(PDFSRCDIR)$(_I) $(I_)$(PDFGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) \
	$(I_)$(LWF_JPXI_)$(_I) $(I_)$(LDF_JB2I_)$(_I) $(JB2CF_) $(C_)

# Define the name of this makefile.
PDF_MAK     = $(PDFSRC)pdf.mak $(TOP_MAKEFILES)

PDF.clean: PDF.config-clean PDF.clean-not-config-clean

PDF.clean-not-config-clean:
	$(RM_) $(PDFOBJ)*.$(OBJ)

PDF.config-clean: clean_gs
	$(RM_) $(PDFOBJ)*.dev
	$(RM_) $(PDFOBJ)devs.tr5

PDF_TOP_OBJ=$(PDFOBJDIR)$(D)pdftop.$(OBJ)
PDF_TOP_OBJS= $(PDF_TOP_OBJ) $(PDFOBJDIR)$(D)pdfimpl.$(OBJ)

PDFINCLUDES=$(PDFSRC)*.h $(GLGEN)arch.h $(strmio_h) $(stream_h) $(gsmatrix_h) $(gslparam_h)\
	$(gstypes_h) $(szlibx_h) $(spngpx_h) $(sstring_h) $(sa85d_h) $(scfx_h) $(srlx_h)\
	$(jpeglib__h) $(sdct_h) $(spdiffx_h)

$(PDFOBJ)ghostpdf.$(OBJ): $(PDFSRC)ghostpdf.c $(PDFINCLUDES) $(plmain_h) $(stream_h) $(strmio_h) \
	$(gsmchunk_h) $(gsstate_h) $(gsicc_manage_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)ghostpdf.c $(PDFO_)ghostpdf.$(OBJ)

$(PDFOBJ)pdf_dict.$(OBJ): $(PDFSRC)pdf_dict.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_dict.c $(PDFO_)pdf_dict.$(OBJ)

$(PDFOBJ)pdf_array.$(OBJ): $(PDFSRC)pdf_array.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_array.c $(PDFO_)pdf_array.$(OBJ)

$(PDFOBJ)pdf_xref.$(OBJ): $(PDFSRC)pdf_xref.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_xref.c $(PDFO_)pdf_xref.$(OBJ)

$(PDFOBJ)pdf_fapi.$(OBJ): $(PDFSRC)pdf_fapi.c $(PDFINCLUDES) \
	$(memory__h) $(gsmemory_h) $(gserrors_h) $(gxdevice_h) $(gxfont_h) $(gxfont0_h) \
	$(gxfcid_h) $(gzstate_h) $(gxchar_h) $(gdebug_h) $(gxfapi_h) $(gscoord_h) \
	$(gspath_h) $(gscencs_h) $(gsagl_h) $(gxfont1_h) $(gscrypt1_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fapi.c $(PDFO_)pdf_fapi.$(OBJ)

$(PDFOBJ)pdf_font.$(OBJ): $(PDFSRC)pdf_font.c $(PDFINCLUDES) $(PDF_MAK) \
	$(gscencs_h) $(stream_h) $(strmio_h) $(gsstate_h) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font.c $(PDFO_)pdf_font.$(OBJ)

$(PDFOBJ)pdf_font0.$(OBJ): $(PDFSRC)pdf_font0.c $(PDFINCLUDES) $(PDF_MAK) \
	$(gxfont_h) $(gxfont0_h) $(gsutil_h) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font0.c $(PDFO_)pdf_font0.$(OBJ)

$(PDFOBJ)pdf_ciddec.$(OBJ): $(PDFSRC)pdf_ciddec.c $(PDFINCLUDES) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_ciddec.c $(PDFO_)pdf_ciddec.$(OBJ)

$(PDFOBJ)pdf_font1.$(OBJ): $(PDFSRC)pdf_font1.c $(PDFINCLUDES) \
	$(gsgdata_h) $(gstype1_h) $(gscencs_h) $(strmio_h) $(strimpl_h) $(stream_h) \
	$(sfilter_h) $(gxtype1_h) $(gsutil_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font1.c $(PDFO_)pdf_font1.$(OBJ)

$(PDFOBJ)pdf_font1C.$(OBJ): $(PDFSRC)pdf_font1C.c $(PDFINCLUDES) \
	$(gscedata_h) $(gscencs_h) $(gxfont0_h) $(gxfcid_h) \
	$(gxtype1_h) $(gsutil_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font1C.c $(PDFO_)pdf_font1C.$(OBJ)

$(PDFOBJ)pdf_fontps.$(OBJ): $(PDFSRC)pdf_fontps.c $(PDFINCLUDES) \
	$(scanchar_h) $(strimpl_h) $(stream_h) $(sfilter_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fontps.c $(PDFO_)pdf_fontps.$(OBJ)

$(PDFOBJ)pdf_font3.$(OBJ): $(PDFSRC)pdf_font3.c $(PDFINCLUDES) \
	$(gscencs_h) $(gscedata_h) $(gsccode_h) $(gsuid_h) $(gsutil_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font3.c $(PDFO_)pdf_font3.$(OBJ)

$(PDFOBJ)pdf_fontTT.$(OBJ): $(PDFSRC)pdf_fontTT.c $(PDFINCLUDES) \
	$(gxfont42_h) $(gscencs_h) $(gsagl_h) $(gsutil_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fontTT.c $(PDFO_)pdf_fontTT.$(OBJ)

$(PDFOBJ)pdf_font9.$(OBJ): $(PDFSRC)pdf_font9.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font9.c $(PDFO_)pdf_font9.$(OBJ)

$(PDFOBJ)pdf_font11.$(OBJ): $(PDFSRC)pdf_font11.c $(PDFINCLUDES) $(gxfont42_h) \
	$(gxfcid_h) $(gsutil_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font11.c $(PDFO_)pdf_font11.$(OBJ)

$(PDFOBJ)pdf_cmap.$(OBJ): $(PDFSRC)pdf_cmap.c $(PDFINCLUDES) \
	$(strmio_h) $(stream_h) $(scanchar_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_cmap.c $(PDFO_)pdf_cmap.$(OBJ)

$(PDFOBJ)pdf_fmap.$(OBJ): $(PDFSRC)pdf_fmap.c $(PDFINCLUDES) \
	$(strmio_h) $(stream_h) $(scanchar_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fmap.c $(PDFO_)pdf_fmap.$(OBJ)

$(PDFOBJ)pdf_text.$(OBJ): $(PDFSRC)pdf_text.c $(PDFINCLUDES) \
	$(gsstate_h) $(gsmatrix_h) $(gdevbbox_h) $(gspaint_h) \
	$(gscoord_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_text.c $(PDFO_)pdf_text.$(OBJ)

$(PDFOBJ)pdf_shading.$(OBJ): $(PDFSRC)pdf_shading.c $(PDFINCLUDES) \
	$(gsfunc3_h) $(gxshade_h) $(gsptype2_h) $(gsfunc0_h) \
	$(gscolor3_h) $(gsstate_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_shading.c $(PDFO_)pdf_shading.$(OBJ)

$(PDFOBJ)pdf_func.$(OBJ): $(PDFSRC)pdf_func.c $(PDFINCLUDES) \
	$(gsdsrc_h) $(gsfunc0_h) $(gsfunc3_h) $(gsfunc4_h) $(stream_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_func.c $(PDFO_)pdf_func.$(OBJ)

$(PDFOBJ)pdf_image.$(OBJ): $(PDFSRC)pdf_image.c $(PDFINCLUDES) \
	$(stream_h) $(gsicc_cache_h) $(gspath2_h) $(gsiparm4_h) $(gsiparm3_h) $(gsiparm3x_h) \
	$(gsform1_h) $(gstrans_h) $(gxdevsop_h) $(gspath_h) $(gsstate_h) $(gscoord_h) \
    $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_image.c $(PDFO_)pdf_image.$(OBJ)

$(PDFOBJ)pdf_page.$(OBJ): $(PDFSRC)pdf_page.c $(PDFINCLUDES) \
	$(gscoord_h) $(gspaint_h) $(gsstate_h) $(gspath2_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_page.c $(PDFO_)pdf_page.$(OBJ)

$(PDFOBJ)pdf_annot.$(OBJ): $(PDFSRC)pdf_annot.c $(PDFINCLUDES) $(gspath2_h) $(gxfarith_h) \
	$(gxdevsop_h) $(gsstrtok_h) $(gscoord_h) $(gsline_h) $(gsutil_h) \
	$(gspaint_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_annot.c $(PDFO_)pdf_annot.$(OBJ)

$(PDFOBJ)pdf_mark.$(OBJ): $(PDFSRC)pdf_mark.c $(PDFINCLUDES) $(gscoord_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_mark.c $(PDFO_)pdf_mark.$(OBJ)

$(PDFOBJ)pdf_sec.$(OBJ): $(PDFSRC)pdf_sec.c $(PDFINCLUDES) \
	$(strmio_h) $(smd5_h) $(sarc4_h) $(aes_h) $(sha2_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_sec.c $(PDFO_)pdf_sec.$(OBJ)

$(PDFOBJ)pdf_utf8_mswin32_.$(OBJ): $(PDFSRC)pdf_utf8.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC_WIN) $(PDFSRC)pdf_utf8.c $(PDFO_)pdf_utf8_mswin32_.$(OBJ)

$(PDFOBJ)pdf_utf8_metro_.$(OBJ): $(PDFOBJ)pdf_utf8_mswin32_.$(OBJ) $(PDF_MAK) $(MAKEDIRS)
	$(CP_) $(PDFOBJ)pdf_utf8_mswin32_.$(OBJ) $(PDFOBJ)pdf_utf8_metro.$(OBJ)

$(PDFOBJ)pdf_utf8_unix_.$(OBJ): $(PDFSRC)pdf_utf8.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_utf8.c $(PDFO_)pdf_utf8_unix_.$(OBJ)

$(PDFOBJ)pdf_utf8_openvms_.$(OBJ): $(PDFOBJ)pdf_utf8_unix_.$(OBJ) $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(CP_) $(PDFOBJ)pdf_utf8_unix_.$(OBJ) $(PDFOBJ)pdf_utf8_openvms_.$(OBJ)

$(PDFOBJ)pdf_utf8.$(OBJ): $(PDFOBJ)pdf_utf8_$(GSPLATFORM).$(OBJ) $(PDF_MAK) $(MAKEDIRS)
	$(CP_) $(PDFOBJ)pdf_utf8_$(GSPLATFORM).$(OBJ) $(PDFOBJ)pdf_utf8.$(OBJ)

$(PDFOBJ)pdf_stack.$(OBJ): $(PDFSRC)pdf_stack.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_stack.c $(PDFO_)pdf_stack.$(OBJ)

$(PDFOBJ)pdf_gstate.$(OBJ): $(PDFSRC)pdf_gstate.c $(PDFINCLUDES) $(gsstate_h) \
	$(gsmatrix_h) $(gslparam_h) $(gstparam_h) $(gxdht_h) $(gxht_h) $(gzht_h) $(gsht_h) \
	$(gscoord_h) $(gsutil_h) $(gscolor3_h) $(PDF_MAK) $(MAKEDIRS) $(gzpath_h) $(gspenum_h)
	$(PDFCCC) $(PDFSRC)pdf_gstate.c $(PDFO_)pdf_gstate.$(OBJ)

$(PDFOBJ)pdf_colour.$(OBJ): $(PDFSRC)pdf_colour.c $(PDFINCLUDES) \
	$(gsicc_manage_h) $(gsicc_profilecache_h) $(gsicc_create_h) $(gsicc_cache_h) $(gsptype2_h) $(gscsepr_h) \
	$(stream_h) $(strmio_h) $(gscdevn_h) $(gxcdevn_h) $(gscolor_h) $(gsicc_h) $(gsstate_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_colour.c $(PDFO_)pdf_colour.$(OBJ)

$(PDFOBJ)pdf_pattern.$(OBJ): $(PDFSRC)pdf_pattern.c $(PDFINCLUDES) \
	$(gsicc_manage_h) $(gsicc_profilecache_h) $(gsicc_create_h) $(gsptype2_h) \
	$(gxdevsop_h) $(gscsepr_h) $(stream_h) $(strmio_h) $(gscdevn_h) $(gscoord_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_pattern.c $(PDFO_)pdf_pattern.$(OBJ)

$(PDFOBJ)pdf_path.$(OBJ): $(PDFSRC)pdf_path.c $(PDFINCLUDES) $(gstypes_h) \
	$(gspath_h) $(gspaint_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_path.c $(PDFO_)pdf_path.$(OBJ)

$(PDFOBJ)pdf_loop_detect.$(OBJ): $(PDFSRC)pdf_loop_detect.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_loop_detect.c $(PDFO_)pdf_loop_detect.$(OBJ)

$(PDFOBJ)pdf_int.$(OBJ): $(PDFSRC)pdf_int.c $(PDFINCLUDES) $(plmain_h) \
	$(stream_h) $(strmio_h) $(gsgstate_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_int.c $(PDFO_)pdf_int.$(OBJ)

$(PDFOBJ)pdf_file_luratech.$(OBJ): $(PDFSRC)pdf_file.c $(sjpeg_h) $(stream_h) $(strimpl_h) \
	$(strmio_h) $(gpmisc_h) $(simscale_h) $(szlibx_h) $(spngx_h) $(spdiffx_h) $(slzw_h) \
	$(sstring_h) $(sa85d_h) $(scfx_h) $(srlx_h) $(jpeglib__h) $(sdct_h) $(sjpeg_h) \
	$(sfilter_h) $(sarc4_h) $(saes_h) $(ssha2_h) $(gxdevsop_h) \
	$(sjbig2_luratech_h) $(sjpx_luratech_h) \
	$(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFLURCC) $(PDFSRC)pdf_file.c $(PDFO_)pdf_file_luratech.$(OBJ)

$(PDFOBJ)pdf_file_jbig2dec.$(OBJ): $(PDFSRC)pdf_file.c $(sjpeg_h) $(stream_h) $(strimpl_h) \
	$(strmio_h) $(gpmisc_h) $(simscale_h) $(szlibx_h) $(spngx_h) $(spdiffx_h) $(slzw_h) \
	$(sstring_h) $(sa85d_h) $(scfx_h) $(srlx_h) $(jpeglib__h) $(sdct_h) $(sjpeg_h) \
	$(sfilter_h) $(sarc4_h) $(saes_h) $(ssha2_h) $(gxdevsop_h) \
	$(sjpx_openjpeg_h) \
	$(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFJB2CC) $(PDFSRC)pdf_file.c $(PDFO_)pdf_file_jbig2dec.$(OBJ)

$(PDFOBJ)pdf_file.$(OBJ): $(PDFOBJ)pdf_file_$(JBIG2_LIB).$(OBJ)
	$(CP_) $(PDFOBJ)pdf_file_$(JBIG2_LIB).$(OBJ) $(PDFOBJ)pdf_file.$(OBJ)

$(PDFOBJ)pdf_trans.$(OBJ): $(PDFSRC)pdf_trans.c $(PDFINCLUDES) $(gstparam_h) \
	$(gsicc_manage_h) $(gscoord_h) $(gsstate_h) $(gspath_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_trans.c $(PDFO_)pdf_trans.$(OBJ)

$(PDFOBJ)pdf_device.$(OBJ): $(PDFSRC)pdf_device.c $(PDFINCLUDES) $(gsdevice_h) $(gspaint_h) \
	$(gdevvec_h) $(gxdevsop_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_device.c $(PDFO_)pdf_device.$(OBJ)

$(PDFOBJ)pdf_misc.$(OBJ): $(PDFSRC)pdf_misc.c $(PDFINCLUDES) $(gspath_h) $(gspaint_h) \
	$(gsicc_manage_h) $(gsstate_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_misc.c $(PDFO_)pdf_misc.$(OBJ)

$(PDFOBJ)pdf_optcontent.$(OBJ): $(PDFSRC)pdf_optcontent.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_optcontent.c $(PDFO_)pdf_optcontent.$(OBJ)

$(PDFOBJ)pdf_check.$(OBJ): $(PDFSRC)pdf_check.c $(PDFINCLUDES) $(gsdevice_h) $(gspaint_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_check.c $(PDFO_)pdf_check.$(OBJ)

$(PDFOBJ)pdf_deref.$(OBJ): $(PDFSRC)pdf_deref.c $(PDFINCLUDES) $(strmio_h) $(stream_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_deref.c $(PDFO_)pdf_deref.$(OBJ)

$(PDFOBJ)pdf_repair.$(OBJ): $(PDFSRC)pdf_repair.c $(PDFINCLUDES) \
	$(strmio_h) $(stream_h) \
	$(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_repair.c $(PDFO_)pdf_repair.$(OBJ)

$(PDFOBJ)pdf_obj.$(OBJ): $(PDFSRC)pdf_obj.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_obj.c $(PDFO_)pdf_obj.$(OBJ)

$(PDFOBJ)pdf_doc.$(OBJ): $(PDFSRC)pdf_doc.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_doc.c $(PDFO_)pdf_doc.$(OBJ)

$(PDFGEN)pdfimpl.c: $(PLSRC)plimpl.c $(PDF_MAK) $(MAKEDIRS)
	$(CP_) $(PLSRC)plimpl.c $(PDFGEN)pdfimpl.c

$(PLOBJ)pdfimpl.$(OBJ): $(PDFGEN)pdfimpl.c          \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)                  \
                        $(PDF_MAK)                  \
                        $(MAKEDIRS)
	$(PDFCCC) $(PDFGEN)pdfimpl.c $(PDFO_)pdfimpl.$(OBJ)

$(PDF_TOP_OBJ): $(PDFSRC)pdftop.c $(plmain_h) $(pltop_h) $(PDFINCLUDES) \
    $(GLGEN)gconfig.$(OBJ) $(pltop_h) $(plmain_h) $(plparse_h) $(gxdevice_h) \
    $(gxht_h) $(gsht1_h) $(pconfig_h) \
     $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(D_)GS_LIB_DEFAULT=$(GS_LIB_DEFAULT)$(_D) $(D_)COMPILE_INITS=$(COMPILE_INITS)$(_D) $(PDFSRC)pdftop.c $(PDFO_)pdftop.$(OBJ)

PDF_OBJS=\
    $(PDFOBJ)pdf_loop_detect.$(OBJ)\
    $(PDFOBJ)ghostpdf.$(OBJ)\
    $(PDFOBJ)pdf_dict.$(OBJ)\
    $(PDFOBJ)pdf_array.$(OBJ)\
    $(PDFOBJ)pdf_xref.$(OBJ)\
    $(PDFOBJ)pdf_int.$(OBJ)\
    $(PDFOBJ)pdf_file.$(OBJ)\
    $(PDFOBJ)pdf_path.$(OBJ)\
    $(PDFOBJ)pdf_colour.$(OBJ)\
    $(PDFOBJ)pdf_pattern.$(OBJ)\
    $(PDFOBJ)pdf_gstate.$(OBJ)\
    $(PDFOBJ)pdf_stack.$(OBJ)\
    $(PDFOBJ)pdf_image.$(OBJ)\
    $(PDFOBJ)pdf_page.$(OBJ)\
    $(PDFOBJ)pdf_annot.$(OBJ)\
    $(PDFOBJ)pdf_mark.$(OBJ)\
    $(PDFOBJ)pdf_fapi.$(OBJ)\
    $(PDFOBJ)pdf_font.$(OBJ)\
    $(PDFOBJ)pdf_font0.$(OBJ)\
    $(PDFOBJ)pdf_ciddec.$(OBJ)\
    $(PDFOBJ)pdf_font1.$(OBJ)\
    $(PDFOBJ)pdf_font1C.$(OBJ)\
    $(PDFOBJ)pdf_fontps.$(OBJ)\
    $(PDFOBJ)pdf_font3.$(OBJ)\
    $(PDFOBJ)pdf_fontTT.$(OBJ)\
    $(PDFOBJ)pdf_font9.$(OBJ)\
    $(PDFOBJ)pdf_font11.$(OBJ)\
    $(PDFOBJ)pdf_cmap.$(OBJ)\
    $(PDFOBJ)pdf_fmap.$(OBJ)\
    $(PDFOBJ)pdf_text.$(OBJ)\
    $(PDFOBJ)pdf_shading.$(OBJ)\
    $(PDFOBJ)pdf_func.$(OBJ)\
    $(PDFOBJ)pdf_trans.$(OBJ)\
    $(PDFOBJ)pdf_device.$(OBJ)\
    $(PDFOBJ)pdf_misc.$(OBJ)\
    $(PDFOBJ)pdf_optcontent.$(OBJ)\
    $(PDFOBJ)pdf_check.$(OBJ)\
    $(PDFOBJ)pdf_sec.$(OBJ)\
    $(PDFOBJ)pdf_utf8.$(OBJ)\
    $(PDFOBJ)pdf_deref.$(OBJ)\
    $(PDFOBJ)pdf_repair.$(OBJ)\
    $(PDFOBJ)pdf_obj.$(OBJ)\
    $(PDFOBJ)pdf_doc.$(OBJ)\


# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==PDFGEN is required.

$(PDFOBJ)pdfi.dev: $(ECHOGS_XE) $(PDF_OBJS) \
		$(PDFGEN)func4lib.dev \
		$(PDFGEN)pdiff.dev $(PDFGEN)psfilters.dev $(PDFGEN)saes.dev $(PDFGEN)ssha2.dev $(PDFGEN)sjpx.dev $(PDFGEN)psfilters.dev \
		$(PDFGEN)sdct.dev $(PDFGEN)simscale.dev $(GLD)gsagl.dev \
		$(PDF_MAK) $(MAKEDIRS)
	$(SETMOD) $(PDFOBJ)pdfi $(PDF_OBJS)
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)func4lib.dev $(GLD)gsagl.dev
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)pdiff.dev $(PDFGEN)psfilters.dev $(PDFGEN)saes.dev $(PDFGEN)sjpx.dev
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)ssha2.dev $(PDFGEN)psfilters.dev $(PDFGEN)sdct.dev $(PDFGEN)simscale.dev

$(PDFOBJ)gpdf.dev: $(ECHOGS_XE) $(PDF_OBJS) \
                  $(PDFGEN)pl.dev $(PDFGEN)$(PL_SCALER).dev $(PDFGEN)pjl.dev $(PDFOBJ)pdfi.dev \
                  $(PDF_MAK) $(MAKEDIRS)
	$(SETMOD) $(PDFOBJ)gpdf -include $(PDFOBJ)pdfi.dev $(PDFGEN)$(PL_SCALER) $(PDFGEN)pjl.dev
