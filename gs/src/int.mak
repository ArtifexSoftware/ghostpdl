#    Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

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
PSGEN=$(PSGENDIR)$(D)
PSOBJ=$(PSOBJDIR)$(D)
PSO_=$(O_)$(PSOBJ)
PSI_=$(PSSRCDIR) $(II)$(PSGENDIR) $(II)$(GLI_)
PSF_=
PSCC=$(CC_) $(I_)$(PSI_)$(_I) $(PSF_)
PSCCLEAF=$(CC_LEAF) $(I_)$(PSI_)$(_I) $(PSF_)

# Define the name of this makefile.
INT_MAK=$(PSSRC)int.mak

# ======================== Interpreter support ======================== #

# This is support code for all interpreters, not just PostScript and PDF.
# It knows about the PostScript data types, but isn't supposed to
# depend on anything outside itself.

errors_h=$(PSSRC)errors.h
idebug_h=$(PSSRC)idebug.h
idict_h=$(PSSRC)idict.h
idictdef_h=$(PSSRC)idictdef.h
igcstr_h=$(PSSRC)igcstr.h
inames_h=$(PSSRC)inames.h
iname_h=$(PSSRC)iname.h $(inames_h)
inamedef_h=$(PSSRC)inamedef.h $(gconfigv_h) $(gsstruct_h) $(inames_h)
ipacked_h=$(PSSRC)ipacked.h
iref_h=$(PSSRC)iref.h
isave_h=$(PSSRC)isave.h
isstate_h=$(PSSRC)isstate.h
istruct_h=$(PSSRC)istruct.h $(gsstruct_h)
iutil_h=$(PSSRC)iutil.h
ivmspace_h=$(PSSRC)ivmspace.h $(gsgc_h)
opdef_h=$(PSSRC)opdef.h
# Nested include files
ghost_h=$(PSSRC)ghost.h $(gx_h) $(iref_h)
igc_h=$(PSSRC)igc.h $(istruct_h)
imemory_h=$(PSSRC)imemory.h $(gsalloc_h) $(ivmspace_h)
ialloc_h=$(PSSRC)ialloc.h $(imemory_h)
iastruct_h=$(PSSRC)iastruct.h $(gxobj_h) $(ialloc_h)
iastate_h=$(PSSRC)iastate.h $(gxalloc_h) $(ialloc_h) $(istruct_h)
store_h=$(PSSRC)store.h $(ialloc_h)

GH=$(AK) $(ghost_h)

isupport1_=$(PSOBJ)ialloc.$(OBJ) $(PSOBJ)igc.$(OBJ) $(PSOBJ)igcref.$(OBJ) $(PSOBJ)igcstr.$(OBJ)
isupport2_=$(PSOBJ)ilocate.$(OBJ) $(PSOBJ)iname.$(OBJ) $(PSOBJ)isave.$(OBJ)
isupport_=$(isupport1_) $(isupport2_)
isupport.dev: $(INT_MAK) $(ECHOGS_XE) $(isupport_)
	$(SETMOD) isupport $(isupport1_)
	$(ADDMOD) isupport -obj $(isupport2_)

$(PSOBJ)ialloc.$(OBJ): $(PSSRC)ialloc.c $(AK) $(memory__h) $(gx_h)\
 $(errors_h) $(gsstruct_h) $(gxarith_h)\
 $(iastate_h) $(ipacked_h) $(iref_h) $(iutil_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)ialloc.$(OBJ) $(C_) $(PSSRC)ialloc.c

# igc.c, igcref.c, and igcstr.c should really be in the dpsand2 list,
# but since all the GC enumeration and relocation routines refer to them,
# it's too hard to separate them out from the Level 1 base.
$(PSOBJ)igc.$(OBJ): $(PSSRC)igc.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h) $(gsutil_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(inamedef_h)\
 $(ipacked_h) $(isave_h) $(isstate_h) $(istruct_h) $(opdef_h)
	$(PSCC) $(PSO_)igc.$(OBJ) $(C_) $(PSSRC)igc.c

$(PSOBJ)igcref.$(OBJ): $(PSSRC)igcref.c $(GH) $(memory__h)\
 $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idebug_h) $(igc_h) $(iname_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)igcref.$(OBJ) $(C_) $(PSSRC)igcref.c

$(PSOBJ)igcstr.$(OBJ): $(PSSRC)igcstr.c $(GH) $(memory__h)\
 $(gsmdebug_h) $(gsstruct_h) $(iastate_h) $(igcstr_h)
	$(PSCC) $(PSO_)igcstr.$(OBJ) $(C_) $(PSSRC)igcstr.c

$(PSOBJ)ilocate.$(OBJ): $(PSSRC)ilocate.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(iname_h)\
 $(ipacked_h) $(isstate_h) $(iutil_h) $(ivmspace_h)\
 $(store_h)
	$(PSCC) $(PSO_)ilocate.$(OBJ) $(C_) $(PSSRC)ilocate.c

$(PSOBJ)iname.$(OBJ): $(PSSRC)iname.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h) $(gxobj_h)\
 $(errors_h) $(imemory_h) $(inamedef_h) $(isave_h) $(store_h)
	$(PSCC) $(PSO_)iname.$(OBJ) $(C_) $(PSSRC)iname.c

$(PSOBJ)isave.$(OBJ): $(PSSRC)isave.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h) $(gsutil_h)\
 $(iastate_h) $(iname_h) $(inamedef_h) $(isave_h) $(isstate_h) $(ivmspace_h)\
 $(ipacked_h) $(store_h) $(stream_h)
	$(PSCC) $(PSO_)isave.$(OBJ) $(C_) $(PSSRC)isave.c

### Include files

idparam_h=$(PSSRC)idparam.h
ilevel_h=$(PSSRC)ilevel.h
iparam_h=$(PSSRC)iparam.h $(gsparam_h)
istack_h=$(PSSRC)istack.h
iutil2_h=$(PSSRC)iutil2.h
opcheck_h=$(PSSRC)opcheck.h
opextern_h=$(PSSRC)opextern.h
# Nested include files
idstack_h=$(PSSRC)idstack.h $(istack_h)
dstack_h=$(PSSRC)dstack.h $(idstack_h)
iestack_h=$(PSSRC)iestack.h $(istack_h)
estack_h=$(PSSRC)estack.h $(iestack_h)
iostack_h=$(PSSRC)iostack.h $(istack_h)
ostack_h=$(PSSRC)ostack.h $(iostack_h)
oper_h=$(PSSRC)oper.h $(errors_h) $(iutil_h) $(opcheck_h) $(opdef_h) $(opextern_h) $(ostack_h)

$(PSOBJ)idebug.$(OBJ): $(PSSRC)idebug.c $(GH) $(string__h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opdef_h) $(ipacked_h)
	$(PSCC) $(PSO_)idebug.$(OBJ) $(C_) $(PSSRC)idebug.c

$(PSOBJ)idict.$(OBJ): $(PSSRC)idict.c $(GH) $(string__h) $(errors_h)\
 $(idebug_h) $(idict_h) $(idictdef_h) $(idstack_h) $(imemory_h) $(iname_h)\
 $(inamedef_h) $(ipacked_h) $(isave_h) $(iutil_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)idict.$(OBJ) $(C_) $(PSSRC)idict.c

$(PSOBJ)idparam.$(OBJ): $(PSSRC)idparam.c $(GH) $(memory__h) $(string__h) $(errors_h)\
 $(gsmatrix_h) $(gsuid_h)\
 $(idict_h) $(idparam_h) $(ilevel_h) $(imemory_h) $(iname_h) $(iutil_h)\
 $(oper_h) $(store_h)
	$(PSCC) $(PSO_)idparam.$(OBJ) $(C_) $(PSSRC)idparam.c

$(PSOBJ)idstack.$(OBJ): $(PSSRC)idstack.c $(GH)\
 $(idebug_h) $(idict_h) $(idictdef_h) $(idstack_h) $(iname_h) $(inamedef_h)\
 $(ipacked_h) $(iutil_h) $(ivmspace_h)
	$(PSCC) $(PSO_)idstack.$(OBJ) $(C_) $(PSSRC)idstack.c

