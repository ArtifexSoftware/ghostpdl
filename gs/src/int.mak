#    Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# (Platform-independent) makefile for language interpreters.
# See the end of gs.mak for where this fits into the build process.

# Define the name of this makefile.
INT_MAK=int.mak

# ======================== Interpreter support ======================== #

# This is support code for all interpreters, not just PostScript and PDF.
# It knows about the PostScript data types, but isn't supposed to
# depend on anything outside itself.

errors_h=errors.h
idebug_h=idebug.h
idict_h=idict.h
igc_h=igc.h
igcstr_h=igcstr.h
iname_h=iname.h
inamedef_h=inamedef.h $(gconfigv_h) $(iname_h)
ipacked_h=ipacked.h
iref_h=iref.h
isave_h=isave.h
isstate_h=isstate.h
istruct_h=istruct.h $(gsstruct_h)
iutil_h=iutil.h
ivmspace_h=ivmspace.h $(gsgc_h)
opdef_h=opdef.h
# Nested include files
ghost_h=ghost.h $(gx_h) $(iref_h)
imemory_h=imemory.h $(gsalloc_h) $(ivmspace_h)
ialloc_h=ialloc.h $(imemory_h)
iastruct_h=iastruct.h $(gxobj_h) $(ialloc_h)
iastate_h=iastate.h $(gxalloc_h) $(ialloc_h) $(istruct_h)
store_h=store.h $(ialloc_h)

GH=$(AK) $(ghost_h)

isupport1_=ialloc.$(OBJ) igc.$(OBJ) igcref.$(OBJ) igcstr.$(OBJ)
isupport2_=ilocate.$(OBJ) iname.$(OBJ) isave.$(OBJ)
isupport_=$(isupport1_) $(isupport2_)
isupport.dev: $(INT_MAK) $(ECHOGS_XE) $(isupport_)
	$(SETMOD) isupport $(isupport1_)
	$(ADDMOD) isupport -obj $(isupport2_)
	$(ADDMOD) isupport -init igcref

ialloc.$(OBJ): ialloc.c $(AK) $(memory__h) $(gx_h)\
 $(errors_h) $(gsstruct_h) $(gxarith_h)\
 $(iastate_h) $(iref_h) $(ivmspace_h) $(store_h)

# igc.c, igcref.c, and igcstr.c should really be in the dpsand2 list,
# but since all the GC enumeration and relocation routines refer to them,
# it's too hard to separate them out from the Level 1 base.
igc.$(OBJ): igc.c $(GH) $(memory__h)\
  $(errors_h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h) $(gsutil_h) \
  $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(inamedef_h) \
  $(ipacked_h) $(isave_h) $(isstate_h) $(istruct_h) $(opdef_h)

igcref.$(OBJ): igcref.c $(GH) $(memory__h)\
 $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idebug_h) $(igc_h) $(iname_h) $(ipacked_h) $(store_h)

igcstr.$(OBJ): igcstr.c $(GH) $(memory__h)\
 $(gsmdebug_h) $(gsstruct_h) $(iastate_h) $(igcstr_h)

ilocate.$(OBJ): ilocate.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h)\
 $(iastate_h) $(idict_h) $(igc_h) $(igcstr_h) $(iname_h)\
 $(ipacked_h) $(isstate_h) $(iutil_h) $(ivmspace_h)\
 $(store_h)

iname.$(OBJ): iname.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h) $(gxobj_h)\
 $(errors_h) $(imemory_h) $(inamedef_h) $(isave_h) $(store_h)

isave.$(OBJ): isave.c $(GH) $(memory__h)\
 $(errors_h) $(gsexit_h) $(gsstruct_h) $(gsutil_h)\
 $(iastate_h) $(inamedef_h) $(isave_h) $(isstate_h) $(ivmspace_h)\
 $(ipacked_h) $(store_h)

### Include files

idparam_h=idparam.h
ilevel_h=ilevel.h
iparam_h=iparam.h $(gsparam_h)
istack_h=istack.h
iutil2_h=iutil2.h
opcheck_h=opcheck.h
opextern_h=opextern.h
# Nested include files
dstack_h=dstack.h $(istack_h)
estack_h=estack.h $(istack_h)
ostack_h=ostack.h $(istack_h)
oper_h=oper.h $(iutil_h) $(opcheck_h) $(opdef_h) $(opextern_h) $(ostack_h)

