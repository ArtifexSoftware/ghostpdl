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


# (Platform-independent) makefile for Ghostscript graphics library
# and other support code.
# Users of this makefile must define the following:
#	GLSRCDIR - the source directory
#	GLGENDIR - the directory for source files generated during building
#	GLOBJDIR - the object code directory

GLSRC=$(GLSRCDIR)$(D)
GLGEN=$(GLGENDIR)$(D)
GLOBJ=$(GLOBJDIR)$(D)
GLO_=$(O_)$(GLOBJ)
GLI_=$(GLSRCDIR) $(II)$(GLGENDIR)
GLF_=
GLCC=$(CC_) $(I_)$(GLI_)$(_I) $(GLF_)
GLJCC=$(CC_) $(I_)$(GLI_) $(II)$(JI_)$(_I) $(GLF_)
GLZCC=$(CC_) $(I_)$(GLI_) $(II)$(ZI_)$(_I) $(GLF_)
GLCCLEAF=$(CC_LEAF) $(I_)$(GLI_)$(_I) $(GLF_)

# Define the name of this makefile.
LIB_MAK=$(GLSRC)lib.mak

# Define the inter-dependencies of the .h files.
# Since not all versions of `make' defer expansion of macros,
# we must list these in bottom-to-top order.

# Generic files

arch_h=$(GLGEN)arch.h
stdpre_h=$(GLSRC)stdpre.h
std_h=$(GLSRC)std.h $(arch_h) $(stdpre_h)

$(GLGEN)arch.h: $(GENARCH_XE)
	$(GENARCH_XE) $(GLGEN)arch.h

# Platform interfaces

gpgetenv_h=$(GLSRC)gpgetenv.h
gp_h=$(GLSRC)gp.h $(gpgetenv_h)
gpcheck_h=$(GLSRC)gpcheck.h
gpsync_h=$(GLSRC)gpsync.h

# Configuration definitions

gconf_h=$(GLSRC)gconf.h $(gconfig_h)
# gconfig*.h are generated dynamically.
gconfig__h=$(GLGEN)gconfig_.h
gconfigv_h=$(GLGEN)gconfigv.h
gscdefs_h=$(GLSRC)gscdefs.h $(gconfigv_h)

# C library interfaces

# Because of variations in the "standard" header files between systems, and
# because we must include std.h before any file that includes sys/types.h,
# we define local include files named *_.h to substitute for <*.h>.

vmsmath_h=$(GLSRC)vmsmath.h

dos__h=$(GLSRC)dos_.h
ctype__h=$(GLSRC)ctype_.h $(std_h)
dirent__h=$(GLSRC)dirent_.h $(std_h) $(gconfig__h)
errno__h=$(GLSRC)errno_.h $(std_h)
malloc__h=$(GLSRC)malloc_.h $(std_h)
math__h=$(GLSRC)math_.h $(std_h) $(vmsmath_h)
memory__h=$(GLSRC)memory_.h $(std_h)
stat__h=$(GLSRC)stat_.h $(std_h)
stdio__h=$(GLSRC)stdio_.h $(std_h)
string__h=$(GLSRC)string_.h $(std_h)
time__h=$(GLSRC)time_.h $(std_h) $(gconfig__h)
windows__h=$(GLSRC)windows_.h
# Out of order
pipe__h=$(GLSRC)pipe_.h $(stdio__h)

# Third-party library interfaces

jerror_h=$(JSRCDIR)$(D)jerror.h
jpeglib_h=$(GLGEN)jpeglib.h

# Miscellaneous

gdebug_h=$(GLSRC)gdebug.h
gsalloc_h=$(GLSRC)gsalloc.h
gsargs_h=$(GLSRC)gsargs.h
gserror_h=$(GLSRC)gserror.h
gserrors_h=$(GLSRC)gserrors.h
gsexit_h=$(GLSRC)gsexit.h
gsgc_h=$(GLSRC)gsgc.h
gsio_h=$(GLSRC)gsio.h
gsmalloc_h=$(GLSRC)gsmalloc.h
gsmdebug_h=$(GLSRC)gsmdebug.h
gsmemraw_h=$(GLSRC)gsmemraw.h
gsmemory_h=$(GLSRC)gsmemory.h $(gsmemraw_h)
gsrefct_h=$(GLSRC)gsrefct.h
gsstruct_h=$(GLSRC)gsstruct.h
gstypes_h=$(GLSRC)gstypes.h
gx_h=$(GLSRC)gx.h $(stdio__h) $(gdebug_h)\
 $(gserror_h) $(gsio_h) $(gsmemory_h) $(gstypes_h)
gxsync_h=$(GLSRC)gxsync.h $(gpsync_h) $(gsmemory_h)
# Out of order
gsmemlok_h=$(GLSRC)gsmemlok.h $(gsmemory_h) $(gxsync_h)

GX=$(AK) $(gx_h)
GXERR=$(GX) $(gserrors_h)

###### Support

### Include files

gsbitmap_h=$(GLSRC)gsbitmap.h $(gsstruct_h)
gsbitops_h=$(GLSRC)gsbitops.h
gsbittab_h=$(GLSRC)gsbittab.h
gsflip_h=$(GLSRC)gsflip.h
gsrect_h=$(GLSRC)gsrect.h
gsuid_h=$(GLSRC)gsuid.h
gsutil_h=$(GLSRC)gsutil.h
gxarith_h=$(GLSRC)gxarith.h
gxbitmap_h=$(GLSRC)gxbitmap.h $(gsbitmap_h) $(gstypes_h)
gxbitops_h=$(GLSRC)gxbitops.h $(gsbitops_h)
gxfarith_h=$(GLSRC)gxfarith.h $(gconfigv_h) $(gxarith_h)
gxfixed_h=$(GLSRC)gxfixed.h
gxobj_h=$(GLSRC)gxobj.h $(gxbitmap_h)
# Out of order
gxalloc_h=$(GLSRC)gxalloc.h $(gsalloc_h) $(gxobj_h)

# Streams
scommon_h=$(GLSRC)scommon.h $(gsmemory_h) $(gstypes_h) $(gsstruct_h)
stream_h=$(GLSRC)stream.h $(scommon_h)

### Memory manager

$(GLOBJ)gsalloc.$(OBJ): $(GLSRC)gsalloc.c $(GXERR) $(memory__h) $(string__h)\
 $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)\
 $(stream_h)
	$(GLCC) $(GLO_)gsalloc.$(OBJ) $(C_) $(GLSRC)gsalloc.c

### Bitmap processing

$(GLOBJ)gsbitops.$(OBJ): $(GLSRC)gsbitops.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gstypes_h) $(gxbitops_h)
	$(GLCC) $(GLO_)gsbitops.$(OBJ) $(C_) $(GLSRC)gsbitops.c

$(GLOBJ)gsbittab.$(OBJ): $(GLSRC)gsbittab.c $(AK) $(stdpre_h) $(gsbittab_h)
	$(GLCC) $(GLO_)gsbittab.$(OBJ) $(C_) $(GLSRC)gsbittab.c

# gsflip is not part of the standard configuration: it's rather large,
# and no standard facility requires it.
#**************** $(CCLEAF) gsflip.c
$(GLOBJ)gsflip.$(OBJ): $(GLSRC)gsflip.c $(GX) $(gsbittab_h) $(gsflip_h)
	$(GLCC) $(GLO_)gsflip.$(OBJ) $(C_) $(GLSRC)gsflip.c

$(GLOBJ)gsmalloc.$(OBJ): $(GLSRC)gsmalloc.c $(malloc__h)\
 $(gdebug_h)\
 $(gsmalloc_h) $(gsmdebug_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h)
	$(GLCC) $(GLO_)gsmalloc.$(OBJ) $(C_) $(GLSRC)gsmalloc.c

$(GLOBJ)gsmemory.$(OBJ): $(GLSRC)gsmemory.c $(memory__h)\
 $(gsmdebug_h) $(gsmemory_h) $(gsrefct_h) $(gsstruct_h) $(gstypes_h)
	$(GLCC) $(GLO_)gsmemory.$(OBJ) $(C_) $(GLSRC)gsmemory.c

# gsnogc currently is only used in library-only configurations.
$(GLOBJ)gsnogc.$(OBJ): $(GLSRC)gsnogc.c $(GX)\
 $(gsgc_h) $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)
	$(GLCC) $(GLO_)gsnogc.$(OBJ) $(C_) $(GLSRC)gsnogc.c

### Multi-threading

# None of this is included in any standard configuration at the moment.

$(GLOBJ)gsmemlok.$(OBJ): $(GLSRC)gsmemlok.c $(GXERR) $(gsmemlok_h)
	$(GLCC) $(GLO_)gsmemlok.$(OBJ) $(C_) $(GLSRC)gsmemlok.c

$(GLOBJ)gxsync.$(OBJ): $(GLSRC)gxsync.c $(GXERR) $(memory__h)\
 $(gsmemory_h) $(gxsync_h)
	$(GLCC) $(GLO_)gxsync.$(OBJ) $(C_) $(GLSRC)gxsync.c

### Miscellaneous

# Command line argument list management
$(GLOBJ)gsargs.$(OBJ): $(GLSRC)gsargs.c $(ctype__h) $(stdio__h) $(string__h)\
 $(gsargs_h) $(gsexit_h) $(gsmemory_h)
	$(GLCC) $(GLO_)gsargs.$(OBJ) $(C_) $(GLSRC)gsargs.c

# FPU emulation
# gsfemu is only used in FPU-less configurations, and currently only with gcc.
# We thought using CCLEAF would produce smaller code, but it actually
# produces larger code!
$(GLOBJ)gsfemu.$(OBJ): $(GLSRC)gsfemu.c $(AK) $(std_h)
	$(GLCC) $(GLO_)gsfemu.$(OBJ) $(C_) $(GLSRC)gsfemu.c

$(GLOBJ)gsmisc.$(OBJ): $(GLSRC)gsmisc.c $(GXERR) $(gconfigv_h)\
 $(ctype__h) $(malloc__h) $(math__h) $(memory__h) $(string__h)\
 $(gpcheck_h) $(gxfarith_h) $(gxfixed_h)
	$(GLCC) $(GLO_)gsmisc.$(OBJ) $(C_) $(GLSRC)gsmisc.c

$(GLOBJ)gsutil.$(OBJ): $(GLSRC)gsutil.c $(AK) $(memory__h) $(string__h)\
 $(gconfigv_h)\
 $(gsmemory_h) $(gsrect_h) $(gstypes_h) $(gsuid_h) $(gsutil_h)
	$(GLCC) $(GLO_)gsutil.$(OBJ) $(C_) $(GLSRC)gsutil.c

###### Low-level facilities and utilities

### Include files

gdevbbox_h=$(GLSRC)gdevbbox.h
gdevmem_h=$(GLSRC)gdevmem.h $(gxbitops_h)
gdevmrop_h=$(GLSRC)gdevmrop.h

gsalpha_h=$(GLSRC)gsalpha.h
gsccode_h=$(GLSRC)gsccode.h
gsccolor_h=$(GLSRC)gsccolor.h $(gsstruct_h)
gscsel_h=$(GLSRC)gscsel.h
gscolor1_h=$(GLSRC)gscolor1.h
gscompt_h=$(GLSRC)gscompt.h
gscoord_h=$(GLSRC)gscoord.h
gscpm_h=$(GLSRC)gscpm.h
gscsepnm_h=$(GLSRC)gscsepnm.h
gsdevice_h=$(GLSRC)gsdevice.h
gsfcmap_h=$(GLSRC)gsfcmap.h $(gsccode_h)
gsfont_h=$(GLSRC)gsfont.h
gshsb_h=$(GLSRC)gshsb.h
gsht_h=$(GLSRC)gsht.h
gsht1_h=$(GLSRC)gsht1.h $(gsht_h)
gsjconf_h=$(GLSRC)gsjconf.h $(arch_h)
gslib_h=$(GLSRC)gslib.h
gslparam_h=$(GLSRC)gslparam.h
gsmatrix_h=$(GLSRC)gsmatrix.h
gspaint_h=$(GLSRC)gspaint.h
gsparam_h=$(GLSRC)gsparam.h
gsparams_h=$(GLSRC)gsparams.h $(gsparam_h) $(stream_h)
gspath2_h=$(GLSRC)gspath2.h
gspcolor_h=$(GLSRC)gspcolor.h $(gsccolor_h) $(gsuid_h)
gspenum_h=$(GLSRC)gspenum.h
gsptype1_h=$(GLSRC)gsptype1.h $(gspcolor_h) $(gxbitmap_h)
gsropt_h=$(GLSRC)gsropt.h
gstext_h=$(GLSRC)gstext.h $(gsccode_h) $(gsrefct_h)
gsxfont_h=$(GLSRC)gsxfont.h
# Out of order
gschar_h=$(GLSRC)gschar.h $(gsccode_h) $(gscpm_h)
gscolor2_h=$(GLSRC)gscolor2.h $(gsptype1_h)
gsiparam_h=$(GLSRC)gsiparam.h $(gsmatrix_h)
gsimage_h=$(GLSRC)gsimage.h $(gsiparam_h)
gsline_h=$(GLSRC)gsline.h $(gslparam_h)
gspath_h=$(GLSRC)gspath.h $(gspenum_h)
gsrop_h=$(GLSRC)gsrop.h $(gsropt_h)

