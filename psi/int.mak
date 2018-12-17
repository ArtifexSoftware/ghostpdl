# Copyright (C) 2001-2018 Artifex Software, Inc.
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
#
# (Platform-independent) makefile for PostScript and PDF language
# interpreters.
# Users of this makefile must define the following:
#	GLSRCDIR - the graphics library source directory
#	GLGENDIR - the directory for graphics library source files
#		generated during building
#	PSSRCDIR - the source directory
#	PSOBJDIR - the object code directory

PSSRC=$(PSSRCDIR)$(D)
PSLIB=$(PSLIBDIR)$(D)
PSINIT=$(PSRESDIR)$(D)Init$(D)
PSGEN=$(PSGENDIR)$(D)
PSOBJ=$(PSOBJDIR)$(D)
PSO_=$(O_)$(PSOBJ)
PSI_=$(PSSRCDIR) $(II)$(PSGENDIR) $(II)$(GLI_)
PSF_=
PSCC=$(CC_) $(I_)$(PSI_)$(_I) $(PSF_)
PSJBIG2CC=$(CC_) $(I_)$(JB2I_) $(II)$(PSI_)$(_I) $(JB2CF_) $(PSF_)
PSJASCC=$(CC_) $(I_)$(JPXI_) $(II)$(PSI_)$(_I) $(JPXCF_) $(PSF)
PSLDFJB2CC=$(CC_) $(I_)$(LDF_JB2I_) $(II)$(LDF_JB2I_) $(II)$(PSI_)$(_I) $(JB2CF_) $(PSF_)
PSLWFJPXCC=$(CC_) $(I_)$(LWF_JPXI_) $(II)$(PSI_)$(_I) $(JPXCF_) $(PSF)
PSOPJJPXCC=$(CC_) $(I_)$(JPX_OPENJPEG_I_)$(D).. $(I_)$(JPX_OPENJPEG_I_) $(II)$(PSI_)$(_I) $(JPXCF_) $(PSF)

# All top-level makefiles define PSD.
#PSD=$(PSGEN)

# Define the name of this makefile.
INT_MAK=$(PSSRC)int.mak $(TOP_MAKEFILES)

# ======================== Interpreter support ======================== #

# This is support code for all interpreters, not just PostScript and PDF.
# It knows about the PostScript data types, but isn't supposed to
# depend on anything outside itself.

ierrors_h=$(PSSRC)ierrors.h $(gserrors_h)
iconf_h=$(PSSRC)iconf.h $(gxiodev_h) $(stdpre_h)
idebug_h=$(PSSRC)idebug.h $(iref_h) $(std_h)
# Having iddstack.h at this level is unfortunate, but unavoidable.
iddstack_h=$(PSSRC)iddstack.h $(iref_h) $(stdpre_h)
idict_h=$(PSSRC)idict.h $(iddstack_h)
idictdef_h=$(PSSRC)idictdef.h
idicttpl_h=$(PSSRC)idicttpl.h
idosave_h=$(PSSRC)idosave.h $(imemory_h)
igcstr_h=$(PSSRC)igcstr.h $(gxalloc_h)
inames_h=$(PSSRC)inames.h $(iref_h) $(std_h)
iname_h=$(PSSRC)iname.h $(inames_h)
inameidx_h=$(PSSRC)inameidx.h
inamestr_h=$(PSSRC)inamestr.h $(inameidx_h) $(stdpre_h)
ipacked_h=$(PSSRC)ipacked.h
iref_h=$(PSSRC)iref.h $(stdint__h)
isave_h=$(PSSRC)isave.h $(idosave_h) $(gsgstate_h)
isstate_h=$(PSSRC)isstate.h $(gxalloc_h) $(gsgc_h)
istruct_h=$(PSSRC)istruct.h $(gsstruct_h) $(iref_h)
iutil_h=$(PSSRC)iutil.h $(imemory_h)
ivmspace_h=$(PSSRC)ivmspace.h $(gsgc_h) $(iref_h)
opdef_h=$(PSSRC)opdef.h $(iref_h)
# Nested include files
ghost_h=$(PSSRC)ghost.h $(gx_h) $(iref_h)
igc_h=$(PSSRC)igc.h $(istruct_h) $(gxalloc_h) $(imemory_h) $(gsgc_h)
imemory_h=$(PSSRC)imemory.h $(ivmspace_h) $(gsmemory_h) $(gsalloc_h)
ialloc_h=$(PSSRC)ialloc.h $(imemory_h)
iastruct_h=$(PSSRC)iastruct.h $(ialloc_h) $(gxobj_h)
iastate_h=$(PSSRC)iastate.h $(ialloc_h) $(istruct_h) $(gxalloc_h)
inamedef_h=$(PSSRC)inamedef.h $(inames_h) $(inamestr_h) $(inameidx_h) $(gsstruct_h)
store_h=$(PSSRC)store.h $(ialloc_h) $(idosave_h)
iplugin_h=$(PSSRC)iplugin.h
ifapi_h=$(PSSRC)ifapi.h $(iplugin_h) $(gxfapi_h) $(gsmatrix_h) $(memory__h) $(gp_h) $(gstypes_h)
zht2_h=$(PSSRC)zht2.h $(gscspace_h)
gen_ordered_h=$(GLSRC)gen_ordered.h $(stdpre_h)
zchar42_h=$(PSSRC)zchar42.h $(gxfapi_h) $(iref_h)
zfunc_h=$(PSSRC)zfunc.h $(gsfunc_h) $(iref_h)

GH=$(AK) $(ghost_h)

isupport1_=$(PSOBJ)ialloc.$(OBJ) $(PSOBJ)igc.$(OBJ) $(PSOBJ)igcref.$(OBJ) $(PSOBJ)igcstr.$(OBJ)
isupport2_=$(PSOBJ)ilocate.$(OBJ) $(PSOBJ)iname.$(OBJ) $(PSOBJ)isave.$(OBJ)
isupport_=$(isupport1_) $(isupport2_)
$(PSD)isupport.dev : $(ECHOGS_XE) $(isupport_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)isupport $(isupport1_)
	$(ADDMOD) $(PSD)isupport -obj $(isupport2_)

$(PSOBJ)ialloc.$(OBJ) : $(PSSRC)ialloc.c $(AK) $(memory__h) $(gx_h)\
 $(ierrors_h) $(gsstruct_h)\
 $(iastate_h) $(igc_h) $(ipacked_h) $(iref_h) $(iutil_h) $(ivmspace_h)\
 $(store_h) $(gsexit_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ialloc.$(OBJ) $(C_) $(PSSRC)ialloc.c

# igc.c, igcref.c, and igcstr.c should really be in the dpsand2 list,
# but since all the GC enumeration and relocation routines refer to them,
# it's too hard to separate them out from the Level 1 base.
$(PSOBJ)igc.$(OBJ) : $(PSSRC)igc.c $(GH) $(memory__h)\
 $(ierrors_h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(inamedef_h)\
 $(ipacked_h) $(isave_h) $(isstate_h) $(istruct_h) $(opdef_h) \
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)igc.$(OBJ) $(C_) $(PSSRC)igc.c

$(PSOBJ)igcref.$(OBJ) : $(PSSRC)igcref.c $(GH) $(memory__h)\
 $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idebug_h) $(igc_h) $(iname_h) $(ipacked_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)igcref.$(OBJ) $(C_) $(PSSRC)igcref.c

$(PSOBJ)igcstr.$(OBJ) : $(PSSRC)igcstr.c $(GH) $(memory__h)\
 $(gsmdebug_h) $(gsstruct_h) $(iastate_h) $(igcstr_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)igcstr.$(OBJ) $(C_) $(PSSRC)igcstr.c

$(PSOBJ)ilocate.$(OBJ) : $(PSSRC)ilocate.c $(GH) $(memory__h)\
 $(ierrors_h) $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(iname_h)\
 $(ipacked_h) $(isstate_h) $(iutil_h) $(ivmspace_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ilocate.$(OBJ) $(C_) $(PSSRC)ilocate.c

$(PSOBJ)iname.$(OBJ) : $(PSSRC)iname.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h) $(gxobj_h)\
 $(ierrors_h) $(imemory_h) $(inamedef_h) $(isave_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iname.$(OBJ) $(C_) $(PSSRC)iname.c

$(PSOBJ)isave.$(OBJ) : $(PSSRC)isave.c $(GH) $(memory__h)\
 $(ierrors_h) $(gsexit_h) $(gsstruct_h) $(gsutil_h)\
 $(iastate_h) $(iname_h) $(inamedef_h) $(isave_h) $(isstate_h) $(ivmspace_h)\
 $(ipacked_h) $(store_h) $(stream_h) $(igc_h) $(icstate_h) $(gsstate_h) \
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)isave.$(OBJ) $(C_) $(PSSRC)isave.c

### Include files

idparam_h=$(PSSRC)idparam.h $(iref_h) $(stdpre_h)
ilevel_h=$(PSSRC)ilevel.h $(imemory_h)
interp_h=$(PSSRC)interp.h $(imemory_h)
iparam_h=$(PSSRC)iparam.h $(imemory_h) $(gsparam_h) $(isdata_h)
isdata_h=$(PSSRC)isdata.h $(iref_h)
istack_h=$(PSSRC)istack.h $(isdata_h)
istkparm_h=$(PSSRC)istkparm.h $(gsstruct_h) $(iref_h) $(stdpre_h)
iutil2_h=$(PSSRC)iutil2.h $(gsfunc_h) $(iref_h) $(stdpre_h)
oparc_h=$(PSSRC)oparc.h $(iref_h)
opcheck_h=$(PSSRC)opcheck.h $(iref_h)
opextern_h=$(PSSRC)opextern.h $(iref_h)
# Nested include files
idsdata_h=$(PSSRC)idsdata.h $(isdata_h)
idstack_h=$(PSSRC)idstack.h $(idsdata_h) $(iddstack_h) $(istack_h)
iesdata_h=$(PSSRC)iesdata.h $(isdata_h)
iestack_h=$(PSSRC)iestack.h $(iesdata_h) $(istack_h)
iosdata_h=$(PSSRC)iosdata.h $(isdata_h)
iostack_h=$(PSSRC)iostack.h $(iosdata_h) $(istack_h)
icstate_h=$(PSSRC)icstate.h $(idsdata_h) $(stream_h) $(iesdata_h) $(opdef_h) $(iosdata_h) $(imemory_h) $(iref_h) $(gsgstate_h)
iddict_h=$(PSSRC)iddict.h $(icstate_h) $(idict_h)
dstack_h=$(PSSRC)dstack.h $(idstack_h) $(icstate_h)
estack_h=$(PSSRC)estack.h $(icstate_h) $(iestack_h)
ostack_h=$(PSSRC)ostack.h $(icstate_h) $(iostack_h)
oper_h=$(PSSRC)oper.h $(ostack_h) $(opdef_h) $(ierrors_h) $(iutil_h) $(opcheck_h) $(opextern_h)

$(PSOBJ)idebug.$(OBJ) : $(PSSRC)idebug.c $(GH) $(string__h)\
 $(gxalloc_h)\
 $(idebug_h) $(idict_h) $(iname_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opdef_h) $(ipacked_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)idebug.$(OBJ) $(C_) $(PSSRC)idebug.c

$(PSOBJ)idict.$(OBJ) : $(PSSRC)idict.c $(GH) $(math__h) $(string__h)\
 $(ierrors_h)\
 $(gxalloc_h)\
 $(iddstack_h) $(idebug_h) $(idict_h) $(idictdef_h) $(idicttpl_h)\
 $(imemory_h) $(iname_h) $(inamedef_h) $(ipacked_h) $(isave_h)\
 $(iutil_h) $(ivmspace_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)idict.$(OBJ) $(C_) $(PSSRC)idict.c

$(PSOBJ)idparam.$(OBJ) : $(PSSRC)idparam.c $(GH) $(memory__h) $(string__h) $(ierrors_h)\
 $(gsmatrix_h) $(gsuid_h) $(dstack_h)\
 $(idict_h) $(iddict_h) $(idparam_h) $(ilevel_h) $(imemory_h) $(iname_h) $(iutil_h)\
 $(oper_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)idparam.$(OBJ) $(C_) $(PSSRC)idparam.c

$(PSOBJ)idstack.$(OBJ) : $(PSSRC)idstack.c $(GH)\
 $(idebug_h) $(idict_h) $(idictdef_h) $(idicttpl_h) $(idstack_h) $(iname_h) $(inamedef_h)\
 $(ipacked_h) $(iutil_h) $(ivmspace_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)idstack.$(OBJ) $(C_) $(PSSRC)idstack.c