idebug.$(OBJ): idebug.c $(GH) $(string__h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(ostack_h) $(opdef_h) $(ipacked_h) $(store_h)

idict.$(OBJ): idict.c $(GH) $(string__h) $(errors_h)\
 $(ialloc_h) $(idebug_h) $(ivmspace_h) $(inamedef_h) $(ipacked_h)\
 $(isave_h) $(store_h) $(iutil_h) $(idict_h) $(dstack_h)

idparam.$(OBJ): idparam.c $(GH) $(memory__h) $(string__h) $(errors_h)\
 $(gsmatrix_h) $(gsuid_h)\
 $(idict_h) $(idparam_h) $(ilevel_h) $(imemory_h) $(iname_h) $(iutil_h)\
 $(oper_h) $(store_h)

iparam.$(OBJ): iparam.c $(GH) $(memory__h) $(string__h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(iname_h) $(imemory_h) $(iparam_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(opcheck_h) $(store_h)

istack.$(OBJ): istack.c $(GH) $(memory__h) \
  $(errors_h) $(gsstruct_h) $(gsutil_h) \
  $(ialloc_h) $(istack_h) $(istruct_h) $(iutil_h) $(ivmspace_h) $(store_h)

iutil.$(OBJ): iutil.c $(GH) $(math__h) $(memory__h) $(string__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsutil_h) $(gxfont_h)\
 $(errors_h) $(idict_h) $(imemory_h) $(iutil_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(oper_h) $(store_h)

# ======================== PostScript Level 1 ======================== #

###### Include files

files_h=files.h
fname_h=fname.h
ichar_h=ichar.h
icharout_h=icharout.h
icolor_h=icolor.h
icontext_h=icontext.h $(imemory_h) $(istack_h)
icsmap_h=icsmap.h
ifont_h=ifont.h $(gsccode_h) $(gsstruct_h)
iht_h=iht.h
iimage_h=iimage.h
imain_h=imain.h $(gsexit_h)
imainarg_h=imainarg.h
iminst_h=iminst.h $(imain_h)
interp_h=interp.h
iparray_h=iparray.h
iscannum_h=iscannum.h
istream_h=istream.h
main_h=main.h $(iminst_h)
overlay_h=overlay.h
sbwbs_h=sbwbs.h
sfilter_h=sfilter.h $(gstypes_h)
shcgen_h=shcgen.h
smtf_h=smtf.h
# Nested include files
bfont_h=bfont.h $(ifont_h)
ifilter_h=ifilter.h $(istream_h) $(ivmspace_h)
igstate_h=igstate.h $(gsstate_h) $(gxstate_h) $(istruct_h)
iscan_h=iscan.h $(sa85x_h) $(sstring_h)
sbhc_h=sbhc.h $(shc_h)
# Include files for optional features
ibnum_h=ibnum.h

### Initialization and scanning

iconfig=iconfig$(CONFIG)
$(iconfig).$(OBJ): iconf.c $(stdio__h) \
  $(gconfig_h) $(gscdefs_h) $(gsmemory_h) \
  $(files_h) $(iminst_h) $(iref_h) $(ivmspace_h) $(opdef_h) $(stream_h)
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) iconf.c $(iconfig).c
	$(CCC) $(iconfig).c
	$(RM_) gconfig.h
	$(RM_) $(iconfig).c

iinit.$(OBJ): iinit.c $(GH) $(string__h)\
 $(gscdefs_h) $(gsexit_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h)\
 $(ilevel_h) $(iname_h) $(interp_h) $(opdef_h)\
 $(ipacked_h) $(iparray_h) $(iutil_h) $(ivmspace_h) $(store_h)

iscan.$(OBJ): iscan.c $(GH) $(memory__h)\
 $(ialloc_h) $(idict_h) $(dstack_h) $(errors_h) $(files_h)\
 $(ilevel_h) $(iutil_h) $(iscan_h) $(iscannum_h) $(istruct_h) $(ivmspace_h)\
 $(iname_h) $(ipacked_h) $(iparray_h) $(istream_h) $(ostack_h) $(store_h)\
 $(stream_h) $(strimpl_h) $(sfilter_h) $(scanchar_h)

iscannum.$(OBJ): iscannum.c $(GH) $(math__h)\
  $(errors_h) $(iscannum_h) $(scanchar_h) $(scommon_h) $(store_h)

### Streams

sfilter1.$(OBJ): sfilter1.c $(AK) $(stdio__h) $(memory__h) \
  $(sfilter_h) $(strimpl_h)

###### Operators

OP=$(GH) $(errors_h) $(oper_h)

### Non-graphics operators

zarith.$(OBJ): zarith.c $(OP) $(math__h) $(store_h)

zarray.$(OBJ): zarray.c $(OP) $(memory__h) $(ialloc_h) $(ipacked_h) $(store_h)

zcontrol.$(OBJ): zcontrol.c $(OP) $(string__h)\
 $(estack_h) $(files_h) $(ipacked_h) $(iutil_h) $(store_h) $(stream_h)

zdict.$(OBJ): zdict.c $(OP) \
  $(dstack_h) $(idict_h) $(ilevel_h) $(iname_h) $(ipacked_h) $(ivmspace_h) \
  $(store_h)

zfile.$(OBJ): zfile.c $(OP) $(memory__h) $(string__h) $(gp_h)\
 $(gsstruct_h) $(gxiodev_h) \
 $(ialloc_h) $(estack_h) $(files_h) $(fname_h) $(ilevel_h) $(interp_h) $(iutil_h)\
 $(isave_h) $(main_h) $(sfilter_h) $(stream_h) $(strimpl_h) $(store_h)

zfileio.$(OBJ): zfileio.c $(OP) $(gp_h) \
  $(files_h) $(ifilter_h) $(store_h) $(stream_h) $(strimpl_h) \
  $(gsmatrix_h) $(gxdevice_h) $(gxdevmem_h)

zfilter.$(OBJ): zfilter.c $(OP) $(memory__h)\
 $(gsstruct_h) $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
 $(sfilter_h) $(srlx_h) $(sstring_h) $(store_h) $(stream_h) $(strimpl_h)

zfname.$(OBJ): zfname.c $(OP) $(memory__h)\
 $(fname_h) $(gxiodev_h) $(ialloc_h) $(stream_h)

zfproc.$(OBJ): zfproc.c $(GH) $(memory__h)\
 $(errors_h) $(oper_h)\
 $(estack_h) $(files_h) $(gsstruct_h) $(ialloc_h) $(ifilter_h) $(istruct_h)\
 $(store_h) $(stream_h) $(strimpl_h)

zgeneric.$(OBJ): zgeneric.c $(OP) $(memory__h)\
 $(idict_h) $(estack_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(store_h)

ziodev.$(OBJ): ziodev.c $(OP) $(memory__h) $(stdio__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsstruct_h) $(gxiodev_h)\
 $(files_h) $(ialloc_h) $(ivmspace_h) $(store_h) $(stream_h)

zmath.$(OBJ): zmath.c $(OP) $(math__h) $(gxfarith_h) $(store_h)

zmisc.$(OBJ): zmisc.c $(OP) $(gscdefs_h) $(gp_h) \
  $(errno__h) $(memory__h) $(string__h) \
  $(ialloc_h) $(idict_h) $(dstack_h) $(iname_h) $(ivmspace_h) $(ipacked_h) $(store_h)

zpacked.$(OBJ): zpacked.c $(OP) \
  $(ialloc_h) $(idict_h) $(ivmspace_h) $(iname_h) $(ipacked_h) $(iparray_h) \
  $(istack_h) $(store_h)

zrelbit.$(OBJ): zrelbit.c $(OP) $(gsutil_h) $(store_h) $(idict_h)

zstack.$(OBJ): zstack.c $(OP) $(memory__h)\
 $(ialloc_h) $(istack_h) $(store_h)

zstring.$(OBJ): zstring.c $(OP) $(memory__h)\
 $(gsutil_h)\
 $(ialloc_h) $(iname_h) $(ivmspace_h) $(store_h)

zsysvm.$(OBJ): zsysvm.c $(GH)\
 $(ialloc_h) $(ivmspace_h) $(oper_h) $(store_h)

ztoken.$(OBJ): ztoken.c $(OP) \
  $(estack_h) $(files_h) $(gsstruct_h) $(iscan_h) \
  $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

ztype.$(OBJ): ztype.c $(OP) $(math__h) $(memory__h) $(string__h)\
 $(dstack_h) $(idict_h) $(imemory_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

zvmem.$(OBJ): zvmem.c $(OP)\
 $(dstack_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(isave_h) $(store_h) $(stream_h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h)

### Graphics operators

zchar.$(OBJ): zchar.c $(OP)\
 $(gsstruct_h) $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxchar_h) $(gxdevice_h) $(gxfont_h) $(gzpath_h) $(gzstate_h)\
 $(dstack_h) $(estack_h) $(ialloc_h) $(ichar_h) $(idict_h) $(ifont_h)\
 $(ilevel_h) $(iname_h) $(igstate_h) $(ipacked_h) $(store_h)

# zcharout is used for Type 1 and Type 42 fonts only.
zcharout.$(OBJ): zcharout.c $(OP)\
 $(gschar_h) $(gxdevice_h) $(gxfont_h)\
 $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h)\
 $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zcolor.$(OBJ): zcolor.c $(OP) \
  $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gxdevice_h) $(gxcmap_h) \
  $(ialloc_h) $(icolor_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zdevice.$(OBJ): zdevice.c $(OP) $(string__h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(interp_h) $(iparam_h) $(ivmspace_h)\
 $(gsmatrix_h) $(gsstate_h) $(gxdevice_h) $(store_h)

zfont.$(OBJ): zfont.c $(OP)\
 $(gschar_h) $(gsstruct_h) $(gxdevice_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)\
 $(ialloc_h) $(idict_h) $(igstate_h) $(iname_h) $(isave_h) $(ivmspace_h)\
 $(bfont_h) $(store_h)

zfont2.$(OBJ): zfont2.c $(OP) $(memory__h) $(string__h)\
 $(gsmatrix_h) $(gxdevice_h) $(gschar_h) $(gxfixed_h) $(gxfont_h)\
 $(ialloc_h) $(bfont_h) $(idict_h) $(idparam_h) $(ilevel_h) $(iname_h) $(istruct_h)\
 $(ipacked_h) $(store_h)

zgstate.$(OBJ): zgstate.c $(OP) $(math__h)\
 $(gsmatrix_h) $(ialloc_h) $(idict_h) $(igstate_h) $(istruct_h) $(store_h)

zht.$(OBJ): zht.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)

zimage.$(OBJ): zimage.c $(OP) \
  $(estack_h) $(ialloc_h) $(ifilter_h) $(igstate_h) $(iimage_h) $(ilevel_h) \
  $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h) \
  $(store_h) $(stream_h)

zmatrix.$(OBJ): zmatrix.c $(OP)\
 $(gsmatrix_h) $(igstate_h) $(gscoord_h) $(store_h)

zpaint.$(OBJ): zpaint.c $(OP)\
 $(gspaint_h) $(igstate_h)

zpath.$(OBJ): zpath.c $(OP) $(math__h) \
  $(gsmatrix_h) $(gspath_h) $(igstate_h) $(store_h)

# Define the base PostScript language interpreter.
# This is the subset of PostScript Level 1 required by our PDF reader.

INT1=idebug.$(OBJ) idict.$(OBJ) idparam.$(OBJ)
INT2=iinit.$(OBJ) interp.$(OBJ) iparam.$(OBJ) ireclaim.$(OBJ)
INT3=iscan.$(OBJ) iscannum.$(OBJ) istack.$(OBJ) iutil.$(OBJ)
INT4=scantab.$(OBJ) sfilter1.$(OBJ) sstring.$(OBJ) stream.$(OBJ)
Z1=zarith.$(OBJ) zarray.$(OBJ) zcontrol.$(OBJ) zdict.$(OBJ)
Z1OPS=zarith zarray zcontrol zdict
Z2=zfile.$(OBJ) zfileio.$(OBJ) zfilter.$(OBJ) zfname.$(OBJ) zfproc.$(OBJ)
Z2OPS=zfile zfileio zfilter zfproc
Z3=zgeneric.$(OBJ) ziodev.$(OBJ) zmath.$(OBJ) zmisc.$(OBJ) zpacked.$(OBJ)
Z3OPS=zgeneric ziodev zmath zmisc zpacked
Z4=zrelbit.$(OBJ) zstack.$(OBJ) zstring.$(OBJ) zsysvm.$(OBJ)
Z4OPS=zrelbit zstack zstring zsysvm
Z5=ztoken.$(OBJ) ztype.$(OBJ) zvmem.$(OBJ)
Z5OPS=ztoken ztype zvmem
Z6=zchar.$(OBJ) zcolor.$(OBJ) zdevice.$(OBJ) zfont.$(OBJ) zfont2.$(OBJ)
Z6OPS=zchar zcolor zdevice zfont zfont2
Z7=zgstate.$(OBJ) zht.$(OBJ) zimage.$(OBJ) zmatrix.$(OBJ) zpaint.$(OBJ) zpath.$(OBJ)
Z7OPS=zgstate zht zimage zmatrix zpaint zpath
# We have to be a little underhanded with *config.$(OBJ) so as to avoid
# circular definitions.
INT_OBJS=imainarg.$(OBJ) gsargs.$(OBJ) imain.$(OBJ) \
  $(INT1) $(INT2) $(INT3) $(INT4) \
  $(Z1) $(Z2) $(Z3) $(Z4) $(Z5) $(Z6) $(Z7)
INT_CONFIG=$(gconfig).$(OBJ) $(gscdefs).$(OBJ) $(iconfig).$(OBJ) \
  iccinit$(COMPILE_INITS).$(OBJ)
INT_ALL=$(INT_OBJS) $(INT_CONFIG)
# We omit libcore.dev, which should be included here, because problems
# with the Unix linker require libcore to appear last in the link list
# when libcore is really a library.
# We omit $(INT_CONFIG) from the dependency list because they have special
# dependency requirements and are added to the link list at the very end.
# zfilter.c shouldn't include the RLE and RLD filters, but we don't want to
# change this now.
psbase.dev: $(INT_MAK) $(ECHOGS_XE) $(INT_OBJS)\
 isupport.dev rld.dev rle.dev sfile.dev
	$(SETMOD) psbase imainarg.$(OBJ) gsargs.$(OBJ) imain.$(OBJ)
	$(ADDMOD) psbase -obj $(INT_CONFIG)
	$(ADDMOD) psbase -obj $(INT1)
	$(ADDMOD) psbase -obj $(INT2)
	$(ADDMOD) psbase -obj $(INT3)
	$(ADDMOD) psbase -obj $(INT4)
	$(ADDMOD) psbase -obj $(Z1)
	$(ADDMOD) psbase -oper $(Z1OPS)
	$(ADDMOD) psbase -obj $(Z2)
	$(ADDMOD) psbase -oper $(Z2OPS)
	$(ADDMOD) psbase -obj $(Z3)
	$(ADDMOD) psbase -oper $(Z3OPS)
	$(ADDMOD) psbase -obj $(Z4)
	$(ADDMOD) psbase -oper $(Z4OPS)
	$(ADDMOD) psbase -obj $(Z5)
	$(ADDMOD) psbase -oper $(Z5OPS)
	$(ADDMOD) psbase -obj $(Z6)
	$(ADDMOD) psbase -oper $(Z6OPS)
	$(ADDMOD) psbase -obj $(Z7)
	$(ADDMOD) psbase -oper $(Z7OPS)
	$(ADDMOD) psbase -iodev stdin stdout stderr lineedit statementedit
	$(ADDMOD) psbase -include isupport rld rle sfile

# -------------------------- Feature definitions -------------------------- #

# ---------------- Full Level 1 interpreter ---------------- #

level1.dev: $(INT_MAK) $(ECHOGS_XE) psbase.dev bcp.dev hsb.dev path1.dev type1.dev
	$(SETMOD) level1 -include psbase bcp hsb path1 type1
	$(ADDMOD) level1 -emulator PostScript PostScriptLevel1

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

color.dev: $(INT_MAK) $(ECHOGS_XE) cmyklib.dev colimlib.dev cmykread.dev
	$(SETMOD) color -include cmyklib colimlib cmykread

cmykread_=zcolor1.$(OBJ) zht1.$(OBJ)
cmykread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmykread_)
	$(SETMOD) cmykread $(cmykread_)
	$(ADDMOD) cmykread -oper zcolor1 zht1

zcolor1.$(OBJ): zcolor1.c $(OP) \
  $(gscolor1_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) \
  $(ialloc_h) $(icolor_h) $(iimage_h) $(estack_h) $(iutil_h) $(igstate_h) $(store_h)

zht1.$(OBJ): zht1.c $(OP) $(memory__h)\
 $(gsmatrix_h) $(gsstate_h) $(gsstruct_h) $(gxdevice_h) $(gzht_h)\
 $(ialloc_h) $(estack_h) $(igstate_h) $(iht_h) $(store_h)

# ---------------- HSB color ---------------- #

hsb_=zhsb.$(OBJ)
hsb.dev: $(INT_MAK) $(ECHOGS_XE) $(hsb_) hsblib.dev
	$(SETMOD) hsb $(hsb_)
	$(ADDMOD) hsb -include hsblib
	$(ADDMOD) hsb -oper zhsb

zhsb.$(OBJ): zhsb.c $(OP) \
  $(gshsb_h) $(igstate_h) $(store_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1_=zpath1.$(OBJ)
path1.dev: $(INT_MAK) $(ECHOGS_XE) $(path1_) path1lib.dev
	$(SETMOD) path1 $(path1_)
	$(ADDMOD) path1 -include path1lib
	$(ADDMOD) path1 -oper zpath1

zpath1.$(OBJ): zpath1.c $(OP) $(memory__h)\
 $(ialloc_h) $(estack_h) $(gspath_h) $(gsstruct_h) $(igstate_h) $(store_h)

# ================ Level-independent PostScript options ================ #

# ---------------- BCP filters ---------------- #

bcp_=sbcp.$(OBJ) zfbcp.$(OBJ)
bcp.dev: $(INT_MAK) $(ECHOGS_XE) $(bcp_)
	$(SETMOD) bcp $(bcp_)
	$(ADDMOD) bcp -oper zfbcp

sbcp.$(OBJ): sbcp.c $(AK) $(stdio__h) \
  $(sfilter_h) $(strimpl_h)

zfbcp.$(OBJ): zfbcp.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)

# ---------------- Incremental font loading ---------------- #
# (This only works for Type 1 fonts without eexec encryption.)

diskfont.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) diskfont -ps gs_diskf

# ---------------- Double-precision floats ---------------- #

double_=zdouble.$(OBJ)
double.dev: $(INT_MAK) $(ECHOGS_XE) $(double_)
	$(SETMOD) double $(double_)
	$(ADDMOD) double -oper zdouble

zdouble.$(OBJ): zdouble.c $(OP) $(ctype__h) $(math__h) $(memory__h) $(string__h) \
  $(gxfarith_h) $(store_h)

# ---------------- EPSF files with binary headers ---------------- #

epsf.dev: $(INT_MAK) $(ECHOGS_XE)
	$(SETMOD) epsf -ps gs_epsf

# ---------------- RasterOp ---------------- #
# This should be a separable feature in the core also....

rasterop.dev: $(INT_MAK) $(ECHOGS_XE) roplib.dev ropread.dev
	$(SETMOD) rasterop -include roplib ropread

ropread_=zrop.$(OBJ)
ropread.dev: $(INT_MAK) $(ECHOGS_XE) $(ropread_)
	$(SETMOD) ropread $(ropread_)
	$(ADDMOD) ropread -oper zrop

zrop.$(OBJ): zrop.c $(OP) $(memory__h)\
 $(gsrop_h) $(gsutil_h) $(gxdevice_h)\
 $(idict_h) $(idparam_h) $(igstate_h) $(store_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1.dev: $(INT_MAK) $(ECHOGS_XE) psf1lib.dev psf1read.dev
	$(SETMOD) type1 -include psf1lib psf1read

psf1read_=seexec.$(OBJ) zchar1.$(OBJ) zcharout.$(OBJ) zfont1.$(OBJ) zmisc1.$(OBJ)
psf1read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf1read_)
	$(SETMOD) psf1read $(psf1read_)
	$(ADDMOD) psf1read -oper zchar1 zfont1 zmisc1
	$(ADDMOD) psf1read -ps gs_type1

seexec.$(OBJ): seexec.c $(AK) $(stdio__h) \
  $(gscrypt1_h) $(scanchar_h) $(sfilter_h) $(strimpl_h)

zchar1.$(OBJ): zchar1.c $(OP) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gxfont_h) $(gxfont1_h) $(gxtype1_h) $(gzstate_h) \
  $(estack_h) $(ialloc_h) $(ichar_h) $(icharout_h) \
  $(idict_h) $(ifont_h) $(igstate_h) $(store_h)

