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

# (Platform-independent) makefile for graphics library and other support code.
# See the end of gs.mak for where this fits into the build process.

# Define the name of this makefile.
LIB_MAK=lib.mak

# Define the inter-dependencies of the .h files.
# Since not all versions of `make' defer expansion of macros,
# we must list these in bottom-to-top order.

# Generic files

arch_h=arch.h
stdpre_h=stdpre.h
std_h=std.h $(arch_h) $(stdpre_h)

# Platform interfaces

gp_h=gp.h
gpcheck_h=gpcheck.h
gpsync_h=gpsync.h

# Configuration definitions

# gconfig*.h are generated dynamically.
gconfig__h=gconfig_.h
gconfigv_h=gconfigv.h
gscdefs_h=gscdefs.h

# C library interfaces

# Because of variations in the "standard" header files between systems, and
# because we must include std.h before any file that includes sys/types.h,
# we define local include files named *_.h to substitute for <*.h>.

vmsmath_h=vmsmath.h

dos__h=dos_.h
ctype__h=ctype_.h $(std_h)
dirent__h=dirent_.h $(std_h) $(gconfig__h)
errno__h=errno_.h $(std_h)
malloc__h=malloc_.h $(std_h)
math__h=math_.h $(std_h) $(vmsmath_h)
memory__h=memory_.h $(std_h)
stat__h=stat_.h $(std_h)
stdio__h=stdio_.h $(std_h)
string__h=string_.h $(std_h)
time__h=time_.h $(std_h) $(gconfig__h)
windows__h=windows_.h

# Miscellaneous

gdebug_h=gdebug.h
gsalloc_h=gsalloc.h
gsargs_h=gsargs.h
gserror_h=gserror.h
gserrors_h=gserrors.h
gsexit_h=gsexit.h
gsgc_h=gsgc.h
gsio_h=gsio.h
gsmdebug_h=gsmdebug.h
gsmemraw_h=gsmemraw.h
gsmemory_h=gsmemory.h $(gsmemraw_h)
gsrefct_h=gsrefct.h
gsstruct_h=gsstruct.h
gstypes_h=gstypes.h
gx_h=gx.h $(stdio__h) $(gdebug_h) $(gserror_h) $(gsio_h) $(gsmemory_h) $(gstypes_h)

GX=$(AK) $(gx_h)
GXERR=$(GX) $(gserrors_h)

###### Support

### Include files

gsbitmap_h=gsbitmap.h $(gsstruct_h)
gsbitops_h=gsbitops.h
gsbittab_h=gsbittab.h
gsflip_h=gsflip.h
gsuid_h=gsuid.h
gsutil_h=gsutil.h
gxarith_h=gxarith.h
gxbitmap_h=gxbitmap.h $(gsbitmap_h) $(gstypes_h)
gxfarith_h=gxfarith.h $(gconfigv_h) $(gxarith_h)
gxfixed_h=gxfixed.h
gxobj_h=gxobj.h $(gxbitmap_h)
# Out of order
gxalloc_h=gxalloc.h $(gsalloc_h) $(gxobj_h)

### Executable code

gsalloc.$(OBJ): gsalloc.c $(GX) $(memory__h) $(string__h) \
  $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)

gsargs.$(OBJ): gsargs.c $(ctype__h) $(stdio__h) $(string__h)\
 $(gsargs_h) $(gsexit_h) $(gsmemory_h)

gsbitops.$(OBJ): gsbitops.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gsbitops_h) $(gstypes_h)

gsbittab.$(OBJ): gsbittab.c $(AK) $(stdpre_h) $(gsbittab_h)

# gsfemu is only used in FPU-less configurations, and currently only with gcc.
# We thought using CCLEAF would produce smaller code, but it actually
# produces larger code!
gsfemu.$(OBJ): gsfemu.c $(AK) $(std_h)

# gsflip is not part of the standard configuration: it's rather large,
# and no standard facility requires it.
gsflip.$(OBJ): gsflip.c $(GX) $(gsbittab_h) $(gsflip_h)
	$(CCLEAF) gsflip.c

gsmemory.$(OBJ): gsmemory.c $(GX) $(malloc__h) $(memory__h) \
  $(gsmdebug_h) $(gsrefct_h) $(gsstruct_h) $(gsmemraw_h)

gsmisc.$(OBJ): gsmisc.c $(GXERR) $(gconfigv_h) \
  $(malloc__h) $(math__h) $(memory__h) $(gpcheck_h) $(gxfarith_h) $(gxfixed_h)

# gsnogc currently is only used in library-only configurations.
gsnogc.$(OBJ): gsnogc.c $(GX)\
 $(gsgc_h) $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)

gsutil.$(OBJ): gsutil.c $(AK) $(memory__h) $(string__h) $(gconfigv_h)\
 $(gstypes_h) $(gsuid_h) $(gsutil_h)

###### Low-level facilities and utilities

### Include files

gdevbbox_h=gdevbbox.h
gdevmem_h=gdevmem.h $(gsbitops_h)
gdevmrop_h=gdevmrop.h

gsccode_h=gsccode.h
gsccolor_h=gsccolor.h $(gsstruct_h)
gscsel_h=gscsel.h
gscolor1_h=gscolor1.h
gscoord_h=gscoord.h
gscpm_h=gscpm.h
gsdevice_h=gsdevice.h
gsfcmap_h=gsfcmap.h $(gsccode_h)
gsfont_h=gsfont.h
gshsb_h=gshsb.h
gsht_h=gsht.h
gsht1_h=gsht1.h $(gsht_h)
gsiparam_h=gsiparam.h
gsjconf_h=gsjconf.h $(std_h)
gslib_h=gslib.h
gslparam_h=gslparam.h
gsmatrix_h=gsmatrix.h
gspaint_h=gspaint.h
gsparam_h=gsparam.h
gsparams_h=gsparams.h $(gsparam_h)
gspath2_h=gspath2.h
gspenum_h=gspenum.h
gsropt_h=gsropt.h
gsxfont_h=gsxfont.h
# Out of order
gschar_h=gschar.h $(gsccode_h) $(gscpm_h)
gscolor2_h=gscolor2.h $(gsccolor_h) $(gsuid_h) $(gxbitmap_h)
gsimage_h=gsimage.h $(gsiparam_h)
gsline_h=gsline.h $(gslparam_h)
gspath_h=gspath.h $(gspenum_h)
gsrop_h=gsrop.h $(gsropt_h)