gxalpha_h=$(GLSRC)gxalpha.h
gxbcache_h=$(GLSRC)gxbcache.h $(gxbitmap_h)
gxbitfmt_h=$(GLSRC)gxbitfmt.h
gxcindex_h=$(GLSRC)gxcindex.h $(gsbitops_h)
gxcvalue_h=$(GLSRC)gxcvalue.h
gxclio_h=$(GLSRC)gxclio.h $(gp_h)
gxclip_h=$(GLSRC)gxclip.h
gxcolor2_h=$(GLSRC)gxcolor2.h $(gscolor2_h) $(gsrefct_h) $(gxbitmap_h)
gxcomp_h=$(GLSRC)gxcomp.h $(gscompt_h) $(gsrefct_h) $(gxbitfmt_h)
gxcoord_h=$(GLSRC)gxcoord.h $(gscoord_h)
gxcpath_h=$(GLSRC)gxcpath.h
gxdda_h=$(GLSRC)gxdda.h
gxdevrop_h=$(GLSRC)gxdevrop.h
gxdevmem_h=$(GLSRC)gxdevmem.h
gxdither_h=$(GLSRC)gxdither.h
gxfcmap_h=$(GLSRC)gxfcmap.h $(gsfcmap_h) $(gsuid_h)
gxfont0_h=$(GLSRC)gxfont0.h
gxfrac_h=$(GLSRC)gxfrac.h
gxftype_h=$(GLSRC)gxftype.h
gxgetbit_h=$(GLSRC)gxgetbit.h $(gxbitfmt_h)
gxhttile_h=$(GLSRC)gxhttile.h
gxhttype_h=$(GLSRC)gxhttype.h
gxiodev_h=$(GLSRC)gxiodev.h $(stat__h)
gxline_h=$(GLSRC)gxline.h $(gslparam_h)
gxlum_h=$(GLSRC)gxlum.h
gxmatrix_h=$(GLSRC)gxmatrix.h $(gsmatrix_h)
gxmclip_h=$(GLSRC)gxmclip.h $(gxclip_h)
gxp1fill_h=$(GLSRC)gxp1fill.h
gxpaint_h=$(GLSRC)gxpaint.h
gxpath_h=$(GLSRC)gxpath.h $(gscpm_h) $(gslparam_h) $(gspenum_h) $(gsrect_h)
gxpcache_h=$(GLSRC)gxpcache.h
gxpcolor_h=$(GLSRC)gxpcolor.h $(gxpcache_h)
gxsample_h=$(GLSRC)gxsample.h
gxstate_h=$(GLSRC)gxstate.h
gxtext_h=$(GLSRC)gxtext.h $(gstext_h)
gxtmap_h=$(GLSRC)gxtmap.h
gxxfont_h=$(GLSRC)gxxfont.h $(gsccode_h) $(gsmatrix_h) $(gsuid_h) $(gsxfont_h)
# The following are out of order because they include other files.
gxband_h=$(GLSRC)gxband.h $(gxclio_h)
gxchar_h=$(GLSRC)gxchar.h $(gschar_h) $(gxtext_h)
gsdcolor_h=$(GLSRC)gsdcolor.h $(gsccolor_h)\
 $(gxarith_h) $(gxbitmap_h) $(gxcindex_h) $(gxhttile_h)
gxdcolor_h=$(GLSRC)gxdcolor.h\
 $(gscsel_h) $(gsdcolor_h) $(gsropt_h) $(gsstruct_h)
gxdevcli_h=$(GLSRC)gxdevcli.h $(std_h)\
 $(gscompt_h) $(gsdcolor_h) $(gsiparam_h) $(gsmatrix_h)\
 $(gsrefct_h) $(gsropt_h) $(gsstruct_h) $(gsxfont_h)\
 $(gxbitmap_h) $(gxcindex_h) $(gxcvalue_h) $(gxtext_h) $(gxfixed_h)
gxdevice_h=$(GLSRC)gxdevice.h $(stdio__h)\
 $(gsmalloc_h) $(gsparam_h) $(gxdevcli_h)
gxdht_h=$(GLSRC)gxdht.h $(gscsepnm_h) $(gsrefct_h) $(gxarith_h) $(gxhttype_h)
gxclip2_h=$(GLSRC)gxclip2.h $(gxmclip_h)
gxclipm_h=$(GLSRC)gxclipm.h $(gxmclip_h)
gxctable_h=$(GLSRC)gxctable.h $(gxfixed_h) $(gxfrac_h)
gxfcache_h=$(GLSRC)gxfcache.h $(gsuid_h) $(gsxfont_h)\
 $(gxbcache_h) $(gxftype_h)
gxfont_h=$(GLSRC)gxfont.h\
 $(gsccode_h) $(gsfont_h) $(gsuid_h) $(gsstruct_h) $(gxftype_h)
gxiparam_h=$(GLSRC)gxiparam.h $(gxdevcli_h)
gscie_h=$(GLSRC)gscie.h $(gsrefct_h) $(gsstruct_h) $(gxctable_h)
gscrd_h=$(GLSRC)gscrd.h $(gscie_h)
gscrdp_h=$(GLSRC)gscrdp.h $(gscie_h) $(gsparam_h)
gscspace_h=$(GLSRC)gscspace.h $(gsmemory_h)
gscsepr_h=$(GLSRC)gscsepr.h $(gscspace_h)
gxdcconv_h=$(GLSRC)gxdcconv.h $(gxfrac_h)
gxfmap_h=$(GLSRC)gxfmap.h $(gsrefct_h) $(gxfrac_h) $(gxtmap_h)
gxistate_h=$(GLSRC)gxistate.h $(gscsel_h) $(gsrefct_h) $(gsropt_h)\
 $(gxcvalue_h) $(gxfixed_h) $(gxline_h) $(gxmatrix_h) $(gxtmap_h)
gxclist_h=$(GLSRC)gxclist.h $(gscspace_h)\
 $(gxband_h) $(gxbcache_h) $(gxclio_h) $(gxistate_h)
gxcmap_h=$(GLSRC)gxcmap.h $(gscsel_h) $(gxcvalue_h) $(gxfmap_h)
gxcspace_h=$(GLSRC)gxcspace.h\
 $(gscspace_h) $(gsccolor_h) $(gscsel_h) $(gxfrac_h)
gxht_h=$(GLSRC)gxht.h $(gsht1_h) $(gscsepnm_h) $(gsrefct_h) $(gxhttype_h) $(gxtmap_h)
gscolor_h=$(GLSRC)gscolor.h $(gxtmap_h)
gsstate_h=$(GLSRC)gsstate.h\
 $(gscolor_h) $(gscsel_h) $(gsdevice_h) $(gsht_h) $(gsline_h)

gzacpath_h=$(GLSRC)gzacpath.h
gzcpath_h=$(GLSRC)gzcpath.h $(gxcpath_h)
gzht_h=$(GLSRC)gzht.h $(gscsel_h)\
 $(gxdht_h) $(gxfmap_h) $(gxht_h) $(gxhttile_h)
gzline_h=$(GLSRC)gzline.h $(gxline_h)
gzpath_h=$(GLSRC)gzpath.h $(gsmatrix_h) $(gsrefct_h) $(gsstruct_h) $(gxpath_h)
gzstate_h=$(GLSRC)gzstate.h $(gscpm_h) $(gsrefct_h) $(gsstate_h)\
 $(gxdcolor_h) $(gxistate_h) $(gxstate_h)

sa85x_h=$(GLSRC)sa85x.h
sbtx_h=$(GLSRC)sbtx.h
scanchar_h=$(GLSRC)scanchar.h
sdct_h=$(GLSRC)sdct.h
shc_h=$(GLSRC)shc.h $(gsbittab_h) $(scommon_h)
siscale_h=$(GLSRC)siscale.h $(gconfigv_h)
sjpeg_h=$(GLSRC)sjpeg.h
slzwx_h=$(GLSRC)slzwx.h
spcxx_h=$(GLSRC)spcxx.h
spdiffx_h=$(GLSRC)spdiffx.h
spngpx_h=$(GLSRC)spngpx.h
srlx_h=$(GLSRC)srlx.h
sstring_h=$(GLSRC)sstring.h
strimpl_h=$(GLSRC)strimpl.h $(scommon_h) $(gstypes_h) $(gsstruct_h)
szlibx_h=$(GLSRC)szlibx.h
szlibxx_h=$(GLSRC)szlibxx.h $(szlibx_h)
# Out of order
scf_h=$(GLSRC)scf.h $(shc_h)
scfx_h=$(GLSRC)scfx.h $(shc_h)
gximage_h=$(GLSRC)gximage.h $(gsiparam_h)\
 $(gxcspace_h) $(gxdda_h) $(gxiparam_h) $(gxsample_h)\
 $(siscale_h) $(strimpl_h)

### Executable code

# gconfig and gscdefs are handled specially.  Currently they go in psbase
# rather than in libcore, which is clearly wrong.
gconfig=gconfig$(CONFIG)
$(GLOBJ)$(gconfig).$(OBJ): $(GLSRC)gconf.c $(GX)\
 $(gscdefs_h) $(gconf_h) $(gxdevice_h) $(gxiodev_h) $(MAKEFILE)
	$(RM_) $(GLGEN)$(gconfig).c
	$(CP_) $(gconfig_h) $(GLGEN)gconfig.h
	$(CP_) $(GLSRC)gconf.c $(GLGEN)$(gconfig).c
	$(GLCC) $(GLO_)$(gconfig).$(OBJ) $(C_) $(GLGEN)$(gconfig).c

gscdefs=gscdefs$(CONFIG)
$(GLOBJ)$(gscdefs).$(OBJ): $(GLSRC)gscdef.c\
 $(stdpre_h) $(gscdefs_h) $(gconf_h) $(MAKEFILE)
	$(RM_) $(GLGEN)$(gscdefs).c
	$(CP_) $(gconfig_h) $(GLGEN)gconfig.h
	$(CP_) $(GLSRC)gscdef.c $(GLGEN)$(gscdefs).c
	$(GLCC) $(GLO_)$(gscdefs).$(OBJ) $(C_) $(GLGEN)$(gscdefs).c

$(GLOBJ)gxacpath.$(OBJ): $(GLSRC)gxacpath.c $(GXERR)\
 $(gsdcolor_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxpaint_h)\
 $(gzacpath_h) $(gzcpath_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxacpath.$(OBJ) $(C_) $(GLSRC)gxacpath.c

$(GLOBJ)gxbcache.$(OBJ): $(GLSRC)gxbcache.c $(GX) $(memory__h)\
 $(gsmdebug_h) $(gxbcache_h)
	$(GLCC) $(GLO_)gxbcache.$(OBJ) $(C_) $(GLSRC)gxbcache.c

$(GLOBJ)gxccache.$(OBJ): $(GLSRC)gxccache.c $(GXERR) $(gpcheck_h)\
 $(gscspace_h) $(gsimage_h) $(gsstruct_h)\
 $(gxchar_h) $(gxdevice_h) $(gxdevmem_h) $(gxfcache_h)\
 $(gxfixed_h) $(gxfont_h) $(gxhttile_h) $(gxmatrix_h) $(gxxfont_h)\
 $(gzstate_h) $(gzpath_h) $(gzcpath_h)
	$(GLCC) $(GLO_)gxccache.$(OBJ) $(C_) $(GLSRC)gxccache.c

$(GLOBJ)gxccman.$(OBJ): $(GLSRC)gxccman.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfont_h) $(gxfcache_h) $(gxchar_h)\
 $(gxxfont_h) $(gzstate_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxccman.$(OBJ) $(C_) $(GLSRC)gxccman.c

$(GLOBJ)gxcht.$(OBJ): $(GLSRC)gxcht.c $(GXERR) $(memory__h)\
 $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h)\
 $(gxistate_h) $(gxmatrix_h) $(gzht_h)
	$(GLCC) $(GLO_)gxcht.$(OBJ) $(C_) $(GLSRC)gxcht.c

$(GLOBJ)gxclip.$(OBJ): $(GLSRC)gxclip.c $(GX)\
 $(gxclip_h) $(gxdevice_h) $(gzpath_h) $(gzcpath_h)
	$(GLCC) $(GLO_)gxclip.$(OBJ) $(C_) $(GLSRC)gxclip.c

$(GLOBJ)gxcmap.$(OBJ): $(GLSRC)gxcmap.c $(GXERR)\
 $(gsccolor_h)\
 $(gxalpha_h) $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h)\
 $(gxdither_h) $(gxfarith_h) $(gxfrac_h) $(gxlum_h) $(gzstate_h)
	$(GLCC) $(GLO_)gxcmap.$(OBJ) $(C_) $(GLSRC)gxcmap.c

$(GLOBJ)gxcpath.$(OBJ): $(GLSRC)gxcpath.c $(GXERR)\
 $(gscoord_h) $(gsline_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxstate_h) $(gzpath_h) $(gzcpath_h)
	$(GLCC) $(GLO_)gxcpath.$(OBJ) $(C_) $(GLSRC)gxcpath.c

$(GLOBJ)gxdcconv.$(OBJ): $(GLSRC)gxdcconv.c $(GX)\
 $(gsdcolor_h) $(gxcmap_h) $(gxdcconv_h) $(gxdevice_h)\
 $(gxfarith_h) $(gxistate_h) $(gxlum_h)
	$(GLCC) $(GLO_)gxdcconv.$(OBJ) $(C_) $(GLSRC)gxdcconv.c

$(GLOBJ)gxdcolor.$(OBJ): $(GLSRC)gxdcolor.c $(GX)\
 $(gsbittab_h) $(gserrors_h) $(gxdcolor_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gxdcolor.$(OBJ) $(C_) $(GLSRC)gxdcolor.c

$(GLOBJ)gxdither.$(OBJ): $(GLSRC)gxdither.c $(GX)\
 $(gsstruct_h) $(gsdcolor_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdither_h) $(gxlum_h) $(gzht_h)
	$(GLCC) $(GLO_)gxdither.$(OBJ) $(C_) $(GLSRC)gxdither.c

$(GLOBJ)gxfill.$(OBJ): $(GLSRC)gxfill.c $(GXERR) $(math__h)\
 $(gsstruct_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxhttile_h)\
 $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxfill.$(OBJ) $(C_) $(GLSRC)gxfill.c

$(GLOBJ)gxht.$(OBJ): $(GLSRC)gxht.c $(GXERR) $(memory__h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gzht_h)
	$(GLCC) $(GLO_)gxht.$(OBJ) $(C_) $(GLSRC)gxht.c

$(GLOBJ)gxiinit.$(OBJ): $(GLSRC)gxiinit.c $(GXERR) $(math__h) $(memory__h)\
 $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxiparam_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gzpath_h) $(gzstate_h)\
 $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h)
	$(GLCC) $(GLO_)gxiinit.$(OBJ) $(C_) $(GLSRC)gxiinit.c

$(GLOBJ)gxidata.$(OBJ): $(GLSRC)gxidata.c $(GXERR) $(memory__h)\
 $(gxcpath_h) $(gxdevice_h) $(gximage_h)
	$(GLCC) $(GLO_)gxidata.$(OBJ) $(C_) $(GLSRC)gxidata.c

$(GLOBJ)gxifast.$(OBJ): $(GLSRC)gxifast.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsbittab_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxifast.$(OBJ) $(C_) $(GLSRC)gxifast.c

$(GLOBJ)gximono.$(OBJ): $(GLSRC)gximono.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzht_h) $(gzpath_h)
	$(GLCC) $(GLO_)gximono.$(OBJ) $(C_) $(GLSRC)gximono.c