zfont1.$(OBJ): zfont1.c $(OP) \
  $(gsmatrix_h) $(gxdevice_h) $(gschar_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont1_h) \
  $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(store_h)

zmisc1.$(OBJ): zmisc1.c $(OP) $(memory__h)\
 $(gscrypt1_h)\
 $(idict_h) $(idparam_h) $(ifilter_h)\
 $(sfilter_h) $(stream_h) $(strimpl_h)

# -------------- Compact Font Format and Type 2 charstrings ------------- #

cff.dev: $(INT_MAK) $(ECHOGS_XE) gs_cff.ps psl2int.dev
	$(SETMOD) cff -ps gs_cff

type2.dev: $(INT_MAK) $(ECHOGS_XE) type1.dev psf2lib.dev
	$(SETMOD) type2 -include psf2lib

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

# Native TrueType support
ttfont.dev: $(INT_MAK) $(ECHOGS_XE) type42.dev
	$(SETMOD) ttfont -include type42
	$(ADDMOD) ttfont -ps gs_mro_e gs_wan_e gs_ttf

# Type 42 (embedded TrueType) support
type42read_=zchar42.$(OBJ) zcharout.$(OBJ) zfont42.$(OBJ)
type42.dev: $(INT_MAK) $(ECHOGS_XE) $(type42read_) ttflib.dev
	$(SETMOD) type42 $(type42read_)
	$(ADDMOD) type42 -include ttflib	
	$(ADDMOD) type42 -oper zchar42 zfont42
	$(ADDMOD) type42 -ps gs_typ42