gxbcache_h=gxbcache.h $(gxbitmap_h)
gxchar_h=gxchar.h $(gschar_h)
gxcindex_h=gxcindex.h
gxcvalue_h=gxcvalue.h
gxclio_h=gxclio.h
gxclip2_h=gxclip2.h
gxcolor2_h=gxcolor2.h $(gscolor2_h) $(gsrefct_h) $(gxbitmap_h)
gxcoord_h=gxcoord.h $(gscoord_h)
gxcpath_h=gxcpath.h
gxdda_h=gxdda.h
gxdevrop_h=gxdevrop.h
gxdevmem_h=gxdevmem.h
gxdither_h=gxdither.h
gxfcmap_h=gxfcmap.h $(gsfcmap_h) $(gsuid_h)
gxfont0_h=gxfont0.h
gxfrac_h=gxfrac.h
gxftype_h=gxftype.h
gxhttile_h=gxhttile.h
gxhttype_h=gxhttype.h
gxiodev_h=gxiodev.h $(stat__h)
gxline_h=gxline.h $(gslparam_h)
gxlum_h=gxlum.h
gxmatrix_h=gxmatrix.h $(gsmatrix_h)
gxpaint_h=gxpaint.h
gxpath_h=gxpath.h $(gscpm_h) $(gslparam_h) $(gspenum_h)
gxpcache_h=gxpcache.h
gxpcolor_h=gxpcolor.h $(gxpcache_h)
gxsample_h=gxsample.h
gxstate_h=gxstate.h
gxtmap_h=gxtmap.h
gxxfont_h=gxxfont.h $(gsccode_h) $(gsmatrix_h) $(gsuid_h) $(gsxfont_h)
# The following are out of order because they include other files.
gsdcolor_h=gsdcolor.h $(gsccolor_h) $(gxarith_h) $(gxbitmap_h) $(gxcindex_h) $(gxhttile_h)
gxdcolor_h=gxdcolor.h $(gscsel_h) $(gsdcolor_h) $(gsropt_h) $(gsstruct_h)
gxdevice_h=gxdevice.h $(stdio__h) $(gsdcolor_h) $(gsiparam_h) $(gsmatrix_h) \
  $(gsropt_h) $(gsstruct_h) $(gsxfont_h) \
  $(gxbitmap_h) $(gxcindex_h) $(gxcvalue_h) $(gxfixed_h)
gxdht_h=gxdht.h $(gsrefct_h) $(gxarith_h) $(gxhttype_h)
gxctable_h=gxctable.h $(gxfixed_h) $(gxfrac_h)
gxfcache_h=gxfcache.h $(gsuid_h) $(gsxfont_h) $(gxbcache_h) $(gxftype_h)
gxfont_h=gxfont.h $(gsfont_h) $(gsuid_h) $(gsstruct_h) $(gxftype_h)
gscie_h=gscie.h $(gsrefct_h) $(gxctable_h)
gscsepr_h=gscsepr.h
gscspace_h=gscspace.h
gxdcconv_h=gxdcconv.h $(gxfrac_h)
gxfmap_h=gxfmap.h $(gsrefct_h) $(gxfrac_h) $(gxtmap_h)
gxistate_h=gxistate.h $(gscsel_h) $(gsropt_h) $(gxcvalue_h) $(gxfixed_h) $(gxline_h) $(gxmatrix_h) $(gxtmap_h)
gxband_h=gxband.h $(gxclio_h)
gxclist_h=gxclist.h $(gscspace_h) $(gxbcache_h) $(gxclio_h) $(gxistate_h) $(gxband_h)
gxcmap_h=gxcmap.h $(gscsel_h) $(gxcvalue_h) $(gxfmap_h)
gxcspace_h=gxcspace.h $(gscspace_h) $(gsccolor_h) $(gscsel_h) $(gsstruct_h) $(gxfrac_h)
gxht_h=gxht.h $(gsht1_h) $(gsrefct_h) $(gxhttype_h) $(gxtmap_h)
gscolor_h=gscolor.h $(gxtmap_h)
gsstate_h=gsstate.h $(gscolor_h) $(gscsel_h) $(gsdevice_h) $(gsht_h) $(gsline_h)

gzacpath_h=gzacpath.h
gzcpath_h=gzcpath.h $(gxcpath_h)
gzht_h=gzht.h $(gscsel_h) $(gxdht_h) $(gxfmap_h) $(gxht_h) $(gxhttile_h)
gzline_h=gzline.h $(gxline_h)
gzpath_h=gzpath.h $(gsstruct_h) $(gxpath_h)
gzstate_h=gzstate.h $(gscpm_h) $(gsrefct_h) $(gsstate_h)\
 $(gxdcolor_h) $(gxistate_h) $(gxstate_h)

gdevprn_h=gdevprn.h $(memory__h) $(string__h) $(gx_h) \
  $(gserrors_h) $(gsmatrix_h) $(gsparam_h) $(gsutil_h) \
  $(gxdevice_h) $(gxdevmem_h) $(gxclist_h)

sa85x_h=sa85x.h
sbtx_h=sbtx.h
scanchar_h=scanchar.h
scommon_h=scommon.h $(gsmemory_h) $(gstypes_h) $(gsstruct_h)
sdct_h=sdct.h
shc_h=shc.h $(gsbittab_h)
siscale_h=siscale.h $(gconfigv_h)
sjpeg_h=sjpeg.h
slzwx_h=slzwx.h
spcxx_h=spcxx.h
spdiffx_h=spdiffx.h
spngpx_h=spngpx.h
srlx_h=srlx.h
sstring_h=sstring.h
strimpl_h=strimpl.h $(scommon_h) $(gstypes_h) $(gsstruct_h)
szlibx_h=szlibx.h
# Out of order
scf_h=scf.h $(shc_h)
scfx_h=scfx.h $(shc_h)
gximage_h=gximage.h $(gsiparam_h) $(gxcspace_h) $(gxdda_h) $(gxsample_h)\
 $(siscale_h) $(strimpl_h)

### Executable code

# gconfig and gscdefs are handled specially.  Currently they go in psbase
# rather than in libcore, which is clearly wrong.
gconfig=gconfig$(CONFIG)
$(gconfig).$(OBJ): gconf.c $(GX) \
  $(gscdefs_h) $(gconfig_h) $(gxdevice_h) $(gxiodev_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gconf.c $(gconfig).c
	$(CCC) $(gconfig).c
	$(RM_) gconfig.h
	$(RM_) $(gconfig).c

gscdefs=gscdefs$(CONFIG)
$(gscdefs).$(OBJ): gscdef.c $(stdpre_h) $(gscdefs_h) $(gconfig_h) $(MAKEFILE)
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c
	$(CP_) $(gconfig_h) gconfig.h
	$(CP_) gscdef.c $(gscdefs).c
	$(CCC) $(gscdefs).c
	$(RM_) gconfig.h
	$(RM_) $(gscdefs).c

gxacpath.$(OBJ): gxacpath.c $(GXERR) \
  $(gsdcolor_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h) \
  $(gxdevice_h) $(gxfixed_h) $(gxpaint_h) \
  $(gzacpath_h) $(gzcpath_h) $(gzpath_h)

gxbcache.$(OBJ): gxbcache.c $(GX) $(memory__h) \
  $(gsmdebug_h) $(gxbcache_h)

gxccache.$(OBJ): gxccache.c $(GXERR) $(gpcheck_h) \
  $(gscspace_h) $(gsimage_h) $(gsstruct_h) \
  $(gxchar_h) $(gxdevice_h) $(gxdevmem_h) $(gxfcache_h) \
  $(gxfixed_h) $(gxfont_h) $(gxhttile_h) $(gxmatrix_h) $(gxxfont_h) \
  $(gzstate_h) $(gzpath_h) $(gzcpath_h) 

gxccman.$(OBJ): gxccman.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfont_h) $(gxfcache_h) $(gxchar_h)\
 $(gxxfont_h) $(gzstate_h) $(gzpath_h)

