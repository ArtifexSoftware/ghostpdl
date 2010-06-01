#    Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL XL interpreters.
# Users of this makefile must define the following:
#	GLSRCDIR - the GS library source directory
#	GLGENDIR - the GS library generated file directory
#	PLSRCDIR - the PCL* support library source directory
#	PLOBJDIR - the PCL* support library object / executable directory
#	PXLSRCDIR - the source directory
#	PXLGENDIR - the directory for source files generated during building
#	PXLOBJDIR - the object / executable directory
#	TOP_OBJ - object file to top-level interpreter API

PLOBJ=$(PLOBJDIR)$(D)

PXLSRC=$(PXLSRCDIR)$(D)
PXLGEN=$(PXLGENDIR)$(D)
PXLOBJ=$(PXLOBJDIR)$(D)
PXLO_=$(O_)$(PXLOBJ)

PXLCCC=$(CC_) $(I_)$(PXLSRCDIR)$(_I) $(I_)$(PXLGENDIR)$(_I) $(I_)$(PCLSRCDIR)$(_I) $(I_)$(PLSRCDIR)$(_I) $(I_)$(GLSRCDIR)$(_I) $(I_)$(GLGENDIR)$(_I) $(C_)

# Define the name of this makefile.
PXL_MAK=$(PXLSRC)pxl.mak

pxl.clean: pxl.config-clean pxl.clean-not-config-clean

pxl.clean-not-config-clean: clean_gs
	$(RM_) $(PXLOBJ)*.$(OBJ)
	$(RMN_) $(PXLGEN)pxbfont.c $(PXLGEN)pxsymbol.c $(PXLGEN)pxsymbol.h
	$(RM_) $(PXLOBJ)devs.tr6

# devices are still created in the current directory.  Until that 
# is fixed we will have to remove them from both directories.
pxl.config-clean:
	$(RM_) $(PXLOBJ)*.dev
	$(RM_) *.dev

################ PCL XL ################

pxattr_h=$(PXLSRC)pxattr.h $(gdevpxat_h)
pxbfont_h=$(PXLSRC)pxbfont.h
pxenum_h=$(PXLSRC)pxenum.h $(gdevpxen_h)
pxerrors_h=$(PXLSRC)pxerrors.h
pxfont_h=$(PXLSRC)pxfont.h $(plfont_h)
pxptable_h=$(PXLSRC)pxptable.h
pxtag_h=$(PXLSRC)pxtag.h $(gdevpxop_h)
pxsymbol_h=$(PXLGEN)pxsymbol.h
pxvalue_h=$(PXLSRC)pxvalue.h $(gstypes_h) $(pxattr_h) $(stdint__h)
pxdict_h=$(PXLSRC)pxdict.h $(pldict_h) $(pxvalue_h)
pxgstate_h=$(PXLSRC)pxgstate.h $(gsccolor_h) $(gsiparam_h) $(gsmatrix_h) $(gsrefct_h) $(gxbitmap_h) $(gxfixed_h) $(plsymbol_h) $(pxdict_h) $(pxenum_h)
pxoper_h=$(PXLSRC)pxoper.h $(gserror_h) $(pxattr_h) $(pxerrors_h) $(pxvalue_h)
pxparse_h=$(PXLSRC)pxparse.h $(pxoper_h)
pxstate_h=$(PXLSRC)pxstate.h $(gsmemory_h) $(pxgstate_h) $(pltop_h)
pxpthr_h=$(PXLSRC)pxpthr.h

$(PXLOBJ)pxbfont.$(OBJ): $(PXLSRC)pxbfont.c $(AK) $(stdpre_h)\
 $(pxbfont_h)
	$(PXLCCC) $(PXLSRC)pxbfont.c $(PXLO_)pxbfont.$(OBJ)

$(PXLOBJ)pxerrors.$(OBJ): $(PXLSRC)pxerrors.c $(AK)\
 $(memory__h) $(stdio__h) $(string__h)\
 $(gsccode_h) $(gscoord_h) $(gsmatrix_h) $(gsmemory_h)\
 $(gspaint_h) $(gspath_h) $(gsstate_h) $(gstypes_h) $(gsutil_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h) $(scommon_h)\
 $(pxbfont_h) $(pxerrors_h) $(pxfont_h) $(pxparse_h) $(pxptable_h) $(pxstate_h)
	$(PXLCCC) $(PXLSRC)pxerrors.c $(PXLO_)pxerrors.$(OBJ)