zchar42.$(OBJ): zchar42.c $(OP) \
  $(gsmatrix_h) $(gspaint_h) $(gspath_h) \
  $(gxfixed_h) $(gxchar_h) $(gxfont_h) $(gxfont42_h) \
  $(gxistate_h) $(gxpath_h) $(gzstate_h) \
  $(dstack_h) $(estack_h) $(ichar_h) $(icharout_h) \
  $(ifont_h) $(igstate_h) $(store_h)

zfont42.$(OBJ): zfont42.c $(OP) \
  $(gsccode_h) $(gsmatrix_h) $(gxfont_h) $(gxfont42_h) \
  $(bfont_h) $(idict_h) $(idparam_h) $(store_h)

# ======================== Precompilation options ======================== #

# ---------------- Precompiled fonts ---------------- #
# See fonts.txt for more information.

ccfont_h=ccfont.h $(std_h) $(gsmemory_h) $(iref_h) $(ivmspace_h) $(store_h)

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
$(gconfigf_h): $(MAKEFILE) $(INT_MAK) $(GENCONF_XE)
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
	$(EXP)genconf ccfonts_.dev -n gs -f $(gconfigf_h)

# We separate icfontab.dev from ccfonts.dev so that a customer can put
# compiled fonts into a separate shared library.