gxcht.$(OBJ): gxcht.c $(GXERR) $(memory__h)\
 $(gsutil_h)\
 $(gxcmap_h) $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h)\
 $(gxmatrix_h) $(gzht_h)

gxcmap.$(OBJ): gxcmap.c $(GXERR) \
  $(gsccolor_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gxdither_h) \
  $(gxfarith_h) $(gxfrac_h) $(gxlum_h) $(gzstate_h)

gxcpath.$(OBJ): gxcpath.c $(GXERR)\
 $(gscoord_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gzpath_h) $(gzcpath_h)

gxdcconv.$(OBJ): gxdcconv.c $(GX) \
  $(gsdcolor_h) $(gxcmap_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gxfarith_h) $(gxistate_h) $(gxlum_h)

gxdcolor.$(OBJ): gxdcolor.c $(GX) \
  $(gsbittab_h) $(gxdcolor_h) $(gxdevice_h)

gxdither.$(OBJ): gxdither.c $(GX) \
  $(gsstruct_h) $(gsdcolor_h) \
  $(gxcmap_h) $(gxdevice_h) $(gxdither_h) $(gxlum_h) $(gzht_h)

gxfill.$(OBJ): gxfill.c $(GXERR) $(math__h) \
  $(gsstruct_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxhttile_h) \
  $(gxistate_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzpath_h)

gxht.$(OBJ): gxht.c $(GXERR) $(memory__h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gzht_h)

gximage.$(OBJ): gximage.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h) $(gsstruct_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gzpath_h) $(gzstate_h)\
 $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h)

gximage0.$(OBJ): gximage0.c $(GXERR) $(memory__h)\
 $(gxcpath_h) $(gxdevice_h) $(gximage_h)

gximage1.$(OBJ): gximage1.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsbittab_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)

gximage2.$(OBJ): gximage2.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)

gxpaint.$(OBJ): gxpaint.c $(GX) \
  $(gxdevice_h) $(gxhttile_h) $(gxpaint_h) $(gxpath_h) $(gzstate_h)

gxpath.$(OBJ): gxpath.c $(GXERR) \
  $(gsstruct_h) $(gxfixed_h) $(gzpath_h)

gxpath2.$(OBJ): gxpath2.c $(GXERR) $(math__h) \
  $(gxfixed_h) $(gxarith_h) $(gzpath_h)

gxpcopy.$(OBJ): gxpcopy.c $(GXERR) $(math__h) $(gconfigv_h) \
  $(gxfarith_h) $(gxfixed_h) $(gzpath_h)

gxpdash.$(OBJ): gxpdash.c $(GX) $(math__h) \
  $(gscoord_h) $(gsline_h) $(gsmatrix_h) \
  $(gxfixed_h) $(gzline_h) $(gzpath_h)

gxpflat.$(OBJ): gxpflat.c $(GX)\
 $(gxarith_h) $(gxfixed_h) $(gzpath_h)

gxsample.$(OBJ): gxsample.c $(GX)\
 $(gxsample_h)

gxstroke.$(OBJ): gxstroke.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gscoord_h) $(gsdcolor_h) $(gsdevice_h) \
  $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) \
  $(gxhttile_h) $(gxistate_h) $(gxmatrix_h) $(gxpaint_h) \
  $(gzcpath_h) $(gzline_h) $(gzpath_h)

###### Higher-level facilities

gschar.$(OBJ): gschar.c $(GXERR) $(memory__h) $(string__h)\
 $(gspath_h) $(gsstruct_h) \
 $(gxfixed_h) $(gxarith_h) $(gxmatrix_h) $(gxcoord_h) $(gxdevice_h) $(gxdevmem_h) \
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxfcache_h) $(gzpath_h) $(gzstate_h)

gscolor.$(OBJ): gscolor.c $(GXERR) \
  $(gsccolor_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzstate_h)

gscoord.$(OBJ): gscoord.c $(GXERR) $(math__h) \
  $(gsccode_h) $(gxcoord_h) $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) $(gxfont_h) \
  $(gxmatrix_h) $(gxpath_h) $(gzstate_h)

gsdevice.$(OBJ): gsdevice.c $(GXERR) $(ctype__h) $(memory__h) $(string__h) $(gp_h)\
 $(gscdefs_h) $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstruct_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdevmem_h) $(gzstate_h)

gsdevmem.$(OBJ): gsdevmem.c $(GXERR) $(math__h) $(memory__h) \
  $(gxarith_h) $(gxdevice_h) $(gxdevmem_h)

gsdparam.$(OBJ): gsdparam.c $(GXERR) $(memory__h) $(string__h) \
  $(gsparam_h) $(gxdevice_h) $(gxfixed_h)

gsfont.$(OBJ): gsfont.c $(GXERR) $(memory__h)\
 $(gschar_h) $(gsstruct_h) \
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)

gsht.$(OBJ): gsht.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gshtscr.$(OBJ): gshtscr.c $(GXERR) $(math__h) \
  $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

gsimage.$(OBJ): gsimage.c $(GXERR) $(memory__h)\
  $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxdevice_h) $(gzstate_h)

gsimpath.$(OBJ): gsimpath.c $(GXERR) \
  $(gsmatrix_h) $(gsstate_h) $(gspath_h)

gsinit.$(OBJ): gsinit.c $(memory__h) $(stdio__h) \
  $(gdebug_h) $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmemory_h)

gsiodev.$(OBJ): gsiodev.c $(GXERR) $(errno__h) $(string__h) \
  $(gp_h) $(gsparam_h) $(gxiodev_h)

gsline.$(OBJ): gsline.c $(GXERR) $(math__h) $(memory__h)\
 $(gsline_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzline_h)

gsmatrix.$(OBJ): gsmatrix.c $(GXERR) $(math__h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h)

gspaint.$(OBJ): gspaint.c $(GXERR) $(math__h) $(gpcheck_h)\
 $(gspaint_h) $(gspath_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdevmem_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzpath_h) $(gzstate_h)

gsparam.$(OBJ): gsparam.c $(GXERR) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsstruct_h)

gsparams.$(OBJ): gsparams.c $(gx_h) $(memory__h) $(gserrors_h) $(gsparam_h)

