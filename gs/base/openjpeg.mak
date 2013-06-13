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

# makefile for Luratech lwf_jp2 library code.
# Users of this makefile must define the following:
#       SHARE_JPX - whether to compile in or link to the library
#       JPXSRCDIR - the library source directory
#
# gs.mak and friends define the following:
#       JPXOBJDIR - the output obj directory
#       JPXGENDIR - generated (.dev) file directory
#	JPX_OPENJPEG_I_ - include path for the library headers
#       JPXCF_ - cflags for building the library
#
# We define the openjpeg.dev target and its dependencies
#
# This partial makefile compiles the openjpeg library for use in
# Ghostscript.

OPEN_JPEG_MAK=$(GLSRC)openjpeg.mak

OPEN_JPEG_SRC=$(JPXSRCDIR)$(D)libopenjpeg$(D)
OPEN_JPEG_GEN=$(JPXOBJDIR)$(D)

OPEN_JPEG_PREFIX=opj_
OPEN_JPEG_OBJ=$(JPXOBJDIR)$(D)


# source files to build from the CSDK source

open_jpeg_OBJS = \
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)bio.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cidx_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cio.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)dwt.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)event.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)function_list.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)image.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)invert.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)j2k.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)jp2.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mct.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mqc.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)openjpeg.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)opj_clock.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)phix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)pi.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)ppix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)raw.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t1.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t2.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tcd.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tgt.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tpix_manager.$(OBJ)			\

open_jpeg_HDRS = \
	$(OPEN_JPEG_SRC)bio.h		\
	$(OPEN_JPEG_SRC)cidx_manager.h	\
	$(OPEN_JPEG_SRC)cio.h		\
	$(OPEN_JPEG_SRC)dwt.h		\
	$(OPEN_JPEG_SRC)event.h			\
	$(OPEN_JPEG_SRC)function_list.h			\
	$(OPEN_JPEG_SRC)image.h			\
	$(OPEN_JPEG_SRC)indexbox_manager.h	\
	$(OPEN_JPEG_SRC)invert.h			\
	$(OPEN_JPEG_SRC)j2k.h		\
	$(OPEN_JPEG_SRC)jp2.h			\
	$(OPEN_JPEG_SRC)mct.h		\
	$(OPEN_JPEG_SRC)mqc.h		\
	$(OPEN_JPEG_SRC)openjpeg.h		\
	$(OPEN_JPEG_SRC)opj_clock.h			\
	$(OPEN_JPEG_SRC)opj_config.h			\
	$(OPEN_JPEG_SRC)opj_config_private.h			\
	$(OPEN_JPEG_SRC)opj_includes.h		\
	$(OPEN_JPEG_SRC)opj_intmath.h			\
	$(OPEN_JPEG_SRC)opj_inttypes.h			\
	$(OPEN_JPEG_SRC)opj_malloc.h			\
	$(OPEN_JPEG_SRC)opj_stdint.h			\
	$(OPEN_JPEG_SRC)pi.h		\
	$(OPEN_JPEG_SRC)raw.h		\
	$(OPEN_JPEG_SRC)t1.h		\
	$(OPEN_JPEG_SRC)t1_luts.h	\
	$(OPEN_JPEG_SRC)t2.h	\
	$(OPEN_JPEG_SRC)tcd.h		\
	$(OPEN_JPEG_SRC)tgt.h	\

# switch in the selected library .dev
$(OPEN_JPEG_GEN)openjpeg.dev : $(TOP_MAKEFILES) $(OPEN_JPEG_GEN)openjpeg_$(SHARE_JPX).dev
	$(CP_) $(OPEN_JPEG_GEN)openjpeg_$(SHARE_JPX).dev $(OPEN_JPEG_GEN)openjpeg.dev

# external link .dev
$(OPEN_JPEG_GEN)openjpeg_1.dev : $(TOP_MAKEFILES) $(OPEN_JPEG_MAK) $(ECHOGS_XE)
	$(SETMOD) $(OPEN_JPEG_GEN)openjpeg_1 -lib lib_openjpeg

# compile our own .dev
$(OPEN_JPEG_GEN)openjpeg_0.dev : $(TOP_MAKEFILES) $(OPEN_JPEG_MAK) $(ECHOGS_XE) $(open_jpeg_OBJS)
	$(SETMOD) $(OPEN_JPEG_GEN)openjpeg_0 $(open_jpeg_OBJS)