# gxmclip is used for Patterns and ImageType 3 images:
# it isn't included in the base library.
$(GLOBJ)gxmclip.$(OBJ): $(GLSRC)gxmclip.c $(GX)\
 $(gxdevice_h) $(gxdevmem_h) $(gxmclip_h)
	$(GLCC) $(GLO_)gxmclip.$(OBJ) $(C_) $(GLSRC)gxmclip.c

$(GLOBJ)gxpaint.$(OBJ): $(GLSRC)gxpaint.c $(GX)\
 $(gxdevice_h) $(gxhttile_h) $(gxpaint_h) $(gxpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gxpaint.$(OBJ) $(C_) $(GLSRC)gxpaint.c

$(GLOBJ)gxpath.$(OBJ): $(GLSRC)gxpath.c $(GXERR)\
 $(gsstruct_h) $(gxfixed_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxpath.$(OBJ) $(C_) $(GLSRC)gxpath.c

$(GLOBJ)gxpath2.$(OBJ): $(GLSRC)gxpath2.c $(GXERR) $(math__h)\
 $(gxfixed_h) $(gxarith_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxpath2.$(OBJ) $(C_) $(GLSRC)gxpath2.c

$(GLOBJ)gxpcopy.$(OBJ): $(GLSRC)gxpcopy.c $(GXERR) $(math__h) $(gconfigv_h)\
 $(gxfarith_h) $(gxfixed_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxpcopy.$(OBJ) $(C_) $(GLSRC)gxpcopy.c

$(GLOBJ)gxpdash.$(OBJ): $(GLSRC)gxpdash.c $(GX) $(math__h)\
 $(gscoord_h) $(gsline_h) $(gsmatrix_h)\
 $(gxfixed_h) $(gzline_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxpdash.$(OBJ) $(C_) $(GLSRC)gxpdash.c

$(GLOBJ)gxpflat.$(OBJ): $(GLSRC)gxpflat.c $(GX)\
 $(gxarith_h) $(gxfixed_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxpflat.$(OBJ) $(C_) $(GLSRC)gxpflat.c

$(GLOBJ)gxsample.$(OBJ): $(GLSRC)gxsample.c $(GX)\
 $(gxsample_h)
	$(GLCC) $(GLO_)gxsample.$(OBJ) $(C_) $(GLSRC)gxsample.c

$(GLOBJ)gxstroke.$(OBJ): $(GLSRC)gxstroke.c $(GXERR) $(math__h) $(gpcheck_h)\
 $(gscoord_h) $(gsdcolor_h) $(gsdevice_h)\
 $(gxdevice_h) $(gxfarith_h) $(gxfixed_h)\
 $(gxhttile_h) $(gxistate_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzline_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxstroke.$(OBJ) $(C_) $(GLSRC)gxstroke.c

###### Higher-level facilities

$(GLOBJ)gsalpha.$(OBJ): $(GLSRC)gsalpha.c $(GX)\
 $(gsalpha_h) $(gxdcolor_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsalpha.$(OBJ) $(C_) $(GLSRC)gsalpha.c

$(GLOBJ)gschar.$(OBJ): $(GLSRC)gschar.c $(GXERR) $(memory__h) $(string__h)\
 $(gspath_h) $(gsstruct_h)\
 $(gxfixed_h) $(gxarith_h) $(gxmatrix_h) $(gxcoord_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxfcache_h) $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gschar.$(OBJ) $(C_) $(GLSRC)gschar.c

$(GLOBJ)gscolor.$(OBJ): $(GLSRC)gscolor.c $(GXERR)\
 $(gsccolor_h) $(gsstruct_h) $(gsutil_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscolor.$(OBJ) $(C_) $(GLSRC)gscolor.c

$(GLOBJ)gscoord.$(OBJ): $(GLSRC)gscoord.c $(GXERR) $(math__h)\
 $(gsccode_h) $(gxcoord_h) $(gxdevice_h) $(gxfarith_h) $(gxfixed_h) $(gxfont_h)\
 $(gxmatrix_h) $(gxpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscoord.$(OBJ) $(C_) $(GLSRC)gscoord.c

$(GLOBJ)gscparam.$(OBJ): $(GLSRC)gscparam.c $(GXERR) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsstruct_h)
	$(GLCC) $(GLO_)gscparam.$(OBJ) $(C_) $(GLSRC)gscparam.c

$(GLOBJ)gscspace.$(OBJ): $(GLSRC)gscspace.c $(GXERR) $(memory__h)\
 $(gsccolor_h) $(gsstruct_h) $(gxcspace_h) $(gxistate_h)
	$(GLCC) $(GLO_)gscspace.$(OBJ) $(C_) $(GLSRC)gscspace.c

$(GLOBJ)gsdevice.$(OBJ): $(GLSRC)gsdevice.c $(GXERR)\
 $(ctype__h) $(memory__h) $(string__h) $(gp_h)\
 $(gscdefs_h) $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstruct_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdevmem_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsdevice.$(OBJ) $(C_) $(GLSRC)gsdevice.c

$(GLOBJ)gsdevmem.$(OBJ): $(GLSRC)gsdevmem.c $(GXERR) $(math__h) $(memory__h)\
 $(gxarith_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gsdevmem.$(OBJ) $(C_) $(GLSRC)gsdevmem.c

$(GLOBJ)gsdparam.$(OBJ): $(GLSRC)gsdparam.c $(GXERR) $(memory__h) $(string__h)\
 $(gsparam_h) $(gxdevice_h) $(gxfixed_h)
	$(GLCC) $(GLO_)gsdparam.$(OBJ) $(C_) $(GLSRC)gsdparam.c

$(GLOBJ)gsfont.$(OBJ): $(GLSRC)gsfont.c $(GXERR) $(memory__h)\
 $(gschar_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfcache_h)\
 $(gzstate_h)
	$(GLCC) $(GLO_)gsfont.$(OBJ) $(C_) $(GLSRC)gsfont.c

$(GLOBJ)gsht.$(OBJ): $(GLSRC)gsht.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsht.$(OBJ) $(C_) $(GLSRC)gsht.c

$(GLOBJ)gshtscr.$(OBJ): $(GLSRC)gshtscr.c $(GXERR) $(math__h)\
 $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)
	$(GLCC) $(GLO_)gshtscr.$(OBJ) $(C_) $(GLSRC)gshtscr.c

$(GLOBJ)gsimage.$(OBJ): $(GLSRC)gsimage.c $(GXERR) $(memory__h)\
 $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxdevice_h) $(gxiparam_h) $(gxpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsimage.$(OBJ) $(C_) $(GLSRC)gsimage.c

$(GLOBJ)gsimpath.$(OBJ): $(GLSRC)gsimpath.c $(GXERR)\
 $(gsmatrix_h) $(gsstate_h) $(gspath_h)
	$(GLCC) $(GLO_)gsimpath.$(OBJ) $(C_) $(GLSRC)gsimpath.c

$(GLOBJ)gsinit.$(OBJ): $(GLSRC)gsinit.c $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmalloc_h) $(gsmemory_h)
	$(GLCC) $(GLO_)gsinit.$(OBJ) $(C_) $(GLSRC)gsinit.c

$(GLOBJ)gsiodev.$(OBJ): $(GLSRC)gsiodev.c $(GXERR) $(errno__h) $(string__h)\
 $(gp_h) $(gscdefs_h) $(gsparam_h) $(gsstruct_h) $(gxiodev_h)
	$(GLCC) $(GLO_)gsiodev.$(OBJ) $(C_) $(GLSRC)gsiodev.c

$(GLOBJ)gsline.$(OBJ): $(GLSRC)gsline.c $(GXERR) $(math__h) $(memory__h)\
 $(gsline_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzline_h)
	$(GLCC) $(GLO_)gsline.$(OBJ) $(C_) $(GLSRC)gsline.c

$(GLOBJ)gsmatrix.$(OBJ): $(GLSRC)gsmatrix.c $(GXERR) $(math__h)\
 $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h)
	$(GLCC) $(GLO_)gsmatrix.$(OBJ) $(C_) $(GLSRC)gsmatrix.c

$(GLOBJ)gspaint.$(OBJ): $(GLSRC)gspaint.c $(GXERR) $(math__h) $(gpcheck_h)\
 $(gspaint_h) $(gspath_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdevmem_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gspaint.$(OBJ) $(C_) $(GLSRC)gspaint.c

$(GLOBJ)gsparam.$(OBJ): $(GLSRC)gsparam.c $(GXERR) $(memory__h) $(string__h)\
 $(gsparam_h) $(gsstruct_h)
	$(GLCC) $(GLO_)gsparam.$(OBJ) $(C_) $(GLSRC)gsparam.c

# Future replacement for gsparams.c
$(GLOBJ)gsparam2.$(OBJ): $(GLSRC)gsparam2.c $(GXERR) $(memory__h)\
 $(gsparams_h)
	$(GLCC) $(GLO_)gsparam2.$(OBJ) $(C_) $(GLSRC)gsparam2.c

$(GLOBJ)gsparams.$(OBJ): $(GLSRC)gsparams.c $(GXERR) $(memory__h)\
 $(gsparams_h)
	$(GLCC) $(GLO_)gsparams.$(OBJ) $(C_) $(GLSRC)gsparams.c

$(GLOBJ)gspath.$(OBJ): $(GLSRC)gspath.c $(GXERR)\
 $(gscoord_h) $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gspath.$(OBJ) $(C_) $(GLSRC)gspath.c

$(GLOBJ)gsstate.$(OBJ): $(GLSRC)gsstate.c $(GXERR) $(memory__h)\
 $(gsalpha_h) $(gscie_h) $(gscolor2_h) $(gscoord_h)\
 $(gspath_h) $(gsstruct_h) $(gsutil_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gxpcache_h)\
 $(gzstate_h) $(gzht_h) $(gzline_h) $(gzpath_h) $(gzcpath_h)
	$(GLCC) $(GLO_)gsstate.$(OBJ) $(C_) $(GLSRC)gsstate.c

$(GLOBJ)gstext.$(OBJ): $(GLSRC)gstext.c $(std_h) $(gdebug_h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(gxdevcli_h) $(gxpath_h) $(gxtext_h) $(gzstate_h)
	$(GLCC) $(GLO_)gstext.$(OBJ) $(C_) $(GLSRC)gstext.c

###### Internal devices

### Memory devices

$(GLOBJ)gdevmem.$(OBJ): $(GLSRC)gdevmem.c $(GXERR) $(memory__h)\
 $(gsrect_h) $(gsstruct_h)\
 $(gxarith_h) $(gxgetbit_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevmem.$(OBJ) $(C_) $(GLSRC)gdevmem.c

$(GLOBJ)gdevm1.$(OBJ): $(GLSRC)gdevm1.c $(GX) $(memory__h) $(gsrop_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm1.$(OBJ) $(C_) $(GLSRC)gdevm1.c

$(GLOBJ)gdevm2.$(OBJ): $(GLSRC)gdevm2.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm2.$(OBJ) $(C_) $(GLSRC)gdevm2.c

$(GLOBJ)gdevm4.$(OBJ): $(GLSRC)gdevm4.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm4.$(OBJ) $(C_) $(GLSRC)gdevm4.c

$(GLOBJ)gdevm8.$(OBJ): $(GLSRC)gdevm8.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm8.$(OBJ) $(C_) $(GLSRC)gdevm8.c

$(GLOBJ)gdevm16.$(OBJ): $(GLSRC)gdevm16.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm16.$(OBJ) $(C_) $(GLSRC)gdevm16.c

$(GLOBJ)gdevm24.$(OBJ): $(GLSRC)gdevm24.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm24.$(OBJ) $(C_) $(GLSRC)gdevm24.c

$(GLOBJ)gdevm32.$(OBJ): $(GLSRC)gdevm32.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevm32.$(OBJ) $(C_) $(GLSRC)gdevm32.c

$(GLOBJ)gdevmpla.$(OBJ): $(GLSRC)gdevmpla.c $(GX) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevmpla.$(OBJ) $(C_) $(GLSRC)gdevmpla.c

### Alpha-channel devices

$(GLOBJ)gdevabuf.$(OBJ): $(GLSRC)gdevabuf.c $(GXERR) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h)
	$(GLCC) $(GLO_)gdevabuf.$(OBJ) $(C_) $(GLSRC)gdevabuf.c

$(GLOBJ)gdevalph.$(OBJ): $(GLSRC)gdevalph.c $(GXERR) $(memory__h)\
 $(gsdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h)
	$(GLCC) $(GLO_)gdevalph.$(OBJ) $(C_) $(GLSRC)gdevalph.c

### Other built-in devices

bbox.dev: $(LIB_MAK) $(ECHOGS_XE) $(GLOBJ)gdevbbox.$(OBJ)
	$(SETDEV2) bbox $(GLOBJ)gdevbbox.$(OBJ)

$(GLOBJ)gdevbbox.$(OBJ): $(GLSRC)gdevbbox.c $(GXERR) $(math__h) $(memory__h)\
 $(gdevbbox_h) $(gsdevice_h) $(gsparam_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxiparam_h) $(gxistate_h)\
 $(gxpaint_h) $(gxpath_h)
	$(GLCC) $(GLO_)gdevbbox.$(OBJ) $(C_) $(GLSRC)gdevbbox.c

$(GLOBJ)gdevhit.$(OBJ): $(GLSRC)gdevhit.c $(std_h)\
  $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gstypes_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gdevhit.$(OBJ) $(C_) $(GLSRC)gdevhit.c

# Define a device that implements the PCL 5 special color mapping
# algorithms.  This is not included in any PostScript or PDF system.

devcmap.dev: $(LIB_MAK) $(ECHOGS_XE) $(GLOBJ)gdevcmap.$(OBJ)
	$(SETMOD) devcmap $(GLOBJ)gdevcmap.$(OBJ)

gdevcmap_h=$(GLSRC)gdevcmap.h

$(GLOBJ)gdevcmap.$(OBJ): $(GLSRC)gdevcmap.c $(GXERR)\
 $(gxdevice_h) $(gxlum_h) $(gdevcmap_h)
	$(GLCC) $(GLO_)gdevcmap.$(OBJ) $(C_) $(GLSRC)gdevcmap.c

# A device that stores its data using run-length encoding.
# This is just a sketch, not usable yet.

$(GLOBJ)gdevrun.$(OBJ): $(GLSRC)gdevrun.c $(GXERR) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gdevrun.$(OBJ) $(C_) $(GLSRC)gdevrun.c

### Default driver procedure implementations

$(GLOBJ)gdevdbit.$(OBJ): $(GLSRC)gdevdbit.c $(GXERR) $(gpcheck_h)\
 $(gdevmem_h) $(gsbittab_h) $(gsrect_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gdevdbit.$(OBJ) $(C_) $(GLSRC)gdevdbit.c

$(GLOBJ)gdevddrw.$(OBJ): $(GLSRC)gdevddrw.c $(GXERR) $(math__h) $(memory__h)\
 $(gpcheck_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxiparam_h) $(gxistate_h) $(gxmatrix_h)
	$(GLCC) $(GLO_)gdevddrw.$(OBJ) $(C_) $(GLSRC)gdevddrw.c

$(GLOBJ)gdevdflt.$(OBJ): $(GLSRC)gdevdflt.c $(GXERR)\
 $(gsropt_h) $(gxcomp_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gdevdflt.$(OBJ) $(C_) $(GLSRC)gdevdflt.c

$(GLOBJ)gdevdgbr.$(OBJ): $(GLSRC)gdevdgbr.c $(GXERR)\
 $(gdevmem_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(gxlum_h)
	$(GLCC) $(GLO_)gdevdgbr.$(OBJ) $(C_) $(GLSRC)gdevdgbr.c

$(GLOBJ)gdevnfwd.$(OBJ): $(GLSRC)gdevnfwd.c $(GXERR)\
 $(gxdevice_h)
	$(GLCC) $(GLO_)gdevnfwd.$(OBJ) $(C_) $(GLSRC)gdevnfwd.c

### Other device support

# Provide a mapping between StandardEncoding and ISOLatin1Encoding.
$(GLOBJ)gdevemap.$(OBJ): $(GLSRC)gdevemap.c $(AK) $(std_h)
	$(GLCC) $(GLO_)gdevemap.$(OBJ) $(C_) $(GLSRC)gdevemap.c

###### Create a pseudo-"feature" for the entire graphics library.

LIB1s=$(GLOBJ)gsalloc.$(OBJ) $(GLOBJ)gsalpha.$(OBJ)
LIB2s=$(GLOBJ)gsbitops.$(OBJ) $(GLOBJ)gsbittab.$(OBJ)
LIB3s=$(GLOBJ)gschar.$(OBJ) $(GLOBJ)gscolor.$(OBJ)
LIB4s=$(GLOBJ)gscoord.$(OBJ) $(GLOBJ)gscparam.$(OBJ) $(GLOBJ)gscspace.$(OBJ)
LIB5s=$(GLOBJ)gsdevice.$(OBJ) $(GLOBJ)gsdevmem.$(OBJ) $(GLOBJ)gsdparam.$(OBJ)
LIB6s=$(GLOBJ)gsfont.$(OBJ) $(GLOBJ)gsht.$(OBJ) $(GLOBJ)gshtscr.$(OBJ)
LIB7s=$(GLOBJ)gsimage.$(OBJ) $(GLOBJ)gsimpath.$(OBJ) $(GLOBJ)gsinit.$(OBJ)
LIB8s=$(GLOBJ)gsiodev.$(OBJ) $(GLOBJ)gsline.$(OBJ) $(GLOBJ)gsmalloc.$(OBJ)
LIB9s=$(GLOBJ)gsmatrix.$(OBJ) $(GLOBJ)gsmemory.$(OBJ) $(GLOBJ)gsmisc.$(OBJ)
LIB10s=$(GLOBJ)gspaint.$(OBJ) $(GLOBJ)gsparam.$(OBJ) $(GLOBJ)gspath.$(OBJ)
LIB11s=$(GLOBJ)gsstate.$(OBJ) $(GLOBJ)gstext.$(OBJ) $(GLOBJ)gsutil.$(OBJ)
LIB1x=$(GLOBJ)gxacpath.$(OBJ) $(GLOBJ)gxbcache.$(OBJ) $(GLOBJ)gxccache.$(OBJ) $(GLOBJ)gxccman.$(OBJ)
LIB2x=$(GLOBJ)gxcht.$(OBJ) $(GLOBJ)gxclip.$(OBJ) $(GLOBJ)gxcmap.$(OBJ) $(GLOBJ)gxcpath.$(OBJ)
LIB3x=$(GLOBJ)gxdcconv.$(OBJ) $(GLOBJ)gxdcolor.$(OBJ) $(GLOBJ)gxdither.$(OBJ)
LIB4x=$(GLOBJ)gxfill.$(OBJ) $(GLOBJ)gxht.$(OBJ)
LIB5x=$(GLOBJ)gxiinit.$(OBJ) $(GLOBJ)gxidata.$(OBJ) $(GLOBJ)gxifast.$(OBJ) $(GLOBJ)gximono.$(OBJ)
LIB6x=$(GLOBJ)gxpaint.$(OBJ) $(GLOBJ)gxpath.$(OBJ) $(GLOBJ)gxpath2.$(OBJ) $(GLOBJ)gxpcopy.$(OBJ)
LIB7x=$(GLOBJ)gxpdash.$(OBJ) $(GLOBJ)gxpflat.$(OBJ) $(GLOBJ)gxsample.$(OBJ) $(GLOBJ)gxstroke.$(OBJ)
LIB1d=$(GLOBJ)gdevabuf.$(OBJ) $(GLOBJ)gdevdbit.$(OBJ) $(GLOBJ)gdevddrw.$(OBJ)
LIB2d=$(GLOBJ)gdevdflt.$(OBJ) $(GLOBJ)gdevdgbr.$(OBJ) $(GLOBJ)gdevnfwd.$(OBJ) $(GLOBJ)gdevmem.$(OBJ)
LIB3d=$(GLOBJ)gdevm1.$(OBJ) $(GLOBJ)gdevm2.$(OBJ) $(GLOBJ)gdevm4.$(OBJ) $(GLOBJ)gdevm8.$(OBJ)
LIB4d=$(GLOBJ)gdevm16.$(OBJ) $(GLOBJ)gdevm24.$(OBJ) $(GLOBJ)gdevm32.$(OBJ) $(GLOBJ)gdevmpla.$(OBJ)
LIBs=$(LIB1s) $(LIB2s) $(LIB3s) $(LIB4s) $(LIB5s) $(LIB6s) $(LIB7s) $(LIB8s)\
 $(LIB9s) $(LIB10s) $(LIB11s)
LIBx=$(LIB1x) $(LIB2x) $(LIB3x) $(LIB4x) $(LIB5x) $(LIB6x) $(LIB7x)
LIBd=$(LIB1d) $(LIB2d) $(LIB3d) $(LIB4d)
LIB_ALL=$(LIBs) $(LIBx) $(LIBd)
libs.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBs)
	$(ECHOGS_XE) -w libs.dev $(LIB1s)
	$(ECHOGS_XE) -a libs.dev $(LIB2s)
	$(ECHOGS_XE) -a libs.dev $(LIB3s)
	$(ECHOGS_XE) -a libs.dev $(LIB4s)
	$(ECHOGS_XE) -a libs.dev $(LIB5s)
	$(ECHOGS_XE) -a libs.dev $(LIB6s)
	$(ECHOGS_XE) -a libs.dev $(LIB7s)
	$(ECHOGS_XE) -a libs.dev $(LIB8s)
	$(ECHOGS_XE) -a libs.dev $(LIB9s)
	$(ECHOGS_XE) -a libs.dev $(LIB10s)
	$(ECHOGS_XE) -a libs.dev $(LIB11s)
	$(ADDMOD) libs -init gshtscr gsutil

libx.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBx)
	$(ECHOGS_XE) -w libx.dev $(LIB1x)
	$(ECHOGS_XE) -a libx.dev $(LIB2x)
	$(ECHOGS_XE) -a libx.dev $(LIB3x)
	$(ECHOGS_XE) -a libx.dev $(LIB4x)
	$(ECHOGS_XE) -a libx.dev $(LIB5x)
	$(ECHOGS_XE) -a libx.dev $(LIB6x)
	$(ECHOGS_XE) -a libx.dev $(LIB7x)
	$(ADDMOD) libx -init gxcht gxifast gximono

libd.dev: $(LIB_MAK) $(ECHOGS_XE) $(LIBd)
	$(ECHOGS_XE) -w libd.dev $(LIB1d)
	$(ECHOGS_XE) -a libd.dev $(LIB2d)
	$(ECHOGS_XE) -a libd.dev $(LIB3d)
	$(ECHOGS_XE) -a libd.dev $(LIB4d)

libcore.dev: $(LIB_MAK) $(ECHOGS_XE)\
 libs.dev libx.dev libd.dev iscale.dev noroplib.dev
	$(SETMOD) libcore
	$(ADDMOD) libcore -dev2 nullpage
	$(ADDMOD) libcore -include libs libx libd iscale noroplib

# ---------------- Stream support ---------------- #
# Currently the only things in the library that use this are clists
# and file streams.

$(GLOBJ)stream.$(OBJ): $(GLSRC)stream.c $(AK) $(stdio__h) $(memory__h)\
 $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)stream.$(OBJ) $(C_) $(GLSRC)stream.c

# ---------------- File streams ---------------- #
# Currently only the high-level drivers use these, but more drivers will
# probably use them eventually.

sfile_=$(GLOBJ)sfx$(FILE_IMPLEMENTATION).$(OBJ) $(GLOBJ)stream.$(OBJ)
sfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(sfile_)
	$(SETMOD) sfile $(sfile_)

$(GLOBJ)sfxstdio.$(OBJ): $(GLSRC)sfxstdio.c $(AK) $(stdio__h) $(memory__h)\
 $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)sfxstdio.$(OBJ) $(C_) $(GLSRC)sfxstdio.c

$(GLOBJ)sfxfd.$(OBJ): $(GLSRC)sfxfd.c $(AK) $(stdio__h) $(errno__h) $(memory__h)\
 $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)sfxfd.$(OBJ) $(C_) $(GLSRC)sfxfd.c

$(GLOBJ)sfxboth.$(OBJ): $(GLSRC)sfxboth.c $(GLSRC)sfxstdio.c $(GLSRC)sfxfd.c
	$(GLCC) $(GLO_)sfxboth.$(OBJ) $(C_) $(GLSRC)sfxboth.c

# ---------------- CCITTFax filters ---------------- #
# These are used by clists, some drivers, and Level 2 in general.

cfe_=$(GLOBJ)scfe.$(OBJ) $(GLOBJ)scfetab.$(OBJ) $(GLOBJ)shc.$(OBJ)
cfe.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfe_)
	$(SETMOD) cfe $(cfe_)

$(GLOBJ)scfe.$(OBJ): $(GLSRC)scfe.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
 $(scf_h) $(strimpl_h) $(scfx_h)
	$(GLCC) $(GLO_)scfe.$(OBJ) $(C_) $(GLSRC)scfe.c

$(GLOBJ)scfetab.$(OBJ): $(GLSRC)scfetab.c $(AK) $(std_h) $(scommon_h) $(scf_h)
	$(GLCC) $(GLO_)scfetab.$(OBJ) $(C_) $(GLSRC)scfetab.c

$(GLOBJ)shc.$(OBJ): $(GLSRC)shc.c $(AK) $(std_h) $(scommon_h) $(shc_h)
	$(GLCC) $(GLO_)shc.$(OBJ) $(C_) $(GLSRC)shc.c

cfd_=$(GLOBJ)scfd.$(OBJ) $(GLOBJ)scfdtab.$(OBJ)
cfd.dev: $(LIB_MAK) $(ECHOGS_XE) $(cfd_)
	$(SETMOD) cfd $(cfd_)

$(GLOBJ)scfd.$(OBJ): $(GLSRC)scfd.c $(AK) $(memory__h) $(stdio__h) $(gdebug_h)\
 $(scf_h) $(strimpl_h) $(scfx_h)
	$(GLCC) $(GLO_)scfd.$(OBJ) $(C_) $(GLSRC)scfd.c

$(GLOBJ)scfdtab.$(OBJ): $(GLSRC)scfdtab.c $(AK) $(std_h) $(scommon_h) $(scf_h)
	$(GLCC) $(GLO_)scfdtab.$(OBJ) $(C_) $(GLSRC)scfdtab.c

# scfparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in cfe or cfd.
$(GLOBJ)scfparam.$(OBJ): $(GLSRC)scfparam.c $(AK) $(std_h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(scommon_h) $(scf_h) $(scfx_h)
	$(GLCC) $(GLO_)scfparam.$(OBJ) $(C_) $(GLSRC)scfparam.c

# ---------------- DCT (JPEG) filters ---------------- #
# These are used by Level 2, and by the JPEG-writing driver.

# Common code

sdcparam_h=$(GLSRC)sdcparam.h

sdctc_=$(GLOBJ)sdctc.$(OBJ) $(GLOBJ)sjpegc.$(OBJ)

$(GLOBJ)sdctc.$(OBJ): $(GLSRC)sdctc.c $(AK) $(stdio__h) $(jpeglib_h)\
 $(gsmalloc_h) $(gsmemory_h) $(sdct_h) $(strimpl_h)
	$(GLCC) $(GLO_)sdctc.$(OBJ) $(C_) $(GLSRC)sdctc.c

$(GLOBJ)sjpegc.$(OBJ): $(GLSRC)sjpegc.c $(AK) $(stdio__h) $(string__h) $(gx_h)\
 $(jerror_h) $(jpeglib_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h)
	$(GLJCC) $(GLO_)sjpegc.$(OBJ) $(C_) $(GLSRC)sjpegc.c

# sdcparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in sdcte/d.
$(GLOBJ)sdcparam.$(OBJ): $(GLSRC)sdcparam.c $(AK) $(memory__h)\
 $(jpeglib_h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(GLCC) $(GLO_)sdcparam.$(OBJ) $(C_) $(GLSRC)sdcparam.c

# Encoding (compression)

sdcte_=$(sdctc_) $(GLOBJ)sdcte.$(OBJ) $(GLOBJ)sjpege.$(OBJ)
sdcte.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdcte_) $(JGENDIR)$(D)jpege.dev
	$(SETMOD) sdcte $(sdcte_)
	$(ADDMOD) sdcte -include $(JGENDIR)$(D)jpege

$(GLOBJ)sdcte.$(OBJ): $(GLSRC)sdcte.c $(AK)\
 $(memory__h) $(stdio__h) $(gdebug_h) $(gsmalloc_h) $(gsmemory_h)\
 $(jerror_h) $(jpeglib_h)\
 $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(GLJCC) $(GLO_)sdcte.$(OBJ) $(C_) $(GLSRC)sdcte.c

$(GLOBJ)sjpege.$(OBJ): $(GLSRC)sjpege.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jerror_h) $(jpeglib_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h)
	$(GLJCC) $(GLO_)sjpege.$(OBJ) $(C_) $(GLSRC)sjpege.c

# sdeparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in sdcte.
$(GLOBJ)sdeparam.$(OBJ): $(GLSRC)sdeparam.c $(AK) $(memory__h)\
 $(jpeglib_h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(GLCC) $(GLO_)sdeparam.$(OBJ) $(C_) $(GLSRC)sdeparam.c

# Decoding (decompression)

sdctd_=$(sdctc_) $(GLOBJ)sdctd.$(OBJ) $(GLOBJ)sjpegd.$(OBJ)
sdctd.dev: $(LIB_MAK) $(ECHOGS_XE) $(sdctd_) $(JGENDIR)$(D)jpegd.dev
	$(SETMOD) sdctd $(sdctd_)
	$(ADDMOD) sdctd -include $(JGENDIR)$(D)jpegd

$(GLOBJ)sdctd.$(OBJ): $(GLSRC)sdctd.c $(AK)\
 $(memory__h) $(stdio__h) $(gdebug_h) $(gsmalloc_h) $(gsmemory_h)\
 $(jerror_h) $(jpeglib_h)\
 $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(GLJCC) $(GLO_)sdctd.$(OBJ) $(C_) $(GLSRC)sdctd.c

$(GLOBJ)sjpegd.$(OBJ): $(GLSRC)sjpegd.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jerror_h) $(jpeglib_h)\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h)
	$(GLJCC) $(GLO_)sjpegd.$(OBJ) $(C_) $(GLSRC)sjpegd.c

# sddparam is used by the filter operator.
# It is not included automatically in sdctd.
$(GLOBJ)sddparam.$(OBJ): $(GLSRC)sddparam.c $(AK) $(std_h)\
 $(jpeglib_h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h)
	$(GLCC) $(GLO_)sddparam.$(OBJ) $(C_) $(GLSRC)sddparam.c

# ---------------- LZW filters ---------------- #
# These are used by Level 2 in general.

slzwe_=slzwce
#slzwe_=slzwe
lzwe_=$(GLOBJ)$(slzwe_).$(OBJ) $(GLOBJ)slzwc.$(OBJ)
lzwe.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwe_)
	$(SETMOD) lzwe $(lzwe_)

# We need slzwe.dev as a synonym for lzwe.dev for BAND_LIST_STORAGE = memory.
slzwe.dev: lzwe.dev
	$(CP_) lzwe.dev slzwe.dev

$(GLOBJ)slzwce.$(OBJ): $(GLSRC)slzwce.c $(AK) $(stdio__h) $(gdebug_h)\
 $(slzwx_h) $(strimpl_h)
	$(GLCC) $(GLO_)slzwce.$(OBJ) $(C_) $(GLSRC)slzwce.c

$(GLOBJ)slzwe.$(OBJ): $(GLSRC)slzwe.c $(AK) $(stdio__h) $(gdebug_h)\
 $(slzwx_h) $(strimpl_h)
	$(GLCC) $(GLO_)slzwe.$(OBJ) $(C_) $(GLSRC)slzwe.c

$(GLOBJ)slzwc.$(OBJ): $(GLSRC)slzwc.c $(AK) $(std_h)\
 $(slzwx_h) $(strimpl_h)
	$(GLCC) $(GLO_)slzwc.$(OBJ) $(C_) $(GLSRC)slzwc.c

lzwd_=$(GLOBJ)slzwd.$(OBJ) $(GLOBJ)slzwc.$(OBJ)
lzwd.dev: $(LIB_MAK) $(ECHOGS_XE) $(lzwd_)
	$(SETMOD) lzwd $(lzwd_)

# We need slzwd.dev as a synonym for lzwd.dev for BAND_LIST_STORAGE = memory.
slzwd.dev: lzwd.dev
	$(CP_) lzwd.dev slzwd.dev

$(GLOBJ)slzwd.$(OBJ): $(GLSRC)slzwd.c $(AK) $(stdio__h) $(gdebug_h)\
 $(slzwx_h) $(strimpl_h)
	$(GLCC) $(GLO_)slzwd.$(OBJ) $(C_) $(GLSRC)slzwd.c

# ---------------- PCX decoding filter ---------------- #
# This is an adhoc filter not used by anything in the standard configuration.

pcxd_=$(GLOBJ)spcxd.$(OBJ)
pcxd.dev: $(LIB_MAK) $(ECHOGS_XE) $(pcxd_)
	$(SETMOD) pcxd $(pcxd_)

$(GLOBJ)spcxd.$(OBJ): $(GLSRC)spcxd.c $(AK) $(stdio__h) $(memory__h)\
 $(spcxx_h) $(strimpl_h)
	$(GLCC) $(GLO_)spcxd.$(OBJ) $(C_) $(GLSRC)spcxd.c

# ---------------- Pixel-difference filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pdiff_=$(GLOBJ)spdiff.$(OBJ)
pdiff.dev: $(LIB_MAK) $(ECHOGS_XE) $(pdiff_)
	$(SETMOD) pdiff $(pdiff_)

$(GLOBJ)spdiff.$(OBJ): $(GLSRC)spdiff.c $(AK) $(stdio__h)\
 $(spdiffx_h) $(strimpl_h)
	$(GLCC) $(GLO_)spdiff.$(OBJ) $(C_) $(GLSRC)spdiff.c

# ---------------- PNG pixel prediction filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pngp_=$(GLOBJ)spngp.$(OBJ)
pngp.dev: $(LIB_MAK) $(ECHOGS_XE) $(pngp_)
	$(SETMOD) pngp $(pngp_)

$(GLOBJ)spngp.$(OBJ): $(GLSRC)spngp.c $(AK) $(memory__h)\
 $(spngpx_h) $(strimpl_h)
	$(GLCC) $(GLO_)spngp.$(OBJ) $(C_) $(GLSRC)spngp.c

# ---------------- RunLength filters ---------------- #
# These are used by clists and also by Level 2 in general.

rle_=$(GLOBJ)srle.$(OBJ)
rle.dev: $(LIB_MAK) $(ECHOGS_XE) $(rle_)
	$(SETMOD) rle $(rle_)

$(GLOBJ)srle.$(OBJ): $(GLSRC)srle.c $(AK) $(stdio__h) $(memory__h)\
 $(srlx_h) $(strimpl_h)
	$(GLCC) $(GLO_)srle.$(OBJ) $(C_) $(GLSRC)srle.c

rld_=$(GLOBJ)srld.$(OBJ)
rld.dev: $(LIB_MAK) $(ECHOGS_XE) $(rld_)
	$(SETMOD) rld $(rld_)

$(GLOBJ)srld.$(OBJ): $(GLSRC)srld.c $(AK) $(stdio__h) $(memory__h)\
 $(srlx_h) $(strimpl_h)
	$(GLCC) $(GLO_)srld.$(OBJ) $(C_) $(GLSRC)srld.c

# ---------------- String encoding/decoding filters ---------------- #
# These are used by the PostScript and PDF writers, and also by the
# PostScript interpreter.

$(GLOBJ)scantab.$(OBJ): $(GLSRC)scantab.c $(AK) $(stdpre_h)\
 $(scanchar_h) $(scommon_h)
	$(GLCC) $(GLO_)scantab.$(OBJ) $(C_) $(GLSRC)scantab.c

$(GLOBJ)sfilter2.$(OBJ): $(GLSRC)sfilter2.c $(AK) $(memory__h) $(stdio__h)\
 $(sa85x_h) $(scanchar_h) $(sbtx_h) $(strimpl_h)
	$(GLCC) $(GLO_)sfilter2.$(OBJ) $(C_) $(GLSRC)sfilter2.c

$(GLOBJ)sstring.$(OBJ): $(GLSRC)sstring.c $(AK)\
 $(stdio__h) $(memory__h) $(string__h)\
 $(scanchar_h) $(sstring_h) $(strimpl_h)
	$(GLCC) $(GLO_)sstring.$(OBJ) $(C_) $(GLSRC)sstring.c

# ---------------- zlib filters ---------------- #
# These are used by clists and are also available as filters.

szlibc_=$(GLOBJ)szlibc.$(OBJ)

$(GLOBJ)szlibc.$(OBJ): $(GLSRC)szlibc.c $(AK) $(std_h)\
 $(gserror_h) $(gserrors_h) $(gsmalloc_h) $(gsmemory_h)\
 $(gsstruct_h) $(gstypes_h)\
 $(strimpl_h) $(szlibxx_h)
	$(GLZCC) $(GLO_)szlibc.$(OBJ) $(C_) $(GLSRC)szlibc.c

szlibe_=$(szlibc_) $(GLOBJ)szlibe.$(OBJ)
szlibe.dev: $(LIB_MAK) $(ECHOGS_XE) $(ZGENDIR)$(D)zlibe.dev $(szlibe_)
	$(SETMOD) szlibe $(szlibe_)
	$(ADDMOD) szlibe -include $(ZGENDIR)$(D)zlibe

$(GLOBJ)szlibe.$(OBJ): $(GLSRC)szlibe.c $(AK) $(std_h)\
 $(gsmalloc_h) $(gsmemory_h) $(strimpl_h) $(szlibxx_h)
	$(GLZCC) $(GLO_)szlibe.$(OBJ) $(C_) $(GLSRC)szlibe.c

szlibd_=$(szlibc_) $(GLOBJ)szlibd.$(OBJ)
szlibd.dev: $(LIB_MAK) $(ECHOGS_XE) $(ZGENDIR)$(D)zlibd.dev $(szlibd_)
	$(SETMOD) szlibd $(szlibd_)
	$(ADDMOD) szlibd -include $(ZGENDIR)$(D)zlibd

$(GLOBJ)szlibd.$(OBJ): $(GLSRC)szlibd.c $(AK) $(std_h)\
 $(gsmalloc_h) $(gsmemory_h) $(strimpl_h) $(szlibxx_h)
	$(GLZCC) $(GLO_)szlibd.$(OBJ) $(C_) $(GLSRC)szlibd.c

# ---------------- Page devices ---------------- #
# We include this here, rather than in devs.mak, because it is more like
# a feature than a simple device.

gdevprn_h=$(GLSRC)gdevprn.h $(memory__h) $(string__h) $(gp_h) $(gx_h)\
 $(gserrors_h) $(gsmatrix_h) $(gsparam_h) $(gsutil_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxclist_h)

page_=$(GLOBJ)gdevprn.$(OBJ)
page.dev: $(LIB_MAK) $(ECHOGS_XE) $(page_) clist.dev
	$(SETMOD) page $(page_)
	$(ADDMOD) page -include clist

$(GLOBJ)gdevprn.$(OBJ): $(GLSRC)gdevprn.c $(ctype__h)\
 $(gdevprn_h) $(gp_h) $(gsparam_h) $(gxclio_h)
	$(GLCC) $(GLO_)gdevprn.$(OBJ) $(C_) $(GLSRC)gdevprn.c

# ---------------- Banded ("command list") devices ---------------- #

gdevht_h=$(GLSRC)gdevht.h $(gzht_h)
gxcldev_h=$(GLSRC)gxcldev.h $(gxclist_h) $(gsropt_h) $(gxht_h) $(gxtmap_h) $(gxdht_h)\
 $(strimpl_h) $(scfx_h) $(srlx_h)
gxclpage_h=$(GLSRC)gxclpage.h $(gxclio_h)
gxclpath_h=$(GLSRC)gxclpath.h $(gxfixed_h)

clbase1_=$(GLOBJ)gxclist.$(OBJ) $(GLOBJ)gxclbits.$(OBJ) $(GLOBJ)gxclpage.$(OBJ)
clbase2_=$(GLOBJ)gxclrast.$(OBJ) $(GLOBJ)gxclread.$(OBJ) $(GLOBJ)gxclrect.$(OBJ)
clbase3_=$(GLOBJ)gxclutil.$(OBJ) $(GLOBJ)gsparams.$(OBJ) $(GLOBJ)stream.$(OBJ)
clpath_=$(GLOBJ)gxclimag.$(OBJ) $(GLOBJ)gxclpath.$(OBJ)
clist_=$(clbase1_) $(clbase2_) $(clbase3_) $(clpath_)
clist.dev: $(LIB_MAK) $(ECHOGS_XE) $(clist_) cl$(BAND_LIST_STORAGE).dev\
 cfe.dev cfd.dev rle.dev rld.dev psl2cs.dev
	$(SETMOD) clist $(clbase1_)
	$(ADDMOD) clist -obj $(clbase2_)
	$(ADDMOD) clist -obj $(clbase3_)
	$(ADDMOD) clist -obj $(clpath_)
	$(ADDMOD) clist -include cl$(BAND_LIST_STORAGE) cfe cfd rle rld psl2cs

$(GLOBJ)gdevht.$(OBJ): $(GLSRC)gdevht.c $(GXERR)\
 $(gdevht_h) $(gxdcconv_h) $(gxdcolor_h) $(gxdevice_h) $(gxdither_h)
	$(GLCC) $(GLO_)gdevht.$(OBJ) $(C_) $(GLSRC)gdevht.c

$(GLOBJ)gxclist.$(OBJ): $(GLSRC)gxclist.c $(GXERR) $(memory__h) $(string__h)\
 $(gp_h) $(gpcheck_h) $(gsparams_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gxclist.$(OBJ) $(C_) $(GLSRC)gxclist.c

$(GLOBJ)gxclbits.$(OBJ): $(GLSRC)gxclbits.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxfmap_h)
	$(GLCC) $(GLO_)gxclbits.$(OBJ) $(C_) $(GLSRC)gxclbits.c

$(GLOBJ)gxclpage.$(OBJ): $(GLSRC)gxclpage.c $(AK)\
 $(gdevprn_h) $(gxcldev_h) $(gxclpage_h)
	$(GLCC) $(GLO_)gxclpage.$(OBJ) $(C_) $(GLSRC)gxclpage.c

$(GLOBJ)gxclrast.$(OBJ): $(GLSRC)gxclrast.c $(GXERR)\
 $(memory__h) $(gp_h) $(gpcheck_h)\
 $(gdevht_h)\
 $(gsbitops_h) $(gscoord_h) $(gsdevice_h) $(gsparams_h) $(gsstate_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcmap_h) $(gxcspace_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h)\
 $(gxhttile_h) $(gxpaint_h) $(gzacpath_h) $(gzcpath_h) $(gzpath_h)\
 $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)gxclrast.$(OBJ) $(C_) $(GLSRC)gxclrast.c

$(GLOBJ)gxclread.$(OBJ): $(GLSRC)gxclread.c $(GXERR)\
 $(memory__h) $(gp_h) $(gpcheck_h)\
 $(gdevht_h)\
 $(gscoord_h) $(gsdevice_h)\
 $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(gxhttile_h)\
 $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)gxclread.$(OBJ) $(C_) $(GLSRC)gxclread.c

$(GLOBJ)gxclrect.$(OBJ): $(GLSRC)gxclrect.c $(GXERR)\
 $(gsutil_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gxclrect.$(OBJ) $(C_) $(GLSRC)gxclrect.c

$(GLOBJ)gxclimag.$(OBJ): $(GLSRC)gxclimag.c $(GXERR) $(math__h) $(memory__h)\
 $(gscspace_h)\
 $(gxarith_h) $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfmap_h) $(gxiparam_h) $(gxpath_h)\
 $(siscale_h) $(strimpl_h)
	$(GLCC) $(GLO_)gxclimag.$(OBJ) $(C_) $(GLSRC)gxclimag.c

$(GLOBJ)gxclpath.$(OBJ): $(GLSRC)gxclpath.c $(GXERR)\
 $(math__h) $(memory__h) $(gpcheck_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxcolor2_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)
	$(GLCC) $(GLO_)gxclpath.$(OBJ) $(C_) $(GLSRC)gxclpath.c

$(GLOBJ)gxclutil.$(OBJ): $(GLSRC)gxclutil.c $(GXERR) $(memory__h) $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsparams_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gxclutil.$(OBJ) $(C_) $(GLSRC)gxclutil.c

# Implement band lists on files.

clfile_=$(GLOBJ)gxclfile.$(OBJ)
clfile.dev: $(LIB_MAK) $(ECHOGS_XE) $(clfile_)
	$(SETMOD) clfile $(clfile_)

$(GLOBJ)gxclfile.$(OBJ): $(GLSRC)gxclfile.c $(stdio__h) $(string__h)\
 $(gp_h) $(gsmemory_h) $(gserror_h) $(gserrors_h) $(gxclio_h)
	$(GLCC) $(GLO_)gxclfile.$(OBJ) $(C_) $(GLSRC)gxclfile.c

# Implement band lists in memory (RAM).

clmemory_=$(GLOBJ)gxclmem.$(OBJ) $(GLOBJ)gxcl$(BAND_LIST_COMPRESSOR).$(OBJ)
clmemory.dev: $(LIB_MAK) $(ECHOGS_XE) $(clmemory_) s$(BAND_LIST_COMPRESSOR)e.dev s$(BAND_LIST_COMPRESSOR)d.dev
	$(SETMOD) clmemory $(clmemory_)
	$(ADDMOD) clmemory -include s$(BAND_LIST_COMPRESSOR)e s$(BAND_LIST_COMPRESSOR)d
	$(ADDMOD) clmemory -init cl_$(BAND_LIST_COMPRESSOR)

gxclmem_h=$(GLSRC)gxclmem.h $(gxclio_h) $(strimpl_h)

$(GLOBJ)gxclmem.$(OBJ): $(GLSRC)gxclmem.c $(GXERR) $(LIB_MAK) $(memory__h)\
 $(gsmalloc_h) $(gxclmem_h)
	$(GLCC) $(GLO_)gxclmem.$(OBJ) $(C_) $(GLSRC)gxclmem.c

# Implement the compression method for RAM-based band lists.

$(GLOBJ)gxcllzw.$(OBJ): $(GLSRC)gxcllzw.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(slzwx_h)
	$(GLCC) $(GLO_)gxcllzw.$(OBJ) $(C_) $(GLSRC)gxcllzw.c

$(GLOBJ)gxclzlib.$(OBJ): $(GLSRC)gxclzlib.c $(std_h)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(szlibx_h)
	$(GLCC) $(GLO_)gxclzlib.$(OBJ) $(C_) $(GLSRC)gxclzlib.c

# ---------------- Vector devices ---------------- #
# We include this here for the same reasons as page.dev.

gdevvec_h=$(GLSRC)gdevvec.h $(gdevbbox_h) $(gp_h)\
 $(gsropt_h) $(gxdevice_h) $(gxiparam_h) $(gxistate_h) $(stream_h)

vector_=$(GLOBJ)gdevvec.$(OBJ)
vector.dev: $(LIB_MAK) $(ECHOGS_XE) $(vector_) bbox.dev sfile.dev
	$(SETMOD) vector $(vector_)
	$(ADDMOD) vector -include bbox sfile

$(GLOBJ)gdevvec.$(OBJ): $(GLSRC)gdevvec.c $(GXERR)\
 $(math__h) $(memory__h) $(string__h)\
 $(gdevvec_h) $(gp_h) $(gscspace_h) $(gsparam_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxfixed_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)
	$(GLCC) $(GLO_)gdevvec.$(OBJ) $(C_) $(GLSRC)gdevvec.c

# ---------------- Image scaling filter ---------------- #

iscale_=$(GLOBJ)siscale.$(OBJ)
iscale.dev: $(LIB_MAK) $(ECHOGS_XE) $(iscale_)
	$(SETMOD) iscale $(iscale_)

$(GLOBJ)siscale.$(OBJ): $(GLSRC)siscale.c $(AK)\
 $(math__h) $(memory__h) $(stdio__h)\
 $(siscale_h) $(strimpl_h)
	$(GLCC) $(GLO_)siscale.$(OBJ) $(C_) $(GLSRC)siscale.c

# ---------------- Extended halftone support ---------------- #
# This is only used by one non-PostScript-based project.

gshtx_h=$(GLSRC)gshtx.h $(gscsepnm_h) $(gsht1_h) $(gsmemory_h) $(gxtmap_h)

htxlib_=$(GLOBJ)gshtx.$(OBJ)
htxlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(htxlib_)
	$(SETMOD) htxlib $(htxlib_)

$(GLOBJ)gshtx.$(OBJ): $(GLSRC)gshtx.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h)\
 $(gxfmap_h) $(gshtx_h) $(gzht_h) $(gzstate_h)
	$(GLCC) $(GLO_)gshtx.$(OBJ) $(C_) $(GLSRC)gshtx.c

# ---------------- RasterOp et al ---------------- #
# Note that noroplib is a default, roplib replaces it.

gsropc_h=$(GLSRC)gsropc.h $(gscompt_h) $(gsropt_h)
gxropc_h=$(GLSRC)gxropc.h $(gsropc_h) $(gxcomp_h)

noroplib_=$(GLOBJ)gsnorop.$(OBJ)
noroplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(noroplib_)
	$(SETMOD) noroplib $(noroplib_)

$(GLOBJ)gsnorop.$(OBJ): $(GLSRC)gsnorop.c $(GXERR)\
 $(gdevmrop_h) $(gsrop_h) $(gxdevcli_h)
	$(GLCC) $(GLO_)gsnorop.$(OBJ) $(C_) $(GLSRC)gsnorop.c

roplib1_=$(GLOBJ)gdevdrop.$(OBJ) $(GLOBJ)gdevmrop.$(OBJ) $(GLOBJ)gdevrops.$(OBJ)
roplib2_=$(GLOBJ)gsrop.$(OBJ) $(GLOBJ)gsroptab.$(OBJ)
roplib_=$(roplib1_) $(roplib2_)
roplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(roplib_)
	$(SETMOD) roplib $(roplib1_)
	$(ADDMOD) roplib $(roplib2_)
	$(ADDMOD) roplib -replace noroplib

$(GLOBJ)gdevdrop.$(OBJ): $(GLSRC)gdevdrop.c $(GXERR) $(memory__h)\
 $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gxgetbit_h)\
 $(gdevmrop_h)
	$(GLCC) $(GLO_)gdevdrop.$(OBJ) $(C_) $(GLSRC)gdevdrop.c

$(GLOBJ)gdevmrop.$(OBJ): $(GLSRC)gdevmrop.c $(GXERR) $(memory__h)\
 $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gdevmrop_h)
	$(GLCC) $(GLO_)gdevmrop.$(OBJ) $(C_) $(GLSRC)gdevmrop.c