gspath.$(OBJ): gspath.c $(GXERR) \
  $(gscoord_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzcpath_h) $(gzpath_h) $(gzstate_h)

gsstate.$(OBJ): gsstate.c $(GXERR) $(memory__h)\
 $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
 $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxpcache_h) \
 $(gzstate_h) $(gzht_h) $(gzline_h) $(gzpath_h) $(gzcpath_h)

###### The internal devices

### The built-in device implementations:

# The bounding box device is not normally a free-standing device.
# To configure it as one for testing, change SETMOD to SETDEV, and also
# define TEST in gdevbbox.c.
bbox.dev: $(LIB_MAK) $(ECHOGS_XE) gdevbbox.$(OBJ)
	$(SETMOD) bbox gdevbbox.$(OBJ)

gdevbbox.$(OBJ): gdevbbox.c $(GXERR) $(math__h) $(memory__h) \
  $(gdevbbox_h) $(gsdevice_h) $(gsparam_h) \
  $(gxcpath_h) $(gxdevice_h) $(gxistate_h) $(gxpaint_h) $(gxpath_h)

gdevddrw.$(OBJ): gdevddrw.c $(GXERR) $(math__h) $(gpcheck_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)

gdevdflt.$(OBJ): gdevdflt.c $(GXERR) $(gpcheck_h)\
 $(gsbittab_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)

gdevnfwd.$(OBJ): gdevnfwd.c $(GX) \
  $(gxdevice_h)

# The render/RGB device is only here as an example, but we can configure
# it as a real device for testing.
rrgb.dev: $(LIB_MAK) $(ECHOGS_XE) gdevrrgb.$(OBJ) page.dev
	$(SETPDEV) rrgb gdevrrgb.$(OBJ)

gdevrrgb.$(OBJ): gdevrrgb.c $(AK)\
 $(gdevprn_h)

### The memory devices:

gdevabuf.$(OBJ): gdevabuf.c $(GXERR) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmem.$(OBJ): gdevmem.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm1.$(OBJ): gdevm1.c $(GX) $(memory__h) $(gsrop_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm2.$(OBJ): gdevm2.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm4.$(OBJ): gdevm4.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm8.$(OBJ): gdevm8.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm16.$(OBJ): gdevm16.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm24.$(OBJ): gdevm24.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevm32.$(OBJ): gdevm32.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

gdevmpla.$(OBJ): gdevmpla.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)

# Create a pseudo-"feature" for the entire graphics library.

LIB1s=gsalloc.$(OBJ) gsbitops.$(OBJ) gsbittab.$(OBJ)
LIB2s=gschar.$(OBJ) gscolor.$(OBJ) gscoord.$(OBJ) gsdevice.$(OBJ) gsdevmem.$(OBJ)
LIB3s=gsdparam.$(OBJ) gsfont.$(OBJ) gsht.$(OBJ) gshtscr.$(OBJ)
LIB4s=gsimage.$(OBJ) gsimpath.$(OBJ) gsinit.$(OBJ) gsiodev.$(OBJ)
LIB5s=gsline.$(OBJ) gsmatrix.$(OBJ) gsmemory.$(OBJ) gsmisc.$(OBJ)
LIB6s=gspaint.$(OBJ) gsparam.$(OBJ) gsparams.$(OBJ) gspath.$(OBJ) gsstate.$(OBJ) gsutil.$(OBJ)
LIB1x=gxacpath.$(OBJ) gxbcache.$(OBJ)
LIB2x=gxccache.$(OBJ) gxccman.$(OBJ) gxcht.$(OBJ) gxcmap.$(OBJ) gxcpath.$(OBJ)
LIB3x=gxdcconv.$(OBJ) gxdcolor.$(OBJ) gxdither.$(OBJ) gxfill.$(OBJ) gxht.$(OBJ)
LIB4x=gximage.$(OBJ) gximage0.$(OBJ) gximage1.$(OBJ) gximage2.$(OBJ)
LIB5x=gxpaint.$(OBJ) gxpath.$(OBJ) gxpath2.$(OBJ) gxpcopy.$(OBJ)
LIB6x=gxpdash.$(OBJ) gxpflat.$(OBJ) gxsample.$(OBJ) gxstroke.$(OBJ)
LIB1d=gdevabuf.$(OBJ) gdevddrw.$(OBJ) gdevdflt.$(OBJ) gdevnfwd.$(OBJ)
LIB2d=gdevmem.$(OBJ) gdevm1.$(OBJ) gdevm2.$(OBJ) gdevm4.$(OBJ) gdevm8.$(OBJ)
LIB3d=gdevm16.$(OBJ) gdevm24.$(OBJ) gdevm32.$(OBJ) gdevmpla.$(OBJ)
LIBs=$(LIB1s) $(LIB2s) $(LIB3s) $(LIB4s) $(LIB5s) $(LIB6s)
LIBx=$(LIB1x) $(LIB2x) $(LIB3x) $(LIB4x) $(LIB5x) $(LIB6x)
LIBd=$(LIB1d) $(LIB2d) $(LIB3d)
LIB_ALL=$(LIBs) $(LIBx) $(LIBd)
libs.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBs)
	$(EXP)echogs -w libs.dev $(LIB1s)
	$(EXP)echogs -a libs.dev $(LIB2s)
	$(EXP)echogs -a libs.dev $(LIB3s)
	$(EXP)echogs -a libs.dev $(LIB4s)
	$(EXP)echogs -a libs.dev $(LIB5s)
	$(EXP)echogs -a libs.dev $(LIB6s)
	$(ADDMOD) libs -init gscolor

libx.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBx)
	$(EXP)echogs -w libx.dev $(LIB1x)
	$(EXP)echogs -a libx.dev $(LIB2x)
	$(EXP)echogs -a libx.dev $(LIB3x)
	$(EXP)echogs -a libx.dev $(LIB4x)
	$(EXP)echogs -a libx.dev $(LIB5x)
	$(EXP)echogs -a libx.dev $(LIB6x)
	$(ADDMOD) libx -init gximage1 gximage2

libd.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBd)
	$(EXP)echogs -w libd.dev $(LIB1d)
	$(EXP)echogs -a libd.dev $(LIB2d)
	$(EXP)echogs -a libd.dev $(LIB3d)

include 51x.mak
# roplib and 51xlib shouldn't be required....
libcore.dev: $(LIB_MAK) $(ECHOGS_XE) 51x.mak \
  libs.dev libx.dev libd.dev iscale.dev roplib.dev 51xlib.dev
	$(SETMOD) libcore
	$(ADDMOD) libcore -dev nullpage
	$(ADDMOD) libcore -include libs libx libd iscale roplib 51xlib

# ---------------- Stream support ---------------- #
# Currently the only things in the library that use this are clists
# and file streams.

stream_h=stream.h $(scommon_h)

stream.$(OBJ): stream.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

# ---------------- File streams ---------------- #
# Currently only the high-level drivers use these, but more drivers will
# probably use them eventually.

