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

ierrors_h=$(PSSRC)ierrors.h
iconf_h=$(PSSRC)iconf.h
idebug_h=$(PSSRC)idebug.h
# Having iddstack.h at this level is unfortunate, but unavoidable.
iddstack_h=$(PSSRC)iddstack.h
idict_h=$(PSSRC)idict.h
idictdef_h=$(PSSRC)idictdef.h
idicttpl_h=$(PSSRC)idicttpl.h
idosave_h=$(PSSRC)idosave.h
igcstr_h=$(PSSRC)igcstr.h
inames_h=$(PSSRC)inames.h
iname_h=$(PSSRC)iname.h
inameidx_h=$(PSSRC)inameidx.h
inamestr_h=$(PSSRC)inamestr.h
ipacked_h=$(PSSRC)ipacked.h
iref_h=$(PSSRC)iref.h
isave_h=$(PSSRC)isave.h
isstate_h=$(PSSRC)isstate.h
istruct_h=$(PSSRC)istruct.h
iutil_h=$(PSSRC)iutil.h
ivmspace_h=$(PSSRC)ivmspace.h
opdef_h=$(PSSRC)opdef.h
# Nested include files
ghost_h=$(PSSRC)ghost.h
igc_h=$(PSSRC)igc.h
imemory_h=$(PSSRC)imemory.h
ialloc_h=$(PSSRC)ialloc.h
iastruct_h=$(PSSRC)iastruct.h
iastate_h=$(PSSRC)iastate.h
inamedef_h=$(PSSRC)inamedef.h
store_h=$(PSSRC)store.h
iplugin_h=$(PSSRC)iplugin.h
ifapi_h=$(PSSRC)ifapi.h
zht2_h=$(PSSRC)zht2.h
gen_ordered_h=$(GLSRC)gen_ordered.h
zchar42_h=$(PSSRC)zchar42.h
zfunc_h=$(PSSRC)zfunc.h

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

idparam_h=$(PSSRC)idparam.h
ilevel_h=$(PSSRC)ilevel.h
interp_h=$(PSSRC)interp.h
iparam_h=$(PSSRC)iparam.h
isdata_h=$(PSSRC)isdata.h
istack_h=$(PSSRC)istack.h
istkparm_h=$(PSSRC)istkparm.h
iutil2_h=$(PSSRC)iutil2.h
oparc_h=$(PSSRC)oparc.h
opcheck_h=$(PSSRC)opcheck.h
opextern_h=$(PSSRC)opextern.h
# Nested include files
idsdata_h=$(PSSRC)idsdata.h
idstack_h=$(PSSRC)idstack.h
iesdata_h=$(PSSRC)iesdata.h
iestack_h=$(PSSRC)iestack.h
iosdata_h=$(PSSRC)iosdata.h
iostack_h=$(PSSRC)iostack.h
icstate_h=$(PSSRC)icstate.h
iddict_h=$(PSSRC)iddict.h
dstack_h=$(PSSRC)dstack.h
estack_h=$(PSSRC)estack.h
ostack_h=$(PSSRC)ostack.h
oper_h=$(PSSRC)oper.h

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
 $(store_h) $(icstate_h) $(iname_h) $(dstack_h) $(idict_h) \
 $(INT_MAK) $(MAKEDIRS)
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
btoken_h=$(PSSRC)btoken.h
files_h=$(PSSRC)files.h
fname_h=$(PSSRC)fname.h
psapi_h=$(PSSRC)psapi.h
iapi_h=$(PSSRC)iapi.h
ichar_h=$(PSSRC)ichar.h
ichar1_h=$(PSSRC)ichar1.h
icharout_h=$(PSSRC)icharout.h
icolor_h=$(PSSRC)icolor.h
icremap_h=$(PSSRC)icremap.h
icsmap_h=$(PSSRC)icsmap.h
idisp_h=$(PSSRC)idisp.h
ifilter2_h=$(PSSRC)ifilter2.h
ifont_h=$(PSSRC)ifont.h
ifont1_h=$(PSSRC)ifont1.h
ifont2_h=$(PSSRC)ifont2.h
ifont42_h=$(PSSRC)ifont42.h
ifrpred_h=$(PSSRC)ifrpred.h
ifwpred_h=$(PSSRC)ifwpred.h
iht_h=$(PSSRC)iht.h
iimage_h=$(PSSRC)iimage.h
iinit_h=$(PSSRC)iinit.h
imain_h=$(PSSRC)imain.h
imainarg_h=$(PSSRC)imainarg.h
iminst_h=$(PSSRC)iminst.h
iparray_h=$(PSSRC)iparray.h
iscanbin_h=$(PSSRC)iscanbin.h
iscannum_h=$(PSSRC)iscannum.h
istream_h=$(PSSRC)istream.h
itoken_h=$(PSSRC)itoken.h
main_h=$(PSSRC)main.h
sbwbs_h=$(PSSRC)sbwbs.h
shcgen_h=$(PSSRC)shcgen.h
smtf_h=$(GLSRC)smtf.h
# Nested include files
bfont_h=$(PSSRC)bfont.h
icontext_h=$(PSSRC)icontext.h
ifilter_h=$(PSSRC)ifilter.h
igstate_h=$(PSSRC)igstate.h
iscan_h=$(PSSRC)iscan.h
sbhc_h=$(PSSRC)sbhc.h $(shc_h)
zfile_h=$(PSSRC)zfile.h
# Include files for optional features
ibnum_h=$(PSSRC)ibnum.h
zcolor_h=$(PSSRC)zcolor.h
zcie_h=$(PSSRC)zcie.h
zicc_h=$(PSSRC)zicc.h
zfrsd_h=$(PSSRC)zfrsd.h

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
	$(PSCC) $(ENABLE_URF) $(URF_INCLUDE) $(PSO_)zfilter.$(OBJ) $(C_) $(PSSRC)zfilter.c

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
 $(gspaint_h) $(igstate_h) $(store_h) $(estack_h) $(INT_MAK) $(MAKEDIRS)
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

dscparse_h=$(PSSRC)dscparse.h

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

ifunc_h=$(PSSRC)ifunc.h

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

ivmem2_h=$(PSSRC)ivmem2.h

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

psl2read_=$(PSOBJ)zcolor2.$(OBJ) $(PSOBJ)zcsindex.$(OBJ) $(PSOBJ)zht2.$(OBJ)
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
 $(iht_h) $(store_h) $(iname_h) $(zht2_h) $(gxgstate_h) $(gp_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)zht2.$(OBJ) $(II)$(GENORDERED_SRCDIR) $(C_) $(PSSRC)zht2.c

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

$(PSD)jbig2_.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jbig2_

$(PSD)jbig2.dev : $(PSD)jbig2_$(JBIG2_LIB).dev $(INT_MAK) $(MAKEDIRS)
	$(CP_) $(PSD)jbig2_$(JBIG2_LIB).dev $(PSD)jbig2.dev


$(PSOBJ)zfjbig2_jbig2dec.$(OBJ) : $(PSSRC)zfjbig2.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(sjbig2_h) $(INT_MAK) $(MAKEDIRS)
	$(PSJBIG2CC) $(PSO_)zfjbig2_jbig2dec.$(OBJ) $(C_) $(PSSRC)zfjbig2.c

# JPX (jpeg 2000) compression filter
# this can be turned on and off with a FEATURE_DEV

$(PSD)jpx.dev : $(ECHOGS_XE) $(PSD)jpx_$(JPX_LIB).dev\
 $(INT_MAK) $(MAKEDIRS)
	$(CP_) $(PSD)jpx_$(JPX_LIB).dev $(PSD)jpx.dev

$(PSD)jpx_.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)jpx_

$(PSOBJ)zfjpx.$(OBJ) : $(PSSRC)zfjpx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(gstypes_h) $(ialloc_h) $(idict_h) $(ifilter_h)\
 $(store_h) $(stream_h) $(strimpl_h) $(ialloc_h) $(iname_h)\
 $(gdebug_h) $(sjpx_h) $(INT_MAK) $(MAKEDIRS)
	$(PSJASCC) $(PSO_)zfjpx.$(OBJ) $(C_) $(PSSRC)zfjpx.c

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
	$(ADDMOD) $(PSD)upath -oper zupath_l2 zupath

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

icid_h=$(PSSRC)icid.h
ifcid_h=$(PSSRC)ifcid.h

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

icie_h=$(PSSRC)icie.h

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

ipcolor_h=$(PSSRC)ipcolor.h

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
 $(idparam_h) $(idict_h) $(igstate_h) $(iimage_h)\
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
 $(idict_h) $(idstack_h) $(idparam_h) $(ifunc_h) $(igstate_h) $(iimage_h) $(iname_h)\
 $(store_h) $(gdevdevn_h)  $(gxdevsop_h) $(gxblend_h) $(gdevp14_h)\
 $(gsicc_cms_h) $(INT_MAK) $(MAKEDIRS)
	$(PSCC) $(PSO_)ztrans.$(OBJ) $(C_) $(PSSRC)ztrans.c

# ---------------- ICCBased color spaces ---------------- #

iccread_=$(PSOBJ)zicc.$(OBJ)
$(PSD)icc.dev : $(ECHOGS_XE) $(PSD)cie.dev $(iccread_) \
                $(GLD)sicclib.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)icc $(iccread_)
	$(ADDMOD) $(PSD)icc -oper zicc
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
gsform1_h=$(GLSRC)gsform1.h
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
 $(PSD)pdffonts.dev $(PSD)psl3.dev $(PSD)pdfread.dev\
 $(PSD)fmd5.dev $(PSD)fsha2.dev $(PSD)farc4.dev $(PSD)faes.dev\
 $(PSD)type2.dev $(PSD)pdfops.dev\
 $(PSD)pdf_r6.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdf -include $(PSD)psbase $(GLD)dps2lib
	$(ADDMOD) $(PSD)pdf -include $(PSD)dps2read $(PSD)pdffonts $(PSD)psl3
	$(ADDMOD) $(PSD)pdf -include $(GLD)psl2lib $(PSD)pdfread
	$(ADDMOD) $(PSD)pdf -include $(PSD)fmd5 $(PSD)fsha2
	$(ADDMOD) $(PSD)pdf -include $(PSD)farc4 $(PSD)faes.dev
	$(ADDMOD) $(PSD)pdf -include $(PSD)type2
	$(ADDMOD) $(PSD)pdf -include $(PSD)pdfops
	$(ADDMOD) $(PSD)pdf -include $(PSD)pdf_r6
	$(ADDMOD) $(PSD)pdf -functiontype 4
	$(ADDMOD) $(PSD)pdf -emulator PDF

# Reader only

$(PSD)pdffonts.dev : $(ECHOGS_XE) $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

$(PSD)pdfread.dev : $(ECHOGS_XE) \
 $(PSD)frsd.dev $(PSD)func4.dev $(PSD)fzlib.dev $(PSD)transpar.dev $(PSD)cff.dev\
 $(PSD)ttfont.dev $(INT_MAK) $(MAKEDIRS)
	$(SETMOD) $(PSD)pdfread -include $(PSD)frsd $(PSD)func4 $(PSD)fzlib
	$(ADDMOD) $(PSD)pdfread -include $(PSD)transpar
	$(ADDMOD) $(PSD)pdfread -ps pdf_ops
	$(ADDMOD) $(PSD)pdfread -ps pdf_rbld
	$(ADDMOD) $(PSD)pdfread -ps pdf_base pdf_draw
	$(ADDMOD) $(PSD)pdfread -include $(PSD)cff
	$(ADDMOD) $(PSD)pdfread -include $(PSD)ttfont
	$(ADDMOD) $(PSD)pdfread -ps pdf_font pdf_main pdf_sec

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
 $(ghost_h) $(gsmchunk_h) $(oper_h) \
 $(igstate_h) $(istack_h) $(iutil_h) $(gspath_h) $(math__h) $(ialloc_h)\
 $(string__h) $(store_h) $(iminst_h) $(idstack_h) $(INT_MAK) $(MAKEDIRS)
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
 $(gsstate_h) $(icstate_h) $(INT_MAK) $(MAKEDIRS)
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
 $(iapi_h) $(iref_h) $(gxgstate_h) $(gxdevsop_h)\
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

# Dependencies:
$(PSSRC)ierrors.h:$(GLSRC)gserrors.h
$(PSSRC)iconf.h:$(GLSRC)gxiodev.h
$(PSSRC)iconf.h:$(GLSRC)gsparam.h
$(PSSRC)iconf.h:$(GLSRC)gp.h
$(PSSRC)iconf.h:$(GLSRC)memory_.h
$(PSSRC)iconf.h:$(GLSRC)srdline.h
$(PSSRC)iconf.h:$(GLSRC)scommon.h
$(PSSRC)iconf.h:$(GLSRC)gsfname.h
$(PSSRC)iconf.h:$(GLSRC)stat_.h
$(PSSRC)iconf.h:$(GLSRC)gsstype.h
$(PSSRC)iconf.h:$(GLSRC)gsmemory.h
$(PSSRC)iconf.h:$(GLSRC)gpgetenv.h
$(PSSRC)iconf.h:$(GLSRC)gscdefs.h
$(PSSRC)iconf.h:$(GLSRC)gslibctx.h
$(PSSRC)iconf.h:$(GLSRC)stdio_.h
$(PSSRC)iconf.h:$(GLSRC)stdint_.h
$(PSSRC)iconf.h:$(GLSRC)gssprintf.h
$(PSSRC)iconf.h:$(GLSRC)gstypes.h
$(PSSRC)iconf.h:$(GLSRC)std.h
$(PSSRC)iconf.h:$(GLSRC)stdpre.h
$(PSSRC)iconf.h:$(GLGEN)arch.h
$(PSSRC)iconf.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idebug.h:$(PSSRC)iref.h
$(PSSRC)idebug.h:$(GLSRC)gxalloc.h
$(PSSRC)idebug.h:$(GLSRC)gxobj.h
$(PSSRC)idebug.h:$(GLSRC)gsnamecl.h
$(PSSRC)idebug.h:$(GLSRC)gxcspace.h
$(PSSRC)idebug.h:$(GLSRC)gscsel.h
$(PSSRC)idebug.h:$(GLSRC)gsdcolor.h
$(PSSRC)idebug.h:$(GLSRC)gxfrac.h
$(PSSRC)idebug.h:$(GLSRC)gscms.h
$(PSSRC)idebug.h:$(GLSRC)gsdevice.h
$(PSSRC)idebug.h:$(GLSRC)gscspace.h
$(PSSRC)idebug.h:$(GLSRC)gsgstate.h
$(PSSRC)idebug.h:$(GLSRC)gsiparam.h
$(PSSRC)idebug.h:$(GLSRC)gsmatrix.h
$(PSSRC)idebug.h:$(GLSRC)gxhttile.h
$(PSSRC)idebug.h:$(GLSRC)gsparam.h
$(PSSRC)idebug.h:$(GLSRC)gsrefct.h
$(PSSRC)idebug.h:$(GLSRC)memento.h
$(PSSRC)idebug.h:$(GLSRC)gsstruct.h
$(PSSRC)idebug.h:$(GLSRC)gxsync.h
$(PSSRC)idebug.h:$(GLSRC)gxbitmap.h
$(PSSRC)idebug.h:$(GLSRC)scommon.h
$(PSSRC)idebug.h:$(GLSRC)gsbitmap.h
$(PSSRC)idebug.h:$(GLSRC)gsccolor.h
$(PSSRC)idebug.h:$(GLSRC)gxarith.h
$(PSSRC)idebug.h:$(GLSRC)gpsync.h
$(PSSRC)idebug.h:$(GLSRC)gsstype.h
$(PSSRC)idebug.h:$(GLSRC)gsmemory.h
$(PSSRC)idebug.h:$(GLSRC)gslibctx.h
$(PSSRC)idebug.h:$(GLSRC)gsalloc.h
$(PSSRC)idebug.h:$(GLSRC)gxcindex.h
$(PSSRC)idebug.h:$(GLSRC)stdio_.h
$(PSSRC)idebug.h:$(GLSRC)stdint_.h
$(PSSRC)idebug.h:$(GLSRC)gssprintf.h
$(PSSRC)idebug.h:$(GLSRC)gstypes.h
$(PSSRC)idebug.h:$(GLSRC)std.h
$(PSSRC)idebug.h:$(GLSRC)stdpre.h
$(PSSRC)idebug.h:$(GLGEN)arch.h
$(PSSRC)idebug.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iddstack.h:$(PSSRC)iref.h
$(PSSRC)iddstack.h:$(GLSRC)gxalloc.h
$(PSSRC)iddstack.h:$(GLSRC)gxobj.h
$(PSSRC)iddstack.h:$(GLSRC)gsnamecl.h
$(PSSRC)iddstack.h:$(GLSRC)gxcspace.h
$(PSSRC)iddstack.h:$(GLSRC)gscsel.h
$(PSSRC)iddstack.h:$(GLSRC)gsdcolor.h
$(PSSRC)iddstack.h:$(GLSRC)gxfrac.h
$(PSSRC)iddstack.h:$(GLSRC)gscms.h
$(PSSRC)iddstack.h:$(GLSRC)gsdevice.h
$(PSSRC)iddstack.h:$(GLSRC)gscspace.h
$(PSSRC)iddstack.h:$(GLSRC)gsgstate.h
$(PSSRC)iddstack.h:$(GLSRC)gsiparam.h
$(PSSRC)iddstack.h:$(GLSRC)gsmatrix.h
$(PSSRC)iddstack.h:$(GLSRC)gxhttile.h
$(PSSRC)iddstack.h:$(GLSRC)gsparam.h
$(PSSRC)iddstack.h:$(GLSRC)gsrefct.h
$(PSSRC)iddstack.h:$(GLSRC)memento.h
$(PSSRC)iddstack.h:$(GLSRC)gsstruct.h
$(PSSRC)iddstack.h:$(GLSRC)gxsync.h
$(PSSRC)iddstack.h:$(GLSRC)gxbitmap.h
$(PSSRC)iddstack.h:$(GLSRC)scommon.h
$(PSSRC)iddstack.h:$(GLSRC)gsbitmap.h
$(PSSRC)iddstack.h:$(GLSRC)gsccolor.h
$(PSSRC)iddstack.h:$(GLSRC)gxarith.h
$(PSSRC)iddstack.h:$(GLSRC)gpsync.h
$(PSSRC)iddstack.h:$(GLSRC)gsstype.h
$(PSSRC)iddstack.h:$(GLSRC)gsmemory.h
$(PSSRC)iddstack.h:$(GLSRC)gslibctx.h
$(PSSRC)iddstack.h:$(GLSRC)gsalloc.h
$(PSSRC)iddstack.h:$(GLSRC)gxcindex.h
$(PSSRC)iddstack.h:$(GLSRC)stdio_.h
$(PSSRC)iddstack.h:$(GLSRC)stdint_.h
$(PSSRC)iddstack.h:$(GLSRC)gssprintf.h
$(PSSRC)iddstack.h:$(GLSRC)gstypes.h
$(PSSRC)iddstack.h:$(GLSRC)std.h
$(PSSRC)iddstack.h:$(GLSRC)stdpre.h
$(PSSRC)iddstack.h:$(GLGEN)arch.h
$(PSSRC)iddstack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idict.h:$(PSSRC)iddstack.h
$(PSSRC)idict.h:$(PSSRC)iref.h
$(PSSRC)idict.h:$(GLSRC)gxalloc.h
$(PSSRC)idict.h:$(GLSRC)gxobj.h
$(PSSRC)idict.h:$(GLSRC)gsnamecl.h
$(PSSRC)idict.h:$(GLSRC)gxcspace.h
$(PSSRC)idict.h:$(GLSRC)gscsel.h
$(PSSRC)idict.h:$(GLSRC)gsdcolor.h
$(PSSRC)idict.h:$(GLSRC)gxfrac.h
$(PSSRC)idict.h:$(GLSRC)gscms.h
$(PSSRC)idict.h:$(GLSRC)gsdevice.h
$(PSSRC)idict.h:$(GLSRC)gscspace.h
$(PSSRC)idict.h:$(GLSRC)gsgstate.h
$(PSSRC)idict.h:$(GLSRC)gsiparam.h
$(PSSRC)idict.h:$(GLSRC)gsmatrix.h
$(PSSRC)idict.h:$(GLSRC)gxhttile.h
$(PSSRC)idict.h:$(GLSRC)gsparam.h
$(PSSRC)idict.h:$(GLSRC)gsrefct.h
$(PSSRC)idict.h:$(GLSRC)memento.h
$(PSSRC)idict.h:$(GLSRC)gsstruct.h
$(PSSRC)idict.h:$(GLSRC)gxsync.h
$(PSSRC)idict.h:$(GLSRC)gxbitmap.h
$(PSSRC)idict.h:$(GLSRC)scommon.h
$(PSSRC)idict.h:$(GLSRC)gsbitmap.h
$(PSSRC)idict.h:$(GLSRC)gsccolor.h
$(PSSRC)idict.h:$(GLSRC)gxarith.h
$(PSSRC)idict.h:$(GLSRC)gpsync.h
$(PSSRC)idict.h:$(GLSRC)gsstype.h
$(PSSRC)idict.h:$(GLSRC)gsmemory.h
$(PSSRC)idict.h:$(GLSRC)gslibctx.h
$(PSSRC)idict.h:$(GLSRC)gsalloc.h
$(PSSRC)idict.h:$(GLSRC)gxcindex.h
$(PSSRC)idict.h:$(GLSRC)stdio_.h
$(PSSRC)idict.h:$(GLSRC)stdint_.h
$(PSSRC)idict.h:$(GLSRC)gssprintf.h
$(PSSRC)idict.h:$(GLSRC)gstypes.h
$(PSSRC)idict.h:$(GLSRC)std.h
$(PSSRC)idict.h:$(GLSRC)stdpre.h
$(PSSRC)idict.h:$(GLGEN)arch.h
$(PSSRC)idict.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idosave.h:$(PSSRC)imemory.h
$(PSSRC)idosave.h:$(PSSRC)ivmspace.h
$(PSSRC)idosave.h:$(PSSRC)iref.h
$(PSSRC)idosave.h:$(GLSRC)gsgc.h
$(PSSRC)idosave.h:$(GLSRC)gxalloc.h
$(PSSRC)idosave.h:$(GLSRC)gxobj.h
$(PSSRC)idosave.h:$(GLSRC)gsnamecl.h
$(PSSRC)idosave.h:$(GLSRC)gxcspace.h
$(PSSRC)idosave.h:$(GLSRC)gscsel.h
$(PSSRC)idosave.h:$(GLSRC)gsdcolor.h
$(PSSRC)idosave.h:$(GLSRC)gxfrac.h
$(PSSRC)idosave.h:$(GLSRC)gscms.h
$(PSSRC)idosave.h:$(GLSRC)gsdevice.h
$(PSSRC)idosave.h:$(GLSRC)gscspace.h
$(PSSRC)idosave.h:$(GLSRC)gsgstate.h
$(PSSRC)idosave.h:$(GLSRC)gsiparam.h
$(PSSRC)idosave.h:$(GLSRC)gsmatrix.h
$(PSSRC)idosave.h:$(GLSRC)gxhttile.h
$(PSSRC)idosave.h:$(GLSRC)gsparam.h
$(PSSRC)idosave.h:$(GLSRC)gsrefct.h
$(PSSRC)idosave.h:$(GLSRC)memento.h
$(PSSRC)idosave.h:$(GLSRC)gsstruct.h
$(PSSRC)idosave.h:$(GLSRC)gxsync.h
$(PSSRC)idosave.h:$(GLSRC)gxbitmap.h
$(PSSRC)idosave.h:$(GLSRC)scommon.h
$(PSSRC)idosave.h:$(GLSRC)gsbitmap.h
$(PSSRC)idosave.h:$(GLSRC)gsccolor.h
$(PSSRC)idosave.h:$(GLSRC)gxarith.h
$(PSSRC)idosave.h:$(GLSRC)gpsync.h
$(PSSRC)idosave.h:$(GLSRC)gsstype.h
$(PSSRC)idosave.h:$(GLSRC)gsmemory.h
$(PSSRC)idosave.h:$(GLSRC)gslibctx.h
$(PSSRC)idosave.h:$(GLSRC)gsalloc.h
$(PSSRC)idosave.h:$(GLSRC)gxcindex.h
$(PSSRC)idosave.h:$(GLSRC)stdio_.h
$(PSSRC)idosave.h:$(GLSRC)stdint_.h
$(PSSRC)idosave.h:$(GLSRC)gssprintf.h
$(PSSRC)idosave.h:$(GLSRC)gstypes.h
$(PSSRC)idosave.h:$(GLSRC)std.h
$(PSSRC)idosave.h:$(GLSRC)stdpre.h
$(PSSRC)idosave.h:$(GLGEN)arch.h
$(PSSRC)idosave.h:$(GLSRC)gs_dll_call.h
$(PSSRC)igcstr.h:$(GLSRC)gxalloc.h
$(PSSRC)igcstr.h:$(GLSRC)gxobj.h
$(PSSRC)igcstr.h:$(GLSRC)gsstruct.h
$(PSSRC)igcstr.h:$(GLSRC)gxbitmap.h
$(PSSRC)igcstr.h:$(GLSRC)scommon.h
$(PSSRC)igcstr.h:$(GLSRC)gsbitmap.h
$(PSSRC)igcstr.h:$(GLSRC)gsstype.h
$(PSSRC)igcstr.h:$(GLSRC)gsmemory.h
$(PSSRC)igcstr.h:$(GLSRC)gslibctx.h
$(PSSRC)igcstr.h:$(GLSRC)gsalloc.h
$(PSSRC)igcstr.h:$(GLSRC)stdio_.h
$(PSSRC)igcstr.h:$(GLSRC)stdint_.h
$(PSSRC)igcstr.h:$(GLSRC)gssprintf.h
$(PSSRC)igcstr.h:$(GLSRC)gstypes.h
$(PSSRC)igcstr.h:$(GLSRC)std.h
$(PSSRC)igcstr.h:$(GLSRC)stdpre.h
$(PSSRC)igcstr.h:$(GLGEN)arch.h
$(PSSRC)igcstr.h:$(GLSRC)gs_dll_call.h
$(PSSRC)inames.h:$(PSSRC)imemory.h
$(PSSRC)inames.h:$(PSSRC)ivmspace.h
$(PSSRC)inames.h:$(PSSRC)iref.h
$(PSSRC)inames.h:$(GLSRC)gsgc.h
$(PSSRC)inames.h:$(GLSRC)gxalloc.h
$(PSSRC)inames.h:$(GLSRC)gxobj.h
$(PSSRC)inames.h:$(GLSRC)gsnamecl.h
$(PSSRC)inames.h:$(GLSRC)gxcspace.h
$(PSSRC)inames.h:$(GLSRC)gscsel.h
$(PSSRC)inames.h:$(GLSRC)gsdcolor.h
$(PSSRC)inames.h:$(GLSRC)gxfrac.h
$(PSSRC)inames.h:$(GLSRC)gscms.h
$(PSSRC)inames.h:$(GLSRC)gsdevice.h
$(PSSRC)inames.h:$(GLSRC)gscspace.h
$(PSSRC)inames.h:$(GLSRC)gsgstate.h
$(PSSRC)inames.h:$(GLSRC)gsiparam.h
$(PSSRC)inames.h:$(GLSRC)gsmatrix.h
$(PSSRC)inames.h:$(GLSRC)gxhttile.h
$(PSSRC)inames.h:$(GLSRC)gsparam.h
$(PSSRC)inames.h:$(GLSRC)gsrefct.h
$(PSSRC)inames.h:$(GLSRC)memento.h
$(PSSRC)inames.h:$(GLSRC)gsstruct.h
$(PSSRC)inames.h:$(GLSRC)gxsync.h
$(PSSRC)inames.h:$(GLSRC)gxbitmap.h
$(PSSRC)inames.h:$(GLSRC)scommon.h
$(PSSRC)inames.h:$(GLSRC)gsbitmap.h
$(PSSRC)inames.h:$(GLSRC)gsccolor.h
$(PSSRC)inames.h:$(GLSRC)gxarith.h
$(PSSRC)inames.h:$(GLSRC)gpsync.h
$(PSSRC)inames.h:$(GLSRC)gsstype.h
$(PSSRC)inames.h:$(GLSRC)gsmemory.h
$(PSSRC)inames.h:$(GLSRC)gslibctx.h
$(PSSRC)inames.h:$(GLSRC)gsalloc.h
$(PSSRC)inames.h:$(GLSRC)gxcindex.h
$(PSSRC)inames.h:$(GLSRC)stdio_.h
$(PSSRC)inames.h:$(GLSRC)stdint_.h
$(PSSRC)inames.h:$(GLSRC)gssprintf.h
$(PSSRC)inames.h:$(GLSRC)gstypes.h
$(PSSRC)inames.h:$(GLSRC)std.h
$(PSSRC)inames.h:$(GLSRC)stdpre.h
$(PSSRC)inames.h:$(GLGEN)arch.h
$(PSSRC)inames.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iname.h:$(PSSRC)inames.h
$(PSSRC)iname.h:$(PSSRC)imemory.h
$(PSSRC)iname.h:$(PSSRC)ivmspace.h
$(PSSRC)iname.h:$(PSSRC)iref.h
$(PSSRC)iname.h:$(GLSRC)gsgc.h
$(PSSRC)iname.h:$(GLSRC)gxalloc.h
$(PSSRC)iname.h:$(GLSRC)gxobj.h
$(PSSRC)iname.h:$(GLSRC)gsnamecl.h
$(PSSRC)iname.h:$(GLSRC)gxcspace.h
$(PSSRC)iname.h:$(GLSRC)gscsel.h
$(PSSRC)iname.h:$(GLSRC)gsdcolor.h
$(PSSRC)iname.h:$(GLSRC)gxfrac.h
$(PSSRC)iname.h:$(GLSRC)gscms.h
$(PSSRC)iname.h:$(GLSRC)gsdevice.h
$(PSSRC)iname.h:$(GLSRC)gscspace.h
$(PSSRC)iname.h:$(GLSRC)gsgstate.h
$(PSSRC)iname.h:$(GLSRC)gsiparam.h
$(PSSRC)iname.h:$(GLSRC)gsmatrix.h
$(PSSRC)iname.h:$(GLSRC)gxhttile.h
$(PSSRC)iname.h:$(GLSRC)gsparam.h
$(PSSRC)iname.h:$(GLSRC)gsrefct.h
$(PSSRC)iname.h:$(GLSRC)memento.h
$(PSSRC)iname.h:$(GLSRC)gsstruct.h
$(PSSRC)iname.h:$(GLSRC)gxsync.h
$(PSSRC)iname.h:$(GLSRC)gxbitmap.h
$(PSSRC)iname.h:$(GLSRC)scommon.h
$(PSSRC)iname.h:$(GLSRC)gsbitmap.h
$(PSSRC)iname.h:$(GLSRC)gsccolor.h
$(PSSRC)iname.h:$(GLSRC)gxarith.h
$(PSSRC)iname.h:$(GLSRC)gpsync.h
$(PSSRC)iname.h:$(GLSRC)gsstype.h
$(PSSRC)iname.h:$(GLSRC)gsmemory.h
$(PSSRC)iname.h:$(GLSRC)gslibctx.h
$(PSSRC)iname.h:$(GLSRC)gsalloc.h
$(PSSRC)iname.h:$(GLSRC)gxcindex.h
$(PSSRC)iname.h:$(GLSRC)stdio_.h
$(PSSRC)iname.h:$(GLSRC)stdint_.h
$(PSSRC)iname.h:$(GLSRC)gssprintf.h
$(PSSRC)iname.h:$(GLSRC)gstypes.h
$(PSSRC)iname.h:$(GLSRC)std.h
$(PSSRC)iname.h:$(GLSRC)stdpre.h
$(PSSRC)iname.h:$(GLGEN)arch.h
$(PSSRC)iname.h:$(GLSRC)gs_dll_call.h
$(PSSRC)inamestr.h:$(PSSRC)inameidx.h
$(PSSRC)inamestr.h:$(GLSRC)stdpre.h
$(PSSRC)iref.h:$(GLSRC)gxalloc.h
$(PSSRC)iref.h:$(GLSRC)gxobj.h
$(PSSRC)iref.h:$(GLSRC)gsnamecl.h
$(PSSRC)iref.h:$(GLSRC)gxcspace.h
$(PSSRC)iref.h:$(GLSRC)gscsel.h
$(PSSRC)iref.h:$(GLSRC)gsdcolor.h
$(PSSRC)iref.h:$(GLSRC)gxfrac.h
$(PSSRC)iref.h:$(GLSRC)gscms.h
$(PSSRC)iref.h:$(GLSRC)gsdevice.h
$(PSSRC)iref.h:$(GLSRC)gscspace.h
$(PSSRC)iref.h:$(GLSRC)gsgstate.h
$(PSSRC)iref.h:$(GLSRC)gsiparam.h
$(PSSRC)iref.h:$(GLSRC)gsmatrix.h
$(PSSRC)iref.h:$(GLSRC)gxhttile.h
$(PSSRC)iref.h:$(GLSRC)gsparam.h
$(PSSRC)iref.h:$(GLSRC)gsrefct.h
$(PSSRC)iref.h:$(GLSRC)memento.h
$(PSSRC)iref.h:$(GLSRC)gsstruct.h
$(PSSRC)iref.h:$(GLSRC)gxsync.h
$(PSSRC)iref.h:$(GLSRC)gxbitmap.h
$(PSSRC)iref.h:$(GLSRC)scommon.h
$(PSSRC)iref.h:$(GLSRC)gsbitmap.h
$(PSSRC)iref.h:$(GLSRC)gsccolor.h
$(PSSRC)iref.h:$(GLSRC)gxarith.h
$(PSSRC)iref.h:$(GLSRC)gpsync.h
$(PSSRC)iref.h:$(GLSRC)gsstype.h
$(PSSRC)iref.h:$(GLSRC)gsmemory.h
$(PSSRC)iref.h:$(GLSRC)gslibctx.h
$(PSSRC)iref.h:$(GLSRC)gsalloc.h
$(PSSRC)iref.h:$(GLSRC)gxcindex.h
$(PSSRC)iref.h:$(GLSRC)stdio_.h
$(PSSRC)iref.h:$(GLSRC)stdint_.h
$(PSSRC)iref.h:$(GLSRC)gssprintf.h
$(PSSRC)iref.h:$(GLSRC)gstypes.h
$(PSSRC)iref.h:$(GLSRC)std.h
$(PSSRC)iref.h:$(GLSRC)stdpre.h
$(PSSRC)iref.h:$(GLGEN)arch.h
$(PSSRC)iref.h:$(GLSRC)gs_dll_call.h
$(PSSRC)isave.h:$(PSSRC)idosave.h
$(PSSRC)isave.h:$(PSSRC)imemory.h
$(PSSRC)isave.h:$(PSSRC)ivmspace.h
$(PSSRC)isave.h:$(PSSRC)iref.h
$(PSSRC)isave.h:$(GLSRC)gsgc.h
$(PSSRC)isave.h:$(GLSRC)gxalloc.h
$(PSSRC)isave.h:$(GLSRC)gxobj.h
$(PSSRC)isave.h:$(GLSRC)gsnamecl.h
$(PSSRC)isave.h:$(GLSRC)gxcspace.h
$(PSSRC)isave.h:$(GLSRC)gscsel.h
$(PSSRC)isave.h:$(GLSRC)gsdcolor.h
$(PSSRC)isave.h:$(GLSRC)gxfrac.h
$(PSSRC)isave.h:$(GLSRC)gscms.h
$(PSSRC)isave.h:$(GLSRC)gsdevice.h
$(PSSRC)isave.h:$(GLSRC)gscspace.h
$(PSSRC)isave.h:$(GLSRC)gsgstate.h
$(PSSRC)isave.h:$(GLSRC)gsiparam.h
$(PSSRC)isave.h:$(GLSRC)gsmatrix.h
$(PSSRC)isave.h:$(GLSRC)gxhttile.h
$(PSSRC)isave.h:$(GLSRC)gsparam.h
$(PSSRC)isave.h:$(GLSRC)gsrefct.h
$(PSSRC)isave.h:$(GLSRC)memento.h
$(PSSRC)isave.h:$(GLSRC)gsstruct.h
$(PSSRC)isave.h:$(GLSRC)gxsync.h
$(PSSRC)isave.h:$(GLSRC)gxbitmap.h
$(PSSRC)isave.h:$(GLSRC)scommon.h
$(PSSRC)isave.h:$(GLSRC)gsbitmap.h
$(PSSRC)isave.h:$(GLSRC)gsccolor.h
$(PSSRC)isave.h:$(GLSRC)gxarith.h
$(PSSRC)isave.h:$(GLSRC)gpsync.h
$(PSSRC)isave.h:$(GLSRC)gsstype.h
$(PSSRC)isave.h:$(GLSRC)gsmemory.h
$(PSSRC)isave.h:$(GLSRC)gslibctx.h
$(PSSRC)isave.h:$(GLSRC)gsalloc.h
$(PSSRC)isave.h:$(GLSRC)gxcindex.h
$(PSSRC)isave.h:$(GLSRC)stdio_.h
$(PSSRC)isave.h:$(GLSRC)stdint_.h
$(PSSRC)isave.h:$(GLSRC)gssprintf.h
$(PSSRC)isave.h:$(GLSRC)gstypes.h
$(PSSRC)isave.h:$(GLSRC)std.h
$(PSSRC)isave.h:$(GLSRC)stdpre.h
$(PSSRC)isave.h:$(GLGEN)arch.h
$(PSSRC)isave.h:$(GLSRC)gs_dll_call.h
$(PSSRC)isstate.h:$(GLSRC)gsgc.h
$(PSSRC)isstate.h:$(GLSRC)gxalloc.h
$(PSSRC)isstate.h:$(GLSRC)gxobj.h
$(PSSRC)isstate.h:$(GLSRC)gsstruct.h
$(PSSRC)isstate.h:$(GLSRC)gxbitmap.h
$(PSSRC)isstate.h:$(GLSRC)scommon.h
$(PSSRC)isstate.h:$(GLSRC)gsbitmap.h
$(PSSRC)isstate.h:$(GLSRC)gsstype.h
$(PSSRC)isstate.h:$(GLSRC)gsmemory.h
$(PSSRC)isstate.h:$(GLSRC)gslibctx.h
$(PSSRC)isstate.h:$(GLSRC)gsalloc.h
$(PSSRC)isstate.h:$(GLSRC)stdio_.h
$(PSSRC)isstate.h:$(GLSRC)stdint_.h
$(PSSRC)isstate.h:$(GLSRC)gssprintf.h
$(PSSRC)isstate.h:$(GLSRC)gstypes.h
$(PSSRC)isstate.h:$(GLSRC)std.h
$(PSSRC)isstate.h:$(GLSRC)stdpre.h
$(PSSRC)isstate.h:$(GLGEN)arch.h
$(PSSRC)isstate.h:$(GLSRC)gs_dll_call.h
$(PSSRC)istruct.h:$(PSSRC)iref.h
$(PSSRC)istruct.h:$(GLSRC)gxalloc.h
$(PSSRC)istruct.h:$(GLSRC)gxobj.h
$(PSSRC)istruct.h:$(GLSRC)gsnamecl.h
$(PSSRC)istruct.h:$(GLSRC)gxcspace.h
$(PSSRC)istruct.h:$(GLSRC)gscsel.h
$(PSSRC)istruct.h:$(GLSRC)gsdcolor.h
$(PSSRC)istruct.h:$(GLSRC)gxfrac.h
$(PSSRC)istruct.h:$(GLSRC)gscms.h
$(PSSRC)istruct.h:$(GLSRC)gsdevice.h
$(PSSRC)istruct.h:$(GLSRC)gscspace.h
$(PSSRC)istruct.h:$(GLSRC)gsgstate.h
$(PSSRC)istruct.h:$(GLSRC)gsiparam.h
$(PSSRC)istruct.h:$(GLSRC)gsmatrix.h
$(PSSRC)istruct.h:$(GLSRC)gxhttile.h
$(PSSRC)istruct.h:$(GLSRC)gsparam.h
$(PSSRC)istruct.h:$(GLSRC)gsrefct.h
$(PSSRC)istruct.h:$(GLSRC)memento.h
$(PSSRC)istruct.h:$(GLSRC)gsstruct.h
$(PSSRC)istruct.h:$(GLSRC)gxsync.h
$(PSSRC)istruct.h:$(GLSRC)gxbitmap.h
$(PSSRC)istruct.h:$(GLSRC)scommon.h
$(PSSRC)istruct.h:$(GLSRC)gsbitmap.h
$(PSSRC)istruct.h:$(GLSRC)gsccolor.h
$(PSSRC)istruct.h:$(GLSRC)gxarith.h
$(PSSRC)istruct.h:$(GLSRC)gpsync.h
$(PSSRC)istruct.h:$(GLSRC)gsstype.h
$(PSSRC)istruct.h:$(GLSRC)gsmemory.h
$(PSSRC)istruct.h:$(GLSRC)gslibctx.h
$(PSSRC)istruct.h:$(GLSRC)gsalloc.h
$(PSSRC)istruct.h:$(GLSRC)gxcindex.h
$(PSSRC)istruct.h:$(GLSRC)stdio_.h
$(PSSRC)istruct.h:$(GLSRC)stdint_.h
$(PSSRC)istruct.h:$(GLSRC)gssprintf.h
$(PSSRC)istruct.h:$(GLSRC)gstypes.h
$(PSSRC)istruct.h:$(GLSRC)std.h
$(PSSRC)istruct.h:$(GLSRC)stdpre.h
$(PSSRC)istruct.h:$(GLGEN)arch.h
$(PSSRC)istruct.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iutil.h:$(PSSRC)imemory.h
$(PSSRC)iutil.h:$(PSSRC)ivmspace.h
$(PSSRC)iutil.h:$(PSSRC)iref.h
$(PSSRC)iutil.h:$(GLSRC)gsgc.h
$(PSSRC)iutil.h:$(GLSRC)gxalloc.h
$(PSSRC)iutil.h:$(GLSRC)gxobj.h
$(PSSRC)iutil.h:$(GLSRC)gsnamecl.h
$(PSSRC)iutil.h:$(GLSRC)gxcspace.h
$(PSSRC)iutil.h:$(GLSRC)gscsel.h
$(PSSRC)iutil.h:$(GLSRC)gsdcolor.h
$(PSSRC)iutil.h:$(GLSRC)gxfrac.h
$(PSSRC)iutil.h:$(GLSRC)gscms.h
$(PSSRC)iutil.h:$(GLSRC)gsdevice.h
$(PSSRC)iutil.h:$(GLSRC)gscspace.h
$(PSSRC)iutil.h:$(GLSRC)gsgstate.h
$(PSSRC)iutil.h:$(GLSRC)gsiparam.h
$(PSSRC)iutil.h:$(GLSRC)gsmatrix.h
$(PSSRC)iutil.h:$(GLSRC)gxhttile.h
$(PSSRC)iutil.h:$(GLSRC)gsparam.h
$(PSSRC)iutil.h:$(GLSRC)gsrefct.h
$(PSSRC)iutil.h:$(GLSRC)memento.h
$(PSSRC)iutil.h:$(GLSRC)gsstruct.h
$(PSSRC)iutil.h:$(GLSRC)gxsync.h
$(PSSRC)iutil.h:$(GLSRC)gxbitmap.h
$(PSSRC)iutil.h:$(GLSRC)scommon.h
$(PSSRC)iutil.h:$(GLSRC)gsbitmap.h
$(PSSRC)iutil.h:$(GLSRC)gsccolor.h
$(PSSRC)iutil.h:$(GLSRC)gxarith.h
$(PSSRC)iutil.h:$(GLSRC)gpsync.h
$(PSSRC)iutil.h:$(GLSRC)gsstype.h
$(PSSRC)iutil.h:$(GLSRC)gsmemory.h
$(PSSRC)iutil.h:$(GLSRC)gslibctx.h
$(PSSRC)iutil.h:$(GLSRC)gsalloc.h
$(PSSRC)iutil.h:$(GLSRC)gxcindex.h
$(PSSRC)iutil.h:$(GLSRC)stdio_.h
$(PSSRC)iutil.h:$(GLSRC)stdint_.h
$(PSSRC)iutil.h:$(GLSRC)gssprintf.h
$(PSSRC)iutil.h:$(GLSRC)gstypes.h
$(PSSRC)iutil.h:$(GLSRC)std.h
$(PSSRC)iutil.h:$(GLSRC)stdpre.h
$(PSSRC)iutil.h:$(GLGEN)arch.h
$(PSSRC)iutil.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ivmspace.h:$(PSSRC)iref.h
$(PSSRC)ivmspace.h:$(GLSRC)gsgc.h
$(PSSRC)ivmspace.h:$(GLSRC)gxalloc.h
$(PSSRC)ivmspace.h:$(GLSRC)gxobj.h
$(PSSRC)ivmspace.h:$(GLSRC)gsnamecl.h
$(PSSRC)ivmspace.h:$(GLSRC)gxcspace.h
$(PSSRC)ivmspace.h:$(GLSRC)gscsel.h
$(PSSRC)ivmspace.h:$(GLSRC)gsdcolor.h
$(PSSRC)ivmspace.h:$(GLSRC)gxfrac.h
$(PSSRC)ivmspace.h:$(GLSRC)gscms.h
$(PSSRC)ivmspace.h:$(GLSRC)gsdevice.h
$(PSSRC)ivmspace.h:$(GLSRC)gscspace.h
$(PSSRC)ivmspace.h:$(GLSRC)gsgstate.h
$(PSSRC)ivmspace.h:$(GLSRC)gsiparam.h
$(PSSRC)ivmspace.h:$(GLSRC)gsmatrix.h
$(PSSRC)ivmspace.h:$(GLSRC)gxhttile.h
$(PSSRC)ivmspace.h:$(GLSRC)gsparam.h
$(PSSRC)ivmspace.h:$(GLSRC)gsrefct.h
$(PSSRC)ivmspace.h:$(GLSRC)memento.h
$(PSSRC)ivmspace.h:$(GLSRC)gsstruct.h
$(PSSRC)ivmspace.h:$(GLSRC)gxsync.h
$(PSSRC)ivmspace.h:$(GLSRC)gxbitmap.h
$(PSSRC)ivmspace.h:$(GLSRC)scommon.h
$(PSSRC)ivmspace.h:$(GLSRC)gsbitmap.h
$(PSSRC)ivmspace.h:$(GLSRC)gsccolor.h
$(PSSRC)ivmspace.h:$(GLSRC)gxarith.h
$(PSSRC)ivmspace.h:$(GLSRC)gpsync.h
$(PSSRC)ivmspace.h:$(GLSRC)gsstype.h
$(PSSRC)ivmspace.h:$(GLSRC)gsmemory.h
$(PSSRC)ivmspace.h:$(GLSRC)gslibctx.h
$(PSSRC)ivmspace.h:$(GLSRC)gsalloc.h
$(PSSRC)ivmspace.h:$(GLSRC)gxcindex.h
$(PSSRC)ivmspace.h:$(GLSRC)stdio_.h
$(PSSRC)ivmspace.h:$(GLSRC)stdint_.h
$(PSSRC)ivmspace.h:$(GLSRC)gssprintf.h
$(PSSRC)ivmspace.h:$(GLSRC)gstypes.h
$(PSSRC)ivmspace.h:$(GLSRC)std.h
$(PSSRC)ivmspace.h:$(GLSRC)stdpre.h
$(PSSRC)ivmspace.h:$(GLGEN)arch.h
$(PSSRC)ivmspace.h:$(GLSRC)gs_dll_call.h
$(PSSRC)opdef.h:$(PSSRC)iref.h
$(PSSRC)opdef.h:$(GLSRC)gxalloc.h
$(PSSRC)opdef.h:$(GLSRC)gxobj.h
$(PSSRC)opdef.h:$(GLSRC)gsnamecl.h
$(PSSRC)opdef.h:$(GLSRC)gxcspace.h
$(PSSRC)opdef.h:$(GLSRC)gscsel.h
$(PSSRC)opdef.h:$(GLSRC)gsdcolor.h
$(PSSRC)opdef.h:$(GLSRC)gxfrac.h
$(PSSRC)opdef.h:$(GLSRC)gscms.h
$(PSSRC)opdef.h:$(GLSRC)gsdevice.h
$(PSSRC)opdef.h:$(GLSRC)gscspace.h
$(PSSRC)opdef.h:$(GLSRC)gsgstate.h
$(PSSRC)opdef.h:$(GLSRC)gsiparam.h
$(PSSRC)opdef.h:$(GLSRC)gsmatrix.h
$(PSSRC)opdef.h:$(GLSRC)gxhttile.h
$(PSSRC)opdef.h:$(GLSRC)gsparam.h
$(PSSRC)opdef.h:$(GLSRC)gsrefct.h
$(PSSRC)opdef.h:$(GLSRC)memento.h
$(PSSRC)opdef.h:$(GLSRC)gsstruct.h
$(PSSRC)opdef.h:$(GLSRC)gxsync.h
$(PSSRC)opdef.h:$(GLSRC)gxbitmap.h
$(PSSRC)opdef.h:$(GLSRC)scommon.h
$(PSSRC)opdef.h:$(GLSRC)gsbitmap.h
$(PSSRC)opdef.h:$(GLSRC)gsccolor.h
$(PSSRC)opdef.h:$(GLSRC)gxarith.h
$(PSSRC)opdef.h:$(GLSRC)gpsync.h
$(PSSRC)opdef.h:$(GLSRC)gsstype.h
$(PSSRC)opdef.h:$(GLSRC)gsmemory.h
$(PSSRC)opdef.h:$(GLSRC)gslibctx.h
$(PSSRC)opdef.h:$(GLSRC)gsalloc.h
$(PSSRC)opdef.h:$(GLSRC)gxcindex.h
$(PSSRC)opdef.h:$(GLSRC)stdio_.h
$(PSSRC)opdef.h:$(GLSRC)stdint_.h
$(PSSRC)opdef.h:$(GLSRC)gssprintf.h
$(PSSRC)opdef.h:$(GLSRC)gstypes.h
$(PSSRC)opdef.h:$(GLSRC)std.h
$(PSSRC)opdef.h:$(GLSRC)stdpre.h
$(PSSRC)opdef.h:$(GLGEN)arch.h
$(PSSRC)opdef.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ghost.h:$(GLSRC)gx.h
$(PSSRC)ghost.h:$(GLSRC)gdebug.h
$(PSSRC)ghost.h:$(PSSRC)iref.h
$(PSSRC)ghost.h:$(GLSRC)gxalloc.h
$(PSSRC)ghost.h:$(GLSRC)gxobj.h
$(PSSRC)ghost.h:$(GLSRC)gsnamecl.h
$(PSSRC)ghost.h:$(GLSRC)gxcspace.h
$(PSSRC)ghost.h:$(GLSRC)gscsel.h
$(PSSRC)ghost.h:$(GLSRC)gsdcolor.h
$(PSSRC)ghost.h:$(GLSRC)gxfrac.h
$(PSSRC)ghost.h:$(GLSRC)gscms.h
$(PSSRC)ghost.h:$(GLSRC)gsdevice.h
$(PSSRC)ghost.h:$(GLSRC)gscspace.h
$(PSSRC)ghost.h:$(GLSRC)gsgstate.h
$(PSSRC)ghost.h:$(GLSRC)gsio.h
$(PSSRC)ghost.h:$(GLSRC)gsiparam.h
$(PSSRC)ghost.h:$(GLSRC)gsmatrix.h
$(PSSRC)ghost.h:$(GLSRC)gxhttile.h
$(PSSRC)ghost.h:$(GLSRC)gsparam.h
$(PSSRC)ghost.h:$(GLSRC)gsrefct.h
$(PSSRC)ghost.h:$(GLSRC)memento.h
$(PSSRC)ghost.h:$(GLSRC)gsstruct.h
$(PSSRC)ghost.h:$(GLSRC)gdbflags.h
$(PSSRC)ghost.h:$(GLSRC)gxsync.h
$(PSSRC)ghost.h:$(GLSRC)gserrors.h
$(PSSRC)ghost.h:$(GLSRC)gxbitmap.h
$(PSSRC)ghost.h:$(GLSRC)scommon.h
$(PSSRC)ghost.h:$(GLSRC)gsbitmap.h
$(PSSRC)ghost.h:$(GLSRC)gsccolor.h
$(PSSRC)ghost.h:$(GLSRC)gxarith.h
$(PSSRC)ghost.h:$(GLSRC)gpsync.h
$(PSSRC)ghost.h:$(GLSRC)gsstype.h
$(PSSRC)ghost.h:$(GLSRC)gsmemory.h
$(PSSRC)ghost.h:$(GLSRC)gslibctx.h
$(PSSRC)ghost.h:$(GLSRC)gsalloc.h
$(PSSRC)ghost.h:$(GLSRC)gxcindex.h
$(PSSRC)ghost.h:$(GLSRC)stdio_.h
$(PSSRC)ghost.h:$(GLSRC)stdint_.h
$(PSSRC)ghost.h:$(GLSRC)gssprintf.h
$(PSSRC)ghost.h:$(GLSRC)gstypes.h
$(PSSRC)ghost.h:$(GLSRC)std.h
$(PSSRC)ghost.h:$(GLSRC)stdpre.h
$(PSSRC)ghost.h:$(GLGEN)arch.h
$(PSSRC)ghost.h:$(GLSRC)gs_dll_call.h
$(PSSRC)igc.h:$(PSSRC)istruct.h
$(PSSRC)igc.h:$(PSSRC)inames.h
$(PSSRC)igc.h:$(PSSRC)imemory.h
$(PSSRC)igc.h:$(PSSRC)ivmspace.h
$(PSSRC)igc.h:$(PSSRC)iref.h
$(PSSRC)igc.h:$(GLSRC)gsgc.h
$(PSSRC)igc.h:$(GLSRC)gxalloc.h
$(PSSRC)igc.h:$(GLSRC)gxobj.h
$(PSSRC)igc.h:$(GLSRC)gsnamecl.h
$(PSSRC)igc.h:$(GLSRC)gxcspace.h
$(PSSRC)igc.h:$(GLSRC)gscsel.h
$(PSSRC)igc.h:$(GLSRC)gsdcolor.h
$(PSSRC)igc.h:$(GLSRC)gxfrac.h
$(PSSRC)igc.h:$(GLSRC)gscms.h
$(PSSRC)igc.h:$(GLSRC)gsdevice.h
$(PSSRC)igc.h:$(GLSRC)gscspace.h
$(PSSRC)igc.h:$(GLSRC)gsgstate.h
$(PSSRC)igc.h:$(GLSRC)gsiparam.h
$(PSSRC)igc.h:$(GLSRC)gsmatrix.h
$(PSSRC)igc.h:$(GLSRC)gxhttile.h
$(PSSRC)igc.h:$(GLSRC)gsparam.h
$(PSSRC)igc.h:$(GLSRC)gsrefct.h
$(PSSRC)igc.h:$(GLSRC)memento.h
$(PSSRC)igc.h:$(GLSRC)gsstruct.h
$(PSSRC)igc.h:$(GLSRC)gxsync.h
$(PSSRC)igc.h:$(GLSRC)gxbitmap.h
$(PSSRC)igc.h:$(GLSRC)scommon.h
$(PSSRC)igc.h:$(GLSRC)gsbitmap.h
$(PSSRC)igc.h:$(GLSRC)gsccolor.h
$(PSSRC)igc.h:$(GLSRC)gxarith.h
$(PSSRC)igc.h:$(GLSRC)gpsync.h
$(PSSRC)igc.h:$(GLSRC)gsstype.h
$(PSSRC)igc.h:$(GLSRC)gsmemory.h
$(PSSRC)igc.h:$(GLSRC)gslibctx.h
$(PSSRC)igc.h:$(GLSRC)gsalloc.h
$(PSSRC)igc.h:$(GLSRC)gxcindex.h
$(PSSRC)igc.h:$(GLSRC)stdio_.h
$(PSSRC)igc.h:$(GLSRC)stdint_.h
$(PSSRC)igc.h:$(GLSRC)gssprintf.h
$(PSSRC)igc.h:$(GLSRC)gstypes.h
$(PSSRC)igc.h:$(GLSRC)std.h
$(PSSRC)igc.h:$(GLSRC)stdpre.h
$(PSSRC)igc.h:$(GLGEN)arch.h
$(PSSRC)igc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)imemory.h:$(PSSRC)ivmspace.h
$(PSSRC)imemory.h:$(PSSRC)iref.h
$(PSSRC)imemory.h:$(GLSRC)gsgc.h
$(PSSRC)imemory.h:$(GLSRC)gxalloc.h
$(PSSRC)imemory.h:$(GLSRC)gxobj.h
$(PSSRC)imemory.h:$(GLSRC)gsnamecl.h
$(PSSRC)imemory.h:$(GLSRC)gxcspace.h
$(PSSRC)imemory.h:$(GLSRC)gscsel.h
$(PSSRC)imemory.h:$(GLSRC)gsdcolor.h
$(PSSRC)imemory.h:$(GLSRC)gxfrac.h
$(PSSRC)imemory.h:$(GLSRC)gscms.h
$(PSSRC)imemory.h:$(GLSRC)gsdevice.h
$(PSSRC)imemory.h:$(GLSRC)gscspace.h
$(PSSRC)imemory.h:$(GLSRC)gsgstate.h
$(PSSRC)imemory.h:$(GLSRC)gsiparam.h
$(PSSRC)imemory.h:$(GLSRC)gsmatrix.h
$(PSSRC)imemory.h:$(GLSRC)gxhttile.h
$(PSSRC)imemory.h:$(GLSRC)gsparam.h
$(PSSRC)imemory.h:$(GLSRC)gsrefct.h
$(PSSRC)imemory.h:$(GLSRC)memento.h
$(PSSRC)imemory.h:$(GLSRC)gsstruct.h
$(PSSRC)imemory.h:$(GLSRC)gxsync.h
$(PSSRC)imemory.h:$(GLSRC)gxbitmap.h
$(PSSRC)imemory.h:$(GLSRC)scommon.h
$(PSSRC)imemory.h:$(GLSRC)gsbitmap.h
$(PSSRC)imemory.h:$(GLSRC)gsccolor.h
$(PSSRC)imemory.h:$(GLSRC)gxarith.h
$(PSSRC)imemory.h:$(GLSRC)gpsync.h
$(PSSRC)imemory.h:$(GLSRC)gsstype.h
$(PSSRC)imemory.h:$(GLSRC)gsmemory.h
$(PSSRC)imemory.h:$(GLSRC)gslibctx.h
$(PSSRC)imemory.h:$(GLSRC)gsalloc.h
$(PSSRC)imemory.h:$(GLSRC)gxcindex.h
$(PSSRC)imemory.h:$(GLSRC)stdio_.h
$(PSSRC)imemory.h:$(GLSRC)stdint_.h
$(PSSRC)imemory.h:$(GLSRC)gssprintf.h
$(PSSRC)imemory.h:$(GLSRC)gstypes.h
$(PSSRC)imemory.h:$(GLSRC)std.h
$(PSSRC)imemory.h:$(GLSRC)stdpre.h
$(PSSRC)imemory.h:$(GLGEN)arch.h
$(PSSRC)imemory.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ialloc.h:$(PSSRC)imemory.h
$(PSSRC)ialloc.h:$(PSSRC)ivmspace.h
$(PSSRC)ialloc.h:$(PSSRC)iref.h
$(PSSRC)ialloc.h:$(GLSRC)gsgc.h
$(PSSRC)ialloc.h:$(GLSRC)gxalloc.h
$(PSSRC)ialloc.h:$(GLSRC)gxobj.h
$(PSSRC)ialloc.h:$(GLSRC)gsnamecl.h
$(PSSRC)ialloc.h:$(GLSRC)gxcspace.h
$(PSSRC)ialloc.h:$(GLSRC)gscsel.h
$(PSSRC)ialloc.h:$(GLSRC)gsdcolor.h
$(PSSRC)ialloc.h:$(GLSRC)gxfrac.h
$(PSSRC)ialloc.h:$(GLSRC)gscms.h
$(PSSRC)ialloc.h:$(GLSRC)gsdevice.h
$(PSSRC)ialloc.h:$(GLSRC)gscspace.h
$(PSSRC)ialloc.h:$(GLSRC)gsgstate.h
$(PSSRC)ialloc.h:$(GLSRC)gsiparam.h
$(PSSRC)ialloc.h:$(GLSRC)gsmatrix.h
$(PSSRC)ialloc.h:$(GLSRC)gxhttile.h
$(PSSRC)ialloc.h:$(GLSRC)gsparam.h
$(PSSRC)ialloc.h:$(GLSRC)gsrefct.h
$(PSSRC)ialloc.h:$(GLSRC)memento.h
$(PSSRC)ialloc.h:$(GLSRC)gsstruct.h
$(PSSRC)ialloc.h:$(GLSRC)gxsync.h
$(PSSRC)ialloc.h:$(GLSRC)gxbitmap.h
$(PSSRC)ialloc.h:$(GLSRC)scommon.h
$(PSSRC)ialloc.h:$(GLSRC)gsbitmap.h
$(PSSRC)ialloc.h:$(GLSRC)gsccolor.h
$(PSSRC)ialloc.h:$(GLSRC)gxarith.h
$(PSSRC)ialloc.h:$(GLSRC)gpsync.h
$(PSSRC)ialloc.h:$(GLSRC)gsstype.h
$(PSSRC)ialloc.h:$(GLSRC)gsmemory.h
$(PSSRC)ialloc.h:$(GLSRC)gslibctx.h
$(PSSRC)ialloc.h:$(GLSRC)gsalloc.h
$(PSSRC)ialloc.h:$(GLSRC)gxcindex.h
$(PSSRC)ialloc.h:$(GLSRC)stdio_.h
$(PSSRC)ialloc.h:$(GLSRC)stdint_.h
$(PSSRC)ialloc.h:$(GLSRC)gssprintf.h
$(PSSRC)ialloc.h:$(GLSRC)gstypes.h
$(PSSRC)ialloc.h:$(GLSRC)std.h
$(PSSRC)ialloc.h:$(GLSRC)stdpre.h
$(PSSRC)ialloc.h:$(GLGEN)arch.h
$(PSSRC)ialloc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iastruct.h:$(PSSRC)ialloc.h
$(PSSRC)iastruct.h:$(PSSRC)imemory.h
$(PSSRC)iastruct.h:$(PSSRC)ivmspace.h
$(PSSRC)iastruct.h:$(PSSRC)iref.h
$(PSSRC)iastruct.h:$(GLSRC)gsgc.h
$(PSSRC)iastruct.h:$(GLSRC)gxalloc.h
$(PSSRC)iastruct.h:$(GLSRC)gxobj.h
$(PSSRC)iastruct.h:$(GLSRC)gsnamecl.h
$(PSSRC)iastruct.h:$(GLSRC)gxcspace.h
$(PSSRC)iastruct.h:$(GLSRC)gscsel.h
$(PSSRC)iastruct.h:$(GLSRC)gsdcolor.h
$(PSSRC)iastruct.h:$(GLSRC)gxfrac.h
$(PSSRC)iastruct.h:$(GLSRC)gscms.h
$(PSSRC)iastruct.h:$(GLSRC)gsdevice.h
$(PSSRC)iastruct.h:$(GLSRC)gscspace.h
$(PSSRC)iastruct.h:$(GLSRC)gsgstate.h
$(PSSRC)iastruct.h:$(GLSRC)gsiparam.h
$(PSSRC)iastruct.h:$(GLSRC)gsmatrix.h
$(PSSRC)iastruct.h:$(GLSRC)gxhttile.h
$(PSSRC)iastruct.h:$(GLSRC)gsparam.h
$(PSSRC)iastruct.h:$(GLSRC)gsrefct.h
$(PSSRC)iastruct.h:$(GLSRC)memento.h
$(PSSRC)iastruct.h:$(GLSRC)gsstruct.h
$(PSSRC)iastruct.h:$(GLSRC)gxsync.h
$(PSSRC)iastruct.h:$(GLSRC)gxbitmap.h
$(PSSRC)iastruct.h:$(GLSRC)scommon.h
$(PSSRC)iastruct.h:$(GLSRC)gsbitmap.h
$(PSSRC)iastruct.h:$(GLSRC)gsccolor.h
$(PSSRC)iastruct.h:$(GLSRC)gxarith.h
$(PSSRC)iastruct.h:$(GLSRC)gpsync.h
$(PSSRC)iastruct.h:$(GLSRC)gsstype.h
$(PSSRC)iastruct.h:$(GLSRC)gsmemory.h
$(PSSRC)iastruct.h:$(GLSRC)gslibctx.h
$(PSSRC)iastruct.h:$(GLSRC)gsalloc.h
$(PSSRC)iastruct.h:$(GLSRC)gxcindex.h
$(PSSRC)iastruct.h:$(GLSRC)stdio_.h
$(PSSRC)iastruct.h:$(GLSRC)stdint_.h
$(PSSRC)iastruct.h:$(GLSRC)gssprintf.h
$(PSSRC)iastruct.h:$(GLSRC)gstypes.h
$(PSSRC)iastruct.h:$(GLSRC)std.h
$(PSSRC)iastruct.h:$(GLSRC)stdpre.h
$(PSSRC)iastruct.h:$(GLGEN)arch.h
$(PSSRC)iastruct.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iastate.h:$(PSSRC)istruct.h
$(PSSRC)iastate.h:$(PSSRC)ialloc.h
$(PSSRC)iastate.h:$(PSSRC)imemory.h
$(PSSRC)iastate.h:$(PSSRC)ivmspace.h
$(PSSRC)iastate.h:$(PSSRC)iref.h
$(PSSRC)iastate.h:$(GLSRC)gsgc.h
$(PSSRC)iastate.h:$(GLSRC)gxalloc.h
$(PSSRC)iastate.h:$(GLSRC)gxobj.h
$(PSSRC)iastate.h:$(GLSRC)gsnamecl.h
$(PSSRC)iastate.h:$(GLSRC)gxcspace.h
$(PSSRC)iastate.h:$(GLSRC)gscsel.h
$(PSSRC)iastate.h:$(GLSRC)gsdcolor.h
$(PSSRC)iastate.h:$(GLSRC)gxfrac.h
$(PSSRC)iastate.h:$(GLSRC)gscms.h
$(PSSRC)iastate.h:$(GLSRC)gsdevice.h
$(PSSRC)iastate.h:$(GLSRC)gscspace.h
$(PSSRC)iastate.h:$(GLSRC)gsgstate.h
$(PSSRC)iastate.h:$(GLSRC)gsiparam.h
$(PSSRC)iastate.h:$(GLSRC)gsmatrix.h
$(PSSRC)iastate.h:$(GLSRC)gxhttile.h
$(PSSRC)iastate.h:$(GLSRC)gsparam.h
$(PSSRC)iastate.h:$(GLSRC)gsrefct.h
$(PSSRC)iastate.h:$(GLSRC)memento.h
$(PSSRC)iastate.h:$(GLSRC)gsstruct.h
$(PSSRC)iastate.h:$(GLSRC)gxsync.h
$(PSSRC)iastate.h:$(GLSRC)gxbitmap.h
$(PSSRC)iastate.h:$(GLSRC)scommon.h
$(PSSRC)iastate.h:$(GLSRC)gsbitmap.h
$(PSSRC)iastate.h:$(GLSRC)gsccolor.h
$(PSSRC)iastate.h:$(GLSRC)gxarith.h
$(PSSRC)iastate.h:$(GLSRC)gpsync.h
$(PSSRC)iastate.h:$(GLSRC)gsstype.h
$(PSSRC)iastate.h:$(GLSRC)gsmemory.h
$(PSSRC)iastate.h:$(GLSRC)gslibctx.h
$(PSSRC)iastate.h:$(GLSRC)gsalloc.h
$(PSSRC)iastate.h:$(GLSRC)gxcindex.h
$(PSSRC)iastate.h:$(GLSRC)stdio_.h
$(PSSRC)iastate.h:$(GLSRC)stdint_.h
$(PSSRC)iastate.h:$(GLSRC)gssprintf.h
$(PSSRC)iastate.h:$(GLSRC)gstypes.h
$(PSSRC)iastate.h:$(GLSRC)std.h
$(PSSRC)iastate.h:$(GLSRC)stdpre.h
$(PSSRC)iastate.h:$(GLGEN)arch.h
$(PSSRC)iastate.h:$(GLSRC)gs_dll_call.h
$(PSSRC)inamedef.h:$(PSSRC)isave.h
$(PSSRC)inamedef.h:$(PSSRC)inames.h
$(PSSRC)inamedef.h:$(PSSRC)idosave.h
$(PSSRC)inamedef.h:$(PSSRC)imemory.h
$(PSSRC)inamedef.h:$(PSSRC)ivmspace.h
$(PSSRC)inamedef.h:$(PSSRC)iref.h
$(PSSRC)inamedef.h:$(GLSRC)gsgc.h
$(PSSRC)inamedef.h:$(PSSRC)inamestr.h
$(PSSRC)inamedef.h:$(PSSRC)inameidx.h
$(PSSRC)inamedef.h:$(GLSRC)gxalloc.h
$(PSSRC)inamedef.h:$(GLSRC)gxobj.h
$(PSSRC)inamedef.h:$(GLSRC)gsnamecl.h
$(PSSRC)inamedef.h:$(GLSRC)gxcspace.h
$(PSSRC)inamedef.h:$(GLSRC)gscsel.h
$(PSSRC)inamedef.h:$(GLSRC)gsdcolor.h
$(PSSRC)inamedef.h:$(GLSRC)gxfrac.h
$(PSSRC)inamedef.h:$(GLSRC)gscms.h
$(PSSRC)inamedef.h:$(GLSRC)gsdevice.h
$(PSSRC)inamedef.h:$(GLSRC)gscspace.h
$(PSSRC)inamedef.h:$(GLSRC)gsgstate.h
$(PSSRC)inamedef.h:$(GLSRC)gsiparam.h
$(PSSRC)inamedef.h:$(GLSRC)gsmatrix.h
$(PSSRC)inamedef.h:$(GLSRC)gxhttile.h
$(PSSRC)inamedef.h:$(GLSRC)gsparam.h
$(PSSRC)inamedef.h:$(GLSRC)gsrefct.h
$(PSSRC)inamedef.h:$(GLSRC)memento.h
$(PSSRC)inamedef.h:$(GLSRC)gsstruct.h
$(PSSRC)inamedef.h:$(GLSRC)gxsync.h
$(PSSRC)inamedef.h:$(GLSRC)gxbitmap.h
$(PSSRC)inamedef.h:$(GLSRC)scommon.h
$(PSSRC)inamedef.h:$(GLSRC)gsbitmap.h
$(PSSRC)inamedef.h:$(GLSRC)gsccolor.h
$(PSSRC)inamedef.h:$(GLSRC)gxarith.h
$(PSSRC)inamedef.h:$(GLSRC)gpsync.h
$(PSSRC)inamedef.h:$(GLSRC)gsstype.h
$(PSSRC)inamedef.h:$(GLSRC)gsmemory.h
$(PSSRC)inamedef.h:$(GLSRC)gslibctx.h
$(PSSRC)inamedef.h:$(GLSRC)gsalloc.h
$(PSSRC)inamedef.h:$(GLSRC)gxcindex.h
$(PSSRC)inamedef.h:$(GLSRC)stdio_.h
$(PSSRC)inamedef.h:$(GLSRC)stdint_.h
$(PSSRC)inamedef.h:$(GLSRC)gssprintf.h
$(PSSRC)inamedef.h:$(GLSRC)gstypes.h
$(PSSRC)inamedef.h:$(GLSRC)std.h
$(PSSRC)inamedef.h:$(GLSRC)stdpre.h
$(PSSRC)inamedef.h:$(GLGEN)arch.h
$(PSSRC)inamedef.h:$(GLSRC)gs_dll_call.h
$(PSSRC)store.h:$(PSSRC)ialloc.h
$(PSSRC)store.h:$(PSSRC)idosave.h
$(PSSRC)store.h:$(PSSRC)imemory.h
$(PSSRC)store.h:$(PSSRC)ivmspace.h
$(PSSRC)store.h:$(PSSRC)iref.h
$(PSSRC)store.h:$(GLSRC)gsgc.h
$(PSSRC)store.h:$(GLSRC)gxalloc.h
$(PSSRC)store.h:$(GLSRC)gxobj.h
$(PSSRC)store.h:$(GLSRC)gsnamecl.h
$(PSSRC)store.h:$(GLSRC)gxcspace.h
$(PSSRC)store.h:$(GLSRC)gscsel.h
$(PSSRC)store.h:$(GLSRC)gsdcolor.h
$(PSSRC)store.h:$(GLSRC)gxfrac.h
$(PSSRC)store.h:$(GLSRC)gscms.h
$(PSSRC)store.h:$(GLSRC)gsdevice.h
$(PSSRC)store.h:$(GLSRC)gscspace.h
$(PSSRC)store.h:$(GLSRC)gsgstate.h
$(PSSRC)store.h:$(GLSRC)gsiparam.h
$(PSSRC)store.h:$(GLSRC)gsmatrix.h
$(PSSRC)store.h:$(GLSRC)gxhttile.h
$(PSSRC)store.h:$(GLSRC)gsparam.h
$(PSSRC)store.h:$(GLSRC)gsrefct.h
$(PSSRC)store.h:$(GLSRC)memento.h
$(PSSRC)store.h:$(GLSRC)gsstruct.h
$(PSSRC)store.h:$(GLSRC)gxsync.h
$(PSSRC)store.h:$(GLSRC)gxbitmap.h
$(PSSRC)store.h:$(GLSRC)scommon.h
$(PSSRC)store.h:$(GLSRC)gsbitmap.h
$(PSSRC)store.h:$(GLSRC)gsccolor.h
$(PSSRC)store.h:$(GLSRC)gxarith.h
$(PSSRC)store.h:$(GLSRC)gpsync.h
$(PSSRC)store.h:$(GLSRC)gsstype.h
$(PSSRC)store.h:$(GLSRC)gsmemory.h
$(PSSRC)store.h:$(GLSRC)gslibctx.h
$(PSSRC)store.h:$(GLSRC)gsalloc.h
$(PSSRC)store.h:$(GLSRC)gxcindex.h
$(PSSRC)store.h:$(GLSRC)stdio_.h
$(PSSRC)store.h:$(GLSRC)stdint_.h
$(PSSRC)store.h:$(GLSRC)gssprintf.h
$(PSSRC)store.h:$(GLSRC)gstypes.h
$(PSSRC)store.h:$(GLSRC)std.h
$(PSSRC)store.h:$(GLSRC)stdpre.h
$(PSSRC)store.h:$(GLGEN)arch.h
$(PSSRC)store.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iplugin.h:$(PSSRC)iref.h
$(PSSRC)iplugin.h:$(GLSRC)gxalloc.h
$(PSSRC)iplugin.h:$(GLSRC)gxobj.h
$(PSSRC)iplugin.h:$(GLSRC)gsnamecl.h
$(PSSRC)iplugin.h:$(GLSRC)gxcspace.h
$(PSSRC)iplugin.h:$(GLSRC)gscsel.h
$(PSSRC)iplugin.h:$(GLSRC)gsdcolor.h
$(PSSRC)iplugin.h:$(GLSRC)gxfrac.h
$(PSSRC)iplugin.h:$(GLSRC)gscms.h
$(PSSRC)iplugin.h:$(GLSRC)gsdevice.h
$(PSSRC)iplugin.h:$(GLSRC)gscspace.h
$(PSSRC)iplugin.h:$(GLSRC)gsgstate.h
$(PSSRC)iplugin.h:$(GLSRC)gsiparam.h
$(PSSRC)iplugin.h:$(GLSRC)gsmatrix.h
$(PSSRC)iplugin.h:$(GLSRC)gxhttile.h
$(PSSRC)iplugin.h:$(GLSRC)gsparam.h
$(PSSRC)iplugin.h:$(GLSRC)gsrefct.h
$(PSSRC)iplugin.h:$(GLSRC)memento.h
$(PSSRC)iplugin.h:$(GLSRC)gsstruct.h
$(PSSRC)iplugin.h:$(GLSRC)gxsync.h
$(PSSRC)iplugin.h:$(GLSRC)gxbitmap.h
$(PSSRC)iplugin.h:$(GLSRC)scommon.h
$(PSSRC)iplugin.h:$(GLSRC)gsbitmap.h
$(PSSRC)iplugin.h:$(GLSRC)gsccolor.h
$(PSSRC)iplugin.h:$(GLSRC)gxarith.h
$(PSSRC)iplugin.h:$(GLSRC)gpsync.h
$(PSSRC)iplugin.h:$(GLSRC)gsstype.h
$(PSSRC)iplugin.h:$(GLSRC)gsmemory.h
$(PSSRC)iplugin.h:$(GLSRC)gslibctx.h
$(PSSRC)iplugin.h:$(GLSRC)gsalloc.h
$(PSSRC)iplugin.h:$(GLSRC)gxcindex.h
$(PSSRC)iplugin.h:$(GLSRC)stdio_.h
$(PSSRC)iplugin.h:$(GLSRC)stdint_.h
$(PSSRC)iplugin.h:$(GLSRC)gssprintf.h
$(PSSRC)iplugin.h:$(GLSRC)gstypes.h
$(PSSRC)iplugin.h:$(GLSRC)std.h
$(PSSRC)iplugin.h:$(GLSRC)stdpre.h
$(PSSRC)iplugin.h:$(GLGEN)arch.h
$(PSSRC)iplugin.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifapi.h:$(PSSRC)iplugin.h
$(PSSRC)ifapi.h:$(GLSRC)gxfapi.h
$(PSSRC)ifapi.h:$(PSSRC)iref.h
$(PSSRC)ifapi.h:$(GLSRC)gxalloc.h
$(PSSRC)ifapi.h:$(GLSRC)gxobj.h
$(PSSRC)ifapi.h:$(GLSRC)gstext.h
$(PSSRC)ifapi.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifapi.h:$(GLSRC)gxcspace.h
$(PSSRC)ifapi.h:$(GLSRC)gscsel.h
$(PSSRC)ifapi.h:$(GLSRC)gsfont.h
$(PSSRC)ifapi.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifapi.h:$(GLSRC)gxpath.h
$(PSSRC)ifapi.h:$(GLSRC)gxfrac.h
$(PSSRC)ifapi.h:$(GLSRC)gscms.h
$(PSSRC)ifapi.h:$(GLSRC)gsrect.h
$(PSSRC)ifapi.h:$(GLSRC)gslparam.h
$(PSSRC)ifapi.h:$(GLSRC)gsdevice.h
$(PSSRC)ifapi.h:$(GLSRC)gscpm.h
$(PSSRC)ifapi.h:$(GLSRC)gscspace.h
$(PSSRC)ifapi.h:$(GLSRC)gsgstate.h
$(PSSRC)ifapi.h:$(GLSRC)gsiparam.h
$(PSSRC)ifapi.h:$(GLSRC)gxfixed.h
$(PSSRC)ifapi.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifapi.h:$(GLSRC)gspenum.h
$(PSSRC)ifapi.h:$(GLSRC)gxhttile.h
$(PSSRC)ifapi.h:$(GLSRC)gsparam.h
$(PSSRC)ifapi.h:$(GLSRC)gsrefct.h
$(PSSRC)ifapi.h:$(GLSRC)gp.h
$(PSSRC)ifapi.h:$(GLSRC)memento.h
$(PSSRC)ifapi.h:$(GLSRC)memory_.h
$(PSSRC)ifapi.h:$(GLSRC)gsstruct.h
$(PSSRC)ifapi.h:$(GLSRC)gxsync.h
$(PSSRC)ifapi.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifapi.h:$(GLSRC)srdline.h
$(PSSRC)ifapi.h:$(GLSRC)scommon.h
$(PSSRC)ifapi.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifapi.h:$(GLSRC)gsccolor.h
$(PSSRC)ifapi.h:$(GLSRC)gxarith.h
$(PSSRC)ifapi.h:$(GLSRC)stat_.h
$(PSSRC)ifapi.h:$(GLSRC)gpsync.h
$(PSSRC)ifapi.h:$(GLSRC)gsstype.h
$(PSSRC)ifapi.h:$(GLSRC)gsmemory.h
$(PSSRC)ifapi.h:$(GLSRC)gpgetenv.h
$(PSSRC)ifapi.h:$(GLSRC)gscdefs.h
$(PSSRC)ifapi.h:$(GLSRC)gslibctx.h
$(PSSRC)ifapi.h:$(GLSRC)gsalloc.h
$(PSSRC)ifapi.h:$(GLSRC)gxcindex.h
$(PSSRC)ifapi.h:$(GLSRC)stdio_.h
$(PSSRC)ifapi.h:$(GLSRC)gsccode.h
$(PSSRC)ifapi.h:$(GLSRC)stdint_.h
$(PSSRC)ifapi.h:$(GLSRC)gssprintf.h
$(PSSRC)ifapi.h:$(GLSRC)gstypes.h
$(PSSRC)ifapi.h:$(GLSRC)std.h
$(PSSRC)ifapi.h:$(GLSRC)stdpre.h
$(PSSRC)ifapi.h:$(GLGEN)arch.h
$(PSSRC)ifapi.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zht2.h:$(GLSRC)gscspace.h
$(PSSRC)zht2.h:$(GLSRC)gsgstate.h
$(PSSRC)zht2.h:$(GLSRC)gsiparam.h
$(PSSRC)zht2.h:$(GLSRC)gsmatrix.h
$(PSSRC)zht2.h:$(GLSRC)gsrefct.h
$(PSSRC)zht2.h:$(GLSRC)memento.h
$(PSSRC)zht2.h:$(GLSRC)gxbitmap.h
$(PSSRC)zht2.h:$(GLSRC)scommon.h
$(PSSRC)zht2.h:$(GLSRC)gsbitmap.h
$(PSSRC)zht2.h:$(GLSRC)gsccolor.h
$(PSSRC)zht2.h:$(GLSRC)gsstype.h
$(PSSRC)zht2.h:$(GLSRC)gsmemory.h
$(PSSRC)zht2.h:$(GLSRC)gslibctx.h
$(PSSRC)zht2.h:$(GLSRC)stdio_.h
$(PSSRC)zht2.h:$(GLSRC)stdint_.h
$(PSSRC)zht2.h:$(GLSRC)gssprintf.h
$(PSSRC)zht2.h:$(GLSRC)gstypes.h
$(PSSRC)zht2.h:$(GLSRC)std.h
$(PSSRC)zht2.h:$(GLSRC)stdpre.h
$(PSSRC)zht2.h:$(GLGEN)arch.h
$(PSSRC)zht2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gen_ordered.h:$(GLSRC)stdpre.h
$(PSSRC)zchar42.h:$(GLSRC)gxfapi.h
$(PSSRC)zchar42.h:$(PSSRC)iref.h
$(PSSRC)zchar42.h:$(GLSRC)gxalloc.h
$(PSSRC)zchar42.h:$(GLSRC)gxobj.h
$(PSSRC)zchar42.h:$(GLSRC)gstext.h
$(PSSRC)zchar42.h:$(GLSRC)gsnamecl.h
$(PSSRC)zchar42.h:$(GLSRC)gxcspace.h
$(PSSRC)zchar42.h:$(GLSRC)gscsel.h
$(PSSRC)zchar42.h:$(GLSRC)gsfont.h
$(PSSRC)zchar42.h:$(GLSRC)gsdcolor.h
$(PSSRC)zchar42.h:$(GLSRC)gxpath.h
$(PSSRC)zchar42.h:$(GLSRC)gxfrac.h
$(PSSRC)zchar42.h:$(GLSRC)gscms.h
$(PSSRC)zchar42.h:$(GLSRC)gsrect.h
$(PSSRC)zchar42.h:$(GLSRC)gslparam.h
$(PSSRC)zchar42.h:$(GLSRC)gsdevice.h
$(PSSRC)zchar42.h:$(GLSRC)gscpm.h
$(PSSRC)zchar42.h:$(GLSRC)gscspace.h
$(PSSRC)zchar42.h:$(GLSRC)gsgstate.h
$(PSSRC)zchar42.h:$(GLSRC)gsiparam.h
$(PSSRC)zchar42.h:$(GLSRC)gxfixed.h
$(PSSRC)zchar42.h:$(GLSRC)gsmatrix.h
$(PSSRC)zchar42.h:$(GLSRC)gspenum.h
$(PSSRC)zchar42.h:$(GLSRC)gxhttile.h
$(PSSRC)zchar42.h:$(GLSRC)gsparam.h
$(PSSRC)zchar42.h:$(GLSRC)gsrefct.h
$(PSSRC)zchar42.h:$(GLSRC)memento.h
$(PSSRC)zchar42.h:$(GLSRC)gsstruct.h
$(PSSRC)zchar42.h:$(GLSRC)gxsync.h
$(PSSRC)zchar42.h:$(GLSRC)gxbitmap.h
$(PSSRC)zchar42.h:$(GLSRC)scommon.h
$(PSSRC)zchar42.h:$(GLSRC)gsbitmap.h
$(PSSRC)zchar42.h:$(GLSRC)gsccolor.h
$(PSSRC)zchar42.h:$(GLSRC)gxarith.h
$(PSSRC)zchar42.h:$(GLSRC)gpsync.h
$(PSSRC)zchar42.h:$(GLSRC)gsstype.h
$(PSSRC)zchar42.h:$(GLSRC)gsmemory.h
$(PSSRC)zchar42.h:$(GLSRC)gslibctx.h
$(PSSRC)zchar42.h:$(GLSRC)gsalloc.h
$(PSSRC)zchar42.h:$(GLSRC)gxcindex.h
$(PSSRC)zchar42.h:$(GLSRC)stdio_.h
$(PSSRC)zchar42.h:$(GLSRC)gsccode.h
$(PSSRC)zchar42.h:$(GLSRC)stdint_.h
$(PSSRC)zchar42.h:$(GLSRC)gssprintf.h
$(PSSRC)zchar42.h:$(GLSRC)gstypes.h
$(PSSRC)zchar42.h:$(GLSRC)std.h
$(PSSRC)zchar42.h:$(GLSRC)stdpre.h
$(PSSRC)zchar42.h:$(GLGEN)arch.h
$(PSSRC)zchar42.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zfunc.h:$(PSSRC)iref.h
$(PSSRC)zfunc.h:$(GLSRC)gxalloc.h
$(PSSRC)zfunc.h:$(GLSRC)gxobj.h
$(PSSRC)zfunc.h:$(GLSRC)gsnamecl.h
$(PSSRC)zfunc.h:$(GLSRC)gsfunc.h
$(PSSRC)zfunc.h:$(GLSRC)gxcspace.h
$(PSSRC)zfunc.h:$(GLSRC)gscsel.h
$(PSSRC)zfunc.h:$(GLSRC)gsdcolor.h
$(PSSRC)zfunc.h:$(GLSRC)gxfrac.h
$(PSSRC)zfunc.h:$(GLSRC)gscms.h
$(PSSRC)zfunc.h:$(GLSRC)gsdevice.h
$(PSSRC)zfunc.h:$(GLSRC)gscspace.h
$(PSSRC)zfunc.h:$(GLSRC)gsgstate.h
$(PSSRC)zfunc.h:$(GLSRC)gsdsrc.h
$(PSSRC)zfunc.h:$(GLSRC)gsiparam.h
$(PSSRC)zfunc.h:$(GLSRC)gsmatrix.h
$(PSSRC)zfunc.h:$(GLSRC)gxhttile.h
$(PSSRC)zfunc.h:$(GLSRC)gsparam.h
$(PSSRC)zfunc.h:$(GLSRC)gsrefct.h
$(PSSRC)zfunc.h:$(GLSRC)memento.h
$(PSSRC)zfunc.h:$(GLSRC)gsstruct.h
$(PSSRC)zfunc.h:$(GLSRC)gxsync.h
$(PSSRC)zfunc.h:$(GLSRC)gxbitmap.h
$(PSSRC)zfunc.h:$(GLSRC)scommon.h
$(PSSRC)zfunc.h:$(GLSRC)gsbitmap.h
$(PSSRC)zfunc.h:$(GLSRC)gsccolor.h
$(PSSRC)zfunc.h:$(GLSRC)gxarith.h
$(PSSRC)zfunc.h:$(GLSRC)gpsync.h
$(PSSRC)zfunc.h:$(GLSRC)gsstype.h
$(PSSRC)zfunc.h:$(GLSRC)gsmemory.h
$(PSSRC)zfunc.h:$(GLSRC)gslibctx.h
$(PSSRC)zfunc.h:$(GLSRC)gsalloc.h
$(PSSRC)zfunc.h:$(GLSRC)gxcindex.h
$(PSSRC)zfunc.h:$(GLSRC)stdio_.h
$(PSSRC)zfunc.h:$(GLSRC)stdint_.h
$(PSSRC)zfunc.h:$(GLSRC)gssprintf.h
$(PSSRC)zfunc.h:$(GLSRC)gstypes.h
$(PSSRC)zfunc.h:$(GLSRC)std.h
$(PSSRC)zfunc.h:$(GLSRC)stdpre.h
$(PSSRC)zfunc.h:$(GLGEN)arch.h
$(PSSRC)zfunc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idparam.h:$(PSSRC)iref.h
$(PSSRC)idparam.h:$(GLSRC)gxalloc.h
$(PSSRC)idparam.h:$(GLSRC)gxobj.h
$(PSSRC)idparam.h:$(GLSRC)gsnamecl.h
$(PSSRC)idparam.h:$(GLSRC)gxcspace.h
$(PSSRC)idparam.h:$(GLSRC)gscsel.h
$(PSSRC)idparam.h:$(GLSRC)gsdcolor.h
$(PSSRC)idparam.h:$(GLSRC)gxfrac.h
$(PSSRC)idparam.h:$(GLSRC)gscms.h
$(PSSRC)idparam.h:$(GLSRC)gsdevice.h
$(PSSRC)idparam.h:$(GLSRC)gscspace.h
$(PSSRC)idparam.h:$(GLSRC)gsgstate.h
$(PSSRC)idparam.h:$(GLSRC)gsiparam.h
$(PSSRC)idparam.h:$(GLSRC)gsmatrix.h
$(PSSRC)idparam.h:$(GLSRC)gxhttile.h
$(PSSRC)idparam.h:$(GLSRC)gsparam.h
$(PSSRC)idparam.h:$(GLSRC)gsrefct.h
$(PSSRC)idparam.h:$(GLSRC)memento.h
$(PSSRC)idparam.h:$(GLSRC)gsuid.h
$(PSSRC)idparam.h:$(GLSRC)gsstruct.h
$(PSSRC)idparam.h:$(GLSRC)gxsync.h
$(PSSRC)idparam.h:$(GLSRC)gxbitmap.h
$(PSSRC)idparam.h:$(GLSRC)scommon.h
$(PSSRC)idparam.h:$(GLSRC)gsbitmap.h
$(PSSRC)idparam.h:$(GLSRC)gsccolor.h
$(PSSRC)idparam.h:$(GLSRC)gxarith.h
$(PSSRC)idparam.h:$(GLSRC)gpsync.h
$(PSSRC)idparam.h:$(GLSRC)gsstype.h
$(PSSRC)idparam.h:$(GLSRC)gsmemory.h
$(PSSRC)idparam.h:$(GLSRC)gslibctx.h
$(PSSRC)idparam.h:$(GLSRC)gsalloc.h
$(PSSRC)idparam.h:$(GLSRC)gxcindex.h
$(PSSRC)idparam.h:$(GLSRC)stdio_.h
$(PSSRC)idparam.h:$(GLSRC)stdint_.h
$(PSSRC)idparam.h:$(GLSRC)gssprintf.h
$(PSSRC)idparam.h:$(GLSRC)gstypes.h
$(PSSRC)idparam.h:$(GLSRC)std.h
$(PSSRC)idparam.h:$(GLSRC)stdpre.h
$(PSSRC)idparam.h:$(GLGEN)arch.h
$(PSSRC)idparam.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ilevel.h:$(PSSRC)imemory.h
$(PSSRC)ilevel.h:$(PSSRC)ivmspace.h
$(PSSRC)ilevel.h:$(PSSRC)iref.h
$(PSSRC)ilevel.h:$(GLSRC)gsgc.h
$(PSSRC)ilevel.h:$(GLSRC)gxalloc.h
$(PSSRC)ilevel.h:$(GLSRC)gxobj.h
$(PSSRC)ilevel.h:$(GLSRC)gsnamecl.h
$(PSSRC)ilevel.h:$(GLSRC)gxcspace.h
$(PSSRC)ilevel.h:$(GLSRC)gscsel.h
$(PSSRC)ilevel.h:$(GLSRC)gsdcolor.h
$(PSSRC)ilevel.h:$(GLSRC)gxfrac.h
$(PSSRC)ilevel.h:$(GLSRC)gscms.h
$(PSSRC)ilevel.h:$(GLSRC)gsdevice.h
$(PSSRC)ilevel.h:$(GLSRC)gscspace.h
$(PSSRC)ilevel.h:$(GLSRC)gsgstate.h
$(PSSRC)ilevel.h:$(GLSRC)gsiparam.h
$(PSSRC)ilevel.h:$(GLSRC)gsmatrix.h
$(PSSRC)ilevel.h:$(GLSRC)gxhttile.h
$(PSSRC)ilevel.h:$(GLSRC)gsparam.h
$(PSSRC)ilevel.h:$(GLSRC)gsrefct.h
$(PSSRC)ilevel.h:$(GLSRC)memento.h
$(PSSRC)ilevel.h:$(GLSRC)gsstruct.h
$(PSSRC)ilevel.h:$(GLSRC)gxsync.h
$(PSSRC)ilevel.h:$(GLSRC)gxbitmap.h
$(PSSRC)ilevel.h:$(GLSRC)scommon.h
$(PSSRC)ilevel.h:$(GLSRC)gsbitmap.h
$(PSSRC)ilevel.h:$(GLSRC)gsccolor.h
$(PSSRC)ilevel.h:$(GLSRC)gxarith.h
$(PSSRC)ilevel.h:$(GLSRC)gpsync.h
$(PSSRC)ilevel.h:$(GLSRC)gsstype.h
$(PSSRC)ilevel.h:$(GLSRC)gsmemory.h
$(PSSRC)ilevel.h:$(GLSRC)gslibctx.h
$(PSSRC)ilevel.h:$(GLSRC)gsalloc.h
$(PSSRC)ilevel.h:$(GLSRC)gxcindex.h
$(PSSRC)ilevel.h:$(GLSRC)stdio_.h
$(PSSRC)ilevel.h:$(GLSRC)stdint_.h
$(PSSRC)ilevel.h:$(GLSRC)gssprintf.h
$(PSSRC)ilevel.h:$(GLSRC)gstypes.h
$(PSSRC)ilevel.h:$(GLSRC)std.h
$(PSSRC)ilevel.h:$(GLSRC)stdpre.h
$(PSSRC)ilevel.h:$(GLGEN)arch.h
$(PSSRC)ilevel.h:$(GLSRC)gs_dll_call.h
$(PSSRC)interp.h:$(PSSRC)imemory.h
$(PSSRC)interp.h:$(PSSRC)ivmspace.h
$(PSSRC)interp.h:$(PSSRC)iref.h
$(PSSRC)interp.h:$(GLSRC)gsgc.h
$(PSSRC)interp.h:$(GLSRC)gxalloc.h
$(PSSRC)interp.h:$(GLSRC)gxobj.h
$(PSSRC)interp.h:$(GLSRC)gsnamecl.h
$(PSSRC)interp.h:$(GLSRC)gxcspace.h
$(PSSRC)interp.h:$(GLSRC)gscsel.h
$(PSSRC)interp.h:$(GLSRC)gsdcolor.h
$(PSSRC)interp.h:$(GLSRC)gxfrac.h
$(PSSRC)interp.h:$(GLSRC)gscms.h
$(PSSRC)interp.h:$(GLSRC)gsdevice.h
$(PSSRC)interp.h:$(GLSRC)gscspace.h
$(PSSRC)interp.h:$(GLSRC)gsgstate.h
$(PSSRC)interp.h:$(GLSRC)gsiparam.h
$(PSSRC)interp.h:$(GLSRC)gsmatrix.h
$(PSSRC)interp.h:$(GLSRC)gxhttile.h
$(PSSRC)interp.h:$(GLSRC)gsparam.h
$(PSSRC)interp.h:$(GLSRC)gsrefct.h
$(PSSRC)interp.h:$(GLSRC)memento.h
$(PSSRC)interp.h:$(GLSRC)gsstruct.h
$(PSSRC)interp.h:$(GLSRC)gxsync.h
$(PSSRC)interp.h:$(GLSRC)gxbitmap.h
$(PSSRC)interp.h:$(GLSRC)scommon.h
$(PSSRC)interp.h:$(GLSRC)gsbitmap.h
$(PSSRC)interp.h:$(GLSRC)gsccolor.h
$(PSSRC)interp.h:$(GLSRC)gxarith.h
$(PSSRC)interp.h:$(GLSRC)gpsync.h
$(PSSRC)interp.h:$(GLSRC)gsstype.h
$(PSSRC)interp.h:$(GLSRC)gsmemory.h
$(PSSRC)interp.h:$(GLSRC)gslibctx.h
$(PSSRC)interp.h:$(GLSRC)gsalloc.h
$(PSSRC)interp.h:$(GLSRC)gxcindex.h
$(PSSRC)interp.h:$(GLSRC)stdio_.h
$(PSSRC)interp.h:$(GLSRC)stdint_.h
$(PSSRC)interp.h:$(GLSRC)gssprintf.h
$(PSSRC)interp.h:$(GLSRC)gstypes.h
$(PSSRC)interp.h:$(GLSRC)std.h
$(PSSRC)interp.h:$(GLSRC)stdpre.h
$(PSSRC)interp.h:$(GLGEN)arch.h
$(PSSRC)interp.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iparam.h:$(PSSRC)isdata.h
$(PSSRC)iparam.h:$(PSSRC)imemory.h
$(PSSRC)iparam.h:$(PSSRC)ivmspace.h
$(PSSRC)iparam.h:$(PSSRC)iref.h
$(PSSRC)iparam.h:$(GLSRC)gsgc.h
$(PSSRC)iparam.h:$(GLSRC)gxalloc.h
$(PSSRC)iparam.h:$(GLSRC)gxobj.h
$(PSSRC)iparam.h:$(GLSRC)gsnamecl.h
$(PSSRC)iparam.h:$(GLSRC)gxcspace.h
$(PSSRC)iparam.h:$(GLSRC)gscsel.h
$(PSSRC)iparam.h:$(GLSRC)gsdcolor.h
$(PSSRC)iparam.h:$(GLSRC)gxfrac.h
$(PSSRC)iparam.h:$(GLSRC)gscms.h
$(PSSRC)iparam.h:$(GLSRC)gsdevice.h
$(PSSRC)iparam.h:$(GLSRC)gscspace.h
$(PSSRC)iparam.h:$(GLSRC)gsgstate.h
$(PSSRC)iparam.h:$(GLSRC)gsiparam.h
$(PSSRC)iparam.h:$(GLSRC)gsmatrix.h
$(PSSRC)iparam.h:$(GLSRC)gxhttile.h
$(PSSRC)iparam.h:$(GLSRC)gsparam.h
$(PSSRC)iparam.h:$(GLSRC)gsrefct.h
$(PSSRC)iparam.h:$(GLSRC)memento.h
$(PSSRC)iparam.h:$(GLSRC)gsstruct.h
$(PSSRC)iparam.h:$(GLSRC)gxsync.h
$(PSSRC)iparam.h:$(GLSRC)gxbitmap.h
$(PSSRC)iparam.h:$(GLSRC)scommon.h
$(PSSRC)iparam.h:$(GLSRC)gsbitmap.h
$(PSSRC)iparam.h:$(GLSRC)gsccolor.h
$(PSSRC)iparam.h:$(GLSRC)gxarith.h
$(PSSRC)iparam.h:$(GLSRC)gpsync.h
$(PSSRC)iparam.h:$(GLSRC)gsstype.h
$(PSSRC)iparam.h:$(GLSRC)gsmemory.h
$(PSSRC)iparam.h:$(GLSRC)gslibctx.h
$(PSSRC)iparam.h:$(GLSRC)gsalloc.h
$(PSSRC)iparam.h:$(GLSRC)gxcindex.h
$(PSSRC)iparam.h:$(GLSRC)stdio_.h
$(PSSRC)iparam.h:$(GLSRC)stdint_.h
$(PSSRC)iparam.h:$(GLSRC)gssprintf.h
$(PSSRC)iparam.h:$(GLSRC)gstypes.h
$(PSSRC)iparam.h:$(GLSRC)std.h
$(PSSRC)iparam.h:$(GLSRC)stdpre.h
$(PSSRC)iparam.h:$(GLGEN)arch.h
$(PSSRC)iparam.h:$(GLSRC)gs_dll_call.h
$(PSSRC)isdata.h:$(PSSRC)imemory.h
$(PSSRC)isdata.h:$(PSSRC)ivmspace.h
$(PSSRC)isdata.h:$(PSSRC)iref.h
$(PSSRC)isdata.h:$(GLSRC)gsgc.h
$(PSSRC)isdata.h:$(GLSRC)gxalloc.h
$(PSSRC)isdata.h:$(GLSRC)gxobj.h
$(PSSRC)isdata.h:$(GLSRC)gsnamecl.h
$(PSSRC)isdata.h:$(GLSRC)gxcspace.h
$(PSSRC)isdata.h:$(GLSRC)gscsel.h
$(PSSRC)isdata.h:$(GLSRC)gsdcolor.h
$(PSSRC)isdata.h:$(GLSRC)gxfrac.h
$(PSSRC)isdata.h:$(GLSRC)gscms.h
$(PSSRC)isdata.h:$(GLSRC)gsdevice.h
$(PSSRC)isdata.h:$(GLSRC)gscspace.h
$(PSSRC)isdata.h:$(GLSRC)gsgstate.h
$(PSSRC)isdata.h:$(GLSRC)gsiparam.h
$(PSSRC)isdata.h:$(GLSRC)gsmatrix.h
$(PSSRC)isdata.h:$(GLSRC)gxhttile.h
$(PSSRC)isdata.h:$(GLSRC)gsparam.h
$(PSSRC)isdata.h:$(GLSRC)gsrefct.h
$(PSSRC)isdata.h:$(GLSRC)memento.h
$(PSSRC)isdata.h:$(GLSRC)gsstruct.h
$(PSSRC)isdata.h:$(GLSRC)gxsync.h
$(PSSRC)isdata.h:$(GLSRC)gxbitmap.h
$(PSSRC)isdata.h:$(GLSRC)scommon.h
$(PSSRC)isdata.h:$(GLSRC)gsbitmap.h
$(PSSRC)isdata.h:$(GLSRC)gsccolor.h
$(PSSRC)isdata.h:$(GLSRC)gxarith.h
$(PSSRC)isdata.h:$(GLSRC)gpsync.h
$(PSSRC)isdata.h:$(GLSRC)gsstype.h
$(PSSRC)isdata.h:$(GLSRC)gsmemory.h
$(PSSRC)isdata.h:$(GLSRC)gslibctx.h
$(PSSRC)isdata.h:$(GLSRC)gsalloc.h
$(PSSRC)isdata.h:$(GLSRC)gxcindex.h
$(PSSRC)isdata.h:$(GLSRC)stdio_.h
$(PSSRC)isdata.h:$(GLSRC)stdint_.h
$(PSSRC)isdata.h:$(GLSRC)gssprintf.h
$(PSSRC)isdata.h:$(GLSRC)gstypes.h
$(PSSRC)isdata.h:$(GLSRC)std.h
$(PSSRC)isdata.h:$(GLSRC)stdpre.h
$(PSSRC)isdata.h:$(GLGEN)arch.h
$(PSSRC)isdata.h:$(GLSRC)gs_dll_call.h
$(PSSRC)istack.h:$(PSSRC)isdata.h
$(PSSRC)istack.h:$(PSSRC)imemory.h
$(PSSRC)istack.h:$(PSSRC)ivmspace.h
$(PSSRC)istack.h:$(PSSRC)iref.h
$(PSSRC)istack.h:$(GLSRC)gsgc.h
$(PSSRC)istack.h:$(GLSRC)gxalloc.h
$(PSSRC)istack.h:$(GLSRC)gxobj.h
$(PSSRC)istack.h:$(GLSRC)gsnamecl.h
$(PSSRC)istack.h:$(GLSRC)gxcspace.h
$(PSSRC)istack.h:$(GLSRC)gscsel.h
$(PSSRC)istack.h:$(GLSRC)gsdcolor.h
$(PSSRC)istack.h:$(GLSRC)gxfrac.h
$(PSSRC)istack.h:$(GLSRC)gscms.h
$(PSSRC)istack.h:$(GLSRC)gsdevice.h
$(PSSRC)istack.h:$(GLSRC)gscspace.h
$(PSSRC)istack.h:$(GLSRC)gsgstate.h
$(PSSRC)istack.h:$(GLSRC)gsiparam.h
$(PSSRC)istack.h:$(GLSRC)gsmatrix.h
$(PSSRC)istack.h:$(GLSRC)gxhttile.h
$(PSSRC)istack.h:$(GLSRC)gsparam.h
$(PSSRC)istack.h:$(GLSRC)gsrefct.h
$(PSSRC)istack.h:$(GLSRC)memento.h
$(PSSRC)istack.h:$(GLSRC)gsstruct.h
$(PSSRC)istack.h:$(GLSRC)gxsync.h
$(PSSRC)istack.h:$(GLSRC)gxbitmap.h
$(PSSRC)istack.h:$(GLSRC)scommon.h
$(PSSRC)istack.h:$(GLSRC)gsbitmap.h
$(PSSRC)istack.h:$(GLSRC)gsccolor.h
$(PSSRC)istack.h:$(GLSRC)gxarith.h
$(PSSRC)istack.h:$(GLSRC)gpsync.h
$(PSSRC)istack.h:$(GLSRC)gsstype.h
$(PSSRC)istack.h:$(GLSRC)gsmemory.h
$(PSSRC)istack.h:$(GLSRC)gslibctx.h
$(PSSRC)istack.h:$(GLSRC)gsalloc.h
$(PSSRC)istack.h:$(GLSRC)gxcindex.h
$(PSSRC)istack.h:$(GLSRC)stdio_.h
$(PSSRC)istack.h:$(GLSRC)stdint_.h
$(PSSRC)istack.h:$(GLSRC)gssprintf.h
$(PSSRC)istack.h:$(GLSRC)gstypes.h
$(PSSRC)istack.h:$(GLSRC)std.h
$(PSSRC)istack.h:$(GLSRC)stdpre.h
$(PSSRC)istack.h:$(GLGEN)arch.h
$(PSSRC)istack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)istkparm.h:$(PSSRC)iref.h
$(PSSRC)istkparm.h:$(GLSRC)gxalloc.h
$(PSSRC)istkparm.h:$(GLSRC)gxobj.h
$(PSSRC)istkparm.h:$(GLSRC)gsnamecl.h
$(PSSRC)istkparm.h:$(GLSRC)gxcspace.h
$(PSSRC)istkparm.h:$(GLSRC)gscsel.h
$(PSSRC)istkparm.h:$(GLSRC)gsdcolor.h
$(PSSRC)istkparm.h:$(GLSRC)gxfrac.h
$(PSSRC)istkparm.h:$(GLSRC)gscms.h
$(PSSRC)istkparm.h:$(GLSRC)gsdevice.h
$(PSSRC)istkparm.h:$(GLSRC)gscspace.h
$(PSSRC)istkparm.h:$(GLSRC)gsgstate.h
$(PSSRC)istkparm.h:$(GLSRC)gsiparam.h
$(PSSRC)istkparm.h:$(GLSRC)gsmatrix.h
$(PSSRC)istkparm.h:$(GLSRC)gxhttile.h
$(PSSRC)istkparm.h:$(GLSRC)gsparam.h
$(PSSRC)istkparm.h:$(GLSRC)gsrefct.h
$(PSSRC)istkparm.h:$(GLSRC)memento.h
$(PSSRC)istkparm.h:$(GLSRC)gsstruct.h
$(PSSRC)istkparm.h:$(GLSRC)gxsync.h
$(PSSRC)istkparm.h:$(GLSRC)gxbitmap.h
$(PSSRC)istkparm.h:$(GLSRC)scommon.h
$(PSSRC)istkparm.h:$(GLSRC)gsbitmap.h
$(PSSRC)istkparm.h:$(GLSRC)gsccolor.h
$(PSSRC)istkparm.h:$(GLSRC)gxarith.h
$(PSSRC)istkparm.h:$(GLSRC)gpsync.h
$(PSSRC)istkparm.h:$(GLSRC)gsstype.h
$(PSSRC)istkparm.h:$(GLSRC)gsmemory.h
$(PSSRC)istkparm.h:$(GLSRC)gslibctx.h
$(PSSRC)istkparm.h:$(GLSRC)gsalloc.h
$(PSSRC)istkparm.h:$(GLSRC)gxcindex.h
$(PSSRC)istkparm.h:$(GLSRC)stdio_.h
$(PSSRC)istkparm.h:$(GLSRC)stdint_.h
$(PSSRC)istkparm.h:$(GLSRC)gssprintf.h
$(PSSRC)istkparm.h:$(GLSRC)gstypes.h
$(PSSRC)istkparm.h:$(GLSRC)std.h
$(PSSRC)istkparm.h:$(GLSRC)stdpre.h
$(PSSRC)istkparm.h:$(GLGEN)arch.h
$(PSSRC)istkparm.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iutil2.h:$(PSSRC)iref.h
$(PSSRC)iutil2.h:$(GLSRC)gxalloc.h
$(PSSRC)iutil2.h:$(GLSRC)gxobj.h
$(PSSRC)iutil2.h:$(GLSRC)gsnamecl.h
$(PSSRC)iutil2.h:$(GLSRC)gsfunc.h
$(PSSRC)iutil2.h:$(GLSRC)gxcspace.h
$(PSSRC)iutil2.h:$(GLSRC)gscsel.h
$(PSSRC)iutil2.h:$(GLSRC)gsdcolor.h
$(PSSRC)iutil2.h:$(GLSRC)gxfrac.h
$(PSSRC)iutil2.h:$(GLSRC)gscms.h
$(PSSRC)iutil2.h:$(GLSRC)gsdevice.h
$(PSSRC)iutil2.h:$(GLSRC)gscspace.h
$(PSSRC)iutil2.h:$(GLSRC)gsgstate.h
$(PSSRC)iutil2.h:$(GLSRC)gsdsrc.h
$(PSSRC)iutil2.h:$(GLSRC)gsiparam.h
$(PSSRC)iutil2.h:$(GLSRC)gsmatrix.h
$(PSSRC)iutil2.h:$(GLSRC)gxhttile.h
$(PSSRC)iutil2.h:$(GLSRC)gsparam.h
$(PSSRC)iutil2.h:$(GLSRC)gsrefct.h
$(PSSRC)iutil2.h:$(GLSRC)memento.h
$(PSSRC)iutil2.h:$(GLSRC)gsstruct.h
$(PSSRC)iutil2.h:$(GLSRC)gxsync.h
$(PSSRC)iutil2.h:$(GLSRC)gxbitmap.h
$(PSSRC)iutil2.h:$(GLSRC)scommon.h
$(PSSRC)iutil2.h:$(GLSRC)gsbitmap.h
$(PSSRC)iutil2.h:$(GLSRC)gsccolor.h
$(PSSRC)iutil2.h:$(GLSRC)gxarith.h
$(PSSRC)iutil2.h:$(GLSRC)gpsync.h
$(PSSRC)iutil2.h:$(GLSRC)gsstype.h
$(PSSRC)iutil2.h:$(GLSRC)gsmemory.h
$(PSSRC)iutil2.h:$(GLSRC)gslibctx.h
$(PSSRC)iutil2.h:$(GLSRC)gsalloc.h
$(PSSRC)iutil2.h:$(GLSRC)gxcindex.h
$(PSSRC)iutil2.h:$(GLSRC)stdio_.h
$(PSSRC)iutil2.h:$(GLSRC)stdint_.h
$(PSSRC)iutil2.h:$(GLSRC)gssprintf.h
$(PSSRC)iutil2.h:$(GLSRC)gstypes.h
$(PSSRC)iutil2.h:$(GLSRC)std.h
$(PSSRC)iutil2.h:$(GLSRC)stdpre.h
$(PSSRC)iutil2.h:$(GLGEN)arch.h
$(PSSRC)iutil2.h:$(GLSRC)gs_dll_call.h
$(PSSRC)oparc.h:$(PSSRC)iref.h
$(PSSRC)oparc.h:$(GLSRC)gxalloc.h
$(PSSRC)oparc.h:$(GLSRC)gxobj.h
$(PSSRC)oparc.h:$(GLSRC)gsnamecl.h
$(PSSRC)oparc.h:$(GLSRC)gxcspace.h
$(PSSRC)oparc.h:$(GLSRC)gscsel.h
$(PSSRC)oparc.h:$(GLSRC)gsdcolor.h
$(PSSRC)oparc.h:$(GLSRC)gxfrac.h
$(PSSRC)oparc.h:$(GLSRC)gscms.h
$(PSSRC)oparc.h:$(GLSRC)gsdevice.h
$(PSSRC)oparc.h:$(GLSRC)gscspace.h
$(PSSRC)oparc.h:$(GLSRC)gsgstate.h
$(PSSRC)oparc.h:$(GLSRC)gsiparam.h
$(PSSRC)oparc.h:$(GLSRC)gsmatrix.h
$(PSSRC)oparc.h:$(GLSRC)gxhttile.h
$(PSSRC)oparc.h:$(GLSRC)gsparam.h
$(PSSRC)oparc.h:$(GLSRC)gsrefct.h
$(PSSRC)oparc.h:$(GLSRC)memento.h
$(PSSRC)oparc.h:$(GLSRC)gsstruct.h
$(PSSRC)oparc.h:$(GLSRC)gxsync.h
$(PSSRC)oparc.h:$(GLSRC)gxbitmap.h
$(PSSRC)oparc.h:$(GLSRC)scommon.h
$(PSSRC)oparc.h:$(GLSRC)gsbitmap.h
$(PSSRC)oparc.h:$(GLSRC)gsccolor.h
$(PSSRC)oparc.h:$(GLSRC)gxarith.h
$(PSSRC)oparc.h:$(GLSRC)gpsync.h
$(PSSRC)oparc.h:$(GLSRC)gsstype.h
$(PSSRC)oparc.h:$(GLSRC)gsmemory.h
$(PSSRC)oparc.h:$(GLSRC)gslibctx.h
$(PSSRC)oparc.h:$(GLSRC)gsalloc.h
$(PSSRC)oparc.h:$(GLSRC)gxcindex.h
$(PSSRC)oparc.h:$(GLSRC)stdio_.h
$(PSSRC)oparc.h:$(GLSRC)stdint_.h
$(PSSRC)oparc.h:$(GLSRC)gssprintf.h
$(PSSRC)oparc.h:$(GLSRC)gstypes.h
$(PSSRC)oparc.h:$(GLSRC)std.h
$(PSSRC)oparc.h:$(GLSRC)stdpre.h
$(PSSRC)oparc.h:$(GLGEN)arch.h
$(PSSRC)oparc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)opcheck.h:$(PSSRC)iref.h
$(PSSRC)opcheck.h:$(GLSRC)gxalloc.h
$(PSSRC)opcheck.h:$(GLSRC)gxobj.h
$(PSSRC)opcheck.h:$(GLSRC)gsnamecl.h
$(PSSRC)opcheck.h:$(GLSRC)gxcspace.h
$(PSSRC)opcheck.h:$(GLSRC)gscsel.h
$(PSSRC)opcheck.h:$(GLSRC)gsdcolor.h
$(PSSRC)opcheck.h:$(GLSRC)gxfrac.h
$(PSSRC)opcheck.h:$(GLSRC)gscms.h
$(PSSRC)opcheck.h:$(GLSRC)gsdevice.h
$(PSSRC)opcheck.h:$(GLSRC)gscspace.h
$(PSSRC)opcheck.h:$(GLSRC)gsgstate.h
$(PSSRC)opcheck.h:$(GLSRC)gsiparam.h
$(PSSRC)opcheck.h:$(GLSRC)gsmatrix.h
$(PSSRC)opcheck.h:$(GLSRC)gxhttile.h
$(PSSRC)opcheck.h:$(GLSRC)gsparam.h
$(PSSRC)opcheck.h:$(GLSRC)gsrefct.h
$(PSSRC)opcheck.h:$(GLSRC)memento.h
$(PSSRC)opcheck.h:$(GLSRC)gsstruct.h
$(PSSRC)opcheck.h:$(GLSRC)gxsync.h
$(PSSRC)opcheck.h:$(GLSRC)gxbitmap.h
$(PSSRC)opcheck.h:$(GLSRC)scommon.h
$(PSSRC)opcheck.h:$(GLSRC)gsbitmap.h
$(PSSRC)opcheck.h:$(GLSRC)gsccolor.h
$(PSSRC)opcheck.h:$(GLSRC)gxarith.h
$(PSSRC)opcheck.h:$(GLSRC)gpsync.h
$(PSSRC)opcheck.h:$(GLSRC)gsstype.h
$(PSSRC)opcheck.h:$(GLSRC)gsmemory.h
$(PSSRC)opcheck.h:$(GLSRC)gslibctx.h
$(PSSRC)opcheck.h:$(GLSRC)gsalloc.h
$(PSSRC)opcheck.h:$(GLSRC)gxcindex.h
$(PSSRC)opcheck.h:$(GLSRC)stdio_.h
$(PSSRC)opcheck.h:$(GLSRC)stdint_.h
$(PSSRC)opcheck.h:$(GLSRC)gssprintf.h
$(PSSRC)opcheck.h:$(GLSRC)gstypes.h
$(PSSRC)opcheck.h:$(GLSRC)std.h
$(PSSRC)opcheck.h:$(GLSRC)stdpre.h
$(PSSRC)opcheck.h:$(GLGEN)arch.h
$(PSSRC)opcheck.h:$(GLSRC)gs_dll_call.h
$(PSSRC)opextern.h:$(PSSRC)iref.h
$(PSSRC)opextern.h:$(GLSRC)gxalloc.h
$(PSSRC)opextern.h:$(GLSRC)gxobj.h
$(PSSRC)opextern.h:$(GLSRC)gsnamecl.h
$(PSSRC)opextern.h:$(GLSRC)gxcspace.h
$(PSSRC)opextern.h:$(GLSRC)gscsel.h
$(PSSRC)opextern.h:$(GLSRC)gsdcolor.h
$(PSSRC)opextern.h:$(GLSRC)gxfrac.h
$(PSSRC)opextern.h:$(GLSRC)gscms.h
$(PSSRC)opextern.h:$(GLSRC)gsdevice.h
$(PSSRC)opextern.h:$(GLSRC)gscspace.h
$(PSSRC)opextern.h:$(GLSRC)gsgstate.h
$(PSSRC)opextern.h:$(GLSRC)gsiparam.h
$(PSSRC)opextern.h:$(GLSRC)gsmatrix.h
$(PSSRC)opextern.h:$(GLSRC)gxhttile.h
$(PSSRC)opextern.h:$(GLSRC)gsparam.h
$(PSSRC)opextern.h:$(GLSRC)gsrefct.h
$(PSSRC)opextern.h:$(GLSRC)memento.h
$(PSSRC)opextern.h:$(GLSRC)gsstruct.h
$(PSSRC)opextern.h:$(GLSRC)gxsync.h
$(PSSRC)opextern.h:$(GLSRC)gxbitmap.h
$(PSSRC)opextern.h:$(GLSRC)scommon.h
$(PSSRC)opextern.h:$(GLSRC)gsbitmap.h
$(PSSRC)opextern.h:$(GLSRC)gsccolor.h
$(PSSRC)opextern.h:$(GLSRC)gxarith.h
$(PSSRC)opextern.h:$(GLSRC)gpsync.h
$(PSSRC)opextern.h:$(GLSRC)gsstype.h
$(PSSRC)opextern.h:$(GLSRC)gsmemory.h
$(PSSRC)opextern.h:$(GLSRC)gslibctx.h
$(PSSRC)opextern.h:$(GLSRC)gsalloc.h
$(PSSRC)opextern.h:$(GLSRC)gxcindex.h
$(PSSRC)opextern.h:$(GLSRC)stdio_.h
$(PSSRC)opextern.h:$(GLSRC)stdint_.h
$(PSSRC)opextern.h:$(GLSRC)gssprintf.h
$(PSSRC)opextern.h:$(GLSRC)gstypes.h
$(PSSRC)opextern.h:$(GLSRC)std.h
$(PSSRC)opextern.h:$(GLSRC)stdpre.h
$(PSSRC)opextern.h:$(GLGEN)arch.h
$(PSSRC)opextern.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idsdata.h:$(PSSRC)iddstack.h
$(PSSRC)idsdata.h:$(PSSRC)isdata.h
$(PSSRC)idsdata.h:$(PSSRC)imemory.h
$(PSSRC)idsdata.h:$(PSSRC)ivmspace.h
$(PSSRC)idsdata.h:$(PSSRC)iref.h
$(PSSRC)idsdata.h:$(GLSRC)gsgc.h
$(PSSRC)idsdata.h:$(GLSRC)gxalloc.h
$(PSSRC)idsdata.h:$(GLSRC)gxobj.h
$(PSSRC)idsdata.h:$(GLSRC)gsnamecl.h
$(PSSRC)idsdata.h:$(GLSRC)gxcspace.h
$(PSSRC)idsdata.h:$(GLSRC)gscsel.h
$(PSSRC)idsdata.h:$(GLSRC)gsdcolor.h
$(PSSRC)idsdata.h:$(GLSRC)gxfrac.h
$(PSSRC)idsdata.h:$(GLSRC)gscms.h
$(PSSRC)idsdata.h:$(GLSRC)gsdevice.h
$(PSSRC)idsdata.h:$(GLSRC)gscspace.h
$(PSSRC)idsdata.h:$(GLSRC)gsgstate.h
$(PSSRC)idsdata.h:$(GLSRC)gsiparam.h
$(PSSRC)idsdata.h:$(GLSRC)gsmatrix.h
$(PSSRC)idsdata.h:$(GLSRC)gxhttile.h
$(PSSRC)idsdata.h:$(GLSRC)gsparam.h
$(PSSRC)idsdata.h:$(GLSRC)gsrefct.h
$(PSSRC)idsdata.h:$(GLSRC)memento.h
$(PSSRC)idsdata.h:$(GLSRC)gsstruct.h
$(PSSRC)idsdata.h:$(GLSRC)gxsync.h
$(PSSRC)idsdata.h:$(GLSRC)gxbitmap.h
$(PSSRC)idsdata.h:$(GLSRC)scommon.h
$(PSSRC)idsdata.h:$(GLSRC)gsbitmap.h
$(PSSRC)idsdata.h:$(GLSRC)gsccolor.h
$(PSSRC)idsdata.h:$(GLSRC)gxarith.h
$(PSSRC)idsdata.h:$(GLSRC)gpsync.h
$(PSSRC)idsdata.h:$(GLSRC)gsstype.h
$(PSSRC)idsdata.h:$(GLSRC)gsmemory.h
$(PSSRC)idsdata.h:$(GLSRC)gslibctx.h
$(PSSRC)idsdata.h:$(GLSRC)gsalloc.h
$(PSSRC)idsdata.h:$(GLSRC)gxcindex.h
$(PSSRC)idsdata.h:$(GLSRC)stdio_.h
$(PSSRC)idsdata.h:$(GLSRC)stdint_.h
$(PSSRC)idsdata.h:$(GLSRC)gssprintf.h
$(PSSRC)idsdata.h:$(GLSRC)gstypes.h
$(PSSRC)idsdata.h:$(GLSRC)std.h
$(PSSRC)idsdata.h:$(GLSRC)stdpre.h
$(PSSRC)idsdata.h:$(GLGEN)arch.h
$(PSSRC)idsdata.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idstack.h:$(PSSRC)idsdata.h
$(PSSRC)idstack.h:$(PSSRC)iddstack.h
$(PSSRC)idstack.h:$(PSSRC)istack.h
$(PSSRC)idstack.h:$(PSSRC)isdata.h
$(PSSRC)idstack.h:$(PSSRC)imemory.h
$(PSSRC)idstack.h:$(PSSRC)ivmspace.h
$(PSSRC)idstack.h:$(PSSRC)iref.h
$(PSSRC)idstack.h:$(GLSRC)gsgc.h
$(PSSRC)idstack.h:$(GLSRC)gxalloc.h
$(PSSRC)idstack.h:$(GLSRC)gxobj.h
$(PSSRC)idstack.h:$(GLSRC)gsnamecl.h
$(PSSRC)idstack.h:$(GLSRC)gxcspace.h
$(PSSRC)idstack.h:$(GLSRC)gscsel.h
$(PSSRC)idstack.h:$(GLSRC)gsdcolor.h
$(PSSRC)idstack.h:$(GLSRC)gxfrac.h
$(PSSRC)idstack.h:$(GLSRC)gscms.h
$(PSSRC)idstack.h:$(GLSRC)gsdevice.h
$(PSSRC)idstack.h:$(GLSRC)gscspace.h
$(PSSRC)idstack.h:$(GLSRC)gsgstate.h
$(PSSRC)idstack.h:$(GLSRC)gsiparam.h
$(PSSRC)idstack.h:$(GLSRC)gsmatrix.h
$(PSSRC)idstack.h:$(GLSRC)gxhttile.h
$(PSSRC)idstack.h:$(GLSRC)gsparam.h
$(PSSRC)idstack.h:$(GLSRC)gsrefct.h
$(PSSRC)idstack.h:$(GLSRC)memento.h
$(PSSRC)idstack.h:$(GLSRC)gsstruct.h
$(PSSRC)idstack.h:$(GLSRC)gxsync.h
$(PSSRC)idstack.h:$(GLSRC)gxbitmap.h
$(PSSRC)idstack.h:$(GLSRC)scommon.h
$(PSSRC)idstack.h:$(GLSRC)gsbitmap.h
$(PSSRC)idstack.h:$(GLSRC)gsccolor.h
$(PSSRC)idstack.h:$(GLSRC)gxarith.h
$(PSSRC)idstack.h:$(GLSRC)gpsync.h
$(PSSRC)idstack.h:$(GLSRC)gsstype.h
$(PSSRC)idstack.h:$(GLSRC)gsmemory.h
$(PSSRC)idstack.h:$(GLSRC)gslibctx.h
$(PSSRC)idstack.h:$(GLSRC)gsalloc.h
$(PSSRC)idstack.h:$(GLSRC)gxcindex.h
$(PSSRC)idstack.h:$(GLSRC)stdio_.h
$(PSSRC)idstack.h:$(GLSRC)stdint_.h
$(PSSRC)idstack.h:$(GLSRC)gssprintf.h
$(PSSRC)idstack.h:$(GLSRC)gstypes.h
$(PSSRC)idstack.h:$(GLSRC)std.h
$(PSSRC)idstack.h:$(GLSRC)stdpre.h
$(PSSRC)idstack.h:$(GLGEN)arch.h
$(PSSRC)idstack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iesdata.h:$(PSSRC)isdata.h
$(PSSRC)iesdata.h:$(PSSRC)imemory.h
$(PSSRC)iesdata.h:$(PSSRC)ivmspace.h
$(PSSRC)iesdata.h:$(PSSRC)iref.h
$(PSSRC)iesdata.h:$(GLSRC)gsgc.h
$(PSSRC)iesdata.h:$(GLSRC)gxalloc.h
$(PSSRC)iesdata.h:$(GLSRC)gxobj.h
$(PSSRC)iesdata.h:$(GLSRC)gsnamecl.h
$(PSSRC)iesdata.h:$(GLSRC)gxcspace.h
$(PSSRC)iesdata.h:$(GLSRC)gscsel.h
$(PSSRC)iesdata.h:$(GLSRC)gsdcolor.h
$(PSSRC)iesdata.h:$(GLSRC)gxfrac.h
$(PSSRC)iesdata.h:$(GLSRC)gscms.h
$(PSSRC)iesdata.h:$(GLSRC)gsdevice.h
$(PSSRC)iesdata.h:$(GLSRC)gscspace.h
$(PSSRC)iesdata.h:$(GLSRC)gsgstate.h
$(PSSRC)iesdata.h:$(GLSRC)gsiparam.h
$(PSSRC)iesdata.h:$(GLSRC)gsmatrix.h
$(PSSRC)iesdata.h:$(GLSRC)gxhttile.h
$(PSSRC)iesdata.h:$(GLSRC)gsparam.h
$(PSSRC)iesdata.h:$(GLSRC)gsrefct.h
$(PSSRC)iesdata.h:$(GLSRC)memento.h
$(PSSRC)iesdata.h:$(GLSRC)gsstruct.h
$(PSSRC)iesdata.h:$(GLSRC)gxsync.h
$(PSSRC)iesdata.h:$(GLSRC)gxbitmap.h
$(PSSRC)iesdata.h:$(GLSRC)scommon.h
$(PSSRC)iesdata.h:$(GLSRC)gsbitmap.h
$(PSSRC)iesdata.h:$(GLSRC)gsccolor.h
$(PSSRC)iesdata.h:$(GLSRC)gxarith.h
$(PSSRC)iesdata.h:$(GLSRC)gpsync.h
$(PSSRC)iesdata.h:$(GLSRC)gsstype.h
$(PSSRC)iesdata.h:$(GLSRC)gsmemory.h
$(PSSRC)iesdata.h:$(GLSRC)gslibctx.h
$(PSSRC)iesdata.h:$(GLSRC)gsalloc.h
$(PSSRC)iesdata.h:$(GLSRC)gxcindex.h
$(PSSRC)iesdata.h:$(GLSRC)stdio_.h
$(PSSRC)iesdata.h:$(GLSRC)stdint_.h
$(PSSRC)iesdata.h:$(GLSRC)gssprintf.h
$(PSSRC)iesdata.h:$(GLSRC)gstypes.h
$(PSSRC)iesdata.h:$(GLSRC)std.h
$(PSSRC)iesdata.h:$(GLSRC)stdpre.h
$(PSSRC)iesdata.h:$(GLGEN)arch.h
$(PSSRC)iesdata.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iestack.h:$(PSSRC)iesdata.h
$(PSSRC)iestack.h:$(PSSRC)istack.h
$(PSSRC)iestack.h:$(PSSRC)isdata.h
$(PSSRC)iestack.h:$(PSSRC)imemory.h
$(PSSRC)iestack.h:$(PSSRC)ivmspace.h
$(PSSRC)iestack.h:$(PSSRC)iref.h
$(PSSRC)iestack.h:$(GLSRC)gsgc.h
$(PSSRC)iestack.h:$(GLSRC)gxalloc.h
$(PSSRC)iestack.h:$(GLSRC)gxobj.h
$(PSSRC)iestack.h:$(GLSRC)gsnamecl.h
$(PSSRC)iestack.h:$(GLSRC)gxcspace.h
$(PSSRC)iestack.h:$(GLSRC)gscsel.h
$(PSSRC)iestack.h:$(GLSRC)gsdcolor.h
$(PSSRC)iestack.h:$(GLSRC)gxfrac.h
$(PSSRC)iestack.h:$(GLSRC)gscms.h
$(PSSRC)iestack.h:$(GLSRC)gsdevice.h
$(PSSRC)iestack.h:$(GLSRC)gscspace.h
$(PSSRC)iestack.h:$(GLSRC)gsgstate.h
$(PSSRC)iestack.h:$(GLSRC)gsiparam.h
$(PSSRC)iestack.h:$(GLSRC)gsmatrix.h
$(PSSRC)iestack.h:$(GLSRC)gxhttile.h
$(PSSRC)iestack.h:$(GLSRC)gsparam.h
$(PSSRC)iestack.h:$(GLSRC)gsrefct.h
$(PSSRC)iestack.h:$(GLSRC)memento.h
$(PSSRC)iestack.h:$(GLSRC)gsstruct.h
$(PSSRC)iestack.h:$(GLSRC)gxsync.h
$(PSSRC)iestack.h:$(GLSRC)gxbitmap.h
$(PSSRC)iestack.h:$(GLSRC)scommon.h
$(PSSRC)iestack.h:$(GLSRC)gsbitmap.h
$(PSSRC)iestack.h:$(GLSRC)gsccolor.h
$(PSSRC)iestack.h:$(GLSRC)gxarith.h
$(PSSRC)iestack.h:$(GLSRC)gpsync.h
$(PSSRC)iestack.h:$(GLSRC)gsstype.h
$(PSSRC)iestack.h:$(GLSRC)gsmemory.h
$(PSSRC)iestack.h:$(GLSRC)gslibctx.h
$(PSSRC)iestack.h:$(GLSRC)gsalloc.h
$(PSSRC)iestack.h:$(GLSRC)gxcindex.h
$(PSSRC)iestack.h:$(GLSRC)stdio_.h
$(PSSRC)iestack.h:$(GLSRC)stdint_.h
$(PSSRC)iestack.h:$(GLSRC)gssprintf.h
$(PSSRC)iestack.h:$(GLSRC)gstypes.h
$(PSSRC)iestack.h:$(GLSRC)std.h
$(PSSRC)iestack.h:$(GLSRC)stdpre.h
$(PSSRC)iestack.h:$(GLGEN)arch.h
$(PSSRC)iestack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iosdata.h:$(PSSRC)isdata.h
$(PSSRC)iosdata.h:$(PSSRC)imemory.h
$(PSSRC)iosdata.h:$(PSSRC)ivmspace.h
$(PSSRC)iosdata.h:$(PSSRC)iref.h
$(PSSRC)iosdata.h:$(GLSRC)gsgc.h
$(PSSRC)iosdata.h:$(GLSRC)gxalloc.h
$(PSSRC)iosdata.h:$(GLSRC)gxobj.h
$(PSSRC)iosdata.h:$(GLSRC)gsnamecl.h
$(PSSRC)iosdata.h:$(GLSRC)gxcspace.h
$(PSSRC)iosdata.h:$(GLSRC)gscsel.h
$(PSSRC)iosdata.h:$(GLSRC)gsdcolor.h
$(PSSRC)iosdata.h:$(GLSRC)gxfrac.h
$(PSSRC)iosdata.h:$(GLSRC)gscms.h
$(PSSRC)iosdata.h:$(GLSRC)gsdevice.h
$(PSSRC)iosdata.h:$(GLSRC)gscspace.h
$(PSSRC)iosdata.h:$(GLSRC)gsgstate.h
$(PSSRC)iosdata.h:$(GLSRC)gsiparam.h
$(PSSRC)iosdata.h:$(GLSRC)gsmatrix.h
$(PSSRC)iosdata.h:$(GLSRC)gxhttile.h
$(PSSRC)iosdata.h:$(GLSRC)gsparam.h
$(PSSRC)iosdata.h:$(GLSRC)gsrefct.h
$(PSSRC)iosdata.h:$(GLSRC)memento.h
$(PSSRC)iosdata.h:$(GLSRC)gsstruct.h
$(PSSRC)iosdata.h:$(GLSRC)gxsync.h
$(PSSRC)iosdata.h:$(GLSRC)gxbitmap.h
$(PSSRC)iosdata.h:$(GLSRC)scommon.h
$(PSSRC)iosdata.h:$(GLSRC)gsbitmap.h
$(PSSRC)iosdata.h:$(GLSRC)gsccolor.h
$(PSSRC)iosdata.h:$(GLSRC)gxarith.h
$(PSSRC)iosdata.h:$(GLSRC)gpsync.h
$(PSSRC)iosdata.h:$(GLSRC)gsstype.h
$(PSSRC)iosdata.h:$(GLSRC)gsmemory.h
$(PSSRC)iosdata.h:$(GLSRC)gslibctx.h
$(PSSRC)iosdata.h:$(GLSRC)gsalloc.h
$(PSSRC)iosdata.h:$(GLSRC)gxcindex.h
$(PSSRC)iosdata.h:$(GLSRC)stdio_.h
$(PSSRC)iosdata.h:$(GLSRC)stdint_.h
$(PSSRC)iosdata.h:$(GLSRC)gssprintf.h
$(PSSRC)iosdata.h:$(GLSRC)gstypes.h
$(PSSRC)iosdata.h:$(GLSRC)std.h
$(PSSRC)iosdata.h:$(GLSRC)stdpre.h
$(PSSRC)iosdata.h:$(GLGEN)arch.h
$(PSSRC)iosdata.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iostack.h:$(PSSRC)istack.h
$(PSSRC)iostack.h:$(PSSRC)iosdata.h
$(PSSRC)iostack.h:$(PSSRC)isdata.h
$(PSSRC)iostack.h:$(PSSRC)imemory.h
$(PSSRC)iostack.h:$(PSSRC)ivmspace.h
$(PSSRC)iostack.h:$(PSSRC)iref.h
$(PSSRC)iostack.h:$(GLSRC)gsgc.h
$(PSSRC)iostack.h:$(GLSRC)gxalloc.h
$(PSSRC)iostack.h:$(GLSRC)gxobj.h
$(PSSRC)iostack.h:$(GLSRC)gsnamecl.h
$(PSSRC)iostack.h:$(GLSRC)gxcspace.h
$(PSSRC)iostack.h:$(GLSRC)gscsel.h
$(PSSRC)iostack.h:$(GLSRC)gsdcolor.h
$(PSSRC)iostack.h:$(GLSRC)gxfrac.h
$(PSSRC)iostack.h:$(GLSRC)gscms.h
$(PSSRC)iostack.h:$(GLSRC)gsdevice.h
$(PSSRC)iostack.h:$(GLSRC)gscspace.h
$(PSSRC)iostack.h:$(GLSRC)gsgstate.h
$(PSSRC)iostack.h:$(GLSRC)gsiparam.h
$(PSSRC)iostack.h:$(GLSRC)gsmatrix.h
$(PSSRC)iostack.h:$(GLSRC)gxhttile.h
$(PSSRC)iostack.h:$(GLSRC)gsparam.h
$(PSSRC)iostack.h:$(GLSRC)gsrefct.h
$(PSSRC)iostack.h:$(GLSRC)memento.h
$(PSSRC)iostack.h:$(GLSRC)gsstruct.h
$(PSSRC)iostack.h:$(GLSRC)gxsync.h
$(PSSRC)iostack.h:$(GLSRC)gxbitmap.h
$(PSSRC)iostack.h:$(GLSRC)scommon.h
$(PSSRC)iostack.h:$(GLSRC)gsbitmap.h
$(PSSRC)iostack.h:$(GLSRC)gsccolor.h
$(PSSRC)iostack.h:$(GLSRC)gxarith.h
$(PSSRC)iostack.h:$(GLSRC)gpsync.h
$(PSSRC)iostack.h:$(GLSRC)gsstype.h
$(PSSRC)iostack.h:$(GLSRC)gsmemory.h
$(PSSRC)iostack.h:$(GLSRC)gslibctx.h
$(PSSRC)iostack.h:$(GLSRC)gsalloc.h
$(PSSRC)iostack.h:$(GLSRC)gxcindex.h
$(PSSRC)iostack.h:$(GLSRC)stdio_.h
$(PSSRC)iostack.h:$(GLSRC)stdint_.h
$(PSSRC)iostack.h:$(GLSRC)gssprintf.h
$(PSSRC)iostack.h:$(GLSRC)gstypes.h
$(PSSRC)iostack.h:$(GLSRC)std.h
$(PSSRC)iostack.h:$(GLSRC)stdpre.h
$(PSSRC)iostack.h:$(GLGEN)arch.h
$(PSSRC)iostack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icstate.h:$(PSSRC)idsdata.h
$(PSSRC)icstate.h:$(PSSRC)iesdata.h
$(PSSRC)icstate.h:$(PSSRC)interp.h
$(PSSRC)icstate.h:$(PSSRC)files.h
$(PSSRC)icstate.h:$(PSSRC)opdef.h
$(PSSRC)icstate.h:$(PSSRC)iddstack.h
$(PSSRC)icstate.h:$(PSSRC)store.h
$(PSSRC)icstate.h:$(PSSRC)iosdata.h
$(PSSRC)icstate.h:$(PSSRC)ialloc.h
$(PSSRC)icstate.h:$(PSSRC)idosave.h
$(PSSRC)icstate.h:$(PSSRC)isdata.h
$(PSSRC)icstate.h:$(PSSRC)imemory.h
$(PSSRC)icstate.h:$(PSSRC)ivmspace.h
$(PSSRC)icstate.h:$(PSSRC)iref.h
$(PSSRC)icstate.h:$(GLSRC)gsgc.h
$(PSSRC)icstate.h:$(GLSRC)gxalloc.h
$(PSSRC)icstate.h:$(GLSRC)gxobj.h
$(PSSRC)icstate.h:$(GLSRC)gsnamecl.h
$(PSSRC)icstate.h:$(GLSRC)stream.h
$(PSSRC)icstate.h:$(GLSRC)gxcspace.h
$(PSSRC)icstate.h:$(GLSRC)gxiodev.h
$(PSSRC)icstate.h:$(GLSRC)gscsel.h
$(PSSRC)icstate.h:$(GLSRC)gsdcolor.h
$(PSSRC)icstate.h:$(GLSRC)gxfrac.h
$(PSSRC)icstate.h:$(GLSRC)gscms.h
$(PSSRC)icstate.h:$(GLSRC)gsdevice.h
$(PSSRC)icstate.h:$(GLSRC)gscspace.h
$(PSSRC)icstate.h:$(GLSRC)gsgstate.h
$(PSSRC)icstate.h:$(GLSRC)gsiparam.h
$(PSSRC)icstate.h:$(GLSRC)gsmatrix.h
$(PSSRC)icstate.h:$(GLSRC)gxhttile.h
$(PSSRC)icstate.h:$(GLSRC)gsparam.h
$(PSSRC)icstate.h:$(GLSRC)gsrefct.h
$(PSSRC)icstate.h:$(GLSRC)gp.h
$(PSSRC)icstate.h:$(GLSRC)memento.h
$(PSSRC)icstate.h:$(GLSRC)memory_.h
$(PSSRC)icstate.h:$(GLSRC)gsstruct.h
$(PSSRC)icstate.h:$(GLSRC)gxsync.h
$(PSSRC)icstate.h:$(GLSRC)gxbitmap.h
$(PSSRC)icstate.h:$(GLSRC)srdline.h
$(PSSRC)icstate.h:$(GLSRC)scommon.h
$(PSSRC)icstate.h:$(GLSRC)gsfname.h
$(PSSRC)icstate.h:$(GLSRC)gsbitmap.h
$(PSSRC)icstate.h:$(GLSRC)gsccolor.h
$(PSSRC)icstate.h:$(GLSRC)gxarith.h
$(PSSRC)icstate.h:$(GLSRC)stat_.h
$(PSSRC)icstate.h:$(GLSRC)gpsync.h
$(PSSRC)icstate.h:$(GLSRC)gsstype.h
$(PSSRC)icstate.h:$(GLSRC)gsmemory.h
$(PSSRC)icstate.h:$(GLSRC)gpgetenv.h
$(PSSRC)icstate.h:$(GLSRC)gscdefs.h
$(PSSRC)icstate.h:$(GLSRC)gslibctx.h
$(PSSRC)icstate.h:$(GLSRC)gsalloc.h
$(PSSRC)icstate.h:$(GLSRC)gxcindex.h
$(PSSRC)icstate.h:$(GLSRC)stdio_.h
$(PSSRC)icstate.h:$(GLSRC)stdint_.h
$(PSSRC)icstate.h:$(GLSRC)gssprintf.h
$(PSSRC)icstate.h:$(GLSRC)gstypes.h
$(PSSRC)icstate.h:$(GLSRC)std.h
$(PSSRC)icstate.h:$(GLSRC)stdpre.h
$(PSSRC)icstate.h:$(GLGEN)arch.h
$(PSSRC)icstate.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iddict.h:$(PSSRC)icstate.h
$(PSSRC)iddict.h:$(PSSRC)idsdata.h
$(PSSRC)iddict.h:$(PSSRC)iesdata.h
$(PSSRC)iddict.h:$(PSSRC)interp.h
$(PSSRC)iddict.h:$(PSSRC)files.h
$(PSSRC)iddict.h:$(PSSRC)opdef.h
$(PSSRC)iddict.h:$(PSSRC)idict.h
$(PSSRC)iddict.h:$(PSSRC)iddstack.h
$(PSSRC)iddict.h:$(PSSRC)store.h
$(PSSRC)iddict.h:$(PSSRC)iosdata.h
$(PSSRC)iddict.h:$(PSSRC)ialloc.h
$(PSSRC)iddict.h:$(PSSRC)idosave.h
$(PSSRC)iddict.h:$(PSSRC)isdata.h
$(PSSRC)iddict.h:$(PSSRC)imemory.h
$(PSSRC)iddict.h:$(PSSRC)ivmspace.h
$(PSSRC)iddict.h:$(PSSRC)iref.h
$(PSSRC)iddict.h:$(GLSRC)gsgc.h
$(PSSRC)iddict.h:$(GLSRC)gxalloc.h
$(PSSRC)iddict.h:$(GLSRC)gxobj.h
$(PSSRC)iddict.h:$(GLSRC)gsnamecl.h
$(PSSRC)iddict.h:$(GLSRC)stream.h
$(PSSRC)iddict.h:$(GLSRC)gxcspace.h
$(PSSRC)iddict.h:$(GLSRC)gxiodev.h
$(PSSRC)iddict.h:$(GLSRC)gscsel.h
$(PSSRC)iddict.h:$(GLSRC)gsdcolor.h
$(PSSRC)iddict.h:$(GLSRC)gxfrac.h
$(PSSRC)iddict.h:$(GLSRC)gscms.h
$(PSSRC)iddict.h:$(GLSRC)gsdevice.h
$(PSSRC)iddict.h:$(GLSRC)gscspace.h
$(PSSRC)iddict.h:$(GLSRC)gsgstate.h
$(PSSRC)iddict.h:$(GLSRC)gsiparam.h
$(PSSRC)iddict.h:$(GLSRC)gsmatrix.h
$(PSSRC)iddict.h:$(GLSRC)gxhttile.h
$(PSSRC)iddict.h:$(GLSRC)gsparam.h
$(PSSRC)iddict.h:$(GLSRC)gsrefct.h
$(PSSRC)iddict.h:$(GLSRC)gp.h
$(PSSRC)iddict.h:$(GLSRC)memento.h
$(PSSRC)iddict.h:$(GLSRC)memory_.h
$(PSSRC)iddict.h:$(GLSRC)gsstruct.h
$(PSSRC)iddict.h:$(GLSRC)gxsync.h
$(PSSRC)iddict.h:$(GLSRC)gxbitmap.h
$(PSSRC)iddict.h:$(GLSRC)srdline.h
$(PSSRC)iddict.h:$(GLSRC)scommon.h
$(PSSRC)iddict.h:$(GLSRC)gsfname.h
$(PSSRC)iddict.h:$(GLSRC)gsbitmap.h
$(PSSRC)iddict.h:$(GLSRC)gsccolor.h
$(PSSRC)iddict.h:$(GLSRC)gxarith.h
$(PSSRC)iddict.h:$(GLSRC)stat_.h
$(PSSRC)iddict.h:$(GLSRC)gpsync.h
$(PSSRC)iddict.h:$(GLSRC)gsstype.h
$(PSSRC)iddict.h:$(GLSRC)gsmemory.h
$(PSSRC)iddict.h:$(GLSRC)gpgetenv.h
$(PSSRC)iddict.h:$(GLSRC)gscdefs.h
$(PSSRC)iddict.h:$(GLSRC)gslibctx.h
$(PSSRC)iddict.h:$(GLSRC)gsalloc.h
$(PSSRC)iddict.h:$(GLSRC)gxcindex.h
$(PSSRC)iddict.h:$(GLSRC)stdio_.h
$(PSSRC)iddict.h:$(GLSRC)stdint_.h
$(PSSRC)iddict.h:$(GLSRC)gssprintf.h
$(PSSRC)iddict.h:$(GLSRC)gstypes.h
$(PSSRC)iddict.h:$(GLSRC)std.h
$(PSSRC)iddict.h:$(GLSRC)stdpre.h
$(PSSRC)iddict.h:$(GLGEN)arch.h
$(PSSRC)iddict.h:$(GLSRC)gs_dll_call.h
$(PSSRC)dstack.h:$(PSSRC)idstack.h
$(PSSRC)dstack.h:$(PSSRC)icstate.h
$(PSSRC)dstack.h:$(PSSRC)idsdata.h
$(PSSRC)dstack.h:$(PSSRC)iesdata.h
$(PSSRC)dstack.h:$(PSSRC)interp.h
$(PSSRC)dstack.h:$(PSSRC)files.h
$(PSSRC)dstack.h:$(PSSRC)opdef.h
$(PSSRC)dstack.h:$(PSSRC)iddstack.h
$(PSSRC)dstack.h:$(PSSRC)istack.h
$(PSSRC)dstack.h:$(PSSRC)store.h
$(PSSRC)dstack.h:$(PSSRC)iosdata.h
$(PSSRC)dstack.h:$(PSSRC)ialloc.h
$(PSSRC)dstack.h:$(PSSRC)idosave.h
$(PSSRC)dstack.h:$(PSSRC)isdata.h
$(PSSRC)dstack.h:$(PSSRC)imemory.h
$(PSSRC)dstack.h:$(PSSRC)ivmspace.h
$(PSSRC)dstack.h:$(PSSRC)iref.h
$(PSSRC)dstack.h:$(GLSRC)gsgc.h
$(PSSRC)dstack.h:$(GLSRC)gxalloc.h
$(PSSRC)dstack.h:$(GLSRC)gxobj.h
$(PSSRC)dstack.h:$(GLSRC)gsnamecl.h
$(PSSRC)dstack.h:$(GLSRC)stream.h
$(PSSRC)dstack.h:$(GLSRC)gxcspace.h
$(PSSRC)dstack.h:$(GLSRC)gxiodev.h
$(PSSRC)dstack.h:$(GLSRC)gscsel.h
$(PSSRC)dstack.h:$(GLSRC)gsdcolor.h
$(PSSRC)dstack.h:$(GLSRC)gxfrac.h
$(PSSRC)dstack.h:$(GLSRC)gscms.h
$(PSSRC)dstack.h:$(GLSRC)gsdevice.h
$(PSSRC)dstack.h:$(GLSRC)gscspace.h
$(PSSRC)dstack.h:$(GLSRC)gsgstate.h
$(PSSRC)dstack.h:$(GLSRC)gsiparam.h
$(PSSRC)dstack.h:$(GLSRC)gsmatrix.h
$(PSSRC)dstack.h:$(GLSRC)gxhttile.h
$(PSSRC)dstack.h:$(GLSRC)gsparam.h
$(PSSRC)dstack.h:$(GLSRC)gsrefct.h
$(PSSRC)dstack.h:$(GLSRC)gp.h
$(PSSRC)dstack.h:$(GLSRC)memento.h
$(PSSRC)dstack.h:$(GLSRC)memory_.h
$(PSSRC)dstack.h:$(GLSRC)gsstruct.h
$(PSSRC)dstack.h:$(GLSRC)gxsync.h
$(PSSRC)dstack.h:$(GLSRC)gxbitmap.h
$(PSSRC)dstack.h:$(GLSRC)srdline.h
$(PSSRC)dstack.h:$(GLSRC)scommon.h
$(PSSRC)dstack.h:$(GLSRC)gsfname.h
$(PSSRC)dstack.h:$(GLSRC)gsbitmap.h
$(PSSRC)dstack.h:$(GLSRC)gsccolor.h
$(PSSRC)dstack.h:$(GLSRC)gxarith.h
$(PSSRC)dstack.h:$(GLSRC)stat_.h
$(PSSRC)dstack.h:$(GLSRC)gpsync.h
$(PSSRC)dstack.h:$(GLSRC)gsstype.h
$(PSSRC)dstack.h:$(GLSRC)gsmemory.h
$(PSSRC)dstack.h:$(GLSRC)gpgetenv.h
$(PSSRC)dstack.h:$(GLSRC)gscdefs.h
$(PSSRC)dstack.h:$(GLSRC)gslibctx.h
$(PSSRC)dstack.h:$(GLSRC)gsalloc.h
$(PSSRC)dstack.h:$(GLSRC)gxcindex.h
$(PSSRC)dstack.h:$(GLSRC)stdio_.h
$(PSSRC)dstack.h:$(GLSRC)stdint_.h
$(PSSRC)dstack.h:$(GLSRC)gssprintf.h
$(PSSRC)dstack.h:$(GLSRC)gstypes.h
$(PSSRC)dstack.h:$(GLSRC)std.h
$(PSSRC)dstack.h:$(GLSRC)stdpre.h
$(PSSRC)dstack.h:$(GLGEN)arch.h
$(PSSRC)dstack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)estack.h:$(PSSRC)iestack.h
$(PSSRC)estack.h:$(PSSRC)icstate.h
$(PSSRC)estack.h:$(PSSRC)idsdata.h
$(PSSRC)estack.h:$(PSSRC)iesdata.h
$(PSSRC)estack.h:$(PSSRC)interp.h
$(PSSRC)estack.h:$(PSSRC)files.h
$(PSSRC)estack.h:$(PSSRC)opdef.h
$(PSSRC)estack.h:$(PSSRC)iddstack.h
$(PSSRC)estack.h:$(PSSRC)istack.h
$(PSSRC)estack.h:$(PSSRC)store.h
$(PSSRC)estack.h:$(PSSRC)iosdata.h
$(PSSRC)estack.h:$(PSSRC)ialloc.h
$(PSSRC)estack.h:$(PSSRC)idosave.h
$(PSSRC)estack.h:$(PSSRC)isdata.h
$(PSSRC)estack.h:$(PSSRC)imemory.h
$(PSSRC)estack.h:$(PSSRC)ivmspace.h
$(PSSRC)estack.h:$(PSSRC)iref.h
$(PSSRC)estack.h:$(GLSRC)gsgc.h
$(PSSRC)estack.h:$(GLSRC)gxalloc.h
$(PSSRC)estack.h:$(GLSRC)gxobj.h
$(PSSRC)estack.h:$(GLSRC)gsnamecl.h
$(PSSRC)estack.h:$(GLSRC)stream.h
$(PSSRC)estack.h:$(GLSRC)gxcspace.h
$(PSSRC)estack.h:$(GLSRC)gxiodev.h
$(PSSRC)estack.h:$(GLSRC)gscsel.h
$(PSSRC)estack.h:$(GLSRC)gsdcolor.h
$(PSSRC)estack.h:$(GLSRC)gxfrac.h
$(PSSRC)estack.h:$(GLSRC)gscms.h
$(PSSRC)estack.h:$(GLSRC)gsdevice.h
$(PSSRC)estack.h:$(GLSRC)gscspace.h
$(PSSRC)estack.h:$(GLSRC)gsgstate.h
$(PSSRC)estack.h:$(GLSRC)gsiparam.h
$(PSSRC)estack.h:$(GLSRC)gsmatrix.h
$(PSSRC)estack.h:$(GLSRC)gxhttile.h
$(PSSRC)estack.h:$(GLSRC)gsparam.h
$(PSSRC)estack.h:$(GLSRC)gsrefct.h
$(PSSRC)estack.h:$(GLSRC)gp.h
$(PSSRC)estack.h:$(GLSRC)memento.h
$(PSSRC)estack.h:$(GLSRC)memory_.h
$(PSSRC)estack.h:$(GLSRC)gsstruct.h
$(PSSRC)estack.h:$(GLSRC)gxsync.h
$(PSSRC)estack.h:$(GLSRC)gxbitmap.h
$(PSSRC)estack.h:$(GLSRC)srdline.h
$(PSSRC)estack.h:$(GLSRC)scommon.h
$(PSSRC)estack.h:$(GLSRC)gsfname.h
$(PSSRC)estack.h:$(GLSRC)gsbitmap.h
$(PSSRC)estack.h:$(GLSRC)gsccolor.h
$(PSSRC)estack.h:$(GLSRC)gxarith.h
$(PSSRC)estack.h:$(GLSRC)stat_.h
$(PSSRC)estack.h:$(GLSRC)gpsync.h
$(PSSRC)estack.h:$(GLSRC)gsstype.h
$(PSSRC)estack.h:$(GLSRC)gsmemory.h
$(PSSRC)estack.h:$(GLSRC)gpgetenv.h
$(PSSRC)estack.h:$(GLSRC)gscdefs.h
$(PSSRC)estack.h:$(GLSRC)gslibctx.h
$(PSSRC)estack.h:$(GLSRC)gsalloc.h
$(PSSRC)estack.h:$(GLSRC)gxcindex.h
$(PSSRC)estack.h:$(GLSRC)stdio_.h
$(PSSRC)estack.h:$(GLSRC)stdint_.h
$(PSSRC)estack.h:$(GLSRC)gssprintf.h
$(PSSRC)estack.h:$(GLSRC)gstypes.h
$(PSSRC)estack.h:$(GLSRC)std.h
$(PSSRC)estack.h:$(GLSRC)stdpre.h
$(PSSRC)estack.h:$(GLGEN)arch.h
$(PSSRC)estack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ostack.h:$(PSSRC)icstate.h
$(PSSRC)ostack.h:$(PSSRC)idsdata.h
$(PSSRC)ostack.h:$(PSSRC)iesdata.h
$(PSSRC)ostack.h:$(PSSRC)iostack.h
$(PSSRC)ostack.h:$(PSSRC)interp.h
$(PSSRC)ostack.h:$(PSSRC)files.h
$(PSSRC)ostack.h:$(PSSRC)opdef.h
$(PSSRC)ostack.h:$(PSSRC)iddstack.h
$(PSSRC)ostack.h:$(PSSRC)istack.h
$(PSSRC)ostack.h:$(PSSRC)store.h
$(PSSRC)ostack.h:$(PSSRC)iosdata.h
$(PSSRC)ostack.h:$(PSSRC)ialloc.h
$(PSSRC)ostack.h:$(PSSRC)idosave.h
$(PSSRC)ostack.h:$(PSSRC)isdata.h
$(PSSRC)ostack.h:$(PSSRC)imemory.h
$(PSSRC)ostack.h:$(PSSRC)ivmspace.h
$(PSSRC)ostack.h:$(PSSRC)iref.h
$(PSSRC)ostack.h:$(GLSRC)gsgc.h
$(PSSRC)ostack.h:$(GLSRC)gxalloc.h
$(PSSRC)ostack.h:$(GLSRC)gxobj.h
$(PSSRC)ostack.h:$(GLSRC)gsnamecl.h
$(PSSRC)ostack.h:$(GLSRC)stream.h
$(PSSRC)ostack.h:$(GLSRC)gxcspace.h
$(PSSRC)ostack.h:$(GLSRC)gxiodev.h
$(PSSRC)ostack.h:$(GLSRC)gscsel.h
$(PSSRC)ostack.h:$(GLSRC)gsdcolor.h
$(PSSRC)ostack.h:$(GLSRC)gxfrac.h
$(PSSRC)ostack.h:$(GLSRC)gscms.h
$(PSSRC)ostack.h:$(GLSRC)gsdevice.h
$(PSSRC)ostack.h:$(GLSRC)gscspace.h
$(PSSRC)ostack.h:$(GLSRC)gsgstate.h
$(PSSRC)ostack.h:$(GLSRC)gsiparam.h
$(PSSRC)ostack.h:$(GLSRC)gsmatrix.h
$(PSSRC)ostack.h:$(GLSRC)gxhttile.h
$(PSSRC)ostack.h:$(GLSRC)gsparam.h
$(PSSRC)ostack.h:$(GLSRC)gsrefct.h
$(PSSRC)ostack.h:$(GLSRC)gp.h
$(PSSRC)ostack.h:$(GLSRC)memento.h
$(PSSRC)ostack.h:$(GLSRC)memory_.h
$(PSSRC)ostack.h:$(GLSRC)gsstruct.h
$(PSSRC)ostack.h:$(GLSRC)gxsync.h
$(PSSRC)ostack.h:$(GLSRC)gxbitmap.h
$(PSSRC)ostack.h:$(GLSRC)srdline.h
$(PSSRC)ostack.h:$(GLSRC)scommon.h
$(PSSRC)ostack.h:$(GLSRC)gsfname.h
$(PSSRC)ostack.h:$(GLSRC)gsbitmap.h
$(PSSRC)ostack.h:$(GLSRC)gsccolor.h
$(PSSRC)ostack.h:$(GLSRC)gxarith.h
$(PSSRC)ostack.h:$(GLSRC)stat_.h
$(PSSRC)ostack.h:$(GLSRC)gpsync.h
$(PSSRC)ostack.h:$(GLSRC)gsstype.h
$(PSSRC)ostack.h:$(GLSRC)gsmemory.h
$(PSSRC)ostack.h:$(GLSRC)gpgetenv.h
$(PSSRC)ostack.h:$(GLSRC)gscdefs.h
$(PSSRC)ostack.h:$(GLSRC)gslibctx.h
$(PSSRC)ostack.h:$(GLSRC)gsalloc.h
$(PSSRC)ostack.h:$(GLSRC)gxcindex.h
$(PSSRC)ostack.h:$(GLSRC)stdio_.h
$(PSSRC)ostack.h:$(GLSRC)stdint_.h
$(PSSRC)ostack.h:$(GLSRC)gssprintf.h
$(PSSRC)ostack.h:$(GLSRC)gstypes.h
$(PSSRC)ostack.h:$(GLSRC)std.h
$(PSSRC)ostack.h:$(GLSRC)stdpre.h
$(PSSRC)ostack.h:$(GLGEN)arch.h
$(PSSRC)ostack.h:$(GLSRC)gs_dll_call.h
$(PSSRC)oper.h:$(PSSRC)ostack.h
$(PSSRC)oper.h:$(PSSRC)icstate.h
$(PSSRC)oper.h:$(PSSRC)idsdata.h
$(PSSRC)oper.h:$(PSSRC)iesdata.h
$(PSSRC)oper.h:$(PSSRC)iostack.h
$(PSSRC)oper.h:$(PSSRC)interp.h
$(PSSRC)oper.h:$(PSSRC)files.h
$(PSSRC)oper.h:$(PSSRC)opdef.h
$(PSSRC)oper.h:$(PSSRC)iddstack.h
$(PSSRC)oper.h:$(PSSRC)istack.h
$(PSSRC)oper.h:$(PSSRC)store.h
$(PSSRC)oper.h:$(PSSRC)iosdata.h
$(PSSRC)oper.h:$(PSSRC)iutil.h
$(PSSRC)oper.h:$(PSSRC)ialloc.h
$(PSSRC)oper.h:$(PSSRC)idosave.h
$(PSSRC)oper.h:$(PSSRC)ierrors.h
$(PSSRC)oper.h:$(PSSRC)opcheck.h
$(PSSRC)oper.h:$(PSSRC)isdata.h
$(PSSRC)oper.h:$(PSSRC)imemory.h
$(PSSRC)oper.h:$(PSSRC)opextern.h
$(PSSRC)oper.h:$(PSSRC)ivmspace.h
$(PSSRC)oper.h:$(PSSRC)iref.h
$(PSSRC)oper.h:$(GLSRC)gsgc.h
$(PSSRC)oper.h:$(GLSRC)gxalloc.h
$(PSSRC)oper.h:$(GLSRC)gxobj.h
$(PSSRC)oper.h:$(GLSRC)gsnamecl.h
$(PSSRC)oper.h:$(GLSRC)stream.h
$(PSSRC)oper.h:$(GLSRC)gxcspace.h
$(PSSRC)oper.h:$(GLSRC)gxiodev.h
$(PSSRC)oper.h:$(GLSRC)gscsel.h
$(PSSRC)oper.h:$(GLSRC)gsdcolor.h
$(PSSRC)oper.h:$(GLSRC)gxfrac.h
$(PSSRC)oper.h:$(GLSRC)gscms.h
$(PSSRC)oper.h:$(GLSRC)gsdevice.h
$(PSSRC)oper.h:$(GLSRC)gscspace.h
$(PSSRC)oper.h:$(GLSRC)gsgstate.h
$(PSSRC)oper.h:$(GLSRC)gsiparam.h
$(PSSRC)oper.h:$(GLSRC)gsmatrix.h
$(PSSRC)oper.h:$(GLSRC)gxhttile.h
$(PSSRC)oper.h:$(GLSRC)gsparam.h
$(PSSRC)oper.h:$(GLSRC)gsrefct.h
$(PSSRC)oper.h:$(GLSRC)gp.h
$(PSSRC)oper.h:$(GLSRC)memento.h
$(PSSRC)oper.h:$(GLSRC)memory_.h
$(PSSRC)oper.h:$(GLSRC)gsstruct.h
$(PSSRC)oper.h:$(GLSRC)gxsync.h
$(PSSRC)oper.h:$(GLSRC)gserrors.h
$(PSSRC)oper.h:$(GLSRC)gxbitmap.h
$(PSSRC)oper.h:$(GLSRC)srdline.h
$(PSSRC)oper.h:$(GLSRC)scommon.h
$(PSSRC)oper.h:$(GLSRC)gsfname.h
$(PSSRC)oper.h:$(GLSRC)gsbitmap.h
$(PSSRC)oper.h:$(GLSRC)gsccolor.h
$(PSSRC)oper.h:$(GLSRC)gxarith.h
$(PSSRC)oper.h:$(GLSRC)stat_.h
$(PSSRC)oper.h:$(GLSRC)gpsync.h
$(PSSRC)oper.h:$(GLSRC)gsstype.h
$(PSSRC)oper.h:$(GLSRC)gsmemory.h
$(PSSRC)oper.h:$(GLSRC)gpgetenv.h
$(PSSRC)oper.h:$(GLSRC)gscdefs.h
$(PSSRC)oper.h:$(GLSRC)gslibctx.h
$(PSSRC)oper.h:$(GLSRC)gsalloc.h
$(PSSRC)oper.h:$(GLSRC)gxcindex.h
$(PSSRC)oper.h:$(GLSRC)stdio_.h
$(PSSRC)oper.h:$(GLSRC)stdint_.h
$(PSSRC)oper.h:$(GLSRC)gssprintf.h
$(PSSRC)oper.h:$(GLSRC)gstypes.h
$(PSSRC)oper.h:$(GLSRC)std.h
$(PSSRC)oper.h:$(GLSRC)stdpre.h
$(PSSRC)oper.h:$(GLGEN)arch.h
$(PSSRC)oper.h:$(GLSRC)gs_dll_call.h
$(PSSRC)btoken.h:$(PSSRC)iref.h
$(PSSRC)btoken.h:$(GLSRC)gxalloc.h
$(PSSRC)btoken.h:$(GLSRC)gxobj.h
$(PSSRC)btoken.h:$(GLSRC)gsnamecl.h
$(PSSRC)btoken.h:$(GLSRC)gxcspace.h
$(PSSRC)btoken.h:$(GLSRC)gscsel.h
$(PSSRC)btoken.h:$(GLSRC)gsdcolor.h
$(PSSRC)btoken.h:$(GLSRC)gxfrac.h
$(PSSRC)btoken.h:$(GLSRC)gscms.h
$(PSSRC)btoken.h:$(GLSRC)gsdevice.h
$(PSSRC)btoken.h:$(GLSRC)gscspace.h
$(PSSRC)btoken.h:$(GLSRC)gsgstate.h
$(PSSRC)btoken.h:$(GLSRC)gsiparam.h
$(PSSRC)btoken.h:$(GLSRC)gsmatrix.h
$(PSSRC)btoken.h:$(GLSRC)gxhttile.h
$(PSSRC)btoken.h:$(GLSRC)gsparam.h
$(PSSRC)btoken.h:$(GLSRC)gsrefct.h
$(PSSRC)btoken.h:$(GLSRC)memento.h
$(PSSRC)btoken.h:$(GLSRC)gsstruct.h
$(PSSRC)btoken.h:$(GLSRC)gxsync.h
$(PSSRC)btoken.h:$(GLSRC)gxbitmap.h
$(PSSRC)btoken.h:$(GLSRC)scommon.h
$(PSSRC)btoken.h:$(GLSRC)gsbitmap.h
$(PSSRC)btoken.h:$(GLSRC)gsccolor.h
$(PSSRC)btoken.h:$(GLSRC)gxarith.h
$(PSSRC)btoken.h:$(GLSRC)gpsync.h
$(PSSRC)btoken.h:$(GLSRC)gsstype.h
$(PSSRC)btoken.h:$(GLSRC)gsmemory.h
$(PSSRC)btoken.h:$(GLSRC)gslibctx.h
$(PSSRC)btoken.h:$(GLSRC)gsalloc.h
$(PSSRC)btoken.h:$(GLSRC)gxcindex.h
$(PSSRC)btoken.h:$(GLSRC)stdio_.h
$(PSSRC)btoken.h:$(GLSRC)stdint_.h
$(PSSRC)btoken.h:$(GLSRC)gssprintf.h
$(PSSRC)btoken.h:$(GLSRC)gstypes.h
$(PSSRC)btoken.h:$(GLSRC)std.h
$(PSSRC)btoken.h:$(GLSRC)stdpre.h
$(PSSRC)btoken.h:$(GLGEN)arch.h
$(PSSRC)btoken.h:$(GLSRC)gs_dll_call.h
$(PSSRC)files.h:$(PSSRC)store.h
$(PSSRC)files.h:$(PSSRC)ialloc.h
$(PSSRC)files.h:$(PSSRC)idosave.h
$(PSSRC)files.h:$(PSSRC)imemory.h
$(PSSRC)files.h:$(PSSRC)ivmspace.h
$(PSSRC)files.h:$(PSSRC)iref.h
$(PSSRC)files.h:$(GLSRC)gsgc.h
$(PSSRC)files.h:$(GLSRC)gxalloc.h
$(PSSRC)files.h:$(GLSRC)gxobj.h
$(PSSRC)files.h:$(GLSRC)gsnamecl.h
$(PSSRC)files.h:$(GLSRC)stream.h
$(PSSRC)files.h:$(GLSRC)gxcspace.h
$(PSSRC)files.h:$(GLSRC)gxiodev.h
$(PSSRC)files.h:$(GLSRC)gscsel.h
$(PSSRC)files.h:$(GLSRC)gsdcolor.h
$(PSSRC)files.h:$(GLSRC)gxfrac.h
$(PSSRC)files.h:$(GLSRC)gscms.h
$(PSSRC)files.h:$(GLSRC)gsdevice.h
$(PSSRC)files.h:$(GLSRC)gscspace.h
$(PSSRC)files.h:$(GLSRC)gsgstate.h
$(PSSRC)files.h:$(GLSRC)gsiparam.h
$(PSSRC)files.h:$(GLSRC)gsmatrix.h
$(PSSRC)files.h:$(GLSRC)gxhttile.h
$(PSSRC)files.h:$(GLSRC)gsparam.h
$(PSSRC)files.h:$(GLSRC)gsrefct.h
$(PSSRC)files.h:$(GLSRC)gp.h
$(PSSRC)files.h:$(GLSRC)memento.h
$(PSSRC)files.h:$(GLSRC)memory_.h
$(PSSRC)files.h:$(GLSRC)gsstruct.h
$(PSSRC)files.h:$(GLSRC)gxsync.h
$(PSSRC)files.h:$(GLSRC)gxbitmap.h
$(PSSRC)files.h:$(GLSRC)srdline.h
$(PSSRC)files.h:$(GLSRC)scommon.h
$(PSSRC)files.h:$(GLSRC)gsfname.h
$(PSSRC)files.h:$(GLSRC)gsbitmap.h
$(PSSRC)files.h:$(GLSRC)gsccolor.h
$(PSSRC)files.h:$(GLSRC)gxarith.h
$(PSSRC)files.h:$(GLSRC)stat_.h
$(PSSRC)files.h:$(GLSRC)gpsync.h
$(PSSRC)files.h:$(GLSRC)gsstype.h
$(PSSRC)files.h:$(GLSRC)gsmemory.h
$(PSSRC)files.h:$(GLSRC)gpgetenv.h
$(PSSRC)files.h:$(GLSRC)gscdefs.h
$(PSSRC)files.h:$(GLSRC)gslibctx.h
$(PSSRC)files.h:$(GLSRC)gsalloc.h
$(PSSRC)files.h:$(GLSRC)gxcindex.h
$(PSSRC)files.h:$(GLSRC)stdio_.h
$(PSSRC)files.h:$(GLSRC)stdint_.h
$(PSSRC)files.h:$(GLSRC)gssprintf.h
$(PSSRC)files.h:$(GLSRC)gstypes.h
$(PSSRC)files.h:$(GLSRC)std.h
$(PSSRC)files.h:$(GLSRC)stdpre.h
$(PSSRC)files.h:$(GLGEN)arch.h
$(PSSRC)files.h:$(GLSRC)gs_dll_call.h
$(PSSRC)psapi.h:$(GLSRC)gsdevice.h
$(PSSRC)psapi.h:$(GLSRC)gsgstate.h
$(PSSRC)psapi.h:$(GLSRC)gsmatrix.h
$(PSSRC)psapi.h:$(GLSRC)gsparam.h
$(PSSRC)psapi.h:$(GLSRC)scommon.h
$(PSSRC)psapi.h:$(GLSRC)gsstype.h
$(PSSRC)psapi.h:$(GLSRC)gsmemory.h
$(PSSRC)psapi.h:$(GLSRC)gslibctx.h
$(PSSRC)psapi.h:$(GLSRC)stdio_.h
$(PSSRC)psapi.h:$(GLSRC)stdint_.h
$(PSSRC)psapi.h:$(GLSRC)gssprintf.h
$(PSSRC)psapi.h:$(GLSRC)gstypes.h
$(PSSRC)psapi.h:$(GLSRC)std.h
$(PSSRC)psapi.h:$(GLSRC)stdpre.h
$(PSSRC)psapi.h:$(GLGEN)arch.h
$(PSSRC)psapi.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ichar.h:$(PSSRC)iostack.h
$(PSSRC)ichar.h:$(PSSRC)istack.h
$(PSSRC)ichar.h:$(PSSRC)iosdata.h
$(PSSRC)ichar.h:$(GLSRC)gxfapi.h
$(PSSRC)ichar.h:$(PSSRC)isdata.h
$(PSSRC)ichar.h:$(PSSRC)imemory.h
$(PSSRC)ichar.h:$(PSSRC)ivmspace.h
$(PSSRC)ichar.h:$(PSSRC)iref.h
$(PSSRC)ichar.h:$(GLSRC)gsgc.h
$(PSSRC)ichar.h:$(GLSRC)gxalloc.h
$(PSSRC)ichar.h:$(GLSRC)gxobj.h
$(PSSRC)ichar.h:$(GLSRC)gstext.h
$(PSSRC)ichar.h:$(GLSRC)gsnamecl.h
$(PSSRC)ichar.h:$(GLSRC)gxcspace.h
$(PSSRC)ichar.h:$(GLSRC)gscsel.h
$(PSSRC)ichar.h:$(GLSRC)gsfont.h
$(PSSRC)ichar.h:$(GLSRC)gsdcolor.h
$(PSSRC)ichar.h:$(GLSRC)gxpath.h
$(PSSRC)ichar.h:$(GLSRC)gxfrac.h
$(PSSRC)ichar.h:$(GLSRC)gscms.h
$(PSSRC)ichar.h:$(GLSRC)gsrect.h
$(PSSRC)ichar.h:$(GLSRC)gslparam.h
$(PSSRC)ichar.h:$(GLSRC)gsdevice.h
$(PSSRC)ichar.h:$(GLSRC)gscpm.h
$(PSSRC)ichar.h:$(GLSRC)gscspace.h
$(PSSRC)ichar.h:$(GLSRC)gsgstate.h
$(PSSRC)ichar.h:$(GLSRC)gsiparam.h
$(PSSRC)ichar.h:$(GLSRC)gxfixed.h
$(PSSRC)ichar.h:$(GLSRC)gsmatrix.h
$(PSSRC)ichar.h:$(GLSRC)gspenum.h
$(PSSRC)ichar.h:$(GLSRC)gxhttile.h
$(PSSRC)ichar.h:$(GLSRC)gsparam.h
$(PSSRC)ichar.h:$(GLSRC)gsrefct.h
$(PSSRC)ichar.h:$(GLSRC)memento.h
$(PSSRC)ichar.h:$(GLSRC)gsstruct.h
$(PSSRC)ichar.h:$(GLSRC)gxsync.h
$(PSSRC)ichar.h:$(GLSRC)gxbitmap.h
$(PSSRC)ichar.h:$(GLSRC)scommon.h
$(PSSRC)ichar.h:$(GLSRC)gsbitmap.h
$(PSSRC)ichar.h:$(GLSRC)gsccolor.h
$(PSSRC)ichar.h:$(GLSRC)gxarith.h
$(PSSRC)ichar.h:$(GLSRC)gpsync.h
$(PSSRC)ichar.h:$(GLSRC)gsstype.h
$(PSSRC)ichar.h:$(GLSRC)gsmemory.h
$(PSSRC)ichar.h:$(GLSRC)gslibctx.h
$(PSSRC)ichar.h:$(GLSRC)gsalloc.h
$(PSSRC)ichar.h:$(GLSRC)gxcindex.h
$(PSSRC)ichar.h:$(GLSRC)stdio_.h
$(PSSRC)ichar.h:$(GLSRC)gsccode.h
$(PSSRC)ichar.h:$(GLSRC)stdint_.h
$(PSSRC)ichar.h:$(GLSRC)gssprintf.h
$(PSSRC)ichar.h:$(GLSRC)gstypes.h
$(PSSRC)ichar.h:$(GLSRC)std.h
$(PSSRC)ichar.h:$(GLSRC)stdpre.h
$(PSSRC)ichar.h:$(GLGEN)arch.h
$(PSSRC)ichar.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ichar1.h:$(GLSRC)gstype1.h
$(PSSRC)ichar1.h:$(GLSRC)gxfont.h
$(PSSRC)ichar1.h:$(GLSRC)gspath.h
$(PSSRC)ichar1.h:$(GLSRC)gsgdata.h
$(PSSRC)ichar1.h:$(GLSRC)gxmatrix.h
$(PSSRC)ichar1.h:$(GLSRC)gxfapi.h
$(PSSRC)ichar1.h:$(GLSRC)gsfcmap.h
$(PSSRC)ichar1.h:$(PSSRC)iref.h
$(PSSRC)ichar1.h:$(GLSRC)gxalloc.h
$(PSSRC)ichar1.h:$(GLSRC)gxobj.h
$(PSSRC)ichar1.h:$(GLSRC)gstext.h
$(PSSRC)ichar1.h:$(GLSRC)gsnamecl.h
$(PSSRC)ichar1.h:$(GLSRC)gxcspace.h
$(PSSRC)ichar1.h:$(GLSRC)gscsel.h
$(PSSRC)ichar1.h:$(GLSRC)gsfont.h
$(PSSRC)ichar1.h:$(GLSRC)gsdcolor.h
$(PSSRC)ichar1.h:$(GLSRC)gxpath.h
$(PSSRC)ichar1.h:$(GLSRC)gxfrac.h
$(PSSRC)ichar1.h:$(GLSRC)gxftype.h
$(PSSRC)ichar1.h:$(GLSRC)gscms.h
$(PSSRC)ichar1.h:$(GLSRC)gsrect.h
$(PSSRC)ichar1.h:$(GLSRC)gslparam.h
$(PSSRC)ichar1.h:$(GLSRC)gsdevice.h
$(PSSRC)ichar1.h:$(GLSRC)gscpm.h
$(PSSRC)ichar1.h:$(GLSRC)gsgcache.h
$(PSSRC)ichar1.h:$(GLSRC)gscspace.h
$(PSSRC)ichar1.h:$(GLSRC)gsgstate.h
$(PSSRC)ichar1.h:$(GLSRC)gsnotify.h
$(PSSRC)ichar1.h:$(GLSRC)gsiparam.h
$(PSSRC)ichar1.h:$(GLSRC)gxfixed.h
$(PSSRC)ichar1.h:$(GLSRC)gsmatrix.h
$(PSSRC)ichar1.h:$(GLSRC)gspenum.h
$(PSSRC)ichar1.h:$(GLSRC)gxhttile.h
$(PSSRC)ichar1.h:$(GLSRC)gsparam.h
$(PSSRC)ichar1.h:$(GLSRC)gsrefct.h
$(PSSRC)ichar1.h:$(GLSRC)memento.h
$(PSSRC)ichar1.h:$(GLSRC)gsuid.h
$(PSSRC)ichar1.h:$(GLSRC)gsstruct.h
$(PSSRC)ichar1.h:$(GLSRC)gxsync.h
$(PSSRC)ichar1.h:$(GLSRC)gxbitmap.h
$(PSSRC)ichar1.h:$(GLSRC)scommon.h
$(PSSRC)ichar1.h:$(GLSRC)gsbitmap.h
$(PSSRC)ichar1.h:$(GLSRC)gsccolor.h
$(PSSRC)ichar1.h:$(GLSRC)gxarith.h
$(PSSRC)ichar1.h:$(GLSRC)gpsync.h
$(PSSRC)ichar1.h:$(GLSRC)gsstype.h
$(PSSRC)ichar1.h:$(GLSRC)gsmemory.h
$(PSSRC)ichar1.h:$(GLSRC)gslibctx.h
$(PSSRC)ichar1.h:$(GLSRC)gsalloc.h
$(PSSRC)ichar1.h:$(GLSRC)gxcindex.h
$(PSSRC)ichar1.h:$(GLSRC)stdio_.h
$(PSSRC)ichar1.h:$(GLSRC)gsccode.h
$(PSSRC)ichar1.h:$(GLSRC)stdint_.h
$(PSSRC)ichar1.h:$(GLSRC)gssprintf.h
$(PSSRC)ichar1.h:$(GLSRC)gstypes.h
$(PSSRC)ichar1.h:$(GLSRC)std.h
$(PSSRC)ichar1.h:$(GLSRC)stdpre.h
$(PSSRC)ichar1.h:$(GLGEN)arch.h
$(PSSRC)ichar1.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icharout.h:$(GLSRC)gsgdata.h
$(PSSRC)icharout.h:$(GLSRC)gxfapi.h
$(PSSRC)icharout.h:$(PSSRC)iref.h
$(PSSRC)icharout.h:$(GLSRC)gxalloc.h
$(PSSRC)icharout.h:$(GLSRC)gxobj.h
$(PSSRC)icharout.h:$(GLSRC)gstext.h
$(PSSRC)icharout.h:$(GLSRC)gsnamecl.h
$(PSSRC)icharout.h:$(GLSRC)gxcspace.h
$(PSSRC)icharout.h:$(GLSRC)gscsel.h
$(PSSRC)icharout.h:$(GLSRC)gsfont.h
$(PSSRC)icharout.h:$(GLSRC)gsdcolor.h
$(PSSRC)icharout.h:$(GLSRC)gxpath.h
$(PSSRC)icharout.h:$(GLSRC)gxfrac.h
$(PSSRC)icharout.h:$(GLSRC)gscms.h
$(PSSRC)icharout.h:$(GLSRC)gsrect.h
$(PSSRC)icharout.h:$(GLSRC)gslparam.h
$(PSSRC)icharout.h:$(GLSRC)gsdevice.h
$(PSSRC)icharout.h:$(GLSRC)gscpm.h
$(PSSRC)icharout.h:$(GLSRC)gsgcache.h
$(PSSRC)icharout.h:$(GLSRC)gscspace.h
$(PSSRC)icharout.h:$(GLSRC)gsgstate.h
$(PSSRC)icharout.h:$(GLSRC)gsiparam.h
$(PSSRC)icharout.h:$(GLSRC)gxfixed.h
$(PSSRC)icharout.h:$(GLSRC)gsmatrix.h
$(PSSRC)icharout.h:$(GLSRC)gspenum.h
$(PSSRC)icharout.h:$(GLSRC)gxhttile.h
$(PSSRC)icharout.h:$(GLSRC)gsparam.h
$(PSSRC)icharout.h:$(GLSRC)gsrefct.h
$(PSSRC)icharout.h:$(GLSRC)memento.h
$(PSSRC)icharout.h:$(GLSRC)gsstruct.h
$(PSSRC)icharout.h:$(GLSRC)gxsync.h
$(PSSRC)icharout.h:$(GLSRC)gxbitmap.h
$(PSSRC)icharout.h:$(GLSRC)scommon.h
$(PSSRC)icharout.h:$(GLSRC)gsbitmap.h
$(PSSRC)icharout.h:$(GLSRC)gsccolor.h
$(PSSRC)icharout.h:$(GLSRC)gxarith.h
$(PSSRC)icharout.h:$(GLSRC)gpsync.h
$(PSSRC)icharout.h:$(GLSRC)gsstype.h
$(PSSRC)icharout.h:$(GLSRC)gsmemory.h
$(PSSRC)icharout.h:$(GLSRC)gslibctx.h
$(PSSRC)icharout.h:$(GLSRC)gsalloc.h
$(PSSRC)icharout.h:$(GLSRC)gxcindex.h
$(PSSRC)icharout.h:$(GLSRC)stdio_.h
$(PSSRC)icharout.h:$(GLSRC)gsccode.h
$(PSSRC)icharout.h:$(GLSRC)stdint_.h
$(PSSRC)icharout.h:$(GLSRC)gssprintf.h
$(PSSRC)icharout.h:$(GLSRC)gstypes.h
$(PSSRC)icharout.h:$(GLSRC)std.h
$(PSSRC)icharout.h:$(GLSRC)stdpre.h
$(PSSRC)icharout.h:$(GLGEN)arch.h
$(PSSRC)icharout.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icolor.h:$(PSSRC)iref.h
$(PSSRC)icolor.h:$(GLSRC)gxalloc.h
$(PSSRC)icolor.h:$(GLSRC)gxobj.h
$(PSSRC)icolor.h:$(GLSRC)gsnamecl.h
$(PSSRC)icolor.h:$(GLSRC)gxcspace.h
$(PSSRC)icolor.h:$(GLSRC)gscsel.h
$(PSSRC)icolor.h:$(GLSRC)gsdcolor.h
$(PSSRC)icolor.h:$(GLSRC)gxfrac.h
$(PSSRC)icolor.h:$(GLSRC)gxtmap.h
$(PSSRC)icolor.h:$(GLSRC)gscms.h
$(PSSRC)icolor.h:$(GLSRC)gsdevice.h
$(PSSRC)icolor.h:$(GLSRC)gscspace.h
$(PSSRC)icolor.h:$(GLSRC)gsgstate.h
$(PSSRC)icolor.h:$(GLSRC)gsiparam.h
$(PSSRC)icolor.h:$(GLSRC)gsmatrix.h
$(PSSRC)icolor.h:$(GLSRC)gxhttile.h
$(PSSRC)icolor.h:$(GLSRC)gsparam.h
$(PSSRC)icolor.h:$(GLSRC)gsrefct.h
$(PSSRC)icolor.h:$(GLSRC)memento.h
$(PSSRC)icolor.h:$(GLSRC)gsstruct.h
$(PSSRC)icolor.h:$(GLSRC)gxsync.h
$(PSSRC)icolor.h:$(GLSRC)gxbitmap.h
$(PSSRC)icolor.h:$(GLSRC)scommon.h
$(PSSRC)icolor.h:$(GLSRC)gsbitmap.h
$(PSSRC)icolor.h:$(GLSRC)gsccolor.h
$(PSSRC)icolor.h:$(GLSRC)gxarith.h
$(PSSRC)icolor.h:$(GLSRC)gpsync.h
$(PSSRC)icolor.h:$(GLSRC)gsstype.h
$(PSSRC)icolor.h:$(GLSRC)gsmemory.h
$(PSSRC)icolor.h:$(GLSRC)gslibctx.h
$(PSSRC)icolor.h:$(GLSRC)gsalloc.h
$(PSSRC)icolor.h:$(GLSRC)gxcindex.h
$(PSSRC)icolor.h:$(GLSRC)stdio_.h
$(PSSRC)icolor.h:$(GLSRC)stdint_.h
$(PSSRC)icolor.h:$(GLSRC)gssprintf.h
$(PSSRC)icolor.h:$(GLSRC)gstypes.h
$(PSSRC)icolor.h:$(GLSRC)std.h
$(PSSRC)icolor.h:$(GLSRC)stdpre.h
$(PSSRC)icolor.h:$(GLGEN)arch.h
$(PSSRC)icolor.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icremap.h:$(PSSRC)igstate.h
$(PSSRC)icremap.h:$(GLSRC)gsstate.h
$(PSSRC)icremap.h:$(GLSRC)gsovrc.h
$(PSSRC)icremap.h:$(GLSRC)gscolor.h
$(PSSRC)icremap.h:$(GLSRC)gsline.h
$(PSSRC)icremap.h:$(GLSRC)gxcomp.h
$(PSSRC)icremap.h:$(PSSRC)istruct.h
$(PSSRC)icremap.h:$(PSSRC)imemory.h
$(PSSRC)icremap.h:$(GLSRC)gsht.h
$(PSSRC)icremap.h:$(PSSRC)ivmspace.h
$(PSSRC)icremap.h:$(PSSRC)iref.h
$(PSSRC)icremap.h:$(GLSRC)gsgc.h
$(PSSRC)icremap.h:$(GLSRC)gxalloc.h
$(PSSRC)icremap.h:$(GLSRC)gxobj.h
$(PSSRC)icremap.h:$(GLSRC)gxstate.h
$(PSSRC)icremap.h:$(GLSRC)gsnamecl.h
$(PSSRC)icremap.h:$(GLSRC)gxcspace.h
$(PSSRC)icremap.h:$(GLSRC)gscsel.h
$(PSSRC)icremap.h:$(GLSRC)gsdcolor.h
$(PSSRC)icremap.h:$(GLSRC)gxfrac.h
$(PSSRC)icremap.h:$(GLSRC)gxtmap.h
$(PSSRC)icremap.h:$(GLSRC)gscms.h
$(PSSRC)icremap.h:$(GLSRC)gslparam.h
$(PSSRC)icremap.h:$(GLSRC)gsdevice.h
$(PSSRC)icremap.h:$(GLSRC)gxbitfmt.h
$(PSSRC)icremap.h:$(GLSRC)gscpm.h
$(PSSRC)icremap.h:$(GLSRC)gscspace.h
$(PSSRC)icremap.h:$(GLSRC)gsgstate.h
$(PSSRC)icremap.h:$(GLSRC)gsiparam.h
$(PSSRC)icremap.h:$(GLSRC)gscompt.h
$(PSSRC)icremap.h:$(GLSRC)gsmatrix.h
$(PSSRC)icremap.h:$(GLSRC)gxhttile.h
$(PSSRC)icremap.h:$(GLSRC)gsparam.h
$(PSSRC)icremap.h:$(GLSRC)gsrefct.h
$(PSSRC)icremap.h:$(GLSRC)memento.h
$(PSSRC)icremap.h:$(GLSRC)gsstruct.h
$(PSSRC)icremap.h:$(GLSRC)gxsync.h
$(PSSRC)icremap.h:$(GLSRC)gxbitmap.h
$(PSSRC)icremap.h:$(GLSRC)scommon.h
$(PSSRC)icremap.h:$(GLSRC)gsbitmap.h
$(PSSRC)icremap.h:$(GLSRC)gsccolor.h
$(PSSRC)icremap.h:$(GLSRC)gxarith.h
$(PSSRC)icremap.h:$(GLSRC)gpsync.h
$(PSSRC)icremap.h:$(GLSRC)gsstype.h
$(PSSRC)icremap.h:$(GLSRC)gsmemory.h
$(PSSRC)icremap.h:$(GLSRC)gslibctx.h
$(PSSRC)icremap.h:$(GLSRC)gsalloc.h
$(PSSRC)icremap.h:$(GLSRC)gxcindex.h
$(PSSRC)icremap.h:$(GLSRC)stdio_.h
$(PSSRC)icremap.h:$(GLSRC)stdint_.h
$(PSSRC)icremap.h:$(GLSRC)gssprintf.h
$(PSSRC)icremap.h:$(GLSRC)gstypes.h
$(PSSRC)icremap.h:$(GLSRC)std.h
$(PSSRC)icremap.h:$(GLSRC)stdpre.h
$(PSSRC)icremap.h:$(GLGEN)arch.h
$(PSSRC)icremap.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icsmap.h:$(PSSRC)iref.h
$(PSSRC)icsmap.h:$(GLSRC)gxalloc.h
$(PSSRC)icsmap.h:$(GLSRC)gxobj.h
$(PSSRC)icsmap.h:$(GLSRC)gsnamecl.h
$(PSSRC)icsmap.h:$(GLSRC)gxcspace.h
$(PSSRC)icsmap.h:$(GLSRC)gscsel.h
$(PSSRC)icsmap.h:$(GLSRC)gsdcolor.h
$(PSSRC)icsmap.h:$(GLSRC)gxfrac.h
$(PSSRC)icsmap.h:$(GLSRC)gscms.h
$(PSSRC)icsmap.h:$(GLSRC)gsdevice.h
$(PSSRC)icsmap.h:$(GLSRC)gscspace.h
$(PSSRC)icsmap.h:$(GLSRC)gsgstate.h
$(PSSRC)icsmap.h:$(GLSRC)gsiparam.h
$(PSSRC)icsmap.h:$(GLSRC)gsmatrix.h
$(PSSRC)icsmap.h:$(GLSRC)gxhttile.h
$(PSSRC)icsmap.h:$(GLSRC)gsparam.h
$(PSSRC)icsmap.h:$(GLSRC)gsrefct.h
$(PSSRC)icsmap.h:$(GLSRC)memento.h
$(PSSRC)icsmap.h:$(GLSRC)gsstruct.h
$(PSSRC)icsmap.h:$(GLSRC)gxsync.h
$(PSSRC)icsmap.h:$(GLSRC)gxbitmap.h
$(PSSRC)icsmap.h:$(GLSRC)scommon.h
$(PSSRC)icsmap.h:$(GLSRC)gsbitmap.h
$(PSSRC)icsmap.h:$(GLSRC)gsccolor.h
$(PSSRC)icsmap.h:$(GLSRC)gxarith.h
$(PSSRC)icsmap.h:$(GLSRC)gpsync.h
$(PSSRC)icsmap.h:$(GLSRC)gsstype.h
$(PSSRC)icsmap.h:$(GLSRC)gsmemory.h
$(PSSRC)icsmap.h:$(GLSRC)gslibctx.h
$(PSSRC)icsmap.h:$(GLSRC)gsalloc.h
$(PSSRC)icsmap.h:$(GLSRC)gxcindex.h
$(PSSRC)icsmap.h:$(GLSRC)stdio_.h
$(PSSRC)icsmap.h:$(GLSRC)stdint_.h
$(PSSRC)icsmap.h:$(GLSRC)gssprintf.h
$(PSSRC)icsmap.h:$(GLSRC)gstypes.h
$(PSSRC)icsmap.h:$(GLSRC)std.h
$(PSSRC)icsmap.h:$(GLSRC)stdpre.h
$(PSSRC)icsmap.h:$(GLGEN)arch.h
$(PSSRC)icsmap.h:$(GLSRC)gs_dll_call.h
$(PSSRC)idisp.h:$(PSSRC)imain.h
$(PSSRC)idisp.h:$(GLSRC)gsexit.h
$(PSSRC)idisp.h:$(PSSRC)iref.h
$(PSSRC)idisp.h:$(GLSRC)gxalloc.h
$(PSSRC)idisp.h:$(GLSRC)gxobj.h
$(PSSRC)idisp.h:$(GLSRC)gsnamecl.h
$(PSSRC)idisp.h:$(GLSRC)gxcspace.h
$(PSSRC)idisp.h:$(GLSRC)gscsel.h
$(PSSRC)idisp.h:$(GLSRC)gsdcolor.h
$(PSSRC)idisp.h:$(GLSRC)gxfrac.h
$(PSSRC)idisp.h:$(GLSRC)gscms.h
$(PSSRC)idisp.h:$(GLSRC)gsdevice.h
$(PSSRC)idisp.h:$(GLSRC)gscspace.h
$(PSSRC)idisp.h:$(GLSRC)gsgstate.h
$(PSSRC)idisp.h:$(GLSRC)gsiparam.h
$(PSSRC)idisp.h:$(GLSRC)gsmatrix.h
$(PSSRC)idisp.h:$(GLSRC)gxhttile.h
$(PSSRC)idisp.h:$(GLSRC)gsparam.h
$(PSSRC)idisp.h:$(GLSRC)gsrefct.h
$(PSSRC)idisp.h:$(GLSRC)memento.h
$(PSSRC)idisp.h:$(GLSRC)gsstruct.h
$(PSSRC)idisp.h:$(GLSRC)gxsync.h
$(PSSRC)idisp.h:$(GLSRC)gxbitmap.h
$(PSSRC)idisp.h:$(GLSRC)scommon.h
$(PSSRC)idisp.h:$(GLSRC)gsbitmap.h
$(PSSRC)idisp.h:$(GLSRC)gsccolor.h
$(PSSRC)idisp.h:$(GLSRC)gxarith.h
$(PSSRC)idisp.h:$(GLSRC)gpsync.h
$(PSSRC)idisp.h:$(GLSRC)gsstype.h
$(PSSRC)idisp.h:$(GLSRC)gsmemory.h
$(PSSRC)idisp.h:$(GLSRC)gslibctx.h
$(PSSRC)idisp.h:$(GLSRC)gsalloc.h
$(PSSRC)idisp.h:$(GLSRC)gxcindex.h
$(PSSRC)idisp.h:$(GLSRC)stdio_.h
$(PSSRC)idisp.h:$(GLSRC)stdint_.h
$(PSSRC)idisp.h:$(GLSRC)gssprintf.h
$(PSSRC)idisp.h:$(GLSRC)gstypes.h
$(PSSRC)idisp.h:$(GLSRC)std.h
$(PSSRC)idisp.h:$(GLSRC)stdpre.h
$(PSSRC)idisp.h:$(GLGEN)arch.h
$(PSSRC)idisp.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifilter2.h:$(GLSRC)scfx.h
$(PSSRC)ifilter2.h:$(GLSRC)slzwx.h
$(PSSRC)ifilter2.h:$(GLSRC)spdiffx.h
$(PSSRC)ifilter2.h:$(GLSRC)spngpx.h
$(PSSRC)ifilter2.h:$(PSSRC)iostack.h
$(PSSRC)ifilter2.h:$(PSSRC)istack.h
$(PSSRC)ifilter2.h:$(GLSRC)shc.h
$(PSSRC)ifilter2.h:$(PSSRC)iosdata.h
$(PSSRC)ifilter2.h:$(PSSRC)isdata.h
$(PSSRC)ifilter2.h:$(PSSRC)imemory.h
$(PSSRC)ifilter2.h:$(PSSRC)ivmspace.h
$(PSSRC)ifilter2.h:$(PSSRC)iref.h
$(PSSRC)ifilter2.h:$(GLSRC)gsgc.h
$(PSSRC)ifilter2.h:$(GLSRC)gxalloc.h
$(PSSRC)ifilter2.h:$(GLSRC)gxobj.h
$(PSSRC)ifilter2.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifilter2.h:$(GLSRC)gxcspace.h
$(PSSRC)ifilter2.h:$(GLSRC)gscsel.h
$(PSSRC)ifilter2.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifilter2.h:$(GLSRC)gxfrac.h
$(PSSRC)ifilter2.h:$(GLSRC)gscms.h
$(PSSRC)ifilter2.h:$(GLSRC)gsdevice.h
$(PSSRC)ifilter2.h:$(GLSRC)gscspace.h
$(PSSRC)ifilter2.h:$(GLSRC)gsgstate.h
$(PSSRC)ifilter2.h:$(GLSRC)gsiparam.h
$(PSSRC)ifilter2.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifilter2.h:$(GLSRC)gxhttile.h
$(PSSRC)ifilter2.h:$(GLSRC)gsparam.h
$(PSSRC)ifilter2.h:$(GLSRC)gsrefct.h
$(PSSRC)ifilter2.h:$(GLSRC)memento.h
$(PSSRC)ifilter2.h:$(GLSRC)gsstruct.h
$(PSSRC)ifilter2.h:$(GLSRC)gxsync.h
$(PSSRC)ifilter2.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifilter2.h:$(GLSRC)scommon.h
$(PSSRC)ifilter2.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifilter2.h:$(GLSRC)gsccolor.h
$(PSSRC)ifilter2.h:$(GLSRC)gxarith.h
$(PSSRC)ifilter2.h:$(GLSRC)gpsync.h
$(PSSRC)ifilter2.h:$(GLSRC)gsstype.h
$(PSSRC)ifilter2.h:$(GLSRC)gsmemory.h
$(PSSRC)ifilter2.h:$(GLSRC)gslibctx.h
$(PSSRC)ifilter2.h:$(GLSRC)gsalloc.h
$(PSSRC)ifilter2.h:$(GLSRC)gxcindex.h
$(PSSRC)ifilter2.h:$(GLSRC)stdio_.h
$(PSSRC)ifilter2.h:$(GLSRC)stdint_.h
$(PSSRC)ifilter2.h:$(GLSRC)gssprintf.h
$(PSSRC)ifilter2.h:$(GLSRC)gsbittab.h
$(PSSRC)ifilter2.h:$(GLSRC)gstypes.h
$(PSSRC)ifilter2.h:$(GLSRC)std.h
$(PSSRC)ifilter2.h:$(GLSRC)stdpre.h
$(PSSRC)ifilter2.h:$(GLGEN)arch.h
$(PSSRC)ifilter2.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifont.h:$(GLSRC)gxfont.h
$(PSSRC)ifont.h:$(GLSRC)gspath.h
$(PSSRC)ifont.h:$(GLSRC)gsgdata.h
$(PSSRC)ifont.h:$(GLSRC)gxmatrix.h
$(PSSRC)ifont.h:$(GLSRC)gxfapi.h
$(PSSRC)ifont.h:$(GLSRC)gsfcmap.h
$(PSSRC)ifont.h:$(PSSRC)iref.h
$(PSSRC)ifont.h:$(GLSRC)gxalloc.h
$(PSSRC)ifont.h:$(GLSRC)gxobj.h
$(PSSRC)ifont.h:$(GLSRC)gstext.h
$(PSSRC)ifont.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifont.h:$(GLSRC)gxcspace.h
$(PSSRC)ifont.h:$(GLSRC)gscsel.h
$(PSSRC)ifont.h:$(GLSRC)gsfont.h
$(PSSRC)ifont.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifont.h:$(GLSRC)gxpath.h
$(PSSRC)ifont.h:$(GLSRC)gxfrac.h
$(PSSRC)ifont.h:$(GLSRC)gxftype.h
$(PSSRC)ifont.h:$(GLSRC)gscms.h
$(PSSRC)ifont.h:$(GLSRC)gsrect.h
$(PSSRC)ifont.h:$(GLSRC)gslparam.h
$(PSSRC)ifont.h:$(GLSRC)gsdevice.h
$(PSSRC)ifont.h:$(GLSRC)gscpm.h
$(PSSRC)ifont.h:$(GLSRC)gsgcache.h
$(PSSRC)ifont.h:$(GLSRC)gscspace.h
$(PSSRC)ifont.h:$(GLSRC)gsgstate.h
$(PSSRC)ifont.h:$(GLSRC)gsnotify.h
$(PSSRC)ifont.h:$(GLSRC)gsiparam.h
$(PSSRC)ifont.h:$(GLSRC)gxfixed.h
$(PSSRC)ifont.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifont.h:$(GLSRC)gspenum.h
$(PSSRC)ifont.h:$(GLSRC)gxhttile.h
$(PSSRC)ifont.h:$(GLSRC)gsparam.h
$(PSSRC)ifont.h:$(GLSRC)gsrefct.h
$(PSSRC)ifont.h:$(GLSRC)memento.h
$(PSSRC)ifont.h:$(GLSRC)gsuid.h
$(PSSRC)ifont.h:$(GLSRC)gsstruct.h
$(PSSRC)ifont.h:$(GLSRC)gxsync.h
$(PSSRC)ifont.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifont.h:$(GLSRC)scommon.h
$(PSSRC)ifont.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifont.h:$(GLSRC)gsccolor.h
$(PSSRC)ifont.h:$(GLSRC)gxarith.h
$(PSSRC)ifont.h:$(GLSRC)gpsync.h
$(PSSRC)ifont.h:$(GLSRC)gsstype.h
$(PSSRC)ifont.h:$(GLSRC)gsmemory.h
$(PSSRC)ifont.h:$(GLSRC)gslibctx.h
$(PSSRC)ifont.h:$(GLSRC)gsalloc.h
$(PSSRC)ifont.h:$(GLSRC)gxcindex.h
$(PSSRC)ifont.h:$(GLSRC)stdio_.h
$(PSSRC)ifont.h:$(GLSRC)gsccode.h
$(PSSRC)ifont.h:$(GLSRC)stdint_.h
$(PSSRC)ifont.h:$(GLSRC)gssprintf.h
$(PSSRC)ifont.h:$(GLSRC)gstypes.h
$(PSSRC)ifont.h:$(GLSRC)std.h
$(PSSRC)ifont.h:$(GLSRC)stdpre.h
$(PSSRC)ifont.h:$(GLGEN)arch.h
$(PSSRC)ifont.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifont1.h:$(PSSRC)ichar1.h
$(PSSRC)ifont1.h:$(GLSRC)gxfont1.h
$(PSSRC)ifont1.h:$(PSSRC)bfont.h
$(PSSRC)ifont1.h:$(PSSRC)ifont.h
$(PSSRC)ifont1.h:$(GLSRC)gstype1.h
$(PSSRC)ifont1.h:$(GLSRC)gxfont.h
$(PSSRC)ifont1.h:$(PSSRC)iostack.h
$(PSSRC)ifont1.h:$(GLSRC)gspath.h
$(PSSRC)ifont1.h:$(PSSRC)istack.h
$(PSSRC)ifont1.h:$(GLSRC)gsgdata.h
$(PSSRC)ifont1.h:$(PSSRC)iosdata.h
$(PSSRC)ifont1.h:$(GLSRC)gxmatrix.h
$(PSSRC)ifont1.h:$(GLSRC)gxfapi.h
$(PSSRC)ifont1.h:$(PSSRC)isdata.h
$(PSSRC)ifont1.h:$(PSSRC)imemory.h
$(PSSRC)ifont1.h:$(GLSRC)gsfcmap.h
$(PSSRC)ifont1.h:$(PSSRC)ivmspace.h
$(PSSRC)ifont1.h:$(PSSRC)iref.h
$(PSSRC)ifont1.h:$(GLSRC)gsgc.h
$(PSSRC)ifont1.h:$(GLSRC)gxalloc.h
$(PSSRC)ifont1.h:$(GLSRC)gxobj.h
$(PSSRC)ifont1.h:$(GLSRC)gstext.h
$(PSSRC)ifont1.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifont1.h:$(GLSRC)gxcspace.h
$(PSSRC)ifont1.h:$(GLSRC)gscsel.h
$(PSSRC)ifont1.h:$(GLSRC)gsfont.h
$(PSSRC)ifont1.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifont1.h:$(GLSRC)gxpath.h
$(PSSRC)ifont1.h:$(GLSRC)gxfrac.h
$(PSSRC)ifont1.h:$(GLSRC)gxftype.h
$(PSSRC)ifont1.h:$(GLSRC)gscms.h
$(PSSRC)ifont1.h:$(GLSRC)gsrect.h
$(PSSRC)ifont1.h:$(GLSRC)gslparam.h
$(PSSRC)ifont1.h:$(GLSRC)gsdevice.h
$(PSSRC)ifont1.h:$(GLSRC)gscpm.h
$(PSSRC)ifont1.h:$(GLSRC)gsgcache.h
$(PSSRC)ifont1.h:$(GLSRC)gscspace.h
$(PSSRC)ifont1.h:$(GLSRC)gsgstate.h
$(PSSRC)ifont1.h:$(GLSRC)gsnotify.h
$(PSSRC)ifont1.h:$(GLSRC)gsiparam.h
$(PSSRC)ifont1.h:$(GLSRC)gxfixed.h
$(PSSRC)ifont1.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifont1.h:$(GLSRC)gspenum.h
$(PSSRC)ifont1.h:$(GLSRC)gxhttile.h
$(PSSRC)ifont1.h:$(GLSRC)gsparam.h
$(PSSRC)ifont1.h:$(GLSRC)gsrefct.h
$(PSSRC)ifont1.h:$(GLSRC)memento.h
$(PSSRC)ifont1.h:$(GLSRC)gsuid.h
$(PSSRC)ifont1.h:$(GLSRC)gsstruct.h
$(PSSRC)ifont1.h:$(GLSRC)gxsync.h
$(PSSRC)ifont1.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifont1.h:$(GLSRC)scommon.h
$(PSSRC)ifont1.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifont1.h:$(GLSRC)gsccolor.h
$(PSSRC)ifont1.h:$(GLSRC)gxarith.h
$(PSSRC)ifont1.h:$(GLSRC)gpsync.h
$(PSSRC)ifont1.h:$(GLSRC)gsstype.h
$(PSSRC)ifont1.h:$(GLSRC)gsmemory.h
$(PSSRC)ifont1.h:$(GLSRC)gslibctx.h
$(PSSRC)ifont1.h:$(GLSRC)gsalloc.h
$(PSSRC)ifont1.h:$(GLSRC)gxcindex.h
$(PSSRC)ifont1.h:$(GLSRC)stdio_.h
$(PSSRC)ifont1.h:$(GLSRC)gsccode.h
$(PSSRC)ifont1.h:$(GLSRC)stdint_.h
$(PSSRC)ifont1.h:$(GLSRC)gssprintf.h
$(PSSRC)ifont1.h:$(GLSRC)gstypes.h
$(PSSRC)ifont1.h:$(GLSRC)std.h
$(PSSRC)ifont1.h:$(GLSRC)stdpre.h
$(PSSRC)ifont1.h:$(GLGEN)arch.h
$(PSSRC)ifont1.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifont2.h:$(PSSRC)ifont1.h
$(PSSRC)ifont2.h:$(PSSRC)ichar1.h
$(PSSRC)ifont2.h:$(GLSRC)gxfont1.h
$(PSSRC)ifont2.h:$(PSSRC)bfont.h
$(PSSRC)ifont2.h:$(PSSRC)ifont.h
$(PSSRC)ifont2.h:$(GLSRC)gstype1.h
$(PSSRC)ifont2.h:$(GLSRC)gxfont.h
$(PSSRC)ifont2.h:$(PSSRC)iostack.h
$(PSSRC)ifont2.h:$(GLSRC)gspath.h
$(PSSRC)ifont2.h:$(PSSRC)istack.h
$(PSSRC)ifont2.h:$(GLSRC)gsgdata.h
$(PSSRC)ifont2.h:$(PSSRC)iosdata.h
$(PSSRC)ifont2.h:$(GLSRC)gxmatrix.h
$(PSSRC)ifont2.h:$(GLSRC)gxfapi.h
$(PSSRC)ifont2.h:$(PSSRC)isdata.h
$(PSSRC)ifont2.h:$(PSSRC)imemory.h
$(PSSRC)ifont2.h:$(GLSRC)gsfcmap.h
$(PSSRC)ifont2.h:$(PSSRC)ivmspace.h
$(PSSRC)ifont2.h:$(PSSRC)iref.h
$(PSSRC)ifont2.h:$(GLSRC)gsgc.h
$(PSSRC)ifont2.h:$(GLSRC)gxalloc.h
$(PSSRC)ifont2.h:$(GLSRC)gxobj.h
$(PSSRC)ifont2.h:$(GLSRC)gstext.h
$(PSSRC)ifont2.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifont2.h:$(GLSRC)gxcspace.h
$(PSSRC)ifont2.h:$(GLSRC)gscsel.h
$(PSSRC)ifont2.h:$(GLSRC)gsfont.h
$(PSSRC)ifont2.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifont2.h:$(GLSRC)gxpath.h
$(PSSRC)ifont2.h:$(GLSRC)gxfrac.h
$(PSSRC)ifont2.h:$(GLSRC)gxftype.h
$(PSSRC)ifont2.h:$(GLSRC)gscms.h
$(PSSRC)ifont2.h:$(GLSRC)gsrect.h
$(PSSRC)ifont2.h:$(GLSRC)gslparam.h
$(PSSRC)ifont2.h:$(GLSRC)gsdevice.h
$(PSSRC)ifont2.h:$(GLSRC)gscpm.h
$(PSSRC)ifont2.h:$(GLSRC)gsgcache.h
$(PSSRC)ifont2.h:$(GLSRC)gscspace.h
$(PSSRC)ifont2.h:$(GLSRC)gsgstate.h
$(PSSRC)ifont2.h:$(GLSRC)gsnotify.h
$(PSSRC)ifont2.h:$(GLSRC)gsiparam.h
$(PSSRC)ifont2.h:$(GLSRC)gxfixed.h
$(PSSRC)ifont2.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifont2.h:$(GLSRC)gspenum.h
$(PSSRC)ifont2.h:$(GLSRC)gxhttile.h
$(PSSRC)ifont2.h:$(GLSRC)gsparam.h
$(PSSRC)ifont2.h:$(GLSRC)gsrefct.h
$(PSSRC)ifont2.h:$(GLSRC)memento.h
$(PSSRC)ifont2.h:$(GLSRC)gsuid.h
$(PSSRC)ifont2.h:$(GLSRC)gsstruct.h
$(PSSRC)ifont2.h:$(GLSRC)gxsync.h
$(PSSRC)ifont2.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifont2.h:$(GLSRC)scommon.h
$(PSSRC)ifont2.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifont2.h:$(GLSRC)gsccolor.h
$(PSSRC)ifont2.h:$(GLSRC)gxarith.h
$(PSSRC)ifont2.h:$(GLSRC)gpsync.h
$(PSSRC)ifont2.h:$(GLSRC)gsstype.h
$(PSSRC)ifont2.h:$(GLSRC)gsmemory.h
$(PSSRC)ifont2.h:$(GLSRC)gslibctx.h
$(PSSRC)ifont2.h:$(GLSRC)gsalloc.h
$(PSSRC)ifont2.h:$(GLSRC)gxcindex.h
$(PSSRC)ifont2.h:$(GLSRC)stdio_.h
$(PSSRC)ifont2.h:$(GLSRC)gsccode.h
$(PSSRC)ifont2.h:$(GLSRC)stdint_.h
$(PSSRC)ifont2.h:$(GLSRC)gssprintf.h
$(PSSRC)ifont2.h:$(GLSRC)gstypes.h
$(PSSRC)ifont2.h:$(GLSRC)std.h
$(PSSRC)ifont2.h:$(GLSRC)stdpre.h
$(PSSRC)ifont2.h:$(GLGEN)arch.h
$(PSSRC)ifont2.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifont42.h:$(PSSRC)bfont.h
$(PSSRC)ifont42.h:$(PSSRC)ifont.h
$(PSSRC)ifont42.h:$(GLSRC)gxfont42.h
$(PSSRC)ifont42.h:$(GLSRC)gxfont.h
$(PSSRC)ifont42.h:$(PSSRC)iostack.h
$(PSSRC)ifont42.h:$(GLSRC)gspath.h
$(PSSRC)ifont42.h:$(PSSRC)istack.h
$(PSSRC)ifont42.h:$(GLSRC)gsgdata.h
$(PSSRC)ifont42.h:$(PSSRC)iosdata.h
$(PSSRC)ifont42.h:$(GLSRC)gxmatrix.h
$(PSSRC)ifont42.h:$(GLSRC)gxfapi.h
$(PSSRC)ifont42.h:$(PSSRC)isdata.h
$(PSSRC)ifont42.h:$(PSSRC)imemory.h
$(PSSRC)ifont42.h:$(GLSRC)gsfcmap.h
$(PSSRC)ifont42.h:$(PSSRC)ivmspace.h
$(PSSRC)ifont42.h:$(PSSRC)iref.h
$(PSSRC)ifont42.h:$(GLSRC)gsgc.h
$(PSSRC)ifont42.h:$(GLSRC)gxalloc.h
$(PSSRC)ifont42.h:$(GLSRC)gxobj.h
$(PSSRC)ifont42.h:$(GLSRC)gstext.h
$(PSSRC)ifont42.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifont42.h:$(GLSRC)gxcspace.h
$(PSSRC)ifont42.h:$(GLSRC)gscsel.h
$(PSSRC)ifont42.h:$(GLSRC)gxfcache.h
$(PSSRC)ifont42.h:$(GLSRC)gsfont.h
$(PSSRC)ifont42.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifont42.h:$(GLSRC)gxbcache.h
$(PSSRC)ifont42.h:$(GLSRC)gxpath.h
$(PSSRC)ifont42.h:$(GLSRC)gxfrac.h
$(PSSRC)ifont42.h:$(GLSRC)gxftype.h
$(PSSRC)ifont42.h:$(GLSRC)gscms.h
$(PSSRC)ifont42.h:$(GLSRC)gsrect.h
$(PSSRC)ifont42.h:$(GLSRC)gslparam.h
$(PSSRC)ifont42.h:$(GLSRC)gsdevice.h
$(PSSRC)ifont42.h:$(GLSRC)gscpm.h
$(PSSRC)ifont42.h:$(GLSRC)gsgcache.h
$(PSSRC)ifont42.h:$(GLSRC)gscspace.h
$(PSSRC)ifont42.h:$(GLSRC)gsgstate.h
$(PSSRC)ifont42.h:$(GLSRC)gsnotify.h
$(PSSRC)ifont42.h:$(GLSRC)gsxfont.h
$(PSSRC)ifont42.h:$(GLSRC)gsiparam.h
$(PSSRC)ifont42.h:$(GLSRC)gxfixed.h
$(PSSRC)ifont42.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifont42.h:$(GLSRC)gspenum.h
$(PSSRC)ifont42.h:$(GLSRC)gxhttile.h
$(PSSRC)ifont42.h:$(GLSRC)gsparam.h
$(PSSRC)ifont42.h:$(GLSRC)gsrefct.h
$(PSSRC)ifont42.h:$(GLSRC)memento.h
$(PSSRC)ifont42.h:$(GLSRC)gsuid.h
$(PSSRC)ifont42.h:$(GLSRC)gsstruct.h
$(PSSRC)ifont42.h:$(GLSRC)gxsync.h
$(PSSRC)ifont42.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifont42.h:$(GLSRC)scommon.h
$(PSSRC)ifont42.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifont42.h:$(GLSRC)gsccolor.h
$(PSSRC)ifont42.h:$(GLSRC)gxarith.h
$(PSSRC)ifont42.h:$(GLSRC)gpsync.h
$(PSSRC)ifont42.h:$(GLSRC)gsstype.h
$(PSSRC)ifont42.h:$(GLSRC)gsmemory.h
$(PSSRC)ifont42.h:$(GLSRC)gslibctx.h
$(PSSRC)ifont42.h:$(GLSRC)gsalloc.h
$(PSSRC)ifont42.h:$(GLSRC)gxcindex.h
$(PSSRC)ifont42.h:$(GLSRC)stdio_.h
$(PSSRC)ifont42.h:$(GLSRC)gsccode.h
$(PSSRC)ifont42.h:$(GLSRC)stdint_.h
$(PSSRC)ifont42.h:$(GLSRC)gssprintf.h
$(PSSRC)ifont42.h:$(GLSRC)gstypes.h
$(PSSRC)ifont42.h:$(GLSRC)std.h
$(PSSRC)ifont42.h:$(GLSRC)stdpre.h
$(PSSRC)ifont42.h:$(GLGEN)arch.h
$(PSSRC)ifont42.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifrpred.h:$(PSSRC)iref.h
$(PSSRC)ifrpred.h:$(GLSRC)gxalloc.h
$(PSSRC)ifrpred.h:$(GLSRC)gxobj.h
$(PSSRC)ifrpred.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifrpred.h:$(GLSRC)gxcspace.h
$(PSSRC)ifrpred.h:$(GLSRC)gscsel.h
$(PSSRC)ifrpred.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifrpred.h:$(GLSRC)gxfrac.h
$(PSSRC)ifrpred.h:$(GLSRC)gscms.h
$(PSSRC)ifrpred.h:$(GLSRC)gsdevice.h
$(PSSRC)ifrpred.h:$(GLSRC)gscspace.h
$(PSSRC)ifrpred.h:$(GLSRC)gsgstate.h
$(PSSRC)ifrpred.h:$(GLSRC)gsiparam.h
$(PSSRC)ifrpred.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifrpred.h:$(GLSRC)gxhttile.h
$(PSSRC)ifrpred.h:$(GLSRC)gsparam.h
$(PSSRC)ifrpred.h:$(GLSRC)gsrefct.h
$(PSSRC)ifrpred.h:$(GLSRC)memento.h
$(PSSRC)ifrpred.h:$(GLSRC)gsstruct.h
$(PSSRC)ifrpred.h:$(GLSRC)gxsync.h
$(PSSRC)ifrpred.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifrpred.h:$(GLSRC)scommon.h
$(PSSRC)ifrpred.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifrpred.h:$(GLSRC)gsccolor.h
$(PSSRC)ifrpred.h:$(GLSRC)gxarith.h
$(PSSRC)ifrpred.h:$(GLSRC)gpsync.h
$(PSSRC)ifrpred.h:$(GLSRC)gsstype.h
$(PSSRC)ifrpred.h:$(GLSRC)gsmemory.h
$(PSSRC)ifrpred.h:$(GLSRC)gslibctx.h
$(PSSRC)ifrpred.h:$(GLSRC)gsalloc.h
$(PSSRC)ifrpred.h:$(GLSRC)gxcindex.h
$(PSSRC)ifrpred.h:$(GLSRC)stdio_.h
$(PSSRC)ifrpred.h:$(GLSRC)stdint_.h
$(PSSRC)ifrpred.h:$(GLSRC)gssprintf.h
$(PSSRC)ifrpred.h:$(GLSRC)gstypes.h
$(PSSRC)ifrpred.h:$(GLSRC)std.h
$(PSSRC)ifrpred.h:$(GLSRC)stdpre.h
$(PSSRC)ifrpred.h:$(GLGEN)arch.h
$(PSSRC)ifrpred.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifwpred.h:$(PSSRC)iref.h
$(PSSRC)ifwpred.h:$(GLSRC)gxalloc.h
$(PSSRC)ifwpred.h:$(GLSRC)gxobj.h
$(PSSRC)ifwpred.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifwpred.h:$(GLSRC)gxcspace.h
$(PSSRC)ifwpred.h:$(GLSRC)gscsel.h
$(PSSRC)ifwpred.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifwpred.h:$(GLSRC)gxfrac.h
$(PSSRC)ifwpred.h:$(GLSRC)gscms.h
$(PSSRC)ifwpred.h:$(GLSRC)gsdevice.h
$(PSSRC)ifwpred.h:$(GLSRC)gscspace.h
$(PSSRC)ifwpred.h:$(GLSRC)gsgstate.h
$(PSSRC)ifwpred.h:$(GLSRC)gsiparam.h
$(PSSRC)ifwpred.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifwpred.h:$(GLSRC)gxhttile.h
$(PSSRC)ifwpred.h:$(GLSRC)gsparam.h
$(PSSRC)ifwpred.h:$(GLSRC)gsrefct.h
$(PSSRC)ifwpred.h:$(GLSRC)memento.h
$(PSSRC)ifwpred.h:$(GLSRC)gsstruct.h
$(PSSRC)ifwpred.h:$(GLSRC)gxsync.h
$(PSSRC)ifwpred.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifwpred.h:$(GLSRC)scommon.h
$(PSSRC)ifwpred.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifwpred.h:$(GLSRC)gsccolor.h
$(PSSRC)ifwpred.h:$(GLSRC)gxarith.h
$(PSSRC)ifwpred.h:$(GLSRC)gpsync.h
$(PSSRC)ifwpred.h:$(GLSRC)gsstype.h
$(PSSRC)ifwpred.h:$(GLSRC)gsmemory.h
$(PSSRC)ifwpred.h:$(GLSRC)gslibctx.h
$(PSSRC)ifwpred.h:$(GLSRC)gsalloc.h
$(PSSRC)ifwpred.h:$(GLSRC)gxcindex.h
$(PSSRC)ifwpred.h:$(GLSRC)stdio_.h
$(PSSRC)ifwpred.h:$(GLSRC)stdint_.h
$(PSSRC)ifwpred.h:$(GLSRC)gssprintf.h
$(PSSRC)ifwpred.h:$(GLSRC)gstypes.h
$(PSSRC)ifwpred.h:$(GLSRC)std.h
$(PSSRC)ifwpred.h:$(GLSRC)stdpre.h
$(PSSRC)ifwpred.h:$(GLGEN)arch.h
$(PSSRC)ifwpred.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iht.h:$(GLSRC)gxht.h
$(PSSRC)iht.h:$(GLSRC)gxhttype.h
$(PSSRC)iht.h:$(GLSRC)gsht1.h
$(PSSRC)iht.h:$(PSSRC)iostack.h
$(PSSRC)iht.h:$(PSSRC)istack.h
$(PSSRC)iht.h:$(PSSRC)iosdata.h
$(PSSRC)iht.h:$(PSSRC)isdata.h
$(PSSRC)iht.h:$(PSSRC)imemory.h
$(PSSRC)iht.h:$(GLSRC)gsht.h
$(PSSRC)iht.h:$(PSSRC)ivmspace.h
$(PSSRC)iht.h:$(PSSRC)iref.h
$(PSSRC)iht.h:$(GLSRC)gsgc.h
$(PSSRC)iht.h:$(GLSRC)gxalloc.h
$(PSSRC)iht.h:$(GLSRC)gxobj.h
$(PSSRC)iht.h:$(GLSRC)gsnamecl.h
$(PSSRC)iht.h:$(GLSRC)gxcspace.h
$(PSSRC)iht.h:$(GLSRC)gscsel.h
$(PSSRC)iht.h:$(GLSRC)gsdcolor.h
$(PSSRC)iht.h:$(GLSRC)gxfrac.h
$(PSSRC)iht.h:$(GLSRC)gxtmap.h
$(PSSRC)iht.h:$(GLSRC)gscms.h
$(PSSRC)iht.h:$(GLSRC)gsdevice.h
$(PSSRC)iht.h:$(GLSRC)gscspace.h
$(PSSRC)iht.h:$(GLSRC)gsgstate.h
$(PSSRC)iht.h:$(GLSRC)gsiparam.h
$(PSSRC)iht.h:$(GLSRC)gsmatrix.h
$(PSSRC)iht.h:$(GLSRC)gxhttile.h
$(PSSRC)iht.h:$(GLSRC)gsparam.h
$(PSSRC)iht.h:$(GLSRC)gsrefct.h
$(PSSRC)iht.h:$(GLSRC)memento.h
$(PSSRC)iht.h:$(GLSRC)gsstruct.h
$(PSSRC)iht.h:$(GLSRC)gxsync.h
$(PSSRC)iht.h:$(GLSRC)gxbitmap.h
$(PSSRC)iht.h:$(GLSRC)scommon.h
$(PSSRC)iht.h:$(GLSRC)gsbitmap.h
$(PSSRC)iht.h:$(GLSRC)gsccolor.h
$(PSSRC)iht.h:$(GLSRC)gxarith.h
$(PSSRC)iht.h:$(GLSRC)gpsync.h
$(PSSRC)iht.h:$(GLSRC)gsstype.h
$(PSSRC)iht.h:$(GLSRC)gsmemory.h
$(PSSRC)iht.h:$(GLSRC)gslibctx.h
$(PSSRC)iht.h:$(GLSRC)gsalloc.h
$(PSSRC)iht.h:$(GLSRC)gxcindex.h
$(PSSRC)iht.h:$(GLSRC)stdio_.h
$(PSSRC)iht.h:$(GLSRC)stdint_.h
$(PSSRC)iht.h:$(GLSRC)gssprintf.h
$(PSSRC)iht.h:$(GLSRC)gstypes.h
$(PSSRC)iht.h:$(GLSRC)std.h
$(PSSRC)iht.h:$(GLSRC)stdpre.h
$(PSSRC)iht.h:$(GLGEN)arch.h
$(PSSRC)iht.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iimage.h:$(PSSRC)iref.h
$(PSSRC)iimage.h:$(GLSRC)gxalloc.h
$(PSSRC)iimage.h:$(GLSRC)gxobj.h
$(PSSRC)iimage.h:$(GLSRC)gsnamecl.h
$(PSSRC)iimage.h:$(GLSRC)gxcspace.h
$(PSSRC)iimage.h:$(GLSRC)gscsel.h
$(PSSRC)iimage.h:$(GLSRC)gsdcolor.h
$(PSSRC)iimage.h:$(GLSRC)gxfrac.h
$(PSSRC)iimage.h:$(GLSRC)gscms.h
$(PSSRC)iimage.h:$(GLSRC)gsdevice.h
$(PSSRC)iimage.h:$(GLSRC)gscspace.h
$(PSSRC)iimage.h:$(GLSRC)gsgstate.h
$(PSSRC)iimage.h:$(GLSRC)gsiparam.h
$(PSSRC)iimage.h:$(GLSRC)gsmatrix.h
$(PSSRC)iimage.h:$(GLSRC)gxhttile.h
$(PSSRC)iimage.h:$(GLSRC)gsparam.h
$(PSSRC)iimage.h:$(GLSRC)gsrefct.h
$(PSSRC)iimage.h:$(GLSRC)memento.h
$(PSSRC)iimage.h:$(GLSRC)gsstruct.h
$(PSSRC)iimage.h:$(GLSRC)gxsync.h
$(PSSRC)iimage.h:$(GLSRC)gxbitmap.h
$(PSSRC)iimage.h:$(GLSRC)scommon.h
$(PSSRC)iimage.h:$(GLSRC)gsbitmap.h
$(PSSRC)iimage.h:$(GLSRC)gsccolor.h
$(PSSRC)iimage.h:$(GLSRC)gxarith.h
$(PSSRC)iimage.h:$(GLSRC)gpsync.h
$(PSSRC)iimage.h:$(GLSRC)gsstype.h
$(PSSRC)iimage.h:$(GLSRC)gsmemory.h
$(PSSRC)iimage.h:$(GLSRC)gslibctx.h
$(PSSRC)iimage.h:$(GLSRC)gsalloc.h
$(PSSRC)iimage.h:$(GLSRC)gxcindex.h
$(PSSRC)iimage.h:$(GLSRC)stdio_.h
$(PSSRC)iimage.h:$(GLSRC)stdint_.h
$(PSSRC)iimage.h:$(GLSRC)gssprintf.h
$(PSSRC)iimage.h:$(GLSRC)gstypes.h
$(PSSRC)iimage.h:$(GLSRC)std.h
$(PSSRC)iimage.h:$(GLSRC)stdpre.h
$(PSSRC)iimage.h:$(GLGEN)arch.h
$(PSSRC)iimage.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iinit.h:$(PSSRC)imemory.h
$(PSSRC)iinit.h:$(PSSRC)ivmspace.h
$(PSSRC)iinit.h:$(PSSRC)iref.h
$(PSSRC)iinit.h:$(GLSRC)gsgc.h
$(PSSRC)iinit.h:$(GLSRC)gxalloc.h
$(PSSRC)iinit.h:$(GLSRC)gxobj.h
$(PSSRC)iinit.h:$(GLSRC)gsnamecl.h
$(PSSRC)iinit.h:$(GLSRC)gxcspace.h
$(PSSRC)iinit.h:$(GLSRC)gscsel.h
$(PSSRC)iinit.h:$(GLSRC)gsdcolor.h
$(PSSRC)iinit.h:$(GLSRC)gxfrac.h
$(PSSRC)iinit.h:$(GLSRC)gscms.h
$(PSSRC)iinit.h:$(GLSRC)gsdevice.h
$(PSSRC)iinit.h:$(GLSRC)gscspace.h
$(PSSRC)iinit.h:$(GLSRC)gsgstate.h
$(PSSRC)iinit.h:$(GLSRC)gsiparam.h
$(PSSRC)iinit.h:$(GLSRC)gsmatrix.h
$(PSSRC)iinit.h:$(GLSRC)gxhttile.h
$(PSSRC)iinit.h:$(GLSRC)gsparam.h
$(PSSRC)iinit.h:$(GLSRC)gsrefct.h
$(PSSRC)iinit.h:$(GLSRC)memento.h
$(PSSRC)iinit.h:$(GLSRC)gsstruct.h
$(PSSRC)iinit.h:$(GLSRC)gxsync.h
$(PSSRC)iinit.h:$(GLSRC)gxbitmap.h
$(PSSRC)iinit.h:$(GLSRC)scommon.h
$(PSSRC)iinit.h:$(GLSRC)gsbitmap.h
$(PSSRC)iinit.h:$(GLSRC)gsccolor.h
$(PSSRC)iinit.h:$(GLSRC)gxarith.h
$(PSSRC)iinit.h:$(GLSRC)gpsync.h
$(PSSRC)iinit.h:$(GLSRC)gsstype.h
$(PSSRC)iinit.h:$(GLSRC)gsmemory.h
$(PSSRC)iinit.h:$(GLSRC)gslibctx.h
$(PSSRC)iinit.h:$(GLSRC)gsalloc.h
$(PSSRC)iinit.h:$(GLSRC)gxcindex.h
$(PSSRC)iinit.h:$(GLSRC)stdio_.h
$(PSSRC)iinit.h:$(GLSRC)stdint_.h
$(PSSRC)iinit.h:$(GLSRC)gssprintf.h
$(PSSRC)iinit.h:$(GLSRC)gstypes.h
$(PSSRC)iinit.h:$(GLSRC)std.h
$(PSSRC)iinit.h:$(GLSRC)stdpre.h
$(PSSRC)iinit.h:$(GLGEN)arch.h
$(PSSRC)iinit.h:$(GLSRC)gs_dll_call.h
$(PSSRC)imain.h:$(GLSRC)gsexit.h
$(PSSRC)imain.h:$(PSSRC)iref.h
$(PSSRC)imain.h:$(GLSRC)gxalloc.h
$(PSSRC)imain.h:$(GLSRC)gxobj.h
$(PSSRC)imain.h:$(GLSRC)gsnamecl.h
$(PSSRC)imain.h:$(GLSRC)gxcspace.h
$(PSSRC)imain.h:$(GLSRC)gscsel.h
$(PSSRC)imain.h:$(GLSRC)gsdcolor.h
$(PSSRC)imain.h:$(GLSRC)gxfrac.h
$(PSSRC)imain.h:$(GLSRC)gscms.h
$(PSSRC)imain.h:$(GLSRC)gsdevice.h
$(PSSRC)imain.h:$(GLSRC)gscspace.h
$(PSSRC)imain.h:$(GLSRC)gsgstate.h
$(PSSRC)imain.h:$(GLSRC)gsiparam.h
$(PSSRC)imain.h:$(GLSRC)gsmatrix.h
$(PSSRC)imain.h:$(GLSRC)gxhttile.h
$(PSSRC)imain.h:$(GLSRC)gsparam.h
$(PSSRC)imain.h:$(GLSRC)gsrefct.h
$(PSSRC)imain.h:$(GLSRC)memento.h
$(PSSRC)imain.h:$(GLSRC)gsstruct.h
$(PSSRC)imain.h:$(GLSRC)gxsync.h
$(PSSRC)imain.h:$(GLSRC)gxbitmap.h
$(PSSRC)imain.h:$(GLSRC)scommon.h
$(PSSRC)imain.h:$(GLSRC)gsbitmap.h
$(PSSRC)imain.h:$(GLSRC)gsccolor.h
$(PSSRC)imain.h:$(GLSRC)gxarith.h
$(PSSRC)imain.h:$(GLSRC)gpsync.h
$(PSSRC)imain.h:$(GLSRC)gsstype.h
$(PSSRC)imain.h:$(GLSRC)gsmemory.h
$(PSSRC)imain.h:$(GLSRC)gslibctx.h
$(PSSRC)imain.h:$(GLSRC)gsalloc.h
$(PSSRC)imain.h:$(GLSRC)gxcindex.h
$(PSSRC)imain.h:$(GLSRC)stdio_.h
$(PSSRC)imain.h:$(GLSRC)stdint_.h
$(PSSRC)imain.h:$(GLSRC)gssprintf.h
$(PSSRC)imain.h:$(GLSRC)gstypes.h
$(PSSRC)imain.h:$(GLSRC)std.h
$(PSSRC)imain.h:$(GLSRC)stdpre.h
$(PSSRC)imain.h:$(GLGEN)arch.h
$(PSSRC)imain.h:$(GLSRC)gs_dll_call.h
$(PSSRC)imainarg.h:$(PSSRC)imain.h
$(PSSRC)imainarg.h:$(GLSRC)gsexit.h
$(PSSRC)imainarg.h:$(PSSRC)iref.h
$(PSSRC)imainarg.h:$(GLSRC)gxalloc.h
$(PSSRC)imainarg.h:$(GLSRC)gxobj.h
$(PSSRC)imainarg.h:$(GLSRC)gsnamecl.h
$(PSSRC)imainarg.h:$(GLSRC)gxcspace.h
$(PSSRC)imainarg.h:$(GLSRC)gscsel.h
$(PSSRC)imainarg.h:$(GLSRC)gsdcolor.h
$(PSSRC)imainarg.h:$(GLSRC)gxfrac.h
$(PSSRC)imainarg.h:$(GLSRC)gscms.h
$(PSSRC)imainarg.h:$(GLSRC)gsdevice.h
$(PSSRC)imainarg.h:$(GLSRC)gscspace.h
$(PSSRC)imainarg.h:$(GLSRC)gsgstate.h
$(PSSRC)imainarg.h:$(GLSRC)gsiparam.h
$(PSSRC)imainarg.h:$(GLSRC)gsmatrix.h
$(PSSRC)imainarg.h:$(GLSRC)gxhttile.h
$(PSSRC)imainarg.h:$(GLSRC)gsparam.h
$(PSSRC)imainarg.h:$(GLSRC)gsrefct.h
$(PSSRC)imainarg.h:$(GLSRC)memento.h
$(PSSRC)imainarg.h:$(GLSRC)gsstruct.h
$(PSSRC)imainarg.h:$(GLSRC)gxsync.h
$(PSSRC)imainarg.h:$(GLSRC)gxbitmap.h
$(PSSRC)imainarg.h:$(GLSRC)scommon.h
$(PSSRC)imainarg.h:$(GLSRC)gsbitmap.h
$(PSSRC)imainarg.h:$(GLSRC)gsccolor.h
$(PSSRC)imainarg.h:$(GLSRC)gxarith.h
$(PSSRC)imainarg.h:$(GLSRC)gpsync.h
$(PSSRC)imainarg.h:$(GLSRC)gsstype.h
$(PSSRC)imainarg.h:$(GLSRC)gsmemory.h
$(PSSRC)imainarg.h:$(GLSRC)gslibctx.h
$(PSSRC)imainarg.h:$(GLSRC)gsalloc.h
$(PSSRC)imainarg.h:$(GLSRC)gxcindex.h
$(PSSRC)imainarg.h:$(GLSRC)stdio_.h
$(PSSRC)imainarg.h:$(GLSRC)stdint_.h
$(PSSRC)imainarg.h:$(GLSRC)gssprintf.h
$(PSSRC)imainarg.h:$(GLSRC)gstypes.h
$(PSSRC)imainarg.h:$(GLSRC)std.h
$(PSSRC)imainarg.h:$(GLSRC)stdpre.h
$(PSSRC)imainarg.h:$(GLGEN)arch.h
$(PSSRC)imainarg.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iminst.h:$(PSSRC)imain.h
$(PSSRC)iminst.h:$(GLSRC)gsexit.h
$(PSSRC)iminst.h:$(PSSRC)iref.h
$(PSSRC)iminst.h:$(GLSRC)gxalloc.h
$(PSSRC)iminst.h:$(GLSRC)gxobj.h
$(PSSRC)iminst.h:$(GLSRC)gsnamecl.h
$(PSSRC)iminst.h:$(GLSRC)gxcspace.h
$(PSSRC)iminst.h:$(GLSRC)gscsel.h
$(PSSRC)iminst.h:$(GLSRC)gsdcolor.h
$(PSSRC)iminst.h:$(GLSRC)gxfrac.h
$(PSSRC)iminst.h:$(GLSRC)gscms.h
$(PSSRC)iminst.h:$(GLSRC)gsdevice.h
$(PSSRC)iminst.h:$(GLSRC)gscspace.h
$(PSSRC)iminst.h:$(GLSRC)gsgstate.h
$(PSSRC)iminst.h:$(GLSRC)gsiparam.h
$(PSSRC)iminst.h:$(GLSRC)gsmatrix.h
$(PSSRC)iminst.h:$(GLSRC)gxhttile.h
$(PSSRC)iminst.h:$(GLSRC)gsparam.h
$(PSSRC)iminst.h:$(GLSRC)gsrefct.h
$(PSSRC)iminst.h:$(GLSRC)memento.h
$(PSSRC)iminst.h:$(GLSRC)gsstruct.h
$(PSSRC)iminst.h:$(GLSRC)gxsync.h
$(PSSRC)iminst.h:$(GLSRC)gxbitmap.h
$(PSSRC)iminst.h:$(GLSRC)scommon.h
$(PSSRC)iminst.h:$(GLSRC)gsbitmap.h
$(PSSRC)iminst.h:$(GLSRC)gsccolor.h
$(PSSRC)iminst.h:$(GLSRC)gxarith.h
$(PSSRC)iminst.h:$(GLSRC)gpsync.h
$(PSSRC)iminst.h:$(GLSRC)gsstype.h
$(PSSRC)iminst.h:$(GLSRC)gsmemory.h
$(PSSRC)iminst.h:$(GLSRC)gslibctx.h
$(PSSRC)iminst.h:$(GLSRC)gsalloc.h
$(PSSRC)iminst.h:$(GLSRC)gxcindex.h
$(PSSRC)iminst.h:$(GLSRC)stdio_.h
$(PSSRC)iminst.h:$(GLSRC)stdint_.h
$(PSSRC)iminst.h:$(GLSRC)gssprintf.h
$(PSSRC)iminst.h:$(GLSRC)gstypes.h
$(PSSRC)iminst.h:$(GLSRC)std.h
$(PSSRC)iminst.h:$(GLSRC)stdpre.h
$(PSSRC)iminst.h:$(GLGEN)arch.h
$(PSSRC)iminst.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iparray.h:$(PSSRC)isdata.h
$(PSSRC)iparray.h:$(PSSRC)imemory.h
$(PSSRC)iparray.h:$(PSSRC)ivmspace.h
$(PSSRC)iparray.h:$(PSSRC)iref.h
$(PSSRC)iparray.h:$(GLSRC)gsgc.h
$(PSSRC)iparray.h:$(GLSRC)gxalloc.h
$(PSSRC)iparray.h:$(GLSRC)gxobj.h
$(PSSRC)iparray.h:$(GLSRC)gsnamecl.h
$(PSSRC)iparray.h:$(GLSRC)gxcspace.h
$(PSSRC)iparray.h:$(GLSRC)gscsel.h
$(PSSRC)iparray.h:$(GLSRC)gsdcolor.h
$(PSSRC)iparray.h:$(GLSRC)gxfrac.h
$(PSSRC)iparray.h:$(GLSRC)gscms.h
$(PSSRC)iparray.h:$(GLSRC)gsdevice.h
$(PSSRC)iparray.h:$(GLSRC)gscspace.h
$(PSSRC)iparray.h:$(GLSRC)gsgstate.h
$(PSSRC)iparray.h:$(GLSRC)gsiparam.h
$(PSSRC)iparray.h:$(GLSRC)gsmatrix.h
$(PSSRC)iparray.h:$(GLSRC)gxhttile.h
$(PSSRC)iparray.h:$(GLSRC)gsparam.h
$(PSSRC)iparray.h:$(GLSRC)gsrefct.h
$(PSSRC)iparray.h:$(GLSRC)memento.h
$(PSSRC)iparray.h:$(GLSRC)gsstruct.h
$(PSSRC)iparray.h:$(GLSRC)gxsync.h
$(PSSRC)iparray.h:$(GLSRC)gxbitmap.h
$(PSSRC)iparray.h:$(GLSRC)scommon.h
$(PSSRC)iparray.h:$(GLSRC)gsbitmap.h
$(PSSRC)iparray.h:$(GLSRC)gsccolor.h
$(PSSRC)iparray.h:$(GLSRC)gxarith.h
$(PSSRC)iparray.h:$(GLSRC)gpsync.h
$(PSSRC)iparray.h:$(GLSRC)gsstype.h
$(PSSRC)iparray.h:$(GLSRC)gsmemory.h
$(PSSRC)iparray.h:$(GLSRC)gslibctx.h
$(PSSRC)iparray.h:$(GLSRC)gsalloc.h
$(PSSRC)iparray.h:$(GLSRC)gxcindex.h
$(PSSRC)iparray.h:$(GLSRC)stdio_.h
$(PSSRC)iparray.h:$(GLSRC)stdint_.h
$(PSSRC)iparray.h:$(GLSRC)gssprintf.h
$(PSSRC)iparray.h:$(GLSRC)gstypes.h
$(PSSRC)iparray.h:$(GLSRC)std.h
$(PSSRC)iparray.h:$(GLSRC)stdpre.h
$(PSSRC)iparray.h:$(GLGEN)arch.h
$(PSSRC)iparray.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iscanbin.h:$(PSSRC)iscan.h
$(PSSRC)iscanbin.h:$(PSSRC)iref.h
$(PSSRC)iscanbin.h:$(GLSRC)sa85x.h
$(PSSRC)iscanbin.h:$(GLSRC)sa85d.h
$(PSSRC)iscanbin.h:$(GLSRC)sstring.h
$(PSSRC)iscanbin.h:$(PSSRC)inamestr.h
$(PSSRC)iscanbin.h:$(PSSRC)inameidx.h
$(PSSRC)iscanbin.h:$(GLSRC)gxalloc.h
$(PSSRC)iscanbin.h:$(GLSRC)gxobj.h
$(PSSRC)iscanbin.h:$(GLSRC)gsnamecl.h
$(PSSRC)iscanbin.h:$(GLSRC)gxcspace.h
$(PSSRC)iscanbin.h:$(GLSRC)gscsel.h
$(PSSRC)iscanbin.h:$(GLSRC)gsdcolor.h
$(PSSRC)iscanbin.h:$(GLSRC)gxfrac.h
$(PSSRC)iscanbin.h:$(GLSRC)gscms.h
$(PSSRC)iscanbin.h:$(GLSRC)gsdevice.h
$(PSSRC)iscanbin.h:$(GLSRC)gscspace.h
$(PSSRC)iscanbin.h:$(GLSRC)gsgstate.h
$(PSSRC)iscanbin.h:$(GLSRC)gsiparam.h
$(PSSRC)iscanbin.h:$(GLSRC)gsmatrix.h
$(PSSRC)iscanbin.h:$(GLSRC)gxhttile.h
$(PSSRC)iscanbin.h:$(GLSRC)gsparam.h
$(PSSRC)iscanbin.h:$(GLSRC)gsrefct.h
$(PSSRC)iscanbin.h:$(GLSRC)memento.h
$(PSSRC)iscanbin.h:$(GLSRC)gsstruct.h
$(PSSRC)iscanbin.h:$(GLSRC)gxsync.h
$(PSSRC)iscanbin.h:$(GLSRC)gxbitmap.h
$(PSSRC)iscanbin.h:$(GLSRC)scommon.h
$(PSSRC)iscanbin.h:$(GLSRC)gsbitmap.h
$(PSSRC)iscanbin.h:$(GLSRC)gsccolor.h
$(PSSRC)iscanbin.h:$(GLSRC)gxarith.h
$(PSSRC)iscanbin.h:$(GLSRC)gpsync.h
$(PSSRC)iscanbin.h:$(GLSRC)gsstype.h
$(PSSRC)iscanbin.h:$(GLSRC)gsmemory.h
$(PSSRC)iscanbin.h:$(GLSRC)gslibctx.h
$(PSSRC)iscanbin.h:$(GLSRC)gsalloc.h
$(PSSRC)iscanbin.h:$(GLSRC)gxcindex.h
$(PSSRC)iscanbin.h:$(GLSRC)stdio_.h
$(PSSRC)iscanbin.h:$(GLSRC)stdint_.h
$(PSSRC)iscanbin.h:$(GLSRC)gssprintf.h
$(PSSRC)iscanbin.h:$(GLSRC)gstypes.h
$(PSSRC)iscanbin.h:$(GLSRC)std.h
$(PSSRC)iscanbin.h:$(GLSRC)stdpre.h
$(PSSRC)iscanbin.h:$(GLGEN)arch.h
$(PSSRC)iscanbin.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iscannum.h:$(PSSRC)imemory.h
$(PSSRC)iscannum.h:$(PSSRC)ivmspace.h
$(PSSRC)iscannum.h:$(PSSRC)iref.h
$(PSSRC)iscannum.h:$(GLSRC)gsgc.h
$(PSSRC)iscannum.h:$(GLSRC)gxalloc.h
$(PSSRC)iscannum.h:$(GLSRC)gxobj.h
$(PSSRC)iscannum.h:$(GLSRC)gsnamecl.h
$(PSSRC)iscannum.h:$(GLSRC)gxcspace.h
$(PSSRC)iscannum.h:$(GLSRC)gscsel.h
$(PSSRC)iscannum.h:$(GLSRC)gsdcolor.h
$(PSSRC)iscannum.h:$(GLSRC)gxfrac.h
$(PSSRC)iscannum.h:$(GLSRC)gscms.h
$(PSSRC)iscannum.h:$(GLSRC)gsdevice.h
$(PSSRC)iscannum.h:$(GLSRC)gscspace.h
$(PSSRC)iscannum.h:$(GLSRC)gsgstate.h
$(PSSRC)iscannum.h:$(GLSRC)gsiparam.h
$(PSSRC)iscannum.h:$(GLSRC)gsmatrix.h
$(PSSRC)iscannum.h:$(GLSRC)gxhttile.h
$(PSSRC)iscannum.h:$(GLSRC)gsparam.h
$(PSSRC)iscannum.h:$(GLSRC)gsrefct.h
$(PSSRC)iscannum.h:$(GLSRC)memento.h
$(PSSRC)iscannum.h:$(GLSRC)gsstruct.h
$(PSSRC)iscannum.h:$(GLSRC)gxsync.h
$(PSSRC)iscannum.h:$(GLSRC)gxbitmap.h
$(PSSRC)iscannum.h:$(GLSRC)scommon.h
$(PSSRC)iscannum.h:$(GLSRC)gsbitmap.h
$(PSSRC)iscannum.h:$(GLSRC)gsccolor.h
$(PSSRC)iscannum.h:$(GLSRC)gxarith.h
$(PSSRC)iscannum.h:$(GLSRC)gpsync.h
$(PSSRC)iscannum.h:$(GLSRC)gsstype.h
$(PSSRC)iscannum.h:$(GLSRC)gsmemory.h
$(PSSRC)iscannum.h:$(GLSRC)gslibctx.h
$(PSSRC)iscannum.h:$(GLSRC)gsalloc.h
$(PSSRC)iscannum.h:$(GLSRC)gxcindex.h
$(PSSRC)iscannum.h:$(GLSRC)stdio_.h
$(PSSRC)iscannum.h:$(GLSRC)stdint_.h
$(PSSRC)iscannum.h:$(GLSRC)gssprintf.h
$(PSSRC)iscannum.h:$(GLSRC)gstypes.h
$(PSSRC)iscannum.h:$(GLSRC)std.h
$(PSSRC)iscannum.h:$(GLSRC)stdpre.h
$(PSSRC)iscannum.h:$(GLGEN)arch.h
$(PSSRC)iscannum.h:$(GLSRC)gs_dll_call.h
$(PSSRC)istream.h:$(PSSRC)imemory.h
$(PSSRC)istream.h:$(PSSRC)ivmspace.h
$(PSSRC)istream.h:$(PSSRC)iref.h
$(PSSRC)istream.h:$(GLSRC)gsgc.h
$(PSSRC)istream.h:$(GLSRC)gxalloc.h
$(PSSRC)istream.h:$(GLSRC)gxobj.h
$(PSSRC)istream.h:$(GLSRC)gsnamecl.h
$(PSSRC)istream.h:$(GLSRC)gxcspace.h
$(PSSRC)istream.h:$(GLSRC)gscsel.h
$(PSSRC)istream.h:$(GLSRC)gsdcolor.h
$(PSSRC)istream.h:$(GLSRC)gxfrac.h
$(PSSRC)istream.h:$(GLSRC)gscms.h
$(PSSRC)istream.h:$(GLSRC)gsdevice.h
$(PSSRC)istream.h:$(GLSRC)gscspace.h
$(PSSRC)istream.h:$(GLSRC)gsgstate.h
$(PSSRC)istream.h:$(GLSRC)gsiparam.h
$(PSSRC)istream.h:$(GLSRC)gsmatrix.h
$(PSSRC)istream.h:$(GLSRC)gxhttile.h
$(PSSRC)istream.h:$(GLSRC)gsparam.h
$(PSSRC)istream.h:$(GLSRC)gsrefct.h
$(PSSRC)istream.h:$(GLSRC)memento.h
$(PSSRC)istream.h:$(GLSRC)gsstruct.h
$(PSSRC)istream.h:$(GLSRC)gxsync.h
$(PSSRC)istream.h:$(GLSRC)gxbitmap.h
$(PSSRC)istream.h:$(GLSRC)scommon.h
$(PSSRC)istream.h:$(GLSRC)gsbitmap.h
$(PSSRC)istream.h:$(GLSRC)gsccolor.h
$(PSSRC)istream.h:$(GLSRC)gxarith.h
$(PSSRC)istream.h:$(GLSRC)gpsync.h
$(PSSRC)istream.h:$(GLSRC)gsstype.h
$(PSSRC)istream.h:$(GLSRC)gsmemory.h
$(PSSRC)istream.h:$(GLSRC)gslibctx.h
$(PSSRC)istream.h:$(GLSRC)gsalloc.h
$(PSSRC)istream.h:$(GLSRC)gxcindex.h
$(PSSRC)istream.h:$(GLSRC)stdio_.h
$(PSSRC)istream.h:$(GLSRC)stdint_.h
$(PSSRC)istream.h:$(GLSRC)gssprintf.h
$(PSSRC)istream.h:$(GLSRC)gstypes.h
$(PSSRC)istream.h:$(GLSRC)std.h
$(PSSRC)istream.h:$(GLSRC)stdpre.h
$(PSSRC)istream.h:$(GLGEN)arch.h
$(PSSRC)istream.h:$(GLSRC)gs_dll_call.h
$(PSSRC)itoken.h:$(PSSRC)iscan.h
$(PSSRC)itoken.h:$(PSSRC)iref.h
$(PSSRC)itoken.h:$(GLSRC)sa85x.h
$(PSSRC)itoken.h:$(GLSRC)sa85d.h
$(PSSRC)itoken.h:$(GLSRC)sstring.h
$(PSSRC)itoken.h:$(PSSRC)inamestr.h
$(PSSRC)itoken.h:$(PSSRC)inameidx.h
$(PSSRC)itoken.h:$(GLSRC)gxalloc.h
$(PSSRC)itoken.h:$(GLSRC)gxobj.h
$(PSSRC)itoken.h:$(GLSRC)gsnamecl.h
$(PSSRC)itoken.h:$(GLSRC)gxcspace.h
$(PSSRC)itoken.h:$(GLSRC)gscsel.h
$(PSSRC)itoken.h:$(GLSRC)gsdcolor.h
$(PSSRC)itoken.h:$(GLSRC)gxfrac.h
$(PSSRC)itoken.h:$(GLSRC)gscms.h
$(PSSRC)itoken.h:$(GLSRC)gsdevice.h
$(PSSRC)itoken.h:$(GLSRC)gscspace.h
$(PSSRC)itoken.h:$(GLSRC)gsgstate.h
$(PSSRC)itoken.h:$(GLSRC)gsiparam.h
$(PSSRC)itoken.h:$(GLSRC)gsmatrix.h
$(PSSRC)itoken.h:$(GLSRC)gxhttile.h
$(PSSRC)itoken.h:$(GLSRC)gsparam.h
$(PSSRC)itoken.h:$(GLSRC)gsrefct.h
$(PSSRC)itoken.h:$(GLSRC)memento.h
$(PSSRC)itoken.h:$(GLSRC)gsstruct.h
$(PSSRC)itoken.h:$(GLSRC)gxsync.h
$(PSSRC)itoken.h:$(GLSRC)gxbitmap.h
$(PSSRC)itoken.h:$(GLSRC)scommon.h
$(PSSRC)itoken.h:$(GLSRC)gsbitmap.h
$(PSSRC)itoken.h:$(GLSRC)gsccolor.h
$(PSSRC)itoken.h:$(GLSRC)gxarith.h
$(PSSRC)itoken.h:$(GLSRC)gpsync.h
$(PSSRC)itoken.h:$(GLSRC)gsstype.h
$(PSSRC)itoken.h:$(GLSRC)gsmemory.h
$(PSSRC)itoken.h:$(GLSRC)gslibctx.h
$(PSSRC)itoken.h:$(GLSRC)gsalloc.h
$(PSSRC)itoken.h:$(GLSRC)gxcindex.h
$(PSSRC)itoken.h:$(GLSRC)stdio_.h
$(PSSRC)itoken.h:$(GLSRC)stdint_.h
$(PSSRC)itoken.h:$(GLSRC)gssprintf.h
$(PSSRC)itoken.h:$(GLSRC)gstypes.h
$(PSSRC)itoken.h:$(GLSRC)std.h
$(PSSRC)itoken.h:$(GLSRC)stdpre.h
$(PSSRC)itoken.h:$(GLGEN)arch.h
$(PSSRC)itoken.h:$(GLSRC)gs_dll_call.h
$(PSSRC)main.h:$(PSSRC)iminst.h
$(PSSRC)main.h:$(PSSRC)imain.h
$(PSSRC)main.h:$(GLSRC)gsexit.h
$(PSSRC)main.h:$(PSSRC)iref.h
$(PSSRC)main.h:$(GLSRC)gxalloc.h
$(PSSRC)main.h:$(GLSRC)gxobj.h
$(PSSRC)main.h:$(GLSRC)gsnamecl.h
$(PSSRC)main.h:$(PSSRC)iapi.h
$(PSSRC)main.h:$(GLSRC)gxcspace.h
$(PSSRC)main.h:$(GLSRC)gscsel.h
$(PSSRC)main.h:$(GLSRC)gsdcolor.h
$(PSSRC)main.h:$(GLSRC)gxfrac.h
$(PSSRC)main.h:$(GLSRC)gscms.h
$(PSSRC)main.h:$(GLSRC)gsdevice.h
$(PSSRC)main.h:$(GLSRC)gscspace.h
$(PSSRC)main.h:$(GLSRC)gsgstate.h
$(PSSRC)main.h:$(GLSRC)gsiparam.h
$(PSSRC)main.h:$(GLSRC)gsmatrix.h
$(PSSRC)main.h:$(GLSRC)gxhttile.h
$(PSSRC)main.h:$(GLSRC)gsparam.h
$(PSSRC)main.h:$(GLSRC)gsrefct.h
$(PSSRC)main.h:$(GLSRC)memento.h
$(PSSRC)main.h:$(GLSRC)gsstruct.h
$(PSSRC)main.h:$(GLSRC)gxsync.h
$(PSSRC)main.h:$(GLSRC)gxbitmap.h
$(PSSRC)main.h:$(GLSRC)scommon.h
$(PSSRC)main.h:$(GLSRC)gsbitmap.h
$(PSSRC)main.h:$(GLSRC)gsccolor.h
$(PSSRC)main.h:$(GLSRC)gxarith.h
$(PSSRC)main.h:$(GLSRC)gpsync.h
$(PSSRC)main.h:$(GLSRC)gsstype.h
$(PSSRC)main.h:$(GLSRC)gsmemory.h
$(PSSRC)main.h:$(GLSRC)gslibctx.h
$(PSSRC)main.h:$(GLSRC)gsalloc.h
$(PSSRC)main.h:$(GLSRC)gxcindex.h
$(PSSRC)main.h:$(GLSRC)stdio_.h
$(PSSRC)main.h:$(GLSRC)stdint_.h
$(PSSRC)main.h:$(GLSRC)gssprintf.h
$(PSSRC)main.h:$(GLSRC)gstypes.h
$(PSSRC)main.h:$(GLSRC)std.h
$(PSSRC)main.h:$(GLSRC)stdpre.h
$(PSSRC)main.h:$(GLGEN)arch.h
$(PSSRC)main.h:$(GLSRC)gs_dll_call.h
$(GLSRC)smtf.h:$(GLSRC)scommon.h
$(GLSRC)smtf.h:$(GLSRC)gsstype.h
$(GLSRC)smtf.h:$(GLSRC)gsmemory.h
$(GLSRC)smtf.h:$(GLSRC)gslibctx.h
$(GLSRC)smtf.h:$(GLSRC)stdio_.h
$(GLSRC)smtf.h:$(GLSRC)stdint_.h
$(GLSRC)smtf.h:$(GLSRC)gssprintf.h
$(GLSRC)smtf.h:$(GLSRC)gstypes.h
$(GLSRC)smtf.h:$(GLSRC)std.h
$(GLSRC)smtf.h:$(GLSRC)stdpre.h
$(GLSRC)smtf.h:$(GLGEN)arch.h
$(GLSRC)smtf.h:$(GLSRC)gs_dll_call.h
$(PSSRC)bfont.h:$(PSSRC)ifont.h
$(PSSRC)bfont.h:$(GLSRC)gxfont.h
$(PSSRC)bfont.h:$(PSSRC)iostack.h
$(PSSRC)bfont.h:$(GLSRC)gspath.h
$(PSSRC)bfont.h:$(PSSRC)istack.h
$(PSSRC)bfont.h:$(GLSRC)gsgdata.h
$(PSSRC)bfont.h:$(PSSRC)iosdata.h
$(PSSRC)bfont.h:$(GLSRC)gxmatrix.h
$(PSSRC)bfont.h:$(GLSRC)gxfapi.h
$(PSSRC)bfont.h:$(PSSRC)isdata.h
$(PSSRC)bfont.h:$(PSSRC)imemory.h
$(PSSRC)bfont.h:$(GLSRC)gsfcmap.h
$(PSSRC)bfont.h:$(PSSRC)ivmspace.h
$(PSSRC)bfont.h:$(PSSRC)iref.h
$(PSSRC)bfont.h:$(GLSRC)gsgc.h
$(PSSRC)bfont.h:$(GLSRC)gxalloc.h
$(PSSRC)bfont.h:$(GLSRC)gxobj.h
$(PSSRC)bfont.h:$(GLSRC)gstext.h
$(PSSRC)bfont.h:$(GLSRC)gsnamecl.h
$(PSSRC)bfont.h:$(GLSRC)gxcspace.h
$(PSSRC)bfont.h:$(GLSRC)gscsel.h
$(PSSRC)bfont.h:$(GLSRC)gsfont.h
$(PSSRC)bfont.h:$(GLSRC)gsdcolor.h
$(PSSRC)bfont.h:$(GLSRC)gxpath.h
$(PSSRC)bfont.h:$(GLSRC)gxfrac.h
$(PSSRC)bfont.h:$(GLSRC)gxftype.h
$(PSSRC)bfont.h:$(GLSRC)gscms.h
$(PSSRC)bfont.h:$(GLSRC)gsrect.h
$(PSSRC)bfont.h:$(GLSRC)gslparam.h
$(PSSRC)bfont.h:$(GLSRC)gsdevice.h
$(PSSRC)bfont.h:$(GLSRC)gscpm.h
$(PSSRC)bfont.h:$(GLSRC)gsgcache.h
$(PSSRC)bfont.h:$(GLSRC)gscspace.h
$(PSSRC)bfont.h:$(GLSRC)gsgstate.h
$(PSSRC)bfont.h:$(GLSRC)gsnotify.h
$(PSSRC)bfont.h:$(GLSRC)gsiparam.h
$(PSSRC)bfont.h:$(GLSRC)gxfixed.h
$(PSSRC)bfont.h:$(GLSRC)gsmatrix.h
$(PSSRC)bfont.h:$(GLSRC)gspenum.h
$(PSSRC)bfont.h:$(GLSRC)gxhttile.h
$(PSSRC)bfont.h:$(GLSRC)gsparam.h
$(PSSRC)bfont.h:$(GLSRC)gsrefct.h
$(PSSRC)bfont.h:$(GLSRC)memento.h
$(PSSRC)bfont.h:$(GLSRC)gsuid.h
$(PSSRC)bfont.h:$(GLSRC)gsstruct.h
$(PSSRC)bfont.h:$(GLSRC)gxsync.h
$(PSSRC)bfont.h:$(GLSRC)gxbitmap.h
$(PSSRC)bfont.h:$(GLSRC)scommon.h
$(PSSRC)bfont.h:$(GLSRC)gsbitmap.h
$(PSSRC)bfont.h:$(GLSRC)gsccolor.h
$(PSSRC)bfont.h:$(GLSRC)gxarith.h
$(PSSRC)bfont.h:$(GLSRC)gpsync.h
$(PSSRC)bfont.h:$(GLSRC)gsstype.h
$(PSSRC)bfont.h:$(GLSRC)gsmemory.h
$(PSSRC)bfont.h:$(GLSRC)gslibctx.h
$(PSSRC)bfont.h:$(GLSRC)gsalloc.h
$(PSSRC)bfont.h:$(GLSRC)gxcindex.h
$(PSSRC)bfont.h:$(GLSRC)stdio_.h
$(PSSRC)bfont.h:$(GLSRC)gsccode.h
$(PSSRC)bfont.h:$(GLSRC)stdint_.h
$(PSSRC)bfont.h:$(GLSRC)gssprintf.h
$(PSSRC)bfont.h:$(GLSRC)gstypes.h
$(PSSRC)bfont.h:$(GLSRC)std.h
$(PSSRC)bfont.h:$(GLSRC)stdpre.h
$(PSSRC)bfont.h:$(GLGEN)arch.h
$(PSSRC)bfont.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icontext.h:$(PSSRC)icstate.h
$(PSSRC)icontext.h:$(PSSRC)idsdata.h
$(PSSRC)icontext.h:$(PSSRC)iesdata.h
$(PSSRC)icontext.h:$(PSSRC)interp.h
$(PSSRC)icontext.h:$(PSSRC)files.h
$(PSSRC)icontext.h:$(PSSRC)opdef.h
$(PSSRC)icontext.h:$(PSSRC)iddstack.h
$(PSSRC)icontext.h:$(PSSRC)store.h
$(PSSRC)icontext.h:$(PSSRC)iosdata.h
$(PSSRC)icontext.h:$(PSSRC)ialloc.h
$(PSSRC)icontext.h:$(PSSRC)idosave.h
$(PSSRC)icontext.h:$(PSSRC)isdata.h
$(PSSRC)icontext.h:$(PSSRC)imemory.h
$(PSSRC)icontext.h:$(PSSRC)ivmspace.h
$(PSSRC)icontext.h:$(PSSRC)iref.h
$(PSSRC)icontext.h:$(GLSRC)gsgc.h
$(PSSRC)icontext.h:$(GLSRC)gxalloc.h
$(PSSRC)icontext.h:$(GLSRC)gxobj.h
$(PSSRC)icontext.h:$(GLSRC)gsnamecl.h
$(PSSRC)icontext.h:$(GLSRC)stream.h
$(PSSRC)icontext.h:$(GLSRC)gxcspace.h
$(PSSRC)icontext.h:$(GLSRC)gxiodev.h
$(PSSRC)icontext.h:$(GLSRC)gscsel.h
$(PSSRC)icontext.h:$(GLSRC)gsdcolor.h
$(PSSRC)icontext.h:$(GLSRC)gxfrac.h
$(PSSRC)icontext.h:$(GLSRC)gscms.h
$(PSSRC)icontext.h:$(GLSRC)gsdevice.h
$(PSSRC)icontext.h:$(GLSRC)gscspace.h
$(PSSRC)icontext.h:$(GLSRC)gsgstate.h
$(PSSRC)icontext.h:$(GLSRC)gsiparam.h
$(PSSRC)icontext.h:$(GLSRC)gsmatrix.h
$(PSSRC)icontext.h:$(GLSRC)gxhttile.h
$(PSSRC)icontext.h:$(GLSRC)gsparam.h
$(PSSRC)icontext.h:$(GLSRC)gsrefct.h
$(PSSRC)icontext.h:$(GLSRC)gp.h
$(PSSRC)icontext.h:$(GLSRC)memento.h
$(PSSRC)icontext.h:$(GLSRC)memory_.h
$(PSSRC)icontext.h:$(GLSRC)gsstruct.h
$(PSSRC)icontext.h:$(GLSRC)gxsync.h
$(PSSRC)icontext.h:$(GLSRC)gxbitmap.h
$(PSSRC)icontext.h:$(GLSRC)srdline.h
$(PSSRC)icontext.h:$(GLSRC)scommon.h
$(PSSRC)icontext.h:$(GLSRC)gsfname.h
$(PSSRC)icontext.h:$(GLSRC)gsbitmap.h
$(PSSRC)icontext.h:$(GLSRC)gsccolor.h
$(PSSRC)icontext.h:$(GLSRC)gxarith.h
$(PSSRC)icontext.h:$(GLSRC)stat_.h
$(PSSRC)icontext.h:$(GLSRC)gpsync.h
$(PSSRC)icontext.h:$(GLSRC)gsstype.h
$(PSSRC)icontext.h:$(GLSRC)gsmemory.h
$(PSSRC)icontext.h:$(GLSRC)gpgetenv.h
$(PSSRC)icontext.h:$(GLSRC)gscdefs.h
$(PSSRC)icontext.h:$(GLSRC)gslibctx.h
$(PSSRC)icontext.h:$(GLSRC)gsalloc.h
$(PSSRC)icontext.h:$(GLSRC)gxcindex.h
$(PSSRC)icontext.h:$(GLSRC)stdio_.h
$(PSSRC)icontext.h:$(GLSRC)stdint_.h
$(PSSRC)icontext.h:$(GLSRC)gssprintf.h
$(PSSRC)icontext.h:$(GLSRC)gstypes.h
$(PSSRC)icontext.h:$(GLSRC)std.h
$(PSSRC)icontext.h:$(GLSRC)stdpre.h
$(PSSRC)icontext.h:$(GLGEN)arch.h
$(PSSRC)icontext.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifilter.h:$(PSSRC)istream.h
$(PSSRC)ifilter.h:$(PSSRC)imemory.h
$(PSSRC)ifilter.h:$(PSSRC)ivmspace.h
$(PSSRC)ifilter.h:$(PSSRC)iref.h
$(PSSRC)ifilter.h:$(GLSRC)gsgc.h
$(PSSRC)ifilter.h:$(GLSRC)gxalloc.h
$(PSSRC)ifilter.h:$(GLSRC)gxobj.h
$(PSSRC)ifilter.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifilter.h:$(GLSRC)gxcspace.h
$(PSSRC)ifilter.h:$(GLSRC)gscsel.h
$(PSSRC)ifilter.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifilter.h:$(GLSRC)gxfrac.h
$(PSSRC)ifilter.h:$(GLSRC)gscms.h
$(PSSRC)ifilter.h:$(GLSRC)gsdevice.h
$(PSSRC)ifilter.h:$(GLSRC)gscspace.h
$(PSSRC)ifilter.h:$(GLSRC)gsgstate.h
$(PSSRC)ifilter.h:$(GLSRC)gsiparam.h
$(PSSRC)ifilter.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifilter.h:$(GLSRC)gxhttile.h
$(PSSRC)ifilter.h:$(GLSRC)gsparam.h
$(PSSRC)ifilter.h:$(GLSRC)gsrefct.h
$(PSSRC)ifilter.h:$(GLSRC)memento.h
$(PSSRC)ifilter.h:$(GLSRC)gsstruct.h
$(PSSRC)ifilter.h:$(GLSRC)gxsync.h
$(PSSRC)ifilter.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifilter.h:$(GLSRC)scommon.h
$(PSSRC)ifilter.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifilter.h:$(GLSRC)gsccolor.h
$(PSSRC)ifilter.h:$(GLSRC)gxarith.h
$(PSSRC)ifilter.h:$(GLSRC)gpsync.h
$(PSSRC)ifilter.h:$(GLSRC)gsstype.h
$(PSSRC)ifilter.h:$(GLSRC)gsmemory.h
$(PSSRC)ifilter.h:$(GLSRC)gslibctx.h
$(PSSRC)ifilter.h:$(GLSRC)gsalloc.h
$(PSSRC)ifilter.h:$(GLSRC)gxcindex.h
$(PSSRC)ifilter.h:$(GLSRC)stdio_.h
$(PSSRC)ifilter.h:$(GLSRC)stdint_.h
$(PSSRC)ifilter.h:$(GLSRC)gssprintf.h
$(PSSRC)ifilter.h:$(GLSRC)gstypes.h
$(PSSRC)ifilter.h:$(GLSRC)std.h
$(PSSRC)ifilter.h:$(GLSRC)stdpre.h
$(PSSRC)ifilter.h:$(GLGEN)arch.h
$(PSSRC)ifilter.h:$(GLSRC)gs_dll_call.h
$(PSSRC)igstate.h:$(GLSRC)gsstate.h
$(PSSRC)igstate.h:$(GLSRC)gsovrc.h
$(PSSRC)igstate.h:$(GLSRC)gscolor.h
$(PSSRC)igstate.h:$(GLSRC)gsline.h
$(PSSRC)igstate.h:$(GLSRC)gxcomp.h
$(PSSRC)igstate.h:$(PSSRC)istruct.h
$(PSSRC)igstate.h:$(PSSRC)imemory.h
$(PSSRC)igstate.h:$(GLSRC)gsht.h
$(PSSRC)igstate.h:$(PSSRC)ivmspace.h
$(PSSRC)igstate.h:$(PSSRC)iref.h
$(PSSRC)igstate.h:$(GLSRC)gsgc.h
$(PSSRC)igstate.h:$(GLSRC)gxalloc.h
$(PSSRC)igstate.h:$(GLSRC)gxobj.h
$(PSSRC)igstate.h:$(GLSRC)gxstate.h
$(PSSRC)igstate.h:$(GLSRC)gsnamecl.h
$(PSSRC)igstate.h:$(GLSRC)gxcspace.h
$(PSSRC)igstate.h:$(GLSRC)gscsel.h
$(PSSRC)igstate.h:$(GLSRC)gsdcolor.h
$(PSSRC)igstate.h:$(GLSRC)gxfrac.h
$(PSSRC)igstate.h:$(GLSRC)gxtmap.h
$(PSSRC)igstate.h:$(GLSRC)gscms.h
$(PSSRC)igstate.h:$(GLSRC)gslparam.h
$(PSSRC)igstate.h:$(GLSRC)gsdevice.h
$(PSSRC)igstate.h:$(GLSRC)gxbitfmt.h
$(PSSRC)igstate.h:$(GLSRC)gscpm.h
$(PSSRC)igstate.h:$(GLSRC)gscspace.h
$(PSSRC)igstate.h:$(GLSRC)gsgstate.h
$(PSSRC)igstate.h:$(GLSRC)gsiparam.h
$(PSSRC)igstate.h:$(GLSRC)gscompt.h
$(PSSRC)igstate.h:$(GLSRC)gsmatrix.h
$(PSSRC)igstate.h:$(GLSRC)gxhttile.h
$(PSSRC)igstate.h:$(GLSRC)gsparam.h
$(PSSRC)igstate.h:$(GLSRC)gsrefct.h
$(PSSRC)igstate.h:$(GLSRC)memento.h
$(PSSRC)igstate.h:$(GLSRC)gsstruct.h
$(PSSRC)igstate.h:$(GLSRC)gxsync.h
$(PSSRC)igstate.h:$(GLSRC)gxbitmap.h
$(PSSRC)igstate.h:$(GLSRC)scommon.h
$(PSSRC)igstate.h:$(GLSRC)gsbitmap.h
$(PSSRC)igstate.h:$(GLSRC)gsccolor.h
$(PSSRC)igstate.h:$(GLSRC)gxarith.h
$(PSSRC)igstate.h:$(GLSRC)gpsync.h
$(PSSRC)igstate.h:$(GLSRC)gsstype.h
$(PSSRC)igstate.h:$(GLSRC)gsmemory.h
$(PSSRC)igstate.h:$(GLSRC)gslibctx.h
$(PSSRC)igstate.h:$(GLSRC)gsalloc.h
$(PSSRC)igstate.h:$(GLSRC)gxcindex.h
$(PSSRC)igstate.h:$(GLSRC)stdio_.h
$(PSSRC)igstate.h:$(GLSRC)stdint_.h
$(PSSRC)igstate.h:$(GLSRC)gssprintf.h
$(PSSRC)igstate.h:$(GLSRC)gstypes.h
$(PSSRC)igstate.h:$(GLSRC)std.h
$(PSSRC)igstate.h:$(GLSRC)stdpre.h
$(PSSRC)igstate.h:$(GLGEN)arch.h
$(PSSRC)igstate.h:$(GLSRC)gs_dll_call.h
$(PSSRC)iscan.h:$(PSSRC)iref.h
$(PSSRC)iscan.h:$(GLSRC)sa85x.h
$(PSSRC)iscan.h:$(GLSRC)sa85d.h
$(PSSRC)iscan.h:$(GLSRC)sstring.h
$(PSSRC)iscan.h:$(PSSRC)inamestr.h
$(PSSRC)iscan.h:$(PSSRC)inameidx.h
$(PSSRC)iscan.h:$(GLSRC)gxalloc.h
$(PSSRC)iscan.h:$(GLSRC)gxobj.h
$(PSSRC)iscan.h:$(GLSRC)gsnamecl.h
$(PSSRC)iscan.h:$(GLSRC)gxcspace.h
$(PSSRC)iscan.h:$(GLSRC)gscsel.h
$(PSSRC)iscan.h:$(GLSRC)gsdcolor.h
$(PSSRC)iscan.h:$(GLSRC)gxfrac.h
$(PSSRC)iscan.h:$(GLSRC)gscms.h
$(PSSRC)iscan.h:$(GLSRC)gsdevice.h
$(PSSRC)iscan.h:$(GLSRC)gscspace.h
$(PSSRC)iscan.h:$(GLSRC)gsgstate.h
$(PSSRC)iscan.h:$(GLSRC)gsiparam.h
$(PSSRC)iscan.h:$(GLSRC)gsmatrix.h
$(PSSRC)iscan.h:$(GLSRC)gxhttile.h
$(PSSRC)iscan.h:$(GLSRC)gsparam.h
$(PSSRC)iscan.h:$(GLSRC)gsrefct.h
$(PSSRC)iscan.h:$(GLSRC)memento.h
$(PSSRC)iscan.h:$(GLSRC)gsstruct.h
$(PSSRC)iscan.h:$(GLSRC)gxsync.h
$(PSSRC)iscan.h:$(GLSRC)gxbitmap.h
$(PSSRC)iscan.h:$(GLSRC)scommon.h
$(PSSRC)iscan.h:$(GLSRC)gsbitmap.h
$(PSSRC)iscan.h:$(GLSRC)gsccolor.h
$(PSSRC)iscan.h:$(GLSRC)gxarith.h
$(PSSRC)iscan.h:$(GLSRC)gpsync.h
$(PSSRC)iscan.h:$(GLSRC)gsstype.h
$(PSSRC)iscan.h:$(GLSRC)gsmemory.h
$(PSSRC)iscan.h:$(GLSRC)gslibctx.h
$(PSSRC)iscan.h:$(GLSRC)gsalloc.h
$(PSSRC)iscan.h:$(GLSRC)gxcindex.h
$(PSSRC)iscan.h:$(GLSRC)stdio_.h
$(PSSRC)iscan.h:$(GLSRC)stdint_.h
$(PSSRC)iscan.h:$(GLSRC)gssprintf.h
$(PSSRC)iscan.h:$(GLSRC)gstypes.h
$(PSSRC)iscan.h:$(GLSRC)std.h
$(PSSRC)iscan.h:$(GLSRC)stdpre.h
$(PSSRC)iscan.h:$(GLGEN)arch.h
$(PSSRC)iscan.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zfile.h:$(PSSRC)iref.h
$(PSSRC)zfile.h:$(GLSRC)gxalloc.h
$(PSSRC)zfile.h:$(GLSRC)gxobj.h
$(PSSRC)zfile.h:$(GLSRC)gsnamecl.h
$(PSSRC)zfile.h:$(GLSRC)gxcspace.h
$(PSSRC)zfile.h:$(GLSRC)gscsel.h
$(PSSRC)zfile.h:$(GLSRC)gsdcolor.h
$(PSSRC)zfile.h:$(GLSRC)gxfrac.h
$(PSSRC)zfile.h:$(GLSRC)gscms.h
$(PSSRC)zfile.h:$(GLSRC)gsdevice.h
$(PSSRC)zfile.h:$(GLSRC)gscspace.h
$(PSSRC)zfile.h:$(GLSRC)gsgstate.h
$(PSSRC)zfile.h:$(GLSRC)gsiparam.h
$(PSSRC)zfile.h:$(GLSRC)gsmatrix.h
$(PSSRC)zfile.h:$(GLSRC)gxhttile.h
$(PSSRC)zfile.h:$(GLSRC)gsparam.h
$(PSSRC)zfile.h:$(GLSRC)gsrefct.h
$(PSSRC)zfile.h:$(GLSRC)memento.h
$(PSSRC)zfile.h:$(GLSRC)gsstruct.h
$(PSSRC)zfile.h:$(GLSRC)gxsync.h
$(PSSRC)zfile.h:$(GLSRC)gxbitmap.h
$(PSSRC)zfile.h:$(GLSRC)scommon.h
$(PSSRC)zfile.h:$(GLSRC)gsfname.h
$(PSSRC)zfile.h:$(GLSRC)gsbitmap.h
$(PSSRC)zfile.h:$(GLSRC)gsccolor.h
$(PSSRC)zfile.h:$(GLSRC)gxarith.h
$(PSSRC)zfile.h:$(GLSRC)gpsync.h
$(PSSRC)zfile.h:$(GLSRC)gsstype.h
$(PSSRC)zfile.h:$(GLSRC)gsmemory.h
$(PSSRC)zfile.h:$(GLSRC)gslibctx.h
$(PSSRC)zfile.h:$(GLSRC)gsalloc.h
$(PSSRC)zfile.h:$(GLSRC)gxcindex.h
$(PSSRC)zfile.h:$(GLSRC)stdio_.h
$(PSSRC)zfile.h:$(GLSRC)stdint_.h
$(PSSRC)zfile.h:$(GLSRC)gssprintf.h
$(PSSRC)zfile.h:$(GLSRC)gstypes.h
$(PSSRC)zfile.h:$(GLSRC)std.h
$(PSSRC)zfile.h:$(GLSRC)stdpre.h
$(PSSRC)zfile.h:$(GLGEN)arch.h
$(PSSRC)zfile.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ibnum.h:$(PSSRC)iref.h
$(PSSRC)ibnum.h:$(GLSRC)gxalloc.h
$(PSSRC)ibnum.h:$(GLSRC)gxobj.h
$(PSSRC)ibnum.h:$(GLSRC)gsnamecl.h
$(PSSRC)ibnum.h:$(GLSRC)gxcspace.h
$(PSSRC)ibnum.h:$(GLSRC)gscsel.h
$(PSSRC)ibnum.h:$(GLSRC)gsdcolor.h
$(PSSRC)ibnum.h:$(GLSRC)gxfrac.h
$(PSSRC)ibnum.h:$(GLSRC)gscms.h
$(PSSRC)ibnum.h:$(GLSRC)gsdevice.h
$(PSSRC)ibnum.h:$(GLSRC)gscspace.h
$(PSSRC)ibnum.h:$(GLSRC)gsgstate.h
$(PSSRC)ibnum.h:$(GLSRC)gsiparam.h
$(PSSRC)ibnum.h:$(GLSRC)gsmatrix.h
$(PSSRC)ibnum.h:$(GLSRC)gxhttile.h
$(PSSRC)ibnum.h:$(GLSRC)gsparam.h
$(PSSRC)ibnum.h:$(GLSRC)gsrefct.h
$(PSSRC)ibnum.h:$(GLSRC)memento.h
$(PSSRC)ibnum.h:$(GLSRC)gsstruct.h
$(PSSRC)ibnum.h:$(GLSRC)gxsync.h
$(PSSRC)ibnum.h:$(GLSRC)gxbitmap.h
$(PSSRC)ibnum.h:$(GLSRC)scommon.h
$(PSSRC)ibnum.h:$(GLSRC)gsbitmap.h
$(PSSRC)ibnum.h:$(GLSRC)gsccolor.h
$(PSSRC)ibnum.h:$(GLSRC)gxarith.h
$(PSSRC)ibnum.h:$(GLSRC)gpsync.h
$(PSSRC)ibnum.h:$(GLSRC)gsstype.h
$(PSSRC)ibnum.h:$(GLSRC)gsmemory.h
$(PSSRC)ibnum.h:$(GLSRC)gslibctx.h
$(PSSRC)ibnum.h:$(GLSRC)gsalloc.h
$(PSSRC)ibnum.h:$(GLSRC)gxcindex.h
$(PSSRC)ibnum.h:$(GLSRC)stdio_.h
$(PSSRC)ibnum.h:$(GLSRC)stdint_.h
$(PSSRC)ibnum.h:$(GLSRC)gssprintf.h
$(PSSRC)ibnum.h:$(GLSRC)gstypes.h
$(PSSRC)ibnum.h:$(GLSRC)std.h
$(PSSRC)ibnum.h:$(GLSRC)stdpre.h
$(PSSRC)ibnum.h:$(GLGEN)arch.h
$(PSSRC)ibnum.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zcolor.h:$(PSSRC)iref.h
$(PSSRC)zcolor.h:$(GLSRC)gxalloc.h
$(PSSRC)zcolor.h:$(GLSRC)gxobj.h
$(PSSRC)zcolor.h:$(GLSRC)gsnamecl.h
$(PSSRC)zcolor.h:$(GLSRC)gxcspace.h
$(PSSRC)zcolor.h:$(GLSRC)gscsel.h
$(PSSRC)zcolor.h:$(GLSRC)gsdcolor.h
$(PSSRC)zcolor.h:$(GLSRC)gxfrac.h
$(PSSRC)zcolor.h:$(GLSRC)gscms.h
$(PSSRC)zcolor.h:$(GLSRC)gsdevice.h
$(PSSRC)zcolor.h:$(GLSRC)gscspace.h
$(PSSRC)zcolor.h:$(GLSRC)gsgstate.h
$(PSSRC)zcolor.h:$(GLSRC)gsiparam.h
$(PSSRC)zcolor.h:$(GLSRC)gsmatrix.h
$(PSSRC)zcolor.h:$(GLSRC)gxhttile.h
$(PSSRC)zcolor.h:$(GLSRC)gsparam.h
$(PSSRC)zcolor.h:$(GLSRC)gsrefct.h
$(PSSRC)zcolor.h:$(GLSRC)memento.h
$(PSSRC)zcolor.h:$(GLSRC)gsstruct.h
$(PSSRC)zcolor.h:$(GLSRC)gxsync.h
$(PSSRC)zcolor.h:$(GLSRC)gxbitmap.h
$(PSSRC)zcolor.h:$(GLSRC)scommon.h
$(PSSRC)zcolor.h:$(GLSRC)gsbitmap.h
$(PSSRC)zcolor.h:$(GLSRC)gsccolor.h
$(PSSRC)zcolor.h:$(GLSRC)gxarith.h
$(PSSRC)zcolor.h:$(GLSRC)gpsync.h
$(PSSRC)zcolor.h:$(GLSRC)gsstype.h
$(PSSRC)zcolor.h:$(GLSRC)gsmemory.h
$(PSSRC)zcolor.h:$(GLSRC)gslibctx.h
$(PSSRC)zcolor.h:$(GLSRC)gsalloc.h
$(PSSRC)zcolor.h:$(GLSRC)gxcindex.h
$(PSSRC)zcolor.h:$(GLSRC)stdio_.h
$(PSSRC)zcolor.h:$(GLSRC)stdint_.h
$(PSSRC)zcolor.h:$(GLSRC)gssprintf.h
$(PSSRC)zcolor.h:$(GLSRC)gstypes.h
$(PSSRC)zcolor.h:$(GLSRC)std.h
$(PSSRC)zcolor.h:$(GLSRC)stdpre.h
$(PSSRC)zcolor.h:$(GLGEN)arch.h
$(PSSRC)zcolor.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zcie.h:$(PSSRC)iref.h
$(PSSRC)zcie.h:$(GLSRC)gxalloc.h
$(PSSRC)zcie.h:$(GLSRC)gxobj.h
$(PSSRC)zcie.h:$(GLSRC)gsnamecl.h
$(PSSRC)zcie.h:$(GLSRC)gxcspace.h
$(PSSRC)zcie.h:$(GLSRC)gscsel.h
$(PSSRC)zcie.h:$(GLSRC)gsdcolor.h
$(PSSRC)zcie.h:$(GLSRC)gxfrac.h
$(PSSRC)zcie.h:$(GLSRC)gscms.h
$(PSSRC)zcie.h:$(GLSRC)gsdevice.h
$(PSSRC)zcie.h:$(GLSRC)gscspace.h
$(PSSRC)zcie.h:$(GLSRC)gsgstate.h
$(PSSRC)zcie.h:$(GLSRC)gsiparam.h
$(PSSRC)zcie.h:$(GLSRC)gsmatrix.h
$(PSSRC)zcie.h:$(GLSRC)gxhttile.h
$(PSSRC)zcie.h:$(GLSRC)gsparam.h
$(PSSRC)zcie.h:$(GLSRC)gsrefct.h
$(PSSRC)zcie.h:$(GLSRC)memento.h
$(PSSRC)zcie.h:$(GLSRC)gsstruct.h
$(PSSRC)zcie.h:$(GLSRC)gxsync.h
$(PSSRC)zcie.h:$(GLSRC)gxbitmap.h
$(PSSRC)zcie.h:$(GLSRC)scommon.h
$(PSSRC)zcie.h:$(GLSRC)gsbitmap.h
$(PSSRC)zcie.h:$(GLSRC)gsccolor.h
$(PSSRC)zcie.h:$(GLSRC)gxarith.h
$(PSSRC)zcie.h:$(GLSRC)gpsync.h
$(PSSRC)zcie.h:$(GLSRC)gsstype.h
$(PSSRC)zcie.h:$(GLSRC)gsmemory.h
$(PSSRC)zcie.h:$(GLSRC)gslibctx.h
$(PSSRC)zcie.h:$(GLSRC)gsalloc.h
$(PSSRC)zcie.h:$(GLSRC)gxcindex.h
$(PSSRC)zcie.h:$(GLSRC)stdio_.h
$(PSSRC)zcie.h:$(GLSRC)stdint_.h
$(PSSRC)zcie.h:$(GLSRC)gssprintf.h
$(PSSRC)zcie.h:$(GLSRC)gstypes.h
$(PSSRC)zcie.h:$(GLSRC)std.h
$(PSSRC)zcie.h:$(GLSRC)stdpre.h
$(PSSRC)zcie.h:$(GLGEN)arch.h
$(PSSRC)zcie.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zicc.h:$(PSSRC)iref.h
$(PSSRC)zicc.h:$(GLSRC)gxalloc.h
$(PSSRC)zicc.h:$(GLSRC)gxobj.h
$(PSSRC)zicc.h:$(GLSRC)gsnamecl.h
$(PSSRC)zicc.h:$(GLSRC)gxcspace.h
$(PSSRC)zicc.h:$(GLSRC)gscsel.h
$(PSSRC)zicc.h:$(GLSRC)gsdcolor.h
$(PSSRC)zicc.h:$(GLSRC)gxfrac.h
$(PSSRC)zicc.h:$(GLSRC)gscms.h
$(PSSRC)zicc.h:$(GLSRC)gsdevice.h
$(PSSRC)zicc.h:$(GLSRC)gscspace.h
$(PSSRC)zicc.h:$(GLSRC)gsgstate.h
$(PSSRC)zicc.h:$(GLSRC)gsiparam.h
$(PSSRC)zicc.h:$(GLSRC)gsmatrix.h
$(PSSRC)zicc.h:$(GLSRC)gxhttile.h
$(PSSRC)zicc.h:$(GLSRC)gsparam.h
$(PSSRC)zicc.h:$(GLSRC)gsrefct.h
$(PSSRC)zicc.h:$(GLSRC)memento.h
$(PSSRC)zicc.h:$(GLSRC)gsstruct.h
$(PSSRC)zicc.h:$(GLSRC)gxsync.h
$(PSSRC)zicc.h:$(GLSRC)gxbitmap.h
$(PSSRC)zicc.h:$(GLSRC)scommon.h
$(PSSRC)zicc.h:$(GLSRC)gsbitmap.h
$(PSSRC)zicc.h:$(GLSRC)gsccolor.h
$(PSSRC)zicc.h:$(GLSRC)gxarith.h
$(PSSRC)zicc.h:$(GLSRC)gpsync.h
$(PSSRC)zicc.h:$(GLSRC)gsstype.h
$(PSSRC)zicc.h:$(GLSRC)gsmemory.h
$(PSSRC)zicc.h:$(GLSRC)gslibctx.h
$(PSSRC)zicc.h:$(GLSRC)gsalloc.h
$(PSSRC)zicc.h:$(GLSRC)gxcindex.h
$(PSSRC)zicc.h:$(GLSRC)stdio_.h
$(PSSRC)zicc.h:$(GLSRC)stdint_.h
$(PSSRC)zicc.h:$(GLSRC)gssprintf.h
$(PSSRC)zicc.h:$(GLSRC)gstypes.h
$(PSSRC)zicc.h:$(GLSRC)std.h
$(PSSRC)zicc.h:$(GLSRC)stdpre.h
$(PSSRC)zicc.h:$(GLGEN)arch.h
$(PSSRC)zicc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)zfrsd.h:$(PSSRC)iostack.h
$(PSSRC)zfrsd.h:$(PSSRC)istack.h
$(PSSRC)zfrsd.h:$(PSSRC)iosdata.h
$(PSSRC)zfrsd.h:$(PSSRC)isdata.h
$(PSSRC)zfrsd.h:$(PSSRC)imemory.h
$(PSSRC)zfrsd.h:$(PSSRC)ivmspace.h
$(PSSRC)zfrsd.h:$(PSSRC)iref.h
$(PSSRC)zfrsd.h:$(GLSRC)gsgc.h
$(PSSRC)zfrsd.h:$(GLSRC)gxalloc.h
$(PSSRC)zfrsd.h:$(GLSRC)gxobj.h
$(PSSRC)zfrsd.h:$(GLSRC)gsnamecl.h
$(PSSRC)zfrsd.h:$(GLSRC)gxcspace.h
$(PSSRC)zfrsd.h:$(GLSRC)gscsel.h
$(PSSRC)zfrsd.h:$(GLSRC)gsdcolor.h
$(PSSRC)zfrsd.h:$(GLSRC)gxfrac.h
$(PSSRC)zfrsd.h:$(GLSRC)gscms.h
$(PSSRC)zfrsd.h:$(GLSRC)gsdevice.h
$(PSSRC)zfrsd.h:$(GLSRC)gscspace.h
$(PSSRC)zfrsd.h:$(GLSRC)gsgstate.h
$(PSSRC)zfrsd.h:$(GLSRC)gsiparam.h
$(PSSRC)zfrsd.h:$(GLSRC)gsmatrix.h
$(PSSRC)zfrsd.h:$(GLSRC)gxhttile.h
$(PSSRC)zfrsd.h:$(GLSRC)gsparam.h
$(PSSRC)zfrsd.h:$(GLSRC)gsrefct.h
$(PSSRC)zfrsd.h:$(GLSRC)memento.h
$(PSSRC)zfrsd.h:$(GLSRC)gsstruct.h
$(PSSRC)zfrsd.h:$(GLSRC)gxsync.h
$(PSSRC)zfrsd.h:$(GLSRC)gxbitmap.h
$(PSSRC)zfrsd.h:$(GLSRC)scommon.h
$(PSSRC)zfrsd.h:$(GLSRC)gsbitmap.h
$(PSSRC)zfrsd.h:$(GLSRC)gsccolor.h
$(PSSRC)zfrsd.h:$(GLSRC)gxarith.h
$(PSSRC)zfrsd.h:$(GLSRC)gpsync.h
$(PSSRC)zfrsd.h:$(GLSRC)gsstype.h
$(PSSRC)zfrsd.h:$(GLSRC)gsmemory.h
$(PSSRC)zfrsd.h:$(GLSRC)gslibctx.h
$(PSSRC)zfrsd.h:$(GLSRC)gsalloc.h
$(PSSRC)zfrsd.h:$(GLSRC)gxcindex.h
$(PSSRC)zfrsd.h:$(GLSRC)stdio_.h
$(PSSRC)zfrsd.h:$(GLSRC)stdint_.h
$(PSSRC)zfrsd.h:$(GLSRC)gssprintf.h
$(PSSRC)zfrsd.h:$(GLSRC)gstypes.h
$(PSSRC)zfrsd.h:$(GLSRC)std.h
$(PSSRC)zfrsd.h:$(GLSRC)stdpre.h
$(PSSRC)zfrsd.h:$(GLGEN)arch.h
$(PSSRC)zfrsd.h:$(GLSRC)gs_dll_call.h
$(PSSRC)dscparse.h:$(GLSRC)stdpre.h
$(PSSRC)ifunc.h:$(PSSRC)iref.h
$(PSSRC)ifunc.h:$(GLSRC)gxalloc.h
$(PSSRC)ifunc.h:$(GLSRC)gxobj.h
$(PSSRC)ifunc.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifunc.h:$(GLSRC)gsfunc.h
$(PSSRC)ifunc.h:$(GLSRC)gxcspace.h
$(PSSRC)ifunc.h:$(GLSRC)gscsel.h
$(PSSRC)ifunc.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifunc.h:$(GLSRC)gxfrac.h
$(PSSRC)ifunc.h:$(GLSRC)gscms.h
$(PSSRC)ifunc.h:$(GLSRC)gsdevice.h
$(PSSRC)ifunc.h:$(GLSRC)gscspace.h
$(PSSRC)ifunc.h:$(GLSRC)gsgstate.h
$(PSSRC)ifunc.h:$(GLSRC)gsdsrc.h
$(PSSRC)ifunc.h:$(GLSRC)gsiparam.h
$(PSSRC)ifunc.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifunc.h:$(GLSRC)gxhttile.h
$(PSSRC)ifunc.h:$(GLSRC)gsparam.h
$(PSSRC)ifunc.h:$(GLSRC)gsrefct.h
$(PSSRC)ifunc.h:$(GLSRC)memento.h
$(PSSRC)ifunc.h:$(GLSRC)gsstruct.h
$(PSSRC)ifunc.h:$(GLSRC)gxsync.h
$(PSSRC)ifunc.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifunc.h:$(GLSRC)scommon.h
$(PSSRC)ifunc.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifunc.h:$(GLSRC)gsccolor.h
$(PSSRC)ifunc.h:$(GLSRC)gxarith.h
$(PSSRC)ifunc.h:$(GLSRC)gpsync.h
$(PSSRC)ifunc.h:$(GLSRC)gsstype.h
$(PSSRC)ifunc.h:$(GLSRC)gsmemory.h
$(PSSRC)ifunc.h:$(GLSRC)gslibctx.h
$(PSSRC)ifunc.h:$(GLSRC)gsalloc.h
$(PSSRC)ifunc.h:$(GLSRC)gxcindex.h
$(PSSRC)ifunc.h:$(GLSRC)stdio_.h
$(PSSRC)ifunc.h:$(GLSRC)stdint_.h
$(PSSRC)ifunc.h:$(GLSRC)gssprintf.h
$(PSSRC)ifunc.h:$(GLSRC)gstypes.h
$(PSSRC)ifunc.h:$(GLSRC)std.h
$(PSSRC)ifunc.h:$(GLSRC)stdpre.h
$(PSSRC)ifunc.h:$(GLGEN)arch.h
$(PSSRC)ifunc.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ivmem2.h:$(PSSRC)iref.h
$(PSSRC)ivmem2.h:$(GLSRC)gxalloc.h
$(PSSRC)ivmem2.h:$(GLSRC)gxobj.h
$(PSSRC)ivmem2.h:$(GLSRC)gsnamecl.h
$(PSSRC)ivmem2.h:$(GLSRC)gxcspace.h
$(PSSRC)ivmem2.h:$(GLSRC)gscsel.h
$(PSSRC)ivmem2.h:$(GLSRC)gsdcolor.h
$(PSSRC)ivmem2.h:$(GLSRC)gxfrac.h
$(PSSRC)ivmem2.h:$(GLSRC)gscms.h
$(PSSRC)ivmem2.h:$(GLSRC)gsdevice.h
$(PSSRC)ivmem2.h:$(GLSRC)gscspace.h
$(PSSRC)ivmem2.h:$(GLSRC)gsgstate.h
$(PSSRC)ivmem2.h:$(GLSRC)gsiparam.h
$(PSSRC)ivmem2.h:$(GLSRC)gsmatrix.h
$(PSSRC)ivmem2.h:$(GLSRC)gxhttile.h
$(PSSRC)ivmem2.h:$(GLSRC)gsparam.h
$(PSSRC)ivmem2.h:$(GLSRC)gsrefct.h
$(PSSRC)ivmem2.h:$(GLSRC)memento.h
$(PSSRC)ivmem2.h:$(GLSRC)gsstruct.h
$(PSSRC)ivmem2.h:$(GLSRC)gxsync.h
$(PSSRC)ivmem2.h:$(GLSRC)gxbitmap.h
$(PSSRC)ivmem2.h:$(GLSRC)scommon.h
$(PSSRC)ivmem2.h:$(GLSRC)gsbitmap.h
$(PSSRC)ivmem2.h:$(GLSRC)gsccolor.h
$(PSSRC)ivmem2.h:$(GLSRC)gxarith.h
$(PSSRC)ivmem2.h:$(GLSRC)gpsync.h
$(PSSRC)ivmem2.h:$(GLSRC)gsstype.h
$(PSSRC)ivmem2.h:$(GLSRC)gsmemory.h
$(PSSRC)ivmem2.h:$(GLSRC)gslibctx.h
$(PSSRC)ivmem2.h:$(GLSRC)gsalloc.h
$(PSSRC)ivmem2.h:$(GLSRC)gxcindex.h
$(PSSRC)ivmem2.h:$(GLSRC)stdio_.h
$(PSSRC)ivmem2.h:$(GLSRC)stdint_.h
$(PSSRC)ivmem2.h:$(GLSRC)gssprintf.h
$(PSSRC)ivmem2.h:$(GLSRC)gstypes.h
$(PSSRC)ivmem2.h:$(GLSRC)std.h
$(PSSRC)ivmem2.h:$(GLSRC)stdpre.h
$(PSSRC)ivmem2.h:$(GLGEN)arch.h
$(PSSRC)ivmem2.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icid.h:$(PSSRC)iref.h
$(PSSRC)icid.h:$(GLSRC)gxalloc.h
$(PSSRC)icid.h:$(GLSRC)gxobj.h
$(PSSRC)icid.h:$(GLSRC)gsnamecl.h
$(PSSRC)icid.h:$(GLSRC)gxcspace.h
$(PSSRC)icid.h:$(GLSRC)gscsel.h
$(PSSRC)icid.h:$(GLSRC)gsdcolor.h
$(PSSRC)icid.h:$(GLSRC)gxfrac.h
$(PSSRC)icid.h:$(GLSRC)gscms.h
$(PSSRC)icid.h:$(GLSRC)gsdevice.h
$(PSSRC)icid.h:$(GLSRC)gscspace.h
$(PSSRC)icid.h:$(GLSRC)gsgstate.h
$(PSSRC)icid.h:$(GLSRC)gxcid.h
$(PSSRC)icid.h:$(GLSRC)gsiparam.h
$(PSSRC)icid.h:$(GLSRC)gsmatrix.h
$(PSSRC)icid.h:$(GLSRC)gxhttile.h
$(PSSRC)icid.h:$(GLSRC)gsparam.h
$(PSSRC)icid.h:$(GLSRC)gsrefct.h
$(PSSRC)icid.h:$(GLSRC)memento.h
$(PSSRC)icid.h:$(GLSRC)gsstruct.h
$(PSSRC)icid.h:$(GLSRC)gxsync.h
$(PSSRC)icid.h:$(GLSRC)gxbitmap.h
$(PSSRC)icid.h:$(GLSRC)scommon.h
$(PSSRC)icid.h:$(GLSRC)gsbitmap.h
$(PSSRC)icid.h:$(GLSRC)gsccolor.h
$(PSSRC)icid.h:$(GLSRC)gxarith.h
$(PSSRC)icid.h:$(GLSRC)gpsync.h
$(PSSRC)icid.h:$(GLSRC)gsstype.h
$(PSSRC)icid.h:$(GLSRC)gsmemory.h
$(PSSRC)icid.h:$(GLSRC)gslibctx.h
$(PSSRC)icid.h:$(GLSRC)gsalloc.h
$(PSSRC)icid.h:$(GLSRC)gxcindex.h
$(PSSRC)icid.h:$(GLSRC)stdio_.h
$(PSSRC)icid.h:$(GLSRC)stdint_.h
$(PSSRC)icid.h:$(GLSRC)gssprintf.h
$(PSSRC)icid.h:$(GLSRC)gstypes.h
$(PSSRC)icid.h:$(GLSRC)std.h
$(PSSRC)icid.h:$(GLSRC)stdpre.h
$(PSSRC)icid.h:$(GLGEN)arch.h
$(PSSRC)icid.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ifcid.h:$(GLSRC)gxfcid.h
$(PSSRC)ifcid.h:$(PSSRC)icid.h
$(PSSRC)ifcid.h:$(GLSRC)gstype1.h
$(PSSRC)ifcid.h:$(GLSRC)gxfont42.h
$(PSSRC)ifcid.h:$(GLSRC)gxfont.h
$(PSSRC)ifcid.h:$(PSSRC)iostack.h
$(PSSRC)ifcid.h:$(GLSRC)gspath.h
$(PSSRC)ifcid.h:$(PSSRC)istack.h
$(PSSRC)ifcid.h:$(GLSRC)gsgdata.h
$(PSSRC)ifcid.h:$(PSSRC)iosdata.h
$(PSSRC)ifcid.h:$(GLSRC)gxmatrix.h
$(PSSRC)ifcid.h:$(GLSRC)gxfapi.h
$(PSSRC)ifcid.h:$(PSSRC)isdata.h
$(PSSRC)ifcid.h:$(PSSRC)imemory.h
$(PSSRC)ifcid.h:$(GLSRC)gsfcmap.h
$(PSSRC)ifcid.h:$(PSSRC)ivmspace.h
$(PSSRC)ifcid.h:$(PSSRC)iref.h
$(PSSRC)ifcid.h:$(GLSRC)gsgc.h
$(PSSRC)ifcid.h:$(GLSRC)gxalloc.h
$(PSSRC)ifcid.h:$(GLSRC)gxobj.h
$(PSSRC)ifcid.h:$(GLSRC)gstext.h
$(PSSRC)ifcid.h:$(GLSRC)gsnamecl.h
$(PSSRC)ifcid.h:$(GLSRC)gxcspace.h
$(PSSRC)ifcid.h:$(GLSRC)gscsel.h
$(PSSRC)ifcid.h:$(GLSRC)gxfcache.h
$(PSSRC)ifcid.h:$(GLSRC)gsfont.h
$(PSSRC)ifcid.h:$(GLSRC)gsdcolor.h
$(PSSRC)ifcid.h:$(GLSRC)gxbcache.h
$(PSSRC)ifcid.h:$(GLSRC)gxpath.h
$(PSSRC)ifcid.h:$(GLSRC)gxfrac.h
$(PSSRC)ifcid.h:$(GLSRC)gxftype.h
$(PSSRC)ifcid.h:$(GLSRC)gscms.h
$(PSSRC)ifcid.h:$(GLSRC)gsrect.h
$(PSSRC)ifcid.h:$(GLSRC)gslparam.h
$(PSSRC)ifcid.h:$(GLSRC)gsdevice.h
$(PSSRC)ifcid.h:$(GLSRC)gscpm.h
$(PSSRC)ifcid.h:$(GLSRC)gsgcache.h
$(PSSRC)ifcid.h:$(GLSRC)gscspace.h
$(PSSRC)ifcid.h:$(GLSRC)gsgstate.h
$(PSSRC)ifcid.h:$(GLSRC)gsnotify.h
$(PSSRC)ifcid.h:$(GLSRC)gsxfont.h
$(PSSRC)ifcid.h:$(GLSRC)gxcid.h
$(PSSRC)ifcid.h:$(GLSRC)gsiparam.h
$(PSSRC)ifcid.h:$(GLSRC)gxfixed.h
$(PSSRC)ifcid.h:$(GLSRC)gsmatrix.h
$(PSSRC)ifcid.h:$(GLSRC)gspenum.h
$(PSSRC)ifcid.h:$(GLSRC)gxhttile.h
$(PSSRC)ifcid.h:$(GLSRC)gsparam.h
$(PSSRC)ifcid.h:$(GLSRC)gsrefct.h
$(PSSRC)ifcid.h:$(GLSRC)memento.h
$(PSSRC)ifcid.h:$(GLSRC)gsuid.h
$(PSSRC)ifcid.h:$(GLSRC)gsstruct.h
$(PSSRC)ifcid.h:$(GLSRC)gxsync.h
$(PSSRC)ifcid.h:$(GLSRC)gxbitmap.h
$(PSSRC)ifcid.h:$(GLSRC)scommon.h
$(PSSRC)ifcid.h:$(GLSRC)gsbitmap.h
$(PSSRC)ifcid.h:$(GLSRC)gsccolor.h
$(PSSRC)ifcid.h:$(GLSRC)gxarith.h
$(PSSRC)ifcid.h:$(GLSRC)gpsync.h
$(PSSRC)ifcid.h:$(GLSRC)gsstype.h
$(PSSRC)ifcid.h:$(GLSRC)gsmemory.h
$(PSSRC)ifcid.h:$(GLSRC)gslibctx.h
$(PSSRC)ifcid.h:$(GLSRC)gsalloc.h
$(PSSRC)ifcid.h:$(GLSRC)gxcindex.h
$(PSSRC)ifcid.h:$(GLSRC)stdio_.h
$(PSSRC)ifcid.h:$(GLSRC)gsccode.h
$(PSSRC)ifcid.h:$(GLSRC)stdint_.h
$(PSSRC)ifcid.h:$(GLSRC)gssprintf.h
$(PSSRC)ifcid.h:$(GLSRC)gstypes.h
$(PSSRC)ifcid.h:$(GLSRC)std.h
$(PSSRC)ifcid.h:$(GLSRC)stdpre.h
$(PSSRC)ifcid.h:$(GLGEN)arch.h
$(PSSRC)ifcid.h:$(GLSRC)gs_dll_call.h
$(PSSRC)icie.h:$(PSSRC)igstate.h
$(PSSRC)icie.h:$(GLSRC)gsstate.h
$(PSSRC)icie.h:$(GLSRC)gsovrc.h
$(PSSRC)icie.h:$(GLSRC)gscolor.h
$(PSSRC)icie.h:$(GLSRC)gsline.h
$(PSSRC)icie.h:$(GLSRC)gxcomp.h
$(PSSRC)icie.h:$(PSSRC)istruct.h
$(PSSRC)icie.h:$(PSSRC)imemory.h
$(PSSRC)icie.h:$(GLSRC)gsht.h
$(PSSRC)icie.h:$(PSSRC)ivmspace.h
$(PSSRC)icie.h:$(PSSRC)iref.h
$(PSSRC)icie.h:$(GLSRC)gsgc.h
$(PSSRC)icie.h:$(GLSRC)gscie.h
$(PSSRC)icie.h:$(GLSRC)gxalloc.h
$(PSSRC)icie.h:$(GLSRC)gxobj.h
$(PSSRC)icie.h:$(GLSRC)gxstate.h
$(PSSRC)icie.h:$(GLSRC)gsnamecl.h
$(PSSRC)icie.h:$(GLSRC)gxcspace.h
$(PSSRC)icie.h:$(GLSRC)gxctable.h
$(PSSRC)icie.h:$(GLSRC)gscsel.h
$(PSSRC)icie.h:$(GLSRC)gsdcolor.h
$(PSSRC)icie.h:$(GLSRC)gxfrac.h
$(PSSRC)icie.h:$(GLSRC)gxtmap.h
$(PSSRC)icie.h:$(GLSRC)gscms.h
$(PSSRC)icie.h:$(GLSRC)gslparam.h
$(PSSRC)icie.h:$(GLSRC)gsdevice.h
$(PSSRC)icie.h:$(GLSRC)gxbitfmt.h
$(PSSRC)icie.h:$(GLSRC)gscpm.h
$(PSSRC)icie.h:$(GLSRC)gscspace.h
$(PSSRC)icie.h:$(GLSRC)gsgstate.h
$(PSSRC)icie.h:$(GLSRC)gsiparam.h
$(PSSRC)icie.h:$(GLSRC)gxfixed.h
$(PSSRC)icie.h:$(GLSRC)gscompt.h
$(PSSRC)icie.h:$(GLSRC)gsmatrix.h
$(PSSRC)icie.h:$(GLSRC)gxhttile.h
$(PSSRC)icie.h:$(GLSRC)gsparam.h
$(PSSRC)icie.h:$(GLSRC)gsrefct.h
$(PSSRC)icie.h:$(GLSRC)memento.h
$(PSSRC)icie.h:$(GLSRC)gsstruct.h
$(PSSRC)icie.h:$(GLSRC)gxsync.h
$(PSSRC)icie.h:$(GLSRC)gxbitmap.h
$(PSSRC)icie.h:$(GLSRC)scommon.h
$(PSSRC)icie.h:$(GLSRC)gsbitmap.h
$(PSSRC)icie.h:$(GLSRC)gsccolor.h
$(PSSRC)icie.h:$(GLSRC)gxarith.h
$(PSSRC)icie.h:$(GLSRC)gpsync.h
$(PSSRC)icie.h:$(GLSRC)gsstype.h
$(PSSRC)icie.h:$(GLSRC)gsmemory.h
$(PSSRC)icie.h:$(GLSRC)gslibctx.h
$(PSSRC)icie.h:$(GLSRC)gsalloc.h
$(PSSRC)icie.h:$(GLSRC)gxcindex.h
$(PSSRC)icie.h:$(GLSRC)stdio_.h
$(PSSRC)icie.h:$(GLSRC)stdint_.h
$(PSSRC)icie.h:$(GLSRC)gssprintf.h
$(PSSRC)icie.h:$(GLSRC)gstypes.h
$(PSSRC)icie.h:$(GLSRC)std.h
$(PSSRC)icie.h:$(GLSRC)stdpre.h
$(PSSRC)icie.h:$(GLGEN)arch.h
$(PSSRC)icie.h:$(GLSRC)gs_dll_call.h
$(PSSRC)ipcolor.h:$(PSSRC)iref.h
$(PSSRC)ipcolor.h:$(GLSRC)gxalloc.h
$(PSSRC)ipcolor.h:$(GLSRC)gxobj.h
$(PSSRC)ipcolor.h:$(GLSRC)gsnamecl.h
$(PSSRC)ipcolor.h:$(GLSRC)gxcspace.h
$(PSSRC)ipcolor.h:$(GLSRC)gscsel.h
$(PSSRC)ipcolor.h:$(GLSRC)gsdcolor.h
$(PSSRC)ipcolor.h:$(GLSRC)gxfrac.h
$(PSSRC)ipcolor.h:$(GLSRC)gscms.h
$(PSSRC)ipcolor.h:$(GLSRC)gsdevice.h
$(PSSRC)ipcolor.h:$(GLSRC)gscspace.h
$(PSSRC)ipcolor.h:$(GLSRC)gsgstate.h
$(PSSRC)ipcolor.h:$(GLSRC)gsiparam.h
$(PSSRC)ipcolor.h:$(GLSRC)gsmatrix.h
$(PSSRC)ipcolor.h:$(GLSRC)gxhttile.h
$(PSSRC)ipcolor.h:$(GLSRC)gsparam.h
$(PSSRC)ipcolor.h:$(GLSRC)gsrefct.h
$(PSSRC)ipcolor.h:$(GLSRC)memento.h
$(PSSRC)ipcolor.h:$(GLSRC)gsstruct.h
$(PSSRC)ipcolor.h:$(GLSRC)gxsync.h
$(PSSRC)ipcolor.h:$(GLSRC)gxbitmap.h
$(PSSRC)ipcolor.h:$(GLSRC)scommon.h
$(PSSRC)ipcolor.h:$(GLSRC)gsbitmap.h
$(PSSRC)ipcolor.h:$(GLSRC)gsccolor.h
$(PSSRC)ipcolor.h:$(GLSRC)gxarith.h
$(PSSRC)ipcolor.h:$(GLSRC)gpsync.h
$(PSSRC)ipcolor.h:$(GLSRC)gsstype.h
$(PSSRC)ipcolor.h:$(GLSRC)gsmemory.h
$(PSSRC)ipcolor.h:$(GLSRC)gslibctx.h
$(PSSRC)ipcolor.h:$(GLSRC)gsalloc.h
$(PSSRC)ipcolor.h:$(GLSRC)gxcindex.h
$(PSSRC)ipcolor.h:$(GLSRC)stdio_.h
$(PSSRC)ipcolor.h:$(GLSRC)stdint_.h
$(PSSRC)ipcolor.h:$(GLSRC)gssprintf.h
$(PSSRC)ipcolor.h:$(GLSRC)gstypes.h
$(PSSRC)ipcolor.h:$(GLSRC)std.h
$(PSSRC)ipcolor.h:$(GLSRC)stdpre.h
$(PSSRC)ipcolor.h:$(GLGEN)arch.h
$(PSSRC)ipcolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsform1.h:$(GLSRC)gxpath.h
$(GLSRC)gsform1.h:$(GLSRC)gsrect.h
$(GLSRC)gsform1.h:$(GLSRC)gslparam.h
$(GLSRC)gsform1.h:$(GLSRC)gscpm.h
$(GLSRC)gsform1.h:$(GLSRC)gsgstate.h
$(GLSRC)gsform1.h:$(GLSRC)gxfixed.h
$(GLSRC)gsform1.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsform1.h:$(GLSRC)gspenum.h
$(GLSRC)gsform1.h:$(GLSRC)scommon.h
$(GLSRC)gsform1.h:$(GLSRC)gsstype.h
$(GLSRC)gsform1.h:$(GLSRC)gsmemory.h
$(GLSRC)gsform1.h:$(GLSRC)gslibctx.h
$(GLSRC)gsform1.h:$(GLSRC)stdio_.h
$(GLSRC)gsform1.h:$(GLSRC)stdint_.h
$(GLSRC)gsform1.h:$(GLSRC)gssprintf.h
$(GLSRC)gsform1.h:$(GLSRC)gstypes.h
$(GLSRC)gsform1.h:$(GLSRC)std.h
$(GLSRC)gsform1.h:$(GLSRC)stdpre.h
$(GLSRC)gsform1.h:$(GLGEN)arch.h
$(GLSRC)gsform1.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxline.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)math_.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxband.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsht.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscie.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gstext.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevdsp2.h:$(DEVSRC)gdevdsp.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscms.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsio.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gp.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)memento.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)memory_.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)srdline.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)scommon.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)stat_.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)std.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevdsp2.h:$(GLGEN)arch.h
$(DEVSRC)gdevdsp2.h:$(GLSRC)gs_dll_call.h
