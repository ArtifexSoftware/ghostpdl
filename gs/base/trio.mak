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
TRIOGEN=$(TRIOGENDIR)$(D)
TRIOO_=$(O_)$(TRIOOBJ)

TRIOCFLAGS=$(CFLAGS) $(TRIO_CFLAGS) $(D_)TRIO_EMBED_STRING$(_D) $(D_)TRIO_FEATURE_CLOSURE=0$(_D) \
$(D_)TRIO_FEATURE_DYNAMICSTRING=0$(_D) $(D_)TRIO_MINIMAL=0$(_D) \
$(D_)TRIO_FEATURE_USER_DEFINED=0$(_D) $(D_)TRIO_EXTENSION=0$(_D)\
$(D_)TRIO_FUNC_TO_FLOAT$(_D) $(I_)$(TRIOSRCDIR)$(_I) \
$(D_)TRIO_MALLOC=no_malloc$(_D) $(D_)TRIO_REALLOC=no_realloc$(_D) $(D_)TRIO_FREE=no_free$(_D)


# NB: we can't use the normal $(CC_) here because msvccmd.mak
# adds /Za which conflicts with the trio source.
TRIOCC=$(CC) $(TRIOCFLAGS)

TRIOOBJS=$(TRIOOBJ)triostr.$(OBJ) $(TRIOOBJ)trio.$(OBJ) $(TRIOOBJ)trionan.$(OBJ)

triodef_h=$(TRIOSRC)triodef.h
trio_h=$(TRIOSRC)trio.h
triop_h=$(TRIOSRC)triop.h
triostr_h=$(TRIOSRC)triostr.h

TRIOHDRS=$(triodef_h) $(trio_h) $(triop_h) $(triostr_h)

$(TRIOOBJ)triostr.$(OBJ) : $(TRIOSRC)triostr.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)triostr.$(OBJ) $(C_) $(TRIOSRC)triostr.c

$(TRIOOBJ)trio.$(OBJ) : $(TRIOSRC)trio.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)trio.$(OBJ) $(C_) $(TRIOSRC)trio.c

$(TRIOOBJ)trionan.$(OBJ) : $(TRIOSRC)trionan.c $(TRIOHDRS) $(TRIO_MAK)
	$(TRIOCC) $(TRIOO_)trionan.$(OBJ) $(C_) $(TRIOSRC)trionan.c

# dev file for shared (separately built) lcms library
$(TRIOGEN)trio_1.dev : $(TOP_MAKEFILES) $(TRIO_MAK) $(ECHOGS_XE)
	$(SETMOD) $(TRIOGEN)trio_1 -lib trio

# dev file for compiling our own from source
$(TRIOGEN)trio_0.dev : $(TOP_MAKEFILES) $(TRIO_MAK) $(ECHOGS_XE) $(TRIOOBJS)
	$(SETMOD) $(TRIOGEN)trio_0 $(TRIOOBJS)

# switch in the version of lcms2.dev we're actually using
$(TRIOGEN)trio.dev : $(TOP_MAKEFILES) $(TRIOGEN)trio_$(SHARE_TRIO).dev
	$(CP_) $(TRIOGEN)trio_$(SHARE_TRIO).dev $(TRIOGEN)trio.dev
