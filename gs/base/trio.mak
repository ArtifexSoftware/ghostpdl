# Copyright (C) 2001-2012 Artifex Software, Inc.
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
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# makefile for trio - locale-less s(n)printf/s(n)canf
#
# Users of this makefile must define the following:
#	TRIO_CFLAGS - Compiler flags for building the source,
#	TRIOSRCDIR - the expat source top-level directory,
#	TIOOBJDIR - directory for object files.

# Define the name of this makefile
TRIO_MAK=$(GLSRCDIR)$(D)trio.mak

# local aliases
TRIOSRC=$(TRIOSRCDIR)$(D)
TRIOOBJ=$(TRIOOBJDIR)$(D)
TRIOO_=$(O_)$(TRIOOBJ)

# NB: we can't use the normal $(CC_) here because msvccmd.mak
# adds /Za which conflicts with the lcms source.
TRIOCC=$(CC) $(CFLAGS) $(D_)TRIO_EMBED_STRING$(_D) $(D_)TRIO_FUNC_TO_FLOAT$(_D) \
$(D_)TRIO_FUNC_TO_LONG$$(_D) $(D_)TRIO_FUNC_TO_LOWER$(_D) $(I_)$(TRIOSRCDIR)$(_I) $(TRIO_CFLAGS)


TRIOOBJS=$(TRIOOBJ)triostr.$(OBJ) $(TRIOOBJ)trio.$(OBJ) $(TRIOOBJ)trionan.$(OBJ)
TRIOHDRS=$(TRIOSRC)trio.h $(TRIOSRC)triop.h $(TRIOSRC)triodef.h $(TRIOSRC)trionan.h $(TRIOSRC)triostr.h

$(TRIOOBJ)triostr.$(OBJ) : $(TRIOSRC)triostr.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)triostr.$(OBJ) $(C_) $(TRIOSRC)triostr.c

$(TRIOOBJ)trio.$(OBJ) : $(TRIOSRC)trio.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)trio.$(OBJ) $(C_) $(TRIOSRC)trio.c

$(TRIOOBJ)trionan.$(OBJ) : $(TRIOSRC)trionan.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)trionan.$(OBJ) $(C_) $(TRIOSRC)trionan.c
