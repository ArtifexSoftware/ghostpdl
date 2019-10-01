# Copyright (C) 2001-2019 Artifex Software, Inc.
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
#

# makefile for jbig2dec library code.
# Users of this makefile must define the following:
#	SHARE_JBIG2 - whether to compile in or link to the library
#	JBIG2SRCDIR - the library source directory
#
# gs.mak and friends define the following:
#	JBIG2OBJDIR - the output obj directory
#	JBIG2GENDIR - generated (.dev) file directory
#	JB2I_ JB2CF_ - include and cflags for compiling the lib

# We define the jbig2dec.dev target and its dependencies
#
# This partial makefile compiles the jbig2dec library for use in
# Ghostscript.

# Define the name of this makefile.
JBIG2_MAK=$(GLSRC)jbig2.mak $(TOP_MAKEFILES)

JBIG2SRC=$(JBIG2SRCDIR)$(D)
JBIG2GEN=$(JBIG2GENDIR)$(D)
JBIG2OBJ=$(JBIG2OBJDIR)$(D)

# This makefile is only known to work with jbig2dec v0.7 and later
# to use an earlier version, remove unknown files from
# the OBJS lists below

libjbig2_OBJS1=\
	$(JBIG2OBJ)jbig2.$(OBJ) \
	$(JBIG2OBJ)jbig2_arith.$(OBJ) \
	$(JBIG2OBJ)jbig2_arith_iaid.$(OBJ) \
	$(JBIG2OBJ)jbig2_arith_int.$(OBJ) \
	$(JBIG2OBJ)jbig2_generic.$(OBJ) \
	$(JBIG2OBJ)jbig2_refinement.$(OBJ) \
	$(JBIG2OBJ)jbig2_huffman.$(OBJ) \
	$(JBIG2OBJ)jbig2_hufftab.$(OBJ) \
	$(JBIG2OBJ)jbig2_image.$(OBJ) \
	$(JBIG2OBJ)jbig2_mmr.$(OBJ)

libjbig2_OBJS2=\
	$(JBIG2OBJ)jbig2_page.$(OBJ) \
	$(JBIG2OBJ)jbig2_segment.$(OBJ) \
	$(JBIG2OBJ)jbig2_symbol_dict.$(OBJ) \
	$(JBIG2OBJ)jbig2_text.$(OBJ) \
	$(JBIG2OBJ)jbig2_halftone.$(OBJ) \
	$(JBIG2_EXTRA_OBJS)

libjbig2_OBJS=$(libjbig2_OBJS1) $(libjbig2_OBJS2)

libjbig2_HDRS=\
	$(JBIG2SRC)jbig2.h \
	$(JBIG2SRC)jbig2_arith.h \
	$(JBIG2SRC)jbig2_arith_iaid.h \
	$(JBIG2SRC)jbig2_arith_int.h \
	$(JBIG2SRC)jbig2_generic.h \
	$(JBIG2SRC)jbig2_huffman.h \
	$(JBIG2SRC)jbig2_hufftab.h \
	$(JBIG2SRC)jbig2_image.h \
	$(JBIG2SRC)jbig2_mmr.h \
	$(JBIG2SRC)jbig2_priv.h \
	$(JBIG2SRC)jbig2_symbol_dict.h \
	$(JBIG2SRC)jbig2_text.h \
	$(JBIG2SRC)jbig2_halftone.h \
	$(JBIG2SRC)config_win32.h

jbig2dec_OBJS=$(JBIG2OBJ)getopt.$(OBJ) $(JBIG2OBJ)getopt1.$(OBJ) $(JBIG2OBJ)sha1.$(OBJ)
jbig2dec_HDRS=$(JBIG2OBJ)getopt.h $(JBIG2OBJ)sha1.h

jbig2.clean : jbig2.config-clean jbig2.clean-not-config-clean

### WRONG.  MUST DELETE OBJ AND GEN FILES SELECTIVELY.
jbig2.clean-not-config-clean :
	$(EXP)$(ECHOGS_XE) $(JBIG2SRCDIR) $(JBIG2OBJDIR)
	$(RM_) $(JBIG2OBJDIR)$(D)*..$(OBJ)

jbig2.config-clean :
	$(RMN_) $(JBIG2GEN)$(D)jbig2*.dev

JBIG2DEP=$(AK)

JBIG2_CC=$(CC_) $(CFLAGS) $(I_)$(JBIG2GENDIR) $(II)$(JB2I_)$(_I) $(JB2CF_) -DJBIG_EXTERNAL_MEMENTO_H=\"../base/memento.h\"
JBIG2O_=$(O_)$(JBIG2OBJ)

# switch in the version of libjbig2.dev we're actually using
$(JBIG2GEN)jbig2dec.dev : $(JBIG2GEN)jbig2dec_$(SHARE_JBIG2).dev $(JBIG2_MAK) $(MAKEDIRS)
	$(CP_) $(JBIG2GEN)jbig2dec_$(SHARE_JBIG2).dev $(JBIG2GEN)jbig2dec.dev

# dev file for shared (separately built) jbig2dec library
$(JBIG2GEN)jbig2dec_1.dev : $(JBIG2_MAK) $(ECHOGS_XE) $(JBIG2_MAK) $(MAKEDIRS)
	$(SETMOD) $(JBIG2GEN)jbig2dec_1 -lib jbig2dec

