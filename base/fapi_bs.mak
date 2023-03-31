# Copyright (C) 2001-2023 Artifex Software, Inc.
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
#
# makefile for Bitstream as part of the monolithic gs build.
#
# WARNING: the FAPI/Bitstream interface is incomplete.
#
# Users of this makefile must define the following:
#	BITSTREAM_ROOT    - the source directory
#	GLGEN    - the generated intermediate file directory
#	GLOBJ    - the object file directory
#	BITSTREAM_CFLAGS   - The include options for the Bitstream library

# Define the name of this makefile.
FAPI_BS_MAK=$(GLSRC)fapi_bs.mak $(TOP_MAKEFILES)

# Bitstream bridge :

BITSTREAM_LIB=$(BITSTREAM_ROOT)$(D)core$(D)
BITSTREAM_INC=$(I_)"$(BITSTREAM_ROOT)$(D)core"

$(GLD)fapib1.dev : $(FAPI_BS_MAK) $(ECHOGS_XE) \
 $(GLOBJ)fapibstm.$(OBJ) $(GLOBJ)t2k.$(OBJ) $(GLOBJ)t2kextra.$(OBJ) $(GLOBJ)tsimem.$(OBJ)\
   $(GLOBJ)t2ktt.$(OBJ) $(GLOBJ)cstream.$(OBJ) $(GLOBJ)fft1hint.$(OBJ) $(GLOBJ)ghints.$(OBJ)\
   $(GLOBJ)glyph.$(OBJ) $(GLOBJ)t1.$(OBJ) $(GLOBJ)t2kstrm.$(OBJ) $(GLOBJ)truetype.$(OBJ)\
   $(GLOBJ)util.$(OBJ) $(GLOBJ)fnt.$(OBJ) $(GLOBJ)pclread.$(OBJ) $(GLOBJ)t2ksc.$(OBJ)\
   $(GLOBJ)write_t1.$(OBJ) $(GLOBJ)write_t2.$(OBJ) $(GLOBJ)wrfont.$(OBJ) $(MAKEDIRS)
	$(SETMOD) $(GLD)fapib1 $(GLOBJ)fapibstm.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)t2k.$(OBJ) $(GLOBJ)t2kextra.$(OBJ) $(GLOBJ)fnt.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)tsimem.$(OBJ) $(GLOBJ)t2ktt.$(OBJ) $(GLOBJ)util.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)t2kstrm.$(OBJ) $(GLOBJ)truetype.$(OBJ) $(GLOBJ)cstream.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)fft1hint.$(OBJ) $(GLOBJ)ghints.$(OBJ) $(GLOBJ)glyph.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)t1.$(OBJ) $(GLOBJ)pclread.$(OBJ) $(GLOBJ)t2ksc.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 $(GLOBJ)write_t1.$(OBJ) $(GLOBJ)write_t2.$(OBJ) $(GLOBJ)wrfont.$(OBJ)
	$(ADDMOD) $(GLD)fapib1 -plugin fapibstm

$(GLOBJ)fapibstm.$(OBJ) : $(FAPI_BS_MAK)  $(GLSRC)fapibstm.c $(AK)\
 $(stdio__h) $(memory__h) $(math__h) $(strmio_h)\
 $(ierrors_h) $(iplugin_h) $(gxfapi_h) $(gxfapi_h) $(gp_h)  $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)fapibstm.$(OBJ) $(C_) $(GLSRC)fapibstm.c

$(GLOBJ)t2k.$(OBJ) : $(FAPI_BS_MAK)  "$(BITSTREAM_LIB)t2k.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t2k.$(OBJ) $(C_) "$(BITSTREAM_LIB)t2k.c"

$(GLOBJ)t2kextra.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)t2kextra.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t2kextra.$(OBJ) $(C_) "$(BITSTREAM_LIB)t2kextra.c"

$(GLOBJ)tsimem.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)tsimem.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)tsimem.$(OBJ) $(C_) "$(BITSTREAM_LIB)tsimem.c"

$(GLOBJ)t2ktt.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)t2ktt.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t2ktt.$(OBJ) $(C_) "$(BITSTREAM_LIB)t2ktt.c"

$(GLOBJ)cstream.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)cstream.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)cstream.$(OBJ) $(C_) "$(BITSTREAM_LIB)cstream.c"

$(GLOBJ)fft1hint.$(OBJ) : $(FAPI_BS_MAK)  "$(BITSTREAM_LIB)fft1hint.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)fft1hint.$(OBJ) $(C_) "$(BITSTREAM_LIB)fft1hint.c"

$(GLOBJ)ghints.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)ghints.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)ghints.$(OBJ) $(C_) "$(BITSTREAM_LIB)ghints.c"

$(GLOBJ)glyph.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)glyph.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)glyph.$(OBJ) $(C_) "$(BITSTREAM_LIB)glyph.c"

$(GLOBJ)t1.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)t1.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t1.$(OBJ) $(C_) "$(BITSTREAM_LIB)t1.c"

$(GLOBJ)t2kstrm.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)t2kstrm.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t2kstrm.$(OBJ) $(C_) "$(BITSTREAM_LIB)t2kstrm.c"

$(GLOBJ)truetype.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)truetype.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)truetype.$(OBJ) $(C_) "$(BITSTREAM_LIB)truetype.c"

$(GLOBJ)util.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)util.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)util.$(OBJ) $(C_) "$(BITSTREAM_LIB)util.c"

$(GLOBJ)fnt.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)fnt.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)fnt.$(OBJ) $(C_) "$(BITSTREAM_LIB)fnt.c"

$(GLOBJ)pclread.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)pclread.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)pclread.$(OBJ) $(C_) "$(BITSTREAM_LIB)pclread.c"

$(GLOBJ)t2ksc.$(OBJ) : $(FAPI_BS_MAK) "$(BITSTREAM_LIB)t2ksc.c" $(AK) $(MAKEDIRS)
	$(GLCC) $(BITSTREAM_CFLAGS) $(BITSTREAM_INC) $(GLO_)t2ksc.$(OBJ) $(C_) "$(BITSTREAM_LIB)t2ksc.c"