$(GLOBJ)gdevrops.$(OBJ): $(GLSRC)gdevrops.c $(GXERR)\
 $(gxdcolor_h) $(gxdevice_h) $(gdevmrop_h)
	$(GLCC) $(GLO_)gdevrops.$(OBJ) $(C_) $(GLSRC)gdevrops.c

$(GLOBJ)gsrop.$(OBJ): $(GLSRC)gsrop.c $(GXERR)\
 $(gsrop_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsrop.$(OBJ) $(C_) $(GLSRC)gsrop.c

#**************** $(CC_LEAF) $(GLSRC)gsroptab.c $(GLO_)gsroptab.$(OBJ)
$(GLOBJ)gsroptab.$(OBJ): $(GLSRC)gsroptab.c $(stdpre_h) $(gsropt_h)
	$(GLCC) $(GLO_)gsroptab.$(OBJ) $(C_) $(GLSRC)gsroptab.c

# The following is not used yet.
$(GLOBJ)gsropc.$(OBJ): $(GLSRC)gsropc.c $(GXERR)\
 $(gsutil_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxropc_h)
	$(GLCC) $(GLO_)gsropc.$(OBJ) $(C_) $(GLSRC)gsropc.c

# ---------------- Async rendering ---------------- #

gxpageq_h=$(GLSRC)gxpageq.h $(gsmemory_h) $(gxband_h) $(gxsync_h)
gdevprna_h=$(GLSRC)gdevprna.h $(gdevprn_h) $(gxsync_h)

async1_=$(GLOBJ)gdevprna.$(OBJ) $(GLOBJ)gxpageq.$(OBJ)
async2_=$(GLOBJ)gsmemlok.$(OBJ) $(GLOBJ)gxsync.$(OBJ)
async_=$(async1_) $(async2_)
async.dev: $(LIB_MAK) $(ECHOGS_XE) $(async_) clist.dev
	$(SETMOD) async $(async1_)
	$(ADDMOD) async $(async2_)
	$(ADDMOD) async -include clist

$(GLOBJ)gdevprna.$(OBJ): $(GLSRC)gdevprna.c $(AK)\
 $(gdevprna_h)\
 $(gsalloc_h) $(gsdevice_h) $(gsmemlok_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxpageq_h) $(gzht_h)
	$(GLCC) $(GLO_)gdevprna.$(OBJ) $(C_) $(GLSRC)gdevprna.c

$(GLOBJ)gxpageq.$(OBJ): $(GLSRC)gxpageq.c $(GXERR)\
 $(gxdevice_h) $(gxclist_h) $(gxpageq_h)
	$(GLCC) $(GLO_)gxpageq.$(OBJ) $(C_) $(GLSRC)gxpageq.c

# -------- Composite (PostScript Type 0) font support -------- #

cmaplib_=$(GLOBJ)gsfcmap.$(OBJ)
cmaplib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmaplib_)
	$(SETMOD) cmaplib $(cmaplib_)

