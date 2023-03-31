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
# makefile for ITU-T Rec. T.835 (ex T.JPEGXR_-5) | ISO/IEC 29199-5 reference software
#
# Users of this makefile must define the following:
#	SHARE_JPEGXR - 1 to link a system (shared) library
#		       0 to compile in the referenced source,
#	JPEGXR_SRCDIR - the jpegxr source top-level directory,
#	JPEGXR_GENDIR - directory for intermediate generated files,
#	JPEGXR_OBJDIR - directory for object files.

# Define the name of this makefile
JPEGXR_MAK=$(GLSRC)jpegxr.mak $(TOP_MAKEFILES)

# local aliases
JPEGXR_SRC=$(JPEGXR_SRCDIR)$(D)
JPEGXR_GEN=$(JPEGXR_GENDIR)$(D)
JPEGXR_OBJ=$(JPEGXR_OBJDIR)$(D)
JPEGXR_O_=$(O_)$(JPEGXR_OBJ)

JPEGXR_CC=$(CC) $(JPEGXR_CFLAGS) $(D_)JXR_DLL_EXPORTS=1$(_D) $(D_)NDEBUG$(_D) $(CCFLAGS)

jpegxr.clean : jpegxr.config-clean jpegxr.clean-not-config-clean

# would be nice if we used an explicit object list here
jpegxr.clean-not-config-clean :
	$(RM_) $(JPEGXR_OBJ)*.$(OBJ)

jpegxr.config-clean :
	$(RM_) $(JPEGXR_GEN)jpegxr.dev
	$(RM_) $(JPEGXR_GEN)jpegxr_0.dev
	$(RM_) $(JPEGXR_GEN)jpegxr_1.dev

jpegxr_objs= \
	$(JPEGXR_OBJ)algo.$(OBJ) \
	$(JPEGXR_OBJ)api.$(OBJ) \
	$(JPEGXR_OBJ)w_emit.$(OBJ) \
	$(JPEGXR_OBJ)flags.$(OBJ) \
	$(JPEGXR_OBJ)init.$(OBJ) \
	$(JPEGXR_OBJ)io.$(OBJ) \
	$(JPEGXR_OBJ)cr_parse.$(OBJ) \
	$(JPEGXR_OBJ)cw_emit.$(OBJ) \
	$(JPEGXR_OBJ)r_parse.$(OBJ) \
	$(JPEGXR_OBJ)jpegxr_pixelformat.$(OBJ) \
	$(JPEGXR_OBJ)r_strip.$(OBJ) \
	$(JPEGXR_OBJ)r_tile_spatial.$(OBJ) \
	$(JPEGXR_OBJ)r_tile_frequency.$(OBJ) \
	$(JPEGXR_OBJ)w_strip.$(OBJ) \
	$(JPEGXR_OBJ)w_tile_spatial.$(OBJ) \
	$(JPEGXR_OBJ)w_tile_frequency.$(OBJ) \
	$(JPEGXR_OBJ)x_strip.$(OBJ)

JPEGXR_DEPS=$(JPEGXR_SRC)jpegxr.h $(JPEGXR_SRC)jxr_priv.h $(JPEGXR_MAK) $(MAKEDIRS)