$(PXLOBJ)pxparse.$(OBJ): $(PXLSRC)pxparse.c $(AK) $(memory__h)\
 $(gdebug_h) $(gserror_h) $(gsio_h) $(gstypes_h)\
 $(plparse_h) $(pxpthr_h)\
 $(pxattr_h) $(pxenum_h) $(pxerrors_h) $(pxoper_h) $(pxparse_h) $(pxptable_h)\
 $(pxstate_h) $(pxtag_h) $(pxvalue_h) $(gsstruct_h)
	$(PXLCCC) $(PXLSRC)pxparse.c $(PXLO_)pxparse.$(OBJ)

$(PXLOBJ)pxstate.$(OBJ): $(PXLSRC)pxstate.c $(AK) $(pxfont_h) $(stdio__h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gxfcache_h)\
 $(pxstate_h) $(pxfont_h) $(pxparse_h) $(scommon_h)
	$(PXLCCC) $(PXLSRC)pxstate.c $(PXLO_)pxstate.$(OBJ)

# See the comment above under pxbfont.c.
#    /usr/bin/gs -I/usr/lib/ghostscript -q -dNODISPLAY -dHEADER pxsymbol.ps >pxsymbol.psh
$(PXLSRC)pxsymbol.psh: $(PXLSRC)pxsymbol.ps
	$(GS_XE) -q -dNODISPLAY -dHEADER $(PXLSRC)pxsymbol.ps >$(PXLGEN)pxsym_.psh
	$(CP_) $(PXLGEN)pxsym_.psh $(PXLSRC)pxsymbol.psh
	$(RM_) $(PXLGEN)pxsym_.psh

$(PXLGEN)pxsymbol.h: $(PXLSRC)pxsymbol.psh
	$(CP_) $(PXLSRC)pxsymbol.psh $(PXLGEN)pxsymbol.h

# See the comment above under pxbfont.c.
#    /usr/bin/gs -I/usr/lib/ghostscript -q -dNODISPLAY pxsymbol.ps >pxsymbol.psc
$(PXLSRC)pxsymbol.psc: $(PXLSRC)pxsymbol.ps
	$(GS_XE) -q -dNODISPLAY $(PXLSRC)pxsymbol.ps >$(PXLGEN)pxsym_.psc
	$(CP_) $(PXLGEN)pxsym_.psc $(PXLSRC)pxsymbol.psc
	$(RM_) $(PXLGEN)pxsym_.psc

$(PXLGEN)pxsymbol.c: $(PXLSRC)pxsymbol.psc
	$(CP_) $(PXLSRC)pxsymbol.psc $(PXLGEN)pxsymbol.c

$(PXLOBJ)pxsymbol.$(OBJ): $(PXLGEN)pxsymbol.c $(AK) $(pxsymbol_h)
	$(PXLCCC) $(PXLGEN)pxsymbol.c $(PXLO_)pxsymbol.$(OBJ)

$(PXLOBJ)pxptable.$(OBJ): $(PXLSRC)pxptable.c $(AK) $(std_h)\
 $(pxenum_h) $(pxoper_h) $(pxptable_h) $(pxvalue_h) $(pxstate_h)
	$(PXLCCC) $(PXLSRC)pxptable.c $(PXLO_)pxptable.$(OBJ)

$(PXLOBJ)pxpthr.$(OBJ): $(PXLSRC)pxpthr.c $(AK) \
 $(pcommand_h) $(pgmand_h) $(pcstate_h) $(pcparse_h) $(pctop_h)\
 $(pcpage_h) $(pxstate_h) $(pxoper_h) $(stdio__h) $(pxpthr_h)\
 $(gsicc_manage_h)
	$(PXLCCC) $(PXLSRC)pxpthr.c $(PXLO_)pxpthr.$(OBJ)

$(PXLOBJ)pxvalue.$(OBJ): $(PXLSRC)pxvalue.c $(AK) $(std_h) $(gsmemory_h) $(pxvalue_h)
	$(PXLCCC) $(PXLSRC)pxvalue.c $(PXLO_)pxvalue.$(OBJ)