$(GLOBJ)gsfcmap.$(OBJ): $(GLSRC)gsfcmap.c $(GXERR)\
 $(gsstruct_h) $(gxfcmap_h)
	$(GLCC) $(GLO_)gsfcmap.$(OBJ) $(C_) $(GLSRC)gsfcmap.c

psf0lib_=$(GLOBJ)gschar0.$(OBJ) $(GLOBJ)gsfont0.$(OBJ)
psf0lib.dev: $(LIB_MAK) $(ECHOGS_XE) cmaplib.dev $(psf0lib_)
	$(SETMOD) psf0lib $(psf0lib_)
	$(ADDMOD) psf0lib -include cmaplib

$(GLOBJ)gschar0.$(OBJ): $(GLSRC)gschar0.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxfixed_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gsfcmap_h) $(gxfont_h) $(gxfont0_h) $(gxchar_h)
	$(GLCC) $(GLO_)gschar0.$(OBJ) $(C_) $(GLSRC)gschar0.c

$(GLOBJ)gsfont0.$(OBJ): $(GLSRC)gsfont0.c $(GXERR) $(memory__h)\
 $(gsmatrix_h) $(gsstruct_h) $(gxfixed_h) $(gxdevmem_h) $(gxfcache_h)\
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gsfont0.$(OBJ) $(C_) $(GLSRC)gsfont0.c