$(PSOBJ)iparam.$(OBJ): $(PSSRC)iparam.c $(GH)\
 $(memory__h) $(string__h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(iname_h) $(imemory_h) $(iparam_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opcheck_h) $(oper_h) $(store_h)
	$(PSCC) $(PSO_)iparam.$(OBJ) $(C_) $(PSSRC)iparam.c

$(PSOBJ)istack.$(OBJ): $(PSSRC)istack.c $(GH) $(memory__h)\
 $(errors_h) $(gsstruct_h) $(gsutil_h)\
 $(ialloc_h) $(istack_h) $(istruct_h) $(iutil_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)istack.$(OBJ) $(C_) $(PSSRC)istack.c

$(PSOBJ)iutil.$(OBJ): $(PSSRC)iutil.c $(GH) $(math__h) $(memory__h) $(string__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h)\
 $(errors_h) $(idict_h) $(imemory_h) $(iutil_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(oper_h) $(store_h)
	$(PSCC) $(PSO_)iutil.$(OBJ) $(C_) $(PSSRC)iutil.c

# ======================== PostScript Level 1 ======================== #

###### Include files

files_h=$(PSSRC)files.h
fname_h=$(PSSRC)fname.h
ichar_h=$(PSSRC)ichar.h
icharout_h=$(PSSRC)icharout.h
icolor_h=$(PSSRC)icolor.h
icsmap_h=$(PSSRC)icsmap.h
icstate_h=$(PSSRC)icstate.h $(imemory_h)
ifont_h=$(PSSRC)ifont.h $(gsccode_h) $(gsstruct_h)
iht_h=$(PSSRC)iht.h
iimage_h=$(PSSRC)iimage.h
imain_h=$(PSSRC)imain.h $(gsexit_h)
imainarg_h=$(PSSRC)imainarg.h
iminst_h=$(PSSRC)iminst.h
interp_h=$(PSSRC)interp.h
iparray_h=$(PSSRC)iparray.h
iscannum_h=$(PSSRC)iscannum.h
istream_h=$(PSSRC)istream.h
main_h=$(PSSRC)main.h $(imain_h) $(iminst_h)
sbwbs_h=$(PSSRC)sbwbs.h
sfilter_h=$(PSSRC)sfilter.h $(gstypes_h)
shcgen_h=$(PSSRC)shcgen.h
smtf_h=$(PSSRC)smtf.h
# Nested include files
bfont_h=$(PSSRC)bfont.h $(ifont_h)
icontext_h=$(PSSRC)icontext.h $(gsstruct_h) $(icstate_h)
ifilter_h=$(PSSRC)ifilter.h $(istream_h) $(ivmspace_h)
igstate_h=$(PSSRC)igstate.h $(gsstate_h) $(gxstate_h) $(istruct_h)
iscan_h=$(PSSRC)iscan.h $(sa85x_h) $(sstring_h)
sbhc_h=$(PSSRC)sbhc.h $(shc_h)
# Include files for optional features
ibnum_h=$(PSSRC)ibnum.h

### Initialization and scanning

iconfig=iconfig$(CONFIG)
$(PSOBJ)$(iconfig).$(OBJ): $(PSSRC)iconf.c $(stdio__h)\
 $(gconf_h) $(gsmemory_h) $(gstypes_h)\
 $(iminst_h) $(iref_h) $(ivmspace_h) $(opdef_h)
	$(RM_) $(PSGEN)$(iconfig).c
	$(CP_) $(gconfig_h) $(PSGEN)gconfig.h
	$(CP_) $(PSSRC)iconf.c $(PSGEN)$(iconfig).c
	$(PSCC) $(PSO_)$(iconfig).$(OBJ) $(C_) $(PSGEN)$(iconfig).c

$(PSOBJ)iinit.$(OBJ): $(PSSRC)iinit.c $(GH) $(string__h)\
 $(gscdefs_h) $(gsexit_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h)\
 $(ilevel_h) $(iname_h) $(interp_h) $(opdef_h)\
 $(ipacked_h) $(iparray_h) $(iutil_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)iinit.$(OBJ) $(C_) $(PSSRC)iinit.c

$(PSOBJ)iscan.$(OBJ): $(PSSRC)iscan.c $(GH) $(memory__h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h) $(files_h)\
 $(ilevel_h) $(iutil_h) $(iscan_h) $(iscannum_h) $(istruct_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(iparray_h) $(istream_h) $(ostack_h) $(store_h)\
 $(stream_h) $(strimpl_h) $(sfilter_h) $(scanchar_h)
	$(PSCC) $(PSO_)iscan.$(OBJ) $(C_) $(PSSRC)iscan.c

$(PSOBJ)iscannum.$(OBJ): $(PSSRC)iscannum.c $(GH) $(math__h)\
 $(errors_h) $(iscannum_h) $(scanchar_h) $(scommon_h) $(store_h)
	$(PSCC) $(PSO_)iscannum.$(OBJ) $(C_) $(PSSRC)iscannum.c

### Streams

$(PSOBJ)sfilter1.$(OBJ): $(PSSRC)sfilter1.c $(AK) $(stdio__h) $(memory__h)\
 $(sfilter_h) $(strimpl_h)
	$(PSCC) $(PSO_)sfilter1.$(OBJ) $(C_) $(PSSRC)sfilter1.c

###### Operators

OP=$(GH) $(oper_h)

### Non-graphics operators

$(PSOBJ)zarith.$(OBJ): $(PSSRC)zarith.c $(OP) $(math__h) $(store_h)
	$(PSCC) $(PSO_)zarith.$(OBJ) $(C_) $(PSSRC)zarith.c

$(PSOBJ)zarray.$(OBJ): $(PSSRC)zarray.c $(OP) $(memory__h)\
 $(ialloc_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)zarray.$(OBJ) $(C_) $(PSSRC)zarray.c

$(PSOBJ)zcontrol.$(OBJ): $(PSSRC)zcontrol.c $(OP) $(string__h)\
 $(estack_h) $(files_h) $(ipacked_h) $(iutil_h) $(store_h) $(stream_h)
	$(PSCC) $(PSO_)zcontrol.$(OBJ) $(C_) $(PSSRC)zcontrol.c

$(PSOBJ)zdict.$(OBJ): $(PSSRC)zdict.c $(OP)\
 $(dstack_h) $(idict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(ivmspace_h)\
 $(store_h)
	$(PSCC) $(PSO_)zdict.$(OBJ) $(C_) $(PSSRC)zdict.c

$(PSOBJ)zfile.$(OBJ): $(PSSRC)zfile.c $(OP) $(memory__h) $(string__h) $(gp_h)\
 $(gsstruct_h) $(gxalloc_h) $(gxiodev_h)\
 $(ialloc_h) $(estack_h) $(files_h) $(fname_h) $(ilevel_h) $(interp_h) $(iutil_h)\
 $(isave_h) $(main_h) $(sfilter_h) $(stream_h) $(strimpl_h) $(store_h)
	$(PSCC) $(PSO_)zfile.$(OBJ) $(C_) $(PSSRC)zfile.c

$(PSOBJ)zfileio.$(OBJ): $(PSSRC)zfileio.c $(OP) $(gp_h)\
 $(files_h) $(ifilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(gsmatrix_h) $(gxdevice_h) $(gxdevmem_h)
	$(PSCC) $(PSO_)zfileio.$(OBJ) $(C_) $(PSSRC)zfileio.c

$(PSOBJ)zfilter.$(OBJ): $(PSSRC)zfilter.c $(OP) $(memory__h)\
 $(gsstruct_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(ilevel_h)\
 $(sfilter_h) $(srlx_h) $(sstring_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfilter.$(OBJ) $(C_) $(PSSRC)zfilter.c

$(PSOBJ)zfname.$(OBJ): $(PSSRC)zfname.c $(OP) $(memory__h)\
 $(fname_h) $(gxiodev_h) $(ialloc_h) $(stream_h)
	$(PSCC) $(PSO_)zfname.$(OBJ) $(C_) $(PSSRC)zfname.c

$(PSOBJ)zfproc.$(OBJ): $(PSSRC)zfproc.c $(GH) $(memory__h)\
 $(oper_h)\
 $(estack_h) $(files_h) $(gsstruct_h) $(ialloc_h) $(ifilter_h) $(istruct_h)\
 $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfproc.$(OBJ) $(C_) $(PSSRC)zfproc.c

$(PSOBJ)zgeneric.$(OBJ): $(PSSRC)zgeneric.c $(OP) $(memory__h)\
 $(idict_h) $(estack_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)zgeneric.$(OBJ) $(C_) $(PSSRC)zgeneric.c

$(PSOBJ)ziodev.$(OBJ): $(PSSRC)ziodev.c $(OP)\
 $(memory__h) $(stdio__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsstruct_h) $(gxiodev_h)\
 $(files_h) $(ialloc_h) $(iscan_h) $(ivmspace_h)\
 $(scanchar_h) $(store_h) $(stream_h)
	$(PSCC) $(PSO_)ziodev.$(OBJ) $(C_) $(PSSRC)ziodev.c

$(PSOBJ)zmath.$(OBJ): $(PSSRC)zmath.c $(OP) $(math__h) $(gxfarith_h) $(store_h)
	$(PSCC) $(PSO_)zmath.$(OBJ) $(C_) $(PSSRC)zmath.c

$(PSOBJ)zmisc.$(OBJ): $(PSSRC)zmisc.c $(OP) $(gscdefs_h) $(gp_h)\
 $(errno__h) $(memory__h) $(string__h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(iname_h) $(ivmspace_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)zmisc.$(OBJ) $(C_) $(PSSRC)zmisc.c

$(PSOBJ)zpacked.$(OBJ): $(PSSRC)zpacked.c $(OP)\
 $(ialloc_h) $(idict_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(iparray_h)\
 $(istack_h) $(store_h)
	$(PSCC) $(PSO_)zpacked.$(OBJ) $(C_) $(PSSRC)zpacked.c

$(PSOBJ)zrelbit.$(OBJ): $(PSSRC)zrelbit.c $(OP)\
 $(gsutil_h) $(store_h) $(idict_h)
	$(PSCC) $(PSO_)zrelbit.$(OBJ) $(C_) $(PSSRC)zrelbit.c

$(PSOBJ)zstack.$(OBJ): $(PSSRC)zstack.c $(OP) $(memory__h)\
 $(ialloc_h) $(istack_h) $(store_h)
	$(PSCC) $(PSO_)zstack.$(OBJ) $(C_) $(PSSRC)zstack.c

$(PSOBJ)zstring.$(OBJ): $(PSSRC)zstring.c $(OP) $(memory__h)\
 $(gsutil_h)\
 $(ialloc_h) $(iname_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zstring.$(OBJ) $(C_) $(PSSRC)zstring.c

$(PSOBJ)zsysvm.$(OBJ): $(PSSRC)zsysvm.c $(GH)\
 $(ialloc_h) $(ivmspace_h) $(oper_h) $(store_h)
	$(PSCC) $(PSO_)zsysvm.$(OBJ) $(C_) $(PSSRC)zsysvm.c

$(PSOBJ)ztoken.$(OBJ): $(PSSRC)ztoken.c $(OP)\
 $(estack_h) $(files_h) $(gsstruct_h) $(iscan_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)ztoken.$(OBJ) $(C_) $(PSSRC)ztoken.c

$(PSOBJ)ztype.$(OBJ): $(PSSRC)ztype.c $(OP)\
 $(math__h) $(memory__h) $(string__h)\
 $(gsexit_h)\
 $(dstack_h) $(idict_h) $(imemory_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)ztype.$(OBJ) $(C_) $(PSSRC)ztype.c

$(PSOBJ)zvmem.$(OBJ): $(PSSRC)zvmem.c $(OP)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(isave_h) $(store_h) $(stream_h)\
 $(gsmalloc_h) $(gsmatrix_h) $(gsstate_h) $(gsstruct_h)
	$(PSCC) $(PSO_)zvmem.$(OBJ) $(C_) $(PSSRC)zvmem.c

### Graphics operators

$(PSOBJ)zchar.$(OBJ): $(PSSRC)zchar.c $(OP)\
 $(gsstruct_h) $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxchar_h) $(gxdevice_h) $(gxfont_h) $(gzpath_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ialloc_h) $(ichar_h) $(idict_h) $(ifont_h)\
 $(ilevel_h) $(iname_h) $(igstate_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)zchar.$(OBJ) $(C_) $(PSSRC)zchar.c

# zcharout is used for Type 1 and Type 42 fonts only.
$(PSOBJ)zcharout.$(OBJ): $(PSSRC)zcharout.c $(OP)\
 $(gschar_h) $(gxdevice_h) $(gxfont_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zcharout.$(OBJ) $(C_) $(PSSRC)zcharout.c

$(PSOBJ)zcolor.$(OBJ): $(PSSRC)zcolor.c $(OP)\
 $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gxdevice_h) $(gxcmap_h)\
 $(ialloc_h) $(icolor_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zcolor.$(OBJ) $(C_) $(PSSRC)zcolor.c

$(PSOBJ)zdevice.$(OBJ): $(PSSRC)zdevice.c $(OP) $(string__h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(interp_h) $(iparam_h) $(ivmspace_h)\
 $(gsmatrix_h) $(gsstate_h) $(gxdevice_h) $(gxgetbit_h) $(store_h)
	$(PSCC) $(PSO_)zdevice.$(OBJ) $(C_) $(PSSRC)zdevice.c

$(PSOBJ)zfont.$(OBJ): $(PSSRC)zfont.c $(OP)\
 $(gschar_h) $(gsstruct_h) $(gxdevice_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(isave_h) $(ivmspace_h)\
 $(bfont_h) $(store_h)
	$(PSCC) $(PSO_)zfont.$(OBJ) $(C_) $(PSSRC)zfont.c

$(PSOBJ)zfont2.$(OBJ): $(PSSRC)zfont2.c $(OP) $(memory__h) $(string__h)\
 $(gsmatrix_h) $(gxdevice_h) $(gschar_h) $(gxfixed_h) $(gxfont_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ilevel_h)\
 $(iname_h) $(interp_h) $(istruct_h) $(ipacked_h) $(store_h)
	$(PSCC) $(PSO_)zfont2.$(OBJ) $(C_) $(PSSRC)zfont2.c

$(PSOBJ)zgstate.$(OBJ): $(PSSRC)zgstate.c $(OP) $(math__h)\
 $(gsmatrix_h) $(ialloc_h) $(idict_h) $(igstate_h) $(istruct_h) $(store_h)
	$(PSCC) $(PSO_)zgstate.$(OBJ) $(C_) $(PSSRC)zgstate.c

$(PSOBJ)zht.$(OBJ): $(PSSRC)zht.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)
	$(PSCC) $(PSO_)zht.$(OBJ) $(C_) $(PSSRC)zht.c

$(PSOBJ)zimage.$(OBJ): $(PSSRC)zimage.c $(OP)\
 $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h) $(gxiparam_h)\
 $(estack_h) $(ialloc_h) $(ifilter_h) $(igstate_h) $(iimage_h) $(ilevel_h)\
 $(store_h) $(stream_h)
	$(PSCC) $(PSO_)zimage.$(OBJ) $(C_) $(PSSRC)zimage.c

$(PSOBJ)zmatrix.$(OBJ): $(PSSRC)zmatrix.c $(OP)\
 $(gsmatrix_h) $(igstate_h) $(gscoord_h) $(store_h)
	$(PSCC) $(PSO_)zmatrix.$(OBJ) $(C_) $(PSSRC)zmatrix.c

$(PSOBJ)zpaint.$(OBJ): $(PSSRC)zpaint.c $(OP)\
 $(gspaint_h) $(igstate_h)
	$(PSCC) $(PSO_)zpaint.$(OBJ) $(C_) $(PSSRC)zpaint.c

$(PSOBJ)zpath.$(OBJ): $(PSSRC)zpath.c $(OP) $(math__h)\
 $(gsmatrix_h) $(gspath_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zpath.$(OBJ) $(C_) $(PSSRC)zpath.c

# Define the base PostScript language interpreter.
# This is the subset of PostScript Level 1 required by our PDF reader.

INT1=$(PSOBJ)icontext.$(OBJ) $(PSOBJ)idebug.$(OBJ) $(PSOBJ)idict.$(OBJ)
INT2=$(PSOBJ)idparam.$(OBJ) $(PSOBJ)idstack.$(OBJ) $(PSOBJ)iinit.$(OBJ)
INT3=$(PSOBJ)interp.$(OBJ)
INT4=$(PSOBJ)iparam.$(OBJ) $(PSOBJ)ireclaim.$(OBJ)
INT5=$(PSOBJ)iscan.$(OBJ) $(PSOBJ)iscannum.$(OBJ) $(PSOBJ)istack.$(OBJ)
INT6=$(PSOBJ)iutil.$(OBJ) $(GLOBJ)scantab.$(OBJ) $(PSOBJ)sfilter1.$(OBJ)
INT7=$(GLOBJ)sstring.$(OBJ) $(GLOBJ)stream.$(OBJ)
Z1=$(PSOBJ)zarith.$(OBJ) $(PSOBJ)zarray.$(OBJ) $(PSOBJ)zcontrol.$(OBJ)
Z2=$(PSOBJ)zdict.$(OBJ) $(PSOBJ)zfile.$(OBJ) $(PSOBJ)zfileio.$(OBJ)
Z3=$(PSOBJ)zfilter.$(OBJ) $(PSOBJ)zfname.$(OBJ) $(PSOBJ)zfproc.$(OBJ)
Z4=$(PSOBJ)zgeneric.$(OBJ) $(PSOBJ)ziodev.$(OBJ) $(PSOBJ)zmath.$(OBJ)
Z5=$(PSOBJ)zmisc.$(OBJ) $(PSOBJ)zpacked.$(OBJ) $(PSOBJ)zrelbit.$(OBJ)
Z6=$(PSOBJ)zstack.$(OBJ) $(PSOBJ)zstring.$(OBJ) $(PSOBJ)zsysvm.$(OBJ)
Z7=$(PSOBJ)ztoken.$(OBJ) $(PSOBJ)ztype.$(OBJ) $(PSOBJ)zvmem.$(OBJ)
Z8=$(PSOBJ)zchar.$(OBJ) $(PSOBJ)zcolor.$(OBJ) $(PSOBJ)zdevice.$(OBJ)
Z9=$(PSOBJ)zfont.$(OBJ) $(PSOBJ)zfont2.$(OBJ) $(PSOBJ)zgstate.$(OBJ)
Z10=$(PSOBJ)zht.$(OBJ) $(PSOBJ)zimage.$(OBJ) $(PSOBJ)zmatrix.$(OBJ)
Z11=$(PSOBJ)zpaint.$(OBJ) $(PSOBJ)zpath.$(OBJ)
Z1_2OPS=zarith zarray zcontrol zdict zfile zfileio
Z3_4OPS=zfilter zfproc zgeneric ziodev zmath
Z5_6OPS=zmisc zpacked zrelbit zstack zstring zsysvm
Z7_8OPS=ztoken ztype zvmem zchar zcolor zdevice
Z9_10OPS=zfont zfont2 zgstate zht zimage zmatrix
Z11OPS=zpaint zpath
# We have to be a little underhanded with *config.$(OBJ) so as to avoid
# circular definitions.
INT_MAIN=$(PSOBJ)imain.$(OBJ) $(PSOBJ)imainarg.$(OBJ) $(GLOBJ)gsargs.$(OBJ)
INT_OBJS=$(INT_MAIN)\
 $(INT1) $(INT2) $(INT3) $(INT4) $(INT5) $(INT6) $(INT7)\
 $(Z1) $(Z2) $(Z3) $(Z4) $(Z5) $(Z6) $(Z7) $(Z8) $(Z9) $(Z10) $(Z11)
INT_CONFIG=$(GLOBJ)$(gconfig).$(OBJ) $(GLOBJ)$(gscdefs).$(OBJ)\
 $(PSOBJ)$(iconfig).$(OBJ) $(PSOBJ)iccinit$(COMPILE_INITS).$(OBJ)
INT_ALL=$(INT_OBJS) $(INT_CONFIG)
# We omit libcore.dev, which should be included here, because problems
# with the Unix linker require libcore to appear last in the link list
# when libcore is really a library.
# We omit $(INT_CONFIG) from the dependency list because they have special
# dependency requirements and are added to the link list at the very end.
# zfilter.c shouldn't include the RLE and RLD filters, but we don't want to
# change this now.
psbase.dev: $(INT_MAK) $(ECHOGS_XE) $(INT_OBJS)\
 isupport.dev nousparm.dev rld.dev rle.dev sfile.dev
	$(SETMOD) psbase $(INT_MAIN)
	$(ADDMOD) psbase -obj $(INT_CONFIG)
	$(ADDMOD) psbase -obj $(INT1)
	$(ADDMOD) psbase -obj $(INT2)
	$(ADDMOD) psbase -obj $(INT3)
	$(ADDMOD) psbase -obj $(INT4)
	$(ADDMOD) psbase -obj $(INT5)
	$(ADDMOD) psbase -obj $(INT6)
	$(ADDMOD) psbase -obj $(INT7)
	$(ADDMOD) psbase -obj $(Z1)
	$(ADDMOD) psbase -obj $(Z2)
	$(ADDMOD) psbase -obj $(Z3)
	$(ADDMOD) psbase -obj $(Z4)
	$(ADDMOD) psbase -obj $(Z5)
	$(ADDMOD) psbase -obj $(Z6)
	$(ADDMOD) psbase -obj $(Z7)
	$(ADDMOD) psbase -obj $(Z8)
	$(ADDMOD) psbase -obj $(Z9)
	$(ADDMOD) psbase -obj $(Z10)
	$(ADDMOD) psbase -obj $(Z11)
	$(ADDMOD) psbase -oper $(Z1_2OPS)
	$(ADDMOD) psbase -oper $(Z3_4OPS)
	$(ADDMOD) psbase -oper $(Z5_6OPS)
	$(ADDMOD) psbase -oper $(Z7_8OPS)
	$(ADDMOD) psbase -oper $(Z9_10OPS)
	$(ADDMOD) psbase -oper $(Z11OPS)
	$(ADDMOD) psbase -iodev stdin stdout stderr lineedit statementedit
	$(ADDMOD) psbase -include isupport nousparm rld rle sfile

# -------------------------- Feature definitions -------------------------- #

# ---------------- Full Level 1 interpreter ---------------- #

# We keep the old name for backward compatibility.
level1.dev: psl1.dev
	$(CP_) psl1.dev level1.dev

psl1.dev: $(INT_MAK) $(ECHOGS_XE) psbase.dev bcp.dev hsb.dev path1.dev type1.dev
	$(SETMOD) psl1 -include psbase bcp hsb path1 type1
	$(ADDMOD) psl1 -emulator PostScript PostScriptLevel1

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

color.dev: $(INT_MAK) $(ECHOGS_XE) cmyklib.dev colimlib.dev cmykread.dev
	$(SETMOD) color -include cmyklib colimlib cmykread

cmykread_=$(PSOBJ)zcolor1.$(OBJ) $(PSOBJ)zht1.$(OBJ)
cmykread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmykread_)
	$(SETMOD) cmykread $(cmykread_)
	$(ADDMOD) cmykread -oper zcolor1 zht1

$(PSOBJ)zcolor1.$(OBJ): $(PSSRC)zcolor1.c $(OP)\
 $(gscolor1_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(ialloc_h) $(icolor_h) $(iimage_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zcolor1.$(OBJ) $(C_) $(PSSRC)zcolor1.c

$(PSOBJ)zht1.$(OBJ): $(PSSRC)zht1.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)
	$(PSCC) $(PSO_)zht1.$(OBJ) $(C_) $(PSSRC)zht1.c

# ---------------- HSB color ---------------- #

hsb_=$(PSOBJ)zhsb.$(OBJ)
hsb.dev: $(INT_MAK) $(ECHOGS_XE) $(hsb_) hsblib.dev
	$(SETMOD) hsb $(hsb_)
	$(ADDMOD) hsb -include hsblib
	$(ADDMOD) hsb -oper zhsb

$(PSOBJ)zhsb.$(OBJ): $(PSSRC)zhsb.c $(OP)\
 $(gshsb_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zhsb.$(OBJ) $(C_) $(PSSRC)zhsb.c

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1_=$(PSOBJ)zpath1.$(OBJ)
path1.dev: $(INT_MAK) $(ECHOGS_XE) $(path1_) path1lib.dev
	$(SETMOD) path1 $(path1_)
	$(ADDMOD) path1 -include path1lib
	$(ADDMOD) path1 -oper zpath1

$(PSOBJ)zpath1.$(OBJ): $(PSSRC)zpath1.c $(OP) $(memory__h)\
 $(ialloc_h) $(estack_h) $(gspath_h) $(gsstruct_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zpath1.$(OBJ) $(C_) $(PSSRC)zpath1.c

# ================ Level-independent PostScript options ================ #

# ---------------- BCP filters ---------------- #

bcp_=$(PSOBJ)sbcp.$(OBJ) $(PSOBJ)zfbcp.$(OBJ)
bcp.dev: $(INT_MAK) $(ECHOGS_XE) $(bcp_)
	$(SETMOD) bcp $(bcp_)
	$(ADDMOD) bcp -oper zfbcp

$(PSOBJ)sbcp.$(OBJ): $(PSSRC)sbcp.c $(AK) $(stdio__h)\
 $(sfilter_h) $(strimpl_h)
	$(PSCC) $(PSO_)sbcp.$(OBJ) $(C_) $(PSSRC)sbcp.c

$(PSOBJ)zfbcp.$(OBJ): $(PSSRC)zfbcp.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfbcp.$(OBJ) $(C_) $(PSSRC)zfbcp.c

# ---------------- Incremental font loading ---------------- #
# (This only works for Type 1 fonts without eexec encryption.)

diskfont.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) diskfont -ps gs_diskf

# ---------------- Double-precision floats ---------------- #

double_=$(PSOBJ)zdouble.$(OBJ)
double.dev: $(INT_MAK) $(ECHOGS_XE) $(double_)
	$(SETMOD) double $(double_)
	$(ADDMOD) double -oper zdouble

$(PSOBJ)zdouble.$(OBJ): $(PSSRC)zdouble.c $(OP)\
 $(ctype__h) $(math__h) $(memory__h) $(string__h)\
 $(gxfarith_h) $(store_h)
	$(PSCC) $(PSO_)zdouble.$(OBJ) $(C_) $(PSSRC)zdouble.c

# ---------------- EPSF files with binary headers ---------------- #

epsf.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) epsf -ps gs_epsf

# ---------------- RasterOp ---------------- #
# This should be a separable feature in the core also....

rasterop.dev: $(INT_MAK) $(ECHOGS_XE) roplib.dev ropread.dev
	$(SETMOD) rasterop -include roplib ropread

ropread_=$(PSOBJ)zrop.$(OBJ)
ropread.dev: $(INT_MAK) $(ECHOGS_XE) $(ropread_)
	$(SETMOD) ropread $(ropread_)
	$(ADDMOD) ropread -oper zrop

$(PSOBJ)zrop.$(OBJ): $(PSSRC)zrop.c $(OP) $(memory__h)\
 $(gsrop_h) $(gsutil_h) $(gxdevice_h)\
 $(idict_h) $(idparam_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zrop.$(OBJ) $(C_) $(PSSRC)zrop.c

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1.dev: $(INT_MAK) $(ECHOGS_XE) psf1lib.dev psf1read.dev
	$(SETMOD) type1 -include psf1lib psf1read

psf1read_1=$(PSOBJ)seexec.$(OBJ) $(PSOBJ)zchar1.$(OBJ) $(PSOBJ)zcharout.$(OBJ)
psf1read_2=$(PSOBJ)zfont1.$(OBJ) $(PSOBJ)zmisc1.$(OBJ)
psf1read_=$(psf1read_1) $(psf1read_2)
psf1read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf1read_)
	$(SETMOD) psf1read $(psf1read_1)
	$(ADDMOD) psf1read -obj $(psf1read_2)
	$(ADDMOD) psf1read -oper zchar1 zfont1 zmisc1
	$(ADDMOD) psf1read -ps gs_type1

$(PSOBJ)seexec.$(OBJ): $(PSSRC)seexec.c $(AK) $(stdio__h)\
 $(gscrypt1_h) $(scanchar_h) $(sfilter_h) $(strimpl_h)
	$(PSCC) $(PSO_)seexec.$(OBJ) $(C_) $(PSSRC)seexec.c

$(PSOBJ)zchar1.$(OBJ): $(PSSRC)zchar1.c $(OP)\
 $(gspaint_h) $(gspath_h) $(gsstruct_h)\
 $(gxchar_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gzstate_h)\
 $(estack_h) $(ialloc_h) $(ichar_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zchar1.$(OBJ) $(C_) $(PSSRC)zchar1.c

# The last line of dependencies is only for a debugging operator.
$(PSOBJ)zfont1.$(OBJ): $(PSSRC)zfont1.c $(OP)\
 $(gsmatrix_h) $(gxdevice_h) $(gschar_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont1_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(store_h)\
  $(files_h) $(igstate_h) $(stream_h)
	$(PSCC) $(PSO_)zfont1.$(OBJ) $(C_) $(PSSRC)zfont1.c

$(PSOBJ)zmisc1.$(OBJ): $(PSSRC)zmisc1.c $(OP) $(memory__h)\
 $(gscrypt1_h)\
 $(idict_h) $(idparam_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)zmisc1.$(OBJ) $(C_) $(PSSRC)zmisc1.c

# -------------- Compact Font Format and Type 2 charstrings ------------- #

cff.dev: $(INT_MAK) $(ECHOGS_XE) $(PSSRC)gs_cff.ps psl2int.dev
	$(SETMOD) cff -ps gs_cff

type2.dev: $(INT_MAK) $(ECHOGS_XE) type1.dev psf2lib.dev
	$(SETMOD) type2 -include psf2lib

# ---------------- Type 32 (downloaded bitmap) fonts ---------------- #

$(PSOBJ)zchar32.$(OBJ): $(PSSRC)zchar32.c $(OP)\
 $(gsccode_h) $(gscoord_h) $(gsutil_h)\
 $(gxchar_h) $(gxdevice_h) $(gxdevmem_h) $(gxfcache_h) $(gxfont_h)\
 $(ifont_h) $(igstate_h)
	$(PSCC) $(PSO_)zchar32.$(OBJ) $(C_) $(PSSRC)zchar32.c

$(PSOBJ)zfont32.$(OBJ): $(PSSRC)zfont32.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h)\
 $(bfont_h) $(store_h)
	$(PSCC) $(PSO_)zfont32.$(OBJ) $(C_) $(PSSRC)zfont32.c

type32_=$(PSOBJ)zchar32.$(OBJ) $(PSOBJ)zfont32.$(OBJ)
type32.dev: $(INT_MAK) $(ECHOGS_XE) $(type32_)
	$(SETMOD) type32 $(type32_)
	$(ADDMOD) type32 -oper zchar32 zfont32
	$(ADDMOD) type32 -ps gs_res gs_typ32

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

# Native TrueType support
ttfont.dev: $(INT_MAK) $(ECHOGS_XE) type42.dev
	$(SETMOD) ttfont -include type42
	$(ADDMOD) ttfont -ps gs_mro_e gs_wan_e gs_ttf

# Type 42 (embedded TrueType) support
type42read_=$(PSOBJ)zchar42.$(OBJ) $(PSOBJ)zcharout.$(OBJ) $(PSOBJ)zfont42.$(OBJ)
type42.dev: $(INT_MAK) $(ECHOGS_XE) $(type42read_) ttflib.dev
	$(SETMOD) type42 $(type42read_)
	$(ADDMOD) type42 -include ttflib	
	$(ADDMOD) type42 -oper zchar42 zfont42
	$(ADDMOD) type42 -ps gs_typ42

$(PSOBJ)zchar42.$(OBJ): $(PSSRC)zchar42.c $(OP)\
 $(gsmatrix_h) $(gspaint_h) $(gspath_h)\
 $(gxfixed_h) $(gxchar_h) $(gxfont_h) $(gxfont42_h)\
 $(gxistate_h) $(gxpath_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(ifont_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zchar42.$(OBJ) $(C_) $(PSSRC)zchar42.c

$(PSOBJ)zfont42.$(OBJ): $(PSSRC)zfont42.c $(OP) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gxfont_h) $(gxfont42_h)\
 $(bfont_h) $(idict_h) $(idparam_h) $(store_h)
	$(PSCC) $(PSO_)zfont42.$(OBJ) $(C_) $(PSSRC)zfont42.c

# ======================== Precompilation options ======================== #

# ---------------- Precompiled fonts ---------------- #
# See fonts.txt for more information.

ccfont_h=$(PSSRC)ccfont.h $(std_h) $(gsmemory_h) $(iref_h) $(ivmspace_h) $(store_h)

CCFONT=$(OP) $(ccfont_h)

# List the fonts we are going to compile.
# Because of intrinsic limitations in `make', we have to list
# the object file names and the font names separately.
# Because of limitations in the DOS shell, we have to break the fonts up
# into lists that will fit on a single line (120 characters).
# The rules for constructing the .c files from the fonts themselves,
# and for compiling the .c files, are in cfonts.mak, not here.
# For example, to compile the Courier fonts, you should invoke
#	make -f cfonts.mak Courier_o
# By convention, the names of the 35 standard compiled fonts use '0' for
# the foundry name.  This allows users to substitute different foundries
# without having to change this makefile.
ccfonts_ps=gs_ccfnt
ccfonts1_=0agk.$(OBJ) 0agko.$(OBJ) 0agd.$(OBJ) 0agdo.$(OBJ)
ccfonts1=agk agko agd agdo
ccfonts2_=0bkl.$(OBJ) 0bkli.$(OBJ) 0bkd.$(OBJ) 0bkdi.$(OBJ)
ccfonts2=bkl bkli bkd bkdi
ccfonts3_=0crr.$(OBJ) 0cri.$(OBJ) 0crb.$(OBJ) 0crbi.$(OBJ)
ccfonts3=crr cri crb crbi
ccfonts4_=0hvr.$(OBJ) 0hvro.$(OBJ) 0hvb.$(OBJ) 0hvbo.$(OBJ)
ccfonts4=hvr hvro hvb hvbo
ccfonts5_=0hvrrn.$(OBJ) 0hvrorn.$(OBJ) 0hvbrn.$(OBJ) 0hvborn.$(OBJ)
ccfonts5=hvrrn hvrorn hvbrn hvborn
ccfonts6_=0ncr.$(OBJ) 0ncri.$(OBJ) 0ncb.$(OBJ) 0ncbi.$(OBJ)
ccfonts6=ncr ncri ncb ncbi
ccfonts7_=0plr.$(OBJ) 0plri.$(OBJ) 0plb.$(OBJ) 0plbi.$(OBJ)
ccfonts7=plr plri plb plbi
ccfonts8_=0tmr.$(OBJ) 0tmri.$(OBJ) 0tmb.$(OBJ) 0tmbi.$(OBJ)
ccfonts8=tmr tmri tmb tmbi
ccfonts9_=0syr.$(OBJ) 0zcmi.$(OBJ) 0zdr.$(OBJ)
ccfonts9=syr zcmi zdr
# The free distribution includes Bitstream Charter, Utopia, and
# freeware Cyrillic and Kana fonts.  We only provide for compiling
# Charter and Utopia.
ccfonts10free_=bchr.$(OBJ) bchri.$(OBJ) bchb.$(OBJ) bchbi.$(OBJ)
ccfonts10free=chr chri chb chbi
ccfonts11free_=putr.$(OBJ) putri.$(OBJ) putb.$(OBJ) putbi.$(OBJ)
ccfonts11free=utr utri utb utbi
# Uncomment the alternatives in the next 4 lines if you want
# Charter and Utopia compiled in.
#ccfonts10_=$(ccfonts10free_)
ccfonts10_=
#ccfonts10=$(ccfonts10free)
ccfonts10=
#ccfonts11_=$(ccfonts11free_)
ccfonts11_=
#ccfonts11=$(ccfonts11free)
ccfonts11=
# Add your own fonts here if desired.
ccfonts12_=
ccfonts12=
ccfonts13_=
ccfonts13=
ccfonts14_=
ccfonts14=
ccfonts15_=
ccfonts15=

# It's OK for ccfonts_.dev not to be CONFIG-dependent, because it only
# exists during the execution of the following rule.
# font2c has the prefix "gs" built into it, so we need to instruct
# genconf to use the same one.
$(gconfigf_h): $(MAKEFILE) $(INT_MAK) $(ECHOGS_XE) $(GENCONF_XE)
	$(SETMOD) ccfonts_ -font $(ccfonts1)
	$(ADDMOD) ccfonts_ -font $(ccfonts2)
	$(ADDMOD) ccfonts_ -font $(ccfonts3)
	$(ADDMOD) ccfonts_ -font $(ccfonts4)
	$(ADDMOD) ccfonts_ -font $(ccfonts5)
	$(ADDMOD) ccfonts_ -font $(ccfonts6)
	$(ADDMOD) ccfonts_ -font $(ccfonts7)
	$(ADDMOD) ccfonts_ -font $(ccfonts8)
	$(ADDMOD) ccfonts_ -font $(ccfonts9)
	$(ADDMOD) ccfonts_ -font $(ccfonts10)
	$(ADDMOD) ccfonts_ -font $(ccfonts11)
	$(ADDMOD) ccfonts_ -font $(ccfonts12)
	$(ADDMOD) ccfonts_ -font $(ccfonts13)
	$(ADDMOD) ccfonts_ -font $(ccfonts14)
	$(ADDMOD) ccfonts_ -font $(ccfonts15)
	$(GENCONF_XE) ccfonts_.dev -n gs -f $(gconfigf_h)

# We separate icfontab.dev from ccfonts.dev so that a customer can put
# compiled fonts into a separate shared library.

icfontab=icfontab$(CONFIG)

# Define ccfont_table separately, so it can be set from the command line
# to select an alternate compiled font table.
ccfont_table=$(icfontab)

$(icfontab).dev: $(MAKEFILE) $(INT_MAK) $(ECHOGS_XE) $(icfontab).$(OBJ)\
 $(ccfonts1_) $(ccfonts2_) $(ccfonts3_) $(ccfonts4_) $(ccfonts5_)\
 $(ccfonts6_) $(ccfonts7_) $(ccfonts8_) $(ccfonts9_) $(ccfonts10_)\
 $(ccfonts11_) $(ccfonts12_) $(ccfonts13_) $(ccfonts14_) $(ccfonts15_)
	$(SETMOD) $(icfontab) -obj $(icfontab).$(OBJ)
	$(ADDMOD) $(icfontab) -obj $(ccfonts1_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts2_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts3_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts4_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts5_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts6_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts7_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts8_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts9_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts10_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts11_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts12_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts13_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts14_)
	$(ADDMOD) $(icfontab) -obj $(ccfonts15_)

$(PSOBJ)$(icfontab).$(OBJ): $(PSSRC)icfontab.c $(AK) $(ccfont_h) $(gconfigf_h)
	$(CP_) $(gconfigf_h) gconfigf.h
	$(PSCC) $(PSO_)icfontab.$(OBJ) $(C_) $(PSSRC)icfontab.c

# Strictly speaking, ccfonts shouldn't need to include type1,
# since one could choose to precompile only Type 0 fonts,
# but getting this exactly right would be too much work.
ccfonts=ccfonts$(CONFIG)
$(ccfonts).dev: $(MAKEFILE) $(INT_MAK) type1.dev iccfont.$(OBJ)\
 $(ccfont_table).dev
	$(SETMOD) $(ccfonts) -include type1
	$(ADDMOD) $(ccfonts) -include $(ccfont_table)
	$(ADDMOD) $(ccfonts) -obj iccfont.$(OBJ)
	$(ADDMOD) $(ccfonts) -oper ccfonts
	$(ADDMOD) $(ccfonts) -ps $(ccfonts_ps)

$(PSOBJ)iccfont.$(OBJ): $(PSSRC)iccfont.c $(GH) $(string__h)\
 $(gsstruct_h) $(ccfont_h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(ifont_h) $(iname_h) $(isave_h) $(iutil_h)\
 $(oper_h) $(ostack_h) $(store_h) $(stream_h) $(strimpl_h) $(sfilter_h) $(iscan_h)
	$(PSCC) $(PSO_)iccfont.$(OBJ) $(C_) $(PSSRC)iccfont.c

# ---------------- Compiled initialization code ---------------- #

# We select either iccinit0 or iccinit1 depending on COMPILE_INITS.

$(PSOBJ)iccinit0.$(OBJ): $(PSSRC)iccinit0.c $(stdpre_h)
	$(PSCC) $(PSO_)iccinit0.$(OBJ) $(C_) $(PSSRC)iccinit0.c

$(PSOBJ)iccinit1.$(OBJ): $(PSOBJ)gs_init.$(OBJ)
	$(CP_) $(PSOBJ)gs_init.$(OBJ) $(PSOBJ)iccinit1.$(OBJ)

# All the gs_*.ps files should be prerequisites of gs_init.c,
# but we don't have any convenient list of them.
$(PSGEN)gs_init.c: $(GS_INIT) $(GENINIT_XE) $(gconfig_h)
	$(GENINIT_XE) $(GS_INIT) $(gconfig_h) -c $(PSGEN)gs_init.c

$(PSOBJ)gs_init.$(OBJ): $(PSGEN)gs_init.c $(stdpre_h)
	$(PSCC) $(PSO_)gs_init.$(OBJ) $(C_) $(PSGEN)gs_init.c

# ======================== PostScript Level 2 ======================== #

# We keep the old name for backward compatibility.
level2.dev: psl2.dev
	$(CP_) psl2.dev level2.dev

psl2.dev: $(INT_MAK) $(ECHOGS_XE)\
 cidfont.dev cie.dev cmapread.dev compfont.dev dct.dev dpsand2.dev\
 filter.dev iodevice.dev pagedev.dev pattern.dev\
 psl1.dev psl2lib.dev psl2read.dev\
 sepr.dev type32.dev type42.dev xfilter.dev
	$(SETMOD) psl2 -include cidfont cie cmapread compfont
	$(ADDMOD) psl2 -include dct dpsand2 filter iodevice
	$(ADDMOD) psl2 -include pagedev pattern psl1 psl2lib psl2read
	$(ADDMOD) psl2 -include sepr type32 type42 xfilter
	$(ADDMOD) psl2 -emulator PostScript PostScriptLevel2

# Define basic Level 2 language support.
# This is the minimum required for CMap and CIDFont support.

psl2int_=$(PSOBJ)iutil2.$(OBJ) $(PSOBJ)zmisc2.$(OBJ)
psl2int.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2int_) dps2int.dev usparam.dev
	$(SETMOD) psl2int $(psl2int_)
	$(ADDMOD) psl2int -include dps2int usparam
	$(ADDMOD) psl2int -oper zmisc2
	$(ADDMOD) psl2int -ps gs_lev2 gs_res

$(PSOBJ)iutil2.$(OBJ): $(PSSRC)iutil2.c $(GH) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsutil_h)\
 $(errors_h) $(idict_h) $(imemory_h) $(iutil_h) $(iutil2_h) $(opcheck_h)
	$(PSCC) $(PSO_)iutil2.$(OBJ) $(C_) $(PSSRC)iutil2.c

$(PSOBJ)zmisc2.$(OBJ): $(PSSRC)zmisc2.c $(OP) $(memory__h) $(string__h)\
 $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(ilevel_h) $(iname_h) $(iutil2_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zmisc2.$(OBJ) $(C_) $(PSSRC)zmisc2.c

# Define support for user and system parameters.
# We make this a separate module only because it must have a default.

nousparm_=$(PSOBJ)inouparm.$(OBJ)
nousparm.dev: $(INT_MAK) $(ECHOGS_XE) $(nousparm_)
	$(SETMOD) nousparm $(nousparm_)

$(PSOBJ)inouparm.$(OBJ): $(PSSRC)inouparm.c\
 $(ghost_h) $(icontext_h)
	$(PSCC) $(PSO_)inouparm.$(OBJ) $(C_) $(PSSRC)inouparm.c

usparam_=$(PSOBJ)zusparam.$(OBJ)
usparam.dev: $(INT_MAK) $(ECHOGS_XE) $(usparam_)
	$(SETMOD) usparam $(usparam_)
	$(ADDMOD) usparam -oper zusparam -replace nousparm

# Note that zusparam includes both Level 1 and Level 2 operators.
$(PSOBJ)zusparam.$(OBJ): $(PSSRC)zusparam.c $(OP) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gsfont_h) $(gsstruct_h) $(gsutil_h) $(gxht_h)\
 $(ialloc_h) $(icontext_h) $(idict_h) $(idparam_h) $(iparam_h)\
 $(iname_h) $(iutil2_h)\
 $(dstack_h) $(estack_h) $(store_h)
	$(PSCC) $(PSO_)zusparam.$(OBJ) $(C_) $(PSSRC)zusparam.c

# Define full Level 2 support.

iimage2_h=$(PSSRC)iimage2.h

psl2read_=$(PSOBJ)zcolor2.$(OBJ) $(PSOBJ)zcsindex.$(OBJ) $(PSOBJ)zht2.$(OBJ) $(PSOBJ)zimage2.$(OBJ)
# Note that zmisc2 includes both Level 1 and Level 2 operators.
psl2read.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2read_) psl2int.dev dps2read.dev
	$(SETMOD) psl2read $(psl2read_)
	$(ADDMOD) psl2read -include psl2int dps2read
	$(ADDMOD) psl2read -oper zcolor2_l2 zcsindex_l2
	$(ADDMOD) psl2read -oper zht2_l2 zimage2_l2

$(PSOBJ)zcolor2.$(OBJ): $(PSSRC)zcolor2.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)
	$(PSCC) $(PSO_)zcolor2.$(OBJ) $(C_) $(PSSRC)zcolor2.c

$(PSOBJ)zcsindex.$(OBJ): $(PSSRC)zcsindex.c $(OP) $(memory__h)\
 $(gscolor_h) $(gsstruct_h) $(gxfixed_h) $(gxcolor2_h) $(gxcspace_h) $(gsmatrix_h)\
 $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zcsindex.$(OBJ) $(C_) $(PSSRC)zcsindex.c

$(PSOBJ)zht2.$(OBJ): $(PSSRC)zht2.c $(OP)\
 $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(estack_h) $(ialloc_h) $(icolor_h) $(idict_h) $(idparam_h) $(igstate_h)\
 $(iht_h) $(store_h)
	$(PSCC) $(PSO_)zht2.$(OBJ) $(C_) $(PSSRC)zht2.c

$(PSOBJ)zimage2.$(OBJ): $(PSSRC)zimage2.c $(OP) $(math__h) $(memory__h)\
 $(gscolor_h) $(gscolor2_h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h)\
 $(gxfixed_h)\
 $(idict_h) $(idparam_h) $(iimage_h) $(iimage2_h) $(ilevel_h) $(igstate_h)
	$(PSCC) $(PSO_)zimage2.$(OBJ) $(C_) $(PSSRC)zimage2.c

# ---------------- setpagedevice ---------------- #

pagedev_=$(PSOBJ)zdevice2.$(OBJ) $(PSOBJ)zmedia2.$(OBJ)
pagedev.dev: $(INT_MAK) $(ECHOGS_XE) $(pagedev_) $(PSSRC)gs_setpd.ps
	$(SETMOD) pagedev $(pagedev_)
	$(ADDMOD) pagedev -oper zdevice2_l2 zmedia2_l2
	$(ADDMOD) pagedev -ps gs_setpd

$(PSOBJ)zdevice2.$(OBJ): $(PSSRC)zdevice2.c $(OP) $(math__h) $(memory__h)\
 $(dstack_h) $(estack_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h) $(store_h)\
 $(gxdevice_h) $(gsstate_h)
	$(PSCC) $(PSO_)zdevice2.$(OBJ) $(C_) $(PSSRC)zdevice2.c

$(PSOBJ)zmedia2.$(OBJ): $(PSSRC)zmedia2.c $(OP) $(math__h) $(memory__h)\
 $(gsmatrix_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zmedia2.$(OBJ) $(C_) $(PSSRC)zmedia2.c

# ---------------- IODevices ---------------- #

iodevice_=$(PSOBJ)ziodev2.$(OBJ) $(PSOBJ)zdevcal.$(OBJ)
iodevice.dev: $(INT_MAK) $(ECHOGS_XE) $(iodevice_)
	$(SETMOD) iodevice $(iodevice_)
	$(ADDMOD) iodevice -oper ziodev2_l2
	$(ADDMOD) iodevice -iodev null ram calendar

$(PSOBJ)ziodev2.$(OBJ): $(PSSRC)ziodev2.c $(OP) $(string__h) $(gp_h)\
 $(gxiodev_h) $(stream_h)\
 $(dstack_h) $(files_h) $(iparam_h) $(iutil2_h) $(store_h)
	$(PSCC) $(PSO_)ziodev2.$(OBJ) $(C_) $(PSSRC)ziodev2.c

$(PSOBJ)zdevcal.$(OBJ): $(PSSRC)zdevcal.c $(GH) $(time__h)\
 $(gxiodev_h) $(iparam_h) $(istack_h)
	$(PSCC) $(PSO_)zdevcal.$(OBJ) $(C_) $(PSSRC)zdevcal.c

# ---------------- Filters other than the ones in sfilter.c ---------------- #

# Standard Level 2 decoding filters only.  The PDF configuration uses this.
fdecode_=$(GLOBJ)scantab.$(OBJ) $(GLOBJ)scfparam.$(OBJ) $(GLOBJ)sfilter2.$(OBJ) $(PSOBJ)zfdecode.$(OBJ)
fdecode.dev: $(INT_MAK) $(ECHOGS_XE) $(fdecode_) cfd.dev lzwd.dev pdiff.dev pngp.dev rld.dev
	$(SETMOD) fdecode $(fdecode_)
	$(ADDMOD) fdecode -include cfd lzwd pdiff pngp rld
	$(ADDMOD) fdecode -oper zfdecode

$(PSOBJ)zfdecode.$(OBJ): $(PSSRC)zfdecode.c $(OP) $(memory__h)\
 $(gsparam_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(ilevel_h) $(iparam_h)\
 $(sa85x_h) $(scf_h) $(scfx_h) $(sfilter_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h)\
 $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfdecode.$(OBJ) $(C_) $(PSSRC)zfdecode.c

# Complete Level 2 filter capability.
filter_=$(PSOBJ)zfilter2.$(OBJ)
filter.dev: $(INT_MAK) $(ECHOGS_XE) fdecode.dev $(filter_) cfe.dev lzwe.dev rle.dev
	$(SETMOD) filter -include fdecode
	$(ADDMOD) filter -obj $(filter_)
	$(ADDMOD) filter -include cfe lzwe rle
	$(ADDMOD) filter -oper zfilter2

$(PSOBJ)zfilter2.$(OBJ): $(PSSRC)zfilter2.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(store_h)\
 $(sfilter_h) $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfilter2.$(OBJ) $(C_) $(PSSRC)zfilter2.c

# Extensions beyond Level 2 standard.
xfilter_=$(PSOBJ)sbhc.$(OBJ) $(PSOBJ)sbwbs.$(OBJ) $(PSOBJ)shcgen.$(OBJ)\
 $(PSOBJ)smtf.$(OBJ) $(PSOBJ)zfilterx.$(OBJ)
xfilter.dev: $(INT_MAK) $(ECHOGS_XE) $(xfilter_) pcxd.dev pngp.dev
	$(SETMOD) xfilter $(xfilter_)
	$(ADDMOD) xfilter -include pcxd
	$(ADDMOD) xfilter -oper zfilterx

$(PSOBJ)sbhc.$(OBJ): $(PSSRC)sbhc.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(sbhc_h) $(shcgen_h) $(strimpl_h)
	$(PSCC) $(PSO_)sbhc.$(OBJ) $(C_) $(PSSRC)sbhc.c

$(PSOBJ)sbwbs.$(OBJ): $(PSSRC)sbwbs.c $(AK) $(stdio__h) $(memory__h)\
 $(gdebug_h) $(sbwbs_h) $(sfilter_h) $(strimpl_h)
	$(PSCC) $(PSO_)sbwbs.$(OBJ) $(C_) $(PSSRC)sbwbs.c

$(PSOBJ)shcgen.$(OBJ): $(PSSRC)shcgen.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gserror_h) $(gserrors_h) $(gsmemory_h)\
 $(scommon_h) $(shc_h) $(shcgen_h)
	$(PSCC) $(PSO_)shcgen.$(OBJ) $(C_) $(PSSRC)shcgen.c

$(PSOBJ)smtf.$(OBJ): $(PSSRC)smtf.c $(AK) $(stdio__h)\
 $(smtf_h) $(strimpl_h)
	$(PSCC) $(PSO_)smtf.$(OBJ) $(C_) $(PSSRC)smtf.c

$(PSOBJ)zfilterx.$(OBJ): $(PSSRC)zfilterx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h)\
 $(store_h) $(sfilter_h) $(sbhc_h) $(sbtx_h) $(sbwbs_h) $(shcgen_h)\
 $(smtf_h) $(spcxx_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfilterx.$(OBJ) $(C_) $(PSSRC)zfilterx.c

# ---------------- Binary tokens ---------------- #

btoken_=$(PSOBJ)iscanbin.$(OBJ) $(PSOBJ)zbseq.$(OBJ)
btoken.dev: $(INT_MAK) $(ECHOGS_XE) $(btoken_)
	$(SETMOD) btoken $(btoken_)
	$(ADDMOD) btoken -oper zbseq_l2
	$(ADDMOD) btoken -ps gs_btokn

btoken_h=$(PSSRC)btoken.h

$(PSOBJ)iscanbin.$(OBJ): $(PSSRC)iscanbin.c $(GH)\
 $(math__h) $(memory__h) $(errors_h)\
 $(gsutil_h) $(ialloc_h) $(ibnum_h) $(idict_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(ivmspace_h)\
 $(btoken_h) $(dstack_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)iscanbin.$(OBJ) $(C_) $(PSSRC)iscanbin.c

$(PSOBJ)zbseq.$(OBJ): $(PSSRC)zbseq.c $(OP) $(memory__h)\
 $(btoken_h) $(ialloc_h) $(store_h)
	$(PSCC) $(PSO_)zbseq.$(OBJ) $(C_) $(PSSRC)zbseq.c

# ---------------- User paths & insideness testing ---------------- #

upath_=$(PSOBJ)zupath.$(OBJ) $(PSOBJ)ibnum.$(OBJ) $(GLOBJ)gdevhit.$(OBJ)
upath.dev: $(INT_MAK) $(ECHOGS_XE) $(upath_)
	$(SETMOD) upath $(upath_)
	$(ADDMOD) upath -oper zupath_l2

$(PSOBJ)zupath.$(OBJ): $(PSSRC)zupath.c $(OP)\
 $(dstack_h) $(store_h)\
 $(ibnum_h) $(idict_h) $(igstate_h) $(iname_h) $(iutil_h) $(stream_h)\
 $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h)\
 $(gxfixed_h) $(gxdevice_h) $(gzpath_h) $(gzstate_h)
	$(PSCC) $(PSO_)zupath.$(OBJ) $(C_) $(PSSRC)zupath.c

# -------- Additions common to Display PostScript and Level 2 -------- #

dpsand2.dev: $(INT_MAK) $(ECHOGS_XE) btoken.dev color.dev upath.dev dps2lib.dev dps2read.dev
	$(SETMOD) dpsand2 -include btoken color upath dps2lib dps2read

dps2int_=$(PSOBJ)zvmem2.$(OBJ) $(PSOBJ)zdps1.$(OBJ)
# Note that zvmem2 includes both Level 1 and Level 2 operators.
dps2int.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2int_)
	$(SETMOD) dps2int $(dps2int_)
	$(ADDMOD) dps2int -oper zvmem2 zdps1_l2
	$(ADDMOD) dps2int -ps gs_dps1

dps2read_=$(PSOBJ)ibnum.$(OBJ) $(PSOBJ)zchar2.$(OBJ)
dps2read.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2read_) dps2int.dev
	$(SETMOD) dps2read $(dps2read_)
	$(ADDMOD) dps2read -include dps2int
	$(ADDMOD) dps2read -oper ireclaim_l2 zchar2
	$(ADDMOD) dps2read -ps gs_dps2

$(PSOBJ)ibnum.$(OBJ): $(PSSRC)ibnum.c $(GH) $(math__h) $(memory__h)\
 $(errors_h) $(stream_h) $(ibnum_h) $(imemory_h) $(iutil_h)
	$(PSCC) $(PSO_)ibnum.$(OBJ) $(C_) $(PSSRC)ibnum.c

$(PSOBJ)zchar2.$(OBJ): $(PSSRC)zchar2.c $(OP)\
 $(gschar_h) $(gsmatrix_h) $(gspath_h) $(gsstruct_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h)\
 $(ialloc_h) $(ichar_h) $(estack_h) $(ifont_h) $(iname_h) $(igstate_h)\
 $(store_h) $(stream_h) $(ibnum_h)
	$(PSCC) $(PSO_)zchar2.$(OBJ) $(C_) $(PSSRC)zchar2.c

$(PSOBJ)zdps1.$(OBJ): $(PSSRC)zdps1.c $(OP)\
 $(gsmatrix_h) $(gspath_h) $(gspath2_h) $(gsstate_h)\
 $(ialloc_h) $(ivmspace_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h)
	$(PSCC) $(PSO_)zdps1.$(OBJ) $(C_) $(PSSRC)zdps1.c

$(PSOBJ)zvmem2.$(OBJ): $(PSSRC)zvmem2.c $(OP)\
 $(estack_h) $(ialloc_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zvmem2.$(OBJ) $(C_) $(PSSRC)zvmem2.c

# -------- Composite (PostScript Type 0) font support -------- #

compfont.dev: $(INT_MAK) $(ECHOGS_XE) psf0lib.dev psf0read.dev
	$(SETMOD) compfont -include psf0lib psf0read

# We always include zfcmap.$(OBJ) because zfont0.c refers to it,
# and it's not worth the trouble to exclude.
psf0read_=$(PSOBJ)zcfont.$(OBJ) $(PSOBJ)zfcmap.$(OBJ) $(PSOBJ)zfont0.$(OBJ)
psf0read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf0read_)
	$(SETMOD) psf0read $(psf0read_)
	$(ADDMOD) psf0read -oper zcfont zfcmap zfont0

$(PSOBJ)zcfont.$(OBJ): $(PSSRC)zcfont.c $(OP)\
 $(gschar_h) $(gsmatrix_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h)\
 $(ichar_h) $(estack_h) $(ifont_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zcfont.$(OBJ) $(C_) $(PSSRC)zcfont.c

$(PSOBJ)zfcmap.$(OBJ): $(PSSRC)zfcmap.c $(OP)\
 $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfcmap_h) $(gxfont_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifont_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zfcmap.$(OBJ) $(C_) $(PSSRC)zfcmap.c

$(PSOBJ)zfont0.$(OBJ): $(PSSRC)zfont0.c $(OP)\
 $(gschar_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfcmap_h) $(gxfixed_h) $(gxfont_h) $(gxfont0_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h)\
 $(store_h)
	$(PSCC) $(PSO_)zfont0.$(OBJ) $(C_) $(PSSRC)zfont0.c

# ---------------- CMap support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cmapread_=$(PSOBJ)zfcmap.$(OBJ)
cmapread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmapread_) cmaplib.dev psl2int.dev
	$(SETMOD) cmapread $(cmapread_)
	$(ADDMOD) cmapread -include cmaplib psl2int
	$(ADDMOD) cmapread -oper zfcmap
	$(ADDMOD) cmapread -ps gs_cmap

# ---------------- CIDFont support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cidread_=$(PSOBJ)zcid.$(OBJ)
cidfont.dev: $(INT_MAK) $(ECHOGS_XE) psf1read.dev psl2int.dev type42.dev\
 $(cidread_)
	$(SETMOD) cidfont $(cidread_)
	$(ADDMOD) cidfont -include psf1read psl2int type42
	$(ADDMOD) cidfont -ps gs_cidfn
	$(ADDMOD) cidfont -oper zcid

$(PSOBJ)zcid.$(OBJ): $(PSSRC)zcid.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gxfont_h)\
 $(bfont_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zcid.$(OBJ) $(C_) $(PSSRC)zcid.c

# ---------------- CIE color ---------------- #

cieread_=$(PSOBJ)zcie.$(OBJ) $(PSOBJ)zcrd.$(OBJ)
cie.dev: $(INT_MAK) $(ECHOGS_XE) $(cieread_) cielib.dev
	$(SETMOD) cie $(cieread_)
	$(ADDMOD) cie -oper zcie_l2 zcrd_l2
	$(ADDMOD) cie -include cielib

icie_h=$(PSSRC)icie.h

$(PSOBJ)zcie.$(OBJ): $(PSSRC)zcie.c $(OP) $(math__h) $(memory__h)\
 $(gscolor2_h) $(gscie_h) $(gsstruct_h) $(gxcspace_h)\
 $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h)\
 $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zcie.$(OBJ) $(C_) $(PSSRC)zcie.c

$(PSOBJ)zcrd.$(OBJ): $(PSSRC)zcrd.c $(OP) $(math__h)\
 $(gscrd_h) $(gscrdp_h) $(gscspace_h) $(gscolor2_h) $(gsstruct_h)\
 $(estack_h) $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(igstate_h)\
 $(iparam_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zcrd.$(OBJ) $(C_) $(PSSRC)zcrd.c

# ---------------- Pattern color ---------------- #

pattern.dev: $(INT_MAK) $(ECHOGS_XE) patlib.dev patread.dev
	$(SETMOD) pattern -include patlib patread

patread_=$(PSOBJ)zpcolor.$(OBJ)
patread.dev: $(INT_MAK) $(ECHOGS_XE) $(patread_)
	$(SETMOD) patread $(patread_)
	$(ADDMOD) patread -oper zpcolor_l2

$(PSOBJ)zpcolor.$(OBJ): $(PSSRC)zpcolor.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)
	$(PSCC) $(PSO_)zpcolor.$(OBJ) $(C_) $(PSSRC)zpcolor.c

# ---------------- Separation color ---------------- #

seprread_=$(PSOBJ)zcssepr.$(OBJ)
sepr.dev: $(INT_MAK) $(ECHOGS_XE) $(seprread_) seprlib.dev
	$(SETMOD) sepr $(seprread_)
	$(ADDMOD) sepr -oper zcssepr_l2
	$(ADDMOD) sepr -include seprlib

$(PSOBJ)zcssepr.$(OBJ): $(PSSRC)zcssepr.c $(OP)\
 $(gscolor_h) $(gscsepr_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h)\
 $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)
	$(PSCC) $(PSO_)zcssepr.$(OBJ) $(C_) $(PSSRC)zcssepr.c

# ---------------- Functions ---------------- #

ifunc_h=$(PSSRC)ifunc.h

# Generic support, and FunctionType 0.
funcread_=$(PSOBJ)zfunc.$(OBJ) $(PSOBJ)zfunc0.$(OBJ)
func.dev: $(INT_MAK) $(ECHOGS_XE) $(funcread_) funclib.dev
	$(SETMOD) func $(funcread_)
	$(ADDMOD) func -oper zfunc zfunc0
	$(ADDMOD) func -include funclib

$(PSOBJ)zfunc.$(OBJ): $(PSSRC)zfunc.c $(OP) $(memory__h)\
 $(gsfunc_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(store_h)
	$(PSCC) $(PSO_)zfunc.$(OBJ) $(C_) $(PSSRC)zfunc.c

$(PSOBJ)zfunc0.$(OBJ): $(PSSRC)zfunc0.c $(OP) $(memory__h)\
 $(gsdsrc_h) $(gsfunc_h) $(gsfunc0_h)\
 $(stream_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)
	$(PSCC) $(PSO_)zfunc0.$(OBJ) $(C_) $(PSSRC)zfunc0.c

# ---------------- DCT filters ---------------- #
# The definitions for jpeg*.dev are in jpeg.mak.

dct.dev: $(INT_MAK) $(ECHOGS_XE) dcte.dev dctd.dev
	$(SETMOD) dct -include dcte dctd

# Encoding (compression)

dcte_=$(PSOBJ)zfdcte.$(OBJ) $(GLOBJ)sdeparam.$(OBJ) $(GLOBJ)sdcparam.$(OBJ)
dcte.dev: $(INT_MAK) $(ECHOGS_XE) sdcte.dev $(dcte_)
	$(SETMOD) dcte -include sdcte
	$(ADDMOD) dcte -obj $(dcte_)
	$(ADDMOD) dcte -oper zfdcte

$(PSOBJ)zfdcte.$(OBJ): $(PSSRC)zfdcte.c $(OP)\
 $(memory__h) $(stdio__h) $(jpeglib_h)\
 $(gsmalloc_h)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(iparam_h)
	$(PSCC) $(PSO_)zfdcte.$(OBJ) $(C_) $(PSSRC)zfdcte.c

# Decoding (decompression)

dctd_=$(PSOBJ)zfdctd.$(OBJ) $(GLOBJ)sddparam.$(OBJ) $(GLOBJ)sdcparam.$(OBJ)
dctd.dev: $(INT_MAK) $(ECHOGS_XE) sdctd.dev $(dctd_)
	$(SETMOD) dctd -include sdctd
	$(ADDMOD) dctd -obj $(dctd_)
	$(ADDMOD) dctd -oper zfdctd

$(PSOBJ)zfdctd.$(OBJ): $(PSSRC)zfdctd.c $(OP)\
 $(memory__h) $(stdio__h) $(jpeglib_h)\
 $(gsmalloc_h)\
 $(ialloc_h) $(ifilter_h) $(iparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(PSCC) $(PSO_)zfdctd.$(OBJ) $(C_) $(PSSRC)zfdctd.c

# ---------------- zlib/Flate filters ---------------- #

fzlib_=$(PSOBJ)zfzlib.$(OBJ)
fzlib.dev: $(INT_MAK) $(ECHOGS_XE) $(fzlib_) szlibe.dev szlibd.dev
	$(SETMOD) fzlib -include szlibe szlibd
	$(ADDMOD) fzlib -obj $(fzlib_)
	$(ADDMOD) fzlib -oper zfzlib

$(PSOBJ)zfzlib.$(OBJ): $(PSSRC)zfzlib.c $(OP)\
 $(idict_h) $(ifilter_h)\
 $(spdiffx_h) $(spngpx_h) $(strimpl_h) $(szlibx_h)
	$(PSCC) $(PSO_)zfzlib.$(OBJ) $(C_) $(PSSRC)zfzlib.c

# ================ Display PostScript ================ #

dps_=$(PSOBJ)zdps.$(OBJ) $(PSOBJ)zcontext.$(OBJ)
dps.dev: $(INT_MAK) $(ECHOGS_XE) dpslib.dev psl2.dev $(dps_)
	$(SETMOD) dps -include dpslib psl2
	$(ADDMOD) dps -obj $(dps_)
	$(ADDMOD) dps -oper zcontext zdps
	$(ADDMOD) dps -ps gs_dps

$(PSOBJ)zdps.$(OBJ): $(PSSRC)zdps.c $(OP)\
 $(gsdps_h) $(gsimage_h) $(gsiparm2_h) $(gsstate_h)\
 $(gxfixed_h) $(gxpath_h)\
 $(btoken_h) $(idparam_h) $(idict_h) $(igstate_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zdps.$(OBJ) $(C_) $(PSSRC)zdps.c

$(PSOBJ)zcontext.$(OBJ): $(PSSRC)zcontext.c $(OP) $(gp_h) $(memory__h)\
 $(gsexit_h) $(gsstruct_h) $(gsutil_h) $(gxalloc_h) $(gxstate_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(interp_h) $(isave_h) $(istruct_h)\
 $(dstack_h) $(estack_h) $(files_h) $(ostack_h) $(store_h) $(stream_h)
	$(PSCC) $(PSO_)zcontext.$(OBJ) $(C_) $(PSSRC)zcontext.c

# ---------------- NeXT Display PostScript ---------------- #

dpsnext_=$(PSOBJ)zdpnext.$(OBJ)
dpsnext.dev: $(INT_MAK) $(ECHOGS_XE) dps.dev dpnxtlib.dev $(dpsnext_)\
 $(PSSRC)gs_dpnxt.ps
	$(SETMOD) dpsnext -include dps dpnxtlib
	$(ADDMOD) dpsnext -obj $(dpsnext_)
	$(ADDMOD) dpsnext -oper zdpnext
	$(ADDMOD) dpsnext -ps gs_dpnxt

$(PSOBJ)zdpnext.$(OBJ): $(PSSRC)zdpnext.c $(math__h) $(OP)\
 $(gscoord_h) $(gscspace_h) $(gsdpnext_h)\
 $(gsiparam_h) $(gsiparm2_h) $(gsmatrix_h) $(gspath2_h)\
 $(gxcvalue_h) $(gxdevice_h) $(gxsample_h)\
 $(ialloc_h) $(igstate_h) $(iimage_h) $(store_h)
	$(PSCC) $(PSO_)zdpnext.$(OBJ) $(C_) $(PSSRC)zdpnext.c

# ==================== PostScript LanguageLevel 3 ===================== #

# ---------------- DevicePixel color space ---------------- #

cspixint_=$(PSOBJ)zcspixel.$(OBJ)
cspixel.dev: $(INT_MAK) $(ECHOGS_XE) $(cspixint_) cspixlib.dev
	$(SETMOD) cspixel $(cspixint_)
	$(ADDMOD) cspixel -oper zcspixel
	$(ADDMOD) cspixel -include cspixlib

$(PSOBJ)zcspixel.$(OBJ): $(PSSRC)zcspixel.c $(OP)\
 $(gscolor2_h) $(gscpixel_h) $(gscspace_h) $(gsmatrix_h)\
 $(igstate_h)
	$(PSCC) $(PSO_)zcspixel.$(OBJ) $(C_) $(PSSRC)zcspixel.c

# ---------------- Rest of LanguageLevel 3 ---------------- #

psl3.dev: $(INT_MAK) $(ECHOGS_XE)\
 psl2.dev cspixel.dev func.dev psl3lib.dev psl3read.dev
	$(SETMOD) psl3 -include psl2 cspixel func psl3lib psl3read

$(PSOBJ)zcsdevn.$(OBJ): $(PSSRC)zcsdevn.c $(OP)\
 $(gscolor2_h) $(gxcspace_h)\
 $(ialloc_h) $(igstate_h) $(iname_h)
	$(PSCC) $(PSO_)zcsdevn.$(OBJ) $(C_) $(PSSRC)zcsdevn.c

$(PSOBJ)zfreuse.$(OBJ): $(PSSRC)zfreuse.c $(OP) $(memory__h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)\
 $(files_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h)
	$(PSCC) $(PSO_)zfreuse.$(OBJ) $(C_) $(PSSRC)zfreuse.c

$(PSOBJ)zfunc3.$(OBJ): $(PSSRC)zfunc3.c $(memory__h) $(OP)\
 $(gsfunc3_h) $(gsstruct_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)\
 $(store_h) $(stream_h)
	$(PSCC) $(PSO_)zfunc3.$(OBJ) $(C_) $(PSSRC)zfunc3.c

$(PSOBJ)zimage3.$(OBJ): $(PSSRC)zimage3.c $(OP) $(memory__h)\
 $(gscolor2_h) $(gsiparm3_h) $(gsiparm4_h) $(gscspace_h) $(gxiparam_h)\
 $(idparam_h) $(idict_h) $(igstate_h) $(iimage_h) $(iimage2_h)
	$(PSCC) $(PSO_)zimage3.$(OBJ) $(C_) $(PSSRC)zimage3.c

$(PSOBJ)zmisc3.$(OBJ): $(PSSRC)zmisc3.c $(GH)\
 $(gsclipsr_h) $(igstate_h) $(oper_h) $(store_h)
	$(PSCC) $(PSO_)zmisc3.$(OBJ) $(C_) $(PSSRC)zmisc3.c

$(PSOBJ)zshade.$(OBJ): $(PSSRC)zshade.c $(memory__h) $(OP)\
 $(gscolor2_h) $(gscolor3_h) $(gscspace_h) $(gsfunc3_h)\
 $(gsshade_h) $(gsstruct_h) $(gsuid_h)\
 $(stream_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(igstate_h) $(store_h)
	$(PSCC) $(PSO_)zshade.$(OBJ) $(C_) $(PSSRC)zshade.c

$(PSOBJ)ztrap.$(OBJ): $(PSSRC)ztrap.c $(OP)\
 $(gstrap_h)\
 $(ialloc_h) $(iparam_h)
	$(PSCC) $(PSO_)ztrap.$(OBJ) $(C_) $(PSSRC)ztrap.c

psl3read_1=$(PSOBJ)zcsdevn.$(OBJ) $(PSOBJ)zfreuse.$(OBJ) $(PSOBJ)zfunc3.$(OBJ) $(PSOBJ)zimage3.$(OBJ)
psl3read_2=$(PSOBJ)zmisc3.$(OBJ) $(PSOBJ)zshade.$(OBJ) $(PSOBJ)ztrap.$(OBJ)
psl3read_=$(psl3read_1) $(psl3read_2)

psl3read.dev: $(INT_MAK) $(ECHOGS_XE) $(psl3read_) fzlib.dev
	$(SETMOD) psl3read $(psl3read_1)
	$(ADDMOD) psl3read $(psl3read_2)
	$(ADDMOD) psl3read -oper zcsdevn zfreuse zfunc3 zimage3
	$(ADDMOD) psl3read -oper zmisc3 zshade ztrap
	$(ADDMOD) psl3read -ps gs_ll3
	$(ADDMOD) psl3read -include fzlib

# ================================ PDF ================================ #

# We need most of the Level 2 interpreter to do PDF, but not all of it.
# In fact, we don't even need all of a Level 1 interpreter.

# Because of the way the PDF encodings are defined, they must get loaded
# before we install the Level 2 resource machinery.
# On the other hand, the PDF .ps files must get loaded after
# level2dict is defined.
pdfmin.dev: $(INT_MAK) $(ECHOGS_XE)\
 psbase.dev color.dev compfont.dev dps2lib.dev dps2read.dev\
 fdecode.dev type1.dev pdffonts.dev psl2lib.dev psl2read.dev pdfread.dev
	$(SETMOD) pdfmin -include psbase color compfont
	$(ADDMOD) pdfmin -include dps2lib dps2read fdecode type1
	$(ADDMOD) pdfmin -include pdffonts psl2lib psl2read pdfread
	$(ADDMOD) pdfmin -emulator PDF

pdf.dev: $(INT_MAK) $(ECHOGS_XE)\
 pdfmin.dev cff.dev cidfont.dev cie.dev cmapread.dev dctd.dev\
 func.dev ttfont.dev type2.dev
	$(SETMOD) pdf -include pdfmin cff cidfont cie cmapread dctd
	$(ADDMOD) pdf -include func ttfont type2

# Reader only

pdffonts.dev: $(INT_MAK) $(ECHOGS_XE)\
 $(PSSRC)gs_mex_e.ps $(PSSRC)gs_mro_e.ps $(PSSRC)gs_pdf_e.ps\
 $(PSSRC)gs_wan_e.ps
	$(SETMOD) pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

pdfread.dev: $(INT_MAK) $(ECHOGS_XE) fzlib.dev
	$(SETMOD) pdfread -include fzlib
	$(ADDMOD) pdfread -ps pdf_ops gs_l2img
	$(ADDMOD) pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec

# ============================= Main program ============================== #

$(PSOBJ)gs.$(OBJ): $(PSSRC)gs.c $(GH)\
 $(imain_h) $(imainarg_h) $(iminst_h)
	$(PSCC) $(PSO_)gs.$(OBJ) $(C_) $(PSSRC)gs.c

$(PSOBJ)icontext.$(OBJ): $(PSSRC)icontext.c $(GH)\
 $(gsstruct_h) $(gxalloc_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h) $(ostack_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(interp_h) $(isave_h) $(store_h)\
 $(stream_h)
	$(PSCC) $(PSO_)icontext.$(OBJ) $(C_) $(PSSRC)icontext.c

$(PSOBJ)imainarg.$(OBJ): $(PSSRC)imainarg.c $(GH)\
 $(ctype__h) $(memory__h) $(string__h)\
 $(gp_h)\
 $(gsargs_h) $(gscdefs_h) $(gsdevice_h) $(gsmalloc_h) $(gsmdebug_h)\
 $(gxdevice_h) $(gxdevmem_h)\
 $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(imain_h) $(imainarg_h) $(iminst_h)\
 $(iname_h) $(interp_h) $(iscan_h) $(iutil_h) $(ivmspace_h)\
 $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)imainarg.$(OBJ) $(C_) $(PSSRC)imainarg.c

$(PSOBJ)imain.$(OBJ): $(PSSRC)imain.c $(GH) $(memory__h) $(string__h)\
 $(gp_h) $(gslib_h) $(gsmatrix_h) $(gsutil_h) $(gxdevice_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(interp_h)\
 $(isave_h) $(iscan_h) $(ivmspace_h)\
 $(main_h) $(oper_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)imain.$(OBJ) $(C_) $(PSSRC)imain.c

#****** $(CCINT) interp.c
$(PSOBJ)interp.$(OBJ): $(PSSRC)interp.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(iastruct_h) $(icontext_h) $(idict_h) $(iname_h) $(inamedef_h)\
 $(interp_h) $(ipacked_h) $(isave_h) $(iscan_h) $(istack_h)\
 $(iutil_h) $(ivmspace_h)\
 $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(PSCC) $(PSO_)interp.$(OBJ) $(C_) $(PSSRC)interp.c

$(PSOBJ)ireclaim.$(OBJ): $(PSSRC)ireclaim.c $(GH)\
 $(gsstruct_h)\
 $(iastate_h) $(icontext_h) $(interp_h) $(isave_h) $(isstate_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(opdef_h) $(ostack_h) $(store_h)
	$(PSCC) $(PSO_)ireclaim.$(OBJ) $(C_) $(PSSRC)ireclaim.c
