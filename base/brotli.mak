# Copyright (C) 2025 Artifex Software, Inc.
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
# makefile for brotli library code.
# Users of this makefile must define the following:
#	GSSRCDIR - the GS library source directory
#	BROTLISRCDIR - the source directory
#	BROTLIGENDIR - the generated intermediate file directory
#	BROTLIOBJDIR - the object directory
#	SHARE_BROTLI - 0 to compile brotli, 1 to share
#	BROTLI_NAME - if SHARE_BROTLI=1, the name of the shared library
#	BROTLIAUXDIR - the directory for auxiliary objects

# This partial makefile compiles the brotli library for use in Ghostscript.
# You can get the source code for this library from:
#   https://github.com/google/brotli
#
# This makefile is known to work with brotli source version 1.1.0.

BROTLISRC=$(BROTLISRCDIR)$(D)
BROTLIGEN=$(BROTLIGENDIR)$(D)
BROTLIOBJ=$(BROTLIOBJDIR)$(D)
BROTLIAUX=$(BROTLIAUXDIR)$(D)
BROTLIO_=$(O_)$(BROTLIOBJ)
BROTLIAO_=$(O_)$(BROTLIAUX)

# We need D_, _D_, and _D because the OpenVMS compiler uses different
# syntax from other compilers.
# BROTLII_ and BROTLIF_ are defined in gs.mak.
BROTLICCFLAGS=$(BROTLI_CFLAGS) $(I_)$(BROTLII_)$(_I) $(BROTLIF_) $(D_)verbose$(_D_)-1$(_D)
BROTLICC=$(CC) $(BROTLICCFLAGS) $(CCFLAGS)

BROTLICCAUXFLAGS=$(BROTLI_CFLAGS) $(I_)$(BROTLII_)$(_I) $(BROTLIF_) $(D_)verbose$(_D_)-1$(_D)
BROTLICCAUX=$(CCAUX) $(BROTLICCAUXFLAGS) $(CCFLAGSAUX)

# Define the name of this makefile.
BROTLI_MAK=$(GLSRC)brotli.mak $(TOP_MAKEFILES)

brotli.clean : brotli.config-clean brotli.clean-not-config-clean

### WRONG.  MUST DELETE OBJ AND GEN FILES SELECTIVELY.
brotli.clean-not-config-clean :
	$(RM_) $(BROTLIOBJ)*.$(OBJ)

brotli.config-clean :
	$(RMN_) $(BROTLIGEN)brotli.dev

BROTLIDEP=$(AK) $(BROTLI_MAK) $(MAKEDIRS)

# Code common to compression and decompression.

brotlic_= \
	  $(BROTLIOBJ)constants.$(OBJ) \
	  $(BROTLIOBJ)context.$(OBJ) \
	  $(BROTLIOBJ)dictionary.$(OBJ) \
	  $(BROTLIOBJ)platform.$(OBJ) \
	  $(BROTLIOBJ)shared_dictionary.$(OBJ) \
	  $(BROTLIOBJ)transform.$(OBJ)

$(BROTLIGEN)brotlic.dev : $(BROTLI_MAK) $(ECHOGS_XE) $(brotlic_)
	$(SETMOD) $(BROTLIGEN)brotlic $(brotlic_)

$(BROTLIOBJ)constants.$(OBJ) : $(BROTLISRC)c$(D)common$(D)constants.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)constants.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)constants.c

$(BROTLIOBJ)context.$(OBJ) : $(BROTLISRC)c$(D)common$(D)context.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)context.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)context.c

$(BROTLIOBJ)dictionary.$(OBJ) : $(BROTLISRC)c$(D)common$(D)dictionary.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)dictionary.c

$(BROTLIOBJ)platform.$(OBJ) : $(BROTLISRC)c$(D)common$(D)platform.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)platform.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)platform.c

$(BROTLIOBJ)shared_dictionary.$(OBJ) : $(BROTLISRC)c$(D)common$(D)shared_dictionary.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)shared_dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)shared_dictionary.c

$(BROTLIOBJ)transform.$(OBJ) : $(BROTLISRC)c$(D)common$(D)transform.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)transform.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)transform.c

# Encoding (compression) code.

$(BROTLIGEN)brotlie.dev : $(BROTLI_MAK) $(BROTLIGEN)brotlie_$(SHARE_BROTLI).dev \
 $(MAKEDIRS)
	$(CP_) $(BROTLIGEN)brotlie_$(SHARE_BROTLI).dev $(BROTLIGEN)brotlie.dev

$(BROTLIGEN)brotli_1.dev : $(BROTLI_MAK) $(BROTLI_MAK) $(ECHOGS_XE) \
 $(MAKEDIRS)
	$(SETMOD) $(BROTLIGEN)brotlie_1 -lib $(BROTLI_NAME)

