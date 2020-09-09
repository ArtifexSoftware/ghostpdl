# Copyright (C) 2001-2020 Artifex Software, Inc.
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

PDFINCLUDES=$(PDFSRC)*.h $(PDFOBJ)arch.h $(strmio_h) $(stream_h) $(gsmatrix_h) $(gslparam_h)\
	$(gstypes_h) $(szlibx_h) $(spngpx_h) $(sstring_h) $(sa85d_h) $(scfx_h) $(srlx_h)\
	$(jpeglib__h) $(sdct_h) $(spdiffx_h)

$(PDFOBJ)ghostpdf.$(OBJ): $(PDFSRC)ghostpdf.c $(PDFINCLUDES) $(plmain_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)ghostpdf.c $(PDFO_)ghostpdf.$(OBJ)

$(PDFOBJ)pdf_dict.$(OBJ): $(PDFSRC)pdf_dict.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_dict.c $(PDFO_)pdf_dict.$(OBJ)

$(PDFOBJ)pdf_array.$(OBJ): $(PDFSRC)pdf_array.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_array.c $(PDFO_)pdf_array.$(OBJ)

$(PDFOBJ)pdf_xref.$(OBJ): $(PDFSRC)pdf_xref.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_xref.c $(PDFO_)pdf_xref.$(OBJ)

$(PDFOBJ)pdf_agl.$(OBJ): $(PDFSRC)pdf_agl.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_agl.c $(PDFO_)pdf_agl.$(OBJ)

$(PDFOBJ)pdf_fapi.$(OBJ): $(PDFSRC)pdf_fapi.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fapi.c $(PDFO_)pdf_fapi.$(OBJ)

$(PDFOBJ)pdf_font.$(OBJ): $(PDFSRC)pdf_font.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font.c $(PDFO_)pdf_font.$(OBJ)

$(PDFOBJ)pdf_font0.$(OBJ): $(PDFSRC)pdf_font0.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font0.c $(PDFO_)pdf_font0.$(OBJ)

$(PDFOBJ)pdf_font1.$(OBJ): $(PDFSRC)pdf_font1.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font1.c $(PDFO_)pdf_font1.$(OBJ)

$(PDFOBJ)pdf_font1C.$(OBJ): $(PDFSRC)pdf_font1C.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font1C.c $(PDFO_)pdf_font1C.$(OBJ)

$(PDFOBJ)pdf_font3.$(OBJ): $(PDFSRC)pdf_font3.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font3.c $(PDFO_)pdf_font3.$(OBJ)

$(PDFOBJ)pdf_fontTT.$(OBJ): $(PDFSRC)pdf_fontTT.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_fontTT.c $(PDFO_)pdf_fontTT.$(OBJ)

$(PDFOBJ)pdf_font9.$(OBJ): $(PDFSRC)pdf_font9.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font9.c $(PDFO_)pdf_font9.$(OBJ)

$(PDFOBJ)pdf_font11.$(OBJ): $(PDFSRC)pdf_font11.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_font11.c $(PDFO_)pdf_font11.$(OBJ)

$(PDFOBJ)pdf_cmap.$(OBJ): $(PDFSRC)pdf_cmap.c $(PDFINCLUDES) \
$(gxfcmap1_h) $(strmio_h) $(stream_h) $(scanchar_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_cmap.c $(PDFO_)pdf_cmap.$(OBJ)

$(PDFOBJ)pdf_text.$(OBJ): $(PDFSRC)pdf_text.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_text.c $(PDFO_)pdf_text.$(OBJ)

$(PDFOBJ)pdf_shading.$(OBJ): $(PDFSRC)pdf_shading.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_shading.c $(PDFO_)pdf_shading.$(OBJ)

$(PDFOBJ)pdf_func.$(OBJ): $(PDFSRC)pdf_func.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_func.c $(PDFO_)pdf_func.$(OBJ)

$(PDFOBJ)pdf_image.$(OBJ): $(PDFSRC)pdf_image.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_image.c $(PDFO_)pdf_image.$(OBJ)

$(PDFOBJ)pdf_page.$(OBJ): $(PDFSRC)pdf_page.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_page.c $(PDFO_)pdf_page.$(OBJ)

$(PDFOBJ)pdf_annot.$(OBJ): $(PDFSRC)pdf_annot.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_annot.c $(PDFO_)pdf_annot.$(OBJ)

$(PDFOBJ)pdf_mark.$(OBJ): $(PDFSRC)pdf_mark.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_mark.c $(PDFO_)pdf_mark.$(OBJ)

$(PDFOBJ)pdf_sec.$(OBJ): $(PDFSRC)pdf_sec.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_sec.c $(PDFO_)pdf_sec.$(OBJ)

$(PDFOBJ)pdf_utf8.$(OBJ): $(PDFSRC)pdf_utf8.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC_WIN) $(PDFSRC)pdf_utf8.c $(PDFO_)pdf_utf8.$(OBJ)

$(PDFOBJ)pdf_stack.$(OBJ): $(PDFSRC)pdf_stack.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_stack.c $(PDFO_)pdf_stack.$(OBJ)

$(PDFOBJ)pdf_gstate.$(OBJ): $(PDFSRC)pdf_gstate.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_gstate.c $(PDFO_)pdf_gstate.$(OBJ)