icfontab=icfontab$(CONFIG)

# Define ccfont_table separately, so it can be set from the command line
# to select an alternate compiled font table.
ccfont_table=$(icfontab)

$(icfontab).dev: $(MAKEFILE) $(INT_MAK) $(ECHOGS_XE) $(icfontab).$(OBJ) \
  $(ccfonts1_) $(ccfonts2_) $(ccfonts3_) $(ccfonts4_) $(ccfonts5_) \
  $(ccfonts6_) $(ccfonts7_) $(ccfonts8_) $(ccfonts9_) $(ccfonts10_) \
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

$(icfontab).$(OBJ): icfontab.c $(AK) $(ccfont_h) $(gconfigf_h)
	$(CP_) $(gconfigf_h) gconfigf.h
	$(CCCF) icfontab.c

# Strictly speaking, ccfonts shouldn't need to include type1,
# since one could choose to precompile only Type 0 fonts,
# but getting this exactly right would be too much work.
ccfonts=ccfonts$(CONFIG)
$(ccfonts).dev: $(MAKEFILE) $(INT_MAK) type1.dev iccfont.$(OBJ) \
  $(ccfont_table).dev
	$(SETMOD) $(ccfonts) -include type1
	$(ADDMOD) $(ccfonts) -include $(ccfont_table)
	$(ADDMOD) $(ccfonts) -obj iccfont.$(OBJ)
	$(ADDMOD) $(ccfonts) -oper ccfonts
	$(ADDMOD) $(ccfonts) -ps $(ccfonts_ps)

iccfont.$(OBJ): iccfont.c $(GH) $(string__h)\
 $(gsstruct_h) $(ccfont_h) $(errors_h)\
 $(ialloc_h) $(idict_h) $(ifont_h) $(iname_h) $(isave_h) $(iutil_h)\
 $(oper_h) $(ostack_h) $(store_h) $(stream_h) $(strimpl_h) $(sfilter_h) $(iscan_h)
	$(CCCF) iccfont.c

# ---------------- Compiled initialization code ---------------- #

# We select either iccinit0 or iccinit1 depending on COMPILE_INITS.

iccinit0.$(OBJ): iccinit0.c $(stdpre_h)
	$(CCCF) iccinit0.c

iccinit1.$(OBJ): gs_init.$(OBJ)
	$(CP_) gs_init.$(OBJ) iccinit1.$(OBJ)

# All the gs_*.ps files should be prerequisites of gs_init.c,
# but we don't have any convenient list of them.
gs_init.c: $(GS_INIT) $(GENINIT_XE) $(gconfig_h)
	$(EXP)geninit $(GS_INIT) $(gconfig_h) -c gs_init.c

gs_init.$(OBJ): gs_init.c $(stdpre_h)
	$(CCCF) gs_init.c

# ======================== PostScript Level 2 ======================== #

level2.dev: $(INT_MAK) $(ECHOGS_XE) \
 cidfont.dev cie.dev cmapread.dev compfont.dev dct.dev devctrl.dev dpsand2.dev\
 filter.dev level1.dev pattern.dev psl2lib.dev psl2read.dev sepr.dev\
 type42.dev xfilter.dev
	$(SETMOD) level2 -include cidfont cie cmapread compfont
	$(ADDMOD) level2 -include dct devctrl dpsand2 filter
	$(ADDMOD) level2 -include level1 pattern psl2lib psl2read
	$(ADDMOD) level2 -include sepr type42 xfilter
	$(ADDMOD) level2 -emulator PostScript PostScriptLevel2

# Define basic Level 2 language support.
# This is the minimum required for CMap and CIDFont support.

psl2int_=iutil2.$(OBJ) zmisc2.$(OBJ) zusparam.$(OBJ)
psl2int.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2int_) dps2int.dev
	$(SETMOD) psl2int $(psl2int_)
	$(ADDMOD) psl2int -include dps2int
	$(ADDMOD) psl2int -oper zmisc2 zusparam
	$(ADDMOD) psl2int -ps gs_lev2 gs_res

iutil2.$(OBJ): iutil2.c $(GH) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsutil_h)\
 $(errors_h) $(opcheck_h) $(imemory_h) $(iutil_h) $(iutil2_h)

zmisc2.$(OBJ): zmisc2.c $(OP) $(memory__h) $(string__h)\
 $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(ilevel_h) $(iname_h) $(iutil2_h) $(ivmspace_h) $(store_h)

# Note that zusparam includes both Level 1 and Level 2 operators.
zusparam.$(OBJ): zusparam.c $(OP) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gsfont_h) $(gsstruct_h) $(gsutil_h) $(gxht_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(iparam_h) $(dstack_h) $(estack_h)\
 $(iname_h) $(iutil2_h) $(store_h)

# Define full Level 2 support.

psl2read_=zcolor2.$(OBJ) zcsindex.$(OBJ) zht2.$(OBJ) zimage2.$(OBJ)
# Note that zmisc2 includes both Level 1 and Level 2 operators.
psl2read.dev: $(INT_MAK) $(ECHOGS_XE) $(psl2read_) psl2int.dev dps2read.dev
	$(SETMOD) psl2read $(psl2read_)
	$(ADDMOD) psl2read -include psl2int dps2read
	$(ADDMOD) psl2read -oper zcolor2_l2 zcsindex_l2
	$(ADDMOD) psl2read -oper zht2_l2 zimage2_l2

zcolor2.$(OBJ): zcolor2.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)

zcsindex.$(OBJ): zcsindex.c $(OP) $(memory__h) \
  $(gscolor_h) $(gsstruct_h) $(gxfixed_h) $(gxcolor2_h) $(gxcspace_h) $(gsmatrix_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

zht2.$(OBJ): zht2.c $(OP) \
  $(gsstruct_h) $(gxdevice_h) $(gzht_h) \
  $(estack_h) $(ialloc_h) $(icolor_h) $(idict_h) $(idparam_h) $(igstate_h) \
  $(iht_h) $(store_h)

zimage2.$(OBJ): zimage2.c $(OP) $(math__h) $(memory__h)\
 $(gscolor_h) $(gscolor2_h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h)\
 $(idict_h) $(idparam_h) $(iimage_h) $(ilevel_h) $(igstate_h)

# ---------------- Device control ---------------- #
# This is a catch-all for setpagedevice and IODevices.

devctrl_=zdevice2.$(OBJ) ziodev2.$(OBJ) zmedia2.$(OBJ) zdevcal.$(OBJ)
devctrl.dev: $(INT_MAK) $(ECHOGS_XE) $(devctrl_)
	$(SETMOD) devctrl $(devctrl_)
	$(ADDMOD) devctrl -oper zdevice2_l2 ziodev2_l2 zmedia2_l2
	$(ADDMOD) devctrl -iodev null ram calendar
	$(ADDMOD) devctrl -ps gs_setpd

zdevice2.$(OBJ): zdevice2.c $(OP) $(math__h) $(memory__h)\
 $(dstack_h) $(estack_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h) $(store_h)\
 $(gxdevice_h) $(gsstate_h)

ziodev2.$(OBJ): ziodev2.c $(OP) $(string__h) $(gp_h)\
 $(gxiodev_h) $(stream_h) $(files_h) $(iparam_h) $(iutil2_h) $(store_h)

zmedia2.$(OBJ): zmedia2.c $(OP) $(math__h) $(memory__h) \
  $(gsmatrix_h) $(idict_h) $(idparam_h) $(iname_h) $(store_h)

zdevcal.$(OBJ): zdevcal.c $(GH) $(time__h) \
  $(gxiodev_h) $(iparam_h) $(istack_h)

# ---------------- Filters other than the ones in sfilter.c ---------------- #

# Standard Level 2 decoding filters only.  The PDF configuration uses this.
fdecode_=scantab.$(OBJ) sfilter2.$(OBJ) zfdecode.$(OBJ)
fdecode.dev: $(INT_MAK) $(ECHOGS_XE) $(fdecode_) cfd.dev lzwd.dev pdiff.dev pngp.dev rld.dev
	$(SETMOD) fdecode $(fdecode_)
	$(ADDMOD) fdecode -include cfd lzwd pdiff pngp rld
	$(ADDMOD) fdecode -oper zfdecode

zfdecode.$(OBJ): zfdecode.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) \
 $(sa85x_h) $(scf_h) $(scfx_h) $(sfilter_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) \
 $(store_h) $(stream_h) $(strimpl_h)