$(PSOBJ)iparam.$(OBJ) : $(PSSRC)iparam.c $(GH)\
 $(memory__h) $(string__h) $(ierrors_h)\
 $(ialloc_h) $(idict_h) $(iname_h) $(imemory_h) $(iparam_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opcheck_h) $(oper_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iparam.$(OBJ) $(C_) $(PSSRC)iparam.c

$(PSOBJ)istack.$(OBJ) : $(PSSRC)istack.c $(GH) $(memory__h)\
 $(ierrors_h) $(gsstruct_h) $(gsutil_h)\
 $(ialloc_h) $(istack_h) $(istkparm_h) $(istruct_h) $(iutil_h) $(ivmspace_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)istack.$(OBJ) $(C_) $(PSSRC)istack.c

$(PSOBJ)iutil.$(OBJ) : $(PSSRC)iutil.c $(GH) $(math__h) $(memory__h) $(string__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h)\
 $(sstring_h) $(strimpl_h)\
 $(ierrors_h) $(idict_h) $(imemory_h) $(iutil_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(oper_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iutil.$(OBJ) $(C_) $(PSSRC)iutil.c

$(PSOBJ)iplugin.$(OBJ) : $(PSSRC)iplugin.c $(GH) $(malloc__h) $(string__h)\
 $(gxalloc_h)\
 $(ierrors_h) $(ialloc_h) $(icstate_h) $(iplugin_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iplugin.$(OBJ) $(C_) $(PSSRC)iplugin.c

# ======================== PostScript Level 1 ======================== #

###### Include files

# Binary tokens are a Level 2 feature, but we need to refer to them
# in the scanner.
btoken_h=$(PSSRC)btoken.h $(iref_h)
files_h=$(PSSRC)files.h $(store_h) $(stream_h)
fname_h=$(PSSRC)fname.h
psapi_h=$(PSSRC)psapi.h $(gsdevice_h) $(gsmemory_h)
iapi_h=$(PSSRC)iapi.h
ichar_h=$(PSSRC)ichar.h $(iostack_h) $(gxfapi_h)
ichar1_h=$(PSSRC)ichar1.h $(gxfont_h) $(gsgdata_h) $(gxfapi_h) $(iref_h)
icharout_h=$(PSSRC)icharout.h $(gsgdata_h) $(gxfapi_h) $(iref_h)
icolor_h=$(PSSRC)icolor.h $(gxtmap_h) $(iref_h) $(gsgstate_h)
icremap_h=$(PSSRC)icremap.h $(iref_h) $(gsccolor_h)
icsmap_h=$(PSSRC)icsmap.h $(gscspace_h) $(iref_h)
idisp_h=$(PSSRC)idisp.h $(imain_h)
ifilter2_h=$(PSSRC)ifilter2.h $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(iostack_h)
ifont_h=$(PSSRC)ifont.h $(gxfont_h) $(iref_h)
ifont1_h=$(PSSRC)ifont1.h $(gxfont1_h) $(ichar1_h) $(bfont_h)
ifont2_h=$(PSSRC)ifont2.h $(ifont1_h)
ifont42_h=$(PSSRC)ifont42.h $(gxfont42_h) $(bfont_h) $(iostack_h) $(gxftype_h) $(gsmemory_h)
ifrpred_h=$(PSSRC)ifrpred.h $(scommon_h) $(iref_h)
ifwpred_h=$(PSSRC)ifwpred.h $(scommon_h) $(iref_h)
iht_h=$(PSSRC)iht.h $(gxht_h) $(iostack_h) $(gsgstate_h)
iimage_h=$(PSSRC)iimage.h $(gsiparam_h) $(iref_h)
iinit_h=$(PSSRC)iinit.h $(imemory_h)
imain_h=$(PSSRC)imain.h $(gsexit_h) $(iref_h) $(gstypes_h)
imainarg_h=$(PSSRC)imainarg.h $(std_h)
iminst_h=$(PSSRC)iminst.h $(iref_h)
iparray_h=$(PSSRC)iparray.h $(imemory_h) $(isdata_h)
iscanbin_h=$(PSSRC)iscanbin.h $(iscan_h)
iscannum_h=$(PSSRC)iscannum.h $(imemory_h) $(stdpre_h)
istream_h=$(PSSRC)istream.h $(imemory_h) $(scommon_h)
itoken_h=$(PSSRC)itoken.h $(iref_h)
main_h=$(PSSRC)main.h $(imain_h) $(iminst_h) $(iapi_h)
sbwbs_h=$(PSSRC)sbwbs.h
shcgen_h=$(PSSRC)shcgen.h
smtf_h=$(GLSRC)smtf.h $(scommon_h)
# Nested include files
bfont_h=$(PSSRC)bfont.h $(ifont_h) $(iostack_h) $(imemory_h)
icontext_h=$(PSSRC)icontext.h $(icstate_h) $(gsstype_h)
ifilter_h=$(PSSRC)ifilter.h $(istream_h) $(ivmspace_h)
igstate_h=$(PSSRC)igstate.h $(gxstate_h) $(gsstate_h) $(istruct_h) $(imemory_h) $(gxcindex_h)
iscan_h=$(PSSRC)iscan.h $(sstring_h) $(sa85x_h) $(inamestr_h) $(iref_h)
sbhc_h=$(PSSRC)sbhc.h $(shc_h)
zfile_h=$(PSSRC)zfile.h $(gsfname_h) $(iref_h)
# Include files for optional features
ibnum_h=$(PSSRC)ibnum.h $(iref_h) $(stdpre_h)
zcolor_h=$(PSSRC)zcolor.h $(iref_h)
zcie_h=$(PSSRC)zcie.h $(iref_h)
zicc_h=$(PSSRC)zicc.h $(iref_h)
zfrsd_h=$(PSSRC)zfrsd.h $(iostack_h)

### Initialization and scanning

$(PSOBJ)iconfig.$(OBJ) : $(gconfig_h) $(PSSRC)iconf.c $(stdio__h)\
 $(gconf_h) $(gconfigd_h) $(gsmemory_h) $(gstypes_h)\
 $(iminst_h) $(iref_h) $(ivmspace_h) $(opdef_h) $(iplugin_h)\
 $(gs_tr) $(INT_MAK) $(MAKEDIRS)
	$(RM_) $(PSGEN)iconfig.c
	$(CP_) $(PSSRC)iconf.c $(PSGEN)iconfig.c
	$(PSCC) $(PSO_)iconfig.$(OBJ) $(C_) $(PSGEN)iconfig.c

$(PSOBJ)iinit.$(OBJ) : $(PSSRC)iinit.c $(GH) $(string__h)\
 $(gscdefs_h) $(gsexit_h) $(gsstruct_h)\
 $(dstack_h) $(ierrors_h) $(ialloc_h) $(iddict_h)\
 $(iinit_h) $(ilevel_h) $(iname_h) $(interp_h) $(opdef_h)\
 $(ipacked_h) $(iparray_h) $(iutil_h) $(ivmspace_h) \
 $(gxiodev_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iinit.$(OBJ) $(C_) $(PSSRC)iinit.c

$(PSOBJ)iscan.$(OBJ) : $(PSSRC)iscan.c $(GH) $(memory__h)\
 $(btoken_h) $(dstack_h) $(ierrors_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(iparray_h)\
 $(iscan_h) $(iscanbin_h) $(iscannum_h)\
 $(istruct_h) $(istream_h) $(iutil_h) $(ivmspace_h)\
 $(ostack_h) $(store_h)\
 $(sa85d_h) $(stream_h) $(strimpl_h) $(sfilter_h) $(scanchar_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iscan.$(OBJ) $(C_) $(PSSRC)iscan.c

$(PSOBJ)iscannum.$(OBJ) : $(PSSRC)iscannum.c $(GH) $(math__h)\
 $(ierrors_h) $(iscan_h) $(iscannum_h) $(scanchar_h) $(scommon_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iscannum.$(OBJ) $(C_) $(PSSRC)iscannum.c

###### Operators

OP=$(GH) $(oper_h)

### Non-graphics operators

$(PSOBJ)zarith.$(OBJ) : $(PSSRC)zarith.c $(OP) $(math__h) $(store_h)\
 $(gsstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zarith.$(OBJ) $(C_) $(PSSRC)zarith.c

$(PSOBJ)zarray.$(OBJ) : $(PSSRC)zarray.c $(OP) $(memory__h)\
 $(ialloc_h) $(ipacked_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zarray.$(OBJ) $(C_) $(PSSRC)zarray.c

$(PSOBJ)zcontrol.$(OBJ) : $(PSSRC)zcontrol.c $(OP) $(string__h)\
 $(estack_h) $(files_h) $(ipacked_h) $(iutil_h) $(store_h) $(stream_h)\
 $(interp_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcontrol.$(OBJ) $(C_) $(PSSRC)zcontrol.c

$(PSOBJ)zdict.$(OBJ) : $(PSSRC)zdict.c $(OP)\
 $(dstack_h) $(iddict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(ivmspace_h)\
 $(store_h) $(iscan_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdict.$(OBJ) $(C_) $(PSSRC)zdict.c

$(PSOBJ)zfile.$(OBJ) : $(PSSRC)zfile.c $(OP)\
 $(memory__h) $(string__h) $(unistd__h) $(stat__h) $(gp_h) $(gpmisc_h)\
 $(gscdefs_h) $(gsfname_h) $(gsstruct_h) $(gsutil_h) $(gxalloc_h) $(gxiodev_h)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(iddict_h) $(ilevel_h) $(iname_h) $(iutil_h)\
 $(isave_h) $(main_h) $(sfilter_h) $(stream_h) $(strimpl_h) $(store_h)\
 $(zfile_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfile.$(OBJ) $(C_) $(PSSRC)zfile.c

$(PSOBJ)zfile1.$(OBJ) : $(PSSRC)zfile1.c $(OP) $(memory__h) $(string__h)\
 $(gp_h) $(ierrors_h) $(oper_h) $(opcheck_h) $(ialloc_h) $(opdef_h) $(store_h)\
 $(gpmisc_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfile1.$(OBJ) $(C_) $(PSSRC)zfile1.c

$(PSOBJ)zfileio.$(OBJ) : $(PSSRC)zfileio.c $(OP) $(memory__h) $(gp_h)\
 $(estack_h) $(files_h) $(ifilter_h) $(interp_h) $(store_h)\
 $(stream_h) $(strimpl_h)\
 $(gsmatrix_h) $(gxdevice_h) $(gxdevmem_h) $(gsstate_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfileio.$(OBJ) $(C_) $(PSSRC)zfileio.c

$(PSOBJ)zfilter.$(OBJ) : $(PSSRC)zfilter.c $(OP) $(memory__h)\
 $(gsstruct_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(ilevel_h)\
 $(sfilter_h) $(srlx_h) $(sstring_h) $(stream_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfilter.$(OBJ) $(C_) $(PSSRC)zfilter.c

$(PSOBJ)zfproc.$(OBJ) : $(PSSRC)zfproc.c $(GH) $(memory__h) $(stat__h)\
 $(oper_h)\
 $(estack_h) $(files_h) $(gsstruct_h) $(ialloc_h) $(ifilter_h) $(istruct_h)\
 $(store_h) $(stream_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfproc.$(OBJ) $(C_) $(PSSRC)zfproc.c

$(PSOBJ)zgeneric.$(OBJ) : $(PSSRC)zgeneric.c $(OP) $(memory__h)\
 $(gsstruct_h)\
 $(dstack_h) $(estack_h) $(iddict_h) $(iname_h) $(ipacked_h) $(ivmspace_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zgeneric.$(OBJ) $(C_) $(PSSRC)zgeneric.c

$(PSOBJ)ziodev.$(OBJ) : $(PSSRC)ziodev.c $(OP)\
 $(memory__h) $(stdio__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gxiodev_h)\
 $(files_h) $(ialloc_h) $(iscan_h) $(ivmspace_h)\
 $(scanchar_h) $(store_h) $(stream_h) $(istream_h) $(ierrors_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ziodev.$(OBJ) $(C_) $(PSSRC)ziodev.c

$(PSOBJ)ziodevsc.$(OBJ) : $(PSSRC)ziodevsc.c $(OP) $(stdio__h)\
 $(gpcheck_h)\
 $(gxiodev_h)\
 $(files_h) $(ifilter_h) $(istream_h) $(store_h) $(stream_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ziodevsc.$(OBJ) $(C_) $(PSSRC)ziodevsc.c

$(PSOBJ)zmath.$(OBJ) : $(PSSRC)zmath.c $(OP) $(math__h) $(gxfarith_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmath.$(OBJ) $(C_) $(PSSRC)zmath.c

$(PSOBJ)zalg.$(OBJ) : $(PSSRC)zalg.c $(OP) $(ghost_h) $(gserrors_h)\
 $(oper_h) $(store_h) $(estack_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zalg.$(OBJ) $(C_) $(PSSRC)zalg.c

$(PSOBJ)zmisc.$(OBJ) : $(PSSRC)zmisc.c $(OP) $(gscdefs_h) $(gp_h)\
 $(errno__h) $(memory__h) $(string__h) $(iscan_h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(iname_h) $(ivmspace_h) $(ipacked_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmisc.$(OBJ) $(C_) $(PSSRC)zmisc.c

$(PSOBJ)zpacked.$(OBJ) : $(PSSRC)zpacked.c $(OP)\
 $(ialloc_h) $(idict_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(iparray_h)\
 $(istack_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpacked.$(OBJ) $(C_) $(PSSRC)zpacked.c

$(PSOBJ)zrelbit.$(OBJ) : $(PSSRC)zrelbit.c $(OP)\
 $(gsutil_h) $(store_h) $(idict_h) $(gsstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zrelbit.$(OBJ) $(C_) $(PSSRC)zrelbit.c

$(PSOBJ)zstack.$(OBJ) : $(PSSRC)zstack.c $(OP) $(memory__h)\
 $(ialloc_h) $(istack_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zstack.$(OBJ) $(C_) $(PSSRC)zstack.c

$(PSOBJ)zstring.$(OBJ) : $(PSSRC)zstring.c $(OP) $(memory__h)\
 $(gsutil_h)\
 $(ialloc_h) $(iname_h) $(ivmspace_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zstring.$(OBJ) $(C_) $(PSSRC)zstring.c

$(PSOBJ)zsysvm.$(OBJ) : $(PSSRC)zsysvm.c $(GH)\
 $(ialloc_h) $(ivmspace_h) $(oper_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zsysvm.$(OBJ) $(C_) $(PSSRC)zsysvm.c

$(PSOBJ)ztoken.$(OBJ) : $(PSSRC)ztoken.c $(OP) $(string__h) $(stat__h)\
 $(gsstruct_h) $(gsutil_h)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(idict_h) $(iname_h) $(iscan_h) $(itoken_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ztoken.$(OBJ) $(C_) $(PSSRC)ztoken.c

$(PSOBJ)ztype.$(OBJ) : $(PSSRC)ztype.c $(OP)\
 $(math__h) $(memory__h) $(string__h)\
 $(gsexit_h)\
 $(dstack_h) $(idict_h) $(imemory_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ztype.$(OBJ) $(C_) $(PSSRC)ztype.c

$(PSOBJ)zvmem.$(OBJ) : $(PSSRC)zvmem.c $(OP) $(stat__h)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(isave_h) $(store_h) $(stream_h)\
 $(gsmalloc_h) $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zvmem.$(OBJ) $(C_) $(PSSRC)zvmem.c

### Graphics operators

$(PSOBJ)zbfont.$(OBJ) : $(PSSRC)zbfont.c $(OP) $(memory__h) $(string__h)\
 $(gscencs_h) $(gsmatrix_h) $(gxdevice_h) $(gxfixed_h) $(gxfont_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ilevel_h)\
 $(iname_h) $(inamedef_h) $(interp_h) $(istruct_h) $(ipacked_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zbfont.$(OBJ) $(C_) $(PSSRC)zbfont.c

$(PSOBJ)zchar.$(OBJ) : $(PSSRC)zchar.c $(OP)\
 $(gsstruct_h) $(gstext_h) $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gxfont_h) $(gxfont42_h) $(gxfont0_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ialloc_h) $(ichar_h)  $(ichar1_h) $(idict_h) $(ifont_h)\
 $(ilevel_h) $(iname_h) $(igstate_h) $(ipacked_h) $(store_h) $(zchar42_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zchar.$(OBJ) $(C_) $(PSSRC)zchar.c

# zcharout is used for Type 1 and Type 42 fonts only.
$(PSOBJ)zcharout.$(OBJ) : $(PSSRC)zcharout.c $(OP) $(memory__h)\
 $(gscrypt1_h) $(gstext_h) $(gxdevice_h) $(gxfont_h) $(gxfont1_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(iname_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcharout.$(OBJ) $(C_) $(PSSRC)zcharout.c

$(PSOBJ)zcolor.$(OBJ) : $(PSSRC)zcolor.c $(OP)\
 $(memory__h) $(math__h) $(dstack_h) $(estack_h) $(ialloc_h)\
 $(igstate_h) $(iutil_h) $(store_h) $(gscolor_h)\
 $(gscsepr_h) $(gscdevn_h) $(gscpixel_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxcmap_h)\
 $(gxcspace_h) $(gxcolor2_h) $(gxpcolor_h)\
 $(idict_h) $(icolor_h) $(idparam_h) $(iname_h) $(iutil_h) $(icsmap_h)\
 $(ifunc_h) $(zht2_h) $(zcolor_h) $(zcie_h) $(zicc_h) $(gscspace_h)\
 $(zfrsd_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcolor.$(OBJ) $(C_) $(PSSRC)zcolor.c

$(PSOBJ)zdevice.$(OBJ) : $(PSSRC)zdevice.c $(OP) $(string__h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(interp_h) $(iparam_h) $(ivmspace_h)\
 $(gsmatrix_h) $(gsstate_h) $(gxdevice_h) $(gxgetbit_h) $(store_h) $(gsicc_manage_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdevice.$(OBJ) $(C_) $(PSSRC)zdevice.c

$(PSOBJ)zfont.$(OBJ) : $(PSSRC)zfont.c $(OP)\
 $(gsstruct_h) $(gxdevice_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)\
 $(ialloc_h) $(iddict_h) $(igstate_h) $(iname_h) $(isave_h) $(ivmspace_h)\
 $(bfont_h) $(store_h) $(gscencs_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont.$(OBJ) $(C_) $(PSSRC)zfont.c

$(PSOBJ)zfontenum.$(OBJ) : $(PSSRC)zfontenum.c $(OP)\
 $(memory__h) $(gsstruct_h) $(ialloc_h) $(idict_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfontenum.$(OBJ) $(C_) $(PSSRC)zfontenum.c

$(PSOBJ)zgstate.$(OBJ) : $(PSSRC)zgstate.c $(OP) $(math__h)\
 $(gsmatrix_h)\
 $(ialloc_h) $(icremap_h) $(idict_h) $(igstate_h) $(istruct_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zgstate.$(OBJ) $(C_) $(PSSRC)zgstate.c

$(PSOBJ)zht.$(OBJ) : $(PSSRC)zht.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zht.$(OBJ) $(C_) $(PSSRC)zht.c

$(PSOBJ)zimage.$(OBJ) : $(PSSRC)zimage.c $(OP) $(math__h) $(memory__h) $(stat__h)\
 $(gscspace_h) $(gscssub_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxiparam_h)\
 $(estack_h) $(ialloc_h) $(ifilter_h) $(igstate_h) $(iimage_h) $(ilevel_h)\
 $(store_h) $(stream_h) $(gxcspace_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zimage.$(OBJ) $(C_) $(PSSRC)zimage.c

$(PSOBJ)zmatrix.$(OBJ) : $(PSSRC)zmatrix.c $(OP)\
 $(gsmatrix_h) $(igstate_h) $(gscoord_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmatrix.$(OBJ) $(C_) $(PSSRC)zmatrix.c

$(PSOBJ)zpaint.$(OBJ) : $(PSSRC)zpaint.c $(OP)\
 $(gspaint_h) $(igstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpaint.$(OBJ) $(C_) $(PSSRC)zpaint.c

$(PSOBJ)zpath.$(OBJ) : $(PSSRC)zpath.c $(OP) $(math__h)\
 $(gsmatrix_h) $(gspath_h) $(igstate_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpath.$(OBJ) $(C_) $(PSSRC)zpath.c

# Define the base PostScript language interpreter.
# This is the subset of PostScript Level 1 required by our PDF reader.

INTAPI=$(PSOBJ)iapi.$(OBJ)
INT1=$(PSOBJ)psapi.$(OBJ) $(PSOBJ)icontext.$(OBJ) $(PSOBJ)idebug.$(OBJ)
INT2=$(PSOBJ)idict.$(OBJ) $(PSOBJ)idparam.$(OBJ) $(PSOBJ)idstack.$(OBJ)
INT3=$(PSOBJ)iinit.$(OBJ) $(PSOBJ)interp.$(OBJ)
INT4=$(PSOBJ)iparam.$(OBJ) $(PSOBJ)ireclaim.$(OBJ) $(PSOBJ)iplugin.$(OBJ)
INT5=$(PSOBJ)iscan.$(OBJ) $(PSOBJ)iscannum.$(OBJ) $(PSOBJ)istack.$(OBJ)
INT6=$(PSOBJ)iutil.$(OBJ) $(GLOBJ)scantab.$(OBJ)
INT7=$(GLOBJ)sstring.$(OBJ) $(GLOBJ)stream.$(OBJ)
Z1=$(PSOBJ)zarith.$(OBJ) $(PSOBJ)zarray.$(OBJ) $(PSOBJ)zcontrol.$(OBJ)
Z2=$(PSOBJ)zdict.$(OBJ) $(PSOBJ)zfile.$(OBJ) $(PSOBJ)zfile1.$(OBJ) $(PSOBJ)zfileio.$(OBJ)
Z3=$(PSOBJ)zfilter.$(OBJ) $(PSOBJ)zfproc.$(OBJ) $(PSOBJ)zgeneric.$(OBJ)
Z4=$(PSOBJ)ziodev.$(OBJ) $(PSOBJ)ziodevsc.$(OBJ) $(PSOBJ)zmath.$(OBJ) $(PSOBJ)zalg.$(OBJ)
Z5=$(PSOBJ)zmisc.$(OBJ) $(PSOBJ)zpacked.$(OBJ) $(PSOBJ)zrelbit.$(OBJ)
Z6=$(PSOBJ)zstack.$(OBJ) $(PSOBJ)zstring.$(OBJ) $(PSOBJ)zsysvm.$(OBJ)
Z7=$(PSOBJ)ztoken.$(OBJ) $(PSOBJ)ztype.$(OBJ) $(PSOBJ)zvmem.$(OBJ)
Z8=$(PSOBJ)zbfont.$(OBJ) $(PSOBJ)zchar.$(OBJ) $(PSOBJ)zcolor.$(OBJ)
Z9=$(PSOBJ)zdevice.$(OBJ) $(PSOBJ)zfont.$(OBJ) $(PSOBJ)zfontenum.$(OBJ) $(PSOBJ)zgstate.$(OBJ)
Z10=$(PSOBJ)zht.$(OBJ) $(PSOBJ)zimage.$(OBJ) $(PSOBJ)zmatrix.$(OBJ)
Z11=$(PSOBJ)zpaint.$(OBJ) $(PSOBJ)zpath.$(OBJ)
Z12=$(PSOBJ)zncdummy.$(OBJ)
Z1OPS=zarith zarray zcontrol1 zcontrol2 zcontrol3
Z2OPS=zdict1 zdict2 zfile zfile1 zfileio1 zfileio2
Z3_4OPS=zfilter zfproc zgeneric ziodev zmath zalg
Z5_6OPS=zmisc_a zmisc_b zpacked zrelbit zstack zstring zsysvm
Z7_8OPS=ztoken ztype zvmem zbfont zchar_a zchar_b zcolor zcolor_ext
Z9OPS=zdevice zdevice_ext zfont zfontenum zgstate1 zgstate2 zgstate3 zgstate4
Z10OPS=zht zimage zmatrix zmatrix2
Z11OPS=zpaint zpath pantone zcolor_pdf
# We have to be a little underhanded with *config.$(OBJ) so as to avoid
# circular definitions.
INT_MAIN=$(PSOBJ)imain.$(OBJ) $(PSOBJ)imainarg.$(OBJ) $(GLOBJ)gsargs.$(OBJ) $(PSOBJ)idisp.$(OBJ)
INT_OBJS=$(INT_MAIN)\
 $(INT1) $(INT2) $(INT3) $(INT4) $(INT5) $(INT6) $(INT7)\
 $(Z1) $(Z2) $(Z3) $(Z4) $(Z5) $(Z6) $(Z7) $(Z8) $(Z9) $(Z10) $(Z11) $(Z12)
INT_CONFIG=$(GLOBJ)gconfig.$(OBJ) $(GLOBJ)gscdefs.$(OBJ)\
 $(PSOBJ)iconfig.$(OBJ)
INT_ALL=$(INT_OBJS) $(INT_CONFIG)
# We omit libcore.dev, which should be included here, because problems
# with the Unix linker require libcore to appear last in the link list
# when libcore is really a library.
# We omit $(INT_CONFIG) from the dependency list because they have special
# dependency requirements and are added to the link list at the very end.
# zfilter.c shouldn't include the RLE and RLD filters, but we don't want to
# change this now.
#
# We add dscparse.dev here since it can be used with any PS level even
# though we don't strictly need it unless we have the pdfwrite device.
$(PSD)psbase.dev : $(ECHOGS_XE) $(INT_OBJS)\
 $(PSD)isupport.dev $(PSD)nobtoken.dev $(PSD)nousparm.dev\
 $(GLD)rld.dev $(GLD)rle.dev $(GLD)sfile.dev $(PSD)dscparse.dev \
 $(PSD)fapi_ps.dev  $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psbase $(INT_MAIN)
	$(ADDMOD) $(PSD)psbase -obj $(INT_CONFIG)
	$(ADDMOD) $(PSD)psbase -obj $(INT1)
	$(ADDMOD) $(PSD)psbase -obj $(INT2)
	$(ADDMOD) $(PSD)psbase -obj $(INT3)
	$(ADDMOD) $(PSD)psbase -obj $(INT4)
	$(ADDMOD) $(PSD)psbase -obj $(INT5)
	$(ADDMOD) $(PSD)psbase -obj $(INT6)
	$(ADDMOD) $(PSD)psbase -obj $(INT7)
	$(ADDMOD) $(PSD)psbase -obj $(Z1)
	$(ADDMOD) $(PSD)psbase -obj $(Z2)
	$(ADDMOD) $(PSD)psbase -obj $(Z3)
	$(ADDMOD) $(PSD)psbase -obj $(Z4)
	$(ADDMOD) $(PSD)psbase -obj $(Z5)
	$(ADDMOD) $(PSD)psbase -obj $(Z6)
	$(ADDMOD) $(PSD)psbase -obj $(Z7)
	$(ADDMOD) $(PSD)psbase -obj $(Z8)
	$(ADDMOD) $(PSD)psbase -obj $(Z9)
	$(ADDMOD) $(PSD)psbase -obj $(Z10)
	$(ADDMOD) $(PSD)psbase -obj $(Z11)
	$(ADDMOD) $(PSD)psbase -obj $(Z12)
	$(ADDMOD) $(PSD)psbase -oper $(Z1OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z2OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z3_4OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z5_6OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z7_8OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z9OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z10OPS)
	$(ADDMOD) $(PSD)psbase -oper $(Z11OPS)
	$(ADDMOD) $(PSD)psbase -iodev stdin stdout stderr lineedit statementedit
	$(ADDMOD) $(PSD)psbase -include $(PSD)isupport $(PSD)nobtoken $(PSD)nousparm
	$(ADDMOD) $(PSD)psbase -include $(GLD)rld $(GLD)rle $(GLD)sfile $(PSD)dscparse
	$(ADDMOD) $(PSD)psbase -include $(GLD)fapi_ps
	$(ADDMOD) $(PSD)psbase -replace $(GLD)gsiodevs

$(PSD)iapi.dev : $(ECHOGS_XE) $(INT_OBJS)\
 $(PSD)isupport.dev $(PSD)nobtoken.dev $(PSD)nousparm.dev\
 $(GLD)rld.dev $(GLD)rle.dev $(GLD)sfile.dev $(PSD)dscparse.dev \
 $(PSD)fapi_ps.dev $(INTAPI) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)iapi $(INTAPI)

# -------------------------- Feature definitions -------------------------- #

# ---------------- Full Level 1 interpreter ---------------- #

# We keep the old name for backward compatibility.
$(PSD)level1.dev : $(PSD)psl1.dev $(INT_MAK) $(MAKEDIRS)
	$(CP_) $(PSD)psl1.dev $(PSD)level1.dev

$(PSD)psl1.dev : $(ECHOGS_XE)\
 $(PSD)psbase.dev $(PSD)bcp.dev $(PSD)path1.dev $(PSD)type1.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl1 -include $(PSD)psbase $(PSD)bcp $(PSD)path1 $(PSD)type1
	$(ADDMOD) $(PSD)psl1 -emulator PostScript PostScriptLevel1

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

$(PSD)color.dev : $(ECHOGS_XE) $(GLD)cmyklib.dev $(GLD)colimlib.dev\
 $(PSD)cmykread.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)color -include $(GLD)cmyklib $(GLD)colimlib $(PSD)cmykread

cmykread_=$(PSOBJ)zcolor1.$(OBJ) $(PSOBJ)zht1.$(OBJ)
$(PSD)cmykread.dev : $(ECHOGS_XE) $(cmykread_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cmykread $(cmykread_)
	$(ADDMOD) $(PSD)cmykread -oper zcolor1 zht1

$(PSOBJ)zcolor1.$(OBJ) : $(PSSRC)zcolor1.c $(OP)\
 $(gscolor1_h) $(gscssub_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(ialloc_h) $(icolor_h) $(iimage_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcolor1.$(OBJ) $(C_) $(PSSRC)zcolor1.c

$(PSOBJ)zht1.$(OBJ) : $(PSSRC)zht1.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zht1.$(OBJ) $(C_) $(PSSRC)zht1.c

# ---------------- DSC Parser ---------------- #

# The basic DSC parsing facility, used both for Orientation detection
# (to compensate for badly-written PostScript producers that don't emit
# the necessary setpagedevice calls) and by the PDF writer.

dscparse_h=$(PSSRC)dscparse.h $(stdpre_h)

$(PSOBJ)zdscpars.$(OBJ) : $(PSSRC)zdscpars.c $(GH) $(memory__h) $(string__h)\
 $(dscparse_h) $(estack_h) $(ialloc_h) $(idict_h) $(iddict_h) $(iname_h)\
 $(iparam_h) $(istack_h) $(ivmspace_h) $(oper_h) $(store_h)\
 $(gsstruct_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdscpars.$(OBJ) $(C_) $(PSSRC)zdscpars.c

$(PSOBJ)dscparse.$(OBJ) : $(PSSRC)dscparse.c $(dscparse_h) $(stdio__h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)dscparse.$(OBJ) $(C_) $(PSSRC)dscparse.c

dscparse_=$(PSOBJ)zdscpars.$(OBJ) $(PSOBJ)dscparse.$(OBJ)

$(PSD)dscparse.dev : $(ECHOGS_XE) $(dscparse_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dscparse -obj $(dscparse_)
	$(ADDMOD) $(PSD)dscparse -oper zdscpars

# A feature to pass the Orientation information from the DSC comments
# to setpagedevice.

$(PSD)usedsc.dev : $(ECHOGS_XE) $(PSD)dscparse.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)usedsc -include $(PSD)dscparse -ps gs_dscp

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1_=$(PSOBJ)zpath1.$(OBJ)
$(PSD)path1.dev : $(ECHOGS_XE) $(path1_) $(GLD)path1lib.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)path1 $(path1_)
	$(ADDMOD) $(PSD)path1 -include $(GLD)path1lib
	$(ADDMOD) $(PSD)path1 -oper zpath1

$(PSOBJ)zpath1.$(OBJ) : $(PSSRC)zpath1.c $(OP) $(memory__h)\
 $(ialloc_h) $(estack_h) $(gspath_h) $(gsstruct_h) $(igstate_h)\
 $(oparc_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpath1.$(OBJ) $(C_) $(PSSRC)zpath1.c

# ================ Level-independent PostScript options ================ #

# ---------------- BCP filters ---------------- #

bcp_=$(GLOBJ)sbcp.$(OBJ) $(PSOBJ)zfbcp.$(OBJ)
$(PSD)bcp.dev : $(ECHOGS_XE) $(bcp_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)bcp $(bcp_)
	$(ADDMOD) $(PSD)bcp -oper zfbcp

$(PSOBJ)zfbcp.$(OBJ) : $(PSSRC)zfbcp.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(ifilter_h)\
 $(sbcp_h) $(stream_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfbcp.$(OBJ) $(C_) $(PSSRC)zfbcp.c

# ---------------- Double-precision floats ---------------- #

double_=$(PSOBJ)zdouble.$(OBJ)
$(PSD)double.dev : $(ECHOGS_XE) $(double_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)double $(double_)
	$(ADDMOD) $(PSD)double -oper zdouble1 zdouble2

$(PSOBJ)zdouble.$(OBJ) : $(PSSRC)zdouble.c $(OP)\
 $(ctype__h) $(math__h) $(memory__h) $(string__h)\
 $(gxfarith_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdouble.$(OBJ) $(C_) $(PSSRC)zdouble.c

# ---------------- EPSF files with binary headers ---------------- #

$(PSD)epsf.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)epsf -ps gs_epsf

# -------- Postscript end of the pdfwriter functionality --------- #

$(PSD)gs_pdfwr.dev : $(ECHOGS_XE) $(PSD)psl3.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)gs_pdfwr -include $(PSD)psl3
	$(ADDMOD) $(PSD)gs_pdfwr -ps gs_pdfwr

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

$(PSD)type1.dev : $(ECHOGS_XE) $(GLD)psf1lib.dev $(PSD)psf1read.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)type1 -include $(GLD)psf1lib $(PSD)psf1read

psf1read_1=$(PSOBJ)zchar1.$(OBJ) $(PSOBJ)zcharout.$(OBJ)
psf1read_2=$(PSOBJ)zfont1.$(OBJ) $(PSOBJ)zmisc1.$(OBJ)
psf1read_=$(psf1read_1) $(psf1read_2)
$(PSD)psf1read.dev : $(ECHOGS_XE) $(psf1read_) $(GLD)seexec.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psf1read $(psf1read_1)
	$(ADDMOD) $(PSD)psf1read -obj $(psf1read_2)
	$(ADDMOD) $(PSD)psf1read -include $(GLD)seexec
	$(ADDMOD) $(PSD)psf1read -oper zchar1 zfont1 zmisc1
	$(ADDMOD) $(PSD)psf1read -ps gs_agl gs_type1

$(PSOBJ)zchar1.$(OBJ) : $(PSSRC)zchar1.c $(OP) $(memory__h)\
 $(gscencs_h) $(gspaint_h) $(gspath_h) $(gsrect_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gxfcid_h) $(gxchar_h) $(gzstate_h)\
 $(estack_h) $(ialloc_h) $(ichar_h) $(ichar1_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(iname_h) $(iutil_h) $(store_h)\
 $(gxfcache_h) \
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zchar1.$(OBJ) $(C_) $(PSSRC)zchar1.c

$(PSOBJ)zfont1.$(OBJ) : $(PSSRC)zfont1.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gxdevice_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont1_h)\
 $(bfont_h) $(ialloc_h) $(ichar1_h) $(icharout_h) $(idict_h) $(idparam_h)\
 $(ifont1_h) $(iname_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont1.$(OBJ) $(C_) $(PSSRC)zfont1.c

$(PSOBJ)zmisc1.$(OBJ) : $(PSSRC)zmisc1.c $(OP) $(memory__h)\
 $(gscrypt1_h)\
 $(idict_h) $(idparam_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmisc1.$(OBJ) $(C_) $(PSSRC)zmisc1.c

# -------------- Compact Font Format and Type 2 charstrings ------------- #

$(PSD)cff.dev : $(ECHOGS_XE) $(PSD)psl2int.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cff -include $(PSD)psl2int -ps gs_cff

$(PSOBJ)zchar2.$(OBJ) : $(PSSRC)zchar2.c $(OP)\
 $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfont1_h) $(gxtype1_h)\
 $(ichar1_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zchar2.$(OBJ) $(C_) $(PSSRC)zchar2.c

$(PSOBJ)zfont2.$(OBJ) : $(PSSRC)zfont2.c $(OP) $(string__h)\
 $(gsmatrix_h) $(gxfixed_h) $(gxfont_h) $(gxfont1_h)\
 $(bfont_h) $(idict_h) $(idparam_h) $(ifont1_h) $(ifont2_h)\
 $(iname_h) $(iddict_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont2.$(OBJ) $(C_) $(PSSRC)zfont2.c

type2_=$(PSOBJ)zchar2.$(OBJ) $(PSOBJ)zfont2.$(OBJ)
$(PSD)type2.dev : $(ECHOGS_XE) $(type2_)\
 $(PSD)type1.dev $(GLD)psf2lib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)type2 $(type2_)
	$(ADDMOD) $(PSD)type2 -oper zchar2 zfont2
	$(ADDMOD) $(PSD)type2 -include $(PSD)type1 $(GLD)psf2lib

# ---------------- Type 32 (downloaded bitmap) fonts ---------------- #

$(PSOBJ)zchar32.$(OBJ) : $(PSSRC)zchar32.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h)\
 $(gxfcache_h) $(gxfixed_h) $(gxfont_h)\
 $(ifont_h) $(igstate_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zchar32.$(OBJ) $(C_) $(PSSRC)zchar32.c

$(PSOBJ)zfont32.$(OBJ) : $(PSSRC)zfont32.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h) $(gxtext_h)\
 $(bfont_h) $(store_h) $(ichar_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont32.$(OBJ) $(C_) $(PSSRC)zfont32.c

type32_=$(PSOBJ)zchar32.$(OBJ) $(PSOBJ)zfont32.$(OBJ)
$(PSD)type32.dev : $(ECHOGS_XE) $(type32_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)type32 $(type32_)
	$(ADDMOD) $(PSD)type32 -oper zchar32 zfont32
	$(ADDMOD) $(PSD)type32 -ps gs_res gs_typ32

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

# Mac glyph support (has an internal dependency)
$(PSD)macroman.dev : $(ECHOGS_XE) $(PSINIT)gs_mro_e.ps\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)macroman -ps gs_mro_e

$(PSD)macglyph.dev : $(ECHOGS_XE) $(PSINIT)gs_mgl_e.ps\
 $(PSD)macroman.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)macglyph -include $(PSD)macroman -ps gs_mgl_e

# Native TrueType support
$(PSD)ttfont.dev : $(ECHOGS_XE) $(PSD)macglyph.dev $(PSD)type42.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)ttfont -include $(PSD)macglyph $(PSD)type42
	$(ADDMOD) $(PSD)ttfont -ps gs_wan_e gs_ttf

# Type 42 (embedded TrueType) support
type42read_=$(PSOBJ)zchar42.$(OBJ) $(PSOBJ)zcharout.$(OBJ) $(PSOBJ)zfont42.$(OBJ)
$(PSD)type42.dev : $(ECHOGS_XE) $(type42read_) $(GLD)ttflib.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)type42 $(type42read_)
	$(ADDMOD) $(PSD)type42 -include $(GLD)ttflib
	$(ADDMOD) $(PSD)type42 -oper zchar42 zfont42
	$(ADDMOD) $(PSD)type42 -ps gs_typ42

$(PSOBJ)zchar42.$(OBJ) : $(PSSRC)zchar42.c $(OP)\
 $(gsmatrix_h) $(gspaint_h) $(gspath_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h)\
 $(gxgstate_h) $(gxpath_h) $(gxtext_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(ifont_h) $(igstate_h) $(store_h) $(string_h) $(zchar42_h)\
 $(idict_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zchar42.$(OBJ) $(C_) $(PSSRC)zchar42.c

$(PSOBJ)zfont42.$(OBJ) : $(PSSRC)zfont42.c $(OP) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gxfont_h) $(gxfont42_h)\
 $(bfont_h) $(icharout_h) $(idict_h) $(idparam_h) $(ifont42_h) $(iname_h)\
 $(ichar1_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont42.$(OBJ) $(C_) $(PSSRC)zfont42.c

# ======================== Precompilation options ======================== #

# ---------------- Stochastic halftone ---------------- #

$(PSD)stocht.dev : $(ECHOGS_XE) $(PSD)stocht$(COMPILE_INITS).dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)stocht -include $(PSD)stocht$(COMPILE_INITS)

# If we aren't compiling, just include the PostScript code.
# Note that the resource machinery must be loaded first.
$(PSD)stocht0.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)stocht0 -ps gs_res ht_ccsto

# If we are compiling, a special compilation step is needed.
stocht1_=$(PSOBJ)ht_ccsto.$(OBJ)
$(PSD)stocht1.dev : $(ECHOGS_XE) $(stocht1_) $(PSD)stocht0.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)stocht1 $(stocht1_)
	$(ADDMOD) $(PSD)stocht1 -halftone $(Q)StochasticDefault$(Q)
	$(ADDMOD) $(PSD)stocht1 -include $(PSD)stocht0

$(PSOBJ)ht_ccsto.$(OBJ) : $(PSGEN)ht_ccsto.c $(gxdhtres_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ht_ccsto.$(OBJ) $(C_) $(PSGEN)ht_ccsto.c

$(PSGEN)ht_ccsto.c : $(PSLIB)ht_ccsto.ps $(GENHT_XE) $(INT_MAK) $(MAKEDIRS)
	$(EXP)$(GENHT_XE) $(PSLIB)ht_ccsto.ps $(PSGEN)ht_ccsto.c

# ================ PS LL3 features used internally in L2 ================ #

# ---------------- Functions ---------------- #

ifunc_h=$(PSSRC)ifunc.h $(gsfunc_h) $(iref_h)

# Generic support, and FunctionType 0.
funcread_=$(PSOBJ)zfunc.$(OBJ) $(PSOBJ)zfunc0.$(OBJ)
$(PSD)func.dev : $(ECHOGS_XE) $(funcread_) $(GLD)funclib.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)func $(funcread_)
	$(ADDMOD) $(PSD)func -oper zfunc
	$(ADDMOD) $(PSD)func -functiontype 0
	$(ADDMOD) $(PSD)func -include $(GLD)funclib

$(PSOBJ)zfunc.$(OBJ) : $(PSSRC)zfunc.c $(OP) $(memory__h)\
 $(gscdefs_h) $(gsfunc_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(store_h) $(zfunc_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfunc.$(OBJ) $(C_) $(PSSRC)zfunc.c

$(PSOBJ)zfunc0.$(OBJ) : $(PSSRC)zfunc0.c $(OP) $(memory__h)\
 $(gsdsrc_h) $(gsfunc_h) $(gsfunc0_h)\
 $(stream_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfunc0.$(OBJ) $(C_) $(PSSRC)zfunc0.c

# ---------------- zlib/Flate filters ---------------- #

fzlib_=$(PSOBJ)zfzlib.$(OBJ)
$(PSD)fzlib.dev : $(ECHOGS_XE) $(fzlib_)\
 $(GLD)szlibe.dev $(GLD)szlibd.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fzlib -include $(GLD)szlibe $(GLD)szlibd
	$(ADDMOD) $(PSD)fzlib -obj $(fzlib_)
	$(ADDMOD) $(PSD)fzlib -oper zfzlib

$(PSOBJ)zfzlib.$(OBJ) : $(PSSRC)zfzlib.c $(OP)\
 $(idict_h) $(idparam_h) $(ifilter_h) $(ifrpred_h) $(ifwpred_h)\
 $(spdiffx_h) $(spngpx_h) $(strimpl_h) $(szlibx_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfzlib.$(OBJ) $(C_) $(PSSRC)zfzlib.c

# ---------------- ReusableStreamDecode filter ---------------- #
# This is also used by the implementation of CIDFontType 0 fonts.

$(PSD)frsd.dev : $(ECHOGS_XE) $(PSD)zfrsd.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)frsd -include $(PSD)zfrsd
	$(ADDMOD) $(PSD)frsd -ps gs_lev2 gs_res gs_frsd

zfrsd_=$(PSOBJ)zfrsd.$(OBJ)
$(PSD)zfrsd.dev : $(ECHOGS_XE) $(zfrsd_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)zfrsd $(zfrsd_)
	$(ADDMOD) $(PSD)zfrsd -oper zfrsd

$(PSOBJ)zfrsd.$(OBJ) : $(PSSRC)zfrsd.c $(OP) $(memory__h)\
 $(gsfname_h) $(gxiodev_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)\
 $(files_h) $(idict_h) $(idparam_h) $(iname_h) $(istruct_h) $(store_h)\
 $(zfile_h) $(zfrsd_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfrsd.$(OBJ) $(C_) $(PSSRC)zfrsd.c

# ======================== PostScript Level 2 ======================== #

# We keep the old name for backward compatibility.
$(PSD)level2.dev : $(PSD)psl2.dev
	$(CP_) $(PSD)psl2.dev $(PSD)level2.dev

# We -include dpsand2 first so that geninit will have access to the
# system name table as soon as possible.
$(PSD)psl2.dev : $(ECHOGS_XE)\
 $(PSD)cidfont.dev $(PSD)cie.dev $(PSD)cmapread.dev $(PSD)compfont.dev\
 $(PSD)dct.dev $(PSD)dpsand2.dev\
 $(PSD)filter.dev $(PSD)iodevice.dev $(PSD)pagedev.dev $(PSD)pattern.dev\
 $(PSD)psl1.dev $(GLD)psl2lib.dev $(PSD)psl2read.dev\
 $(PSD)sepr.dev $(PSD)type32.dev $(PSD)type42.dev\
 $(PSD)fimscale.dev $(PSD)form.dev $(PSD)icc.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl2 -include $(PSD)dpsand2
	$(ADDMOD) $(PSD)psl2 -include $(PSD)cidfont $(PSD)cie $(PSD)cmapread $(PSD)compfont
	$(ADDMOD) $(PSD)psl2 -include $(PSD)dct $(PSD)filter $(PSD)iodevice
	$(ADDMOD) $(PSD)psl2 -include $(PSD)pagedev $(PSD)pattern $(PSD)psl1 $(GLD)psl2lib $(PSD)psl2read
	$(ADDMOD) $(PSD)psl2 -include $(PSD)sepr $(PSD)type32 $(PSD)type42
	$(ADDMOD) $(PSD)psl2 -include $(PSD)fimscale $(PSD)form
	$(ADDMOD) $(PSD)psl3 -include $(PSD)icc
	$(ADDMOD) $(PSD)psl2 -emulator PostScript PostScriptLevel2

# Define basic Level 2 language support.
# This is the minimum required for CMap and CIDFont support.

psl2int_=$(PSOBJ)iutil2.$(OBJ) $(PSOBJ)zmisc2.$(OBJ)
$(PSD)psl2int.dev : $(ECHOGS_XE) $(psl2int_)\
 $(PSD)dps2int.dev $(PSD)usparam.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl2int $(psl2int_)
	$(ADDMOD) $(PSD)psl2int -include $(PSD)dps2int $(PSD)usparam
	$(ADDMOD) $(PSD)psl2int -oper zmisc2
	$(ADDMOD) $(PSD)psl2int -ps gs_lev2 gs_res

ivmem2_h=$(PSSRC)ivmem2.h $(iref_h)

$(PSOBJ)iutil2.$(OBJ) : $(PSSRC)iutil2.c $(GH) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsutil_h)\
 $(ierrors_h) $(idict_h) $(imemory_h) $(iutil_h) $(iutil2_h) $(opcheck_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iutil2.$(OBJ) $(C_) $(PSSRC)iutil2.c

$(PSOBJ)zmisc2.$(OBJ) : $(PSSRC)zmisc2.c $(OP) $(memory__h) $(string__h)\
 $(iddict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(ilevel_h) $(iname_h) $(iutil2_h) $(ivmspace_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmisc2.$(OBJ) $(C_) $(PSSRC)zmisc2.c

# Define support for user and system parameters.
# We make this a separate module only because it must have a default.

nousparm_=$(PSOBJ)inouparm.$(OBJ)
$(PSD)nousparm.dev : $(ECHOGS_XE) $(nousparm_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)nousparm $(nousparm_)

$(PSOBJ)inouparm.$(OBJ) : $(PSSRC)inouparm.c\
 $(ghost_h) $(icontext_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)inouparm.$(OBJ) $(C_) $(PSSRC)inouparm.c

usparam_=$(PSOBJ)zusparam.$(OBJ)
$(PSD)usparam.dev : $(ECHOGS_XE) $(usparam_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)usparam $(usparam_)
	$(ADDMOD) $(PSD)usparam -oper zusparam -replace $(PSD)nousparm


# Note that zusparam includes both Level 1 and Level 2 operators.
$(PSOBJ)zusparam.$(OBJ) : $(PSSRC)zusparam.c $(OP) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gsfont_h) $(gsstruct_h) $(gsutil_h) $(gxht_h)\
 $(ialloc_h) $(icontext_h) $(idict_h) $(idparam_h) $(iparam_h)\
 $(iname_h) $(itoken_h) $(iutil2_h) $(ivmem2_h)\
 $(dstack_h) $(estack_h) $(store_h) $(gsnamecl_h) $(gslibctx_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zusparam.$(OBJ) $(C_) $(PSSRC)zusparam.c

# Define full Level 2 support.

iimage2_h=$(PSSRC)iimage2.h $(gsiparam_h) $(iref_h)

psl2read_=$(PSOBJ)zcolor2.$(OBJ) $(PSOBJ)zcsindex.$(OBJ) $(PSOBJ)zht2.$(OBJ) $(PSOBJ)zimage2.$(OBJ)
# Note that zmisc2 includes both Level 1 and Level 2 operators.
$(PSD)psl2read.dev : $(ECHOGS_XE) $(psl2read_)\
 $(PSD)psl2int.dev $(PSD)dps2read.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl2read $(psl2read_)
	$(ADDMOD) $(PSD)psl2read -include $(PSD)psl2int $(PSD)dps2read
	$(ADDMOD) $(PSD)psl2read -oper zht2_l2

$(PSOBJ)zcolor2.$(OBJ) : $(PSSRC)zcolor2.c $(OP) $(string__h)\
 $(gscolor_h) $(gscssub_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(iname_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcolor2.$(OBJ) $(C_) $(PSSRC)zcolor2.c

$(PSOBJ)zcsindex.$(OBJ) : $(PSSRC)zcsindex.c $(OP) $(memory__h)\
 $(gscolor_h) $(gsstruct_h) $(gxfixed_h) $(gxcolor2_h) $(gxcspace_h) $(gsmatrix_h)\
 $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcsindex.$(OBJ) $(C_) $(PSSRC)zcsindex.c

$(PSOBJ)zht2.$(OBJ) : $(PSSRC)zht2.c $(OP)\
 $(memory__h) $(gsstruct_h) $(gxdevice_h) $(gzht_h) $(gen_ordered_h)\
 $(estack_h) $(ialloc_h) $(icolor_h) $(iddict_h) $(idparam_h) $(igstate_h)\
 $(iht_h) $(store_h) $(iname_h) $(zht2_h) $(gxgstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zht2.$(OBJ) $(II)$(GENORDERED_SRCDIR) $(C_) $(PSSRC)zht2.c

$(PSOBJ)zimage2.$(OBJ) : $(PSSRC)zimage2.c $(OP) $(math__h) $(memory__h)\
 $(gscolor_h) $(gscolor2_h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h)\
 $(gxfixed_h)\
 $(idict_h) $(idparam_h) $(iimage_h) $(iimage2_h) $(ilevel_h) $(igstate_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zimage2.$(OBJ) $(C_) $(PSSRC)zimage2.c

# ---------------- setpagedevice ---------------- #

# NOTE: gs_pdfwr.ps is not strictly speaking an interpreter feature
# but is logically part of the PS resources, and requires the content
# of gs_setpd.ps to work, so we include it here (rather than in the
# graphics library).

pagedev_=$(PSOBJ)zdevice2.$(OBJ) $(PSOBJ)zmedia2.$(OBJ)
$(PSD)pagedev.dev : $(ECHOGS_XE) $(pagedev_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pagedev $(pagedev_)
	$(ADDMOD) $(PSD)pagedev -oper zdevice2_l2 zmedia2_l2
	$(ADDMOD) $(PSD)pagedev -ps gs_setpd

$(PSOBJ)zdevice2.$(OBJ) : $(PSSRC)zdevice2.c $(OP) $(math__h) $(memory__h)\
 $(dstack_h) $(estack_h)\
 $(idict_h) $(idparam_h) $(igstate_h) $(iname_h) $(isave_h) $(iutil_h) \
 $(store_h) $(gxdevice_h) $(gsstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdevice2.$(OBJ) $(C_) $(PSSRC)zdevice2.c

$(PSOBJ)zmedia2.$(OBJ) : $(PSSRC)zmedia2.c $(OP) $(math__h) $(memory__h)\
 $(gsmatrix_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmedia2.$(OBJ) $(C_) $(PSSRC)zmedia2.c

# ---------------- IODevices ---------------- #

iodevice_=$(PSOBJ)ziodev2.$(OBJ) $(PSOBJ)zdevcal.$(OBJ)
$(PSD)iodevice.dev : $(ECHOGS_XE) $(iodevice_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)iodevice $(iodevice_)
	$(ADDMOD) $(PSD)iodevice -oper ziodev2_l2
	$(ADDMOD) $(PSD)iodevice -iodev null calendar

$(PSOBJ)ziodev2.$(OBJ) : $(PSSRC)ziodev2.c $(OP) $(string__h) $(gp_h)\
 $(gxiodev_h) $(stream_h)\
 $(dstack_h) $(files_h) $(iparam_h) $(iutil2_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ziodev2.$(OBJ) $(C_) $(PSSRC)ziodev2.c

$(PSOBJ)zdevcal.$(OBJ) : $(PSSRC)zdevcal.c $(GH) $(time__h)\
 $(gxiodev_h) $(iparam_h) $(istack_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdevcal.$(OBJ) $(C_) $(PSSRC)zdevcal.c

# ---------------- Filters other than the ones in sfilter.c ---------------- #

# Standard Level 2 decoding filters only.  The PDF configuration uses this.
fdecode_=$(GLOBJ)scantab.$(OBJ) $(GLOBJ)scfparam.$(OBJ) $(PSOBJ)zfdecode.$(OBJ)
$(PSD)fdecode.dev : $(ECHOGS_XE) $(fdecode_)\
 $(GLD)cfd.dev $(GLD)lzwd.dev $(GLD)pdiff.dev $(GLD)pngp.dev $(GLD)rld.dev\
 $(GLD)pwgd.dev $(GLD)psfilters.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fdecode $(fdecode_)
	$(ADDMOD) $(PSD)fdecode -include $(GLD)cfd $(GLD)lzwd $(GLD)pdiff $(GLD)pngp $(GLD)rld $(GLD)psfilters.dev
	$(ADDMOD) $(PSD)fdecode -oper zfdecode

$(PSOBJ)zfdecode.$(OBJ) : $(PSSRC)zfdecode.c $(OP) $(memory__h)\
 $(gsparam_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(ifilter2_h) $(ifrpred_h)\
 $(ilevel_h) $(iparam_h)\
 $(sa85x_h) $(scf_h) $(scfx_h) $(sfilter_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfdecode.$(OBJ) $(C_) $(PSSRC)zfdecode.c

# Complete Level 2 filter capability.
filter_=$(PSOBJ)zfilter2.$(OBJ)
$(PSD)filter.dev : $(ECHOGS_XE) $(PSD)fdecode.dev $(filter_)\
 $(GLD)cfe.dev $(GLD)lzwe.dev $(GLD)rle.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)filter -include $(PSD)fdecode
	$(ADDMOD) $(PSD)filter -obj $(filter_)
	$(ADDMOD) $(PSD)filter -include $(GLD)cfe $(GLD)lzwe $(GLD)rle
	$(ADDMOD) $(PSD)filter -oper zfilter2

$(PSOBJ)zfilter2.$(OBJ) : $(PSSRC)zfilter2.c $(OP) $(memory__h)\
 $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(ifilter2_h) $(ifwpred_h)\
 $(store_h)\
 $(sfilter_h) $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(strimpl_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfilter2.$(OBJ) $(C_) $(PSSRC)zfilter2.c

# MD5 digest filter
fmd5_=$(PSOBJ)zfmd5.$(OBJ)
$(PSD)fmd5.dev : $(ECHOGS_XE) $(fmd5_) $(GLD)smd5.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fmd5 $(fmd5_)
	$(ADDMOD) $(PSD)fmd5 -include $(GLD)smd5
	$(ADDMOD) $(PSD)fmd5 -oper zfmd5

$(PSOBJ)zfmd5.$(OBJ) : $(PSSRC)zfmd5.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(ifilter_h)\
 $(smd5_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfmd5.$(OBJ) $(C_) $(PSSRC)zfmd5.c

# SHA-256 digest filter
fsha2_=$(PSOBJ)zfsha2.$(OBJ)
$(PSD)fsha2.dev : $(ECHOGS_XE) $(fsha2_) $(GLD)ssha2.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fsha2 $(fsha2_)
	$(ADDMOD) $(PSD)fsha2 -include $(GLD)ssha2
	$(ADDMOD) $(PSD)fsha2 -oper zfsha2

$(PSOBJ)zfsha2.$(OBJ) : $(PSSRC)zfsha2.c $(OP) $(memory__h)\
 $(ghost_h) $(oper_h) $(gsstruct_h) $(stream_h) $(strimpl_h)\
 $(ialloc_h) $(ifilter_h) $(ssha2_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfsha2.$(OBJ) $(C_) $(PSSRC)zfsha2.c

# Arcfour cipher filter
farc4_=$(PSOBJ)zfarc4.$(OBJ)
$(PSD)farc4.dev : $(ECHOGS_XE) $(farc4_) $(GLD)sarc4.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)farc4 $(farc4_)
	$(ADDMOD) $(PSD)farc4 -include $(GLD)sarc4
	$(ADDMOD) $(PSD)farc4 -oper zfarc4

$(PSOBJ)zfarc4.$(OBJ) : $(PSSRC)zfarc4.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(sarc4_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfarc4.$(OBJ) $(C_) $(PSSRC)zfarc4.c

# AES cipher filter
faes_=$(PSOBJ)zfaes.$(OBJ)
$(PSD)faes.dev : $(ECHOGS_XE) $(faes_) $(GLD)saes.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)faes $(faes_)
	$(ADDMOD) $(PSD)faes -include $(GLD)saes
	$(ADDMOD) $(PSD)faes -oper zfaes

$(PSOBJ)zfaes.$(OBJ) : $(PSSRC)zfaes.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h)\
 $(saes_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfaes.$(OBJ) $(C_) $(PSSRC)zfaes.c

# JBIG2 compression filter
# this can be turned on and off with a FEATURE_DEV

fjbig2_=$(PSOBJ)zfjbig2_$(JBIG2_LIB).$(OBJ)

$(PSD)jbig2_jbig2dec.dev : $(ECHOGS_XE) $(fjbig2_) $(GLD)sjbig2.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jbig2_jbig2dec $(fjbig2_)
	$(ADDMOD) $(PSD)jbig2_jbig2dec -include $(GLD)sjbig2
	$(ADDMOD) $(PSD)jbig2_jbig2dec -oper zfjbig2

$(PSD)jbig2_luratech.dev : $(ECHOGS_XE) $(fjbig2_) $(GLD)sjbig2.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jbig2_luratech $(fjbig2_)
	$(ADDMOD) $(PSD)jbig2_luratech -include $(GLD)sjbig2
	$(ADDMOD) $(PSD)jbig2_luratech -oper zfjbig2

$(PSD)jbig2_.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jbig2_

$(PSD)jbig2.dev : $(PSD)jbig2_$(JBIG2_LIB).dev $(INT_MAK) $(MAKEDIRS)
	$(CP_) $(PSD)jbig2_$(JBIG2_LIB).dev $(PSD)jbig2.dev


$(PSOBJ)zfjbig2_jbig2dec.$(OBJ) : $(PSSRC)zfjbig2.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(sjbig2_h) $(INT_MAK) $(MAKEDIRS)
	$(PSJBIG2CC) $(PSO_)zfjbig2_jbig2dec.$(OBJ) $(C_) $(PSSRC)zfjbig2.c

$(PSOBJ)zfjbig2_luratech.$(OBJ) : $(PSSRC)zfjbig2.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(sjbig2_h) $(INT_MAK) $(MAKEDIRS)
	$(PSLDFJB2CC) $(PSO_)zfjbig2_luratech.$(OBJ) $(C_) $(PSSRC)zfjbig2.c

# JPX (jpeg 2000) compression filter
# this can be turned on and off with a FEATURE_DEV

$(PSD)jpx.dev : $(ECHOGS_XE) $(PSD)jpx_$(JPX_LIB).dev\
 $(INT_MAK) $(MAKEDIRS)
	$(CP_) $(PSD)jpx_$(JPX_LIB).dev $(PSD)jpx.dev

$(PSD)jpx_.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jpx_

fjpx_luratech=$(PSOBJ)zfjpx_luratech.$(OBJ)

$(PSOBJ)zfjpx.$(OBJ) : $(PSSRC)zfjpx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(ialloc_h) $(iname_h)\
 $(gdebug_h) $(sjpx_h) $(INT_MAK) $(MAKEDIRS)
	$(PSJASCC) $(PSO_)zfjpx.$(OBJ) $(C_) $(PSSRC)zfjpx.c

$(PSD)jpx_luratech.dev : $(ECHOGS_XE) $(fjpx_luratech)\
 $(GLD)sjpx.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jpx_luratech $(fjpx_luratech)
	$(ADDMOD) $(PSD)jpx_luratech -include $(GLD)sjpx
	$(ADDMOD) $(PSD)jpx_luratech -include $(GLD)lwf_jp2
	$(ADDMOD) $(PSD)jpx_luratech -oper zfjpx

$(PSOBJ)zfjpx_luratech.$(OBJ) : $(PSSRC)zfjpx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(sjpx_luratech_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSLWFJPXCC) $(PSO_)zfjpx_luratech.$(OBJ) \
		$(C_) $(PSSRC)zfjpx.c

fjpx_openjpeg=$(PSOBJ)zfjpx_openjpeg.$(OBJ)

$(PSD)jpx_openjpeg.dev : $(ECHOGS_XE) $(fjpx_openjpeg)\
 $(GLD)sjpx.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jpx_openjpeg $(fjpx_openjpeg)
	$(ADDMOD) $(PSD)jpx_openjpeg -include $(GLD)sjpx
	$(ADDMOD) $(PSD)jpx_openjpeg -include $(GLD)openjpeg
	$(ADDMOD) $(PSD)jpx_openjpeg -oper zfjpx

$(PSOBJ)zfjpx_openjpeg.$(OBJ) : $(PSSRC)zfjpx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(sjpx_openjpeg_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSOPJJPXCC) $(PSO_)zfjpx_openjpeg.$(OBJ) \
		$(C_) $(PSSRC)zfjpx.c


# imagemask scaling filter
fimscale_=$(PSOBJ)zfimscale.$(OBJ)
$(PSD)fimscale.dev : $(ECHOGS_XE) $(fimscale_)\
 $(GLD)simscale.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fimscale $(fimscale_)
	$(ADDMOD) $(PSD)fimscale -include $(GLD)simscale
	$(ADDMOD) $(PSD)fimscale -oper zfimscale

$(PSOBJ)zfimscale.$(OBJ) : $(PSSRC)zfimscale.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(simscale_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfimscale.$(OBJ) $(C_) $(PSSRC)zfimscale.c

# ---------------- Binary tokens ---------------- #

nobtoken_=$(PSOBJ)inobtokn.$(OBJ)
$(PSD)nobtoken.dev : $(ECHOGS_XE) $(nobtoken_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)nobtoken $(nobtoken_)

$(PSOBJ)inobtokn.$(OBJ) : $(PSSRC)inobtokn.c $(GH)\
 $(stream_h) $(ierrors_h) $(iscan_h) $(iscanbin_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)inobtokn.$(OBJ) $(C_) $(PSSRC)inobtokn.c

btoken_=$(PSOBJ)iscanbin.$(OBJ) $(PSOBJ)zbseq.$(OBJ)
$(PSD)btoken.dev : $(ECHOGS_XE) $(btoken_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)btoken $(btoken_)
	$(ADDMOD) $(PSD)btoken -oper zbseq_l2 -replace $(PSD)nobtoken
	$(ADDMOD) $(PSD)btoken -ps gs_btokn

$(PSOBJ)iscanbin.$(OBJ) : $(PSSRC)iscanbin.c $(GH)\
 $(math__h) $(memory__h) $(ierrors_h)\
 $(gsutil_h) $(gxalloc_h) $(ialloc_h) $(ibnum_h) $(iddict_h) $(iname_h)\
 $(iscan_h) $(iscanbin_h) $(iutil_h) $(ivmspace_h)\
 $(btoken_h) $(dstack_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iscanbin.$(OBJ) $(C_) $(PSSRC)iscanbin.c

$(PSOBJ)zbseq.$(OBJ) : $(PSSRC)zbseq.c $(OP) $(memory__h)\
 $(gxalloc_h)\
 $(btoken_h) $(ialloc_h) $(istruct_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zbseq.$(OBJ) $(C_) $(PSSRC)zbseq.c

# ---------------- User paths & insideness testing ---------------- #

upath_=$(PSOBJ)zupath.$(OBJ) $(PSOBJ)ibnum.$(OBJ) $(GLOBJ)gdevhit.$(OBJ)
$(PSD)upath.dev : $(ECHOGS_XE) $(upath_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)upath $(upath_)
	$(ADDMOD) $(PSD)upath -oper zupath_l2

$(PSOBJ)zupath.$(OBJ) : $(PSSRC)zupath.c $(OP)\
 $(dstack_h) $(oparc_h) $(store_h)\
 $(ibnum_h) $(idict_h) $(igstate_h) $(iname_h) $(iutil_h) $(stream_h)\
 $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h)\
 $(gxfixed_h) $(gxdevice_h) $(gzpath_h) $(gzstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zupath.$(OBJ) $(C_) $(PSSRC)zupath.c

# -------- Additions common to Display PostScript and Level 2 -------- #

$(PSD)dpsand2.dev : $(ECHOGS_XE)\
 $(PSD)btoken.dev $(PSD)color.dev $(PSD)upath.dev $(GLD)dps2lib.dev $(PSD)dps2read.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dpsand2 -include $(PSD)btoken $(PSD)color $(PSD)upath $(GLD)dps2lib $(PSD)dps2read

dps2int_=$(PSOBJ)zvmem2.$(OBJ) $(PSOBJ)zdps1.$(OBJ)
# Note that zvmem2 includes both Level 1 and Level 2 operators.
$(PSD)dps2int.dev : $(ECHOGS_XE) $(dps2int_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dps2int $(dps2int_)
	$(ADDMOD) $(PSD)dps2int -oper zvmem2 zdps1_l2
	$(ADDMOD) $(PSD)dps2int -ps gs_dps1

dps2read_=$(PSOBJ)ibnum.$(OBJ) $(PSOBJ)zcharx.$(OBJ)
$(PSD)dps2read.dev : $(ECHOGS_XE) $(dps2read_) $(PSD)dps2int.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dps2read $(dps2read_)
	$(ADDMOD) $(PSD)dps2read -include $(PSD)dps2int
	$(ADDMOD) $(PSD)dps2read -oper ireclaim_l2 zcharx
	$(ADDMOD) $(PSD)dps2read -ps gs_dps2

$(PSOBJ)ibnum.$(OBJ) : $(PSSRC)ibnum.c $(GH) $(math__h) $(memory__h)\
 $(ierrors_h) $(stream_h) $(ibnum_h) $(imemory_h) $(iutil_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ibnum.$(OBJ) $(C_) $(PSSRC)ibnum.c

$(PSOBJ)zcharx.$(OBJ) : $(PSSRC)zcharx.c $(OP)\
 $(gsmatrix_h) $(gstext_h) $(gxfixed_h) $(gxfont_h) $(gxtext_h)\
 $(ialloc_h) $(ibnum_h) $(ichar_h) $(iname_h) $(igstate_h) $(memory__h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcharx.$(OBJ) $(C_) $(PSSRC)zcharx.c

$(PSOBJ)zdps1.$(OBJ) : $(PSSRC)zdps1.c $(OP)\
 $(gsmatrix_h) $(gspath_h) $(gspath2_h) $(gsstate_h) $(gxgstate_h) \
 $(ialloc_h) $(ivmspace_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdps1.$(OBJ) $(C_) $(PSSRC)zdps1.c

$(PSOBJ)zvmem2.$(OBJ) : $(PSSRC)zvmem2.c $(OP)\
 $(estack_h) $(ialloc_h) $(ivmspace_h) $(store_h) $(ivmem2_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zvmem2.$(OBJ) $(C_) $(PSSRC)zvmem2.c

# -------- Composite (PostScript Type 0) font support -------- #

$(PSD)compfont.dev : $(ECHOGS_XE)\
 $(GLD)psf0lib.dev $(PSD)psf0read.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)compfont -include $(GLD)psf0lib $(PSD)psf0read

# We always include cmapread because zfont0.c refers to it,
# and it's not worth the trouble to exclude.
psf0read_=$(PSOBJ)zcfont.$(OBJ) $(PSOBJ)zfont0.$(OBJ)
$(PSD)psf0read.dev : $(ECHOGS_XE) $(psf0read_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psf0read $(psf0read_)
	$(ADDMOD) $(PSD)psf0read -oper zcfont zfont0
	$(ADDMOD) $(PSD)psf0read -include $(PSD)cmapread

$(PSOBJ)zcfont.$(OBJ) : $(PSSRC)zcfont.c $(OP)\
 $(gsmatrix_h)\
 $(gxfixed_h) $(gxfont_h) $(gxtext_h)\
 $(ichar_h) $(estack_h) $(ifont_h) $(igstate_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcfont.$(OBJ) $(C_) $(PSSRC)zcfont.c

$(PSOBJ)zfont0.$(OBJ) : $(PSSRC)zfont0.c $(OP)\
 $(gsstruct_h)\
 $(gxdevice_h) $(gxfcmap_h) $(gxfixed_h) $(gxfont_h) $(gxfont0_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(bfont_h) $(ialloc_h) $(iddict_h) $(idparam_h) $(igstate_h) $(iname_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfont0.$(OBJ) $(C_) $(PSSRC)zfont0.c

# ---------------- CMap and CIDFont support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

icid_h=$(PSSRC)icid.h $(iref_h) $(std_h)
ifcid_h=$(PSSRC)ifcid.h $(gxfcid_h) $(icid_h) $(iostack_h)

cmapread_=$(PSOBJ)zcid.$(OBJ) $(PSOBJ)zfcmap.$(OBJ)
$(PSD)cmapread.dev : $(ECHOGS_XE) $(cmapread_)\
 $(GLD)cmaplib.dev $(PSD)psl2int.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cmapread $(cmapread_)
	$(ADDMOD) $(PSD)cmapread -include $(GLD)cmaplib $(PSD)psl2int
	$(ADDMOD) $(PSD)cmapread -oper zfcmap
	$(ADDMOD) $(PSD)cmapread -ps gs_cmap

$(PSOBJ)zfcmap.$(OBJ) : $(PSSRC)zfcmap.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfcmap1_h) $(gxfont_h)\
 $(ialloc_h) $(icid_h) $(iddict_h) $(idparam_h) $(ifont_h) $(iname_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfcmap.$(OBJ) $(C_) $(PSSRC)zfcmap.c

cidread_=$(PSOBJ)zcid.$(OBJ) $(PSOBJ)zfcid.$(OBJ) $(PSOBJ)zfcid0.$(OBJ) $(PSOBJ)zfcid1.$(OBJ)
$(PSD)cidfont.dev : $(ECHOGS_XE) $(cidread_)\
 $(PSD)psf1read.dev $(PSD)psl2int.dev $(PSD)type2.dev $(PSD)type42.dev\
 $(PSD)zfrsd.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cidfont $(cidread_)
	$(ADDMOD) $(PSD)cidfont -include $(PSD)psf1read $(PSD)psl2int
	$(ADDMOD) $(PSD)cidfont -include $(PSD)type2 $(PSD)type42 $(PSD)zfrsd
	$(ADDMOD) $(PSD)cidfont -oper zfcid0 zfcid1
	$(ADDMOD) $(PSD)cidfont -ps gs_cidfn gs_cidcm gs_fntem gs_cidtt gs_cidfm

$(PSOBJ)zcid.$(OBJ) : $(PSSRC)zcid.c $(OP)\
 $(gxcid_h) $(ierrors_h) $(icid_h) $(idict_h) $(idparam_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcid.$(OBJ) $(C_) $(PSSRC)zcid.c

$(PSOBJ)zfcid.$(OBJ) : $(PSSRC)zfcid.c $(OP)\
 $(gsmatrix_h) $(gxfcid_h)\
 $(bfont_h) $(icid_h) $(idict_h) $(idparam_h) $(ifcid_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfcid.$(OBJ) $(C_) $(PSSRC)zfcid.c

$(PSOBJ)zfcid0.$(OBJ) : $(PSSRC)zfcid0.c $(OP) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxalloc_h) $(gxfcid_h) $(gxfont1_h)\
 $(stream_h)\
 $(bfont_h) $(files_h) $(ichar_h) $(ichar1_h) $(icid_h) $(idict_h) $(idparam_h)\
 $(ifcid_h) $(ifont1_h) $(ifont2_h) $(ifont42_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfcid0.$(OBJ) $(C_) $(PSSRC)zfcid0.c

$(PSOBJ)zfcid1.$(OBJ) : $(PSSRC)zfcid1.c $(OP) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsstruct_h) $(gsgcache_h) $(gsutil_h)\
 $(gxfcid_h) $(gxfcache_h)\
 $(bfont_h) $(icid_h) $(ichar1_h) $(idict_h) $(idparam_h)\
 $(ifcid_h) $(ifont42_h) $(store_h) $(stream_h) $(files_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfcid1.$(OBJ) $(C_) $(PSSRC)zfcid1.c

# ---------------- CIE color ---------------- #

cieread_=$(PSOBJ)zcie.$(OBJ) $(PSOBJ)zcrd.$(OBJ)
$(PSD)cie.dev : $(ECHOGS_XE) $(cieread_) $(GLD)cielib.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cie $(cieread_)
	$(ADDMOD) $(PSD)cie -oper zcrd_l2
	$(ADDMOD) $(PSD)cie -include $(GLD)cielib

icie_h=$(PSSRC)icie.h $(gscie_h) $(igstate_h)

$(PSOBJ)zcie.$(OBJ) : $(PSSRC)zcie.c $(OP) $(math__h) $(memory__h)\
 $(gscolor2_h) $(gscie_h) $(gsstruct_h) $(gxcspace_h)\
 $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h)\
 $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)\
 $(zcie_h) $(gsicc_create_h) $(gsicc_manage_h) $(gsicc_profilecache_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcie.$(OBJ) $(C_) $(PSSRC)zcie.c

$(PSOBJ)zcrd.$(OBJ) : $(PSSRC)zcrd.c $(OP) $(math__h)\
 $(gscrd_h) $(gscrdp_h) $(gscspace_h) $(gscolor2_h) $(gsstruct_h)\
 $(estack_h) $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(igstate_h)\
 $(iparam_h) $(ivmspace_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcrd.$(OBJ) $(C_) $(PSSRC)zcrd.c

# ---------------- Pattern color ---------------- #

ipcolor_h=$(PSSRC)ipcolor.h $(iref_h)

$(PSD)pattern.dev : $(ECHOGS_XE) $(GLD)patlib.dev\
 $(PSD)patread.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pattern -include $(GLD)patlib $(PSD)patread

patread_=$(PSOBJ)zpcolor.$(OBJ)
$(PSD)patread.dev : $(ECHOGS_XE) $(patread_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)patread $(patread_)
	$(ADDMOD) $(PSD)patread -oper zpcolor_l2

$(PSOBJ)zpcolor.$(OBJ) : $(PSSRC)zpcolor.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h) $(gscoord_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxpcolor_h) $(gxpath_h)\
 $(estack_h)\
 $(ialloc_h) $(icremap_h) $(idict_h) $(idparam_h) $(igstate_h)\
 $(ipcolor_h) $(istruct_h)\
 $(store_h) $(gzstate_h) $(memory__h) $(gdevp14_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpcolor.$(OBJ) $(C_) $(PSSRC)zpcolor.c

# ---------------- Separation color ---------------- #

seprread_=$(PSOBJ)zcssepr.$(OBJ) $(PSOBJ)zfsample.$(OBJ)
$(PSD)sepr.dev : $(ECHOGS_XE) $(seprread_)\
 $(PSD)func4.dev $(GLD)seprlib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)sepr $(seprread_)
	$(ADDMOD) $(PSD)sepr -oper zcssepr_l2
	$(ADDMOD) $(PSD)sepr -oper zfsample
	$(ADDMOD) $(PSD)sepr -include $(PSD)func4 $(GLD)seprlib

$(PSOBJ)zcssepr.$(OBJ) : $(PSSRC)zcssepr.c $(OP) $(memory__h)\
 $(gscolor_h) $(gscsepr_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(zht2_h)\
 $(estack_h) $(ialloc_h) $(icsmap_h) $(ifunc_h) $(igstate_h)\
 $(iname_h) $(ivmspace_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcssepr.$(OBJ) $(C_) $(PSSRC)zcssepr.c

$(PSOBJ)zfsample.$(OBJ) : $(PSSRC)zfsample.c $(OP) $(memory__h)\
 $(gxcspace_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(ostack_h)\
 $(store_h) $(gsfunc0_h) $(gscdevn_h) $(zfunc_h) $(zcolor_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfsample.$(OBJ) $(C_) $(PSSRC)zfsample.c

# ---------------- DCT filters ---------------- #
# The definitions for jpeg*.dev are in jpeg.mak.

$(PSD)dct.dev : $(ECHOGS_XE) $(PSD)dcte.dev $(PSD)dctd.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dct -include $(PSD)dcte $(PSD)dctd

# Encoding (compression)

dcte_=$(PSOBJ)zfdcte.$(OBJ)
$(PSD)dcte.dev : $(ECHOGS_XE) $(GLD)sdcte.dev $(GLD)sdeparam.dev\
 $(dcte_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dcte -include $(GLD)sdcte $(GLD)sdeparam
	$(ADDMOD) $(PSD)dcte -obj $(dcte_)
	$(ADDMOD) $(PSD)dcte -oper zfdcte

$(PSOBJ)zfdcte.$(OBJ) : $(PSSRC)zfdcte.c $(OP)\
 $(memory__h) $(stdio__h) $(jpeglib__h) $(gsmemory_h)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h)\
 $(iparam_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfdcte.$(OBJ) $(C_) $(PSSRC)zfdcte.c

# Decoding (decompression)

dctd_=$(PSOBJ)zfdctd.$(OBJ)
$(PSD)dctd.dev : $(ECHOGS_XE) $(GLD)sdctd.dev $(GLD)sddparam.dev\
 $(dctd_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dctd -include $(GLD)sdctd $(GLD)sddparam
	$(ADDMOD) $(PSD)dctd -obj $(dctd_)
	$(ADDMOD) $(PSD)dctd -oper zfdctd

$(PSOBJ)zfdctd.$(OBJ) : $(PSSRC)zfdctd.c $(OP)\
 $(memory__h) $(stdio__h) $(jpeglib__h) $(gsmemory_h)\
 $(ialloc_h) $(ifilter_h) $(iparam_h) $(sdct_h) $(sjpeg_h)\
 $(strimpl_h) $(igstate_h) $(gxdevcli_h) $(gxdevsop_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfdctd.$(OBJ) $(C_) $(PSSRC)zfdctd.c

# ================ Display PostScript ================ #

dps_=$(PSOBJ)zdps.$(OBJ) $(PSOBJ)zcontext.$(OBJ)
$(PSD)dps.dev : $(ECHOGS_XE) $(GLD)dpslib.dev $(PSD)psl2.dev\
 $(dps_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dps -include $(GLD)dpslib $(PSD)psl2
	$(ADDMOD) $(PSD)dps -obj $(dps_)
	$(ADDMOD) $(PSD)dps -oper zcontext1 zcontext2 zdps
	$(ADDMOD) $(PSD)dps -ps gs_dps

$(PSOBJ)zdps.$(OBJ) : $(PSSRC)zdps.c $(OP)\
 $(gsdps_h) $(gsimage_h) $(gsiparm2_h) $(gsstate_h)\
 $(gxalloc_h) $(gxfixed_h) $(gxpath_h)\
 $(btoken_h)\
 $(idparam_h) $(iddict_h) $(igstate_h) $(iimage2_h) $(iname_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdps.$(OBJ) $(C_) $(PSSRC)zdps.c

$(PSOBJ)zcontext.$(OBJ) : $(PSSRC)zcontext.c $(OP) $(gp_h) $(memory__h)\
 $(gsexit_h) $(gsgc_h) $(gsstruct_h) $(gsutil_h) $(gxalloc_h) $(gxstate_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(interp_h) $(isave_h) $(istruct_h)\
 $(dstack_h) $(estack_h) $(files_h) $(ostack_h) $(store_h) $(stream_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcontext.$(OBJ) $(C_) $(PSSRC)zcontext.c

# ---------------- NeXT Display PostScript ---------------- #

dpsnext_=$(PSOBJ)zdpnext.$(OBJ)
$(PSD)dpsnext.dev : $(ECHOGS_XE) $(dpsnext_)\
 $(PSD)dps.dev $(GLD)dpnxtlib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)dpsnext -include $(PSD)dps $(GLD)dpnxtlib
	$(ADDMOD) $(PSD)dpsnext -obj $(dpsnext_)
	$(ADDMOD) $(PSD)dpsnext -oper zdpnext
	$(ADDMOD) $(PSD)dpsnext -ps gs_dpnxt

$(PSOBJ)zdpnext.$(OBJ) : $(PSSRC)zdpnext.c $(math__h) $(OP)\
 $(gscoord_h) $(gscspace_h) $(gsdpnext_h)\
 $(gsiparam_h) $(gsiparm2_h) $(gsmatrix_h) $(gspath2_h)\
 $(gxcvalue_h) $(gxdevice_h) $(gxsample_h)\
 $(ialloc_h) $(igstate_h) $(iimage_h) $(iimage2_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zdpnext.$(OBJ) $(C_) $(PSSRC)zdpnext.c

# ==================== PostScript LanguageLevel 3 ===================== #

# ---------------- DevicePixel color space ---------------- #

cspixint_=$(PSOBJ)zcspixel.$(OBJ)
$(PSD)cspixel.dev : $(ECHOGS_XE) $(cspixint_) $(GLD)cspixlib.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)cspixel $(cspixint_)
	$(ADDMOD) $(PSD)cspixel -include $(GLD)cspixlib

$(PSOBJ)zcspixel.$(OBJ) : $(PSSRC)zcspixel.c $(OP)\
 $(gscolor2_h) $(gscpixel_h) $(gscspace_h) $(gsmatrix_h)\
 $(igstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcspixel.$(OBJ) $(C_) $(PSSRC)zcspixel.c

# ---------------- Rest of LanguageLevel 3 ---------------- #

$(PSD)psl3.dev : $(ECHOGS_XE)\
 $(PSD)psl2.dev $(PSD)cspixel.dev $(PSD)frsd.dev $(PSD)func.dev\
 $(GLD)psl3lib.dev $(PSD)psl3read.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl3 -include $(PSD)psl2 $(PSD)cspixel $(PSD)frsd $(PSD)func
	$(ADDMOD) $(PSD)psl3 -include $(GLD)psl3lib $(PSD)psl3read
	$(ADDMOD) $(PSD)psl3 -include $(PSD)icc
	$(ADDMOD) $(PSD)psl3 -emulator PostScript PostScriptLevel2 PostScriptLevel3

$(PSOBJ)zfunc3.$(OBJ) : $(PSSRC)zfunc3.c $(memory__h) $(OP)\
 $(gsfunc3_h) $(gsstruct_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)\
 $(store_h) $(stream_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfunc3.$(OBJ) $(C_) $(PSSRC)zfunc3.c

# FunctionType 4 functions are not a PostScript feature, but they
# are used in the implementation of Separation and DeviceN color spaces.

func4read_=$(PSOBJ)zfunc4.$(OBJ)
$(PSD)func4.dev : $(ECHOGS_XE) $(func4read_)\
 $(PSD)func.dev $(GLD)func4lib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)func4 $(func4read_)
	$(ADDMOD) $(PSD)func4 -functiontype 4
	$(ADDMOD) $(PSD)func4 -include $(PSD)func $(GLD)func4lib

# Note: opextern.h is included from oper.h and is a dependency of oper.h
$(PSOBJ)zfunc4.$(OBJ) : $(PSSRC)zfunc4.c $(memory__h) $(string__h)\
 $(OP) $(opextern_h)\
 $(gsfunc_h) $(gsfunc4_h) $(gsutil_h)\
 $(idict_h) $(ifunc_h) $(iname_h) $(ialloc_h)\
 $(dstack_h) $(gzstate_h) $(gxdevcli_h) $(string__h) $(zfunc_h)\
 $(zcolor_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfunc4.$(OBJ) $(C_) $(PSSRC)zfunc4.c

$(PSOBJ)zimage3.$(OBJ) : $(PSSRC)zimage3.c $(OP) $(memory__h)\
 $(gscolor2_h) $(gsiparm3_h) $(gsiparm4_h) $(gscspace_h) $(gxiparam_h)\
 $(idparam_h) $(idict_h) $(igstate_h) $(iimage_h) $(iimage2_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zimage3.$(OBJ) $(C_) $(PSSRC)zimage3.c

$(PSOBJ)zmisc3.$(OBJ) : $(PSSRC)zmisc3.c $(GH)\
 $(gsclipsr_h) $(gscolor2_h) $(gscspace_h) $(gscssub_h) $(gsmatrix_h)\
 $(igstate_h) $(oper_h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zmisc3.$(OBJ) $(C_) $(PSSRC)zmisc3.c

$(PSOBJ)zcolor3.$(OBJ) : $(PSSRC)zcolor3.c $(GH)\
 $(oper_h) $(igstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zcolor3.$(OBJ) $(C_) $(PSSRC)zcolor3.c

$(PSOBJ)zshade.$(OBJ) : $(PSSRC)zshade.c $(memory__h) $(OP)\
 $(gscolor2_h) $(gscolor3_h) $(gscspace_h) $(gsfunc3_h)\
 $(gsptype2_h) $(gsshade_h) $(gsstruct_h) $(gsuid_h) $(gscie_h)\
 $(stream_h)\
 $(files_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(igstate_h) $(ipcolor_h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zshade.$(OBJ) $(C_) $(PSSRC)zshade.c

psl3read_1=$(PSOBJ)zfunc3.$(OBJ) $(PSOBJ)zfsample.$(OBJ)
psl3read_2=$(PSOBJ)zimage3.$(OBJ) $(PSOBJ)zmisc3.$(OBJ) $(PSOBJ)zcolor3.$(OBJ)\
 $(PSOBJ)zshade.$(OBJ)
psl3read_=$(psl3read_1) $(psl3read_2)

# Note: we need the ReusableStreamDecode filter for shadings.
$(PSD)psl3read.dev : $(ECHOGS_XE) $(psl3read_)\
 $(PSD)frsd.dev $(PSD)fzlib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)psl3read $(psl3read_1)
	$(ADDMOD) $(PSD)psl3read $(psl3read_2)
	$(ADDMOD) $(PSD)psl3read -oper zfsample
	$(ADDMOD) $(PSD)psl3read -oper zimage3 zmisc3 zcolor3_l3 zshade
	$(ADDMOD) $(PSD)psl3read -functiontype 2 3
	$(ADDMOD) $(PSD)psl3read -ps gs_ll3
	$(ADDMOD) $(PSD)psl3read -include $(PSD)frsd $(PSD)fzlib

# ---------------- Trapping ---------------- #

trapread_=$(PSOBJ)ztrap.$(OBJ)
$(PSD)trapread.dev : $(ECHOGS_XE) $(trapread_)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)trapread $(trapread_)
	$(ADDMOD) $(PSD)trapread -oper ztrap
	$(ADDMOD) $(PSD)trapread -ps gs_trap

$(PSOBJ)ztrap.$(OBJ) : $(PSSRC)ztrap.c $(OP)\
 $(gstrap_h)\
 $(ialloc_h) $(iparam_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ztrap.$(OBJ) $(C_) $(PSSRC)ztrap.c

$(PSD)trapping.dev : $(ECHOGS_XE) $(GLD)traplib.dev $(PSD)trapread.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)trapping -include $(GLD)traplib $(PSD)trapread

# ---------------- Transparency ---------------- #

transread_=$(PSOBJ)ztrans.$(OBJ)
$(PSD)transpar.dev : $(ECHOGS_XE)\
 $(PSD)psl2read.dev $(GLD)translib.dev $(transread_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)transpar $(transread_)
	$(ADDMOD) $(PSD)transpar -oper ztrans1 ztrans2 ztrans3
	$(ADDMOD) $(PSD)transpar -include $(PSD)psl2read $(GLD)translib

$(PSOBJ)ztrans.$(OBJ) : $(PSSRC)ztrans.c $(OP) $(memory__h) $(string__h)\
 $(ghost_h) $(oper_h) $(gscspace_h) $(gscolor2_h) $(gsipar3x_h) $(gstrans_h)\
 $(gxiparam_h) $(gxcspace_h)\
 $(idict_h) $(idparam_h) $(ifunc_h) $(igstate_h) $(iimage_h) $(iname_h)\
 $(store_h) $(gdevdevn_h)  $(gxdevsop_h) $(gxblend_h) $(gdevp14_h)\
 $(gsicc_cms_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ztrans.$(OBJ) $(C_) $(PSSRC)ztrans.c

# ---------------- ICCBased color spaces ---------------- #

iccread_=$(PSOBJ)zicc.$(OBJ)
$(PSD)icc.dev : $(ECHOGS_XE) $(PSD)cie.dev $(iccread_) \
                $(GLD)sicclib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)icc $(iccread_)
	$(ADDMOD) $(PSD)icc -oper zicc_ll3
	$(ADDMOD) $(PSD)icc -ps gs_icc
	$(ADDMOD) $(PSD)icc -include $(GLD)sicclib $(PSD)cie

$(PSOBJ)zicc.$(OBJ) : $(PSSRC)zicc.c  $(OP) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gxcspace_h) $(stream_h) $(files_h) $(gscolor2_h)\
 $(gsicc_h) $(estack_h) $(idict_h) $(idparam_h) $(igstate_h)\
 $(icie_h) $(ialloc_h) $(zicc_h) $(gsicc_manage_h) $(GX) $(gxgstate_h)\
 $(gsicc_create_h) $(gsicc_profilecache_h) $(gxdevice_h)\
 $(gsicc_cache_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zicc.$(OBJ) $(C_) $(PSSRC)zicc.c

# ---------------- Support for %disk IODevices ---------------- #

# Note that we go ahead and create 7 %disk devices. The internal
# overhead of any unused %disk structures is minimal.
# We could have more, but the DynaLab font installer has problems
# with more than 7 disk devices.
diskn_=$(GLOBJ)gsiodisk.$(OBJ)
$(GLD)diskn.dev : $(LIB_MAK) $(ECHOGS_XE) $(diskn_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)diskn $(diskn_)
	$(ADDMOD) $(GLD)diskn -iodev disk0 disk1 disk2 disk3 disk4 disk5 disk6
	$(ADDMOD) $(GLD)diskn -ps gs_diskn

# ------------------ Support high level Forms ------------------ #
form_=$(GLOBJ)zform.$(OBJ)
gsform1_h=$(GLSRC)gsform1.h $(gxpath_h) $(gsmatrix_h) $(gsgstate_h) $(gstypes_h)
$(GLD)form.dev : $(LIB_MAK) $(ECHOGS_XE) $(form_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)form $(form_)
	$(ADDMOD) $(PSD)form -oper zform

$(PSOBJ)zform.$(OBJ) : $(PSSRC)zform.c $(OP) $(ghost_h) $(oper_h)\
  $(gxdevice_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h)\
  $(gxdevsop_h) $(gscoord_h) $(gsform1_h) $(gspath_h) $(gxpath_h)\
  $(gzstate_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zform.$(OBJ) $(C_) $(PSSRC)zform.c

# ================================ PDF ================================ #

# We need nearly all of the PostScript LanguageLevel 3 interpreter for PDF,
# but not all of it: we could do without the arc operators (Level 1),
# the Encode filters (Level 2), and some LL3 features (clipsave/cliprestore,
# UseCIEColor, IdiomSets).  However, we've decided it isn't worth the
# trouble to do the fine-grain factoring to enable this, since code size
# is not at a premium for PDF interpreters.

# Because of the way the PDF encodings are defined, they must get loaded
# before we install the Level 2 resource machinery.
# On the other hand, the PDF .ps files must get loaded after
# level2dict is defined.
$(PSD)pdf.dev : $(ECHOGS_XE)\
 $(GLD)dps2lib.dev $(PSD)dps2read.dev\
 $(PSD)pdffonts.dev $(PSD)psl3.dev $(PSD)pdfread.dev $(PSD)cff.dev\
 $(PSD)fmd5.dev $(PSD)fsha2.dev $(PSD)farc4.dev $(PSD)faes.dev\
 $(PSD)ttfont.dev $(PSD)type2.dev $(PSD)pdfops.dev\
 $(PSD)pdf_r6.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdf -include $(PSD)psbase $(GLD)dps2lib
	$(ADDMOD) $(PSD)pdf -include $(PSD)dps2read $(PSD)pdffonts $(PSD)psl3
	$(ADDMOD) $(PSD)pdf -include $(GLD)psl2lib $(PSD)pdfread $(PSD)cff
	$(ADDMOD) $(PSD)pdf -include $(PSD)fmd5 $(PSD)fsha2
	$(ADDMOD) $(PSD)pdf -include $(PSD)farc4 $(PSD)faes.dev
	$(ADDMOD) $(PSD)pdf -include $(PSD)ttfont $(PSD)type2
	$(ADDMOD) $(PSD)pdf -include $(PSD)pdfops
	$(ADDMOD) $(PSD)pdf -include $(PSD)pdf_r6
	$(ADDMOD) $(PSD)pdf -functiontype 4
	$(ADDMOD) $(PSD)pdf -emulator PDF

# Reader only

$(PSD)pdffonts.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

$(PSD)pdfread.dev : $(ECHOGS_XE) \
 $(PSD)frsd.dev $(PSD)func4.dev $(PSD)fzlib.dev $(PSD)transpar.dev\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdfread -include $(PSD)frsd $(PSD)func4 $(PSD)fzlib
	$(ADDMOD) $(PSD)pdfread -include $(PSD)transpar
	$(ADDMOD) $(PSD)pdfread -ps pdf_ops
	$(ADDMOD) $(PSD)pdfread -ps pdf_rbld
	$(ADDMOD) $(PSD)pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec

# ---------------- PS Support for Font API ---------------- #
$(PSD)fapi_ps.dev : $(LIB_MAK) $(ECHOGS_XE) $(PSOBJ)zfapi.$(OBJ)\
 $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)fapi_ps $(PSOBJ)zfapi.$(OBJ)
	$(ADDMOD) $(PSD)fapi_ps -oper zfapi
	$(ADDMOD) $(PSD)fapi_ps -ps gs_fntem gs_fapi

$(PSOBJ)zfapi.$(OBJ) : $(PSSRC)zfapi.c $(OP) $(math__h) $(memory__h) $(string__h)\
 $(stat__h)\
 $(gp_h) $(gscoord_h) $(gscrypt1_h) $(gsfont_h) $(gspaint_h) $(gspath_h)\
 $(gxchar_h) $(gxchrout_h) $(gximask_h) $(gxdevice_h) $(gxfcache_h) $(gxfcid_h)\
 $(gxfont_h) $(gxfont1_h) $(gxpath_h) $(gzstate_h) \
 $(bfont_h) $(dstack_h) $(files_h) \
 $(ichar_h) $(idict_h) $(iddict_h) $(idparam_h) $(iname_h) $(ifont_h)\
 $(icid_h) $(igstate_h) $(icharout_h) $(ifapi_h) $(iplugin_h) \
 $(oper_h) $(store_h) $(stream_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zfapi.$(OBJ) $(C_) $(PSSRC)zfapi.c

# ---------------- Custom color dummy callback ---------------- #

$(PSOBJ)zncdummy.$(OBJ) : $(PSSRC)zncdummy.c $(OP) $(GX) $(math_h)\
  $(memory__h) $(gscdefs_h) $(gsnamecl_h) $(malloc__h) $(gsncdummy_h)\
  $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zncdummy.$(OBJ) $(C_) $(PSSRC)zncdummy.c

# ---------------- Custom operators for PDF interpreter ---------------- #

zpdfops_=$(PSOBJ)zpdfops.$(OBJ)
$(PSD)pdfops.dev : $(ECHOGS_XE) $(zpdfops_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdfops $(zpdfops_)
	$(ADDMOD) $(PSD)pdfops -oper zpdfops

$(PSOBJ)zpdfops.$(OBJ) : $(PSSRC)zpdfops.c $(OP) $(MAKEFILE)\
 $(igstate_h) $(istack_h) $(iutil_h) $(gspath_h) $(math__h) $(ialloc_h)\
 $(string__h) $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpdfops.$(OBJ) $(C_) $(PSSRC)zpdfops.c

zutf8_=$(PSOBJ)zutf8.$(OBJ)
$(PSD)utf8.dev : $(ECHOGS_XE) $(zutf8_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)utf8 $(zutf8_)
	$(ADDMOD) $(PSD)utf8 -oper zutf8

$(PSOBJ)zutf8.$(OBJ) : $(PSSRC)zutf8.c $(OP)\
 $(ghost_h) $(oper_h) $(iutil_h) $(ialloc_h) $(malloc__h) $(string__h)\
 $(store_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zutf8.$(OBJ) $(C_) $(PSSRC)zutf8.c

zpdf_r6_=$(PSOBJ)zpdf_r6.$(OBJ)
$(PSD)pdf_r6.dev : $(ECHOGS_XE) $(zpdf_r6_) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdf_r6 $(zpdf_r6_)
	$(ADDMOD) $(PSD)pdf_r6 -oper zpdf_r6

$(PSOBJ)zpdf_r6.$(OBJ) : $(PSSRC)zpdf_r6.c $(OP) $(MAKEFILE) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zpdf_r6.$(OBJ) $(C_) $(PSSRC)zpdf_r6.c

# ================ Dependencies for auxiliary programs ================ #

# ============================= Main program ============================== #

$(PSOBJ)gs.$(OBJ) : $(PSSRC)gs.c $(GH)\
 $(ierrors_h) $(iapi_h) $(imain_h) $(imainarg_h) $(iminst_h) $(gsmalloc_h)\
 $(locale__h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)gs.$(OBJ) $(C_) $(PSSRC)gs.c

$(PSOBJ)apitest.$(OBJ) : $(PSSRC)apitest.c $(GH)\
 $(ierrors_h) $(iapi_h) $(imain_h) $(imainarg_h) $(iminst_h) $(gsmalloc_h)\
 $(locale__h) $(gp_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)apitest.$(OBJ) $(C_) $(PSSRC)apitest.c

$(PSOBJ)iapi.$(OBJ) : $(PSSRC)iapi.c $(AK) $(psapi_h)\
 $(string__h) $(ierrors_h) $(gscdefs_h) $(gstypes_h) $(iapi_h)\
 $(iref_h) $(imain_h) $(imainarg_h) $(iminst_h) $(gslibctx_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)iapi.$(OBJ) $(C_) $(PSSRC)iapi.c

$(PSOBJ)psapi.$(OBJ) : $(PSSRC)psapi.c $(AK)\
 $(string__h) $(ierrors_h) $(gscdefs_h) $(gstypes_h) $(iapi_h)\
 $(iref_h) $(imain_h) $(imainarg_h) $(iminst_h) $(gslibctx_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)psapi.$(OBJ) $(C_) $(PSSRC)psapi.c

$(PSOBJ)icontext.$(OBJ) : $(PSSRC)icontext.c $(GH)\
 $(gsstruct_h) $(gxalloc_h)\
 $(dstack_h) $(ierrors_h) $(estack_h) $(files_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(interp_h) $(isave_h) $(store_h)\
 $(stream_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)icontext.$(OBJ) $(C_) $(PSSRC)icontext.c

gdevdsp_h=$(DEVSRCDIR)$(D)gdevdsp.h
gdevdsp2_h=$(DEVSRCDIR)$(D)gdevdsp2.h

$(PSOBJ)idisp.$(OBJ) : $(PSSRC)idisp.c $(OP) $(stdio__h) $(gp_h)\
 $(stdpre_h) $(gscdefs_h) $(gsdevice_h) $(gsmemory_h) $(gstypes_h)\
 $(iapi_h) $(iref_h)\
 $(imain_h) $(iminst_h) $(idisp_h) $(ostack_h)\
 $(gx_h) $(gxdevice_h) $(gxdevmem_h) $(gdevdsp_h) $(gdevdsp2_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(I_)$(DEVSRCDIR) $(PSO_)idisp.$(OBJ) $(C_) $(PSSRC)idisp.c

$(PSOBJ)imainarg.$(OBJ) : $(PSSRC)imainarg.c $(GH)\
 $(ctype__h) $(memory__h) $(string__h)\
 $(gp_h)\
 $(gsargs_h) $(gscdefs_h) $(gsdevice_h) $(gsmalloc_h) $(gsmdebug_h)\
 $(gspaint_h) $(gxclpage_h) $(gdevprn_h) $(gxdevice_h) $(gxdevmem_h)\
 $(ierrors_h) $(estack_h) $(files_h)\
 $(iapi_h) $(ialloc_h) $(iconf_h) $(imain_h) $(imainarg_h) $(iminst_h)\
 $(iname_h) $(interp_h) $(iscan_h) $(iutil_h) $(ivmspace_h)\
 $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h) \
 $(vdtrace_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)imainarg.$(OBJ) $(C_) $(PSSRC)imainarg.c

$(PSOBJ)imain.$(OBJ) : $(PSSRC)imain.c $(GH) $(memory__h) $(string__h)\
 $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmatrix_h) $(gsutil_h)\
 $(gspaint_h) $(gxclpage_h) $(gxalloc_h) $(gxdevice_h) $(gzstate_h)\
 $(dstack_h) $(ierrors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(iconf_h) $(idebug_h) $(iddict_h) $(idisp_h) $(iinit_h)\
 $(iname_h) $(interp_h) $(iplugin_h) $(isave_h) $(iscan_h) $(ivmspace_h)\
 $(iinit_h) $(main_h) $(oper_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h) $(zfile_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)imain.$(OBJ) $(C_) $(PSSRC)imain.c

#****** $(CCINT) interp.c
$(PSOBJ)interp.$(OBJ) : $(PSSRC)interp.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h) $(idebug_h)\
 $(dstack_h) $(ierrors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(iastruct_h) $(icontext_h) $(icremap_h) $(iddict_h) $(igstate_h)\
 $(iname_h) $(inamedef_h) $(interp_h) $(ipacked_h)\
 $(isave_h) $(iscan_h) $(istack_h) $(itoken_h) $(iutil_h) $(ivmspace_h)\
 $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(gpcheck_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)interp.$(OBJ) $(C_) $(PSSRC)interp.c

$(PSOBJ)ireclaim.$(OBJ) : $(PSSRC)ireclaim.c $(GH)\
 $(gsstruct_h)\
 $(iastate_h) $(icontext_h) $(interp_h) $(isave_h) $(isstate_h)\
 $(dstack_h) $(ierrors_h) $(estack_h) $(opdef_h) $(ostack_h) $(store_h)\
 $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ireclaim.$(OBJ) $(C_) $(PSSRC)ireclaim.c