sfile_=sfx$(FILE_IMPLEMENTATION).$(OBJ) stream.$(OBJ)
sfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(sfile_)
	$(SETMOD) sfile $(sfile_)

sfxstdio.$(OBJ): sfxstdio.c $(AK) $(stdio__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfxfd.$(OBJ): sfxfd.c $(AK) $(stdio__h) $(errno__h) $(memory__h) \
  $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)

sfxboth.$(OBJ): sfxboth.c sfxstdio.c sfxfd.c

# ---------------- CCITTFax filters ---------------- #
# These are used by clists, some drivers, and Level 2 in general.

cfe_=scfe.$(OBJ) scfetab.$(OBJ) shc.$(OBJ)
cfe.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfe_)
	$(SETMOD) cfe $(cfe_)

scfe.$(OBJ): scfe.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfetab.$(OBJ): scfetab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

shc.$(OBJ): shc.c $(AK) $(std_h) $(scommon_h) $(shc_h)

cfd_=scfd.$(OBJ) scfdtab.$(OBJ)
cfd.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfd_)
	$(SETMOD) cfd $(cfd_)

scfd.$(OBJ): scfd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(scf_h) $(strimpl_h) $(scfx_h)

scfdtab.$(OBJ): scfdtab.c $(AK) $(std_h) $(scommon_h) $(scf_h)

# ---------------- DCT (JPEG) filters ---------------- #
# These are used by Level 2, and by the JPEG-writing driver.

# Common code

sdctc_=sdctc.$(OBJ) sjpegc.$(OBJ)

sdctc.$(OBJ): sdctc.c $(AK) $(stdio__h)\
 $(sdct_h) $(strimpl_h)\
 jpeglib.h

sjpegc.$(OBJ): sjpegc.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
 jerror.h jpeglib.h

# Encoding (compression)

sdcte_=$(sdctc_) sdcte.$(OBJ) sjpege.$(OBJ)
sdcte.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdcte_) jpege.dev
	$(SETMOD) sdcte $(sdcte_)
	$(ADDMOD) sdcte -include jpege

sdcte.$(OBJ): sdcte.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpege.$(OBJ): sjpege.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) \
 jerror.h jpeglib.h

# Decoding (decompression)

sdctd_=$(sdctc_) sdctd.$(OBJ) sjpegd.$(OBJ)
sdctd.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdctd_) jpegd.dev
	$(SETMOD) sdctd $(sdctd_)
	$(ADDMOD) sdctd -include jpegd

sdctd.$(OBJ): sdctd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
  $(sdct_h) $(sjpeg_h) $(strimpl_h) \
  jerror.h jpeglib.h

sjpegd.$(OBJ): sjpegd.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h)\
 jerror.h jpeglib.h

# ---------------- LZW filters ---------------- #
# These are used by Level 2 in general.

slzwe_=slzwce
#slzwe_=slzwe
lzwe_=$(slzwe_).$(OBJ) slzwc.$(OBJ)
lzwe.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwe_)
	$(SETMOD) lzwe $(lzwe_)

# We need slzwe.dev as a synonym for lzwe.dev for BAND_LIST_STORAGE = memory.
slzwe.dev: lzwe.dev
	$(CP_) lzwe.dev slzwe.dev

slzwce.$(OBJ): slzwce.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwe.$(OBJ): slzwe.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

slzwc.$(OBJ): slzwc.c $(AK) $(std_h)\
  $(slzwx_h) $(strimpl_h)

lzwd_=slzwd.$(OBJ) slzwc.$(OBJ)
lzwd.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwd_)
	$(SETMOD) lzwd $(lzwd_)

# We need slzwd.dev as a synonym for lzwd.dev for BAND_LIST_STORAGE = memory.
slzwd.dev: lzwd.dev
	$(CP_) lzwd.dev slzwd.dev

slzwd.$(OBJ): slzwd.c $(AK) $(stdio__h) $(gdebug_h)\
  $(slzwx_h) $(strimpl_h)

# ---------------- PCX decoding filter ---------------- #
# This is an adhoc filter not used by anything in the standard configuration.

pcxd_=spcxd.$(OBJ)
pcxd.dev: $(LIB_MAK) $(ECHOGS_XE) $(pcxd_)
	$(SETMOD) pcxd $(pcxd_)

spcxd.$(OBJ): spcxd.c $(AK) $(stdio__h) $(memory__h) \
  $(spcxx_h) $(strimpl_h)

# ---------------- Pixel-difference filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pdiff_=spdiff.$(OBJ)
pdiff.dev: $(LIB_MAK) $(ECHOGS_XE) $(pdiff_)
	$(SETMOD) pdiff $(pdiff_)

spdiff.$(OBJ): spdiff.c $(AK) $(stdio__h)\
 $(spdiffx_h) $(strimpl_h)

# ---------------- PNG pixel prediction filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pngp_=spngp.$(OBJ)
pngp.dev: $(LIB_MAK) $(ECHOGS_XE) $(pngp_)
	$(SETMOD) pngp $(pngp_)

spngp.$(OBJ): spngp.c $(AK) $(memory__h)\
  $(spngpx_h) $(strimpl_h)

# ---------------- RunLength filters ---------------- #
# These are used by clists and also by Level 2 in general.

rle_=srle.$(OBJ)
rle.dev: $(LIB_MAK) $(ECHOGS_XE) $(rle_)
	$(SETMOD) rle $(rle_)

srle.$(OBJ): srle.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

rld_=srld.$(OBJ)
rld.dev: $(LIB_MAK) $(ECHOGS_XE) $(rld_)
	$(SETMOD) rld $(rld_)

srld.$(OBJ): srld.c $(AK) $(stdio__h) $(memory__h) \
  $(srlx_h) $(strimpl_h)

# ---------------- String encoding/decoding filters ---------------- #
# These are used by the PostScript and PDF writers, and also by the
# PostScript interpreter.

scantab.$(OBJ): scantab.c $(AK) $(stdpre_h)\
 $(scanchar_h) $(scommon_h)

sfilter2.$(OBJ): sfilter2.c $(AK) $(memory__h) $(stdio__h)\
 $(sa85x_h) $(scanchar_h) $(sbtx_h) $(strimpl_h)

sstring.$(OBJ): sstring.c $(AK) $(stdio__h) $(memory__h) $(string__h)\
 $(scanchar_h) $(sstring_h) $(strimpl_h)

# ---------------- zlib filters ---------------- #
# These are used by clists and are also available as filters.

szlibc_=szlibc.$(OBJ)

szlibc.$(OBJ): szlibc.c $(AK) $(std_h) \
  $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibc.c

szlibe_=$(szlibc_) szlibe.$(OBJ)
szlibe.dev: $(LIB_MAK) $(ECHOGS_XE) zlibe.dev $(szlibe_)
	$(SETMOD) szlibe $(szlibe_)
	$(ADDMOD) szlibe -include zlibe