# Complete Level 2 filter capability.
filter_=zfilter2.$(OBJ)
filter.dev: $(INT_MAK) $(ECHOGS_XE) fdecode.dev $(filter_) cfe.dev lzwe.dev rle.dev
	$(SETMOD) filter -include fdecode
	$(ADDMOD) filter -obj $(filter_)
	$(ADDMOD) filter -include cfe lzwe rle
	$(ADDMOD) filter -oper zfilter2

zfilter2.$(OBJ): zfilter2.c $(OP) $(memory__h)\
  $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h) $(store_h) \
  $(sfilter_h) $(scfx_h) $(slzwx_h) $(spdiffx_h) $(spngpx_h) $(strimpl_h)

# Extensions beyond Level 2 standard.
xfilter_=sbhc.$(OBJ) sbwbs.$(OBJ) shcgen.$(OBJ) smtf.$(OBJ) \
 zfilterx.$(OBJ)
xfilter.dev: $(INT_MAK) $(ECHOGS_XE) $(xfilter_) pcxd.dev pngp.dev
	$(SETMOD) xfilter $(xfilter_)
	$(ADDMOD) xfilter -include pcxd
	$(ADDMOD) xfilter -oper zfilterx

sbhc.$(OBJ): sbhc.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(sbhc_h) $(shcgen_h) $(strimpl_h)

sbwbs.$(OBJ): sbwbs.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(sbwbs_h) $(sfilter_h) $(strimpl_h)

shcgen.$(OBJ): shcgen.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gserror_h) $(gserrors_h) $(gsmemory_h)\
 $(scommon_h) $(shc_h) $(shcgen_h)

smtf.$(OBJ): smtf.c $(AK) $(stdio__h) \
  $(smtf_h) $(strimpl_h)

zfilterx.$(OBJ): zfilterx.c $(OP) $(memory__h)\
 $(gsstruct_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifilter_h)\
 $(store_h) $(sfilter_h) $(sbhc_h) $(sbtx_h) $(sbwbs_h) $(shcgen_h)\
 $(smtf_h) $(spcxx_h) $(strimpl_h)

# ---------------- Binary tokens ---------------- #

btoken_=iscanbin.$(OBJ) zbseq.$(OBJ)
btoken.dev: $(INT_MAK) $(ECHOGS_XE) $(btoken_)
	$(SETMOD) btoken $(btoken_)
	$(ADDMOD) btoken -oper zbseq_l2
	$(ADDMOD) btoken -ps gs_btokn

bseq_h=bseq.h
btoken_h=btoken.h

iscanbin.$(OBJ): iscanbin.c $(GH) $(math__h) $(memory__h) $(errors_h)\
 $(gsutil_h) $(ialloc_h) $(ibnum_h) $(idict_h) $(iname_h)\
 $(iscan_h) $(iutil_h) $(ivmspace_h)\
 $(bseq_h) $(btoken_h) $(dstack_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

zbseq.$(OBJ): zbseq.c $(OP) $(memory__h)\
 $(ialloc_h) $(idict_h) $(isave_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)\
 $(iname_h) $(ibnum_h) $(btoken_h) $(bseq_h)

# ---------------- User paths & insideness testing ---------------- #

upath_=zupath.$(OBJ) ibnum.$(OBJ)
upath.dev: $(INT_MAK) $(ECHOGS_XE) $(upath_)
	$(SETMOD) upath $(upath_)
	$(ADDMOD) upath -oper zupath_l2

zupath.$(OBJ): zupath.c $(OP) \
  $(idict_h) $(dstack_h) $(iutil_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h) \
  $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h) \
  $(gxfixed_h) $(gxdevice_h) $(gzpath_h) $(gzstate_h)

# -------- Additions common to Display PostScript and Level 2 -------- #

dpsand2.dev: $(INT_MAK) $(ECHOGS_XE) btoken.dev color.dev upath.dev dps2lib.dev dps2read.dev
	$(SETMOD) dpsand2 -include btoken color upath dps2lib dps2read

dps2int_=zvmem2.$(OBJ) zdps1.$(OBJ)
# Note that zvmem2 includes both Level 1 and Level 2 operators.
dps2int.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2int_)
	$(SETMOD) dps2int $(dps2int_)
	$(ADDMOD) dps2int -oper zvmem2 zdps1_l2
	$(ADDMOD) dps2int -ps gs_dps1

dps2read_=ibnum.$(OBJ) zchar2.$(OBJ)
dps2read.dev: $(INT_MAK) $(ECHOGS_XE) $(dps2read_) dps2int.dev
	$(SETMOD) dps2read $(dps2read_)
	$(ADDMOD) dps2read -include dps2int
	$(ADDMOD) dps2read -oper ireclaim_l2 zchar2_l2
	$(ADDMOD) dps2read -ps gs_dps2

ibnum.$(OBJ): ibnum.c $(GH) $(math__h) $(memory__h)\
 $(errors_h) $(stream_h) $(ibnum_h) $(imemory_h) $(iutil_h)

zchar2.$(OBJ): zchar2.c $(OP)\
 $(gschar_h) $(gsmatrix_h) $(gspath_h) $(gsstruct_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h)\
 $(ialloc_h) $(ichar_h) $(estack_h) $(ifont_h) $(iname_h) $(igstate_h)\
 $(store_h) $(stream_h) $(ibnum_h)

zdps1.$(OBJ): zdps1.c $(OP) \
  $(gsmatrix_h) $(gspath_h) $(gspath2_h) $(gsstate_h) \
  $(ialloc_h) $(ivmspace_h) $(igstate_h) $(store_h) $(stream_h) $(ibnum_h)

