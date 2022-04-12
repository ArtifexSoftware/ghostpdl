# Copyright (C) 2019-2022 Artifex Software, Inc.
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

# makefile for CAL library code.
# Users of this makefile must define the following:
#       WITH_CAL - whether to compile in or link to the library
#       CALSRCDIR - the library source directory
#
# gs.mak and friends define the following:
#       CALOBJDIR - the output obj directory
#       CALGENDIR - generated (.dev) file directory
#

# Rather than detecting the capabilities of the machine at compile-time
# and then only building the required source, we build the "best"
# CAL library we can, and then select from the available functions at
# run-time.
#
# This means we detect architecture at build time (and just build the
# appropriate files), but build all the different cores that the given
# compiler will understand.
#
# Unfortunately, this brings us some problems; if you build SSE code
# with -mavx2, then the sse2 code is built differently than if it is
# built with -msse4.2, leading to "illegal instruction" crashes on
# machines that do not support AVX2. We therefore need to build each
# core routine with *just* the machine flags that it needs.

CAL_MAK=$(GLSRC)cal.mak $(TOP_MAKEFILES)

CAL_GEN=$(CALOBJDIR)$(D)

CAL_PREFIX=cal_
CAL_OBJ=$(CALOBJDIR)$(D)
CAL_SRC=$(CALSRCDIR)$(D)

# source files to build from the CAL source

cal_OBJS = \
	$(CAL_OBJ)$(CAL_PREFIX)cal.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)rescale.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)halftone.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)doubler.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)blendavx2.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)blendsse42.$(OBJ)\
	$(CAL_OBJ)$(CAL_PREFIX)blend.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)skew.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)deskew.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)cmsavx2.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)cmssse42.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)cmsneon.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)lcms2mt_cal.$(OBJ)

cal_HDRS = \
	$(CAL_SRC)cal.h		\
	$(CAL_SRC)cal-impl.h	\
	$(CAL_SRC)cal-impl.h	\
	$(CAL_SRC)cal-immintrin.h	\
	$(CAL_SRC)cal-nmmintrin.h	\
	$(arch_h)

# external link .dev - empty, as we add the lib to LDFLAGS
#$(GLOBJ)cal.dev : $(CAL_MAK) $(ECHOGS_XE) \
# $(CAL_MAK) $(MAKEDIRS)
#	$(SETMOD) $(CAL_GEN)cal

# compile our own .dev
$(GLOBJ)cal.dev : $(ECHOGS_XE) $(cal_OBJS) \
 $(CAL_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLOBJ)cal $(cal_OBJS)

# define our specific compiler
CAL_CC=$(CC) $(CCFLAGS) $(CAL_CFLAGS) $(I_)$(LCMS2MTSRCDIR)$(D)include $(I_)$(CAL_GEN)$(_I) $(I_)$(CAL_SRC)$(_I)
CAL_O=$(O_)$(CAL_OBJ)$(CAL_PREFIX)

CAL_DEP=$(AK) $(CAL_MAK) $(MAKEDIRS)

# explicit rules for building each source file
# for simplicity we have every source file depend on all headers

$(CAL_OBJ)$(CAL_PREFIX)cal.$(OBJ) : $(CAL_SRC)cal.c $(cal_HDRS) $(CAL_DEP)
	$(CAL_CC) $(CAL_O)cal.$(OBJ) $(C_) $(CAL_SRC)cal.c

$(CAL_OBJ)$(CAL_PREFIX)rescale.$(OBJ) : $(CAL_SRC)rescale.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)rescale_c.h $(CAL_SRC)rescale_sse.h
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(CAL_O)rescale.$(OBJ) $(C_) $(CAL_SRC)rescale.c

$(CAL_OBJ)$(CAL_PREFIX)halftone.$(OBJ) : $(CAL_SRC)halftone.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)halftone_c.h $(CAL_SRC)halftone_sse.h
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(CAL_O)halftone.$(OBJ) $(C_) $(CAL_SRC)halftone.c

$(CAL_OBJ)$(CAL_PREFIX)doubler.$(OBJ) : $(CAL_SRC)doubler.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)double_c.h $(CAL_SRC)double_sse.h
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(CAL_O)doubler.$(OBJ) $(C_) $(CAL_SRC)doubler.c

