#    Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL* interpreter libraries and for PJL.
# Users of this makefile must define the following:
#	GLSRCDIR - the GS library source directory
#	GLGENDIR - the GS library generated file directory
#	PLSRCDIR - the source directory
#	PLOBJDIR - the object / executable directory

PLSRC=$(PLSRCDIR)$(D)
PLOBJ=$(PLOBJDIR)$(D)
PLO_=$(O_)$(PLOBJ)

PLCCC=$(CC_) -I$(PLSRCDIR) -I$(GLSRCDIR) -I$(GLGENDIR) $(C_)

# Define the name of this makefile.
PL_MAK=$(PLSRC)pl.mak

pl.clean: pl.config-clean pl.clean-not-config-clean

pl.clean-not-config-clean:
	$(RM_) $(PLOBJ)*.$(OBJ)

pl.config-clean:
	$(RM_) $(PLOBJ)*.dev

################ PJL ################

# Currently we only parse PJL enough to detect UELs.

pjparse_h=$(PLSRC)pjparse.h

$(PLOBJ)pjparse.$(OBJ): $(PLSRC)pjparse.c $(memory__h)\
 $(scommon_h) $(pjparse_h)
	$(PLCCC) $(PLSRC)pjparse.c $(PLO_)pjparse.$(OBJ)

pjl_obj=$(PLOBJ)pjparse.$(OBJ)
$(PLOBJ)pjl.dev: $(PL_MAK) $(ECHOGS_XE) $(pjl_obj)
	$(SETMOD) $(PLOBJ)pjl $(pjl_obj)

################ Shared libraries ################

pldict_h=$(PLSRC)pldict.h
pldraw_h=$(PLSRC)pldraw.h $(gsiparam_h)
plmain_h=$(PLSRC)plmain.h $(gsargs_h) $(gsgc_h)
plsymbol_h=$(PLSRC)plsymbol.h
plvalue_h=$(PLSRC)plvalue.h
plvocab_h=$(PLSRC)plvocab.h
# Out of order because of inclusion
plfont_h=$(PLSRC)plfont.h $(gsccode_h) $(plsymbol_h)

$(PLOBJ)plchar.$(OBJ): $(PLSRC)plchar.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gsbittab_h) $(gschar_h) $(gscoord_h) $(gserror_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(plfont_h) $(plvalue_h)
	$(PLCCC) $(PLSRC)plchar.c $(PLO_)plchar.$(OBJ)

$(PLOBJ)pldict.$(OBJ): $(PLSRC)pldict.c $(AK) $(memory__h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(pldict_h)
	$(PLCCC) $(PLSRC)pldict.c $(PLO_)pldict.$(OBJ)

$(PLOBJ)pldraw.$(OBJ): $(PLSRC)pldraw.c $(AK) $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxdevice_h) $(gzstate_h)\
 $(pldraw_h)
	$(PLCCC) $(PLSRC)pldraw.c $(PLO_)pldraw.$(OBJ)

$(PLOBJ)plfont.$(OBJ): $(PLSRC)plfont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gschar_h) $(gserror_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxfont_h) $(gxfont42_h)\
 $(plfont_h) $(plvalue_h)
	$(PLCCC) $(PLSRC)plfont.c $(PLO_)plfont.$(OBJ)

$(PLOBJ)plmain.$(OBJ): $(PLSRC)plmain.c $(AK) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gp_h) $(gscdefs_h) $(gsdevice_h) $(gslib_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gsparam_h) $(gsstate_h) $(gstypes_h)\
 $(plmain_h)
	$(PLCCC) $(PLSRC)plmain.c $(PLO_)plmain.$(OBJ)

$(PLOBJ)plsymbol.$(OBJ): $(PLSRC)plsymbol.c $(AK) $(stdpre_h)\
 $(plsymbol_h)
	$(PLCCC) $(PLSRC)plsymbol.c $(PLO_)plsymbol.$(OBJ)

$(PLOBJ)plvalue.$(OBJ): $(PLSRC)plvalue.c $(AK) $(std_h)\
 $(plvalue_h)
	$(PLCCC) $(PLSRC)plvalue.c $(PLO_)plvalue.$(OBJ)

$(PLOBJ)plvocab.$(OBJ): $(PLSRC)plvocab.c $(AK) $(stdpre_h)\
 $(plvocab_h)
	$(PLCCC) $(PLSRC)plvocab.c $(PLO_)plvocab.$(OBJ)

pl_obj1=$(PLOBJ)plchar.$(OBJ) $(PLOBJ)pldict.$(OBJ) $(PLOBJ)pldraw.$(OBJ) $(PLOBJ)plfont.$(OBJ)
pl_obj2=$(PLOBJ)plmain.$(OBJ) $(PLOBJ)plsymbol.$(OBJ) $(PLOBJ)plvalue.$(OBJ) $(PLOBJ)plvocab.$(OBJ)
pl_obj=$(pl_obj1) $(pl_obj2)

$(PLOBJ)pl.dev: $(PL_MAK) $(ECHOGS_XE) $(pl_obj)
	$(SETMOD) $(PLOBJ)pl $(pl_obj1)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj2)
