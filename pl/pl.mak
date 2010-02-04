#    Copyright (C) 1996-2010 Artifex Software. Inc. All rights reserved.
#    See the file LICENSE for use and distribution terms.

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


################ Shared library include definitions ################

pldebug_h=$(PLSRC)pldebug.h
pldict_h=$(PLSRC)pldict.h
pldraw_h=$(PLSRC)pldraw.h $(gsiparam_h)
plht_h=$(PLSRC)plht.h
pllfont_h=$(PLSRC)pllfont.h
plmain_h=$(PLSRC)plmain.h $(gsargs_h) $(gsgc_h)
plplatf_h=$(PLSRC)plplatf.h
plparse_h=$(PLSRC)plparse.h $(scommon_h)
plsymbol_h=$(PLSRC)plsymbol.h
plsrgb_h=$(PLSRC)plsrgb.h
plvalue_h=$(PLSRC)plvalue.h
plvocab_h=$(PLSRC)plvocab.h
romfnttab_h=$(PLSRC)romfnttab.h
# Out of order because of inclusion
plfont_h=$(PLSRC)plfont.h $(gsccode_h) $(plsymbol_h)

################ PJL ################


PJLVERSION="8.71 (release candidate)"

# Translate pjl file system volume "0:" to a directory of your choice 
# Use forward slash '/' not '\\'; no trailing slash 
# PJL_VOLUME_0=./foo
# PJL_VOLUME_0=/tmp/pjl0
# PJL_VOLUME_0=c:/pjl_volume_0  		

PJL_VOLUME_0=/tmp/pjl0
PJL_VOLUME_1=/tmp/pjl1

plver_h=$(PLOBJ)plver.h

$(PLOBJ)plver.h: $(PLSRC)pl.mak
	$(GLGEN)echogs$(XE) -e .h -w $(PLOBJ)plver -n -x 23 "define PJLVERSION"
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJLVERSION) -x 22
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJLBUILDDATE"
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -s -x 22 -d -x 22
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJL_VOLUME_0"
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJL_VOLUME_0) -x 22
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJL_VOLUME_1"
	$(GLGEN)echogs$(XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJL_VOLUME_1) -x 22

pjparse_h=$(PLSRC)pjparse.h
pjtop_h=$(PLSRC)pjtop.h $(pltop_h)

$(PLOBJ)pjparse.$(OBJ): $(PLSRC)pjparse.c\
	$(ctype__h)  \
        $(stat__h)   \
        $(memory__h) \
        $(scommon_h) \
        $(gdebug_h)  \
        $(gp_h)      \
        $(gxiodev_h) \
        $(pjparse_h) \
        $(plfont_h)  \
        $(plver_h)
	$(PLCCC) $(PLSRC)pjparse.c $(PLO_)pjparse.$(OBJ)

$(PLOBJ)pjparsei.$(OBJ): $(PLSRC)pjparsei.c \
 $(string__h) $(pjtop_h) $(pjparse_h) $(plparse_h) $(string__h) $(gserrors_h) $(plver_h)
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
pllfont_h=$(PLSRC)pllfont.h
plmain_h=$(PLSRC)plmain.h $(gsargs_h) $(gsgc_h)
plplatf_h=$(PLSRC)plplatf.h
plparse_h=$(PLSRC)plparse.h $(scommon_h)
plsymbol_h=$(PLSRC)plsymbol.h
plvalue_h=$(PLSRC)plvalue.h
plvocab_h=$(PLSRC)plvocab.h
romfnttab_h=$(PLSRC)romfnttab.h
# Out of order because of inclusion
plfont_h=$(PLSRC)plfont.h $(gsccode_h) $(plsymbol_h)

# artifex character module.
$(PLOBJ)plchar.$(OBJ): $(PLSRC)plchar.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gsbittab_h) $(gschar_h) $(gscoord_h) $(gserror_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(plfont_h) $(plvalue_h)
	$(PLCCC) $(PLSRC)plchar.c $(PLO_)plchar.$(OBJ)

# agfa ufst character module.
$(PLOBJ)pluchar.$(OBJ): $(PLSRC)pluchar.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gsbittab_h) $(gschar_h) $(gscoord_h) $(gserror_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpath_h) $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(gxchar_h) $(gxfcache_h) $(plfont_h) $(plvalue_h)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)pluchar.c $(PLO_)pluchar.$(OBJ)