szlibe.$(OBJ): szlibe.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibe.c

szlibd_=$(szlibc_) szlibd.$(OBJ)
szlibd.dev: $(LIB_MAK) $(ECHOGS_XE) zlibd.dev $(szlibd_)
	$(SETMOD) szlibd $(szlibd_)
	$(ADDMOD) szlibd -include zlibd

szlibd.$(OBJ): szlibd.c $(AK) $(std_h) \
  $(gsmemory_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) szlibd.c

# ---------------- Command lists ---------------- #

gxcldev_h=gxcldev.h $(gxclist_h) $(gsropt_h) $(gxht_h) $(gxtmap_h) $(gxdht_h)\
  $(strimpl_h) $(scfx_h) $(srlx_h)
gxclpage_h=gxclpage.h $(gxclio_h)
gxclpath_h=gxclpath.h $(gxfixed_h)

# Command list package.  Currently the higher-level facilities are required,
# but eventually they will be optional.
clist.dev: $(LIB_MAK) $(ECHOGS_XE) clbase.dev clpath.dev
	$(SETMOD) clist -include clbase clpath

# Base command list facility.
clbase1_=gxclist.$(OBJ) gxclbits.$(OBJ) gxclpage.$(OBJ)
clbase2_=gxclread.$(OBJ) gxclrect.$(OBJ) stream.$(OBJ)
clbase_=$(clbase1_) $(clbase2_)
clbase.dev: $(LIB_MAK) $(ECHOGS_XE) $(clbase_) cl$(BAND_LIST_STORAGE).dev \
  cfe.dev cfd.dev rle.dev rld.dev
	$(SETMOD) clbase $(clbase1_)
	$(ADDMOD) clbase -obj $(clbase2_)
	$(ADDMOD) clbase -include cl$(BAND_LIST_STORAGE) cfe cfd rle rld

gdevht_h=gdevht.h $(gzht_h)

gdevht.$(OBJ): gdevht.c $(GXERR) \
  $(gdevht_h) $(gxdcconv_h) $(gxdcolor_h) $(gxdevice_h) $(gxdither_h)

gxclist.$(OBJ): gxclist.c $(GXERR) $(memory__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) $(gsparams_h)

gxclbits.$(OBJ): gxclbits.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxfmap_h)

gxclpage.$(OBJ): gxclpage.c $(AK)\
 $(gdevprn_h) $(gxcldev_h) $(gxclpage_h)

# (gxclread shouldn't need gxclpath.h)
gxclread.$(OBJ): gxclread.c $(GXERR) $(memory__h) $(gp_h) $(gpcheck_h)\
 $(gdevht_h)\
 $(gsbitops_h) $(gscoord_h) $(gsdevice_h) $(gsstate_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcmap_h) $(gxcspace_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gsparams_h)\
 $(gxhttile_h) $(gxpaint_h) $(gzacpath_h) $(gzcpath_h) $(gzpath_h)\
 $(stream_h) $(strimpl_h)

gxclrect.$(OBJ): gxclrect.c $(GXERR)\
 $(gsutil_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h)

# Higher-level command list facilities.
clpath_=gxclimag.$(OBJ) gxclpath.$(OBJ)
clpath.dev: $(LIB_MAK) $(ECHOGS_XE) $(clpath_) psl2cs.dev
	$(SETMOD) clpath $(clpath_)
	$(ADDMOD) clpath -include psl2cs
	$(ADDMOD) clpath -init climag clpath

gxclimag.$(OBJ): gxclimag.c $(GXERR) $(math__h) $(memory__h)\
 $(gscspace_h)\
 $(gxarith_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpath_h) $(gxfmap_h)\
 $(siscale_h) $(strimpl_h)

gxclpath.$(OBJ): gxclpath.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcolor2_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpaint_h) \
 $(gzcpath_h) $(gzpath_h)

# Implement band lists on files.

clfile_=gxclfile.$(OBJ)
clfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(clfile_)
	$(SETMOD) clfile $(clfile_)

gxclfile.$(OBJ): gxclfile.c $(stdio__h) $(string__h) \
  $(gp_h) $(gsmemory_h) $(gserror_h) $(gserrors_h) $(gxclio_h)

# Implement band lists in memory (RAM).

clmemory_=gxclmem.$(OBJ) gxcl$(BAND_LIST_COMPRESSOR).$(OBJ)
clmemory.dev: $(LIB_MAK) $(ECHOGS_XE) $(clmemory_) s$(BAND_LIST_COMPRESSOR)e.dev s$(BAND_LIST_COMPRESSOR)d.dev
	$(SETMOD) clmemory $(clmemory_)
	$(ADDMOD) clmemory -include s$(BAND_LIST_COMPRESSOR)e s$(BAND_LIST_COMPRESSOR)d
	$(ADDMOD) clmemory -init cl_$(BAND_LIST_COMPRESSOR)

gxclmem_h=gxclmem.h $(gxclio_h) $(strimpl_h)

gxclmem.$(OBJ): gxclmem.c $(GXERR) $(LIB_MAK) $(memory__h) \
  $(gxclmem_h)

# Implement the compression method for RAM-based band lists.

gxcllzw.$(OBJ): gxcllzw.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(slzwx_h)

gxclzlib.$(OBJ): gxclzlib.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(szlibx_h)
	$(CCCZ) gxclzlib.c

# ---------------- Page devices ---------------- #
# We include this here, rather than in devs.mak, because it is more like
# a feature than a simple device.

page_=gdevprn.$(OBJ)
page.dev: $(LIB_MAK) $(ECHOGS_XE) $(page_) clist.dev
	$(SETMOD) page $(page_)
	$(ADDMOD) page -include clist

gdevprn.$(OBJ): gdevprn.c $(ctype__h) \
  $(gdevprn_h) $(gp_h) $(gsparam_h) $(gxclio_h)

# ---------------- Vector devices ---------------- #
# We include this here for the same reasons as page.dev.

gdevvec_h=gdevvec.h $(gdevbbox_h) $(gsropt_h) $(gxdevice_h) $(gxistate_h) $(stream_h)

vector_=gdevvec.$(OBJ)
vector.dev: $(LIB_MAK) $(ECHOGS_XE) $(vector_) bbox.dev sfile.dev
	$(SETMOD) vector $(vector_)
	$(ADDMOD) vector -include bbox sfile

gdevvec.$(OBJ): gdevvec.c $(GXERR) $(math__h) $(memory__h) $(string__h)\
 $(gdevvec_h) $(gp_h) $(gscspace_h) $(gsparam_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxfixed_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)

# ---------------- Image scaling filter ---------------- #

iscale_=siscale.$(OBJ)
iscale.dev: $(LIB_MAK) $(ECHOGS_XE) $(iscale_)
	$(SETMOD) iscale $(iscale_)