# We have to break up pxl_other because of the MS-DOS command line
# limit of 120 characters.
pxl_other_obj1=$(PXLOBJ)pxbfont.$(OBJ) $(PXLOBJ)pxerrors.$(OBJ) $(PXLOBJ)pxparse.$(OBJ)
pxl_other_obj2=$(PXLOBJ)pxstate.$(OBJ) $(PXLOBJ)pxptable.$(OBJ) $(PXLOBJ)pxpthr.$(OBJ) $(PXLOBJ)pxvalue.$(OBJ)
pxl_other_obj=$(pxl_other_obj1) $(pxl_other_obj2)

# Operators

# This implements finding fonts by name, but doesn't implement any operators
# per se.
$(PXLOBJ)pxffont.$(OBJ): $(PXLSRC)pxffont.c $(AK) $(string__h)\
 $(gschar_h) $(gsmatrix_h) $(gx_h) $(gxfont_h) $(gxfont42_h)\
 $(pxfont_h) $(pxoper_h) $(pxstate_h) $(pjtop_h)
	$(PXLCCC) $(PXLSRC)pxffont.c $(PXLO_)pxffont.$(OBJ)

$(PXLOBJ)pxfont.$(OBJ): $(PXLSRC)pxfont.c $(AK) $(math__h) $(stdio__h) $(string__h)\
 $(gdebug_h) $(gschar_h) $(gscoord_h) $(gserrors_h) $(gsimage_h)\
 $(gspaint_h) $(gspath_h) $(gsstate_h) $(gsstruct_h) $(gsutil_h)\
 $(gxchar_h) $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gzstate_h)\
 $(plvalue_h)\
 $(pxfont_h) $(pxoper_h) $(pxptable_h) $(pxstate_h)
	$(PXLCCC) $(UFST_INCLUDES) $(PXLSRC)pxfont.c $(PXLO_)pxfont.$(OBJ)

$(PXLOBJ)pxgstate.$(OBJ): $(PXLSRC)pxgstate.c $(AK) $(math__h) $(memory__h) $(stdio__h)\
 $(gscoord_h) $(gsimage_h) $(gsmemory_h) $(gspath_h) $(gspath2_h) $(gsrop_h)\
 $(gsstate_h) $(gsstruct_h)  $(gdebug_h) $(gscie_h) $(gscolor2_h) $(gstypes_h)\
 $(gxcspace_h) $(gxpath_h) $(gzstate_h)\
 $(pxoper_h) $(pxptable_h) $(pxstate_h)  $(plsrgb_h)
	$(PXLCCC) $(PXLSRC)pxgstate.c $(PXLO_)pxgstate.$(OBJ)

$(PXLOBJ)pximage.$(OBJ): $(PXLSRC)pximage.c $(AK) $(std_h)\
 $(string__h) $(gscoord_h) $(gsimage_h) $(gspaint_h) $(gspath_h) $(gspath2_h)\
 $(gsrefct_h) $(gsrop_h) $(gsstate_h) $(gsstruct_h) $(gsuid_h) $(gsutil_h)\
 $(gxbitmap_h) $(gxcspace_h) $(gxdevice_h) $(gxcolor2_h) $(gxpcolor_h)\
 $(scommon_h) $(srlx_h) $(strimpl_h) $(gxdcolor_h) \
 $(pldraw_h) $(jpeglib__h) $(sdct_h) $(sjpeg_h)\
 $(pxerrors_h) $(pxptable_h) $(pxoper_h) $(pxstate_h) $(plsrgb_h) $(gdebug_h)
	$(PXLCCC) $(PXLSRC)pximage.c $(PXLO_)pximage.$(OBJ)

$(PXLOBJ)pxink.$(OBJ): $(PXLSRC)pxink.c $(math__h) $(stdio__h) $(memory__h)\
 $(gdebug_h) $(gscolor2_h) $(gscoord_h) $(gsimage_h) $(gsmemory_h) $(gspath_h)\
 $(gspath2_h) $(gstypes_h) $(gscie_h) $(gscrd_h) $(gsstate_h)\
 $(gxarith_h) $(gxcspace_h) $(gxdevice_h) $(gxht_h) $(gxstate_h)\
 $(pxoper_h) $(pxptable_h) $(pxstate_h) $(plht_h) $(pldraw_h)
	$(PXLCCC) $(PXLSRC)pxink.c $(PXLO_)pxink.$(OBJ)

