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
# makefile for OCR code.
# Users of this makefile must define the following:
#	LEPTONICALIBDIR - the leptonica source directory
#	TESSERACTLIBDIR - the tesseract source directory
#	OCR_VERSION - which OCR implementation are we using.

# Define the name of this makefile.
LIBOCR_MAK=$(GLSRC)ocr.mak $(TOP_MAKEFILES)

$(GLGEN)libocr.dev : $(LIBOCR_MAK) $(ECHOGS_XE)$(MAKEDIRS)\
 $(GLGEN)libocr_$(OCR_VERSION).dev
	$(CP_) $(GLGEN)libocr_$(OCR_VERSION).dev $(GLGEN)libocr.dev

# Tesseract veneer.
$(GLGEN)tessocr.$(OBJ) : $(GLSRC)tessocr.cpp $(GLSRC)tessocr.h $(LIBOCR_MAK) \
	$(gsmemory_h) $(gxiodev_h) $(stream_h) $(TESSDEPS)
	$(TESSCXX) $(D_)LEPTONICA_INTERCEPT_MALLOC=1$(_D) $(I_)$(LEPTONICADIR)$(D)src$(_I) $(GLO_)tessocr.$(OBJ) $(C_) $(D_)TESSDATA="$(TESSDATA)"$(_D) $(GLSRC)tessocr.cpp

# 0 = No version.

# 1 = Tesseract/Leptonica
$(GLGEN)libocr_1.dev : $(LIBOCR_MAK) $(ECHOGS_XE) $(MAKEDIRS) \
 $(GLGEN)tessocr.$(OBJ) $(LEPTONICA_OBJS) $(TESSERACT_OBJS_1) \
 $(TESSERACT_OBJS_2) $(TESSERACT_OBJS_3) $(TESSERACT_OBJS_4) \
 $(TESSERACT_LEGACY)
	$(SETMOD) $(GLGEN)libocr_1 $(GLGEN)tessocr.$(OBJ)
	$(ADDMOD) $(GLGEN)libocr_1 $(LEPTONICA_OBJS)
	$(ADDMOD) $(GLGEN)libocr_1 $(TESSERACT_OBJS_1)
	$(ADDMOD) $(GLGEN)libocr_1 $(TESSERACT_OBJS_2)
	$(ADDMOD) $(GLGEN)libocr_1 $(TESSERACT_OBJS_3)
	$(ADDMOD) $(GLGEN)libocr_1 $(TESSERACT_OBJS_4)
	$(ADDMOD) $(GLGEN)libocr_1 $(TESSERACT_LEGACY)
