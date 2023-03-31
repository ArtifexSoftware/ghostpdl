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

extract_cc = $(CC) $(CCFLAGS) -DEXTRACT_CV=0 $(I_)$(EXTRACT_DIR)$(D)include$(_I) $(I_)$(EXTRACT_DIR)$(D)src$(_I) $(I_)$(ZSRCDIR)$(_I) $(O_)
extract_out_prefix = $(GLOBJDIR)$(D)extract_

$(extract_out_prefix)alloc.$(OBJ):          $(EXTRACT_DIR)/src/alloc.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/alloc.c

$(extract_out_prefix)astring.$(OBJ):        $(EXTRACT_DIR)/src/astring.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/astring.c

$(extract_out_prefix)boxer.$(OBJ):         $(EXTRACT_DIR)/src/boxer.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/boxer.c

$(extract_out_prefix)buffer.$(OBJ):         $(EXTRACT_DIR)/src/buffer.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/buffer.c

$(extract_out_prefix)document.$(OBJ):       $(EXTRACT_DIR)/src/document.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/document.c

$(extract_out_prefix)docx.$(OBJ):           $(EXTRACT_DIR)/src/docx.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/docx.c

$(extract_out_prefix)docx_template.$(OBJ):  $(EXTRACT_DIR)/src/docx_template.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/docx_template.c

$(extract_out_prefix)extract.$(OBJ):        $(EXTRACT_DIR)/src/extract.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/extract.c

$(extract_out_prefix)join.$(OBJ):           $(EXTRACT_DIR)/src/join.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/join.c

$(extract_out_prefix)html.$(OBJ):            $(EXTRACT_DIR)/src/html.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/html.c

$(extract_out_prefix)json.$(OBJ):           $(EXTRACT_DIR)/src/json.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/json.c

$(extract_out_prefix)mem.$(OBJ):            $(EXTRACT_DIR)/src/mem.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/mem.c

$(extract_out_prefix)odt.$(OBJ):            $(EXTRACT_DIR)/src/odt.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/odt.c

$(extract_out_prefix)odt_template.$(OBJ):   $(EXTRACT_DIR)/src/odt_template.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/odt_template.c

$(extract_out_prefix)outf.$(OBJ):           $(EXTRACT_DIR)/src/outf.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/outf.c

$(extract_out_prefix)rect.$(OBJ):           $(EXTRACT_DIR)/src/rect.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/rect.c

$(extract_out_prefix)sys.$(OBJ):           $(EXTRACT_DIR)/src/sys.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/sys.c

$(extract_out_prefix)text.$(OBJ):           $(EXTRACT_DIR)/src/text.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/text.c

$(extract_out_prefix)xml.$(OBJ):            $(EXTRACT_DIR)/src/xml.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/xml.c

$(extract_out_prefix)zip.$(OBJ):            $(EXTRACT_DIR)/src/zip.c $(MAKEDIRS)
	$(extract_cc)$@ $(C_) $(EXTRACT_DIR)/src/zip.c

EXTRACT_OBJS = \
	$(extract_out_prefix)alloc.$(OBJ) \
	$(extract_out_prefix)astring.$(OBJ) \
	$(extract_out_prefix)boxer.$(OBJ) \
	$(extract_out_prefix)buffer.$(OBJ) \
	$(extract_out_prefix)document.$(OBJ) \
	$(extract_out_prefix)docx.$(OBJ) \
	$(extract_out_prefix)docx_template.$(OBJ) \
	$(extract_out_prefix)extract.$(OBJ) \
	$(extract_out_prefix)html.$(OBJ) \
	$(extract_out_prefix)join.$(OBJ) \
	$(extract_out_prefix)json.$(OBJ) \
	$(extract_out_prefix)mem.$(OBJ) \
	$(extract_out_prefix)odt.$(OBJ) \
	$(extract_out_prefix)odt_template.$(OBJ) \
	$(extract_out_prefix)outf.$(OBJ) \
	$(extract_out_prefix)rect.$(OBJ) \
	$(extract_out_prefix)sys.$(OBJ) \
	$(extract_out_prefix)text.$(OBJ) \
	$(extract_out_prefix)xml.$(OBJ) \
	$(extract_out_prefix)zip.$(OBJ) \