siscale.$(OBJ): siscale.c $(AK) $(math__h) $(memory__h) $(stdio__h) \
  $(siscale_h) $(strimpl_h)

# ---------------- RasterOp et al ---------------- #
# Currently this module is required, but it should be optional.

roplib_=gdevmrop.$(OBJ) gsrop.$(OBJ) gsroptab.$(OBJ)
roplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(roplib_)
	$(SETMOD) roplib $(roplib_)
	$(ADDMOD) roplib -init roplib

gdevrun.$(OBJ): gdevrun.c $(GXERR) $(memory__h) \
  $(gxdevice_h) $(gxdevmem_h)

gdevmrop.$(OBJ): gdevmrop.c $(GXERR) $(memory__h) \
  $(gsbittab_h) $(gsropt_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h) \
  $(gdevmrop_h)

gsrop.$(OBJ): gsrop.c $(GXERR) \
  $(gsrop_h) $(gzstate_h)

gsroptab.$(OBJ): gsroptab.c $(stdpre_h) $(gsropt_h)
	$(CCLEAF) gsroptab.c

# ---------------- Async rendering ---------------- #

gsmemfix_h=gsmemfix.h $(gsmemraw_h)
gxsync_h=gxsync.h $(gpsync_h) $(gsmemory_h)
gxpageq_h=gxpageq.h $(gsmemory_h) $(gxband_h) $(gxsync_h)
gsmemlok_h=gsmemlok.h $(gsmemory_h) $(gxsync_h)
gdevprna_h=gdevprna.h $(gdevprn_h) $(gxsync_h)

async_=gdevprna.$(OBJ) gxsync.$(OBJ) gxpageq.$(OBJ) gsmemlok.$(OBJ)\
 gsmemfix.$(OBJ)
async.dev: $(INT_MAK) $(ECHOGS_XE) $(async_) clist.dev
	$(SETMOD) async $(async_)

gdevprna.$(OBJ): gdevprna.c $(AK) $(ctype__h) $(gdevprna_h) $(gsparam_h)\
 $(gsdevice_h) $(gxcldev_h) $(gxclpath_h) $(gxpageq_h) $(gsmemory_h)\
 $(gsmemlok_h) $(gsmemfix_h)

gsmemfix.$(OBJ): gsmemfix.c $(AK) $(memory__h) $(gsmemraw_h) $(gsmemfix_h)

gxsync.$(OBJ): gxsync.c $(AK) $(gxsync_h) $(memory__h) $(gx_h) $(gserrors_h)\
 $(gsmemory_h)

gxpageq.$(OBJ): gxpageq.c $(GXERR) $(gxdevice_h) $(gxclist_h)\
 $(gxpageq_h) $(gserrors_h)

gsmemlok.$(OBJ): gsmemlok.c $(GXERR) $(gsmemlok_h) $(gserrors_h)

# -------- Composite (PostScript Type 0) font support -------- #

cmaplib_=gsfcmap.$(OBJ)
cmaplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmaplib_)
	$(SETMOD) cmaplib $(cmaplib_)

gsfcmap.$(OBJ): gsfcmap.c $(GXERR)\
 $(gsstruct_h) $(gxfcmap_h)

psf0lib_=gschar0.$(OBJ) gsfont0.$(OBJ)
psf0lib.dev: $(LIB_MAK) $(ECHOGS_XE) cmaplib.dev $(psf0lib_)
	$(SETMOD) psf0lib $(psf0lib_)
	$(ADDMOD) psf0lib -include cmaplib

gschar0.$(OBJ): gschar0.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxfixed_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gsfcmap_h) $(gxfont_h) $(gxfont0_h) $(gxchar_h)

gsfont0.$(OBJ): gsfont0.c $(GXERR) $(memory__h)\
 $(gsmatrix_h) $(gsstruct_h) $(gxfixed_h) $(gxdevmem_h) $(gxfcache_h)\
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxdevice_h)

# ---------------- Pattern color ---------------- #

patlib_=gspcolor.$(OBJ) gxclip2.$(OBJ) gxpcmap.$(OBJ)
patlib.dev: $(LIB_MAK) $(ECHOGS_XE) cmyklib.dev psl2cs.dev $(patlib_)
	$(SETMOD) patlib -include cmyklib psl2cs
	$(ADDMOD) patlib -obj $(patlib_)

gspcolor.$(OBJ): gspcolor.c $(GXERR) $(math__h) \
  $(gsimage_h) $(gspath_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h) \
  $(gxarith_h) $(gxcolor2_h) $(gxcoord_h) $(gxclip2_h) $(gxcspace_h) \
  $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) \
  $(gxfixed_h) $(gxmatrix_h) $(gxpath_h) $(gxpcolor_h) $(gzstate_h)

gxclip2.$(OBJ): gxclip2.c $(GXERR) $(memory__h) \
  $(gsstruct_h) $(gxclip2_h) $(gxdevice_h) $(gxdevmem_h)

gxpcmap.$(OBJ): gxpcmap.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1lib_=gxtype1.$(OBJ) gxhint1.$(OBJ) gxhint2.$(OBJ) gxhint3.$(OBJ)

gscrypt1_h=gscrypt1.h
gstype1_h=gstype1.h
gxfont1_h=gxfont1.h
gxop1_h=gxop1.h
gxtype1_h=gxtype1.h $(gscrypt1_h) $(gstype1_h) $(gxop1_h)

gxtype1.$(OBJ): gxtype1.c $(GXERR) $(math__h)\
 $(gsccode_h) $(gsline_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

gxhint1.$(OBJ): gxhint1.c $(GXERR)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)

gxhint2.$(OBJ): gxhint2.c $(GXERR) $(memory__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)

gxhint3.$(OBJ): gxhint3.c $(GXERR) $(math__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)\
 $(gzpath_h)

# Type 1 charstrings

psf1lib_=gstype1.$(OBJ)
psf1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf1lib_) $(type1lib_)
	$(SETMOD) psf1lib $(psf1lib_)
	$(ADDMOD) psf1lib $(type1lib_)
	$(ADDMOD) psf1lib -init gstype1

gstype1.$(OBJ): gstype1.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

# Type 2 charstrings

psf2lib_=gstype2.$(OBJ)
psf2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf2lib_) $(type1lib_)
	$(SETMOD) psf2lib $(psf2lib_)
	$(ADDMOD) psf2lib $(type1lib_)
	$(ADDMOD) psf2lib -init gstype2

gstype2.$(OBJ): gstype2.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h) $(gxchar_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=gstype42.$(OBJ)
ttflib.dev: $(LIB_MAK) $(ECHOGS_XE) $(ttflib_)
	$(SETMOD) ttflib $(ttflib_)

gxfont42_h=gxfont42.h

gstype42.$(OBJ): gstype42.c $(GXERR) $(memory__h) \
  $(gsccode_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxistate_h) $(gxpath_h)

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

cmyklib_=gscolor1.$(OBJ) gsht1.$(OBJ)
cmyklib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmyklib_)
	$(SETMOD) cmyklib $(cmyklib_)
	$(ADDMOD) cmyklib -init gscolor1