$(JPEGXR_OBJ)algo.$(OBJ) : $(JPEGXR_SRC)algo.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)algo.$(OBJ) $(C_) $(JPEGXR_SRC)algo.c
$(JPEGXR_OBJ)api.$(OBJ) : $(JPEGXR_SRC)api.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)api.$(OBJ) $(C_) $(JPEGXR_SRC)api.c
$(JPEGXR_OBJ)w_emit.$(OBJ) : $(JPEGXR_SRC)w_emit.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)w_emit.$(OBJ) $(C_) $(JPEGXR_SRC)w_emit.c
$(JPEGXR_OBJ)flags.$(OBJ) : $(JPEGXR_SRC)flags.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)flags.$(OBJ) $(C_) $(JPEGXR_SRC)flags.c
$(JPEGXR_OBJ)init.$(OBJ) : $(JPEGXR_SRC)init.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)init.$(OBJ) $(C_) $(JPEGXR_SRC)init.c
$(JPEGXR_OBJ)io.$(OBJ) : $(JPEGXR_SRC)io.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)io.$(OBJ) $(C_) $(JPEGXR_SRC)io.c
$(JPEGXR_OBJ)cr_parse.$(OBJ) : $(JPEGXR_SRC)cr_parse.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)cr_parse.$(OBJ) $(C_) $(JPEGXR_SRC)cr_parse.c
$(JPEGXR_OBJ)cw_emit.$(OBJ) : $(JPEGXR_SRC)cw_emit.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)cw_emit.$(OBJ) $(C_) $(JPEGXR_SRC)cw_emit.c
$(JPEGXR_OBJ)r_parse.$(OBJ) : $(JPEGXR_SRC)r_parse.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)r_parse.$(OBJ) $(C_) $(JPEGXR_SRC)r_parse.c
$(JPEGXR_OBJ)jpegxr_pixelformat.$(OBJ) : $(JPEGXR_SRC)jpegxr_pixelformat.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)jpegxr_pixelformat.$(OBJ) $(C_) $(JPEGXR_SRC)jpegxr_pixelformat.c
$(JPEGXR_OBJ)r_strip.$(OBJ) : $(JPEGXR_SRC)r_strip.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)r_strip.$(OBJ) $(C_) $(JPEGXR_SRC)r_strip.c
$(JPEGXR_OBJ)r_tile_spatial.$(OBJ) : $(JPEGXR_SRC)r_tile_spatial.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)r_tile_spatial.$(OBJ) $(C_) $(JPEGXR_SRC)r_tile_spatial.c
$(JPEGXR_OBJ)r_tile_frequency.$(OBJ) : $(JPEGXR_SRC)r_tile_frequency.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)r_tile_frequency.$(OBJ) $(C_) $(JPEGXR_SRC)r_tile_frequency.c
$(JPEGXR_OBJ)w_strip.$(OBJ) : $(JPEGXR_SRC)w_strip.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)w_strip.$(OBJ) $(C_) $(JPEGXR_SRC)w_strip.c
$(JPEGXR_OBJ)w_tile_spatial.$(OBJ) : $(JPEGXR_SRC)w_tile_spatial.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)w_tile_spatial.$(OBJ) $(C_) $(JPEGXR_SRC)w_tile_spatial.c
$(JPEGXR_OBJ)w_tile_frequency.$(OBJ) : $(JPEGXR_SRC)w_tile_frequency.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)w_tile_frequency.$(OBJ) $(C_) $(JPEGXR_SRC)w_tile_frequency.c
$(JPEGXR_OBJ)x_strip.$(OBJ) : $(JPEGXR_SRC)x_strip.c $(JPEGXR_DEPS)
	$(JPEGXR_CC) $(JPEGXR_O_)x_strip.$(OBJ) $(C_) $(JPEGXR_SRC)x_strip.c

# Define the compiled in target
$(JPEGXR_GEN)jpegxr_0.dev : $(ECHOGS_XE) $(jpegxr_objs) $(JPEGXR_DEPS)
	$(SETMOD) $(JPEGXR_GEN)jpegxr_0 $(jpegxr_objs)

# Define the external link target
$(JPEGXR_GEN)jpegxr_1.dev : $(ECHOGS_XE) $(JPEGXR_MAK) $(MAKEDIRS)
	$(SETMOD) $(JPEGXR_GEN)jpegxr_1 -lib jpegxr

# Copy the target definition we want
$(JPEGXR_GEN)jpegxr.dev : $(TOP_MAKEFILES) \
 $(JPEGXR_GEN)jpegxr_$(SHARE_JPEGXR).dev $(JPEGXR_MAK) $(MAKEDIRS)
	$(CP_) $(JPEGXR_GEN)jpegxr_$(SHARE_JPEGXR).dev $(JPEGXR_GEN)jpegxr.dev