$(PDFOBJ)pdf_colour.$(OBJ): $(PDFSRC)pdf_colour.c $(PDFINCLUDES) $(gscolor1_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_colour.c $(PDFO_)pdf_colour.$(OBJ)

$(PDFOBJ)pdf_pattern.$(OBJ): $(PDFSRC)pdf_pattern.c $(PDFINCLUDES) $(gscolor1_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_pattern.c $(PDFO_)pdf_pattern.$(OBJ)

$(PDFOBJ)pdf_path.$(OBJ): $(PDFSRC)pdf_path.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_path.c $(PDFO_)pdf_path.$(OBJ)

$(PDFOBJ)pdf_loop_detect.$(OBJ): $(PDFSRC)pdf_loop_detect.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_loop_detect.c $(PDFO_)pdf_loop_detect.$(OBJ)

$(PDFOBJ)pdf_int.$(OBJ): $(PDFSRC)pdf_int.c $(PDFINCLUDES) $(plmain_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_int.c $(PDFO_)pdf_int.$(OBJ)

$(PDFOBJ)pdf_file_luratech.$(OBJ): $(PDFSRC)pdf_file.c $(sjpeg_h) $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFLURCC) $(PDFSRC)pdf_file.c $(PDFO_)pdf_file_luratech.$(OBJ)

$(PDFOBJ)pdf_file_jbig2dec.$(OBJ): $(PDFSRC)pdf_file.c $(sjpeg_h) $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFJB2CC) $(PDFSRC)pdf_file.c $(PDFO_)pdf_file_jbig2dec.$(OBJ)

$(PDFOBJ)pdf_file.$(OBJ): $(PDFOBJ)pdf_file_$(JBIG2_LIB).$(OBJ)
	$(CP_) $(PDFOBJ)pdf_file_$(JBIG2_LIB).$(OBJ) $(PDFOBJ)pdf_file.$(OBJ)

$(PDFOBJ)pdf_trans.$(OBJ): $(PDFSRC)pdf_trans.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_trans.c $(PDFO_)pdf_trans.$(OBJ)

$(PDFOBJ)pdf_device.$(OBJ): $(PDFSRC)pdf_device.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_device.c $(PDFO_)pdf_device.$(OBJ)

$(PDFOBJ)pdf_misc.$(OBJ): $(PDFSRC)pdf_misc.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_misc.c $(PDFO_)pdf_misc.$(OBJ)

$(PDFOBJ)pdf_optcontent.$(OBJ): $(PDFSRC)pdf_optcontent.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_optcontent.c $(PDFO_)pdf_optcontent.$(OBJ)

$(PDFOBJ)pdf_check.$(OBJ): $(PDFSRC)pdf_check.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_check.c $(PDFO_)pdf_check.$(OBJ)

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

$(PDF_TOP_OBJ): $(PDFSRC)pdftop.c $(plmain_h) $(pltop_h) $(PDFINCLUDES) $(GLOBJ)gconfig.$(OBJ) \
                $(pconfig_h) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdftop.c $(PDFO_)pdftop.$(OBJ)

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
    $(PDFOBJ)pdf_agl.$(OBJ) \
    $(PDFOBJ)pdf_fapi.$(OBJ)\
    $(PDFOBJ)pdf_font.$(OBJ)\
    $(PDFOBJ)pdf_font0.$(OBJ)\
    $(PDFOBJ)pdf_font1.$(OBJ)\
    $(PDFOBJ)pdf_font1C.$(OBJ)\
    $(PDFOBJ)pdf_font3.$(OBJ)\
    $(PDFOBJ)pdf_fontTT.$(OBJ)\
    $(PDFOBJ)pdf_font9.$(OBJ)\
    $(PDFOBJ)pdf_font11.$(OBJ)\
    $(PDFOBJ)pdf_cmap.$(OBJ)\
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


# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==PDFGEN is required.

$(PDFOBJ)pdfi.dev: $(ECHOGS_XE) $(PDF_OBJS) \
                  $(PDFGEN)func4lib.dev \
                  $(PDFGEN)pdiff.dev $(PDFGEN)psfilters.dev $(PDFGEN)saes.dev $(PDFGEN)ssha2.dev $(PDFGEN)sjpx.dev $(PDFGEN)psfilters.dev \
                  $(PDFGEN)sdct.dev \
                  $(PDF_MAK) $(MAKEDIRS)
	$(SETMOD) $(PDFOBJ)pdfi $(PDF_OBJS)
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)func4lib.dev
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)pdiff.dev $(PDFGEN)psfilters.dev $(PDFGEN)saes.dev $(PDFGEN)sjpx.dev
	$(ADDMOD) $(PDFOBJ)pdfi -include $(PDFGEN)ssha2.dev $(PDFGEN)psfilters.dev $(PDFGEN)sdct.dev

$(PDFOBJ)gpdf.dev: $(ECHOGS_XE) $(PDF_OBJS) \
                  $(PDFGEN)pl.dev $(PDFGEN)$(PL_SCALER).dev $(PDFGEN)pjl.dev $(PDFOBJ)pdfi.dev \
                  $(PDF_MAK) $(MAKEDIRS)
	$(SETMOD) $(PDFOBJ)gpdf -include $(PDFOBJ)pdfi.dev $(PDFGEN)$(PL_SCALER) $(PDFGEN)pjl.dev
