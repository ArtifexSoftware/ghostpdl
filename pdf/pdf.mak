# Copyright (C) 2001-2018 Artifex Software, Inc.
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
EXPATINCDIR = $(EXPATSRCDIR)$(D)lib
PLOBJ       = $(PLOBJDIR)$(D)

PDFCCC  = $(CC_) $(D_)PDF_INCLUDED$(_D) $(I_)$(PDFSRCDIR)$(_I) $(I_)$(PDFGENDIR)$(_I) \
	$(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) \
	$(I_)$(EXPATINCDIR)$(_I) $(I_)$(JPEGXR_SRCDIR)$(_I) $(I_)$(ZSRCDIR)$(_I) $(C_)

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

PDFINCLUDES=$(PDFSRC)*.h $(PDFOBJ)arch.h $(strmio_h) $(stream_h)

$(PDFOBJ)pdf_path.$(OBJ): $(PDFSRC)pdf_path.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_path.c $(XPSO_)pdf_path.$(OBJ)

$(PDFOBJ)pdf_loop_detect.$(OBJ): $(PDFSRC)pdf_loop_detect.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_loop_detect.c $(XPSO_)pdf_loop_detect.$(OBJ)

$(PDFOBJ)pdf_int.$(OBJ): $(PDFSRC)pdf_int.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_int.c $(XPSO_)pdf_int.$(OBJ)

$(PDFOBJ)pdf_file.$(OBJ): $(PDFSRC)pdf_file.c $(PDFINCLUDES) $(PDF_MAK) $(MAKEDIRS)
	$(PDFCCC) $(PDFSRC)pdf_file.c $(XPSO_)pdf_file.$(OBJ)

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
    $(PDFOBJ)pdf_int.$(OBJ)\
    $(PDFOBJ)pdf_file.$(OBJ)\
    $(PDFOBJ)pdf_path.$(OBJ)



# NB - note this is a bit squirrely.  Right now the pjl interpreter is
# required and shouldn't be and PLOBJ==PDFGEN is required.

$(PDFOBJ)gpdf.dev: $(ECHOGS_XE) $(PDF_OBJS)  $(PDFOBJDIR)$(D)expat.dev $(JPEGXR_GENDIR)$(D)jpegxr.dev \
                  $(PDFGEN)pl.dev $(PDFGEN)$(PL_SCALER).dev $(PDFGEN)pjl.dev $(PDF_MAK) $(MAKEDIRS)
	$(SETMOD) $(PDFOBJ)gpdf $(PDF_OBJS)
	$(ADDMOD) $(PDFOBJ)gpdf -include $(PDFGEN)$(PL_SCALER) $(PDFGEN)pjl.dev
