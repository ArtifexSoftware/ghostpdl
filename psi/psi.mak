#    Copyright (C) 1996-2008 Artifex Software Inc. All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PS Inteface (PSI) to Ghostscript PostScript.
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
 $(PSIGEN)pconf.h $(plfont_h) $(uconfig_h)
	$(CP_) $(PSIGEN)pconf.h $(PSIGEN)pconfig.h
	$(PSICCC) $(PSISRC)psitop.c $(O_)$(PSI_TOP_OBJ)

$(PSIOBJ)psi.dev: $(PSI_MAK) $(ECHOGS_XE) $(PLOBJ)pl.dev $(PLOBJ)pjl.dev 
	$(SETMOD) $(PSIOBJ)psi $(PSI_TOP_OBJ)
	$(ADDMOD) $(PSIOBJ)psi -include $(PLOBJ)pl $(PLOBJ)pjl

