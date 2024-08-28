# Copyright (C) 2001-2024 Artifex Software, Inc.
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
# makefile for OCR code.
# Users of this makefile must define the following:
#	LEPTONICALIBDIR - the leptonica source directory
#	TESSERACTLIBDIR - the tesseract source directory
#	OCR_VERSION - which OCR implementation are we using.

# Define the name of this makefile.
LIBOCR_MAK=$(GLSRC)ocr.mak $(TOP_MAKEFILES)
OCRCXX = $(CXX) $(TESSINCLUDES) $(TESSCXXFLAGS) $(CCFLAGS) -DTESSERACT_IMAGEDATA_AS_PIX -DTESSERACT_DISABLE_DEBUG_FONTS -DGRAPHICS_DISABLED -UCLUSTER

# Tesseract veneer.
$(GLGEN)tessocr.$(OBJ) : $(GLSRC)tessocr.cpp $(GLSRC)tessocr.h $(LIBOCR_MAK) \
	$(gsmemory_h) $(gxiodev_h) $(stream_h) $(TESSDEPS)
	$(OCRCXX) $(D_)OCR_SHARED=$(OCR_SHARED)$(_D) $(D_)LEPTONICA_INTERCEPT_ALLOC=1$(_D) $(I_)$(GLGEN)$(_I) $(GLO_)tessocr.$(OBJ) $(C_) $(D_)TESSDATA="$(TESSDATA)"$(_D) $(GLSRC)tessocr.cpp

# 0_0 = No version.
# .dev files dont' allow comments, but write a space,
# just so there is something there
$(GLGEN)libocr_0_0.dev : $(LIBOCR_MAK) $(ECHOGS_XE) $(MAKEDIRS)
	$(ECHOGS_XE) -w $(GLGEN)libocr_0_0.dev -n -x 20

# 1_0 = Tesseract/Leptonica (local source)
$(GLGEN)libocr_1_0.dev : $(LIBOCR_MAK) $(ECHOGS_XE) \
 $(GLGEN)tessocr.$(OBJ) $(LEPTONICA_OBJS) $(TESSERACT_OBJS_1) \
 $(TESSERACT_OBJS_2) $(TESSERACT_OBJS_3) $(TESSERACT_OBJS_4) \
 $(TESSERACT_LEGACY) $(MAKEDIRS)
	$(SETMOD) $(GLGEN)libocr_1_0 $(GLGEN)tessocr.$(OBJ)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(LEPTONICA_OBJS)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(TESSERACT_OBJS_1)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(TESSERACT_OBJS_2)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(TESSERACT_OBJS_3)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(TESSERACT_OBJS_4)
	$(ADDMOD) $(GLGEN)libocr_1_0 $(TESSERACT_LEGACY)

# 1_1 = Tesseract/Leptonica (shared lib)
$(GLGEN)libocr_1_1.dev : $(LIBOCR_MAK) $(ECHOGS_XE) \
 $(GLGEN)tessocr.$(OBJ) $(MAKEDIRS)
	$(SETMOD) $(GLGEN)libocr_1_1 $(GLGEN)tessocr.$(OBJ)

$(GLGEN)libocr.dev : $(LIBOCR_MAK) $(ECHOGS_XE) \
 $(GLGEN)libocr_$(OCR_VERSION)_$(OCR_SHARED).dev $(MAKEDIRS)
	$(CP_) $(GLGEN)libocr_$(OCR_VERSION)_$(OCR_SHARED).dev $(GLGEN)libocr.dev