$(PLOBJ)pldict.$(OBJ): $(PLSRC)pldict.c $(AK) $(memory__h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(pldict_h)
	$(PLCCC) $(PLSRC)pldict.c $(PLO_)pldict.$(OBJ)

$(PLOBJ)plht.$(OBJ): $(PLSRC)plht.c  $(stdpre_h) $(plht_h) $(gxdevice_h)\
   $(gsstate_h) $(gxtmap_h) $(gsmemory_h) $(gstypes_h) $(gxht_h)
	$(PLCCC) $(PLSRC)plht.c $(PLO_)plht.$(OBJ)

$(PLOBJ)plsrgb.$(OBJ): $(PLSRC)plsrgb.c $(math__h) $(string__h) $(plsrgb_h)\
  $(gzstate_h) $(gxstate_h) $(gsparam_h) $(gscrd_h) $(gscrdp_h) $(gscie_h)\
  $(gsstate_h) $(gscspace_h) $(gsmemory_h) $(gstypes_h)
	$(PLCCC) $(PLSRC)plsrgb.c $(PLO_)plsrgb.$(OBJ)

$(PLOBJ)pldraw.$(OBJ): $(PLSRC)pldraw.c $(AK) $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxdevice_h) $(gzstate_h)\
 $(pldraw_h)
	$(PLCCC) $(PLSRC)pldraw.c $(PLO_)pldraw.$(OBJ)


#artifex font module.
$(PLOBJ)plfont.$(OBJ): $(PLSRC)plfont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h)\
 $(gschar_h) $(gserror_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gsmatrix_h) $(gstypes_h) $(gsutil_h)\
 $(gxfont_h) $(gxfont42_h)\
 $(plfont_h) $(plvalue_h)
	$(PLCCC) $(PLSRC)plfont.c $(PLO_)plfont.$(OBJ)

#ufst font module.
$(PLOBJ)plufont.$(OBJ): $(PLSRC)plufont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gschar_h) $(gserror_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h)\
 $(gxfont_h) $(gxfont42_h)\
 $(plfont_h) $(plvalue_h)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)plufont.c $(PLO_)plufont.$(OBJ)

$(PLOBJ)plplatf$(PLPLATFORM).$(OBJ): $(PLSRC)plplatf$(PLPLATFORM).c $(AK) $(string__h)\
 $(string__h)\
 $(gdebug_h) $(gp_h) $(gsio_h) $(gslib_h) $(gsmemory_h) $(gstypes_h)\
 $(gsstruct_h) $(plplatf_h)
	$(PLCCC) $(PLSRC)plplatf$(PLPLATFORM).c $(PLO_)plplatf$(PLPLATFORM).$(OBJ)

plftable_h=$(PLSRC)plftable.h

# hack - need ufst included for -DAGFA_FONT_TABLE
$(PLOBJ)plftable.$(OBJ): $(PLSRC)plftable.c $(AK) $(plftable_h)\
  $(ctype__h) $(gstypes_h) $(plfont_h)
	$(PLCCC) $(UFST_INCLUDES) $(PLSRC)plftable.c $(PLO_)plftable.$(OBJ)

$(PLOBJ)pltop.$(OBJ): $(PLSRC)pltop.c $(AK) $(string__h)\
 $(gdebug_h) $(gsnogc_h) $(gsdevice_h) $(gsmemory_h) $(gsstruct_h)\
 $(gstypes_h) $(pltop_h)
	$(PLCCC) $(PLSRC)pltop.c $(PLO_)pltop.$(OBJ)

$(PLOBJ)pltoputl.$(OBJ): $(PLSRC)pltoputl.c $(AK) $(string__h)\
 $(gdebug_h) $(gsmemory_h) $(gstypes_h) $(gsstruct_h) $(pltoputl_h)
	$(PLCCC) $(PLSRC)pltoputl.c $(PLO_)pltoputl.$(OBJ)

$(PLOBJ)plsymbol.$(OBJ): $(PLSRC)plsymbol.c $(AK) $(stdpre_h)\
 $(std_h) $(gdebug_h) $(plsymbol_h) $(plvocab_h) $(plvalue_h)
	$(PLCCC) $(PLSRC)plsymbol.c $(PLO_)plsymbol.$(OBJ)

$(PLOBJ)plvalue.$(OBJ): $(PLSRC)plvalue.c $(AK) $(std_h)\
 $(plvalue_h)
	$(PLCCC) $(PLSRC)plvalue.c $(PLO_)plvalue.$(OBJ)

$(PLOBJ)plvocab.$(OBJ): $(PLSRC)plvocab.c $(AK) $(stdpre_h)\
 $(plvocab_h)
	$(PLCCC) $(PLSRC)plvocab.c $(PLO_)plvocab.$(OBJ)

plalloc_h=$(PLSRC)plalloc.h

$(PLOBJ)plalloc.$(OBJ): $(PLSRC)plalloc.c $(AK) \
  $(malloc__h) $(memory__h) $(gdebug_h)\
  $(gsmalloc_h) $(gsmemret_h) $(gsstype_h)\
  $(plalloc_h)
	$(PLCCC) $(PLSRC)plalloc.c $(PLO_)plalloc.$(OBJ)

# ufst font loading module.
uconfig_h=$(PLOBJ)uconfig.h