zvmem2.$(OBJ): zvmem2.c $(OP) \
  $(estack_h) $(ialloc_h) $(ivmspace_h) $(store_h)

# ---------------- Display PostScript ---------------- #

dps_=zdps.$(OBJ) icontext.$(OBJ) zcontext.$(OBJ)
dps.dev: $(INT_MAK) $(ECHOGS_XE) dpslib.dev level2.dev $(dps_)
	$(SETMOD) dps -include dpslib level2
	$(ADDMOD) dps -obj $(dps_)
	$(ADDMOD) dps -oper zcontext zdps
	$(ADDMOD) dps -ps gs_dps

icontext.$(OBJ): icontext.c $(GH)\
 $(gsstruct_h) $(gxalloc_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(ostack_h)\
 $(icontext_h) $(igstate_h) $(interp_h) $(store_h)

zdps.$(OBJ): zdps.c $(OP)\
 $(gsdps_h) $(gsstate_h) $(igstate_h) $(iname_h) $(store_h)

zcontext.$(OBJ): zcontext.c $(OP) $(gp_h) $(memory__h)\
 $(gsexit_h) $(gsstruct_h) $(gsutil_h) $(gxalloc_h)\
 $(icontext_h) $(idict_h) $(igstate_h) $(istruct_h)\
 $(dstack_h) $(estack_h) $(ostack_h) $(store_h)

# The following #ifdef ... #endif are just a comment to mark a DPNEXT area.
#ifdef DPNEXT

# ---------------- NeXT Display PostScript ---------------- #
#**************** NOT READY FOR USE YET ****************#

# There should be a gsdpnext.c, but there isn't yet.
#dpsnext_=zdpnext.$(OBJ) gsdpnext.$(OBJ)
dpsnext_=zdpnext.$(OBJ)
dpsnext.dev: $(INT_MAK) $(ECHOGS_XE) dps.dev $(dpsnext_) gs_dpnxt.ps
	$(SETMOD) dpsnext -include dps
	$(ADDMOD) dpsnext -obj $(dpsnext_)
	$(ADDMOD) dpsnext -oper zdpnext
	$(ADDMOD) dpsnext -ps gs_dpnxt

zdpnext.$(OBJ): zdpnext.c $(OP)\
 $(gscspace_h) $(gsiparam_h) $(gsmatrix_h) $(gxcvalue_h) $(gxsample_h)\
 $(ialloc_h) $(igstate_h) $(iimage_h)

# See above re the following.
#endif				/* DPNEXT */

# -------- Composite (PostScript Type 0) font support -------- #

compfont.dev: $(INT_MAK) $(ECHOGS_XE) psf0lib.dev psf0read.dev
	$(SETMOD) compfont -include psf0lib psf0read

# We always include zfcmap.$(OBJ) because zfont0.c refers to it,
# and it's not worth the trouble to exclude.
psf0read_=zchar2.$(OBJ) zfcmap.$(OBJ) zfont0.$(OBJ)
psf0read.dev: $(INT_MAK) $(ECHOGS_XE) $(psf0read_)
	$(SETMOD) psf0read $(psf0read_)
	$(ADDMOD) psf0read -oper zfont0 zchar2 zfcmap

zfcmap.$(OBJ): zfcmap.c $(OP)\
 $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfcmap_h) $(gxfont_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifont_h) $(iname_h) $(store_h)

