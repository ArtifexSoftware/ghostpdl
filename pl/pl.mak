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
GLGEN=$(GLGENDIR)$(D)

PLCCC=$(CC_) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

# Define the name of this makefile.
PL_MAK=$(PLSRC)pl.mak

pl.clean: pl.config-clean pl.clean-not-config-clean

pl.clean-not-config-clean:
	$(RM_) $(PLOBJ)*.$(OBJ)

pl.config-clean:
	$(RM_) $(PLOBJ)*.dev

########### Common definitions ######
pltop_h=$(PLSRC)pltop.h $(scommon_h) $(gsgc_h)
pltoputl_h=$(PLSRC)pltoputl.h $(scommon_h)


################ PJL ################


PJLVERSION=1.33

plver_h=$(PLSRC)plver.h

$(PLSRC)plver.h: $(PLSRC)pl.mak
	$(GLGEN)echogs$(XE) -e .h -w $(PLSRC)plver -n -x 23 "define PJLVERSION"
	$(GLGEN)echogs$(XE) -e .h -a $(PLSRC)plver -s -x 22 $(PJLVERSION) -x 22
	$(GLGEN)echogs$(XE) -e .h -a $(PLSRC)plver -n -x 23 "define PJLBUILDDATE"
	$(GLGEN)echogs$(XE) -e .h -a $(PLSRC)plver -s -x 22 -d -x 22



# Currently we only parse PJL enough to detect UELs.

pjparse_h=$(PLSRC)pjparse.h
pjtop_h=$(PLSRC)pjtop.h $(pltop_h)

$(PLOBJ)pjparse.$(OBJ): $(PLSRC)pjparse.c $(memory__h)\
 $(scommon_h) $(pjparse_h)
	$(PLCCC) $(PLSRC)pjparse.c $(PLO_)pjparse.$(OBJ)

$(PLOBJ)pjparsei.$(OBJ): $(PLSRC)pjparsei.c $(memory__h)\
 $(scommon_h) $(pjparsei_h) $(plparse_h) $(string__h) $(gserrors_h) $(plver_h)
	$(PLCCC) $(PLSRC)pjparsei.c $(PLO_)pjparsei.$(OBJ)

$(PLOBJ)pjtop.$(OBJ): $(PLSRC)pjtop.c $(AK) $(pjtop_h) $(string__h)
	$(PLCCC) $(PLSRC)pjtop.c $(PLO_)pjtop.$(OBJ)

pjl_obj=$(PLOBJ)pjparse.$(OBJ) $(PLOBJ)pjparsei.$(OBJ) $(PLOBJ)pjtop.$(OBJ) $(PLOBJ)pltop.$(OBJ)
$(PLOBJ)pjl.dev: $(PL_MAK) $(ECHOGS_XE) $(pjl_obj)
	$(SETMOD) $(PLOBJ)pjl $(pjl_obj)

################ Shared libraries ################

pldebug_h=$(PLSRC)pldebug.h
pldict_h=$(PLSRC)pldict.h
pldraw_h=$(PLSRC)pldraw.h $(gsiparam_h)
plplatf_h=$(PLSRC)plplatf.h
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

$(PLOBJ)plplatf.$(OBJ): $(PLSRC)plplatf.c $(AK) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gp_h) $(gsio_h) $(gslib_h) $(gsmemory_h) $(gstypes_h)\
 $(plplatf_h)
	$(PLCCC) $(PLSRC)plplatf.c $(PLO_)plplatf.$(OBJ)

$(PLOBJ)pltop.$(OBJ): $(PLSRC)pltop.c $(AK) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gsdevice_h) $(gsmemory_h) $(gstypes_h) $(pltop_h)
	$(PLCCC) $(PLSRC)pltop.c $(PLO_)pltop.$(OBJ)

$(PLOBJ)pltoputl.$(OBJ): $(PLSRC)pltoputl.c $(AK) $(string__h)\
 $(gdebug_h) $(gsmemory_h) $(gstypes_h) $(pltoputl_h)
	$(PLCCC) $(PLSRC)pltoputl.c $(PLO_)pltoputl.$(OBJ)

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
pl_obj2=$(PLOBJ)plsymbol.$(OBJ) $(PLOBJ)plvalue.$(OBJ) $(PLOBJ)plvocab.$(OBJ)
pl_obj3=$(PLOBJ)pltop.$(OBJ) $(PLOBJ)pltoputl.$(OBJ) $(PLOBJ)plplatf.$(OBJ)
pl_obj=$(pl_obj1) $(pl_obj2) $(pl_obj3)

$(PLOBJ)pl.dev: $(PL_MAK) $(ECHOGS_XE) $(pl_obj)
	$(SETMOD) $(PLOBJ)pl $(pl_obj1)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj2)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj3)

###### Command-line driver's main program #####

$(PLOBJ)plmain.$(OBJ): $(PLSRC)plmain.c $(AK) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gp_h) $(gsdevice_h) $(gsio_h) $(gsmemory_h) $(gsparam_h)\
 $(gstypes_h) $(gserrors_h) $(gsmalloc_h) $(gsstruct_h) $(gxalloc_h)\
 $(gsalloc_h) $(plparse_h) $(pltop_h) $(pltoputl_h)
	$(PLCCC) $(PLSRC)plmain.c $(PLO_)plmain.$(OBJ)

$(PLOBJ)plimpl.$(OBJ):  $(PLSRC)plimpl.c            \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PLCCC) $(PLSRC)plimpl.c $(PLO_)plimpl.$(OBJ)
