# Copyright (C) 2001-2021 Artifex Software, Inc.
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

# makefile for PCL* interpreter libraries and for PJL.
# Users of this makefile must define the following:
#	GLSRCDIR - the GS library source directory
#	GLGENDIR - the GS library generated file directory
#	PLSRCDIR - the source directory
#	PLOBJDIR - the object / executable directory

MAIN_OBJ=$(PLOBJDIR)$(D)plmain.$(OBJ)
REALMAIN_OBJ=$(PLOBJDIR)$(D)realmain.$(OBJ)
REALMAIN_SRC=realmain
PCL_TOP_OBJ=$(PCL5OBJDIR)$(D)pctop.$(OBJ)
PXL_TOP_OBJ=$(PXLOBJDIR)$(D)pxtop.$(OBJ)
PCL_PXL_TOP_OBJS=$(PCL_TOP_OBJ) $(PXL_TOP_OBJ)
TOP_OBJ=$(PLOBJDIR)$(D)plimpl.$(OBJ) $(PCL_PXL_TOP_OBJS)

PLSRC=$(PLSRCDIR)$(D)
PLOBJ=$(PLOBJDIR)$(D)
PLO_=$(O_)$(PLOBJ)
GLGEN=$(GLGENDIR)$(D)

PLCCC=$(CC_) $(D_)PCL_INCLUDED$(_D) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(DEVSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)
PLATCCC=$(PLCCC) $(D_)PCL_INCLUDED$(_D)

#### Note PLCCC brings /Za, which can't compile Windows headers, so we define and use PLCCC_W instead. :
CC_W=$(CC_WX) $(D_)PCL_INCLUDED$(_D) $(COMPILE_FULL_OPTIMIZED) $(ZM)
PLCCC_W=$(CC_W) $(D_)PCL_INCLUDED$(_D) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(DEVSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

# Define the name of this makefile.
PL_MAK=$(PLSRC)pl.mak $(TOP_MAKEFILES)

pl.clean: pl.config-clean pl.clean-not-config-clean

pl.clean-not-config-clean:
	$(RM_) $(PLOBJ)*.$(OBJ)

pl.config-clean:
	$(RM_) $(PLOBJ)*.dev

########### Common definitions ######
pltop_h=$(PLSRC)pltop.h $(scommon_h) $(gsgc_h)

PL_SCALER=afs
PCL_FONT_SCALER=$(PL_SCALER)
PXL_FONT_SCALER=$(PL_SCALER)


################ Shared library include definitions ################

pldebug_h=$(PLSRC)pldebug.h
pldict_h=$(PLSRC)pldict.h
pldraw_h=$(PLSRC)pldraw.h $(gsiparam_h)
plht_h=$(PLSRC)plht.h
pllfont_h=$(PLSRC)pllfont.h
plmain_h=$(PLSRC)plmain.h $(gsargs_h) $(gsgc_h)
plparse_h=$(PLSRC)plparse.h $(scommon_h)
plsymbol_h=$(PLSRC)plsymbol.h
plvalue_h=$(PLSRC)plvalue.h
plvocab_h=$(PLSRC)plvocab.h
romfnttab_h=$(PLSRC)romfnttab.h
# Out of order because of inclusion
plfont_h=$(PLSRC)plfont.h $(gsccode_h) $(plsymbol_h)          $(pldict_h) $(stream_h) $(strmio_h)
plchar_h=$(PLSRC)plchar.h

pconfig_h=$(GLGEN)pconfig.h

$(pconfig_h): $(GLGEN)pconf.h
	$(CP_) $(GLGEN)pconf.h $(GLGEN)pconfig.h

################ PJL ################


PJLVERSION="$(GS_DOT_VERSION)"

# Translate pjl file system volume "0:" to a directory of your choice
# Use forward slash '/' not '\\'; no trailing slash
# PJL_VOLUME_0=./foo
# PJL_VOLUME_0=/tmp/pjl0
# PJL_VOLUME_0=c:/pjl_volume_0

PJL_VOLUME_0=/tmp/pjl0
PJL_VOLUME_1=/tmp/pjl1

plver_h=$(PLOBJ)plver.h

# FIXME: move elsewhere
$(GLGEN)pconf.h $(GLGEN)/ldconf.tr: $(TARGET_DEVS) $(GENCONF_XE) $(PL_MAK) $(MAKEDIRS)
	$(GENCONF_XE) -n - $(TARGET_DEVS) -h $(GLGEN)/pconf.h -p "%s&s&&" -o $(GLGEN)/ldconf.tr

$(PLOBJ)plver.h: $(ECHOGS_XE) $(PL_MAK) $(MAKEDIRS)
	$(ECHOGS_XE) -e .h -w $(PLOBJ)plver -n -x 23 "define PJLVERSION"
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJLVERSION) -x 22
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJLBUILDDATE"
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -s -x 22 -d -x 22
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJL_VOLUME_0"
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJL_VOLUME_0) -x 22
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -n -x 23 "define PJL_VOLUME_1"
	$(ECHOGS_XE) -e .h -a $(PLOBJ)plver -s -x 22 $(PJL_VOLUME_1) -x 22

