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
GLLDFJB2CC=$(CC_) $(I_)$(LDF_JB2I_) $(II)$(GLI_)$(_I) $(JB2CF_) $(GLF_)
GLLWFJPXCC=$(CC_) $(I_)$(LWF_JPXI_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_)
GLCCSHARED=$(CC_SHARED) $(GLCCFLAGS)

GLJCC=$(CC) $(I_)$(GLI_) $(II)$(JI_)$(_I) $(JCF_) $(GLF_) $(CCFLAGS)
GLZCC=$(CC) $(I_)$(GLI_) $(II)$(ZI_)$(_I) $(ZCF_) $(GLF_) $(CCFLAGS)
GLJBIG2CC=$(CC) $(I_)$(GLI_) $(II)$(JB2I_)$(_I) $(JB2CF_) $(GLF_) $(CCFLAGS)
GLJASCC=$(CC) $(I_)$(JPXI_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_) $(CCFLAGS)
GLJPXOPJCC=$(CC) $(I_)$(JPX_OPENJPEG_I_)$(D).. $(I_)$(JPX_OPENJPEG_I_) $(II)$(GLI_)$(_I) $(JPXCF_) $(GLF_) $(CCFLAGS)
GLFTCC=$(CC) $(FT_CFLAGS) $(D_)FT_CONFIG_OPTIONS_H=\"$(FTCONFH)\"$(_D) $(CCFLAGS) $(GLCCFLAGS)

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

gdevdcrd_h=$(GLSRC)gdevdcrd.h
gdevpccm_h=$(GLSRC)gdevpccm.h

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
stdint__h=$(GLSRC)stdint_.h

$(GLGEN)arch.h : $(GENARCH_XE)
	$(EXP)$(GENARCH_XE) $(GLGEN)arch.h $(TARGET_ARCH_FILE)

# Platform interfaces

# gp.h requires gstypes.h and srdline.h.
gstypes_h=$(GLSRC)gstypes.h
srdline_h=$(GLSRC)srdline.h
gpgetenv_h=$(GLSRC)gpgetenv.h
gpmisc_h=$(GLSRC)gpmisc.h
gp_h=$(GLSRC)gp.h
gpcheck_h=$(GLSRC)gpcheck.h
gpsync_h=$(GLSRC)gpsync.h

# Configuration definitions

gconf_h=$(GLSRC)gconf.h
# gconfig*.h are generated dynamically.
gconfig__h=$(GLGEN)gconfig_.h
gscdefs_h=$(GLSRC)gscdefs.h

std_h=$(GLSRC)std.h

# C library interfaces

# Because of variations in the "standard" header files between systems, and
# because we must include std.h before any file that includes sys/types.h,
# we define local include files named *_.h to substitute for <*.h>.

vmsmath_h=$(GLSRC)vmsmath.h

# declare here for use by string__h
gssprintf_h=$(GLSRC)gssprintf.h
gsstrtok_h=$(GLSRC)gsstrtok.h
gsstrl_h=$(GLSRC)gsstrl.h

dos__h=$(GLSRC)dos_.h
ctype__h=$(GLSRC)ctype_.h
dirent__h=$(GLSRC)dirent_.h
errno__h=$(GLSRC)errno_.h
fcntl__h=$(GLSRC)fcntl_.h
locale__h=$(GLSRC)locale_.h $(MAKEFILE)
memento_h=$(GLSRC)memento.h
bobbin_h=$(GLSRC)bobbin.h
malloc__h=$(GLSRC)malloc_.h
math__h=$(GLSRC)math_.h
memory__h=$(GLSRC)memory_.h
setjmp__h=$(GLSRC)setjmp_.h
stat__h=$(GLSRC)stat_.h
stdio__h=$(GLSRC)stdio_.h
string__h=$(GLSRC)string_.h
time__h=$(GLSRC)time_.h $(std_h) $(gconfig__h)
unistd__h=$(GLSRC)unistd_.h
windows__h=$(GLSRC)windows_.h
assert__h=$(GLSRC)assert_.h
# Out of order
pipe__h=$(GLSRC)pipe_.h

# Third-party library interfaces

jerror_h=$(JSRCDIR)$(D)jerror.h
jerror__h=$(GLSRC)jerror_.h $(MAKEFILE)
jpeglib__h=$(GLGEN)jpeglib_.h

cal_h=$(CALSRCDIR)$(D)cal.h

# The following would logically be better in freetype.mak
# but we need it for fapi_ft.c below
FTCONFH=gsftopts.h
GENFTCONFH=$(FTGENDIR)$(D)$(FTCONFH)
BASEFTCONFH=$(GLSRC)$(FTCONFH)

$(GENFTCONFH) : $(BASEFTCONFH) $(MAKEDIRS)
	$(CP_) $(BASEFTCONFH) $(GENFTCONFH)

# Miscellaneous

gsio_h=$(GLSRC)gsio.h
gxstdio_h=$(GLSRC)gxstdio.h
gs_dll_call_h=$(GLSRC)gs_dll_call.h
gslibctx_h=$(GLSRC)gslibctx.h
gdbflags_h=$(GLSRC)gdbflags.h
gdebug_h=$(GLSRC)gdebug.h
gsalloc_h=$(GLSRC)gsalloc.h
gsargs_h=$(GLSRC)gsargs.h
gserrors_h=$(GLSRC)gserrors.h
gsexit_h=$(GLSRC)gsexit.h
gsgc_h=$(GLSRC)gsgc.h
gsgstate_h=$(GLSRC)gsgstate.h
gsmalloc_h=$(GLSRC)gsmalloc.h
gsmchunk_h=$(GLSRC)gsmchunk.h
valgrind_h=$(GLSRC)valgrind.h
gsmdebug_h=$(GLSRC)gsmdebug.h
gsmemraw_h=$(GLSRC)gsmemraw.h
gsmemory_h=$(GLSRC)gsmemory.h
gsmemret_h=$(GLSRC)gsmemret.h
gsnogc_h=$(GLSRC)gsnogc.h
gsrefct_h=$(GLSRC)gsrefct.h
gsserial_h=$(GLSRC)gsserial.h
gsstype_h=$(GLSRC)gsstype.h
gx_h=$(GLSRC)gx.h
gxsync_h=$(GLSRC)gxsync.h
gxclthrd_h=$(GLSRC)gxclthrd.h
gxdevsop_h=$(GLSRC)gxdevsop.h
gdevflp_h=$(GLSRC)gdevflp.h
gdevkrnlsclass_h=$(GLSRC)gdevkrnlsclass.h
gdevsclass_h=$(GLSRC)gdevsclass.h

# Out of order
gsnotify_h=$(GLSRC)gsnotify.h
gsstruct_h=$(GLSRC)gsstruct.h

###### Support

### Include files

gsbitmap_h=$(GLSRC)gsbitmap.h
gsbitops_h=$(GLSRC)gsbitops.h
gsbittab_h=$(GLSRC)gsbittab.h
gsflip_h=$(GLSRC)gsflip.h
gsuid_h=$(GLSRC)gsuid.h
gsutil_h=$(GLSRC)gsutil.h
gxarith_h=$(GLSRC)gxarith.h
gxbitmap_h=$(GLSRC)gxbitmap.h
gxfarith_h=$(GLSRC)gxfarith.h
gxfixed_h=$(GLSRC)gxfixed.h
gxobj_h=$(GLSRC)gxobj.h
gxrplane_h=$(GLSRC)gxrplane.h
# Out of order
gsrect_h=$(GLSRC)gsrect.h
gxalloc_h=$(GLSRC)gxalloc.h
gxbitops_h=$(GLSRC)gxbitops.h
gxcindex_h=$(GLSRC)gxcindex.h
gxfont42_h=$(GLSRC)gxfont42.h
gstrans_h=$(GLSRC)gstrans.h

# Streams
scommon_h=$(GLSRC)scommon.h
stream_h=$(GLSRC)stream.h
ramfs_h=$(GLSRC)ramfs.h

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
$(GLOBJ)gpmisc.$(OBJ) : $(GLSRC)gpmisc.c $(errno__h) $(unistd__h) $(fcntl__h) \
 $(stat__h) $(stdio__h) $(memory__h) $(string__h) $(gp_h) $(gpgetenv_h) \
 $(gpmisc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gpmisc.$(OBJ) $(C_) $(GLSRC)gpmisc.c

$(AUX)gpmisc.$(OBJ) : $(GLSRC)gpmisc.c $(errno__h) $(unistd__h) $(fcntl__h) \
 $(stat__h) $(stdio__h) $(memory__h) $(string__h) $(gp_h) $(gpgetenv_h) \
 $(gpmisc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(AUXO_)gpmisc.$(OBJ) $(C_) $(GLSRC)gpmisc.c

# Command line argument list management
$(GLOBJ)gsargs.$(OBJ) : $(GLSRC)gsargs.c\
 $(ctype__h) $(stdio__h) $(string__h)\
 $(gsargs_h) $(gsexit_h) $(gsmemory_h)\
 $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsargs.$(OBJ) $(C_) $(GLSRC)gsargs.c

$(GLOBJ)gsmisc.$(OBJ) : $(GLSRC)gsmisc.c $(AK) $(gx_h) $(gserrors_h)\
 $(vmsmath_h) $(std_h) $(ctype__h) $(malloc__h) $(math__h) $(memory__h)\
 $(string__h) $(gpcheck_h) $(gxfarith_h) $(gxfixed_h) $(stdint__h) $(stdio__h)\
 $(gdbflags_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmisc.$(OBJ) $(C_) $(GLSRC)gsmisc.c

$(AUX)gsmisc.$(OBJ) : $(GLSRC)gsmisc.c $(AK) $(gx_h) $(gpmisc_h) $(gserrors_h)\
 $(vmsmath_h) $(std_h)  $(ctype__h) $(malloc__h) $(math__h) $(memory__h)\
 $(string__h) $(gpcheck_h) $(gxfarith_h) $(gxfixed_h) $(stdint__h) $(stdio__h)\
 $(gdbflags_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(C_) $(AUXO_)gsmisc.$(OBJ) $(GLSRC)gsmisc.c

$(GLOBJ)gslibctx_1.$(OBJ) : $(GLSRC)gslibctx.c  $(AK) $(gp_h) $(gpmisc_h) $(gsmemory_h)\
  $(gslibctx_h) $(stdio__h) $(string__h) $(gsicc_manage_h) $(gserrors_h)\
  $(gscdefs_h) $(gsstruct_h)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gslibctx_1.$(OBJ) $(C_) $(GLSRC)gslibctx.c

$(GLOBJ)gslibctx_0.$(OBJ) : $(GLSRC)gslibctx.c  $(AK) $(gp_h) $(gpmisc_h) $(gsmemory_h)\
  $(gslibctx_h) $(stdio__h) $(string__h) $(gsicc_manage_h) $(gserrors_h)\
  $(gscdefs_h) $(gsstruct_h)
	$(GLCC) $(GLO_)gslibctx_0.$(OBJ) $(C_) $(GLSRC)gslibctx.c

$(GLOBJ)gslibctx.$(OBJ) : $(GLOBJ)gslibctx_$(WITH_CAL).$(OBJ)  $(AK) $(gp_h)
	$(CP_) $(GLOBJ)gslibctx_$(WITH_CAL).$(OBJ) $(GLOBJ)gslibctx.$(OBJ)

$(AUX)gslibctx.$(OBJ) : $(GLSRC)gslibctx.c  $(AK) $(gp_h) $(gsmemory_h)\
  $(gslibctx_h) $(stdio__h) $(string__h) $(gsicc_manage_h) $(gserrors_h)\
  $(gscdefs_h) $(gsstruct_h)
	$(GLCCAUX) $(C_) $(AUXO_)gslibctx.$(OBJ) $(GLSRC)gslibctx.c

$(GLOBJ)gsnotify.$(OBJ) : $(GLSRC)gsnotify.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsnotify_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsnotify.$(OBJ) $(C_) $(GLSRC)gsnotify.c

$(GLOBJ)gsserial.$(OBJ) : $(GLSRC)gsserial.c $(stdpre_h) $(gstypes_h)\
 $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsserial.$(OBJ) $(C_) $(GLSRC)gsserial.c

$(GLOBJ)gsutil.$(OBJ) : $(GLSRC)gsutil.c $(AK) $(memory__h)\
 $(string__h) $(gstypes_h) $(gserrors_h) $(gsmemory_h)\
 $(gsrect_h) $(gsuid_h) $(gsutil_h) $(gxsync_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsutil.$(OBJ) $(C_) $(GLSRC)gsutil.c

$(AUX)gsutil.$(OBJ) : $(GLSRC)gsutil.c $(AK) $(memory__h) $(string__h)\
 $(gstypes_h) $(gserrors_h) $(gsmemory_h)\
 $(gsrect_h) $(gsuid_h) $(gsutil_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCCAUX) $(C_) $(AUXO_)gsutil.$(OBJ) $(GLSRC)gsutil.c

$(GLOBJ)gssprintf.$(OBJ) : $(GLSRC)gssprintf.c $(gssprintf_h) $(unistd__h) \
 $(gp_h) $(stdio__h) $(stdint__h) $(string__h) $(math__h)
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
$(GLOBJ)gsmd5.$(OBJ) : $(GLSRC)gsmd5.c $(AK) $(gsmd5_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmd5.$(OBJ) $(C_) $(GLSRC)gsmd5.c

# SHA-256 digest
sha2_h=$(GLSRC)sha2.h
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

gsccode_h=$(GLSRC)gsccode.h $(std_h) $(stdint__h) $(gstypes_h)
gsccolor_h=$(GLSRC)gsccolor.h
# gscedata.[ch] are generated automatically by lib/encs2c.ps.
gscedata_h=$(GLSRC)gscedata.h
gscencs_h=$(GLSRC)gscencs.h
gsagl_h=$(GLSRC)gsagl.h
gsclipsr_h=$(GLSRC)gsclipsr.h
gscsel_h=$(GLSRC)gscsel.h
gscolor1_h=$(GLSRC)gscolor1.h
gscompt_h=$(GLSRC)gscompt.h
gscoord_h=$(GLSRC)gscoord.h
gscpm_h=$(GLSRC)gscpm.h
gscsepnm_h=$(GLSRC)gscsepnm.h
gsdevice_h=$(GLSRC)gsdevice.h
gsfcmap_h=$(GLSRC)gsfcmap.h
gsfname_h=$(GLSRC)gsfname.h
gsfont_h=$(GLSRC)gsfont.h
gsgdata_h=$(GLSRC)gsgdata.h
gsgcache_h=$(GLSRC)gsgcache.h
gshsb_h=$(GLSRC)gshsb.h
gsht_h=$(GLSRC)gsht.h
gsht1_h=$(GLSRC)gsht1.h
gsjconf_h=$(GLSRC)gsjconf.h
gslib_h=$(GLSRC)gslib.h
gslparam_h=$(GLSRC)gslparam.h
gsmatrix_h=$(GLSRC)gsmatrix.h
# Out of order
gxbitfmt_h=$(GLSRC)gxbitfmt.h
gxcomp_h=$(GLSRC)gxcomp.h
gsovrc_h=$(GLSRC)gsovrc.h
gspaint_h=$(GLSRC)gspaint.h
gsparam_h=$(GLSRC)gsparam.h
gsparams_h=$(GLSRC)gsparams.h
gsparamx_h=$(GLSRC)gsparamx.h
gspath2_h=$(GLSRC)gspath2.h
gspcolor_h=$(GLSRC)gspcolor.h
gspenum_h=$(GLSRC)gspenum.h
gsptype1_h=$(GLSRC)gsptype1.h
gsropt_h=$(GLSRC)gsropt.h
gstext_h=$(GLSRC)gstext.h
gsxfont_h=$(GLSRC)gsxfont.h
# Out of order
gschar_h=$(GLSRC)gschar.h
gsiparam_h=$(GLSRC)gsiparam.h
gsimage_h=$(GLSRC)gsimage.h
gsline_h=$(GLSRC)gsline.h
gspath_h=$(GLSRC)gspath.h
gsrop_h=$(GLSRC)gsrop.h
gstparam_h=$(GLSRC)gstparam.h

gxalpha_h=$(GLSRC)gxalpha.h
gxbcache_h=$(GLSRC)gxbcache.h
gxcvalue_h=$(GLSRC)gxcvalue.h
gxclio_h=$(GLSRC)gxclio.h
gxclip_h=$(GLSRC)gxclip.h
gxclipsr_h=$(GLSRC)gxclipsr.h
gxcoord_h=$(GLSRC)gxcoord.h
gxcpath_h=$(GLSRC)gxcpath.h
gxdda_h=$(GLSRC)gxdda.h
gxdevbuf_h=$(GLSRC)gxdevbuf.h
gxdevmem_h=$(GLSRC)gxdevmem.h
gxdhtres_h=$(GLSRC)gxdhtres.h
gxfont0_h=$(GLSRC)gxfont0.h
gxfrac_h=$(GLSRC)gxfrac.h
gxftype_h=$(GLSRC)gxftype.h
gxgetbit_h=$(GLSRC)gxgetbit.h
gxhttile_h=$(GLSRC)gxhttile.h
gxhttype_h=$(GLSRC)gxhttype.h
gxiclass_h=$(GLSRC)gxiclass.h
gxiodev_h=$(GLSRC)gxiodev.h
gxline_h=$(GLSRC)gxline.h
gxlum_h=$(GLSRC)gxlum.h
gxmatrix_h=$(GLSRC)gxmatrix.h
gxmclip_h=$(GLSRC)gxmclip.h
gxoprect_h=$(GLSRC)gxoprect.h
gxp1impl_h=$(GLSRC)gxp1impl.h
gxpaint_h=$(GLSRC)gxpaint.h
gxpath_h=$(GLSRC)gxpath.h
gxpcache_h=$(GLSRC)gxpcache.h
gxsample_h=$(GLSRC)gxsample.h
gxsamplp_h=$(GLSRC)gxsamplp.h
gxscanc_h=$(GLSRC)gxscanc.h
gxstate_h=$(GLSRC)gxstate.h
gxtext_h=$(GLSRC)gxtext.h
gxtmap_h=$(GLSRC)gxtmap.h
gxxfont_h=$(GLSRC)gxxfont.h
# The following are out of order because they include other files.
gxband_h=$(GLSRC)gxband.h
gxcdevn_h=$(GLSRC)gxcdevn.h
gxchar_h=$(GLSRC)gxchar.h
gxchrout_h=$(GLSRC)gxchrout.h
gsdcolor_h=$(GLSRC)gsdcolor.h
gxdcolor_h=$(GLSRC)gxdcolor.h
gsnamecl_h=$(GLSRC)gsnamecl.h
gsncdummy_h=$(GLSRC)gsncdummy.h
gscspace_h=$(GLSRC)gscspace.h
# FIXME: gscspace_h should depend on $(gscms_h) too.
gscssub_h=$(GLSRC)gscssub.h
gxdevcli_h=$(GLSRC)gxdevcli.h
gscicach_h=$(GLSRC)gscicach.h
gxdevice_h=$(GLSRC)gxdevice.h
gxdht_h=$(GLSRC)gxdht.h
gxdhtserial_h=$(GLSRC)gxdhtserial.h
gxdither_h=$(GLSRC)gxdither.h
gxclip2_h=$(GLSRC)gxclip2.h
gxclipm_h=$(GLSRC)gxclipm.h
gxctable_h=$(GLSRC)gxctable.h
gxfcache_h=$(GLSRC)gxfcache.h

gxfont_h=$(GLSRC)gxfont.h
gxiparam_h=$(GLSRC)gxiparam.h
gximask_h=$(GLSRC)gximask.h
gscie_h=$(GLSRC)gscie.h
gsicc_h=$(GLSRC)gsicc.h
gscrd_h=$(GLSRC)gscrd.h
gscrdp_h=$(GLSRC)gscrdp.h
gscdevn_h=$(GLSRC)gscdevn.h
gscindex_h=$(GLSRC)gscindex.h
gscolor2_h=$(GLSRC)gscolor2.h
gscsepr_h=$(GLSRC)gscsepr.h
gxdcconv_h=$(GLSRC)gxdcconv.h
gxfmap_h=$(GLSRC)gxfmap.h
gxcmap_h=$(GLSRC)gxcmap.h

gxgstate_h=$(GLSRC)gxgstate.h

gxcolor2_h=$(GLSRC)gxcolor2.h
gxclist_h=$(GLSRC)gxclist.h
gxcspace_h=$(GLSRC)gxcspace.h
gxht_h=$(GLSRC)gxht.h
gxcie_h=$(GLSRC)gxcie.h
gxht_thresh_h=$(GLSRC)gxht_thresh.h
gxpcolor_h=$(GLSRC)gxpcolor.h
gscolor_h=$(GLSRC)gscolor.h
gsstate_h=$(GLSRC)gsstate.h
gsicc_create_h=$(GLSRC)gsicc_create.h
gximdecode_h=$(GLSRC)gximdecode.h

gzacpath_h=$(GLSRC)gzacpath.h
gzcpath_h=$(GLSRC)gzcpath.h
gzht_h=$(GLSRC)gzht.h
gzline_h=$(GLSRC)gzline.h
gzpath_h=$(GLSRC)gzpath.h
gzstate_h=$(GLSRC)gzstate.h

gdevbbox_h=$(GLSRC)gdevbbox.h
gdevmem_h=$(GLSRC)gdevmem.h
gdevmpla_h=$(GLSRC)gdevmpla.h
gdevmrop_h=$(GLSRC)gdevmrop.h
gdevmrun_h=$(GLSRC)gdevmrun.h
gdevplnx_h=$(GLSRC)gdevplnx.h
gdevepo_h=$(GLSRC)gdevepo.h

sa85d_h=$(GLSRC)sa85d.h
sa85x_h=$(GLSRC)sa85x.h
sbcp_h=$(GLSRC)sbcp.h
sbtx_h=$(GLSRC)sbtx.h
scanchar_h=$(GLSRC)scanchar.h
sfilter_h=$(GLSRC)sfilter.h
sdct_h=$(GLSRC)sdct.h
shc_h=$(GLSRC)shc.h
sisparam_h=$(GLSRC)sisparam.h
sjpeg_h=$(GLSRC)sjpeg.h
slzwx_h=$(GLSRC)slzwx.h
smd5_h=$(GLSRC)smd5.h
sarc4_h=$(GLSRC)sarc4.h
saes_h=$(GLSRC)saes.h
sjbig2_h=$(GLSRC)sjbig2.h
sjpx_openjpeg_h=$(GLSRC)sjpx_openjpeg.h $(scommon_h) $(openjpeg_h)
spdiffx_h=$(GLSRC)spdiffx.h
spngpx_h=$(GLSRC)spngpx.h
spprint_h=$(GLSRC)spprint.h
spsdf_h=$(GLSRC)spsdf.h
srlx_h=$(GLSRC)srlx.h
spwgx_h=$(GLSRC)spwgx.h
surfx_h=$(SURFX_H)
sstring_h=$(GLSRC)sstring.h
strimpl_h=$(GLSRC)strimpl.h
szlibx_h=$(GLSRC)szlibx.h
zlib_h=$(ZSRCDIR)$(D)zlib.h
# We have two of the following, for shared zlib (_1)
# and 'local' zlib (_0)
szlibxx_h_1=$(GLSRC)szlibxx.h $(szlibx_h)
szlibxx_h_0=$(GLSRC)szlibxx.h $(szlibx_h) $(zlib_h)
# Out of order
scf_h=$(GLSRC)scf.h
scfx_h=$(GLSRC)scfx.h
siinterp_h=$(GLSRC)siinterp.h
siscale_h=$(GLSRC)siscale.h
sidscale_h=$(GLSRC)sidscale.h
simscale_h=$(GLSRC)simscale.h
simscale_foo_h=$(GLSRC)simscale_foo.h
gximage_h=$(GLSRC)gximage.h
gxhldevc_h=$(GLSRC)gxhldevc.h
gsptype2_h=$(GLSRC)gsptype2.h
gdevddrw_h=$(GLSRC)gdevddrw.h
gxfill_h=$(GLSRC)gxfill.h
gxfilltr_h=$(GLSRC)gxfilltr.h
gxfillsl_h=$(GLSRC)gxfillsl.h
gxfillts_h=$(GLSRC)gxfillts.h
gxdtfill_h=$(GLSRC)gxdtfill.h

ttfoutl_h=$(GLSRC)ttfoutl.h
gxttfb_h=$(GLSRC)gxttfb.h
gzspotan_h=$(GLSRC)gzspotan.h

gdevpxen_h=$(GLSRC)gdevpxen.h
gdevpxat_h=$(GLSRC)gdevpxat.h
gdevpxop_h=$(GLSRC)gdevpxop.h

gsequivc_h=$(GLSRC)gsequivc.h
gdevdevn_h=$(GLSRC)gdevdevn.h
gdevdevnprn_h=$(GLSRC)gdevdevnprn.h

gdevoflt_h=$(GLSRC)gdevoflt.h

gdevnup_h=$(GLSRC)gdevnup.h

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

$(GLOBJ)gxbcache.$(OBJ) : $(GLSRC)gxbcache.c $(AK) $(gx_h) $(gxobj_h) \
 $(memory__h) $(gsmdebug_h) $(gxbcache_h) $(LIB_MAK) $(MAKEDIRS)
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
 $(gxpath_h) $(gxxfont_h) $(gzstate_h) $(gxttfb_h) $(gxfont42_h) $(gxobj_h) \
 $(LIB_MAK) $(MAKEDIRS)
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
 $(gsstruct_h) $(gxdevsop_h) $(assert__h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxfixed_h) $(gxhttile_h) $(gxgstate_h) $(gxpaint_h) $(gxfill_h) $(gxpath_h)\
 $(gsptype1_h) $(gsptype2_h) $(gxpcolor_h) $(gsstate_h) $(gzcpath_h)\
 $(gzpath_h) $(gzspotan_h) $(gdevddrw_h) $(memory__h) $(stdint__h)\
 $(gxfilltr_h) $(gxfillsl_h) $(gxfillts_h) $(gxscanc_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxfill.$(OBJ) $(C_) $(GLSRC)gxfill.c

$(GLOBJ)gxht.$(OBJ) : $(GLSRC)gxht.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsbitops_h) $(gsstruct_h) $(gsutil_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxfixed_h) $(gxgstate_h) $(gzht_h)\
 $(gsserial_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxht.$(OBJ) $(C_) $(GLSRC)gxht.c

$(GLOBJ)gxhtbit.$(OBJ) : $(GLSRC)gxhtbit.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsbitops_h) $(gscdefs_h)\
 $(gxbitmap_h) $(gxdht_h) $(gxdhtres_h) $(gxhttile_h) $(gxtmap_h) $(gp_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxhtbit.$(OBJ) $(C_) $(GLSRC)gxhtbit.c

$(GLOBJ)gxht_thresh.$(OBJ) : $(GLSRC)gxht_thresh.c $(AK) $(memory__h)\
 $(gx_h) $(gxgstate_h) $(gsiparam_h) $(math__h) $(gxfixed_h) $(gximage_h)\
 $(gxdevice_h) $(gxdht_h) $(gxht_thresh_h) $(gzht_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxht_thresh.$(OBJ) $(C_) $(GLSRC)gxht_thresh.c

$(GLOBJ)gxidata_0.$(OBJ) : $(GLSRC)gxidata.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gxcpath_h) $(gxdevice_h) $(gximage_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxidata_0.$(OBJ) $(C_) $(GLSRC)gxidata.c

$(GLOBJ)gxidata_1.$(OBJ) : $(GLSRC)gxidata.c $(AK) $(cal_h) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gxcpath_h) $(gxdevice_h) $(gximage_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gxidata_1.$(OBJ) $(C_) $(GLSRC)gxidata.c

$(GLOBJ)gxidata.$(OBJ) : $(GLOBJ)gxidata_$(WITH_CAL).$(OBJ) $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gxcpath_h) $(gxdevice_h) $(gximage_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gxidata_$(WITH_CAL).$(OBJ) $(GLOBJ)gxidata.$(OBJ)

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

$(GLOBJ)gximono_0.$(OBJ) : $(GLSRC)gximono.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h) $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxgstate_h) $(gxmatrix_h)\
 $(gzht_h) $(gsicc_h) $(gsicc_cache_h)  $(gsicc_cms_h)\
 $(gxcie_h) $(gscie_h) $(gxht_thresh_h) $(gxdda_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximono_0.$(OBJ) $(C_) $(GLSRC)gximono.c

$(GLOBJ)gximono_1.$(OBJ) : $(GLSRC)gximono.c $(AK) $(cal_h) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h) $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxgstate_h) $(gxmatrix_h)\
 $(gzht_h) $(gsicc_h) $(gsicc_cache_h)  $(gsicc_cms_h)\
 $(gxcie_h) $(gscie_h) $(gxht_thresh_h) $(gxdda_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gximono_1.$(OBJ) $(C_) $(GLSRC)gximono.c

$(GLOBJ)gximono.$(OBJ) : $(GLOBJ)gximono_$(WITH_CAL).$(OBJ) $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gpcheck_h) $(gdevmem_h) $(gsccolor_h) $(gspaint_h) $(gsutil_h)\
 $(gxarith_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
  $(gxdevmem_h) $(gxfixed_h) $(gximage_h) $(gxgstate_h) $(gxmatrix_h)\
 $(gzht_h) $(gsicc_h) $(gsicc_cache_h)  $(gsicc_cms_h)\
 $(gxcie_h) $(gscie_h) $(gxht_thresh_h) $(gxdda_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gximono_$(WITH_CAL).$(OBJ) $(GLOBJ)gximono.$(OBJ)

$(GLOBJ)gximask.$(OBJ) : $(GLSRC)gximask.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsptype1_h) $(gsptype2_h) $(gxdevice_h) $(gxdcolor_h)\
 $(gxcpath_h) $(gximask_h) $(gzacpath_h) $(gzcpath_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximask.$(OBJ) $(C_) $(GLSRC)gximask.c

$(GLOBJ)gxipixel.$(OBJ) : $(GLSRC)gxipixel.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gpcheck_h) $(gscindex_h) $(gscspace_h)\
 $(gsccolor_h) $(gscdefs_h) $(gspaint_h) $(gsstruct_h) $(gsutil_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxarith_h) $(gxiparam_h) $(gxmatrix_h)\
 $(gxdevice_h) $(gzpath_h) $(gzstate_h) $(gsicc_cache_h) $(gsicc_cms_h)\
 $(gzcpath_h) $(gxdevmem_h) $(gximage_h) $(gdevmrop_h) $(gsicc_manage_h)\
 $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
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

$(GLOBJ)gxscanc.$(OBJ) : $(GLSRC)gxscanc.c $(GX) $(gxscanc_h) $(gx_h)\
 $(assert__h) $(gpcheck_h) $(gscoord_h) $(gsdcolor_h) $(gsdevice_h)\
 $(gserrors_h) $(gsptype1_h) $(gxdcolor_h) $(gxdevice_h) $(gserrors_h)\
 $(gsptype1_h) $(gxdcolor_h) $(gxdevice_h) $(gxfarith_h) $(gxfill_h)\
 $(gxfixed_h) $(gxgstate_h) $(gxhttile_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzline_h) $(gzpath_h) $(math__h) $(memory__h) $(string__h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxscanc.$(OBJ) $(C_) $(GLSRC)gxscanc.c

$(GLOBJ)gxstroke.$(OBJ) : $(GLSRC)gxstroke.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(gpcheck_h) $(gsstate_h)\
 $(gscoord_h) $(gsdcolor_h) $(gsdevice_h) $(gsptype1_h)\
 $(gxdevice_h) $(gxfarith_h) $(gxfixed_h)\
 $(gxhttile_h) $(gxgstate_h) $(gxmatrix_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzline_h) $(gzpath_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxstroke.$(OBJ) $(C_) $(GLSRC)gxstroke.c

###### Higher-level facilities

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
 $(gsicc_h) $(gsicc_manage_h) $(string__h) $(strmio_h) $(gsicc_cache_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscspace.$(OBJ) $(C_) $(GLSRC)gscspace.c

$(GLOBJ)gscicach.$(OBJ) : $(GLSRC)gscicach.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsccolor_h) $(gxcspace_h) $(gxdcolor_h) $(gscicach_h)\
 $(memory__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gscicach.$(OBJ) $(C_) $(GLSRC)gscicach.c

$(GLOBJ)gsovrc.$(OBJ) : $(GLSRC)gsovrc.c $(AK) $(gx_h) $(gserrors_h)\
 $(assert__h) $(memory__h) $(gsutil_h) $(gxcomp_h) $(gxdevice_h) $(gsdevice_h)\
 $(gxgetbit_h) $(gsovrc_h) $(gxdcolor_h) $(gxoprect_h) $(gsbitops_h) $(gxgstate_h)\
 $(gxdevsop_h) $(gxcldev_h) $(LIB_MAK) $(MAKEDIRS)
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
 $(gsdevice_h) $(gsparam_h) $(gsparamx_h) $(gxdevice_h) $(gxfixed_h)\
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
 $(memory__h) $(string__h) $(gsstruct_h) $(gsutil_h) $(gxarith_h) $(gxdevsop_h)\
 $(gxdevice_h) $(gzht_h) $(gzstate_h) $(gxfmap_h) $(gp_h) $(LIB_MAK) $(MAKEDIRS)
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

$(GLOBJ)gspaint.$(OBJ) : $(GLSRC)gspaint.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gpcheck_h) $(gsropt_h) $(gxfixed_h) $(gxmatrix_h) $(gspaint_h)\
 $(gspath_h) $(gzpath_h) $(gxpaint_h) $(gzstate_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gzcpath_h) $(gxhldevc_h) $(gsutil_h) $(gxdevsop_h) $(gsicc_cms_h)\
 $(gdevepo_h) $(gxscanc_h) $(gxpcolor_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspaint.$(OBJ) $(C_) $(GLSRC)gspaint.c

$(GLOBJ)gsparam.$(OBJ) : $(GLSRC)gsparam.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(string__h) $(gsparam_h) $(gsstruct_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparam.$(OBJ) $(C_) $(GLSRC)gsparam.c

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

$(GLOBJ)gsparaml.$(OBJ) : $(GLSRC)gsparaml.c $(AK) $(gx_h)\
 $(gserrors_h) $(gsparam_h) $(string__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsparaml.$(OBJ) $(C_) $(GLSRC)gsparaml.c

$(GLOBJ)gspath.$(OBJ) : $(GLSRC)gspath.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(gscoord_h) $(gspath_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxmatrix_h)\
 $(gzcpath_h) $(gzpath_h) $(gzstate_h) $(gxpaint_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gspath.$(OBJ) $(C_) $(GLSRC)gspath.c

$(GLOBJ)gsstate.$(OBJ) : $(GLSRC)gsstate.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsstruct_h) $(gsutil_h) $(gzstate_h) $(gxcspace_h)\
 $(gscolor2_h) $(gscoord_h) $(gscie_h)\
 $(gxclipsr_h) $(gxcmap_h) $(gxdevice_h) $(gxpcache_h)\
 $(gzht_h) $(gzline_h) $(gspath_h) $(gzpath_h) $(gzcpath_h)\
 $(gsovrc_h) $(gxcolor2_h) $(gscolor3_h) $(gxpcolor_h) $(gsicc_manage_h)\
 $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsstate.$(OBJ) $(C_) $(GLSRC)gsstate.c

$(GLOBJ)gstext.$(OBJ) : $(GLSRC)gstext.c $(AK) $(memory__h) $(gdebug_h)\
 $(gserrors_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h)\
 $(gxfcache_h) $(gxdevcli_h) $(gxdcolor_h) $(gxfont_h) $(gxpath_h)\
 $(gxtext_h) $(gzstate_h) $(gsutil_h) $(gxdevsop_h)\
 $(gscspace_h) $(gsicc_blacktext_h) $(LIB_MAK) $(MAKEDIRS)
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
 $(gxgetbit_h) $(gxiparam_h) $(gxgstate_h) $(gsstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevplnx.$(OBJ) $(C_) $(GLSRC)gdevplnx.c

### Default driver procedure implementations

$(GLOBJ)gdevdbit.$(OBJ) : $(GLSRC)gdevdbit.c $(AK) $(gx_h) $(gserrors_h)\
 $(gpcheck_h) $(gdevmem_h) $(gsbittab_h) $(gsrect_h) $(gsropt_h) $(gxcpath_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(LIB_MAK) $(MAKEDIRS)
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

$(GLOBJ)gdevdflt.$(OBJ) : $(GLSRC)gdevdflt.c $(AK) $(gx_h) $(gserrors_h) \
 $(gsropt_h) $(gxcomp_h) $(gxdevice_h) $(gxdevsop_h) $(math__h) $(gsstruct_h) \
 $(gxobj_h) $(gdevp14_h) $(gstrans_h) $(gxgstate_h) $(memory__h)\
 $(LIB_MAK) $(MAKEDIRS)
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

gxfapi_h=$(GLSRC)gxfapi.h

# stub for UFST bridge support  :

$(GLD)gxfapiu.dev : $(LIB_MAK) $(ECHOGS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gxfapiu

wrfont_h=$(GLSRC)wrfont.h
write_t1_h=$(GLSRC)write_t1.h
write_t2_h=$(GLSRC)write_t2.h

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

$(GLOBJ)fapi_ft_0.$(OBJ) : $(GLSRC)fapi_ft.c $(AK)\
 $(stdio__h) $(malloc__h) $(write_t1_h) $(write_t2_h) $(math__h) $(gserrors_h)\
 $(gsmemory_h) $(gsmalloc_h) $(gxfixed_h) $(gdebug_h) $(gxbitmap_h)\
 $(gsmchunk_h) $(stream_h) $(gxiodev_h) $(gsfname_h) $(gxfapi_h) $(gxfont1_h)\
 $(gxfont_h) $(BASEFTCONFH) $(LIB_MAK) $(MAKEDIRS)
	$(GLFTCC) $(FT_CFLAGS) $(D_)FT_CONFIG_OPTIONS_H=\"$(FTCONFH)\"$(_D) $(GLO_)fapi_ft_0.$(OBJ) $(C_) $(GLSRC)fapi_ft.c

$(GLOBJ)fapi_ft_1.$(OBJ) : $(GLSRC)fapi_ft.c $(AK)\
 $(stdio__h) $(malloc__h) $(write_t1_h) $(write_t2_h) $(math__h) $(gserrors_h)\
 $(gsmemory_h) $(gsmalloc_h) $(gxfixed_h) $(gdebug_h) $(gxbitmap_h)\
 $(gsmchunk_h) $(stream_h) $(gxiodev_h) $(gsfname_h) $(gxfapi_h) $(gxfont1_h)\
 $(gxfont_h) $(BASEFTCONFH) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(FT_CFLAGS) $(GLO_)fapi_ft_1.$(OBJ) $(C_) $(GLSRC)fapi_ft.c

$(GLOBJ)fapi_ft.$(OBJ) : $(GLOBJ)fapi_ft_$(SHARE_FT).$(OBJ)
	$(CP_) $(GLOBJ)fapi_ft_$(SHARE_FT).$(OBJ) $(GLOBJ)fapi_ft.$(OBJ)

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
claptrap_h=$(GLSRC)claptrap.h
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
ets_h=$(GLSRC)ets.h
ets_tm_h=$(GLSRC)ets_tm.h
ets=$(GLOBJ)ets.$(OBJ)

$(GLOBJ)ets_0.$(OBJ) : $(GLSRC)ets.c $(AK) \
 $(ets_h) $(ets_tm_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)ets_0.$(OBJ) $(C_) $(GLSRC)ets.c

$(GLOBJ)ets.$(OBJ) : $(GLOBJ)ets_$(WITH_CAL).$(OBJ)  $(AK) $(gp_h)
	$(CP_) $(GLOBJ)ets_$(WITH_CAL).$(OBJ) $(GLOBJ)ets.$(OBJ)

# ----------- Downsampling routines ------------ #
gxdownscale_h=$(GLSRC)gxdownscale.h
downscale_=$(GLOBJ)gxdownscale.$(OBJ) $(claptrap) $(ets)

$(GLOBJ)gxdownscale_0.$(OBJ) : $(GLSRC)gxdownscale.c $(AK) $(string__h)\
 $(gxdownscale_h) $(gserrors_h) $(gdevprn_h) $(assert__h) $(ets_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxdownscale_0.$(OBJ) $(C_) $(GLSRC)gxdownscale.c

$(GLOBJ)gxdownscale_1.$(OBJ) : $(GLSRC)gxdownscale.c $(AK) $(string__h)\
 $(gxdownscale_h) $(gserrors_h) $(gdevprn_h) $(assert__h) $(ets_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gxdownscale_1.$(OBJ) $(C_) $(GLSRC)gxdownscale.c

$(GLOBJ)gxdownscale.$(OBJ) : $(GLOBJ)gxdownscale_$(WITH_CAL).$(OBJ) $(AK) $(gp_h)
	$(CP_) $(GLOBJ)gxdownscale_$(WITH_CAL).$(OBJ) $(GLOBJ)gxdownscale.$(OBJ)

###### Create a pseudo-"feature" for the entire graphics library.

LIB0s=$(GLOBJ)gpmisc.$(OBJ) $(GLOBJ)stream.$(OBJ) $(GLOBJ)strmio.$(OBJ)
LIB1s=$(GLOBJ)gsalloc.$(OBJ) $(GLOBJ)gxdownscale.$(OBJ) $(downscale_) $(GLOBJ)gdevprn.$(OBJ) $(GLOBJ)gdevflp.$(OBJ) $(GLOBJ)gdevkrnlsclass.$(OBJ) $(GLOBJ)gdevepo.$(OBJ)
LIB2s=$(GLOBJ)gdevmplt.$(OBJ) $(GLOBJ)gsbitcom.$(OBJ) $(GLOBJ)gsbitops.$(OBJ) $(GLOBJ)gsbittab.$(OBJ) $(GLOBJ)gdevoflt.$(OBJ) $(GLOBJ)gdevnup.$(OBJ) $(GLOBJ)gdevsclass.$(OBJ)
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

strmio_h=$(GLSRC)strmio.h

$(GLOBJ)strmio.$(OBJ) : $(GLSRC)strmio.c $(AK) $(malloc__h)\
  $(memory__h) $(gdebug_h) $(gsfname_h) $(gslibctx_h) $(gsstype_h)\
  $(gsmalloc_h) $(gsmemret_h) $(strmio_h) $(stream_h) $(gxiodev_h)\
 $(gserrors_h) $(LIB_MAK) $(MAKEDIRS)
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

sdcparam_h=$(GLSRC)sdcparam.h

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

ssha2_h=$(GLSRC)ssha2.h
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

# ---------------- JPEG 2000 compression filter ---------------- #

$(GLD)sjpx.dev : $(LIB_MAK) $(ECHOGS_XE) $(GLD)sjpx_$(JPX_LIB).dev $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLD)sjpx_$(JPX_LIB).dev $(GLD)sjpx.dev

$(GLOBJ)sjpx.$(OBJ) : $(GLSRC)sjpx.c $(AK) \
 $(memory__h) $(gsmalloc_h) \
 $(gdebug_h) $(strimpl_h) $(sjpx_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLJASCC) $(GLO_)sjpx.$(OBJ) $(C_) $(GLSRC)sjpx.c

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

# ---------------- URF RunLength decode filter ---------------- #

urfd_=$(GLOBJ)surfd.$(OBJ)
$(GLD)urfd.dev : $(LIB_MAK) $(ECHOGS_XE) $(urfd_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)urfd $(urfd_)

$(GLOBJ)surfd.$(OBJ) : $(URFSRCDIR)$(D)surfd.c $(AK) $(stdio__h) $(memory__h)\
 $(surfx_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)surfd.$(OBJ) $(C_) $(URFSRCDIR)$(D)surfd.c

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
  $(GLOBJ)sfilter1.$(OBJ) $(GLOBJ)sa85d.$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)psfilters $(GLOBJ)sfilter1.$(OBJ) $(GLOBJ)sfilter2.$(OBJ) $(GLOBJ)sa85d.$(OBJ)
	$(ADDMOD) $(GLD)psfilters $(GLOBJ)sa85d.$(OBJ)

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

gdevprn_h=$(GLSRC)gdevprn.h
gdevmplt_h=$(GLSRC)gdevmplt.h

page_=$(GLOBJ)gdevprn.$(OBJ) $(GLOBJ)gdevppla.$(OBJ) $(GLOBJ)gdevmplt.$(OBJ) $(GLOBJ)gdevflp.$(OBJ)\
 $(downscale_) $(GLOBJ)gdevoflt.$(OBJ) $(GLOBJ)gdevnup.$(OBJ) $(GLOBJ)gdevsclass.$(OBJ) $(GLOBJ)gdevepo.$(OBJ)

$(GLD)page.dev : $(LIB_MAK) $(ECHOGS_XE) $(page_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)page $(page_)
	$(ADDMOD) $(GLD)page -include $(GLD)clist

$(GLOBJ)gdevprn.$(OBJ) : $(GLSRC)gdevprn.c $(ctype__h) $(gdevprn_h) $(gp_h)\
 $(gsdevice_h) $(gsfname_h) $(gsparam_h) $(gxclio_h) $(gxgetbit_h)\
 $(gdevplnx_h) $(gstrans_h) $(gdevkrnlsclass_h) $(gxdownscale_h) $(gdevdevn_h)\
 $(gxdevsop_h) $(gsbitops_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevprn.$(OBJ) $(C_) $(GLSRC)gdevprn.c

$(GLOBJ)gdevmplt.$(OBJ) : $(GLSRC)gdevmplt.c $(gdevmplt_h) $(gdevp14_h)\
 $(gdevprn_h) $(gdevsclass_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h)\
 $(gsstype_h) $(gx_h) $(gxcmap_h) $(gxcpath_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxgstate_h) $(gxiparam_h) $(gxpaint_h) $(gxpath_h) $(math__h)\
 $(memory__h)
	$(GLCC) $(GLO_)gdevmplt.$(OBJ) $(C_) $(GLSRC)gdevmplt.c

$(GLOBJ)gdevflp.$(OBJ) : $(GLSRC)gdevflp.c $(gdevflp_h) $(gdevp14_h) \
 $(gdevprn_h) $(gdevsclass_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h)\
 $(gsstype_h) $(gx_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) \
 $(gxgstate_h) $(gximage_h) $(gxiparam_h) $(gxpaint_h) $(gxpath_h) $(math__h)\
 $(memory__h)
	$(GLCC) $(GLO_)gdevflp.$(OBJ) $(C_) $(GLSRC)gdevflp.c

$(GLOBJ)gdevepo.$(OBJ) : $(GLSRC)gdevepo.c $(gdevepo_h) $(gdevp14_h) \
 $(gdevprn_h) $(gdevsclass_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h)\
 $(gsstate_h) $(gsstype_h) $(gx_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h)\
 $(gxdevice_h) $(gxdevsop_h) $(gxgstate_h) $(gximage_h) $(gxiparam_h)\
 $(gxpaint_h) $(gxpath_h) $(math__h) $(memory__h)
	$(GLCC) $(GLO_)gdevepo.$(OBJ) $(C_) $(GLSRC)gdevepo.c

$(GLOBJ)gdevoflt.$(OBJ) : $(GLSRC)gdevoflt.c $(gdevoflt_h) $(gdevp14_h)\
 $(gdevprn_h) $(gdevsclass_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h)\
 $(gsstype_h) $(gx_h) $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h)\
 $(gximage_h) $(gxiparam_h) $(gxpaint_h) $(gxpath_h) $(math__h) $(memory__h)
	$(GLCC) $(GLO_)gdevoflt.$(OBJ) $(C_) $(GLSRC)gdevoflt.c

$(GLOBJ)gdevnup.$(OBJ) : $(GLSRC)gdevnup.c $(gdevnup_h) $(gdevp14_h)\
 $(gdevprn_h) $(gdevsclass_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h)\
 $(gsstype_h) $(gx_h) $(gxdevice_h)\
 $(math__h) $(memory__h)
	$(GLCC) $(GLO_)gdevnup.$(OBJ) $(C_) $(GLSRC)gdevnup.c

$(GLOBJ)gdevsclass.$(OBJ) : $(GLSRC)gdevsclass.c $(gdevsclass_h) $(gdevp14_h)\
 $(gdevprn_h) $(gsdevice_h) $(gserrors_h) $(gsparam_h) $(gsstype_h) $(gx_h)\
 $(gxcmap_h) $(gxcpath_h) $(gxdcolor_h) $(gxdevice_h) $(gxgstate_h)\
 $(gxiparam_h) $(gxpaint_h) $(gxpath_h) $(math__h) $(memory__h)
	$(GLCC) $(GLO_)gdevsclass.$(OBJ) $(C_) $(GLSRC)gdevsclass.c

$(GLOBJ)gdevkrnlsclass.$(OBJ) : $(GLSRC)gdevkrnlsclass.c $(gdevkrnlsclass_h)\
 $(gx_h) $(gxdcolor_h)
	$(GLCC) $(GLO_)gdevkrnlsclass.$(OBJ) $(C_) $(GLSRC)gdevkrnlsclass.c

# Planar page devices
gdevppla_h=$(GLSRC)gdevppla.h

$(GLOBJ)gdevppla.$(OBJ) : $(GLSRC)gdevppla.c\
 $(gdevmpla_h) $(gdevppla_h) $(gdevprn_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevppla.$(OBJ) $(C_) $(GLSRC)gdevppla.c

# ---------------- Masked images ---------------- #
# This feature is out of level order because Patterns require it
# (which they shouldn't) and because band lists treat ImageType 4
# images as a special case (which they shouldn't).

gsiparm3_h=$(GLSRC)gsiparm3.h
gsiparm4_h=$(GLSRC)gsiparm4.h
gximage3_h=$(GLSRC)gximage3.h

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

gxcldev_h=$(GLSRC)gxcldev.h
gxclpage_h=$(GLSRC)gxclpage.h
gxclpath_h=$(GLSRC)gxclpath.h

clbase1_=$(GLOBJ)gxclist.$(OBJ) $(GLOBJ)gxclbits.$(OBJ) $(GLOBJ)gxclpage.$(OBJ)
clbase2_=$(GLOBJ)gxclrast.$(OBJ) $(GLOBJ)gxclread.$(OBJ) $(GLOBJ)gxclrect.$(OBJ)
clbase3_=$(GLOBJ)gxclutil.$(OBJ) $(GLOBJ)gsparams.$(OBJ) $(GLOBJ)gsparaml.$(OBJ) $(GLOBJ)gsparamx.$(OBJ) $(GLOBJ)gxshade6.$(OBJ)
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
 $(gscms_h) $(gsicc_manage_h) $(gsicc_cache_h) $(gxdevsop_h) $(gxobj_h) \
 $(LIB_MAK) $(MAKEDIRS)
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

$(GLOBJ)gxclread.$(OBJ) : $(GLSRC)gxclread.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gp_h) $(gpcheck_h) $(gdevplnx_h) $(gdevprn_h) $(gscoord_h)\
 $(gsdevice_h) $(gxcldev_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h)\
 $(gxhttile_h) $(gsmemory_h) $(stream_h) $(strimpl_h) $(gsicc_cache_h)\
 $(gdevp14_h) $(LIB_MAK) $(MAKEDIRS)
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

$(GLOBJ)gxclpath.$(OBJ) : $(GLSRC)gxclpath.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gpcheck_h) $(gsptype2_h) $(gsptype1_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gxcldev_h) $(gxclpath_h) $(gxcolor2_h)\
 $(gxdcolor_h) $(gxpaint_h) $(gxdevsop_h) $(gzpath_h) $(gzcpath_h)\
 $(stream_h) $(gsserial_h) $(gxpcolor_h) $(LIB_MAK) $(MAKEDIRS)
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
 $(assert__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclfile.$(OBJ) $(C_) $(GLSRC)gxclfile.c

# Implement band lists in memory (RAM).

clmemory_=$(GLOBJ)gxclmem.$(OBJ) $(GLOBJ)gxcl$(BAND_LIST_COMPRESSOR).$(OBJ)
$(GLD)clmemory.dev : $(LIB_MAK) $(ECHOGS_XE) $(clmemory_) $(GLD)s$(BAND_LIST_COMPRESSOR)e.dev \
  $(GLD)s$(BAND_LIST_COMPRESSOR)d.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)clmemory $(clmemory_)
	$(ADDMOD) $(GLD)clmemory -include $(GLD)s$(BAND_LIST_COMPRESSOR)e
	$(ADDMOD) $(GLD)clmemory -include $(GLD)s$(BAND_LIST_COMPRESSOR)d
	$(ADDMOD) $(GLD)clmemory -init gxclmem

gxclmem_h=$(GLSRC)gxclmem.h

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
$(GLOBJ)gxclthrd.$(OBJ) :  $(GLSRC)gxclthrd.c $(gxsync_h) $(AK) $(gxclthrd_h)\
 $(gdevplnx_h) $(gdevprn_h) $(gp_h) $(gpcheck_h) $(gsdevice_h) $(gserrors_h)\
 $(gsmchunk_h) $(gsmemory_h) $(gx_h) $(gxcldev_h) $(gdevdevn_h)\
 $(gsicc_cache_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) $(memory__h)\
 $(gsicc_manage_h) $(gdevppla_h) $(gstrans_h) $(gzht_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxclthrd.$(OBJ) $(C_) $(GLSRC)gxclthrd.c

$(GLOBJ)gsmchunk.$(OBJ) :  $(GLSRC)gsmchunk.c $(AK) $(gx_h) $(gsstype_h)\
 $(gserrors_h) $(gsmchunk_h) $(memory__h) $(gxsync_h) $(malloc__h)\
 $(gsstruct_h) $(gxobj_h) $(assert__h) $(gsmdebug_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsmchunk.$(OBJ) $(C_) $(GLSRC)gsmchunk.c

# ---------------- Vector devices ---------------- #
# We include this here for the same reasons as page.dev.

gdevvec_h=$(GLSRC)gdevvec.h

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

$(GLOBJ)siscale_0.$(OBJ) : $(GLSRC)siscale.c $(AK)\
 $(math__h) $(memory__h) $(stdio__h) $(stdint__h) $(gdebug_h) $(gxfrac_h)\
 $(siscale_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)siscale_0.$(OBJ) $(C_) $(GLSRC)siscale.c

$(GLOBJ)siscale_1.$(OBJ) : $(GLSRC)siscale_cal.c $(AK) $(cal_h)\
 $(math__h) $(memory__h) $(stdio__h) $(stdint__h) $(gdebug_h) $(gxfrac_h)\
 $(siscale_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)siscale_1.$(OBJ) $(I_)$(CALSRCDIR)$(_I) $(C_) $(GLSRC)siscale_cal.c

$(GLOBJ)siscale.$(OBJ) : $(GLOBJ)siscale_$(WITH_CAL).$(OBJ) $(AK)\
 $(math__h) $(memory__h) $(stdio__h) $(stdint__h) $(gdebug_h) $(gxfrac_h)\
 $(siscale_h) $(strimpl_h) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)siscale_$(WITH_CAL).$(OBJ) $(GLOBJ)siscale.$(OBJ)

# Dev file contains library files (or nothing)
$(GLOBJ)siscale.dev : $(GLOBJ)siscale_$(WITH_CAL).dev\
 $(CAL_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)siscale_$(WITH_CAL).dev $(GLOBJ)siscale.dev

$(GLOBJ)siscale_0.dev: $(ECHOGS_XE) $(MAKEDIRS)
	$(SETMOD) $(GLOBJ)siscale_0

$(GLOBJ)siscale_1.dev: $(GLOBJ)cal.dev $(ECHOGS_XE) $(CAL_OBJS) $(MAKEDIRS)
	$(SETMOD) $(GLOBJ)siscale_1 -include $(GLOBJ)cal.dev

$(GLOBJ)sidscale.$(OBJ) : $(GLSRC)sidscale.c $(AK)\
 $(math__h) $(memory__h) $(stdio__h) $(gdebug_h) $(gxdda_h) $(gxfixed_h)\
 $(sidscale_h) $(strimpl_h) $(gxfrac_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)sidscale.$(OBJ) $(C_) $(GLSRC)sidscale.c

# -------------- imagemask scaling filter --------------- #

simscale_=$(GLOBJ)simscale.$(OBJ) $(GLOBJ)simscale_foo.$(OBJ)
$(GLD)simscale.dev : $(LIB_MAK) $(ECHOGS_XE) $(simscale_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)simscale $(simscale_)

$(GLOBJ)simscale.$(OBJ) : $(GLSRC)simscale.c $(AK) $(memory__h)\
 $(gserrors_h) $(simscale_h) $(strimpl_h) $(sisparam_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)simscale.$(OBJ) $(C_) $(GLSRC)simscale.c

$(GLOBJ)simscale_foo.$(OBJ) : $(GLSRC)simscale_foo.c $(AK) $(simscale_foo_h)\
  $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)simscale_foo.$(OBJ) $(C_) $(GLSRC)simscale_foo.c

# ---------------- Extended halftone support ---------------- #
# This is only used by one non-PostScript-based project.

gshtx_h=$(GLSRC)gshtx.h

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

$(GLOBJ)gdevdrop_1.$(OBJ) : $(GLSRC)gdevdrop.c $(AK) $(gx_h) $(gserrors_h) \
 $(memory__h) $(gxdevsop_h) $(gsbittab_h) $(gsropt_h) $(gxcindex_h) \
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) \
 $(gdevmem_h) $(gdevmrop_h) $(gdevmpla_h) $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gdevdrop_1.$(OBJ) $(C_) $(GLSRC)gdevdrop.c

$(GLOBJ)gdevdrop_0.$(OBJ) : $(GLSRC)gdevdrop.c $(AK) $(gx_h) $(gserrors_h) \
 $(memory__h) $(gxdevsop_h) $(gsbittab_h) $(gsropt_h) $(gxcindex_h) \
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxgetbit_h) \
 $(gdevmem_h) $(gdevmrop_h) $(gdevmpla_h) $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevdrop_0.$(OBJ) $(C_) $(GLSRC)gdevdrop.c

$(GLOBJ)gdevdrop.$(OBJ) : $(GLOBJ)gdevdrop_$(WITH_CAL).$(OBJ)
	$(CP_) $(GLOBJ)gdevdrop_$(WITH_CAL).$(OBJ) $(GLOBJ)gdevdrop.$(OBJ)

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
 $(gsroprun1_h) $(gsroprun8_h) $(gsroprun24_h) $(gp_h) $(gxcindex_h) \
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsroprun.$(OBJ) $(C_) $(GLSRC)gsroprun.c

# ---------------- TrueType and PostScript Type 42 fonts ---------------- #

ttflib_=$(GLOBJ)gstype42.$(OBJ) $(GLOBJ)gxchrout.$(OBJ) \
 $(GLOBJ)ttcalc.$(OBJ) $(GLOBJ)ttfinp.$(OBJ) $(GLOBJ)ttfmain.$(OBJ) $(GLOBJ)ttfmemd.$(OBJ) \
 $(GLOBJ)ttinterp.$(OBJ) $(GLOBJ)ttload.$(OBJ) $(GLOBJ)ttobjs.$(OBJ) \
 $(GLOBJ)gxttfb.$(OBJ) $(GLOBJ)gzspotan.$(OBJ)

$(GLD)ttflib.dev : $(LIB_MAK) $(ECHOGS_XE) $(ttflib_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)ttflib $(ttflib_)

# "gxfont42_h=$(GLSRC)gxfont42.h" already defined above
gxttf_h=$(GLSRC)gxttf.h

$(GLOBJ)gstype42.$(OBJ) : $(GLSRC)gstype42.c $(AK) $(gx_h) $(gserrors_h)\
 $(memory__h) $(gsccode_h) $(gsline_h) $(gsmatrix_h) $(gsstruct_h) $(gsutil_h)\
 $(gxchrout_h) $(gxfixed_h) $(gxfont_h) $(gxfont42_h) $(gxpath_h) $(gxttf_h)\
 $(gxttfb_h) $(gxtext_h) $(gxchar_h) $(gxfcache_h) $(gxgstate_h) $(gzstate_h)\
 $(stdint__h) $(strimpl_h) $(stream_h) $(strmio_h) $(szlibx_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstype42.$(OBJ) $(C_) $(GLSRC)gstype42.c

ttfsfnt_h=$(GLSRC)ttfsfnt.h
ttcommon_h=$(GLSRC)ttcommon.h
ttconf_h=$(GLSRC)ttconf.h
ttfinp_h=$(GLSRC)ttfinp.h
ttfmemd_h=$(GLSRC)ttfmemd.h
tttype_h=$(GLSRC)tttype.h
ttconfig_h=$(GLSRC)ttconfig.h
tttypes_h=$(GLSRC)tttypes.h
ttmisc_h=$(GLSRC)ttmisc.h
tttables_h=$(GLSRC)tttables.h
ttobjs_h=$(GLSRC)ttobjs.h
ttcalc_h=$(GLSRC)ttcalc.h
ttinterp_h=$(GLSRC)ttinterp.h
ttload_h=$(GLSRC)ttload.h
gxhintn_h=$(GLSRC)gxhintn.h

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

gxcid_h=$(GLSRC)gxcid.h
gxfcid_h=$(GLSRC)gxfcid.h
gxfcmap_h=$(GLSRC)gxfcmap.h
gxfcmap1_h=$(GLSRC)gxfcmap1.h
gxfont0c_h=$(GLSRC)gxfont0c.h

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
 $(gsstruct_h) $(gsutil_h) $(gp_h) $(gxcoord_h) $(gxgetbit_h)\
 $(gxcolor2_h) $(gxcspace_h) $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gxfixed_h) $(gxmatrix_h) $(gxpcolor_h) $(gxclist_h) $(gxcldev_h)\
 $(gzstate_h) $(gdevp14_h) $(gdevmpla_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxpcmap.$(OBJ) $(C_) $(GLSRC)gxpcmap.c

# ---------------- PostScript Type 1 (and Type 4) fonts ---------------- #

type1lib_=$(GLOBJ)gxtype1.$(OBJ)\
 $(GLOBJ)gxhintn.$(OBJ) $(GLOBJ)gxhintn1.$(OBJ) $(GLOBJ)gscrypt1.$(OBJ) $(GLOBJ)gxchrout.$(OBJ)

gscrypt1_h=$(GLSRC)gscrypt1.h
gstype1_h=$(GLSRC)gstype1.h
gxfont1_h=$(GLSRC)gxfont1.h
gxtype1_h=$(GLSRC)gxtype1.h

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

$(GLOBJ)gxicolor_0.$(OBJ) : $(GLSRC)gxicolor.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gxarith_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxmatrix_h)\
 $(gsccolor_h) $(gspaint_h) $(gzstate_h)\
 $(gxdevice_h) $(gxcmap_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxgstate_h) $(gxdevmem_h) $(gxcpath_h) $(gximage_h)\
 $(gsicc_h) $(gsicc_cache_h) $(gsicc_cms_h) $(gxcie_h)\
 $(gscie_h) $(gzht_h) $(gxht_thresh_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxicolor_0.$(OBJ) $(C_) $(GLSRC)gxicolor.c

$(GLOBJ)gxicolor_1.$(OBJ) : $(GLSRC)gxicolor.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gpcheck_h) $(gxarith_h)\
 $(gxfixed_h) $(gxfrac_h) $(gxmatrix_h)\
 $(gsccolor_h) $(gspaint_h) $(gzstate_h)\
 $(gxdevice_h) $(gxcmap_h) $(gxdcconv_h) $(gxdcolor_h)\
 $(gxgstate_h) $(gxdevmem_h) $(gxcpath_h) $(gximage_h)\
 $(gsicc_h) $(gsicc_cache_h) $(gsicc_cms_h) $(gxcie_h)\
 $(gscie_h) $(gzht_h) $(gxht_thresh_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gxicolor_1.$(OBJ) $(C_) $(GLSRC)gxicolor.c

$(GLOBJ)gxicolor.$(OBJ) : $(GLOBJ)gxicolor_$(WITH_CAL).$(OBJ)
	$(CP_) $(GLOBJ)gxicolor_$(WITH_CAL).$(OBJ) $(GLOBJ)gxicolor.$(OBJ)

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

$(GLOBJ)gxiscale.$(OBJ) : $(GLSRC)gxiscale.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(stdint__h) $(gpcheck_h) $(gsccolor_h) $(gspaint_h)\
 $(sidscale_h) $(gxdevsop_h) $(gxarith_h) $(gxcmap_h) $(gxcpath_h)\
 $(gxdcolor_h) $(gxdevice_h) $(gxdevmem_h) $(gxfixed_h) $(gxfrac_h)\
 $(gximage_h) $(gxgstate_h) $(gxmatrix_h) $(siinterp_h) $(siscale_h)\
 $(stream_h) $(gscindex_h) $(gxcolor2_h) $(gscspace_h) $(gsicc_cache_h)\
 $(gsicc_manage_h) $(gsicc_h) $(gsbitops_h) $(LIB_MAK) $(MAKEDIRS)
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

gsdsrc_h=$(GLSRC)gsdsrc.h
gsfunc_h=$(GLSRC)gsfunc.h
gsfunc0_h=$(GLSRC)gsfunc0.h
gxfunc_h=$(GLSRC)gxfunc.h

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

gsfunc4_h=$(GLSRC)gsfunc4.h

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

gscpixel_h=$(GLSRC)gscpixel.h

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
 $(GLOBJ)gsicc_replacecm.$(OBJ) $(GLOBJ)gsicc_monitorcm.$(OBJ)\
 $(GLOBJ)gsicc_blacktext.$(OBJ)

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
 $(gsicc_manage_h) $(gxdevice_h) $(gsccolor_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc.$(OBJ) $(C_) $(GLSRC)gsicc.c

gscms_h=$(GLSRC)gscms.h
gsicc_cms_h=$(GLSRC)gsicc_cms.h
gsicc_manage_h=$(GLSRC)gsicc_manage.h
gsicc_cache_h=$(GLSRC)gsicc_cache.h
gsicc_profilecache_h=$(GLSRC)gsicc_profilecache.h
gsicc_blacktext_h=$(GLSRC)gsicc_blacktext.h

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
 $(gpsync_h) $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_cache.$(OBJ) $(C_) $(GLSRC)gsicc_cache.c

$(GLOBJ)gsicc_profilecache.$(OBJ) : $(GLSRC)gsicc_profilecache.c $(AK)\
 $(std_h) $(stdpre_h) $(gstypes_h) $(gsmemory_h) $(gsstruct_h) $(scommon_h)\
 $(gscms_h) $(gsicc_profilecache_h) $(gzstate_h) $(gserrors_h) $(gx_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_profilecache.$(OBJ) $(C_) $(GLSRC)gsicc_profilecache.c

$(GLOBJ)gsicc_blacktext.$(OBJ) : $(GLSRC)gsicc_blacktext.c $(AK)\
 $(gsmemory_h) $(gsstruct_h) $(gzstate_h) $(gsicc_blacktext_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsicc_blacktext.$(OBJ) $(C_) $(GLSRC)gsicc_blacktext.c

$(GLOBJ)gsicc_lcms2mt_1_0.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(gslibctx_h) $(gserrors_h) $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_lcms2mt_1_0.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c

$(GLOBJ)gsicc_lcms2mt_0_0.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(lcms2mt_h) $(gslibctx_h) $(lcms2mt_plugin_h) $(gserrors_h) \
 $(gxdevice_h) $(lcms2mt_cobalt_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLLCMS2MTCC) $(GLO_)gsicc_lcms2mt_0_0.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c

$(GLOBJ)gsicc_lcms2mt_1_1.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(gslibctx_h) $(gserrors_h)\
 $(gxdevice_h) $(LIB_MAK) $(MAKEDIRS) $(cal_h)
	$(GLLCMS2MTCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gsicc_lcms2mt_1_1.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c

$(GLOBJ)gsicc_lcms2mt_0_1.$(OBJ) : $(GLSRC)gsicc_lcms2mt.c\
 $(memory__h) $(gsicc_cms_h) $(lcms2mt_h) $(gslibctx_h) $(lcms2mt_plugin_h) $(gserrors_h) \
 $(gxdevice_h) $(lcms2mt_cobalt_h) $(LIB_MAK) $(MAKEDIRS) $(cal_h)
	$(GLLCMS2MTCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gsicc_lcms2mt_0_1.$(OBJ) $(C_) $(GLSRC)gsicc_lcms2mt.c

$(GLOBJ)gsicc_lcms2mt.$(OBJ) : $(GLOBJ)gsicc_lcms2mt_$(SHARE_LCMS)_$(WITH_CAL).$(OBJ) $(gp_h) \
 $(gxsync_h) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gsicc_lcms2mt_$(SHARE_LCMS)_$(WITH_CAL).$(OBJ) $(GLOBJ)gsicc_lcms2mt.$(OBJ)

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

gstrap_h=$(GLSRC)gstrap.h

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

$(GLOBJ)gsequivc.$(OBJ) : $(GLSRC)gsequivc.c $(math__h) $(PDEVH) $(gsparam_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gsequivc_h) $(gzstate_h)\
 $(gsstate_h) $(gscspace_h) $(gxcspace_h) $(gsicc_manage_h) $(gxdevsop_h)\
 $(gdevprn_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsequivc.$(OBJ) $(C_) $(GLSRC)gsequivc.c


# ---------------- Transparency ---------------- #

gsipar3x_h=$(GLSRC)gsipar3x.h
gximag3x_h=$(GLSRC)gximag3x.h
gxblend_h=$(GLSRC)gxblend.h
gdevp14_h=$(GLSRC)gdevp14.h

$(GLOBJ)gstrans.$(OBJ) : $(GLSRC)gstrans.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gdevp14_h) $(gstrans_h) $(gsicc_cache_h)\
 $(gsutil_h) $(gxdevcli_h) $(gzstate_h) $(gscspace_h)\
 $(gxclist_h) $(gsicc_manage_h) $(gdevdevn_h) $(gxarith_h) $(gxblend_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gstrans.$(OBJ) $(C_) $(GLSRC)gstrans.c

$(GLOBJ)gximag3x.$(OBJ) : $(GLSRC)gximag3x.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gdevbbox_h)\
 $(gsbitops_h) $(gscpixel_h) $(gscspace_h) $(gsstruct_h)\
 $(gxdevice_h) $(gxdevmem_h) $(gximag3x_h) $(gxgstate_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gximag3x.$(OBJ) $(C_) $(GLSRC)gximag3x.c

$(GLOBJ)gxblend_0.$(OBJ) : $(GLSRC)gxblend.c $(AK) $(gx_h) $(memory__h)\
 $(gstparam_h) $(gxblend_h) $(gxcolor2_h) $(gsicc_cache_h) $(gsrect_h)\
 $(gsicc_manage_h) $(gdevp14_h) $(gp_h) $(math__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxblend_0.$(OBJ) $(C_) $(GLSRC)gxblend.c

$(GLOBJ)gxblend_1.$(OBJ) : $(GLSRC)gxblend.c $(AK) $(gx_h) $(memory__h)\
 $(gstparam_h) $(gxblend_h) $(gxcolor2_h) $(gsicc_cache_h) $(gsrect_h)\
 $(gsicc_manage_h) $(gdevp14_h) $(gp_h) $(math__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gxblend_1.$(OBJ) $(C_) $(GLSRC)gxblend.c

$(GLOBJ)gxblend.$(OBJ) : $(GLOBJ)gxblend_$(WITH_CAL).$(OBJ) $(AK) $(gx_h)\
 $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gxblend_$(WITH_CAL).$(OBJ) $(GLOBJ)gxblend.$(OBJ)

$(GLOBJ)gxblend1.$(OBJ) : $(GLSRC)gxblend1.c $(AK) $(gx_h) $(memory__h)\
 $(gstparam_h) $(gsrect_h) $(gxdcconv_h) $(gxblend_h) $(gxdevcli_h)\
 $(gxgstate_h) $(gdevdevn_h) $(gdevp14_h) $(png__h) $(gp_h)\
 $(gsicc_cache_h) $(gxdevsop_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gxblend1.$(OBJ) $(C_) $(GLSRC)gxblend1.c

$(GLOBJ)gdevp14_0.$(OBJ) : $(GLSRC)gdevp14.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gscdefs_h) $(gxdevice_h) $(gsdevice_h)\
 $(gsstruct_h) $(gscoord_h) $(gxgstate_h) $(gxdcolor_h) $(gxiparam_h)\
 $(gstparam_h) $(gxblend_h) $(gxtext_h) $(gsimage_h)\
 $(gsrect_h) $(gzstate_h) $(gdevdevn_h) $(gdevp14_h) $(gdevprn_h) $(gdevppla_h) $(gdevdevnprn_h)\
 $(gsovrc_h) $(gxcmap_h) $(gscolor1_h) $(gstrans_h) $(gsutil_h) $(gxcldev_h) $(gxclpath_h)\
 $(gxdcconv_h) $(gsptype2_h) $(gxpcolor_h) $(gscdevn_h)\
 $(gsptype1_h) $(gzcpath_h) $(gxpaint_h) $(gsicc_manage_h) $(gxclist_h)\
 $(gxiclass_h) $(gximage_h) $(gsmatrix_h) $(gsicc_cache_h) $(gxdevsop_h)\
 $(gsicc_h) $(gscms_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gdevp14_0.$(OBJ) $(C_) $(GLSRC)gdevp14.c

$(GLOBJ)gdevp14_1.$(OBJ) : $(GLSRC)gdevp14.c $(AK) $(gx_h) $(gserrors_h)\
 $(math__h) $(memory__h) $(gscdefs_h) $(gxdevice_h) $(gsdevice_h)\
 $(gsstruct_h) $(gscoord_h) $(gxgstate_h) $(gxdcolor_h) $(gxiparam_h)\
 $(gstparam_h) $(gxblend_h) $(gxtext_h) $(gsimage_h)\
 $(gsrect_h) $(gzstate_h) $(gdevdevn_h) $(gdevp14_h) $(gdevprn_h) $(gdevppla_h) $(gdevdevnprn_h)\
 $(gsovrc_h) $(gxcmap_h) $(gscolor1_h) $(gstrans_h) $(gsutil_h) $(gxcldev_h) $(gxclpath_h)\
 $(gxdcconv_h) $(gsptype2_h) $(gxpcolor_h) $(gscdevn_h)\
 $(gsptype1_h) $(gzcpath_h) $(gxpaint_h) $(gsicc_manage_h) $(gxclist_h)\
 $(gxiclass_h) $(gximage_h) $(gsmatrix_h) $(gsicc_cache_h) $(gxdevsop_h)\
 $(gsicc_h) $(gscms_h) $(gdevmem_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(GLO_)gdevp14_1.$(OBJ) $(C_) $(GLSRC)gdevp14.c

$(GLOBJ)gdevp14.$(OBJ) : $(GLOBJ)gdevp14_$(WITH_CAL).$(OBJ) $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLOBJ)gdevp14_$(WITH_CAL).$(OBJ) $(GLOBJ)gdevp14.$(OBJ)

translib_=$(GLOBJ)gstrans.$(OBJ) $(GLOBJ)gximag3x.$(OBJ)\
 $(GLOBJ)gxblend.$(OBJ) $(GLOBJ)gxblend1.$(OBJ) $(GLOBJ)gdevp14.$(OBJ) $(GLOBJ)gdevdevn.$(OBJ)\
 $(GLOBJ)gsequivc.$(OBJ)  $(GLOBJ)gdevdcrd.$(OBJ)

$(GLD)translib.dev : $(LIB_MAK) $(ECHOGS_XE) $(translib_)\
 $(GLD)cspixlib.dev $(GLD)bboxutil.dev $(GLD)cielib.dev $(GLD)page.dev $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)translib $(translib_)
	$(ADDMOD) $(GLD)translib -imagetype 3x
	$(ADDMOD) $(GLD)translib -include $(GLD)cspixlib $(GLD)bboxutil $(GLD)page
	$(ADDMOD) $(GLD)translib -include $(GLD)cielib.dev

# ---------------- Smooth shading ---------------- #

gscolor3_h=$(GLSRC)gscolor3.h
gsfunc3_h=$(GLSRC)gsfunc3.h
gsshade_h=$(GLSRC)gsshade.h
gxshade_h=$(GLSRC)gxshade.h
gxshade4_h=$(GLSRC)gxshade4.h

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
 $(std_h) $(gsmemory_h) $(math__h) $(string__h) $(gp_h) $(LIB_MAK) $(MAKEDIRS)
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
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(TESS_ROMFS_ARGS) $(PS_ROMFS_ARGS) $(PS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)gsromfs1_1.c : $(MKROMFS_XE) $(PS_ROMFS_DEPS) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)gsromfs1_1.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PS_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)gsromfs1.c : $(GLGEN)gsromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)gsromfs1_$(UFST_BRIDGE).c $(GLGEN)gsromfs1.c

# pcl
$(GLGEN)pclromfs1_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs1_.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PCLXL_FONT_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
	$(PJL_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)pclromfs1_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs1_1.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
	$(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)pclromfs1.c : $(GLGEN)pclromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pclromfs1_$(UFST_BRIDGE).c $(GLGEN)pclromfs1.c

$(GLGEN)pclromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs0_.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)pclromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pclromfs0_1.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)pclromfs0.c : $(GLGEN)pclromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pclromfs0_$(UFST_BRIDGE).c $(GLGEN)pclromfs0.c

# xps
$(GLGEN)xpsromfs1_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs1_.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(XPS_ROMFS_ARGS) $(XPS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)xpsromfs1_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs1_1.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(XPS_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)xpsromfs1.c : $(GLGEN)xpsromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)xpsromfs1_$(UFST_BRIDGE).c $(GLGEN)xpsromfs1.c

$(GLGEN)xpsromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs0_.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)xpsromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)xpsromfs0_1.c \
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

$(GLGEN)xpsromfs0.c : $(GLGEN)xpsromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)xpsromfs0_$(UFST_BRIDGE).c $(GLGEN)xpsromfs0.c

# pdf
$(GLGEN)pdfromfs1_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdfromfs1_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PDF_ROMFS_ARGS) $(PDF_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)pdfromfs1_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdfromfs1_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PDF_ROMFS_ARGS) $(GL_ROMFS_ARGS)

$(GLGEN)pdfromfs1.c : $(GLGEN)pdfromfs1_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pdfromfs1_$(UFST_BRIDGE).c $(GLGEN)pdfromfs1.c

$(GLGEN)pdfromfs0_.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdfromfs0_.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)pdfromfs0_1.c : $(MKROMFS_XE) $(LIB_MAK) $(MAKEDIRS)
	$(EXP)$(MKROMFS_XE) -o $(GLGEN)pdfromfs0_1.c \
	-X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(GL_ROMFS_ARGS)

$(GLGEN)pdfromfs0.c : $(GLGEN)pdfromfs0_$(UFST_BRIDGE).c $(LIB_MAK) $(MAKEDIRS)
	$(CP_) $(GLGEN)pdfromfs0_$(UFST_BRIDGE).c $(GLGEN)pdfromfs0.c

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
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(PCLXL_ROMFS_ARGS) $(PCLXL_FONT_ROMFS_ARGS) $(PJL_ROMFS_ARGS) \
        $(XPS_ROMFS_ARGS) $(XPS_FONT_ROMFS_ARGS) \
	$(PS_ROMFS_ARGS) $(PS_FONT_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

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
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
	$(UFST_ROMFS_ARGS) $(PCLXL_ROMFS_ARGS) $(PJL_ROMFS_ARGS) $(XPS_ROMFS_ARGS) \
	$(PS_ROMFS_ARGS) $(GL_ROMFS_ARGS) $(TESS_ROMFS_ARGS)

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
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
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
	$(MKROMFS_FLAGS) -X .svn -X CVS -P $(GLSRCDIR)$(D)..$(D) iccprofiles$(D)* \
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

# A pdfromfs module with only ICC profiles  for COMPILE_INITS=0
$(GLOBJ)pdfromfs0.$(OBJ) : $(GLGEN)pdfromfs0.c $(stdint__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdfromfs0.$(OBJ) $(C_) $(GLGEN)pdfromfs0.c

$(GLOBJ)pdfromfs1.$(OBJ) : $(GLOBJ)pdfromfs1.c $(time__h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)pdfromfs1.$(OBJ) $(C_) $(GLOBJ)pdfromfs1.c

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
 $(std_h) $(gpsync_h) $(gserrors_h) $(assert__h) $(unistd__h) $(LIB_MAK)\
 $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_psync.$(OBJ) $(C_) $(GLSRC)gp_psync.c

# Other stuff.

# Other MS-DOS facilities.
$(GLOBJ)gp_msdos.$(OBJ) : $(GLSRC)gp_msdos.c $(AK)\
 $(dos__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gp_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_msdos.$(OBJ) $(C_) $(GLSRC)gp_msdos.c

# Dummy XPS printing function - the only real one is in the Windows
# platform code
$(GLOBJ)gp_nxpsprn.$(OBJ) : $(GLSRC)gp_nxpsprn.c $(gp_h) $(std_h) $(LIB_MAK) \
 $(AK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gp_nxpsprn.$(OBJ) $(C_) $(GLSRC)gp_nxpsprn.c

# ================ Adobe Glyph List ================ #

gsagl_=$(GLOBJ)gsagl.$(OBJ)
$(GLD)gsagl.dev : $(LIB_MAK) $(ECHOGS_XE) $(gsagl_) $(LIB_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)gsagl $(gsagl_)

$(GLOBJ)gsagl.$(OBJ) : $(GLSRC)gsagl.c $(GDEV)\
 $(gsagl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gsagl.$(OBJ) $(C_) $(GLSRC)gsagl.c

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
 $(math__h) $(stdio__h) $(string__h) $(gx_h) $(gp_h) $(gsalloc_h)\
 $(gserrors_h) $(gsmatrix_h) $(gsrop_h) $(gsstate_h) $(gscspace_h)\
 $(gscdefs_h) $(gscie_h) $(gscolor2_h) $(gscoord_h) $(gscrd_h)\
 $(gshtx_h) $(gsiparm3_h) $(gsiparm4_h) $(gslib_h) $(gsparam_h)\
 $(gspaint_h) $(gspath_h) $(gspath2_h) $(gsstruct_h) $(gsutil_h)\
 $(gxalloc_h) $(gxdcolor_h) $(gxdevice_h) $(gxht_h) $(gdevbbox_h)\
 $(gxiodev_h) $(LIB_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)gslib.$(OBJ) $(C_) $(GLSRC)gslib.c

# Dependencies:
$(GLSRC)gdevdcrd.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxtext.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gstext.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gstparam.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscsel.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsfont.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsimage.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsropt.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxdda.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxpath.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxftype.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscms.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsrect.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gslparam.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscpm.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscspace.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscompt.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gspenum.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsparam.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gp.h
$(GLSRC)gdevdcrd.h:$(GLSRC)memento.h
$(GLSRC)gdevdcrd.h:$(GLSRC)memory_.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsuid.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxsync.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevdcrd.h:$(GLSRC)srdline.h
$(GLSRC)gdevdcrd.h:$(GLSRC)scommon.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxarith.h
$(GLSRC)gdevdcrd.h:$(GLSRC)stat_.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gpsync.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsstype.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevdcrd.h:$(GLSRC)stdio_.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gsccode.h
$(GLSRC)gdevdcrd.h:$(GLSRC)stdint_.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gstypes.h
$(GLSRC)gdevdcrd.h:$(GLSRC)std.h
$(GLSRC)gdevdcrd.h:$(GLSRC)stdpre.h
$(GLSRC)gdevdcrd.h:$(GLGEN)arch.h
$(GLSRC)gdevdcrd.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxtext.h
$(GLSRC)gdevpccm.h:$(GLSRC)gstext.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevpccm.h:$(GLSRC)gstparam.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscsel.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsfont.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsimage.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsropt.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxdda.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxpath.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxftype.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscms.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsrect.h
$(GLSRC)gdevpccm.h:$(GLSRC)gslparam.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscpm.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscspace.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscompt.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevpccm.h:$(GLSRC)gspenum.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsparam.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevpccm.h:$(GLSRC)gp.h
$(GLSRC)gdevpccm.h:$(GLSRC)memento.h
$(GLSRC)gdevpccm.h:$(GLSRC)memory_.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsuid.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxsync.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevpccm.h:$(GLSRC)srdline.h
$(GLSRC)gdevpccm.h:$(GLSRC)scommon.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxarith.h
$(GLSRC)gdevpccm.h:$(GLSRC)stat_.h
$(GLSRC)gdevpccm.h:$(GLSRC)gpsync.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsstype.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevpccm.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevpccm.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevpccm.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevpccm.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevpccm.h:$(GLSRC)stdio_.h
$(GLSRC)gdevpccm.h:$(GLSRC)gsccode.h
$(GLSRC)gdevpccm.h:$(GLSRC)stdint_.h
$(GLSRC)gdevpccm.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevpccm.h:$(GLSRC)gstypes.h
$(GLSRC)gdevpccm.h:$(GLSRC)std.h
$(GLSRC)gdevpccm.h:$(GLSRC)stdpre.h
$(GLSRC)gdevpccm.h:$(GLGEN)arch.h
$(GLSRC)gdevpccm.h:$(GLSRC)gs_dll_call.h
$(GLSRC)stdint_.h:$(GLSRC)std.h
$(GLSRC)stdint_.h:$(GLSRC)stdpre.h
$(GLSRC)stdint_.h:$(GLGEN)arch.h
$(GLSRC)gstypes.h:$(GLSRC)stdpre.h
$(GLSRC)srdline.h:$(GLSRC)scommon.h
$(GLSRC)srdline.h:$(GLSRC)gsstype.h
$(GLSRC)srdline.h:$(GLSRC)gsmemory.h
$(GLSRC)srdline.h:$(GLSRC)gslibctx.h
$(GLSRC)srdline.h:$(GLSRC)stdio_.h
$(GLSRC)srdline.h:$(GLSRC)stdint_.h
$(GLSRC)srdline.h:$(GLSRC)gssprintf.h
$(GLSRC)srdline.h:$(GLSRC)gstypes.h
$(GLSRC)srdline.h:$(GLSRC)std.h
$(GLSRC)srdline.h:$(GLSRC)stdpre.h
$(GLSRC)srdline.h:$(GLGEN)arch.h
$(GLSRC)srdline.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gpmisc.h:$(GLSRC)gp.h
$(GLSRC)gpmisc.h:$(GLSRC)memory_.h
$(GLSRC)gpmisc.h:$(GLSRC)srdline.h
$(GLSRC)gpmisc.h:$(GLSRC)scommon.h
$(GLSRC)gpmisc.h:$(GLSRC)stat_.h
$(GLSRC)gpmisc.h:$(GLSRC)gsstype.h
$(GLSRC)gpmisc.h:$(GLSRC)gsmemory.h
$(GLSRC)gpmisc.h:$(GLSRC)gpgetenv.h
$(GLSRC)gpmisc.h:$(GLSRC)gscdefs.h
$(GLSRC)gpmisc.h:$(GLSRC)gslibctx.h
$(GLSRC)gpmisc.h:$(GLSRC)stdio_.h
$(GLSRC)gpmisc.h:$(GLSRC)stdint_.h
$(GLSRC)gpmisc.h:$(GLSRC)gssprintf.h
$(GLSRC)gpmisc.h:$(GLSRC)gstypes.h
$(GLSRC)gpmisc.h:$(GLSRC)std.h
$(GLSRC)gpmisc.h:$(GLSRC)stdpre.h
$(GLSRC)gpmisc.h:$(GLGEN)arch.h
$(GLSRC)gpmisc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gp.h:$(GLSRC)memory_.h
$(GLSRC)gp.h:$(GLSRC)srdline.h
$(GLSRC)gp.h:$(GLSRC)scommon.h
$(GLSRC)gp.h:$(GLSRC)stat_.h
$(GLSRC)gp.h:$(GLSRC)gsstype.h
$(GLSRC)gp.h:$(GLSRC)gsmemory.h
$(GLSRC)gp.h:$(GLSRC)gpgetenv.h
$(GLSRC)gp.h:$(GLSRC)gscdefs.h
$(GLSRC)gp.h:$(GLSRC)gslibctx.h
$(GLSRC)gp.h:$(GLSRC)stdio_.h
$(GLSRC)gp.h:$(GLSRC)stdint_.h
$(GLSRC)gp.h:$(GLSRC)gssprintf.h
$(GLSRC)gp.h:$(GLSRC)gstypes.h
$(GLSRC)gp.h:$(GLSRC)std.h
$(GLSRC)gp.h:$(GLSRC)stdpre.h
$(GLSRC)gp.h:$(GLGEN)arch.h
$(GLSRC)gp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gpcheck.h:$(GLSRC)std.h
$(GLSRC)gpcheck.h:$(GLSRC)stdpre.h
$(GLSRC)gpcheck.h:$(GLGEN)arch.h
$(GLSRC)gpsync.h:$(GLSRC)stdint_.h
$(GLSRC)gpsync.h:$(GLSRC)std.h
$(GLSRC)gpsync.h:$(GLSRC)stdpre.h
$(GLSRC)gpsync.h:$(GLGEN)arch.h
$(GLSRC)gconf.h:$(GLGEN)gconfig.h
$(GLSRC)std.h:$(GLSRC)stdpre.h
$(GLSRC)std.h:$(GLGEN)arch.h
$(GLSRC)gsstrl.h:$(GLSRC)stdpre.h
$(GLSRC)ctype_.h:$(GLSRC)std.h
$(GLSRC)ctype_.h:$(GLSRC)stdpre.h
$(GLSRC)ctype_.h:$(GLGEN)arch.h
$(GLSRC)dirent_.h:$(GLGEN)gconfig_.h
$(GLSRC)dirent_.h:$(GLSRC)std.h
$(GLSRC)dirent_.h:$(GLSRC)stdpre.h
$(GLSRC)dirent_.h:$(GLGEN)arch.h
$(GLSRC)errno_.h:$(GLSRC)std.h
$(GLSRC)errno_.h:$(GLSRC)stdpre.h
$(GLSRC)errno_.h:$(GLGEN)arch.h
$(GLSRC)fcntl_.h:$(GLSRC)std.h
$(GLSRC)fcntl_.h:$(GLSRC)stdpre.h
$(GLSRC)fcntl_.h:$(GLGEN)arch.h
$(GLSRC)locale_.h:$(GLSRC)std.h
$(GLSRC)locale_.h:$(GLSRC)stdpre.h
$(GLSRC)locale_.h:$(GLGEN)arch.h
$(GLSRC)malloc_.h:$(GLSRC)bobbin.h
$(GLSRC)malloc_.h:$(GLSRC)memento.h
$(GLSRC)malloc_.h:$(GLSRC)std.h
$(GLSRC)malloc_.h:$(GLSRC)stdpre.h
$(GLSRC)malloc_.h:$(GLGEN)arch.h
$(GLSRC)math_.h:$(GLSRC)vmsmath.h
$(GLSRC)math_.h:$(GLSRC)std.h
$(GLSRC)math_.h:$(GLSRC)stdpre.h
$(GLSRC)math_.h:$(GLGEN)arch.h
$(GLSRC)memory_.h:$(GLSRC)std.h
$(GLSRC)memory_.h:$(GLSRC)stdpre.h
$(GLSRC)memory_.h:$(GLGEN)arch.h
$(GLSRC)stat_.h:$(GLSRC)std.h
$(GLSRC)stat_.h:$(GLSRC)stdpre.h
$(GLSRC)stat_.h:$(GLGEN)arch.h
$(GLSRC)stdio_.h:$(GLSRC)gssprintf.h
$(GLSRC)stdio_.h:$(GLSRC)std.h
$(GLSRC)stdio_.h:$(GLSRC)stdpre.h
$(GLSRC)stdio_.h:$(GLGEN)arch.h
$(GLSRC)string_.h:$(GLSRC)gsstrtok.h
$(GLSRC)string_.h:$(GLSRC)gsstrl.h
$(GLSRC)string_.h:$(GLSRC)std.h
$(GLSRC)string_.h:$(GLSRC)stdpre.h
$(GLSRC)string_.h:$(GLGEN)arch.h
$(GLSRC)time_.h:$(GLGEN)gconfig_.h
$(GLSRC)time_.h:$(GLSRC)std.h
$(GLSRC)time_.h:$(GLSRC)stdpre.h
$(GLSRC)time_.h:$(GLGEN)arch.h
$(GLSRC)unistd_.h:$(GLSRC)std.h
$(GLSRC)unistd_.h:$(GLSRC)stdpre.h
$(GLSRC)unistd_.h:$(GLGEN)arch.h
$(GLSRC)pipe_.h:$(GLSRC)stdio_.h
$(GLSRC)pipe_.h:$(GLSRC)gssprintf.h
$(GLSRC)pipe_.h:$(GLSRC)std.h
$(GLSRC)pipe_.h:$(GLSRC)stdpre.h
$(GLSRC)pipe_.h:$(GLGEN)arch.h
$(GLSRC)jerror_.h:$(JSRCDIR)$(D)jerror.h
$(GLSRC)gxstdio.h:$(GLSRC)gsio.h
$(GLSRC)gslibctx.h:$(GLSRC)stdio_.h
$(GLSRC)gslibctx.h:$(GLSRC)gssprintf.h
$(GLSRC)gslibctx.h:$(GLSRC)std.h
$(GLSRC)gslibctx.h:$(GLSRC)stdpre.h
$(GLSRC)gslibctx.h:$(GLGEN)arch.h
$(GLSRC)gslibctx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdebug.h:$(GLSRC)gdbflags.h
$(GLSRC)gdebug.h:$(GLSRC)std.h
$(GLSRC)gdebug.h:$(GLSRC)stdpre.h
$(GLSRC)gdebug.h:$(GLGEN)arch.h
$(GLSRC)gsalloc.h:$(GLSRC)stdint_.h
$(GLSRC)gsalloc.h:$(GLSRC)std.h
$(GLSRC)gsalloc.h:$(GLSRC)stdpre.h
$(GLSRC)gsalloc.h:$(GLGEN)arch.h
$(GLSRC)gsargs.h:$(GLSRC)std.h
$(GLSRC)gsargs.h:$(GLSRC)stdpre.h
$(GLSRC)gsargs.h:$(GLGEN)arch.h
$(GLSRC)gsexit.h:$(GLSRC)std.h
$(GLSRC)gsexit.h:$(GLSRC)stdpre.h
$(GLSRC)gsexit.h:$(GLGEN)arch.h
$(GLSRC)gsgc.h:$(GLSRC)gsalloc.h
$(GLSRC)gsgc.h:$(GLSRC)stdint_.h
$(GLSRC)gsgc.h:$(GLSRC)std.h
$(GLSRC)gsgc.h:$(GLSRC)stdpre.h
$(GLSRC)gsgc.h:$(GLGEN)arch.h
$(GLSRC)gsmalloc.h:$(GLSRC)gxsync.h
$(GLSRC)gsmalloc.h:$(GLSRC)gpsync.h
$(GLSRC)gsmalloc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsmalloc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsmalloc.h:$(GLSRC)stdio_.h
$(GLSRC)gsmalloc.h:$(GLSRC)stdint_.h
$(GLSRC)gsmalloc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsmalloc.h:$(GLSRC)gstypes.h
$(GLSRC)gsmalloc.h:$(GLSRC)std.h
$(GLSRC)gsmalloc.h:$(GLSRC)stdpre.h
$(GLSRC)gsmalloc.h:$(GLGEN)arch.h
$(GLSRC)gsmalloc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsmchunk.h:$(GLSRC)std.h
$(GLSRC)gsmchunk.h:$(GLSRC)stdpre.h
$(GLSRC)gsmchunk.h:$(GLGEN)arch.h
$(GLSRC)valgrind.h:$(GLSRC)stdpre.h
$(GLSRC)gsmdebug.h:$(GLSRC)valgrind.h
$(GLSRC)gsmdebug.h:$(GLSRC)stdpre.h
$(GLSRC)gsmemory.h:$(GLSRC)gslibctx.h
$(GLSRC)gsmemory.h:$(GLSRC)stdio_.h
$(GLSRC)gsmemory.h:$(GLSRC)gssprintf.h
$(GLSRC)gsmemory.h:$(GLSRC)gstypes.h
$(GLSRC)gsmemory.h:$(GLSRC)std.h
$(GLSRC)gsmemory.h:$(GLSRC)stdpre.h
$(GLSRC)gsmemory.h:$(GLGEN)arch.h
$(GLSRC)gsmemory.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsmemret.h:$(GLSRC)gsmemory.h
$(GLSRC)gsmemret.h:$(GLSRC)gslibctx.h
$(GLSRC)gsmemret.h:$(GLSRC)stdio_.h
$(GLSRC)gsmemret.h:$(GLSRC)gssprintf.h
$(GLSRC)gsmemret.h:$(GLSRC)gstypes.h
$(GLSRC)gsmemret.h:$(GLSRC)std.h
$(GLSRC)gsmemret.h:$(GLSRC)stdpre.h
$(GLSRC)gsmemret.h:$(GLGEN)arch.h
$(GLSRC)gsmemret.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsnogc.h:$(GLSRC)gsgc.h
$(GLSRC)gsnogc.h:$(GLSRC)gsalloc.h
$(GLSRC)gsnogc.h:$(GLSRC)stdint_.h
$(GLSRC)gsnogc.h:$(GLSRC)std.h
$(GLSRC)gsnogc.h:$(GLSRC)stdpre.h
$(GLSRC)gsnogc.h:$(GLGEN)arch.h
$(GLSRC)gsrefct.h:$(GLSRC)memento.h
$(GLSRC)gsrefct.h:$(GLSRC)std.h
$(GLSRC)gsrefct.h:$(GLSRC)stdpre.h
$(GLSRC)gsrefct.h:$(GLGEN)arch.h
$(GLSRC)gsserial.h:$(GLSRC)stdpre.h
$(GLSRC)gsstype.h:$(GLSRC)gsmemory.h
$(GLSRC)gsstype.h:$(GLSRC)gslibctx.h
$(GLSRC)gsstype.h:$(GLSRC)stdio_.h
$(GLSRC)gsstype.h:$(GLSRC)gssprintf.h
$(GLSRC)gsstype.h:$(GLSRC)gstypes.h
$(GLSRC)gsstype.h:$(GLSRC)std.h
$(GLSRC)gsstype.h:$(GLSRC)stdpre.h
$(GLSRC)gsstype.h:$(GLGEN)arch.h
$(GLSRC)gsstype.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gx.h:$(GLSRC)gdebug.h
$(GLSRC)gx.h:$(GLSRC)gsgstate.h
$(GLSRC)gx.h:$(GLSRC)gsio.h
$(GLSRC)gx.h:$(GLSRC)gdbflags.h
$(GLSRC)gx.h:$(GLSRC)gserrors.h
$(GLSRC)gx.h:$(GLSRC)gsmemory.h
$(GLSRC)gx.h:$(GLSRC)gslibctx.h
$(GLSRC)gx.h:$(GLSRC)stdio_.h
$(GLSRC)gx.h:$(GLSRC)gssprintf.h
$(GLSRC)gx.h:$(GLSRC)gstypes.h
$(GLSRC)gx.h:$(GLSRC)std.h
$(GLSRC)gx.h:$(GLSRC)stdpre.h
$(GLSRC)gx.h:$(GLGEN)arch.h
$(GLSRC)gx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxsync.h:$(GLSRC)gpsync.h
$(GLSRC)gxsync.h:$(GLSRC)gsmemory.h
$(GLSRC)gxsync.h:$(GLSRC)gslibctx.h
$(GLSRC)gxsync.h:$(GLSRC)stdio_.h
$(GLSRC)gxsync.h:$(GLSRC)stdint_.h
$(GLSRC)gxsync.h:$(GLSRC)gssprintf.h
$(GLSRC)gxsync.h:$(GLSRC)gstypes.h
$(GLSRC)gxsync.h:$(GLSRC)std.h
$(GLSRC)gxsync.h:$(GLSRC)stdpre.h
$(GLSRC)gxsync.h:$(GLGEN)arch.h
$(GLSRC)gxsync.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxclist.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxgstate.h
$(GLSRC)gxclthrd.h:$(GLSRC)gstrans.h
$(GLSRC)gxclthrd.h:$(GLSRC)gdevp14.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxline.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsht1.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcomp.h
$(GLSRC)gxclthrd.h:$(GLSRC)math_.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclthrd.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxband.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxblend.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscolor2.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdevice.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcpath.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsht.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsequivc.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxpcache.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscindex.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsptype1.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscie.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxtext.h
$(GLSRC)gxclthrd.h:$(GLSRC)gstext.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxstate.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclthrd.h:$(GLSRC)gstparam.h
$(GLSRC)gxclthrd.h:$(GLSRC)gspcolor.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxctable.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscsel.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsfont.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsimage.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsropt.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxdda.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxpath.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxiclass.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxftype.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscms.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsrect.h
$(GLSRC)gxclthrd.h:$(GLSRC)gslparam.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscpm.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscspace.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxstdio.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsio.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxclio.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscompt.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclthrd.h:$(GLSRC)gspenum.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsparam.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclthrd.h:$(GLSRC)gp.h
$(GLSRC)gxclthrd.h:$(GLSRC)memento.h
$(GLSRC)gxclthrd.h:$(GLSRC)memory_.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsuid.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxsync.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclthrd.h:$(GLSRC)vmsmath.h
$(GLSRC)gxclthrd.h:$(GLSRC)srdline.h
$(GLSRC)gxclthrd.h:$(GLSRC)scommon.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsfname.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxarith.h
$(GLSRC)gxclthrd.h:$(GLSRC)stat_.h
$(GLSRC)gxclthrd.h:$(GLSRC)gpsync.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsstype.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclthrd.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclthrd.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclthrd.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclthrd.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclthrd.h:$(GLSRC)stdio_.h
$(GLSRC)gxclthrd.h:$(GLSRC)gsccode.h
$(GLSRC)gxclthrd.h:$(GLSRC)stdint_.h
$(GLSRC)gxclthrd.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclthrd.h:$(GLSRC)gstypes.h
$(GLSRC)gxclthrd.h:$(GLSRC)std.h
$(GLSRC)gxclthrd.h:$(GLSRC)stdpre.h
$(GLSRC)gxclthrd.h:$(GLGEN)arch.h
$(GLSRC)gxclthrd.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxtext.h
$(GLSRC)gxdevsop.h:$(GLSRC)gstext.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdevsop.h:$(GLSRC)gstparam.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscsel.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsfont.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsimage.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsropt.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxdda.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxpath.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxftype.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscms.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsrect.h
$(GLSRC)gxdevsop.h:$(GLSRC)gslparam.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscpm.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscspace.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscompt.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdevsop.h:$(GLSRC)gspenum.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsparam.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdevsop.h:$(GLSRC)gp.h
$(GLSRC)gxdevsop.h:$(GLSRC)memento.h
$(GLSRC)gxdevsop.h:$(GLSRC)memory_.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsuid.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxsync.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdevsop.h:$(GLSRC)srdline.h
$(GLSRC)gxdevsop.h:$(GLSRC)scommon.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxarith.h
$(GLSRC)gxdevsop.h:$(GLSRC)stat_.h
$(GLSRC)gxdevsop.h:$(GLSRC)gpsync.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsstype.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdevsop.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdevsop.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdevsop.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdevsop.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdevsop.h:$(GLSRC)stdio_.h
$(GLSRC)gxdevsop.h:$(GLSRC)gsccode.h
$(GLSRC)gxdevsop.h:$(GLSRC)stdint_.h
$(GLSRC)gxdevsop.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdevsop.h:$(GLSRC)gstypes.h
$(GLSRC)gxdevsop.h:$(GLSRC)std.h
$(GLSRC)gxdevsop.h:$(GLSRC)stdpre.h
$(GLSRC)gxdevsop.h:$(GLGEN)arch.h
$(GLSRC)gxdevsop.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevflp.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevflp.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevflp.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevflp.h:$(GLSRC)gxtext.h
$(GLSRC)gdevflp.h:$(GLSRC)gstext.h
$(GLSRC)gdevflp.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevflp.h:$(GLSRC)gstparam.h
$(GLSRC)gdevflp.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevflp.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevflp.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevflp.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevflp.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevflp.h:$(GLSRC)gscsel.h
$(GLSRC)gdevflp.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevflp.h:$(GLSRC)gsfont.h
$(GLSRC)gdevflp.h:$(GLSRC)gsimage.h
$(GLSRC)gdevflp.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevflp.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevflp.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevflp.h:$(GLSRC)gsropt.h
$(GLSRC)gdevflp.h:$(GLSRC)gxdda.h
$(GLSRC)gdevflp.h:$(GLSRC)gxpath.h
$(GLSRC)gdevflp.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevflp.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevflp.h:$(GLSRC)gxftype.h
$(GLSRC)gdevflp.h:$(GLSRC)gscms.h
$(GLSRC)gdevflp.h:$(GLSRC)gsrect.h
$(GLSRC)gdevflp.h:$(GLSRC)gslparam.h
$(GLSRC)gdevflp.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevflp.h:$(GLSRC)gscpm.h
$(GLSRC)gdevflp.h:$(GLSRC)gscspace.h
$(GLSRC)gdevflp.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevflp.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevflp.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevflp.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevflp.h:$(GLSRC)gsio.h
$(GLSRC)gdevflp.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevflp.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevflp.h:$(GLSRC)gscompt.h
$(GLSRC)gdevflp.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevflp.h:$(GLSRC)gspenum.h
$(GLSRC)gdevflp.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevflp.h:$(GLSRC)gsparam.h
$(GLSRC)gdevflp.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevflp.h:$(GLSRC)gp.h
$(GLSRC)gdevflp.h:$(GLSRC)memento.h
$(GLSRC)gdevflp.h:$(GLSRC)memory_.h
$(GLSRC)gdevflp.h:$(GLSRC)gsuid.h
$(GLSRC)gdevflp.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevflp.h:$(GLSRC)gxsync.h
$(GLSRC)gdevflp.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevflp.h:$(GLSRC)srdline.h
$(GLSRC)gdevflp.h:$(GLSRC)scommon.h
$(GLSRC)gdevflp.h:$(GLSRC)gsfname.h
$(GLSRC)gdevflp.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevflp.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevflp.h:$(GLSRC)gxarith.h
$(GLSRC)gdevflp.h:$(GLSRC)stat_.h
$(GLSRC)gdevflp.h:$(GLSRC)gpsync.h
$(GLSRC)gdevflp.h:$(GLSRC)gsstype.h
$(GLSRC)gdevflp.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevflp.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevflp.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevflp.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevflp.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevflp.h:$(GLSRC)stdio_.h
$(GLSRC)gdevflp.h:$(GLSRC)gsccode.h
$(GLSRC)gdevflp.h:$(GLSRC)stdint_.h
$(GLSRC)gdevflp.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevflp.h:$(GLSRC)gstypes.h
$(GLSRC)gdevflp.h:$(GLSRC)std.h
$(GLSRC)gdevflp.h:$(GLSRC)stdpre.h
$(GLSRC)gdevflp.h:$(GLGEN)arch.h
$(GLSRC)gdevflp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gdevflp.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gdevoflt.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gdevnup.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxtext.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gstext.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gstparam.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscsel.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsfont.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsimage.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsropt.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxdda.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxpath.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxftype.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscms.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsrect.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gslparam.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscpm.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscspace.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsio.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscompt.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gspenum.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsparam.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gp.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)memento.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)memory_.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsuid.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxsync.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)srdline.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)scommon.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsfname.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxarith.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)stat_.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gpsync.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsstype.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)stdio_.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gsccode.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)stdint_.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gstypes.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)std.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)stdpre.h
$(GLSRC)gdevkrnlsclass.h:$(GLGEN)arch.h
$(GLSRC)gdevkrnlsclass.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxtext.h
$(GLSRC)gdevsclass.h:$(GLSRC)gstext.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevsclass.h:$(GLSRC)gstparam.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscsel.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsfont.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsimage.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsropt.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxdda.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxpath.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxftype.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscms.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsrect.h
$(GLSRC)gdevsclass.h:$(GLSRC)gslparam.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscpm.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscspace.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsio.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscompt.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevsclass.h:$(GLSRC)gspenum.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsparam.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevsclass.h:$(GLSRC)gp.h
$(GLSRC)gdevsclass.h:$(GLSRC)memento.h
$(GLSRC)gdevsclass.h:$(GLSRC)memory_.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsuid.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxsync.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevsclass.h:$(GLSRC)srdline.h
$(GLSRC)gdevsclass.h:$(GLSRC)scommon.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsfname.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxarith.h
$(GLSRC)gdevsclass.h:$(GLSRC)stat_.h
$(GLSRC)gdevsclass.h:$(GLSRC)gpsync.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsstype.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevsclass.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevsclass.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevsclass.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevsclass.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevsclass.h:$(GLSRC)stdio_.h
$(GLSRC)gdevsclass.h:$(GLSRC)gsccode.h
$(GLSRC)gdevsclass.h:$(GLSRC)stdint_.h
$(GLSRC)gdevsclass.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevsclass.h:$(GLSRC)gstypes.h
$(GLSRC)gdevsclass.h:$(GLSRC)std.h
$(GLSRC)gdevsclass.h:$(GLSRC)stdpre.h
$(GLSRC)gdevsclass.h:$(GLGEN)arch.h
$(GLSRC)gdevsclass.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsnotify.h:$(GLSRC)gsstype.h
$(GLSRC)gsnotify.h:$(GLSRC)gsmemory.h
$(GLSRC)gsnotify.h:$(GLSRC)gslibctx.h
$(GLSRC)gsnotify.h:$(GLSRC)stdio_.h
$(GLSRC)gsnotify.h:$(GLSRC)gssprintf.h
$(GLSRC)gsnotify.h:$(GLSRC)gstypes.h
$(GLSRC)gsnotify.h:$(GLSRC)std.h
$(GLSRC)gsnotify.h:$(GLSRC)stdpre.h
$(GLSRC)gsnotify.h:$(GLGEN)arch.h
$(GLSRC)gsnotify.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsstruct.h:$(GLSRC)gsstype.h
$(GLSRC)gsstruct.h:$(GLSRC)gsmemory.h
$(GLSRC)gsstruct.h:$(GLSRC)gslibctx.h
$(GLSRC)gsstruct.h:$(GLSRC)stdio_.h
$(GLSRC)gsstruct.h:$(GLSRC)gssprintf.h
$(GLSRC)gsstruct.h:$(GLSRC)gstypes.h
$(GLSRC)gsstruct.h:$(GLSRC)std.h
$(GLSRC)gsstruct.h:$(GLSRC)stdpre.h
$(GLSRC)gsstruct.h:$(GLGEN)arch.h
$(GLSRC)gsstruct.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsbitmap.h:$(GLSRC)gsstype.h
$(GLSRC)gsbitmap.h:$(GLSRC)gsmemory.h
$(GLSRC)gsbitmap.h:$(GLSRC)gslibctx.h
$(GLSRC)gsbitmap.h:$(GLSRC)stdio_.h
$(GLSRC)gsbitmap.h:$(GLSRC)gssprintf.h
$(GLSRC)gsbitmap.h:$(GLSRC)gstypes.h
$(GLSRC)gsbitmap.h:$(GLSRC)std.h
$(GLSRC)gsbitmap.h:$(GLSRC)stdpre.h
$(GLSRC)gsbitmap.h:$(GLGEN)arch.h
$(GLSRC)gsbitmap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsbitops.h:$(GLSRC)gxcindex.h
$(GLSRC)gsbitops.h:$(GLSRC)stdint_.h
$(GLSRC)gsbitops.h:$(GLSRC)gstypes.h
$(GLSRC)gsbitops.h:$(GLSRC)std.h
$(GLSRC)gsbitops.h:$(GLSRC)stdpre.h
$(GLSRC)gsbitops.h:$(GLGEN)arch.h
$(GLSRC)gsbittab.h:$(GLSRC)gstypes.h
$(GLSRC)gsbittab.h:$(GLSRC)stdpre.h
$(GLSRC)gsflip.h:$(GLSRC)stdpre.h
$(GLSRC)gsuid.h:$(GLSRC)std.h
$(GLSRC)gsuid.h:$(GLSRC)stdpre.h
$(GLSRC)gsuid.h:$(GLGEN)arch.h
$(GLSRC)gsutil.h:$(GLSRC)gstypes.h
$(GLSRC)gsutil.h:$(GLSRC)std.h
$(GLSRC)gsutil.h:$(GLSRC)stdpre.h
$(GLSRC)gsutil.h:$(GLGEN)arch.h
$(GLSRC)gxbitmap.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxbitmap.h:$(GLSRC)gsstype.h
$(GLSRC)gxbitmap.h:$(GLSRC)gsmemory.h
$(GLSRC)gxbitmap.h:$(GLSRC)gslibctx.h
$(GLSRC)gxbitmap.h:$(GLSRC)stdio_.h
$(GLSRC)gxbitmap.h:$(GLSRC)gssprintf.h
$(GLSRC)gxbitmap.h:$(GLSRC)gstypes.h
$(GLSRC)gxbitmap.h:$(GLSRC)std.h
$(GLSRC)gxbitmap.h:$(GLSRC)stdpre.h
$(GLSRC)gxbitmap.h:$(GLGEN)arch.h
$(GLSRC)gxbitmap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfarith.h:$(GLSRC)gxarith.h
$(GLSRC)gxfarith.h:$(GLSRC)stdpre.h
$(GLSRC)gxfixed.h:$(GLSRC)std.h
$(GLSRC)gxfixed.h:$(GLSRC)stdpre.h
$(GLSRC)gxfixed.h:$(GLGEN)arch.h
$(GLSRC)gxobj.h:$(GLSRC)gsstruct.h
$(GLSRC)gxobj.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxobj.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxobj.h:$(GLSRC)gsstype.h
$(GLSRC)gxobj.h:$(GLSRC)gsmemory.h
$(GLSRC)gxobj.h:$(GLSRC)gslibctx.h
$(GLSRC)gxobj.h:$(GLSRC)stdio_.h
$(GLSRC)gxobj.h:$(GLSRC)gssprintf.h
$(GLSRC)gxobj.h:$(GLSRC)gstypes.h
$(GLSRC)gxobj.h:$(GLSRC)std.h
$(GLSRC)gxobj.h:$(GLSRC)stdpre.h
$(GLSRC)gxobj.h:$(GLGEN)arch.h
$(GLSRC)gxobj.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxrplane.h:$(GLSRC)gsdevice.h
$(GLSRC)gxrplane.h:$(GLSRC)gsgstate.h
$(GLSRC)gxrplane.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxrplane.h:$(GLSRC)gsparam.h
$(GLSRC)gxrplane.h:$(GLSRC)scommon.h
$(GLSRC)gxrplane.h:$(GLSRC)gsstype.h
$(GLSRC)gxrplane.h:$(GLSRC)gsmemory.h
$(GLSRC)gxrplane.h:$(GLSRC)gslibctx.h
$(GLSRC)gxrplane.h:$(GLSRC)stdio_.h
$(GLSRC)gxrplane.h:$(GLSRC)stdint_.h
$(GLSRC)gxrplane.h:$(GLSRC)gssprintf.h
$(GLSRC)gxrplane.h:$(GLSRC)gstypes.h
$(GLSRC)gxrplane.h:$(GLSRC)std.h
$(GLSRC)gxrplane.h:$(GLSRC)stdpre.h
$(GLSRC)gxrplane.h:$(GLGEN)arch.h
$(GLSRC)gxrplane.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsrect.h:$(GLSRC)gxfixed.h
$(GLSRC)gsrect.h:$(GLSRC)gstypes.h
$(GLSRC)gsrect.h:$(GLSRC)std.h
$(GLSRC)gsrect.h:$(GLSRC)stdpre.h
$(GLSRC)gsrect.h:$(GLGEN)arch.h
$(GLSRC)gxalloc.h:$(GLSRC)gxobj.h
$(GLSRC)gxalloc.h:$(GLSRC)gsstruct.h
$(GLSRC)gxalloc.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxalloc.h:$(GLSRC)scommon.h
$(GLSRC)gxalloc.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxalloc.h:$(GLSRC)gsstype.h
$(GLSRC)gxalloc.h:$(GLSRC)gsmemory.h
$(GLSRC)gxalloc.h:$(GLSRC)gslibctx.h
$(GLSRC)gxalloc.h:$(GLSRC)gsalloc.h
$(GLSRC)gxalloc.h:$(GLSRC)stdio_.h
$(GLSRC)gxalloc.h:$(GLSRC)stdint_.h
$(GLSRC)gxalloc.h:$(GLSRC)gssprintf.h
$(GLSRC)gxalloc.h:$(GLSRC)gstypes.h
$(GLSRC)gxalloc.h:$(GLSRC)std.h
$(GLSRC)gxalloc.h:$(GLSRC)stdpre.h
$(GLSRC)gxalloc.h:$(GLGEN)arch.h
$(GLSRC)gxalloc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxbitops.h:$(GLSRC)gsbitops.h
$(GLSRC)gxbitops.h:$(GLSRC)gxcindex.h
$(GLSRC)gxbitops.h:$(GLSRC)stdint_.h
$(GLSRC)gxbitops.h:$(GLSRC)gstypes.h
$(GLSRC)gxbitops.h:$(GLSRC)std.h
$(GLSRC)gxbitops.h:$(GLSRC)stdpre.h
$(GLSRC)gxbitops.h:$(GLGEN)arch.h
$(GLSRC)gxcindex.h:$(GLSRC)stdint_.h
$(GLSRC)gxcindex.h:$(GLSRC)std.h
$(GLSRC)gxcindex.h:$(GLSRC)stdpre.h
$(GLSRC)gxcindex.h:$(GLGEN)arch.h
$(GLSRC)gxfont42.h:$(GLSRC)gxfont.h
$(GLSRC)gxfont42.h:$(GLSRC)gspath.h
$(GLSRC)gxfont42.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfont42.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfont42.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfont42.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfont42.h:$(GLSRC)gstext.h
$(GLSRC)gxfont42.h:$(GLSRC)gxfcache.h
$(GLSRC)gxfont42.h:$(GLSRC)gsfont.h
$(GLSRC)gxfont42.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfont42.h:$(GLSRC)gxbcache.h
$(GLSRC)gxfont42.h:$(GLSRC)gxpath.h
$(GLSRC)gxfont42.h:$(GLSRC)gxftype.h
$(GLSRC)gxfont42.h:$(GLSRC)gscms.h
$(GLSRC)gxfont42.h:$(GLSRC)gsrect.h
$(GLSRC)gxfont42.h:$(GLSRC)gslparam.h
$(GLSRC)gxfont42.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfont42.h:$(GLSRC)gscpm.h
$(GLSRC)gxfont42.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfont42.h:$(GLSRC)gscspace.h
$(GLSRC)gxfont42.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfont42.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfont42.h:$(GLSRC)gsxfont.h
$(GLSRC)gxfont42.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfont42.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfont42.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfont42.h:$(GLSRC)gspenum.h
$(GLSRC)gxfont42.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfont42.h:$(GLSRC)gsparam.h
$(GLSRC)gxfont42.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfont42.h:$(GLSRC)memento.h
$(GLSRC)gxfont42.h:$(GLSRC)gsuid.h
$(GLSRC)gxfont42.h:$(GLSRC)gxsync.h
$(GLSRC)gxfont42.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfont42.h:$(GLSRC)scommon.h
$(GLSRC)gxfont42.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfont42.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfont42.h:$(GLSRC)gxarith.h
$(GLSRC)gxfont42.h:$(GLSRC)gpsync.h
$(GLSRC)gxfont42.h:$(GLSRC)gsstype.h
$(GLSRC)gxfont42.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfont42.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfont42.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfont42.h:$(GLSRC)stdio_.h
$(GLSRC)gxfont42.h:$(GLSRC)gsccode.h
$(GLSRC)gxfont42.h:$(GLSRC)stdint_.h
$(GLSRC)gxfont42.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfont42.h:$(GLSRC)gstypes.h
$(GLSRC)gxfont42.h:$(GLSRC)std.h
$(GLSRC)gxfont42.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont42.h:$(GLGEN)arch.h
$(GLSRC)gxfont42.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gstrans.h:$(GLSRC)gdevp14.h
$(GLSRC)gstrans.h:$(GLSRC)gxcomp.h
$(GLSRC)gstrans.h:$(GLSRC)gxcolor2.h
$(GLSRC)gstrans.h:$(GLSRC)gxpcolor.h
$(GLSRC)gstrans.h:$(GLSRC)gxdevmem.h
$(GLSRC)gstrans.h:$(GLSRC)gdevdevn.h
$(GLSRC)gstrans.h:$(GLSRC)gxdcolor.h
$(GLSRC)gstrans.h:$(GLSRC)gxblend.h
$(GLSRC)gstrans.h:$(GLSRC)gscolor2.h
$(GLSRC)gstrans.h:$(GLSRC)gxdevice.h
$(GLSRC)gstrans.h:$(GLSRC)gxcpath.h
$(GLSRC)gstrans.h:$(GLSRC)gsequivc.h
$(GLSRC)gstrans.h:$(GLSRC)gxdevcli.h
$(GLSRC)gstrans.h:$(GLSRC)gxpcache.h
$(GLSRC)gstrans.h:$(GLSRC)gscindex.h
$(GLSRC)gstrans.h:$(GLSRC)gxcmap.h
$(GLSRC)gstrans.h:$(GLSRC)gsptype1.h
$(GLSRC)gstrans.h:$(GLSRC)gscie.h
$(GLSRC)gstrans.h:$(GLSRC)gxtext.h
$(GLSRC)gstrans.h:$(GLSRC)gstext.h
$(GLSRC)gstrans.h:$(GLSRC)gsnamecl.h
$(GLSRC)gstrans.h:$(GLSRC)gstparam.h
$(GLSRC)gstrans.h:$(GLSRC)gspcolor.h
$(GLSRC)gstrans.h:$(GLSRC)gxfmap.h
$(GLSRC)gstrans.h:$(GLSRC)gsmalloc.h
$(GLSRC)gstrans.h:$(GLSRC)gsfunc.h
$(GLSRC)gstrans.h:$(GLSRC)gxcspace.h
$(GLSRC)gstrans.h:$(GLSRC)gxctable.h
$(GLSRC)gstrans.h:$(GLSRC)gxrplane.h
$(GLSRC)gstrans.h:$(GLSRC)gscsel.h
$(GLSRC)gstrans.h:$(GLSRC)gxfcache.h
$(GLSRC)gstrans.h:$(GLSRC)gsfont.h
$(GLSRC)gstrans.h:$(GLSRC)gsimage.h
$(GLSRC)gstrans.h:$(GLSRC)gsdcolor.h
$(GLSRC)gstrans.h:$(GLSRC)gxcvalue.h
$(GLSRC)gstrans.h:$(GLSRC)gxbcache.h
$(GLSRC)gstrans.h:$(GLSRC)gsropt.h
$(GLSRC)gstrans.h:$(GLSRC)gxdda.h
$(GLSRC)gstrans.h:$(GLSRC)gxpath.h
$(GLSRC)gstrans.h:$(GLSRC)gxiclass.h
$(GLSRC)gstrans.h:$(GLSRC)gxfrac.h
$(GLSRC)gstrans.h:$(GLSRC)gxtmap.h
$(GLSRC)gstrans.h:$(GLSRC)gxftype.h
$(GLSRC)gstrans.h:$(GLSRC)gscms.h
$(GLSRC)gstrans.h:$(GLSRC)gsrect.h
$(GLSRC)gstrans.h:$(GLSRC)gslparam.h
$(GLSRC)gstrans.h:$(GLSRC)gsdevice.h
$(GLSRC)gstrans.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gstrans.h:$(GLSRC)gscpm.h
$(GLSRC)gstrans.h:$(GLSRC)gscspace.h
$(GLSRC)gstrans.h:$(GLSRC)gsgstate.h
$(GLSRC)gstrans.h:$(GLSRC)gxstdio.h
$(GLSRC)gstrans.h:$(GLSRC)gsxfont.h
$(GLSRC)gstrans.h:$(GLSRC)gsdsrc.h
$(GLSRC)gstrans.h:$(GLSRC)gsio.h
$(GLSRC)gstrans.h:$(GLSRC)gsiparam.h
$(GLSRC)gstrans.h:$(GLSRC)gxfixed.h
$(GLSRC)gstrans.h:$(GLSRC)gscompt.h
$(GLSRC)gstrans.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstrans.h:$(GLSRC)gspenum.h
$(GLSRC)gstrans.h:$(GLSRC)gxhttile.h
$(GLSRC)gstrans.h:$(GLSRC)gsparam.h
$(GLSRC)gstrans.h:$(GLSRC)gsrefct.h
$(GLSRC)gstrans.h:$(GLSRC)gp.h
$(GLSRC)gstrans.h:$(GLSRC)memento.h
$(GLSRC)gstrans.h:$(GLSRC)memory_.h
$(GLSRC)gstrans.h:$(GLSRC)gsuid.h
$(GLSRC)gstrans.h:$(GLSRC)gsstruct.h
$(GLSRC)gstrans.h:$(GLSRC)gxsync.h
$(GLSRC)gstrans.h:$(GLSRC)gxbitmap.h
$(GLSRC)gstrans.h:$(GLSRC)srdline.h
$(GLSRC)gstrans.h:$(GLSRC)scommon.h
$(GLSRC)gstrans.h:$(GLSRC)gsfname.h
$(GLSRC)gstrans.h:$(GLSRC)gsbitmap.h
$(GLSRC)gstrans.h:$(GLSRC)gsccolor.h
$(GLSRC)gstrans.h:$(GLSRC)gxarith.h
$(GLSRC)gstrans.h:$(GLSRC)stat_.h
$(GLSRC)gstrans.h:$(GLSRC)gpsync.h
$(GLSRC)gstrans.h:$(GLSRC)gsstype.h
$(GLSRC)gstrans.h:$(GLSRC)gsmemory.h
$(GLSRC)gstrans.h:$(GLSRC)gpgetenv.h
$(GLSRC)gstrans.h:$(GLSRC)gscdefs.h
$(GLSRC)gstrans.h:$(GLSRC)gslibctx.h
$(GLSRC)gstrans.h:$(GLSRC)gxcindex.h
$(GLSRC)gstrans.h:$(GLSRC)stdio_.h
$(GLSRC)gstrans.h:$(GLSRC)gsccode.h
$(GLSRC)gstrans.h:$(GLSRC)stdint_.h
$(GLSRC)gstrans.h:$(GLSRC)gssprintf.h
$(GLSRC)gstrans.h:$(GLSRC)gstypes.h
$(GLSRC)gstrans.h:$(GLSRC)std.h
$(GLSRC)gstrans.h:$(GLSRC)stdpre.h
$(GLSRC)gstrans.h:$(GLGEN)arch.h
$(GLSRC)gstrans.h:$(GLSRC)gs_dll_call.h
$(GLSRC)scommon.h:$(GLSRC)gsstype.h
$(GLSRC)scommon.h:$(GLSRC)gsmemory.h
$(GLSRC)scommon.h:$(GLSRC)gslibctx.h
$(GLSRC)scommon.h:$(GLSRC)stdio_.h
$(GLSRC)scommon.h:$(GLSRC)stdint_.h
$(GLSRC)scommon.h:$(GLSRC)gssprintf.h
$(GLSRC)scommon.h:$(GLSRC)gstypes.h
$(GLSRC)scommon.h:$(GLSRC)std.h
$(GLSRC)scommon.h:$(GLSRC)stdpre.h
$(GLSRC)scommon.h:$(GLGEN)arch.h
$(GLSRC)scommon.h:$(GLSRC)gs_dll_call.h
$(GLSRC)stream.h:$(GLSRC)gxiodev.h
$(GLSRC)stream.h:$(GLSRC)gsparam.h
$(GLSRC)stream.h:$(GLSRC)gp.h
$(GLSRC)stream.h:$(GLSRC)memory_.h
$(GLSRC)stream.h:$(GLSRC)srdline.h
$(GLSRC)stream.h:$(GLSRC)scommon.h
$(GLSRC)stream.h:$(GLSRC)gsfname.h
$(GLSRC)stream.h:$(GLSRC)stat_.h
$(GLSRC)stream.h:$(GLSRC)gsstype.h
$(GLSRC)stream.h:$(GLSRC)gsmemory.h
$(GLSRC)stream.h:$(GLSRC)gpgetenv.h
$(GLSRC)stream.h:$(GLSRC)gscdefs.h
$(GLSRC)stream.h:$(GLSRC)gslibctx.h
$(GLSRC)stream.h:$(GLSRC)stdio_.h
$(GLSRC)stream.h:$(GLSRC)stdint_.h
$(GLSRC)stream.h:$(GLSRC)gssprintf.h
$(GLSRC)stream.h:$(GLSRC)gstypes.h
$(GLSRC)stream.h:$(GLSRC)std.h
$(GLSRC)stream.h:$(GLSRC)stdpre.h
$(GLSRC)stream.h:$(GLGEN)arch.h
$(GLSRC)stream.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ramfs.h:$(GLSRC)stream.h
$(GLSRC)ramfs.h:$(GLSRC)gxiodev.h
$(GLSRC)ramfs.h:$(GLSRC)gsparam.h
$(GLSRC)ramfs.h:$(GLSRC)gp.h
$(GLSRC)ramfs.h:$(GLSRC)memory_.h
$(GLSRC)ramfs.h:$(GLSRC)srdline.h
$(GLSRC)ramfs.h:$(GLSRC)scommon.h
$(GLSRC)ramfs.h:$(GLSRC)gsfname.h
$(GLSRC)ramfs.h:$(GLSRC)stat_.h
$(GLSRC)ramfs.h:$(GLSRC)gsstype.h
$(GLSRC)ramfs.h:$(GLSRC)gsmemory.h
$(GLSRC)ramfs.h:$(GLSRC)gpgetenv.h
$(GLSRC)ramfs.h:$(GLSRC)gscdefs.h
$(GLSRC)ramfs.h:$(GLSRC)gslibctx.h
$(GLSRC)ramfs.h:$(GLSRC)stdio_.h
$(GLSRC)ramfs.h:$(GLSRC)stdint_.h
$(GLSRC)ramfs.h:$(GLSRC)gssprintf.h
$(GLSRC)ramfs.h:$(GLSRC)gstypes.h
$(GLSRC)ramfs.h:$(GLSRC)std.h
$(GLSRC)ramfs.h:$(GLSRC)stdpre.h
$(GLSRC)ramfs.h:$(GLGEN)arch.h
$(GLSRC)ramfs.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsmd5.h:$(GLSRC)memory_.h
$(GLSRC)gsmd5.h:$(GLSRC)std.h
$(GLSRC)gsmd5.h:$(GLSRC)stdpre.h
$(GLSRC)gsmd5.h:$(GLGEN)arch.h
$(GLSRC)sha2.h:$(GLSRC)stdint_.h
$(GLSRC)sha2.h:$(GLSRC)std.h
$(GLSRC)sha2.h:$(GLSRC)stdpre.h
$(GLSRC)sha2.h:$(GLGEN)arch.h
$(GLSRC)gsccode.h:$(GLSRC)stdint_.h
$(GLSRC)gsccode.h:$(GLSRC)gstypes.h
$(GLSRC)gsccode.h:$(GLSRC)std.h
$(GLSRC)gsccode.h:$(GLSRC)stdpre.h
$(GLSRC)gsccode.h:$(GLGEN)arch.h
$(GLSRC)gsccolor.h:$(GLSRC)gsstype.h
$(GLSRC)gsccolor.h:$(GLSRC)gsmemory.h
$(GLSRC)gsccolor.h:$(GLSRC)gslibctx.h
$(GLSRC)gsccolor.h:$(GLSRC)stdio_.h
$(GLSRC)gsccolor.h:$(GLSRC)gssprintf.h
$(GLSRC)gsccolor.h:$(GLSRC)gstypes.h
$(GLSRC)gsccolor.h:$(GLSRC)std.h
$(GLSRC)gsccolor.h:$(GLSRC)stdpre.h
$(GLSRC)gsccolor.h:$(GLGEN)arch.h
$(GLSRC)gsccolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscedata.h:$(GLSRC)stdpre.h
$(GLSRC)gscencs.h:$(GLSRC)gsccode.h
$(GLSRC)gscencs.h:$(GLSRC)stdint_.h
$(GLSRC)gscencs.h:$(GLSRC)gstypes.h
$(GLSRC)gscencs.h:$(GLSRC)std.h
$(GLSRC)gscencs.h:$(GLSRC)stdpre.h
$(GLSRC)gscencs.h:$(GLGEN)arch.h
$(GLSRC)gsclipsr.h:$(GLSRC)gsgstate.h
$(GLSRC)gscolor1.h:$(GLSRC)gxtmap.h
$(GLSRC)gscolor1.h:$(GLSRC)gsgstate.h
$(GLSRC)gscolor1.h:$(GLSRC)stdpre.h
$(GLSRC)gscompt.h:$(GLSRC)gstypes.h
$(GLSRC)gscompt.h:$(GLSRC)stdpre.h
$(GLSRC)gscoord.h:$(GLSRC)gsgstate.h
$(GLSRC)gscoord.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscoord.h:$(GLSRC)scommon.h
$(GLSRC)gscoord.h:$(GLSRC)gsstype.h
$(GLSRC)gscoord.h:$(GLSRC)gsmemory.h
$(GLSRC)gscoord.h:$(GLSRC)gslibctx.h
$(GLSRC)gscoord.h:$(GLSRC)stdio_.h
$(GLSRC)gscoord.h:$(GLSRC)stdint_.h
$(GLSRC)gscoord.h:$(GLSRC)gssprintf.h
$(GLSRC)gscoord.h:$(GLSRC)gstypes.h
$(GLSRC)gscoord.h:$(GLSRC)std.h
$(GLSRC)gscoord.h:$(GLSRC)stdpre.h
$(GLSRC)gscoord.h:$(GLGEN)arch.h
$(GLSRC)gscoord.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsdevice.h:$(GLSRC)gsgstate.h
$(GLSRC)gsdevice.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsdevice.h:$(GLSRC)gsparam.h
$(GLSRC)gsdevice.h:$(GLSRC)scommon.h
$(GLSRC)gsdevice.h:$(GLSRC)gsstype.h
$(GLSRC)gsdevice.h:$(GLSRC)gsmemory.h
$(GLSRC)gsdevice.h:$(GLSRC)gslibctx.h
$(GLSRC)gsdevice.h:$(GLSRC)stdio_.h
$(GLSRC)gsdevice.h:$(GLSRC)stdint_.h
$(GLSRC)gsdevice.h:$(GLSRC)gssprintf.h
$(GLSRC)gsdevice.h:$(GLSRC)gstypes.h
$(GLSRC)gsdevice.h:$(GLSRC)std.h
$(GLSRC)gsdevice.h:$(GLSRC)stdpre.h
$(GLSRC)gsdevice.h:$(GLGEN)arch.h
$(GLSRC)gsdevice.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsfcmap.h:$(GLSRC)gsccode.h
$(GLSRC)gsfcmap.h:$(GLSRC)stdint_.h
$(GLSRC)gsfcmap.h:$(GLSRC)gstypes.h
$(GLSRC)gsfcmap.h:$(GLSRC)std.h
$(GLSRC)gsfcmap.h:$(GLSRC)stdpre.h
$(GLSRC)gsfcmap.h:$(GLGEN)arch.h
$(GLSRC)gsfname.h:$(GLSRC)std.h
$(GLSRC)gsfname.h:$(GLSRC)stdpre.h
$(GLSRC)gsfname.h:$(GLGEN)arch.h
$(GLSRC)gsfont.h:$(GLSRC)gsgstate.h
$(GLSRC)gsfont.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsfont.h:$(GLSRC)scommon.h
$(GLSRC)gsfont.h:$(GLSRC)gsstype.h
$(GLSRC)gsfont.h:$(GLSRC)gsmemory.h
$(GLSRC)gsfont.h:$(GLSRC)gslibctx.h
$(GLSRC)gsfont.h:$(GLSRC)stdio_.h
$(GLSRC)gsfont.h:$(GLSRC)stdint_.h
$(GLSRC)gsfont.h:$(GLSRC)gssprintf.h
$(GLSRC)gsfont.h:$(GLSRC)gstypes.h
$(GLSRC)gsfont.h:$(GLSRC)std.h
$(GLSRC)gsfont.h:$(GLSRC)stdpre.h
$(GLSRC)gsfont.h:$(GLGEN)arch.h
$(GLSRC)gsfont.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsgdata.h:$(GLSRC)gsfont.h
$(GLSRC)gsgdata.h:$(GLSRC)gsgcache.h
$(GLSRC)gsgdata.h:$(GLSRC)gsgstate.h
$(GLSRC)gsgdata.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsgdata.h:$(GLSRC)scommon.h
$(GLSRC)gsgdata.h:$(GLSRC)gsstype.h
$(GLSRC)gsgdata.h:$(GLSRC)gsmemory.h
$(GLSRC)gsgdata.h:$(GLSRC)gslibctx.h
$(GLSRC)gsgdata.h:$(GLSRC)stdio_.h
$(GLSRC)gsgdata.h:$(GLSRC)stdint_.h
$(GLSRC)gsgdata.h:$(GLSRC)gssprintf.h
$(GLSRC)gsgdata.h:$(GLSRC)gstypes.h
$(GLSRC)gsgdata.h:$(GLSRC)std.h
$(GLSRC)gsgdata.h:$(GLSRC)stdpre.h
$(GLSRC)gsgdata.h:$(GLGEN)arch.h
$(GLSRC)gsgdata.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsgcache.h:$(GLSRC)scommon.h
$(GLSRC)gsgcache.h:$(GLSRC)gsstype.h
$(GLSRC)gsgcache.h:$(GLSRC)gsmemory.h
$(GLSRC)gsgcache.h:$(GLSRC)gslibctx.h
$(GLSRC)gsgcache.h:$(GLSRC)stdio_.h
$(GLSRC)gsgcache.h:$(GLSRC)stdint_.h
$(GLSRC)gsgcache.h:$(GLSRC)gssprintf.h
$(GLSRC)gsgcache.h:$(GLSRC)gstypes.h
$(GLSRC)gsgcache.h:$(GLSRC)std.h
$(GLSRC)gsgcache.h:$(GLSRC)stdpre.h
$(GLSRC)gsgcache.h:$(GLGEN)arch.h
$(GLSRC)gsgcache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gshsb.h:$(GLSRC)gsgstate.h
$(GLSRC)gsht.h:$(GLSRC)gsgstate.h
$(GLSRC)gsht.h:$(GLSRC)gstypes.h
$(GLSRC)gsht.h:$(GLSRC)std.h
$(GLSRC)gsht.h:$(GLSRC)stdpre.h
$(GLSRC)gsht.h:$(GLGEN)arch.h
$(GLSRC)gsht1.h:$(GLSRC)gsht.h
$(GLSRC)gsht1.h:$(GLSRC)gsgstate.h
$(GLSRC)gsht1.h:$(GLSRC)gstypes.h
$(GLSRC)gsht1.h:$(GLSRC)std.h
$(GLSRC)gsht1.h:$(GLSRC)stdpre.h
$(GLSRC)gsht1.h:$(GLGEN)arch.h
$(GLSRC)gsjconf.h:$(GLSRC)stdpre.h
$(GLSRC)gsjconf.h:$(GLGEN)arch.h
$(GLSRC)gslib.h:$(GLSRC)std.h
$(GLSRC)gslib.h:$(GLSRC)stdpre.h
$(GLSRC)gslib.h:$(GLGEN)arch.h
$(GLSRC)gsmatrix.h:$(GLSRC)scommon.h
$(GLSRC)gsmatrix.h:$(GLSRC)gsstype.h
$(GLSRC)gsmatrix.h:$(GLSRC)gsmemory.h
$(GLSRC)gsmatrix.h:$(GLSRC)gslibctx.h
$(GLSRC)gsmatrix.h:$(GLSRC)stdio_.h
$(GLSRC)gsmatrix.h:$(GLSRC)stdint_.h
$(GLSRC)gsmatrix.h:$(GLSRC)gssprintf.h
$(GLSRC)gsmatrix.h:$(GLSRC)gstypes.h
$(GLSRC)gsmatrix.h:$(GLSRC)std.h
$(GLSRC)gsmatrix.h:$(GLSRC)stdpre.h
$(GLSRC)gsmatrix.h:$(GLGEN)arch.h
$(GLSRC)gsmatrix.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxbitfmt.h:$(GLSRC)stdpre.h
$(GLSRC)gxcomp.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcomp.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxcomp.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcomp.h:$(GLSRC)gscompt.h
$(GLSRC)gxcomp.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcomp.h:$(GLSRC)gsparam.h
$(GLSRC)gxcomp.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcomp.h:$(GLSRC)memento.h
$(GLSRC)gxcomp.h:$(GLSRC)scommon.h
$(GLSRC)gxcomp.h:$(GLSRC)gsstype.h
$(GLSRC)gxcomp.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcomp.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcomp.h:$(GLSRC)stdio_.h
$(GLSRC)gxcomp.h:$(GLSRC)stdint_.h
$(GLSRC)gxcomp.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcomp.h:$(GLSRC)gstypes.h
$(GLSRC)gxcomp.h:$(GLSRC)std.h
$(GLSRC)gxcomp.h:$(GLSRC)stdpre.h
$(GLSRC)gxcomp.h:$(GLGEN)arch.h
$(GLSRC)gxcomp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsovrc.h:$(GLSRC)gxcomp.h
$(GLSRC)gsovrc.h:$(GLSRC)gsdevice.h
$(GLSRC)gsovrc.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gsovrc.h:$(GLSRC)gsgstate.h
$(GLSRC)gsovrc.h:$(GLSRC)gscompt.h
$(GLSRC)gsovrc.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsovrc.h:$(GLSRC)gsparam.h
$(GLSRC)gsovrc.h:$(GLSRC)gsrefct.h
$(GLSRC)gsovrc.h:$(GLSRC)memento.h
$(GLSRC)gsovrc.h:$(GLSRC)scommon.h
$(GLSRC)gsovrc.h:$(GLSRC)gsstype.h
$(GLSRC)gsovrc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsovrc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsovrc.h:$(GLSRC)gxcindex.h
$(GLSRC)gsovrc.h:$(GLSRC)stdio_.h
$(GLSRC)gsovrc.h:$(GLSRC)stdint_.h
$(GLSRC)gsovrc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsovrc.h:$(GLSRC)gstypes.h
$(GLSRC)gsovrc.h:$(GLSRC)std.h
$(GLSRC)gsovrc.h:$(GLSRC)stdpre.h
$(GLSRC)gsovrc.h:$(GLGEN)arch.h
$(GLSRC)gsovrc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gspaint.h:$(GLSRC)gsgstate.h
$(GLSRC)gspaint.h:$(GLSRC)stdpre.h
$(GLSRC)gsparam.h:$(GLSRC)gsstype.h
$(GLSRC)gsparam.h:$(GLSRC)gsmemory.h
$(GLSRC)gsparam.h:$(GLSRC)gslibctx.h
$(GLSRC)gsparam.h:$(GLSRC)stdio_.h
$(GLSRC)gsparam.h:$(GLSRC)stdint_.h
$(GLSRC)gsparam.h:$(GLSRC)gssprintf.h
$(GLSRC)gsparam.h:$(GLSRC)gstypes.h
$(GLSRC)gsparam.h:$(GLSRC)std.h
$(GLSRC)gsparam.h:$(GLSRC)stdpre.h
$(GLSRC)gsparam.h:$(GLGEN)arch.h
$(GLSRC)gsparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsparams.h:$(GLSRC)stream.h
$(GLSRC)gsparams.h:$(GLSRC)gxiodev.h
$(GLSRC)gsparams.h:$(GLSRC)gsparam.h
$(GLSRC)gsparams.h:$(GLSRC)gp.h
$(GLSRC)gsparams.h:$(GLSRC)memory_.h
$(GLSRC)gsparams.h:$(GLSRC)srdline.h
$(GLSRC)gsparams.h:$(GLSRC)scommon.h
$(GLSRC)gsparams.h:$(GLSRC)gsfname.h
$(GLSRC)gsparams.h:$(GLSRC)stat_.h
$(GLSRC)gsparams.h:$(GLSRC)gsstype.h
$(GLSRC)gsparams.h:$(GLSRC)gsmemory.h
$(GLSRC)gsparams.h:$(GLSRC)gpgetenv.h
$(GLSRC)gsparams.h:$(GLSRC)gscdefs.h
$(GLSRC)gsparams.h:$(GLSRC)gslibctx.h
$(GLSRC)gsparams.h:$(GLSRC)stdio_.h
$(GLSRC)gsparams.h:$(GLSRC)stdint_.h
$(GLSRC)gsparams.h:$(GLSRC)gssprintf.h
$(GLSRC)gsparams.h:$(GLSRC)gstypes.h
$(GLSRC)gsparams.h:$(GLSRC)std.h
$(GLSRC)gsparams.h:$(GLSRC)stdpre.h
$(GLSRC)gsparams.h:$(GLGEN)arch.h
$(GLSRC)gsparams.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsparamx.h:$(GLSRC)gsparam.h
$(GLSRC)gsparamx.h:$(GLSRC)gsstype.h
$(GLSRC)gsparamx.h:$(GLSRC)gsmemory.h
$(GLSRC)gsparamx.h:$(GLSRC)gslibctx.h
$(GLSRC)gsparamx.h:$(GLSRC)stdio_.h
$(GLSRC)gsparamx.h:$(GLSRC)stdint_.h
$(GLSRC)gsparamx.h:$(GLSRC)gssprintf.h
$(GLSRC)gsparamx.h:$(GLSRC)gstypes.h
$(GLSRC)gsparamx.h:$(GLSRC)std.h
$(GLSRC)gsparamx.h:$(GLSRC)stdpre.h
$(GLSRC)gsparamx.h:$(GLGEN)arch.h
$(GLSRC)gsparamx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gspath2.h:$(GLSRC)gsgstate.h
$(GLSRC)gspath2.h:$(GLSRC)gsmatrix.h
$(GLSRC)gspath2.h:$(GLSRC)scommon.h
$(GLSRC)gspath2.h:$(GLSRC)gsstype.h
$(GLSRC)gspath2.h:$(GLSRC)gsmemory.h
$(GLSRC)gspath2.h:$(GLSRC)gslibctx.h
$(GLSRC)gspath2.h:$(GLSRC)stdio_.h
$(GLSRC)gspath2.h:$(GLSRC)stdint_.h
$(GLSRC)gspath2.h:$(GLSRC)gssprintf.h
$(GLSRC)gspath2.h:$(GLSRC)gstypes.h
$(GLSRC)gspath2.h:$(GLSRC)std.h
$(GLSRC)gspath2.h:$(GLSRC)stdpre.h
$(GLSRC)gspath2.h:$(GLGEN)arch.h
$(GLSRC)gspath2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gspcolor.h:$(GLSRC)gsgstate.h
$(GLSRC)gspcolor.h:$(GLSRC)gsmatrix.h
$(GLSRC)gspcolor.h:$(GLSRC)gsrefct.h
$(GLSRC)gspcolor.h:$(GLSRC)memento.h
$(GLSRC)gspcolor.h:$(GLSRC)gsuid.h
$(GLSRC)gspcolor.h:$(GLSRC)scommon.h
$(GLSRC)gspcolor.h:$(GLSRC)gsccolor.h
$(GLSRC)gspcolor.h:$(GLSRC)gsstype.h
$(GLSRC)gspcolor.h:$(GLSRC)gsmemory.h
$(GLSRC)gspcolor.h:$(GLSRC)gslibctx.h
$(GLSRC)gspcolor.h:$(GLSRC)stdio_.h
$(GLSRC)gspcolor.h:$(GLSRC)stdint_.h
$(GLSRC)gspcolor.h:$(GLSRC)gssprintf.h
$(GLSRC)gspcolor.h:$(GLSRC)gstypes.h
$(GLSRC)gspcolor.h:$(GLSRC)std.h
$(GLSRC)gspcolor.h:$(GLSRC)stdpre.h
$(GLSRC)gspcolor.h:$(GLGEN)arch.h
$(GLSRC)gspcolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsptype1.h:$(GLSRC)gspcolor.h
$(GLSRC)gsptype1.h:$(GLSRC)gsdcolor.h
$(GLSRC)gsptype1.h:$(GLSRC)gscms.h
$(GLSRC)gsptype1.h:$(GLSRC)gsdevice.h
$(GLSRC)gsptype1.h:$(GLSRC)gscspace.h
$(GLSRC)gsptype1.h:$(GLSRC)gsgstate.h
$(GLSRC)gsptype1.h:$(GLSRC)gsiparam.h
$(GLSRC)gsptype1.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsptype1.h:$(GLSRC)gxhttile.h
$(GLSRC)gsptype1.h:$(GLSRC)gsparam.h
$(GLSRC)gsptype1.h:$(GLSRC)gsrefct.h
$(GLSRC)gsptype1.h:$(GLSRC)memento.h
$(GLSRC)gsptype1.h:$(GLSRC)gsuid.h
$(GLSRC)gsptype1.h:$(GLSRC)gxsync.h
$(GLSRC)gsptype1.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsptype1.h:$(GLSRC)scommon.h
$(GLSRC)gsptype1.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsptype1.h:$(GLSRC)gsccolor.h
$(GLSRC)gsptype1.h:$(GLSRC)gxarith.h
$(GLSRC)gsptype1.h:$(GLSRC)gpsync.h
$(GLSRC)gsptype1.h:$(GLSRC)gsstype.h
$(GLSRC)gsptype1.h:$(GLSRC)gsmemory.h
$(GLSRC)gsptype1.h:$(GLSRC)gslibctx.h
$(GLSRC)gsptype1.h:$(GLSRC)gxcindex.h
$(GLSRC)gsptype1.h:$(GLSRC)stdio_.h
$(GLSRC)gsptype1.h:$(GLSRC)stdint_.h
$(GLSRC)gsptype1.h:$(GLSRC)gssprintf.h
$(GLSRC)gsptype1.h:$(GLSRC)gstypes.h
$(GLSRC)gsptype1.h:$(GLSRC)std.h
$(GLSRC)gsptype1.h:$(GLSRC)stdpre.h
$(GLSRC)gsptype1.h:$(GLGEN)arch.h
$(GLSRC)gsptype1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsropt.h:$(GLSRC)stdpre.h
$(GLSRC)gstext.h:$(GLSRC)gsfont.h
$(GLSRC)gstext.h:$(GLSRC)gsdcolor.h
$(GLSRC)gstext.h:$(GLSRC)gxpath.h
$(GLSRC)gstext.h:$(GLSRC)gscms.h
$(GLSRC)gstext.h:$(GLSRC)gsrect.h
$(GLSRC)gstext.h:$(GLSRC)gslparam.h
$(GLSRC)gstext.h:$(GLSRC)gsdevice.h
$(GLSRC)gstext.h:$(GLSRC)gscpm.h
$(GLSRC)gstext.h:$(GLSRC)gscspace.h
$(GLSRC)gstext.h:$(GLSRC)gsgstate.h
$(GLSRC)gstext.h:$(GLSRC)gsiparam.h
$(GLSRC)gstext.h:$(GLSRC)gxfixed.h
$(GLSRC)gstext.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstext.h:$(GLSRC)gspenum.h
$(GLSRC)gstext.h:$(GLSRC)gxhttile.h
$(GLSRC)gstext.h:$(GLSRC)gsparam.h
$(GLSRC)gstext.h:$(GLSRC)gsrefct.h
$(GLSRC)gstext.h:$(GLSRC)memento.h
$(GLSRC)gstext.h:$(GLSRC)gxsync.h
$(GLSRC)gstext.h:$(GLSRC)gxbitmap.h
$(GLSRC)gstext.h:$(GLSRC)scommon.h
$(GLSRC)gstext.h:$(GLSRC)gsbitmap.h
$(GLSRC)gstext.h:$(GLSRC)gsccolor.h
$(GLSRC)gstext.h:$(GLSRC)gxarith.h
$(GLSRC)gstext.h:$(GLSRC)gpsync.h
$(GLSRC)gstext.h:$(GLSRC)gsstype.h
$(GLSRC)gstext.h:$(GLSRC)gsmemory.h
$(GLSRC)gstext.h:$(GLSRC)gslibctx.h
$(GLSRC)gstext.h:$(GLSRC)gxcindex.h
$(GLSRC)gstext.h:$(GLSRC)stdio_.h
$(GLSRC)gstext.h:$(GLSRC)gsccode.h
$(GLSRC)gstext.h:$(GLSRC)stdint_.h
$(GLSRC)gstext.h:$(GLSRC)gssprintf.h
$(GLSRC)gstext.h:$(GLSRC)gstypes.h
$(GLSRC)gstext.h:$(GLSRC)std.h
$(GLSRC)gstext.h:$(GLSRC)stdpre.h
$(GLSRC)gstext.h:$(GLGEN)arch.h
$(GLSRC)gstext.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsxfont.h:$(GLSRC)stdpre.h
$(GLSRC)gschar.h:$(GLSRC)gsstate.h
$(GLSRC)gschar.h:$(GLSRC)gsovrc.h
$(GLSRC)gschar.h:$(GLSRC)gscolor.h
$(GLSRC)gschar.h:$(GLSRC)gsline.h
$(GLSRC)gschar.h:$(GLSRC)gxcomp.h
$(GLSRC)gschar.h:$(GLSRC)gsht.h
$(GLSRC)gschar.h:$(GLSRC)gxtext.h
$(GLSRC)gschar.h:$(GLSRC)gstext.h
$(GLSRC)gschar.h:$(GLSRC)gscsel.h
$(GLSRC)gschar.h:$(GLSRC)gxfcache.h
$(GLSRC)gschar.h:$(GLSRC)gsfont.h
$(GLSRC)gschar.h:$(GLSRC)gsdcolor.h
$(GLSRC)gschar.h:$(GLSRC)gxbcache.h
$(GLSRC)gschar.h:$(GLSRC)gxpath.h
$(GLSRC)gschar.h:$(GLSRC)gxtmap.h
$(GLSRC)gschar.h:$(GLSRC)gxftype.h
$(GLSRC)gschar.h:$(GLSRC)gscms.h
$(GLSRC)gschar.h:$(GLSRC)gsrect.h
$(GLSRC)gschar.h:$(GLSRC)gslparam.h
$(GLSRC)gschar.h:$(GLSRC)gsdevice.h
$(GLSRC)gschar.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gschar.h:$(GLSRC)gscpm.h
$(GLSRC)gschar.h:$(GLSRC)gscspace.h
$(GLSRC)gschar.h:$(GLSRC)gsgstate.h
$(GLSRC)gschar.h:$(GLSRC)gsxfont.h
$(GLSRC)gschar.h:$(GLSRC)gsiparam.h
$(GLSRC)gschar.h:$(GLSRC)gxfixed.h
$(GLSRC)gschar.h:$(GLSRC)gscompt.h
$(GLSRC)gschar.h:$(GLSRC)gsmatrix.h
$(GLSRC)gschar.h:$(GLSRC)gspenum.h
$(GLSRC)gschar.h:$(GLSRC)gxhttile.h
$(GLSRC)gschar.h:$(GLSRC)gsparam.h
$(GLSRC)gschar.h:$(GLSRC)gsrefct.h
$(GLSRC)gschar.h:$(GLSRC)memento.h
$(GLSRC)gschar.h:$(GLSRC)gsuid.h
$(GLSRC)gschar.h:$(GLSRC)gxsync.h
$(GLSRC)gschar.h:$(GLSRC)gxbitmap.h
$(GLSRC)gschar.h:$(GLSRC)scommon.h
$(GLSRC)gschar.h:$(GLSRC)gsbitmap.h
$(GLSRC)gschar.h:$(GLSRC)gsccolor.h
$(GLSRC)gschar.h:$(GLSRC)gxarith.h
$(GLSRC)gschar.h:$(GLSRC)gpsync.h
$(GLSRC)gschar.h:$(GLSRC)gsstype.h
$(GLSRC)gschar.h:$(GLSRC)gsmemory.h
$(GLSRC)gschar.h:$(GLSRC)gslibctx.h
$(GLSRC)gschar.h:$(GLSRC)gxcindex.h
$(GLSRC)gschar.h:$(GLSRC)stdio_.h
$(GLSRC)gschar.h:$(GLSRC)gsccode.h
$(GLSRC)gschar.h:$(GLSRC)stdint_.h
$(GLSRC)gschar.h:$(GLSRC)gssprintf.h
$(GLSRC)gschar.h:$(GLSRC)gstypes.h
$(GLSRC)gschar.h:$(GLSRC)std.h
$(GLSRC)gschar.h:$(GLSRC)stdpre.h
$(GLSRC)gschar.h:$(GLGEN)arch.h
$(GLSRC)gschar.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsiparam.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsiparam.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsiparam.h:$(GLSRC)scommon.h
$(GLSRC)gsiparam.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsiparam.h:$(GLSRC)gsccolor.h
$(GLSRC)gsiparam.h:$(GLSRC)gsstype.h
$(GLSRC)gsiparam.h:$(GLSRC)gsmemory.h
$(GLSRC)gsiparam.h:$(GLSRC)gslibctx.h
$(GLSRC)gsiparam.h:$(GLSRC)stdio_.h
$(GLSRC)gsiparam.h:$(GLSRC)stdint_.h
$(GLSRC)gsiparam.h:$(GLSRC)gssprintf.h
$(GLSRC)gsiparam.h:$(GLSRC)gstypes.h
$(GLSRC)gsiparam.h:$(GLSRC)std.h
$(GLSRC)gsiparam.h:$(GLSRC)stdpre.h
$(GLSRC)gsiparam.h:$(GLGEN)arch.h
$(GLSRC)gsiparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsimage.h:$(GLSRC)gsdevice.h
$(GLSRC)gsimage.h:$(GLSRC)gsgstate.h
$(GLSRC)gsimage.h:$(GLSRC)gsiparam.h
$(GLSRC)gsimage.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsimage.h:$(GLSRC)gsparam.h
$(GLSRC)gsimage.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsimage.h:$(GLSRC)scommon.h
$(GLSRC)gsimage.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsimage.h:$(GLSRC)gsccolor.h
$(GLSRC)gsimage.h:$(GLSRC)gsstype.h
$(GLSRC)gsimage.h:$(GLSRC)gsmemory.h
$(GLSRC)gsimage.h:$(GLSRC)gslibctx.h
$(GLSRC)gsimage.h:$(GLSRC)stdio_.h
$(GLSRC)gsimage.h:$(GLSRC)stdint_.h
$(GLSRC)gsimage.h:$(GLSRC)gssprintf.h
$(GLSRC)gsimage.h:$(GLSRC)gstypes.h
$(GLSRC)gsimage.h:$(GLSRC)std.h
$(GLSRC)gsimage.h:$(GLSRC)stdpre.h
$(GLSRC)gsimage.h:$(GLGEN)arch.h
$(GLSRC)gsimage.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsline.h:$(GLSRC)gslparam.h
$(GLSRC)gsline.h:$(GLSRC)gsgstate.h
$(GLSRC)gsline.h:$(GLSRC)stdpre.h
$(GLSRC)gspath.h:$(GLSRC)gxmatrix.h
$(GLSRC)gspath.h:$(GLSRC)gsgstate.h
$(GLSRC)gspath.h:$(GLSRC)gxfixed.h
$(GLSRC)gspath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gspath.h:$(GLSRC)gspenum.h
$(GLSRC)gspath.h:$(GLSRC)scommon.h
$(GLSRC)gspath.h:$(GLSRC)gsstype.h
$(GLSRC)gspath.h:$(GLSRC)gsmemory.h
$(GLSRC)gspath.h:$(GLSRC)gslibctx.h
$(GLSRC)gspath.h:$(GLSRC)stdio_.h
$(GLSRC)gspath.h:$(GLSRC)stdint_.h
$(GLSRC)gspath.h:$(GLSRC)gssprintf.h
$(GLSRC)gspath.h:$(GLSRC)gstypes.h
$(GLSRC)gspath.h:$(GLSRC)std.h
$(GLSRC)gspath.h:$(GLSRC)stdpre.h
$(GLSRC)gspath.h:$(GLGEN)arch.h
$(GLSRC)gspath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsrop.h:$(GLSRC)gsropt.h
$(GLSRC)gsrop.h:$(GLSRC)gsgstate.h
$(GLSRC)gsrop.h:$(GLSRC)stdpre.h
$(GLSRC)gstparam.h:$(GLSRC)gsfunc.h
$(GLSRC)gstparam.h:$(GLSRC)gscspace.h
$(GLSRC)gstparam.h:$(GLSRC)gsgstate.h
$(GLSRC)gstparam.h:$(GLSRC)gsdsrc.h
$(GLSRC)gstparam.h:$(GLSRC)gsiparam.h
$(GLSRC)gstparam.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstparam.h:$(GLSRC)gsparam.h
$(GLSRC)gstparam.h:$(GLSRC)gsrefct.h
$(GLSRC)gstparam.h:$(GLSRC)memento.h
$(GLSRC)gstparam.h:$(GLSRC)gsstruct.h
$(GLSRC)gstparam.h:$(GLSRC)gxbitmap.h
$(GLSRC)gstparam.h:$(GLSRC)scommon.h
$(GLSRC)gstparam.h:$(GLSRC)gsbitmap.h
$(GLSRC)gstparam.h:$(GLSRC)gsccolor.h
$(GLSRC)gstparam.h:$(GLSRC)gsstype.h
$(GLSRC)gstparam.h:$(GLSRC)gsmemory.h
$(GLSRC)gstparam.h:$(GLSRC)gslibctx.h
$(GLSRC)gstparam.h:$(GLSRC)stdio_.h
$(GLSRC)gstparam.h:$(GLSRC)stdint_.h
$(GLSRC)gstparam.h:$(GLSRC)gssprintf.h
$(GLSRC)gstparam.h:$(GLSRC)gstypes.h
$(GLSRC)gstparam.h:$(GLSRC)std.h
$(GLSRC)gstparam.h:$(GLSRC)stdpre.h
$(GLSRC)gstparam.h:$(GLGEN)arch.h
$(GLSRC)gstparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxbcache.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxbcache.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxbcache.h:$(GLSRC)gsstype.h
$(GLSRC)gxbcache.h:$(GLSRC)gsmemory.h
$(GLSRC)gxbcache.h:$(GLSRC)gslibctx.h
$(GLSRC)gxbcache.h:$(GLSRC)stdio_.h
$(GLSRC)gxbcache.h:$(GLSRC)gssprintf.h
$(GLSRC)gxbcache.h:$(GLSRC)gstypes.h
$(GLSRC)gxbcache.h:$(GLSRC)std.h
$(GLSRC)gxbcache.h:$(GLSRC)stdpre.h
$(GLSRC)gxbcache.h:$(GLGEN)arch.h
$(GLSRC)gxbcache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclio.h:$(GLSRC)gp.h
$(GLSRC)gxclio.h:$(GLSRC)memory_.h
$(GLSRC)gxclio.h:$(GLSRC)srdline.h
$(GLSRC)gxclio.h:$(GLSRC)scommon.h
$(GLSRC)gxclio.h:$(GLSRC)stat_.h
$(GLSRC)gxclio.h:$(GLSRC)gsstype.h
$(GLSRC)gxclio.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclio.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclio.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclio.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclio.h:$(GLSRC)stdio_.h
$(GLSRC)gxclio.h:$(GLSRC)stdint_.h
$(GLSRC)gxclio.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclio.h:$(GLSRC)gstypes.h
$(GLSRC)gxclio.h:$(GLSRC)std.h
$(GLSRC)gxclio.h:$(GLSRC)stdpre.h
$(GLSRC)gxclio.h:$(GLGEN)arch.h
$(GLSRC)gxclio.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclip.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclip.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclip.h:$(GLSRC)gxtext.h
$(GLSRC)gxclip.h:$(GLSRC)gstext.h
$(GLSRC)gxclip.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclip.h:$(GLSRC)gstparam.h
$(GLSRC)gxclip.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclip.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclip.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclip.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclip.h:$(GLSRC)gscsel.h
$(GLSRC)gxclip.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclip.h:$(GLSRC)gsfont.h
$(GLSRC)gxclip.h:$(GLSRC)gsimage.h
$(GLSRC)gxclip.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclip.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclip.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclip.h:$(GLSRC)gsropt.h
$(GLSRC)gxclip.h:$(GLSRC)gxdda.h
$(GLSRC)gxclip.h:$(GLSRC)gxpath.h
$(GLSRC)gxclip.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclip.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclip.h:$(GLSRC)gxftype.h
$(GLSRC)gxclip.h:$(GLSRC)gscms.h
$(GLSRC)gxclip.h:$(GLSRC)gsrect.h
$(GLSRC)gxclip.h:$(GLSRC)gslparam.h
$(GLSRC)gxclip.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclip.h:$(GLSRC)gscpm.h
$(GLSRC)gxclip.h:$(GLSRC)gscspace.h
$(GLSRC)gxclip.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclip.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclip.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclip.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclip.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclip.h:$(GLSRC)gscompt.h
$(GLSRC)gxclip.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclip.h:$(GLSRC)gspenum.h
$(GLSRC)gxclip.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclip.h:$(GLSRC)gsparam.h
$(GLSRC)gxclip.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclip.h:$(GLSRC)gp.h
$(GLSRC)gxclip.h:$(GLSRC)memento.h
$(GLSRC)gxclip.h:$(GLSRC)memory_.h
$(GLSRC)gxclip.h:$(GLSRC)gsuid.h
$(GLSRC)gxclip.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclip.h:$(GLSRC)gxsync.h
$(GLSRC)gxclip.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclip.h:$(GLSRC)srdline.h
$(GLSRC)gxclip.h:$(GLSRC)scommon.h
$(GLSRC)gxclip.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclip.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclip.h:$(GLSRC)gxarith.h
$(GLSRC)gxclip.h:$(GLSRC)stat_.h
$(GLSRC)gxclip.h:$(GLSRC)gpsync.h
$(GLSRC)gxclip.h:$(GLSRC)gsstype.h
$(GLSRC)gxclip.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclip.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclip.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclip.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclip.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclip.h:$(GLSRC)stdio_.h
$(GLSRC)gxclip.h:$(GLSRC)gsccode.h
$(GLSRC)gxclip.h:$(GLSRC)stdint_.h
$(GLSRC)gxclip.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclip.h:$(GLSRC)gstypes.h
$(GLSRC)gxclip.h:$(GLSRC)std.h
$(GLSRC)gxclip.h:$(GLSRC)stdpre.h
$(GLSRC)gxclip.h:$(GLGEN)arch.h
$(GLSRC)gxclip.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclipsr.h:$(GLSRC)gxpath.h
$(GLSRC)gxclipsr.h:$(GLSRC)gsrect.h
$(GLSRC)gxclipsr.h:$(GLSRC)gslparam.h
$(GLSRC)gxclipsr.h:$(GLSRC)gscpm.h
$(GLSRC)gxclipsr.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclipsr.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclipsr.h:$(GLSRC)gspenum.h
$(GLSRC)gxclipsr.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclipsr.h:$(GLSRC)memento.h
$(GLSRC)gxclipsr.h:$(GLSRC)gstypes.h
$(GLSRC)gxclipsr.h:$(GLSRC)std.h
$(GLSRC)gxclipsr.h:$(GLSRC)stdpre.h
$(GLSRC)gxclipsr.h:$(GLGEN)arch.h
$(GLSRC)gxcoord.h:$(GLSRC)gscoord.h
$(GLSRC)gxcoord.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxcoord.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcoord.h:$(GLSRC)gxfixed.h
$(GLSRC)gxcoord.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcoord.h:$(GLSRC)scommon.h
$(GLSRC)gxcoord.h:$(GLSRC)gsstype.h
$(GLSRC)gxcoord.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcoord.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcoord.h:$(GLSRC)stdio_.h
$(GLSRC)gxcoord.h:$(GLSRC)stdint_.h
$(GLSRC)gxcoord.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcoord.h:$(GLSRC)gstypes.h
$(GLSRC)gxcoord.h:$(GLSRC)std.h
$(GLSRC)gxcoord.h:$(GLSRC)stdpre.h
$(GLSRC)gxcoord.h:$(GLGEN)arch.h
$(GLSRC)gxcoord.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcpath.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxcpath.h:$(GLSRC)gxcmap.h
$(GLSRC)gxcpath.h:$(GLSRC)gxtext.h
$(GLSRC)gxcpath.h:$(GLSRC)gstext.h
$(GLSRC)gxcpath.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxcpath.h:$(GLSRC)gstparam.h
$(GLSRC)gxcpath.h:$(GLSRC)gxfmap.h
$(GLSRC)gxcpath.h:$(GLSRC)gsfunc.h
$(GLSRC)gxcpath.h:$(GLSRC)gxcspace.h
$(GLSRC)gxcpath.h:$(GLSRC)gxrplane.h
$(GLSRC)gxcpath.h:$(GLSRC)gscsel.h
$(GLSRC)gxcpath.h:$(GLSRC)gxfcache.h
$(GLSRC)gxcpath.h:$(GLSRC)gsfont.h
$(GLSRC)gxcpath.h:$(GLSRC)gsimage.h
$(GLSRC)gxcpath.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcpath.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxcpath.h:$(GLSRC)gxbcache.h
$(GLSRC)gxcpath.h:$(GLSRC)gsropt.h
$(GLSRC)gxcpath.h:$(GLSRC)gxdda.h
$(GLSRC)gxcpath.h:$(GLSRC)gxpath.h
$(GLSRC)gxcpath.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcpath.h:$(GLSRC)gxtmap.h
$(GLSRC)gxcpath.h:$(GLSRC)gxftype.h
$(GLSRC)gxcpath.h:$(GLSRC)gscms.h
$(GLSRC)gxcpath.h:$(GLSRC)gsrect.h
$(GLSRC)gxcpath.h:$(GLSRC)gslparam.h
$(GLSRC)gxcpath.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcpath.h:$(GLSRC)gscpm.h
$(GLSRC)gxcpath.h:$(GLSRC)gscspace.h
$(GLSRC)gxcpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcpath.h:$(GLSRC)gsxfont.h
$(GLSRC)gxcpath.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxcpath.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gxcpath.h:$(GLSRC)gscompt.h
$(GLSRC)gxcpath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcpath.h:$(GLSRC)gspenum.h
$(GLSRC)gxcpath.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcpath.h:$(GLSRC)gsparam.h
$(GLSRC)gxcpath.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcpath.h:$(GLSRC)gp.h
$(GLSRC)gxcpath.h:$(GLSRC)memento.h
$(GLSRC)gxcpath.h:$(GLSRC)memory_.h
$(GLSRC)gxcpath.h:$(GLSRC)gsuid.h
$(GLSRC)gxcpath.h:$(GLSRC)gsstruct.h
$(GLSRC)gxcpath.h:$(GLSRC)gxsync.h
$(GLSRC)gxcpath.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcpath.h:$(GLSRC)srdline.h
$(GLSRC)gxcpath.h:$(GLSRC)scommon.h
$(GLSRC)gxcpath.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcpath.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcpath.h:$(GLSRC)gxarith.h
$(GLSRC)gxcpath.h:$(GLSRC)stat_.h
$(GLSRC)gxcpath.h:$(GLSRC)gpsync.h
$(GLSRC)gxcpath.h:$(GLSRC)gsstype.h
$(GLSRC)gxcpath.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcpath.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxcpath.h:$(GLSRC)gscdefs.h
$(GLSRC)gxcpath.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcpath.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcpath.h:$(GLSRC)stdio_.h
$(GLSRC)gxcpath.h:$(GLSRC)gsccode.h
$(GLSRC)gxcpath.h:$(GLSRC)stdint_.h
$(GLSRC)gxcpath.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcpath.h:$(GLSRC)gstypes.h
$(GLSRC)gxcpath.h:$(GLSRC)std.h
$(GLSRC)gxcpath.h:$(GLSRC)stdpre.h
$(GLSRC)gxcpath.h:$(GLGEN)arch.h
$(GLSRC)gxcpath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdda.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdda.h:$(GLSRC)std.h
$(GLSRC)gxdda.h:$(GLSRC)stdpre.h
$(GLSRC)gxdda.h:$(GLGEN)arch.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxband.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxtext.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gstext.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gstparam.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscsel.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsfont.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsimage.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsropt.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxdda.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxpath.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxftype.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscms.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsrect.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gslparam.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscpm.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscspace.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxclio.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscompt.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gspenum.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsparam.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gp.h
$(GLSRC)gxdevbuf.h:$(GLSRC)memento.h
$(GLSRC)gxdevbuf.h:$(GLSRC)memory_.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsuid.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxsync.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdevbuf.h:$(GLSRC)srdline.h
$(GLSRC)gxdevbuf.h:$(GLSRC)scommon.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxarith.h
$(GLSRC)gxdevbuf.h:$(GLSRC)stat_.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gpsync.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsstype.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdevbuf.h:$(GLSRC)stdio_.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gsccode.h
$(GLSRC)gxdevbuf.h:$(GLSRC)stdint_.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gstypes.h
$(GLSRC)gxdevbuf.h:$(GLSRC)std.h
$(GLSRC)gxdevbuf.h:$(GLSRC)stdpre.h
$(GLSRC)gxdevbuf.h:$(GLGEN)arch.h
$(GLSRC)gxdevbuf.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxtext.h
$(GLSRC)gxdevmem.h:$(GLSRC)gstext.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdevmem.h:$(GLSRC)gstparam.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscsel.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsfont.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsimage.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsropt.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxdda.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxpath.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxftype.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscms.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsrect.h
$(GLSRC)gxdevmem.h:$(GLSRC)gslparam.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscpm.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscspace.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscompt.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdevmem.h:$(GLSRC)gspenum.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsparam.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdevmem.h:$(GLSRC)gp.h
$(GLSRC)gxdevmem.h:$(GLSRC)memento.h
$(GLSRC)gxdevmem.h:$(GLSRC)memory_.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsuid.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxsync.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdevmem.h:$(GLSRC)srdline.h
$(GLSRC)gxdevmem.h:$(GLSRC)scommon.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxarith.h
$(GLSRC)gxdevmem.h:$(GLSRC)stat_.h
$(GLSRC)gxdevmem.h:$(GLSRC)gpsync.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsstype.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdevmem.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdevmem.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdevmem.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdevmem.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdevmem.h:$(GLSRC)stdio_.h
$(GLSRC)gxdevmem.h:$(GLSRC)gsccode.h
$(GLSRC)gxdevmem.h:$(GLSRC)stdint_.h
$(GLSRC)gxdevmem.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdevmem.h:$(GLSRC)gstypes.h
$(GLSRC)gxdevmem.h:$(GLSRC)std.h
$(GLSRC)gxdevmem.h:$(GLSRC)stdpre.h
$(GLSRC)gxdevmem.h:$(GLGEN)arch.h
$(GLSRC)gxdevmem.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdhtres.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont0.h:$(GLSRC)gxfont.h
$(GLSRC)gxfont0.h:$(GLSRC)gspath.h
$(GLSRC)gxfont0.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfont0.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfont0.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfont0.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfont0.h:$(GLSRC)gstext.h
$(GLSRC)gxfont0.h:$(GLSRC)gsfont.h
$(GLSRC)gxfont0.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfont0.h:$(GLSRC)gxpath.h
$(GLSRC)gxfont0.h:$(GLSRC)gxftype.h
$(GLSRC)gxfont0.h:$(GLSRC)gscms.h
$(GLSRC)gxfont0.h:$(GLSRC)gsrect.h
$(GLSRC)gxfont0.h:$(GLSRC)gslparam.h
$(GLSRC)gxfont0.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfont0.h:$(GLSRC)gscpm.h
$(GLSRC)gxfont0.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfont0.h:$(GLSRC)gscspace.h
$(GLSRC)gxfont0.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfont0.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfont0.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfont0.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfont0.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfont0.h:$(GLSRC)gspenum.h
$(GLSRC)gxfont0.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfont0.h:$(GLSRC)gsparam.h
$(GLSRC)gxfont0.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfont0.h:$(GLSRC)memento.h
$(GLSRC)gxfont0.h:$(GLSRC)gsuid.h
$(GLSRC)gxfont0.h:$(GLSRC)gxsync.h
$(GLSRC)gxfont0.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfont0.h:$(GLSRC)scommon.h
$(GLSRC)gxfont0.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfont0.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfont0.h:$(GLSRC)gxarith.h
$(GLSRC)gxfont0.h:$(GLSRC)gpsync.h
$(GLSRC)gxfont0.h:$(GLSRC)gsstype.h
$(GLSRC)gxfont0.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfont0.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfont0.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfont0.h:$(GLSRC)stdio_.h
$(GLSRC)gxfont0.h:$(GLSRC)gsccode.h
$(GLSRC)gxfont0.h:$(GLSRC)stdint_.h
$(GLSRC)gxfont0.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfont0.h:$(GLSRC)gstypes.h
$(GLSRC)gxfont0.h:$(GLSRC)std.h
$(GLSRC)gxfont0.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont0.h:$(GLGEN)arch.h
$(GLSRC)gxfont0.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxcmap.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxtext.h
$(GLSRC)gxgetbit.h:$(GLSRC)gstext.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxgetbit.h:$(GLSRC)gstparam.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxfmap.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsfunc.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxcspace.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxrplane.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscsel.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxfcache.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsfont.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsimage.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxbcache.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsropt.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxdda.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxpath.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxfrac.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxtmap.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxftype.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscms.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsrect.h
$(GLSRC)gxgetbit.h:$(GLSRC)gslparam.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsdevice.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscpm.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscspace.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsgstate.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsxfont.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsiparam.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxfixed.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscompt.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxgetbit.h:$(GLSRC)gspenum.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxhttile.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsparam.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsrefct.h
$(GLSRC)gxgetbit.h:$(GLSRC)gp.h
$(GLSRC)gxgetbit.h:$(GLSRC)memento.h
$(GLSRC)gxgetbit.h:$(GLSRC)memory_.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsuid.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsstruct.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxsync.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxgetbit.h:$(GLSRC)srdline.h
$(GLSRC)gxgetbit.h:$(GLSRC)scommon.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsccolor.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxarith.h
$(GLSRC)gxgetbit.h:$(GLSRC)stat_.h
$(GLSRC)gxgetbit.h:$(GLSRC)gpsync.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsstype.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsmemory.h
$(GLSRC)gxgetbit.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxgetbit.h:$(GLSRC)gscdefs.h
$(GLSRC)gxgetbit.h:$(GLSRC)gslibctx.h
$(GLSRC)gxgetbit.h:$(GLSRC)gxcindex.h
$(GLSRC)gxgetbit.h:$(GLSRC)stdio_.h
$(GLSRC)gxgetbit.h:$(GLSRC)gsccode.h
$(GLSRC)gxgetbit.h:$(GLSRC)stdint_.h
$(GLSRC)gxgetbit.h:$(GLSRC)gssprintf.h
$(GLSRC)gxgetbit.h:$(GLSRC)gstypes.h
$(GLSRC)gxgetbit.h:$(GLSRC)std.h
$(GLSRC)gxgetbit.h:$(GLSRC)stdpre.h
$(GLSRC)gxgetbit.h:$(GLGEN)arch.h
$(GLSRC)gxgetbit.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxhttile.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxhttile.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxhttile.h:$(GLSRC)gsstype.h
$(GLSRC)gxhttile.h:$(GLSRC)gsmemory.h
$(GLSRC)gxhttile.h:$(GLSRC)gslibctx.h
$(GLSRC)gxhttile.h:$(GLSRC)stdio_.h
$(GLSRC)gxhttile.h:$(GLSRC)gssprintf.h
$(GLSRC)gxhttile.h:$(GLSRC)gstypes.h
$(GLSRC)gxhttile.h:$(GLSRC)std.h
$(GLSRC)gxhttile.h:$(GLSRC)stdpre.h
$(GLSRC)gxhttile.h:$(GLGEN)arch.h
$(GLSRC)gxhttile.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxiclass.h:$(GLSRC)gsdevice.h
$(GLSRC)gxiclass.h:$(GLSRC)gsgstate.h
$(GLSRC)gxiclass.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxiclass.h:$(GLSRC)gsparam.h
$(GLSRC)gxiclass.h:$(GLSRC)scommon.h
$(GLSRC)gxiclass.h:$(GLSRC)gsstype.h
$(GLSRC)gxiclass.h:$(GLSRC)gsmemory.h
$(GLSRC)gxiclass.h:$(GLSRC)gslibctx.h
$(GLSRC)gxiclass.h:$(GLSRC)stdio_.h
$(GLSRC)gxiclass.h:$(GLSRC)stdint_.h
$(GLSRC)gxiclass.h:$(GLSRC)gssprintf.h
$(GLSRC)gxiclass.h:$(GLSRC)gstypes.h
$(GLSRC)gxiclass.h:$(GLSRC)std.h
$(GLSRC)gxiclass.h:$(GLSRC)stdpre.h
$(GLSRC)gxiclass.h:$(GLGEN)arch.h
$(GLSRC)gxiclass.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxiodev.h:$(GLSRC)gsparam.h
$(GLSRC)gxiodev.h:$(GLSRC)gp.h
$(GLSRC)gxiodev.h:$(GLSRC)memory_.h
$(GLSRC)gxiodev.h:$(GLSRC)srdline.h
$(GLSRC)gxiodev.h:$(GLSRC)scommon.h
$(GLSRC)gxiodev.h:$(GLSRC)gsfname.h
$(GLSRC)gxiodev.h:$(GLSRC)stat_.h
$(GLSRC)gxiodev.h:$(GLSRC)gsstype.h
$(GLSRC)gxiodev.h:$(GLSRC)gsmemory.h
$(GLSRC)gxiodev.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxiodev.h:$(GLSRC)gscdefs.h
$(GLSRC)gxiodev.h:$(GLSRC)gslibctx.h
$(GLSRC)gxiodev.h:$(GLSRC)stdio_.h
$(GLSRC)gxiodev.h:$(GLSRC)stdint_.h
$(GLSRC)gxiodev.h:$(GLSRC)gssprintf.h
$(GLSRC)gxiodev.h:$(GLSRC)gstypes.h
$(GLSRC)gxiodev.h:$(GLSRC)std.h
$(GLSRC)gxiodev.h:$(GLSRC)stdpre.h
$(GLSRC)gxiodev.h:$(GLGEN)arch.h
$(GLSRC)gxiodev.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxline.h:$(GLSRC)math_.h
$(GLSRC)gxline.h:$(GLSRC)gslparam.h
$(GLSRC)gxline.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxline.h:$(GLSRC)vmsmath.h
$(GLSRC)gxline.h:$(GLSRC)scommon.h
$(GLSRC)gxline.h:$(GLSRC)gsstype.h
$(GLSRC)gxline.h:$(GLSRC)gsmemory.h
$(GLSRC)gxline.h:$(GLSRC)gslibctx.h
$(GLSRC)gxline.h:$(GLSRC)stdio_.h
$(GLSRC)gxline.h:$(GLSRC)stdint_.h
$(GLSRC)gxline.h:$(GLSRC)gssprintf.h
$(GLSRC)gxline.h:$(GLSRC)gstypes.h
$(GLSRC)gxline.h:$(GLSRC)std.h
$(GLSRC)gxline.h:$(GLSRC)stdpre.h
$(GLSRC)gxline.h:$(GLGEN)arch.h
$(GLSRC)gxline.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxmatrix.h:$(GLSRC)gxfixed.h
$(GLSRC)gxmatrix.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxmatrix.h:$(GLSRC)scommon.h
$(GLSRC)gxmatrix.h:$(GLSRC)gsstype.h
$(GLSRC)gxmatrix.h:$(GLSRC)gsmemory.h
$(GLSRC)gxmatrix.h:$(GLSRC)gslibctx.h
$(GLSRC)gxmatrix.h:$(GLSRC)stdio_.h
$(GLSRC)gxmatrix.h:$(GLSRC)stdint_.h
$(GLSRC)gxmatrix.h:$(GLSRC)gssprintf.h
$(GLSRC)gxmatrix.h:$(GLSRC)gstypes.h
$(GLSRC)gxmatrix.h:$(GLSRC)std.h
$(GLSRC)gxmatrix.h:$(GLSRC)stdpre.h
$(GLSRC)gxmatrix.h:$(GLGEN)arch.h
$(GLSRC)gxmatrix.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxmclip.h:$(GLSRC)gxclip.h
$(GLSRC)gxmclip.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxmclip.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxmclip.h:$(GLSRC)gxcmap.h
$(GLSRC)gxmclip.h:$(GLSRC)gxtext.h
$(GLSRC)gxmclip.h:$(GLSRC)gstext.h
$(GLSRC)gxmclip.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxmclip.h:$(GLSRC)gstparam.h
$(GLSRC)gxmclip.h:$(GLSRC)gxfmap.h
$(GLSRC)gxmclip.h:$(GLSRC)gsfunc.h
$(GLSRC)gxmclip.h:$(GLSRC)gxcspace.h
$(GLSRC)gxmclip.h:$(GLSRC)gxrplane.h
$(GLSRC)gxmclip.h:$(GLSRC)gscsel.h
$(GLSRC)gxmclip.h:$(GLSRC)gxfcache.h
$(GLSRC)gxmclip.h:$(GLSRC)gsfont.h
$(GLSRC)gxmclip.h:$(GLSRC)gsimage.h
$(GLSRC)gxmclip.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxmclip.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxmclip.h:$(GLSRC)gxbcache.h
$(GLSRC)gxmclip.h:$(GLSRC)gsropt.h
$(GLSRC)gxmclip.h:$(GLSRC)gxdda.h
$(GLSRC)gxmclip.h:$(GLSRC)gxpath.h
$(GLSRC)gxmclip.h:$(GLSRC)gxfrac.h
$(GLSRC)gxmclip.h:$(GLSRC)gxtmap.h
$(GLSRC)gxmclip.h:$(GLSRC)gxftype.h
$(GLSRC)gxmclip.h:$(GLSRC)gscms.h
$(GLSRC)gxmclip.h:$(GLSRC)gsrect.h
$(GLSRC)gxmclip.h:$(GLSRC)gslparam.h
$(GLSRC)gxmclip.h:$(GLSRC)gsdevice.h
$(GLSRC)gxmclip.h:$(GLSRC)gscpm.h
$(GLSRC)gxmclip.h:$(GLSRC)gscspace.h
$(GLSRC)gxmclip.h:$(GLSRC)gsgstate.h
$(GLSRC)gxmclip.h:$(GLSRC)gsxfont.h
$(GLSRC)gxmclip.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxmclip.h:$(GLSRC)gsiparam.h
$(GLSRC)gxmclip.h:$(GLSRC)gxfixed.h
$(GLSRC)gxmclip.h:$(GLSRC)gscompt.h
$(GLSRC)gxmclip.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxmclip.h:$(GLSRC)gspenum.h
$(GLSRC)gxmclip.h:$(GLSRC)gxhttile.h
$(GLSRC)gxmclip.h:$(GLSRC)gsparam.h
$(GLSRC)gxmclip.h:$(GLSRC)gsrefct.h
$(GLSRC)gxmclip.h:$(GLSRC)gp.h
$(GLSRC)gxmclip.h:$(GLSRC)memento.h
$(GLSRC)gxmclip.h:$(GLSRC)memory_.h
$(GLSRC)gxmclip.h:$(GLSRC)gsuid.h
$(GLSRC)gxmclip.h:$(GLSRC)gsstruct.h
$(GLSRC)gxmclip.h:$(GLSRC)gxsync.h
$(GLSRC)gxmclip.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxmclip.h:$(GLSRC)srdline.h
$(GLSRC)gxmclip.h:$(GLSRC)scommon.h
$(GLSRC)gxmclip.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxmclip.h:$(GLSRC)gsccolor.h
$(GLSRC)gxmclip.h:$(GLSRC)gxarith.h
$(GLSRC)gxmclip.h:$(GLSRC)stat_.h
$(GLSRC)gxmclip.h:$(GLSRC)gpsync.h
$(GLSRC)gxmclip.h:$(GLSRC)gsstype.h
$(GLSRC)gxmclip.h:$(GLSRC)gsmemory.h
$(GLSRC)gxmclip.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxmclip.h:$(GLSRC)gscdefs.h
$(GLSRC)gxmclip.h:$(GLSRC)gslibctx.h
$(GLSRC)gxmclip.h:$(GLSRC)gxcindex.h
$(GLSRC)gxmclip.h:$(GLSRC)stdio_.h
$(GLSRC)gxmclip.h:$(GLSRC)gsccode.h
$(GLSRC)gxmclip.h:$(GLSRC)stdint_.h
$(GLSRC)gxmclip.h:$(GLSRC)gssprintf.h
$(GLSRC)gxmclip.h:$(GLSRC)gstypes.h
$(GLSRC)gxmclip.h:$(GLSRC)std.h
$(GLSRC)gxmclip.h:$(GLSRC)stdpre.h
$(GLSRC)gxmclip.h:$(GLGEN)arch.h
$(GLSRC)gxmclip.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxoprect.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxoprect.h:$(GLSRC)gxcmap.h
$(GLSRC)gxoprect.h:$(GLSRC)gxtext.h
$(GLSRC)gxoprect.h:$(GLSRC)gstext.h
$(GLSRC)gxoprect.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxoprect.h:$(GLSRC)gstparam.h
$(GLSRC)gxoprect.h:$(GLSRC)gxfmap.h
$(GLSRC)gxoprect.h:$(GLSRC)gsfunc.h
$(GLSRC)gxoprect.h:$(GLSRC)gxcspace.h
$(GLSRC)gxoprect.h:$(GLSRC)gxrplane.h
$(GLSRC)gxoprect.h:$(GLSRC)gscsel.h
$(GLSRC)gxoprect.h:$(GLSRC)gxfcache.h
$(GLSRC)gxoprect.h:$(GLSRC)gsfont.h
$(GLSRC)gxoprect.h:$(GLSRC)gsimage.h
$(GLSRC)gxoprect.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxoprect.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxoprect.h:$(GLSRC)gxbcache.h
$(GLSRC)gxoprect.h:$(GLSRC)gsropt.h
$(GLSRC)gxoprect.h:$(GLSRC)gxdda.h
$(GLSRC)gxoprect.h:$(GLSRC)gxpath.h
$(GLSRC)gxoprect.h:$(GLSRC)gxfrac.h
$(GLSRC)gxoprect.h:$(GLSRC)gxtmap.h
$(GLSRC)gxoprect.h:$(GLSRC)gxftype.h
$(GLSRC)gxoprect.h:$(GLSRC)gscms.h
$(GLSRC)gxoprect.h:$(GLSRC)gsrect.h
$(GLSRC)gxoprect.h:$(GLSRC)gslparam.h
$(GLSRC)gxoprect.h:$(GLSRC)gsdevice.h
$(GLSRC)gxoprect.h:$(GLSRC)gscpm.h
$(GLSRC)gxoprect.h:$(GLSRC)gscspace.h
$(GLSRC)gxoprect.h:$(GLSRC)gsgstate.h
$(GLSRC)gxoprect.h:$(GLSRC)gsxfont.h
$(GLSRC)gxoprect.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxoprect.h:$(GLSRC)gsiparam.h
$(GLSRC)gxoprect.h:$(GLSRC)gxfixed.h
$(GLSRC)gxoprect.h:$(GLSRC)gscompt.h
$(GLSRC)gxoprect.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxoprect.h:$(GLSRC)gspenum.h
$(GLSRC)gxoprect.h:$(GLSRC)gxhttile.h
$(GLSRC)gxoprect.h:$(GLSRC)gsparam.h
$(GLSRC)gxoprect.h:$(GLSRC)gsrefct.h
$(GLSRC)gxoprect.h:$(GLSRC)gp.h
$(GLSRC)gxoprect.h:$(GLSRC)memento.h
$(GLSRC)gxoprect.h:$(GLSRC)memory_.h
$(GLSRC)gxoprect.h:$(GLSRC)gsuid.h
$(GLSRC)gxoprect.h:$(GLSRC)gsstruct.h
$(GLSRC)gxoprect.h:$(GLSRC)gxsync.h
$(GLSRC)gxoprect.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxoprect.h:$(GLSRC)srdline.h
$(GLSRC)gxoprect.h:$(GLSRC)scommon.h
$(GLSRC)gxoprect.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxoprect.h:$(GLSRC)gsccolor.h
$(GLSRC)gxoprect.h:$(GLSRC)gxarith.h
$(GLSRC)gxoprect.h:$(GLSRC)stat_.h
$(GLSRC)gxoprect.h:$(GLSRC)gpsync.h
$(GLSRC)gxoprect.h:$(GLSRC)gsstype.h
$(GLSRC)gxoprect.h:$(GLSRC)gsmemory.h
$(GLSRC)gxoprect.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxoprect.h:$(GLSRC)gscdefs.h
$(GLSRC)gxoprect.h:$(GLSRC)gslibctx.h
$(GLSRC)gxoprect.h:$(GLSRC)gxcindex.h
$(GLSRC)gxoprect.h:$(GLSRC)stdio_.h
$(GLSRC)gxoprect.h:$(GLSRC)gsccode.h
$(GLSRC)gxoprect.h:$(GLSRC)stdint_.h
$(GLSRC)gxoprect.h:$(GLSRC)gssprintf.h
$(GLSRC)gxoprect.h:$(GLSRC)gstypes.h
$(GLSRC)gxoprect.h:$(GLSRC)std.h
$(GLSRC)gxoprect.h:$(GLSRC)stdpre.h
$(GLSRC)gxoprect.h:$(GLGEN)arch.h
$(GLSRC)gxoprect.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxblend.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxdevice.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxcpath.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxpcache.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxcmap.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxtext.h
$(GLSRC)gxp1impl.h:$(GLSRC)gstext.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxp1impl.h:$(GLSRC)gstparam.h
$(GLSRC)gxp1impl.h:$(GLSRC)gspcolor.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxfmap.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsfunc.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxcspace.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxrplane.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscsel.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxfcache.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsfont.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsimage.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxbcache.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsropt.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxdda.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxpath.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxiclass.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxfrac.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxtmap.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxftype.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscms.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsrect.h
$(GLSRC)gxp1impl.h:$(GLSRC)gslparam.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsdevice.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscpm.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscspace.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsgstate.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxstdio.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsxfont.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsio.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsiparam.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxfixed.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscompt.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxp1impl.h:$(GLSRC)gspenum.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxhttile.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsparam.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsrefct.h
$(GLSRC)gxp1impl.h:$(GLSRC)gp.h
$(GLSRC)gxp1impl.h:$(GLSRC)memento.h
$(GLSRC)gxp1impl.h:$(GLSRC)memory_.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsuid.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsstruct.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxsync.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxp1impl.h:$(GLSRC)srdline.h
$(GLSRC)gxp1impl.h:$(GLSRC)scommon.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsfname.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsccolor.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxarith.h
$(GLSRC)gxp1impl.h:$(GLSRC)stat_.h
$(GLSRC)gxp1impl.h:$(GLSRC)gpsync.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsstype.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsmemory.h
$(GLSRC)gxp1impl.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxp1impl.h:$(GLSRC)gscdefs.h
$(GLSRC)gxp1impl.h:$(GLSRC)gslibctx.h
$(GLSRC)gxp1impl.h:$(GLSRC)gxcindex.h
$(GLSRC)gxp1impl.h:$(GLSRC)stdio_.h
$(GLSRC)gxp1impl.h:$(GLSRC)gsccode.h
$(GLSRC)gxp1impl.h:$(GLSRC)stdint_.h
$(GLSRC)gxp1impl.h:$(GLSRC)gssprintf.h
$(GLSRC)gxp1impl.h:$(GLSRC)gstypes.h
$(GLSRC)gxp1impl.h:$(GLSRC)std.h
$(GLSRC)gxp1impl.h:$(GLSRC)stdpre.h
$(GLSRC)gxp1impl.h:$(GLGEN)arch.h
$(GLSRC)gxp1impl.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxpaint.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxpaint.h:$(GLSRC)gxpath.h
$(GLSRC)gxpaint.h:$(GLSRC)gscms.h
$(GLSRC)gxpaint.h:$(GLSRC)gsrect.h
$(GLSRC)gxpaint.h:$(GLSRC)gslparam.h
$(GLSRC)gxpaint.h:$(GLSRC)gsdevice.h
$(GLSRC)gxpaint.h:$(GLSRC)gscpm.h
$(GLSRC)gxpaint.h:$(GLSRC)gscspace.h
$(GLSRC)gxpaint.h:$(GLSRC)gsgstate.h
$(GLSRC)gxpaint.h:$(GLSRC)gsiparam.h
$(GLSRC)gxpaint.h:$(GLSRC)gxfixed.h
$(GLSRC)gxpaint.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxpaint.h:$(GLSRC)gspenum.h
$(GLSRC)gxpaint.h:$(GLSRC)gxhttile.h
$(GLSRC)gxpaint.h:$(GLSRC)gsparam.h
$(GLSRC)gxpaint.h:$(GLSRC)gsrefct.h
$(GLSRC)gxpaint.h:$(GLSRC)memento.h
$(GLSRC)gxpaint.h:$(GLSRC)gxsync.h
$(GLSRC)gxpaint.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxpaint.h:$(GLSRC)scommon.h
$(GLSRC)gxpaint.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxpaint.h:$(GLSRC)gsccolor.h
$(GLSRC)gxpaint.h:$(GLSRC)gxarith.h
$(GLSRC)gxpaint.h:$(GLSRC)gpsync.h
$(GLSRC)gxpaint.h:$(GLSRC)gsstype.h
$(GLSRC)gxpaint.h:$(GLSRC)gsmemory.h
$(GLSRC)gxpaint.h:$(GLSRC)gslibctx.h
$(GLSRC)gxpaint.h:$(GLSRC)gxcindex.h
$(GLSRC)gxpaint.h:$(GLSRC)stdio_.h
$(GLSRC)gxpaint.h:$(GLSRC)stdint_.h
$(GLSRC)gxpaint.h:$(GLSRC)gssprintf.h
$(GLSRC)gxpaint.h:$(GLSRC)gstypes.h
$(GLSRC)gxpaint.h:$(GLSRC)std.h
$(GLSRC)gxpaint.h:$(GLSRC)stdpre.h
$(GLSRC)gxpaint.h:$(GLGEN)arch.h
$(GLSRC)gxpaint.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxpath.h:$(GLSRC)gsrect.h
$(GLSRC)gxpath.h:$(GLSRC)gslparam.h
$(GLSRC)gxpath.h:$(GLSRC)gscpm.h
$(GLSRC)gxpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gxpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gxpath.h:$(GLSRC)gspenum.h
$(GLSRC)gxpath.h:$(GLSRC)gstypes.h
$(GLSRC)gxpath.h:$(GLSRC)std.h
$(GLSRC)gxpath.h:$(GLSRC)stdpre.h
$(GLSRC)gxpath.h:$(GLGEN)arch.h
$(GLSRC)gxpcache.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxpcache.h:$(GLSRC)gscms.h
$(GLSRC)gxpcache.h:$(GLSRC)gsdevice.h
$(GLSRC)gxpcache.h:$(GLSRC)gscspace.h
$(GLSRC)gxpcache.h:$(GLSRC)gsgstate.h
$(GLSRC)gxpcache.h:$(GLSRC)gsiparam.h
$(GLSRC)gxpcache.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxpcache.h:$(GLSRC)gxhttile.h
$(GLSRC)gxpcache.h:$(GLSRC)gsparam.h
$(GLSRC)gxpcache.h:$(GLSRC)gsrefct.h
$(GLSRC)gxpcache.h:$(GLSRC)memento.h
$(GLSRC)gxpcache.h:$(GLSRC)gxsync.h
$(GLSRC)gxpcache.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxpcache.h:$(GLSRC)scommon.h
$(GLSRC)gxpcache.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxpcache.h:$(GLSRC)gsccolor.h
$(GLSRC)gxpcache.h:$(GLSRC)gxarith.h
$(GLSRC)gxpcache.h:$(GLSRC)gpsync.h
$(GLSRC)gxpcache.h:$(GLSRC)gsstype.h
$(GLSRC)gxpcache.h:$(GLSRC)gsmemory.h
$(GLSRC)gxpcache.h:$(GLSRC)gslibctx.h
$(GLSRC)gxpcache.h:$(GLSRC)gxcindex.h
$(GLSRC)gxpcache.h:$(GLSRC)stdio_.h
$(GLSRC)gxpcache.h:$(GLSRC)stdint_.h
$(GLSRC)gxpcache.h:$(GLSRC)gssprintf.h
$(GLSRC)gxpcache.h:$(GLSRC)gstypes.h
$(GLSRC)gxpcache.h:$(GLSRC)std.h
$(GLSRC)gxpcache.h:$(GLSRC)stdpre.h
$(GLSRC)gxpcache.h:$(GLGEN)arch.h
$(GLSRC)gxpcache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxsample.h:$(GLSRC)std.h
$(GLSRC)gxsample.h:$(GLSRC)stdpre.h
$(GLSRC)gxsample.h:$(GLGEN)arch.h
$(GLSRC)gxsamplp.h:$(GLSRC)valgrind.h
$(GLSRC)gxsamplp.h:$(GLSRC)stdpre.h
$(GLSRC)gxscanc.h:$(GLSRC)gxdevice.h
$(GLSRC)gxscanc.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxscanc.h:$(GLSRC)gxcmap.h
$(GLSRC)gxscanc.h:$(GLSRC)gxtext.h
$(GLSRC)gxscanc.h:$(GLSRC)gstext.h
$(GLSRC)gxscanc.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxscanc.h:$(GLSRC)gstparam.h
$(GLSRC)gxscanc.h:$(GLSRC)gxfmap.h
$(GLSRC)gxscanc.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxscanc.h:$(GLSRC)gsfunc.h
$(GLSRC)gxscanc.h:$(GLSRC)gxcspace.h
$(GLSRC)gxscanc.h:$(GLSRC)gxrplane.h
$(GLSRC)gxscanc.h:$(GLSRC)gscsel.h
$(GLSRC)gxscanc.h:$(GLSRC)gxfcache.h
$(GLSRC)gxscanc.h:$(GLSRC)gsfont.h
$(GLSRC)gxscanc.h:$(GLSRC)gsimage.h
$(GLSRC)gxscanc.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxscanc.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxscanc.h:$(GLSRC)gxbcache.h
$(GLSRC)gxscanc.h:$(GLSRC)gsropt.h
$(GLSRC)gxscanc.h:$(GLSRC)gxdda.h
$(GLSRC)gxscanc.h:$(GLSRC)gxpath.h
$(GLSRC)gxscanc.h:$(GLSRC)gxfrac.h
$(GLSRC)gxscanc.h:$(GLSRC)gxtmap.h
$(GLSRC)gxscanc.h:$(GLSRC)gxftype.h
$(GLSRC)gxscanc.h:$(GLSRC)gscms.h
$(GLSRC)gxscanc.h:$(GLSRC)gsrect.h
$(GLSRC)gxscanc.h:$(GLSRC)gslparam.h
$(GLSRC)gxscanc.h:$(GLSRC)gsdevice.h
$(GLSRC)gxscanc.h:$(GLSRC)gscpm.h
$(GLSRC)gxscanc.h:$(GLSRC)gscspace.h
$(GLSRC)gxscanc.h:$(GLSRC)gsgstate.h
$(GLSRC)gxscanc.h:$(GLSRC)gxstdio.h
$(GLSRC)gxscanc.h:$(GLSRC)gsxfont.h
$(GLSRC)gxscanc.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxscanc.h:$(GLSRC)gsio.h
$(GLSRC)gxscanc.h:$(GLSRC)gsiparam.h
$(GLSRC)gxscanc.h:$(GLSRC)gxfixed.h
$(GLSRC)gxscanc.h:$(GLSRC)gscompt.h
$(GLSRC)gxscanc.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxscanc.h:$(GLSRC)gspenum.h
$(GLSRC)gxscanc.h:$(GLSRC)gxhttile.h
$(GLSRC)gxscanc.h:$(GLSRC)gsparam.h
$(GLSRC)gxscanc.h:$(GLSRC)gsrefct.h
$(GLSRC)gxscanc.h:$(GLSRC)gp.h
$(GLSRC)gxscanc.h:$(GLSRC)memento.h
$(GLSRC)gxscanc.h:$(GLSRC)memory_.h
$(GLSRC)gxscanc.h:$(GLSRC)gsuid.h
$(GLSRC)gxscanc.h:$(GLSRC)gsstruct.h
$(GLSRC)gxscanc.h:$(GLSRC)gxsync.h
$(GLSRC)gxscanc.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxscanc.h:$(GLSRC)srdline.h
$(GLSRC)gxscanc.h:$(GLSRC)scommon.h
$(GLSRC)gxscanc.h:$(GLSRC)gsfname.h
$(GLSRC)gxscanc.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxscanc.h:$(GLSRC)gsccolor.h
$(GLSRC)gxscanc.h:$(GLSRC)gxarith.h
$(GLSRC)gxscanc.h:$(GLSRC)stat_.h
$(GLSRC)gxscanc.h:$(GLSRC)gpsync.h
$(GLSRC)gxscanc.h:$(GLSRC)gsstype.h
$(GLSRC)gxscanc.h:$(GLSRC)gsmemory.h
$(GLSRC)gxscanc.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxscanc.h:$(GLSRC)gscdefs.h
$(GLSRC)gxscanc.h:$(GLSRC)gslibctx.h
$(GLSRC)gxscanc.h:$(GLSRC)gxcindex.h
$(GLSRC)gxscanc.h:$(GLSRC)stdio_.h
$(GLSRC)gxscanc.h:$(GLSRC)gsccode.h
$(GLSRC)gxscanc.h:$(GLSRC)stdint_.h
$(GLSRC)gxscanc.h:$(GLSRC)gssprintf.h
$(GLSRC)gxscanc.h:$(GLSRC)gstypes.h
$(GLSRC)gxscanc.h:$(GLSRC)std.h
$(GLSRC)gxscanc.h:$(GLSRC)stdpre.h
$(GLSRC)gxscanc.h:$(GLGEN)arch.h
$(GLSRC)gxscanc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxstate.h:$(GLSRC)gscspace.h
$(GLSRC)gxstate.h:$(GLSRC)gsgstate.h
$(GLSRC)gxstate.h:$(GLSRC)gsiparam.h
$(GLSRC)gxstate.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxstate.h:$(GLSRC)gsrefct.h
$(GLSRC)gxstate.h:$(GLSRC)memento.h
$(GLSRC)gxstate.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxstate.h:$(GLSRC)scommon.h
$(GLSRC)gxstate.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxstate.h:$(GLSRC)gsccolor.h
$(GLSRC)gxstate.h:$(GLSRC)gsstype.h
$(GLSRC)gxstate.h:$(GLSRC)gsmemory.h
$(GLSRC)gxstate.h:$(GLSRC)gslibctx.h
$(GLSRC)gxstate.h:$(GLSRC)stdio_.h
$(GLSRC)gxstate.h:$(GLSRC)stdint_.h
$(GLSRC)gxstate.h:$(GLSRC)gssprintf.h
$(GLSRC)gxstate.h:$(GLSRC)gstypes.h
$(GLSRC)gxstate.h:$(GLSRC)std.h
$(GLSRC)gxstate.h:$(GLSRC)stdpre.h
$(GLSRC)gxstate.h:$(GLGEN)arch.h
$(GLSRC)gxstate.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxtext.h:$(GLSRC)gstext.h
$(GLSRC)gxtext.h:$(GLSRC)gxfcache.h
$(GLSRC)gxtext.h:$(GLSRC)gsfont.h
$(GLSRC)gxtext.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxtext.h:$(GLSRC)gxbcache.h
$(GLSRC)gxtext.h:$(GLSRC)gxpath.h
$(GLSRC)gxtext.h:$(GLSRC)gxftype.h
$(GLSRC)gxtext.h:$(GLSRC)gscms.h
$(GLSRC)gxtext.h:$(GLSRC)gsrect.h
$(GLSRC)gxtext.h:$(GLSRC)gslparam.h
$(GLSRC)gxtext.h:$(GLSRC)gsdevice.h
$(GLSRC)gxtext.h:$(GLSRC)gscpm.h
$(GLSRC)gxtext.h:$(GLSRC)gscspace.h
$(GLSRC)gxtext.h:$(GLSRC)gsgstate.h
$(GLSRC)gxtext.h:$(GLSRC)gsxfont.h
$(GLSRC)gxtext.h:$(GLSRC)gsiparam.h
$(GLSRC)gxtext.h:$(GLSRC)gxfixed.h
$(GLSRC)gxtext.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxtext.h:$(GLSRC)gspenum.h
$(GLSRC)gxtext.h:$(GLSRC)gxhttile.h
$(GLSRC)gxtext.h:$(GLSRC)gsparam.h
$(GLSRC)gxtext.h:$(GLSRC)gsrefct.h
$(GLSRC)gxtext.h:$(GLSRC)memento.h
$(GLSRC)gxtext.h:$(GLSRC)gsuid.h
$(GLSRC)gxtext.h:$(GLSRC)gxsync.h
$(GLSRC)gxtext.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxtext.h:$(GLSRC)scommon.h
$(GLSRC)gxtext.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxtext.h:$(GLSRC)gsccolor.h
$(GLSRC)gxtext.h:$(GLSRC)gxarith.h
$(GLSRC)gxtext.h:$(GLSRC)gpsync.h
$(GLSRC)gxtext.h:$(GLSRC)gsstype.h
$(GLSRC)gxtext.h:$(GLSRC)gsmemory.h
$(GLSRC)gxtext.h:$(GLSRC)gslibctx.h
$(GLSRC)gxtext.h:$(GLSRC)gxcindex.h
$(GLSRC)gxtext.h:$(GLSRC)stdio_.h
$(GLSRC)gxtext.h:$(GLSRC)gsccode.h
$(GLSRC)gxtext.h:$(GLSRC)stdint_.h
$(GLSRC)gxtext.h:$(GLSRC)gssprintf.h
$(GLSRC)gxtext.h:$(GLSRC)gstypes.h
$(GLSRC)gxtext.h:$(GLSRC)std.h
$(GLSRC)gxtext.h:$(GLSRC)stdpre.h
$(GLSRC)gxtext.h:$(GLGEN)arch.h
$(GLSRC)gxtext.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxxfont.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxxfont.h:$(GLSRC)gxcmap.h
$(GLSRC)gxxfont.h:$(GLSRC)gxtext.h
$(GLSRC)gxxfont.h:$(GLSRC)gstext.h
$(GLSRC)gxxfont.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxxfont.h:$(GLSRC)gstparam.h
$(GLSRC)gxxfont.h:$(GLSRC)gxfmap.h
$(GLSRC)gxxfont.h:$(GLSRC)gsfunc.h
$(GLSRC)gxxfont.h:$(GLSRC)gxcspace.h
$(GLSRC)gxxfont.h:$(GLSRC)gxrplane.h
$(GLSRC)gxxfont.h:$(GLSRC)gscsel.h
$(GLSRC)gxxfont.h:$(GLSRC)gxfcache.h
$(GLSRC)gxxfont.h:$(GLSRC)gsfont.h
$(GLSRC)gxxfont.h:$(GLSRC)gsimage.h
$(GLSRC)gxxfont.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxxfont.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxxfont.h:$(GLSRC)gxbcache.h
$(GLSRC)gxxfont.h:$(GLSRC)gsropt.h
$(GLSRC)gxxfont.h:$(GLSRC)gxdda.h
$(GLSRC)gxxfont.h:$(GLSRC)gxpath.h
$(GLSRC)gxxfont.h:$(GLSRC)gxfrac.h
$(GLSRC)gxxfont.h:$(GLSRC)gxtmap.h
$(GLSRC)gxxfont.h:$(GLSRC)gxftype.h
$(GLSRC)gxxfont.h:$(GLSRC)gscms.h
$(GLSRC)gxxfont.h:$(GLSRC)gsrect.h
$(GLSRC)gxxfont.h:$(GLSRC)gslparam.h
$(GLSRC)gxxfont.h:$(GLSRC)gsdevice.h
$(GLSRC)gxxfont.h:$(GLSRC)gscpm.h
$(GLSRC)gxxfont.h:$(GLSRC)gscspace.h
$(GLSRC)gxxfont.h:$(GLSRC)gsgstate.h
$(GLSRC)gxxfont.h:$(GLSRC)gsxfont.h
$(GLSRC)gxxfont.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxxfont.h:$(GLSRC)gsiparam.h
$(GLSRC)gxxfont.h:$(GLSRC)gxfixed.h
$(GLSRC)gxxfont.h:$(GLSRC)gscompt.h
$(GLSRC)gxxfont.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxxfont.h:$(GLSRC)gspenum.h
$(GLSRC)gxxfont.h:$(GLSRC)gxhttile.h
$(GLSRC)gxxfont.h:$(GLSRC)gsparam.h
$(GLSRC)gxxfont.h:$(GLSRC)gsrefct.h
$(GLSRC)gxxfont.h:$(GLSRC)gp.h
$(GLSRC)gxxfont.h:$(GLSRC)memento.h
$(GLSRC)gxxfont.h:$(GLSRC)memory_.h
$(GLSRC)gxxfont.h:$(GLSRC)gsuid.h
$(GLSRC)gxxfont.h:$(GLSRC)gsstruct.h
$(GLSRC)gxxfont.h:$(GLSRC)gxsync.h
$(GLSRC)gxxfont.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxxfont.h:$(GLSRC)srdline.h
$(GLSRC)gxxfont.h:$(GLSRC)scommon.h
$(GLSRC)gxxfont.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxxfont.h:$(GLSRC)gsccolor.h
$(GLSRC)gxxfont.h:$(GLSRC)gxarith.h
$(GLSRC)gxxfont.h:$(GLSRC)stat_.h
$(GLSRC)gxxfont.h:$(GLSRC)gpsync.h
$(GLSRC)gxxfont.h:$(GLSRC)gsstype.h
$(GLSRC)gxxfont.h:$(GLSRC)gsmemory.h
$(GLSRC)gxxfont.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxxfont.h:$(GLSRC)gscdefs.h
$(GLSRC)gxxfont.h:$(GLSRC)gslibctx.h
$(GLSRC)gxxfont.h:$(GLSRC)gxcindex.h
$(GLSRC)gxxfont.h:$(GLSRC)stdio_.h
$(GLSRC)gxxfont.h:$(GLSRC)gsccode.h
$(GLSRC)gxxfont.h:$(GLSRC)stdint_.h
$(GLSRC)gxxfont.h:$(GLSRC)gssprintf.h
$(GLSRC)gxxfont.h:$(GLSRC)gstypes.h
$(GLSRC)gxxfont.h:$(GLSRC)std.h
$(GLSRC)gxxfont.h:$(GLSRC)stdpre.h
$(GLSRC)gxxfont.h:$(GLGEN)arch.h
$(GLSRC)gxxfont.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxband.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxband.h:$(GLSRC)gxcmap.h
$(GLSRC)gxband.h:$(GLSRC)gxtext.h
$(GLSRC)gxband.h:$(GLSRC)gstext.h
$(GLSRC)gxband.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxband.h:$(GLSRC)gstparam.h
$(GLSRC)gxband.h:$(GLSRC)gxfmap.h
$(GLSRC)gxband.h:$(GLSRC)gsfunc.h
$(GLSRC)gxband.h:$(GLSRC)gxcspace.h
$(GLSRC)gxband.h:$(GLSRC)gxrplane.h
$(GLSRC)gxband.h:$(GLSRC)gscsel.h
$(GLSRC)gxband.h:$(GLSRC)gxfcache.h
$(GLSRC)gxband.h:$(GLSRC)gsfont.h
$(GLSRC)gxband.h:$(GLSRC)gsimage.h
$(GLSRC)gxband.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxband.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxband.h:$(GLSRC)gxbcache.h
$(GLSRC)gxband.h:$(GLSRC)gsropt.h
$(GLSRC)gxband.h:$(GLSRC)gxdda.h
$(GLSRC)gxband.h:$(GLSRC)gxpath.h
$(GLSRC)gxband.h:$(GLSRC)gxfrac.h
$(GLSRC)gxband.h:$(GLSRC)gxtmap.h
$(GLSRC)gxband.h:$(GLSRC)gxftype.h
$(GLSRC)gxband.h:$(GLSRC)gscms.h
$(GLSRC)gxband.h:$(GLSRC)gsrect.h
$(GLSRC)gxband.h:$(GLSRC)gslparam.h
$(GLSRC)gxband.h:$(GLSRC)gsdevice.h
$(GLSRC)gxband.h:$(GLSRC)gscpm.h
$(GLSRC)gxband.h:$(GLSRC)gscspace.h
$(GLSRC)gxband.h:$(GLSRC)gsgstate.h
$(GLSRC)gxband.h:$(GLSRC)gsxfont.h
$(GLSRC)gxband.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxband.h:$(GLSRC)gsiparam.h
$(GLSRC)gxband.h:$(GLSRC)gxfixed.h
$(GLSRC)gxband.h:$(GLSRC)gxclio.h
$(GLSRC)gxband.h:$(GLSRC)gscompt.h
$(GLSRC)gxband.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxband.h:$(GLSRC)gspenum.h
$(GLSRC)gxband.h:$(GLSRC)gxhttile.h
$(GLSRC)gxband.h:$(GLSRC)gsparam.h
$(GLSRC)gxband.h:$(GLSRC)gsrefct.h
$(GLSRC)gxband.h:$(GLSRC)gp.h
$(GLSRC)gxband.h:$(GLSRC)memento.h
$(GLSRC)gxband.h:$(GLSRC)memory_.h
$(GLSRC)gxband.h:$(GLSRC)gsuid.h
$(GLSRC)gxband.h:$(GLSRC)gsstruct.h
$(GLSRC)gxband.h:$(GLSRC)gxsync.h
$(GLSRC)gxband.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxband.h:$(GLSRC)srdline.h
$(GLSRC)gxband.h:$(GLSRC)scommon.h
$(GLSRC)gxband.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxband.h:$(GLSRC)gsccolor.h
$(GLSRC)gxband.h:$(GLSRC)gxarith.h
$(GLSRC)gxband.h:$(GLSRC)stat_.h
$(GLSRC)gxband.h:$(GLSRC)gpsync.h
$(GLSRC)gxband.h:$(GLSRC)gsstype.h
$(GLSRC)gxband.h:$(GLSRC)gsmemory.h
$(GLSRC)gxband.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxband.h:$(GLSRC)gscdefs.h
$(GLSRC)gxband.h:$(GLSRC)gslibctx.h
$(GLSRC)gxband.h:$(GLSRC)gxcindex.h
$(GLSRC)gxband.h:$(GLSRC)stdio_.h
$(GLSRC)gxband.h:$(GLSRC)gsccode.h
$(GLSRC)gxband.h:$(GLSRC)stdint_.h
$(GLSRC)gxband.h:$(GLSRC)gssprintf.h
$(GLSRC)gxband.h:$(GLSRC)gstypes.h
$(GLSRC)gxband.h:$(GLSRC)std.h
$(GLSRC)gxband.h:$(GLSRC)stdpre.h
$(GLSRC)gxband.h:$(GLGEN)arch.h
$(GLSRC)gxband.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcdevn.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcdevn.h:$(GLSRC)gscspace.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcdevn.h:$(GLSRC)memento.h
$(GLSRC)gxcdevn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcdevn.h:$(GLSRC)scommon.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsstype.h
$(GLSRC)gxcdevn.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcdevn.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcdevn.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcdevn.h:$(GLSRC)stdio_.h
$(GLSRC)gxcdevn.h:$(GLSRC)stdint_.h
$(GLSRC)gxcdevn.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcdevn.h:$(GLSRC)gstypes.h
$(GLSRC)gxcdevn.h:$(GLSRC)std.h
$(GLSRC)gxcdevn.h:$(GLSRC)stdpre.h
$(GLSRC)gxcdevn.h:$(GLGEN)arch.h
$(GLSRC)gxcdevn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxchar.h:$(GLSRC)gschar.h
$(GLSRC)gxchar.h:$(GLSRC)gsstate.h
$(GLSRC)gxchar.h:$(GLSRC)gsovrc.h
$(GLSRC)gxchar.h:$(GLSRC)gscolor.h
$(GLSRC)gxchar.h:$(GLSRC)gsline.h
$(GLSRC)gxchar.h:$(GLSRC)gxcomp.h
$(GLSRC)gxchar.h:$(GLSRC)gsht.h
$(GLSRC)gxchar.h:$(GLSRC)gxtext.h
$(GLSRC)gxchar.h:$(GLSRC)gstext.h
$(GLSRC)gxchar.h:$(GLSRC)gscsel.h
$(GLSRC)gxchar.h:$(GLSRC)gxfcache.h
$(GLSRC)gxchar.h:$(GLSRC)gsfont.h
$(GLSRC)gxchar.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxchar.h:$(GLSRC)gxbcache.h
$(GLSRC)gxchar.h:$(GLSRC)gxpath.h
$(GLSRC)gxchar.h:$(GLSRC)gxtmap.h
$(GLSRC)gxchar.h:$(GLSRC)gxftype.h
$(GLSRC)gxchar.h:$(GLSRC)gscms.h
$(GLSRC)gxchar.h:$(GLSRC)gsrect.h
$(GLSRC)gxchar.h:$(GLSRC)gslparam.h
$(GLSRC)gxchar.h:$(GLSRC)gsdevice.h
$(GLSRC)gxchar.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxchar.h:$(GLSRC)gscpm.h
$(GLSRC)gxchar.h:$(GLSRC)gscspace.h
$(GLSRC)gxchar.h:$(GLSRC)gsgstate.h
$(GLSRC)gxchar.h:$(GLSRC)gsxfont.h
$(GLSRC)gxchar.h:$(GLSRC)gsiparam.h
$(GLSRC)gxchar.h:$(GLSRC)gxfixed.h
$(GLSRC)gxchar.h:$(GLSRC)gscompt.h
$(GLSRC)gxchar.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxchar.h:$(GLSRC)gspenum.h
$(GLSRC)gxchar.h:$(GLSRC)gxhttile.h
$(GLSRC)gxchar.h:$(GLSRC)gsparam.h
$(GLSRC)gxchar.h:$(GLSRC)gsrefct.h
$(GLSRC)gxchar.h:$(GLSRC)memento.h
$(GLSRC)gxchar.h:$(GLSRC)gsuid.h
$(GLSRC)gxchar.h:$(GLSRC)gxsync.h
$(GLSRC)gxchar.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxchar.h:$(GLSRC)scommon.h
$(GLSRC)gxchar.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxchar.h:$(GLSRC)gsccolor.h
$(GLSRC)gxchar.h:$(GLSRC)gxarith.h
$(GLSRC)gxchar.h:$(GLSRC)gpsync.h
$(GLSRC)gxchar.h:$(GLSRC)gsstype.h
$(GLSRC)gxchar.h:$(GLSRC)gsmemory.h
$(GLSRC)gxchar.h:$(GLSRC)gslibctx.h
$(GLSRC)gxchar.h:$(GLSRC)gxcindex.h
$(GLSRC)gxchar.h:$(GLSRC)stdio_.h
$(GLSRC)gxchar.h:$(GLSRC)gsccode.h
$(GLSRC)gxchar.h:$(GLSRC)stdint_.h
$(GLSRC)gxchar.h:$(GLSRC)gssprintf.h
$(GLSRC)gxchar.h:$(GLSRC)gstypes.h
$(GLSRC)gxchar.h:$(GLSRC)std.h
$(GLSRC)gxchar.h:$(GLSRC)stdpre.h
$(GLSRC)gxchar.h:$(GLGEN)arch.h
$(GLSRC)gxchar.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxchrout.h:$(GLSRC)gsgstate.h
$(GLSRC)gsdcolor.h:$(GLSRC)gscms.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsdevice.h
$(GLSRC)gsdcolor.h:$(GLSRC)gscspace.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsgstate.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsiparam.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsdcolor.h:$(GLSRC)gxhttile.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsparam.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsrefct.h
$(GLSRC)gsdcolor.h:$(GLSRC)memento.h
$(GLSRC)gsdcolor.h:$(GLSRC)gxsync.h
$(GLSRC)gsdcolor.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsdcolor.h:$(GLSRC)scommon.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsccolor.h
$(GLSRC)gsdcolor.h:$(GLSRC)gxarith.h
$(GLSRC)gsdcolor.h:$(GLSRC)gpsync.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsstype.h
$(GLSRC)gsdcolor.h:$(GLSRC)gsmemory.h
$(GLSRC)gsdcolor.h:$(GLSRC)gslibctx.h
$(GLSRC)gsdcolor.h:$(GLSRC)gxcindex.h
$(GLSRC)gsdcolor.h:$(GLSRC)stdio_.h
$(GLSRC)gsdcolor.h:$(GLSRC)stdint_.h
$(GLSRC)gsdcolor.h:$(GLSRC)gssprintf.h
$(GLSRC)gsdcolor.h:$(GLSRC)gstypes.h
$(GLSRC)gsdcolor.h:$(GLSRC)std.h
$(GLSRC)gsdcolor.h:$(GLSRC)stdpre.h
$(GLSRC)gsdcolor.h:$(GLGEN)arch.h
$(GLSRC)gsdcolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdcolor.h:$(GLSRC)gscsel.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsropt.h
$(GLSRC)gxdcolor.h:$(GLSRC)gscms.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdcolor.h:$(GLSRC)gscspace.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdcolor.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsparam.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdcolor.h:$(GLSRC)memento.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdcolor.h:$(GLSRC)gxsync.h
$(GLSRC)gxdcolor.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdcolor.h:$(GLSRC)scommon.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdcolor.h:$(GLSRC)gxarith.h
$(GLSRC)gxdcolor.h:$(GLSRC)gpsync.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsstype.h
$(GLSRC)gxdcolor.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdcolor.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdcolor.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdcolor.h:$(GLSRC)stdio_.h
$(GLSRC)gxdcolor.h:$(GLSRC)stdint_.h
$(GLSRC)gxdcolor.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdcolor.h:$(GLSRC)gstypes.h
$(GLSRC)gxdcolor.h:$(GLSRC)std.h
$(GLSRC)gxdcolor.h:$(GLSRC)stdpre.h
$(GLSRC)gxdcolor.h:$(GLGEN)arch.h
$(GLSRC)gxdcolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxcspace.h
$(GLSRC)gsnamecl.h:$(GLSRC)gscsel.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsdcolor.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxfrac.h
$(GLSRC)gsnamecl.h:$(GLSRC)gscms.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsdevice.h
$(GLSRC)gsnamecl.h:$(GLSRC)gscspace.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsgstate.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsiparam.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxhttile.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsparam.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsrefct.h
$(GLSRC)gsnamecl.h:$(GLSRC)memento.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxsync.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsnamecl.h:$(GLSRC)scommon.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsccolor.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxarith.h
$(GLSRC)gsnamecl.h:$(GLSRC)gpsync.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsstype.h
$(GLSRC)gsnamecl.h:$(GLSRC)gsmemory.h
$(GLSRC)gsnamecl.h:$(GLSRC)gslibctx.h
$(GLSRC)gsnamecl.h:$(GLSRC)gxcindex.h
$(GLSRC)gsnamecl.h:$(GLSRC)stdio_.h
$(GLSRC)gsnamecl.h:$(GLSRC)stdint_.h
$(GLSRC)gsnamecl.h:$(GLSRC)gssprintf.h
$(GLSRC)gsnamecl.h:$(GLSRC)gstypes.h
$(GLSRC)gsnamecl.h:$(GLSRC)std.h
$(GLSRC)gsnamecl.h:$(GLSRC)stdpre.h
$(GLSRC)gsnamecl.h:$(GLGEN)arch.h
$(GLSRC)gsnamecl.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscspace.h:$(GLSRC)gsgstate.h
$(GLSRC)gscspace.h:$(GLSRC)gsiparam.h
$(GLSRC)gscspace.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscspace.h:$(GLSRC)gsrefct.h
$(GLSRC)gscspace.h:$(GLSRC)memento.h
$(GLSRC)gscspace.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscspace.h:$(GLSRC)scommon.h
$(GLSRC)gscspace.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscspace.h:$(GLSRC)gsccolor.h
$(GLSRC)gscspace.h:$(GLSRC)gsstype.h
$(GLSRC)gscspace.h:$(GLSRC)gsmemory.h
$(GLSRC)gscspace.h:$(GLSRC)gslibctx.h
$(GLSRC)gscspace.h:$(GLSRC)stdio_.h
$(GLSRC)gscspace.h:$(GLSRC)stdint_.h
$(GLSRC)gscspace.h:$(GLSRC)gssprintf.h
$(GLSRC)gscspace.h:$(GLSRC)gstypes.h
$(GLSRC)gscspace.h:$(GLSRC)std.h
$(GLSRC)gscspace.h:$(GLSRC)stdpre.h
$(GLSRC)gscspace.h:$(GLGEN)arch.h
$(GLSRC)gscspace.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscssub.h:$(GLSRC)gscspace.h
$(GLSRC)gscssub.h:$(GLSRC)gsgstate.h
$(GLSRC)gscssub.h:$(GLSRC)gsiparam.h
$(GLSRC)gscssub.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscssub.h:$(GLSRC)gsrefct.h
$(GLSRC)gscssub.h:$(GLSRC)memento.h
$(GLSRC)gscssub.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscssub.h:$(GLSRC)scommon.h
$(GLSRC)gscssub.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscssub.h:$(GLSRC)gsccolor.h
$(GLSRC)gscssub.h:$(GLSRC)gsstype.h
$(GLSRC)gscssub.h:$(GLSRC)gsmemory.h
$(GLSRC)gscssub.h:$(GLSRC)gslibctx.h
$(GLSRC)gscssub.h:$(GLSRC)stdio_.h
$(GLSRC)gscssub.h:$(GLSRC)stdint_.h
$(GLSRC)gscssub.h:$(GLSRC)gssprintf.h
$(GLSRC)gscssub.h:$(GLSRC)gstypes.h
$(GLSRC)gscssub.h:$(GLSRC)std.h
$(GLSRC)gscssub.h:$(GLSRC)stdpre.h
$(GLSRC)gscssub.h:$(GLGEN)arch.h
$(GLSRC)gscssub.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxtext.h
$(GLSRC)gxdevcli.h:$(GLSRC)gstext.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdevcli.h:$(GLSRC)gstparam.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscsel.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsfont.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsimage.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsropt.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxdda.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxpath.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxftype.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscms.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsrect.h
$(GLSRC)gxdevcli.h:$(GLSRC)gslparam.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscpm.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscspace.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscompt.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdevcli.h:$(GLSRC)gspenum.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsparam.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdevcli.h:$(GLSRC)gp.h
$(GLSRC)gxdevcli.h:$(GLSRC)memento.h
$(GLSRC)gxdevcli.h:$(GLSRC)memory_.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsuid.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxsync.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdevcli.h:$(GLSRC)srdline.h
$(GLSRC)gxdevcli.h:$(GLSRC)scommon.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxarith.h
$(GLSRC)gxdevcli.h:$(GLSRC)stat_.h
$(GLSRC)gxdevcli.h:$(GLSRC)gpsync.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsstype.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdevcli.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdevcli.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdevcli.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdevcli.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdevcli.h:$(GLSRC)stdio_.h
$(GLSRC)gxdevcli.h:$(GLSRC)gsccode.h
$(GLSRC)gxdevcli.h:$(GLSRC)stdint_.h
$(GLSRC)gxdevcli.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdevcli.h:$(GLSRC)gstypes.h
$(GLSRC)gxdevcli.h:$(GLSRC)std.h
$(GLSRC)gxdevcli.h:$(GLSRC)stdpre.h
$(GLSRC)gxdevcli.h:$(GLGEN)arch.h
$(GLSRC)gxdevcli.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscicach.h:$(GLSRC)gxdevcli.h
$(GLSRC)gscicach.h:$(GLSRC)gxcmap.h
$(GLSRC)gscicach.h:$(GLSRC)gxtext.h
$(GLSRC)gscicach.h:$(GLSRC)gstext.h
$(GLSRC)gscicach.h:$(GLSRC)gsnamecl.h
$(GLSRC)gscicach.h:$(GLSRC)gstparam.h
$(GLSRC)gscicach.h:$(GLSRC)gxfmap.h
$(GLSRC)gscicach.h:$(GLSRC)gsfunc.h
$(GLSRC)gscicach.h:$(GLSRC)gxcspace.h
$(GLSRC)gscicach.h:$(GLSRC)gxrplane.h
$(GLSRC)gscicach.h:$(GLSRC)gscsel.h
$(GLSRC)gscicach.h:$(GLSRC)gxfcache.h
$(GLSRC)gscicach.h:$(GLSRC)gsfont.h
$(GLSRC)gscicach.h:$(GLSRC)gsimage.h
$(GLSRC)gscicach.h:$(GLSRC)gsdcolor.h
$(GLSRC)gscicach.h:$(GLSRC)gxcvalue.h
$(GLSRC)gscicach.h:$(GLSRC)gxbcache.h
$(GLSRC)gscicach.h:$(GLSRC)gsropt.h
$(GLSRC)gscicach.h:$(GLSRC)gxdda.h
$(GLSRC)gscicach.h:$(GLSRC)gxpath.h
$(GLSRC)gscicach.h:$(GLSRC)gxfrac.h
$(GLSRC)gscicach.h:$(GLSRC)gxtmap.h
$(GLSRC)gscicach.h:$(GLSRC)gxftype.h
$(GLSRC)gscicach.h:$(GLSRC)gscms.h
$(GLSRC)gscicach.h:$(GLSRC)gsrect.h
$(GLSRC)gscicach.h:$(GLSRC)gslparam.h
$(GLSRC)gscicach.h:$(GLSRC)gsdevice.h
$(GLSRC)gscicach.h:$(GLSRC)gscpm.h
$(GLSRC)gscicach.h:$(GLSRC)gscspace.h
$(GLSRC)gscicach.h:$(GLSRC)gsgstate.h
$(GLSRC)gscicach.h:$(GLSRC)gsxfont.h
$(GLSRC)gscicach.h:$(GLSRC)gsdsrc.h
$(GLSRC)gscicach.h:$(GLSRC)gsiparam.h
$(GLSRC)gscicach.h:$(GLSRC)gxfixed.h
$(GLSRC)gscicach.h:$(GLSRC)gscompt.h
$(GLSRC)gscicach.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscicach.h:$(GLSRC)gspenum.h
$(GLSRC)gscicach.h:$(GLSRC)gxhttile.h
$(GLSRC)gscicach.h:$(GLSRC)gsparam.h
$(GLSRC)gscicach.h:$(GLSRC)gsrefct.h
$(GLSRC)gscicach.h:$(GLSRC)gp.h
$(GLSRC)gscicach.h:$(GLSRC)memento.h
$(GLSRC)gscicach.h:$(GLSRC)memory_.h
$(GLSRC)gscicach.h:$(GLSRC)gsuid.h
$(GLSRC)gscicach.h:$(GLSRC)gsstruct.h
$(GLSRC)gscicach.h:$(GLSRC)gxsync.h
$(GLSRC)gscicach.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscicach.h:$(GLSRC)srdline.h
$(GLSRC)gscicach.h:$(GLSRC)scommon.h
$(GLSRC)gscicach.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscicach.h:$(GLSRC)gsccolor.h
$(GLSRC)gscicach.h:$(GLSRC)gxarith.h
$(GLSRC)gscicach.h:$(GLSRC)stat_.h
$(GLSRC)gscicach.h:$(GLSRC)gpsync.h
$(GLSRC)gscicach.h:$(GLSRC)gsstype.h
$(GLSRC)gscicach.h:$(GLSRC)gsmemory.h
$(GLSRC)gscicach.h:$(GLSRC)gpgetenv.h
$(GLSRC)gscicach.h:$(GLSRC)gscdefs.h
$(GLSRC)gscicach.h:$(GLSRC)gslibctx.h
$(GLSRC)gscicach.h:$(GLSRC)gxcindex.h
$(GLSRC)gscicach.h:$(GLSRC)stdio_.h
$(GLSRC)gscicach.h:$(GLSRC)gsccode.h
$(GLSRC)gscicach.h:$(GLSRC)stdint_.h
$(GLSRC)gscicach.h:$(GLSRC)gssprintf.h
$(GLSRC)gscicach.h:$(GLSRC)gstypes.h
$(GLSRC)gscicach.h:$(GLSRC)std.h
$(GLSRC)gscicach.h:$(GLSRC)stdpre.h
$(GLSRC)gscicach.h:$(GLGEN)arch.h
$(GLSRC)gscicach.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdevice.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxdevice.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdevice.h:$(GLSRC)gxtext.h
$(GLSRC)gxdevice.h:$(GLSRC)gstext.h
$(GLSRC)gxdevice.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdevice.h:$(GLSRC)gstparam.h
$(GLSRC)gxdevice.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdevice.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxdevice.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdevice.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdevice.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdevice.h:$(GLSRC)gscsel.h
$(GLSRC)gxdevice.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdevice.h:$(GLSRC)gsfont.h
$(GLSRC)gxdevice.h:$(GLSRC)gsimage.h
$(GLSRC)gxdevice.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdevice.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdevice.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdevice.h:$(GLSRC)gsropt.h
$(GLSRC)gxdevice.h:$(GLSRC)gxdda.h
$(GLSRC)gxdevice.h:$(GLSRC)gxpath.h
$(GLSRC)gxdevice.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdevice.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdevice.h:$(GLSRC)gxftype.h
$(GLSRC)gxdevice.h:$(GLSRC)gscms.h
$(GLSRC)gxdevice.h:$(GLSRC)gsrect.h
$(GLSRC)gxdevice.h:$(GLSRC)gslparam.h
$(GLSRC)gxdevice.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdevice.h:$(GLSRC)gscpm.h
$(GLSRC)gxdevice.h:$(GLSRC)gscspace.h
$(GLSRC)gxdevice.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdevice.h:$(GLSRC)gxstdio.h
$(GLSRC)gxdevice.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdevice.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdevice.h:$(GLSRC)gsio.h
$(GLSRC)gxdevice.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdevice.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdevice.h:$(GLSRC)gscompt.h
$(GLSRC)gxdevice.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdevice.h:$(GLSRC)gspenum.h
$(GLSRC)gxdevice.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdevice.h:$(GLSRC)gsparam.h
$(GLSRC)gxdevice.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdevice.h:$(GLSRC)gp.h
$(GLSRC)gxdevice.h:$(GLSRC)memento.h
$(GLSRC)gxdevice.h:$(GLSRC)memory_.h
$(GLSRC)gxdevice.h:$(GLSRC)gsuid.h
$(GLSRC)gxdevice.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdevice.h:$(GLSRC)gxsync.h
$(GLSRC)gxdevice.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdevice.h:$(GLSRC)srdline.h
$(GLSRC)gxdevice.h:$(GLSRC)scommon.h
$(GLSRC)gxdevice.h:$(GLSRC)gsfname.h
$(GLSRC)gxdevice.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdevice.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdevice.h:$(GLSRC)gxarith.h
$(GLSRC)gxdevice.h:$(GLSRC)stat_.h
$(GLSRC)gxdevice.h:$(GLSRC)gpsync.h
$(GLSRC)gxdevice.h:$(GLSRC)gsstype.h
$(GLSRC)gxdevice.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdevice.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdevice.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdevice.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdevice.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdevice.h:$(GLSRC)stdio_.h
$(GLSRC)gxdevice.h:$(GLSRC)gsccode.h
$(GLSRC)gxdevice.h:$(GLSRC)stdint_.h
$(GLSRC)gxdevice.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdevice.h:$(GLSRC)gstypes.h
$(GLSRC)gxdevice.h:$(GLSRC)std.h
$(GLSRC)gxdevice.h:$(GLSRC)stdpre.h
$(GLSRC)gxdevice.h:$(GLGEN)arch.h
$(GLSRC)gxdevice.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdht.h:$(GLSRC)gxht.h
$(GLSRC)gxdht.h:$(GLSRC)gxhttype.h
$(GLSRC)gxdht.h:$(GLSRC)gsht1.h
$(GLSRC)gxdht.h:$(GLSRC)gsht.h
$(GLSRC)gxdht.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdht.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdht.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdht.h:$(GLSRC)gscms.h
$(GLSRC)gxdht.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdht.h:$(GLSRC)gscspace.h
$(GLSRC)gxdht.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdht.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdht.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdht.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdht.h:$(GLSRC)gsparam.h
$(GLSRC)gxdht.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdht.h:$(GLSRC)memento.h
$(GLSRC)gxdht.h:$(GLSRC)gxsync.h
$(GLSRC)gxdht.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdht.h:$(GLSRC)scommon.h
$(GLSRC)gxdht.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdht.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdht.h:$(GLSRC)gxarith.h
$(GLSRC)gxdht.h:$(GLSRC)gpsync.h
$(GLSRC)gxdht.h:$(GLSRC)gsstype.h
$(GLSRC)gxdht.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdht.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdht.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdht.h:$(GLSRC)stdio_.h
$(GLSRC)gxdht.h:$(GLSRC)stdint_.h
$(GLSRC)gxdht.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdht.h:$(GLSRC)gstypes.h
$(GLSRC)gxdht.h:$(GLSRC)std.h
$(GLSRC)gxdht.h:$(GLSRC)stdpre.h
$(GLSRC)gxdht.h:$(GLGEN)arch.h
$(GLSRC)gxdht.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gscms.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gscspace.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsparam.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdhtserial.h:$(GLSRC)memento.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gxsync.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdhtserial.h:$(GLSRC)scommon.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gxarith.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gpsync.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsstype.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdhtserial.h:$(GLSRC)stdio_.h
$(GLSRC)gxdhtserial.h:$(GLSRC)stdint_.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gstypes.h
$(GLSRC)gxdhtserial.h:$(GLSRC)std.h
$(GLSRC)gxdhtserial.h:$(GLSRC)stdpre.h
$(GLSRC)gxdhtserial.h:$(GLGEN)arch.h
$(GLSRC)gxdhtserial.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdither.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdither.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdither.h:$(GLSRC)gscms.h
$(GLSRC)gxdither.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdither.h:$(GLSRC)gscspace.h
$(GLSRC)gxdither.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdither.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdither.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdither.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdither.h:$(GLSRC)gsparam.h
$(GLSRC)gxdither.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdither.h:$(GLSRC)memento.h
$(GLSRC)gxdither.h:$(GLSRC)gxsync.h
$(GLSRC)gxdither.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdither.h:$(GLSRC)scommon.h
$(GLSRC)gxdither.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdither.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdither.h:$(GLSRC)gxarith.h
$(GLSRC)gxdither.h:$(GLSRC)gpsync.h
$(GLSRC)gxdither.h:$(GLSRC)gsstype.h
$(GLSRC)gxdither.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdither.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdither.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdither.h:$(GLSRC)stdio_.h
$(GLSRC)gxdither.h:$(GLSRC)stdint_.h
$(GLSRC)gxdither.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdither.h:$(GLSRC)gstypes.h
$(GLSRC)gxdither.h:$(GLSRC)std.h
$(GLSRC)gxdither.h:$(GLSRC)stdpre.h
$(GLSRC)gxdither.h:$(GLGEN)arch.h
$(GLSRC)gxdither.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclip2.h:$(GLSRC)gxmclip.h
$(GLSRC)gxclip2.h:$(GLSRC)gxclip.h
$(GLSRC)gxclip2.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclip2.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclip2.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclip2.h:$(GLSRC)gxtext.h
$(GLSRC)gxclip2.h:$(GLSRC)gstext.h
$(GLSRC)gxclip2.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclip2.h:$(GLSRC)gstparam.h
$(GLSRC)gxclip2.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclip2.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclip2.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclip2.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclip2.h:$(GLSRC)gscsel.h
$(GLSRC)gxclip2.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclip2.h:$(GLSRC)gsfont.h
$(GLSRC)gxclip2.h:$(GLSRC)gsimage.h
$(GLSRC)gxclip2.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclip2.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclip2.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclip2.h:$(GLSRC)gsropt.h
$(GLSRC)gxclip2.h:$(GLSRC)gxdda.h
$(GLSRC)gxclip2.h:$(GLSRC)gxpath.h
$(GLSRC)gxclip2.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclip2.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclip2.h:$(GLSRC)gxftype.h
$(GLSRC)gxclip2.h:$(GLSRC)gscms.h
$(GLSRC)gxclip2.h:$(GLSRC)gsrect.h
$(GLSRC)gxclip2.h:$(GLSRC)gslparam.h
$(GLSRC)gxclip2.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclip2.h:$(GLSRC)gscpm.h
$(GLSRC)gxclip2.h:$(GLSRC)gscspace.h
$(GLSRC)gxclip2.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclip2.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclip2.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclip2.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclip2.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclip2.h:$(GLSRC)gscompt.h
$(GLSRC)gxclip2.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclip2.h:$(GLSRC)gspenum.h
$(GLSRC)gxclip2.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclip2.h:$(GLSRC)gsparam.h
$(GLSRC)gxclip2.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclip2.h:$(GLSRC)gp.h
$(GLSRC)gxclip2.h:$(GLSRC)memento.h
$(GLSRC)gxclip2.h:$(GLSRC)memory_.h
$(GLSRC)gxclip2.h:$(GLSRC)gsuid.h
$(GLSRC)gxclip2.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclip2.h:$(GLSRC)gxsync.h
$(GLSRC)gxclip2.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclip2.h:$(GLSRC)srdline.h
$(GLSRC)gxclip2.h:$(GLSRC)scommon.h
$(GLSRC)gxclip2.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclip2.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclip2.h:$(GLSRC)gxarith.h
$(GLSRC)gxclip2.h:$(GLSRC)stat_.h
$(GLSRC)gxclip2.h:$(GLSRC)gpsync.h
$(GLSRC)gxclip2.h:$(GLSRC)gsstype.h
$(GLSRC)gxclip2.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclip2.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclip2.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclip2.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclip2.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclip2.h:$(GLSRC)stdio_.h
$(GLSRC)gxclip2.h:$(GLSRC)gsccode.h
$(GLSRC)gxclip2.h:$(GLSRC)stdint_.h
$(GLSRC)gxclip2.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclip2.h:$(GLSRC)gstypes.h
$(GLSRC)gxclip2.h:$(GLSRC)std.h
$(GLSRC)gxclip2.h:$(GLSRC)stdpre.h
$(GLSRC)gxclip2.h:$(GLGEN)arch.h
$(GLSRC)gxclip2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclipm.h:$(GLSRC)gxmclip.h
$(GLSRC)gxclipm.h:$(GLSRC)gxclip.h
$(GLSRC)gxclipm.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclipm.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclipm.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclipm.h:$(GLSRC)gxtext.h
$(GLSRC)gxclipm.h:$(GLSRC)gstext.h
$(GLSRC)gxclipm.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclipm.h:$(GLSRC)gstparam.h
$(GLSRC)gxclipm.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclipm.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclipm.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclipm.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclipm.h:$(GLSRC)gscsel.h
$(GLSRC)gxclipm.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclipm.h:$(GLSRC)gsfont.h
$(GLSRC)gxclipm.h:$(GLSRC)gsimage.h
$(GLSRC)gxclipm.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclipm.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclipm.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclipm.h:$(GLSRC)gsropt.h
$(GLSRC)gxclipm.h:$(GLSRC)gxdda.h
$(GLSRC)gxclipm.h:$(GLSRC)gxpath.h
$(GLSRC)gxclipm.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclipm.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclipm.h:$(GLSRC)gxftype.h
$(GLSRC)gxclipm.h:$(GLSRC)gscms.h
$(GLSRC)gxclipm.h:$(GLSRC)gsrect.h
$(GLSRC)gxclipm.h:$(GLSRC)gslparam.h
$(GLSRC)gxclipm.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclipm.h:$(GLSRC)gscpm.h
$(GLSRC)gxclipm.h:$(GLSRC)gscspace.h
$(GLSRC)gxclipm.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclipm.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclipm.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclipm.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclipm.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclipm.h:$(GLSRC)gscompt.h
$(GLSRC)gxclipm.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclipm.h:$(GLSRC)gspenum.h
$(GLSRC)gxclipm.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclipm.h:$(GLSRC)gsparam.h
$(GLSRC)gxclipm.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclipm.h:$(GLSRC)gp.h
$(GLSRC)gxclipm.h:$(GLSRC)memento.h
$(GLSRC)gxclipm.h:$(GLSRC)memory_.h
$(GLSRC)gxclipm.h:$(GLSRC)gsuid.h
$(GLSRC)gxclipm.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclipm.h:$(GLSRC)gxsync.h
$(GLSRC)gxclipm.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclipm.h:$(GLSRC)srdline.h
$(GLSRC)gxclipm.h:$(GLSRC)scommon.h
$(GLSRC)gxclipm.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclipm.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclipm.h:$(GLSRC)gxarith.h
$(GLSRC)gxclipm.h:$(GLSRC)stat_.h
$(GLSRC)gxclipm.h:$(GLSRC)gpsync.h
$(GLSRC)gxclipm.h:$(GLSRC)gsstype.h
$(GLSRC)gxclipm.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclipm.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclipm.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclipm.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclipm.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclipm.h:$(GLSRC)stdio_.h
$(GLSRC)gxclipm.h:$(GLSRC)gsccode.h
$(GLSRC)gxclipm.h:$(GLSRC)stdint_.h
$(GLSRC)gxclipm.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclipm.h:$(GLSRC)gstypes.h
$(GLSRC)gxclipm.h:$(GLSRC)std.h
$(GLSRC)gxclipm.h:$(GLSRC)stdpre.h
$(GLSRC)gxclipm.h:$(GLGEN)arch.h
$(GLSRC)gxclipm.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxctable.h:$(GLSRC)gxfrac.h
$(GLSRC)gxctable.h:$(GLSRC)gxfixed.h
$(GLSRC)gxctable.h:$(GLSRC)gstypes.h
$(GLSRC)gxctable.h:$(GLSRC)std.h
$(GLSRC)gxctable.h:$(GLSRC)stdpre.h
$(GLSRC)gxctable.h:$(GLGEN)arch.h
$(GLSRC)gxfcache.h:$(GLSRC)gsfont.h
$(GLSRC)gxfcache.h:$(GLSRC)gxbcache.h
$(GLSRC)gxfcache.h:$(GLSRC)gxftype.h
$(GLSRC)gxfcache.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfcache.h:$(GLSRC)gsxfont.h
$(GLSRC)gxfcache.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfcache.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfcache.h:$(GLSRC)gsuid.h
$(GLSRC)gxfcache.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfcache.h:$(GLSRC)scommon.h
$(GLSRC)gxfcache.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfcache.h:$(GLSRC)gsstype.h
$(GLSRC)gxfcache.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfcache.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfcache.h:$(GLSRC)stdio_.h
$(GLSRC)gxfcache.h:$(GLSRC)gsccode.h
$(GLSRC)gxfcache.h:$(GLSRC)stdint_.h
$(GLSRC)gxfcache.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfcache.h:$(GLSRC)gstypes.h
$(GLSRC)gxfcache.h:$(GLSRC)std.h
$(GLSRC)gxfcache.h:$(GLSRC)stdpre.h
$(GLSRC)gxfcache.h:$(GLGEN)arch.h
$(GLSRC)gxfcache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfont.h:$(GLSRC)gspath.h
$(GLSRC)gxfont.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfont.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfont.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfont.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfont.h:$(GLSRC)gstext.h
$(GLSRC)gxfont.h:$(GLSRC)gsfont.h
$(GLSRC)gxfont.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfont.h:$(GLSRC)gxpath.h
$(GLSRC)gxfont.h:$(GLSRC)gxftype.h
$(GLSRC)gxfont.h:$(GLSRC)gscms.h
$(GLSRC)gxfont.h:$(GLSRC)gsrect.h
$(GLSRC)gxfont.h:$(GLSRC)gslparam.h
$(GLSRC)gxfont.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfont.h:$(GLSRC)gscpm.h
$(GLSRC)gxfont.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfont.h:$(GLSRC)gscspace.h
$(GLSRC)gxfont.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfont.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfont.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfont.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfont.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfont.h:$(GLSRC)gspenum.h
$(GLSRC)gxfont.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfont.h:$(GLSRC)gsparam.h
$(GLSRC)gxfont.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfont.h:$(GLSRC)memento.h
$(GLSRC)gxfont.h:$(GLSRC)gsuid.h
$(GLSRC)gxfont.h:$(GLSRC)gxsync.h
$(GLSRC)gxfont.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfont.h:$(GLSRC)scommon.h
$(GLSRC)gxfont.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfont.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfont.h:$(GLSRC)gxarith.h
$(GLSRC)gxfont.h:$(GLSRC)gpsync.h
$(GLSRC)gxfont.h:$(GLSRC)gsstype.h
$(GLSRC)gxfont.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfont.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfont.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfont.h:$(GLSRC)stdio_.h
$(GLSRC)gxfont.h:$(GLSRC)gsccode.h
$(GLSRC)gxfont.h:$(GLSRC)stdint_.h
$(GLSRC)gxfont.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfont.h:$(GLSRC)gstypes.h
$(GLSRC)gxfont.h:$(GLSRC)std.h
$(GLSRC)gxfont.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont.h:$(GLGEN)arch.h
$(GLSRC)gxfont.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxiparam.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxiparam.h:$(GLSRC)gxcmap.h
$(GLSRC)gxiparam.h:$(GLSRC)gxtext.h
$(GLSRC)gxiparam.h:$(GLSRC)gstext.h
$(GLSRC)gxiparam.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxiparam.h:$(GLSRC)gstparam.h
$(GLSRC)gxiparam.h:$(GLSRC)gxfmap.h
$(GLSRC)gxiparam.h:$(GLSRC)gsfunc.h
$(GLSRC)gxiparam.h:$(GLSRC)gxcspace.h
$(GLSRC)gxiparam.h:$(GLSRC)gxrplane.h
$(GLSRC)gxiparam.h:$(GLSRC)gscsel.h
$(GLSRC)gxiparam.h:$(GLSRC)gxfcache.h
$(GLSRC)gxiparam.h:$(GLSRC)gsfont.h
$(GLSRC)gxiparam.h:$(GLSRC)gsimage.h
$(GLSRC)gxiparam.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxiparam.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxiparam.h:$(GLSRC)gxbcache.h
$(GLSRC)gxiparam.h:$(GLSRC)gsropt.h
$(GLSRC)gxiparam.h:$(GLSRC)gxdda.h
$(GLSRC)gxiparam.h:$(GLSRC)gxpath.h
$(GLSRC)gxiparam.h:$(GLSRC)gxfrac.h
$(GLSRC)gxiparam.h:$(GLSRC)gxtmap.h
$(GLSRC)gxiparam.h:$(GLSRC)gxftype.h
$(GLSRC)gxiparam.h:$(GLSRC)gscms.h
$(GLSRC)gxiparam.h:$(GLSRC)gsrect.h
$(GLSRC)gxiparam.h:$(GLSRC)gslparam.h
$(GLSRC)gxiparam.h:$(GLSRC)gsdevice.h
$(GLSRC)gxiparam.h:$(GLSRC)gscpm.h
$(GLSRC)gxiparam.h:$(GLSRC)gscspace.h
$(GLSRC)gxiparam.h:$(GLSRC)gsgstate.h
$(GLSRC)gxiparam.h:$(GLSRC)gsxfont.h
$(GLSRC)gxiparam.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxiparam.h:$(GLSRC)gsiparam.h
$(GLSRC)gxiparam.h:$(GLSRC)gxfixed.h
$(GLSRC)gxiparam.h:$(GLSRC)gscompt.h
$(GLSRC)gxiparam.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxiparam.h:$(GLSRC)gspenum.h
$(GLSRC)gxiparam.h:$(GLSRC)gxhttile.h
$(GLSRC)gxiparam.h:$(GLSRC)gsparam.h
$(GLSRC)gxiparam.h:$(GLSRC)gsrefct.h
$(GLSRC)gxiparam.h:$(GLSRC)gp.h
$(GLSRC)gxiparam.h:$(GLSRC)memento.h
$(GLSRC)gxiparam.h:$(GLSRC)memory_.h
$(GLSRC)gxiparam.h:$(GLSRC)gsuid.h
$(GLSRC)gxiparam.h:$(GLSRC)gsstruct.h
$(GLSRC)gxiparam.h:$(GLSRC)gxsync.h
$(GLSRC)gxiparam.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxiparam.h:$(GLSRC)srdline.h
$(GLSRC)gxiparam.h:$(GLSRC)scommon.h
$(GLSRC)gxiparam.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxiparam.h:$(GLSRC)gsccolor.h
$(GLSRC)gxiparam.h:$(GLSRC)gxarith.h
$(GLSRC)gxiparam.h:$(GLSRC)stat_.h
$(GLSRC)gxiparam.h:$(GLSRC)gpsync.h
$(GLSRC)gxiparam.h:$(GLSRC)gsstype.h
$(GLSRC)gxiparam.h:$(GLSRC)gsmemory.h
$(GLSRC)gxiparam.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxiparam.h:$(GLSRC)gscdefs.h
$(GLSRC)gxiparam.h:$(GLSRC)gslibctx.h
$(GLSRC)gxiparam.h:$(GLSRC)gxcindex.h
$(GLSRC)gxiparam.h:$(GLSRC)stdio_.h
$(GLSRC)gxiparam.h:$(GLSRC)gsccode.h
$(GLSRC)gxiparam.h:$(GLSRC)stdint_.h
$(GLSRC)gxiparam.h:$(GLSRC)gssprintf.h
$(GLSRC)gxiparam.h:$(GLSRC)gstypes.h
$(GLSRC)gxiparam.h:$(GLSRC)std.h
$(GLSRC)gxiparam.h:$(GLSRC)stdpre.h
$(GLSRC)gxiparam.h:$(GLGEN)arch.h
$(GLSRC)gxiparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gximask.h:$(GLSRC)gsdcolor.h
$(GLSRC)gximask.h:$(GLSRC)gsropt.h
$(GLSRC)gximask.h:$(GLSRC)gxpath.h
$(GLSRC)gximask.h:$(GLSRC)gscms.h
$(GLSRC)gximask.h:$(GLSRC)gsrect.h
$(GLSRC)gximask.h:$(GLSRC)gslparam.h
$(GLSRC)gximask.h:$(GLSRC)gsdevice.h
$(GLSRC)gximask.h:$(GLSRC)gscpm.h
$(GLSRC)gximask.h:$(GLSRC)gscspace.h
$(GLSRC)gximask.h:$(GLSRC)gsgstate.h
$(GLSRC)gximask.h:$(GLSRC)gsiparam.h
$(GLSRC)gximask.h:$(GLSRC)gxfixed.h
$(GLSRC)gximask.h:$(GLSRC)gsmatrix.h
$(GLSRC)gximask.h:$(GLSRC)gspenum.h
$(GLSRC)gximask.h:$(GLSRC)gxhttile.h
$(GLSRC)gximask.h:$(GLSRC)gsparam.h
$(GLSRC)gximask.h:$(GLSRC)gsrefct.h
$(GLSRC)gximask.h:$(GLSRC)memento.h
$(GLSRC)gximask.h:$(GLSRC)gxsync.h
$(GLSRC)gximask.h:$(GLSRC)gxbitmap.h
$(GLSRC)gximask.h:$(GLSRC)scommon.h
$(GLSRC)gximask.h:$(GLSRC)gsbitmap.h
$(GLSRC)gximask.h:$(GLSRC)gsccolor.h
$(GLSRC)gximask.h:$(GLSRC)gxarith.h
$(GLSRC)gximask.h:$(GLSRC)gpsync.h
$(GLSRC)gximask.h:$(GLSRC)gsstype.h
$(GLSRC)gximask.h:$(GLSRC)gsmemory.h
$(GLSRC)gximask.h:$(GLSRC)gslibctx.h
$(GLSRC)gximask.h:$(GLSRC)gxcindex.h
$(GLSRC)gximask.h:$(GLSRC)stdio_.h
$(GLSRC)gximask.h:$(GLSRC)stdint_.h
$(GLSRC)gximask.h:$(GLSRC)gssprintf.h
$(GLSRC)gximask.h:$(GLSRC)gstypes.h
$(GLSRC)gximask.h:$(GLSRC)std.h
$(GLSRC)gximask.h:$(GLSRC)stdpre.h
$(GLSRC)gximask.h:$(GLGEN)arch.h
$(GLSRC)gximask.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscie.h:$(GLSRC)gxctable.h
$(GLSRC)gscie.h:$(GLSRC)gxfrac.h
$(GLSRC)gscie.h:$(GLSRC)gscspace.h
$(GLSRC)gscie.h:$(GLSRC)gsgstate.h
$(GLSRC)gscie.h:$(GLSRC)gsiparam.h
$(GLSRC)gscie.h:$(GLSRC)gxfixed.h
$(GLSRC)gscie.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscie.h:$(GLSRC)gsrefct.h
$(GLSRC)gscie.h:$(GLSRC)memento.h
$(GLSRC)gscie.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscie.h:$(GLSRC)scommon.h
$(GLSRC)gscie.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscie.h:$(GLSRC)gsccolor.h
$(GLSRC)gscie.h:$(GLSRC)gsstype.h
$(GLSRC)gscie.h:$(GLSRC)gsmemory.h
$(GLSRC)gscie.h:$(GLSRC)gslibctx.h
$(GLSRC)gscie.h:$(GLSRC)stdio_.h
$(GLSRC)gscie.h:$(GLSRC)stdint_.h
$(GLSRC)gscie.h:$(GLSRC)gssprintf.h
$(GLSRC)gscie.h:$(GLSRC)gstypes.h
$(GLSRC)gscie.h:$(GLSRC)std.h
$(GLSRC)gscie.h:$(GLSRC)stdpre.h
$(GLSRC)gscie.h:$(GLGEN)arch.h
$(GLSRC)gscie.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc.h:$(GLSRC)gscie.h
$(GLSRC)gsicc.h:$(GLSRC)gxcspace.h
$(GLSRC)gsicc.h:$(GLSRC)gxctable.h
$(GLSRC)gsicc.h:$(GLSRC)gscsel.h
$(GLSRC)gsicc.h:$(GLSRC)gsdcolor.h
$(GLSRC)gsicc.h:$(GLSRC)gxfrac.h
$(GLSRC)gsicc.h:$(GLSRC)gscms.h
$(GLSRC)gsicc.h:$(GLSRC)gsdevice.h
$(GLSRC)gsicc.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc.h:$(GLSRC)gxfixed.h
$(GLSRC)gsicc.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc.h:$(GLSRC)gxhttile.h
$(GLSRC)gsicc.h:$(GLSRC)gsparam.h
$(GLSRC)gsicc.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc.h:$(GLSRC)memento.h
$(GLSRC)gsicc.h:$(GLSRC)gxsync.h
$(GLSRC)gsicc.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc.h:$(GLSRC)scommon.h
$(GLSRC)gsicc.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc.h:$(GLSRC)gxarith.h
$(GLSRC)gsicc.h:$(GLSRC)gpsync.h
$(GLSRC)gsicc.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc.h:$(GLSRC)gxcindex.h
$(GLSRC)gsicc.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc.h:$(GLSRC)std.h
$(GLSRC)gsicc.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc.h:$(GLGEN)arch.h
$(GLSRC)gsicc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscrd.h:$(GLSRC)gscie.h
$(GLSRC)gscrd.h:$(GLSRC)gxctable.h
$(GLSRC)gscrd.h:$(GLSRC)gxfrac.h
$(GLSRC)gscrd.h:$(GLSRC)gscspace.h
$(GLSRC)gscrd.h:$(GLSRC)gsgstate.h
$(GLSRC)gscrd.h:$(GLSRC)gsiparam.h
$(GLSRC)gscrd.h:$(GLSRC)gxfixed.h
$(GLSRC)gscrd.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscrd.h:$(GLSRC)gsrefct.h
$(GLSRC)gscrd.h:$(GLSRC)memento.h
$(GLSRC)gscrd.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscrd.h:$(GLSRC)scommon.h
$(GLSRC)gscrd.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscrd.h:$(GLSRC)gsccolor.h
$(GLSRC)gscrd.h:$(GLSRC)gsstype.h
$(GLSRC)gscrd.h:$(GLSRC)gsmemory.h
$(GLSRC)gscrd.h:$(GLSRC)gslibctx.h
$(GLSRC)gscrd.h:$(GLSRC)stdio_.h
$(GLSRC)gscrd.h:$(GLSRC)stdint_.h
$(GLSRC)gscrd.h:$(GLSRC)gssprintf.h
$(GLSRC)gscrd.h:$(GLSRC)gstypes.h
$(GLSRC)gscrd.h:$(GLSRC)std.h
$(GLSRC)gscrd.h:$(GLSRC)stdpre.h
$(GLSRC)gscrd.h:$(GLGEN)arch.h
$(GLSRC)gscrd.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscrdp.h:$(GLSRC)gscie.h
$(GLSRC)gscrdp.h:$(GLSRC)gxctable.h
$(GLSRC)gscrdp.h:$(GLSRC)gxfrac.h
$(GLSRC)gscrdp.h:$(GLSRC)gsdevice.h
$(GLSRC)gscrdp.h:$(GLSRC)gscspace.h
$(GLSRC)gscrdp.h:$(GLSRC)gsgstate.h
$(GLSRC)gscrdp.h:$(GLSRC)gsiparam.h
$(GLSRC)gscrdp.h:$(GLSRC)gxfixed.h
$(GLSRC)gscrdp.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscrdp.h:$(GLSRC)gsparam.h
$(GLSRC)gscrdp.h:$(GLSRC)gsrefct.h
$(GLSRC)gscrdp.h:$(GLSRC)memento.h
$(GLSRC)gscrdp.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscrdp.h:$(GLSRC)scommon.h
$(GLSRC)gscrdp.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscrdp.h:$(GLSRC)gsccolor.h
$(GLSRC)gscrdp.h:$(GLSRC)gsstype.h
$(GLSRC)gscrdp.h:$(GLSRC)gsmemory.h
$(GLSRC)gscrdp.h:$(GLSRC)gslibctx.h
$(GLSRC)gscrdp.h:$(GLSRC)stdio_.h
$(GLSRC)gscrdp.h:$(GLSRC)stdint_.h
$(GLSRC)gscrdp.h:$(GLSRC)gssprintf.h
$(GLSRC)gscrdp.h:$(GLSRC)gstypes.h
$(GLSRC)gscrdp.h:$(GLSRC)std.h
$(GLSRC)gscrdp.h:$(GLSRC)stdpre.h
$(GLSRC)gscrdp.h:$(GLGEN)arch.h
$(GLSRC)gscrdp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscdevn.h:$(GLSRC)gsfunc.h
$(GLSRC)gscdevn.h:$(GLSRC)gscspace.h
$(GLSRC)gscdevn.h:$(GLSRC)gsgstate.h
$(GLSRC)gscdevn.h:$(GLSRC)gsdsrc.h
$(GLSRC)gscdevn.h:$(GLSRC)gsiparam.h
$(GLSRC)gscdevn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscdevn.h:$(GLSRC)gsparam.h
$(GLSRC)gscdevn.h:$(GLSRC)gsrefct.h
$(GLSRC)gscdevn.h:$(GLSRC)memento.h
$(GLSRC)gscdevn.h:$(GLSRC)gsstruct.h
$(GLSRC)gscdevn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscdevn.h:$(GLSRC)scommon.h
$(GLSRC)gscdevn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscdevn.h:$(GLSRC)gsccolor.h
$(GLSRC)gscdevn.h:$(GLSRC)gsstype.h
$(GLSRC)gscdevn.h:$(GLSRC)gsmemory.h
$(GLSRC)gscdevn.h:$(GLSRC)gslibctx.h
$(GLSRC)gscdevn.h:$(GLSRC)stdio_.h
$(GLSRC)gscdevn.h:$(GLSRC)stdint_.h
$(GLSRC)gscdevn.h:$(GLSRC)gssprintf.h
$(GLSRC)gscdevn.h:$(GLSRC)gstypes.h
$(GLSRC)gscdevn.h:$(GLSRC)std.h
$(GLSRC)gscdevn.h:$(GLSRC)stdpre.h
$(GLSRC)gscdevn.h:$(GLGEN)arch.h
$(GLSRC)gscdevn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscindex.h:$(GLSRC)gxfrac.h
$(GLSRC)gscindex.h:$(GLSRC)gscspace.h
$(GLSRC)gscindex.h:$(GLSRC)gsgstate.h
$(GLSRC)gscindex.h:$(GLSRC)gsiparam.h
$(GLSRC)gscindex.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscindex.h:$(GLSRC)gsrefct.h
$(GLSRC)gscindex.h:$(GLSRC)memento.h
$(GLSRC)gscindex.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscindex.h:$(GLSRC)scommon.h
$(GLSRC)gscindex.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscindex.h:$(GLSRC)gsccolor.h
$(GLSRC)gscindex.h:$(GLSRC)gsstype.h
$(GLSRC)gscindex.h:$(GLSRC)gsmemory.h
$(GLSRC)gscindex.h:$(GLSRC)gslibctx.h
$(GLSRC)gscindex.h:$(GLSRC)stdio_.h
$(GLSRC)gscindex.h:$(GLSRC)stdint_.h
$(GLSRC)gscindex.h:$(GLSRC)gssprintf.h
$(GLSRC)gscindex.h:$(GLSRC)gstypes.h
$(GLSRC)gscindex.h:$(GLSRC)std.h
$(GLSRC)gscindex.h:$(GLSRC)stdpre.h
$(GLSRC)gscindex.h:$(GLGEN)arch.h
$(GLSRC)gscindex.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscolor2.h:$(GLSRC)gscindex.h
$(GLSRC)gscolor2.h:$(GLSRC)gsptype1.h
$(GLSRC)gscolor2.h:$(GLSRC)gscie.h
$(GLSRC)gscolor2.h:$(GLSRC)gspcolor.h
$(GLSRC)gscolor2.h:$(GLSRC)gxctable.h
$(GLSRC)gscolor2.h:$(GLSRC)gsdcolor.h
$(GLSRC)gscolor2.h:$(GLSRC)gxfrac.h
$(GLSRC)gscolor2.h:$(GLSRC)gscms.h
$(GLSRC)gscolor2.h:$(GLSRC)gsdevice.h
$(GLSRC)gscolor2.h:$(GLSRC)gscspace.h
$(GLSRC)gscolor2.h:$(GLSRC)gsgstate.h
$(GLSRC)gscolor2.h:$(GLSRC)gsiparam.h
$(GLSRC)gscolor2.h:$(GLSRC)gxfixed.h
$(GLSRC)gscolor2.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscolor2.h:$(GLSRC)gxhttile.h
$(GLSRC)gscolor2.h:$(GLSRC)gsparam.h
$(GLSRC)gscolor2.h:$(GLSRC)gsrefct.h
$(GLSRC)gscolor2.h:$(GLSRC)memento.h
$(GLSRC)gscolor2.h:$(GLSRC)gsuid.h
$(GLSRC)gscolor2.h:$(GLSRC)gxsync.h
$(GLSRC)gscolor2.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscolor2.h:$(GLSRC)scommon.h
$(GLSRC)gscolor2.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscolor2.h:$(GLSRC)gsccolor.h
$(GLSRC)gscolor2.h:$(GLSRC)gxarith.h
$(GLSRC)gscolor2.h:$(GLSRC)gpsync.h
$(GLSRC)gscolor2.h:$(GLSRC)gsstype.h
$(GLSRC)gscolor2.h:$(GLSRC)gsmemory.h
$(GLSRC)gscolor2.h:$(GLSRC)gslibctx.h
$(GLSRC)gscolor2.h:$(GLSRC)gxcindex.h
$(GLSRC)gscolor2.h:$(GLSRC)stdio_.h
$(GLSRC)gscolor2.h:$(GLSRC)stdint_.h
$(GLSRC)gscolor2.h:$(GLSRC)gssprintf.h
$(GLSRC)gscolor2.h:$(GLSRC)gstypes.h
$(GLSRC)gscolor2.h:$(GLSRC)std.h
$(GLSRC)gscolor2.h:$(GLSRC)stdpre.h
$(GLSRC)gscolor2.h:$(GLGEN)arch.h
$(GLSRC)gscolor2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscsepr.h:$(GLSRC)gsfunc.h
$(GLSRC)gscsepr.h:$(GLSRC)gscspace.h
$(GLSRC)gscsepr.h:$(GLSRC)gsgstate.h
$(GLSRC)gscsepr.h:$(GLSRC)gsdsrc.h
$(GLSRC)gscsepr.h:$(GLSRC)gsiparam.h
$(GLSRC)gscsepr.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscsepr.h:$(GLSRC)gsparam.h
$(GLSRC)gscsepr.h:$(GLSRC)gsrefct.h
$(GLSRC)gscsepr.h:$(GLSRC)memento.h
$(GLSRC)gscsepr.h:$(GLSRC)gsstruct.h
$(GLSRC)gscsepr.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscsepr.h:$(GLSRC)scommon.h
$(GLSRC)gscsepr.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscsepr.h:$(GLSRC)gsccolor.h
$(GLSRC)gscsepr.h:$(GLSRC)gsstype.h
$(GLSRC)gscsepr.h:$(GLSRC)gsmemory.h
$(GLSRC)gscsepr.h:$(GLSRC)gslibctx.h
$(GLSRC)gscsepr.h:$(GLSRC)stdio_.h
$(GLSRC)gscsepr.h:$(GLSRC)stdint_.h
$(GLSRC)gscsepr.h:$(GLSRC)gssprintf.h
$(GLSRC)gscsepr.h:$(GLSRC)gstypes.h
$(GLSRC)gscsepr.h:$(GLSRC)std.h
$(GLSRC)gscsepr.h:$(GLSRC)stdpre.h
$(GLSRC)gscsepr.h:$(GLGEN)arch.h
$(GLSRC)gscsepr.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxdcconv.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdcconv.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdcconv.h:$(GLSRC)std.h
$(GLSRC)gxdcconv.h:$(GLSRC)stdpre.h
$(GLSRC)gxdcconv.h:$(GLGEN)arch.h
$(GLSRC)gxfmap.h:$(GLSRC)gxfrac.h
$(GLSRC)gxfmap.h:$(GLSRC)gxtmap.h
$(GLSRC)gxfmap.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfmap.h:$(GLSRC)memento.h
$(GLSRC)gxfmap.h:$(GLSRC)gsstype.h
$(GLSRC)gxfmap.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfmap.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfmap.h:$(GLSRC)stdio_.h
$(GLSRC)gxfmap.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfmap.h:$(GLSRC)gstypes.h
$(GLSRC)gxfmap.h:$(GLSRC)std.h
$(GLSRC)gxfmap.h:$(GLSRC)stdpre.h
$(GLSRC)gxfmap.h:$(GLGEN)arch.h
$(GLSRC)gxfmap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcmap.h:$(GLSRC)gxfmap.h
$(GLSRC)gxcmap.h:$(GLSRC)gscsel.h
$(GLSRC)gxcmap.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcmap.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxcmap.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcmap.h:$(GLSRC)gxtmap.h
$(GLSRC)gxcmap.h:$(GLSRC)gscms.h
$(GLSRC)gxcmap.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcmap.h:$(GLSRC)gscspace.h
$(GLSRC)gxcmap.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcmap.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcmap.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcmap.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcmap.h:$(GLSRC)gsparam.h
$(GLSRC)gxcmap.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcmap.h:$(GLSRC)memento.h
$(GLSRC)gxcmap.h:$(GLSRC)gxsync.h
$(GLSRC)gxcmap.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcmap.h:$(GLSRC)scommon.h
$(GLSRC)gxcmap.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcmap.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcmap.h:$(GLSRC)gxarith.h
$(GLSRC)gxcmap.h:$(GLSRC)gpsync.h
$(GLSRC)gxcmap.h:$(GLSRC)gsstype.h
$(GLSRC)gxcmap.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcmap.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcmap.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcmap.h:$(GLSRC)stdio_.h
$(GLSRC)gxcmap.h:$(GLSRC)stdint_.h
$(GLSRC)gxcmap.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcmap.h:$(GLSRC)gstypes.h
$(GLSRC)gxcmap.h:$(GLSRC)std.h
$(GLSRC)gxcmap.h:$(GLSRC)stdpre.h
$(GLSRC)gxcmap.h:$(GLGEN)arch.h
$(GLSRC)gxcmap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxgstate.h:$(GLSRC)gstrans.h
$(GLSRC)gxgstate.h:$(GLSRC)gdevp14.h
$(GLSRC)gxgstate.h:$(GLSRC)gxline.h
$(GLSRC)gxgstate.h:$(GLSRC)gsht1.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcomp.h
$(GLSRC)gxgstate.h:$(GLSRC)math_.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxgstate.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxgstate.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxgstate.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxgstate.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxgstate.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxgstate.h:$(GLSRC)gxblend.h
$(GLSRC)gxgstate.h:$(GLSRC)gscolor2.h
$(GLSRC)gxgstate.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxgstate.h:$(GLSRC)gxdevice.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcpath.h
$(GLSRC)gxgstate.h:$(GLSRC)gsht.h
$(GLSRC)gxgstate.h:$(GLSRC)gsequivc.h
$(GLSRC)gxgstate.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxgstate.h:$(GLSRC)gxpcache.h
$(GLSRC)gxgstate.h:$(GLSRC)gscindex.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcmap.h
$(GLSRC)gxgstate.h:$(GLSRC)gsptype1.h
$(GLSRC)gxgstate.h:$(GLSRC)gscie.h
$(GLSRC)gxgstate.h:$(GLSRC)gxtext.h
$(GLSRC)gxgstate.h:$(GLSRC)gstext.h
$(GLSRC)gxgstate.h:$(GLSRC)gxstate.h
$(GLSRC)gxgstate.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxgstate.h:$(GLSRC)gstparam.h
$(GLSRC)gxgstate.h:$(GLSRC)gspcolor.h
$(GLSRC)gxgstate.h:$(GLSRC)gxfmap.h
$(GLSRC)gxgstate.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxgstate.h:$(GLSRC)gsfunc.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcspace.h
$(GLSRC)gxgstate.h:$(GLSRC)gxctable.h
$(GLSRC)gxgstate.h:$(GLSRC)gxrplane.h
$(GLSRC)gxgstate.h:$(GLSRC)gscsel.h
$(GLSRC)gxgstate.h:$(GLSRC)gxfcache.h
$(GLSRC)gxgstate.h:$(GLSRC)gsfont.h
$(GLSRC)gxgstate.h:$(GLSRC)gsimage.h
$(GLSRC)gxgstate.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxgstate.h:$(GLSRC)gxbcache.h
$(GLSRC)gxgstate.h:$(GLSRC)gsropt.h
$(GLSRC)gxgstate.h:$(GLSRC)gxdda.h
$(GLSRC)gxgstate.h:$(GLSRC)gxpath.h
$(GLSRC)gxgstate.h:$(GLSRC)gxiclass.h
$(GLSRC)gxgstate.h:$(GLSRC)gxfrac.h
$(GLSRC)gxgstate.h:$(GLSRC)gxtmap.h
$(GLSRC)gxgstate.h:$(GLSRC)gxftype.h
$(GLSRC)gxgstate.h:$(GLSRC)gscms.h
$(GLSRC)gxgstate.h:$(GLSRC)gsrect.h
$(GLSRC)gxgstate.h:$(GLSRC)gslparam.h
$(GLSRC)gxgstate.h:$(GLSRC)gsdevice.h
$(GLSRC)gxgstate.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxgstate.h:$(GLSRC)gscpm.h
$(GLSRC)gxgstate.h:$(GLSRC)gscspace.h
$(GLSRC)gxgstate.h:$(GLSRC)gsgstate.h
$(GLSRC)gxgstate.h:$(GLSRC)gxstdio.h
$(GLSRC)gxgstate.h:$(GLSRC)gsxfont.h
$(GLSRC)gxgstate.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxgstate.h:$(GLSRC)gsio.h
$(GLSRC)gxgstate.h:$(GLSRC)gsiparam.h
$(GLSRC)gxgstate.h:$(GLSRC)gxfixed.h
$(GLSRC)gxgstate.h:$(GLSRC)gscompt.h
$(GLSRC)gxgstate.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxgstate.h:$(GLSRC)gspenum.h
$(GLSRC)gxgstate.h:$(GLSRC)gxhttile.h
$(GLSRC)gxgstate.h:$(GLSRC)gsparam.h
$(GLSRC)gxgstate.h:$(GLSRC)gsrefct.h
$(GLSRC)gxgstate.h:$(GLSRC)gp.h
$(GLSRC)gxgstate.h:$(GLSRC)memento.h
$(GLSRC)gxgstate.h:$(GLSRC)memory_.h
$(GLSRC)gxgstate.h:$(GLSRC)gsuid.h
$(GLSRC)gxgstate.h:$(GLSRC)gsstruct.h
$(GLSRC)gxgstate.h:$(GLSRC)gxsync.h
$(GLSRC)gxgstate.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxgstate.h:$(GLSRC)vmsmath.h
$(GLSRC)gxgstate.h:$(GLSRC)srdline.h
$(GLSRC)gxgstate.h:$(GLSRC)scommon.h
$(GLSRC)gxgstate.h:$(GLSRC)gsfname.h
$(GLSRC)gxgstate.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxgstate.h:$(GLSRC)gsccolor.h
$(GLSRC)gxgstate.h:$(GLSRC)gxarith.h
$(GLSRC)gxgstate.h:$(GLSRC)stat_.h
$(GLSRC)gxgstate.h:$(GLSRC)gpsync.h
$(GLSRC)gxgstate.h:$(GLSRC)gsstype.h
$(GLSRC)gxgstate.h:$(GLSRC)gsmemory.h
$(GLSRC)gxgstate.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxgstate.h:$(GLSRC)gscdefs.h
$(GLSRC)gxgstate.h:$(GLSRC)gslibctx.h
$(GLSRC)gxgstate.h:$(GLSRC)gxcindex.h
$(GLSRC)gxgstate.h:$(GLSRC)stdio_.h
$(GLSRC)gxgstate.h:$(GLSRC)gsccode.h
$(GLSRC)gxgstate.h:$(GLSRC)stdint_.h
$(GLSRC)gxgstate.h:$(GLSRC)gssprintf.h
$(GLSRC)gxgstate.h:$(GLSRC)gstypes.h
$(GLSRC)gxgstate.h:$(GLSRC)std.h
$(GLSRC)gxgstate.h:$(GLSRC)stdpre.h
$(GLSRC)gxgstate.h:$(GLGEN)arch.h
$(GLSRC)gxgstate.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscolor2.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscindex.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxcmap.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsptype1.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscie.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxtext.h
$(GLSRC)gxcolor2.h:$(GLSRC)gstext.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxcolor2.h:$(GLSRC)gstparam.h
$(GLSRC)gxcolor2.h:$(GLSRC)gspcolor.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxfmap.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsfunc.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxcspace.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxctable.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxrplane.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscsel.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxfcache.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsfont.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsimage.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxbcache.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsropt.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxdda.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxpath.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxtmap.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxftype.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscms.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsrect.h
$(GLSRC)gxcolor2.h:$(GLSRC)gslparam.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscpm.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscspace.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsxfont.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxfixed.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscompt.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcolor2.h:$(GLSRC)gspenum.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsparam.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcolor2.h:$(GLSRC)gp.h
$(GLSRC)gxcolor2.h:$(GLSRC)memento.h
$(GLSRC)gxcolor2.h:$(GLSRC)memory_.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsuid.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsstruct.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxsync.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcolor2.h:$(GLSRC)srdline.h
$(GLSRC)gxcolor2.h:$(GLSRC)scommon.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxarith.h
$(GLSRC)gxcolor2.h:$(GLSRC)stat_.h
$(GLSRC)gxcolor2.h:$(GLSRC)gpsync.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsstype.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcolor2.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxcolor2.h:$(GLSRC)gscdefs.h
$(GLSRC)gxcolor2.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcolor2.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcolor2.h:$(GLSRC)stdio_.h
$(GLSRC)gxcolor2.h:$(GLSRC)gsccode.h
$(GLSRC)gxcolor2.h:$(GLSRC)stdint_.h
$(GLSRC)gxcolor2.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcolor2.h:$(GLSRC)gstypes.h
$(GLSRC)gxcolor2.h:$(GLSRC)std.h
$(GLSRC)gxcolor2.h:$(GLSRC)stdpre.h
$(GLSRC)gxcolor2.h:$(GLGEN)arch.h
$(GLSRC)gxcolor2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclist.h:$(GLSRC)gxgstate.h
$(GLSRC)gxclist.h:$(GLSRC)gstrans.h
$(GLSRC)gxclist.h:$(GLSRC)gdevp14.h
$(GLSRC)gxclist.h:$(GLSRC)gxline.h
$(GLSRC)gxclist.h:$(GLSRC)gsht1.h
$(GLSRC)gxclist.h:$(GLSRC)gxcomp.h
$(GLSRC)gxclist.h:$(GLSRC)math_.h
$(GLSRC)gxclist.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxclist.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxclist.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclist.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxclist.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxclist.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gxclist.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxclist.h:$(GLSRC)gxband.h
$(GLSRC)gxclist.h:$(GLSRC)gxblend.h
$(GLSRC)gxclist.h:$(GLSRC)gscolor2.h
$(GLSRC)gxclist.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxclist.h:$(GLSRC)gxdevice.h
$(GLSRC)gxclist.h:$(GLSRC)gxcpath.h
$(GLSRC)gxclist.h:$(GLSRC)gsht.h
$(GLSRC)gxclist.h:$(GLSRC)gsequivc.h
$(GLSRC)gxclist.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclist.h:$(GLSRC)gxpcache.h
$(GLSRC)gxclist.h:$(GLSRC)gscindex.h
$(GLSRC)gxclist.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclist.h:$(GLSRC)gsptype1.h
$(GLSRC)gxclist.h:$(GLSRC)gscie.h
$(GLSRC)gxclist.h:$(GLSRC)gxtext.h
$(GLSRC)gxclist.h:$(GLSRC)gstext.h
$(GLSRC)gxclist.h:$(GLSRC)gxstate.h
$(GLSRC)gxclist.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclist.h:$(GLSRC)gstparam.h
$(GLSRC)gxclist.h:$(GLSRC)gspcolor.h
$(GLSRC)gxclist.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclist.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxclist.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclist.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclist.h:$(GLSRC)gxctable.h
$(GLSRC)gxclist.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclist.h:$(GLSRC)gscsel.h
$(GLSRC)gxclist.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclist.h:$(GLSRC)gsfont.h
$(GLSRC)gxclist.h:$(GLSRC)gsimage.h
$(GLSRC)gxclist.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclist.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclist.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclist.h:$(GLSRC)gsropt.h
$(GLSRC)gxclist.h:$(GLSRC)gxdda.h
$(GLSRC)gxclist.h:$(GLSRC)gxpath.h
$(GLSRC)gxclist.h:$(GLSRC)gxiclass.h
$(GLSRC)gxclist.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclist.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclist.h:$(GLSRC)gxftype.h
$(GLSRC)gxclist.h:$(GLSRC)gscms.h
$(GLSRC)gxclist.h:$(GLSRC)gsrect.h
$(GLSRC)gxclist.h:$(GLSRC)gslparam.h
$(GLSRC)gxclist.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclist.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxclist.h:$(GLSRC)gscpm.h
$(GLSRC)gxclist.h:$(GLSRC)gscspace.h
$(GLSRC)gxclist.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclist.h:$(GLSRC)gxstdio.h
$(GLSRC)gxclist.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclist.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclist.h:$(GLSRC)gsio.h
$(GLSRC)gxclist.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclist.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclist.h:$(GLSRC)gxclio.h
$(GLSRC)gxclist.h:$(GLSRC)gscompt.h
$(GLSRC)gxclist.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclist.h:$(GLSRC)gspenum.h
$(GLSRC)gxclist.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclist.h:$(GLSRC)gsparam.h
$(GLSRC)gxclist.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclist.h:$(GLSRC)gp.h
$(GLSRC)gxclist.h:$(GLSRC)memento.h
$(GLSRC)gxclist.h:$(GLSRC)memory_.h
$(GLSRC)gxclist.h:$(GLSRC)gsuid.h
$(GLSRC)gxclist.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclist.h:$(GLSRC)gxsync.h
$(GLSRC)gxclist.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclist.h:$(GLSRC)vmsmath.h
$(GLSRC)gxclist.h:$(GLSRC)srdline.h
$(GLSRC)gxclist.h:$(GLSRC)scommon.h
$(GLSRC)gxclist.h:$(GLSRC)gsfname.h
$(GLSRC)gxclist.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclist.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclist.h:$(GLSRC)gxarith.h
$(GLSRC)gxclist.h:$(GLSRC)stat_.h
$(GLSRC)gxclist.h:$(GLSRC)gpsync.h
$(GLSRC)gxclist.h:$(GLSRC)gsstype.h
$(GLSRC)gxclist.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclist.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclist.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclist.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclist.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclist.h:$(GLSRC)stdio_.h
$(GLSRC)gxclist.h:$(GLSRC)gsccode.h
$(GLSRC)gxclist.h:$(GLSRC)stdint_.h
$(GLSRC)gxclist.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclist.h:$(GLSRC)gstypes.h
$(GLSRC)gxclist.h:$(GLSRC)std.h
$(GLSRC)gxclist.h:$(GLSRC)stdpre.h
$(GLSRC)gxclist.h:$(GLGEN)arch.h
$(GLSRC)gxclist.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcspace.h:$(GLSRC)gscsel.h
$(GLSRC)gxcspace.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcspace.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcspace.h:$(GLSRC)gscms.h
$(GLSRC)gxcspace.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcspace.h:$(GLSRC)gscspace.h
$(GLSRC)gxcspace.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcspace.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcspace.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcspace.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcspace.h:$(GLSRC)gsparam.h
$(GLSRC)gxcspace.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcspace.h:$(GLSRC)memento.h
$(GLSRC)gxcspace.h:$(GLSRC)gxsync.h
$(GLSRC)gxcspace.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcspace.h:$(GLSRC)scommon.h
$(GLSRC)gxcspace.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcspace.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcspace.h:$(GLSRC)gxarith.h
$(GLSRC)gxcspace.h:$(GLSRC)gpsync.h
$(GLSRC)gxcspace.h:$(GLSRC)gsstype.h
$(GLSRC)gxcspace.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcspace.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcspace.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcspace.h:$(GLSRC)stdio_.h
$(GLSRC)gxcspace.h:$(GLSRC)stdint_.h
$(GLSRC)gxcspace.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcspace.h:$(GLSRC)gstypes.h
$(GLSRC)gxcspace.h:$(GLSRC)std.h
$(GLSRC)gxcspace.h:$(GLSRC)stdpre.h
$(GLSRC)gxcspace.h:$(GLGEN)arch.h
$(GLSRC)gxcspace.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxht.h:$(GLSRC)gxhttype.h
$(GLSRC)gxht.h:$(GLSRC)gsht1.h
$(GLSRC)gxht.h:$(GLSRC)gsht.h
$(GLSRC)gxht.h:$(GLSRC)gxtmap.h
$(GLSRC)gxht.h:$(GLSRC)gscspace.h
$(GLSRC)gxht.h:$(GLSRC)gsgstate.h
$(GLSRC)gxht.h:$(GLSRC)gsiparam.h
$(GLSRC)gxht.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxht.h:$(GLSRC)gsrefct.h
$(GLSRC)gxht.h:$(GLSRC)memento.h
$(GLSRC)gxht.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxht.h:$(GLSRC)scommon.h
$(GLSRC)gxht.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxht.h:$(GLSRC)gsccolor.h
$(GLSRC)gxht.h:$(GLSRC)gsstype.h
$(GLSRC)gxht.h:$(GLSRC)gsmemory.h
$(GLSRC)gxht.h:$(GLSRC)gslibctx.h
$(GLSRC)gxht.h:$(GLSRC)stdio_.h
$(GLSRC)gxht.h:$(GLSRC)stdint_.h
$(GLSRC)gxht.h:$(GLSRC)gssprintf.h
$(GLSRC)gxht.h:$(GLSRC)gstypes.h
$(GLSRC)gxht.h:$(GLSRC)std.h
$(GLSRC)gxht.h:$(GLSRC)stdpre.h
$(GLSRC)gxht.h:$(GLGEN)arch.h
$(GLSRC)gxht.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcie.h:$(GLSRC)gscie.h
$(GLSRC)gxcie.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxcie.h:$(GLSRC)gxcspace.h
$(GLSRC)gxcie.h:$(GLSRC)gxctable.h
$(GLSRC)gxcie.h:$(GLSRC)gscsel.h
$(GLSRC)gxcie.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcie.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcie.h:$(GLSRC)gscms.h
$(GLSRC)gxcie.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcie.h:$(GLSRC)gscspace.h
$(GLSRC)gxcie.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcie.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcie.h:$(GLSRC)gxfixed.h
$(GLSRC)gxcie.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcie.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcie.h:$(GLSRC)gsparam.h
$(GLSRC)gxcie.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcie.h:$(GLSRC)memento.h
$(GLSRC)gxcie.h:$(GLSRC)gxsync.h
$(GLSRC)gxcie.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcie.h:$(GLSRC)scommon.h
$(GLSRC)gxcie.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcie.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcie.h:$(GLSRC)gxarith.h
$(GLSRC)gxcie.h:$(GLSRC)gpsync.h
$(GLSRC)gxcie.h:$(GLSRC)gsstype.h
$(GLSRC)gxcie.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcie.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcie.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcie.h:$(GLSRC)stdio_.h
$(GLSRC)gxcie.h:$(GLSRC)stdint_.h
$(GLSRC)gxcie.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcie.h:$(GLSRC)gstypes.h
$(GLSRC)gxcie.h:$(GLSRC)std.h
$(GLSRC)gxcie.h:$(GLSRC)stdpre.h
$(GLSRC)gxcie.h:$(GLGEN)arch.h
$(GLSRC)gxcie.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gxdda.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gxiclass.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsdevice.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsgstate.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsiparam.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gxfixed.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsparam.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxht_thresh.h:$(GLSRC)scommon.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsccolor.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsstype.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gsmemory.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gslibctx.h
$(GLSRC)gxht_thresh.h:$(GLSRC)stdio_.h
$(GLSRC)gxht_thresh.h:$(GLSRC)stdint_.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gssprintf.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gstypes.h
$(GLSRC)gxht_thresh.h:$(GLSRC)std.h
$(GLSRC)gxht_thresh.h:$(GLSRC)stdpre.h
$(GLSRC)gxht_thresh.h:$(GLGEN)arch.h
$(GLSRC)gxht_thresh.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxblend.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxdevice.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxcpath.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxpcache.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxcmap.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxtext.h
$(GLSRC)gxpcolor.h:$(GLSRC)gstext.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxpcolor.h:$(GLSRC)gstparam.h
$(GLSRC)gxpcolor.h:$(GLSRC)gspcolor.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxfmap.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsfunc.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxcspace.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxrplane.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscsel.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxfcache.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsfont.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsimage.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxbcache.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsropt.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxdda.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxpath.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxiclass.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxfrac.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxtmap.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxftype.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscms.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsrect.h
$(GLSRC)gxpcolor.h:$(GLSRC)gslparam.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsdevice.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscpm.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscspace.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsgstate.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxstdio.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsxfont.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsio.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsiparam.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxfixed.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscompt.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxpcolor.h:$(GLSRC)gspenum.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxhttile.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsparam.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsrefct.h
$(GLSRC)gxpcolor.h:$(GLSRC)gp.h
$(GLSRC)gxpcolor.h:$(GLSRC)memento.h
$(GLSRC)gxpcolor.h:$(GLSRC)memory_.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsuid.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsstruct.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxsync.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxpcolor.h:$(GLSRC)srdline.h
$(GLSRC)gxpcolor.h:$(GLSRC)scommon.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsfname.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsccolor.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxarith.h
$(GLSRC)gxpcolor.h:$(GLSRC)stat_.h
$(GLSRC)gxpcolor.h:$(GLSRC)gpsync.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsstype.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsmemory.h
$(GLSRC)gxpcolor.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxpcolor.h:$(GLSRC)gscdefs.h
$(GLSRC)gxpcolor.h:$(GLSRC)gslibctx.h
$(GLSRC)gxpcolor.h:$(GLSRC)gxcindex.h
$(GLSRC)gxpcolor.h:$(GLSRC)stdio_.h
$(GLSRC)gxpcolor.h:$(GLSRC)gsccode.h
$(GLSRC)gxpcolor.h:$(GLSRC)stdint_.h
$(GLSRC)gxpcolor.h:$(GLSRC)gssprintf.h
$(GLSRC)gxpcolor.h:$(GLSRC)gstypes.h
$(GLSRC)gxpcolor.h:$(GLSRC)std.h
$(GLSRC)gxpcolor.h:$(GLSRC)stdpre.h
$(GLSRC)gxpcolor.h:$(GLGEN)arch.h
$(GLSRC)gxpcolor.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscolor.h:$(GLSRC)gxtmap.h
$(GLSRC)gscolor.h:$(GLSRC)gsgstate.h
$(GLSRC)gscolor.h:$(GLSRC)stdpre.h
$(GLSRC)gsstate.h:$(GLSRC)gsovrc.h
$(GLSRC)gsstate.h:$(GLSRC)gscolor.h
$(GLSRC)gsstate.h:$(GLSRC)gsline.h
$(GLSRC)gsstate.h:$(GLSRC)gxcomp.h
$(GLSRC)gsstate.h:$(GLSRC)gsht.h
$(GLSRC)gsstate.h:$(GLSRC)gscsel.h
$(GLSRC)gsstate.h:$(GLSRC)gxtmap.h
$(GLSRC)gsstate.h:$(GLSRC)gslparam.h
$(GLSRC)gsstate.h:$(GLSRC)gsdevice.h
$(GLSRC)gsstate.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gsstate.h:$(GLSRC)gscpm.h
$(GLSRC)gsstate.h:$(GLSRC)gsgstate.h
$(GLSRC)gsstate.h:$(GLSRC)gscompt.h
$(GLSRC)gsstate.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsstate.h:$(GLSRC)gsparam.h
$(GLSRC)gsstate.h:$(GLSRC)gsrefct.h
$(GLSRC)gsstate.h:$(GLSRC)memento.h
$(GLSRC)gsstate.h:$(GLSRC)scommon.h
$(GLSRC)gsstate.h:$(GLSRC)gsstype.h
$(GLSRC)gsstate.h:$(GLSRC)gsmemory.h
$(GLSRC)gsstate.h:$(GLSRC)gslibctx.h
$(GLSRC)gsstate.h:$(GLSRC)gxcindex.h
$(GLSRC)gsstate.h:$(GLSRC)stdio_.h
$(GLSRC)gsstate.h:$(GLSRC)stdint_.h
$(GLSRC)gsstate.h:$(GLSRC)gssprintf.h
$(GLSRC)gsstate.h:$(GLSRC)gstypes.h
$(GLSRC)gsstate.h:$(GLSRC)std.h
$(GLSRC)gsstate.h:$(GLSRC)stdpre.h
$(GLSRC)gsstate.h:$(GLGEN)arch.h
$(GLSRC)gsstate.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc_create.h:$(GLSRC)gscie.h
$(GLSRC)gsicc_create.h:$(GLSRC)gxctable.h
$(GLSRC)gsicc_create.h:$(GLSRC)gxfrac.h
$(GLSRC)gsicc_create.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc_create.h:$(GLSRC)gxfixed.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc_create.h:$(GLSRC)memento.h
$(GLSRC)gsicc_create.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc_create.h:$(GLSRC)scommon.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc_create.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc_create.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc_create.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc_create.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc_create.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc_create.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc_create.h:$(GLSRC)std.h
$(GLSRC)gsicc_create.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc_create.h:$(GLGEN)arch.h
$(GLSRC)gsicc_create.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gximdecode.h:$(GLSRC)gximage.h
$(GLSRC)gximdecode.h:$(GLSRC)gxsample.h
$(GLSRC)gximdecode.h:$(GLSRC)sisparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gx.h
$(GLSRC)gximdecode.h:$(GLSRC)gdebug.h
$(GLSRC)gximdecode.h:$(GLSRC)gxcpath.h
$(GLSRC)gximdecode.h:$(GLSRC)gxiparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gxdevcli.h
$(GLSRC)gximdecode.h:$(GLSRC)gxcmap.h
$(GLSRC)gximdecode.h:$(GLSRC)gxtext.h
$(GLSRC)gximdecode.h:$(GLSRC)gstext.h
$(GLSRC)gximdecode.h:$(GLSRC)gsnamecl.h
$(GLSRC)gximdecode.h:$(GLSRC)gstparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gxfmap.h
$(GLSRC)gximdecode.h:$(GLSRC)gsfunc.h
$(GLSRC)gximdecode.h:$(GLSRC)gxcspace.h
$(GLSRC)gximdecode.h:$(GLSRC)strimpl.h
$(GLSRC)gximdecode.h:$(GLSRC)gxrplane.h
$(GLSRC)gximdecode.h:$(GLSRC)gscsel.h
$(GLSRC)gximdecode.h:$(GLSRC)gxfcache.h
$(GLSRC)gximdecode.h:$(GLSRC)gsfont.h
$(GLSRC)gximdecode.h:$(GLSRC)gsimage.h
$(GLSRC)gximdecode.h:$(GLSRC)gsdcolor.h
$(GLSRC)gximdecode.h:$(GLSRC)gxcvalue.h
$(GLSRC)gximdecode.h:$(GLSRC)gxbcache.h
$(GLSRC)gximdecode.h:$(GLSRC)gsropt.h
$(GLSRC)gximdecode.h:$(GLSRC)gxdda.h
$(GLSRC)gximdecode.h:$(GLSRC)gxpath.h
$(GLSRC)gximdecode.h:$(GLSRC)gxiclass.h
$(GLSRC)gximdecode.h:$(GLSRC)gxfrac.h
$(GLSRC)gximdecode.h:$(GLSRC)gxtmap.h
$(GLSRC)gximdecode.h:$(GLSRC)gxftype.h
$(GLSRC)gximdecode.h:$(GLSRC)gscms.h
$(GLSRC)gximdecode.h:$(GLSRC)gsrect.h
$(GLSRC)gximdecode.h:$(GLSRC)gslparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gsdevice.h
$(GLSRC)gximdecode.h:$(GLSRC)gscpm.h
$(GLSRC)gximdecode.h:$(GLSRC)gscspace.h
$(GLSRC)gximdecode.h:$(GLSRC)gsgstate.h
$(GLSRC)gximdecode.h:$(GLSRC)gsxfont.h
$(GLSRC)gximdecode.h:$(GLSRC)gsdsrc.h
$(GLSRC)gximdecode.h:$(GLSRC)gsio.h
$(GLSRC)gximdecode.h:$(GLSRC)gsiparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gxfixed.h
$(GLSRC)gximdecode.h:$(GLSRC)gscompt.h
$(GLSRC)gximdecode.h:$(GLSRC)gsmatrix.h
$(GLSRC)gximdecode.h:$(GLSRC)gspenum.h
$(GLSRC)gximdecode.h:$(GLSRC)gxhttile.h
$(GLSRC)gximdecode.h:$(GLSRC)gsparam.h
$(GLSRC)gximdecode.h:$(GLSRC)gsrefct.h
$(GLSRC)gximdecode.h:$(GLSRC)gp.h
$(GLSRC)gximdecode.h:$(GLSRC)memento.h
$(GLSRC)gximdecode.h:$(GLSRC)memory_.h
$(GLSRC)gximdecode.h:$(GLSRC)gsuid.h
$(GLSRC)gximdecode.h:$(GLSRC)gsstruct.h
$(GLSRC)gximdecode.h:$(GLSRC)gdbflags.h
$(GLSRC)gximdecode.h:$(GLSRC)gxsync.h
$(GLSRC)gximdecode.h:$(GLSRC)gserrors.h
$(GLSRC)gximdecode.h:$(GLSRC)gxbitmap.h
$(GLSRC)gximdecode.h:$(GLSRC)srdline.h
$(GLSRC)gximdecode.h:$(GLSRC)scommon.h
$(GLSRC)gximdecode.h:$(GLSRC)gsbitmap.h
$(GLSRC)gximdecode.h:$(GLSRC)gsccolor.h
$(GLSRC)gximdecode.h:$(GLSRC)gxarith.h
$(GLSRC)gximdecode.h:$(GLSRC)stat_.h
$(GLSRC)gximdecode.h:$(GLSRC)gpsync.h
$(GLSRC)gximdecode.h:$(GLSRC)gsstype.h
$(GLSRC)gximdecode.h:$(GLSRC)gsmemory.h
$(GLSRC)gximdecode.h:$(GLSRC)gpgetenv.h
$(GLSRC)gximdecode.h:$(GLSRC)gscdefs.h
$(GLSRC)gximdecode.h:$(GLSRC)gslibctx.h
$(GLSRC)gximdecode.h:$(GLSRC)gxcindex.h
$(GLSRC)gximdecode.h:$(GLSRC)stdio_.h
$(GLSRC)gximdecode.h:$(GLSRC)gsccode.h
$(GLSRC)gximdecode.h:$(GLSRC)stdint_.h
$(GLSRC)gximdecode.h:$(GLSRC)gssprintf.h
$(GLSRC)gximdecode.h:$(GLSRC)gstypes.h
$(GLSRC)gximdecode.h:$(GLSRC)std.h
$(GLSRC)gximdecode.h:$(GLSRC)stdpre.h
$(GLSRC)gximdecode.h:$(GLGEN)arch.h
$(GLSRC)gximdecode.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzacpath.h:$(GLSRC)gxcpath.h
$(GLSRC)gzacpath.h:$(GLSRC)gxdevcli.h
$(GLSRC)gzacpath.h:$(GLSRC)gxcmap.h
$(GLSRC)gzacpath.h:$(GLSRC)gxtext.h
$(GLSRC)gzacpath.h:$(GLSRC)gstext.h
$(GLSRC)gzacpath.h:$(GLSRC)gsnamecl.h
$(GLSRC)gzacpath.h:$(GLSRC)gstparam.h
$(GLSRC)gzacpath.h:$(GLSRC)gxfmap.h
$(GLSRC)gzacpath.h:$(GLSRC)gsfunc.h
$(GLSRC)gzacpath.h:$(GLSRC)gxcspace.h
$(GLSRC)gzacpath.h:$(GLSRC)gxrplane.h
$(GLSRC)gzacpath.h:$(GLSRC)gscsel.h
$(GLSRC)gzacpath.h:$(GLSRC)gxfcache.h
$(GLSRC)gzacpath.h:$(GLSRC)gsfont.h
$(GLSRC)gzacpath.h:$(GLSRC)gsimage.h
$(GLSRC)gzacpath.h:$(GLSRC)gsdcolor.h
$(GLSRC)gzacpath.h:$(GLSRC)gxcvalue.h
$(GLSRC)gzacpath.h:$(GLSRC)gxbcache.h
$(GLSRC)gzacpath.h:$(GLSRC)gsropt.h
$(GLSRC)gzacpath.h:$(GLSRC)gxdda.h
$(GLSRC)gzacpath.h:$(GLSRC)gxpath.h
$(GLSRC)gzacpath.h:$(GLSRC)gxfrac.h
$(GLSRC)gzacpath.h:$(GLSRC)gxtmap.h
$(GLSRC)gzacpath.h:$(GLSRC)gxftype.h
$(GLSRC)gzacpath.h:$(GLSRC)gscms.h
$(GLSRC)gzacpath.h:$(GLSRC)gsrect.h
$(GLSRC)gzacpath.h:$(GLSRC)gslparam.h
$(GLSRC)gzacpath.h:$(GLSRC)gsdevice.h
$(GLSRC)gzacpath.h:$(GLSRC)gscpm.h
$(GLSRC)gzacpath.h:$(GLSRC)gscspace.h
$(GLSRC)gzacpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gzacpath.h:$(GLSRC)gsxfont.h
$(GLSRC)gzacpath.h:$(GLSRC)gsdsrc.h
$(GLSRC)gzacpath.h:$(GLSRC)gsiparam.h
$(GLSRC)gzacpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gzacpath.h:$(GLSRC)gscompt.h
$(GLSRC)gzacpath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzacpath.h:$(GLSRC)gspenum.h
$(GLSRC)gzacpath.h:$(GLSRC)gxhttile.h
$(GLSRC)gzacpath.h:$(GLSRC)gsparam.h
$(GLSRC)gzacpath.h:$(GLSRC)gsrefct.h
$(GLSRC)gzacpath.h:$(GLSRC)gp.h
$(GLSRC)gzacpath.h:$(GLSRC)memento.h
$(GLSRC)gzacpath.h:$(GLSRC)memory_.h
$(GLSRC)gzacpath.h:$(GLSRC)gsuid.h
$(GLSRC)gzacpath.h:$(GLSRC)gsstruct.h
$(GLSRC)gzacpath.h:$(GLSRC)gxsync.h
$(GLSRC)gzacpath.h:$(GLSRC)gxbitmap.h
$(GLSRC)gzacpath.h:$(GLSRC)srdline.h
$(GLSRC)gzacpath.h:$(GLSRC)scommon.h
$(GLSRC)gzacpath.h:$(GLSRC)gsbitmap.h
$(GLSRC)gzacpath.h:$(GLSRC)gsccolor.h
$(GLSRC)gzacpath.h:$(GLSRC)gxarith.h
$(GLSRC)gzacpath.h:$(GLSRC)stat_.h
$(GLSRC)gzacpath.h:$(GLSRC)gpsync.h
$(GLSRC)gzacpath.h:$(GLSRC)gsstype.h
$(GLSRC)gzacpath.h:$(GLSRC)gsmemory.h
$(GLSRC)gzacpath.h:$(GLSRC)gpgetenv.h
$(GLSRC)gzacpath.h:$(GLSRC)gscdefs.h
$(GLSRC)gzacpath.h:$(GLSRC)gslibctx.h
$(GLSRC)gzacpath.h:$(GLSRC)gxcindex.h
$(GLSRC)gzacpath.h:$(GLSRC)stdio_.h
$(GLSRC)gzacpath.h:$(GLSRC)gsccode.h
$(GLSRC)gzacpath.h:$(GLSRC)stdint_.h
$(GLSRC)gzacpath.h:$(GLSRC)gssprintf.h
$(GLSRC)gzacpath.h:$(GLSRC)gstypes.h
$(GLSRC)gzacpath.h:$(GLSRC)std.h
$(GLSRC)gzacpath.h:$(GLSRC)stdpre.h
$(GLSRC)gzacpath.h:$(GLGEN)arch.h
$(GLSRC)gzacpath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzcpath.h:$(GLSRC)gzpath.h
$(GLSRC)gzcpath.h:$(GLSRC)gxcpath.h
$(GLSRC)gzcpath.h:$(GLSRC)gxdevcli.h
$(GLSRC)gzcpath.h:$(GLSRC)gxcmap.h
$(GLSRC)gzcpath.h:$(GLSRC)gxtext.h
$(GLSRC)gzcpath.h:$(GLSRC)gstext.h
$(GLSRC)gzcpath.h:$(GLSRC)gsnamecl.h
$(GLSRC)gzcpath.h:$(GLSRC)gstparam.h
$(GLSRC)gzcpath.h:$(GLSRC)gxfmap.h
$(GLSRC)gzcpath.h:$(GLSRC)gsfunc.h
$(GLSRC)gzcpath.h:$(GLSRC)gxcspace.h
$(GLSRC)gzcpath.h:$(GLSRC)gxrplane.h
$(GLSRC)gzcpath.h:$(GLSRC)gscsel.h
$(GLSRC)gzcpath.h:$(GLSRC)gxfcache.h
$(GLSRC)gzcpath.h:$(GLSRC)gsfont.h
$(GLSRC)gzcpath.h:$(GLSRC)gsimage.h
$(GLSRC)gzcpath.h:$(GLSRC)gsdcolor.h
$(GLSRC)gzcpath.h:$(GLSRC)gxcvalue.h
$(GLSRC)gzcpath.h:$(GLSRC)gxbcache.h
$(GLSRC)gzcpath.h:$(GLSRC)gsropt.h
$(GLSRC)gzcpath.h:$(GLSRC)gxdda.h
$(GLSRC)gzcpath.h:$(GLSRC)gxpath.h
$(GLSRC)gzcpath.h:$(GLSRC)gxfrac.h
$(GLSRC)gzcpath.h:$(GLSRC)gxtmap.h
$(GLSRC)gzcpath.h:$(GLSRC)gxftype.h
$(GLSRC)gzcpath.h:$(GLSRC)gscms.h
$(GLSRC)gzcpath.h:$(GLSRC)gsrect.h
$(GLSRC)gzcpath.h:$(GLSRC)gslparam.h
$(GLSRC)gzcpath.h:$(GLSRC)gsdevice.h
$(GLSRC)gzcpath.h:$(GLSRC)gscpm.h
$(GLSRC)gzcpath.h:$(GLSRC)gscspace.h
$(GLSRC)gzcpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gzcpath.h:$(GLSRC)gsxfont.h
$(GLSRC)gzcpath.h:$(GLSRC)gsdsrc.h
$(GLSRC)gzcpath.h:$(GLSRC)gsiparam.h
$(GLSRC)gzcpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gzcpath.h:$(GLSRC)gscompt.h
$(GLSRC)gzcpath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzcpath.h:$(GLSRC)gspenum.h
$(GLSRC)gzcpath.h:$(GLSRC)gxhttile.h
$(GLSRC)gzcpath.h:$(GLSRC)gsparam.h
$(GLSRC)gzcpath.h:$(GLSRC)gsrefct.h
$(GLSRC)gzcpath.h:$(GLSRC)gp.h
$(GLSRC)gzcpath.h:$(GLSRC)memento.h
$(GLSRC)gzcpath.h:$(GLSRC)memory_.h
$(GLSRC)gzcpath.h:$(GLSRC)gsuid.h
$(GLSRC)gzcpath.h:$(GLSRC)gsstruct.h
$(GLSRC)gzcpath.h:$(GLSRC)gxsync.h
$(GLSRC)gzcpath.h:$(GLSRC)gxbitmap.h
$(GLSRC)gzcpath.h:$(GLSRC)srdline.h
$(GLSRC)gzcpath.h:$(GLSRC)scommon.h
$(GLSRC)gzcpath.h:$(GLSRC)gsbitmap.h
$(GLSRC)gzcpath.h:$(GLSRC)gsccolor.h
$(GLSRC)gzcpath.h:$(GLSRC)gxarith.h
$(GLSRC)gzcpath.h:$(GLSRC)stat_.h
$(GLSRC)gzcpath.h:$(GLSRC)gpsync.h
$(GLSRC)gzcpath.h:$(GLSRC)gsstype.h
$(GLSRC)gzcpath.h:$(GLSRC)gsmemory.h
$(GLSRC)gzcpath.h:$(GLSRC)gpgetenv.h
$(GLSRC)gzcpath.h:$(GLSRC)gscdefs.h
$(GLSRC)gzcpath.h:$(GLSRC)gslibctx.h
$(GLSRC)gzcpath.h:$(GLSRC)gxcindex.h
$(GLSRC)gzcpath.h:$(GLSRC)stdio_.h
$(GLSRC)gzcpath.h:$(GLSRC)gsccode.h
$(GLSRC)gzcpath.h:$(GLSRC)stdint_.h
$(GLSRC)gzcpath.h:$(GLSRC)gssprintf.h
$(GLSRC)gzcpath.h:$(GLSRC)gstypes.h
$(GLSRC)gzcpath.h:$(GLSRC)std.h
$(GLSRC)gzcpath.h:$(GLSRC)stdpre.h
$(GLSRC)gzcpath.h:$(GLGEN)arch.h
$(GLSRC)gzcpath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzht.h:$(GLSRC)gxdht.h
$(GLSRC)gzht.h:$(GLSRC)gxht.h
$(GLSRC)gzht.h:$(GLSRC)gxhttype.h
$(GLSRC)gzht.h:$(GLSRC)gsht1.h
$(GLSRC)gzht.h:$(GLSRC)gsht.h
$(GLSRC)gzht.h:$(GLSRC)gxdevcli.h
$(GLSRC)gzht.h:$(GLSRC)gxcmap.h
$(GLSRC)gzht.h:$(GLSRC)gxtext.h
$(GLSRC)gzht.h:$(GLSRC)gstext.h
$(GLSRC)gzht.h:$(GLSRC)gsnamecl.h
$(GLSRC)gzht.h:$(GLSRC)gstparam.h
$(GLSRC)gzht.h:$(GLSRC)gxfmap.h
$(GLSRC)gzht.h:$(GLSRC)gsfunc.h
$(GLSRC)gzht.h:$(GLSRC)gxcspace.h
$(GLSRC)gzht.h:$(GLSRC)gxrplane.h
$(GLSRC)gzht.h:$(GLSRC)gscsel.h
$(GLSRC)gzht.h:$(GLSRC)gxfcache.h
$(GLSRC)gzht.h:$(GLSRC)gsfont.h
$(GLSRC)gzht.h:$(GLSRC)gsimage.h
$(GLSRC)gzht.h:$(GLSRC)gsdcolor.h
$(GLSRC)gzht.h:$(GLSRC)gxcvalue.h
$(GLSRC)gzht.h:$(GLSRC)gxbcache.h
$(GLSRC)gzht.h:$(GLSRC)gsropt.h
$(GLSRC)gzht.h:$(GLSRC)gxdda.h
$(GLSRC)gzht.h:$(GLSRC)gxpath.h
$(GLSRC)gzht.h:$(GLSRC)gxfrac.h
$(GLSRC)gzht.h:$(GLSRC)gxtmap.h
$(GLSRC)gzht.h:$(GLSRC)gxftype.h
$(GLSRC)gzht.h:$(GLSRC)gscms.h
$(GLSRC)gzht.h:$(GLSRC)gsrect.h
$(GLSRC)gzht.h:$(GLSRC)gslparam.h
$(GLSRC)gzht.h:$(GLSRC)gsdevice.h
$(GLSRC)gzht.h:$(GLSRC)gscpm.h
$(GLSRC)gzht.h:$(GLSRC)gscspace.h
$(GLSRC)gzht.h:$(GLSRC)gsgstate.h
$(GLSRC)gzht.h:$(GLSRC)gsxfont.h
$(GLSRC)gzht.h:$(GLSRC)gsdsrc.h
$(GLSRC)gzht.h:$(GLSRC)gsiparam.h
$(GLSRC)gzht.h:$(GLSRC)gxfixed.h
$(GLSRC)gzht.h:$(GLSRC)gscompt.h
$(GLSRC)gzht.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzht.h:$(GLSRC)gspenum.h
$(GLSRC)gzht.h:$(GLSRC)gxhttile.h
$(GLSRC)gzht.h:$(GLSRC)gsparam.h
$(GLSRC)gzht.h:$(GLSRC)gsrefct.h
$(GLSRC)gzht.h:$(GLSRC)gp.h
$(GLSRC)gzht.h:$(GLSRC)memento.h
$(GLSRC)gzht.h:$(GLSRC)memory_.h
$(GLSRC)gzht.h:$(GLSRC)gsuid.h
$(GLSRC)gzht.h:$(GLSRC)gsstruct.h
$(GLSRC)gzht.h:$(GLSRC)gxsync.h
$(GLSRC)gzht.h:$(GLSRC)gxbitmap.h
$(GLSRC)gzht.h:$(GLSRC)srdline.h
$(GLSRC)gzht.h:$(GLSRC)scommon.h
$(GLSRC)gzht.h:$(GLSRC)gsbitmap.h
$(GLSRC)gzht.h:$(GLSRC)gsccolor.h
$(GLSRC)gzht.h:$(GLSRC)gxarith.h
$(GLSRC)gzht.h:$(GLSRC)stat_.h
$(GLSRC)gzht.h:$(GLSRC)gpsync.h
$(GLSRC)gzht.h:$(GLSRC)gsstype.h
$(GLSRC)gzht.h:$(GLSRC)gsmemory.h
$(GLSRC)gzht.h:$(GLSRC)gpgetenv.h
$(GLSRC)gzht.h:$(GLSRC)gscdefs.h
$(GLSRC)gzht.h:$(GLSRC)gslibctx.h
$(GLSRC)gzht.h:$(GLSRC)gxcindex.h
$(GLSRC)gzht.h:$(GLSRC)stdio_.h
$(GLSRC)gzht.h:$(GLSRC)gsccode.h
$(GLSRC)gzht.h:$(GLSRC)stdint_.h
$(GLSRC)gzht.h:$(GLSRC)gssprintf.h
$(GLSRC)gzht.h:$(GLSRC)gstypes.h
$(GLSRC)gzht.h:$(GLSRC)std.h
$(GLSRC)gzht.h:$(GLSRC)stdpre.h
$(GLSRC)gzht.h:$(GLGEN)arch.h
$(GLSRC)gzht.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzline.h:$(GLSRC)gxline.h
$(GLSRC)gzline.h:$(GLSRC)math_.h
$(GLSRC)gzline.h:$(GLSRC)gslparam.h
$(GLSRC)gzline.h:$(GLSRC)gsgstate.h
$(GLSRC)gzline.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzline.h:$(GLSRC)vmsmath.h
$(GLSRC)gzline.h:$(GLSRC)scommon.h
$(GLSRC)gzline.h:$(GLSRC)gsstype.h
$(GLSRC)gzline.h:$(GLSRC)gsmemory.h
$(GLSRC)gzline.h:$(GLSRC)gslibctx.h
$(GLSRC)gzline.h:$(GLSRC)stdio_.h
$(GLSRC)gzline.h:$(GLSRC)stdint_.h
$(GLSRC)gzline.h:$(GLSRC)gssprintf.h
$(GLSRC)gzline.h:$(GLSRC)gstypes.h
$(GLSRC)gzline.h:$(GLSRC)std.h
$(GLSRC)gzline.h:$(GLSRC)stdpre.h
$(GLSRC)gzline.h:$(GLGEN)arch.h
$(GLSRC)gzline.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzpath.h:$(GLSRC)gxpath.h
$(GLSRC)gzpath.h:$(GLSRC)gsrect.h
$(GLSRC)gzpath.h:$(GLSRC)gslparam.h
$(GLSRC)gzpath.h:$(GLSRC)gscpm.h
$(GLSRC)gzpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gzpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gzpath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzpath.h:$(GLSRC)gspenum.h
$(GLSRC)gzpath.h:$(GLSRC)gsrefct.h
$(GLSRC)gzpath.h:$(GLSRC)memento.h
$(GLSRC)gzpath.h:$(GLSRC)scommon.h
$(GLSRC)gzpath.h:$(GLSRC)gsstype.h
$(GLSRC)gzpath.h:$(GLSRC)gsmemory.h
$(GLSRC)gzpath.h:$(GLSRC)gslibctx.h
$(GLSRC)gzpath.h:$(GLSRC)stdio_.h
$(GLSRC)gzpath.h:$(GLSRC)stdint_.h
$(GLSRC)gzpath.h:$(GLSRC)gssprintf.h
$(GLSRC)gzpath.h:$(GLSRC)gstypes.h
$(GLSRC)gzpath.h:$(GLSRC)std.h
$(GLSRC)gzpath.h:$(GLSRC)stdpre.h
$(GLSRC)gzpath.h:$(GLGEN)arch.h
$(GLSRC)gzpath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzstate.h:$(GLSRC)gsstate.h
$(GLSRC)gzstate.h:$(GLSRC)gsovrc.h
$(GLSRC)gzstate.h:$(GLSRC)gxgstate.h
$(GLSRC)gzstate.h:$(GLSRC)gstrans.h
$(GLSRC)gzstate.h:$(GLSRC)gdevp14.h
$(GLSRC)gzstate.h:$(GLSRC)gxline.h
$(GLSRC)gzstate.h:$(GLSRC)gscolor.h
$(GLSRC)gzstate.h:$(GLSRC)gsht1.h
$(GLSRC)gzstate.h:$(GLSRC)gsline.h
$(GLSRC)gzstate.h:$(GLSRC)gxcomp.h
$(GLSRC)gzstate.h:$(GLSRC)math_.h
$(GLSRC)gzstate.h:$(GLSRC)gxcolor2.h
$(GLSRC)gzstate.h:$(GLSRC)gxpcolor.h
$(GLSRC)gzstate.h:$(GLSRC)gxdevmem.h
$(GLSRC)gzstate.h:$(GLSRC)gdevdevn.h
$(GLSRC)gzstate.h:$(GLSRC)gxclipsr.h
$(GLSRC)gzstate.h:$(GLSRC)gxdcolor.h
$(GLSRC)gzstate.h:$(GLSRC)gxblend.h
$(GLSRC)gzstate.h:$(GLSRC)gscolor2.h
$(GLSRC)gzstate.h:$(GLSRC)gxmatrix.h
$(GLSRC)gzstate.h:$(GLSRC)gxdevice.h
$(GLSRC)gzstate.h:$(GLSRC)gxcpath.h
$(GLSRC)gzstate.h:$(GLSRC)gsht.h
$(GLSRC)gzstate.h:$(GLSRC)gsequivc.h
$(GLSRC)gzstate.h:$(GLSRC)gxdevcli.h
$(GLSRC)gzstate.h:$(GLSRC)gxpcache.h
$(GLSRC)gzstate.h:$(GLSRC)gscindex.h
$(GLSRC)gzstate.h:$(GLSRC)gxcmap.h
$(GLSRC)gzstate.h:$(GLSRC)gsptype1.h
$(GLSRC)gzstate.h:$(GLSRC)gscie.h
$(GLSRC)gzstate.h:$(GLSRC)gxtext.h
$(GLSRC)gzstate.h:$(GLSRC)gstext.h
$(GLSRC)gzstate.h:$(GLSRC)gxstate.h
$(GLSRC)gzstate.h:$(GLSRC)gsnamecl.h
$(GLSRC)gzstate.h:$(GLSRC)gstparam.h
$(GLSRC)gzstate.h:$(GLSRC)gspcolor.h
$(GLSRC)gzstate.h:$(GLSRC)gxfmap.h
$(GLSRC)gzstate.h:$(GLSRC)gsmalloc.h
$(GLSRC)gzstate.h:$(GLSRC)gsfunc.h
$(GLSRC)gzstate.h:$(GLSRC)gxcspace.h
$(GLSRC)gzstate.h:$(GLSRC)gxctable.h
$(GLSRC)gzstate.h:$(GLSRC)gxrplane.h
$(GLSRC)gzstate.h:$(GLSRC)gscsel.h
$(GLSRC)gzstate.h:$(GLSRC)gxfcache.h
$(GLSRC)gzstate.h:$(GLSRC)gsfont.h
$(GLSRC)gzstate.h:$(GLSRC)gsimage.h
$(GLSRC)gzstate.h:$(GLSRC)gsdcolor.h
$(GLSRC)gzstate.h:$(GLSRC)gxcvalue.h
$(GLSRC)gzstate.h:$(GLSRC)gxbcache.h
$(GLSRC)gzstate.h:$(GLSRC)gsropt.h
$(GLSRC)gzstate.h:$(GLSRC)gxdda.h
$(GLSRC)gzstate.h:$(GLSRC)gxpath.h
$(GLSRC)gzstate.h:$(GLSRC)gxiclass.h
$(GLSRC)gzstate.h:$(GLSRC)gxfrac.h
$(GLSRC)gzstate.h:$(GLSRC)gxtmap.h
$(GLSRC)gzstate.h:$(GLSRC)gxftype.h
$(GLSRC)gzstate.h:$(GLSRC)gscms.h
$(GLSRC)gzstate.h:$(GLSRC)gsrect.h
$(GLSRC)gzstate.h:$(GLSRC)gslparam.h
$(GLSRC)gzstate.h:$(GLSRC)gsdevice.h
$(GLSRC)gzstate.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gzstate.h:$(GLSRC)gscpm.h
$(GLSRC)gzstate.h:$(GLSRC)gscspace.h
$(GLSRC)gzstate.h:$(GLSRC)gsgstate.h
$(GLSRC)gzstate.h:$(GLSRC)gxstdio.h
$(GLSRC)gzstate.h:$(GLSRC)gsxfont.h
$(GLSRC)gzstate.h:$(GLSRC)gsdsrc.h
$(GLSRC)gzstate.h:$(GLSRC)gsio.h
$(GLSRC)gzstate.h:$(GLSRC)gsiparam.h
$(GLSRC)gzstate.h:$(GLSRC)gxfixed.h
$(GLSRC)gzstate.h:$(GLSRC)gscompt.h
$(GLSRC)gzstate.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzstate.h:$(GLSRC)gspenum.h
$(GLSRC)gzstate.h:$(GLSRC)gxhttile.h
$(GLSRC)gzstate.h:$(GLSRC)gsparam.h
$(GLSRC)gzstate.h:$(GLSRC)gsrefct.h
$(GLSRC)gzstate.h:$(GLSRC)gp.h
$(GLSRC)gzstate.h:$(GLSRC)memento.h
$(GLSRC)gzstate.h:$(GLSRC)memory_.h
$(GLSRC)gzstate.h:$(GLSRC)gsuid.h
$(GLSRC)gzstate.h:$(GLSRC)gsstruct.h
$(GLSRC)gzstate.h:$(GLSRC)gxsync.h
$(GLSRC)gzstate.h:$(GLSRC)gxbitmap.h
$(GLSRC)gzstate.h:$(GLSRC)vmsmath.h
$(GLSRC)gzstate.h:$(GLSRC)srdline.h
$(GLSRC)gzstate.h:$(GLSRC)scommon.h
$(GLSRC)gzstate.h:$(GLSRC)gsfname.h
$(GLSRC)gzstate.h:$(GLSRC)gsbitmap.h
$(GLSRC)gzstate.h:$(GLSRC)gsccolor.h
$(GLSRC)gzstate.h:$(GLSRC)gxarith.h
$(GLSRC)gzstate.h:$(GLSRC)stat_.h
$(GLSRC)gzstate.h:$(GLSRC)gpsync.h
$(GLSRC)gzstate.h:$(GLSRC)gsstype.h
$(GLSRC)gzstate.h:$(GLSRC)gsmemory.h
$(GLSRC)gzstate.h:$(GLSRC)gpgetenv.h
$(GLSRC)gzstate.h:$(GLSRC)gscdefs.h
$(GLSRC)gzstate.h:$(GLSRC)gslibctx.h
$(GLSRC)gzstate.h:$(GLSRC)gxcindex.h
$(GLSRC)gzstate.h:$(GLSRC)stdio_.h
$(GLSRC)gzstate.h:$(GLSRC)gsccode.h
$(GLSRC)gzstate.h:$(GLSRC)stdint_.h
$(GLSRC)gzstate.h:$(GLSRC)gssprintf.h
$(GLSRC)gzstate.h:$(GLSRC)gstypes.h
$(GLSRC)gzstate.h:$(GLSRC)std.h
$(GLSRC)gzstate.h:$(GLSRC)stdpre.h
$(GLSRC)gzstate.h:$(GLGEN)arch.h
$(GLSRC)gzstate.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxtext.h
$(GLSRC)gdevbbox.h:$(GLSRC)gstext.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevbbox.h:$(GLSRC)gstparam.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscsel.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsfont.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsimage.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsropt.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxdda.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxpath.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxftype.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscms.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsrect.h
$(GLSRC)gdevbbox.h:$(GLSRC)gslparam.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscpm.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscspace.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscompt.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevbbox.h:$(GLSRC)gspenum.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsparam.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevbbox.h:$(GLSRC)gp.h
$(GLSRC)gdevbbox.h:$(GLSRC)memento.h
$(GLSRC)gdevbbox.h:$(GLSRC)memory_.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsuid.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxsync.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevbbox.h:$(GLSRC)srdline.h
$(GLSRC)gdevbbox.h:$(GLSRC)scommon.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxarith.h
$(GLSRC)gdevbbox.h:$(GLSRC)stat_.h
$(GLSRC)gdevbbox.h:$(GLSRC)gpsync.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsstype.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevbbox.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevbbox.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevbbox.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevbbox.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevbbox.h:$(GLSRC)stdio_.h
$(GLSRC)gdevbbox.h:$(GLSRC)gsccode.h
$(GLSRC)gdevbbox.h:$(GLSRC)stdint_.h
$(GLSRC)gdevbbox.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevbbox.h:$(GLSRC)gstypes.h
$(GLSRC)gdevbbox.h:$(GLSRC)std.h
$(GLSRC)gdevbbox.h:$(GLSRC)stdpre.h
$(GLSRC)gdevbbox.h:$(GLGEN)arch.h
$(GLSRC)gdevbbox.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevmem.h:$(GLSRC)gxbitops.h
$(GLSRC)gdevmem.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevmem.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevmem.h:$(GLSRC)gxtext.h
$(GLSRC)gdevmem.h:$(GLSRC)gstext.h
$(GLSRC)gdevmem.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevmem.h:$(GLSRC)gstparam.h
$(GLSRC)gdevmem.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevmem.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevmem.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevmem.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevmem.h:$(GLSRC)gscsel.h
$(GLSRC)gdevmem.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevmem.h:$(GLSRC)gsfont.h
$(GLSRC)gdevmem.h:$(GLSRC)gsimage.h
$(GLSRC)gdevmem.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevmem.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevmem.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevmem.h:$(GLSRC)gsropt.h
$(GLSRC)gdevmem.h:$(GLSRC)gxdda.h
$(GLSRC)gdevmem.h:$(GLSRC)gxpath.h
$(GLSRC)gdevmem.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevmem.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevmem.h:$(GLSRC)gxftype.h
$(GLSRC)gdevmem.h:$(GLSRC)gscms.h
$(GLSRC)gdevmem.h:$(GLSRC)gsrect.h
$(GLSRC)gdevmem.h:$(GLSRC)gslparam.h
$(GLSRC)gdevmem.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevmem.h:$(GLSRC)gscpm.h
$(GLSRC)gdevmem.h:$(GLSRC)gscspace.h
$(GLSRC)gdevmem.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevmem.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevmem.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevmem.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevmem.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevmem.h:$(GLSRC)gscompt.h
$(GLSRC)gdevmem.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevmem.h:$(GLSRC)gspenum.h
$(GLSRC)gdevmem.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevmem.h:$(GLSRC)gsparam.h
$(GLSRC)gdevmem.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevmem.h:$(GLSRC)gp.h
$(GLSRC)gdevmem.h:$(GLSRC)memento.h
$(GLSRC)gdevmem.h:$(GLSRC)memory_.h
$(GLSRC)gdevmem.h:$(GLSRC)gsuid.h
$(GLSRC)gdevmem.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevmem.h:$(GLSRC)gxsync.h
$(GLSRC)gdevmem.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevmem.h:$(GLSRC)srdline.h
$(GLSRC)gdevmem.h:$(GLSRC)scommon.h
$(GLSRC)gdevmem.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevmem.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevmem.h:$(GLSRC)gxarith.h
$(GLSRC)gdevmem.h:$(GLSRC)stat_.h
$(GLSRC)gdevmem.h:$(GLSRC)gpsync.h
$(GLSRC)gdevmem.h:$(GLSRC)gsstype.h
$(GLSRC)gdevmem.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevmem.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevmem.h:$(GLSRC)gsbitops.h
$(GLSRC)gdevmem.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevmem.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevmem.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevmem.h:$(GLSRC)stdio_.h
$(GLSRC)gdevmem.h:$(GLSRC)gsccode.h
$(GLSRC)gdevmem.h:$(GLSRC)stdint_.h
$(GLSRC)gdevmem.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevmem.h:$(GLSRC)gstypes.h
$(GLSRC)gdevmem.h:$(GLSRC)std.h
$(GLSRC)gdevmem.h:$(GLSRC)stdpre.h
$(GLSRC)gdevmem.h:$(GLGEN)arch.h
$(GLSRC)gdevmem.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevmpla.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsparam.h
$(GLSRC)gdevmpla.h:$(GLSRC)scommon.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsstype.h
$(GLSRC)gdevmpla.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevmpla.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevmpla.h:$(GLSRC)stdio_.h
$(GLSRC)gdevmpla.h:$(GLSRC)stdint_.h
$(GLSRC)gdevmpla.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevmpla.h:$(GLSRC)gstypes.h
$(GLSRC)gdevmpla.h:$(GLSRC)std.h
$(GLSRC)gdevmpla.h:$(GLSRC)stdpre.h
$(GLSRC)gdevmpla.h:$(GLGEN)arch.h
$(GLSRC)gdevmpla.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevmrop.h:$(GLSRC)gximage.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxsample.h
$(GLSRC)gdevmrop.h:$(GLSRC)sisparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxcpath.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxiparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxtext.h
$(GLSRC)gdevmrop.h:$(GLSRC)gstext.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevmrop.h:$(GLSRC)gstparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevmrop.h:$(GLSRC)strimpl.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscsel.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsfont.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsimage.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsropt.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxdda.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxpath.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxiclass.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxftype.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscms.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsrect.h
$(GLSRC)gdevmrop.h:$(GLSRC)gslparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscpm.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscspace.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscompt.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevmrop.h:$(GLSRC)gspenum.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsparam.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevmrop.h:$(GLSRC)gp.h
$(GLSRC)gdevmrop.h:$(GLSRC)memento.h
$(GLSRC)gdevmrop.h:$(GLSRC)memory_.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsuid.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxsync.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevmrop.h:$(GLSRC)srdline.h
$(GLSRC)gdevmrop.h:$(GLSRC)scommon.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxarith.h
$(GLSRC)gdevmrop.h:$(GLSRC)stat_.h
$(GLSRC)gdevmrop.h:$(GLSRC)gpsync.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsstype.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevmrop.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevmrop.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevmrop.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevmrop.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevmrop.h:$(GLSRC)stdio_.h
$(GLSRC)gdevmrop.h:$(GLSRC)gsccode.h
$(GLSRC)gdevmrop.h:$(GLSRC)stdint_.h
$(GLSRC)gdevmrop.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevmrop.h:$(GLSRC)gstypes.h
$(GLSRC)gdevmrop.h:$(GLSRC)std.h
$(GLSRC)gdevmrop.h:$(GLSRC)stdpre.h
$(GLSRC)gdevmrop.h:$(GLGEN)arch.h
$(GLSRC)gdevmrop.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxdevmem.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxtext.h
$(GLSRC)gdevmrun.h:$(GLSRC)gstext.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevmrun.h:$(GLSRC)gstparam.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscsel.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsfont.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsimage.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsropt.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxdda.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxpath.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxftype.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscms.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsrect.h
$(GLSRC)gdevmrun.h:$(GLSRC)gslparam.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscpm.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscspace.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscompt.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevmrun.h:$(GLSRC)gspenum.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsparam.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevmrun.h:$(GLSRC)gp.h
$(GLSRC)gdevmrun.h:$(GLSRC)memento.h
$(GLSRC)gdevmrun.h:$(GLSRC)memory_.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsuid.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxsync.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevmrun.h:$(GLSRC)srdline.h
$(GLSRC)gdevmrun.h:$(GLSRC)scommon.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxarith.h
$(GLSRC)gdevmrun.h:$(GLSRC)stat_.h
$(GLSRC)gdevmrun.h:$(GLSRC)gpsync.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsstype.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevmrun.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevmrun.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevmrun.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevmrun.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevmrun.h:$(GLSRC)stdio_.h
$(GLSRC)gdevmrun.h:$(GLSRC)gsccode.h
$(GLSRC)gdevmrun.h:$(GLSRC)stdint_.h
$(GLSRC)gdevmrun.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevmrun.h:$(GLSRC)gstypes.h
$(GLSRC)gdevmrun.h:$(GLSRC)std.h
$(GLSRC)gdevmrun.h:$(GLSRC)stdpre.h
$(GLSRC)gdevmrun.h:$(GLGEN)arch.h
$(GLSRC)gdevmrun.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxtext.h
$(GLSRC)gdevplnx.h:$(GLSRC)gstext.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevplnx.h:$(GLSRC)gstparam.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscsel.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsfont.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsimage.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsropt.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxdda.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxpath.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxftype.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscms.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsrect.h
$(GLSRC)gdevplnx.h:$(GLSRC)gslparam.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscpm.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscspace.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscompt.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevplnx.h:$(GLSRC)gspenum.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsparam.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevplnx.h:$(GLSRC)gp.h
$(GLSRC)gdevplnx.h:$(GLSRC)memento.h
$(GLSRC)gdevplnx.h:$(GLSRC)memory_.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsuid.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxsync.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevplnx.h:$(GLSRC)srdline.h
$(GLSRC)gdevplnx.h:$(GLSRC)scommon.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxarith.h
$(GLSRC)gdevplnx.h:$(GLSRC)stat_.h
$(GLSRC)gdevplnx.h:$(GLSRC)gpsync.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsstype.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevplnx.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevplnx.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevplnx.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevplnx.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevplnx.h:$(GLSRC)stdio_.h
$(GLSRC)gdevplnx.h:$(GLSRC)gsccode.h
$(GLSRC)gdevplnx.h:$(GLSRC)stdint_.h
$(GLSRC)gdevplnx.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevplnx.h:$(GLSRC)gstypes.h
$(GLSRC)gdevplnx.h:$(GLSRC)std.h
$(GLSRC)gdevplnx.h:$(GLSRC)stdpre.h
$(GLSRC)gdevplnx.h:$(GLGEN)arch.h
$(GLSRC)gdevplnx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevepo.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevepo.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevepo.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevepo.h:$(GLSRC)gxtext.h
$(GLSRC)gdevepo.h:$(GLSRC)gstext.h
$(GLSRC)gdevepo.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevepo.h:$(GLSRC)gstparam.h
$(GLSRC)gdevepo.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevepo.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevepo.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevepo.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevepo.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevepo.h:$(GLSRC)gscsel.h
$(GLSRC)gdevepo.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevepo.h:$(GLSRC)gsfont.h
$(GLSRC)gdevepo.h:$(GLSRC)gsimage.h
$(GLSRC)gdevepo.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevepo.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevepo.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevepo.h:$(GLSRC)gsropt.h
$(GLSRC)gdevepo.h:$(GLSRC)gxdda.h
$(GLSRC)gdevepo.h:$(GLSRC)gxpath.h
$(GLSRC)gdevepo.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevepo.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevepo.h:$(GLSRC)gxftype.h
$(GLSRC)gdevepo.h:$(GLSRC)gscms.h
$(GLSRC)gdevepo.h:$(GLSRC)gsrect.h
$(GLSRC)gdevepo.h:$(GLSRC)gslparam.h
$(GLSRC)gdevepo.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevepo.h:$(GLSRC)gscpm.h
$(GLSRC)gdevepo.h:$(GLSRC)gscspace.h
$(GLSRC)gdevepo.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevepo.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevepo.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevepo.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevepo.h:$(GLSRC)gsio.h
$(GLSRC)gdevepo.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevepo.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevepo.h:$(GLSRC)gscompt.h
$(GLSRC)gdevepo.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevepo.h:$(GLSRC)gspenum.h
$(GLSRC)gdevepo.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevepo.h:$(GLSRC)gsparam.h
$(GLSRC)gdevepo.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevepo.h:$(GLSRC)gp.h
$(GLSRC)gdevepo.h:$(GLSRC)memento.h
$(GLSRC)gdevepo.h:$(GLSRC)memory_.h
$(GLSRC)gdevepo.h:$(GLSRC)gsuid.h
$(GLSRC)gdevepo.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevepo.h:$(GLSRC)gxsync.h
$(GLSRC)gdevepo.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevepo.h:$(GLSRC)srdline.h
$(GLSRC)gdevepo.h:$(GLSRC)scommon.h
$(GLSRC)gdevepo.h:$(GLSRC)gsfname.h
$(GLSRC)gdevepo.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevepo.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevepo.h:$(GLSRC)gxarith.h
$(GLSRC)gdevepo.h:$(GLSRC)stat_.h
$(GLSRC)gdevepo.h:$(GLSRC)gpsync.h
$(GLSRC)gdevepo.h:$(GLSRC)gsstype.h
$(GLSRC)gdevepo.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevepo.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevepo.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevepo.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevepo.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevepo.h:$(GLSRC)stdio_.h
$(GLSRC)gdevepo.h:$(GLSRC)gsccode.h
$(GLSRC)gdevepo.h:$(GLSRC)stdint_.h
$(GLSRC)gdevepo.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevepo.h:$(GLSRC)gstypes.h
$(GLSRC)gdevepo.h:$(GLSRC)std.h
$(GLSRC)gdevepo.h:$(GLSRC)stdpre.h
$(GLSRC)gdevepo.h:$(GLGEN)arch.h
$(GLSRC)gdevepo.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sa85d.h:$(GLSRC)scommon.h
$(GLSRC)sa85d.h:$(GLSRC)gsstype.h
$(GLSRC)sa85d.h:$(GLSRC)gsmemory.h
$(GLSRC)sa85d.h:$(GLSRC)gslibctx.h
$(GLSRC)sa85d.h:$(GLSRC)stdio_.h
$(GLSRC)sa85d.h:$(GLSRC)stdint_.h
$(GLSRC)sa85d.h:$(GLSRC)gssprintf.h
$(GLSRC)sa85d.h:$(GLSRC)gstypes.h
$(GLSRC)sa85d.h:$(GLSRC)std.h
$(GLSRC)sa85d.h:$(GLSRC)stdpre.h
$(GLSRC)sa85d.h:$(GLGEN)arch.h
$(GLSRC)sa85d.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sa85x.h:$(GLSRC)sa85d.h
$(GLSRC)sa85x.h:$(GLSRC)scommon.h
$(GLSRC)sa85x.h:$(GLSRC)gsstype.h
$(GLSRC)sa85x.h:$(GLSRC)gsmemory.h
$(GLSRC)sa85x.h:$(GLSRC)gslibctx.h
$(GLSRC)sa85x.h:$(GLSRC)stdio_.h
$(GLSRC)sa85x.h:$(GLSRC)stdint_.h
$(GLSRC)sa85x.h:$(GLSRC)gssprintf.h
$(GLSRC)sa85x.h:$(GLSRC)gstypes.h
$(GLSRC)sa85x.h:$(GLSRC)std.h
$(GLSRC)sa85x.h:$(GLSRC)stdpre.h
$(GLSRC)sa85x.h:$(GLGEN)arch.h
$(GLSRC)sa85x.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sbcp.h:$(GLSRC)scommon.h
$(GLSRC)sbcp.h:$(GLSRC)gsstype.h
$(GLSRC)sbcp.h:$(GLSRC)gsmemory.h
$(GLSRC)sbcp.h:$(GLSRC)gslibctx.h
$(GLSRC)sbcp.h:$(GLSRC)stdio_.h
$(GLSRC)sbcp.h:$(GLSRC)stdint_.h
$(GLSRC)sbcp.h:$(GLSRC)gssprintf.h
$(GLSRC)sbcp.h:$(GLSRC)gstypes.h
$(GLSRC)sbcp.h:$(GLSRC)std.h
$(GLSRC)sbcp.h:$(GLSRC)stdpre.h
$(GLSRC)sbcp.h:$(GLGEN)arch.h
$(GLSRC)sbcp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sbtx.h:$(GLSRC)scommon.h
$(GLSRC)sbtx.h:$(GLSRC)gsstype.h
$(GLSRC)sbtx.h:$(GLSRC)gsmemory.h
$(GLSRC)sbtx.h:$(GLSRC)gslibctx.h
$(GLSRC)sbtx.h:$(GLSRC)stdio_.h
$(GLSRC)sbtx.h:$(GLSRC)stdint_.h
$(GLSRC)sbtx.h:$(GLSRC)gssprintf.h
$(GLSRC)sbtx.h:$(GLSRC)gstypes.h
$(GLSRC)sbtx.h:$(GLSRC)std.h
$(GLSRC)sbtx.h:$(GLSRC)stdpre.h
$(GLSRC)sbtx.h:$(GLGEN)arch.h
$(GLSRC)sbtx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)scanchar.h:$(GLSRC)scommon.h
$(GLSRC)scanchar.h:$(GLSRC)gsstype.h
$(GLSRC)scanchar.h:$(GLSRC)gsmemory.h
$(GLSRC)scanchar.h:$(GLSRC)gslibctx.h
$(GLSRC)scanchar.h:$(GLSRC)stdio_.h
$(GLSRC)scanchar.h:$(GLSRC)stdint_.h
$(GLSRC)scanchar.h:$(GLSRC)gssprintf.h
$(GLSRC)scanchar.h:$(GLSRC)gstypes.h
$(GLSRC)scanchar.h:$(GLSRC)std.h
$(GLSRC)scanchar.h:$(GLSRC)stdpre.h
$(GLSRC)scanchar.h:$(GLGEN)arch.h
$(GLSRC)scanchar.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sfilter.h:$(GLSRC)scommon.h
$(GLSRC)sfilter.h:$(GLSRC)gsstype.h
$(GLSRC)sfilter.h:$(GLSRC)gsmemory.h
$(GLSRC)sfilter.h:$(GLSRC)gslibctx.h
$(GLSRC)sfilter.h:$(GLSRC)stdio_.h
$(GLSRC)sfilter.h:$(GLSRC)stdint_.h
$(GLSRC)sfilter.h:$(GLSRC)gssprintf.h
$(GLSRC)sfilter.h:$(GLSRC)gstypes.h
$(GLSRC)sfilter.h:$(GLSRC)std.h
$(GLSRC)sfilter.h:$(GLSRC)stdpre.h
$(GLSRC)sfilter.h:$(GLGEN)arch.h
$(GLSRC)sfilter.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sdct.h:$(GLSRC)setjmp_.h
$(GLSRC)sdct.h:$(GLSRC)strimpl.h
$(GLSRC)sdct.h:$(GLSRC)gscms.h
$(GLSRC)sdct.h:$(GLSRC)gsdevice.h
$(GLSRC)sdct.h:$(GLSRC)gscspace.h
$(GLSRC)sdct.h:$(GLSRC)gsgstate.h
$(GLSRC)sdct.h:$(GLSRC)gsiparam.h
$(GLSRC)sdct.h:$(GLSRC)gsmatrix.h
$(GLSRC)sdct.h:$(GLSRC)gsparam.h
$(GLSRC)sdct.h:$(GLSRC)gsrefct.h
$(GLSRC)sdct.h:$(GLSRC)memento.h
$(GLSRC)sdct.h:$(GLSRC)gsstruct.h
$(GLSRC)sdct.h:$(GLSRC)gxsync.h
$(GLSRC)sdct.h:$(GLSRC)gxbitmap.h
$(GLSRC)sdct.h:$(GLSRC)scommon.h
$(GLSRC)sdct.h:$(GLSRC)gsbitmap.h
$(GLSRC)sdct.h:$(GLSRC)gsccolor.h
$(GLSRC)sdct.h:$(GLSRC)gpsync.h
$(GLSRC)sdct.h:$(GLSRC)gsstype.h
$(GLSRC)sdct.h:$(GLSRC)gsmemory.h
$(GLSRC)sdct.h:$(GLSRC)gslibctx.h
$(GLSRC)sdct.h:$(GLSRC)stdio_.h
$(GLSRC)sdct.h:$(GLSRC)stdint_.h
$(GLSRC)sdct.h:$(GLSRC)gssprintf.h
$(GLSRC)sdct.h:$(GLSRC)gstypes.h
$(GLSRC)sdct.h:$(GLSRC)std.h
$(GLSRC)sdct.h:$(GLSRC)stdpre.h
$(GLSRC)sdct.h:$(GLGEN)arch.h
$(GLSRC)sdct.h:$(GLSRC)gs_dll_call.h
$(GLSRC)shc.h:$(GLSRC)scommon.h
$(GLSRC)shc.h:$(GLSRC)gsstype.h
$(GLSRC)shc.h:$(GLSRC)gsmemory.h
$(GLSRC)shc.h:$(GLSRC)gslibctx.h
$(GLSRC)shc.h:$(GLSRC)stdio_.h
$(GLSRC)shc.h:$(GLSRC)stdint_.h
$(GLSRC)shc.h:$(GLSRC)gssprintf.h
$(GLSRC)shc.h:$(GLSRC)gsbittab.h
$(GLSRC)shc.h:$(GLSRC)gstypes.h
$(GLSRC)shc.h:$(GLSRC)std.h
$(GLSRC)shc.h:$(GLSRC)stdpre.h
$(GLSRC)shc.h:$(GLGEN)arch.h
$(GLSRC)shc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sisparam.h:$(GLSRC)gxdda.h
$(GLSRC)sisparam.h:$(GLSRC)gxfixed.h
$(GLSRC)sisparam.h:$(GLSRC)scommon.h
$(GLSRC)sisparam.h:$(GLSRC)gsstype.h
$(GLSRC)sisparam.h:$(GLSRC)gsmemory.h
$(GLSRC)sisparam.h:$(GLSRC)gslibctx.h
$(GLSRC)sisparam.h:$(GLSRC)stdio_.h
$(GLSRC)sisparam.h:$(GLSRC)stdint_.h
$(GLSRC)sisparam.h:$(GLSRC)gssprintf.h
$(GLSRC)sisparam.h:$(GLSRC)gstypes.h
$(GLSRC)sisparam.h:$(GLSRC)std.h
$(GLSRC)sisparam.h:$(GLSRC)stdpre.h
$(GLSRC)sisparam.h:$(GLGEN)arch.h
$(GLSRC)sisparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sjpeg.h:$(GLSRC)sdct.h
$(GLSRC)sjpeg.h:$(GLSRC)setjmp_.h
$(GLSRC)sjpeg.h:$(GLSRC)strimpl.h
$(GLSRC)sjpeg.h:$(GLSRC)gscms.h
$(GLSRC)sjpeg.h:$(GLSRC)gsdevice.h
$(GLSRC)sjpeg.h:$(GLSRC)gscspace.h
$(GLSRC)sjpeg.h:$(GLSRC)gsgstate.h
$(GLSRC)sjpeg.h:$(GLSRC)gsiparam.h
$(GLSRC)sjpeg.h:$(GLSRC)gsmatrix.h
$(GLSRC)sjpeg.h:$(GLSRC)gsparam.h
$(GLSRC)sjpeg.h:$(GLSRC)gsrefct.h
$(GLSRC)sjpeg.h:$(GLSRC)memento.h
$(GLSRC)sjpeg.h:$(GLSRC)gsstruct.h
$(GLSRC)sjpeg.h:$(GLSRC)gxsync.h
$(GLSRC)sjpeg.h:$(GLSRC)gxbitmap.h
$(GLSRC)sjpeg.h:$(GLSRC)scommon.h
$(GLSRC)sjpeg.h:$(GLSRC)gsbitmap.h
$(GLSRC)sjpeg.h:$(GLSRC)gsccolor.h
$(GLSRC)sjpeg.h:$(GLSRC)gpsync.h
$(GLSRC)sjpeg.h:$(GLSRC)gsstype.h
$(GLSRC)sjpeg.h:$(GLSRC)gsmemory.h
$(GLSRC)sjpeg.h:$(GLSRC)gslibctx.h
$(GLSRC)sjpeg.h:$(GLSRC)stdio_.h
$(GLSRC)sjpeg.h:$(GLSRC)stdint_.h
$(GLSRC)sjpeg.h:$(GLSRC)gssprintf.h
$(GLSRC)sjpeg.h:$(GLSRC)gstypes.h
$(GLSRC)sjpeg.h:$(GLSRC)std.h
$(GLSRC)sjpeg.h:$(GLSRC)stdpre.h
$(GLSRC)sjpeg.h:$(GLGEN)arch.h
$(GLSRC)sjpeg.h:$(GLSRC)gs_dll_call.h
$(GLSRC)slzwx.h:$(GLSRC)scommon.h
$(GLSRC)slzwx.h:$(GLSRC)gsstype.h
$(GLSRC)slzwx.h:$(GLSRC)gsmemory.h
$(GLSRC)slzwx.h:$(GLSRC)gslibctx.h
$(GLSRC)slzwx.h:$(GLSRC)stdio_.h
$(GLSRC)slzwx.h:$(GLSRC)stdint_.h
$(GLSRC)slzwx.h:$(GLSRC)gssprintf.h
$(GLSRC)slzwx.h:$(GLSRC)gstypes.h
$(GLSRC)slzwx.h:$(GLSRC)std.h
$(GLSRC)slzwx.h:$(GLSRC)stdpre.h
$(GLSRC)slzwx.h:$(GLGEN)arch.h
$(GLSRC)slzwx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)smd5.h:$(GLSRC)gsmd5.h
$(GLSRC)smd5.h:$(GLSRC)memory_.h
$(GLSRC)smd5.h:$(GLSRC)scommon.h
$(GLSRC)smd5.h:$(GLSRC)gsstype.h
$(GLSRC)smd5.h:$(GLSRC)gsmemory.h
$(GLSRC)smd5.h:$(GLSRC)gslibctx.h
$(GLSRC)smd5.h:$(GLSRC)stdio_.h
$(GLSRC)smd5.h:$(GLSRC)stdint_.h
$(GLSRC)smd5.h:$(GLSRC)gssprintf.h
$(GLSRC)smd5.h:$(GLSRC)gstypes.h
$(GLSRC)smd5.h:$(GLSRC)std.h
$(GLSRC)smd5.h:$(GLSRC)stdpre.h
$(GLSRC)smd5.h:$(GLGEN)arch.h
$(GLSRC)smd5.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sarc4.h:$(GLSRC)scommon.h
$(GLSRC)sarc4.h:$(GLSRC)gsstype.h
$(GLSRC)sarc4.h:$(GLSRC)gsmemory.h
$(GLSRC)sarc4.h:$(GLSRC)gslibctx.h
$(GLSRC)sarc4.h:$(GLSRC)stdio_.h
$(GLSRC)sarc4.h:$(GLSRC)stdint_.h
$(GLSRC)sarc4.h:$(GLSRC)gssprintf.h
$(GLSRC)sarc4.h:$(GLSRC)gstypes.h
$(GLSRC)sarc4.h:$(GLSRC)std.h
$(GLSRC)sarc4.h:$(GLSRC)stdpre.h
$(GLSRC)sarc4.h:$(GLGEN)arch.h
$(GLSRC)sarc4.h:$(GLSRC)gs_dll_call.h
$(GLSRC)saes.h:$(GLSRC)aes.h
$(GLSRC)saes.h:$(GLSRC)scommon.h
$(GLSRC)saes.h:$(GLSRC)gsstype.h
$(GLSRC)saes.h:$(GLSRC)gsmemory.h
$(GLSRC)saes.h:$(GLSRC)gslibctx.h
$(GLSRC)saes.h:$(GLSRC)stdio_.h
$(GLSRC)saes.h:$(GLSRC)stdint_.h
$(GLSRC)saes.h:$(GLSRC)gssprintf.h
$(GLSRC)saes.h:$(GLSRC)gstypes.h
$(GLSRC)saes.h:$(GLSRC)std.h
$(GLSRC)saes.h:$(GLSRC)stdpre.h
$(GLSRC)saes.h:$(GLGEN)arch.h
$(GLSRC)saes.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sjbig2.h:$(GLSRC)scommon.h
$(GLSRC)sjbig2.h:$(GLSRC)gsstype.h
$(GLSRC)sjbig2.h:$(GLSRC)gsmemory.h
$(GLSRC)sjbig2.h:$(GLSRC)gslibctx.h
$(GLSRC)sjbig2.h:$(GLSRC)stdio_.h
$(GLSRC)sjbig2.h:$(GLSRC)stdint_.h
$(GLSRC)sjbig2.h:$(GLSRC)gssprintf.h
$(GLSRC)sjbig2.h:$(GLSRC)gstypes.h
$(GLSRC)sjbig2.h:$(GLSRC)std.h
$(GLSRC)sjbig2.h:$(GLSRC)stdpre.h
$(GLSRC)sjbig2.h:$(GLGEN)arch.h
$(GLSRC)sjbig2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)spdiffx.h:$(GLSRC)scommon.h
$(GLSRC)spdiffx.h:$(GLSRC)gsstype.h
$(GLSRC)spdiffx.h:$(GLSRC)gsmemory.h
$(GLSRC)spdiffx.h:$(GLSRC)gslibctx.h
$(GLSRC)spdiffx.h:$(GLSRC)stdio_.h
$(GLSRC)spdiffx.h:$(GLSRC)stdint_.h
$(GLSRC)spdiffx.h:$(GLSRC)gssprintf.h
$(GLSRC)spdiffx.h:$(GLSRC)gstypes.h
$(GLSRC)spdiffx.h:$(GLSRC)std.h
$(GLSRC)spdiffx.h:$(GLSRC)stdpre.h
$(GLSRC)spdiffx.h:$(GLGEN)arch.h
$(GLSRC)spdiffx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)spngpx.h:$(GLSRC)scommon.h
$(GLSRC)spngpx.h:$(GLSRC)gsstype.h
$(GLSRC)spngpx.h:$(GLSRC)gsmemory.h
$(GLSRC)spngpx.h:$(GLSRC)gslibctx.h
$(GLSRC)spngpx.h:$(GLSRC)stdio_.h
$(GLSRC)spngpx.h:$(GLSRC)stdint_.h
$(GLSRC)spngpx.h:$(GLSRC)gssprintf.h
$(GLSRC)spngpx.h:$(GLSRC)gstypes.h
$(GLSRC)spngpx.h:$(GLSRC)std.h
$(GLSRC)spngpx.h:$(GLSRC)stdpre.h
$(GLSRC)spngpx.h:$(GLGEN)arch.h
$(GLSRC)spngpx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)spprint.h:$(GLSRC)scommon.h
$(GLSRC)spprint.h:$(GLSRC)gsstype.h
$(GLSRC)spprint.h:$(GLSRC)gsmemory.h
$(GLSRC)spprint.h:$(GLSRC)gslibctx.h
$(GLSRC)spprint.h:$(GLSRC)stdio_.h
$(GLSRC)spprint.h:$(GLSRC)stdint_.h
$(GLSRC)spprint.h:$(GLSRC)gssprintf.h
$(GLSRC)spprint.h:$(GLSRC)gstypes.h
$(GLSRC)spprint.h:$(GLSRC)std.h
$(GLSRC)spprint.h:$(GLSRC)stdpre.h
$(GLSRC)spprint.h:$(GLGEN)arch.h
$(GLSRC)spprint.h:$(GLSRC)gs_dll_call.h
$(GLSRC)spsdf.h:$(GLSRC)gsparam.h
$(GLSRC)spsdf.h:$(GLSRC)scommon.h
$(GLSRC)spsdf.h:$(GLSRC)gsstype.h
$(GLSRC)spsdf.h:$(GLSRC)gsmemory.h
$(GLSRC)spsdf.h:$(GLSRC)gslibctx.h
$(GLSRC)spsdf.h:$(GLSRC)stdio_.h
$(GLSRC)spsdf.h:$(GLSRC)stdint_.h
$(GLSRC)spsdf.h:$(GLSRC)gssprintf.h
$(GLSRC)spsdf.h:$(GLSRC)gstypes.h
$(GLSRC)spsdf.h:$(GLSRC)std.h
$(GLSRC)spsdf.h:$(GLSRC)stdpre.h
$(GLSRC)spsdf.h:$(GLGEN)arch.h
$(GLSRC)spsdf.h:$(GLSRC)gs_dll_call.h
$(GLSRC)srlx.h:$(GLSRC)scommon.h
$(GLSRC)srlx.h:$(GLSRC)gsstype.h
$(GLSRC)srlx.h:$(GLSRC)gsmemory.h
$(GLSRC)srlx.h:$(GLSRC)gslibctx.h
$(GLSRC)srlx.h:$(GLSRC)stdio_.h
$(GLSRC)srlx.h:$(GLSRC)stdint_.h
$(GLSRC)srlx.h:$(GLSRC)gssprintf.h
$(GLSRC)srlx.h:$(GLSRC)gstypes.h
$(GLSRC)srlx.h:$(GLSRC)std.h
$(GLSRC)srlx.h:$(GLSRC)stdpre.h
$(GLSRC)srlx.h:$(GLGEN)arch.h
$(GLSRC)srlx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)spwgx.h:$(GLSRC)scommon.h
$(GLSRC)spwgx.h:$(GLSRC)gsstype.h
$(GLSRC)spwgx.h:$(GLSRC)gsmemory.h
$(GLSRC)spwgx.h:$(GLSRC)gslibctx.h
$(GLSRC)spwgx.h:$(GLSRC)stdio_.h
$(GLSRC)spwgx.h:$(GLSRC)stdint_.h
$(GLSRC)spwgx.h:$(GLSRC)gssprintf.h
$(GLSRC)spwgx.h:$(GLSRC)gstypes.h
$(GLSRC)spwgx.h:$(GLSRC)std.h
$(GLSRC)spwgx.h:$(GLSRC)stdpre.h
$(GLSRC)spwgx.h:$(GLGEN)arch.h
$(GLSRC)spwgx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sstring.h:$(GLSRC)scommon.h
$(GLSRC)sstring.h:$(GLSRC)gsstype.h
$(GLSRC)sstring.h:$(GLSRC)gsmemory.h
$(GLSRC)sstring.h:$(GLSRC)gslibctx.h
$(GLSRC)sstring.h:$(GLSRC)stdio_.h
$(GLSRC)sstring.h:$(GLSRC)stdint_.h
$(GLSRC)sstring.h:$(GLSRC)gssprintf.h
$(GLSRC)sstring.h:$(GLSRC)gstypes.h
$(GLSRC)sstring.h:$(GLSRC)std.h
$(GLSRC)sstring.h:$(GLSRC)stdpre.h
$(GLSRC)sstring.h:$(GLGEN)arch.h
$(GLSRC)sstring.h:$(GLSRC)gs_dll_call.h
$(GLSRC)strimpl.h:$(GLSRC)gsstruct.h
$(GLSRC)strimpl.h:$(GLSRC)scommon.h
$(GLSRC)strimpl.h:$(GLSRC)gsstype.h
$(GLSRC)strimpl.h:$(GLSRC)gsmemory.h
$(GLSRC)strimpl.h:$(GLSRC)gslibctx.h
$(GLSRC)strimpl.h:$(GLSRC)stdio_.h
$(GLSRC)strimpl.h:$(GLSRC)stdint_.h
$(GLSRC)strimpl.h:$(GLSRC)gssprintf.h
$(GLSRC)strimpl.h:$(GLSRC)gstypes.h
$(GLSRC)strimpl.h:$(GLSRC)std.h
$(GLSRC)strimpl.h:$(GLSRC)stdpre.h
$(GLSRC)strimpl.h:$(GLGEN)arch.h
$(GLSRC)strimpl.h:$(GLSRC)gs_dll_call.h
$(GLSRC)szlibx.h:$(GLSRC)scommon.h
$(GLSRC)szlibx.h:$(GLSRC)gsstype.h
$(GLSRC)szlibx.h:$(GLSRC)gsmemory.h
$(GLSRC)szlibx.h:$(GLSRC)gslibctx.h
$(GLSRC)szlibx.h:$(GLSRC)stdio_.h
$(GLSRC)szlibx.h:$(GLSRC)stdint_.h
$(GLSRC)szlibx.h:$(GLSRC)gssprintf.h
$(GLSRC)szlibx.h:$(GLSRC)gstypes.h
$(GLSRC)szlibx.h:$(GLSRC)std.h
$(GLSRC)szlibx.h:$(GLSRC)stdpre.h
$(GLSRC)szlibx.h:$(GLGEN)arch.h
$(GLSRC)szlibx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)scf.h:$(GLSRC)shc.h
$(GLSRC)scf.h:$(GLSRC)scommon.h
$(GLSRC)scf.h:$(GLSRC)gsstype.h
$(GLSRC)scf.h:$(GLSRC)gsmemory.h
$(GLSRC)scf.h:$(GLSRC)gslibctx.h
$(GLSRC)scf.h:$(GLSRC)stdio_.h
$(GLSRC)scf.h:$(GLSRC)stdint_.h
$(GLSRC)scf.h:$(GLSRC)gssprintf.h
$(GLSRC)scf.h:$(GLSRC)gsbittab.h
$(GLSRC)scf.h:$(GLSRC)gstypes.h
$(GLSRC)scf.h:$(GLSRC)std.h
$(GLSRC)scf.h:$(GLSRC)stdpre.h
$(GLSRC)scf.h:$(GLGEN)arch.h
$(GLSRC)scf.h:$(GLSRC)gs_dll_call.h
$(GLSRC)scfx.h:$(GLSRC)shc.h
$(GLSRC)scfx.h:$(GLSRC)scommon.h
$(GLSRC)scfx.h:$(GLSRC)gsstype.h
$(GLSRC)scfx.h:$(GLSRC)gsmemory.h
$(GLSRC)scfx.h:$(GLSRC)gslibctx.h
$(GLSRC)scfx.h:$(GLSRC)stdio_.h
$(GLSRC)scfx.h:$(GLSRC)stdint_.h
$(GLSRC)scfx.h:$(GLSRC)gssprintf.h
$(GLSRC)scfx.h:$(GLSRC)gsbittab.h
$(GLSRC)scfx.h:$(GLSRC)gstypes.h
$(GLSRC)scfx.h:$(GLSRC)std.h
$(GLSRC)scfx.h:$(GLSRC)stdpre.h
$(GLSRC)scfx.h:$(GLGEN)arch.h
$(GLSRC)scfx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)siinterp.h:$(GLSRC)sisparam.h
$(GLSRC)siinterp.h:$(GLSRC)gxdda.h
$(GLSRC)siinterp.h:$(GLSRC)gxfixed.h
$(GLSRC)siinterp.h:$(GLSRC)scommon.h
$(GLSRC)siinterp.h:$(GLSRC)gsstype.h
$(GLSRC)siinterp.h:$(GLSRC)gsmemory.h
$(GLSRC)siinterp.h:$(GLSRC)gslibctx.h
$(GLSRC)siinterp.h:$(GLSRC)stdio_.h
$(GLSRC)siinterp.h:$(GLSRC)stdint_.h
$(GLSRC)siinterp.h:$(GLSRC)gssprintf.h
$(GLSRC)siinterp.h:$(GLSRC)gstypes.h
$(GLSRC)siinterp.h:$(GLSRC)std.h
$(GLSRC)siinterp.h:$(GLSRC)stdpre.h
$(GLSRC)siinterp.h:$(GLGEN)arch.h
$(GLSRC)siinterp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)siscale.h:$(GLSRC)sisparam.h
$(GLSRC)siscale.h:$(GLSRC)gxdda.h
$(GLSRC)siscale.h:$(GLSRC)gxfixed.h
$(GLSRC)siscale.h:$(GLSRC)scommon.h
$(GLSRC)siscale.h:$(GLSRC)gsstype.h
$(GLSRC)siscale.h:$(GLSRC)gsmemory.h
$(GLSRC)siscale.h:$(GLSRC)gslibctx.h
$(GLSRC)siscale.h:$(GLSRC)stdio_.h
$(GLSRC)siscale.h:$(GLSRC)stdint_.h
$(GLSRC)siscale.h:$(GLSRC)gssprintf.h
$(GLSRC)siscale.h:$(GLSRC)gstypes.h
$(GLSRC)siscale.h:$(GLSRC)std.h
$(GLSRC)siscale.h:$(GLSRC)stdpre.h
$(GLSRC)siscale.h:$(GLGEN)arch.h
$(GLSRC)siscale.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sidscale.h:$(GLSRC)sisparam.h
$(GLSRC)sidscale.h:$(GLSRC)gxdda.h
$(GLSRC)sidscale.h:$(GLSRC)gxfixed.h
$(GLSRC)sidscale.h:$(GLSRC)scommon.h
$(GLSRC)sidscale.h:$(GLSRC)gsstype.h
$(GLSRC)sidscale.h:$(GLSRC)gsmemory.h
$(GLSRC)sidscale.h:$(GLSRC)gslibctx.h
$(GLSRC)sidscale.h:$(GLSRC)stdio_.h
$(GLSRC)sidscale.h:$(GLSRC)stdint_.h
$(GLSRC)sidscale.h:$(GLSRC)gssprintf.h
$(GLSRC)sidscale.h:$(GLSRC)gstypes.h
$(GLSRC)sidscale.h:$(GLSRC)std.h
$(GLSRC)sidscale.h:$(GLSRC)stdpre.h
$(GLSRC)sidscale.h:$(GLGEN)arch.h
$(GLSRC)sidscale.h:$(GLSRC)gs_dll_call.h
$(GLSRC)simscale.h:$(GLSRC)sisparam.h
$(GLSRC)simscale.h:$(GLSRC)gxdda.h
$(GLSRC)simscale.h:$(GLSRC)gxfixed.h
$(GLSRC)simscale.h:$(GLSRC)scommon.h
$(GLSRC)simscale.h:$(GLSRC)gsstype.h
$(GLSRC)simscale.h:$(GLSRC)gsmemory.h
$(GLSRC)simscale.h:$(GLSRC)gslibctx.h
$(GLSRC)simscale.h:$(GLSRC)stdio_.h
$(GLSRC)simscale.h:$(GLSRC)stdint_.h
$(GLSRC)simscale.h:$(GLSRC)gssprintf.h
$(GLSRC)simscale.h:$(GLSRC)gstypes.h
$(GLSRC)simscale.h:$(GLSRC)std.h
$(GLSRC)simscale.h:$(GLSRC)stdpre.h
$(GLSRC)simscale.h:$(GLGEN)arch.h
$(GLSRC)simscale.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gximage.h:$(GLSRC)gxsample.h
$(GLSRC)gximage.h:$(GLSRC)sisparam.h
$(GLSRC)gximage.h:$(GLSRC)gxcpath.h
$(GLSRC)gximage.h:$(GLSRC)gxiparam.h
$(GLSRC)gximage.h:$(GLSRC)gxdevcli.h
$(GLSRC)gximage.h:$(GLSRC)gxcmap.h
$(GLSRC)gximage.h:$(GLSRC)gxtext.h
$(GLSRC)gximage.h:$(GLSRC)gstext.h
$(GLSRC)gximage.h:$(GLSRC)gsnamecl.h
$(GLSRC)gximage.h:$(GLSRC)gstparam.h
$(GLSRC)gximage.h:$(GLSRC)gxfmap.h
$(GLSRC)gximage.h:$(GLSRC)gsfunc.h
$(GLSRC)gximage.h:$(GLSRC)gxcspace.h
$(GLSRC)gximage.h:$(GLSRC)strimpl.h
$(GLSRC)gximage.h:$(GLSRC)gxrplane.h
$(GLSRC)gximage.h:$(GLSRC)gscsel.h
$(GLSRC)gximage.h:$(GLSRC)gxfcache.h
$(GLSRC)gximage.h:$(GLSRC)gsfont.h
$(GLSRC)gximage.h:$(GLSRC)gsimage.h
$(GLSRC)gximage.h:$(GLSRC)gsdcolor.h
$(GLSRC)gximage.h:$(GLSRC)gxcvalue.h
$(GLSRC)gximage.h:$(GLSRC)gxbcache.h
$(GLSRC)gximage.h:$(GLSRC)gsropt.h
$(GLSRC)gximage.h:$(GLSRC)gxdda.h
$(GLSRC)gximage.h:$(GLSRC)gxpath.h
$(GLSRC)gximage.h:$(GLSRC)gxiclass.h
$(GLSRC)gximage.h:$(GLSRC)gxfrac.h
$(GLSRC)gximage.h:$(GLSRC)gxtmap.h
$(GLSRC)gximage.h:$(GLSRC)gxftype.h
$(GLSRC)gximage.h:$(GLSRC)gscms.h
$(GLSRC)gximage.h:$(GLSRC)gsrect.h
$(GLSRC)gximage.h:$(GLSRC)gslparam.h
$(GLSRC)gximage.h:$(GLSRC)gsdevice.h
$(GLSRC)gximage.h:$(GLSRC)gscpm.h
$(GLSRC)gximage.h:$(GLSRC)gscspace.h
$(GLSRC)gximage.h:$(GLSRC)gsgstate.h
$(GLSRC)gximage.h:$(GLSRC)gsxfont.h
$(GLSRC)gximage.h:$(GLSRC)gsdsrc.h
$(GLSRC)gximage.h:$(GLSRC)gsiparam.h
$(GLSRC)gximage.h:$(GLSRC)gxfixed.h
$(GLSRC)gximage.h:$(GLSRC)gscompt.h
$(GLSRC)gximage.h:$(GLSRC)gsmatrix.h
$(GLSRC)gximage.h:$(GLSRC)gspenum.h
$(GLSRC)gximage.h:$(GLSRC)gxhttile.h
$(GLSRC)gximage.h:$(GLSRC)gsparam.h
$(GLSRC)gximage.h:$(GLSRC)gsrefct.h
$(GLSRC)gximage.h:$(GLSRC)gp.h
$(GLSRC)gximage.h:$(GLSRC)memento.h
$(GLSRC)gximage.h:$(GLSRC)memory_.h
$(GLSRC)gximage.h:$(GLSRC)gsuid.h
$(GLSRC)gximage.h:$(GLSRC)gsstruct.h
$(GLSRC)gximage.h:$(GLSRC)gxsync.h
$(GLSRC)gximage.h:$(GLSRC)gxbitmap.h
$(GLSRC)gximage.h:$(GLSRC)srdline.h
$(GLSRC)gximage.h:$(GLSRC)scommon.h
$(GLSRC)gximage.h:$(GLSRC)gsbitmap.h
$(GLSRC)gximage.h:$(GLSRC)gsccolor.h
$(GLSRC)gximage.h:$(GLSRC)gxarith.h
$(GLSRC)gximage.h:$(GLSRC)stat_.h
$(GLSRC)gximage.h:$(GLSRC)gpsync.h
$(GLSRC)gximage.h:$(GLSRC)gsstype.h
$(GLSRC)gximage.h:$(GLSRC)gsmemory.h
$(GLSRC)gximage.h:$(GLSRC)gpgetenv.h
$(GLSRC)gximage.h:$(GLSRC)gscdefs.h
$(GLSRC)gximage.h:$(GLSRC)gslibctx.h
$(GLSRC)gximage.h:$(GLSRC)gxcindex.h
$(GLSRC)gximage.h:$(GLSRC)stdio_.h
$(GLSRC)gximage.h:$(GLSRC)gsccode.h
$(GLSRC)gximage.h:$(GLSRC)stdint_.h
$(GLSRC)gximage.h:$(GLSRC)gssprintf.h
$(GLSRC)gximage.h:$(GLSRC)gstypes.h
$(GLSRC)gximage.h:$(GLSRC)std.h
$(GLSRC)gximage.h:$(GLSRC)stdpre.h
$(GLSRC)gximage.h:$(GLGEN)arch.h
$(GLSRC)gximage.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxhldevc.h:$(GLSRC)gscms.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsdevice.h
$(GLSRC)gxhldevc.h:$(GLSRC)gscspace.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsgstate.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsiparam.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxhldevc.h:$(GLSRC)gxhttile.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsparam.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsrefct.h
$(GLSRC)gxhldevc.h:$(GLSRC)memento.h
$(GLSRC)gxhldevc.h:$(GLSRC)gxsync.h
$(GLSRC)gxhldevc.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxhldevc.h:$(GLSRC)scommon.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsccolor.h
$(GLSRC)gxhldevc.h:$(GLSRC)gxarith.h
$(GLSRC)gxhldevc.h:$(GLSRC)gpsync.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsstype.h
$(GLSRC)gxhldevc.h:$(GLSRC)gsmemory.h
$(GLSRC)gxhldevc.h:$(GLSRC)gslibctx.h
$(GLSRC)gxhldevc.h:$(GLSRC)gxcindex.h
$(GLSRC)gxhldevc.h:$(GLSRC)stdio_.h
$(GLSRC)gxhldevc.h:$(GLSRC)stdint_.h
$(GLSRC)gxhldevc.h:$(GLSRC)gssprintf.h
$(GLSRC)gxhldevc.h:$(GLSRC)gstypes.h
$(GLSRC)gxhldevc.h:$(GLSRC)std.h
$(GLSRC)gxhldevc.h:$(GLSRC)stdpre.h
$(GLSRC)gxhldevc.h:$(GLGEN)arch.h
$(GLSRC)gxhldevc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsptype2.h:$(GLSRC)gsshade.h
$(GLSRC)gsptype2.h:$(GLSRC)gspath.h
$(GLSRC)gsptype2.h:$(GLSRC)gxmatrix.h
$(GLSRC)gsptype2.h:$(GLSRC)gscie.h
$(GLSRC)gsptype2.h:$(GLSRC)gspcolor.h
$(GLSRC)gsptype2.h:$(GLSRC)gsfunc.h
$(GLSRC)gsptype2.h:$(GLSRC)gxctable.h
$(GLSRC)gsptype2.h:$(GLSRC)gsdcolor.h
$(GLSRC)gsptype2.h:$(GLSRC)gxpath.h
$(GLSRC)gsptype2.h:$(GLSRC)gxfrac.h
$(GLSRC)gsptype2.h:$(GLSRC)gscms.h
$(GLSRC)gsptype2.h:$(GLSRC)gsrect.h
$(GLSRC)gsptype2.h:$(GLSRC)gslparam.h
$(GLSRC)gsptype2.h:$(GLSRC)gsdevice.h
$(GLSRC)gsptype2.h:$(GLSRC)gscpm.h
$(GLSRC)gsptype2.h:$(GLSRC)gscspace.h
$(GLSRC)gsptype2.h:$(GLSRC)gsgstate.h
$(GLSRC)gsptype2.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsptype2.h:$(GLSRC)gsiparam.h
$(GLSRC)gsptype2.h:$(GLSRC)gxfixed.h
$(GLSRC)gsptype2.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsptype2.h:$(GLSRC)gspenum.h
$(GLSRC)gsptype2.h:$(GLSRC)gxhttile.h
$(GLSRC)gsptype2.h:$(GLSRC)gsparam.h
$(GLSRC)gsptype2.h:$(GLSRC)gsrefct.h
$(GLSRC)gsptype2.h:$(GLSRC)memento.h
$(GLSRC)gsptype2.h:$(GLSRC)gsuid.h
$(GLSRC)gsptype2.h:$(GLSRC)gsstruct.h
$(GLSRC)gsptype2.h:$(GLSRC)gxsync.h
$(GLSRC)gsptype2.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsptype2.h:$(GLSRC)scommon.h
$(GLSRC)gsptype2.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsptype2.h:$(GLSRC)gsccolor.h
$(GLSRC)gsptype2.h:$(GLSRC)gxarith.h
$(GLSRC)gsptype2.h:$(GLSRC)gpsync.h
$(GLSRC)gsptype2.h:$(GLSRC)gsstype.h
$(GLSRC)gsptype2.h:$(GLSRC)gsmemory.h
$(GLSRC)gsptype2.h:$(GLSRC)gslibctx.h
$(GLSRC)gsptype2.h:$(GLSRC)gxcindex.h
$(GLSRC)gsptype2.h:$(GLSRC)stdio_.h
$(GLSRC)gsptype2.h:$(GLSRC)stdint_.h
$(GLSRC)gsptype2.h:$(GLSRC)gssprintf.h
$(GLSRC)gsptype2.h:$(GLSRC)gstypes.h
$(GLSRC)gsptype2.h:$(GLSRC)std.h
$(GLSRC)gsptype2.h:$(GLSRC)stdpre.h
$(GLSRC)gsptype2.h:$(GLGEN)arch.h
$(GLSRC)gsptype2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxtext.h
$(GLSRC)gdevddrw.h:$(GLSRC)gstext.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevddrw.h:$(GLSRC)gstparam.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscsel.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsfont.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsimage.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsropt.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxdda.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxpath.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxftype.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscms.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsrect.h
$(GLSRC)gdevddrw.h:$(GLSRC)gslparam.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscpm.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscspace.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscompt.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevddrw.h:$(GLSRC)gspenum.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsparam.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevddrw.h:$(GLSRC)gp.h
$(GLSRC)gdevddrw.h:$(GLSRC)memento.h
$(GLSRC)gdevddrw.h:$(GLSRC)memory_.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsuid.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxsync.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevddrw.h:$(GLSRC)srdline.h
$(GLSRC)gdevddrw.h:$(GLSRC)scommon.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxarith.h
$(GLSRC)gdevddrw.h:$(GLSRC)stat_.h
$(GLSRC)gdevddrw.h:$(GLSRC)gpsync.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsstype.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevddrw.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevddrw.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevddrw.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevddrw.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevddrw.h:$(GLSRC)stdio_.h
$(GLSRC)gdevddrw.h:$(GLSRC)gsccode.h
$(GLSRC)gdevddrw.h:$(GLSRC)stdint_.h
$(GLSRC)gdevddrw.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevddrw.h:$(GLSRC)gstypes.h
$(GLSRC)gdevddrw.h:$(GLSRC)std.h
$(GLSRC)gdevddrw.h:$(GLSRC)stdpre.h
$(GLSRC)gdevddrw.h:$(GLGEN)arch.h
$(GLSRC)gdevddrw.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfill.h:$(GLSRC)gzpath.h
$(GLSRC)gxfill.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxfill.h:$(GLSRC)gxcmap.h
$(GLSRC)gxfill.h:$(GLSRC)gxtext.h
$(GLSRC)gxfill.h:$(GLSRC)gstext.h
$(GLSRC)gxfill.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxfill.h:$(GLSRC)gstparam.h
$(GLSRC)gxfill.h:$(GLSRC)gxfmap.h
$(GLSRC)gxfill.h:$(GLSRC)gsfunc.h
$(GLSRC)gxfill.h:$(GLSRC)gxcspace.h
$(GLSRC)gxfill.h:$(GLSRC)gxrplane.h
$(GLSRC)gxfill.h:$(GLSRC)gscsel.h
$(GLSRC)gxfill.h:$(GLSRC)gxfcache.h
$(GLSRC)gxfill.h:$(GLSRC)gsfont.h
$(GLSRC)gxfill.h:$(GLSRC)gsimage.h
$(GLSRC)gxfill.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfill.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxfill.h:$(GLSRC)gxbcache.h
$(GLSRC)gxfill.h:$(GLSRC)gsropt.h
$(GLSRC)gxfill.h:$(GLSRC)gxdda.h
$(GLSRC)gxfill.h:$(GLSRC)gxpath.h
$(GLSRC)gxfill.h:$(GLSRC)gxfrac.h
$(GLSRC)gxfill.h:$(GLSRC)gxtmap.h
$(GLSRC)gxfill.h:$(GLSRC)gxftype.h
$(GLSRC)gxfill.h:$(GLSRC)gscms.h
$(GLSRC)gxfill.h:$(GLSRC)gsrect.h
$(GLSRC)gxfill.h:$(GLSRC)gslparam.h
$(GLSRC)gxfill.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfill.h:$(GLSRC)gscpm.h
$(GLSRC)gxfill.h:$(GLSRC)gscspace.h
$(GLSRC)gxfill.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfill.h:$(GLSRC)gsxfont.h
$(GLSRC)gxfill.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxfill.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfill.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfill.h:$(GLSRC)gscompt.h
$(GLSRC)gxfill.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfill.h:$(GLSRC)gspenum.h
$(GLSRC)gxfill.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfill.h:$(GLSRC)gsparam.h
$(GLSRC)gxfill.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfill.h:$(GLSRC)gp.h
$(GLSRC)gxfill.h:$(GLSRC)memento.h
$(GLSRC)gxfill.h:$(GLSRC)memory_.h
$(GLSRC)gxfill.h:$(GLSRC)gsuid.h
$(GLSRC)gxfill.h:$(GLSRC)gsstruct.h
$(GLSRC)gxfill.h:$(GLSRC)gxsync.h
$(GLSRC)gxfill.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfill.h:$(GLSRC)srdline.h
$(GLSRC)gxfill.h:$(GLSRC)scommon.h
$(GLSRC)gxfill.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfill.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfill.h:$(GLSRC)gxarith.h
$(GLSRC)gxfill.h:$(GLSRC)stat_.h
$(GLSRC)gxfill.h:$(GLSRC)gpsync.h
$(GLSRC)gxfill.h:$(GLSRC)gsstype.h
$(GLSRC)gxfill.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfill.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxfill.h:$(GLSRC)gscdefs.h
$(GLSRC)gxfill.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfill.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfill.h:$(GLSRC)stdio_.h
$(GLSRC)gxfill.h:$(GLSRC)gsccode.h
$(GLSRC)gxfill.h:$(GLSRC)stdint_.h
$(GLSRC)gxfill.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfill.h:$(GLSRC)gstypes.h
$(GLSRC)gxfill.h:$(GLSRC)std.h
$(GLSRC)gxfill.h:$(GLSRC)stdpre.h
$(GLSRC)gxfill.h:$(GLGEN)arch.h
$(GLSRC)gxfill.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ttfoutl.h:$(GLSRC)malloc_.h
$(GLSRC)ttfoutl.h:$(GLSRC)bobbin.h
$(GLSRC)ttfoutl.h:$(GLSRC)gxfcache.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsfont.h
$(GLSRC)ttfoutl.h:$(GLSRC)gxbcache.h
$(GLSRC)ttfoutl.h:$(GLSRC)gxftype.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsgstate.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsxfont.h
$(GLSRC)ttfoutl.h:$(GLSRC)gxfixed.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsmatrix.h
$(GLSRC)ttfoutl.h:$(GLSRC)memento.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsuid.h
$(GLSRC)ttfoutl.h:$(GLSRC)gxbitmap.h
$(GLSRC)ttfoutl.h:$(GLSRC)scommon.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsbitmap.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsstype.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsmemory.h
$(GLSRC)ttfoutl.h:$(GLSRC)gslibctx.h
$(GLSRC)ttfoutl.h:$(GLSRC)stdio_.h
$(GLSRC)ttfoutl.h:$(GLSRC)gsccode.h
$(GLSRC)ttfoutl.h:$(GLSRC)stdint_.h
$(GLSRC)ttfoutl.h:$(GLSRC)gssprintf.h
$(GLSRC)ttfoutl.h:$(GLSRC)gstypes.h
$(GLSRC)ttfoutl.h:$(GLSRC)std.h
$(GLSRC)ttfoutl.h:$(GLSRC)stdpre.h
$(GLSRC)ttfoutl.h:$(GLGEN)arch.h
$(GLSRC)ttfoutl.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxttfb.h:$(GLSRC)ttfoutl.h
$(GLSRC)gxttfb.h:$(GLSRC)malloc_.h
$(GLSRC)gxttfb.h:$(GLSRC)bobbin.h
$(GLSRC)gxttfb.h:$(GLSRC)gxfont.h
$(GLSRC)gxttfb.h:$(GLSRC)gspath.h
$(GLSRC)gxttfb.h:$(GLSRC)gsgdata.h
$(GLSRC)gxttfb.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxttfb.h:$(GLSRC)gxfapi.h
$(GLSRC)gxttfb.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxttfb.h:$(GLSRC)gstext.h
$(GLSRC)gxttfb.h:$(GLSRC)gxfcache.h
$(GLSRC)gxttfb.h:$(GLSRC)gsfont.h
$(GLSRC)gxttfb.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxttfb.h:$(GLSRC)gxbcache.h
$(GLSRC)gxttfb.h:$(GLSRC)gxpath.h
$(GLSRC)gxttfb.h:$(GLSRC)gxftype.h
$(GLSRC)gxttfb.h:$(GLSRC)gscms.h
$(GLSRC)gxttfb.h:$(GLSRC)gsrect.h
$(GLSRC)gxttfb.h:$(GLSRC)gslparam.h
$(GLSRC)gxttfb.h:$(GLSRC)gsdevice.h
$(GLSRC)gxttfb.h:$(GLSRC)gscpm.h
$(GLSRC)gxttfb.h:$(GLSRC)gsgcache.h
$(GLSRC)gxttfb.h:$(GLSRC)gscspace.h
$(GLSRC)gxttfb.h:$(GLSRC)gsgstate.h
$(GLSRC)gxttfb.h:$(GLSRC)gsnotify.h
$(GLSRC)gxttfb.h:$(GLSRC)gsxfont.h
$(GLSRC)gxttfb.h:$(GLSRC)gsiparam.h
$(GLSRC)gxttfb.h:$(GLSRC)gxfixed.h
$(GLSRC)gxttfb.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxttfb.h:$(GLSRC)gspenum.h
$(GLSRC)gxttfb.h:$(GLSRC)gxhttile.h
$(GLSRC)gxttfb.h:$(GLSRC)gsparam.h
$(GLSRC)gxttfb.h:$(GLSRC)gsrefct.h
$(GLSRC)gxttfb.h:$(GLSRC)memento.h
$(GLSRC)gxttfb.h:$(GLSRC)gsuid.h
$(GLSRC)gxttfb.h:$(GLSRC)gxsync.h
$(GLSRC)gxttfb.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxttfb.h:$(GLSRC)scommon.h
$(GLSRC)gxttfb.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxttfb.h:$(GLSRC)gsccolor.h
$(GLSRC)gxttfb.h:$(GLSRC)gxarith.h
$(GLSRC)gxttfb.h:$(GLSRC)gpsync.h
$(GLSRC)gxttfb.h:$(GLSRC)gsstype.h
$(GLSRC)gxttfb.h:$(GLSRC)gsmemory.h
$(GLSRC)gxttfb.h:$(GLSRC)gslibctx.h
$(GLSRC)gxttfb.h:$(GLSRC)gxcindex.h
$(GLSRC)gxttfb.h:$(GLSRC)stdio_.h
$(GLSRC)gxttfb.h:$(GLSRC)gsccode.h
$(GLSRC)gxttfb.h:$(GLSRC)stdint_.h
$(GLSRC)gxttfb.h:$(GLSRC)gssprintf.h
$(GLSRC)gxttfb.h:$(GLSRC)gstypes.h
$(GLSRC)gxttfb.h:$(GLSRC)std.h
$(GLSRC)gxttfb.h:$(GLSRC)stdpre.h
$(GLSRC)gxttfb.h:$(GLGEN)arch.h
$(GLSRC)gxttfb.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gzspotan.h:$(GLSRC)gzpath.h
$(GLSRC)gzspotan.h:$(GLSRC)gxdevcli.h
$(GLSRC)gzspotan.h:$(GLSRC)gxcmap.h
$(GLSRC)gzspotan.h:$(GLSRC)gxtext.h
$(GLSRC)gzspotan.h:$(GLSRC)gstext.h
$(GLSRC)gzspotan.h:$(GLSRC)gsnamecl.h
$(GLSRC)gzspotan.h:$(GLSRC)gstparam.h
$(GLSRC)gzspotan.h:$(GLSRC)gxfmap.h
$(GLSRC)gzspotan.h:$(GLSRC)gsfunc.h
$(GLSRC)gzspotan.h:$(GLSRC)gxcspace.h
$(GLSRC)gzspotan.h:$(GLSRC)gxrplane.h
$(GLSRC)gzspotan.h:$(GLSRC)gscsel.h
$(GLSRC)gzspotan.h:$(GLSRC)gxfcache.h
$(GLSRC)gzspotan.h:$(GLSRC)gsfont.h
$(GLSRC)gzspotan.h:$(GLSRC)gsimage.h
$(GLSRC)gzspotan.h:$(GLSRC)gsdcolor.h
$(GLSRC)gzspotan.h:$(GLSRC)gxcvalue.h
$(GLSRC)gzspotan.h:$(GLSRC)gxbcache.h
$(GLSRC)gzspotan.h:$(GLSRC)gsropt.h
$(GLSRC)gzspotan.h:$(GLSRC)gxdda.h
$(GLSRC)gzspotan.h:$(GLSRC)gxpath.h
$(GLSRC)gzspotan.h:$(GLSRC)gxfrac.h
$(GLSRC)gzspotan.h:$(GLSRC)gxtmap.h
$(GLSRC)gzspotan.h:$(GLSRC)gxftype.h
$(GLSRC)gzspotan.h:$(GLSRC)gscms.h
$(GLSRC)gzspotan.h:$(GLSRC)gsrect.h
$(GLSRC)gzspotan.h:$(GLSRC)gslparam.h
$(GLSRC)gzspotan.h:$(GLSRC)gsdevice.h
$(GLSRC)gzspotan.h:$(GLSRC)gscpm.h
$(GLSRC)gzspotan.h:$(GLSRC)gscspace.h
$(GLSRC)gzspotan.h:$(GLSRC)gsgstate.h
$(GLSRC)gzspotan.h:$(GLSRC)gsxfont.h
$(GLSRC)gzspotan.h:$(GLSRC)gsdsrc.h
$(GLSRC)gzspotan.h:$(GLSRC)gsiparam.h
$(GLSRC)gzspotan.h:$(GLSRC)gxfixed.h
$(GLSRC)gzspotan.h:$(GLSRC)gscompt.h
$(GLSRC)gzspotan.h:$(GLSRC)gsmatrix.h
$(GLSRC)gzspotan.h:$(GLSRC)gspenum.h
$(GLSRC)gzspotan.h:$(GLSRC)gxhttile.h
$(GLSRC)gzspotan.h:$(GLSRC)gsparam.h
$(GLSRC)gzspotan.h:$(GLSRC)gsrefct.h
$(GLSRC)gzspotan.h:$(GLSRC)gp.h
$(GLSRC)gzspotan.h:$(GLSRC)memento.h
$(GLSRC)gzspotan.h:$(GLSRC)memory_.h
$(GLSRC)gzspotan.h:$(GLSRC)gsuid.h
$(GLSRC)gzspotan.h:$(GLSRC)gsstruct.h
$(GLSRC)gzspotan.h:$(GLSRC)gxsync.h
$(GLSRC)gzspotan.h:$(GLSRC)gxbitmap.h
$(GLSRC)gzspotan.h:$(GLSRC)srdline.h
$(GLSRC)gzspotan.h:$(GLSRC)scommon.h
$(GLSRC)gzspotan.h:$(GLSRC)gsbitmap.h
$(GLSRC)gzspotan.h:$(GLSRC)gsccolor.h
$(GLSRC)gzspotan.h:$(GLSRC)gxarith.h
$(GLSRC)gzspotan.h:$(GLSRC)stat_.h
$(GLSRC)gzspotan.h:$(GLSRC)gpsync.h
$(GLSRC)gzspotan.h:$(GLSRC)gsstype.h
$(GLSRC)gzspotan.h:$(GLSRC)gsmemory.h
$(GLSRC)gzspotan.h:$(GLSRC)gpgetenv.h
$(GLSRC)gzspotan.h:$(GLSRC)gscdefs.h
$(GLSRC)gzspotan.h:$(GLSRC)gslibctx.h
$(GLSRC)gzspotan.h:$(GLSRC)gxcindex.h
$(GLSRC)gzspotan.h:$(GLSRC)stdio_.h
$(GLSRC)gzspotan.h:$(GLSRC)gsccode.h
$(GLSRC)gzspotan.h:$(GLSRC)stdint_.h
$(GLSRC)gzspotan.h:$(GLSRC)gssprintf.h
$(GLSRC)gzspotan.h:$(GLSRC)gstypes.h
$(GLSRC)gzspotan.h:$(GLSRC)std.h
$(GLSRC)gzspotan.h:$(GLSRC)stdpre.h
$(GLSRC)gzspotan.h:$(GLGEN)arch.h
$(GLSRC)gzspotan.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsequivc.h:$(GLSRC)gxdevcli.h
$(GLSRC)gsequivc.h:$(GLSRC)gxcmap.h
$(GLSRC)gsequivc.h:$(GLSRC)gxtext.h
$(GLSRC)gsequivc.h:$(GLSRC)gstext.h
$(GLSRC)gsequivc.h:$(GLSRC)gsnamecl.h
$(GLSRC)gsequivc.h:$(GLSRC)gstparam.h
$(GLSRC)gsequivc.h:$(GLSRC)gxfmap.h
$(GLSRC)gsequivc.h:$(GLSRC)gsfunc.h
$(GLSRC)gsequivc.h:$(GLSRC)gxcspace.h
$(GLSRC)gsequivc.h:$(GLSRC)gxrplane.h
$(GLSRC)gsequivc.h:$(GLSRC)gscsel.h
$(GLSRC)gsequivc.h:$(GLSRC)gxfcache.h
$(GLSRC)gsequivc.h:$(GLSRC)gsfont.h
$(GLSRC)gsequivc.h:$(GLSRC)gsimage.h
$(GLSRC)gsequivc.h:$(GLSRC)gsdcolor.h
$(GLSRC)gsequivc.h:$(GLSRC)gxcvalue.h
$(GLSRC)gsequivc.h:$(GLSRC)gxbcache.h
$(GLSRC)gsequivc.h:$(GLSRC)gsropt.h
$(GLSRC)gsequivc.h:$(GLSRC)gxdda.h
$(GLSRC)gsequivc.h:$(GLSRC)gxpath.h
$(GLSRC)gsequivc.h:$(GLSRC)gxfrac.h
$(GLSRC)gsequivc.h:$(GLSRC)gxtmap.h
$(GLSRC)gsequivc.h:$(GLSRC)gxftype.h
$(GLSRC)gsequivc.h:$(GLSRC)gscms.h
$(GLSRC)gsequivc.h:$(GLSRC)gsrect.h
$(GLSRC)gsequivc.h:$(GLSRC)gslparam.h
$(GLSRC)gsequivc.h:$(GLSRC)gsdevice.h
$(GLSRC)gsequivc.h:$(GLSRC)gscpm.h
$(GLSRC)gsequivc.h:$(GLSRC)gscspace.h
$(GLSRC)gsequivc.h:$(GLSRC)gsgstate.h
$(GLSRC)gsequivc.h:$(GLSRC)gsxfont.h
$(GLSRC)gsequivc.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsequivc.h:$(GLSRC)gsiparam.h
$(GLSRC)gsequivc.h:$(GLSRC)gxfixed.h
$(GLSRC)gsequivc.h:$(GLSRC)gscompt.h
$(GLSRC)gsequivc.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsequivc.h:$(GLSRC)gspenum.h
$(GLSRC)gsequivc.h:$(GLSRC)gxhttile.h
$(GLSRC)gsequivc.h:$(GLSRC)gsparam.h
$(GLSRC)gsequivc.h:$(GLSRC)gsrefct.h
$(GLSRC)gsequivc.h:$(GLSRC)gp.h
$(GLSRC)gsequivc.h:$(GLSRC)memento.h
$(GLSRC)gsequivc.h:$(GLSRC)memory_.h
$(GLSRC)gsequivc.h:$(GLSRC)gsuid.h
$(GLSRC)gsequivc.h:$(GLSRC)gsstruct.h
$(GLSRC)gsequivc.h:$(GLSRC)gxsync.h
$(GLSRC)gsequivc.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsequivc.h:$(GLSRC)srdline.h
$(GLSRC)gsequivc.h:$(GLSRC)scommon.h
$(GLSRC)gsequivc.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsequivc.h:$(GLSRC)gsccolor.h
$(GLSRC)gsequivc.h:$(GLSRC)gxarith.h
$(GLSRC)gsequivc.h:$(GLSRC)stat_.h
$(GLSRC)gsequivc.h:$(GLSRC)gpsync.h
$(GLSRC)gsequivc.h:$(GLSRC)gsstype.h
$(GLSRC)gsequivc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsequivc.h:$(GLSRC)gpgetenv.h
$(GLSRC)gsequivc.h:$(GLSRC)gscdefs.h
$(GLSRC)gsequivc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsequivc.h:$(GLSRC)gxcindex.h
$(GLSRC)gsequivc.h:$(GLSRC)stdio_.h
$(GLSRC)gsequivc.h:$(GLSRC)gsccode.h
$(GLSRC)gsequivc.h:$(GLSRC)stdint_.h
$(GLSRC)gsequivc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsequivc.h:$(GLSRC)gstypes.h
$(GLSRC)gsequivc.h:$(GLSRC)std.h
$(GLSRC)gsequivc.h:$(GLSRC)stdpre.h
$(GLSRC)gsequivc.h:$(GLGEN)arch.h
$(GLSRC)gsequivc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxblend.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsequivc.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxtext.h
$(GLSRC)gdevdevn.h:$(GLSRC)gstext.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevdevn.h:$(GLSRC)gstparam.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscsel.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsfont.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsimage.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsropt.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxdda.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxpath.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxftype.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscms.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsrect.h
$(GLSRC)gdevdevn.h:$(GLSRC)gslparam.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscpm.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscspace.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscompt.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevdevn.h:$(GLSRC)gspenum.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsparam.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevdevn.h:$(GLSRC)gp.h
$(GLSRC)gdevdevn.h:$(GLSRC)memento.h
$(GLSRC)gdevdevn.h:$(GLSRC)memory_.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsuid.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxsync.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevdevn.h:$(GLSRC)srdline.h
$(GLSRC)gdevdevn.h:$(GLSRC)scommon.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxarith.h
$(GLSRC)gdevdevn.h:$(GLSRC)stat_.h
$(GLSRC)gdevdevn.h:$(GLSRC)gpsync.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsstype.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevdevn.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevdevn.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevdevn.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevdevn.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevdevn.h:$(GLSRC)stdio_.h
$(GLSRC)gdevdevn.h:$(GLSRC)gsccode.h
$(GLSRC)gdevdevn.h:$(GLSRC)stdint_.h
$(GLSRC)gdevdevn.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevdevn.h:$(GLSRC)gstypes.h
$(GLSRC)gdevdevn.h:$(GLSRC)std.h
$(GLSRC)gdevdevn.h:$(GLSRC)stdpre.h
$(GLSRC)gdevdevn.h:$(GLGEN)arch.h
$(GLSRC)gdevdevn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gdevprn.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)string_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsstrtok.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxclthrd.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxclpage.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxclist.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxgstate.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gstrans.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gdevp14.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxline.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsht1.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcomp.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)math_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcolor2.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxpcolor.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdevmem.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gdevdevn.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gx.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxclipsr.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gdebug.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdcolor.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxband.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxblend.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscolor2.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxmatrix.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcpath.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsht.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsequivc.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxpcache.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscindex.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsptype1.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscie.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxtext.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gstext.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxstate.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gstparam.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gspcolor.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxctable.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscsel.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsfont.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsimage.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsropt.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxdda.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxpath.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxiclass.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxftype.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscms.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsrect.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gslparam.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscpm.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscspace.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsio.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxclio.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscompt.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gspenum.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsparam.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gp.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)memento.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)memory_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsutil.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsuid.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsstrl.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gdbflags.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxsync.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gserrors.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)vmsmath.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)srdline.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)scommon.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsfname.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxarith.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)stat_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gpsync.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsstype.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)stdio_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gsccode.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)stdint_.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gstypes.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)std.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)stdpre.h
$(GLSRC)gdevdevnprn.h:$(GLGEN)arch.h
$(GLSRC)gdevdevnprn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxtext.h
$(GLSRC)gdevoflt.h:$(GLSRC)gstext.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevoflt.h:$(GLSRC)gstparam.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscsel.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsfont.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsimage.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsropt.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxdda.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxpath.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxftype.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscms.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsrect.h
$(GLSRC)gdevoflt.h:$(GLSRC)gslparam.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscpm.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscspace.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsio.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscompt.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevoflt.h:$(GLSRC)gspenum.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsparam.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevoflt.h:$(GLSRC)gp.h
$(GLSRC)gdevoflt.h:$(GLSRC)memento.h
$(GLSRC)gdevoflt.h:$(GLSRC)memory_.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsuid.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxsync.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevoflt.h:$(GLSRC)srdline.h
$(GLSRC)gdevoflt.h:$(GLSRC)scommon.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsfname.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxarith.h
$(GLSRC)gdevoflt.h:$(GLSRC)stat_.h
$(GLSRC)gdevoflt.h:$(GLSRC)gpsync.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsstype.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevoflt.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevoflt.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevoflt.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevoflt.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevoflt.h:$(GLSRC)stdio_.h
$(GLSRC)gdevoflt.h:$(GLSRC)gsccode.h
$(GLSRC)gdevoflt.h:$(GLSRC)stdint_.h
$(GLSRC)gdevoflt.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevoflt.h:$(GLSRC)gstypes.h
$(GLSRC)gdevoflt.h:$(GLSRC)std.h
$(GLSRC)gdevoflt.h:$(GLSRC)stdpre.h
$(GLSRC)gdevoflt.h:$(GLGEN)arch.h
$(GLSRC)gdevoflt.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevnup.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevnup.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevnup.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevnup.h:$(GLSRC)gstparam.h
$(GLSRC)gdevnup.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevnup.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevnup.h:$(GLSRC)gscsel.h
$(GLSRC)gdevnup.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevnup.h:$(GLSRC)gxdda.h
$(GLSRC)gdevnup.h:$(GLSRC)gxpath.h
$(GLSRC)gdevnup.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevnup.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevnup.h:$(GLSRC)gxftype.h
$(GLSRC)gdevnup.h:$(GLSRC)gsrect.h
$(GLSRC)gdevnup.h:$(GLSRC)gslparam.h
$(GLSRC)gdevnup.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevnup.h:$(GLSRC)gscpm.h
$(GLSRC)gdevnup.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevnup.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevnup.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevnup.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevnup.h:$(GLSRC)gsio.h
$(GLSRC)gdevnup.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevnup.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevnup.h:$(GLSRC)gscompt.h
$(GLSRC)gdevnup.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevnup.h:$(GLSRC)gspenum.h
$(GLSRC)gdevnup.h:$(GLSRC)gsparam.h
$(GLSRC)gdevnup.h:$(GLSRC)gp.h
$(GLSRC)gdevnup.h:$(GLSRC)memento.h
$(GLSRC)gdevnup.h:$(GLSRC)memory_.h
$(GLSRC)gdevnup.h:$(GLSRC)gsuid.h
$(GLSRC)gdevnup.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevnup.h:$(GLSRC)gxsync.h
$(GLSRC)gdevnup.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevnup.h:$(GLSRC)srdline.h
$(GLSRC)gdevnup.h:$(GLSRC)scommon.h
$(GLSRC)gdevnup.h:$(GLSRC)gsfname.h
$(GLSRC)gdevnup.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevnup.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevnup.h:$(GLSRC)gxarith.h
$(GLSRC)gdevnup.h:$(GLSRC)stat_.h
$(GLSRC)gdevnup.h:$(GLSRC)gpsync.h
$(GLSRC)gdevnup.h:$(GLSRC)gsstype.h
$(GLSRC)gdevnup.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevnup.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevnup.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevnup.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevnup.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevnup.h:$(GLSRC)stdio_.h
$(GLSRC)gdevnup.h:$(GLSRC)gsccode.h
$(GLSRC)gdevnup.h:$(GLSRC)stdint_.h
$(GLSRC)gdevnup.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevnup.h:$(GLSRC)gstypes.h
$(GLSRC)gdevnup.h:$(GLSRC)std.h
$(GLSRC)gdevnup.h:$(GLSRC)stdpre.h
$(GLSRC)gdevnup.h:$(GLGEN)arch.h
$(GLSRC)gdevnup.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfapi.h:$(GLSRC)gstext.h
$(GLSRC)gxfapi.h:$(GLSRC)gsfont.h
$(GLSRC)gxfapi.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfapi.h:$(GLSRC)gxpath.h
$(GLSRC)gxfapi.h:$(GLSRC)gscms.h
$(GLSRC)gxfapi.h:$(GLSRC)gsrect.h
$(GLSRC)gxfapi.h:$(GLSRC)gslparam.h
$(GLSRC)gxfapi.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfapi.h:$(GLSRC)gscpm.h
$(GLSRC)gxfapi.h:$(GLSRC)gscspace.h
$(GLSRC)gxfapi.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfapi.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfapi.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfapi.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfapi.h:$(GLSRC)gspenum.h
$(GLSRC)gxfapi.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfapi.h:$(GLSRC)gsparam.h
$(GLSRC)gxfapi.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfapi.h:$(GLSRC)memento.h
$(GLSRC)gxfapi.h:$(GLSRC)gxsync.h
$(GLSRC)gxfapi.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfapi.h:$(GLSRC)scommon.h
$(GLSRC)gxfapi.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfapi.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfapi.h:$(GLSRC)gxarith.h
$(GLSRC)gxfapi.h:$(GLSRC)gpsync.h
$(GLSRC)gxfapi.h:$(GLSRC)gsstype.h
$(GLSRC)gxfapi.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfapi.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfapi.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfapi.h:$(GLSRC)stdio_.h
$(GLSRC)gxfapi.h:$(GLSRC)gsccode.h
$(GLSRC)gxfapi.h:$(GLSRC)stdint_.h
$(GLSRC)gxfapi.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfapi.h:$(GLSRC)gstypes.h
$(GLSRC)gxfapi.h:$(GLSRC)std.h
$(GLSRC)gxfapi.h:$(GLSRC)stdpre.h
$(GLSRC)gxfapi.h:$(GLGEN)arch.h
$(GLSRC)gxfapi.h:$(GLSRC)gs_dll_call.h
$(GLSRC)wrfont.h:$(GLSRC)std.h
$(GLSRC)wrfont.h:$(GLSRC)stdpre.h
$(GLSRC)wrfont.h:$(GLGEN)arch.h
$(GLSRC)write_t1.h:$(GLSRC)gxfapi.h
$(GLSRC)write_t1.h:$(GLSRC)gstext.h
$(GLSRC)write_t1.h:$(GLSRC)gsfont.h
$(GLSRC)write_t1.h:$(GLSRC)gsdcolor.h
$(GLSRC)write_t1.h:$(GLSRC)gxpath.h
$(GLSRC)write_t1.h:$(GLSRC)gscms.h
$(GLSRC)write_t1.h:$(GLSRC)gsrect.h
$(GLSRC)write_t1.h:$(GLSRC)gslparam.h
$(GLSRC)write_t1.h:$(GLSRC)gsdevice.h
$(GLSRC)write_t1.h:$(GLSRC)gscpm.h
$(GLSRC)write_t1.h:$(GLSRC)gscspace.h
$(GLSRC)write_t1.h:$(GLSRC)gsgstate.h
$(GLSRC)write_t1.h:$(GLSRC)gsiparam.h
$(GLSRC)write_t1.h:$(GLSRC)gxfixed.h
$(GLSRC)write_t1.h:$(GLSRC)gsmatrix.h
$(GLSRC)write_t1.h:$(GLSRC)gspenum.h
$(GLSRC)write_t1.h:$(GLSRC)gxhttile.h
$(GLSRC)write_t1.h:$(GLSRC)gsparam.h
$(GLSRC)write_t1.h:$(GLSRC)gsrefct.h
$(GLSRC)write_t1.h:$(GLSRC)memento.h
$(GLSRC)write_t1.h:$(GLSRC)gxsync.h
$(GLSRC)write_t1.h:$(GLSRC)gxbitmap.h
$(GLSRC)write_t1.h:$(GLSRC)scommon.h
$(GLSRC)write_t1.h:$(GLSRC)gsbitmap.h
$(GLSRC)write_t1.h:$(GLSRC)gsccolor.h
$(GLSRC)write_t1.h:$(GLSRC)gxarith.h
$(GLSRC)write_t1.h:$(GLSRC)gpsync.h
$(GLSRC)write_t1.h:$(GLSRC)gsstype.h
$(GLSRC)write_t1.h:$(GLSRC)gsmemory.h
$(GLSRC)write_t1.h:$(GLSRC)gslibctx.h
$(GLSRC)write_t1.h:$(GLSRC)gxcindex.h
$(GLSRC)write_t1.h:$(GLSRC)stdio_.h
$(GLSRC)write_t1.h:$(GLSRC)gsccode.h
$(GLSRC)write_t1.h:$(GLSRC)stdint_.h
$(GLSRC)write_t1.h:$(GLSRC)gssprintf.h
$(GLSRC)write_t1.h:$(GLSRC)gstypes.h
$(GLSRC)write_t1.h:$(GLSRC)std.h
$(GLSRC)write_t1.h:$(GLSRC)stdpre.h
$(GLSRC)write_t1.h:$(GLGEN)arch.h
$(GLSRC)write_t1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)write_t2.h:$(GLSRC)gxfapi.h
$(GLSRC)write_t2.h:$(GLSRC)gstext.h
$(GLSRC)write_t2.h:$(GLSRC)gsfont.h
$(GLSRC)write_t2.h:$(GLSRC)gsdcolor.h
$(GLSRC)write_t2.h:$(GLSRC)gxpath.h
$(GLSRC)write_t2.h:$(GLSRC)gscms.h
$(GLSRC)write_t2.h:$(GLSRC)gsrect.h
$(GLSRC)write_t2.h:$(GLSRC)gslparam.h
$(GLSRC)write_t2.h:$(GLSRC)gsdevice.h
$(GLSRC)write_t2.h:$(GLSRC)gscpm.h
$(GLSRC)write_t2.h:$(GLSRC)gscspace.h
$(GLSRC)write_t2.h:$(GLSRC)gsgstate.h
$(GLSRC)write_t2.h:$(GLSRC)gsiparam.h
$(GLSRC)write_t2.h:$(GLSRC)gxfixed.h
$(GLSRC)write_t2.h:$(GLSRC)gsmatrix.h
$(GLSRC)write_t2.h:$(GLSRC)gspenum.h
$(GLSRC)write_t2.h:$(GLSRC)gxhttile.h
$(GLSRC)write_t2.h:$(GLSRC)gsparam.h
$(GLSRC)write_t2.h:$(GLSRC)gsrefct.h
$(GLSRC)write_t2.h:$(GLSRC)memento.h
$(GLSRC)write_t2.h:$(GLSRC)gxsync.h
$(GLSRC)write_t2.h:$(GLSRC)gxbitmap.h
$(GLSRC)write_t2.h:$(GLSRC)scommon.h
$(GLSRC)write_t2.h:$(GLSRC)gsbitmap.h
$(GLSRC)write_t2.h:$(GLSRC)gsccolor.h
$(GLSRC)write_t2.h:$(GLSRC)gxarith.h
$(GLSRC)write_t2.h:$(GLSRC)gpsync.h
$(GLSRC)write_t2.h:$(GLSRC)gsstype.h
$(GLSRC)write_t2.h:$(GLSRC)gsmemory.h
$(GLSRC)write_t2.h:$(GLSRC)gslibctx.h
$(GLSRC)write_t2.h:$(GLSRC)gxcindex.h
$(GLSRC)write_t2.h:$(GLSRC)stdio_.h
$(GLSRC)write_t2.h:$(GLSRC)gsccode.h
$(GLSRC)write_t2.h:$(GLSRC)stdint_.h
$(GLSRC)write_t2.h:$(GLSRC)gssprintf.h
$(GLSRC)write_t2.h:$(GLSRC)gstypes.h
$(GLSRC)write_t2.h:$(GLSRC)std.h
$(GLSRC)write_t2.h:$(GLSRC)stdpre.h
$(GLSRC)write_t2.h:$(GLGEN)arch.h
$(GLSRC)write_t2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)claptrap.h:$(GLSRC)gsmemory.h
$(GLSRC)claptrap.h:$(GLSRC)gslibctx.h
$(GLSRC)claptrap.h:$(GLSRC)stdio_.h
$(GLSRC)claptrap.h:$(GLSRC)gssprintf.h
$(GLSRC)claptrap.h:$(GLSRC)gstypes.h
$(GLSRC)claptrap.h:$(GLSRC)std.h
$(GLSRC)claptrap.h:$(GLSRC)stdpre.h
$(GLSRC)claptrap.h:$(GLGEN)arch.h
$(GLSRC)claptrap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ets.h:$(GLSRC)stdpre.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxgetbit.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxcmap.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxtext.h
$(GLSRC)gxdownscale.h:$(GLSRC)gstext.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxdownscale.h:$(GLSRC)gstparam.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxfmap.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsfunc.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxcspace.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxrplane.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscsel.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxfcache.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsfont.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsimage.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxbcache.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsropt.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxdda.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxpath.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxfrac.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxtmap.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxftype.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscms.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsrect.h
$(GLSRC)gxdownscale.h:$(GLSRC)gslparam.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsdevice.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscpm.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscspace.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsgstate.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsxfont.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsiparam.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxfixed.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscompt.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxdownscale.h:$(GLSRC)gspenum.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxhttile.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsparam.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsrefct.h
$(GLSRC)gxdownscale.h:$(GLSRC)gp.h
$(GLSRC)gxdownscale.h:$(GLSRC)memento.h
$(GLSRC)gxdownscale.h:$(GLSRC)memory_.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsuid.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsstruct.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxsync.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxdownscale.h:$(GLSRC)claptrap.h
$(GLSRC)gxdownscale.h:$(GLSRC)srdline.h
$(GLSRC)gxdownscale.h:$(GLSRC)scommon.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsccolor.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxarith.h
$(GLSRC)gxdownscale.h:$(GLSRC)stat_.h
$(GLSRC)gxdownscale.h:$(GLSRC)gpsync.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsstype.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsmemory.h
$(GLSRC)gxdownscale.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxdownscale.h:$(GLSRC)gscdefs.h
$(GLSRC)gxdownscale.h:$(GLSRC)gslibctx.h
$(GLSRC)gxdownscale.h:$(GLSRC)ctype_.h
$(GLSRC)gxdownscale.h:$(GLSRC)gxcindex.h
$(GLSRC)gxdownscale.h:$(GLSRC)stdio_.h
$(GLSRC)gxdownscale.h:$(GLSRC)gsccode.h
$(GLSRC)gxdownscale.h:$(GLSRC)stdint_.h
$(GLSRC)gxdownscale.h:$(GLSRC)gssprintf.h
$(GLSRC)gxdownscale.h:$(GLSRC)gstypes.h
$(GLSRC)gxdownscale.h:$(GLSRC)std.h
$(GLSRC)gxdownscale.h:$(GLSRC)stdpre.h
$(GLSRC)gxdownscale.h:$(GLGEN)arch.h
$(GLSRC)gxdownscale.h:$(GLSRC)gs_dll_call.h
$(GLSRC)strmio.h:$(GLSRC)scommon.h
$(GLSRC)strmio.h:$(GLSRC)gsstype.h
$(GLSRC)strmio.h:$(GLSRC)gsmemory.h
$(GLSRC)strmio.h:$(GLSRC)gslibctx.h
$(GLSRC)strmio.h:$(GLSRC)stdio_.h
$(GLSRC)strmio.h:$(GLSRC)stdint_.h
$(GLSRC)strmio.h:$(GLSRC)gssprintf.h
$(GLSRC)strmio.h:$(GLSRC)gstypes.h
$(GLSRC)strmio.h:$(GLSRC)std.h
$(GLSRC)strmio.h:$(GLSRC)stdpre.h
$(GLSRC)strmio.h:$(GLGEN)arch.h
$(GLSRC)strmio.h:$(GLSRC)gs_dll_call.h
$(GLSRC)sdcparam.h:$(GLSRC)sdct.h
$(GLSRC)sdcparam.h:$(GLSRC)setjmp_.h
$(GLSRC)sdcparam.h:$(GLSRC)strimpl.h
$(GLSRC)sdcparam.h:$(GLSRC)gscms.h
$(GLSRC)sdcparam.h:$(GLSRC)gsdevice.h
$(GLSRC)sdcparam.h:$(GLSRC)gscspace.h
$(GLSRC)sdcparam.h:$(GLSRC)gsgstate.h
$(GLSRC)sdcparam.h:$(GLSRC)gsiparam.h
$(GLSRC)sdcparam.h:$(GLSRC)gsmatrix.h
$(GLSRC)sdcparam.h:$(GLSRC)gsparam.h
$(GLSRC)sdcparam.h:$(GLSRC)gsrefct.h
$(GLSRC)sdcparam.h:$(GLSRC)memento.h
$(GLSRC)sdcparam.h:$(GLSRC)gsstruct.h
$(GLSRC)sdcparam.h:$(GLSRC)gxsync.h
$(GLSRC)sdcparam.h:$(GLSRC)gxbitmap.h
$(GLSRC)sdcparam.h:$(GLSRC)scommon.h
$(GLSRC)sdcparam.h:$(GLSRC)gsbitmap.h
$(GLSRC)sdcparam.h:$(GLSRC)gsccolor.h
$(GLSRC)sdcparam.h:$(GLSRC)gpsync.h
$(GLSRC)sdcparam.h:$(GLSRC)gsstype.h
$(GLSRC)sdcparam.h:$(GLSRC)gsmemory.h
$(GLSRC)sdcparam.h:$(GLSRC)gslibctx.h
$(GLSRC)sdcparam.h:$(GLSRC)stdio_.h
$(GLSRC)sdcparam.h:$(GLSRC)stdint_.h
$(GLSRC)sdcparam.h:$(GLSRC)gssprintf.h
$(GLSRC)sdcparam.h:$(GLSRC)gstypes.h
$(GLSRC)sdcparam.h:$(GLSRC)std.h
$(GLSRC)sdcparam.h:$(GLSRC)stdpre.h
$(GLSRC)sdcparam.h:$(GLGEN)arch.h
$(GLSRC)sdcparam.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ssha2.h:$(GLSRC)sha2.h
$(GLSRC)ssha2.h:$(GLSRC)scommon.h
$(GLSRC)ssha2.h:$(GLSRC)gsstype.h
$(GLSRC)ssha2.h:$(GLSRC)gsmemory.h
$(GLSRC)ssha2.h:$(GLSRC)gslibctx.h
$(GLSRC)ssha2.h:$(GLSRC)stdio_.h
$(GLSRC)ssha2.h:$(GLSRC)stdint_.h
$(GLSRC)ssha2.h:$(GLSRC)gssprintf.h
$(GLSRC)ssha2.h:$(GLSRC)gstypes.h
$(GLSRC)ssha2.h:$(GLSRC)std.h
$(GLSRC)ssha2.h:$(GLSRC)stdpre.h
$(GLSRC)ssha2.h:$(GLGEN)arch.h
$(GLSRC)ssha2.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevprn.h:$(GLSRC)string_.h
$(GLSRC)gdevprn.h:$(GLSRC)gsstrtok.h
$(GLSRC)gdevprn.h:$(GLSRC)gxclthrd.h
$(GLSRC)gdevprn.h:$(GLSRC)gxclpage.h
$(GLSRC)gdevprn.h:$(GLSRC)gxclist.h
$(GLSRC)gdevprn.h:$(GLSRC)gxgstate.h
$(GLSRC)gdevprn.h:$(GLSRC)gstrans.h
$(GLSRC)gdevprn.h:$(GLSRC)gdevp14.h
$(GLSRC)gdevprn.h:$(GLSRC)gxline.h
$(GLSRC)gdevprn.h:$(GLSRC)gsht1.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcomp.h
$(GLSRC)gdevprn.h:$(GLSRC)math_.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcolor2.h
$(GLSRC)gdevprn.h:$(GLSRC)gxpcolor.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdevmem.h
$(GLSRC)gdevprn.h:$(GLSRC)gdevdevn.h
$(GLSRC)gdevprn.h:$(GLSRC)gx.h
$(GLSRC)gdevprn.h:$(GLSRC)gxclipsr.h
$(GLSRC)gdevprn.h:$(GLSRC)gdebug.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdcolor.h
$(GLSRC)gdevprn.h:$(GLSRC)gxband.h
$(GLSRC)gdevprn.h:$(GLSRC)gxblend.h
$(GLSRC)gdevprn.h:$(GLSRC)gscolor2.h
$(GLSRC)gdevprn.h:$(GLSRC)gxmatrix.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcpath.h
$(GLSRC)gdevprn.h:$(GLSRC)gsht.h
$(GLSRC)gdevprn.h:$(GLSRC)gsequivc.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevprn.h:$(GLSRC)gxpcache.h
$(GLSRC)gdevprn.h:$(GLSRC)gscindex.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevprn.h:$(GLSRC)gsptype1.h
$(GLSRC)gdevprn.h:$(GLSRC)gscie.h
$(GLSRC)gdevprn.h:$(GLSRC)gxtext.h
$(GLSRC)gdevprn.h:$(GLSRC)gstext.h
$(GLSRC)gdevprn.h:$(GLSRC)gxstate.h
$(GLSRC)gdevprn.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevprn.h:$(GLSRC)gstparam.h
$(GLSRC)gdevprn.h:$(GLSRC)gspcolor.h
$(GLSRC)gdevprn.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevprn.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevprn.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevprn.h:$(GLSRC)gxctable.h
$(GLSRC)gdevprn.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevprn.h:$(GLSRC)gscsel.h
$(GLSRC)gdevprn.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevprn.h:$(GLSRC)gsfont.h
$(GLSRC)gdevprn.h:$(GLSRC)gsimage.h
$(GLSRC)gdevprn.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevprn.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevprn.h:$(GLSRC)gsropt.h
$(GLSRC)gdevprn.h:$(GLSRC)gxdda.h
$(GLSRC)gdevprn.h:$(GLSRC)gxpath.h
$(GLSRC)gdevprn.h:$(GLSRC)gxiclass.h
$(GLSRC)gdevprn.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevprn.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevprn.h:$(GLSRC)gxftype.h
$(GLSRC)gdevprn.h:$(GLSRC)gscms.h
$(GLSRC)gdevprn.h:$(GLSRC)gsrect.h
$(GLSRC)gdevprn.h:$(GLSRC)gslparam.h
$(GLSRC)gdevprn.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevprn.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gdevprn.h:$(GLSRC)gscpm.h
$(GLSRC)gdevprn.h:$(GLSRC)gscspace.h
$(GLSRC)gdevprn.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevprn.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevprn.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevprn.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevprn.h:$(GLSRC)gsio.h
$(GLSRC)gdevprn.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevprn.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevprn.h:$(GLSRC)gxclio.h
$(GLSRC)gdevprn.h:$(GLSRC)gscompt.h
$(GLSRC)gdevprn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevprn.h:$(GLSRC)gspenum.h
$(GLSRC)gdevprn.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevprn.h:$(GLSRC)gsparam.h
$(GLSRC)gdevprn.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevprn.h:$(GLSRC)gp.h
$(GLSRC)gdevprn.h:$(GLSRC)memento.h
$(GLSRC)gdevprn.h:$(GLSRC)memory_.h
$(GLSRC)gdevprn.h:$(GLSRC)gsutil.h
$(GLSRC)gdevprn.h:$(GLSRC)gsuid.h
$(GLSRC)gdevprn.h:$(GLSRC)gsstrl.h
$(GLSRC)gdevprn.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevprn.h:$(GLSRC)gdbflags.h
$(GLSRC)gdevprn.h:$(GLSRC)gxsync.h
$(GLSRC)gdevprn.h:$(GLSRC)gserrors.h
$(GLSRC)gdevprn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevprn.h:$(GLSRC)vmsmath.h
$(GLSRC)gdevprn.h:$(GLSRC)srdline.h
$(GLSRC)gdevprn.h:$(GLSRC)scommon.h
$(GLSRC)gdevprn.h:$(GLSRC)gsfname.h
$(GLSRC)gdevprn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevprn.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevprn.h:$(GLSRC)gxarith.h
$(GLSRC)gdevprn.h:$(GLSRC)stat_.h
$(GLSRC)gdevprn.h:$(GLSRC)gpsync.h
$(GLSRC)gdevprn.h:$(GLSRC)gsstype.h
$(GLSRC)gdevprn.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevprn.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevprn.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevprn.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevprn.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevprn.h:$(GLSRC)stdio_.h
$(GLSRC)gdevprn.h:$(GLSRC)gsccode.h
$(GLSRC)gdevprn.h:$(GLSRC)stdint_.h
$(GLSRC)gdevprn.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevprn.h:$(GLSRC)gstypes.h
$(GLSRC)gdevprn.h:$(GLSRC)std.h
$(GLSRC)gdevprn.h:$(GLSRC)stdpre.h
$(GLSRC)gdevprn.h:$(GLGEN)arch.h
$(GLSRC)gdevprn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxtext.h
$(GLSRC)gdevmplt.h:$(GLSRC)gstext.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevmplt.h:$(GLSRC)gstparam.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscsel.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsfont.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsimage.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsropt.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxdda.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxpath.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxftype.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscms.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsrect.h
$(GLSRC)gdevmplt.h:$(GLSRC)gslparam.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscpm.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscspace.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsio.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscompt.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevmplt.h:$(GLSRC)gspenum.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsparam.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevmplt.h:$(GLSRC)gp.h
$(GLSRC)gdevmplt.h:$(GLSRC)memento.h
$(GLSRC)gdevmplt.h:$(GLSRC)memory_.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsuid.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxsync.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevmplt.h:$(GLSRC)srdline.h
$(GLSRC)gdevmplt.h:$(GLSRC)scommon.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsfname.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxarith.h
$(GLSRC)gdevmplt.h:$(GLSRC)stat_.h
$(GLSRC)gdevmplt.h:$(GLSRC)gpsync.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsstype.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevmplt.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevmplt.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevmplt.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevmplt.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevmplt.h:$(GLSRC)stdio_.h
$(GLSRC)gdevmplt.h:$(GLSRC)gsccode.h
$(GLSRC)gdevmplt.h:$(GLSRC)stdint_.h
$(GLSRC)gdevmplt.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevmplt.h:$(GLSRC)gstypes.h
$(GLSRC)gdevmplt.h:$(GLSRC)std.h
$(GLSRC)gdevmplt.h:$(GLSRC)stdpre.h
$(GLSRC)gdevmplt.h:$(GLGEN)arch.h
$(GLSRC)gdevmplt.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevppla.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gdevppla.h:$(GLSRC)gxband.h
$(GLSRC)gdevppla.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevppla.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevppla.h:$(GLSRC)gxtext.h
$(GLSRC)gdevppla.h:$(GLSRC)gstext.h
$(GLSRC)gdevppla.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevppla.h:$(GLSRC)gstparam.h
$(GLSRC)gdevppla.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevppla.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevppla.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevppla.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevppla.h:$(GLSRC)gscsel.h
$(GLSRC)gdevppla.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevppla.h:$(GLSRC)gsfont.h
$(GLSRC)gdevppla.h:$(GLSRC)gsimage.h
$(GLSRC)gdevppla.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevppla.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevppla.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevppla.h:$(GLSRC)gsropt.h
$(GLSRC)gdevppla.h:$(GLSRC)gxdda.h
$(GLSRC)gdevppla.h:$(GLSRC)gxpath.h
$(GLSRC)gdevppla.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevppla.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevppla.h:$(GLSRC)gxftype.h
$(GLSRC)gdevppla.h:$(GLSRC)gscms.h
$(GLSRC)gdevppla.h:$(GLSRC)gsrect.h
$(GLSRC)gdevppla.h:$(GLSRC)gslparam.h
$(GLSRC)gdevppla.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevppla.h:$(GLSRC)gscpm.h
$(GLSRC)gdevppla.h:$(GLSRC)gscspace.h
$(GLSRC)gdevppla.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevppla.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevppla.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevppla.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevppla.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevppla.h:$(GLSRC)gxclio.h
$(GLSRC)gdevppla.h:$(GLSRC)gscompt.h
$(GLSRC)gdevppla.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevppla.h:$(GLSRC)gspenum.h
$(GLSRC)gdevppla.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevppla.h:$(GLSRC)gsparam.h
$(GLSRC)gdevppla.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevppla.h:$(GLSRC)gp.h
$(GLSRC)gdevppla.h:$(GLSRC)memento.h
$(GLSRC)gdevppla.h:$(GLSRC)memory_.h
$(GLSRC)gdevppla.h:$(GLSRC)gsuid.h
$(GLSRC)gdevppla.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevppla.h:$(GLSRC)gxsync.h
$(GLSRC)gdevppla.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevppla.h:$(GLSRC)srdline.h
$(GLSRC)gdevppla.h:$(GLSRC)scommon.h
$(GLSRC)gdevppla.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevppla.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevppla.h:$(GLSRC)gxarith.h
$(GLSRC)gdevppla.h:$(GLSRC)stat_.h
$(GLSRC)gdevppla.h:$(GLSRC)gpsync.h
$(GLSRC)gdevppla.h:$(GLSRC)gsstype.h
$(GLSRC)gdevppla.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevppla.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevppla.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevppla.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevppla.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevppla.h:$(GLSRC)stdio_.h
$(GLSRC)gdevppla.h:$(GLSRC)gsccode.h
$(GLSRC)gdevppla.h:$(GLSRC)stdint_.h
$(GLSRC)gdevppla.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevppla.h:$(GLSRC)gstypes.h
$(GLSRC)gdevppla.h:$(GLSRC)std.h
$(GLSRC)gdevppla.h:$(GLSRC)stdpre.h
$(GLSRC)gdevppla.h:$(GLGEN)arch.h
$(GLSRC)gdevppla.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsiparam.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsiparm3.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsiparm3.h:$(GLSRC)scommon.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsccolor.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsstype.h
$(GLSRC)gsiparm3.h:$(GLSRC)gsmemory.h
$(GLSRC)gsiparm3.h:$(GLSRC)gslibctx.h
$(GLSRC)gsiparm3.h:$(GLSRC)stdio_.h
$(GLSRC)gsiparm3.h:$(GLSRC)stdint_.h
$(GLSRC)gsiparm3.h:$(GLSRC)gssprintf.h
$(GLSRC)gsiparm3.h:$(GLSRC)gstypes.h
$(GLSRC)gsiparm3.h:$(GLSRC)std.h
$(GLSRC)gsiparm3.h:$(GLSRC)stdpre.h
$(GLSRC)gsiparm3.h:$(GLGEN)arch.h
$(GLSRC)gsiparm3.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsiparam.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsiparm4.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsiparm4.h:$(GLSRC)scommon.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsccolor.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsstype.h
$(GLSRC)gsiparm4.h:$(GLSRC)gsmemory.h
$(GLSRC)gsiparm4.h:$(GLSRC)gslibctx.h
$(GLSRC)gsiparm4.h:$(GLSRC)stdio_.h
$(GLSRC)gsiparm4.h:$(GLSRC)stdint_.h
$(GLSRC)gsiparm4.h:$(GLSRC)gssprintf.h
$(GLSRC)gsiparm4.h:$(GLSRC)gstypes.h
$(GLSRC)gsiparm4.h:$(GLSRC)std.h
$(GLSRC)gsiparm4.h:$(GLSRC)stdpre.h
$(GLSRC)gsiparm4.h:$(GLGEN)arch.h
$(GLSRC)gsiparm4.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gximage3.h:$(GLSRC)gsiparm3.h
$(GLSRC)gximage3.h:$(GLSRC)gxiparam.h
$(GLSRC)gximage3.h:$(GLSRC)gxdevcli.h
$(GLSRC)gximage3.h:$(GLSRC)gxcmap.h
$(GLSRC)gximage3.h:$(GLSRC)gxtext.h
$(GLSRC)gximage3.h:$(GLSRC)gstext.h
$(GLSRC)gximage3.h:$(GLSRC)gsnamecl.h
$(GLSRC)gximage3.h:$(GLSRC)gstparam.h
$(GLSRC)gximage3.h:$(GLSRC)gxfmap.h
$(GLSRC)gximage3.h:$(GLSRC)gsfunc.h
$(GLSRC)gximage3.h:$(GLSRC)gxcspace.h
$(GLSRC)gximage3.h:$(GLSRC)gxrplane.h
$(GLSRC)gximage3.h:$(GLSRC)gscsel.h
$(GLSRC)gximage3.h:$(GLSRC)gxfcache.h
$(GLSRC)gximage3.h:$(GLSRC)gsfont.h
$(GLSRC)gximage3.h:$(GLSRC)gsimage.h
$(GLSRC)gximage3.h:$(GLSRC)gsdcolor.h
$(GLSRC)gximage3.h:$(GLSRC)gxcvalue.h
$(GLSRC)gximage3.h:$(GLSRC)gxbcache.h
$(GLSRC)gximage3.h:$(GLSRC)gsropt.h
$(GLSRC)gximage3.h:$(GLSRC)gxdda.h
$(GLSRC)gximage3.h:$(GLSRC)gxpath.h
$(GLSRC)gximage3.h:$(GLSRC)gxfrac.h
$(GLSRC)gximage3.h:$(GLSRC)gxtmap.h
$(GLSRC)gximage3.h:$(GLSRC)gxftype.h
$(GLSRC)gximage3.h:$(GLSRC)gscms.h
$(GLSRC)gximage3.h:$(GLSRC)gsrect.h
$(GLSRC)gximage3.h:$(GLSRC)gslparam.h
$(GLSRC)gximage3.h:$(GLSRC)gsdevice.h
$(GLSRC)gximage3.h:$(GLSRC)gscpm.h
$(GLSRC)gximage3.h:$(GLSRC)gscspace.h
$(GLSRC)gximage3.h:$(GLSRC)gsgstate.h
$(GLSRC)gximage3.h:$(GLSRC)gsxfont.h
$(GLSRC)gximage3.h:$(GLSRC)gsdsrc.h
$(GLSRC)gximage3.h:$(GLSRC)gsiparam.h
$(GLSRC)gximage3.h:$(GLSRC)gxfixed.h
$(GLSRC)gximage3.h:$(GLSRC)gscompt.h
$(GLSRC)gximage3.h:$(GLSRC)gsmatrix.h
$(GLSRC)gximage3.h:$(GLSRC)gspenum.h
$(GLSRC)gximage3.h:$(GLSRC)gxhttile.h
$(GLSRC)gximage3.h:$(GLSRC)gsparam.h
$(GLSRC)gximage3.h:$(GLSRC)gsrefct.h
$(GLSRC)gximage3.h:$(GLSRC)gp.h
$(GLSRC)gximage3.h:$(GLSRC)memento.h
$(GLSRC)gximage3.h:$(GLSRC)memory_.h
$(GLSRC)gximage3.h:$(GLSRC)gsuid.h
$(GLSRC)gximage3.h:$(GLSRC)gsstruct.h
$(GLSRC)gximage3.h:$(GLSRC)gxsync.h
$(GLSRC)gximage3.h:$(GLSRC)gxbitmap.h
$(GLSRC)gximage3.h:$(GLSRC)srdline.h
$(GLSRC)gximage3.h:$(GLSRC)scommon.h
$(GLSRC)gximage3.h:$(GLSRC)gsbitmap.h
$(GLSRC)gximage3.h:$(GLSRC)gsccolor.h
$(GLSRC)gximage3.h:$(GLSRC)gxarith.h
$(GLSRC)gximage3.h:$(GLSRC)stat_.h
$(GLSRC)gximage3.h:$(GLSRC)gpsync.h
$(GLSRC)gximage3.h:$(GLSRC)gsstype.h
$(GLSRC)gximage3.h:$(GLSRC)gsmemory.h
$(GLSRC)gximage3.h:$(GLSRC)gpgetenv.h
$(GLSRC)gximage3.h:$(GLSRC)gscdefs.h
$(GLSRC)gximage3.h:$(GLSRC)gslibctx.h
$(GLSRC)gximage3.h:$(GLSRC)gxcindex.h
$(GLSRC)gximage3.h:$(GLSRC)stdio_.h
$(GLSRC)gximage3.h:$(GLSRC)gsccode.h
$(GLSRC)gximage3.h:$(GLSRC)stdint_.h
$(GLSRC)gximage3.h:$(GLSRC)gssprintf.h
$(GLSRC)gximage3.h:$(GLSRC)gstypes.h
$(GLSRC)gximage3.h:$(GLSRC)std.h
$(GLSRC)gximage3.h:$(GLSRC)stdpre.h
$(GLSRC)gximage3.h:$(GLGEN)arch.h
$(GLSRC)gximage3.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdht.h
$(GLSRC)gxcldev.h:$(GLSRC)srlx.h
$(GLSRC)gxcldev.h:$(GLSRC)gxht.h
$(GLSRC)gxcldev.h:$(GLSRC)gxhttype.h
$(GLSRC)gxcldev.h:$(GLSRC)gxclist.h
$(GLSRC)gxcldev.h:$(GLSRC)gxgstate.h
$(GLSRC)gxcldev.h:$(GLSRC)gstrans.h
$(GLSRC)gxcldev.h:$(GLSRC)gdevp14.h
$(GLSRC)gxcldev.h:$(GLSRC)gxline.h
$(GLSRC)gxcldev.h:$(GLSRC)gsht1.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcomp.h
$(GLSRC)gxcldev.h:$(GLSRC)math_.h
$(GLSRC)gxcldev.h:$(GLSRC)scfx.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxcldev.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxcldev.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxcldev.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxcldev.h:$(GLSRC)gxband.h
$(GLSRC)gxcldev.h:$(GLSRC)gxblend.h
$(GLSRC)gxcldev.h:$(GLSRC)shc.h
$(GLSRC)gxcldev.h:$(GLSRC)gscolor2.h
$(GLSRC)gxcldev.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdevice.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcpath.h
$(GLSRC)gxcldev.h:$(GLSRC)gsht.h
$(GLSRC)gxcldev.h:$(GLSRC)gsequivc.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxcldev.h:$(GLSRC)gxpcache.h
$(GLSRC)gxcldev.h:$(GLSRC)gscindex.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcmap.h
$(GLSRC)gxcldev.h:$(GLSRC)gsptype1.h
$(GLSRC)gxcldev.h:$(GLSRC)gscie.h
$(GLSRC)gxcldev.h:$(GLSRC)gxtext.h
$(GLSRC)gxcldev.h:$(GLSRC)gstext.h
$(GLSRC)gxcldev.h:$(GLSRC)gxstate.h
$(GLSRC)gxcldev.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxcldev.h:$(GLSRC)gstparam.h
$(GLSRC)gxcldev.h:$(GLSRC)gspcolor.h
$(GLSRC)gxcldev.h:$(GLSRC)gxfmap.h
$(GLSRC)gxcldev.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxcldev.h:$(GLSRC)gsfunc.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcspace.h
$(GLSRC)gxcldev.h:$(GLSRC)gxctable.h
$(GLSRC)gxcldev.h:$(GLSRC)strimpl.h
$(GLSRC)gxcldev.h:$(GLSRC)gxrplane.h
$(GLSRC)gxcldev.h:$(GLSRC)gscsel.h
$(GLSRC)gxcldev.h:$(GLSRC)gxfcache.h
$(GLSRC)gxcldev.h:$(GLSRC)gsfont.h
$(GLSRC)gxcldev.h:$(GLSRC)gsimage.h
$(GLSRC)gxcldev.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxcldev.h:$(GLSRC)gxbcache.h
$(GLSRC)gxcldev.h:$(GLSRC)gsropt.h
$(GLSRC)gxcldev.h:$(GLSRC)gxdda.h
$(GLSRC)gxcldev.h:$(GLSRC)gxpath.h
$(GLSRC)gxcldev.h:$(GLSRC)gxiclass.h
$(GLSRC)gxcldev.h:$(GLSRC)gxfrac.h
$(GLSRC)gxcldev.h:$(GLSRC)gxtmap.h
$(GLSRC)gxcldev.h:$(GLSRC)gxftype.h
$(GLSRC)gxcldev.h:$(GLSRC)gscms.h
$(GLSRC)gxcldev.h:$(GLSRC)gsrect.h
$(GLSRC)gxcldev.h:$(GLSRC)gslparam.h
$(GLSRC)gxcldev.h:$(GLSRC)gsdevice.h
$(GLSRC)gxcldev.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxcldev.h:$(GLSRC)gscpm.h
$(GLSRC)gxcldev.h:$(GLSRC)gscspace.h
$(GLSRC)gxcldev.h:$(GLSRC)gsgstate.h
$(GLSRC)gxcldev.h:$(GLSRC)gxstdio.h
$(GLSRC)gxcldev.h:$(GLSRC)gsxfont.h
$(GLSRC)gxcldev.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxcldev.h:$(GLSRC)gsio.h
$(GLSRC)gxcldev.h:$(GLSRC)gsiparam.h
$(GLSRC)gxcldev.h:$(GLSRC)gxfixed.h
$(GLSRC)gxcldev.h:$(GLSRC)gxclio.h
$(GLSRC)gxcldev.h:$(GLSRC)gscompt.h
$(GLSRC)gxcldev.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxcldev.h:$(GLSRC)gspenum.h
$(GLSRC)gxcldev.h:$(GLSRC)gxhttile.h
$(GLSRC)gxcldev.h:$(GLSRC)gsparam.h
$(GLSRC)gxcldev.h:$(GLSRC)gsrefct.h
$(GLSRC)gxcldev.h:$(GLSRC)gp.h
$(GLSRC)gxcldev.h:$(GLSRC)memento.h
$(GLSRC)gxcldev.h:$(GLSRC)memory_.h
$(GLSRC)gxcldev.h:$(GLSRC)gsuid.h
$(GLSRC)gxcldev.h:$(GLSRC)gsstruct.h
$(GLSRC)gxcldev.h:$(GLSRC)gxsync.h
$(GLSRC)gxcldev.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxcldev.h:$(GLSRC)vmsmath.h
$(GLSRC)gxcldev.h:$(GLSRC)srdline.h
$(GLSRC)gxcldev.h:$(GLSRC)scommon.h
$(GLSRC)gxcldev.h:$(GLSRC)gsfname.h
$(GLSRC)gxcldev.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxcldev.h:$(GLSRC)gsccolor.h
$(GLSRC)gxcldev.h:$(GLSRC)gxarith.h
$(GLSRC)gxcldev.h:$(GLSRC)stat_.h
$(GLSRC)gxcldev.h:$(GLSRC)gpsync.h
$(GLSRC)gxcldev.h:$(GLSRC)gsstype.h
$(GLSRC)gxcldev.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcldev.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxcldev.h:$(GLSRC)gscdefs.h
$(GLSRC)gxcldev.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcldev.h:$(GLSRC)gxcindex.h
$(GLSRC)gxcldev.h:$(GLSRC)stdio_.h
$(GLSRC)gxcldev.h:$(GLSRC)gsccode.h
$(GLSRC)gxcldev.h:$(GLSRC)stdint_.h
$(GLSRC)gxcldev.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcldev.h:$(GLSRC)gsbittab.h
$(GLSRC)gxcldev.h:$(GLSRC)gstypes.h
$(GLSRC)gxcldev.h:$(GLSRC)std.h
$(GLSRC)gxcldev.h:$(GLSRC)stdpre.h
$(GLSRC)gxcldev.h:$(GLGEN)arch.h
$(GLSRC)gxcldev.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclpage.h:$(GLSRC)gxclist.h
$(GLSRC)gxclpage.h:$(GLSRC)gxgstate.h
$(GLSRC)gxclpage.h:$(GLSRC)gstrans.h
$(GLSRC)gxclpage.h:$(GLSRC)gdevp14.h
$(GLSRC)gxclpage.h:$(GLSRC)gxline.h
$(GLSRC)gxclpage.h:$(GLSRC)gsht1.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcomp.h
$(GLSRC)gxclpage.h:$(GLSRC)math_.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxclpage.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclpage.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxclpage.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxclpage.h:$(GLSRC)gxband.h
$(GLSRC)gxclpage.h:$(GLSRC)gxblend.h
$(GLSRC)gxclpage.h:$(GLSRC)gscolor2.h
$(GLSRC)gxclpage.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdevice.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcpath.h
$(GLSRC)gxclpage.h:$(GLSRC)gsht.h
$(GLSRC)gxclpage.h:$(GLSRC)gsequivc.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclpage.h:$(GLSRC)gxpcache.h
$(GLSRC)gxclpage.h:$(GLSRC)gscindex.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclpage.h:$(GLSRC)gsptype1.h
$(GLSRC)gxclpage.h:$(GLSRC)gscie.h
$(GLSRC)gxclpage.h:$(GLSRC)gxtext.h
$(GLSRC)gxclpage.h:$(GLSRC)gstext.h
$(GLSRC)gxclpage.h:$(GLSRC)gxstate.h
$(GLSRC)gxclpage.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclpage.h:$(GLSRC)gstparam.h
$(GLSRC)gxclpage.h:$(GLSRC)gspcolor.h
$(GLSRC)gxclpage.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclpage.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxclpage.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclpage.h:$(GLSRC)gxctable.h
$(GLSRC)gxclpage.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclpage.h:$(GLSRC)gscsel.h
$(GLSRC)gxclpage.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclpage.h:$(GLSRC)gsfont.h
$(GLSRC)gxclpage.h:$(GLSRC)gsimage.h
$(GLSRC)gxclpage.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclpage.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclpage.h:$(GLSRC)gsropt.h
$(GLSRC)gxclpage.h:$(GLSRC)gxdda.h
$(GLSRC)gxclpage.h:$(GLSRC)gxpath.h
$(GLSRC)gxclpage.h:$(GLSRC)gxiclass.h
$(GLSRC)gxclpage.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclpage.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclpage.h:$(GLSRC)gxftype.h
$(GLSRC)gxclpage.h:$(GLSRC)gscms.h
$(GLSRC)gxclpage.h:$(GLSRC)gsrect.h
$(GLSRC)gxclpage.h:$(GLSRC)gslparam.h
$(GLSRC)gxclpage.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclpage.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxclpage.h:$(GLSRC)gscpm.h
$(GLSRC)gxclpage.h:$(GLSRC)gscspace.h
$(GLSRC)gxclpage.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclpage.h:$(GLSRC)gxstdio.h
$(GLSRC)gxclpage.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclpage.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclpage.h:$(GLSRC)gsio.h
$(GLSRC)gxclpage.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclpage.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclpage.h:$(GLSRC)gxclio.h
$(GLSRC)gxclpage.h:$(GLSRC)gscompt.h
$(GLSRC)gxclpage.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclpage.h:$(GLSRC)gspenum.h
$(GLSRC)gxclpage.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclpage.h:$(GLSRC)gsparam.h
$(GLSRC)gxclpage.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclpage.h:$(GLSRC)gp.h
$(GLSRC)gxclpage.h:$(GLSRC)memento.h
$(GLSRC)gxclpage.h:$(GLSRC)memory_.h
$(GLSRC)gxclpage.h:$(GLSRC)gsuid.h
$(GLSRC)gxclpage.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclpage.h:$(GLSRC)gxsync.h
$(GLSRC)gxclpage.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclpage.h:$(GLSRC)vmsmath.h
$(GLSRC)gxclpage.h:$(GLSRC)srdline.h
$(GLSRC)gxclpage.h:$(GLSRC)scommon.h
$(GLSRC)gxclpage.h:$(GLSRC)gsfname.h
$(GLSRC)gxclpage.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclpage.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclpage.h:$(GLSRC)gxarith.h
$(GLSRC)gxclpage.h:$(GLSRC)stat_.h
$(GLSRC)gxclpage.h:$(GLSRC)gpsync.h
$(GLSRC)gxclpage.h:$(GLSRC)gsstype.h
$(GLSRC)gxclpage.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclpage.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclpage.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclpage.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclpage.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclpage.h:$(GLSRC)stdio_.h
$(GLSRC)gxclpage.h:$(GLSRC)gsccode.h
$(GLSRC)gxclpage.h:$(GLSRC)stdint_.h
$(GLSRC)gxclpage.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclpage.h:$(GLSRC)gstypes.h
$(GLSRC)gxclpage.h:$(GLSRC)std.h
$(GLSRC)gxclpage.h:$(GLSRC)stdpre.h
$(GLSRC)gxclpage.h:$(GLGEN)arch.h
$(GLSRC)gxclpage.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcldev.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdht.h
$(GLSRC)gxclpath.h:$(GLSRC)srlx.h
$(GLSRC)gxclpath.h:$(GLSRC)gxht.h
$(GLSRC)gxclpath.h:$(GLSRC)gxhttype.h
$(GLSRC)gxclpath.h:$(GLSRC)gxclist.h
$(GLSRC)gxclpath.h:$(GLSRC)gxgstate.h
$(GLSRC)gxclpath.h:$(GLSRC)gstrans.h
$(GLSRC)gxclpath.h:$(GLSRC)gdevp14.h
$(GLSRC)gxclpath.h:$(GLSRC)gxline.h
$(GLSRC)gxclpath.h:$(GLSRC)gsht1.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcomp.h
$(GLSRC)gxclpath.h:$(GLSRC)math_.h
$(GLSRC)gxclpath.h:$(GLSRC)scfx.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcolor2.h
$(GLSRC)gxclpath.h:$(GLSRC)gxpcolor.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdevmem.h
$(GLSRC)gxclpath.h:$(GLSRC)gdevdevn.h
$(GLSRC)gxclpath.h:$(GLSRC)gxclipsr.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdcolor.h
$(GLSRC)gxclpath.h:$(GLSRC)gxband.h
$(GLSRC)gxclpath.h:$(GLSRC)gxblend.h
$(GLSRC)gxclpath.h:$(GLSRC)shc.h
$(GLSRC)gxclpath.h:$(GLSRC)gscolor2.h
$(GLSRC)gxclpath.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdevice.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcpath.h
$(GLSRC)gxclpath.h:$(GLSRC)gsht.h
$(GLSRC)gxclpath.h:$(GLSRC)gsequivc.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxclpath.h:$(GLSRC)gxpcache.h
$(GLSRC)gxclpath.h:$(GLSRC)gscindex.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcmap.h
$(GLSRC)gxclpath.h:$(GLSRC)gsptype1.h
$(GLSRC)gxclpath.h:$(GLSRC)gscie.h
$(GLSRC)gxclpath.h:$(GLSRC)gxtext.h
$(GLSRC)gxclpath.h:$(GLSRC)gstext.h
$(GLSRC)gxclpath.h:$(GLSRC)gxstate.h
$(GLSRC)gxclpath.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxclpath.h:$(GLSRC)gstparam.h
$(GLSRC)gxclpath.h:$(GLSRC)gspcolor.h
$(GLSRC)gxclpath.h:$(GLSRC)gxfmap.h
$(GLSRC)gxclpath.h:$(GLSRC)gsmalloc.h
$(GLSRC)gxclpath.h:$(GLSRC)gsfunc.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcspace.h
$(GLSRC)gxclpath.h:$(GLSRC)gxctable.h
$(GLSRC)gxclpath.h:$(GLSRC)strimpl.h
$(GLSRC)gxclpath.h:$(GLSRC)gxrplane.h
$(GLSRC)gxclpath.h:$(GLSRC)gscsel.h
$(GLSRC)gxclpath.h:$(GLSRC)gxfcache.h
$(GLSRC)gxclpath.h:$(GLSRC)gsfont.h
$(GLSRC)gxclpath.h:$(GLSRC)gsimage.h
$(GLSRC)gxclpath.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxclpath.h:$(GLSRC)gxbcache.h
$(GLSRC)gxclpath.h:$(GLSRC)gsropt.h
$(GLSRC)gxclpath.h:$(GLSRC)gxdda.h
$(GLSRC)gxclpath.h:$(GLSRC)gxpath.h
$(GLSRC)gxclpath.h:$(GLSRC)gxiclass.h
$(GLSRC)gxclpath.h:$(GLSRC)gxfrac.h
$(GLSRC)gxclpath.h:$(GLSRC)gxtmap.h
$(GLSRC)gxclpath.h:$(GLSRC)gxftype.h
$(GLSRC)gxclpath.h:$(GLSRC)gscms.h
$(GLSRC)gxclpath.h:$(GLSRC)gsrect.h
$(GLSRC)gxclpath.h:$(GLSRC)gslparam.h
$(GLSRC)gxclpath.h:$(GLSRC)gsdevice.h
$(GLSRC)gxclpath.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gxclpath.h:$(GLSRC)gscpm.h
$(GLSRC)gxclpath.h:$(GLSRC)gscspace.h
$(GLSRC)gxclpath.h:$(GLSRC)gsgstate.h
$(GLSRC)gxclpath.h:$(GLSRC)gxstdio.h
$(GLSRC)gxclpath.h:$(GLSRC)gsxfont.h
$(GLSRC)gxclpath.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxclpath.h:$(GLSRC)gsio.h
$(GLSRC)gxclpath.h:$(GLSRC)gsiparam.h
$(GLSRC)gxclpath.h:$(GLSRC)gxfixed.h
$(GLSRC)gxclpath.h:$(GLSRC)gxclio.h
$(GLSRC)gxclpath.h:$(GLSRC)gscompt.h
$(GLSRC)gxclpath.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxclpath.h:$(GLSRC)gspenum.h
$(GLSRC)gxclpath.h:$(GLSRC)gxhttile.h
$(GLSRC)gxclpath.h:$(GLSRC)gsparam.h
$(GLSRC)gxclpath.h:$(GLSRC)gsrefct.h
$(GLSRC)gxclpath.h:$(GLSRC)gp.h
$(GLSRC)gxclpath.h:$(GLSRC)memento.h
$(GLSRC)gxclpath.h:$(GLSRC)memory_.h
$(GLSRC)gxclpath.h:$(GLSRC)gsuid.h
$(GLSRC)gxclpath.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclpath.h:$(GLSRC)gxsync.h
$(GLSRC)gxclpath.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxclpath.h:$(GLSRC)vmsmath.h
$(GLSRC)gxclpath.h:$(GLSRC)srdline.h
$(GLSRC)gxclpath.h:$(GLSRC)scommon.h
$(GLSRC)gxclpath.h:$(GLSRC)gsfname.h
$(GLSRC)gxclpath.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxclpath.h:$(GLSRC)gsccolor.h
$(GLSRC)gxclpath.h:$(GLSRC)gxarith.h
$(GLSRC)gxclpath.h:$(GLSRC)stat_.h
$(GLSRC)gxclpath.h:$(GLSRC)gpsync.h
$(GLSRC)gxclpath.h:$(GLSRC)gsstype.h
$(GLSRC)gxclpath.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclpath.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclpath.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclpath.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclpath.h:$(GLSRC)gxcindex.h
$(GLSRC)gxclpath.h:$(GLSRC)stdio_.h
$(GLSRC)gxclpath.h:$(GLSRC)gsccode.h
$(GLSRC)gxclpath.h:$(GLSRC)stdint_.h
$(GLSRC)gxclpath.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclpath.h:$(GLSRC)gsbittab.h
$(GLSRC)gxclpath.h:$(GLSRC)gstypes.h
$(GLSRC)gxclpath.h:$(GLSRC)std.h
$(GLSRC)gxclpath.h:$(GLSRC)stdpre.h
$(GLSRC)gxclpath.h:$(GLGEN)arch.h
$(GLSRC)gxclpath.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxclmem.h:$(GLSRC)strimpl.h
$(GLSRC)gxclmem.h:$(GLSRC)gxclio.h
$(GLSRC)gxclmem.h:$(GLSRC)gp.h
$(GLSRC)gxclmem.h:$(GLSRC)memory_.h
$(GLSRC)gxclmem.h:$(GLSRC)gsstruct.h
$(GLSRC)gxclmem.h:$(GLSRC)srdline.h
$(GLSRC)gxclmem.h:$(GLSRC)scommon.h
$(GLSRC)gxclmem.h:$(GLSRC)stat_.h
$(GLSRC)gxclmem.h:$(GLSRC)gsstype.h
$(GLSRC)gxclmem.h:$(GLSRC)gsmemory.h
$(GLSRC)gxclmem.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxclmem.h:$(GLSRC)gscdefs.h
$(GLSRC)gxclmem.h:$(GLSRC)gslibctx.h
$(GLSRC)gxclmem.h:$(GLSRC)stdio_.h
$(GLSRC)gxclmem.h:$(GLSRC)stdint_.h
$(GLSRC)gxclmem.h:$(GLSRC)gssprintf.h
$(GLSRC)gxclmem.h:$(GLSRC)gstypes.h
$(GLSRC)gxclmem.h:$(GLSRC)std.h
$(GLSRC)gxclmem.h:$(GLSRC)stdpre.h
$(GLSRC)gxclmem.h:$(GLGEN)arch.h
$(GLSRC)gxclmem.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevvec.h:$(GLSRC)gxgstate.h
$(GLSRC)gdevvec.h:$(GLSRC)gstrans.h
$(GLSRC)gdevvec.h:$(GLSRC)gdevp14.h
$(GLSRC)gdevvec.h:$(GLSRC)gxline.h
$(GLSRC)gdevvec.h:$(GLSRC)gsht1.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcomp.h
$(GLSRC)gdevvec.h:$(GLSRC)math_.h
$(GLSRC)gdevvec.h:$(GLSRC)gdevbbox.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcolor2.h
$(GLSRC)gdevvec.h:$(GLSRC)gxpcolor.h
$(GLSRC)gdevvec.h:$(GLSRC)gxdevmem.h
$(GLSRC)gdevvec.h:$(GLSRC)gdevdevn.h
$(GLSRC)gdevvec.h:$(GLSRC)gxclipsr.h
$(GLSRC)gdevvec.h:$(GLSRC)gxdcolor.h
$(GLSRC)gdevvec.h:$(GLSRC)gxblend.h
$(GLSRC)gdevvec.h:$(GLSRC)gscolor2.h
$(GLSRC)gdevvec.h:$(GLSRC)gxmatrix.h
$(GLSRC)gdevvec.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcpath.h
$(GLSRC)gdevvec.h:$(GLSRC)gsht.h
$(GLSRC)gdevvec.h:$(GLSRC)gxiparam.h
$(GLSRC)gdevvec.h:$(GLSRC)gsequivc.h
$(GLSRC)gdevvec.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevvec.h:$(GLSRC)gxpcache.h
$(GLSRC)gdevvec.h:$(GLSRC)gscindex.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevvec.h:$(GLSRC)gsptype1.h
$(GLSRC)gdevvec.h:$(GLSRC)gscie.h
$(GLSRC)gdevvec.h:$(GLSRC)gxtext.h
$(GLSRC)gdevvec.h:$(GLSRC)gstext.h
$(GLSRC)gdevvec.h:$(GLSRC)gxstate.h
$(GLSRC)gdevvec.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevvec.h:$(GLSRC)gstparam.h
$(GLSRC)gdevvec.h:$(GLSRC)gspcolor.h
$(GLSRC)gdevvec.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevvec.h:$(GLSRC)stream.h
$(GLSRC)gdevvec.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevvec.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevvec.h:$(GLSRC)gxhldevc.h
$(GLSRC)gdevvec.h:$(GLSRC)gxctable.h
$(GLSRC)gdevvec.h:$(GLSRC)gxiodev.h
$(GLSRC)gdevvec.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevvec.h:$(GLSRC)gscsel.h
$(GLSRC)gdevvec.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevvec.h:$(GLSRC)gsfont.h
$(GLSRC)gdevvec.h:$(GLSRC)gsimage.h
$(GLSRC)gdevvec.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevvec.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevvec.h:$(GLSRC)gsropt.h
$(GLSRC)gdevvec.h:$(GLSRC)gxdda.h
$(GLSRC)gdevvec.h:$(GLSRC)gxpath.h
$(GLSRC)gdevvec.h:$(GLSRC)gxiclass.h
$(GLSRC)gdevvec.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevvec.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevvec.h:$(GLSRC)gxftype.h
$(GLSRC)gdevvec.h:$(GLSRC)gscms.h
$(GLSRC)gdevvec.h:$(GLSRC)gsrect.h
$(GLSRC)gdevvec.h:$(GLSRC)gslparam.h
$(GLSRC)gdevvec.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevvec.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gdevvec.h:$(GLSRC)gscpm.h
$(GLSRC)gdevvec.h:$(GLSRC)gscspace.h
$(GLSRC)gdevvec.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevvec.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevvec.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevvec.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevvec.h:$(GLSRC)gsio.h
$(GLSRC)gdevvec.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevvec.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevvec.h:$(GLSRC)gscompt.h
$(GLSRC)gdevvec.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevvec.h:$(GLSRC)gspenum.h
$(GLSRC)gdevvec.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevvec.h:$(GLSRC)gsparam.h
$(GLSRC)gdevvec.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevvec.h:$(GLSRC)gp.h
$(GLSRC)gdevvec.h:$(GLSRC)memento.h
$(GLSRC)gdevvec.h:$(GLSRC)memory_.h
$(GLSRC)gdevvec.h:$(GLSRC)gsuid.h
$(GLSRC)gdevvec.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevvec.h:$(GLSRC)gxsync.h
$(GLSRC)gdevvec.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevvec.h:$(GLSRC)vmsmath.h
$(GLSRC)gdevvec.h:$(GLSRC)srdline.h
$(GLSRC)gdevvec.h:$(GLSRC)scommon.h
$(GLSRC)gdevvec.h:$(GLSRC)gsfname.h
$(GLSRC)gdevvec.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevvec.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevvec.h:$(GLSRC)gxarith.h
$(GLSRC)gdevvec.h:$(GLSRC)stat_.h
$(GLSRC)gdevvec.h:$(GLSRC)gpsync.h
$(GLSRC)gdevvec.h:$(GLSRC)gsstype.h
$(GLSRC)gdevvec.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevvec.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevvec.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevvec.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevvec.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevvec.h:$(GLSRC)stdio_.h
$(GLSRC)gdevvec.h:$(GLSRC)gsccode.h
$(GLSRC)gdevvec.h:$(GLSRC)stdint_.h
$(GLSRC)gdevvec.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevvec.h:$(GLSRC)gstypes.h
$(GLSRC)gdevvec.h:$(GLSRC)std.h
$(GLSRC)gdevvec.h:$(GLSRC)stdpre.h
$(GLSRC)gdevvec.h:$(GLGEN)arch.h
$(GLSRC)gdevvec.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gshtx.h:$(GLSRC)gsht1.h
$(GLSRC)gshtx.h:$(GLSRC)gsht.h
$(GLSRC)gshtx.h:$(GLSRC)gxtmap.h
$(GLSRC)gshtx.h:$(GLSRC)gscspace.h
$(GLSRC)gshtx.h:$(GLSRC)gsgstate.h
$(GLSRC)gshtx.h:$(GLSRC)gsiparam.h
$(GLSRC)gshtx.h:$(GLSRC)gsmatrix.h
$(GLSRC)gshtx.h:$(GLSRC)gsrefct.h
$(GLSRC)gshtx.h:$(GLSRC)memento.h
$(GLSRC)gshtx.h:$(GLSRC)gxbitmap.h
$(GLSRC)gshtx.h:$(GLSRC)scommon.h
$(GLSRC)gshtx.h:$(GLSRC)gsbitmap.h
$(GLSRC)gshtx.h:$(GLSRC)gsccolor.h
$(GLSRC)gshtx.h:$(GLSRC)gsstype.h
$(GLSRC)gshtx.h:$(GLSRC)gsmemory.h
$(GLSRC)gshtx.h:$(GLSRC)gslibctx.h
$(GLSRC)gshtx.h:$(GLSRC)stdio_.h
$(GLSRC)gshtx.h:$(GLSRC)stdint_.h
$(GLSRC)gshtx.h:$(GLSRC)gssprintf.h
$(GLSRC)gshtx.h:$(GLSRC)gstypes.h
$(GLSRC)gshtx.h:$(GLSRC)std.h
$(GLSRC)gshtx.h:$(GLSRC)stdpre.h
$(GLSRC)gshtx.h:$(GLGEN)arch.h
$(GLSRC)gshtx.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxttf.h:$(GLSRC)stdpre.h
$(GLSRC)ttfsfnt.h:$(GLSRC)tttypes.h
$(GLSRC)ttfsfnt.h:$(GLSRC)ttconfig.h
$(GLSRC)ttfsfnt.h:$(GLSRC)tttype.h
$(GLSRC)ttfsfnt.h:$(GLSRC)ttconf.h
$(GLSRC)ttfsfnt.h:$(GLSRC)stdint_.h
$(GLSRC)ttfsfnt.h:$(GLSRC)std.h
$(GLSRC)ttfsfnt.h:$(GLSRC)stdpre.h
$(GLSRC)ttfsfnt.h:$(GLGEN)arch.h
$(GLSRC)ttfinp.h:$(GLSRC)ttfoutl.h
$(GLSRC)ttfinp.h:$(GLSRC)malloc_.h
$(GLSRC)ttfinp.h:$(GLSRC)bobbin.h
$(GLSRC)ttfinp.h:$(GLSRC)gxfcache.h
$(GLSRC)ttfinp.h:$(GLSRC)gsfont.h
$(GLSRC)ttfinp.h:$(GLSRC)gxbcache.h
$(GLSRC)ttfinp.h:$(GLSRC)gxftype.h
$(GLSRC)ttfinp.h:$(GLSRC)gsgstate.h
$(GLSRC)ttfinp.h:$(GLSRC)gsxfont.h
$(GLSRC)ttfinp.h:$(GLSRC)gxfixed.h
$(GLSRC)ttfinp.h:$(GLSRC)gsmatrix.h
$(GLSRC)ttfinp.h:$(GLSRC)memento.h
$(GLSRC)ttfinp.h:$(GLSRC)gsuid.h
$(GLSRC)ttfinp.h:$(GLSRC)gxbitmap.h
$(GLSRC)ttfinp.h:$(GLSRC)scommon.h
$(GLSRC)ttfinp.h:$(GLSRC)gsbitmap.h
$(GLSRC)ttfinp.h:$(GLSRC)gsstype.h
$(GLSRC)ttfinp.h:$(GLSRC)gsmemory.h
$(GLSRC)ttfinp.h:$(GLSRC)gslibctx.h
$(GLSRC)ttfinp.h:$(GLSRC)stdio_.h
$(GLSRC)ttfinp.h:$(GLSRC)gsccode.h
$(GLSRC)ttfinp.h:$(GLSRC)stdint_.h
$(GLSRC)ttfinp.h:$(GLSRC)gssprintf.h
$(GLSRC)ttfinp.h:$(GLSRC)gstypes.h
$(GLSRC)ttfinp.h:$(GLSRC)std.h
$(GLSRC)ttfinp.h:$(GLSRC)stdpre.h
$(GLSRC)ttfinp.h:$(GLGEN)arch.h
$(GLSRC)ttfinp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ttfmemd.h:$(GLSRC)gsstype.h
$(GLSRC)ttfmemd.h:$(GLSRC)gsmemory.h
$(GLSRC)ttfmemd.h:$(GLSRC)gslibctx.h
$(GLSRC)ttfmemd.h:$(GLSRC)stdio_.h
$(GLSRC)ttfmemd.h:$(GLSRC)gssprintf.h
$(GLSRC)ttfmemd.h:$(GLSRC)gstypes.h
$(GLSRC)ttfmemd.h:$(GLSRC)std.h
$(GLSRC)ttfmemd.h:$(GLSRC)stdpre.h
$(GLSRC)ttfmemd.h:$(GLGEN)arch.h
$(GLSRC)ttfmemd.h:$(GLSRC)gs_dll_call.h
$(GLSRC)tttype.h:$(GLSRC)std.h
$(GLSRC)tttype.h:$(GLSRC)stdpre.h
$(GLSRC)tttype.h:$(GLGEN)arch.h
$(GLSRC)ttconfig.h:$(GLSRC)ttconf.h
$(GLSRC)tttypes.h:$(GLSRC)ttconfig.h
$(GLSRC)tttypes.h:$(GLSRC)tttype.h
$(GLSRC)tttypes.h:$(GLSRC)ttconf.h
$(GLSRC)tttypes.h:$(GLSRC)std.h
$(GLSRC)tttypes.h:$(GLSRC)stdpre.h
$(GLSRC)tttypes.h:$(GLGEN)arch.h
$(GLSRC)ttmisc.h:$(GLSRC)string_.h
$(GLSRC)ttmisc.h:$(GLSRC)gsstrtok.h
$(GLSRC)ttmisc.h:$(GLSRC)math_.h
$(GLSRC)ttmisc.h:$(GLSRC)tttypes.h
$(GLSRC)ttmisc.h:$(GLSRC)ttconfig.h
$(GLSRC)ttmisc.h:$(GLSRC)tttype.h
$(GLSRC)ttmisc.h:$(GLSRC)gx.h
$(GLSRC)ttmisc.h:$(GLSRC)ttconf.h
$(GLSRC)ttmisc.h:$(GLSRC)gdebug.h
$(GLSRC)ttmisc.h:$(GLSRC)gsgstate.h
$(GLSRC)ttmisc.h:$(GLSRC)gsio.h
$(GLSRC)ttmisc.h:$(GLSRC)gsstrl.h
$(GLSRC)ttmisc.h:$(GLSRC)gdbflags.h
$(GLSRC)ttmisc.h:$(GLSRC)gserrors.h
$(GLSRC)ttmisc.h:$(GLSRC)vmsmath.h
$(GLSRC)ttmisc.h:$(GLSRC)gsmemory.h
$(GLSRC)ttmisc.h:$(GLSRC)gslibctx.h
$(GLSRC)ttmisc.h:$(GLSRC)stdio_.h
$(GLSRC)ttmisc.h:$(GLSRC)gssprintf.h
$(GLSRC)ttmisc.h:$(GLSRC)gstypes.h
$(GLSRC)ttmisc.h:$(GLSRC)std.h
$(GLSRC)ttmisc.h:$(GLSRC)stdpre.h
$(GLSRC)ttmisc.h:$(GLGEN)arch.h
$(GLSRC)ttmisc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)tttables.h:$(GLSRC)tttypes.h
$(GLSRC)tttables.h:$(GLSRC)ttconfig.h
$(GLSRC)tttables.h:$(GLSRC)tttype.h
$(GLSRC)tttables.h:$(GLSRC)ttconf.h
$(GLSRC)tttables.h:$(GLSRC)std.h
$(GLSRC)tttables.h:$(GLSRC)stdpre.h
$(GLSRC)tttables.h:$(GLGEN)arch.h
$(GLSRC)ttobjs.h:$(GLSRC)ttfoutl.h
$(GLSRC)ttobjs.h:$(GLSRC)malloc_.h
$(GLSRC)ttobjs.h:$(GLSRC)bobbin.h
$(GLSRC)ttobjs.h:$(GLSRC)setjmp_.h
$(GLSRC)ttobjs.h:$(GLSRC)ttcommon.h
$(GLSRC)ttobjs.h:$(GLSRC)tttables.h
$(GLSRC)ttobjs.h:$(GLSRC)tttypes.h
$(GLSRC)ttobjs.h:$(GLSRC)ttconfig.h
$(GLSRC)ttobjs.h:$(GLSRC)tttype.h
$(GLSRC)ttobjs.h:$(GLSRC)ttconf.h
$(GLSRC)ttobjs.h:$(GLSRC)gxfcache.h
$(GLSRC)ttobjs.h:$(GLSRC)gsfont.h
$(GLSRC)ttobjs.h:$(GLSRC)gxbcache.h
$(GLSRC)ttobjs.h:$(GLSRC)gxftype.h
$(GLSRC)ttobjs.h:$(GLSRC)gsgstate.h
$(GLSRC)ttobjs.h:$(GLSRC)gsxfont.h
$(GLSRC)ttobjs.h:$(GLSRC)gxfixed.h
$(GLSRC)ttobjs.h:$(GLSRC)gsmatrix.h
$(GLSRC)ttobjs.h:$(GLSRC)memento.h
$(GLSRC)ttobjs.h:$(GLSRC)gsuid.h
$(GLSRC)ttobjs.h:$(GLSRC)gxbitmap.h
$(GLSRC)ttobjs.h:$(GLSRC)scommon.h
$(GLSRC)ttobjs.h:$(GLSRC)gsbitmap.h
$(GLSRC)ttobjs.h:$(GLSRC)gsstype.h
$(GLSRC)ttobjs.h:$(GLSRC)gsmemory.h
$(GLSRC)ttobjs.h:$(GLSRC)gslibctx.h
$(GLSRC)ttobjs.h:$(GLSRC)stdio_.h
$(GLSRC)ttobjs.h:$(GLSRC)gsccode.h
$(GLSRC)ttobjs.h:$(GLSRC)stdint_.h
$(GLSRC)ttobjs.h:$(GLSRC)gssprintf.h
$(GLSRC)ttobjs.h:$(GLSRC)gstypes.h
$(GLSRC)ttobjs.h:$(GLSRC)std.h
$(GLSRC)ttobjs.h:$(GLSRC)stdpre.h
$(GLSRC)ttobjs.h:$(GLGEN)arch.h
$(GLSRC)ttobjs.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ttcalc.h:$(GLSRC)ttcommon.h
$(GLSRC)ttcalc.h:$(GLSRC)tttypes.h
$(GLSRC)ttcalc.h:$(GLSRC)ttconfig.h
$(GLSRC)ttcalc.h:$(GLSRC)tttype.h
$(GLSRC)ttcalc.h:$(GLSRC)ttconf.h
$(GLSRC)ttcalc.h:$(GLSRC)std.h
$(GLSRC)ttcalc.h:$(GLSRC)stdpre.h
$(GLSRC)ttcalc.h:$(GLGEN)arch.h
$(GLSRC)ttinterp.h:$(GLSRC)ttobjs.h
$(GLSRC)ttinterp.h:$(GLSRC)ttfoutl.h
$(GLSRC)ttinterp.h:$(GLSRC)malloc_.h
$(GLSRC)ttinterp.h:$(GLSRC)bobbin.h
$(GLSRC)ttinterp.h:$(GLSRC)setjmp_.h
$(GLSRC)ttinterp.h:$(GLSRC)ttcommon.h
$(GLSRC)ttinterp.h:$(GLSRC)tttables.h
$(GLSRC)ttinterp.h:$(GLSRC)tttypes.h
$(GLSRC)ttinterp.h:$(GLSRC)ttconfig.h
$(GLSRC)ttinterp.h:$(GLSRC)tttype.h
$(GLSRC)ttinterp.h:$(GLSRC)ttconf.h
$(GLSRC)ttinterp.h:$(GLSRC)gxfcache.h
$(GLSRC)ttinterp.h:$(GLSRC)gsfont.h
$(GLSRC)ttinterp.h:$(GLSRC)gxbcache.h
$(GLSRC)ttinterp.h:$(GLSRC)gxftype.h
$(GLSRC)ttinterp.h:$(GLSRC)gsgstate.h
$(GLSRC)ttinterp.h:$(GLSRC)gsxfont.h
$(GLSRC)ttinterp.h:$(GLSRC)gxfixed.h
$(GLSRC)ttinterp.h:$(GLSRC)gsmatrix.h
$(GLSRC)ttinterp.h:$(GLSRC)memento.h
$(GLSRC)ttinterp.h:$(GLSRC)gsuid.h
$(GLSRC)ttinterp.h:$(GLSRC)gxbitmap.h
$(GLSRC)ttinterp.h:$(GLSRC)scommon.h
$(GLSRC)ttinterp.h:$(GLSRC)gsbitmap.h
$(GLSRC)ttinterp.h:$(GLSRC)gsstype.h
$(GLSRC)ttinterp.h:$(GLSRC)gsmemory.h
$(GLSRC)ttinterp.h:$(GLSRC)gslibctx.h
$(GLSRC)ttinterp.h:$(GLSRC)stdio_.h
$(GLSRC)ttinterp.h:$(GLSRC)gsccode.h
$(GLSRC)ttinterp.h:$(GLSRC)stdint_.h
$(GLSRC)ttinterp.h:$(GLSRC)gssprintf.h
$(GLSRC)ttinterp.h:$(GLSRC)gstypes.h
$(GLSRC)ttinterp.h:$(GLSRC)std.h
$(GLSRC)ttinterp.h:$(GLSRC)stdpre.h
$(GLSRC)ttinterp.h:$(GLGEN)arch.h
$(GLSRC)ttinterp.h:$(GLSRC)gs_dll_call.h
$(GLSRC)ttload.h:$(GLSRC)ttobjs.h
$(GLSRC)ttload.h:$(GLSRC)ttfoutl.h
$(GLSRC)ttload.h:$(GLSRC)malloc_.h
$(GLSRC)ttload.h:$(GLSRC)bobbin.h
$(GLSRC)ttload.h:$(GLSRC)setjmp_.h
$(GLSRC)ttload.h:$(GLSRC)ttcommon.h
$(GLSRC)ttload.h:$(GLSRC)tttables.h
$(GLSRC)ttload.h:$(GLSRC)tttypes.h
$(GLSRC)ttload.h:$(GLSRC)ttconfig.h
$(GLSRC)ttload.h:$(GLSRC)tttype.h
$(GLSRC)ttload.h:$(GLSRC)ttconf.h
$(GLSRC)ttload.h:$(GLSRC)gxfcache.h
$(GLSRC)ttload.h:$(GLSRC)gsfont.h
$(GLSRC)ttload.h:$(GLSRC)gxbcache.h
$(GLSRC)ttload.h:$(GLSRC)gxftype.h
$(GLSRC)ttload.h:$(GLSRC)gsgstate.h
$(GLSRC)ttload.h:$(GLSRC)gsxfont.h
$(GLSRC)ttload.h:$(GLSRC)gxfixed.h
$(GLSRC)ttload.h:$(GLSRC)gsmatrix.h
$(GLSRC)ttload.h:$(GLSRC)memento.h
$(GLSRC)ttload.h:$(GLSRC)gsuid.h
$(GLSRC)ttload.h:$(GLSRC)gxbitmap.h
$(GLSRC)ttload.h:$(GLSRC)scommon.h
$(GLSRC)ttload.h:$(GLSRC)gsbitmap.h
$(GLSRC)ttload.h:$(GLSRC)gsstype.h
$(GLSRC)ttload.h:$(GLSRC)gsmemory.h
$(GLSRC)ttload.h:$(GLSRC)gslibctx.h
$(GLSRC)ttload.h:$(GLSRC)stdio_.h
$(GLSRC)ttload.h:$(GLSRC)gsccode.h
$(GLSRC)ttload.h:$(GLSRC)stdint_.h
$(GLSRC)ttload.h:$(GLSRC)gssprintf.h
$(GLSRC)ttload.h:$(GLSRC)gstypes.h
$(GLSRC)ttload.h:$(GLSRC)std.h
$(GLSRC)ttload.h:$(GLSRC)stdpre.h
$(GLSRC)ttload.h:$(GLGEN)arch.h
$(GLSRC)ttload.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfont1.h
$(GLSRC)gxhintn.h:$(GLSRC)gstype1.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfont42.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfont.h
$(GLSRC)gxhintn.h:$(GLSRC)gspath.h
$(GLSRC)gxhintn.h:$(GLSRC)gsgdata.h
$(GLSRC)gxhintn.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfapi.h
$(GLSRC)gxhintn.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxhintn.h:$(GLSRC)gstext.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfcache.h
$(GLSRC)gxhintn.h:$(GLSRC)gsfont.h
$(GLSRC)gxhintn.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxhintn.h:$(GLSRC)gxbcache.h
$(GLSRC)gxhintn.h:$(GLSRC)gxpath.h
$(GLSRC)gxhintn.h:$(GLSRC)gxftype.h
$(GLSRC)gxhintn.h:$(GLSRC)gscms.h
$(GLSRC)gxhintn.h:$(GLSRC)gsrect.h
$(GLSRC)gxhintn.h:$(GLSRC)gslparam.h
$(GLSRC)gxhintn.h:$(GLSRC)gsdevice.h
$(GLSRC)gxhintn.h:$(GLSRC)gscpm.h
$(GLSRC)gxhintn.h:$(GLSRC)gsgcache.h
$(GLSRC)gxhintn.h:$(GLSRC)gscspace.h
$(GLSRC)gxhintn.h:$(GLSRC)gsgstate.h
$(GLSRC)gxhintn.h:$(GLSRC)gsnotify.h
$(GLSRC)gxhintn.h:$(GLSRC)gsxfont.h
$(GLSRC)gxhintn.h:$(GLSRC)gsiparam.h
$(GLSRC)gxhintn.h:$(GLSRC)gxfixed.h
$(GLSRC)gxhintn.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxhintn.h:$(GLSRC)gspenum.h
$(GLSRC)gxhintn.h:$(GLSRC)gxhttile.h
$(GLSRC)gxhintn.h:$(GLSRC)gsparam.h
$(GLSRC)gxhintn.h:$(GLSRC)gsrefct.h
$(GLSRC)gxhintn.h:$(GLSRC)memento.h
$(GLSRC)gxhintn.h:$(GLSRC)gsuid.h
$(GLSRC)gxhintn.h:$(GLSRC)gxsync.h
$(GLSRC)gxhintn.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxhintn.h:$(GLSRC)scommon.h
$(GLSRC)gxhintn.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxhintn.h:$(GLSRC)gsccolor.h
$(GLSRC)gxhintn.h:$(GLSRC)gxarith.h
$(GLSRC)gxhintn.h:$(GLSRC)gpsync.h
$(GLSRC)gxhintn.h:$(GLSRC)gsstype.h
$(GLSRC)gxhintn.h:$(GLSRC)gsmemory.h
$(GLSRC)gxhintn.h:$(GLSRC)gslibctx.h
$(GLSRC)gxhintn.h:$(GLSRC)gxcindex.h
$(GLSRC)gxhintn.h:$(GLSRC)stdio_.h
$(GLSRC)gxhintn.h:$(GLSRC)gsccode.h
$(GLSRC)gxhintn.h:$(GLSRC)stdint_.h
$(GLSRC)gxhintn.h:$(GLSRC)gssprintf.h
$(GLSRC)gxhintn.h:$(GLSRC)gstypes.h
$(GLSRC)gxhintn.h:$(GLSRC)std.h
$(GLSRC)gxhintn.h:$(GLSRC)stdpre.h
$(GLSRC)gxhintn.h:$(GLGEN)arch.h
$(GLSRC)gxhintn.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxcid.h:$(GLSRC)gsstype.h
$(GLSRC)gxcid.h:$(GLSRC)gsmemory.h
$(GLSRC)gxcid.h:$(GLSRC)gslibctx.h
$(GLSRC)gxcid.h:$(GLSRC)stdio_.h
$(GLSRC)gxcid.h:$(GLSRC)gssprintf.h
$(GLSRC)gxcid.h:$(GLSRC)gstypes.h
$(GLSRC)gxcid.h:$(GLSRC)std.h
$(GLSRC)gxcid.h:$(GLSRC)stdpre.h
$(GLSRC)gxcid.h:$(GLGEN)arch.h
$(GLSRC)gxcid.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfcid.h:$(GLSRC)gstype1.h
$(GLSRC)gxfcid.h:$(GLSRC)gxfont42.h
$(GLSRC)gxfcid.h:$(GLSRC)gxfont.h
$(GLSRC)gxfcid.h:$(GLSRC)gspath.h
$(GLSRC)gxfcid.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfcid.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfcid.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfcid.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfcid.h:$(GLSRC)gstext.h
$(GLSRC)gxfcid.h:$(GLSRC)gxfcache.h
$(GLSRC)gxfcid.h:$(GLSRC)gsfont.h
$(GLSRC)gxfcid.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfcid.h:$(GLSRC)gxbcache.h
$(GLSRC)gxfcid.h:$(GLSRC)gxpath.h
$(GLSRC)gxfcid.h:$(GLSRC)gxftype.h
$(GLSRC)gxfcid.h:$(GLSRC)gscms.h
$(GLSRC)gxfcid.h:$(GLSRC)gsrect.h
$(GLSRC)gxfcid.h:$(GLSRC)gslparam.h
$(GLSRC)gxfcid.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfcid.h:$(GLSRC)gscpm.h
$(GLSRC)gxfcid.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfcid.h:$(GLSRC)gscspace.h
$(GLSRC)gxfcid.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfcid.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfcid.h:$(GLSRC)gsxfont.h
$(GLSRC)gxfcid.h:$(GLSRC)gxcid.h
$(GLSRC)gxfcid.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfcid.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfcid.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfcid.h:$(GLSRC)gspenum.h
$(GLSRC)gxfcid.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfcid.h:$(GLSRC)gsparam.h
$(GLSRC)gxfcid.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfcid.h:$(GLSRC)memento.h
$(GLSRC)gxfcid.h:$(GLSRC)gsuid.h
$(GLSRC)gxfcid.h:$(GLSRC)gxsync.h
$(GLSRC)gxfcid.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfcid.h:$(GLSRC)scommon.h
$(GLSRC)gxfcid.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfcid.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfcid.h:$(GLSRC)gxarith.h
$(GLSRC)gxfcid.h:$(GLSRC)gpsync.h
$(GLSRC)gxfcid.h:$(GLSRC)gsstype.h
$(GLSRC)gxfcid.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfcid.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfcid.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfcid.h:$(GLSRC)stdio_.h
$(GLSRC)gxfcid.h:$(GLSRC)gsccode.h
$(GLSRC)gxfcid.h:$(GLSRC)stdint_.h
$(GLSRC)gxfcid.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfcid.h:$(GLSRC)gstypes.h
$(GLSRC)gxfcid.h:$(GLSRC)std.h
$(GLSRC)gxfcid.h:$(GLSRC)stdpre.h
$(GLSRC)gxfcid.h:$(GLGEN)arch.h
$(GLSRC)gxfcid.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfcmap.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfcmap.h:$(GLSRC)gxcid.h
$(GLSRC)gxfcmap.h:$(GLSRC)gsuid.h
$(GLSRC)gxfcmap.h:$(GLSRC)gsstype.h
$(GLSRC)gxfcmap.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfcmap.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfcmap.h:$(GLSRC)stdio_.h
$(GLSRC)gxfcmap.h:$(GLSRC)gsccode.h
$(GLSRC)gxfcmap.h:$(GLSRC)stdint_.h
$(GLSRC)gxfcmap.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfcmap.h:$(GLSRC)gstypes.h
$(GLSRC)gxfcmap.h:$(GLSRC)std.h
$(GLSRC)gxfcmap.h:$(GLSRC)stdpre.h
$(GLSRC)gxfcmap.h:$(GLGEN)arch.h
$(GLSRC)gxfcmap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gxfcmap.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gxcid.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gsuid.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gsstype.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfcmap1.h:$(GLSRC)stdio_.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gsccode.h
$(GLSRC)gxfcmap1.h:$(GLSRC)stdint_.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gstypes.h
$(GLSRC)gxfcmap1.h:$(GLSRC)std.h
$(GLSRC)gxfcmap1.h:$(GLSRC)stdpre.h
$(GLSRC)gxfcmap1.h:$(GLGEN)arch.h
$(GLSRC)gxfcmap1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfont0.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfcid.h
$(GLSRC)gxfont0c.h:$(GLSRC)gstype1.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfont42.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfont.h
$(GLSRC)gxfont0c.h:$(GLSRC)gspath.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfont0c.h:$(GLSRC)gstext.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfcache.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsfont.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxbcache.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxpath.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxftype.h
$(GLSRC)gxfont0c.h:$(GLSRC)gscms.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsrect.h
$(GLSRC)gxfont0c.h:$(GLSRC)gslparam.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfont0c.h:$(GLSRC)gscpm.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfont0c.h:$(GLSRC)gscspace.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsxfont.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxcid.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfont0c.h:$(GLSRC)gspenum.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsparam.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfont0c.h:$(GLSRC)memento.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsuid.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxsync.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfont0c.h:$(GLSRC)scommon.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxarith.h
$(GLSRC)gxfont0c.h:$(GLSRC)gpsync.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsstype.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfont0c.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfont0c.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfont0c.h:$(GLSRC)stdio_.h
$(GLSRC)gxfont0c.h:$(GLSRC)gsccode.h
$(GLSRC)gxfont0c.h:$(GLSRC)stdint_.h
$(GLSRC)gxfont0c.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfont0c.h:$(GLSRC)gstypes.h
$(GLSRC)gxfont0c.h:$(GLSRC)std.h
$(GLSRC)gxfont0c.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont0c.h:$(GLGEN)arch.h
$(GLSRC)gxfont0c.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscrypt1.h:$(GLSRC)stdpre.h
$(GLSRC)gstype1.h:$(GLSRC)gspath.h
$(GLSRC)gstype1.h:$(GLSRC)gsgdata.h
$(GLSRC)gstype1.h:$(GLSRC)gxmatrix.h
$(GLSRC)gstype1.h:$(GLSRC)gsfont.h
$(GLSRC)gstype1.h:$(GLSRC)gsgcache.h
$(GLSRC)gstype1.h:$(GLSRC)gsgstate.h
$(GLSRC)gstype1.h:$(GLSRC)gxfixed.h
$(GLSRC)gstype1.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstype1.h:$(GLSRC)gspenum.h
$(GLSRC)gstype1.h:$(GLSRC)scommon.h
$(GLSRC)gstype1.h:$(GLSRC)gsstype.h
$(GLSRC)gstype1.h:$(GLSRC)gsmemory.h
$(GLSRC)gstype1.h:$(GLSRC)gslibctx.h
$(GLSRC)gstype1.h:$(GLSRC)stdio_.h
$(GLSRC)gstype1.h:$(GLSRC)stdint_.h
$(GLSRC)gstype1.h:$(GLSRC)gssprintf.h
$(GLSRC)gstype1.h:$(GLSRC)gstypes.h
$(GLSRC)gstype1.h:$(GLSRC)std.h
$(GLSRC)gstype1.h:$(GLSRC)stdpre.h
$(GLSRC)gstype1.h:$(GLGEN)arch.h
$(GLSRC)gstype1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfont1.h:$(GLSRC)gstype1.h
$(GLSRC)gxfont1.h:$(GLSRC)gxfont.h
$(GLSRC)gxfont1.h:$(GLSRC)gspath.h
$(GLSRC)gxfont1.h:$(GLSRC)gsgdata.h
$(GLSRC)gxfont1.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxfont1.h:$(GLSRC)gxfapi.h
$(GLSRC)gxfont1.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxfont1.h:$(GLSRC)gstext.h
$(GLSRC)gxfont1.h:$(GLSRC)gsfont.h
$(GLSRC)gxfont1.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxfont1.h:$(GLSRC)gxpath.h
$(GLSRC)gxfont1.h:$(GLSRC)gxftype.h
$(GLSRC)gxfont1.h:$(GLSRC)gscms.h
$(GLSRC)gxfont1.h:$(GLSRC)gsrect.h
$(GLSRC)gxfont1.h:$(GLSRC)gslparam.h
$(GLSRC)gxfont1.h:$(GLSRC)gsdevice.h
$(GLSRC)gxfont1.h:$(GLSRC)gscpm.h
$(GLSRC)gxfont1.h:$(GLSRC)gsgcache.h
$(GLSRC)gxfont1.h:$(GLSRC)gscspace.h
$(GLSRC)gxfont1.h:$(GLSRC)gsgstate.h
$(GLSRC)gxfont1.h:$(GLSRC)gsnotify.h
$(GLSRC)gxfont1.h:$(GLSRC)gsiparam.h
$(GLSRC)gxfont1.h:$(GLSRC)gxfixed.h
$(GLSRC)gxfont1.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxfont1.h:$(GLSRC)gspenum.h
$(GLSRC)gxfont1.h:$(GLSRC)gxhttile.h
$(GLSRC)gxfont1.h:$(GLSRC)gsparam.h
$(GLSRC)gxfont1.h:$(GLSRC)gsrefct.h
$(GLSRC)gxfont1.h:$(GLSRC)memento.h
$(GLSRC)gxfont1.h:$(GLSRC)gsuid.h
$(GLSRC)gxfont1.h:$(GLSRC)gxsync.h
$(GLSRC)gxfont1.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxfont1.h:$(GLSRC)scommon.h
$(GLSRC)gxfont1.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxfont1.h:$(GLSRC)gsccolor.h
$(GLSRC)gxfont1.h:$(GLSRC)gxarith.h
$(GLSRC)gxfont1.h:$(GLSRC)gpsync.h
$(GLSRC)gxfont1.h:$(GLSRC)gsstype.h
$(GLSRC)gxfont1.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfont1.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfont1.h:$(GLSRC)gxcindex.h
$(GLSRC)gxfont1.h:$(GLSRC)stdio_.h
$(GLSRC)gxfont1.h:$(GLSRC)gsccode.h
$(GLSRC)gxfont1.h:$(GLSRC)stdint_.h
$(GLSRC)gxfont1.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfont1.h:$(GLSRC)gstypes.h
$(GLSRC)gxfont1.h:$(GLSRC)std.h
$(GLSRC)gxfont1.h:$(GLSRC)stdpre.h
$(GLSRC)gxfont1.h:$(GLGEN)arch.h
$(GLSRC)gxfont1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxtype1.h:$(GLSRC)gxhintn.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfont1.h
$(GLSRC)gxtype1.h:$(GLSRC)gzpath.h
$(GLSRC)gxtype1.h:$(GLSRC)gstype1.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfont42.h
$(GLSRC)gxtype1.h:$(GLSRC)gscrypt1.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfont.h
$(GLSRC)gxtype1.h:$(GLSRC)gspath.h
$(GLSRC)gxtype1.h:$(GLSRC)gsgdata.h
$(GLSRC)gxtype1.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfapi.h
$(GLSRC)gxtype1.h:$(GLSRC)gsfcmap.h
$(GLSRC)gxtype1.h:$(GLSRC)gstext.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfcache.h
$(GLSRC)gxtype1.h:$(GLSRC)gsfont.h
$(GLSRC)gxtype1.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxtype1.h:$(GLSRC)gxbcache.h
$(GLSRC)gxtype1.h:$(GLSRC)gxpath.h
$(GLSRC)gxtype1.h:$(GLSRC)gxftype.h
$(GLSRC)gxtype1.h:$(GLSRC)gscms.h
$(GLSRC)gxtype1.h:$(GLSRC)gsrect.h
$(GLSRC)gxtype1.h:$(GLSRC)gslparam.h
$(GLSRC)gxtype1.h:$(GLSRC)gsdevice.h
$(GLSRC)gxtype1.h:$(GLSRC)gscpm.h
$(GLSRC)gxtype1.h:$(GLSRC)gsgcache.h
$(GLSRC)gxtype1.h:$(GLSRC)gscspace.h
$(GLSRC)gxtype1.h:$(GLSRC)gsgstate.h
$(GLSRC)gxtype1.h:$(GLSRC)gsnotify.h
$(GLSRC)gxtype1.h:$(GLSRC)gsxfont.h
$(GLSRC)gxtype1.h:$(GLSRC)gsiparam.h
$(GLSRC)gxtype1.h:$(GLSRC)gxfixed.h
$(GLSRC)gxtype1.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxtype1.h:$(GLSRC)gspenum.h
$(GLSRC)gxtype1.h:$(GLSRC)gxhttile.h
$(GLSRC)gxtype1.h:$(GLSRC)gsparam.h
$(GLSRC)gxtype1.h:$(GLSRC)gsrefct.h
$(GLSRC)gxtype1.h:$(GLSRC)memento.h
$(GLSRC)gxtype1.h:$(GLSRC)gsuid.h
$(GLSRC)gxtype1.h:$(GLSRC)gxsync.h
$(GLSRC)gxtype1.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxtype1.h:$(GLSRC)scommon.h
$(GLSRC)gxtype1.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxtype1.h:$(GLSRC)gsccolor.h
$(GLSRC)gxtype1.h:$(GLSRC)gxarith.h
$(GLSRC)gxtype1.h:$(GLSRC)gpsync.h
$(GLSRC)gxtype1.h:$(GLSRC)gsstype.h
$(GLSRC)gxtype1.h:$(GLSRC)gsmemory.h
$(GLSRC)gxtype1.h:$(GLSRC)gslibctx.h
$(GLSRC)gxtype1.h:$(GLSRC)gxcindex.h
$(GLSRC)gxtype1.h:$(GLSRC)stdio_.h
$(GLSRC)gxtype1.h:$(GLSRC)gsccode.h
$(GLSRC)gxtype1.h:$(GLSRC)stdint_.h
$(GLSRC)gxtype1.h:$(GLSRC)gssprintf.h
$(GLSRC)gxtype1.h:$(GLSRC)gstypes.h
$(GLSRC)gxtype1.h:$(GLSRC)std.h
$(GLSRC)gxtype1.h:$(GLSRC)stdpre.h
$(GLSRC)gxtype1.h:$(GLGEN)arch.h
$(GLSRC)gxtype1.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsdsrc.h:$(GLSRC)gsstruct.h
$(GLSRC)gsdsrc.h:$(GLSRC)scommon.h
$(GLSRC)gsdsrc.h:$(GLSRC)gsstype.h
$(GLSRC)gsdsrc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsdsrc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsdsrc.h:$(GLSRC)stdio_.h
$(GLSRC)gsdsrc.h:$(GLSRC)stdint_.h
$(GLSRC)gsdsrc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsdsrc.h:$(GLSRC)gstypes.h
$(GLSRC)gsdsrc.h:$(GLSRC)std.h
$(GLSRC)gsdsrc.h:$(GLSRC)stdpre.h
$(GLSRC)gsdsrc.h:$(GLGEN)arch.h
$(GLSRC)gsdsrc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsfunc.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsfunc.h:$(GLSRC)gsparam.h
$(GLSRC)gsfunc.h:$(GLSRC)memento.h
$(GLSRC)gsfunc.h:$(GLSRC)gsstruct.h
$(GLSRC)gsfunc.h:$(GLSRC)scommon.h
$(GLSRC)gsfunc.h:$(GLSRC)gsstype.h
$(GLSRC)gsfunc.h:$(GLSRC)gsmemory.h
$(GLSRC)gsfunc.h:$(GLSRC)gslibctx.h
$(GLSRC)gsfunc.h:$(GLSRC)stdio_.h
$(GLSRC)gsfunc.h:$(GLSRC)stdint_.h
$(GLSRC)gsfunc.h:$(GLSRC)gssprintf.h
$(GLSRC)gsfunc.h:$(GLSRC)gstypes.h
$(GLSRC)gsfunc.h:$(GLSRC)std.h
$(GLSRC)gsfunc.h:$(GLSRC)stdpre.h
$(GLSRC)gsfunc.h:$(GLGEN)arch.h
$(GLSRC)gsfunc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsfunc.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsparam.h
$(GLSRC)gsfunc0.h:$(GLSRC)memento.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsstruct.h
$(GLSRC)gsfunc0.h:$(GLSRC)scommon.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsstype.h
$(GLSRC)gsfunc0.h:$(GLSRC)gsmemory.h
$(GLSRC)gsfunc0.h:$(GLSRC)gslibctx.h
$(GLSRC)gsfunc0.h:$(GLSRC)stdio_.h
$(GLSRC)gsfunc0.h:$(GLSRC)stdint_.h
$(GLSRC)gsfunc0.h:$(GLSRC)gssprintf.h
$(GLSRC)gsfunc0.h:$(GLSRC)gstypes.h
$(GLSRC)gsfunc0.h:$(GLSRC)std.h
$(GLSRC)gsfunc0.h:$(GLSRC)stdpre.h
$(GLSRC)gsfunc0.h:$(GLGEN)arch.h
$(GLSRC)gsfunc0.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxfunc.h:$(GLSRC)gsfunc.h
$(GLSRC)gxfunc.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxfunc.h:$(GLSRC)gsparam.h
$(GLSRC)gxfunc.h:$(GLSRC)memento.h
$(GLSRC)gxfunc.h:$(GLSRC)gsstruct.h
$(GLSRC)gxfunc.h:$(GLSRC)scommon.h
$(GLSRC)gxfunc.h:$(GLSRC)gsstype.h
$(GLSRC)gxfunc.h:$(GLSRC)gsmemory.h
$(GLSRC)gxfunc.h:$(GLSRC)gslibctx.h
$(GLSRC)gxfunc.h:$(GLSRC)stdio_.h
$(GLSRC)gxfunc.h:$(GLSRC)stdint_.h
$(GLSRC)gxfunc.h:$(GLSRC)gssprintf.h
$(GLSRC)gxfunc.h:$(GLSRC)gstypes.h
$(GLSRC)gxfunc.h:$(GLSRC)std.h
$(GLSRC)gxfunc.h:$(GLSRC)stdpre.h
$(GLSRC)gxfunc.h:$(GLGEN)arch.h
$(GLSRC)gxfunc.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsfunc.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsparam.h
$(GLSRC)gsfunc4.h:$(GLSRC)memento.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsstruct.h
$(GLSRC)gsfunc4.h:$(GLSRC)scommon.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsstype.h
$(GLSRC)gsfunc4.h:$(GLSRC)gsmemory.h
$(GLSRC)gsfunc4.h:$(GLSRC)gslibctx.h
$(GLSRC)gsfunc4.h:$(GLSRC)stdio_.h
$(GLSRC)gsfunc4.h:$(GLSRC)stdint_.h
$(GLSRC)gsfunc4.h:$(GLSRC)gssprintf.h
$(GLSRC)gsfunc4.h:$(GLSRC)gstypes.h
$(GLSRC)gsfunc4.h:$(GLSRC)std.h
$(GLSRC)gsfunc4.h:$(GLSRC)stdpre.h
$(GLSRC)gsfunc4.h:$(GLGEN)arch.h
$(GLSRC)gsfunc4.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscpixel.h:$(GLSRC)gscspace.h
$(GLSRC)gscpixel.h:$(GLSRC)gsgstate.h
$(GLSRC)gscpixel.h:$(GLSRC)gsiparam.h
$(GLSRC)gscpixel.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscpixel.h:$(GLSRC)gsrefct.h
$(GLSRC)gscpixel.h:$(GLSRC)memento.h
$(GLSRC)gscpixel.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscpixel.h:$(GLSRC)scommon.h
$(GLSRC)gscpixel.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscpixel.h:$(GLSRC)gsccolor.h
$(GLSRC)gscpixel.h:$(GLSRC)gsstype.h
$(GLSRC)gscpixel.h:$(GLSRC)gsmemory.h
$(GLSRC)gscpixel.h:$(GLSRC)gslibctx.h
$(GLSRC)gscpixel.h:$(GLSRC)stdio_.h
$(GLSRC)gscpixel.h:$(GLSRC)stdint_.h
$(GLSRC)gscpixel.h:$(GLSRC)gssprintf.h
$(GLSRC)gscpixel.h:$(GLSRC)gstypes.h
$(GLSRC)gscpixel.h:$(GLSRC)std.h
$(GLSRC)gscpixel.h:$(GLSRC)stdpre.h
$(GLSRC)gscpixel.h:$(GLGEN)arch.h
$(GLSRC)gscpixel.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscms.h:$(GLSRC)gsdevice.h
$(GLSRC)gscms.h:$(GLSRC)gscspace.h
$(GLSRC)gscms.h:$(GLSRC)gsgstate.h
$(GLSRC)gscms.h:$(GLSRC)gsiparam.h
$(GLSRC)gscms.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscms.h:$(GLSRC)gsparam.h
$(GLSRC)gscms.h:$(GLSRC)gsrefct.h
$(GLSRC)gscms.h:$(GLSRC)memento.h
$(GLSRC)gscms.h:$(GLSRC)gxsync.h
$(GLSRC)gscms.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscms.h:$(GLSRC)scommon.h
$(GLSRC)gscms.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscms.h:$(GLSRC)gsccolor.h
$(GLSRC)gscms.h:$(GLSRC)gpsync.h
$(GLSRC)gscms.h:$(GLSRC)gsstype.h
$(GLSRC)gscms.h:$(GLSRC)gsmemory.h
$(GLSRC)gscms.h:$(GLSRC)gslibctx.h
$(GLSRC)gscms.h:$(GLSRC)stdio_.h
$(GLSRC)gscms.h:$(GLSRC)stdint_.h
$(GLSRC)gscms.h:$(GLSRC)gssprintf.h
$(GLSRC)gscms.h:$(GLSRC)gstypes.h
$(GLSRC)gscms.h:$(GLSRC)std.h
$(GLSRC)gscms.h:$(GLSRC)stdpre.h
$(GLSRC)gscms.h:$(GLGEN)arch.h
$(GLSRC)gscms.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gxcvalue.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gscms.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsdevice.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsparam.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc_cms.h:$(GLSRC)memento.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gxsync.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc_cms.h:$(GLSRC)scommon.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gpsync.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc_cms.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc_cms.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc_cms.h:$(GLSRC)std.h
$(GLSRC)gsicc_cms.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc_cms.h:$(GLGEN)arch.h
$(GLSRC)gsicc_cms.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsicc_cms.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gxcvalue.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gscms.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsdevice.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsparam.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc_manage.h:$(GLSRC)memento.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gxsync.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc_manage.h:$(GLSRC)scommon.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gpsync.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc_manage.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc_manage.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc_manage.h:$(GLSRC)std.h
$(GLSRC)gsicc_manage.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc_manage.h:$(GLGEN)arch.h
$(GLSRC)gsicc_manage.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gxcvalue.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gscms.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsdevice.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsparam.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc_cache.h:$(GLSRC)memento.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gxsync.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc_cache.h:$(GLSRC)scommon.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gpsync.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc_cache.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc_cache.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc_cache.h:$(GLSRC)std.h
$(GLSRC)gsicc_cache.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc_cache.h:$(GLGEN)arch.h
$(GLSRC)gsicc_cache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gscms.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsdevice.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gscspace.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsgstate.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsiparam.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsparam.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsrefct.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)memento.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gxsync.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)scommon.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsccolor.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gpsync.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsstype.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gsmemory.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gslibctx.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)stdio_.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)stdint_.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gssprintf.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gstypes.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)std.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)stdpre.h
$(GLSRC)gsicc_profilecache.h:$(GLGEN)arch.h
$(GLSRC)gsicc_profilecache.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gstrap.h:$(GLSRC)gspath.h
$(GLSRC)gstrap.h:$(GLSRC)gxmatrix.h
$(GLSRC)gstrap.h:$(GLSRC)gsgstate.h
$(GLSRC)gstrap.h:$(GLSRC)gxfixed.h
$(GLSRC)gstrap.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstrap.h:$(GLSRC)gspenum.h
$(GLSRC)gstrap.h:$(GLSRC)gsparam.h
$(GLSRC)gstrap.h:$(GLSRC)scommon.h
$(GLSRC)gstrap.h:$(GLSRC)gsstype.h
$(GLSRC)gstrap.h:$(GLSRC)gsmemory.h
$(GLSRC)gstrap.h:$(GLSRC)gslibctx.h
$(GLSRC)gstrap.h:$(GLSRC)stdio_.h
$(GLSRC)gstrap.h:$(GLSRC)stdint_.h
$(GLSRC)gstrap.h:$(GLSRC)gssprintf.h
$(GLSRC)gstrap.h:$(GLSRC)gstypes.h
$(GLSRC)gstrap.h:$(GLSRC)std.h
$(GLSRC)gstrap.h:$(GLSRC)stdpre.h
$(GLSRC)gstrap.h:$(GLGEN)arch.h
$(GLSRC)gstrap.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsiparm3.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsiparam.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsipar3x.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsipar3x.h:$(GLSRC)scommon.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsccolor.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsstype.h
$(GLSRC)gsipar3x.h:$(GLSRC)gsmemory.h
$(GLSRC)gsipar3x.h:$(GLSRC)gslibctx.h
$(GLSRC)gsipar3x.h:$(GLSRC)stdio_.h
$(GLSRC)gsipar3x.h:$(GLSRC)stdint_.h
$(GLSRC)gsipar3x.h:$(GLSRC)gssprintf.h
$(GLSRC)gsipar3x.h:$(GLSRC)gstypes.h
$(GLSRC)gsipar3x.h:$(GLSRC)std.h
$(GLSRC)gsipar3x.h:$(GLSRC)stdpre.h
$(GLSRC)gsipar3x.h:$(GLGEN)arch.h
$(GLSRC)gsipar3x.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gximag3x.h:$(GLSRC)gsipar3x.h
$(GLSRC)gximag3x.h:$(GLSRC)gsiparm3.h
$(GLSRC)gximag3x.h:$(GLSRC)gxiparam.h
$(GLSRC)gximag3x.h:$(GLSRC)gxdevcli.h
$(GLSRC)gximag3x.h:$(GLSRC)gxcmap.h
$(GLSRC)gximag3x.h:$(GLSRC)gxtext.h
$(GLSRC)gximag3x.h:$(GLSRC)gstext.h
$(GLSRC)gximag3x.h:$(GLSRC)gsnamecl.h
$(GLSRC)gximag3x.h:$(GLSRC)gstparam.h
$(GLSRC)gximag3x.h:$(GLSRC)gxfmap.h
$(GLSRC)gximag3x.h:$(GLSRC)gsfunc.h
$(GLSRC)gximag3x.h:$(GLSRC)gxcspace.h
$(GLSRC)gximag3x.h:$(GLSRC)gxrplane.h
$(GLSRC)gximag3x.h:$(GLSRC)gscsel.h
$(GLSRC)gximag3x.h:$(GLSRC)gxfcache.h
$(GLSRC)gximag3x.h:$(GLSRC)gsfont.h
$(GLSRC)gximag3x.h:$(GLSRC)gsimage.h
$(GLSRC)gximag3x.h:$(GLSRC)gsdcolor.h
$(GLSRC)gximag3x.h:$(GLSRC)gxcvalue.h
$(GLSRC)gximag3x.h:$(GLSRC)gxbcache.h
$(GLSRC)gximag3x.h:$(GLSRC)gsropt.h
$(GLSRC)gximag3x.h:$(GLSRC)gxdda.h
$(GLSRC)gximag3x.h:$(GLSRC)gxpath.h
$(GLSRC)gximag3x.h:$(GLSRC)gxfrac.h
$(GLSRC)gximag3x.h:$(GLSRC)gxtmap.h
$(GLSRC)gximag3x.h:$(GLSRC)gxftype.h
$(GLSRC)gximag3x.h:$(GLSRC)gscms.h
$(GLSRC)gximag3x.h:$(GLSRC)gsrect.h
$(GLSRC)gximag3x.h:$(GLSRC)gslparam.h
$(GLSRC)gximag3x.h:$(GLSRC)gsdevice.h
$(GLSRC)gximag3x.h:$(GLSRC)gscpm.h
$(GLSRC)gximag3x.h:$(GLSRC)gscspace.h
$(GLSRC)gximag3x.h:$(GLSRC)gsgstate.h
$(GLSRC)gximag3x.h:$(GLSRC)gsxfont.h
$(GLSRC)gximag3x.h:$(GLSRC)gsdsrc.h
$(GLSRC)gximag3x.h:$(GLSRC)gsiparam.h
$(GLSRC)gximag3x.h:$(GLSRC)gxfixed.h
$(GLSRC)gximag3x.h:$(GLSRC)gscompt.h
$(GLSRC)gximag3x.h:$(GLSRC)gsmatrix.h
$(GLSRC)gximag3x.h:$(GLSRC)gspenum.h
$(GLSRC)gximag3x.h:$(GLSRC)gxhttile.h
$(GLSRC)gximag3x.h:$(GLSRC)gsparam.h
$(GLSRC)gximag3x.h:$(GLSRC)gsrefct.h
$(GLSRC)gximag3x.h:$(GLSRC)gp.h
$(GLSRC)gximag3x.h:$(GLSRC)memento.h
$(GLSRC)gximag3x.h:$(GLSRC)memory_.h
$(GLSRC)gximag3x.h:$(GLSRC)gsuid.h
$(GLSRC)gximag3x.h:$(GLSRC)gsstruct.h
$(GLSRC)gximag3x.h:$(GLSRC)gxsync.h
$(GLSRC)gximag3x.h:$(GLSRC)gxbitmap.h
$(GLSRC)gximag3x.h:$(GLSRC)srdline.h
$(GLSRC)gximag3x.h:$(GLSRC)scommon.h
$(GLSRC)gximag3x.h:$(GLSRC)gsbitmap.h
$(GLSRC)gximag3x.h:$(GLSRC)gsccolor.h
$(GLSRC)gximag3x.h:$(GLSRC)gxarith.h
$(GLSRC)gximag3x.h:$(GLSRC)stat_.h
$(GLSRC)gximag3x.h:$(GLSRC)gpsync.h
$(GLSRC)gximag3x.h:$(GLSRC)gsstype.h
$(GLSRC)gximag3x.h:$(GLSRC)gsmemory.h
$(GLSRC)gximag3x.h:$(GLSRC)gpgetenv.h
$(GLSRC)gximag3x.h:$(GLSRC)gscdefs.h
$(GLSRC)gximag3x.h:$(GLSRC)gslibctx.h
$(GLSRC)gximag3x.h:$(GLSRC)gxcindex.h
$(GLSRC)gximag3x.h:$(GLSRC)stdio_.h
$(GLSRC)gximag3x.h:$(GLSRC)gsccode.h
$(GLSRC)gximag3x.h:$(GLSRC)stdint_.h
$(GLSRC)gximag3x.h:$(GLSRC)gssprintf.h
$(GLSRC)gximag3x.h:$(GLSRC)gstypes.h
$(GLSRC)gximag3x.h:$(GLSRC)std.h
$(GLSRC)gximag3x.h:$(GLSRC)stdpre.h
$(GLSRC)gximag3x.h:$(GLGEN)arch.h
$(GLSRC)gximag3x.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxblend.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxblend.h:$(GLSRC)gxcmap.h
$(GLSRC)gxblend.h:$(GLSRC)gxtext.h
$(GLSRC)gxblend.h:$(GLSRC)gstext.h
$(GLSRC)gxblend.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxblend.h:$(GLSRC)gstparam.h
$(GLSRC)gxblend.h:$(GLSRC)gxfmap.h
$(GLSRC)gxblend.h:$(GLSRC)gsfunc.h
$(GLSRC)gxblend.h:$(GLSRC)gxcspace.h
$(GLSRC)gxblend.h:$(GLSRC)gxrplane.h
$(GLSRC)gxblend.h:$(GLSRC)gscsel.h
$(GLSRC)gxblend.h:$(GLSRC)gxfcache.h
$(GLSRC)gxblend.h:$(GLSRC)gsfont.h
$(GLSRC)gxblend.h:$(GLSRC)gsimage.h
$(GLSRC)gxblend.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxblend.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxblend.h:$(GLSRC)gxbcache.h
$(GLSRC)gxblend.h:$(GLSRC)gsropt.h
$(GLSRC)gxblend.h:$(GLSRC)gxdda.h
$(GLSRC)gxblend.h:$(GLSRC)gxpath.h
$(GLSRC)gxblend.h:$(GLSRC)gxfrac.h
$(GLSRC)gxblend.h:$(GLSRC)gxtmap.h
$(GLSRC)gxblend.h:$(GLSRC)gxftype.h
$(GLSRC)gxblend.h:$(GLSRC)gscms.h
$(GLSRC)gxblend.h:$(GLSRC)gsrect.h
$(GLSRC)gxblend.h:$(GLSRC)gslparam.h
$(GLSRC)gxblend.h:$(GLSRC)gsdevice.h
$(GLSRC)gxblend.h:$(GLSRC)gscpm.h
$(GLSRC)gxblend.h:$(GLSRC)gscspace.h
$(GLSRC)gxblend.h:$(GLSRC)gsgstate.h
$(GLSRC)gxblend.h:$(GLSRC)gsxfont.h
$(GLSRC)gxblend.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxblend.h:$(GLSRC)gsiparam.h
$(GLSRC)gxblend.h:$(GLSRC)gxfixed.h
$(GLSRC)gxblend.h:$(GLSRC)gscompt.h
$(GLSRC)gxblend.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxblend.h:$(GLSRC)gspenum.h
$(GLSRC)gxblend.h:$(GLSRC)gxhttile.h
$(GLSRC)gxblend.h:$(GLSRC)gsparam.h
$(GLSRC)gxblend.h:$(GLSRC)gsrefct.h
$(GLSRC)gxblend.h:$(GLSRC)gp.h
$(GLSRC)gxblend.h:$(GLSRC)memento.h
$(GLSRC)gxblend.h:$(GLSRC)memory_.h
$(GLSRC)gxblend.h:$(GLSRC)gsuid.h
$(GLSRC)gxblend.h:$(GLSRC)gsstruct.h
$(GLSRC)gxblend.h:$(GLSRC)gxsync.h
$(GLSRC)gxblend.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxblend.h:$(GLSRC)srdline.h
$(GLSRC)gxblend.h:$(GLSRC)scommon.h
$(GLSRC)gxblend.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxblend.h:$(GLSRC)gsccolor.h
$(GLSRC)gxblend.h:$(GLSRC)gxarith.h
$(GLSRC)gxblend.h:$(GLSRC)stat_.h
$(GLSRC)gxblend.h:$(GLSRC)gpsync.h
$(GLSRC)gxblend.h:$(GLSRC)gsstype.h
$(GLSRC)gxblend.h:$(GLSRC)gsmemory.h
$(GLSRC)gxblend.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxblend.h:$(GLSRC)gscdefs.h
$(GLSRC)gxblend.h:$(GLSRC)gslibctx.h
$(GLSRC)gxblend.h:$(GLSRC)gxcindex.h
$(GLSRC)gxblend.h:$(GLSRC)stdio_.h
$(GLSRC)gxblend.h:$(GLSRC)gsccode.h
$(GLSRC)gxblend.h:$(GLSRC)stdint_.h
$(GLSRC)gxblend.h:$(GLSRC)gssprintf.h
$(GLSRC)gxblend.h:$(GLSRC)gstypes.h
$(GLSRC)gxblend.h:$(GLSRC)std.h
$(GLSRC)gxblend.h:$(GLSRC)stdpre.h
$(GLSRC)gxblend.h:$(GLGEN)arch.h
$(GLSRC)gxblend.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcolor2.h
$(GLSRC)gdevp14.h:$(GLSRC)gxpcolor.h
$(GLSRC)gdevp14.h:$(GLSRC)gxdevmem.h
$(GLSRC)gdevp14.h:$(GLSRC)gdevdevn.h
$(GLSRC)gdevp14.h:$(GLSRC)gxdcolor.h
$(GLSRC)gdevp14.h:$(GLSRC)gxblend.h
$(GLSRC)gdevp14.h:$(GLSRC)gscolor2.h
$(GLSRC)gdevp14.h:$(GLSRC)gxdevice.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcpath.h
$(GLSRC)gdevp14.h:$(GLSRC)gsequivc.h
$(GLSRC)gdevp14.h:$(GLSRC)gxdevcli.h
$(GLSRC)gdevp14.h:$(GLSRC)gxpcache.h
$(GLSRC)gdevp14.h:$(GLSRC)gscindex.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcmap.h
$(GLSRC)gdevp14.h:$(GLSRC)gsptype1.h
$(GLSRC)gdevp14.h:$(GLSRC)gscie.h
$(GLSRC)gdevp14.h:$(GLSRC)gxtext.h
$(GLSRC)gdevp14.h:$(GLSRC)gstext.h
$(GLSRC)gdevp14.h:$(GLSRC)gsnamecl.h
$(GLSRC)gdevp14.h:$(GLSRC)gstparam.h
$(GLSRC)gdevp14.h:$(GLSRC)gspcolor.h
$(GLSRC)gdevp14.h:$(GLSRC)gxfmap.h
$(GLSRC)gdevp14.h:$(GLSRC)gsmalloc.h
$(GLSRC)gdevp14.h:$(GLSRC)gsfunc.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcspace.h
$(GLSRC)gdevp14.h:$(GLSRC)gxctable.h
$(GLSRC)gdevp14.h:$(GLSRC)gxrplane.h
$(GLSRC)gdevp14.h:$(GLSRC)gscsel.h
$(GLSRC)gdevp14.h:$(GLSRC)gxfcache.h
$(GLSRC)gdevp14.h:$(GLSRC)gsfont.h
$(GLSRC)gdevp14.h:$(GLSRC)gsimage.h
$(GLSRC)gdevp14.h:$(GLSRC)gsdcolor.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcvalue.h
$(GLSRC)gdevp14.h:$(GLSRC)gxbcache.h
$(GLSRC)gdevp14.h:$(GLSRC)gsropt.h
$(GLSRC)gdevp14.h:$(GLSRC)gxdda.h
$(GLSRC)gdevp14.h:$(GLSRC)gxpath.h
$(GLSRC)gdevp14.h:$(GLSRC)gxiclass.h
$(GLSRC)gdevp14.h:$(GLSRC)gxfrac.h
$(GLSRC)gdevp14.h:$(GLSRC)gxtmap.h
$(GLSRC)gdevp14.h:$(GLSRC)gxftype.h
$(GLSRC)gdevp14.h:$(GLSRC)gscms.h
$(GLSRC)gdevp14.h:$(GLSRC)gsrect.h
$(GLSRC)gdevp14.h:$(GLSRC)gslparam.h
$(GLSRC)gdevp14.h:$(GLSRC)gsdevice.h
$(GLSRC)gdevp14.h:$(GLSRC)gscpm.h
$(GLSRC)gdevp14.h:$(GLSRC)gscspace.h
$(GLSRC)gdevp14.h:$(GLSRC)gsgstate.h
$(GLSRC)gdevp14.h:$(GLSRC)gxstdio.h
$(GLSRC)gdevp14.h:$(GLSRC)gsxfont.h
$(GLSRC)gdevp14.h:$(GLSRC)gsdsrc.h
$(GLSRC)gdevp14.h:$(GLSRC)gsio.h
$(GLSRC)gdevp14.h:$(GLSRC)gsiparam.h
$(GLSRC)gdevp14.h:$(GLSRC)gxfixed.h
$(GLSRC)gdevp14.h:$(GLSRC)gscompt.h
$(GLSRC)gdevp14.h:$(GLSRC)gsmatrix.h
$(GLSRC)gdevp14.h:$(GLSRC)gspenum.h
$(GLSRC)gdevp14.h:$(GLSRC)gxhttile.h
$(GLSRC)gdevp14.h:$(GLSRC)gsparam.h
$(GLSRC)gdevp14.h:$(GLSRC)gsrefct.h
$(GLSRC)gdevp14.h:$(GLSRC)gp.h
$(GLSRC)gdevp14.h:$(GLSRC)memento.h
$(GLSRC)gdevp14.h:$(GLSRC)memory_.h
$(GLSRC)gdevp14.h:$(GLSRC)gsuid.h
$(GLSRC)gdevp14.h:$(GLSRC)gsstruct.h
$(GLSRC)gdevp14.h:$(GLSRC)gxsync.h
$(GLSRC)gdevp14.h:$(GLSRC)gxbitmap.h
$(GLSRC)gdevp14.h:$(GLSRC)srdline.h
$(GLSRC)gdevp14.h:$(GLSRC)scommon.h
$(GLSRC)gdevp14.h:$(GLSRC)gsfname.h
$(GLSRC)gdevp14.h:$(GLSRC)gsbitmap.h
$(GLSRC)gdevp14.h:$(GLSRC)gsccolor.h
$(GLSRC)gdevp14.h:$(GLSRC)gxarith.h
$(GLSRC)gdevp14.h:$(GLSRC)stat_.h
$(GLSRC)gdevp14.h:$(GLSRC)gpsync.h
$(GLSRC)gdevp14.h:$(GLSRC)gsstype.h
$(GLSRC)gdevp14.h:$(GLSRC)gsmemory.h
$(GLSRC)gdevp14.h:$(GLSRC)gpgetenv.h
$(GLSRC)gdevp14.h:$(GLSRC)gscdefs.h
$(GLSRC)gdevp14.h:$(GLSRC)gslibctx.h
$(GLSRC)gdevp14.h:$(GLSRC)gxcindex.h
$(GLSRC)gdevp14.h:$(GLSRC)stdio_.h
$(GLSRC)gdevp14.h:$(GLSRC)gsccode.h
$(GLSRC)gdevp14.h:$(GLSRC)stdint_.h
$(GLSRC)gdevp14.h:$(GLSRC)gssprintf.h
$(GLSRC)gdevp14.h:$(GLSRC)gstypes.h
$(GLSRC)gdevp14.h:$(GLSRC)std.h
$(GLSRC)gdevp14.h:$(GLSRC)stdpre.h
$(GLSRC)gdevp14.h:$(GLGEN)arch.h
$(GLSRC)gdevp14.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gscolor3.h:$(GLSRC)gsshade.h
$(GLSRC)gscolor3.h:$(GLSRC)gspath.h
$(GLSRC)gscolor3.h:$(GLSRC)gxmatrix.h
$(GLSRC)gscolor3.h:$(GLSRC)gscie.h
$(GLSRC)gscolor3.h:$(GLSRC)gsfunc.h
$(GLSRC)gscolor3.h:$(GLSRC)gxctable.h
$(GLSRC)gscolor3.h:$(GLSRC)gxfrac.h
$(GLSRC)gscolor3.h:$(GLSRC)gsdevice.h
$(GLSRC)gscolor3.h:$(GLSRC)gscspace.h
$(GLSRC)gscolor3.h:$(GLSRC)gsgstate.h
$(GLSRC)gscolor3.h:$(GLSRC)gsdsrc.h
$(GLSRC)gscolor3.h:$(GLSRC)gsiparam.h
$(GLSRC)gscolor3.h:$(GLSRC)gxfixed.h
$(GLSRC)gscolor3.h:$(GLSRC)gsmatrix.h
$(GLSRC)gscolor3.h:$(GLSRC)gspenum.h
$(GLSRC)gscolor3.h:$(GLSRC)gsparam.h
$(GLSRC)gscolor3.h:$(GLSRC)gsrefct.h
$(GLSRC)gscolor3.h:$(GLSRC)memento.h
$(GLSRC)gscolor3.h:$(GLSRC)gsstruct.h
$(GLSRC)gscolor3.h:$(GLSRC)gxbitmap.h
$(GLSRC)gscolor3.h:$(GLSRC)scommon.h
$(GLSRC)gscolor3.h:$(GLSRC)gsbitmap.h
$(GLSRC)gscolor3.h:$(GLSRC)gsccolor.h
$(GLSRC)gscolor3.h:$(GLSRC)gsstype.h
$(GLSRC)gscolor3.h:$(GLSRC)gsmemory.h
$(GLSRC)gscolor3.h:$(GLSRC)gslibctx.h
$(GLSRC)gscolor3.h:$(GLSRC)stdio_.h
$(GLSRC)gscolor3.h:$(GLSRC)stdint_.h
$(GLSRC)gscolor3.h:$(GLSRC)gssprintf.h
$(GLSRC)gscolor3.h:$(GLSRC)gstypes.h
$(GLSRC)gscolor3.h:$(GLSRC)std.h
$(GLSRC)gscolor3.h:$(GLSRC)stdpre.h
$(GLSRC)gscolor3.h:$(GLGEN)arch.h
$(GLSRC)gscolor3.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsfunc.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsparam.h
$(GLSRC)gsfunc3.h:$(GLSRC)memento.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsstruct.h
$(GLSRC)gsfunc3.h:$(GLSRC)scommon.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsstype.h
$(GLSRC)gsfunc3.h:$(GLSRC)gsmemory.h
$(GLSRC)gsfunc3.h:$(GLSRC)gslibctx.h
$(GLSRC)gsfunc3.h:$(GLSRC)stdio_.h
$(GLSRC)gsfunc3.h:$(GLSRC)stdint_.h
$(GLSRC)gsfunc3.h:$(GLSRC)gssprintf.h
$(GLSRC)gsfunc3.h:$(GLSRC)gstypes.h
$(GLSRC)gsfunc3.h:$(GLSRC)std.h
$(GLSRC)gsfunc3.h:$(GLSRC)stdpre.h
$(GLSRC)gsfunc3.h:$(GLGEN)arch.h
$(GLSRC)gsfunc3.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gsshade.h:$(GLSRC)gspath.h
$(GLSRC)gsshade.h:$(GLSRC)gxmatrix.h
$(GLSRC)gsshade.h:$(GLSRC)gscie.h
$(GLSRC)gsshade.h:$(GLSRC)gsfunc.h
$(GLSRC)gsshade.h:$(GLSRC)gxctable.h
$(GLSRC)gsshade.h:$(GLSRC)gxfrac.h
$(GLSRC)gsshade.h:$(GLSRC)gsdevice.h
$(GLSRC)gsshade.h:$(GLSRC)gscspace.h
$(GLSRC)gsshade.h:$(GLSRC)gsgstate.h
$(GLSRC)gsshade.h:$(GLSRC)gsdsrc.h
$(GLSRC)gsshade.h:$(GLSRC)gsiparam.h
$(GLSRC)gsshade.h:$(GLSRC)gxfixed.h
$(GLSRC)gsshade.h:$(GLSRC)gsmatrix.h
$(GLSRC)gsshade.h:$(GLSRC)gspenum.h
$(GLSRC)gsshade.h:$(GLSRC)gsparam.h
$(GLSRC)gsshade.h:$(GLSRC)gsrefct.h
$(GLSRC)gsshade.h:$(GLSRC)memento.h
$(GLSRC)gsshade.h:$(GLSRC)gsstruct.h
$(GLSRC)gsshade.h:$(GLSRC)gxbitmap.h
$(GLSRC)gsshade.h:$(GLSRC)scommon.h
$(GLSRC)gsshade.h:$(GLSRC)gsbitmap.h
$(GLSRC)gsshade.h:$(GLSRC)gsccolor.h
$(GLSRC)gsshade.h:$(GLSRC)gsstype.h
$(GLSRC)gsshade.h:$(GLSRC)gsmemory.h
$(GLSRC)gsshade.h:$(GLSRC)gslibctx.h
$(GLSRC)gsshade.h:$(GLSRC)stdio_.h
$(GLSRC)gsshade.h:$(GLSRC)stdint_.h
$(GLSRC)gsshade.h:$(GLSRC)gssprintf.h
$(GLSRC)gsshade.h:$(GLSRC)gstypes.h
$(GLSRC)gsshade.h:$(GLSRC)std.h
$(GLSRC)gsshade.h:$(GLSRC)stdpre.h
$(GLSRC)gsshade.h:$(GLGEN)arch.h
$(GLSRC)gsshade.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxshade.h:$(GLSRC)gsshade.h
$(GLSRC)gxshade.h:$(GLSRC)gspath.h
$(GLSRC)gxshade.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxshade.h:$(GLSRC)gscie.h
$(GLSRC)gxshade.h:$(GLSRC)stream.h
$(GLSRC)gxshade.h:$(GLSRC)gsfunc.h
$(GLSRC)gxshade.h:$(GLSRC)gxctable.h
$(GLSRC)gxshade.h:$(GLSRC)gxiodev.h
$(GLSRC)gxshade.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxshade.h:$(GLSRC)gxfrac.h
$(GLSRC)gxshade.h:$(GLSRC)gscms.h
$(GLSRC)gxshade.h:$(GLSRC)gsdevice.h
$(GLSRC)gxshade.h:$(GLSRC)gscspace.h
$(GLSRC)gxshade.h:$(GLSRC)gsgstate.h
$(GLSRC)gxshade.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxshade.h:$(GLSRC)gsiparam.h
$(GLSRC)gxshade.h:$(GLSRC)gxfixed.h
$(GLSRC)gxshade.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxshade.h:$(GLSRC)gspenum.h
$(GLSRC)gxshade.h:$(GLSRC)gxhttile.h
$(GLSRC)gxshade.h:$(GLSRC)gsparam.h
$(GLSRC)gxshade.h:$(GLSRC)gsrefct.h
$(GLSRC)gxshade.h:$(GLSRC)gp.h
$(GLSRC)gxshade.h:$(GLSRC)memento.h
$(GLSRC)gxshade.h:$(GLSRC)memory_.h
$(GLSRC)gxshade.h:$(GLSRC)gsstruct.h
$(GLSRC)gxshade.h:$(GLSRC)gxsync.h
$(GLSRC)gxshade.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxshade.h:$(GLSRC)srdline.h
$(GLSRC)gxshade.h:$(GLSRC)scommon.h
$(GLSRC)gxshade.h:$(GLSRC)gsfname.h
$(GLSRC)gxshade.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxshade.h:$(GLSRC)gsccolor.h
$(GLSRC)gxshade.h:$(GLSRC)gxarith.h
$(GLSRC)gxshade.h:$(GLSRC)stat_.h
$(GLSRC)gxshade.h:$(GLSRC)gpsync.h
$(GLSRC)gxshade.h:$(GLSRC)gsstype.h
$(GLSRC)gxshade.h:$(GLSRC)gsmemory.h
$(GLSRC)gxshade.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxshade.h:$(GLSRC)gscdefs.h
$(GLSRC)gxshade.h:$(GLSRC)gslibctx.h
$(GLSRC)gxshade.h:$(GLSRC)gxcindex.h
$(GLSRC)gxshade.h:$(GLSRC)stdio_.h
$(GLSRC)gxshade.h:$(GLSRC)stdint_.h
$(GLSRC)gxshade.h:$(GLSRC)gssprintf.h
$(GLSRC)gxshade.h:$(GLSRC)gstypes.h
$(GLSRC)gxshade.h:$(GLSRC)std.h
$(GLSRC)gxshade.h:$(GLSRC)stdpre.h
$(GLSRC)gxshade.h:$(GLGEN)arch.h
$(GLSRC)gxshade.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gxshade4.h:$(GLSRC)gxshade.h
$(GLSRC)gxshade4.h:$(GLSRC)gsshade.h
$(GLSRC)gxshade4.h:$(GLSRC)gscicach.h
$(GLSRC)gxshade4.h:$(GLSRC)gspath.h
$(GLSRC)gxshade4.h:$(GLSRC)gxmatrix.h
$(GLSRC)gxshade4.h:$(GLSRC)gxdevcli.h
$(GLSRC)gxshade4.h:$(GLSRC)gxcmap.h
$(GLSRC)gxshade4.h:$(GLSRC)gscie.h
$(GLSRC)gxshade4.h:$(GLSRC)gxtext.h
$(GLSRC)gxshade4.h:$(GLSRC)gstext.h
$(GLSRC)gxshade4.h:$(GLSRC)gsnamecl.h
$(GLSRC)gxshade4.h:$(GLSRC)gstparam.h
$(GLSRC)gxshade4.h:$(GLSRC)gxfmap.h
$(GLSRC)gxshade4.h:$(GLSRC)stream.h
$(GLSRC)gxshade4.h:$(GLSRC)gsfunc.h
$(GLSRC)gxshade4.h:$(GLSRC)gxcspace.h
$(GLSRC)gxshade4.h:$(GLSRC)gxctable.h
$(GLSRC)gxshade4.h:$(GLSRC)gxiodev.h
$(GLSRC)gxshade4.h:$(GLSRC)gxrplane.h
$(GLSRC)gxshade4.h:$(GLSRC)gscsel.h
$(GLSRC)gxshade4.h:$(GLSRC)gxfcache.h
$(GLSRC)gxshade4.h:$(GLSRC)gsfont.h
$(GLSRC)gxshade4.h:$(GLSRC)gsimage.h
$(GLSRC)gxshade4.h:$(GLSRC)gsdcolor.h
$(GLSRC)gxshade4.h:$(GLSRC)gxcvalue.h
$(GLSRC)gxshade4.h:$(GLSRC)gxbcache.h
$(GLSRC)gxshade4.h:$(GLSRC)gsropt.h
$(GLSRC)gxshade4.h:$(GLSRC)gxdda.h
$(GLSRC)gxshade4.h:$(GLSRC)gxpath.h
$(GLSRC)gxshade4.h:$(GLSRC)gxfrac.h
$(GLSRC)gxshade4.h:$(GLSRC)gxtmap.h
$(GLSRC)gxshade4.h:$(GLSRC)gxftype.h
$(GLSRC)gxshade4.h:$(GLSRC)gscms.h
$(GLSRC)gxshade4.h:$(GLSRC)gsrect.h
$(GLSRC)gxshade4.h:$(GLSRC)gslparam.h
$(GLSRC)gxshade4.h:$(GLSRC)gsdevice.h
$(GLSRC)gxshade4.h:$(GLSRC)gscpm.h
$(GLSRC)gxshade4.h:$(GLSRC)gscspace.h
$(GLSRC)gxshade4.h:$(GLSRC)gsgstate.h
$(GLSRC)gxshade4.h:$(GLSRC)gsxfont.h
$(GLSRC)gxshade4.h:$(GLSRC)gsdsrc.h
$(GLSRC)gxshade4.h:$(GLSRC)gsiparam.h
$(GLSRC)gxshade4.h:$(GLSRC)gxfixed.h
$(GLSRC)gxshade4.h:$(GLSRC)gscompt.h
$(GLSRC)gxshade4.h:$(GLSRC)gsmatrix.h
$(GLSRC)gxshade4.h:$(GLSRC)gspenum.h
$(GLSRC)gxshade4.h:$(GLSRC)gxhttile.h
$(GLSRC)gxshade4.h:$(GLSRC)gsparam.h
$(GLSRC)gxshade4.h:$(GLSRC)gsrefct.h
$(GLSRC)gxshade4.h:$(GLSRC)gp.h
$(GLSRC)gxshade4.h:$(GLSRC)memento.h
$(GLSRC)gxshade4.h:$(GLSRC)memory_.h
$(GLSRC)gxshade4.h:$(GLSRC)gsuid.h
$(GLSRC)gxshade4.h:$(GLSRC)gsstruct.h
$(GLSRC)gxshade4.h:$(GLSRC)gxsync.h
$(GLSRC)gxshade4.h:$(GLSRC)gxbitmap.h
$(GLSRC)gxshade4.h:$(GLSRC)srdline.h
$(GLSRC)gxshade4.h:$(GLSRC)scommon.h
$(GLSRC)gxshade4.h:$(GLSRC)gsfname.h
$(GLSRC)gxshade4.h:$(GLSRC)gsbitmap.h
$(GLSRC)gxshade4.h:$(GLSRC)gsccolor.h
$(GLSRC)gxshade4.h:$(GLSRC)gxarith.h
$(GLSRC)gxshade4.h:$(GLSRC)stat_.h
$(GLSRC)gxshade4.h:$(GLSRC)gpsync.h
$(GLSRC)gxshade4.h:$(GLSRC)gsstype.h
$(GLSRC)gxshade4.h:$(GLSRC)gsmemory.h
$(GLSRC)gxshade4.h:$(GLSRC)gpgetenv.h
$(GLSRC)gxshade4.h:$(GLSRC)gscdefs.h
$(GLSRC)gxshade4.h:$(GLSRC)gslibctx.h
$(GLSRC)gxshade4.h:$(GLSRC)gxcindex.h
$(GLSRC)gxshade4.h:$(GLSRC)stdio_.h
$(GLSRC)gxshade4.h:$(GLSRC)gsccode.h
$(GLSRC)gxshade4.h:$(GLSRC)stdint_.h
$(GLSRC)gxshade4.h:$(GLSRC)gssprintf.h
$(GLSRC)gxshade4.h:$(GLSRC)gstypes.h
$(GLSRC)gxshade4.h:$(GLSRC)std.h
$(GLSRC)gxshade4.h:$(GLSRC)stdpre.h
$(GLSRC)gxshade4.h:$(GLGEN)arch.h
$(GLSRC)gxshade4.h:$(GLSRC)gs_dll_call.h