$(PXLOBJ)pxpaint.$(OBJ): $(PXLSRC)pxpaint.c $(AK) $(math__h) $(stdio__h)\
 $(gscoord_h) $(gspaint_h) $(gspath_h) $(gspath2_h) $(gsrop_h) $(gsstate_h)\
 $(gxfarith_h) $(gxfixed_h) $(gxistate_h) $(gxmatrix_h) $(gxpath_h)\
 $(pxfont_h) $(pxoper_h) $(pxptable_h) $(pxstate_h)
	$(PXLCCC) $(PXLSRC)pxpaint.c $(PXLO_)pxpaint.$(OBJ)

$(PXLOBJ)pxsessio.$(OBJ): $(PXLSRC)pxsessio.c $(AK) $(math__h) $(stdio__h)\
 $(string__h) $(pxoper_h) $(pxstate_h) $(pxfont_h) \
 $(pjparse_h) $(gschar_h) $(gscoord_h) $(gserrors_h) $(gspaint_h) $(gsparam_h)\
 $(gsstate_h) $(gxfixed_h) $(gxpath_h) $(gxfcache_h)\
 $(gxdevice_h) $(pjtop_h) $(pllfont_h) $(pxptable_h) 
	$(PXLCCC) $(PXLSRC)pxsessio.c $(PXLO_)pxsessio.$(OBJ)

$(PXLOBJ)pxstream.$(OBJ): $(PXLSRC)pxstream.c $(AK) $(memory__h)\
 $(gsmemory_h) $(scommon_h)\
 $(pxoper_h) $(pxparse_h) $(pxstate_h)
	$(PXLCCC) $(PXLSRC)pxstream.c $(PXLO_)pxstream.$(OBJ)

# We have to break up pxl_ops because of the MS-DOS command line
# limit of 120 characters.
pxl_ops_obj1=$(PXLOBJ)pxffont.$(OBJ) $(PXLOBJ)pxfont.$(OBJ) $(PXLOBJ)pxgstate.$(OBJ) $(PXLOBJ)pximage.$(OBJ)
pxl_ops_obj2=$(PXLOBJ)pxink.$(OBJ) $(PXLOBJ)pxpaint.$(OBJ) $(PXLOBJ)pxsessio.$(OBJ) $(PXLOBJ)pxstream.$(OBJ)
pxl_ops_obj=$(pxl_ops_obj1) $(pxl_ops_obj2)

# Top-level API
$(PXL_TOP_OBJ): $(PXLSRC)pxtop.c $(AK) $(stdio__h)\
 $(string__h) $(gdebug_h) $(gp_h) $(gsdevice_h) $(gserrors_h) $(gsmemory_h)\
 $(gsstate_h) $(gsstruct_h) $(gspaint_h) $(gstypes_h) $(gxalloc_h) $(gxstate_h)\
 $(gsnogc_h) $(pltop_h) $(plparse_h)\
 $(pxattr_h) $(pxerrors_h) $(pxoper_h) $(pxparse_h) $(pxptable_h) $(pxstate_h)\
 $(pxfont_h) $(pxvalue_h) $(PXLGEN)pconf.h $(plfont_h)
	$(CP_) $(PXLGEN)pconf.h $(PXLGEN)pconfig.h
	$(PXLCCC) $(PXLSRC)pxtop.c $(PXLO_)pxtop.$(OBJ)

# Note that we must initialize pxfont before pxerrors.
$(PXLOBJ)pxl.dev: $(PXL_MAK) $(ECHOGS_XE) $(pxl_other_obj) $(pxl_ops_obj)\
 $(PLOBJ)pl.dev $(PLOBJ)pjl.dev $(PLOBJ)$(PXL_FONT_SCALER).dev
	$(SETMOD) $(PXLOBJ)pxl $(pxl_other_obj1)
	$(ADDMOD) $(PXLOBJ)pxl $(pxl_other_obj2)
	$(ADDMOD) $(PXLOBJ)pxl $(pxl_ops_obj1)
	$(ADDMOD) $(PXLOBJ)pxl $(pxl_ops_obj2)
	$(ADDMOD) $(PXLOBJ)pxl -include $(PLOBJ)pl $(PLOBJ)pjl $(PLOBJ)$(PXL_FONT_SCALER)
	$(ADDMOD) $(PXLOBJ)pxl -init pxfont pxerrors