# ---------------- Masked images ---------------- #
# This feature is out of level order beause Patterns require it
# (which they shouldn't).

gsiparm3_h=$(GLSRC)gsiparm3.h $(gsiparam_h)
gsiparm4_h=$(GLSRC)gsiparm4.h $(gsiparam_h)

$(GLOBJ)gxclipm.$(OBJ): $(GLSRC)gxclipm.c $(GX) $(memory__h)\
 $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gxclipm.$(OBJ) $(C_) $(GLSRC)gxclipm.c

$(GLOBJ)gximage3.$(OBJ): $(GLSRC)gximage3.c $(GXERR) $(math__h) $(memory__h)\
 $(gsbitops_h) $(gscspace_h) $(gsiparm3_h) $(gsstruct_h)\
 $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h) $(gxiparam_h) $(gxistate_h)
	$(GLCC) $(GLO_)gximage3.$(OBJ) $(C_) $(GLSRC)gximage3.c

$(GLOBJ)gximage4.$(OBJ): $(GLSRC)gximage4.c $(memory__h) $(GXERR)\
 $(gscspace_h) $(gsiparm3_h) $(gsiparm4_h) $(gxiparam_h)
	$(GLCC) $(GLO_)gximage4.$(OBJ) $(C_) $(GLSRC)gximage4.c

imasklib_=$(GLOBJ)gxclipm.$(OBJ) $(GLOBJ)gximage3.$(OBJ) $(GLOBJ)gximage4.$(OBJ) $(GLOBJ)gxmclip.$(OBJ)
imasklib.dev: $(LIB_MAK) $(ECHOGS_XE) $(imasklib_)
	$(SETMOD) imasklib $(imasklib_)

# ---------------- Pattern color ---------------- #