pjparse_h=$(PLSRC)pjparse.h
pjtop_h=$(PLSRC)pjtop.h $(pltop_h)
plparams_h=$(PLSRC)plparams.h

$(PLOBJ)pjparse.$(OBJ): $(PLSRC)pjparse.c\
	$(ctype__h)   \
        $(stat__h)    \
        $(memory__h)  \
        $(scommon_h)  \
        $(gdebug_h)   \
        $(gp_h)       \
        $(gxiodev_h)  \
        $(gdevpxen_h) \
        $(pjparse_h)  \
        $(plfont_h)   \
        $(plver_h)    \
        $(plmain_h)   \
        $(PL_MAK)     \
        $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pjparse.c $(PLO_)pjparse.$(OBJ)

$(PLOBJ)pjparsei.$(OBJ): $(PLSRC)pjparsei.c \
 $(string__h) $(pjtop_h) $(pjparse_h) $(plparse_h) $(string__h) $(gserrors_h) $(plver_h) \
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pjparsei.c $(PLO_)pjparsei.$(OBJ)

$(PLOBJ)pjtop.$(OBJ): $(PLSRC)pjtop.c $(AK) $(pjtop_h) $(string__h) \
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pjtop.c $(PLO_)pjtop.$(OBJ)

$(PLOBJ)plparams.$(OBJ): $(PLSRC)plparams.c \
 $(memory__h) $(gsmatrix_h) $(gsdevice_h) $(gp_h) $(gsparam_h) \
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plparams.c $(PLO_)plparams.$(OBJ)

pjl_obj=$(PLOBJ)pjparse.$(OBJ) $(PLOBJ)pjparsei.$(OBJ) $(PLOBJ)pjtop.$(OBJ) $(PLOBJ)plparams.$(OBJ) $(PLOBJ)pltop.$(OBJ)
$(PLOBJ)pjl.dev: $(PL_MAK) $(ECHOGS_XE) $(pjl_obj) $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)pjl $(pjl_obj)

################ Shared libraries ################

pldebug_h=$(PLSRC)pldebug.h
pldict_h=$(PLSRC)pldict.h
pldraw_h=$(PLSRC)pldraw.h $(gsiparam_h)
plfapi_h=$(PLSRC)plfapi.h
pllfont_h=$(PLSRC)pllfont.h
plmain_h=$(PLSRC)plmain.h $(gsargs_h) $(gsgc_h)
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
 $(gsbittab_h) $(gschar_h) $(gscoord_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(plfont_h) $(plvalue_h) $(plchar_h) $(gxgstate_h)\
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plchar.c $(PLO_)plchar.$(OBJ)

# agfa ufst character module.
$(PLOBJ)pluchar.$(OBJ): $(PLSRC)pluchar.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gsbittab_h) $(gschar_h) $(gscoord_h) $(gserrors_h) $(gsimage_h)\
 $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gspath_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxarith_h) $(gxchar_h) $(gxfcache_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpath_h) $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(gxchar_h) $(gxfcache_h) $(plfont_h) $(plvalue_h) $(plchar_h) \
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)pluchar.c $(PLO_)pluchar.$(OBJ)