zfont0.$(OBJ): zfont0.c $(OP)\
 $(gschar_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfcmap_h) $(gxfixed_h) $(gxfont_h) $(gxfont0_h) $(gxmatrix_h)\
 $(gzstate_h)\
 $(bfont_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(iname_h)\
 $(store_h)

# ---------------- CMap support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cmapread_=zfcmap.$(OBJ)
cmapread.dev: $(INT_MAK) $(ECHOGS_XE) $(cmapread_) cmaplib.dev psl2int.dev
	$(SETMOD) cmapread $(cmapread_)
	$(ADDMOD) cmapread -include cmaplib psl2int
	$(ADDMOD) cmapread -oper zfcmap
	$(ADDMOD) cmapread -ps gs_cmap

# ---------------- CIDFont support ---------------- #
# Note that this requires at least minimal Level 2 support,
# because it requires findresource.

cidread_=zcid.$(OBJ)
cidfont.dev: $(INT_MAK) $(ECHOGS_XE) psf1read.dev psl2int.dev type42.dev\
 $(cidread_)
	$(SETMOD) cidfont $(cidread_)
	$(ADDMOD) cidfont -include psf1read psl2int type42
	$(ADDMOD) cidfont -ps gs_cidfn
	$(ADDMOD) cidfont -oper zcid

zcid.$(OBJ): zcid.c $(OP)\
 $(gsccode_h) $(gsmatrix_h) $(gxfont_h)\
 $(bfont_h) $(iname_h) $(store_h)

# ---------------- CIE color ---------------- #

cieread_=zcie.$(OBJ) zcrd.$(OBJ)
cie.dev: $(INT_MAK) $(ECHOGS_XE) $(cieread_) cielib.dev
	$(SETMOD) cie $(cieread_)
	$(ADDMOD) cie -oper zcie_l2 zcrd_l2
	$(ADDMOD) cie -include cielib

icie_h=icie.h

zcie.$(OBJ): zcie.c $(OP) $(math__h) $(memory__h) \
  $(gscolor2_h) $(gscie_h) $(gsstruct_h) $(gxcspace_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

zcrd.$(OBJ): zcrd.c $(OP) $(math__h) \
  $(gscspace_h) $(gscolor2_h) $(gscie_h) $(gsstruct_h) \
  $(ialloc_h) $(icie_h) $(idict_h) $(idparam_h) $(estack_h) \
  $(isave_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- Pattern color ---------------- #

pattern.dev: $(INT_MAK) $(ECHOGS_XE) patlib.dev patread.dev
	$(SETMOD) pattern -include patlib patread

patread_=zpcolor.$(OBJ)
patread.dev: $(INT_MAK) $(ECHOGS_XE) $(patread_)
	$(SETMOD) patread $(patread_)
	$(ADDMOD) patread -oper zpcolor_l2

zpcolor.$(OBJ): zpcolor.c $(OP)\
 $(gscolor_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
   $(gxfixed_h) $(gxpcolor_h)\
 $(estack_h) $(ialloc_h) $(idict_h) $(idparam_h) $(igstate_h) $(istruct_h)\
 $(store_h)

# ---------------- Separation color ---------------- #

seprread_=zcssepr.$(OBJ)
sepr.dev: $(INT_MAK) $(ECHOGS_XE) $(seprread_) seprlib.dev
	$(SETMOD) sepr $(seprread_)
	$(ADDMOD) sepr -oper zcssepr_l2
	$(ADDMOD) sepr -include seprlib

zcssepr.$(OBJ): zcssepr.c $(OP) \
  $(gscolor_h) $(gscsepr_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) \
  $(ialloc_h) $(icsmap_h) $(estack_h) $(igstate_h) $(ivmspace_h) $(store_h)

# ---------------- Functions ---------------- #

ifunc_h=ifunc.h

# Generic support, and FunctionType 0.
funcread_=zfunc.$(OBJ) zfunc0.$(OBJ)
func.dev: $(INT_MAK) $(ECHOGS_XE) $(funcread_) funclib.dev
	$(SETMOD) func $(funcread_)
	$(ADDMOD) func -oper zfunc zfunc0
	$(ADDMOD) func -include funclib

zfunc.$(OBJ): zfunc.c $(OP) $(memory__h)\
 $(gsfunc_h) $(gsstruct_h)\
 $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h) $(store_h)

zfunc0.$(OBJ): zfunc0.c $(OP) $(memory__h)\
 $(gsdsrc_h) $(gsfunc_h) $(gsfunc0_h)\
 $(stream_h)\
 $(files_h) $(ialloc_h) $(idict_h) $(idparam_h) $(ifunc_h)

# ---------------- DCT filters ---------------- #
# The definitions for jpeg*.dev are in jpeg.mak.

dct.dev: $(INT_MAK) $(ECHOGS_XE) dcte.dev dctd.dev
	$(SETMOD) dct -include dcte dctd

# Common code

dctc_=zfdctc.$(OBJ)

zfdctc.$(OBJ): zfdctc.c $(GH) $(memory__h) $(stdio__h)\
 $(errors_h) $(opcheck_h)\
 $(idict_h) $(idparam_h) $(imemory_h) $(ipacked_h) $(iutil_h)\
 $(sdct_h) $(sjpeg_h) $(strimpl_h)\
 jpeglib.h

# Encoding (compression)

dcte_=$(dctc_) zfdcte.$(OBJ)
dcte.dev: $(INT_MAK) $(ECHOGS_XE) sdcte.dev $(dcte_)
	$(SETMOD) dcte -include sdcte
	$(ADDMOD) dcte -obj $(dcte_)
	$(ADDMOD) dcte -oper zfdcte

zfdcte.$(OBJ): zfdcte.c $(OP) $(memory__h) $(stdio__h)\
  $(idict_h) $(idparam_h) $(ifilter_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jpeglib.h

# Decoding (decompression)

dctd_=$(dctc_) zfdctd.$(OBJ)
dctd.dev: $(INT_MAK) $(ECHOGS_XE) sdctd.dev $(dctd_)
	$(SETMOD) dctd -include sdctd
	$(ADDMOD) dctd -obj $(dctd_)
	$(ADDMOD) dctd -oper zfdctd

zfdctd.$(OBJ): zfdctd.c $(OP) $(memory__h) $(stdio__h)\
 $(ifilter_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) \
 jpeglib.h

# ---------------- zlib/Flate filters ---------------- #

fzlib.dev: $(INT_MAK) $(ECHOGS_XE) zfzlib.$(OBJ) szlibe.dev szlibd.dev
	$(SETMOD) fzlib -include szlibe szlibd
	$(ADDMOD) fzlib -obj zfzlib.$(OBJ)
	$(ADDMOD) fzlib -oper zfzlib

zfzlib.$(OBJ): zfzlib.c $(OP) \
  $(errors_h) $(idict_h) $(ifilter_h) \
  $(spdiffx_h) $(spngpx_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) zfzlib.c

# ================================ PDF ================================ #

# We need most of the Level 2 interpreter to do PDF, but not all of it.
# In fact, we don't even need all of a Level 1 interpreter.

# Because of the way the PDF encodings are defined, they must get loaded
# before we install the Level 2 resource machinery.
# On the other hand, the PDF .ps files must get loaded after
# level2dict is defined.
pdfmin.dev: $(INT_MAK) $(ECHOGS_XE)\
 psbase.dev color.dev dps2lib.dev dps2read.dev\
 fdecode.dev type1.dev pdffonts.dev psl2lib.dev psl2read.dev pdfread.dev
	$(SETMOD) pdfmin -include psbase color dps2lib dps2read
	$(ADDMOD) pdfmin -include fdecode type1
	$(ADDMOD) pdfmin -include pdffonts psl2lib psl2read pdfread
	$(ADDMOD) pdfmin -emulator PDF

pdf.dev: $(INT_MAK) $(ECHOGS_XE)\
 pdfmin.dev cff.dev cidfont.dev cie.dev compfont.dev cmapread.dev dctd.dev\
 func.dev ttfont.dev type2.dev
	$(SETMOD) pdf -include pdfmin cff cidfont cie cmapread compfont dctd
	$(ADDMOD) pdf -include func ttfont type2

# Reader only

pdffonts.dev: $(INT_MAK) $(ECHOGS_XE) \
  gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps
	$(SETMOD) pdffonts -ps gs_mex_e gs_mro_e gs_pdf_e gs_wan_e

# pdf_2ps must be the last .ps file loaded.
pdfread.dev: $(INT_MAK) $(ECHOGS_XE) fzlib.dev
	$(SETMOD) pdfread -include fzlib
	$(ADDMOD) pdfread -ps gs_pdf gs_l2img
	$(ADDMOD) pdfread -ps pdf_base pdf_draw pdf_font pdf_main pdf_sec
	$(ADDMOD) pdfread -ps pdf_2ps

# ============================= Main program ============================== #

gs.$(OBJ): gs.c $(GH) \
  $(imain_h) $(imainarg_h) $(iminst_h)

imainarg.$(OBJ): imainarg.c $(GH) $(ctype__h) $(memory__h) $(string__h) \
  $(gp_h) \
  $(gsargs_h) $(gscdefs_h) $(gsdevice_h) $(gsmdebug_h) $(gxdevice_h) $(gxdevmem_h) \
  $(errors_h) $(estack_h) $(files_h) \
  $(ialloc_h) $(imain_h) $(imainarg_h) $(iminst_h) \
  $(iname_h) $(interp_h) $(iscan_h) $(iutil_h) $(ivmspace_h) \
  $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

imain.$(OBJ): imain.c $(GH) $(memory__h) $(string__h)\
 $(gp_h) $(gslib_h) $(gsmatrix_h) $(gsutil_h) $(gxdevice_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(idebug_h) $(idict_h) $(iname_h) $(interp_h)\
 $(isave_h) $(iscan_h) $(ivmspace_h)\
 $(main_h) $(oper_h) $(ostack_h)\
 $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)

interp.$(OBJ): interp.c $(GH) $(memory__h) $(string__h)\
 $(gsstruct_h)\
 $(dstack_h) $(errors_h) $(estack_h) $(files_h)\
 $(ialloc_h) $(iastruct_h) $(inamedef_h) $(idict_h) $(interp_h) $(ipacked_h)\
 $(iscan_h) $(isave_h) $(istack_h) $(iutil_h) $(ivmspace_h)\
 $(oper_h) $(ostack_h) $(sfilter_h) $(store_h) $(stream_h) $(strimpl_h)
	$(CCINT) interp.c

ireclaim.$(OBJ): ireclaim.c $(GH) \
  $(errors_h) $(gsstruct_h) $(iastate_h) $(opdef_h) $(store_h) \
  $(dstack_h) $(estack_h) $(ostack_h)