gscolor1.$(OBJ): gscolor1.c $(GXERR) \
  $(gsccolor_h) $(gscolor1_h) $(gsstruct_h) $(gsutil_h) \
  $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) \
  $(gzstate_h)

gsht1.$(OBJ): gsht1.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)

colimlib_=gximage3.$(OBJ)
colimlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(colimlib_)
	$(SETMOD) colimlib $(colimlib_)
	$(ADDMOD) colimlib -init gximage3

gximage3.$(OBJ): gximage3.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h)\
 $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzpath_h) $(gzstate_h)

# ---------------- HSB color ---------------- #

hsblib_=gshsb.$(OBJ)
hsblib.dev: $(LIB_MAK) $(ECHOGS_XE) $(hsblib_)
	$(SETMOD) hsblib $(hsblib_)

gshsb.$(OBJ): gshsb.c $(GX) \
  $(gscolor_h) $(gshsb_h) $(gxfrac_h)

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1lib_=gspath1.$(OBJ)
path1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(path1lib_)
	$(SETMOD) path1lib $(path1lib_)

gspath1.$(OBJ): gspath1.c $(GXERR) $(math__h) \
  $(gscoord_h) $(gspath_h) $(gsstruct_h) \
  $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h) $(gzpath_h)

# --------------- Level 2 color space and color image support --------------- #

psl2cs_=gscolor2.$(OBJ)
psl2cs.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2cs_)
	$(SETMOD) psl2cs $(psl2cs_)

gscolor2.$(OBJ): gscolor2.c $(GXERR) \
  $(gxarith_h) $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gxmatrix_h) \
  $(gzstate_h)

psl2lib_=gximage4.$(OBJ) gximage5.$(OBJ)
psl2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2lib_) colimlib.dev psl2cs.dev
	$(SETMOD) psl2lib $(psl2lib_)
	$(ADDMOD) psl2lib -init gximage4 gximage5
	$(ADDMOD) psl2lib -include colimlib psl2cs

gximage4.$(OBJ): gximage4.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)

gximage5.$(OBJ): gximage5.c $(GXERR) $(math__h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)

# ---------------- Display Postscript / Level 2 support ---------------- #

dps2lib_=gsdps1.$(OBJ)
dps2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dps2lib_)
	$(SETMOD) dps2lib $(dps2lib_)

gsdps1.$(OBJ): gsdps1.c $(GXERR) $(math__h)\
 $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gspath2_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- Display Postscript extensions ---------------- #

gsdps_h=gsdps.h

dpslib_=gsdps.$(OBJ)
dpslib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dpslib_)
	$(SETMOD) dpslib $(dpslib_)

gsdps.$(OBJ): gsdps.c $(GX) $(gsdps_h)\
 $(gsdps_h) $(gspath_h) $(gxdevice_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)

# ---------------- CIE color ---------------- #

cielib_=gscie.$(OBJ) gxctable.$(OBJ)
cielib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cielib_)
	$(SETMOD) cielib $(cielib_)

gscie.$(OBJ): gscie.c $(GXERR) $(math__h) \
  $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h) \
  $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)

gxctable.$(OBJ): gxctable.c $(GX) \
  $(gxfixed_h) $(gxfrac_h) $(gxctable_h)

# ---------------- Separation colors ---------------- #

seprlib_=gscsepr.$(OBJ)
seprlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(seprlib_)
	$(SETMOD) seprlib $(seprlib_)

gscsepr.$(OBJ): gscsepr.c $(GXERR)\
 $(gscsepr_h) $(gsmatrix_h) $(gsrefct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gzstate_h)

# ---------------- Functions ---------------- #

gsdsrc_h=gsdsrc.h $(gsstruct_h)
gsfunc_h=gsfunc.h
gsfunc0_h=gsfunc0.h $(gsdsrc_h) $(gsfunc_h)
gxfunc_h=gxfunc.h $(gsfunc_h) $(gsstruct_h)

# Generic support, and FunctionType 0.
funclib_=gsdsrc.$(OBJ) gsfunc.$(OBJ) gsfunc0.$(OBJ)
funclib.dev: $(LIB_MAK) $(ECHOGS_XE) $(funclib_)
	$(SETMOD) funclib $(funclib_)

gsdsrc.$(OBJ): gsdsrc.c $(GX) $(memory__h)\
 $(gsdsrc_h) $(gserrors_h) $(stream_h)

gsfunc.$(OBJ): gsfunc.c $(GX)\
 $(gserrors_h) $(gxfunc_h)

gsfunc0.$(OBJ): gsfunc0.c $(GX) $(math__h)\
 $(gserrors_h) $(gsfunc0_h) $(gxfunc_h)

# ----------------------- Platform-specific modules ----------------------- #
# Platform-specific code doesn't really belong here: this is code that is
# shared among multiple platforms.

# Frame buffer implementations.

gp_nofb.$(OBJ): gp_nofb.c $(GX) \
  $(gp_h) $(gxdevice_h)

gp_dosfb.$(OBJ): gp_dosfb.c $(AK) $(malloc__h) $(memory__h)\
 $(gx_h) $(gp_h) $(gserrors_h) $(gxdevice_h)

# MS-DOS file system, also used by Desqview/X.
gp_dosfs.$(OBJ): gp_dosfs.c $(AK) $(dos__h) $(gp_h) $(gx_h)

# MS-DOS file enumeration, *not* used by Desqview/X.
gp_dosfe.$(OBJ): gp_dosfe.c $(AK) $(stdio__h) $(memory__h) $(string__h) \
  $(dos__h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(gp_h) $(gsutil_h)

# Other MS-DOS facilities.
gp_msdos.$(OBJ): gp_msdos.c $(AK) $(dos__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gp_h)

# Unix(-like) file system, also used by Desqview/X.
gp_unifs.$(OBJ): gp_unifs.c $(AK) $(memory__h) $(string__h) $(gx_h) $(gp_h) \
  $(gsstruct_h) $(gsutil_h) $(stat__h) $(dirent__h)

# Unix(-like) file name syntax, *not* used by Desqview/X.
gp_unifn.$(OBJ): gp_unifn.c $(AK) $(gx_h) $(gp_h)

# ----------------------------- Main program ------------------------------ #

# Main program for library testing

gslib.$(OBJ): gslib.c $(AK) $(math__h) \
  $(gx_h) $(gp_h) $(gserrors_h) $(gsmatrix_h) $(gsstate_h) $(gscspace_h) \
  $(gscdefs_h) $(gscolor2_h) $(gscoord_h) $(gslib_h) $(gsparam_h) \
  $(gspaint_h) $(gspath_h) $(gsstruct_h) $(gsutil_h) \
  $(gxalloc_h) $(gxdevice_h)
