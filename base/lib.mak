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
# (Platform-independent) makefile for Ghostscript graphics library
# and other support code.
# Users of this makefile must define the following:
#       GLSRCDIR - the source directory
#       GLGENDIR - the directory for source files generated during building
#       GLOBJDIR - the object code directory
#       DEVSRCDIR - source directory for the device drivers
#       AUXDIR - the directory to build the "aux" (host executable) files
#       JSRCDIR - JPEG library source directory
#       JGENDIR - JPEG library object code directory
#       ZSRCDIR - zlib source directory
#       ZGENDIR - zlib object code directory
#       LCMSGENDIR - Little CMS object code directory
# One of:
#       LCMS2MTSRCDIR - Artifex (Thread Safe) Little CMS 2 source directory
#       LCMS2SRCDIR - Little CMS verion 2 source directory
#
# For dependencies:
#       MAKEDIRS

GLSRC=$(GLSRCDIR)$(D)
GLGEN=$(GLGENDIR)$(D)
GLOBJ=$(GLOBJDIR)$(D)
AUX=$(AUXDIR)$(D)
GLO_=$(O_)$(GLOBJ)
AUXO_=$(O_)$(AUX)
GLI_=$(GLGENDIR) $(II)$(GLSRCDIR) $(II)$(DEVSRCDIR)
GLF_=
GLINCLUDES=$(I_)$(GLI_)$(_I)
GLCCFLAGS=$(GLINCLUDES) $(GLF_) $(D_)WHICH_CMS="$(WHICH_CMS)"$(_D)
GLCC=$(CC_) $(GLCCFLAGS)
GLCCAUX=$(CCAUX_) $(GLCCFLAGS)
GLJCC=$(CC_) $(I_)$(GLI_) $(II)$(JI_)$(_I) $(JCF_) $(GLF_)
GLZCC=$(CC_) $(I_)$(GLI_) $(II)$(ZI_)$(_I) $(ZCF_) $(GLF_)
GLJBIG2CC=$(CC_) $(I_)$(GLI_) $(II)$(JB2I_)$(_I) $(JB2CF_) $(GLF_)
GLJASCC=$(CC_) $(I_)$(JPXI_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_)
GLLDFJB2CC=$(CC_) $(I_)$(LDF_JB2I_) $(II)$(GLI_)$(_I) $(JB2CF_) $(GLF_)
GLLWFJPXCC=$(CC_) $(I_)$(LWF_JPXI_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_)
GLJPXOPJCC=$(CC_) $(I_)$(JPX_OPENJPEG_I_)$(D).. $(I_)$(JPX_OPENJPEG_I_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_)
GLCCSHARED=$(CC_SHARED) $(GLCCFLAGS)
# We can't use $(CC_) for GLLCMS2MTCC because that includes /Za on
# msvc builds, and lcms configures itself to depend on msvc extensions
# (inline asm, including windows.h) when compiled under msvc.
GLLCMS2MTCC=$(CC) $(LCMS2MT_CFLAGS) $(CFLAGS) $(I_)$(GLI_) $(II)$(LCMS2MTSRCDIR)$(D)include$(_I) $(GLF_)
lcms2mt_h=$(LCMS2MTSRCDIR)$(D)include$(D)lcms2mt.h
lcms2mt_plugin_h=$(LCMS2MTSRCDIR)$(D)include$(D)lcms2mt_plugin.h
icc34_h=$(GLSRC)icc34.h
# We can't use $(CC_) for GLLCMS2CC because that includes /Za on
# msvc builds, and lcms configures itself to depend on msvc extensions
# (inline asm, including windows.h) when compiled under msvc.
GLLCMS2CC=$(CC) $(LCMS2_CFLAGS) $(CFLAGS) $(I_)$(GLI_) $(II)$(LCMS2SRCDIR)$(D)include$(_I) $(GLF_)
lcms2_h=$(LCMS2SRCDIR)$(D)include$(D)lcms2.h
lcms2_plugin_h=$(LCMS2SRCDIR)$(D)include$(D)lcms2_plugin.h

gdevdcrd_h=$(GLSRC)gdevdcrd.h $(gxdevcli_h)
gdevpccm_h=$(GLSRC)gdevpccm.h $(gxdevcli_h)

gs_mro_e_h=$(GLSRC)gs_mro_e.h
gs_mgl_e_h=$(GLSRC)gs_mgl_e.h

# All top-level makefiles define GLD.
#GLD=$(GLGEN)
# Define the name of this makefile.
LIB_MAK=$(GLSRC)lib.mak $(TOP_MAKEFILES)

# Define the inter-dependencies of the .h files.
# Since not all versions of `make' defer expansion of macros,
# we must list these in bottom-to-top order.

# Generic files

arch_h=$(GLGEN)arch.h
stdpre_h=$(GLSRC)stdpre.h
stdint__h=$(GLSRC)stdint_.h $(std_h)

$(GLGEN)arch.h : $(GENARCH_XE)
	$(EXP)$(GENARCH_XE) $(GLGEN)arch.h $(TARGET_ARCH_FILE)

# Platform interfaces

# gp.h requires gstypes.h and srdline.h.
gstypes_h=$(GLSRC)gstypes.h $(stdpre_h)
srdline_h=$(GLSRC)srdline.h $(gstypes_h) $(std_h)
gpgetenv_h=$(GLSRC)gpgetenv.h
gpmisc_h=$(GLSRC)gpmisc.h $(gp_h)
gp_h=$(GLSRC)gp.h $(srdline_h) $(gpgetenv_h) $(gscdefs_h) $(stdint__h) $(gstypes_h)
gpcheck_h=$(GLSRC)gpcheck.h $(std_h)
gpsync_h=$(GLSRC)gpsync.h $(stdint__h)

# Configuration definitions

gconf_h=$(GLSRC)gconf.h $(gconfig_h)
# gconfig*.h are generated dynamically.
gconfig__h=$(GLGEN)gconfig_.h
gscdefs_h=$(GLSRC)gscdefs.h

std_h=$(GLSRC)std.h $(arch_h) $(stdpre_h)

# C library interfaces

# Because of variations in the "standard" header files between systems, and
# because we must include std.h before any file that includes sys/types.h,
# we define local include files named *_.h to substitute for <*.h>.

vmsmath_h=$(GLSRC)vmsmath.h

# declare here for use by string__h
gssprintf_h=$(GLSRC)gssprintf.h
gsstrtok_h=$(GLSRC)gsstrtok.h
gsstrl_h=$(GLSRC)gsstrl.h $(stdpre_h)

dos__h=$(GLSRC)dos_.h
ctype__h=$(GLSRC)ctype_.h $(std_h)
dirent__h=$(GLSRC)dirent_.h $(gconfig__h) $(std_h)
errno__h=$(GLSRC)errno_.h $(std_h)
fcntl__h=$(GLSRC)fcntl_.h $(std_h)
locale__h=$(GLSRC)locale_.h $(std_h) $(MAKEFILE)
memento_h=$(GLSRC)memento.h
bobbin_h=$(GLSRC)bobbin.h
malloc__h=$(GLSRC)malloc_.h $(bobbin_h) $(memento_h) $(std_h)
math__h=$(GLSRC)math_.h $(vmsmath_h) $(std_h)
memory__h=$(GLSRC)memory_.h $(std_h)
setjmp__h=$(GLSRC)setjmp_.h
stat__h=$(GLSRC)stat_.h $(std_h)
stdio__h=$(GLSRC)stdio_.h $(std_h) $(gssprintf_h)
string__h=$(GLSRC)string_.h $(gsstrtok_h) $(gsstrl_h) $(std_h)
time__h=$(GLSRC)time_.h $(std_h) $(gconfig__h)
unistd__h=$(GLSRC)unistd_.h $(std_h)
windows__h=$(GLSRC)windows_.h
assert__h=$(GLSRC)assert_.h
# Out of order
pipe__h=$(GLSRC)pipe_.h $(stdio__h)

# Third-party library interfaces

jerror_h=$(JSRCDIR)$(D)jerror.h
jerror__h=$(GLSRC)jerror_.h $(jerror_h) $(MAKEFILE)
jpeglib__h=$(GLGEN)jpeglib_.h

# Miscellaneous

gsio_h=$(GLSRC)gsio.h
gxstdio_h=$(GLSRC)gxstdio.h $(gsio_h)
gs_dll_call_h=$(GLSRC)gs_dll_call.h
gslibctx_h=$(GLSRC)gslibctx.h $(gs_dll_call_h) $(stdio__h) $(std_h)
gdbflags_h=$(GLSRC)gdbflags.h
gdebug_h=$(GLSRC)gdebug.h $(gdbflags_h) $(std_h)
gsalloc_h=$(GLSRC)gsalloc.h $(std_h)
gsargs_h=$(GLSRC)gsargs.h $(std_h)
gserrors_h=$(GLSRC)gserrors.h
gsexit_h=$(GLSRC)gsexit.h $(std_h)
gsgc_h=$(GLSRC)gsgc.h $(stdpre_h)
gsgstate_h=$(GLSRC)gsgstate.h
gsmalloc_h=$(GLSRC)gsmalloc.h $(gxsync_h)
gsmchunk_h=$(GLSRC)gsmchunk.h $(std_h)
valgrind_h=$(GLSRC)valgrind.h $(stdpre_h)
gsmdebug_h=$(GLSRC)gsmdebug.h $(valgrind_h)
gsmemraw_h=$(GLSRC)gsmemraw.h
gsmemory_h=$(GLSRC)gsmemory.h $(gslibctx_h) $(gstypes_h) $(std_h)
gsmemret_h=$(GLSRC)gsmemret.h $(gsmemory_h)
gsnogc_h=$(GLSRC)gsnogc.h $(gsgc_h)
gsrefct_h=$(GLSRC)gsrefct.h $(memento_h) $(std_h)
gsserial_h=$(GLSRC)gsserial.h $(stdpre_h)
gsstype_h=$(GLSRC)gsstype.h $(gsmemory_h)
gx_h=$(GLSRC)gx.h $(gdebug_h) $(gserrors_h) $(gsio_h) $(gsmemory_h) $(gsgstate_h) $(stdio__h) $(gstypes_h)
gxsync_h=$(GLSRC)gxsync.h $(gpsync_h) $(gsmemory_h)
gxclthrd_h=$(GLSRC)gxclthrd.h $(gxdevcli_h) $(gscms_h)
gxdevsop_h=$(GLSRC)gxdevsop.h $(gxdevcli_h)
gdevflp_h=$(GLSRC)gdevflp.h $(gxdevice_h)
gdevkrnlsclass_h=$(GLSRC)gdevkrnlsclass.h $(gdevflp_h) $(gdevoflt_h)
gdevsclass_h=$(GLSRC)gdevsclass.h $(gxdevice_h)

# Out of order
gsnotify_h=$(GLSRC)gsnotify.h $(gsstype_h)
gsstruct_h=$(GLSRC)gsstruct.h $(gsstype_h)

###### Support

### Include files

gsbitmap_h=$(GLSRC)gsbitmap.h $(gsstype_h)
gsbitops_h=$(GLSRC)gsbitops.h $(gxcindex_h) $(gstypes_h)
gsbittab_h=$(GLSRC)gsbittab.h $(gstypes_h)
gsflip_h=$(GLSRC)gsflip.h $(stdpre_h)
gsuid_h=$(GLSRC)gsuid.h $(std_h)
gsutil_h=$(GLSRC)gsutil.h $(gstypes_h) $(std_h)
gxarith_h=$(GLSRC)gxarith.h
gxbitmap_h=$(GLSRC)gxbitmap.h $(gsbitmap_h) $(gstypes_h)
gxfarith_h=$(GLSRC)gxfarith.h $(gxarith_h) $(stdpre_h)
gxfixed_h=$(GLSRC)gxfixed.h $(std_h)
gxobj_h=$(GLSRC)gxobj.h $(gsstruct_h) $(gxbitmap_h)
gxrplane_h=$(GLSRC)gxrplane.h
# Out of order
gsrect_h=$(GLSRC)gsrect.h $(gxfixed_h) $(gstypes_h)
gxalloc_h=$(GLSRC)gxalloc.h $(gxobj_h) $(gsalloc_h)
gxbitops_h=$(GLSRC)gxbitops.h $(gsbitops_h)
gxcindex_h=$(GLSRC)gxcindex.h $(stdint__h)
gxfont42_h=$(GLSRC)gxfont42.h $(gxfont_h)
gstrans_h=$(GLSRC)gstrans.h $(gxblend_h) $(gstparam_h) $(gsmatrix_h) $(gxcomp_h)

# Streams
scommon_h=$(GLSRC)scommon.h $(gsstype_h) $(gsmemory_h) $(stdint__h) $(gstypes_h)
stream_h=$(GLSRC)stream.h $(scommon_h) $(gxiodev_h) $(srdline_h)
ramfs_h=$(GLSRC)ramfs.h $(stream_h)

### Memory manager