brotlie_= \
	  $(BROTLIOBJ)backward_references.$(OBJ) \
	  $(BROTLIOBJ)backward_references_hq.$(OBJ) \
	  $(BROTLIOBJ)bit_cost.$(OBJ) \
	  $(BROTLIOBJ)block_splitter.$(OBJ) \
	  $(BROTLIOBJ)brotli_bit_stream.$(OBJ) \
	  $(BROTLIOBJ)cluster.$(OBJ) \
	  $(BROTLIOBJ)command.$(OBJ) \
	  $(BROTLIOBJ)compound_dictionary.$(OBJ) \
	  $(BROTLIOBJ)compress_fragment.$(OBJ) \
	  $(BROTLIOBJ)compress_fragment_two_pass.$(OBJ) \
	  $(BROTLIOBJ)dictionary_hash.$(OBJ) \
	  $(BROTLIOBJ)br_encode.$(OBJ) \
	  $(BROTLIOBJ)encoder_dict.$(OBJ) \
	  $(BROTLIOBJ)entropy_encode.$(OBJ) \
	  $(BROTLIOBJ)fast_log.$(OBJ) \
	  $(BROTLIOBJ)histogram.$(OBJ) \
	  $(BROTLIOBJ)literal_cost.$(OBJ) \
	  $(BROTLIOBJ)memory.$(OBJ) \
	  $(BROTLIOBJ)metablock.$(OBJ) \
	  $(BROTLIOBJ)static_dict.$(OBJ) \
	  $(BROTLIOBJ)utf8_util.$(OBJ)
$(BROTLIGEN)brotlie_0.dev : $(BROTLI_MAK) $(ECHOGS_XE) $(BROTLIGEN)brotlic.dev $(brotlie_) \
 $(MAKEDIRS)
	$(SETMOD) $(BROTLIGEN)brotlie_0 $(brotlie_)
	$(ADDMOD) $(BROTLIGEN)brotlie_0 -include $(BROTLIGEN)brotlic.dev

$(BROTLIOBJ)backward_references.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)backward_references.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)backward_references.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)backward_references.c

$(BROTLIOBJ)backward_references_hq.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)backward_references_hq.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)backward_references_hq.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)backward_references_hq.c

$(BROTLIOBJ)bit_cost.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)bit_cost.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)bit_cost.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)bit_cost.c

$(BROTLIOBJ)block_splitter.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)block_splitter.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)block_splitter.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)block_splitter.c

$(BROTLIOBJ)brotli_bit_stream.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)brotli_bit_stream.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)brotli_bit_stream.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)brotli_bit_stream.c

$(BROTLIOBJ)cluster.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)cluster.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)cluster.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)cluster.c

$(BROTLIOBJ)command.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)command.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)command.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)command.c

$(BROTLIOBJ)compound_dictionary.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compound_dictionary.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)compound_dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compound_dictionary.c

$(BROTLIOBJ)compress_fragment.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compress_fragment.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)compress_fragment.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compress_fragment.c

$(BROTLIOBJ)compress_fragment_two_pass.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compress_fragment_two_pass.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)compress_fragment_two_pass.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compress_fragment_two_pass.c

$(BROTLIOBJ)dictionary_hash.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)dictionary_hash.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)dictionary_hash.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)dictionary_hash.c

$(BROTLIOBJ)br_encode.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)encode.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)br_encode.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)encode.c

$(BROTLIOBJ)encoder_dict.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)encoder_dict.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)encoder_dict.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)encoder_dict.c

$(BROTLIOBJ)entropy_encode.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)entropy_encode.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)entropy_encode.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)entropy_encode.c

$(BROTLIOBJ)fast_log.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)fast_log.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)fast_log.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)fast_log.c

$(BROTLIOBJ)histogram.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)histogram.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)histogram.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)histogram.c

$(BROTLIOBJ)literal_cost.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)literal_cost.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)literal_cost.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)literal_cost.c

$(BROTLIOBJ)memory.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)memory.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)memory.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)memory.c

$(BROTLIOBJ)metablock.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)metablock.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)metablock.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)metablock.c

$(BROTLIOBJ)static_dict.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)static_dict.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)static_dict.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)static_dict.c

$(BROTLIOBJ)utf8_util.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)utf8_util.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)utf8_util.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)utf8_util.c

# Decoding (decompression) code.

$(BROTLIGEN)brotlid.dev : $(BROTLI_MAK) $(BROTLIGEN)brotlid_$(SHARE_BROTLI).dev $(MAKEDIRS)
	$(CP_) $(BROTLIGEN)brotlid_$(SHARE_BROTLI).dev $(BROTLIGEN)brotlid.dev

brotlid_= \
	  $(BROTLIOBJ)bit_reader.$(OBJ) \
	  $(BROTLIOBJ)decode.$(OBJ) \
	  $(BROTLIOBJ)huffman.$(OBJ) \
	  $(BROTLIOBJ)state.$(OBJ)

