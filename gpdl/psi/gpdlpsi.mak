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

# makefile for PS Interface (PSI) to Ghostscript PostScript.
# Users of this makefile must define the following:
#	PSSRCDIR - the PS interpreter source directory
#	GLSRCDIR - the GS library source directory
#	GLGENDIR - the GS library generated file directory
#	PLSRCDIR - the PCL* support library source directory
#	PLOBJDIR - the PCL* support library object / executable directory
#	PSISRCDIR - the source directory
#	PSIGENDIR - the directory for source files generated during building
#	PSIOBJDIR - the object / executable directory
#	PSI_TOP_OBJ - object file to top-level interpreter API

PLOBJ=$(PLOBJDIR)$(D)

PSISRC=$(PSISRCDIR)$(D)
PSIGEN=$(PSIGENDIR)$(D)
PSIOBJ=$(PSIOBJDIR)$(D)
PSIO_=$(O_)$(PSIOBJ)

PSICCC=$(CC_) $(I_)$(PSISRCDIR)$(_I) $(I_)$(PSIGENDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(PSSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

# Define the name of this makefile.
PSI_MAK=$(PSISRC)psi.mak

psi.clean: psi.config-clean psi.clean-not-config-clean

psi.clean-not-config-clean: clean_gs
	$(RM_) $(PSIOBJ)*.$(OBJ)
	$(RM_) $(PSIOBJ)devs.tr6

# devices are still created in the current directory.  Until that 
# is fixed we will have to remove them from both directories.
psi.config-clean:
	$(RM_) $(PSIOBJ)*.dev
	$(RM_) *.dev

################ PS Language Interface ################

# Top-level API
$(PSI_TOP_OBJ): $(PSISRC)psitop.c $(AK) $(stdio__h)\
 $(string__h) $(gdebug_h) $(gp_h) $(gsdevice_h) $(gserrors_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gspaint_h) $(gstypes_h) $(gxalloc_h) $(gxstate_h)\
 $(gsnogc_h) $(pltop_h) $(psitop_h) $(plparse_h) $(gsicc_manage_h)\
 $(PSIGEN)pconf.h $(plfont_h) $(uconfig_h) $(pconfig_h)
	$(PSICCC) $(PSISRC)psitop.c $(O_)$(PSI_TOP_OBJ)

$(PSIOBJ)psi.dev: $(PSI_MAK) $(ECHOGS_XE) $(PLOBJ)pjl.dev 
	$(SETMOD) $(PSIOBJ)psi $(PSI_TOP_OBJ)
	$(ADDMOD) $(PSIOBJ)psi -include $(PLOBJ)pl $(PLOBJ)pjl