$(PLOBJ)pldict.$(OBJ): $(PLSRC)pldict.c $(AK) $(memory__h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(pldict_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pldict.c $(PLO_)pldict.$(OBJ)

$(PLOBJ)plht.$(OBJ): $(PLSRC)plht.c  $(stdpre_h) $(plht_h) $(gxdevice_h)\
   $(gsstate_h) $(gxtmap_h) $(gsmemory_h) $(gstypes_h) $(gxht_h) \
   $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plht.c $(PLO_)plht.$(OBJ)

$(PLOBJ)pldraw.$(OBJ): $(PLSRC)pldraw.c $(AK) $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxdevice_h) $(gzstate_h)\
 $(pldraw_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pldraw.c $(PLO_)pldraw.$(OBJ)


#artifex font module.
$(PLOBJ)plfont.$(OBJ): $(PLSRC)plfont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h)\
 $(gschar_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gsmatrix_h) $(gstypes_h) $(gsutil_h)\
 $(gsimage_h) $(gxfont_h) $(gxfont42_h) $(gzstate_h)\
 $(gxfache_h) $(plfont_h) $(plvalue_h) $(plchar_h) \
 $(plfapi_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plfont.c $(PLO_)plfont.$(OBJ)

$(PLOBJ)plfapi.$(OBJ): $(PLSRC)plfapi.c $(plfapi_h) $(gxfapi_h) $(memory__h) \
           $(gsmemory_h) $(gserrors_h) $(gxdevice_h) $(gxfont_h) $(gzstate_h) \
           $(gxchar_h) $(gdebug_h) $(plfont_h) $(gxfapi_h) $(plchar_h) \
           $(gsimage_h) $(gspath_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plfapi.c $(PLO_)plfapi.$(OBJ)

#ufst font module.
$(PLOBJ)plufont.$(OBJ): $(PLSRC)plufont.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h)\
 $(gschar_h) $(gserrors_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h)\
 $(gxfont_h) $(gxfont42_h)\
 $(plfont_h) $(plvalue_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)plufont.c $(PLO_)plufont.$(OBJ)

plftable_h=$(PLSRC)plftable.h

# hack - need ufst included for -DAGFA_FONT_TABLE
$(PLOBJ)plftable.$(OBJ): $(PLSRC)plftable.c $(AK) $(plftable_h)\
  $(ctype__h) $(gstypes_h) $(plfont_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(UFST_INCLUDES) $(PLSRC)plftable.c $(PLO_)plftable.$(OBJ)

$(PLOBJ)pltop.$(OBJ): $(PLSRC)pltop.c $(AK) $(string__h)\
 $(gdebug_h) $(gsnogc_h) $(gsdevice_h) $(gsmemory_h) $(gsstruct_h)\
 $(gstypes_h) $(pltop_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pltop.c $(PLO_)pltop.$(OBJ)

$(PLOBJ)plsymbol.$(OBJ): $(PLSRC)plsymbol.c $(AK) $(stdpre_h)\
 $(std_h) $(gdebug_h) $(plsymbol_h) $(plvocab_h) $(plvalue_h) \
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plsymbol.c $(PLO_)plsymbol.$(OBJ)

$(PLOBJ)plvalue.$(OBJ): $(PLSRC)plvalue.c $(AK) $(std_h)\
 $(plvalue_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plvalue.c $(PLO_)plvalue.$(OBJ)

$(PLOBJ)plvocab.$(OBJ): $(PLSRC)plvocab.c $(AK) $(stdpre_h)\
 $(plvocab_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plvocab.c $(PLO_)plvocab.$(OBJ)

# ufst font loading module.
uconfig_h=$(PLOBJ)uconfig.h

$(uconfig_h): $(PLSRC)pl.mak $(ECHOGS_XE) $(PL_MAK) $(MAKEDIRS)
	$(ECHOGS_XE) -e .h -w $(PLOBJ)uconfig -x 23 "define UFSTFONTDIR" -s -x 22 $(UFSTFONTDIR) -x 22

$(PLOBJ)plulfont.$(OBJ): $(PLSRC)plulfont.c $(pllfont_h) $(uconfig_h) $(AK)\
	$(stdio__h) $(string__h)\
        $(gpgetenv_h) $(gsmemory_h) $(gp_h) $(gstypes_h)\
	$(plfont_h) $(pldict_h) $(pllfont_h) $(plvalue_h)\
	$(plftable_h) $(plvocab_h) $(uconfig_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(UFST_CFLAGS) $(UFST_INCLUDES) $(PLSRC)plulfont.c $(PLO_)plulfont.$(OBJ)

# artifex font loading module.
$(PLOBJ)pllfont.$(OBJ): $(PLSRC)pllfont.c $(pllfont_h) $(AK)\
	$(ctype__h) $(stdio__h) $(string__h) $(strmio_h) $(stream_h)\
	$(gx_h) $(gp_h) $(gsccode_h) $(gserrors_h) $(gsmatrix_h) $(gsutil_h)\
	$(gxfont_h) $(gxfont42_h) $(gxiodev_h) \
        $(plfont_h) $(pldict_h) $(plvalue_h) $(plftable_h) $(plfapi_h) \
        $(gxfapi_h) $(plufstlp_h) $(plvocab_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)pllfont.c $(PLO_)pllfont.$(OBJ)

$(PLOBJ)plapi.$(OBJ): $(PLSRC)plapi.c $(plmain_h) $(plapi_h)\
	$(gsmchunk_h) $(gsmalloc_h) $(gserrors_h) $(gsexit_h)\
         $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plapi.c $(PLO_)plapi.$(OBJ)

pl_obj1=$(PLOBJ)pldict.$(OBJ) $(PLOBJ)plsymbol.$(OBJ) $(PLOBJ)plvalue.$(OBJ) $(PLOBJ)plht.$(OBJ)

# NB plapi is misplaced here.
pl_obj2=$(PLOBJ)plvocab.$(OBJ) $(PLOBJ)pltop.$(OBJ) $(PLOBJ)plapi.$(OBJ)

# shared objects - non font
pl_obj=$(pl_obj1) $(pl_obj2)

# common (afs and ufst systems) font objects
font_common_obj=$(PLOBJ)plchar.$(OBJ) $(PLOBJ)plfont.$(OBJ) $(PLOBJ)plftable.$(OBJ)

# artifex specific objects
afs_obj=$(font_common_obj) $(PLOBJ)pllfont.$(OBJ)

# ufst specific objects
ufst_obj=$(font_common_obj) $(PLOBJ)pluchar.$(OBJ) $(PLOBJ)plufont.$(OBJ) $(PLOBJ)plulfont.$(OBJ)

# artifex font device.
$(PLOBJ)afs.dev: $(ECHOGS_XE) $(afs_obj) $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)afs $(afs_obj)

$(PLOBJ)plufstlp1.$(OBJ): $(PLSRC)plufstlp1.c $(plufstlp_h) $(stdio__h) $(string__h) $(gsmemory_h) \
                                         $(gstypes_h) $(gxfapi_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(UFST_CFLAGS) $(I_)$(UFST_ROOT)$(D)rts$(D)inc$(_I) $(PLSRC)plufstlp1.c $(PLO_)plufstlp1.$(OBJ)

$(PLOBJ)plufstlp.$(OBJ): $(PLSRC)plufstlp.c $(plufstlp_h)$(stdio__h) $(string__h) $(gsmemory_h) \
                                         $(gstypes_h) $(gxfapi_h) $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plufstlp.c $(PLO_)plufstlp.$(OBJ)

# ufst font device.  the libraries are expected to be linked in the
# main platform makefile.
$(PLOBJ)ufst.dev: $(ECHOGS_XE)  $(ufst_obj) $(PLOBJ)plufstlp1.$(OBJ) $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)ufst $(ufst_obj)
	$(ADDMOD) $(PLOBJ)ufst $(PLOBJ)plufstlp1.$(OBJ)

plufstlp_h=$(PLSRC)plufstlp.h $(studio__h) $(string__h) $(gsmemory_h)            $(gstypes_h)

fapi_objs=$(PLOBJ)plfapi.$(OBJ)
$(PLOBJ)fapi_pl.dev: $(ECHOGS_XE) $(fapi_objs) $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)fapi_pl $(fapi_objs)


### BROKEN #####
# Bitstream font device
$(PLOBJ)bfs.dev: $(ECHOGS_XE) $(pl_obj1) $(pl_obj2) $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)bfs $(pl_obj1) $(pl_obj2)
### END BROKEN ###

$(PLOBJ)pl.dev: $(ECHOGS_XE) $(pl_obj) $(PLOBJ)fapi_pl.dev \
                $(PLOBJ)plufstlp$(UFST_BRIDGE).$(OBJ) $(GLOBJ)gsargs.$(OBJ)\
                $(PL_MAK) $(MAKEDIRS)
	$(SETMOD) $(PLOBJ)pl $(pl_obj1)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj2)
	$(ADDMOD) $(PLOBJ)pl $(pl_obj3)
	$(ADDMOD) $(PLOBJ)pl $(PLOBJ)plufstlp$(UFST_BRIDGE).$(OBJ)
	$(ADDMOD) $(PLOBJ)pl $(GLOBJ)gsargs.$(OBJ)
	$(ADDMOD) $(PLOBJ)pl -include $(PLOBJ)fapi_pl
	$(ADDMOD) $(PLOBJ)pl -include $(PLOBJ)$(PL_SCALER)

###### Command-line driver's main program #####

# Almost the top level; provides pl_main
$(PLOBJ)plmain.$(OBJ): $(PLSRC)plmain.c $(AK) $(std_h) $(ctype__h) $(string__h)\
 $(gdebug_h) $(gscdefs_h) $(gsio_h) $(gstypes_h) $(gserrors_h) \
 $(gsmemory_h) $(gsmalloc_h) $(gsmchunk_h) $(gsstruct_h) $(gxalloc_h)\
 $(gsalloc_h) $(gsargs_h) $(gp_h) $(gsdevice_h) $(gslib_h) $(gslibctx_h)\
 $(gxdevice_h) $(gsparam_h) $(pjtop_h) $(plapi_h) $(plparse_h)\
 $(plmain_h) $(pltop_h) $(stream_h) $(strmio_h) $(gsargs_h) $(dwtrace_h) $(vdtrace_h)\
 $(gxclpage_h) $(gdevprn_h) $(gxiodev_h) $(assert__h) $(gserrors_h)\
 $(PL_MAK) $(MAKEDIRS)
	$(PLCCC) $(D_)PJLVERSION=$(PJLVERSION)$(_D) $(PLSRC)plmain.c $(PLO_)plmain.$(OBJ)

# Real top level; provides main that just calls pl_main
# On Windows this also sets up the display device so that we
# can view the output.
$(PLOBJ)$(REALMAIN_SRC).$(OBJ): $(PLSRC)$(REALMAIN_SRC).c $(PL_MAK) $(MAKEDIRS) \
 $(string__h) $(plapi_h) $(gserrors_h)
	$(PLATCCC) $(PLSRC)$(REALMAIN_SRC).c $(PLO_)$(REALMAIN_SRC).$(OBJ)


$(PLOBJ)plwmainc.$(OBJ): $(PLSRC)plwmainc.c $(PL_MAK) $(MAKEDIRS)
	$(PLCCC_W) $(COMPILE_FOR_CONSOLE_EXE) $(PLSRC)plwmainc.c $(PLO_)plwmainc.$(OBJ)

$(PLOBJ)plwimg.$(OBJ): $(PLSRC)plwimg.c $(PL_MAK) $(MAKEDIRS)
	$(PLCCC_W)$(COMPILE_FOR_CONSOLE_EXE)  $(PLSRC)plwimg.c $(PLO_)plwimg.$(OBJ)

$(PLOBJ)plwreg.$(OBJ): $(PLSRC)plwreg.c $(PL_MAK) $(MAKEDIRS)
	$(PLCCC_W) $(COMPILE_FOR_CONSOLE_EXE) $(PLSRC)plwreg.c $(PLO_)plwreg.$(OBJ)

WINPLOBJS=$(PLOBJ)plwimg.$(OBJ) $(PLOBJ)plwreg.$(OBJ)
WINMAINOBJ=$(PLOBJ)plwmainc.$(OBJ)
WINMAINOBJS=$(WINMAINOBJ) $(WINPLOBJS)
DWMAINOBJS=$(WINMAINOBJS) $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj $(GLOBJ)gp_wutf8.obj

$(PLOBJ)plimpl.$(OBJ):  $(PLSRC)plimpl.c            \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)                  \
                        $(PL_MAK)                   \
                        $(MAKEDIRS)
	$(PLCCC) $(PLSRC)plimpl.c $(PLO_)plimpl.$(OBJ)