# dev file for compiling our own from source
$(JBIG2GEN)jbig2dec_0.dev : $(JBIG2_MAK) $(ECHOGS_XE) $(libjbig2_OBJS) $(JBIG2_MAK) $(MAKEDIRS)
	$(SETMOD) $(JBIG2GEN)jbig2dec_0 $(libjbig2_OBJS1)
	$(ADDMOD) $(JBIG2GEN)jbig2dec_0 $(libjbig2_OBJS2)

# explicit rules for building the source files. 

$(JBIG2OBJ)snprintf.$(OBJ) : $(JBIG2SRC)snprintf.c $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)snprintf.$(OBJ) $(C_) $(JBIG2SRC)snprintf.c

$(JBIG2OBJ)getopt.$(OBJ) : $(JBIG2SRC)getopt.c $(JBIG2SRC)getopt.h $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)getopt.$(OBJ) $(C_) $(JBIG2SRC)getopt.c

$(JBIG2OBJ)getopt1.$(OBJ) : $(JBIG2SRC)getopt1.c $(JBIG2SRC)getopt.h $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)getopt1.$(OBJ) $(C_) $(JBIG2SRC)getopt1.c

$(JBIG2OBJ)jbig2.$(OBJ) : $(JBIG2SRC)jbig2.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2.$(OBJ) $(C_) $(JBIG2SRC)jbig2.c

$(JBIG2OBJ)jbig2_arith.$(OBJ) : $(JBIG2SRC)jbig2_arith.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_arith.$(OBJ) $(C_) $(JBIG2SRC)jbig2_arith.c

$(JBIG2OBJ)jbig2_arith_iaid.$(OBJ) : $(JBIG2SRC)jbig2_arith_iaid.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_arith_iaid.$(OBJ) $(C_) $(JBIG2SRC)jbig2_arith_iaid.c

$(JBIG2OBJ)jbig2_arith_int.$(OBJ) : $(JBIG2SRC)jbig2_arith_int.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_arith_int.$(OBJ) $(C_) $(JBIG2SRC)jbig2_arith_int.c

$(JBIG2OBJ)jbig2_generic.$(OBJ) : $(JBIG2SRC)jbig2_generic.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_generic.$(OBJ) $(C_) $(JBIG2SRC)jbig2_generic.c

$(JBIG2OBJ)jbig2_refinement.$(OBJ) : $(JBIG2SRC)jbig2_refinement.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_refinement.$(OBJ) $(C_) $(JBIG2SRC)jbig2_refinement.c
 
$(JBIG2OBJ)jbig2_huffman.$(OBJ) : $(JBIG2SRC)jbig2_huffman.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_huffman.$(OBJ) $(C_) $(JBIG2SRC)jbig2_huffman.c

$(JBIG2OBJ)jbig2_hufftab.$(OBJ) : $(JBIG2SRC)jbig2_hufftab.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_hufftab.$(OBJ) $(C_) $(JBIG2SRC)jbig2_hufftab.c

$(JBIG2OBJ)jbig2_image.$(OBJ) : $(JBIG2SRC)jbig2_image.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_image.$(OBJ) $(C_) $(JBIG2SRC)jbig2_image.c

$(JBIG2OBJ)jbig2_image_pbm.$(OBJ) : $(JBIG2SRC)jbig2_image_pbm.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_image_pbm.$(OBJ) $(C_) $(JBIG2SRC)jbig2_image_pbm.c

$(JBIG2OBJ)jbig2_image_png.$(OBJ) : $(JBIG2SRC)jbig2_image_png.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_image_png.$(OBJ) $(C_) $(JBIG2SRC)jbig2_image_png.c

$(JBIG2OBJ)jbig2_mmr.$(OBJ) : $(JBIG2SRC)jbig2_mmr.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_mmr.$(OBJ) $(C_) $(JBIG2SRC)jbig2_mmr.c

$(JBIG2OBJ)jbig2_page.$(OBJ) : $(JBIG2SRC)jbig2_page.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_page.$(OBJ) $(C_) $(JBIG2SRC)jbig2_page.c

$(JBIG2OBJ)jbig2_segment.$(OBJ) : $(JBIG2SRC)jbig2_segment.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_segment.$(OBJ) $(C_) $(JBIG2SRC)jbig2_segment.c

$(JBIG2OBJ)jbig2_symbol_dict.$(OBJ) : $(JBIG2SRC)jbig2_symbol_dict.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_symbol_dict.$(OBJ) $(C_) $(JBIG2SRC)jbig2_symbol_dict.c

$(JBIG2OBJ)jbig2_text.$(OBJ) : $(JBIG2SRC)jbig2_text.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_text.$(OBJ) $(C_) $(JBIG2SRC)jbig2_text.c

$(JBIG2OBJ)jbig2_halftone.$(OBJ) : $(JBIG2SRC)jbig2_halftone.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2_halftone.$(OBJ) $(C_) $(JBIG2SRC)jbig2_halftone.c

$(JBIG2OBJ)jbig2dec.$(OBJ) : $(JBIG2SRC)jbig2dec.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)jbig2dec.$(OBJ) $(C_) $(JBIG2SRC)jbig2dec.c

$(JBIG2OBJ)sha1.$(OBJ) : $(JBIG2SRC)sha1.c $(libjbig2_HDRS) $(JBIG2DEP) $(JBIG2_MAK) $(MAKEDIRS)
	$(JBIG2_CC) $(JBIG2O_)sha1.$(OBJ) $(C_) $(JBIG2SRC)sha1.c