$(CAL_OBJ)$(CAL_PREFIX)cmsavx2.$(OBJ) : $(CAL_SRC)cmsavx2.c $(cal_HDRS) $(CAL_DEP)
	$(CAL_CC) $(CAL_AVX2_CFLAGS) $(I_)$(LCMS2MTSRCDIR)$(D)src $(CAL_O)cmsavx2.$(OBJ) $(C_) $(CAL_SRC)cmsavx2.c

$(CAL_OBJ)$(CAL_PREFIX)cmssse42.$(OBJ) : $(CAL_SRC)cmssse42.c $(cal_HDRS) $(CAL_DEP)
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(I_)$(LCMS2MTSRCDIR)$(D)src $(CAL_O)cmssse42.$(OBJ) $(C_) $(CAL_SRC)cmssse42.c

$(CAL_OBJ)$(CAL_PREFIX)cmsneon.$(OBJ) : $(CAL_SRC)cmsneon.c $(cal_HDRS) $(CAL_DEP)
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(I_)$(LCMS2MTSRCDIR)$(D)src $(CAL_O)cmsneon.$(OBJ) $(C_) $(CAL_SRC)cmsneon.c

$(CAL_OBJ)$(CAL_PREFIX)lcms2mt_cal.$(OBJ) : $(CAL_SRC)lcms2mt_cal.c $(cal_HDRS) $(CAL_DEP) $(gsmemory_h)
	$(CAL_CC) $(I_)$(GLSRC) $(I_)$(LCMS2MTSRCDIR)$(D)src $(CAL_O)lcms2mt_cal.$(OBJ) $(C_) $(CAL_SRC)lcms2mt_cal.c

$(CAL_OBJ)$(CAL_PREFIX)blendavx2.$(OBJ) : $(CAL_SRC)blendavx2.c $(cal_HDRS) $(CAL_DEP) $(gxblend_h)
	$(CAL_CC) $(CAL_AVX2_CFLAGS) $(I_)$(GLSRC) $(CAL_O)blendavx2.$(OBJ) $(C_) $(CAL_SRC)blendavx2.c

$(CAL_OBJ)$(CAL_PREFIX)blendsse42.$(OBJ) : $(CAL_SRC)blendsse42.c $(cal_HDRS) $(CAL_DEP) $(gxblend_h)
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(I_)$(GLSRC) $(CAL_O)blendsse42.$(OBJ) $(C_) $(CAL_SRC)blendsse42.c

$(CAL_OBJ)$(CAL_PREFIX)blend.$(OBJ) : $(CAL_SRC)blend.c $(cal_HDRS) $(CAL_DEP) $(gsmemory_h)
	$(CAL_CC) $(I_)$(GLSRC) $(CAL_O)blend.$(OBJ) $(C_) $(CAL_SRC)blend.c

$(CAL_OBJ)$(CAL_PREFIX)skew.$(OBJ) : $(CAL_SRC)skew.c $(cal_HDRS) $(CAL_DEP) $(gsmemory_h)
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(I_)$(GLSRC) $(CAL_O)skew.$(OBJ) $(C_) $(CAL_SRC)skew.c

$(CAL_OBJ)$(CAL_PREFIX)deskew.$(OBJ) : $(CAL_SRC)deskew.c $(cal_HDRS) $(CAL_DEP) $(gsmemory_h)
	$(CAL_CC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(I_)$(GLSRC) $(CAL_O)deskew.$(OBJ) $(C_) $(CAL_SRC)deskew.c

cal_ets_h=$(CAL_SRC)cal_ets.h
ca_ets_tm_h=$(CAL_SRC)cal_ets_tm.h
$(GLOBJ)ets_1.$(OBJ) : $(CAL_SRC)cal_ets.c $(CAL_SRC)ets_template.c \
 $(cal_ets_h) $(cal_ets_tm_h) $(cal_HDRS) $(CAL_DEP) $(LIB_MAK)
	$(GLCC) $(CAL_SSE4_2_CFLAGS) $(CAL_NEON_CFLAGS) $(GLO_)ets_1.$(OBJ) $(C_) $(CAL_SRC)cal_ets.c


# end of file