$(uconfig_h): $(PLSRC)pl.mak
	$(GLGEN)echogs$(XE) -e .h -w $(PLOBJ)uconfig -x 23 "define UFSTFONTDIR" -s -x 22 $(UFSTFONTDIR) -x 22

$(PLOBJ)plulfont.$(OBJ): $(PLSRC)plulfont.c $(pllfont_h) $(uconfig_h) $(AK)\
	$(stdio__h) $(string__h) $(gserror_h)\
        $(gpgetenv_h) $(gsmemory_h) $(gp_h) $(gstypes_h)\
	$(plfont_h) $(pldict_h) $(pllfont_h) $(plvalue_h)\
	$(plftable_h) $(uconfig_h)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)plulfont.c $(PLO_)plulfont.$(OBJ)

# artifex font loading module.
$(PLOBJ)pllfont.$(OBJ): $(PLSRC)pllfont.c $(pllfont_h) $(AK)\
	$(ctype__h) $(stdio__h) $(string__h)\
	$(gx_h) $(gp_h) $(gsccode_h) $(gserrors_h) $(gsmatrix_h) $(gsutil_h)\
	$(gxfont_h) $(gxfont42_h) $(gxiodev_h) \
        $(plfont_h) $(pldict_h) $(plvalue_h) $(plftable_h)
	$(PLCCC) $(PLSRC)pllfont.c $(PLO_)pllfont.$(OBJ)

pl_obj1=$(PLOBJ)pldict.$(OBJ) $(PLOBJ)pldraw.$(OBJ) $(PLOBJ)plsymbol.$(OBJ) $(PLOBJ)plvalue.$(OBJ) $(PLOBJ)plht.$(OBJ) $(PLOBJ)plsrgb.$(OBJ)
pl_obj2=$(PLOBJ)plvocab.$(OBJ) $(PLOBJ)pltop.$(OBJ) $(PLOBJ)pltoputl.$(OBJ)
pl_obj3=$(PLOBJ)plplatf$(PLPLATFORM).$(OBJ) $(PLOBJ)plalloc.$(OBJ)

# shared objects - non font
pl_obj=$(pl_obj1) $(pl_obj2) $(pl_obj3)

# common (afs and ufst systems) font objects
font_common_obj=$(PLOBJ)plchar.$(OBJ) $(PLOBJ)plfont.$(OBJ) $(PLOBJ)plftable.$(OBJ)

# artifex specific objects
afs_obj=$(font_common_obj) $(PLOBJ)pllfont.$(OBJ) 

# ufst specific objects
ufst_obj=$(font_common_obj) $(PLOBJ)pluchar.$(OBJ) $(PLOBJ)plufont.$(OBJ) $(PLOBJ)plulfont.$(OBJ)

# artifex font device.
$(PLOBJ)afs.dev: $(PL_MAK) $(ECHOGS_XE) $(afs_obj) 
	$(SETMOD) $(PLOBJ)afs $(afs_obj)

# ufst font device.  the libraries are expected to be linked in the
# main platform makefile.
$(PLOBJ)ufst.dev: $(PL_MAK) $(ECHOGS_XE)  $(ufst_obj)
	$(SETMOD) $(PLOBJ)ufst $(ufst_obj)


### BROKEN #####
# Bitstream font device
$(PLOBJ)bfs.dev: $(PL_MAK) $(ECHOGS_XE) $(pl_obj1) $(pl_obj2)
	$(SETMOD) $(PLOBJ)bfs $(pl_obj1) $(pl_obj2)
### END BROKEN ###

$(PLOBJ)pl.dev: $(PL_MAK) $(ECHOGS_XE) $(pl_obj)
	$(SETMOD) $(PLOBJ)pl $(pl_obj1)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj2)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj3)
	$(ADDMOD) $(PLOBJ)pl -include $(PLOBJ)$(PL_SCALER) 

###### Command-line driver's main program #####

$(PLOBJ)plmain.$(OBJ): $(PLSRC)plmain.c $(AK) $(string__h)\
 $(gdebug_h) $(gscdefs_h) $(gsio_h) $(gstypes_h) $(gserrors_h) \
 $(gsmemory_h) $(plalloc_h) $(gsmalloc_h) $(gsstruct_h) $(gxalloc_h)\
 $(gsalloc_h) $(gsargs_h) $(gp_h) $(gsdevice_h) $(gslib_h) $(gslibctx_h)\
 $(gxdevice_h) $(gsparam_h) $(pjtop_h) $(plapi_h) $(plparse_h) $(plplatf_h)\
 $(plmain_h) $(pltop_h) $(pltoputl_h) $(gsargs_h) $(dwtrace_h) $(vdtrace_h)
	$(PLCCC) $(PLSRC)plmain.c $(PLO_)plmain.$(OBJ)

$(PLOBJ)plimpl.$(OBJ):  $(PLSRC)plimpl.c            \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PLCCC) $(PLSRC)plimpl.c $(PLO_)plimpl.$(OBJ)
