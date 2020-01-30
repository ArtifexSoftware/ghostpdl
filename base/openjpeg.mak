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

OPEN_JPEG_MAK=$(GLSRC)openjpeg.mak $(TOP_MAKEFILES)

OPEN_JPEG_SRC=$(JPXSRCDIR)$(D)src$(D)lib$(D)openjp2$(D)
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
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)phix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)pi.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)ppix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t1.$(OBJ)		\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t2.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tcd.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tgt.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tpix_manager.$(OBJ)			\
	$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thread.$(OBJ)                \
        $(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)sparse_array.$(OBJ) 

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
	$(OPEN_JPEG_SRC)t1.h		\
	$(OPEN_JPEG_SRC)t1_luts.h	\
	$(OPEN_JPEG_SRC)t2.h	\
	$(OPEN_JPEG_SRC)tcd.h		\
	$(OPEN_JPEG_SRC)tgt.h	\
	$(OPEN_JPEG_SRC)thread.h	\
	$(OPEN_JPEG_SRC)tls_keys.h	\
        $(OPEN_JPEG_SRC)sparse_array.h

# switch in the selected library .dev
$(OPEN_JPEG_GEN)openjpeg.dev : $(OPEN_JPEG_GEN)openjpeg_$(SHARE_JPX).dev \
 $(OPEN_JPEG_MAK) $(MAKEDIRS)
	$(CP_) $(OPEN_JPEG_GEN)openjpeg_$(SHARE_JPX).dev $(OPEN_JPEG_GEN)openjpeg.dev

# external link .dev - empty, as we add the lib to LDFLAGS
$(OPEN_JPEG_GEN)openjpeg_1.dev : $(OPEN_JPEG_MAK) $(ECHOGS_XE) \
 $(OPEN_JPEG_MAK) $(MAKEDIRS)
	$(SETMOD) $(OPEN_JPEG_GEN)openjpeg_1

# compile our own .dev
$(OPEN_JPEG_GEN)openjpeg_0.dev : $(ECHOGS_XE) $(open_jpeg_OBJS) \
 $(OPEN_JPEG_MAK) $(MAKEDIRS)
	$(SETMOD) $(OPEN_JPEG_GEN)openjpeg_0 $(open_jpeg_OBJS)

# define our specific compiler
OPEN_JPEG_CC=$(CC) $(CFLAGS) $(D_)OPJ_STATIC$(_D) $(I_)$(OPEN_JPEG_GEN)$(_I) $(I_)$(JPX_OPENJPEG_I_)$(_I) $(I_)$(JPX_OPENJPEG_I_)$(D)..$(_I) $(JPXCF_)
OPEN_JPEG_O=$(O_)$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)

OPEN_JPEG_DEP=$(AK) $(OPEN_JPEG_MAK) $(MAKEDIRS)

# explicit rules for building each source file
# for simplicity we have every source file depend on all headers

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)bio.$(OBJ) : $(OPEN_JPEG_SRC)bio.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)bio.$(OBJ) $(C_) $(OPEN_JPEG_SRC)bio.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cio.$(OBJ) : $(OPEN_JPEG_SRC)cio.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)cio.$(OBJ) $(C_) $(OPEN_JPEG_SRC)cio.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)dwt.$(OBJ) : $(OPEN_JPEG_SRC)dwt.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)dwt.$(OBJ) $(C_) $(OPEN_JPEG_SRC)dwt.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)event.$(OBJ) : $(OPEN_JPEG_SRC)event.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)event.$(OBJ) $(C_) $(OPEN_JPEG_SRC)event.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)function_list.$(OBJ) : $(OPEN_JPEG_SRC)function_list.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)function_list.$(OBJ) $(C_) $(OPEN_JPEG_SRC)function_list.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)image.$(OBJ) : $(OPEN_JPEG_SRC)image.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)image.$(OBJ) $(C_) $(OPEN_JPEG_SRC)image.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)invert.$(OBJ) : $(OPEN_JPEG_SRC)invert.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)invert.$(OBJ) $(C_) $(OPEN_JPEG_SRC)invert.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)j2k.$(OBJ) : $(OPEN_JPEG_SRC)j2k.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)j2k.$(OBJ) $(C_) $(OPEN_JPEG_SRC)j2k.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)jp2.$(OBJ) : $(OPEN_JPEG_SRC)jp2.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)jp2.$(OBJ) $(C_) $(OPEN_JPEG_SRC)jp2.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mct.$(OBJ) : $(OPEN_JPEG_SRC)mct.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)mct.$(OBJ) $(C_) $(OPEN_JPEG_SRC)mct.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)mqc.$(OBJ) : $(OPEN_JPEG_SRC)mqc.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)mqc.$(OBJ) $(C_) $(OPEN_JPEG_SRC)mqc.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)openjpeg.$(OBJ) : $(OPEN_JPEG_SRC)openjpeg.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)openjpeg.$(OBJ) $(C_) $(OPEN_JPEG_SRC)openjpeg.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)opj_clock.$(OBJ) : $(OPEN_JPEG_SRC)opj_clock.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)opj_clock.$(OBJ) $(C_) $(OPEN_JPEG_SRC)opj_clock.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)pi.$(OBJ) : $(OPEN_JPEG_SRC)pi.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)pi.$(OBJ) $(C_) $(OPEN_JPEG_SRC)pi.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t1.$(OBJ) : $(OPEN_JPEG_SRC)t1.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)t1.$(OBJ) $(C_) $(OPEN_JPEG_SRC)t1.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)t2.$(OBJ) : $(OPEN_JPEG_SRC)t2.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)t2.$(OBJ) $(C_) $(OPEN_JPEG_SRC)t2.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tcd.$(OBJ) : $(OPEN_JPEG_SRC)tcd.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tcd.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tcd.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tgt.$(OBJ) : $(OPEN_JPEG_SRC)tgt.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tgt.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tgt.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)cidx_manager.$(OBJ) : $(OPEN_JPEG_SRC)cidx_manager.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)cidx_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)cidx_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)tpix_manager.$(OBJ) : $(OPEN_JPEG_SRC)tpix_manager.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)tpix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)tpix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thix_manager.$(OBJ) : $(OPEN_JPEG_SRC)thix_manager.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)thix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)thix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)ppix_manager.$(OBJ) : $(OPEN_JPEG_SRC)ppix_manager.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)ppix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)ppix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)phix_manager.$(OBJ) : $(OPEN_JPEG_SRC)phix_manager.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)phix_manager.$(OBJ) $(C_) $(OPEN_JPEG_SRC)phix_manager.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)thread.$(OBJ) : $(OPEN_JPEG_SRC)thread.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)thread.$(OBJ) $(C_) $(OPEN_JPEG_SRC)thread.c

$(OPEN_JPEG_OBJ)$(OPEN_JPEG_PREFIX)sparse_array.$(OBJ) : $(OPEN_JPEG_SRC)sparse_array.c $(open_jpeg_HDRS) $(OPEN_JPEG_DEP)
	$(OPEN_JPEG_CC) $(OPEN_JPEG_O)sparse_array.$(OBJ) $(C_) $(OPEN_JPEG_SRC)sparse_array.c

# end of file
