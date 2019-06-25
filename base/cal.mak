# Copyright (C) 2019 Artifex Software, Inc.
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

CAL_MAK=$(GLSRC)cal.mak $(TOP_MAKEFILES)

CAL_GEN=$(CALOBJDIR)$(D)

CAL_PREFIX=cal_
CAL_OBJ=$(CALOBJDIR)$(D)
CAL_SRC=$(CALSRCDIR)$(D)

# source files to build from the CSDK source

cal_OBJS = \
	$(CAL_OBJ)$(CAL_PREFIX)cal.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)rescale.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)halftone.$(OBJ)	\
	$(CAL_OBJ)$(CAL_PREFIX)doubler.$(OBJ)

cal_HDRS = \
	$(CAL_SRC)cal.h		\
	$(CAL_SRC)cal-impl.h	\

# external link .dev - empty, as we add the lib to LDFLAGS
#$(GLOBJ)cal.dev : $(CAL_MAK) $(ECHOGS_XE) \
# $(CAL_MAK) $(MAKEDIRS)
#	$(SETMOD) $(CAL_GEN)cal

# compile our own .dev
$(GLOBJ)cal.dev : $(ECHOGS_XE) $(cal_OBJS) \
 $(CAL_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLOBJ)cal $(cal_OBJS)

# define our specific compiler
CAL_CC=$(CC) $(CFLAGS) $(D_)OPJ_STATIC$(_D) $(D_)STANDARD_SLOW_VERSION$(_D) $(I_)$(CAL_GEN)$(_I) $(I_)$(CAL_SRC)$(_I)
CAL_O=$(O_)$(CAL_OBJ)$(CAL_PREFIX)

CAL_DEP=$(AK) $(CAL_MAK) $(MAKEDIRS)

# explicit rules for building each source file
# for simplicity we have every source file depend on all headers

$(CAL_OBJ)$(CAL_PREFIX)cal.$(OBJ) : $(CAL_SRC)cal.c $(cal_HDRS) $(CAL_DEP)
	$(CAL_CC) $(CAL_O)cal.$(OBJ) $(C_) $(CAL_SRC)cal.c

$(CAL_OBJ)$(CAL_PREFIX)rescale.$(OBJ) : $(CAL_SRC)rescale.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)rescale_c.h $(CAL_SRC)rescale_sse.h
	$(CAL_CC) $(CAL_O)rescale.$(OBJ) $(C_) $(CAL_SRC)rescale.c

$(CAL_OBJ)$(CAL_PREFIX)halftone.$(OBJ) : $(CAL_SRC)halftone.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)halftone_c.h $(CAL_SRC)halftone_sse.h
	$(CAL_CC) $(CAL_O)halftone.$(OBJ) $(C_) $(CAL_SRC)halftone.c

$(CAL_OBJ)$(CAL_PREFIX)doubler.$(OBJ) : $(CAL_SRC)doubler.c $(cal_HDRS) $(CAL_DEP) $(CAL_SRC)double_c.h $(CAL_SRC)double_sse.h
	$(CAL_CC) $(CAL_O)doubler.$(OBJ) $(C_) $(CAL_SRC)doubler.c

# end of file