patlib_1=$(GLOBJ)gspcolor.$(OBJ) $(GLOBJ)gxclip2.$(OBJ) $(GLOBJ)gxmclip.$(OBJ)
patlib_2=$(GLOBJ)gxp1fill.$(OBJ) $(GLOBJ)gxpcmap.$(OBJ)
patlib_=$(patlib_1) $(patlib_2)
# Currently this feature requires masked images, but it shouldn't.
patlib.dev: $(LIB_MAK) $(ECHOGS_XE) cmyklib.dev imasklib.dev psl2cs.dev $(patlib_)
	$(SETMOD) patlib -include cmyklib imasklib psl2cs
	$(ADDMOD) patlib -obj $(patlib_1)
	$(ADDMOD) patlib -obj $(patlib_2)

$(GLOBJ)gspcolor.$(OBJ): $(GLSRC)gspcolor.c $(GXERR) $(math__h)\
 $(gsimage_h) $(gsiparm4_h) $(gspath_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h)\
 $(gxarith_h) $(gxcolor2_h) $(gxcoord_h) $(gxclip2_h) $(gxcspace_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxp1fill_h) $(gxpath_h) $(gxpcolor_h) $(gzstate_h)
	$(GLCC) $(GLO_)gspcolor.$(OBJ) $(C_) $(GLSRC)gspcolor.c

$(GLOBJ)gxclip2.$(OBJ): $(GLSRC)gxclip2.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gxclip2_h) $(gxdevice_h) $(gxdevmem_h)
	$(GLCC) $(GLO_)gxclip2.$(OBJ) $(C_) $(GLSRC)gxclip2.c

$(GLOBJ)gxp1fill.$(OBJ): $(GLSRC)gxp1fill.c $(GXERR) $(math__h)\
 $(gsrop_h) $(gsmatrix_h)\
 $(gxcolor2_h) $(gxclip2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevcli_h)\
 $(gxdevmem_h) $(gxp1fill_h) $(gxpcolor_h)
	$(GLCC) $(GLO_)gxp1fill.$(OBJ) $(C_) $(GLSRC)gxp1fill.c

$(GLOBJ)gxpcmap.$(OBJ): $(GLSRC)gxpcmap.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gxpcmap.$(OBJ) $(C_) $(GLSRC)gxpcmap.c

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1lib_=$(GLOBJ)gxtype1.$(OBJ) $(GLOBJ)gxhint1.$(OBJ) $(GLOBJ)gxhint2.$(OBJ) $(GLOBJ)gxhint3.$(OBJ)

gscrypt1_h=$(GLSRC)gscrypt1.h
gstype1_h=$(GLSRC)gstype1.h
gxfont1_h=$(GLSRC)gxfont1.h
gxop1_h=$(GLSRC)gxop1.h
gxtype1_h=$(GLSRC)gxtype1.h $(gscrypt1_h) $(gstype1_h) $(gxop1_h)

$(GLOBJ)gxtype1.$(OBJ): $(GLSRC)gxtype1.c $(GXERR) $(math__h)\
 $(gsccode_h) $(gsline_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gxtype1.$(OBJ) $(C_) $(GLSRC)gxtype1.c

$(GLOBJ)gxhint1.$(OBJ): $(GLSRC)gxhint1.c $(GXERR)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)
	$(GLCC) $(GLO_)gxhint1.$(OBJ) $(C_) $(GLSRC)gxhint1.c

$(GLOBJ)gxhint2.$(OBJ): $(GLSRC)gxhint2.c $(GXERR) $(memory__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)
	$(GLCC) $(GLO_)gxhint2.$(OBJ) $(C_) $(GLSRC)gxhint2.c

$(GLOBJ)gxhint3.$(OBJ): $(GLSRC)gxhint3.c $(GXERR) $(math__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxtype1_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gxhint3.$(OBJ) $(C_) $(GLSRC)gxhint3.c

# Type 1 charstrings

psf1lib_=$(GLOBJ)gstype1.$(OBJ)
psf1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf1lib_) $(type1lib_)
	$(SETMOD) psf1lib $(psf1lib_)
	$(ADDMOD) psf1lib $(type1lib_)
	$(ADDMOD) psf1lib -init gstype1

$(GLOBJ)gstype1.$(OBJ): $(GLSRC)gstype1.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gstype1.$(OBJ) $(C_) $(GLSRC)gstype1.c

# Type 2 charstrings

psf2lib_=$(GLOBJ)gstype2.$(OBJ)
psf2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psf2lib_) $(type1lib_)
	$(SETMOD) psf2lib $(psf2lib_)
	$(ADDMOD) psf2lib $(type1lib_)
	$(ADDMOD) psf2lib -init gstype2

$(GLOBJ)gstype2.$(OBJ): $(GLSRC)gstype2.c $(GXERR) $(math__h) $(memory__h)\
 $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxistate_h) $(gxtype1_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gstype2.$(OBJ) $(C_) $(GLSRC)gstype2.c

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=$(GLOBJ)gstype42.$(OBJ)
ttflib.dev: $(LIB_MAK) $(ECHOGS_XE) $(ttflib_)
	$(SETMOD) ttflib $(ttflib_)

gxfont42_h=$(GLSRC)gxfont42.h

$(GLOBJ)gstype42.$(OBJ): $(GLSRC)gstype42.c $(GXERR) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxistate_h) $(gxpath_h)
	$(GLCC) $(GLO_)gstype42.$(OBJ) $(C_) $(GLSRC)gstype42.c

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

cmyklib_=$(GLOBJ)gscolor1.$(OBJ) $(GLOBJ)gsht1.$(OBJ)
cmyklib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cmyklib_)
	$(SETMOD) cmyklib $(cmyklib_)

$(GLOBJ)gscolor1.$(OBJ): $(GLSRC)gscolor1.c $(GXERR)\
 $(gsccolor_h) $(gscolor1_h) $(gsstruct_h) $(gsutil_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h)\
 $(gzstate_h)
	$(GLCC) $(GLO_)gscolor1.$(OBJ) $(C_) $(GLSRC)gscolor1.c

$(GLOBJ)gsht1.$(OBJ): $(GLSRC)gsht1.c $(GXERR) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsht1.$(OBJ) $(C_) $(GLSRC)gsht1.c

colimlib_=$(GLOBJ)gxicolor.$(OBJ)
colimlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(colimlib_)
	$(SETMOD) colimlib $(colimlib_)
	$(ADDMOD) colimlib -init gxicolor

$(GLOBJ)gxicolor.$(OBJ): $(GLSRC)gxicolor.c $(GXERR) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h)\
 $(gximage_h) $(gxistate_h) $(gxmatrix_h)\
 $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gxicolor.$(OBJ) $(C_) $(GLSRC)gxicolor.c

# ---------------- HSB color ---------------- #

hsblib_=$(GLOBJ)gshsb.$(OBJ)
hsblib.dev: $(LIB_MAK) $(ECHOGS_XE) $(hsblib_)
	$(SETMOD) hsblib $(hsblib_)

$(GLOBJ)gshsb.$(OBJ): $(GLSRC)gshsb.c $(GX)\
 $(gscolor_h) $(gshsb_h) $(gxfrac_h)
	$(GLCC) $(GLO_)gshsb.$(OBJ) $(C_) $(GLSRC)gshsb.c

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1lib_=$(GLOBJ)gspath1.$(OBJ)
path1lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(path1lib_)
	$(SETMOD) path1lib $(path1lib_)

$(GLOBJ)gspath1.$(OBJ): $(GLSRC)gspath1.c $(GXERR) $(math__h)\
 $(gscoord_h) $(gspath_h) $(gsstruct_h)\
 $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h) $(gzpath_h)
	$(GLCC) $(GLO_)gspath1.$(OBJ) $(C_) $(GLSRC)gspath1.c

# --------------- Level 2 color space and color image support --------------- #

psl2cs_=$(GLOBJ)gscolor2.$(OBJ)
psl2cs.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2cs_)
	$(SETMOD) psl2cs $(psl2cs_)

$(GLOBJ)gscolor2.$(OBJ): $(GLSRC)gscolor2.c $(GXERR) $(memory__h)\
 $(gxarith_h) $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h)
	$(GLCC) $(GLO_)gscolor2.$(OBJ) $(C_) $(GLSRC)gscolor2.c

psl2lib_=$(GLOBJ)gxi12bit.$(OBJ) $(GLOBJ)gxiscale.$(OBJ)
psl2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl2lib_) colimlib.dev psl2cs.dev
	$(SETMOD) psl2lib $(psl2lib_)
	$(ADDMOD) psl2lib -init gxi12bit gxiscale
	$(ADDMOD) psl2lib -include colimlib psl2cs

$(GLOBJ)gxi12bit.$(OBJ): $(GLSRC)gxi12bit.c $(GXERR)\
 $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gxi12bit.$(OBJ) $(C_) $(GLSRC)gxi12bit.c

$(GLOBJ)gxiscale.$(OBJ): $(GLSRC)gxiscale.c $(GXERR)\
 $(math__h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxistate_h)\
 $(gxmatrix_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gxiscale.$(OBJ) $(C_) $(GLSRC)gxiscale.c

# ---------------- Display Postscript / Level 2 support ---------------- #

dps2lib_=$(GLOBJ)gsdps1.$(OBJ)
dps2lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dps2lib_)
	$(SETMOD) dps2lib $(dps2lib_)

$(GLOBJ)gsdps1.$(OBJ): $(GLSRC)gsdps1.c $(GXERR) $(math__h)\
 $(gscoord_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gspath2_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsdps1.$(OBJ) $(C_) $(GLSRC)gsdps1.c

# ---------------- DevicePixel color space ---------------- #

gscpixel_h=$(GLSRC)gscpixel.h

cspixlib_=$(GLOBJ)gscpixel.$(OBJ)
cspixlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cspixlib_)
	$(SETMOD) cspixlib $(cspixlib_)

$(GLOBJ)gscpixel.$(OBJ): $(GLSRC)gscpixel.c $(GXERR)\
 $(gsrefct_h) $(gscpixel_h) $(gxcspace_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gscpixel.$(OBJ) $(C_) $(GLSRC)gscpixel.c

# ---------------- CIE color ---------------- #

cielib1_=$(GLOBJ)gscie.$(OBJ) $(GLOBJ)gscscie.$(OBJ) $(GLOBJ)gxctable.$(OBJ)
cielib2_=$(GLOBJ)gscrd.$(OBJ) $(GLOBJ)gscrdp.$(OBJ)
cielib_=$(cielib1_) $(cielib2_)
cielib.dev: $(LIB_MAK) $(ECHOGS_XE) $(cielib_)
	$(SETMOD) cielib $(cielib1_)
	$(ADDMOD) cielib $(cielib2_)

$(GLOBJ)gscie.$(OBJ): $(GLSRC)gscie.c $(GXERR) $(math__h)\
 $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscie.$(OBJ) $(C_) $(GLSRC)gscie.c

$(GLOBJ)gscrd.$(OBJ): $(GLSRC)gscrd.c $(GXERR) $(math__h) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gscolor2_h) $(gscrd_h) $(gsdevice_h)\
 $(gsmatrix_h) $(gsparam_h) $(gxcspace_h)
	$(GLCC) $(GLO_)gscrd.$(OBJ) $(C_) $(GLSRC)gscrd.c

$(GLOBJ)gscrdp.$(OBJ): $(GLSRC)gscrdp.c $(GXERR) $(math__h) $(memory__h)\
 $(gscolor2_h) $(gscrdp_h) $(gsdevice_h) $(gsmatrix_h)\
 $(gxarith_h) $(gxcspace_h)
	$(GLCC) $(GLO_)gscrdp.$(OBJ) $(C_) $(GLSRC)gscrdp.c

