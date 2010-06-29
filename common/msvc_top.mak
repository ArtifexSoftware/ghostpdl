#  Copyright (C) 1997-2007 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# msvc_top.mak
# Generic top-level makefile for Win32 Microsoft Visual C (ver >= 4.1)

# The product-specific top-level makefile defines the following:
#	MAKEFILE, COMMONDIR, CONFIG, DEBUG, DEVICE_DEVS, GSDIR, MAIN_OBJ,
#	NOPRIVATE, TDEBUG, TARGET_DEVS, TARGET_XE, MSVC_VERSION
# It also must include the product-specific *.mak.

# XE isn't defined yet.
default: $(TARGET_XE).exe

# Define the ANSI-to-K&R dependency.  Borland C, Microsoft C and
# Watcom C all accept ANSI syntax, but we need to preconstruct ccf32.tr 
# to get around the limit on the maximum length of a command line.

AK=$(GLGENDIR)\ccf32.tr

# clean gs files using the, we also clean up the platform files here
# this should be separate.  TODO remove genarch.ilk and genarch.pdb as
# well.

clean_gs:
	nmake /f $(GLSRCDIR)\msvclib.mak \
	GLSRCDIR=$(GLSRCDIR) GLGENDIR=$(GLGENDIR) \
	GLOBJDIR=$(GLOBJDIR) clean
	erase $(TARGET_XE).ilk
	erase $(TARGET_XE).pdb
	erase $(TARGET_XE).exp
	erase $(TARGET_XE).lib

# Define names of utility programs
AUXGENDIR=$(GLGENDIR)
AUXGEN=$(AUXGENDIR)$(D)
ANSI2KNR_XE=$(AUXGEN)ansi2knr.exe
ECHOGS_XE=$(AUXGEN)echogs.exe
GENARCH_XE=$(AUXGEN)genarch.exe
GENCONF_XE=$(AUXGEN)genconf.exe
GENDEV_XE=$(AUXGEN)gendev.exe
GENINIT_XE=$(AUXGEN)geninit.exe
GENHT_XE=$(AUXGEN)genht.exe
MKROMFS_XE=$(AUXGEN)mkromfs$(XEAUX)


# Platform specification
#**************** PATCHES expected by some of the GS files
JGENDIR=$(GLGENDIR)
JOBJDIR=$(GLOBJDIR)
PNGGENDIR=$(GLGENDIR)
PNGOBJDIR=$(GLOBJDIR)
ZGENDIR=$(GLGENDIR)
ZOBJDIR=$(GLOBJDIR)
GLSRC=$(GLSRCDIR)$(D)
GLGEN=$(GLGENDIR)$(D)
GLOBJ=$(GLOBJDIR)$(D)
FTSRCDIR=$(GLSRCDIR)$(D)..$(D)freetype
#**************** END PATCHES


!if "$(PSICFLAGS)" == "/DPSI_INCLUDED"
# 1 --> Use 64 bits for gx_color_index.  This is required only for
# non standard devices or DeviceN process color model devices.
USE_LARGE_COLOR_INDEX=1

!if $(USE_LARGE_COLOR_INDEX) == 1
# Definitions to force gx_color_index to 64 bits
LARGEST_UINTEGER_TYPE=unsigned __int64
GX_COLOR_INDEX_TYPE=$(LARGEST_UINTEGER_TYPE)

CFLAGS=$(CFLAGS) /DGX_COLOR_INDEX_TYPE="$(GX_COLOR_INDEX_TYPE)"
!endif
!endif # psi included.

!include $(COMMONDIR)\msvcdefs.mak
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\generic.mak
!include $(GLSRCDIR)\msvccmd.mak
!include $(GLSRCDIR)\lib.mak
!include $(GLSRCDIR)\msvctail.mak

!IF "$(PSICFLAGS)" == "/DPSI_INCLUDED"

# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs .c files are out of date.

FORCE:

$(GENDIR)/ldgs.tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	$(MAKE) /F $(PSSRCDIR)\msvc32.mak MSVC_VERSION="$(MSVC_VERSION)" \
	GLSRCDIR="$(GLSRCDIR)" DEBUG=$(DEBUG) NOPRIVATE=$(NOPRIVATE) \
	DEBUGSYM=$(DEBUGSYM) TDEBUG=$(TDEBUG) \
	GLGENDIR="$(GLGENDIR)" GLOBJDIR="$(GLOBJDIR)" \
	EXPATSRCDIR="$(EXPATSRCDIR)" SHARE_EXPAT="$(SHARE_EXPAT)" \
	EXPAT_CFLAGS="$(EXPAT_CFLAGS)" LCMSSRCDIR="$(LCMSSRCDIR)" \
	LCMSPLATFORM="$(LCMSPLATFORM)" \
	ICCSRCDIR="$(ICCSRCDIR)" IMDISRCDIR="$(IMDISRCDIR)" \
	PNGSRCDIR="$(PNGSRCDIR)" \
	SHARE_LIBPNG="$(SHARE_LIBPNG)" \
	TIFFSRCDIR="$(TIFFSRCDIR)" \
	TIFFCONFIG_SUFFIX="$(TIFFCONFIG_SUFFIX)" \
	TIFFPLATFORM="$(TIFFPLATFORM)" \
	JSRCDIR="$(JSRCDIR)" \
	ZSRCDIR="$(ZSRCDIR)" ZGENDIR="$(ZGENDIR)" ZOBJDIR="$(ZOBJDIR)" ZLIB_NAME="$(ZLIB_NAME)" SHARE_ZLIB="$(SHARE_ZLIB)" \
	PSSRCDIR=$(PSSRCDIR) PSGENDIR=$(GENDIR) \
	PSLIBDIR=$(PSLIBDIR) PSRESDIR=$(PSRESDIR)\
   FTSRCDIR="$(FTSRCDIR)" \
	DEVSTUDIO="$(DEVSTUDIO)" \
	XCFLAGS="$(XCFLAGS)" \
	SBRFLAGS="$(SBRFLAGS)" \
	COMPILE_INITS=$(COMPILE_INITS) PCLXL_ROMFS_ARGS="$(PCLXL_ROMFS_ARGS)" PJL_ROMFS_ARGS="$(PJL_ROMFS_ARGS)" \
	UFST_ROOT=$(UFST_ROOT) UFST_BRIDGE=$(UFST_BRIDGE) UFST_LIB_EXT=$(UFST_LIB_EXT) \
	UFST_ROMFS_ARGS="$(UFST_ROMFS_ARGS)" \
	UFST_CFLAGS="$(UFST_CFLAGS)" \
	FEATURE_DEVS="$(FEATURE_DEVS)" DEVICE_DEVS="$(DEVICE_DEVS)" \
	BAND_LIST_STORAGE=$(BAND_LIST_STORAGE) BAND_LIST_COMPRESSOR=$(BAND_LIST_COMPRESSOR) \
	CPU_TYPE="$(CPU_TYPE)" CONFIG="$(CONFIG)" \
	$(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\echogs.exe \
	$(GLOBJDIR)\ld.tr $(GLOBJDIR)\gconfig.$(OBJ) \
	$(GLOBJDIR)\gscdefs.$(OBJ) $(GLOBJDIR)\iconfig.$(OBJ) \
	$(GLOBJDIR)\gsromfs$(COMPILE_INITS).$(OBJ)
	$(CP_) $(GENDIR)\ld.tr $(GENDIR)\ldgs.tr

!ELSE

FORCE:
# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs .c files are out of date.
# We make a dummy gs_init.ps since this is hard coded as a dependency of gsromfs.c
# to avoid having to define everything in the top level makefiles (also of a hack)
$(GENDIR)/ldgs.tr: FORCE
	-echo $(PSICFLAGS)
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	echo > $(GLOBJDIR)/gs_init.ps
	$(MAKE) /F $(GLSRCDIR)\msvclib.mak MSVC_VERSION="$(MSVC_VERSION)" \
	GLSRCDIR="$(GLSRCDIR)" DEBUG=$(DEBUG) NOPRIVATE=$(NOPRIVATE) \
	DEBUGSYM=$(DEBUGSYM) TDEBUG=$(TDEBUG) \
	GLGENDIR="$(GLGENDIR)" GLOBJDIR="$(GLOBJDIR)" \
	EXPATSRCDIR="$(EXPATSRCDIR)" SHARE_EXPAT="$(SHARE_EXPAT)" \
	EXPAT_CFLAGS="$(EXPAT_CFLAGS)" LCMSSRCDIR="$(LCMSSRCDIR)" \
	LCMSPLATFORM="$(LCMSPLATFORM)" \
	ICCSRCDIR="$(ICCSRCDIR)" IMDISRCDIR="$(IMDISRCDIR)" \
	PNGSRCDIR="$(PNGSRCDIR)" \
	SHARE_LIBPNG="$(SHARE_LIBPNG)" \
	TIFFSRCDIR="$(TIFFSRCDIR)" \
	TIFFCONFIG_SUFFIX="$(TIFFCONFIG_SUFFIX)" \
	TIFFPLATFORM="$(TIFFPLATFORM)" \
	JSRCDIR="$(JSRCDIR)" \
	ZSRCDIR="$(ZSRCDIR)" ZGENDIR="$(ZGENDIR)" ZOBJDIR="$(ZOBJDIR)" ZLIB_NAME="$(ZLIB_NAME)" SHARE_ZLIB="$(SHARE_ZLIB)" \
	PSSRCDIR=$(PSSRCDIR) PSGENDIR=$(GENDIR) \
	PSLIBDIR=$(PSLIBDIR) PSRESDIR=$(PSRESDIR)\
   FTSRCDIR="$(FTSRCDIR)" \
	DEVSTUDIO="$(DEVSTUDIO)" \
	XCFLAGS="$(XCFLAGS)" \
	SBRFLAGS="$(SBRFLAGS)" \
	COMPILE_INITS=$(COMPILE_INITS) PCLXL_ROMFS_ARGS="$(PCLXL_ROMFS_ARGS)" PJL_ROMFS_ARGS="$(PJL_ROMFS_ARGS)" \
	UFST_ROOT=$(UFST_ROOT) UFST_BRIDGE=$(UFST_BRIDGE) UFST_LIB_EXT=$(UFST_LIB_EXT) \
	UFST_ROMFS_ARGS="$(UFST_ROMFS_ARGS)" \
	UFST_CFLAGS="$(UFST_CFLAGS)" \
	PSD="$(GENDIR)/" \
	FEATURE_DEVS="$(FEATURE_DEVS)" DEVICE_DEVS="$(DEVICE_DEVS)" \
	BAND_LIST_STORAGE=$(BAND_LIST_STORAGE) BAND_LIST_COMPRESSOR=$(BAND_LIST_COMPRESSOR) \
	GLOBJ=$(GLOBJ) GLGEN=$(GLGEN) \
	CPU_TYPE="$(CPU_TYPE)" CONFIG="$(CONFIG)" \
	$(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\echogs.exe \
	$(GLOBJDIR)\ld.tr $(GLOBJDIR)\gconfig.$(OBJ) \
	$(GLOBJDIR)\gscdefs.$(OBJ) $(GLOBJDIR)\gsromfs$(COMPILE_INITS).$(OBJ)
	$(CP_) $(GENDIR)\ld.tr $(GENDIR)\ldgs.tr

!ENDIF

# Build the configuration file.
$(GENDIR)\pconf.h $(GENDIR)\ldconf.tr: $(TARGET_DEVS) $(GLOBJDIR)\genconf$(XE)
	$(GLOBJDIR)\genconf -n - $(TARGET_DEVS) -h $(GENDIR)\pconf.h -ol $(GENDIR)\ldconf.tr

!if "$(TDEBUG)" == "1"
$(GENDIR)\lib32.rsp: $(MAKEFILE)
	echo /NODEFAULTLIB:LIBC.lib > $(GENDIR)\lib32.rsp
	echo /NODEFAULTLIB:LIBCMT.lib >> $(GENDIR)\lib32.rsp
	echo LIBCMTD.lib >> $(GENDIR)\lib32.rsp
!else
$(GENDIR)\lib32.rsp: $(MAKEFILE)
	echo /NODEFAULTLIB:LIBC.lib > $(GENDIR)\lib32.rsp
	echo /NODEFAULTLIB:LIBCMTD.lib >> $(GENDIR)\lib32.rsp
	echo LIBCMT.lib >> $(GENDIR)\lib32.rsp
!endif

# Link an MS executable.
$(GENDIR)\ldall.tr: $(MAKEFILE) $(GENDIR)\ldgs.tr $(GENDIR)\ldconf.tr $(GENDIR)\lib32.rsp
	echo /SUBSYSTEM:CONSOLE >$(GENDIR)\ldall.tr
	$(CP_) $(GENDIR)\ldall.tr+$(GENDIR)\ldgs.tr $(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gsargs.$(OBJ) >>$(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gconfig.$(OBJ) >>$(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gscdefs.$(OBJ) >>$(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gsromfs$(COMPILE_INITS).$(OBJ) >>$(GENDIR)\ldall.tr
	$(CP_) $(GENDIR)\ldall.tr+$(GENDIR)\ldconf.tr $(GENDIR)\ldall.tr

# AGFA Workaround to add needed ufst font libraries.
!IF "$(PL_SCALER)" == "ufst"
FONTLIB=$(GENDIR)\fontlib.tr
$(FONTLIB): $(MAKEFILE)
	echo $(UFST_LIB)\fco_lib.lib >>$(FONTLIB)
	echo $(UFST_LIB)\if_lib.lib >>$(FONTLIB)
	echo $(UFST_LIB)\tt_lib.lib >>$(FONTLIB)

$(TARGET_XE)$(XE): $(GENDIR)\ldall.tr $(MAIN_OBJ) $(TOP_OBJ) $(LIBCTR) $(FONTLIB)
	$(LINK_SETUP)
	$(LINK) $(LCT) /OUT:$(TARGET_XE)$(XE) $(MAIN_OBJ) $(TOP_OBJ) @$(GENDIR)\ldall.tr @$(GENDIR)\lib32.rsp @$(LIBCTR) @$(FONTLIB)

!ELSE
$(TARGET_XE)$(XE): $(GENDIR)\ldall.tr $(MAIN_OBJ) $(TOP_OBJ) $(LIBCTR)
	$(LINK_SETUP)
	$(LINK) $(LCT) /OUT:$(TARGET_XE)$(XE) $(MAIN_OBJ) $(TOP_OBJ) @$(GENDIR)\ldall.tr @$(GENDIR)\lib32.rsp @$(LIBCTR)
!ENDIF