# define our specific compiler
OPEN_JPEG_CC=$(CC) $(CFLAGS) $(D_)OPJ_STATIC$(_D) $(I_)$(OPEN_JPEG_GEN)$(_I) $(I_)$(JPX_OPENJPEG_I_)$(_I) $(I_)$(JPX_OPENJPEG_I_)$(D)..$(_I) $(JPXCF_)
OPEN_JPEG_O=$(O_)$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)

OPEN_JPEG_DEP=$(AK) $(OPEN_JPEG_MAK)

# explicit rules for building each source file
# for simplicity we have every source file depend on all headers

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)bio.$(OBJ) : $(OPEN_JPEG_SRC)bio.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)bio.$(OBJ) $(C_) $(OPEN_JPEG_SRC)bio.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cio.$(OBJ) : $(OPEN_JPEG_SRC)cio.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)cio.$(OBJ) $(C_) $(OPEN_JPEG_SRC)cio.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)dwt.$(OBJ) : $(OPEN_JPEG_SRC)dwt.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)dwt.$(OBJ) $(C_) $(OPEN_JPEG_SRC)dwt.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)event.$(OBJ) : $(OPEN_JPEG_SRC)event.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)event.$(OBJ) $(C_) $(OPEN_JPEG_SRC)event.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)function_list.$(OBJ) : $(OPEN_JPEG_SRC)function_list.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)function_list.$(OBJ) $(C_) $(OPEN_JPEG_SRC)function_list.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)image.$(OBJ) : $(OPEN_JPEG_SRC)image.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)image.$(OBJ) $(C_) $(OPEN_JPEG_SRC)image.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)invert.$(OBJ) : $(OPEN_JPEG_SRC)invert.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)invert.$(OBJ) $(C_) $(OPEN_JPEG_SRC)invert.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)j2k.$(OBJ) : $(OPEN_JPEG_SRC)j2k.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)j2k.$(OBJ) $(C_) $(OPEN_JPEG_SRC)j2k.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)jp2.$(OBJ) : $(OPEN_JPEG_SRC)jp2.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)jp2.$(OBJ) $(C_) $(OPEN_JPEG_SRC)jp2.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mct.$(OBJ) : $(OPEN_JPEG_SRC)mct.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)mct.$(OBJ) $(C_) $(OPEN_JPEG_SRC)mct.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mqc.$(OBJ) : $(OPEN_JPEG_SRC)mqc.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)mqc.$(OBJ) $(C_) $(OPEN_JPEG_SRC)mqc.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)openjpeg.$(OBJ) : $(OPEN_JPEG_SRC)openjpeg.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)openjpeg.$(OBJ) $(C_) $(OPEN_JPEG_SRC)openjpeg.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)opj_clock.$(OBJ) : $(OPEN_JPEG_SRC)opj_clock.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)opj_clock.$(OBJ) $(C_) $(OPEN_JPEG_SRC)opj_clock.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)pi.$(OBJ) : $(OPEN_JPEG_SRC)pi.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)pi.$(OBJ) $(C_) $(OPEN_JPEG_SRC)pi.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)raw.$(OBJ) : $(OPEN_JPEG_SRC)raw.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)raw.$(OBJ) $(C_) $(OPEN_JPEG_SRC)raw.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t1.$(OBJ) : $(OPEN_JPEG_SRC)t1.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)t1.$(OBJ) $(C_) $(OPEN_JPEG_SRC)t1.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t2.$(OBJ) : $(OPEN_JPEG_SRC)t2.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)t2.$(OBJ) $(C_) $(OPEN_JPEG_SRC)t2.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tcd.$(OBJ) : $(OPEN_JPEG_SRC)tcd.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tcd.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tcd.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tgt.$(OBJ) : $(OPEN_JPEG_SRC)tgt.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tgt.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tgt.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cidx_manager.$(OBJ) : $(OPEN_JPEG_SRC)cidx_manager.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)cidx_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)cidx_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tpix_manager.$(OBJ) : $(OPEN_JPEG_SRC)tpix_manager.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tpix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tpix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thix_manager.$(OBJ) : $(OPEN_JPEG_SRC)thix_manager.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)thix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)thix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)ppix_manager.$(OBJ) : $(OPEN_JPEG_SRC)ppix_manager.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)ppix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)ppix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)phix_manager.$(OBJ) : $(OPEN_JPEG_SRC)phix_manager.c $(OPEN_JPEG_DEP) $(open_jpeg_HDRS)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)phix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)phix_manager.c

# end of file