$(GLOBJ)gscscie.$(OBJ): $(GLSRC)gscscie.c $(GXERR) $(math__h)\
 $(gscie_h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscscie.$(OBJ) $(C_) $(GLSRC)gscscie.c

$(GLOBJ)gxctable.$(OBJ): $(GLSRC)gxctable.c $(GX)\
 $(gxfixed_h) $(gxfrac_h) $(gxctable_h)
	$(GLCC) $(GLO_)gxctable.$(OBJ) $(C_) $(GLSRC)gxctable.c

# ---------------- Separation colors ---------------- #

seprlib_=$(GLOBJ)gscsepr.$(OBJ)
seprlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(seprlib_)
	$(SETMOD) seprlib $(seprlib_)

$(GLOBJ)gscsepr.$(OBJ): $(GLSRC)gscsepr.c $(GXERR)\
 $(gscsepr_h) $(gsmatrix_h) $(gsrefct_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxfixed_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscsepr.$(OBJ) $(C_) $(GLSRC)gscsepr.c

# ================ PDF support ================ #

# ---------------- Functions ---------------- #
# These are also used by LanguageLevel 3.

gsdsrc_h=$(GLSRC)gsdsrc.h $(gsstruct_h)
gsfunc_h=$(GLSRC)gsfunc.h
gsfunc0_h=$(GLSRC)gsfunc0.h $(gsdsrc_h) $(gsfunc_h)
gxfunc_h=$(GLSRC)gxfunc.h $(gsfunc_h) $(gsstruct_h)

# Generic support, and FunctionType 0.
funclib_=$(GLOBJ)gsdsrc.$(OBJ) $(GLOBJ)gsfunc.$(OBJ) $(GLOBJ)gsfunc0.$(OBJ)
funclib.dev: $(LIB_MAK) $(ECHOGS_XE) $(funclib_)
	$(SETMOD) funclib $(funclib_)

$(GLOBJ)gsdsrc.$(OBJ): $(GLSRC)gsdsrc.c $(GX) $(memory__h)\
 $(gsdsrc_h) $(gserrors_h) $(stream_h)
	$(GLCC) $(GLO_)gsdsrc.$(OBJ) $(C_) $(GLSRC)gsdsrc.c

$(GLOBJ)gsfunc.$(OBJ): $(GLSRC)gsfunc.c $(GX)\
 $(gserrors_h) $(gxfunc_h)
	$(GLCC) $(GLO_)gsfunc.$(OBJ) $(C_) $(GLSRC)gsfunc.c

$(GLOBJ)gsfunc0.$(OBJ): $(GLSRC)gsfunc0.c $(GX) $(math__h)\
 $(gserrors_h) $(gsfunc0_h) $(gxfarith_h) $(gxfunc_h)
	$(GLCC) $(GLO_)gsfunc0.$(OBJ) $(C_) $(GLSRC)gsfunc0.c

# ================ Display Postscript extensions ================ #

gsiparm2_h=$(GLSRC)gsiparm2.h $(gsiparam_h)
gsdps_h=$(GLSRC)gsdps.h $(gsiparm2_h)

# Display PostScript needs the DevicePixel color space to implement
# the PixelCopy option of ImageType 2 images.
dpslib_=$(GLOBJ)gsdps.$(OBJ) $(GLOBJ)gximage2.$(OBJ)
dpslib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dpslib_) cspixlib.dev
	$(SETMOD) dpslib $(dpslib_)
	$(ADDMOD) dpslib -include cspixlib

$(GLOBJ)gsdps.$(OBJ): $(GLSRC)gsdps.c $(GX)\
 $(gsdps_h) $(gserrors_h) $(gspath_h)\
 $(gxdevice_h) $(gzcpath_h) $(gzpath_h) $(gzstate_h)
	$(GLCC) $(GLO_)gsdps.$(OBJ) $(C_) $(GLSRC)gsdps.c

$(GLOBJ)gximage2.$(OBJ): $(GLSRC)gximage2.c $(math__h) $(memory__h) $(GXERR)\
 $(gscpixel_h) $(gscoord_h) $(gscspace_h) $(gsdevice_h) $(gsiparm2_h)\
 $(gsmatrix_h) $(gxgetbit_h) $(gxiparam_h) $(gxpath_h)
	$(GLCC) $(GLO_)gximage2.$(OBJ) $(C_) $(GLSRC)gximage2.c

# ---------------- NeXT Display PostScript ---------------- #

gsalphac_h=$(GLSRC)gsalphac.h $(gscompt_h)
gsdpnext_h=$(GLSRC)gsdpnext.h $(gsalpha_h) $(gsalphac_h)

$(GLOBJ)gsalphac.$(OBJ): $(GLSRC)gsalphac.c $(GXERR) $(memory__h)\
 $(gsalphac_h) $(gsiparam_h) $(gsutil_h)\
 $(gxalpha_h) $(gxcomp_h) $(gxdevice_h) $(gxgetbit_h) $(gxlum_h)
	$(GLCC) $(GLO_)gsalphac.$(OBJ) $(C_) $(GLSRC)gsalphac.c

$(GLOBJ)gximagec.$(OBJ): $(GLSRC)gximagec.c $(GXERR)\
 $(gsdpnext_h) $(gxiparam_h)
	$(GLCC) $(GLO_)gximagec.$(OBJ) $(C_) $(GLSRC)gximagec.c

dpnxtlib_=$(GLOBJ)gsalphac.$(OBJ)
dpnxtlib.dev: $(LIB_MAK) $(ECHOGS_XE) $(dpnxtlib_)
	$(SETMOD) dpnxtlib $(dpnxtlib_)

# ================ PostScript LanguageLevel 3 support ================ #

gsclipsr_h=$(GLSRC)gsclipsr.h
gstrap_h=$(GLSRC)gstrap.h $(gsparam_h)

$(GLOBJ)gscdevn.$(OBJ): $(GLSRC)gscdevn.c $(GXERR)\
 $(gsmatrix_h) $(gsrefct_h) $(gxcspace_h)
	$(GLCC) $(GLO_)gscdevn.$(OBJ) $(C_) $(GLSRC)gscdevn.c

$(GLOBJ)gstrap.$(OBJ): $(GLSRC)gstrap.c $(string__h) $(GXERR)\
 $(gstrap_h)
	$(GLCC) $(GLO_)gstrap.$(OBJ) $(C_) $(GLSRC)gstrap.c

$(GLOBJ)gsclipsr.$(OBJ): $(GLSRC)gsclipsr.c $(GXERR)\
 $(gsclipsr_h)
	$(GLCC) $(GLO_)gsclipsr.$(OBJ) $(C_) $(GLSRC)gsclipsr.c

psl3lib_=$(GLOBJ)gscdevn.$(OBJ) $(GLOBJ)gstrap.$(OBJ) $(GLOBJ)gsclipsr.$(OBJ)
psl3lib.dev: $(LIB_MAK) $(ECHOGS_XE) $(psl3lib_) imasklib.dev shadelib.dev
	$(SETMOD) psl3lib $(psl3lib_)
	$(ADDMOD) psl3lib -include imasklib shadelib

# ---------------- Smooth shading ---------------- #

gscolor3_h=$(GLSRC)gscolor3.h
gsfunc3_h=$(GLSRC)gsfunc3.h $(gsdsrc_h) $(gsfunc_h)
gsptype2_h=$(GLSRC)gsptype2.h $(gspcolor_h)
gsshade_h=$(GLSRC)gsshade.h\
 $(gsccolor_h) $(gscspace_h) $(gsdsrc_h) $(gsfunc_h) $(gsmatrix_h)
gxshade_h=$(GLSRC)gxshade.h $(gsshade_h) $(gxfixed_h) $(gxmatrix_h) $(stream_h)
gxshade4_h=$(GLSRC)gxshade4.h

$(GLOBJ)gscolor3.$(OBJ): $(GLSRC)gscolor3.c $(GXERR)\
 $(gscolor2_h) $(gscolor3_h) $(gscspace_h) $(gsmatrix_h) $(gspath_h)\
 $(gxshade_h) $(gzstate_h)
	$(GLCC) $(GLO_)gscolor3.$(OBJ) $(C_) $(GLSRC)gscolor3.c

$(GLOBJ)gsfunc3.$(OBJ): $(GLSRC)gsfunc3.c $(math__h) $(GXERR)\
 $(gsfunc3_h) $(gxfunc_h)
	$(GLCC) $(GLO_)gsfunc3.$(OBJ) $(C_) $(GLSRC)gsfunc3.c

$(GLOBJ)gsshade.$(OBJ): $(GLSRC)gsshade.c $(GXERR)\
 $(gscspace_h) $(gsstruct_h) $(gxshade_h)
	$(GLCC) $(GLO_)gsshade.$(OBJ) $(C_) $(GLSRC)gsshade.c

$(GLOBJ)gxshade.$(OBJ): $(GLSRC)gxshade.c $(GXERR) $(math__h)\
 $(gscie_h) $(gsrect_h)\
 $(gxcspace_h) $(gxdevcli_h) $(gxdht_h) $(gxistate_h) $(gxpaint_h) $(gxshade_h)
	$(GLCC) $(GLO_)gxshade.$(OBJ) $(C_) $(GLSRC)gxshade.c

$(GLOBJ)gxshade1.$(OBJ): $(GLSRC)gxshade1.c $(GXERR) $(math__h)\
 $(gscoord_h) $(gsmatrix_h) $(gspath_h)\
 $(gxcspace_h) $(gxdcolor_h) $(gxfarith_h) $(gxfixed_h) $(gxistate_h)\
 $(gxpath_h) $(gxshade_h)
	$(GLCC) $(GLO_)gxshade1.$(OBJ) $(C_) $(GLSRC)gxshade1.c

$(GLOBJ)gxshade4.$(OBJ): $(GLSRC)gxshade4.c $(GXERR) $(memory__h)\
 $(gscoord_h) $(gsmatrix_h)\
 $(gxcspace_h) $(gxdcolor_h) $(gxdevcli_h) $(gxistate_h) $(gxpath_h)\
 $(gxshade_h) $(gxshade4_h)
	$(GLCC) $(GLO_)gxshade4.$(OBJ) $(C_) $(GLSRC)gxshade4.c

$(GLOBJ)gxshade6.$(OBJ): $(GLSRC)gxshade6.c $(GXERR) $(memory__h)\
 $(gscoord_h) $(gsmatrix_h)\
 $(gxcspace_h) $(gxdcolor_h) $(gxistate_h) $(gxshade_h) $(gxshade4_h)\
 $(gzpath_h)
	$(GLCC) $(GLO_)gxshade6.$(OBJ) $(C_) $(GLSRC)gxshade6.c

shadelib_1=$(GLOBJ)gscolor3.$(OBJ) $(GLOBJ)gsfunc3.$(OBJ) $(GLOBJ)gsshade.$(OBJ) $(GLOBJ)gxshade.$(OBJ)
shadelib_2=$(GLOBJ)gxshade1.$(OBJ) $(GLOBJ)gxshade4.$(OBJ) $(GLOBJ)gxshade6.$(OBJ)
shadelib_=$(shadelib_1) $(shadelib_2)
shadelib.dev: $(LIB_MAK) $(ECHOGS_XE) $(shadelib_) funclib.dev patlib.dev
	$(SETMOD) shadelib $(shadelib_1)
	$(ADDMOD) shadelib -obj $(shadelib_2)
	$(ADDMOD) shadelib -include funclib patlib

# ================ Platform-specific modules ================ #
# Platform-specific code doesn't really belong here: this is code that is
# shared among multiple platforms.

# Standard implementation of gp_getenv.
$(GLOBJ)gp_getnv.$(OBJ): $(GLSRC)gp_getnv.c $(AK) $(stdio__h) $(string__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h)
	$(GLCC) $(GLO_)gp_getnv.$(OBJ) $(C_) $(GLSRC)gp_getnv.c

# Frame buffer implementations.

$(GLOBJ)gp_nofb.$(OBJ): $(GLSRC)gp_nofb.c $(GX)\
 $(gp_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gp_nofb.$(OBJ) $(C_) $(GLSRC)gp_nofb.c

$(GLOBJ)gp_dosfb.$(OBJ): $(GLSRC)gp_dosfb.c $(AK) $(malloc__h) $(memory__h)\
 $(gx_h) $(gp_h) $(gserrors_h) $(gxdevice_h)
	$(GLCC) $(GLO_)gp_dosfb.$(OBJ) $(C_) $(GLSRC)gp_dosfb.c

# File system implementation.

# MS-DOS file system, also used by Desqview/X.
$(GLOBJ)gp_dosfs.$(OBJ): $(GLSRC)gp_dosfs.c $(AK) $(dos__h) $(gp_h) $(gx_h)
	$(GLCC) $(GLO_)gp_dosfs.$(OBJ) $(C_) $(GLSRC)gp_dosfs.c

# MS-DOS file enumeration, *not* used by Desqview/X.
$(GLOBJ)gp_dosfe.$(OBJ): $(GLSRC)gp_dosfe.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h)\
 $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(gp_h) $(gsutil_h)
	$(GLCC) $(GLO_)gp_dosfe.$(OBJ) $(C_) $(GLSRC)gp_dosfe.c

# Unix(-like) file system, also used by Desqview/X.
$(GLOBJ)gp_unifs.$(OBJ): $(GLSRC)gp_unifs.c $(AK)\
 $(memory__h) $(string__h) $(gx_h) $(gp_h)\
 $(gsstruct_h) $(gsutil_h) $(stat__h) $(dirent__h)
	$(GLCC) $(GLO_)gp_unifs.$(OBJ) $(C_) $(GLSRC)gp_unifs.c

# Unix(-like) file name syntax, *not* used by Desqview/X.
$(GLOBJ)gp_unifn.$(OBJ): $(GLSRC)gp_unifn.c $(AK) $(gx_h) $(gp_h)
	$(GLCC) $(GLO_)gp_unifn.$(OBJ) $(C_) $(GLSRC)gp_unifn.c

# Thread / semaphore / monitor implementation.

# Dummy implementation.
nosync_=$(GLOBJ)gp_nsync.$(OBJ)
nosync.dev: $(LIB_MAK) $(ECHOGS_XE) $(nosync_)
	$(SETMOD) nosync $(nosync_)

$(GLOBJ)gp_nsync.$(OBJ): $(GLSRC)gp_nsync.c $(AK) $(std_h)\
 $(gpsync_h) $(gserror_h) $(gserrors_h)
	$(GLCC) $(GLO_)gp_nsync.$(OBJ) $(C_) $(GLSRC)gp_nsync.c

# POSIX semaphores.
$(GLOBJ)gp_posem.$(OBJ): $(GLSRC)gp_posem.c $(AK) $(std_h)\
 $(gpsync_h) $(gserror_h) $(gserrors_h)
	$(GLCC) $(GLO_)gp_posem.$(OBJ) $(C_) $(GLSRC)gp_posem.c

# Monitors built out of semaphores.
$(GLOBJ)gp_semon.$(OBJ): $(GLSRC)gp_semon.c $(AK) $(std_h)\
 $(gpsync_h) $(gserror_h) $(gserrors_h)
	$(GLCC) $(GLO_)gp_semon.$(OBJ) $(C_) $(GLSRC)gp_semon.c

# pthreads threads.
pthreads_=$(GLOBJ)gp_pthr.$(OBJ)
pthreads.dev: $(LIB_MAK) $(ECHOGS_XE) $(pthreads_)
	$(SETMOD) pthreads $(pthreads_) -lib pthread

$(GLOBJ)gp_pthr.$(OBJ): $(GLSRC)gp_pthr.c $(AK) $(std_h)\
 $(gpsync_h) $(gserror_h) $(gserrors_h)
	$(GLCC) $(GLO_)gp_pthr.$(OBJ) $(C_) $(GLSRC)gp_pthr.c

# Replace the dummy implementation with POSIX semaphores + pthreads.
posync_=$(GLOBJ)gp_posem.$(OBJ) $(GLOBJ)gp_semon.$(OBJ)
posync.dev: $(LIB_MAK) $(ECHOGS_XE) $(posync_) pthreads.dev
	$(SETMOD) posync $(posync_) -include pthreads -replace nosync

# Other stuff.

# Other MS-DOS facilities.
$(GLOBJ)gp_msdos.$(OBJ): $(GLSRC)gp_msdos.c $(AK)\
 $(dos__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gp_h)
	$(GLCC) $(GLO_)gp_msdos.$(OBJ) $(C_) $(GLSRC)gp_msdos.c

# ----------------------------- Main program ------------------------------ #

# Main program for library testing

$(GLOBJ)gslib.$(OBJ): $(GLSRC)gslib.c $(AK) $(math__h) $(stdio__h)\
 $(gx_h) $(gp_h)\
 $(gsalloc_h) $(gserrors_h) $(gsmatrix_h) $(gsrop_h) $(gsstate_h) $(gscspace_h)\
 $(gscdefs_h) $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gscrd_h)\
 $(gshtx_h) $(gsiparm3_h) $(gsiparm4_h) $(gslib_h) $(gsparam_h)\
 $(gspaint_h) $(gspath_h) $(gspath2_h) $(gsstruct_h) $(gsutil_h)\
 $(gxalloc_h) $(gxdcolor_h) $(gxdevice_h) $(gxht_h)\
 $(gdevbbox_h) $(gdevcmap_h)
	$(GLCC) $(GLO_)gslib.$(OBJ) $(C_) $(GLSRC)gslib.c