$(GLOBJ)gsalloc.$(OBJ) : $(GLSRC)gsalloc.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsexit_h) $(gsmdebug_h) $(gsstruct_h) $(gxalloc_h)\
 $(stream_h) $(malloc__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsalloc.$(OBJ) $(C_) $(GLSRC)gsalloc.c

$(GLOBJ)gsmalloc.$(OBJ) : $(GLSRC)gsmalloc.c $(malloc__h)\
 $(gdebug_h)\
 $(gserrors_h)\
 $(gsmalloc_h) $(gsmdebug_h) $(gsmemret_h)\
 $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmalloc.$(OBJ) $(C_) $(GLSRC)gsmalloc.c

# Memento uses windows.h on windows. This requires that /Za not be
# used (as this disables Microsoft extensions, which breaks windows.h).
# GLCC has the /Za pickled into it on windows, so we can't use GLCC.
# Therefore use our own compiler invocation.
MEMENTO_CC=$(CC) $(GENOPT) $(GLINCLUDES) $(CFLAGS)

# We have an extra dependency here on malloc__h. This is deliberate to allow
# windows users to set #define MEMENTO in the top of malloc_h and have
# rebuilds work.
$(GLOBJ)memento.$(OBJ) : $(GLSRC)memento.c $(valgrind_h) $(memento_h)\
 $(malloc__h) $(LIB_MAK) $(MAKEDIRS)
	$(MEMENTO_CC) $(GLO_)memento.$(OBJ) $(C_) $(GLSRC)memento.c

$(AUX)memento.$(OBJ) : $(GLSRC)memento.c $(valgrind_h) $(memento_h)\
 $(malloc__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)memento.$(OBJ) $(C_) $(GLSRC)memento.c

# Bobbin uses windows.h on windows. This requires that /Za not be
# used (as this disables Microsoft extensions, which breaks windows.h).
# GLCC has the /Za pickled into it on windows, so we can't use GLCC.
# Therefore use our own compiler invocation.
BOBBIN_CC=$(CC) $(GENOPT) $(GLINCLUDES) $(CFLAGS)

$(GLOBJ)bobbin.$(OBJ) : $(GLSRC)bobbin.c $(bobbin_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(BOBBIN_CC) $(GLO_)bobbin.$(OBJ) $(C_) $(GLSRC)bobbin.c

$(AUX)bobbin.$(OBJ) : $(GLSRC)bobbin.c $(valgrind_h) $(bobbin_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)bobbin.$(OBJ) $(C_) $(GLSRC)bobbin.c

$(GLOBJ)gsmemory.$(OBJ) : $(GLSRC)gsmemory.c $(memory__h)\
 $(gdebug_h)\
 $(gsmdebug_h) $(gsmemory_h) $(gsrefct_h) $(gsstruct_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmemory.$(OBJ) $(C_) $(GLSRC)gsmemory.c

$(GLOBJ)gsmemret.$(OBJ) : $(GLSRC)gsmemret.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsmemret_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmemret.$(OBJ) $(C_) $(GLSRC)gsmemret.c

# gsnogc is not part of the base configuration.
# We make it available as a .dev so it can be used in configurations that
# don't include the garbage collector, as well as by the "async" logic.
gsnogc_=$(GLOBJ)gsnogc.$(OBJ)
$(GLD)gsnogc.dev : $(ECHOGS_XE) $(gsnogc_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gsnogc $(gsnogc_)

$(GLOBJ)gsnogc.$(OBJ) : $(GLSRC)gsnogc.c $(AK) $(gx_h)\
 $(gsmdebug_h) $(gsnogc_h) $(gsstruct_h) $(gxalloc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsnogc.$(OBJ) $(C_) $(GLSRC)gsnogc.c

### Bitmap processing

$(GLOBJ)gsbitcom.$(OBJ) : $(GLSRC)gsbitcom.c $(AK) $(std_h)\
 $(gdebug_h) $(gsbitops_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsbitcom.$(OBJ) $(C_) $(GLSRC)gsbitcom.c

$(GLOBJ)gsbitops.$(OBJ) : $(GLSRC)gsbitops.c $(AK) $(memory__h)\
 $(stdio__h) $(gdebug_h) $(gsbittab_h) $(gserrors_h) $(gstypes_h)\
 $(gxbitops_h) $(gxcindex_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsbitops.$(OBJ) $(C_) $(GLSRC)gsbitops.c

$(GLOBJ)gsbittab.$(OBJ) : $(GLSRC)gsbittab.c $(AK) $(stdpre_h)\
 $(gsbittab_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsbittab.$(OBJ) $(C_) $(GLSRC)gsbittab.c

# gsflip is not part of the standard configuration: it's rather large,
# and no standard facility requires it.
$(GLOBJ)gsflip.$(OBJ) : $(GLSRC)gsflip.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsbitops_h) $(gsbittab_h) $(gsflip_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsflip.$(OBJ) $(C_) $(GLSRC)gsflip.c

### Multi-threading

# These are required in the standard configuration, because gsmalloc.c
# needs them even if the underlying primitives are dummies.

$(GLOBJ)gxsync.$(OBJ) : $(GLSRC)gxsync.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsmemory_h) $(gxsync_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxsync.$(OBJ) $(C_) $(GLSRC)gxsync.c

### Miscellaneous

# Support for platform code
$(GLOBJ)gpmisc.$(OBJ) : $(GLSRC)gpmisc.c\
 $(unistd__h) $(fcntl__h) $(stat__h) $(stdio__h)\
 $(memory__h) $(string__h) $(gp_h) $(gpgetenv_h) $(gpmisc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gpmisc.$(OBJ) $(C_) $(GLSRC)gpmisc.c

$(AUX)gpmisc.$(OBJ) : $(GLSRC)gpmisc.c\
 $(unistd__h) $(fcntl__h) $(stat__h) $(stdio__h)\
 $(memory__h) $(string__h) $(gp_h) $(gpgetenv_h) $(gpmisc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gpmisc.$(OBJ) $(C_) $(GLSRC)gpmisc.c

# Command line argument list management
$(GLOBJ)gsargs.$(OBJ) : $(GLSRC)gsargs.c\
 $(ctype__h) $(stdio__h) $(string__h)\
 $(gsargs_h) $(gsexit_h) $(gsmemory_h)\
 $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsargs.$(OBJ) $(C_) $(GLSRC)gsargs.c

$(GLOBJ)gsmisc.$(OBJ) : $(GLSRC)gsmisc.c $(AK) $(gx_h) $(gserrors_h)\
 $(vmsmath_h) $(std_h)\
 $(ctype__h) $(malloc__h) $(math__h) $(memory__h) $(string__h)\
 $(gpcheck_h) $(gxfarith_h) $(gxfixed_h) $(stdint__h) $(stdio__h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmisc.$(OBJ) $(C_) $(GLSRC)gsmisc.c

$(AUX)gsmisc.$(OBJ) : $(GLSRC)gsmisc.c $(AK) $(gx_h) $(gserrors_h)\
 $(vmsmath_h) $(std_h)\
 $(ctype__h) $(malloc__h) $(math__h) $(memory__h) $(string__h)\
 $(gpcheck_h) $(gxfarith_h) $(gxfixed_h) $(stdint__h) $(stdio__h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(C_) $(AUXO_)gsmisc.$(OBJ) $(GLSRC)gsmisc.c

$(GLOBJ)gslibctx.$(OBJ) : $(GLSRC)gslibctx.c  $(AK) $(gp_h) $(gsmemory_h)\
  $(gslibctx_h) $(stdio__h) $(string__h) $(gsicc_manage_h) $(gserrors_h) \
  $(gscdefs_h)
	$(GLCC) $(GLO_)gslibctx.$(OBJ) $(C_) $(GLSRC)gslibctx.c

$(AUX)gslibctx.$(OBJ) : $(GLSRC)gslibctx.c  $(AK) $(gp_h) $(gsmemory_h)\
  $(gslibctx_h) $(stdio__h) $(string__h) $(gsicc_manage_h)
	$(GLCCAUX) $(C_) $(AUXO_)gslibctx.$(OBJ) $(GLSRC)gslibctx.c

$(GLOBJ)gsnotify.$(OBJ) : $(GLSRC)gsnotify.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsnotify_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsnotify.$(OBJ) $(C_) $(GLSRC)gsnotify.c

$(GLOBJ)gsserial.$(OBJ) : $(GLSRC)gsserial.c $(stdpre_h) $(gstypes_h)\
 $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsserial.$(OBJ) $(C_) $(GLSRC)gsserial.c

$(GLOBJ)gsutil.$(OBJ) : $(GLSRC)gsutil.c $(AK) $(memory__h)\
 $(string__h) $(gstypes_h) $(gserrors_h) $(gsmemory_h)\
 $(gsrect_h) $(gsuid_h) $(gsutil_h) $(gzstate_h) $(gxdcolor_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsutil.$(OBJ) $(C_) $(GLSRC)gsutil.c

$(AUX)gsutil.$(OBJ) : $(GLSRC)gsutil.c $(AK) $(memory__h) $(string__h)\
 $(gstypes_h) $(gserrors_h) $(gsmemory_h)\
 $(gsrect_h) $(gsuid_h) $(gsutil_h) $(gzstate_h) $(gxdcolor_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(C_) $(AUXO_)gsutil.$(OBJ) $(GLSRC)gsutil.c

$(GLOBJ)gssprintf.$(OBJ) : $(GLSRC)gssprintf.c $(gssprintf_h) $(stdio__h) \
 $(stdint__h) $(string__h) $(math__h)
	$(GLCC) $(GLO_)gssprintf.$(OBJ) $(C_) $(GLSRC)gssprintf.c

$(GLOBJ)gsstrtok.$(OBJ) : $(GLSRC)gsstrtok.c $(gsstrtok_h) $(string__h)
	$(GLCC) $(GLO_)gsstrtok.$(OBJ) $(C_) $(GLSRC)gsstrtok.c

$(GLOBJ)gsstrl.$(OBJ) : $(GLSRC)gsstrl.c $(gsstrl_h) $(string__h)
	$(GLCC) $(GLO_)gsstrl.$(OBJ) $(C_) $(GLSRC)gsstrl.c

# MD5 digest
gsmd5_h=$(GLSRC)gsmd5.h
# We have to use a slightly different compilation approach in order to
# get std.h included when compiling md5.c.
md5_=$(GLOBJ)gsmd5.$(OBJ)
$(GLOBJ)gsmd5.$(OBJ) : $(GLSRC)gsmd5.c $(AK) $(gsmd5_h) $(std_h) $(EXP)$(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(ECHOGS_XE) -w $(GLGEN)gsmd5.h -x 23 include -x 2022 memory_.h -x 22
	$(EXP)$(ECHOGS_XE) -a $(GLGEN)gsmd5.h -+R $(GLSRC)gsmd5.h
	$(CP_) $(GLSRC)gsmd5.c $(GLGEN)gsmd5.c
	$(GLCC) $(GLO_)gsmd5.$(OBJ) $(C_) $(GLGEN)gsmd5.c
	$(RM_) $(GLGEN)gsmd5.c $(GLGEN)gsmd5.h

# SHA-256 digest
sha2_h=$(GLSRC)sha2.h $(stdint__h) $(std_h)
sha2_=$(GLOBJ)sha2.$(OBJ)
$(GLOBJ)sha2.$(OBJ) : $(GLSRC)sha2.c $(AK) $(std_h) $(string__h)\
 $(sha2_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sha2.$(OBJ) $(C_) $(GLSRC)sha2.c

# AES cipher
aes_h=$(GLSRC)aes.h
aes_=$(GLOBJ)aes.$(OBJ)
$(GLOBJ)aes.$(OBJ) : $(GLSRC)aes.c $(AK) $(string__h) $(aes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)aes.$(OBJ) $(C_) $(GLSRC)aes.c

###### Low-level facilities and utilities

### Include files

gsalpha_h=$(GLSRC)gsalpha.h $(gsgstate_h)
gsccode_h=$(GLSRC)gsccode.h $(gstypes_h) $(std_h)
gsccolor_h=$(GLSRC)gsccolor.h $(gsstype_h)
# gscedata.[ch] are generated automatically by lib/encs2c.ps.
gscedata_h=$(GLSRC)gscedata.h $(stdpre_h)
gscencs_h=$(GLSRC)gscencs.h $(gsccode_h) $(gstypes_h) $(stdpre_h)
gsclipsr_h=$(GLSRC)gsclipsr.h $(gsgstate_h)
gscsel_h=$(GLSRC)gscsel.h
gscolor1_h=$(GLSRC)gscolor1.h $(gxtmap_h) $(gsgstate_h) $(stdpre_h)
gscompt_h=$(GLSRC)gscompt.h $(gstypes_h)
gscoord_h=$(GLSRC)gscoord.h $(gsmatrix_h) $(gsgstate_h)
gscpm_h=$(GLSRC)gscpm.h
gscsepnm_h=$(GLSRC)gscsepnm.h
gsdevice_h=$(GLSRC)gsdevice.h $(gsgstate_h) $(std_h)
gsfcmap_h=$(GLSRC)gsfcmap.h $(gsccode_h)
gsfname_h=$(GLSRC)gsfname.h $(std_h)
gsfont_h=$(GLSRC)gsfont.h $(gsgstate_h) $(gstypes_h) $(std_h)
gsgdata_h=$(GLSRC)gsgdata.h $(gsstype_h)
gsgcache_h=$(GLSRC)gsgcache.h $(stdpre_h)
gshsb_h=$(GLSRC)gshsb.h $(gsgstate_h)
gsht_h=$(GLSRC)gsht.h $(gsgstate_h) $(gstypes_h) $(std_h)
gsht1_h=$(GLSRC)gsht1.h $(gsht_h)
gsjconf_h=$(GLSRC)gsjconf.h $(arch_h) $(stdpre_h)
gslib_h=$(GLSRC)gslib.h $(std_h)
gslparam_h=$(GLSRC)gslparam.h
gsmatrix_h=$(GLSRC)gsmatrix.h $(gstypes_h)
# Out of order
gxbitfmt_h=$(GLSRC)gxbitfmt.h $(stdpre_h)
gxcomp_h=$(GLSRC)gxcomp.h $(gsrefct_h) $(gxbitfmt_h) $(gscompt_h) $(gsgstate_h)
gsovrc_h=$(GLSRC)gsovrc.h $(gxcomp_h) $(gsstype_h) $(gxcindex_h)
gspaint_h=$(GLSRC)gspaint.h $(gsgstate_h) $(stdpre_h)
gsparam_h=$(GLSRC)gsparam.h $(gsstype_h)
gsparams_h=$(GLSRC)gsparams.h $(stream_h) $(gsparam_h)
gsparamx_h=$(GLSRC)gsparamx.h $(gsparam_h) $(gstypes_h) $(stdpre_h)
gspath2_h=$(GLSRC)gspath2.h $(gsmatrix_h) $(gsgstate_h)
gspcolor_h=$(GLSRC)gspcolor.h $(gsrefct_h) $(gsuid_h) $(gsccolor_h) $(gsgstate_h)
gspenum_h=$(GLSRC)gspenum.h
gsptype1_h=$(GLSRC)gsptype1.h $(gspcolor_h) $(gxbitmap_h)
gsropt_h=$(GLSRC)gsropt.h $(stdpre_h)
gstext_h=$(GLSRC)gstext.h $(gscpm_h) $(gsgstate_h) $(gsccode_h)
gsxfont_h=$(GLSRC)gsxfont.h $(stdpre_h)
# Out of order
gschar_h=$(GLSRC)gschar.h $(gsstate_h) $(gscpm_h) $(gsccode_h)
gsiparam_h=$(GLSRC)gsiparam.h $(gsmatrix_h) $(gxbitmap_h) $(gsccolor_h) $(gsstype_h)
gsimage_h=$(GLSRC)gsimage.h $(gsiparam_h) $(gsgstate_h)
gsline_h=$(GLSRC)gsline.h $(gslparam_h) $(gsgstate_h) $(stdpre_h)
gspath_h=$(GLSRC)gspath.h $(gspenum_h) $(gsgstate_h) $(gstypes_h) $(std_h)
gsrop_h=$(GLSRC)gsrop.h $(gsropt_h) $(gsgstate_h)
gstparam_h=$(GLSRC)gstparam.h $(gscspace_h) $(gsrefct_h) $(gsccolor_h) $(stdint__h)

gxalpha_h=$(GLSRC)gxalpha.h
gxbcache_h=$(GLSRC)gxbcache.h $(gxbitmap_h)
gxcvalue_h=$(GLSRC)gxcvalue.h
gxclio_h=$(GLSRC)gxclio.h $(gp_h)
gxclip_h=$(GLSRC)gxclip.h $(gxdevcli_h)
gxclipsr_h=$(GLSRC)gxclipsr.h $(gsrefct_h)
gxcoord_h=$(GLSRC)gxcoord.h $(gxmatrix_h) $(gscoord_h)
gxcpath_h=$(GLSRC)gxcpath.h $(gxdevcli_h)
gxdda_h=$(GLSRC)gxdda.h $(gxfixed_h) $(stdpre_h)
gxdevbuf_h=$(GLSRC)gxdevbuf.h $(gxband_h) $(gxrplane_h)
gxdevrop_h=$(GLSRC)gxdevrop.h $(gxdevcli_h)
gxdevmem_h=$(GLSRC)gxdevmem.h $(gxdevcli_h) $(gxrplane_h)
gxdhtres_h=$(GLSRC)gxdhtres.h $(stdpre_h)
gxfont0_h=$(GLSRC)gxfont0.h $(gxfont_h)
gxfrac_h=$(GLSRC)gxfrac.h
gxftype_h=$(GLSRC)gxftype.h
gxgetbit_h=$(GLSRC)gxgetbit.h $(gxdevcli_h) $(gxbitfmt_h)
gxhttile_h=$(GLSRC)gxhttile.h $(gxbitmap_h)
gxhttype_h=$(GLSRC)gxhttype.h
gxiclass_h=$(GLSRC)gxiclass.h $(stdpre_h)
gxiodev_h=$(GLSRC)gxiodev.h $(stat__h) $(gsstype_h)
gxline_h=$(GLSRC)gxline.h $(math__h) $(gslparam_h) $(gsmatrix_h)
gxlum_h=$(GLSRC)gxlum.h
gxmatrix_h=$(GLSRC)gxmatrix.h $(gxfixed_h) $(gsmatrix_h)
gxmclip_h=$(GLSRC)gxmclip.h $(gxclip_h) $(gxdevmem_h)
gxoprect_h=$(GLSRC)gxoprect.h $(gxdevcli_h)
gxp1impl_h=$(GLSRC)gxp1impl.h $(gxpcolor_h) $(gxdcolor_h) $(gxdevcli_h)
gxpaint_h=$(GLSRC)gxpaint.h $(gxpath_h) $(gxfixed_h)
gxpath_h=$(GLSRC)gxpath.h $(gspenum_h) $(gsrect_h) $(gslparam_h) $(gscpm_h) $(gsgstate_h)
gxpcache_h=$(GLSRC)gxpcache.h $(std_h)
gxsample_h=$(GLSRC)gxsample.h $(std_h)
gxsamplp_h=$(GLSRC)gxsamplp.h
gxscanc_h=$(GLSRC)gxscanc.h $(gxdevice_h) $(gxpath_h)
gxstate_h=$(GLSRC)gxstate.h $(gscspace_h) $(gsgstate_h)
gxtext_h=$(GLSRC)gxtext.h $(gxfixed_h) $(gstext_h) $(gsrefct_h)
gxtmap_h=$(GLSRC)gxtmap.h
gxxfont_h=$(GLSRC)gxxfont.h $(gxdevcli_h) $(gsxfont_h) $(gsuid_h) $(gsccode_h)
# The following are out of order because they include other files.
gxband_h=$(GLSRC)gxband.h $(gxdevcli_h) $(gxclio_h)
gxcdevn_h=$(GLSRC)gxcdevn.h $(gxfrac_h) $(gscspace_h) $(gsccolor_h) $(gxcindex_h)
gxchar_h=$(GLSRC)gxchar.h $(gschar_h) $(gxtext_h)
gxchrout_h=$(GLSRC)gxchrout.h $(gsgstate_h)
gsdcolor_h=$(GLSRC)gsdcolor.h $(gscms_h) $(gxhttile_h) $(gxbitmap_h) $(gsccolor_h) $(gxarith_h) $(gxcindex_h)
gxdcolor_h=$(GLSRC)gxdcolor.h $(gsropt_h) $(gscsel_h) $(gsdcolor_h) $(gsstruct_h) $(stdint__h) $(gsgstate_h)
gsnamecl_h=$(GLSRC)gsnamecl.h $(gxcspace_h) $(gxfrac_h) $(gscsel_h) $(gsccolor_h) $(gsgstate_h)
gsncdummy_h=$(GLSRC)gsncdummy.h
gscspace_h=$(GLSRC)gscspace.h $(gsiparam_h) $(gsrefct_h) $(gsmemory_h) $(gsgstate_h)
# FIXME: gscspace_h should depend on $(gscms_h) too.
gscssub_h=$(GLSRC)gscssub.h $(gscspace_h)
gxdevcli_h=$(GLSRC)gxdevcli.h $(gxtext_h) $(gsnamecl_h) $(gxcmap_h) $(gxcvalue_h) $(gxdda_h) $(gxfixed_h) $(gxrplane_h) $(gsropt_h) $(gstparam_h) $(gsdcolor_h) $(gscms_h) $(gsstruct_h) $(gsiparam_h) $(gsmatrix_h) $(gsrefct_h) $(gsxfont_h) $(gxbitmap_h) $(gp_h) $(gscompt_h) $(gxcindex_h) $(stdint__h) $(gsgstate_h) $(std_h)
gscicach_h=$(GLSRC)gscicach.h $(gxdevcli_h)
gxdevice_h=$(GLSRC)gxdevice.h $(gsmalloc_h) $(gxdevcli_h) $(gsparam_h) $(gsfname_h) $(gxstdio_h) $(stdio__h)
gxdht_h=$(GLSRC)gxdht.h $(gxhttype_h) $(gxfrac_h) $(gxtmap_h) $(gscspace_h) $(gsmatrix_h) $(gsrefct_h) $(gxarith_h) $(gxcindex_h)
gxdhtserial_h=$(GLSRC)gxdhtserial.h $(gsgstate_h) $(stdpre_h)
gxdither_h=$(GLSRC)gxdither.h $(gxfrac_h) $(gsdcolor_h)
gxclip2_h=$(GLSRC)gxclip2.h $(gxmclip_h)
gxclipm_h=$(GLSRC)gxclipm.h $(gxmclip_h)
gxctable_h=$(GLSRC)gxctable.h $(gxfixed_h) $(gxfrac_h) $(gstypes_h)
gxfcache_h=$(GLSRC)gxfcache.h $(gxbcache_h) $(gxfixed_h) $(gxftype_h) $(gsxfont_h) $(gsuid_h) $(gsgstate_h) $(gsccode_h)

gxfont_h=$(GLSRC)gxfont.h $(gsnotify_h) $(gsgdata_h) $(gsfont_h) $(gxftype_h) $(gsmatrix_h) $(gsuid_h) $(gsstype_h) $(gsccode_h)
gxiparam_h=$(GLSRC)gxiparam.h $(gxdevcli_h) $(gsstype_h)
gximask_h=$(GLSRC)gximask.h $(gsropt_h) $(gxbitmap_h)
gscie_h=$(GLSRC)gscie.h $(gxctable_h) $(gscspace_h) $(gsstype_h) $(gstypes_h) $(std_h)
gsicc_h=$(GLSRC)gsicc.h $(gscie_h) $(gxcspace_h)
gscrd_h=$(GLSRC)gscrd.h $(gscie_h)
gscrdp_h=$(GLSRC)gscrdp.h $(gscie_h) $(gsparam_h)
gscdevn_h=$(GLSRC)gscdevn.h $(gscspace_h)
gxdevndi_h=$(GLSRC)gxdevndi.h $(gxfrac_h)
gscindex_h=$(GLSRC)gscindex.h $(gxfrac_h) $(gscspace_h)
gscolor2_h=$(GLSRC)gscolor2.h $(gscindex_h) $(gsptype1_h)
gscsepr_h=$(GLSRC)gscsepr.h $(gscspace_h)
gxdcconv_h=$(GLSRC)gxdcconv.h $(gxfrac_h) $(gsgstate_h) $(std_h)
gxfmap_h=$(GLSRC)gxfmap.h $(gxfrac_h) $(gxtmap_h) $(gsrefct_h) $(gsstype_h)
gxcmap_h=$(GLSRC)gxcmap.h $(gxfmap_h) $(gxcvalue_h) $(gscsel_h) $(gsdcolor_h) $(gscspace_h) $(gxcindex_h) $(gsgstate_h)

gxgstate_h=$(GLSRC)gxgstate.h $(gxmatrix_h) $(gstrans_h) $(gxstate_h) $(gxdcolor_h) $(gxline_h) $(gsnamecl_h) $(gxcmap_h) $(gxcvalue_h) $(gxfixed_h) $(gsropt_h) $(gxtmap_h) $(gscsel_h) $(gstparam_h) $(gscms_h) $(gscspace_h) $(gsrefct_h) $(gscpm_h) $(gsgstate_h)

gxcolor2_h=$(GLSRC)gxcolor2.h $(gscolor2_h) $(gsmatrix_h) $(gsrefct_h) $(gxbitmap_h)
gxclist_h=$(GLSRC)gxclist.h $(gxcolor2_h) $(gxgstate_h) $(gxdevbuf_h) $(gxbcache_h) $(gxband_h) $(gxrplane_h) $(gscms_h) $(gscspace_h) $(gxclio_h)
gxcspace_h=$(GLSRC)gxcspace.h $(gxfrac_h) $(gscsel_h) $(gscspace_h) $(gsccolor_h) $(gxcindex_h)
gxht_h=$(GLSRC)gxht.h $(gxhttype_h) $(gsht1_h) $(gxtmap_h) $(gscspace_h) $(gsrefct_h)
gxcie_h=$(GLSRC)gxcie.h $(gscie_h) $(gsnamecl_h)
gxht_thresh_h=$(GLSRC)gxht_thresh.h $(gxiclass_h) $(gxdda_h) $(gsiparam_h)
gxpcolor_h=$(GLSRC)gxpcolor.h $(gxcpath_h) $(gxdevmem_h) $(gxblend_h) $(gxdevice_h) $(gxdcolor_h) $(gxiclass_h) $(gxpcache_h) $(gxcspace_h) $(gspcolor_h)
gscolor_h=$(GLSRC)gscolor.h $(gxtmap_h) $(gsgstate_h) $(stdpre_h)
gsstate_h=$(GLSRC)gsstate.h $(gsline_h) $(gscolor_h) $(gscsel_h) $(gsht_h) $(gsdevice_h) $(gscpm_h) $(gsgstate_h) $(std_h)
gsicc_create_h=$(GLSRC)gsicc_create.h $(gscie_h)
gximdecode_h=$(GLSRC)gximdecode.h $(gximage_h) $(gxsample_h) $(gx_h) $(gxfixed_h) $(gxfrac_h)

gzacpath_h=$(GLSRC)gzacpath.h $(gxcpath_h)
gzcpath_h=$(GLSRC)gzcpath.h $(gzpath_h) $(gxcpath_h)
gzht_h=$(GLSRC)gzht.h $(gxdht_h) $(gxht_h) $(gxdevcli_h) $(gxfmap_h) $(gscsel_h) $(gxhttile_h)
gzline_h=$(GLSRC)gzline.h $(gxline_h) $(gsgstate_h)
gzpath_h=$(GLSRC)gzpath.h $(gxpath_h) $(gsmatrix_h) $(gsrefct_h) $(gsstype_h)
gzstate_h=$(GLSRC)gzstate.h $(gxgstate_h) $(gsstate_h)

gdevbbox_h=$(GLSRC)gdevbbox.h $(gxdevcli_h)
gdevmem_h=$(GLSRC)gdevmem.h $(gxdevcli_h) $(gxbitops_h)
gdevmpla_h=$(GLSRC)gdevmpla.h $(gxrplane_h) $(gsdevice_h)
gdevmrop_h=$(GLSRC)gdevmrop.h $(gxdevcli_h) $(gsropt_h)
gdevmrun_h=$(GLSRC)gdevmrun.h $(gxdevmem_h)
gdevplnx_h=$(GLSRC)gdevplnx.h $(gxdevcli_h) $(gxrplane_h)
gdevepo_h=$(GLSRC)gdevepo.h $(gxdevice_h)

sa85d_h=$(GLSRC)sa85d.h $(scommon_h)
sa85x_h=$(GLSRC)sa85x.h $(sa85d_h)
sbcp_h=$(GLSRC)sbcp.h $(scommon_h)
sbtx_h=$(GLSRC)sbtx.h $(scommon_h)
scanchar_h=$(GLSRC)scanchar.h $(scommon_h)
sfilter_h=$(GLSRC)sfilter.h $(scommon_h) $(gstypes_h)
sdct_h=$(GLSRC)sdct.h $(strimpl_h) $(setjmp__h) $(gscms_h)
shc_h=$(GLSRC)shc.h $(scommon_h) $(gsbittab_h)
sisparam_h=$(GLSRC)sisparam.h $(gxdda_h) $(gxfixed_h) $(scommon_h)
sjpeg_h=$(GLSRC)sjpeg.h $(sdct_h)
slzwx_h=$(GLSRC)slzwx.h $(scommon_h)
smd5_h=$(GLSRC)smd5.h $(gsmd5_h) $(scommon_h)
sarc4_h=$(GLSRC)sarc4.h $(scommon_h)
saes_h=$(GLSRC)saes.h $(aes_h) $(scommon_h)
sjbig2_h=$(GLSRC)sjbig2.h $(scommon_h) $(stdint__h)
sjbig2_luratech_h=$(GLSRC)sjbig2_luratech.h $(scommon_h)
sjpx_h=$(GLSRC)sjpx.h $(scommon_h)
sjpx_luratech_h=$(GLSRC)sjpx_luratech.h $(scommon_h)
sjpx_openjpeg_h=$(GLSRC)sjpx_openjpeg.h $(scommon_h) $(openjpeg_h)
spdiffx_h=$(GLSRC)spdiffx.h $(scommon_h)
spngpx_h=$(GLSRC)spngpx.h $(scommon_h)
spprint_h=$(GLSRC)spprint.h $(stdpre_h)
spsdf_h=$(GLSRC)spsdf.h $(gsparam_h)
srlx_h=$(GLSRC)srlx.h $(scommon_h)
spwgx_h=$(GLSRC)spwgx.h $(scommon_h)
sstring_h=$(GLSRC)sstring.h $(scommon_h)
strimpl_h=$(GLSRC)strimpl.h $(scommon_h) $(gsstruct_h) $(gstypes_h)
szlibx_h=$(GLSRC)szlibx.h $(scommon_h)
zlib_h=$(ZSRCDIR)$(D)zlib.h
# We have two of the following, for shared zlib (_1)
# and 'local' zlib (_0)
szlibxx_h_1=$(GLSRC)szlibxx.h $(szlibx_h)
szlibxx_h_0=$(GLSRC)szlibxx.h $(szlibx_h) $(zlib_h)
# Out of order
scf_h=$(GLSRC)scf.h $(shc_h)
scfx_h=$(GLSRC)scfx.h $(shc_h)
siinterp_h=$(GLSRC)siinterp.h $(sisparam_h)
siscale_h=$(GLSRC)siscale.h $(sisparam_h)
sidscale_h=$(GLSRC)sidscale.h $(sisparam_h)
simscale_h=$(GLSRC)simscale.h $(sisparam_h)
gximage_h=$(GLSRC)gximage.h $(gxiclass_h) $(strimpl_h) $(gxsample_h) $(gxiparam_h) $(gxcspace_h) $(sisparam_h) $(gxdda_h) $(gscms_h) $(gsiparam_h)
gxhldevc_h=$(GLSRC)gxhldevc.h $(gsdcolor_h) $(gsgstate_h)
gsptype2_h=$(GLSRC)gsptype2.h $(gxfixed_h) $(gspcolor_h) $(gsdcolor_h)
gdevddrw_h=$(GLSRC)gdevddrw.h $(gxdevcli_h)
gxfill_h=$(GLSRC)gxfill.h $(gzpath_h) $(gxdevcli_h)
gxfilltr_h=$(GLSRC)gxfilltr.h
gxfillsl_h=$(GLSRC)gxfillsl.h
gxfillts_h=$(GLSRC)gxfillts.h
gxdtfill_h=$(GLSRC)gxdtfill.h

ttfoutl_h=$(GLSRC)ttfoutl.h $(malloc__h)
gxttfb_h=$(GLSRC)gxttfb.h $(ttfoutl_h) $(gxfont_h) $(gslibctx_h)
gzspotan_h=$(GLSRC)gzspotan.h $(gxdevcli_h)

gdevpxen_h=$(GLSRC)gdevpxen.h
gdevpxat_h=$(GLSRC)gdevpxat.h
gdevpxop_h=$(GLSRC)gdevpxop.h

gsequivc_h=$(GLSRC)gsequivc.h $(gxdevcli_h) $(gxfrac_h) $(gxcindex_h) $(stdpre_h)
gdevdevn_h=$(GLSRC)gdevdevn.h $(gsequivc_h) $(gxblend_h)
gdevdevnprn_h=$(GLSRC)gdevdevnprn.h $(gdevprn_h) $(gdevdevn_h)

gdevoflt_h=$(GLSRC)gdevoflt.h $(gxdevice_h)

png__h=$(GLSRC)png_.h $(MAKEFILE)
x__h=$(GLSRC)x_.h

### Executable code

# gconfig and gscdefs are handled specially.  Currently they go in psbase
# rather than in libcore, which is clearly wrong.
$(GLOBJ)gconfig.$(OBJ) : $(gconfig_h) $(GLSRC)gconf.c $(AK) $(gx_h)\
 $(gscdefs_h) $(gconf_h)\
 $(gxdevice_h) $(gxiclass_h) $(gxiodev_h) $(gxiparam_h) $(LIB_MAK) $(MAKEDIRS)
	$(RM_) $(GLGEN)gconfig.c
	$(CP_) $(GLSRC)gconf.c $(GLGEN)gconfig.c
	$(GLCC) $(GLO_)gconfig.$(OBJ) $(C_) $(GLGEN)gconfig.c

$(GLOBJ)gscdefs.$(OBJ) : $(GLSRC)gscdef.c\
 $(std_h) $(gscdefs_h) $(gconfigd_h) $(LIB_MAK) $(MAKEDIRS)
	$(RM_) $(GLGEN)gscdefs.c
	$(CP_) $(GLSRC)gscdef.c $(GLGEN)gscdefs.c
	$(GLCC) $(GLO_)gscdefs.$(OBJ) $(C_) $(GLGEN)gscdefs.c

$(AUX)gscdefs.$(OBJ) : $(GLSRC)gscdef.c\
 $(std_h) $(gscdefs_h) $(gconfigd_h) $(LIB_MAK) $(MAKEDIRS)
	$(RM_) $(AUX)gscdefs.c
	$(CP_) $(GLSRC)gscdef.c $(AUX)gscdefs.c
	$(GLCCAUX) $(C_) $(AUXO_)gscdefs.$(OBJ) $(AUX)gscdefs.c

$(GLOBJ)gxacpath.$(OBJ) : $(GLSRC)gxacpath.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsdcolor_h) $(gsrop_h) $(gsstate_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxgstate_h) $(gxpaint_h)\
 $(gzacpath_h) $(gzcpath_h) $(gzpath_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxacpath.$(OBJ) $(C_) $(GLSRC)gxacpath.c

$(GLOBJ)gxbcache.$(OBJ) : $(GLSRC)gxbcache.c $(AK) $(gx_h) $(memory__h)\
 $(gsmdebug_h) $(gxbcache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxbcache.$(OBJ) $(C_) $(GLSRC)gxbcache.c

$(GLOBJ)gxccache.$(OBJ) : $(GLSRC)gxccache.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gsstruct_h)\
 $(gscencs_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzstate_h) $(gzpath_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gzcpath_h) $(gxchar_h) $(gxfont_h) $(gxfcache_h)\
 $(gxxfont_h) $(gximask_h) $(gscspace_h) $(gsimage_h) $(gxhttile_h)\
 $(gsptype1_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxccache.$(OBJ) $(C_) $(GLSRC)gxccache.c

$(GLOBJ)gxccman.$(OBJ) : $(GLSRC)gxccman.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h)\
 $(gsbitops_h) $(gsstruct_h) $(gsutil_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfont_h) $(gxfcache_h) $(gxchar_h)\
 $(gxpath_h) $(gxxfont_h) $(gzstate_h) $(gxttfb_h) $(gxfont42_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxccman.$(OBJ) $(C_) $(GLSRC)gxccman.c

$(GLOBJ)gxchar.$(OBJ) : $(GLSRC)gxchar.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gspath_h) $(gsstruct_h) $(gxfcid_h)\
 $(gxfixed_h) $(gxarith_h) $(gxmatrix_h) $(gxcoord_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfont_h) $(gxfont0_h) $(gxchar_h) $(gxfcache_h) $(gzpath_h) $(gzstate_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxchar.$(OBJ) $(C_) $(GLSRC)gxchar.c

$(GLOBJ)gxchrout.$(OBJ) : $(GLSRC)gxchrout.c $(AK) $(gx_h) $(math__h)\
 $(gxchrout_h) $(gxfarith_h) $(gxgstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxchrout.$(OBJ) $(C_) $(GLSRC)gxchrout.c

$(GLOBJ)gxcht.$(OBJ) : $(GLSRC)gxcht.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsutil_h) $(gxdevsop_h)\
 $(gxarith_h) $(gxcmap_h) $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h)\
 $(gxgstate_h) $(gxmatrix_h) $(gzht_h) $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxcht.$(OBJ) $(C_) $(GLSRC)gxcht.c

$(GLOBJ)gxclip.$(OBJ) : $(GLSRC)gxclip.c $(AK) $(gx_h)\
 $(gxclip_h) $(gxcpath_h) $(gxdevice_h) $(gxpath_h) $(gzcpath_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclip.$(OBJ) $(C_) $(GLSRC)gxclip.c

$(GLOBJ)gxcmap.$(OBJ) : $(GLSRC)gxcmap.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsccolor_h)\
 $(gxalpha_h) $(gxcspace_h) $(gxfarith_h) $(gxfrac_h)\
 $(gxdcconv_h) $(gxdevice_h) $(gxcmap_h) $(gxlum_h)\
 $(gzstate_h) $(gxdither_h) $(gxcdevn_h) $(string__h)\
 $(gsicc_manage_h) $(gdevdevn_h) $(gsicc_cache_h)\
 $(gscms_h) $(gsicc_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxcmap.$(OBJ) $(C_) $(GLSRC)gxcmap.c

$(GLOBJ)gxcpath.$(OBJ) : $(GLSRC)gxcpath.c $(AK) $(gx_h) $(gserrors_h)\
 $(gscoord_h) $(gsline_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxgstate_h) $(gxpaint_h)\
 $(gzpath_h) $(gzcpath_h) $(gzacpath_h) $(string__h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxcpath.$(OBJ) $(C_) $(GLSRC)gxcpath.c

$(GLOBJ)gxdcconv.$(OBJ) : $(GLSRC)gxdcconv.c $(AK) $(gx_h)\
 $(gsdcolor_h) $(gxcmap_h) $(gxdcconv_h) $(gxdevice_h)\
 $(gxfarith_h) $(gxgstate_h) $(gxlum_h) $(gsstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdcconv.$(OBJ) $(C_) $(GLSRC)gxdcconv.c

$(GLOBJ)gxdcolor.$(OBJ) : $(GLSRC)gxdcolor.c $(AK) $(gx_h)\
 $(memory__h) $(gsbittab_h) $(gserrors_h) $(gxdcolor_h) $(gxpcolor_h)\
 $(gxdevice_h) $(gxdevcli_h) $(gxclist_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdcolor.$(OBJ) $(C_) $(GLSRC)gxdcolor.c

$(GLOBJ)gxhldevc.$(OBJ) : $(GLSRC)gxhldevc.c $(AK) $(gx_h)\
 $(gzstate_h) $(gscspace_h) $(gxcspace_h) $(gxhldevc_h) $(memory__h)\
 $(gxpcolor_h) $(gsptype2_h) $(gsptype1_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxhldevc.$(OBJ) $(C_) $(GLSRC)gxhldevc.c

$(GLOBJ)gxfill.$(OBJ) : $(GLSRC)gxfill.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsstruct_h) $(gxdevsop_h) $(assert__h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxhttile_h)\
 $(gxgstate_h) $(gxpaint_h) $(gxfill_h) $(gxpath_h)\
 $(gsptype1_h) $(gsptype2_h) $(gxpcolor_h) $(gsstate_h)\
 $(gzcpath_h) $(gzpath_h) $(gzspotan_h) $(gdevddrw_h) $(memory__h)\
 $(stdint__h) $(gxfilltr_h) $(gxfillsl_h) $(gxfillts_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxfill.$(OBJ) $(C_) $(GLSRC)gxfill.c

$(GLOBJ)gxht.$(OBJ) : $(GLSRC)gxht.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsbitops_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxgstate_h) $(gzht_h)\
 $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxht.$(OBJ) $(C_) $(GLSRC)gxht.c

$(GLOBJ)gxhtbit.$(OBJ) : $(GLSRC)gxhtbit.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsbitops_h) $(gscdefs_h)\
 $(gxbitmap_h) $(gxdht_h) $(gxdhtres_h) $(gxhttile_h) $(gxtmap_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxhtbit.$(OBJ) $(C_) $(GLSRC)gxhtbit.c

$(GLOBJ)gxht_thresh.$(OBJ) : $(GLSRC)gxht_thresh.c $(AK) $(memory__h)\
 $(gx_h) $(gxgstate_h) $(gsiparam_h) $(math__h) $(gxfixed_h) $(gximage_h)\
 $(gxdevice_h) $(gxdht_h) $(gxht_thresh_h) $(gzht_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxht_thresh.$(OBJ) $(C_) $(GLSRC)gxht_thresh.c

$(GLOBJ)gxidata.$(OBJ) : $(GLSRC)gxidata.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gxcpath_h) $(gxdevice_h) $(gximage_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxidata.$(OBJ) $(C_) $(GLSRC)gxidata.c

$(GLOBJ)gxifast.$(OBJ) : $(GLSRC)gxifast.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h) $(gdevmem_h) $(gsbittab_h) $(gsccolor_h)\
 $(gspaint_h) $(gsutil_h) $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxgstate_h) \
 $(gxmatrix_h) $(gzht_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxifast.$(OBJ) $(C_) $(GLSRC)gxifast.c

$(GLOBJ)gximage.$(OBJ) : $(GLSRC)gximage.c $(AK) $(gx_h) $(gserrors_h)\
 $(gscspace_h) $(gsmatrix_h) $(gsutil_h)\
 $(gxcolor2_h) $(gxiparam_h) $(stream_h) $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximage.$(OBJ) $(C_) $(GLSRC)gximage.c

$(GLOBJ)gximage1.$(OBJ) : $(GLSRC)gximage1.c $(AK) $(gx_h)\
 $(gserrors_h) $(gximage_h) $(gxiparam_h) $(stream_h) $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximage1.$(OBJ) $(C_) $(GLSRC)gximage1.c

$(GLOBJ)gximono.$(OBJ) : $(GLSRC)gximono.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h) $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxgstate_h) $(gxmatrix_h)\
 $(gzht_h) $(gsicc_h) $(gsicc_cache_h)  $(gsicc_cms_h)\
 $(gxcie_h) $(gscie_h) $(gxht_thresh_h) $(gxdda_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximono.$(OBJ) $(C_) $(GLSRC)gximono.c

$(GLOBJ)gximask.$(OBJ) : $(GLSRC)gximask.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsptype1_h) $(gsptype2_h) $(gxdevice_h) $(gxdcolor_h)\
 $(gxcpath_h) $(gximask_h) $(gzacpath_h) $(gzcpath_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximask.$(OBJ) $(C_) $(GLSRC)gximask.c

$(GLOBJ)gxipixel.$(OBJ) : $(GLSRC)gxipixel.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gpcheck_h) $(gscindex_h) $(gscspace_h)\
 $(gsccolor_h) $(gscdefs_h) $(gspaint_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxiparam_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gzpath_h) $(gzstate_h) $(gsicc_cache_h) $(gsicc_cms_h)\
 $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxipixel.$(OBJ) $(C_) $(GLSRC)gxipixel.c

$(GLOBJ)gxi12bit.$(OBJ) : $(GLSRC)gxi12bit.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxgstate_h)\
 $(gxmatrix_h) $(gsicc_h) $(gsicc_cache_h) $(gsicc_cms_h)\
 $(gxcie_h) $(gscie_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxi12bit.$(OBJ) $(C_) $(GLSRC)gxi12bit.c

$(GLOBJ)gxi16bit.$(OBJ) : $(GLSRC)gxi16bit.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gsccolor_h) $(gspaint_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxgstate_h)\
 $(gxmatrix_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxi16bit.$(OBJ) $(C_) $(GLSRC)gxi16bit.c

# gxmclip is used for Patterns and ImageType 3 images:
# it isn't included in the base library.
$(GLOBJ)gxmclip.$(OBJ) : $(GLSRC)gxmclip.c $(AK) $(gx_h) $(gserrors_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxmclip_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxmclip.$(OBJ) $(C_) $(GLSRC)gxmclip.c

$(GLOBJ)gxpaint.$(OBJ) : $(GLSRC)gxpaint.c $(AK) $(gx_h)\
 $(gxdevice_h) $(gxhttile_h) $(gxpaint_h) $(gxpath_h) $(gzstate_h) $(gxfont_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpaint.$(OBJ) $(C_) $(GLSRC)gxpaint.c

$(GLOBJ)gxpath.$(OBJ) : $(GLSRC)gxpath.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsstruct_h) $(gxfixed_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpath.$(OBJ) $(C_) $(GLSRC)gxpath.c

$(GLOBJ)gxpath2.$(OBJ) : $(GLSRC)gxpath2.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gspath_h) $(gsstruct_h) $(gxfixed_h) $(gxarith_h) $(gzpath_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpath2.$(OBJ) $(C_) $(GLSRC)gxpath2.c

$(GLOBJ)gxpcopy.$(OBJ) : $(GLSRC)gxpcopy.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gxfarith_h) $(gxfixed_h) $(gxgstate_h) $(gzpath_h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpcopy.$(OBJ) $(C_) $(GLSRC)gxpcopy.c

$(GLOBJ)gxpdash.$(OBJ) : $(GLSRC)gxpdash.c $(AK) $(gx_h) $(math__h)\
 $(gscoord_h) $(gsline_h) $(gsmatrix_h) $(gxarith_h) $(gxgstate_h)\
 $(gxfixed_h) $(gzline_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpdash.$(OBJ) $(C_) $(GLSRC)gxpdash.c

$(GLOBJ)gxpflat.$(OBJ) : $(GLSRC)gxpflat.c $(AK) $(gx_h)\
 $(gserrors_h) $(gxarith_h) $(gxfixed_h) $(gzpath_h) $(memory__h) $(string__h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpflat.$(OBJ) $(C_) $(GLSRC)gxpflat.c

$(GLOBJ)gxsample.$(OBJ) : $(GLSRC)gxsample.c $(AK) $(gx_h)\
 $(gxsample_h) $(gxfixed_h) $(gximage_h) $(gxsamplp_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxsample.$(OBJ) $(C_) $(GLSRC)gxsample.c

$(GLOBJ)gxscanc.$(OBJ) : $(GLSRC)gxscanc.c $(GX) $(gxscanc_h) $(gx_h)
	$(GLCC) $(GLO_)gxscanc.$(OBJ) $(C_) $(GLSRC)gxscanc.c

$(GLOBJ)gxstroke.$(OBJ) : $(GLSRC)gxstroke.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(gpcheck_h) $(gsstate_h)\
 $(gscoord_h) $(gsdcolor_h) $(gsdevice_h) $(gsptype1_h)\
 $(gxdevice_h) $(gxfarith_h) $(gxfixed_h)\
 $(gxhttile_h) $(gxgstate_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzline_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxstroke.$(OBJ) $(C_) $(GLSRC)gxstroke.c

###### Higher-level facilities

$(GLOBJ)gsalpha.$(OBJ) : $(GLSRC)gsalpha.c $(AK) $(gx_h)\
 $(gsalpha_h) $(gxdcolor_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsalpha.$(OBJ) $(C_) $(GLSRC)gsalpha.c

# gscedata.[ch] are generated automatically by lib/encs2c.ps.
$(GLOBJ)gscedata.$(OBJ) : $(GLSRC)gscedata.c\
 $(stdpre_h) $(gstypes_h) $(gscedata_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscedata.$(OBJ) $(C_) $(GLSRC)gscedata.c

$(GLOBJ)gscencs.$(OBJ) : $(GLSRC)gscencs.c\
 $(memory__h) $(gscedata_h) $(gscencs_h) $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscencs.$(OBJ) $(C_) $(GLSRC)gscencs.c

$(GLOBJ)gschar.$(OBJ) : $(GLSRC)gschar.c $(AK) $(gx_h) $(gserrors_h)\
 $(gscoord_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxchar_h) $(gxfont_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gschar.$(OBJ) $(C_) $(GLSRC)gschar.c

$(GLOBJ)gscolor.$(OBJ) : $(GLSRC)gscolor.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsccolor_h) $(gsstruct_h) $(gsutil_h) $(gscolor2_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscolor.$(OBJ) $(C_) $(GLSRC)gscolor.c

$(GLOBJ)gscoord.$(OBJ) : $(GLSRC)gscoord.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gsccode_h) $(gxcoord_h) $(gxdevice_h) $(gxfarith_h) $(gxfixed_h)\
 $(gxfont_h) $(gxmatrix_h) $(gxpath_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscoord.$(OBJ) $(C_) $(GLSRC)gscoord.c

$(GLOBJ)gscparam.$(OBJ) : $(GLSRC)gscparam.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(string__h) $(gsparam_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscparam.$(OBJ) $(C_) $(GLSRC)gscparam.c

$(GLOBJ)gscspace.$(OBJ) : $(GLSRC)gscspace.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsstruct_h) $(gsccolor_h) $(gsutil_h)\
 $(gxcmap_h) $(gxcspace_h) $(gxgstate_h) $(gsovrc_h) $(gsstate_h)\
 $(gsdevice_h) $(gxdevcli_h) $(gzstate_h) $(gsnamecl_h) $(stream_h)\
 $(gsicc_h) $(gsicc_manage_h) $(string__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscspace.$(OBJ) $(C_) $(GLSRC)gscspace.c

$(GLOBJ)gscicach.$(OBJ) : $(GLSRC)gscicach.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsccolor_h) $(gxcspace_h) $(gxdcolor_h) $(gscicach_h)\
 $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscicach.$(OBJ) $(C_) $(GLSRC)gscicach.c

$(GLOBJ)gsovrc.$(OBJ) : $(GLSRC)gsovrc.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsutil_h) $(gxcomp_h) $(gxdevice_h) $(gsdevice_h) $(gxgetbit_h)\
 $(gsovrc_h) $(gxdcolor_h) $(gxoprect_h) $(gsbitops_h) $(gxgstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsovrc.$(OBJ) $(C_) $(GLSRC)gsovrc.c

$(GLOBJ)gxoprect.$(OBJ) : $(GLSRC)gxoprect.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsutil_h) $(gxdevice_h) $(gsdevice_h)\
 $(gxgetbit_h) $(gxoprect_h) $(gsbitops_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxoprect.$(OBJ) $(C_) $(GLSRC)gxoprect.c

$(GLOBJ)gsdevice.$(OBJ) : $(GLSRC)gsdevice.c $(AK) $(gx_h)\
 $(gserrors_h) $(ctype__h) $(memory__h) $(string__h) $(gp_h)\
 $(gscdefs_h) $(gsfname_h) $(gsstruct_h) $(gspath_h)\
 $(gspaint_h) $(gsmatrix_h) $(gscoord_h) $(gzstate_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdevmem_h) $(gxiodev_h) $(gxcspace_h)\
 $(gsicc_manage_h) $(gscms_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsdevice.$(OBJ) $(C_) $(GLSRC)gsdevice.c

$(GLOBJ)gsdevmem.$(OBJ) : $(GLSRC)gsdevmem.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gsdevice_h) $(gxarith_h)\
 $(gxdevice_h) $(gxdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsdevmem.$(OBJ) $(C_) $(GLSRC)gsdevmem.c

$(GLOBJ)gsdparam.$(OBJ) : $(GLSRC)gsdparam.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(string__h)\
 $(gsdevice_h) $(gsparam_h) $(gxdevice_h) $(gxfixed_h)\
 $(gsicc_manage_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsdparam.$(OBJ) $(C_) $(GLSRC)gsdparam.c

$(GLOBJ)gsfname.$(OBJ) : $(GLSRC)gsfname.c $(AK) $(memory__h)\
 $(gserrors_h) $(gsfname_h) $(gsmemory_h) $(gstypes_h)\
 $(gxiodev_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfname.$(OBJ) $(C_) $(GLSRC)gsfname.c

$(GLOBJ)gsfont.$(OBJ) : $(GLSRC)gsfont.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsstruct_h) $(gsutil_h)\
 $(gxdevice_h) $(gxfixed_h) $(gxmatrix_h) $(gxfont_h) $(gxfcache_h)\
 $(gzpath_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfont.$(OBJ) $(C_) $(GLSRC)gsfont.c

$(GLOBJ)gsgdata.$(OBJ) : $(GLSRC)gsgdata.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsgdata_h) $(gsmatrix_h) $(gsstruct_h) $(gxfont_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsgdata.$(OBJ) $(C_) $(GLSRC)gsgdata.c

$(GLOBJ)gsgcache.$(OBJ) : $(GLSRC)gsgcache.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsstruct_h) $(gsgdata_h) $(gsgcache_h)\
 $(gxfont_h) $(gxfont42_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsgcache.$(OBJ) $(C_) $(GLSRC)gsgcache.c

$(GLOBJ)gsht.$(OBJ) : $(GLSRC)gsht.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsstruct_h) $(gsutil_h) $(gxarith_h)\
 $(gxdevice_h) $(gzht_h) $(gzstate_h) $(gxfmap_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsht.$(OBJ) $(C_) $(GLSRC)gsht.c

$(GLOBJ)gshtscr.$(OBJ) : $(GLSRC)gshtscr.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gzht_h) $(gzstate_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gshtscr.$(OBJ) $(C_) $(GLSRC)gshtscr.c

$(GLOBJ)gsimage.$(OBJ) : $(GLSRC)gsimage.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(math__h) $(gscspace_h) $(gsimage_h) $(gsmatrix_h) $(gximage_h)\
 $(gsstruct_h) $(gxarith_h) $(gxdevice_h) $(gxiparam_h) $(gxpath_h)\
 $(gximask_h) $(gzstate_h) $(gxdevsop_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsimage.$(OBJ) $(C_) $(GLSRC)gsimage.c

$(GLOBJ)gsimpath.$(OBJ) : $(GLSRC)gsimpath.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsmatrix_h) $(gspaint_h) $(gspath_h) $(gsstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsimpath.$(OBJ) $(C_) $(GLSRC)gsimpath.c

$(GLOBJ)gsinit.$(OBJ) : $(GLSRC)gsinit.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h) $(gscdefs_h) $(gslib_h) $(gsmalloc_h) $(gsmemory_h)\
 $(gxfapi_h) $(valgrind_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsinit.$(OBJ) $(C_) $(GLSRC)gsinit.c

$(GLOBJ)gsiodev.$(OBJ) : $(GLSRC)gsiodev.c $(AK) $(gx_h) $(gserrors_h)\
 $(errno__h) $(string__h) $(unistd__h) $(gsfname_h)\
 $(gp_h) $(gscdefs_h) $(gsparam_h) $(gsstruct_h) $(gxiodev_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsiodev.$(OBJ) $(C_) $(GLSRC)gsiodev.c

$(GLOBJ)gsgstate.$(OBJ) : $(GLSRC)gsgstate.c $(AK) $(gx_h)\
 $(gserrors_h) $(gscie_h) $(gscspace_h) $(gsstruct_h) $(gsutil_h) $(gxfmap_h)\
 $(gxbitmap_h) $(gxcmap_h) $(gxdht_h) $(gxgstate_h) $(gzht_h) $(gzline_h)\
 $(gsicc_cache_h) $(gsicc_manage_h) $(gsicc_profilecache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsgstate.$(OBJ) $(C_) $(GLSRC)gsgstate.c

$(GLOBJ)gsline.$(OBJ) : $(GLSRC)gsline.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h)\
 $(gscoord_h) $(gsline_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzline_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsline.$(OBJ) $(C_) $(GLSRC)gsline.c

$(GLOBJ)gsmatrix.$(OBJ) : $(GLSRC)gsmatrix.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h)\
 $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h) $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmatrix.$(OBJ) $(C_) $(GLSRC)gsmatrix.c

$(GLOBJ)gspaint.$(OBJ) : $(GLSRC)gspaint.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(gpcheck_h)\
 $(gsropt_h) $(gxfixed_h) $(gxmatrix_h) $(gspaint_h) $(gspath_h)\
 $(gzpath_h) $(gxpaint_h) $(gzstate_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gzcpath_h) $(gxhldevc_h) $(gsutil_h) $(gxdevsop_h) $(gsicc_cms_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspaint.$(OBJ) $(C_) $(GLSRC)gspaint.c

$(GLOBJ)gsparam.$(OBJ) : $(GLSRC)gsparam.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsparam_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparam.$(OBJ) $(C_) $(GLSRC)gsparam.c

# gsparamx is not included in the base configuration.
$(GLOBJ)gsparamx.$(OBJ) : $(AK) $(GLSRC)gsparamx.c $(string__h)\
 $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gsparamx_h)\
 $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparamx.$(OBJ) $(C_) $(GLSRC)gsparamx.c

# Future replacement for gsparams.c
$(GLOBJ)gsparam2.$(OBJ) : $(GLSRC)gsparam2.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsparams_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparam2.$(OBJ) $(C_) $(GLSRC)gsparam2.c

$(GLOBJ)gsparams.$(OBJ) : $(GLSRC)gsparams.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsparams_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparams.$(OBJ) $(C_) $(GLSRC)gsparams.c

$(GLOBJ)gspath.$(OBJ) : $(GLSRC)gspath.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gscoord_h) $(gspath_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h) $(gxpaint_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspath.$(OBJ) $(C_) $(GLSRC)gspath.c

$(GLOBJ)gsstate.$(OBJ) : $(GLSRC)gsstate.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsstruct_h) $(gsutil_h) $(gzstate_h) $(gxcspace_h)\
 $(gsalpha_h) $(gscolor2_h) $(gscoord_h) $(gscie_h)\
 $(gxclipsr_h) $(gxcmap_h) $(gxdevice_h) $(gxpcache_h)\
 $(gzht_h) $(gzline_h) $(gspath_h) $(gzpath_h) $(gzcpath_h)\
 $(gsovrc_h) $(gxcolor2_h) $(gxpcolor_h) $(gsicc_manage_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsstate.$(OBJ) $(C_) $(GLSRC)gsstate.c

$(GLOBJ)gstext.$(OBJ) : $(GLSRC)gstext.c $(AK) $(memory__h) $(gdebug_h)\
 $(gserrors_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(gxfcache_h) $(gxdevcli_h) $(gxdcolor_h) $(gxfont_h) $(gxpath_h)\
 $(gxtext_h) $(gzstate_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstext.$(OBJ) $(C_) $(GLSRC)gstext.c

# We make gsiodevs a separate module so the PS interpreter can replace it.

$(GLD)gsiodevs.dev : $(ECHOGS_XE) $(LIB_MAK) $(GLOBJ)gsiodevs.$(OBJ)\
 $(GLD)sfile.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gsiodevs $(GLOBJ)gsiodevs.$(OBJ)
	$(ADDMOD) $(GLD)gsiodevs -include $(GLD)sfile
	$(ADDMOD) $(GLD)gsiodevs -iodev stdin stdout stderr

$(GLOBJ)gsiodevs.$(OBJ) : $(GLSRC)gsiodevs.c $(AK) $(gx_h)\
 $(gserrors_h) $(gxiodev_h) $(stream_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsiodevs.$(OBJ) $(C_) $(GLSRC)gsiodevs.c

###### Internal devices

### Device support
# PC display color mapping
$(GLOBJ)gdevpccm.$(OBJ) : $(GLSRC)gdevpccm.c $(AK)\
 $(gx_h) $(gsmatrix_h) $(gxdevice_h) $(gdevpccm_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevpccm.$(OBJ) $(C_) $(GLSRC)gdevpccm.c

### Memory devices

$(GLOBJ)gdevmem.$(OBJ) : $(GLSRC)gdevmem.c $(AK) $(gx_h) $(gserrors_h) \
 $(memory__h)\
 $(gsdevice_h) $(gsrect_h) $(gsstruct_h) $(gstrans_h)\
 $(gxarith_h) $(gxgetbit_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmem.$(OBJ) $(C_) $(GLSRC)gdevmem.c

$(GLOBJ)gdevm1.$(OBJ) : $(GLSRC)gdevm1.c $(AK) $(gx_h) $(memory__h)\
 $(gsrop_h) $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)\
 $(gserrors_h)
	$(GLCC) $(GLO_)gdevm1.$(OBJ) $(C_) $(GLSRC)gdevm1.c

$(GLOBJ)gdevm2.$(OBJ) : $(GLSRC)gdevm2.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm2.$(OBJ) $(C_) $(GLSRC)gdevm2.c

$(GLOBJ)gdevm4.$(OBJ) : $(GLSRC)gdevm4.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm4.$(OBJ) $(C_) $(GLSRC)gdevm4.c

$(GLOBJ)gdevm8.$(OBJ) : $(GLSRC)gdevm8.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm8.$(OBJ) $(C_) $(GLSRC)gdevm8.c

$(GLOBJ)gdevm16.$(OBJ) : $(GLSRC)gdevm16.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm16.$(OBJ) $(C_) $(GLSRC)gdevm16.c

$(GLOBJ)gdevm24.$(OBJ) : $(GLSRC)gdevm24.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm24.$(OBJ) $(C_) $(GLSRC)gdevm24.c

$(GLOBJ)gdevm32.$(OBJ) : $(GLSRC)gdevm32.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm32.$(OBJ) $(C_) $(GLSRC)gdevm32.c

$(GLOBJ)gdevm40.$(OBJ) : $(GLSRC)gdevm40.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm40.$(OBJ) $(C_) $(GLSRC)gdevm40.c

$(GLOBJ)gdevm48.$(OBJ) : $(GLSRC)gdevm48.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm48.$(OBJ) $(C_) $(GLSRC)gdevm48.c

$(GLOBJ)gdevm56.$(OBJ) : $(GLSRC)gdevm56.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm56.$(OBJ) $(C_) $(GLSRC)gdevm56.c

$(GLOBJ)gdevm64.$(OBJ) : $(GLSRC)gdevm64.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevm64.$(OBJ) $(C_) $(GLSRC)gdevm64.c

$(GLOBJ)gdevmx.$(OBJ) : $(GLSRC)gdevmx.c $(AK) $(gx_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmx.$(OBJ) $(C_) $(GLSRC)gdevmx.c

$(GLOBJ)gdevmpla.$(OBJ) : $(GLSRC)gdevmpla.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsbitops_h) $(gxdcolor_h) $(gxpcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(gdevmem_h) $(gdevmpla_h)\
 $(gxdevsop_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmpla.$(OBJ) $(C_) $(GLSRC)gdevmpla.c

### Alpha-channel devices

$(GLOBJ)gdevabuf.$(OBJ) : $(GLSRC)gdevabuf.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h)\
 $(gxdevice_h) $(gxdevmem_h) $(gdevmem_h) $(gzstate_h) $(gxdevcli_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevabuf.$(OBJ) $(C_) $(GLSRC)gdevabuf.c

### Other built-in devices

# The bbox device can either be used as forwarding device to support
# graphics functions, or it can be a real target device. We create
# the bboxutil.dev pseudo device to allow inclusion without putting
# the bbox device on the list of devices.

$(GLD)bboxutil.dev : $(ECHOGS_XE) $(LIB_MAK) $(GLOBJ)gdevbbox.$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)bboxutil $(GLOBJ)gdevbbox.$(OBJ)

$(GLD)bbox.dev : $(ECHOGS_XE) $(LIB_MAK) $(GLOBJ)gdevbbox.$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(SETDEV2) $(GLD)bbox $(GLOBJ)gdevbbox.$(OBJ)

$(GLOBJ)gdevbbox.$(OBJ) : $(GLSRC)gdevbbox.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gdevbbox_h) $(gsdevice_h) $(gsparam_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxiparam_h) $(gxgstate_h)\
 $(gxpaint_h) $(gxpath_h) $(gdevkrnlsclass_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevbbox.$(OBJ) $(C_) $(GLSRC)gdevbbox.c

$(GLOBJ)gdevhit.$(OBJ) : $(GLSRC)gdevhit.c $(AK) $(std_h)\
  $(gserrors_h) $(gsmemory_h) $(gstypes_h) $(gxdevice_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevhit.$(OBJ) $(C_) $(GLSRC)gdevhit.c

# A device that stores its data using run-length encoding.

$(GLOBJ)gdevmrun.$(OBJ) : $(GLSRC)gdevmrun.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gxdevice_h) $(gdevmrun_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmrun.$(OBJ) $(C_) $(GLSRC)gdevmrun.c

# A device that extracts a single plane from multi-plane color.

$(GLOBJ)gdevplnx.$(OBJ) : $(GLSRC)gdevplnx.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsbitops_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h)\
 $(gdevplnx_h)\
 $(gxcmap_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdither_h)\
 $(gxgetbit_h) $(gxiparam_h) $(gxgstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevplnx.$(OBJ) $(C_) $(GLSRC)gdevplnx.c

### Default driver procedure implementations

$(GLOBJ)gdevdbit.$(OBJ) : $(GLSRC)gdevdbit.c $(AK) $(gx_h)\
 $(gserrors_h) $(gpcheck_h)\
 $(gdevmem_h) $(gsbittab_h) $(gsrect_h) $(gsropt_h)\
 $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdbit.$(OBJ) $(C_) $(GLSRC)gdevdbit.c

$(GLOBJ)gdevddrw.$(OBJ) : $(GLSRC)gdevddrw.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(stdint__h) $(gpcheck_h) $(gsrect_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxiparam_h) $(gxgstate_h)\
 $(gxmatrix_h) $(gxhldevc_h) $(gdevddrw_h) $(gxdtfill_h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevddrw.$(OBJ) $(C_) $(GLSRC)gdevddrw.c

$(GLOBJ)gdevdsha.$(OBJ) : $(GLSRC)gdevdsha.c $(AK) $(gx_h)\
 $(gserrors_h) $(gxdevice_h) $(gxcindex_h) \
 $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdsha.$(OBJ) $(C_) $(GLSRC)gdevdsha.c

$(GLOBJ)gdevdflt.$(OBJ) : $(GLSRC)gdevdflt.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsropt_h) $(gxcomp_h) $(gxdevice_h) $(gxdevsop_h) $(math__h)\
 $(gsstruct_h) $(gxobj_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdflt.$(OBJ) $(C_) $(GLSRC)gdevdflt.c

$(GLOBJ)gdevdgbr.$(OBJ) : $(GLSRC)gdevdgbr.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gxdevsop_h)\
 $(gdevmem_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(gxlum_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdgbr.$(OBJ) $(C_) $(GLSRC)gdevdgbr.c

$(GLOBJ)gdevnfwd.$(OBJ) : $(GLSRC)gdevnfwd.c $(AK) $(gx_h)\
 $(gserrors_h) $(gxdevice_h) $(gxcmap_h) $(memory__h) $(gxdevsop_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevnfwd.$(OBJ) $(C_) $(GLSRC)gdevnfwd.c

# ---------------- Font API ---------------- #

gxfapi_h=$(GLSRC)gxfapi.h $(gsmatrix_h) $(gsmemory_h) $(stdint__h) $(gsgstate_h) $(gsccode_h)

# stub for UFST bridge support  :

$(GLD)gxfapiu.dev : $(LIB_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gxfapiu

wrfont_h=$(GLSRC)wrfont.h $(std_h) $(stdpre_h)
write_t1_h=$(GLSRC)write_t1.h $(gxfapi_h)
write_t2_h=$(GLSRC)write_t2.h $(gxfapi_h)

$(GLOBJ)write_t1.$(OBJ) : $(GLSRC)write_t1.c $(AK)\
 $(wrfont_h) $(write_t1_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(FT_CFLAGS) $(GLO_)write_t1.$(OBJ) $(C_) $(GLSRC)write_t1.c

$(GLOBJ)write_t2.$(OBJ) : $(GLSRC)write_t2.c $(AK)\
 $(wrfont_h) $(write_t2_h) $(gxfont_h) $(gxfont1_h) $(gzstate_h) $(stdpre_h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(FT_CFLAGS) $(GLO_)write_t2.$(OBJ) $(C_) $(GLSRC)write_t2.c

$(GLOBJ)wrfont.$(OBJ) : $(GLSRC)wrfont.c $(AK)\
 $(wrfont_h) $(stdio__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(FT_CFLAGS) $(GLO_)wrfont.$(OBJ) $(C_) $(GLSRC)wrfont.c

# stub for UFST bridge :

$(GLD)fapiu.dev : $(INT_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)fapiu

# stub for Bitstream bridge (see fapi_bs.mak):

$(GLD)fapib.dev : $(INT_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)fapib

# FreeType bridge :

# the top-level makefile should define
# FT_CFLAGS for the include directive and other switches

$(GLD)fapif1.dev : $(INT_MAK) $(ECHOGS_XE) $(GLOBJ)fapi_ft.$(OBJ) \
 $(GLOBJ)write_t1.$(OBJ) $(GLOBJ)write_t2.$(OBJ) $(GLOBJ)wrfont.$(OBJ) \
 $(GLD)freetype.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)fapif1 $(GLOBJ)fapi_ft.$(OBJ) $(GLOBJ)write_t1.$(OBJ)
	$(ADDMOD) $(GLD)fapif1 $(GLOBJ)write_t2.$(OBJ) $(GLOBJ)wrfont.$(OBJ)
	$(ADDMOD) $(GLD)fapif1 -include $(GLD)freetype
	$(ADDMOD) $(GLD)fapif1 -fapi fapi_ft

$(GLOBJ)fapi_ft.$(OBJ) : $(GLSRC)fapi_ft.c $(AK)\
 $(stdio__h) $(malloc__h) $(write_t1_h) $(write_t2_h) $(math__h) $(gserrors_h)\
 $(gsmemory_h) $(gsmalloc_h) $(gxfixed_h) $(gdebug_h) $(gxbitmap_h) $(gsmchunk_h) \
 $(stream_h) $(gxiodev_h) $(gsfname_h) $(gxfapi_h)  $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(FT_CFLAGS) $(GLO_)fapi_ft.$(OBJ) $(C_) $(GLSRC)fapi_ft.c

# stub for FreeType bridge :

$(GLD)fapif0.dev : $(INT_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)fapif0


$(GLOBJ)gxfapi.$(OBJ) : $(GLSRC)gxfapi.c $(memory__h) $(gsmemory_h) $(gserrors_h) $(gxdevice_h) \
                 $(gxfont_h) $(gxfont1_h) $(gxpath_h) $(gxfcache_h) $(gxchrout_h) $(gximask_h) \
                 $(gscoord_h) $(gspaint_h) $(gspath_h) $(gzstate_h) $(gxfcid_h) $(gxchar_h) \
                 $(gdebug_h) $(gsimage_h) $(gxfapi_h) $(gsbittab_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxfapi.$(OBJ) $(C_) $(GLSRC)gxfapi.c

$(GLD)gxfapi.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLOBJ)gxfapi.$(OBJ) $(GLD)fapiu$(UFST_BRIDGE).dev \
                 $(GLD)fapif$(FT_BRIDGE).dev $(GLD)fapib$(BITSTREAM_BRIDGE).dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gxfapi $(GLOBJ)gxfapi.$(OBJ)
	$(ADDMOD) $(GLD)gxfapi -include $(GLD)fapiu$(UFST_BRIDGE)
	$(ADDMOD) $(GLD)gxfapi -include $(GLD)fapif$(FT_BRIDGE)
	$(ADDMOD) $(GLD)gxfapi -include $(GLD)fapib$(BITSTREAM_BRIDGE)

### Other device support

# Provide a mapping between StandardEncoding and ISOLatin1Encoding.
$(GLOBJ)gdevemap.$(OBJ) : $(GLSRC)gdevemap.c $(AK) $(std_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevemap.$(OBJ) $(C_) $(GLSRC)gdevemap.c

# ----------- Trapping routines ------------ #
claptrap_h=$(GLSRC)claptrap.h $(gsmemory_h) $(std_h) $(stdpre_h)
claptrap_impl_h=$(GLSRC)claptrap-impl.h
claptrap=$(GLOBJ)claptrap.$(OBJ) $(GLOBJ)claptrap-init.$(OBJ) \
 $(GLOBJ)claptrap-planar.$(OBJ)

$(GLOBJ)claptrap.$(OBJ) : $(GLSRC)claptrap.c $(AK) \
 $(claptrap_h) $(claptrap_impl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)claptrap.$(OBJ) $(C_) $(GLSRC)claptrap.c

$(GLOBJ)claptrap-init.$(OBJ) : $(GLSRC)claptrap-init.c $(AK) \
 $(claptrap_h) $(claptrap_impl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)claptrap-init.$(OBJ) $(C_) $(GLSRC)claptrap-init.c

$(GLOBJ)claptrap-planar.$(OBJ) : $(GLSRC)claptrap-planar.c $(AK) \
 $(claptrap_h) $(claptrap_impl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)claptrap-planar.$(OBJ) $(C_) $(GLSRC)claptrap-planar.c

# ----------- ETS routines ------------ #
ets_h=$(GLSRC)ets.h $(stdpre_h)
ets_tm_h=$(GLSRC)ets_tm.h
ets=$(GLOBJ)ets.$(OBJ)

$(GLOBJ)ets.$(OBJ) : $(GLSRC)ets.c $(AK) \
 $(ets_h) $(ets_tm_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ets.$(OBJ) $(C_) $(GLSRC)ets.c

# ----------- Downsampling routines ------------ #
gxdownscale_h=$(GLSRC)gxdownscale.h $(gxgetbit_h) $(gxdevcli_h) $(claptrap_h) $(gsmemory_h) $(ctype__h) $(gstypes_h)
downscale_=$(GLOBJ)gxdownscale.$(OBJ) $(claptrap) $(ets)

$(GLOBJ)gxdownscale.$(OBJ) : $(GLSRC)gxdownscale.c $(AK) $(string__h) \
 $(gxdownscale_h) $(gserrors_h) $(gdevprn_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdownscale.$(OBJ) $(C_) $(GLSRC)gxdownscale.c

###### Create a pseudo-"feature" for the entire graphics library.

LIB0s=$(GLOBJ)gpmisc.$(OBJ) $(GLOBJ)stream.$(OBJ) $(GLOBJ)strmio.$(OBJ)
LIB1s=$(GLOBJ)gsalloc.$(OBJ) $(GLOBJ)gsalpha.$(OBJ) $(GLOBJ)gxdownscale.$(OBJ) $(downscale_) $(GLOBJ)gdevprn.$(OBJ) $(GLOBJ)gdevflp.$(OBJ) $(GLOBJ)gdevkrnlsclass.$(OBJ) $(GLOBJ)gdevepo.$(OBJ)
LIB2s=$(GLOBJ)gdevmplt.$(OBJ) $(GLOBJ)gsbitcom.$(OBJ) $(GLOBJ)gsbitops.$(OBJ) $(GLOBJ)gsbittab.$(OBJ) $(GLOBJ)gdevoflt.$(OBJ) $(GLOBJ)gdevsclass.$(OBJ)
# Note: gschar.c is no longer required for a standard build;
# we include it only for backward compatibility for library clients.
LIB3s=$(GLOBJ)gscedata.$(OBJ) $(GLOBJ)gscencs.$(OBJ) $(GLOBJ)gschar.$(OBJ) $(GLOBJ)gscolor.$(OBJ)
LIB4s=$(GLOBJ)gscoord.$(OBJ) $(GLOBJ)gscparam.$(OBJ) $(GLOBJ)gscspace.$(OBJ)  $(GLOBJ)gscicach.$(OBJ) $(GLOBJ)gsovrc.$(OBJ) $(GLOBJ)gxoprect.$(OBJ)
LIB5s=$(GLOBJ)gsdevice.$(OBJ) $(GLOBJ)gsdevmem.$(OBJ) $(GLOBJ)gsdparam.$(OBJ)
LIB6s=$(GLOBJ)gsfname.$(OBJ) $(GLOBJ)gsfont.$(OBJ) $(GLOBJ)gsgdata.$(OBJ) $(GLOBJ)gsgcache.$(OBJ)
LIB7s=$(GLOBJ)gsht.$(OBJ) $(GLOBJ)gshtscr.$(OBJ) $(GLOBJ)gen_ordered.$(OBJ)
LIB8s=$(GLOBJ)gsimage.$(OBJ) $(GLOBJ)gsimpath.$(OBJ) $(GLOBJ)gsinit.$(OBJ)
LIB9s=$(GLOBJ)gsiodev.$(OBJ) $(GLOBJ)gsgstate.$(OBJ) $(GLOBJ)gsline.$(OBJ)
LIB10s=$(GLOBJ)gsmalloc.$(OBJ) $(GLOBJ)memento.$(OBJ) $(GLOBJ)bobbin.$(OBJ) $(GLOBJ)gsmatrix.$(OBJ)
LIB11s=$(GLOBJ)gsmemory.$(OBJ) $(GLOBJ)gsmemret.$(OBJ) $(GLOBJ)gsmisc.$(OBJ) $(GLOBJ)gsnotify.$(OBJ) $(GLOBJ)gslibctx.$(OBJ)
LIB12s=$(GLOBJ)gspaint.$(OBJ) $(GLOBJ)gsparam.$(OBJ) $(GLOBJ)gspath.$(OBJ)
LIB13s=$(GLOBJ)gsserial.$(OBJ) $(GLOBJ)gsstate.$(OBJ) $(GLOBJ)gstext.$(OBJ)\
  $(GLOBJ)gsutil.$(OBJ) $(GLOBJ)gssprintf.$(OBJ) $(GLOBJ)gsstrtok.$(OBJ) $(GLOBJ)gsstrl.$(OBJ)
LIB1x=$(GLOBJ)gxacpath.$(OBJ) $(GLOBJ)gxbcache.$(OBJ) $(GLOBJ)gxccache.$(OBJ)
LIB2x=$(GLOBJ)gxccman.$(OBJ) $(GLOBJ)gxchar.$(OBJ) $(GLOBJ)gxcht.$(OBJ)
LIB3x=$(GLOBJ)gxclip.$(OBJ) $(GLOBJ)gxcmap.$(OBJ) $(GLOBJ)gxcpath.$(OBJ)
LIB4x=$(GLOBJ)gxdcconv.$(OBJ) $(GLOBJ)gxdcolor.$(OBJ) $(GLOBJ)gxhldevc.$(OBJ)
LIB5x=$(GLOBJ)gxfill.$(OBJ) $(GLOBJ)gxht.$(OBJ) $(GLOBJ)gxhtbit.$(OBJ)\
  $(GLOBJ)gxht_thresh.$(OBJ)
LIB6x=$(GLOBJ)gxidata.$(OBJ) $(GLOBJ)gxifast.$(OBJ) $(GLOBJ)gximage.$(OBJ) $(GLOBJ)gximdecode.$(OBJ)
LIB7x=$(GLOBJ)gximage1.$(OBJ) $(GLOBJ)gximono.$(OBJ) $(GLOBJ)gxipixel.$(OBJ) $(GLOBJ)gximask.$(OBJ)
LIB8x=$(GLOBJ)gxi12bit.$(OBJ) $(GLOBJ)gxi16bit.$(OBJ) $(GLOBJ)gxiscale.$(OBJ) $(GLOBJ)gxpaint.$(OBJ) $(GLOBJ)gxpath.$(OBJ) $(GLOBJ)gxpath2.$(OBJ)
LIB9x=$(GLOBJ)gxpcopy.$(OBJ) $(GLOBJ)gxpdash.$(OBJ) $(GLOBJ)gxpflat.$(OBJ)
LIB10x=$(GLOBJ)gxsample.$(OBJ) $(GLOBJ)gxstroke.$(OBJ) $(GLOBJ)gxsync.$(OBJ)
LIB1d=$(GLOBJ)gdevabuf.$(OBJ) $(GLOBJ)gdevdbit.$(OBJ) $(GLOBJ)gdevddrw.$(OBJ) $(GLOBJ)gdevdflt.$(OBJ)
LIB2d=$(GLOBJ)gdevdgbr.$(OBJ) $(GLOBJ)gdevnfwd.$(OBJ) $(GLOBJ)gdevmem.$(OBJ) $(GLOBJ)gdevplnx.$(OBJ)
LIB3d=$(GLOBJ)gdevm1.$(OBJ) $(GLOBJ)gdevm2.$(OBJ) $(GLOBJ)gdevm4.$(OBJ) $(GLOBJ)gdevm8.$(OBJ)
LIB4d=$(GLOBJ)gdevm16.$(OBJ) $(GLOBJ)gdevm24.$(OBJ) $(GLOBJ)gdevm32.$(OBJ) $(GLOBJ)gdevmpla.$(OBJ)
LIB5d=$(GLOBJ)gdevm40.$(OBJ) $(GLOBJ)gdevm48.$(OBJ) $(GLOBJ)gdevm56.$(OBJ) $(GLOBJ)gdevm64.$(OBJ) $(GLOBJ)gdevmx.$(OBJ)
LIB6d=$(GLOBJ)gdevdsha.$(OBJ) $(GLOBJ)gxscanc.$(OBJ)
LIBs=$(LIB0s) $(LIB1s) $(LIB2s) $(LIB3s) $(LIB4s) $(LIB5s) $(LIB6s) $(LIB7s)\
 $(LIB8s) $(LIB9s) $(LIB10s) $(LIB11s) $(LIB12s) $(LIB13s)
LIBx=$(LIB1x) $(LIB2x) $(LIB3x) $(LIB4x) $(LIB5x) $(LIB6x) $(LIB7x) $(LIB8x) $(LIB9x) $(LIB10x)
LIBd=$(LIB1d) $(LIB2d) $(LIB3d) $(LIB4d) $(LIB5d) $(LIB6d)
LIB_ALL=$(LIBs) $(LIBx) $(LIBd)
# We include some optional library modules in the dependency list,
# but not in the link, to catch compilation problems.
LIB_O=$(GLOBJ)gdevmpla.$(OBJ) $(GLOBJ)gdevmrun.$(OBJ) $(GLOBJ)gshtx.$(OBJ) $(GLOBJ)gsnogc.$(OBJ)
$(GLD)libs.dev : $(LIB_MAK) $(ECHOGS_XE) $(LIBs) $(LIB_O) $(GLD)gsiodevs.dev $(GLD)translib.dev \
                 $(GLD)clist.dev $(GLD)gxfapi.dev $(GLD)romfs1.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)libs $(LIB0s)
	$(ADDMOD) $(GLD)libs $(LIB1s)
	$(ADDMOD) $(GLD)libs $(LIB2s)
	$(ADDMOD) $(GLD)libs $(LIB3s)
	$(ADDMOD) $(GLD)libs $(LIB4s)
	$(ADDMOD) $(GLD)libs $(LIB5s)
	$(ADDMOD) $(GLD)libs $(LIB6s)
	$(ADDMOD) $(GLD)libs $(LIB7s)
	$(ADDMOD) $(GLD)libs $(LIB8s)
	$(ADDMOD) $(GLD)libs $(LIB9s)
	$(ADDMOD) $(GLD)libs $(LIB10s)
	$(ADDMOD) $(GLD)libs $(LIB11s)
	$(ADDMOD) $(GLD)libs $(LIB12s)
	$(ADDMOD) $(GLD)libs $(LIB13s)
	$(ADDCOMP) $(GLD)libs overprint
	$(ADDCOMP) $(GLD)libs pdf14trans
	$(ADDMOD) $(GLD)libs -init gshtscr
	$(ADDMOD) $(GLD)libs -include $(GLD)gsiodevs
	$(ADDMOD) $(GLD)libs -include $(GLD)translib
	$(ADDMOD) $(GLD)libs -include $(GLD)clist
	$(ADDMOD) $(GLD)libs -include $(GLD)romfs1
	$(ADDMOD) $(GLD)libs $(GLD)gxfapi
	$(ADDMOD) $(GLD)libs -init fapi
$(GLD)libx.dev : $(LIB_MAK) $(ECHOGS_XE) $(LIBx) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)libx $(LIB1x)
	$(ADDMOD) $(GLD)libx $(LIB2x)
	$(ADDMOD) $(GLD)libx $(LIB3x)
	$(ADDMOD) $(GLD)libx $(LIB4x)
	$(ADDMOD) $(GLD)libx $(LIB5x)
	$(ADDMOD) $(GLD)libx $(LIB6x)
	$(ADDMOD) $(GLD)libx $(LIB7x)
	$(ADDMOD) $(GLD)libx $(LIB8x)
	$(ADDMOD) $(GLD)libx $(LIB9x)
	$(ADDMOD) $(GLD)libx $(LIB10x)
	$(ADDMOD) $(GLD)libx -imageclass 0_interpolate
	$(ADDMOD) $(GLD)libx -imageclass 1_simple 3_mono
	$(ADDMOD) $(GLD)libx -imagetype 1 mask1

$(GLD)libd.dev : $(LIB_MAK) $(ECHOGS_XE) $(LIBd) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)libd $(LIB1d)
	$(ADDMOD) $(GLD)libd $(LIB2d)
	$(ADDMOD) $(GLD)libd $(LIB3d)
	$(ADDMOD) $(GLD)libd $(LIB4d)
	$(ADDMOD) $(GLD)libd $(LIB5d)
	$(ADDMOD) $(GLD)libd $(LIB6d)

$(GLD)libcore.dev : $(LIB_MAK) $(ECHOGS_XE)\
 $(GLD)libs.dev $(GLD)libx.dev $(GLD)libd.dev\
 $(GLD)iscale.dev $(GLD)roplib.dev $(GLD)strdline.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)libcore
	$(ADDMOD) $(GLD)libcore -dev2 nullpage
	$(ADDMOD) $(GLD)libcore -include $(GLD)libs $(GLD)libx $(GLD)libd
	$(ADDMOD) $(GLD)libcore -include $(GLD)iscale $(GLD)roplib
	$(ADDMOD) $(GLD)libcore -include $(GLD)strdline

# ---------------- Stream support ---------------- #
# Currently the only things in the library that use this are clists
# and file streams.

$(GLOBJ)stream.$(OBJ) : $(GLSRC)stream.c $(AK) $(stdio__h) $(memory__h)\
 $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)stream.$(OBJ) $(C_) $(GLSRC)stream.c

# Default, stream-based readline.
strdline_=$(GLOBJ)gp_strdl.$(OBJ)
$(GLD)strdline.dev : $(LIB_MAK) $(ECHOGS_XE) $(strdline_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)strdline $(strdline_)

$(GLOBJ)gp_strdl.$(OBJ) : $(GLSRC)gp_strdl.c $(AK) $(std_h) $(gp_h)\
 $(gsmemory_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_strdl.$(OBJ) $(C_) $(GLSRC)gp_strdl.c

# ---------------- File streams ---------------- #
# Currently only the high-level drivers use these, but more drivers will
# probably use them eventually.

sfile_=$(GLOBJ)sfx$(FILE_IMPLEMENTATION).$(OBJ) $(GLOBJ)sfxcommon.$(OBJ)\
 $(GLOBJ)stream.$(OBJ)

$(GLD)sfile.dev : $(LIB_MAK) $(ECHOGS_XE) $(sfile_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sfile $(sfile_)

$(GLOBJ)sfxcommon.$(OBJ) : $(GLSRC)sfxcommon.c $(AK) $(stdio__h)\
 $(memory__h) $(unistd__h) $(gsmemory_h) $(gp_h) $(stream_h)\
 $(gserrors_h) $(assert__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfxcommon.$(OBJ) $(C_) $(GLSRC)sfxcommon.c

$(GLOBJ)sfxstdio.$(OBJ) : $(GLSRC)sfxstdio.c $(AK) $(stdio__h)\
 $(memory__h) $(unistd__h) $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h)\
 $(gp_h) $(gserrors_h) $(gsmemory_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfxstdio.$(OBJ) $(C_) $(GLSRC)sfxstdio.c

$(GLOBJ)sfxfd.$(OBJ) : $(GLSRC)sfxfd.c $(AK)\
 $(stdio__h) $(errno__h) $(memory__h) $(unistd__h)\
 $(gdebug_h) $(gpcheck_h) $(stream_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfxfd.$(OBJ) $(C_) $(GLSRC)sfxfd.c

$(GLOBJ)sfxboth.$(OBJ) : $(GLSRC)sfxboth.c $(GLSRC)sfxstdio.c $(GLSRC)sfxfd.c\
 $(AK) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfxboth.$(OBJ) $(C_) $(GLSRC)sfxboth.c

strmio_h=$(GLSRC)strmio.h $(scommon_h)

$(GLOBJ)strmio.$(OBJ) : $(GLSRC)strmio.c $(AK) $(malloc__h)\
  $(memory__h) $(gdebug_h) $(gsfname_h) $(gslibctx_h) $(gsstype_h)\
  $(gsmalloc_h) $(gsmemret_h) $(strmio_h) $(stream_h) $(gxiodev_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)strmio.$(OBJ) $(C_) $(GLSRC)strmio.c

# ---------------- BCP filters ---------------- #

$(GLOBJ)sbcp.$(OBJ) : $(GLSRC)sbcp.c $(AK) $(stdio__h)\
 $(sbcp_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sbcp.$(OBJ) $(C_) $(GLSRC)sbcp.c

# ---------------- CCITTFax filters ---------------- #
# These are used by clists, some drivers, and Level 2 in general.

cfe_=$(GLOBJ)scfe.$(OBJ) $(GLOBJ)scfetab.$(OBJ) $(GLOBJ)shc.$(OBJ)
$(GLD)cfe.dev : $(LIB_MAK) $(ECHOGS_XE) $(cfe_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cfe $(cfe_)

$(GLOBJ)scfe.$(OBJ) : $(GLSRC)scfe.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(scf_h) $(strimpl_h) $(scfx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scfe.$(OBJ) $(C_) $(GLSRC)scfe.c

$(GLOBJ)scfetab.$(OBJ) : $(GLSRC)scfetab.c $(AK) $(std_h) $(scommon_h)\
 $(scf_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scfetab.$(OBJ) $(C_) $(GLSRC)scfetab.c

$(GLOBJ)shc.$(OBJ) : $(GLSRC)shc.c $(AK) $(std_h) $(scommon_h) $(shc_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)shc.$(OBJ) $(C_) $(GLSRC)shc.c

cfd_=$(GLOBJ)scfd.$(OBJ) $(GLOBJ)scfdtab.$(OBJ)
$(GLD)cfd.dev : $(LIB_MAK) $(ECHOGS_XE) $(cfd_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cfd $(cfd_)

$(GLOBJ)scfd.$(OBJ) : $(GLSRC)scfd.c $(AK) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(scf_h) $(strimpl_h) $(scfx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scfd.$(OBJ) $(C_) $(GLSRC)scfd.c

$(GLOBJ)scfdtab.$(OBJ) : $(GLSRC)scfdtab.c $(AK) $(std_h) $(scommon_h)\
 $(scf_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scfdtab.$(OBJ) $(C_) $(GLSRC)scfdtab.c

# scfparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in cfe or cfd.
$(GLOBJ)scfparam.$(OBJ) : $(GLSRC)scfparam.c $(AK) $(std_h)\
 $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(scommon_h) $(scf_h) $(scfx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scfparam.$(OBJ) $(C_) $(GLSRC)scfparam.c

# ---------------- DCT (JPEG) filters ---------------- #
# These are used by Level 2, and by the JPEG-writing driver.

# Common code

sdcparam_h=$(GLSRC)sdcparam.h $(sdct_h) $(gsparam_h)

sdctc_=$(GLOBJ)sdctc.$(OBJ) $(GLOBJ)sjpegc.$(OBJ)

$(GLOBJ)sdctc.$(OBJ) : $(GLSRC)sdctc.c $(AK) $(stdio__h) $(jpeglib__h)\
 $(strimpl_h) $(sdct_h) $(sjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sdctc.$(OBJ) $(C_) $(GLSRC)sdctc.c

$(GLOBJ)sjpegc_1.$(OBJ) : $(GLSRC)sjpegc.c $(AK) $(stdio__h) $(string__h)\
 $(gx_h) $(jpeglib__h) $(gconfig__h) \
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpegc_1.$(OBJ) $(C_) $(GLSRC)sjpegc.c

$(GLOBJ)sjpegc_0.$(OBJ) : $(GLSRC)sjpegc.c $(AK) $(stdio__h) $(string__h)\
 $(gx_h) $(jerror__h) $(jpeglib__h) $(gconfig__h) $(JSRCDIR)$(D)jmemsys.h\
 $(gserrors_h) $(sjpeg_h) $(sdct_h) $(strimpl_h) $(gsmchunk_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpegc_0.$(OBJ) $(C_) $(GLSRC)sjpegc.c

$(GLOBJ)sjpegc.$(OBJ) : $(GLOBJ)sjpegc_$(SHARE_JPEG).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)sjpegc_$(SHARE_JPEG).$(OBJ) $(GLOBJ)sjpegc.$(OBJ)

# sdcparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in sdcte/d.
$(GLOBJ)sdcparam.$(OBJ) : $(GLSRC)sdcparam.c $(AK) $(memory__h)\
 $(jpeglib__h)\
 $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sdcparam.$(OBJ) $(C_) $(GLSRC)sdcparam.c

# Encoding (compression)

sdcte_=$(sdctc_) $(GLOBJ)sdcte.$(OBJ) $(GLOBJ)sjpege.$(OBJ)
$(GLD)sdcte.dev : $(LIB_MAK) $(ECHOGS_XE) $(sdcte_) $(JGENDIR)$(D)jpege.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sdcte $(sdcte_)
	$(ADDMOD) $(GLD)sdcte -include $(JGENDIR)$(D)jpege.dev

$(GLOBJ)sdcte_1.$(OBJ) : $(GLSRC)sdcte.c $(AK)\
 $(memory__h) $(stdio__h) $(jpeglib__h)\
 $(gdebug_h) $(gsmemory_h) $(strimpl_h) $(sdct_h) $(sjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sdcte_1.$(OBJ) $(C_) $(GLSRC)sdcte.c

$(GLOBJ)sdcte_0.$(OBJ) : $(GLSRC)sdcte.c $(AK)\
 $(memory__h) $(stdio__h) $(jerror__h) $(jpeglib__h)\
 $(gdebug_h) $(gsmemory_h) $(strimpl_h) $(sdct_h) $(sjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sdcte_0.$(OBJ) $(C_) $(GLSRC)sdcte.c

$(GLOBJ)sdcte.$(OBJ) : $(GLOBJ)sdcte_$(SHARE_JPEG).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)sdcte_$(SHARE_JPEG).$(OBJ) $(GLOBJ)sdcte.$(OBJ)


$(GLOBJ)sjpege_1.$(OBJ) : $(GLSRC)sjpege.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jpeglib__h)\
 $(sjpeg_h) $(sdct_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpege_1.$(OBJ) $(C_) $(GLSRC)sjpege.c

$(GLOBJ)sjpege_0.$(OBJ) : $(GLSRC)sjpege.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jerror__h) $(jpeglib__h)\
 $(sjpeg_h) $(sdct_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpege_0.$(OBJ) $(C_) $(GLSRC)sjpege.c

$(GLOBJ)sjpege.$(OBJ) : $(GLOBJ)sjpege_$(SHARE_JPEG).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)sjpege_$(SHARE_JPEG).$(OBJ) $(GLOBJ)sjpege.$(OBJ)

# sdeparam is used by the filter operator and the PS/PDF writer.
# It is not included automatically in sdcte.
sdeparam_=$(GLOBJ)sdeparam.$(OBJ) $(GLOBJ)sdcparam.$(OBJ)
$(GLD)sdeparam.dev : $(LIB_MAK) $(ECHOGS_XE) $(sdeparam_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sdeparam $(sdeparam_)

$(GLOBJ)sdeparam.$(OBJ) : $(GLSRC)sdeparam.c $(AK) $(memory__h)\
 $(jpeglib__h)\
 $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sdeparam.$(OBJ) $(C_) $(GLSRC)sdeparam.c

# Decoding (decompression)

sdctd_=$(sdctc_) $(GLOBJ)sdctd.$(OBJ) $(GLOBJ)sjpegd.$(OBJ)
$(GLD)sdctd.dev : $(LIB_MAK) $(ECHOGS_XE) $(sdctd_) $(JGENDIR)$(D)jpegd.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sdctd $(sdctd_)
	$(ADDMOD) $(GLD)sdctd -include $(JGENDIR)$(D)jpegd.dev

$(GLOBJ)sdctd_1.$(OBJ) : $(GLSRC)sdctd.c $(AK)\
 $(memory__h) $(stdio__h) $(jpeglib__h)\
 $(gdebug_h) $(gsmemory_h) $(strimpl_h) $(sdct_h) $(sjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sdctd_1.$(OBJ) $(C_) $(GLSRC)sdctd.c

$(GLOBJ)sdctd_0.$(OBJ) : $(GLSRC)sdctd.c $(AK)\
 $(memory__h) $(stdio__h) $(jerror__h) $(jpeglib__h)\
 $(gdebug_h) $(gsmemory_h) $(strimpl_h) $(sdct_h) $(sjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sdctd_0.$(OBJ) $(C_) $(GLSRC)sdctd.c

$(GLOBJ)sdctd.$(OBJ) : $(GLOBJ)sdctd_$(SHARE_JPEG).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)sdctd_$(SHARE_JPEG).$(OBJ) $(GLOBJ)sdctd.$(OBJ)


$(GLOBJ)sjpegd_1.$(OBJ) : $(GLSRC)sjpegd.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jpeglib__h) $(gserrors_h)\
 $(sjpeg_h) $(sdct_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpegd_1.$(OBJ) $(C_) $(GLSRC)sjpegd.c

$(GLOBJ)sjpegd_0.$(OBJ) : $(GLSRC)sjpegd.c $(AK)\
 $(stdio__h) $(string__h) $(gx_h)\
 $(jerror__h) $(jpeglib__h)\
 $(sjpeg_h) $(sdct_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJCC) $(GLO_)sjpegd_0.$(OBJ) $(C_) $(GLSRC)sjpegd.c


$(GLOBJ)sjpegd.$(OBJ) : $(GLOBJ)sjpegd_$(SHARE_JPEG).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)sjpegd_$(SHARE_JPEG).$(OBJ) $(GLOBJ)sjpegd.$(OBJ)

# One .dev for both encoding and decoding
$(GLD)sdct.dev: $(GLD)sdctd.dev $(GLD)sdcte.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sdct -include $(GLD)sdctd.dev $(GLD)sdcte.dev

# sddparam is used by the filter operator.
# It is not included automatically in sdctd.
sddparam_=$(GLOBJ)sddparam.$(OBJ) $(GLOBJ)sdcparam.$(OBJ)
$(GLD)sddparam.dev : $(LIB_MAK) $(ECHOGS_XE) $(sddparam_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sddparam $(sddparam_)

$(GLOBJ)sddparam.$(OBJ) : $(GLSRC)sddparam.c $(AK) $(std_h)\
 $(jpeglib__h)\
 $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(sdcparam_h) $(sdct_h) $(sjpeg_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sddparam.$(OBJ) $(C_) $(GLSRC)sddparam.c

# ---------------- LZW filters ---------------- #
# These are used by Level 2 in general.

lzwe_=$(GLOBJ)slzwe.$(OBJ) $(GLOBJ)slzwc.$(OBJ)
$(GLD)lzwe.dev : $(LIB_MAK) $(ECHOGS_XE) $(lzwe_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)lzwe $(lzwe_)

# We need slzwe.dev as a synonym for lzwe.dev for BAND_LIST_STORAGE = memory.
$(GLD)slzwe.dev : $(GLD)lzwe.dev $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLD)lzwe.dev $(GLD)slzwe.dev

$(GLOBJ)slzwe.$(OBJ) : $(GLSRC)slzwe.c $(AK) $(stdio__h) $(gdebug_h)\
 $(slzwx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)slzwe.$(OBJ) $(C_) $(GLSRC)slzwe.c

$(GLOBJ)slzwc.$(OBJ) : $(GLSRC)slzwc.c $(AK) $(std_h)\
 $(slzwx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)slzwc.$(OBJ) $(C_) $(GLSRC)slzwc.c

lzwd_=$(GLOBJ)slzwd.$(OBJ) $(GLOBJ)slzwc.$(OBJ)
$(GLD)lzwd.dev : $(LIB_MAK) $(ECHOGS_XE) $(lzwd_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)lzwd $(lzwd_)

# We need slzwd.dev as a synonym for lzwd.dev for BAND_LIST_STORAGE = memory.
$(GLD)slzwd.dev : $(GLD)lzwd.dev $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLD)lzwd.dev $(GLD)slzwd.dev

$(GLOBJ)slzwd.$(OBJ) : $(GLSRC)slzwd.c $(AK) $(stdio__h) $(gdebug_h)\
 $(slzwx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)slzwd.$(OBJ) $(C_) $(GLSRC)slzwd.c

# ---------------- MD5 digest filter ---------------- #

smd5_=$(GLOBJ)smd5.$(OBJ)
$(GLD)smd5.dev : $(LIB_MAK) $(ECHOGS_XE) $(smd5_) $(md5_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)smd5 $(smd5_) $(md5_)

$(GLOBJ)smd5.$(OBJ) : $(GLSRC)smd5.c $(AK) $(memory__h)\
 $(smd5_h) $(strimpl_h) $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)smd5.$(OBJ) $(C_) $(GLSRC)smd5.c

# -------------- SHA-256 digest filter -------------- #

ssha2_h=$(GLSRC)ssha2.h $(sha2_h) $(scommon_h)
ssha2_=$(GLOBJ)ssha2.$(OBJ)
$(GLD)ssha2.dev : $(LIB_MAK) $(ECHOGS_XE) $(ssha2_) $(sha2_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)ssha2 $(ssha2_) $(sha2_)

$(GLOBJ)ssha2.$(OBJ) : $(GLSRC)ssha2.c $(AK) $(memory__h)\
 $(strimpl_h) $(stream_h) $(ssha2_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ssha2.$(OBJ) $(C_) $(GLSRC)ssha2.c

# -------------- Arcfour cipher filter --------------- #

sarc4_=$(GLOBJ)sarc4.$(OBJ)
$(GLD)sarc4.dev : $(LIB_MAK) $(ECHOGS_XE) $(sarc4_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sarc4 $(sarc4_)

$(GLOBJ)sarc4.$(OBJ) : $(GLSRC)sarc4.c $(AK) $(memory__h)\
 $(gserrors_h) $(sarc4_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sarc4.$(OBJ) $(C_) $(GLSRC)sarc4.c

# -------------- AES cipher filter --------------- #

saes_=$(GLOBJ)saes.$(OBJ)
$(GLD)saes.dev : $(LIB_MAK) $(ECHOGS_XE) $(saes_) $(aes_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)saes $(saes_) $(aes_)

$(GLOBJ)saes.$(OBJ) : $(GLSRC)saes.c $(AK) $(memory__h)\
 $(gserrors_h) $(strimpl_h) $(saes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)saes.$(OBJ) $(C_) $(GLSRC)saes.c

# ---------------- JBIG2 compression filter ---------------- #

$(GLD)sjbig2.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLD)sjbig2_$(JBIG2_LIB).dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLD)sjbig2_$(JBIG2_LIB).dev $(GLD)sjbig2.dev

# jbig2dec version
sjbig2_jbig2dec=$(GLOBJ)sjbig2.$(OBJ)

$(GLD)sjbig2_jbig2dec.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(GLD)jbig2dec.dev $(sjbig2_jbig2dec) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sjbig2_jbig2dec $(sjbig2_jbig2dec)
	$(ADDMOD) $(GLD)sjbig2_jbig2dec -include $(GLD)jbig2dec.dev

$(GLD)sjbig2_.dev : $(LIB_MAK) $(ECHOGS_XE) \
  $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sjbig2_

# jbig2dec.dev is defined in jbig2.mak

$(GLOBJ)sjbig2.$(OBJ) : $(GLSRC)sjbig2.c $(AK) \
 $(stdint__h) $(memory__h) $(stdio__h) $(gserrors_h) $(gdebug_h) \
 $(sjbig2_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJBIG2CC) $(GLO_)sjbig2.$(OBJ) $(C_) $(GLSRC)sjbig2.c

$(GLOBJ)snojbig2.$(OBJ) : $(GLSRC)snojbig2.c $(AK) \
 $(stdint__h) $(memory__h) $(stdio__h) $(gserrors_h) $(gdebug_h) \
 $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJBIG2CC) $(GLO_)snojbig2.$(OBJ) $(C_) $(GLSRC)snojbig2.c

# luratech version
sjbig2_luratech=$(GLOBJ)sjbig2_luratech.$(OBJ)

$(GLD)sjbig2_luratech.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(GLD)ldf_jb2.dev $(sjbig2_luratech)
	$(SETMOD) $(GLD)sjbig2_luratech $(sjbig2_luratech)
	$(ADDMOD) $(GLD)sjbig2_luratech -include $(GLD)ldf_jb2.dev

# ldf_jb2.dev is defined in jbig2_luratech.mak

$(GLOBJ)sjbig2_luratech.$(OBJ) : $(GLSRC)sjbig2_luratech.c $(AK) \
 $(memory__h) $(malloc__h) $(gserrors_h) $(gdebug_h) \
 $(strimpl_h) $(sjbig2_luratech_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLDFJB2CC) $(GLO_)sjbig2_luratech.$(OBJ) \
		$(C_) $(GLSRC)sjbig2_luratech.c

# ---------------- JPEG 2000 compression filter ---------------- #

$(GLD)sjpx.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLD)sjpx_$(JPX_LIB).dev $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLD)sjpx_$(JPX_LIB).dev $(GLD)sjpx.dev

$(GLOBJ)sjpx.$(OBJ) : $(GLSRC)sjpx.c $(AK) \
 $(memory__h) $(gsmalloc_h) \
 $(gdebug_h) $(strimpl_h) $(sjpx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJASCC) $(GLO_)sjpx.$(OBJ) $(C_) $(GLSRC)sjpx.c

# luratech version
sjpx_luratech=$(GLOBJ)sjpx_luratech.$(OBJ)
$(GLD)sjpx_luratech.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(GLD)lwf_jp2.dev $(sjpx_luratech) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sjpx_luratech $(sjpx_luratech)
	$(ADDMOD) $(GLD)sjpx_luratech -include $(GLD)lwf_jp2.dev

$(GLD)luratech_jp2.dev : $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)luratech_jp2 $(GLD)liblwf_jp2.a

$(GLOBJ)sjpx_luratech.$(OBJ) : $(GLSRC)sjpx_luratech.c $(AK) \
 $(memory__h) $(gserrors_h) \
 $(gdebug_h) $(strimpl_h) $(sjpx_luratech_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLWFJPXCC) $(GLO_)sjpx_luratech.$(OBJ) \
		$(C_) $(GLSRC)sjpx_luratech.c

# openjpeg version
sjpx_openjpeg=$(GLOBJ)sjpx_openjpeg.$(OBJ)
$(GLD)sjpx_openjpeg.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(GLD)openjpeg.dev $(sjpx_openjpeg) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sjpx_openjpeg $(sjpx_openjpeg)
	$(ADDMOD) $(GLD)sjpx_openjpeg -include $(GLD)openjpeg.dev

$(GLOBJ)sjpx_openjpeg.$(OBJ) : $(GLSRC)sjpx_openjpeg.c $(AK) \
 $(memory__h) $(gserror_h) $(gserrors_h) \
 $(gdebug_h) $(strimpl_h) $(sjpx_openjpeg_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJPXOPJCC) $(GLO_)sjpx_openjpeg.$(OBJ) \
		$(C_) $(GLSRC)sjpx_openjpeg.c

# no jpx version
sjpx_none=$(GLOBJ)sjpx_none.$(OBJ)
$(GLD)sjpx_.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(sjpx_none) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sjpx_ $(sjpx_none)

$(GLOBJ)sjpx_none.$(OBJ) : $(GLSRC)sjpx_none.c $(AK) \
 $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJPXOPJCC) $(GLO_)sjpx_none.$(OBJ) $(C_) $(GLSRC)sjpx_none.c

# ---------------- Pixel-difference filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pdiff_=$(GLOBJ)spdiff.$(OBJ)
$(GLD)pdiff.dev : $(LIB_MAK) $(ECHOGS_XE) $(pdiff_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)pdiff $(pdiff_)

$(GLOBJ)spdiff.$(OBJ) : $(GLSRC)spdiff.c $(AK) $(memory__h) $(stdio__h)\
 $(spdiffx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)spdiff.$(OBJ) $(C_) $(GLSRC)spdiff.c

# ---------------- PNG pixel prediction filters ---------------- #
# The Predictor facility of the LZW and Flate filters uses these.

pngp_=$(GLOBJ)spngp.$(OBJ)
$(GLD)pngp.dev : $(LIB_MAK) $(ECHOGS_XE) $(pngp_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)pngp $(pngp_)

$(GLOBJ)spngp.$(OBJ) : $(GLSRC)spngp.c $(AK) $(memory__h)\
 $(spngpx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)spngp.$(OBJ) $(C_) $(GLSRC)spngp.c

# ---------------- RunLength filters ---------------- #
# These are used by clists and also by Level 2 in general.

rle_=$(GLOBJ)srle.$(OBJ)
$(GLD)rle.dev : $(LIB_MAK) $(ECHOGS_XE) $(rle_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)rle $(rle_)

$(GLOBJ)srle.$(OBJ) : $(GLSRC)srle.c $(AK) $(stdio__h) $(memory__h)\
 $(srlx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)srle.$(OBJ) $(C_) $(GLSRC)srle.c

rld_=$(GLOBJ)srld.$(OBJ)
$(GLD)rld.dev : $(LIB_MAK) $(ECHOGS_XE) $(rld_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)rld $(rld_)

$(GLOBJ)srld.$(OBJ) : $(GLSRC)srld.c $(AK) $(stdio__h) $(memory__h)\
 $(srlx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)srld.$(OBJ) $(C_) $(GLSRC)srld.c

# ---------------- PWG RunLength decode filter ---------------- #

pwgd_=$(GLOBJ)spwgd.$(OBJ)
$(GLD)pwgd.dev : $(LIB_MAK) $(ECHOGS_XE) $(pwgd_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)pwgd $(pwgd_)

$(GLOBJ)spwgd.$(OBJ) : $(GLSRC)spwgd.c $(AK) $(stdio__h) $(memory__h)\
 $(spwgx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)spwgd.$(OBJ) $(C_) $(GLSRC)spwgd.c

# ---------------- String encoding/decoding filters ---------------- #
# These are used by the PostScript and PDF writers, and also by the
# PostScript interpreter.

$(GLOBJ)sa85d.$(OBJ) : $(GLSRC)sa85d.c $(AK) $(std_h)\
 $(sa85d_h) $(scanchar_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sa85d.$(OBJ) $(C_) $(GLSRC)sa85d.c

$(GLOBJ)scantab.$(OBJ) : $(GLSRC)scantab.c $(AK) $(stdpre_h)\
 $(scanchar_h) $(scommon_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)scantab.$(OBJ) $(C_) $(GLSRC)scantab.c

$(GLOBJ)sfilter2.$(OBJ) : $(GLSRC)sfilter2.c $(AK) $(memory__h)\
 $(stdio__h) $(gdebug_h) $(sa85x_h) $(scanchar_h) $(sbtx_h) $(strimpl_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfilter2.$(OBJ) $(C_) $(GLSRC)sfilter2.c

$(GLOBJ)sfilter1.$(OBJ) : $(GLSRC)sfilter1.c $(AK) $(stdio__h) $(memory__h)\
 $(sfilter_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sfilter1.$(OBJ) $(C_) $(GLSRC)sfilter1.c

$(GLD)psfilters.dev : $(ECHOGS_XE) $(LIB_MAK) $(GLOBJ)sfilter2.$(OBJ)\
  $(GLOBJ)sfilter1.$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psfilters $(GLOBJ)sfilter1.$(OBJ) $(GLOBJ)sfilter2.$(OBJ)

$(GLOBJ)sstring.$(OBJ) : $(GLSRC)sstring.c $(AK)\
 $(stdio__h) $(memory__h) $(string__h)\
 $(scanchar_h) $(sstring_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sstring.$(OBJ) $(C_) $(GLSRC)sstring.c

$(GLOBJ)spprint.$(OBJ) : $(GLSRC)spprint.c $(AK)\
 $(math__h) $(stdio__h) $(string__h)\
 $(spprint_h) $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)spprint.$(OBJ) $(C_) $(GLSRC)spprint.c

$(GLOBJ)spsdf.$(OBJ) : $(GLSRC)spsdf.c $(AK) $(stdio__h) $(string__h)\
 $(gserrors_h) $(gsmemory_h) $(gstypes_h)\
 $(sa85x_h) $(scanchar_h) $(spprint_h) $(spsdf_h)\
 $(sstring_h) $(stream_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)spsdf.$(OBJ) $(C_) $(GLSRC)spsdf.c

# ---------------- zlib filters ---------------- #
# These are used by clists and are also available as filters.

szlibc_=$(GLOBJ)szlibc.$(OBJ)

zconf_h=$(ZSRCDIR)$(D)zconf.h
$(GLOBJ)szlibc_1.$(OBJ) : $(GLSRC)szlibc.c $(AK) $(std_h)\
 $(gserrors_h) $(gsmemory_h) \
 $(gsstruct_h) $(gstypes_h)\
 $(strimpl_h) $(szlibxx_h_1) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibc_1.$(OBJ) $(C_) $(GLSRC)szlibc.c

$(GLOBJ)szlibc_0.$(OBJ) : $(GLSRC)szlibc.c $(AK) $(std_h)\
 $(gserrors_h) $(gsmemory_h) $(zconf_h)\
 $(gsstruct_h) $(gstypes_h) $(zlib_h)\
 $(strimpl_h) $(szlibxx_h_0) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibc_0.$(OBJ) $(C_) $(GLSRC)szlibc.c

$(GLOBJ)szlibc.$(OBJ) : $(GLOBJ)szlibc_$(SHARE_ZLIB).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)szlibc_$(SHARE_ZLIB).$(OBJ) $(GLOBJ)szlibc.$(OBJ)

szlibe_=$(szlibc_) $(GLOBJ)szlibe.$(OBJ)
$(GLD)szlibe.dev : $(LIB_MAK) $(ECHOGS_XE) $(ZGENDIR)$(D)zlibe.dev $(szlibe_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)szlibe $(szlibe_)
	$(ADDMOD) $(GLD)szlibe -include $(ZGENDIR)$(D)zlibe.dev

$(GLOBJ)szlibe_1.$(OBJ) : $(GLSRC)szlibe.c $(AK) $(std_h)\
 $(strimpl_h) $(szlibxx_h_1) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibe_1.$(OBJ) $(C_) $(GLSRC)szlibe.c

$(GLOBJ)szlibe_0.$(OBJ) : $(GLSRC)szlibe.c $(AK) $(std_h)\
 $(strimpl_h) $(szlibxx_h_0) $(zlib_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibe_0.$(OBJ) $(C_) $(GLSRC)szlibe.c

$(GLOBJ)szlibe.$(OBJ) : $(GLOBJ)szlibe_$(SHARE_ZLIB).$(OBJ)  $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)szlibe_$(SHARE_ZLIB).$(OBJ) $(GLOBJ)szlibe.$(OBJ)

szlibd_=$(szlibc_) $(GLOBJ)szlibd.$(OBJ)
$(GLD)szlibd.dev : $(LIB_MAK) $(ECHOGS_XE) $(ZGENDIR)$(D)zlibd.dev $(szlibd_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)szlibd $(szlibd_)
	$(ADDMOD) $(GLD)szlibd -include $(ZGENDIR)$(D)zlibd.dev

$(GLOBJ)szlibd_1.$(OBJ) : $(GLSRC)szlibd.c $(AK) $(std_h) $(memory__h)\
 $(strimpl_h) $(szlibxx_h_1) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibd_1.$(OBJ) $(C_) $(GLSRC)szlibd.c

$(GLOBJ)szlibd_0.$(OBJ) : $(GLSRC)szlibd.c $(AK) $(std_h) $(memory__h)\
 $(strimpl_h) $(szlibxx_h_0) $(zlib_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLZCC) $(GLO_)szlibd_0.$(OBJ) $(C_) $(GLSRC)szlibd.c

$(GLOBJ)szlibd.$(OBJ) : $(GLOBJ)szlibd_$(SHARE_ZLIB).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)szlibd_$(SHARE_ZLIB).$(OBJ) $(GLOBJ)szlibd.$(OBJ)

# ---------------- Page devices ---------------- #
# We include this here, rather than in devs.mak, because it is more like
# a feature than a simple device.

gdevprn_h=$(GLSRC)gdevprn.h $(gxclpage_h) $(gxclist_h) $(string__h) $(gxdevmem_h) $(gxdevice_h) $(gxclthrd_h) $(gx_h) $(gxrplane_h) $(gsparam_h) $(gsmatrix_h) $(memory__h) $(gsutil_h) $(gserrors_h) $(gp_h)
gdevmplt_h=$(GLSRC)gdevmplt.h $(gxdevice_h)

page_=$(GLOBJ)gdevprn.$(OBJ) $(GLOBJ)gdevppla.$(OBJ) $(GLOBJ)gdevmplt.$(OBJ) $(GLOBJ)gdevflp.$(OBJ)\
 $(downscale_) $(GLOBJ)gdevoflt.$(OBJ) $(GLOBJ)gdevsclass.$(OBJ) $(GLOBJ)gdevepo.$(OBJ)

$(GLD)page.dev : $(LIB_MAK) $(ECHOGS_XE) $(page_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)page $(page_)
	$(ADDMOD) $(GLD)page -include $(GLD)clist

$(GLOBJ)gdevprn.$(OBJ) : $(GLSRC)gdevprn.c $(ctype__h)\
 $(gdevprn_h) $(gp_h) $(gsdevice_h) $(gsfname_h) $(gsparam_h)\
 $(gxclio_h) $(gxgetbit_h) $(gdevplnx_h) $(gstrans_h) $(gdevkrnlsclass_h) \
 $(gxdownscale_h) $(gdevdevn_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevprn.$(OBJ) $(C_) $(GLSRC)gdevprn.c

$(GLOBJ)gdevmplt.$(OBJ) : $(GLSRC)gdevmplt.c $(gdevmplt_h) $(std_h)
	$(GLCC) $(GLO_)gdevmplt.$(OBJ) $(C_) $(GLSRC)gdevmplt.c

$(GLOBJ)gdevflp.$(OBJ) : $(GLSRC)gdevflp.c $(gdevflp_h) $(std_h)
	$(GLCC) $(GLO_)gdevflp.$(OBJ) $(C_) $(GLSRC)gdevflp.c

$(GLOBJ)gdevepo.$(OBJ) : $(GLSRC)gdevepo.c $(gdevepo_h) $(std_h)
	$(GLCC) $(GLO_)gdevepo.$(OBJ) $(C_) $(GLSRC)gdevepo.c

$(GLOBJ)gdevoflt.$(OBJ) : $(GLSRC)gdevoflt.c $(gdevoflp_h) $(std_h)
	$(GLCC) $(GLO_)gdevoflt.$(OBJ) $(C_) $(GLSRC)gdevoflt.c

$(GLOBJ)gdevsclass.$(OBJ) : $(GLSRC)gdevsclass.c $(gdevsclass_h) $(std_h)
	$(GLCC) $(GLO_)gdevsclass.$(OBJ) $(C_) $(GLSRC)gdevsclass.c

$(GLOBJ)gdevkrnlsclass.$(OBJ) : $(GLSRC)gdevkrnlsclass.c $(gdevkrnlsclass_h) $(std_h)
	$(GLCC) $(GLO_)gdevkrnlsclass.$(OBJ) $(C_) $(GLSRC)gdevkrnlsclass.c

# Planar page devices
gdevppla_h=$(GLSRC)gdevppla.h $(gxdevbuf_h) $(gsdevice_h)

$(GLOBJ)gdevppla.$(OBJ) : $(GLSRC)gdevppla.c\
 $(gdevmpla_h) $(gdevppla_h) $(gdevprn_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevppla.$(OBJ) $(C_) $(GLSRC)gdevppla.c

# ---------------- Masked images ---------------- #
# This feature is out of level order because Patterns require it
# (which they shouldn't) and because band lists treat ImageType 4
# images as a special case (which they shouldn't).

gsiparm3_h=$(GLSRC)gsiparm3.h $(gsiparam_h)
gsiparm4_h=$(GLSRC)gsiparm4.h $(gsiparam_h)
gximage3_h=$(GLSRC)gximage3.h $(gsiparm3_h) $(gxiparam_h)

$(GLOBJ)gxclipm.$(OBJ) : $(GLSRC)gxclipm.c $(AK) $(gx_h) $(memory__h)\
 $(gsbittab_h) $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h) $(gxdcolor_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclipm.$(OBJ) $(C_) $(GLSRC)gxclipm.c

$(GLOBJ)gximage3.$(OBJ) : $(GLSRC)gximage3.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h)\
 $(gsbitops_h) $(gscspace_h) $(gsstruct_h)\
 $(gxclipm_h) $(gxdevice_h) $(gxdevmem_h) $(gximage3_h) $(gxgstate_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximage3.$(OBJ) $(C_) $(GLSRC)gximage3.c

$(GLOBJ)gximage4.$(OBJ) : $(GLSRC)gximage4.c $(memory__h) $(AK)\
 $(gx_h) $(gserrors_h)\
 $(gscspace_h) $(gsiparm4_h) $(gxiparam_h) $(gximage_h)\
 $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximage4.$(OBJ) $(C_) $(GLSRC)gximage4.c

imasklib_=$(GLOBJ)gxclipm.$(OBJ) $(GLOBJ)gximage3.$(OBJ) $(GLOBJ)gximage4.$(OBJ) $(GLOBJ)gxmclip.$(OBJ)
$(GLD)imasklib.dev : $(LIB_MAK) $(ECHOGS_XE) $(imasklib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)imasklib $(imasklib_)
	$(ADDMOD) $(GLD)imasklib -imagetype 3 4

# ---------------- Banded ("command list") devices ---------------- #

gxcldev_h=$(GLSRC)gxcldev.h $(gxclist_h) $(gxdht_h) $(srlx_h) $(gxht_h) $(strimpl_h) $(scfx_h) $(gsropt_h) $(gxtmap_h) $(gsdcolor_h)
gxclpage_h=$(GLSRC)gxclpage.h $(gxclist_h)
gxclpath_h=$(GLSRC)gxclpath.h $(gxcldev_h) $(gxdevcli_h)

clbase1_=$(GLOBJ)gxclist.$(OBJ) $(GLOBJ)gxclbits.$(OBJ) $(GLOBJ)gxclpage.$(OBJ)
clbase2_=$(GLOBJ)gxclrast.$(OBJ) $(GLOBJ)gxclread.$(OBJ) $(GLOBJ)gxclrect.$(OBJ)
clbase3_=$(GLOBJ)gxclutil.$(OBJ) $(GLOBJ)gsparams.$(OBJ) $(GLOBJ)gxshade6.$(OBJ)
# gxclrect.c requires rop_proc_table, so we need gsroptab here.
clbase4_=$(GLOBJ)gsroptab.$(OBJ) $(GLOBJ)gsroprun.$(OBJ) $(GLOBJ)stream.$(OBJ)
clpath_=$(GLOBJ)gxclimag.$(OBJ) $(GLOBJ)gxclpath.$(OBJ) $(GLOBJ)gxdhtserial.$(OBJ)
clthread_=$(GLOBJ)gxclthrd.$(OBJ) $(GLOBJ)gsmchunk.$(OBJ)
clist_=$(clbase1_) $(clbase2_) $(clbase3_) $(clbase4_) $(clpath_) $(clthread_)

# The old code selected one of clmemory, clfile depending on BAND_LIST_STORAGE.
# Now we meed clmemory to be included permanently for large patterns,
# and clfile to be included on demand.
# clfile works for page clist iff it is included.

$(GLD)clist.dev : $(LIB_MAK) $(ECHOGS_XE) $(clist_)\
 $(GLD)cl$(BAND_LIST_STORAGE).dev $(GLD)clmemory.dev $(GLD)$(SYNC).dev\
 $(GLD)cfe.dev $(GLD)cfd.dev $(GLD)rle.dev $(GLD)rld.dev $(GLD)psl2cs.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)clist $(clbase1_)
	$(ADDMOD) $(GLD)clist -obj $(clbase2_)
	$(ADDMOD) $(GLD)clist -obj $(clbase3_)
	$(ADDMOD) $(GLD)clist -obj $(clbase4_)
	$(ADDMOD) $(GLD)clist -obj $(clpath_)
	$(ADDMOD) $(GLD)clist -obj $(clthread_)
	$(ADDMOD) $(GLD)clist -include $(GLD)cl$(BAND_LIST_STORAGE)
	$(ADDMOD) $(GLD)clist -include $(GLD)clmemory $(GLD)$(SYNC).dev
	$(ADDMOD) $(GLD)clist -include $(GLD)cfe $(GLD)cfd $(GLD)rle $(GLD)rld $(GLD)psl2cs

$(GLOBJ)gxclist.$(OBJ) : $(GLSRC)gxclist.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gp_h) $(gpcheck_h) $(gsparams_h) $(valgrind_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) $(gxdcolor_h)\
 $(gscms_h) $(gsicc_manage_h) $(gsicc_cache_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclist.$(OBJ) $(C_) $(GLSRC)gxclist.c

$(GLOBJ)gxclbits.$(OBJ) : $(GLSRC)gxclbits.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gdevprn_h) $(gxpcolor_h)\
 $(gsbitops_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxfmap_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclbits.$(OBJ) $(C_) $(GLSRC)gxclbits.c

$(GLOBJ)gxclpage.$(OBJ) : $(GLSRC)gxclpage.c $(AK)\
 $(gdevprn_h) $(gdevdevn_h) $(gxcldev_h) $(gxclpage_h) $(gsicc_cache_h) $(string__h)\
 $(gsparams_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclpage.$(OBJ) $(C_) $(GLSRC)gxclpage.c

$(GLOBJ)gxclrast.$(OBJ) : $(GLSRC)gxclrast.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gp_h) $(gpcheck_h) $(gscoord_h)\
 $(gscdefs_h) $(gsbitops_h) $(gsparams_h) $(gsstate_h)\
 $(gstrans_h) $(gxdcolor_h) $(gxpcolor_h) $(gxdevice_h)\
 $(gsdevice_h) $(gsiparm4_h)\
 $(gxdevmem_h) $(gxcldev_h) $(gxclpath_h) $(gxcmap_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdhtres_h) $(gxgetbit_h)\
 $(gxpaint_h) $(gxhttile_h) $(gxiparam_h) $(gximask_h)\
 $(gzpath_h) $(gzcpath_h) $(gzacpath_h)\
 $(stream_h) $(strimpl_h) $(gxcomp_h)\
 $(gsserial_h) $(gxdhtserial_h) $(gzht_h)\
 $(gxshade_h) $(gxshade4_h) $(gsicc_manage_h)\
 $(gsicc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclrast.$(OBJ) $(C_) $(GLSRC)gxclrast.c

$(GLOBJ)gxclread.$(OBJ) : $(GLSRC)gxclread.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gp_h) $(gpcheck_h)\
 $(gdevplnx_h) $(gdevprn_h)\
 $(gscoord_h) $(gsdevice_h)\
 $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(gxhttile_h)\
 $(gsmemory_h) \
 $(stream_h) $(strimpl_h) $(gsicc_cache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclread.$(OBJ) $(C_) $(GLSRC)gxclread.c

$(GLOBJ)gxclrect.$(OBJ) : $(GLSRC)gxclrect.c $(AK) $(gx_h)\
 $(gserrors_h)\
 $(gsutil_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxclpath_h)\
 $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclrect.$(OBJ) $(C_) $(GLSRC)gxclrect.c

$(GLOBJ)gxclimag.$(OBJ) : $(GLSRC)gxclimag.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(string__h) $(gscdefs_h) $(gscspace_h)\
 $(gxarith_h) $(gxcldev_h) $(gxclpath_h) $(gxcspace_h) $(gxpcolor_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfmap_h) $(gxiparam_h) $(gxpath_h)\
 $(sisparam_h) $(stream_h) $(strimpl_h) $(gxcomp_h) $(gsserial_h)\
 $(gxdhtserial_h) $(gsptype1_h) $(gsicc_manage_h) $(gsicc_cache_h)\
 $(gxdevsop_h) $(gscindex_h) $(gsicc_cms_h) $(gximdecode_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclimag.$(OBJ) $(C_) $(GLSRC)gxclimag.c

$(GLOBJ)gxclpath.$(OBJ) : $(GLSRC)gxclpath.c $(AK) $(gx_h)\
 $(gserrors_h)\
 $(math__h) $(memory__h) $(gpcheck_h) $(gsptype2_h) $(gsptype1_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxcldev_h) $(gxclpath_h) $(gxcolor2_h)\
 $(gxdcolor_h) $(gxpaint_h) $(gxdevsop_h)\
 $(gzpath_h) $(gzcpath_h) $(stream_h) $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclpath.$(OBJ) $(C_) $(GLSRC)gxclpath.c

$(GLOBJ)gxdhtserial.$(OBJ) : $(GLSRC)gxdhtserial.c $(memory__h) $(AK)\
 $(gx_h) $(gserrors_h)\
 $(gscdefs_h) $(gsstruct_h) $(gsutil_h) $(gzstate_h) $(gxdevice_h) $(gzht_h)\
 $(gxdhtres_h) $(gsserial_h) $(gxdhtserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdhtserial.$(OBJ) $(C_) $(GLSRC)gxdhtserial.c

$(GLOBJ)gxclutil.$(OBJ) : $(GLSRC)gxclutil.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(string__h) $(gp_h) $(gpcheck_h) $(gsparams_h)\
 $(gxcldev_h) $(gxclpath_h) $(gxdevice_h) $(gxdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclutil.$(OBJ) $(C_) $(GLSRC)gxclutil.c

# Implement band lists on files.

clfile_=$(GLOBJ)gxclfile.$(OBJ)
$(GLD)clfile.dev : $(LIB_MAK) $(ECHOGS_XE) $(clfile_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)clfile $(clfile_)
	$(ADDMOD) $(GLD)clfile -init gxclfile

$(GLOBJ)gxclfile.$(OBJ) : $(GLSRC)gxclfile.c $(stdio__h) $(string__h)\
 $(gp_h) $(gsmemory_h) $(gserrors_h) $(gxclio_h) $(unistd__h) $(valgrind_h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclfile.$(OBJ) $(C_) $(GLSRC)gxclfile.c

# Implement band lists in memory (RAM).

clmemory_=$(GLOBJ)gxclmem.$(OBJ) $(GLOBJ)gxcl$(BAND_LIST_COMPRESSOR).$(OBJ)
$(GLD)clmemory.dev : $(LIB_MAK) $(ECHOGS_XE) $(clmemory_) $(GLD)s$(BAND_LIST_COMPRESSOR)e.dev \
  $(GLD)s$(BAND_LIST_COMPRESSOR)d.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)clmemory $(clmemory_)
	$(ADDMOD) $(GLD)clmemory -include $(GLD)s$(BAND_LIST_COMPRESSOR)e
	$(ADDMOD) $(GLD)clmemory -include $(GLD)s$(BAND_LIST_COMPRESSOR)d
	$(ADDMOD) $(GLD)clmemory -init gxclmem

gxclmem_h=$(GLSRC)gxclmem.h $(strimpl_h) $(gxclio_h)

$(GLOBJ)gxclmem.$(OBJ) : $(GLSRC)gxclmem.c $(AK) $(gx_h) $(gserrors_h)\
 $(LIB_MAK) $(memory__h) $(gxclmem_h) $(gssprintf_h) $(valgrind_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclmem.$(OBJ) $(C_) $(GLSRC)gxclmem.c

# Implement the compression method for RAM-based band lists.

$(GLOBJ)gxcllzw.$(OBJ) : $(GLSRC)gxcllzw.c $(std_h) $(AK)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(slzwx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxcllzw.$(OBJ) $(C_) $(GLSRC)gxcllzw.c

$(GLOBJ)gxclzlib.$(OBJ) : $(GLSRC)gxclzlib.c $(std_h) $(AK)\
 $(gsmemory_h) $(gstypes_h) $(gxclmem_h) $(szlibx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclzlib.$(OBJ) $(C_) $(GLSRC)gxclzlib.c

# Support for multi-threaded rendering from the clist. The chunk memory wrapper
# is used to prevent mutex (locking) contention among threads. The underlying
# memory allocator must implement the mutex (non-gc memory is usually gsmalloc)
$(GLOBJ)gxclthrd.$(OBJ) :  $(GLSRC)gxclthrd.c $(gxsync_h) $(AK)\
 $(gxclthrd_h) $(gdevplnx_h) $(gdevprn_h) $(gp_h)\
 $(gpcheck_h) $(gsdevice_h) $(gserrors_h) $(gsmchunk_h)\
 $(gsmemory_h) $(gx_h) $(gxcldev_h) $(gdevdevn_h) $(gsicc_cache_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(memory__h) $(gscms_h) $(gsicc_manage_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclthrd.$(OBJ) $(C_) $(GLSRC)gxclthrd.c

$(GLOBJ)gsmchunk.$(OBJ) :  $(GLSRC)gsmchunk.c $(AK) $(gx_h)\
 $(gsstype_h) $(gserrors_h) $(gsmchunk_h) $(memory__h) $(gxsync_h) $(malloc__h)\
 $(gsstruct_h) $(gxobj_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmchunk.$(OBJ) $(C_) $(GLSRC)gsmchunk.c

# ---------------- Vector devices ---------------- #
# We include this here for the same reasons as page.dev.

gdevvec_h=$(GLSRC)gdevvec.h $(gxgstate_h) $(gxdevice_h) $(gdevbbox_h) $(gxiparam_h) $(gsropt_h) $(stream_h) $(gxhldevc_h) $(gp_h)

vector_=$(GLOBJ)gdevvec.$(OBJ)
$(GLD)vector.dev : $(LIB_MAK) $(ECHOGS_XE) $(vector_)\
 $(GLD)bboxutil.dev $(GLD)sfile.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)vector $(vector_)
	$(ADDMOD) $(GLD)vector -include $(GLD)bboxutil $(GLD)sfile

$(GLOBJ)gdevvec.$(OBJ) : $(GLSRC)gdevvec.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(string__h) $(gdevkrnlsclass_h)\
 $(gdevvec_h) $(gp_h) $(gscspace_h) $(gxiparam_h) $(gsparam_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxfixed_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevvec.$(OBJ) $(C_) $(GLSRC)gdevvec.c

# ---------------- Image scaling filters ---------------- #

iscale_=$(GLOBJ)siinterp.$(OBJ) $(GLOBJ)siscale.$(OBJ) $(GLOBJ)sidscale.$(OBJ)
$(GLD)iscale.dev : $(LIB_MAK) $(ECHOGS_XE) $(iscale_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)iscale $(iscale_)

$(GLOBJ)siinterp.$(OBJ) : $(GLSRC)siinterp.c $(AK)\
 $(memory__h) $(gxdda_h) $(gxfixed_h) $(gxfrac_h)\
 $(siinterp_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)siinterp.$(OBJ) $(C_) $(GLSRC)siinterp.c

$(GLOBJ)siscale.$(OBJ) : $(GLSRC)siscale.c $(AK)\
 $(math__h) $(memory__h) $(stdio__h) $(stdint__h) $(gdebug_h) $(gxfrac_h)\
 $(siscale_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)siscale.$(OBJ) $(C_) $(GLSRC)siscale.c

$(GLOBJ)sidscale.$(OBJ) : $(GLSRC)sidscale.c $(AK)\
 $(math__h) $(memory__h) $(stdio__h) $(gdebug_h) $(gxdda_h) $(gxfixed_h)\
 $(sidscale_h) $(strimpl_h) $(gxfrac_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sidscale.$(OBJ) $(C_) $(GLSRC)sidscale.c

# -------------- imagemask scaling filter --------------- #

simscale_=$(GLOBJ)simscale.$(OBJ)
$(GLD)simscale.dev : $(LIB_MAK) $(ECHOGS_XE) $(simscale_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)simscale $(simscale_)

$(GLOBJ)simscale.$(OBJ) : $(GLSRC)simscale.c $(AK) $(memory__h)\
 $(gserrors_h) $(simscale_h) $(strimpl_h) $(sisparam_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)simscale.$(OBJ) $(C_) $(GLSRC)simscale.c

# ---------------- Extended halftone support ---------------- #
# This is only used by one non-PostScript-based project.

gshtx_h=$(GLSRC)gshtx.h $(gsht1_h) $(gxtmap_h) $(gscspace_h) $(gsmemory_h) $(gsgstate_h)

htxlib_=$(GLOBJ)gshtx.$(OBJ)
$(GLD)htxlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(htxlib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)htxlib $(htxlib_)

$(GLOBJ)gshtx.$(OBJ) : $(GLSRC)gshtx.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsstruct_h) $(gsutil_h)\
 $(gxfmap_h) $(gshtx_h) $(gzht_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gshtx.$(OBJ) $(C_) $(GLSRC)gshtx.c

# ----- Ternary raster operations and device level transparency ------#

roplib1_=$(GLOBJ)gdevdrop.$(OBJ) $(GLOBJ)gsroprun.$(OBJ)
roplib2_=$(GLOBJ)gdevmr1.$(OBJ) $(GLOBJ)gdevmr2n.$(OBJ) $(GLOBJ)gdevmr8n.$(OBJ)
roplib3_=$(GLOBJ)gdevrops.$(OBJ) $(GLOBJ)gsrop.$(OBJ) $(GLOBJ)gsroptab.$(OBJ)
roplib_=$(roplib1_) $(roplib2_) $(roplib3_)
$(GLD)roplib.dev : $(LIB_MAK) $(ECHOGS_XE) $(roplib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)roplib $(roplib1_)
	$(ADDMOD) $(GLD)roplib $(roplib2_)
	$(ADDMOD) $(GLD)roplib $(roplib3_)

$(GLOBJ)gdevdrop.$(OBJ) : $(GLSRC)gdevdrop.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gxdevsop_h)\
 $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gxgetbit_h) $(gdevmem_h) $(gdevmrop_h) $(gdevmpla_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdrop.$(OBJ) $(C_) $(GLSRC)gdevdrop.c

$(GLOBJ)gdevmr1.$(OBJ) : $(GLSRC)gdevmr1.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gdevmem_h) $(gdevmrop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmr1.$(OBJ) $(C_) $(GLSRC)gdevmr1.c

$(GLOBJ)gdevmr2n.$(OBJ) : $(GLSRC)gdevmr2n.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gdevmem_h) $(gdevmrop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmr2n.$(OBJ) $(C_) $(GLSRC)gdevmr2n.c

$(GLOBJ)gdevmr8n.$(OBJ) : $(GLSRC)gdevmr8n.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsbittab_h) $(gsropt_h)\
 $(gxcindex_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxdevrop_h)\
 $(gdevmem_h) $(gdevmrop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevmr8n.$(OBJ) $(C_) $(GLSRC)gdevmr8n.c

$(GLOBJ)gdevrops.$(OBJ) : $(GLSRC)gdevrops.c $(AK) $(gx_h)\
 $(gserrors_h) $(gxdcolor_h) $(gxdevice_h) $(gdevmrop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevrops.$(OBJ) $(C_) $(GLSRC)gdevrops.c

$(GLOBJ)gsrop.$(OBJ) : $(GLSRC)gsrop.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsrop_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsrop.$(OBJ) $(C_) $(GLSRC)gsrop.c

$(GLOBJ)gsroptab.$(OBJ) : $(GLSRC)gsroptab.c $(AK) $(stdpre_h)\
 $(gsropt_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsroptab.$(OBJ) $(C_) $(GLSRC)gsroptab.c

gsroprun1_h=$(GLSRC)gsroprun1.h
gsroprun8_h=$(GLSRC)gsroprun8.h
gsroprun24_h=$(GLSRC)gsroprun24.h
$(GLOBJ)gsroprun.$(OBJ) : $(GLSRC)gsroprun.c $(std_h) $(stdpre_h) $(gsropt_h)\
 $(gsroprun1_h) $(gsroprun8_h) $(gsroprun24_h) $(gp_h) $(arch_h) \
 $(gscindex_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsroprun.$(OBJ) $(C_) $(GLSRC)gsroprun.c

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=$(GLOBJ)gstype42.$(OBJ) $(GLOBJ)gxchrout.$(OBJ) \
 $(GLOBJ)ttcalc.$(OBJ) $(GLOBJ)ttfinp.$(OBJ) $(GLOBJ)ttfmain.$(OBJ) $(GLOBJ)ttfmemd.$(OBJ) \
 $(GLOBJ)ttinterp.$(OBJ) $(GLOBJ)ttload.$(OBJ) $(GLOBJ)ttobjs.$(OBJ) \
 $(GLOBJ)gxttfb.$(OBJ) $(GLOBJ)gzspotan.$(OBJ)

$(GLD)ttflib.dev : $(LIB_MAK) $(ECHOGS_XE) $(ttflib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)ttflib $(ttflib_)

# "gxfont42_h=$(GLSRC)gxfont42.h" already defined above
gxttf_h=$(GLSRC)gxttf.h $(stdpre_h)

$(GLOBJ)gstype42.$(OBJ) : $(GLSRC)gstype42.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h)\
 $(gsccode_h) $(gsline_h) $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxchrout_h) $(gxfixed_h) $(gxfont_h) $(gxfont42_h)\
 $(gxpath_h) $(gxttf_h) $(gxttfb_h) $(gxtext_h) $(gxchar_h) $(gxfcache_h)\
 $(gxgstate_h) $(gzstate_h) $(stream_h) $(stdint__h) \
 $(strimpl_h) $(stream_h) $(strmio_h) $(szlibx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstype42.$(OBJ) $(C_) $(GLSRC)gstype42.c

ttfsfnt_h=$(GLSRC)ttfsfnt.h $(tttypes_h) $(stdint__h)
ttcommon_h=$(GLSRC)ttcommon.h
ttconf_h=$(GLSRC)ttconf.h
ttfinp_h=$(GLSRC)ttfinp.h $(ttfoutl_h)
ttfmemd_h=$(GLSRC)ttfmemd.h $(gsstype_h)
tttype_h=$(GLSRC)tttype.h $(std_h)
ttconfig_h=$(GLSRC)ttconfig.h $(ttconf_h)
tttypes_h=$(GLSRC)tttypes.h $(ttconfig_h) $(tttype_h) $(std_h)
ttmisc_h=$(GLSRC)ttmisc.h $(string__h) $(gx_h) $(math__h) $(tttypes_h) $(std_h)
tttables_h=$(GLSRC)tttables.h $(tttypes_h)
ttobjs_h=$(GLSRC)ttobjs.h $(setjmp__h) $(ttcommon_h) $(tttables_h) $(tttypes_h) $(ttfoutl_h)
ttcalc_h=$(GLSRC)ttcalc.h $(ttcommon_h) $(tttypes_h)
ttinterp_h=$(GLSRC)ttinterp.h $(ttobjs_h) $(ttcommon_h)
ttload_h=$(GLSRC)ttload.h $(ttobjs_h)
gxhintn_h=$(GLSRC)gxhintn.h $(gspath_h) $(gxfixed_h) $(gsmatrix_h) $(stdint__h)

$(GLOBJ)ttcalc.$(OBJ) : $(GLSRC)ttcalc.c $(AK) $(ttmisc_h) $(ttcalc_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttcalc.$(OBJ) $(C_) $(GLSRC)ttcalc.c

$(GLOBJ)ttfinp.$(OBJ) : $(GLSRC)ttfinp.c $(AK) $(ttmisc_h)\
 $(ttfoutl_h) $(ttfsfnt_h) $(ttfinp_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttfinp.$(OBJ) $(C_) $(GLSRC)ttfinp.c

$(GLOBJ)ttfmain.$(OBJ) : $(GLSRC)ttfmain.c $(AK) $(ttmisc_h) $(ttfinp_h)\
 $(ttfoutl_h) $(ttfmemd_h) $(ttfsfnt_h) $(ttobjs_h) $(ttinterp_h) $(ttcalc_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttfmain.$(OBJ) $(C_) $(GLSRC)ttfmain.c

$(GLOBJ)ttfmemd.$(OBJ) : $(GLSRC)ttfmemd.c $(AK) $(ttmisc_h)\
 $(ttfmemd_h) $(ttfoutl_h) $(ttobjs_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttfmemd.$(OBJ) $(C_) $(GLSRC)ttfmemd.c

$(GLOBJ)ttinterp.$(OBJ) : $(GLSRC)ttinterp.c $(AK) $(ttmisc_h)\
 $(ttfoutl_h) $(tttypes_h) $(ttcalc_h) $(ttinterp_h) $(ttfinp_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttinterp.$(OBJ) $(C_) $(GLSRC)ttinterp.c

$(GLOBJ)ttload.$(OBJ) : $(GLSRC)ttload.c $(AK) $(ttmisc_h)\
 $(ttfoutl_h) $(tttypes_h) $(ttcalc_h) $(ttobjs_h) $(ttload_h) $(ttfinp_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttload.$(OBJ) $(C_) $(GLSRC)ttload.c

$(GLOBJ)ttobjs.$(OBJ) : $(GLSRC)ttobjs.c $(AK) $(ttmisc_h)\
 $(ttfoutl_h) $(ttobjs_h) $(ttcalc_h) $(ttload_h) $(ttinterp_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ttobjs.$(OBJ) $(C_) $(GLSRC)ttobjs.c

$(GLOBJ)gxttfb.$(OBJ) : $(GLSRC)gxttfb.c $(AK) $(gx_h) $(gserrors_h) \
 $(gxfixed_h) $(gxpath_h) $(gxfont_h) $(gxfont42_h) $(gxttfb_h) $(gxfcache_h)\
 $(gxmatrix_h) $(gxhintn_h) $(gzpath_h) $(ttfmemd_h)\
 $(gsstruct_h) $(gsfont_h) $(gdebug_h) $(memory__h) $(math__h)\
 $(gxgstate_h) $(gxpaint_h) $(gzspotan_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxttfb.$(OBJ) $(C_) $(GLSRC)gxttfb.c

$(GLOBJ)gzspotan.$(OBJ) : $(GLSRC)gzspotan.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsdevice_h) $(gzspotan_h) $(gxfixed_h) $(gxdevice_h)\
 $(gzpath_h) $(memory__h) $(math__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gzspotan.$(OBJ) $(C_) $(GLSRC)gzspotan.c


# -------- Composite (PostScript Type 0) font support -------- #

gxcid_h=$(GLSRC)gxcid.h $(gsstype_h)
gxfcid_h=$(GLSRC)gxfcid.h $(gxcid_h) $(gxfont42_h) $(gxfont_h) $(gsrefct_h)
gxfcmap_h=$(GLSRC)gxfcmap.h $(gxcid_h) $(gsfcmap_h) $(gsuid_h)
gxfcmap1_h=$(GLSRC)gxfcmap1.h $(gxfcmap_h)
gxfont0c_h=$(GLSRC)gxfont0c.h $(gxfcid_h) $(gxfont0_h)

cidlib_=$(GLOBJ)gsfcid.$(OBJ) $(GLOBJ)gsfcid2.$(OBJ)
# cidlib requires ttflib for CIDFontType 2 fonts.
$(GLD)cidlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(cidlib_) $(GLD)ttflib.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cidlib $(cidlib_)
	$(ADDMOD) $(GLD)cidlib -include $(GLD)ttflib

$(GLOBJ)gsfcid.$(OBJ) : $(GLSRC)gsfcid.c $(AK) $(gx_h) $(memory__h)\
 $(gsmatrix_h) $(gsstruct_h) $(gxfcid_h) $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfcid.$(OBJ) $(C_) $(GLSRC)gsfcid.c

$(GLOBJ)gsfcid2.$(OBJ) : $(GLSRC)gsfcid2.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsstruct_h) $(gsutil_h) $(gxfcid_h) $(gxfcmap_h) $(gxfont_h)\
 $(gxfont0c_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfcid2.$(OBJ) $(C_) $(GLSRC)gsfcid2.c

cmaplib_=$(GLOBJ)gsfcmap.$(OBJ) $(GLOBJ)gsfcmap1.$(OBJ)
$(GLD)cmaplib.dev : $(LIB_MAK) $(ECHOGS_XE) $(cmaplib_) $(GLD)cidlib.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cmaplib $(cmaplib_)
	$(ADDMOD) $(GLD)cmaplib -include $(GLD)cidlib

$(GLOBJ)gsfcmap.$(OBJ) : $(GLSRC)gsfcmap.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsstruct_h) $(gsutil_h) $(gxfcmap_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfcmap.$(OBJ) $(C_) $(GLSRC)gsfcmap.c

$(GLOBJ)gsfcmap1.$(OBJ) : $(GLSRC)gsfcmap1.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(string__h)\
 $(gsstruct_h) $(gsutil_h) $(gxfcmap1_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfcmap1.$(OBJ) $(C_) $(GLSRC)gsfcmap1.c

psf0lib_=$(GLOBJ)gschar0.$(OBJ) $(GLOBJ)gsfont0.$(OBJ)
$(GLD)psf0lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLD)cmaplib.dev $(psf0lib_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psf0lib $(psf0lib_)
	$(ADDMOD) $(GLD)psf0lib -include $(GLD)cmaplib

$(GLOBJ)gschar0.$(OBJ) : $(GLSRC)gschar0.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsfcmap_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxfcmap_h) $(gxfixed_h) $(gxfont_h) $(gxfont0_h)\
 $(gxfcid_h) $(gxtext_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gschar0.$(OBJ) $(C_) $(GLSRC)gschar0.c

$(GLOBJ)gsfont0.$(OBJ) : $(GLSRC)gsfont0.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsmatrix_h) $(gsstruct_h) $(gxfixed_h) $(gxdevmem_h)\
 $(gxfcache_h) $(gxfont_h) $(gxfont0_h) $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfont0.$(OBJ) $(C_) $(GLSRC)gsfont0.c

# gsfont0c is not needed for the PS interpreter, other than for testing,
# but it is used by pdfwrite and by the PCL interpreter.
$(GLOBJ)gsfont0c.$(OBJ) : $(GLSRC)gsfont0c.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gxfont_h) $(gxfont0_h) $(gxfont0c_h)\
 $(gxfcid_h) $(gxfcmap_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfont0c.$(OBJ) $(C_) $(GLSRC)gsfont0c.c

# ---------------- Pattern color ---------------- #

patlib_1=$(GLOBJ)gspcolor.$(OBJ) $(GLOBJ)gsptype1.$(OBJ) $(GLOBJ)gxclip2.$(OBJ)
patlib_2=$(GLOBJ)gxmclip.$(OBJ) $(GLOBJ)gxp1fill.$(OBJ) $(GLOBJ)gxpcmap.$(OBJ)
patlib_=$(patlib_1) $(patlib_2)
# Currently this feature requires masked images, but it shouldn't.
$(GLD)patlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLD)cmyklib.dev $(GLD)imasklib.dev $(GLD)psl2cs.dev $(patlib_)
	$(SETMOD) $(GLD)patlib -include $(GLD)cmyklib $(GLD)imasklib $(GLD)psl2cs
	$(ADDMOD) $(GLD)patlib -obj $(patlib_1)
	$(ADDMOD) $(GLD)patlib -obj $(patlib_2)

$(GLOBJ)gspcolor.$(OBJ) : $(GLSRC)gspcolor.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h)\
 $(gsimage_h) $(gsiparm4_h) $(gspath_h) $(gsrop_h) $(gsstruct_h) $(gsutil_h)\
 $(gxarith_h) $(gxcolor2_h) $(gxcoord_h) $(gxclip2_h) $(gxcspace_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxpath_h) $(gxpcolor_h) $(gzstate_h) $(stream_h) $(gsovrc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspcolor.$(OBJ) $(C_) $(GLSRC)gspcolor.c

$(GLOBJ)gsptype1.$(OBJ) : $(GLSRC)gsptype1.c $(AK)\
 $(math__h) $(memory__h) $(gx_h) $(gserrors_h) $(gxdevsop_h)\
 $(gsrop_h) $(gsstruct_h) $(gsutil_h)\
 $(gxarith_h)  $(gxfixed_h) $(gxmatrix_h) $(gxcoord_h) $(gxcspace_h)\
 $(gxcolor2_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxclip2_h)\
 $(gspath_h) $(gxpath_h) $(gxpcolor_h) $(gxp1impl_h) $(gxclist_h) $(gzstate_h)\
 $(gsimage_h) $(gsiparm4_h) $(gsovrc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsptype1.$(OBJ) $(C_) $(GLSRC)gsptype1.c

$(GLOBJ)gxclip2.$(OBJ) : $(GLSRC)gxclip2.c $(AK) $(gx_h) $(gpcheck_h)\
 $(gserrors_h) $(memory__h) $(gsstruct_h) $(gxclip2_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxdcolor_h) $(gdevmpla_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclip2.$(OBJ) $(C_) $(GLSRC)gxclip2.c

$(GLOBJ)gxp1fill.$(OBJ) : $(GLSRC)gxp1fill.c $(AK) $(gx_h)\
 $(gserrors_h) $(string__h) $(math__h) $(gsrop_h) $(gsmatrix_h)\
 $(gxcolor2_h) $(gxclip2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevcli_h)\
 $(gxdevmem_h) $(gxpcolor_h) $(gxp1impl_h) $(gxcldev_h) $(gxblend_h)\
 $(gsicc_cache_h) $(gxdevsop_h) $(gdevp14_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxp1fill.$(OBJ) $(C_) $(GLSRC)gxp1fill.c

$(GLOBJ)gxpcmap.$(OBJ) : $(GLSRC)gxpcmap.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gspath2_h) $(gxdevsop_h) $(gxp1impl_h)\
 $(gsstruct_h) $(gsutil_h) $(gp_h) \
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h) $(gxclist_h) $(gxcldev_h)\
 $(gzstate_h) $(gdevp14_h) $(gdevmpla_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpcmap.$(OBJ) $(C_) $(GLSRC)gxpcmap.c

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1lib_=$(GLOBJ)gxtype1.$(OBJ)\
 $(GLOBJ)gxhintn.$(OBJ) $(GLOBJ)gxhintn1.$(OBJ) $(GLOBJ)gscrypt1.$(OBJ) $(GLOBJ)gxchrout.$(OBJ)

gscrypt1_h=$(GLSRC)gscrypt1.h $(stdpre_h)
gstype1_h=$(GLSRC)gstype1.h $(gsgdata_h) $(gsgstate_h) $(gstypes_h)
gxfont1_h=$(GLSRC)gxfont1.h $(gxfixed_h) $(gxfont_h) $(gstype1_h) $(gxfapi_h)
gxtype1_h=$(GLSRC)gxtype1.h $(gxhintn_h) $(gxmatrix_h) $(gscrypt1_h) $(gstype1_h) $(gsgdata_h)

$(GLOBJ)gxtype1.$(OBJ) : $(GLSRC)gxtype1.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gsccode_h) $(gsline_h) $(gsstruct_h) $(memory__h)\
 $(gxarith_h) $(gxchrout_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxgstate_h) $(gxtype1_h)\
 $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxtype1.$(OBJ) $(C_) $(GLSRC)gxtype1.c

$(GLOBJ)gxhintn.$(OBJ) : $(GLSRC)gxhintn.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(math__h)\
 $(gxfixed_h) $(gxarith_h) $(gstypes_h) $(gxmatrix_h)\
 $(gxpath_h) $(gzpath_h) $(gxhintn_h) $(gxfont_h) $(gxfont1_h) $(gxtype1_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxhintn.$(OBJ) $(C_) $(GLSRC)gxhintn.c

$(GLOBJ)gxhintn1.$(OBJ) : $(GLSRC)gxhintn1.c $(AK) $(gx_h)\
 $(memory__h) $(math__h)\
 $(gxfixed_h) $(gxarith_h) $(gstypes_h) $(gxmatrix_h)\
 $(gzpath_h) $(gxhintn_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxhintn1.$(OBJ) $(C_) $(GLSRC)gxhintn1.c

# CharString and eexec encryption

# Note that seexec is not needed for rasterizing Type 1/2/4 fonts,
# only for reading or writing them.
seexec_=$(GLOBJ)seexec.$(OBJ) $(GLOBJ)gscrypt1.$(OBJ)
$(GLD)seexec.dev : $(LIB_MAK) $(ECHOGS_XE) $(seexec_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)seexec $(seexec_)

$(GLOBJ)seexec.$(OBJ) : $(GLSRC)seexec.c $(AK) $(stdio__h)\
 $(gscrypt1_h) $(scanchar_h) $(sfilter_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)seexec.$(OBJ) $(C_) $(GLSRC)seexec.c

$(GLOBJ)gscrypt1.$(OBJ) : $(GLSRC)gscrypt1.c $(AK) $(stdpre_h)\
 $(gscrypt1_h) $(gstypes_h) $(std_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscrypt1.$(OBJ) $(C_) $(GLSRC)gscrypt1.c

# Type 1 charstrings

psf1lib_=$(GLOBJ)gstype1.$(OBJ)
$(GLD)psf1lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(psf1lib_) $(type1lib_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psf1lib $(psf1lib_)
	$(ADDMOD) $(GLD)psf1lib $(type1lib_)

$(GLOBJ)gstype1.$(OBJ) : $(GLSRC)gstype1.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gsstruct_h) $(gxhintn_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxgstate_h) $(gxtype1_h)\
 $(gxpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstype1.$(OBJ) $(C_) $(GLSRC)gstype1.c

# Type 2 charstrings

psf2lib_=$(GLOBJ)gstype2.$(OBJ)
$(GLD)psf2lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(psf2lib_) $(type1lib_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psf2lib $(psf2lib_)
	$(ADDMOD) $(GLD)psf2lib $(type1lib_)

$(GLOBJ)gstype2.$(OBJ) : $(GLSRC)gstype2.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gsstruct_h)\
 $(gxarith_h) $(gxcoord_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gxfont_h) $(gxfont1_h) $(gxgstate_h) $(gxtype1_h)\
 $(gxpath_h) $(gxhintn_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstype2.$(OBJ) $(C_) $(GLSRC)gstype2.c

# -------- Level 1 color extensions (CMYK color and colorimage) -------- #

cmyklib_=$(GLOBJ)gscolor1.$(OBJ) $(GLOBJ)gsht1.$(OBJ)
$(GLD)cmyklib.dev : $(LIB_MAK) $(ECHOGS_XE) $(cmyklib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cmyklib $(cmyklib_)

$(GLOBJ)gscolor1.$(OBJ) : $(GLSRC)gscolor1.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsccolor_h) $(gscolor1_h) $(gsstruct_h) $(gsutil_h)\
 $(gscolor2_h) $(gxcmap_h) $(gxcspace_h) $(gxdcconv_h) $(gxdevice_h) $(gzht_h)\
 $(gzstate_h) $(gxhttype_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscolor1.$(OBJ) $(C_) $(GLSRC)gscolor1.c

$(GLOBJ)gsht1.$(OBJ) : $(GLSRC)gsht1.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsstruct_h) $(gsutil_h) $(gxdevice_h) $(gzht_h)\
 $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsht1.$(OBJ) $(C_) $(GLSRC)gsht1.c

colimlib_=$(GLOBJ)gxicolor.$(OBJ)
$(GLD)colimlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(colimlib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)colimlib $(colimlib_)
	$(ADDMOD) $(GLD)colimlib -imageclass 4_color

$(GLOBJ)gxicolor.$(OBJ) : $(GLSRC)gxicolor.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gxarith_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxmatrix_h)\
 $(gsccolor_h) $(gspaint_h) $(gzstate_h)\
 $(gxdevice_h) $(gxcmap_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxgstate_h) $(gxdevmem_h) $(gxcpath_h) $(gximage_h)\
 $(gsicc_h) $(gsicc_cache_h) $(gsicc_cms_h) $(gxcie_h)\
 $(gscie_h) $(gzht_h) $(gxht_thresh_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxicolor.$(OBJ) $(C_) $(GLSRC)gxicolor.c

# ---- Level 1 path miscellany (arcs, pathbbox, path enumeration) ---- #

path1lib_=$(GLOBJ)gspath1.$(OBJ)
$(GLD)path1lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(path1lib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)path1lib $(path1lib_)

$(GLOBJ)gspath1.$(OBJ) : $(GLSRC)gspath1.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gscoord_h) $(gspath_h) $(gsstruct_h)\
 $(gxfarith_h) $(gxfixed_h) $(gxmatrix_h) $(gzstate_h) $(gzpath_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspath1.$(OBJ) $(C_) $(GLSRC)gspath1.c

# --------------- Level 2 color space and color image support --------------- #

psl2cs_=$(GLOBJ)gscolor2.$(OBJ)
$(GLD)psl2cs.dev : $(LIB_MAK) $(ECHOGS_XE) $(psl2cs_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psl2cs $(psl2cs_)

$(GLOBJ)gscolor2.$(OBJ) : $(GLSRC)gscolor2.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h)\
 $(gxarith_h) $(gxfixed_h) $(gxmatrix_h) $(gxcspace_h)\
 $(gxcolor2_h) $(gzstate_h) $(gxpcolor_h) $(stream_h) $(gxcie_h)\
 $(gxfrac_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscolor2.$(OBJ) $(C_) $(GLSRC)gscolor2.c

$(GLD)psl2lib.dev : $(LIB_MAK) $(ECHOGS_XE) \
 $(GLD)colimlib.dev $(GLD)psl2cs.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psl2lib -include $(GLD)colimlib $(GLD)psl2cs
	$(ADDMOD) $(GLD)psl2lib -imageclass 2_fracs

$(GLOBJ)gxiscale.$(OBJ) : $(GLSRC)gxiscale.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(stdint__h) $(gpcheck_h)\
 $(gsccolor_h) $(gspaint_h) $(sidscale_h) $(gxdevsop_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h) $(gximage_h) $(gxgstate_h)\
 $(gxmatrix_h) $(siinterp_h) $(siscale_h) $(stream_h) \
 $(gscindex_h) $(gxcolor2_h) $(gscspace_h) $(gsicc_cache_h)\
 $(gsicc_manage_h) $(gsicc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxiscale.$(OBJ) $(C_) $(GLSRC)gxiscale.c

# ---------------- Display Postscript / Level 2 support ---------------- #

dps2lib_=$(GLOBJ)gsdps1.$(OBJ)
$(GLD)dps2lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(dps2lib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)dps2lib $(dps2lib_)

$(GLOBJ)gsdps1.$(OBJ) : $(GLSRC)gsdps1.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsmatrix_h) $(gscoord_h) $(gspaint_h) $(gxdevice_h) $(gsutil_h)\
 $(math__h) $(gxfixed_h) $(gspath_h) $(gspath2_h) $(gxhldevc_h)\
 $(gzpath_h) $(gzcpath_h) $(gzstate_h) $(gxmatrix_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsdps1.$(OBJ) $(C_) $(GLSRC)gsdps1.c

# ---------------- Functions ---------------- #
# These are used by PDF and by LanguageLevel 3.  They are here because
# the implementation of Separation colors also uses them.

gsdsrc_h=$(GLSRC)gsdsrc.h $(gsstruct_h)
gsfunc_h=$(GLSRC)gsfunc.h $(memento_h) $(gstypes_h) $(std_h)
gsfunc0_h=$(GLSRC)gsfunc0.h $(gsdsrc_h) $(gsfunc_h)
gxfunc_h=$(GLSRC)gxfunc.h $(gsstruct_h) $(gsfunc_h)

# Generic support, and FunctionType 0.
funclib_=$(GLOBJ)gsdsrc.$(OBJ) $(GLOBJ)gsfunc.$(OBJ) $(GLOBJ)gsfunc0.$(OBJ)
$(GLD)funclib.dev : $(LIB_MAK) $(ECHOGS_XE) $(funclib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)funclib $(funclib_)

$(GLOBJ)gsdsrc.$(OBJ) : $(GLSRC)gsdsrc.c $(AK) $(gx_h) $(memory__h)\
 $(gsdsrc_h) $(gserrors_h) $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsdsrc.$(OBJ) $(C_) $(GLSRC)gsdsrc.c

$(GLOBJ)gsfunc.$(OBJ) : $(GLSRC)gsfunc.c $(AK) $(gx_h) $(memory__h)\
 $(gserrors_h) $(gsparam_h) $(gxfunc_h) $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfunc.$(OBJ) $(C_) $(GLSRC)gsfunc.c

$(GLOBJ)gsfunc0.$(OBJ) : $(GLSRC)gsfunc0.c $(AK) $(gx_h) $(math__h)\
 $(gserrors_h) $(gsfunc0_h) $(gsparam_h) $(gxfarith_h) $(gxfunc_h) $(stream_h)\
 $(gscolor_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfunc0.$(OBJ) $(C_) $(GLSRC)gsfunc0.c

# FunctionType 4 may be is used for tintTransform and similar functions,
# in LanguageLevel 2 and above.

gsfunc4_h=$(GLSRC)gsfunc4.h $(gsfunc_h)

func4lib_=$(GLOBJ)gsfunc4.$(OBJ) $(GLOBJ)spprint.$(OBJ)
$(GLD)func4lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(func4lib_) $(GLD)funclib.dev \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)func4lib $(func4lib_)
	$(ADDMOD) $(GLD)func4lib -include $(GLD)funclib

$(GLOBJ)gsfunc4.$(OBJ) : $(GLSRC)gsfunc4.c $(AK) $(gx_h) $(math__h)\
 $(memory__h) $(gsdsrc_h) $(gserrors_h) $(gsfunc4_h)\
 $(gxfarith_h) $(gxfunc_h) $(stream_h)\
 $(sfilter_h) $(spprint_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfunc4.$(OBJ) $(C_) $(GLSRC)gsfunc4.c

# ---------------- DevicePixel color space ---------------- #

gscpixel_h=$(GLSRC)gscpixel.h $(gscspace_h)

cspixlib_=$(GLOBJ)gscpixel.$(OBJ)
$(GLD)cspixlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(cspixlib_) \
 $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cspixlib $(cspixlib_)

$(GLOBJ)gscpixel.$(OBJ) : $(GLSRC)gscpixel.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsrefct_h) $(gxcspace_h) $(gscpixel_h) $(gxdevice_h)\
 $(gxgstate_h) $(gsovrc_h) $(gsstate_h) $(gzstate_h) $(stream_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscpixel.$(OBJ) $(C_) $(GLSRC)gscpixel.c

# ---------------- CIE color ---------------- #

cielib1_=$(GLOBJ)gscie.$(OBJ) $(GLOBJ)gsciemap.$(OBJ) $(GLOBJ)gscscie.$(OBJ)
cielib2_=$(GLOBJ)gscrd.$(OBJ) $(GLOBJ)gscrdp.$(OBJ) $(GLOBJ)gxctable.$(OBJ)
cielib_=$(cielib1_) $(cielib2_)
$(GLD)cielib.dev : $(LIB_MAK) $(ECHOGS_XE) $(cielib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)cielib $(cielib1_)
	$(ADDMOD) $(GLD)cielib $(cielib2_)

$(GLOBJ)gscie.$(OBJ) : $(GLSRC)gscie.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcie_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)\
 $(gsicc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscie.$(OBJ) $(C_) $(GLSRC)gscie.c

$(GLOBJ)gsciemap.$(OBJ) : $(GLSRC)gsciemap.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h)\
 $(gxarith_h) $(gxcie_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) \
 $(gxgstate_h) $(gscolor2_h) $(gsicc_create_h) $(gsicc_manage_h)\
 $(gsicc_h) $(gscspace_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsciemap.$(OBJ) $(C_) $(GLSRC)gsciemap.c

$(GLOBJ)gscrd.$(OBJ) : $(GLSRC)gscrd.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(string__h)\
 $(gscdefs_h) $(gscolor2_h) $(gscrd_h) $(gsdevice_h)\
 $(gsmatrix_h) $(gsparam_h) $(gsstruct_h) $(gsutil_h) $(gxcspace_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscrd.$(OBJ) $(C_) $(GLSRC)gscrd.c

$(GLOBJ)gscrdp.$(OBJ) : $(GLSRC)gscrdp.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(string__h)\
 $(gscolor2_h) $(gscrdp_h) $(gsdevice_h) $(gsmatrix_h) $(gsstruct_h)\
 $(gxarith_h) $(gxcspace_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscrdp.$(OBJ) $(C_) $(GLSRC)gscrdp.c

$(GLOBJ)gscscie.$(OBJ) : $(GLSRC)gscscie.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gscolor2_h) $(gsmatrix_h) $(gsstruct_h) $(gsicc_manage_h)\
 $(gxarith_h) $(gxcie_h) $(gxcmap_h) $(gxcspace_h) $(gxdevice_h) $(gzstate_h)\
 $(stream_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscscie.$(OBJ) $(C_) $(GLSRC)gscscie.c

$(GLOBJ)gxctable.$(OBJ) : $(GLSRC)gxctable.c $(AK) $(gx_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxctable_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxctable.$(OBJ) $(C_) $(GLSRC)gxctable.c

# ---------------- ICCBased color ---------------- #

gsicc_=$(GLOBJ)gsicc_manage.$(OBJ) $(GLOBJ)gsicc_cache.$(OBJ)\
 $(GLOBJ)gsicc_$(WHICH_CMS).$(OBJ) $(GLOBJ)gsicc_profilecache.$(OBJ)\
 $(GLOBJ)gsicc_create.$(OBJ)  $(GLOBJ)gsicc_nocm.$(OBJ)\
 $(GLOBJ)gsicc_replacecm.$(OBJ) $(GLOBJ)gsicc_monitorcm.$(OBJ)

sicclib_=$(GLOBJ)gsicc.$(OBJ)
$(GLD)sicclib.dev : $(LIB_MAK) $(ECHOGS_XE) $(sicclib_) $(gsicc_) $(md5_)\
 $(GLD)cielib.dev $(LCMSGENDIR)$(D)$(WHICH_CMS).dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)sicclib $(sicclib_)
	$(ADDMOD) $(GLD)sicclib $(gsicc_)
	$(ADDMOD) $(GLD)sicclib $(md5_)
	$(ADDMOD) $(GLD)sicclib -include $(LCMSGENDIR)$(D)$(WHICH_CMS).dev

$(GLOBJ)gsicc.$(OBJ) : $(GLSRC)gsicc.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gsstruct_h) $(stream_h) $(gxcspace_h) $(gxarith_h)\
 $(gxcie_h) $(gzstate_h) $(gsicc_h) $(gsicc_cache_h) $(gsicc_cms_h)\
 $(gsicc_manage_h) $(gxdevice_h) $(gsccolor_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc.$(OBJ) $(C_) $(GLSRC)gsicc.c

gscms_h=$(GLSRC)gscms.h $(gxsync_h) $(gscspace_h) $(gsdevice_h) $(stdint__h) $(gstypes_h) $(std_h)
gsicc_cms_h=$(GLSRC)gsicc_cms.h $(gxcvalue_h) $(gscms_h)
gsicc_manage_h=$(GLSRC)gsicc_manage.h $(gsicc_cms_h)
gsicc_cache_h=$(GLSRC)gsicc_cache.h $(gxcvalue_h) $(gscms_h) $(gsgstate_h)
gsicc_profilecache_h=$(GLSRC)gsicc_profilecache.h $(gscms_h)

$(GLOBJ)gsicc_monitorcm.$(OBJ) : $(GLSRC)gsicc_monitorcm.c $(AK) $(std_h)\
 $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gxdevcli_h)\
 $(gxcspace_h) $(gsicc_cms_h) $(gxcvalue_h)\
 $(gsicc_cache_h) $(gxdevsop_h) $(gdevp14_h) $(string__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_monitorcm.$(OBJ) $(C_) $(GLSRC)gsicc_monitorcm.c

$(GLOBJ)gsicc_nocm.$(OBJ) : $(GLSRC)gsicc_nocm.c $(AK) $(std_h) $(gx_h)\
 $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h) $(strmio_h)\
 $(string__h) $(gxgstate_h) $(gxcspace_h) $(gsicc_cms_h)\
 $(gsicc_cache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_nocm.$(OBJ) $(C_) $(GLSRC)gsicc_nocm.c

$(GLOBJ)gsicc_replacecm.$(OBJ) : $(GLSRC)gsicc_replacecm.c $(AK) $(std_h) $(gx_h)\
 $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h) $(strmio_h)\
 $(string__h) $(gxgstate_h) $(gxcspace_h) $(gsicc_cms_h)\
 $(gsicc_cache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_replacecm.$(OBJ) $(C_) $(GLSRC)gsicc_replacecm.c

$(GLOBJ)gsicc_manage.$(OBJ) : $(GLSRC)gsicc_manage.c $(AK) $(gx_h)\
 $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h) $(strmio_h)\
 $(gxgstate_h) $(gxcspace_h) $(gscms_h) $(gsicc_manage_h) $(gsicc_cache_h)\
 $(gsicc_profilecache_h) $(gserrors_h) $(string__h) $(gxclist_h) $(gxcldev_h)\
 $(gzstate_h) $(gsicc_create_h) $(gpmisc_h) $(gxdevice_h) $(std_h)\
 $(gsicc_cms_h) $(gp_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_manage.$(OBJ) $(C_) $(GLSRC)gsicc_manage.c

$(GLOBJ)gsicc_cache.$(OBJ) : $(GLSRC)gsicc_cache.c $(AK) $(gx_h)\
 $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h) $(smd5_h)\
 $(gxgstate_h) $(gscms_h) $(gsicc_manage_h) $(gsicc_cache_h) $(gzstate_h)\
 $(gserrors_h) $(gsmalloc_h) $(string__h) $(gxsync_h) $(std_h) $(gsicc_cms_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_cache.$(OBJ) $(C_) $(GLSRC)gsicc_cache.c

$(GLOBJ)gsicc_profilecache.$(OBJ) : $(GLSRC)gsicc_profilecache.c $(AK)\
 $(std_h) $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h)\
 $(gscms_h) $(gsicc_profilecache_h) $(gzstate_h) $(gserrors_h) $(gx_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_profilecache.$(OBJ) $(C_) $(GLSRC)gsicc_profilecache.c

$(GLOBJ)gsicc_lcms2mt_1.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(gslibctx_h) $(gserrors_h) $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_lcms2mt_1.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c
$(GLOBJ)gsicc_lcms2mt_0.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(lcms2mt_h) $(gslibctx_h) $(lcms2mt_plugin_h) $(gserrors_h) \
 $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_lcms2mt_0.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c
$(GLOBJ)gsicc_lcms2mt.$(OBJ) : $(GLOBJ)gsicc_lcms2mt_$(SHARE_LCMS).$(OBJ) $(gp_h) \
 $(gxsync_h) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gsicc_lcms2mt_$(SHARE_LCMS).$(OBJ) $(GLOBJ)gsicc_lcms2mt.$(OBJ)
$(GLOBJ)gsicc_lcms2_1.$(OBJ) : $(GLSRC)gsicc_lcms2.c\
 $(memory__h) $(gsicc_cms_h) $(gslibctx_h) $(gserrors_h) $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2CC) $(GLO_)gsicc_lcms2_1.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2.c

$(GLOBJ)gsicc_lcms2_0.$(OBJ) : $(GLSRC)gsicc_lcms2.c\
 $(memory__h) $(gsicc_cms_h) $(lcms2_h) $(gslibctx_h) $(lcms2_plugin_h) $(gserrors_h) \
 $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2CC) $(GLO_)gsicc_lcms2_0.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2.c

$(GLOBJ)gsicc_lcms2.$(OBJ) : $(GLOBJ)gsicc_lcms2_$(SHARE_LCMS).$(OBJ) $(gp_h) \
 $(gxsync_h) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gsicc_lcms2_$(SHARE_LCMS).$(OBJ) $(GLOBJ)gsicc_lcms2.$(OBJ)

# Note that gsicc_create requires compile with lcms to obtain icc34.h
# header file that is used for creating ICC structures from PS objects.
# This is needed even if PDF/PS interpreter is built with a different CMS.
# This object is here instead of in psi since it is used lazily by the
# remap operations.
$(GLOBJ)gsicc_create_1.$(OBJ) : $(GLSRC)gsicc_create.c $(AK) $(string__h)\
 $(gsmemory_h) $(gx_h) $(gxgstate_h) $(gstypes_h) $(gscspace_h)\
 $(gscie_h) $(gsicc_create_h) $(gxarith_h) $(gsicc_manage_h) $(gsicc_cache_h)\
 $(math__h) $(gscolor2_h) $(gxcie_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_create_1.$(OBJ) $(C_) $(GLSRC)gsicc_create.c

$(GLOBJ)gsicc_create_0.$(OBJ) : $(GLSRC)gsicc_create.c $(AK) $(string__h)\
 $(gsmemory_h) $(gx_h) $(gxgstate_h) $(gstypes_h) $(gscspace_h)\
 $(gscie_h) $(gsicc_create_h) $(gxarith_h) $(gsicc_manage_h) $(gsicc_cache_h)\
 $(math__h) $(gscolor2_h) $(gxcie_h) $(icc34_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_create_0.$(OBJ) $(C_) $(GLSRC)gsicc_create.c

$(GLOBJ)gsicc_create.$(OBJ) : $(GLOBJ)gsicc_create_$(SHARE_LCMS).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gsicc_create_$(SHARE_LCMS).$(OBJ) $(GLOBJ)gsicc_create.$(OBJ)


#include "icc34.h"   /* Note this header is needed even if lcms is not compiled as default CMS */

# ---------------- Separation colors ---------------- #

seprlib_=$(GLOBJ)gscsepr.$(OBJ) $(GLOBJ)gsnamecl.$(OBJ) $(GLOBJ)gsncdummy.$(OBJ)
$(GLD)seprlib.dev : $(LIB_MAK) $(ECHOGS_XE) $(seprlib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)seprlib $(seprlib_)

$(GLOBJ)gscsepr.$(OBJ) : $(GLSRC)gscsepr.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsfunc_h) $(gsrefct_h) $(gsmatrix_h) $(gscsepr_h) $(gxcspace_h)\
 $(gxfixed_h) $(gxcolor2_h) $(gzstate_h) $(gscdevn_h) $(gxcdevn_h)\
 $(gxcmap_h) $(gxdevcli_h) $(gsovrc_h) $(stream_h) $(gsicc_cache_h) $(gxdevice_h)\
 $(gxcie_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscsepr.$(OBJ) $(C_) $(GLSRC)gscsepr.c

$(GLOBJ)gsnamecl.$(OBJ) : $(GLSRC)gsnamecl.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gscspace_h) $(math__h) $(stdpre_h) $(gsutil_h)\
 $(gscdefs_h) $(gxdevice_h) $(gsnamecl_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsnamecl.$(OBJ) $(C_) $(GLSRC)gsnamecl.c

$(GLOBJ)gsncdummy.$(OBJ) : $(GLSRC)gsncdummy.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(gxcspace_h) $(stdpre_h)\
 $(memory__h) $(gscdefs_h) $(gscspace_h) $(gscie_h) $(gsicc_h)\
 $(gxdevice_h) $(gzstate_h) $(gsutil_h) $(gxcie_h) $(gsncdummy_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsncdummy.$(OBJ) $(C_) $(GLSRC)gsncdummy.c

# ------------- Image Color Decode.  Used in color mon and xpswrite --------- #

$(GLOBJ)gximdecode.$(OBJ) : $(GLSRC)gximdecode.c $(gximdecode_h) $(string__h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximdecode.$(OBJ) $(C_) $(GLSRC)gximdecode.c

# ================ PostScript LanguageLevel 3 support ================ #

$(GLOBJ)gscdevn.$(OBJ) : $(GLSRC)gscdevn.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsicc_h)\
 $(gscdevn_h) $(gsfunc_h) $(gsmatrix_h) $(gsrefct_h) $(gsstruct_h)\
 $(gxcspace_h) $(gxcdevn_h) $(gxfarith_h) $(gxfrac_h) $(gsnamecl_h) $(gxcmap_h)\
 $(gxgstate_h) $(gscoord_h) $(gzstate_h) $(gxdevcli_h) $(gsovrc_h) $(stream_h)\
 $(gsicc_manage_h) $(gsicc_cache_h) $(gxdevice_h) $(gxcie_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscdevn.$(OBJ) $(C_) $(GLSRC)gscdevn.c

$(GLOBJ)gxdevndi.$(OBJ) : $(GLSRC)gxdevndi.c $(AK) $(gx_h)\
 $(gsstruct_h) $(gsdcolor_h) $(gxfrac_h)\
 $(gxcmap_h) $(gxdevice_h) $(gxdither_h) $(gxlum_h) $(gzht_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdevndi.$(OBJ) $(C_) $(GLSRC)gxdevndi.c

$(GLOBJ)gsclipsr.$(OBJ) : $(GLSRC)gsclipsr.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsclipsr_h) $(gsstruct_h) $(gxclipsr_h) $(gxfixed_h)\
 $(gxpath_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsclipsr.$(OBJ) $(C_) $(GLSRC)gsclipsr.c

psl3lib_=$(GLOBJ)gsclipsr.$(OBJ) $(GLOBJ)gscdevn.$(OBJ) $(GLOBJ)gxdevndi.$(OBJ)

$(GLD)psl3lib.dev : $(LIB_MAK) $(ECHOGS_XE) $(psl3lib_)\
 $(GLD)imasklib.dev $(GLD)shadelib.dev $(GLD)gxfapiu$(UFST_BRIDGE).dev \
  $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psl3lib $(psl3lib_)
	$(ADDMOD) $(GLD)psl3lib -include $(GLD)imasklib $(GLD)shadelib
	$(ADDMOD) $(GLD)psl3lib -include $(GLD)gxfapiu$(UFST_BRIDGE)

# ---------------- Trapping ---------------- #

gstrap_h=$(GLSRC)gstrap.h $(gsparam_h)

$(GLOBJ)gstrap.$(OBJ) : $(GLSRC)gstrap.c $(AK) $(gx_h) $(gserrors_h)\
 $(string__h) $(gsparamx_h) $(gstrap_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstrap.$(OBJ) $(C_) $(GLSRC)gstrap.c

traplib_=$(GLOBJ)gsparamx.$(OBJ) $(GLOBJ)gstrap.$(OBJ)
$(GLD)traplib.dev : $(LIB_MAK) $(ECHOGS_XE) $(traplib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)traplib $(traplib_)

### ------------------------ The DeviceN device ------------------------ ###
# Required by transparency

devn_=$(GLOBJ)gdevdevn.$(OBJ)

$(DD)spotcmyk.dev : $(LIB_MAK) $(devn_) $(GLD)page.dev $(GDEV) $(LIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)spotcmyk $(devn_)

$(DD)devicen.dev : $(LIB_MAK) $(devn_) $(GLD)page.dev $(GDEV) $(LIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)devicen $(devn_)

$(GLOBJ)gdevdevn.$(OBJ) : $(GLSRC)gdevdevn.c $(gx_h) $(math__h) $(string__h)\
 $(gdevprn_h) $(gsparam_h) $(gscrd_h) $(gscrdp_h) $(gxlum_h) $(gdevdcrd_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gsequivc_h) $(gdevp14_h)\
 $(gxblend_h) $(gdevdevnprn_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdevn.$(OBJ) $(C_) $(GLSRC)gdevdevn.c


# Provide a sample device CRD.
# Required by transparency
$(GLOBJ)gdevdcrd.$(OBJ) : $(GLSRC)gdevdcrd.c $(AK)\
 $(math__h) $(memory__h) $(string__h)\
 $(gscrd_h) $(gscrdp_h) $(gserrors_h) $(gsparam_h) $(gscspace_h)\
 $(gx_h) $(gxdevcli_h) $(gdevdcrd_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdcrd.$(OBJ) $(C_) $(GLSRC)gdevdcrd.c

$(GLOBJ)gsequivc.$(OBJ) : $(GLSRC)gsequivc.c $(math__h)\
 $(PDEVH) $(gsparam_h) $(gstypes_h) $(gxdconv_h) $(gdevdevn_h)\
 $(gsequivc_h) $(gzstate_h) $(gsstate_h) $(gscspace_h) $(gxcspace_h)\
 $(gsicc_manage_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsequivc.$(OBJ) $(C_) $(GLSRC)gsequivc.c


# ---------------- Transparency ---------------- #

gsipar3x_h=$(GLSRC)gsipar3x.h $(gsiparm3_h) $(gsiparam_h)
gximag3x_h=$(GLSRC)gximag3x.h $(gsipar3x_h) $(gxiparam_h)
gxblend_h=$(GLSRC)gxblend.h $(gxdevcli_h) $(gxcvalue_h) $(gxfrac_h) $(gxcindex_h)
gdevp14_h=$(GLSRC)gdevp14.h $(gxcolor2_h) $(gdevdevn_h) $(gxpcolor_h) $(gxdcolor_h) $(gxcmap_h) $(gsmatrix_h) $(gsgstate_h)

$(GLOBJ)gstrans.$(OBJ) : $(GLSRC)gstrans.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gdevp14_h) $(gstrans_h)\
 $(gsutil_h) $(gxdevcli_h) $(gzstate_h) $(gscspace_h)\
 $(gxclist_h) $(gsicc_manage_h) $(gdevdevn_h) $(gxarith_h) $(gxblend_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstrans.$(OBJ) $(C_) $(GLSRC)gstrans.c

$(GLOBJ)gximag3x.$(OBJ) : $(GLSRC)gximag3x.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gdevbbox_h)\
 $(gsbitops_h) $(gscpixel_h) $(gscspace_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gximag3x_h) $(gxgstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximag3x.$(OBJ) $(C_) $(GLSRC)gximag3x.c

$(GLOBJ)gxblend.$(OBJ) : $(GLSRC)gxblend.c $(AK) $(gx_h) $(memory__h)\
 $(gstparam_h) $(gxblend_h) $(gxcolor2_h) $(gsicc_cache_h) $(gsrect_h)\
 $(gsicc_manage_h) $(gdevp14_h) $(gp_h) $(math__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxblend.$(OBJ) $(C_) $(GLSRC)gxblend.c

$(GLOBJ)gxblend1.$(OBJ) : $(GLSRC)gxblend1.c $(AK) $(gx_h) $(memory__h)\
 $(gstparam_h) $(gsrect_h) $(gxdcconv_h) $(gxblend_h) $(gxdevcli_h)\
 $(gxgstate_h) $(gdevdevn_h) $(gdevp14_h) $(png__h) $(gp_h)\
 $(gsicc_cache_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxblend1.$(OBJ) $(C_) $(GLSRC)gxblend1.c

$(GLOBJ)gdevp14.$(OBJ) : $(GLSRC)gdevp14.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gscdefs_h) $(gxdevice_h) $(gsdevice_h)\
 $(gsstruct_h) $(gscoord_h) $(gxgstate_h) $(gxdcolor_h) $(gxiparam_h)\
 $(gstparam_h) $(gxblend_h) $(gxtext_h) $(gsimage_h)\
 $(gsrect_h) $(gzstate_h) $(gdevdevn_h) $(gdevp14_h) $(gdevprn_h) $(gsovrc_h) $(gxcmap_h)\
 $(gscolor1_h) $(gstrans_h) $(gsutil_h) $(gxcldev_h) $(gxclpath_h)\
 $(gxdcconv_h) $(gsptype2_h) $(gxpcolor_h)\
 $(gsptype1_h) $(gzcpath_h) $(gxpaint_h) $(gsicc_manage_h) $(gxclist_h)\
 $(gxiclass_h) $(gximage_h) $(gsmatrix_h) $(gsicc_cache_h) $(gxdevsop_h)\
 $(gsicc_h) $(gscms_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevp14.$(OBJ) $(C_) $(GLSRC)gdevp14.c

translib_=$(GLOBJ)gstrans.$(OBJ) $(GLOBJ)gximag3x.$(OBJ)\
 $(GLOBJ)gxblend.$(OBJ) $(GLOBJ)gxblend1.$(OBJ) $(GLOBJ)gdevp14.$(OBJ) $(GLOBJ)gdevdevn.$(OBJ)\
 $(GLOBJ)gsequivc.$(OBJ)  $(GLOBJ)gdevdcrd.$(OBJ)

$(GLD)translib.dev : $(LIB_MAK) $(ECHOGS_XE) $(translib_)\
 $(GLD)cspixlib.dev $(GLD)bboxutil.dev $(GLD)cielib.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)translib $(translib_)
	$(ADDMOD) $(GLD)translib -imagetype 3x
	$(ADDMOD) $(GLD)translib -include $(GLD)cspixlib $(GLD)bboxutil
	$(ADDMOD) $(GLD)translib -include $(GLD)cielib.dev

# ---------------- Smooth shading ---------------- #

gscolor3_h=$(GLSRC)gscolor3.h $(gsgstate_h)
gsfunc3_h=$(GLSRC)gsfunc3.h $(gsdsrc_h) $(gsfunc_h)
gsshade_h=$(GLSRC)gsshade.h $(gsdsrc_h) $(gxfixed_h) $(gscspace_h) $(gsfunc_h) $(gsmatrix_h) $(gsccolor_h)
gxshade_h=$(GLSRC)gxshade.h $(gxmatrix_h) $(gsshade_h) $(gxfixed_h) $(stream_h)
gxshade4_h=$(GLSRC)gxshade4.h $(gxshade_h) $(gxdevcli_h)

$(GLOBJ)gscolor3.$(OBJ) : $(GLSRC)gscolor3.c $(AK) $(gx_h)\
 $(gserrors_h) $(gscolor3_h) $(gsmatrix_h) $(gsptype2_h) $(gscie_h)\
 $(gxdevsop_h) $(gxcolor2_h) $(gxcspace_h) $(gxpaint_h) $(gxdcolor_h)\
 $(gxpcolor_h) $(gxshade_h) $(gzpath_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscolor3.$(OBJ) $(C_) $(GLSRC)gscolor3.c

$(GLOBJ)gsfunc3.$(OBJ) : $(GLSRC)gsfunc3.c $(AK) $(gx_h) $(gserrors_h)\
 $(gsfunc3_h) $(gsparam_h) $(gxfunc_h) $(stream_h) $(gxarith_h) $(math__h)\
 $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsfunc3.$(OBJ) $(C_) $(GLSRC)gsfunc3.c

$(GLOBJ)gsptype2.$(OBJ) : $(GLSRC)gsptype2.c $(AK) $(gx_h)\
 $(gserrors_h) $(gscspace_h) $(gsshade_h) $(gsmatrix_h) $(gsstate_h)\
 $(gxdevsop_h) $(gxcolor2_h) $(gxdcolor_h) $(gsptype2_h) $(gxpcolor_h)\
 $(gxstate_h) $(gzpath_h) $(gzcpath_h) $(gzstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsptype2.$(OBJ) $(C_) $(GLSRC)gsptype2.c

$(GLOBJ)gsshade.$(OBJ) : $(GLSRC)gsshade.c $(AK) $(gx_h) $(gserrors_h)\
 $(gscspace_h) $(gsstruct_h) $(gsptype2_h)\
 $(gxcspace_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevcli_h) $(gxgstate_h)\
 $(gxpaint_h) $(gxpath_h) $(gxshade_h) $(gxshade4_h)\
 $(gzcpath_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsshade.$(OBJ) $(C_) $(GLSRC)gsshade.c

$(GLOBJ)gxshade.$(OBJ) : $(GLSRC)gxshade.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gsrect_h) $(gxcspace_h) $(gscindex_h) $(gscie_h) \
 $(gxdevcli_h) $(gxgstate_h) $(gxdht_h) $(gxpaint_h) $(gxshade_h) $(gxshade4_h)\
 $(gsicc_h) $(gsicc_cache_h) $(gxcdevn_h) $(gximage_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxshade.$(OBJ) $(C_) $(GLSRC)gxshade.c

$(GLOBJ)gxshade1.$(OBJ) : $(GLSRC)gxshade1.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) \
 $(gscoord_h) $(gsmatrix_h) $(gspath_h) $(gsptype2_h)\
 $(gxcspace_h) $(gxdcolor_h) $(gxfarith_h) $(gxfixed_h) $(gxgstate_h)\
 $(gxpath_h) $(gxshade_h) $(gxshade4_h) $(gxdevcli_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxshade1.$(OBJ) $(C_) $(GLSRC)gxshade1.c

$(GLOBJ)gxshade4.$(OBJ) : $(GLSRC)gxshade4.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h)\
 $(gscoord_h) $(gsmatrix_h) $(gsptype2_h)\
 $(gxcspace_h) $(gxdcolor_h) $(gxdevcli_h) $(gxgstate_h) $(gxpath_h)\
 $(gxshade_h) $(gxshade4_h) $(gsicc_cache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxshade4.$(OBJ) $(C_) $(GLSRC)gxshade4.c

$(GLOBJ)gxshade6.$(OBJ) : $(GLSRC)gxshade6.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gxdevsop_h) $(stdint__h) $(gscoord_h)\
 $(gscicach_h) $(gsmatrix_h) $(gxcspace_h) $(gxdcolor_h) $(gxgstate_h)\
 $(gxshade_h) $(gxshade4_h) $(gxdevcli_h) $(gxarith_h) $(gzpath_h) $(math__h)\
 $(gsicc_cache_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxshade6.$(OBJ) $(C_) $(GLSRC)gxshade6.c

shadelib_1=$(GLOBJ)gscolor3.$(OBJ) $(GLOBJ)gsfunc3.$(OBJ) $(GLOBJ)gsptype2.$(OBJ) $(GLOBJ)gsshade.$(OBJ)
shadelib_2=$(GLOBJ)gxshade.$(OBJ) $(GLOBJ)gxshade1.$(OBJ) $(GLOBJ)gxshade4.$(OBJ) $(GLOBJ)gxshade6.$(OBJ)
shadelib_=$(shadelib_1) $(shadelib_2)
$(GLD)shadelib.dev : $(LIB_MAK) $(ECHOGS_XE) $(shadelib_)\
 $(GLD)funclib.dev $(GLD)patlib.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)shadelib $(shadelib_1)
	$(ADDMOD) $(GLD)shadelib -obj $(shadelib_2)
	$(ADDMOD) $(GLD)shadelib -include $(GLD)funclib $(GLD)patlib

$(GLOBJ)gen_ordered.$(OBJ) : $(GLSRC)gen_ordered.c $(GLSRC)gen_ordered.h\
 $(std_h) $(gsmemory_h)  $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gen_ordered.$(OBJ) $(C_)  $(D_)GS_LIB_BUILD$(_D) \
        $(GLSRC)gen_ordered.c

# ---------------- Support for %rom% IODevice ----------------- #
# This is used to access compressed, compiled-in support files
gsiorom_h=$(GLSRC)gsiorom.h
romfs_=$(GLOBJ)gsiorom.$(OBJ)
$(GLD)romfs1.dev : $(LIB_MAK) $(ECHOGS_XE) $(romfs_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)romfs1 $(romfs_)
	$(ADDMOD) $(GLD)romfs1 -iodev rom

# A dummy romfs when we aren't using COMPILE_INITS
$(GLD)romfs0.dev :  $(LIB_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)romfs0
# psi
$(GLGEN)gsromfs1_.c : $(MKROMFS_XE) $(PS_ROMFS_DEPS) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)gsromfs1_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PS_ROMFS_ARGS) $(PS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)gsromfs1_1.c : $(MKROMFS_XE) $(PS_ROMFS_DEPS) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)gsromfs1_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PS_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)gsromfs1.c : $(GLGEN)gsromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)gsromfs1_$(UFST_BRIDGE).c $(GLGEN)gsromfs1.c

# pcl
$(GLGEN)pclromfs1_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs1_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PCLXL_FONT_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
        $(PJL_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)pclromfs1_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs1_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
	$(GL_ROMFS_ARGS)

$(GLGEN)pclromfs1.c : $(GLGEN)pclromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pclromfs1_$(UFST_BRIDGE).c $(GLGEN)pclromfs1.c

$(GLGEN)pclromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs0_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)pclromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs0_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)pclromfs0.c : $(GLGEN)pclromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pclromfs0_$(UFST_BRIDGE).c $(GLGEN)pclromfs0.c

# xps
$(GLGEN)xpsromfs1_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs1_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(XPS_ROMFS_ARGS) $(XPS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)xpsromfs1_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs1_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(XPS_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)xpsromfs1.c : $(GLGEN)xpsromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)xpsromfs1_$(UFST_BRIDGE).c $(GLGEN)xpsromfs1.c

$(GLGEN)xpsromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs0_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)xpsromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs0_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)xpsromfs0.c : $(GLGEN)xpsromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)xpsromfs0_$(UFST_BRIDGE).c $(GLGEN)xpsromfs0.c

# pdl
# We generate the pdl romfs in index + 4 lumps because of size

# COMPILE_INITS + Non UFST variant
$(GLGEN)pdlromfs1_c0.c : $(GLGEN)pdlromfs1_.c
	$(NO_OP)

$(GLGEN)pdlromfs1_c1.c : $(GLGEN)pdlromfs1_.c
	$(NO_OP)

$(GLGEN)pdlromfs1_c2.c : $(GLGEN)pdlromfs1_.c
	$(NO_OP)

$(GLGEN)pdlromfs1_c3.c : $(GLGEN)pdlromfs1_.c
	$(NO_OP)

$(GLGEN)pdlromfs1_.c: $(MKROMFS_XE) $(PS_ROMFS_DEPS) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdlromfs1_.c -s 4 \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PCLXL_ROMFS_ARGS) $(PCLXL_FONT_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
        $(XPS_ROMFS_ARGS) $(XPS_FONT_ROMFS_ARGS) \
	$(PS_ROMFS_ARGS) $(PS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS)

# COMPILE_INITS + UFST variant
$(GLGEN)pdlromfs1_1c0.c : $(GLGEN)pdlromfs1_1.c
	$(NO_OP)

$(GLGEN)pdlromfs1_1c1.c : $(GLGEN)pdlromfs1_1.c
	$(NO_OP)

$(GLGEN)pdlromfs1_1c2.c : $(GLGEN)pdlromfs1_1.c
	$(NO_OP)

$(GLGEN)pdlromfs1_1c3.c : $(GLGEN)pdlromfs1_1.c
	$(NO_OP)

$(GLGEN)pdlromfs1_1.c: $(MKROMFS_XE) $(PS_ROMFS_DEPS) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdlromfs1_1.c -s 4 \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) $(XPS_ROMFS_ARGS) \
	$(PS_ROMFS_ARGS) $(GL_ROMFS_ARGS)

# Rules to fold COMPILE_INITS +/- UFST into 1 set of targets
$(GLGEN)pdlromfs1c0.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c0.c
	$(CP_) $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c0.c $(GLGEN)pdlromfs1c0.c

$(GLGEN)pdlromfs1c1.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c1.c
	$(CP_) $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c1.c $(GLGEN)pdlromfs1c1.c

$(GLGEN)pdlromfs1c2.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c2.c
	$(CP_) $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c2.c $(GLGEN)pdlromfs1c2.c

$(GLGEN)pdlromfs1c3.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c3.c
	$(CP_) $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c3.c $(GLGEN)pdlromfs1c3.c

$(GLGEN)pdlromfs1.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pdlromfs1_$(UFST_BRIDGE).c $(GLGEN)pdlromfs1.c

# Non COMPILE_INITS + Non UFST variant
$(GLGEN)pdlromfs0_c0.c : $(GLGEN)pdlromfs0_.c
	$(NO_OP)

$(GLGEN)pdlromfs0_c1.c : $(GLGEN)pdlromfs0_.c
	$(NO_OP)

$(GLGEN)pdlromfs0_c2.c : $(GLGEN)pdlromfs0_.c
	$(NO_OP)

$(GLGEN)pdlromfs0_c3.c : $(GLGEN)pdlromfs0_.c
	$(NO_OP)

$(GLGEN)pdlromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdlromfs0_.c -s 4 \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

# Non COMPILE_INITS + UFST variant
$(GLGEN)pdlromfs0_1c0.c : $(GLGEN)pdlromfs0_1.c
	$(NO_OP)

$(GLGEN)pdlromfs0_1c1.c : $(GLGEN)pdlromfs0_1.c
	$(NO_OP)

$(GLGEN)pdlromfs0_1c2.c : $(GLGEN)pdlromfs0_1.c
	$(NO_OP)

$(GLGEN)pdlromfs0_1c3.c : $(GLGEN)pdlromfs0_1.c
	$(NO_OP)

$(GLGEN)pdlromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdlromfs0_1.c -s 4 \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

# Rules to fold Non COMPILE_INITS +/- UFST into 1 set of targets
$(GLGEN)pdlromfs0c0.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c0.c
	$(CP_) $(GLGEN)pdlromfs0_$(UFST_BRIDGE)c0.c $(GLGEN)pdlromfs0c0.c

$(GLGEN)pdlromfs0c1.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c1.c
	$(CP_) $(GLGEN)pdlromfs0_$(UFST_BRIDGE)c1.c $(GLGEN)pdlromfs0c1.c

$(GLGEN)pdlromfs0c2.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c2.c
	$(CP_) $(GLGEN)pdlromfs0_$(UFST_BRIDGE)c2.c $(GLGEN)pdlromfs0c2.c

$(GLGEN)pdlromfs0c3.c : $(GLGEN)pdlromfs1_$(UFST_BRIDGE)c3.c
	$(CP_) $(GLGEN)pdlromfs0_$(UFST_BRIDGE)c3.c $(GLGEN)pdlromfs0c3.c

$(GLGEN)pdlromfs0.c : $(GLGEN)pdlromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pdlromfs0_$(UFST_BRIDGE).c $(GLGEN)pdlromfs0.c


# the following module is only included if the romfs.dev FEATURE is enabled
$(GLOBJ)gsiorom_1.$(OBJ) : $(GLSRC)gsiorom.c $(gsiorom_h) \
 $(std_h) $(gx_h) $(gserrors_h) $(gsstruct_h) $(gxiodev_h) $(stat__h)\
 $(gpcheck_h) $(gsutil_h) $(stdint__h) $(stream_h) $(string__h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsiorom_1.$(OBJ) $(I_)$(ZI_)$(_I) $(C_) $(GLSRC)gsiorom.c

$(GLOBJ)gsiorom_0.$(OBJ) : $(GLSRC)gsiorom.c $(gsiorom_h) \
 $(std_h) $(gx_h) $(gserrors_h) $(gsstruct_h) $(gxiodev_h) $(stat__h)\
 $(gpcheck_h) $(gsutil_h) $(stdint__h) $(stream_h) $(string__h) $(zlib_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsiorom_0.$(OBJ) $(I_)$(ZI_)$(_I) $(C_) $(GLSRC)gsiorom.c

$(GLOBJ)gsiorom.$(OBJ) : $(GLOBJ)gsiorom_$(SHARE_ZLIB).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gsiorom_$(SHARE_ZLIB).$(OBJ) $(GLOBJ)gsiorom.$(OBJ)

# A dummy gsromfs module for COMPILE_INITS=0
$(GLOBJ)gsromfs0.$(OBJ) : $(GLSRC)gsromfs0.c $(stdint__h) $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsromfs0.$(OBJ) $(C_) $(GLSRC)gsromfs0.c

$(GLOBJ)gsromfs1.$(OBJ) : $(GLOBJ)gsromfs1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsromfs1.$(OBJ) $(C_) $(GLOBJ)gsromfs1.c

# A pclromfs module with only ICC profiles for COMPILE_INITS=0
$(GLOBJ)pclromfs0.$(OBJ) : $(GLGEN)pclromfs0.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pclromfs0.$(OBJ) $(C_) $(GLGEN)pclromfs0.c

$(GLOBJ)pclromfs1.$(OBJ) : $(GLOBJ)pclromfs1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pclromfs1.$(OBJ) $(C_) $(GLOBJ)pclromfs1.c

# A xpsromfs module with only ICC profiles for COMPILE_INITS=0
$(GLOBJ)xpsromfs0.$(OBJ) : $(GLGEN)xpsromfs0.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)xpsromfs0.$(OBJ) $(C_) $(GLGEN)xpsromfs0.c

$(GLOBJ)xpsromfs1.$(OBJ) : $(GLOBJ)xpsromfs1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)xpsromfs1.$(OBJ) $(C_) $(GLOBJ)xpsromfs1.c

# A pdlromfs module with only ICC profiles for COMPILE_INITS=0
$(GLOBJ)pdlromfs0.$(OBJ) : $(GLGEN)pdlromfs0.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs0.$(OBJ) $(C_) $(GLGEN)pdlromfs0.c

$(GLOBJ)pdlromfs0c0.$(OBJ) : $(GLGEN)pdlromfs0c0.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs0c0.$(OBJ) $(C_) $(GLGEN)pdlromfs0c0.c

$(GLOBJ)pdlromfs0c1.$(OBJ) : $(GLGEN)pdlromfs0c1.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs0c1.$(OBJ) $(C_) $(GLGEN)pdlromfs0c1.c

$(GLOBJ)pdlromfs0c2.$(OBJ) : $(GLGEN)pdlromfs0c2.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs0c2.$(OBJ) $(C_) $(GLGEN)pdlromfs0c2.c

$(GLOBJ)pdlromfs0c3.$(OBJ) : $(GLGEN)pdlromfs0c3.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs0c3.$(OBJ) $(C_) $(GLGEN)pdlromfs0c3.c

$(GLOBJ)pdlromfs1.$(OBJ) : $(GLOBJ)pdlromfs1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs1.$(OBJ) $(C_) $(GLOBJ)pdlromfs1.c

$(GLOBJ)pdlromfs1c0.$(OBJ) : $(GLOBJ)pdlromfs1c0.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs1c0.$(OBJ) $(C_) $(GLOBJ)pdlromfs1c0.c

$(GLOBJ)pdlromfs1c1.$(OBJ) : $(GLOBJ)pdlromfs1c1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs1c1.$(OBJ) $(C_) $(GLOBJ)pdlromfs1c1.c

$(GLOBJ)pdlromfs1c2.$(OBJ) : $(GLOBJ)pdlromfs1c2.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs1c2.$(OBJ) $(C_) $(GLOBJ)pdlromfs1c2.c

$(GLOBJ)pdlromfs1c3.$(OBJ) : $(GLOBJ)pdlromfs1c3.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdlromfs1c3.$(OBJ) $(C_) $(GLOBJ)pdlromfs1c3.c

# Define the ZLIB modules needed by mnkromfs here to factor it out of top makefiles
# Also put the .h dependencies here for the same reason
MKROMFS_ZLIB_OBJS=$(AUX)compress.$(OBJ) $(AUX)deflate.$(OBJ) \
	$(AUX)zutil.$(OBJ) $(AUX)adler32.$(OBJ) $(AUX)crc32.$(OBJ) \
	$(AUX)trees.$(OBJ)

MKROMFS_COMMON_DEPS=$(stdpre_h) $(stdint__h) $(gsiorom_h) $(arch_h)\
	$(gsmemret_h) $(gsmalloc_h) $(gsstype_h) $(gp_h) $(time__h)

# ---------------- Support for %ram% IODevice ----------------- #
gsioram_h=$(GLSRC)gsioram.h
ramfs_=$(GLOBJ)gsioram.$(OBJ) $(GLOBJ)ramfs.$(OBJ)
$(GLD)ramfs.dev : $(LIB_MAK) $(ECHOGS_XE) $(ramfs_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)ramfs $(ramfs_)
	$(ADDMOD) $(GLD)ramfs -iodev ram
	$(ADDMOD) $(GLD)ramfs -obj $(GLOBJ)ramfs.$(OBJ)

$(GLOBJ)ramfs.$(OBJ) : $(GLSRC)ramfs.c $(gp_h) $(gscdefs_h) $(gserrors_h)\
  $(gsparam_h) $(gsstruct_h) $(gx_h) $(ramfs_h) $(string__h) $(unistd__h)\
   $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ramfs.$(OBJ) $(C_) $(GLSRC)ramfs.c

$(GLOBJ)gsioram.$(OBJ) : $(GLSRC)gsioram.c $(gp_h) $(gscdefs_h) $(gserrors_h)\
  $(gsparam_h) $(gsstruct_h) $(gsutil_h) $(gx_h) $(gxiodev_h) $(ramfs_h)\
  $(stream_h) $(string__h) $(unistd__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsioram.$(OBJ) $(C_) $(GLSRC)gsioram.c


# ---------------- Support for %disk IODevices ---------------- #
# The following module is included only if the diskn.dev FEATURE is included
$(GLOBJ)gsiodisk.$(OBJ) : $(GLSRC)gsiodisk.c $(AK) $(gx_h)\
 $(gserrors_h) $(errno__h) $(string__h) $(unistd__h)\
 $(gp_h) $(gscdefs_h) $(gsparam_h) $(gsstruct_h) $(gxiodev_h) $(gsutil_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsiodisk.$(OBJ) $(C_) $(GLSRC)gsiodisk.c

# ================ Platform-specific modules ================ #
# Platform-specific code doesn't really belong here: this is code that is
# shared among multiple platforms.

# Standard implementation of gp_getenv.
$(GLOBJ)gp_getnv.$(OBJ) : $(GLSRC)gp_getnv.c $(AK) $(stdio__h)\
 $(string__h) $(gp_h) $(gsmemory_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_getnv.$(OBJ) $(C_) $(GLSRC)gp_getnv.c

$(AUX)gp_getnv.$(OBJ) : $(GLSRC)gp_getnv.c $(AK) $(stdio__h)\
 $(string__h) $(gp_h) $(gsmemory_h) $(gstypes_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gp_getnv.$(OBJ) $(C_) $(GLSRC)gp_getnv.c

# Standard implementation of gp_defaultpapersize.
$(GLOBJ)gp_paper.$(OBJ) : $(GLSRC)gp_paper.c $(AK) $(gp_h) $(gx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_paper.$(OBJ) $(C_) $(GLSRC)gp_paper.c

# Unix implementation of gp_defaultpapersize.
$(GLOBJ)gp_upapr.$(OBJ) : $(GLSRC)gp_upapr.c $(malloc__h) $(AK) $(gp_h)\
 $(gx_h) $(string__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_upapr.$(OBJ) $(C_) $(GLSRC)gp_upapr.c

# File system implementation.

# MS-DOS file system, also used by Desqview/X.
$(GLOBJ)gp_dosfs.$(OBJ) : $(GLSRC)gp_dosfs.c $(AK) $(dos__h) $(gp_h)\
 $(gpmisc_h) $(gx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_dosfs.$(OBJ) $(C_) $(GLSRC)gp_dosfs.c

# MS-DOS file enumeration, *not* used by Desqview/X.
$(GLOBJ)gp_dosfe.$(OBJ) : $(GLSRC)gp_dosfe.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h)\
 $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(gp_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_dosfe.$(OBJ) $(C_) $(GLSRC)gp_dosfe.c

# Unix(-like) file system, also used by Desqview/X.
$(GLOBJ)gp_unifs.$(OBJ) : $(GLSRC)gp_unifs.c $(AK)\
 $(memory__h) $(string__h) $(stdio__h) $(unistd__h) \
 $(gx_h) $(gp_h) $(gpmisc_h) $(gsstruct_h) $(gsutil_h) \
 $(stat__h) $(dirent__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_unifs.$(OBJ) $(C_) $(GLSRC)gp_unifs.c

$(AUX)gp_unifs.$(OBJ) : $(GLSRC)gp_unifs.c $(AK)\
 $(memory__h) $(string__h) $(stdio__h) $(unistd__h) \
 $(gx_h) $(gp_h) $(gpmisc_h) $(gsstruct_h) $(gsutil_h) \
 $(stat__h) $(dirent__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gp_unifs.$(OBJ) $(C_) $(GLSRC)gp_unifs.c

# Unix(-like) file name syntax, *not* used by Desqview/X.
$(GLOBJ)gp_unifn.$(OBJ) : $(GLSRC)gp_unifn.c $(AK) $(gx_h) $(gp_h)\
 $(gpmisc_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_unifn.$(OBJ) $(C_) $(GLSRC)gp_unifn.c

$(AUX)gp_unifn.$(OBJ) : $(GLSRC)gp_unifn.c $(AK) $(gx_h) $(gp_h)\
 $(gpmisc_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gp_unifn.$(OBJ) $(C_) $(GLSRC)gp_unifn.c

# Pipes.  These are actually the same on all platforms that have them.

pipe_=$(GLOBJ)gdevpipe.$(OBJ)
$(GLD)pipe.dev : $(LIB_MAK) $(ECHOGS_XE) $(pipe_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)pipe $(pipe_)
	$(ADDMOD) $(GLD)pipe -iodev pipe

$(GLOBJ)gdevpipe.$(OBJ) : $(GLSRC)gdevpipe.c $(AK)\
 $(errno__h) $(pipe__h) $(stdio__h) $(string__h) \
 $(gserrors_h) $(gsmemory_h) $(gstypes_h) $(gxiodev_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevpipe.$(OBJ) $(C_) $(GLSRC)gdevpipe.c

# Thread / semaphore / monitor implementation.

# Dummy implementation.
nosync_=$(GLOBJ)gp_nsync.$(OBJ)
$(GLD)nosync.dev : $(LIB_MAK) $(ECHOGS_XE) $(nosync_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)nosync $(nosync_)

$(GLOBJ)gp_nsync.$(OBJ) : $(GLSRC)gp_nsync.c $(AK) $(std_h)\
 $(gpsync_h) $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_nsync.$(OBJ) $(C_) $(GLSRC)gp_nsync.c

# POSIX pthreads-based implementation.
pthreads_=$(GLOBJ)gp_psync.$(OBJ)
$(GLD)posync.dev : $(LIB_MAK) $(ECHOGS_XE) $(pthreads_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)posync $(pthreads_)
	$(ADDMOD) $(GLD)posync -replace $(GLD)nosync

$(GLOBJ)gp_psync.$(OBJ) : $(GLSRC)gp_psync.c $(AK) $(malloc__h) $(string__h) \
 $(std_h) $(gpsync_h) $(gserrors_h) $(assert__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_psync.$(OBJ) $(C_) $(GLSRC)gp_psync.c

# Other stuff.

# Other MS-DOS facilities.
$(GLOBJ)gp_msdos.$(OBJ) : $(GLSRC)gp_msdos.c $(AK)\
 $(dos__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gp_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_msdos.$(OBJ) $(C_) $(GLSRC)gp_msdos.c

# Dummy XPS printing function - the only real one is in the Windows
# platform code
$(GLOBJ)gp_nxpsprn.$(OBJ) : $(GLSRC)gp_nxpsprn.c $(gp_h) $(LIB_MAK) \
                            $(AK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_nxpsprn.$(OBJ) $(C_) $(GLSRC)gp_nxpsprn.c

# ================ Dependencies for auxiliary programs ================ #

GENARCH_DEPS=$(stdpre_h)
GENCONF_DEPS=$(stdpre_h)
GENDEV_DEPS=$(stdpre_h)
# For the included .c files, we need to include both the .c and the
# compiled file in the dependencies, to express the implicit dependency
# on all .h files included by those .c files.
GENHT_DEPS=$(malloc__h) $(stdio__h) $(string__h)\
 $(gscdefs_h) $(gsmemory_h)\
 $(gxbitmap_h) $(gxdht_h) $(gxhttile_h) $(gxtmap_h)\
 $(sstring_h) $(strimpl_h)\
 $(GLSRC)gxhtbit.c $(GLOBJ)gxhtbit.$(OBJ)\
 $(GLSRC)scantab.c $(GLOBJ)scantab.$(OBJ)\
 $(GLSRC)sstring.c $(GLOBJ)sstring.$(OBJ)
GENHT_CFLAGS=$(I_)$(GLI_)$(_I) $(GLF_)

# ============================= Main program ============================== #

# Main program for library testing

$(GLOBJ)gslib.$(OBJ) : $(GLSRC)gslib.c $(AK)\
 $(math__h) $(stdio__h) $(string__h)\
 $(gx_h) $(gp_h)\
 $(gsalloc_h) $(gserrors_h) $(gsmatrix_h)\
 $(gsrop_h) $(gsstate_h) $(gscspace_h)\
 $(gscdefs_h) $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gscrd_h)\
 $(gshtx_h) $(gsiparm3_h) $(gsiparm4_h) $(gslib_h) $(gsparam_h)\
 $(gspaint_h) $(gspath_h) $(gspath2_h) $(gsstruct_h) $(gsutil_h)\
 $(gxalloc_h) $(gxdcolor_h) $(gxdevice_h) $(gxht_h) $(gdevbbox_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gslib.$(OBJ) $(C_) $(GLSRC)gslib.c