$(BROTLIGEN)brotlid_0.dev : $(BROTLI_MAK) $(ECHOGS_XE) $(BROTLIGEN)brotlic.dev $(brotlid_) $(MAKEDIRS)
	$(SETMOD) $(BROTLIGEN)brotlid_0 $(brotlid_)
	$(ADDMOD) $(BROTLIGEN)brotlid_0 -include $(BROTLIGEN)brotlic.dev

$(BROTLIOBJ)bit_reader.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)bit_reader.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)bit_reader.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)bit_reader.c

$(BROTLIOBJ)decode.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)decode.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)decode.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)decode.c

$(BROTLIOBJ)huffman.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)huffman.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)huffman.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)huffman.c

$(BROTLIOBJ)state.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)state.c $(BROTLIDEP)
	$(BROTLICC) $(BROTLIO_)state.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)state.c

# Auxiliary objects

$(BROTLIAUX)constants.$(OBJ) : $(BROTLISRC)c$(D)common$(D)constants.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)constants.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)constants.c

$(BROTLIAUX)context.$(OBJ) : $(BROTLISRC)c$(D)common$(D)context.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)context.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)context.c

$(BROTLIAUX)dictionary.$(OBJ) : $(BROTLISRC)c$(D)common$(D)dictionary.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)dictionary.c

$(BROTLIAUX)platform.$(OBJ) : $(BROTLISRC)c$(D)common$(D)platform.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)platform.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)platform.c

$(BROTLIAUX)shared_dictionary.$(OBJ) : $(BROTLISRC)c$(D)common$(D)shared_dictionary.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)shared_dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)shared_dictionary.c

$(BROTLIAUX)transform.$(OBJ) : $(BROTLISRC)c$(D)common$(D)transform.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)transform.$(OBJ) $(C_) $(BROTLISRC)c$(D)common$(D)transform.c

$(BROTLIAUX)backward_references.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)backward_references.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)backward_references.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)backward_references.c

$(BROTLIAUX)backward_references_hq.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)backward_references_hq.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)backward_references_hq.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)backward_references_hq.c

$(BROTLIAUX)bit_cost.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)bit_cost.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)bit_cost.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)bit_cost.c

$(BROTLIAUX)block_splitter.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)block_splitter.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)block_splitter.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)block_splitter.c

$(BROTLIAUX)brotli_bit_stream.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)brotli_bit_stream.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)brotli_bit_stream.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)brotli_bit_stream.c

$(BROTLIAUX)cluster.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)cluster.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)cluster.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)cluster.c

$(BROTLIAUX)command.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)command.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)command.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)command.c

$(BROTLIAUX)compound_dictionary.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compound_dictionary.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)compound_dictionary.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compound_dictionary.c

$(BROTLIAUX)compress_fragment.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compress_fragment.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)compress_fragment.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compress_fragment.c

$(BROTLIAUX)compress_fragment_two_pass.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)compress_fragment_two_pass.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)compress_fragment_two_pass.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)compress_fragment_two_pass.c

$(BROTLIAUX)dictionary_hash.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)dictionary_hash.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)dictionary_hash.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)dictionary_hash.c

$(BROTLIAUX)br_encode.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)encode.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)br_encode.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)encode.c

$(BROTLIAUX)encoder_dict.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)encoder_dict.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)encoder_dict.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)encoder_dict.c

$(BROTLIAUX)entropy_encode.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)entropy_encode.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)entropy_encode.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)entropy_encode.c

$(BROTLIAUX)fast_log.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)fast_log.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)fast_log.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)fast_log.c

$(BROTLIAUX)histogram.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)histogram.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)histogram.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)histogram.c

$(BROTLIAUX)literal_cost.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)literal_cost.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)literal_cost.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)literal_cost.c

$(BROTLIAUX)memory.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)memory.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)memory.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)memory.c

$(BROTLIAUX)metablock.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)metablock.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)metablock.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)metablock.c

$(BROTLIAUX)static_dict.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)static_dict.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)static_dict.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)static_dict.c

$(BROTLIAUX)utf8_util.$(OBJ) : $(BROTLISRC)c$(D)enc$(D)utf8_util.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)utf8_util.$(OBJ) $(C_) $(BROTLISRC)c$(D)enc$(D)utf8_util.c

# Decoding (decompression) code.

$(BROTLIAUX)bit_reader.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)bit_reader.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)bit_reader.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)bit_reader.c

$(BROTLIAUX)decode.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)decode.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)decode.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)decode.c

$(BROTLIAUX)huffman.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)huffman.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)huffman.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)huffman.c

$(BROTLIAUX)state.$(OBJ) : $(BROTLISRC)c$(D)dec$(D)state.c $(BROTLIDEP)
	$(BROTLICCAUX) $(BROTLIAO_)state.$(OBJ) $(C_) $(BROTLISRC)c$(D)dec$(D)state.c
