#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

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
#**************** END PATCHES
!include $(COMMONDIR)\msvcdefs.mak
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\generic.mak
!include $(GLSRCDIR)\msvccmd.mak
!include $(GLSRCDIR)\lib.mak
!include $(GLSRCDIR)\msvctail.mak

FORCE:

# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs .c files are out of date.
$(GENDIR)/ldgs.tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	$(MAKE) /F $(GLSRCDIR)\msvclib.mak MSVC_VERSION="$(MSVC_VERSION)" \
	GLSRCDIR="$(GLSRCDIR)" DEBUG=$(DEBUG) NOPRIVATE=$(NOPRIVATE) \
	GLGENDIR="$(GLGENDIR)" GLOBJDIR="$(GLOBJDIR)" \
	PSRCDIR="$(PSRCDIR)" PVERSION="$(PVERSION)" \
	JSRCDIR="$(JSRCDIR)" JVERSION="$(JVERSION)" \
	ZSRCDIR="$(ZSRCDIR)" ZGENDIR="$(ZGENDIR)" ZOBJDIR="$(ZOBJDIR)" ZLIB_NAME="$(ZLIB_NAME)" SHARE_ZLIB="$(SHARE_ZLIB)" \
	DEVSTUDIO="$(DEVSTUDIO)" \
	FEATURE_DEVS="$(FEATURE_DEVS)" DEVICE_DEVS="$(DEVICE_DEVS)" \
	BAND_LIST_STORAGE=$(BAND_LIST_STORAGE) BAND_LIST_COMPRESSOR=$(BAND_LIST_COMPRESSOR) \
	FPU_TYPE="$(FPU_TYPE)" CPU_TYPE="$(CPU_TYPE)" CONFIG="$(CONFIG)" \
	$(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\echogs.exe \
	$(GLOBJDIR)\ld.tr $(GLOBJDIR)\gconfig.$(OBJ) \
	$(GLOBJDIR)\gscdefs.$(OBJ)
	$(CP_) $(GENDIR)\ld.tr $(GENDIR)\ldgs.tr

# Build the configuration file.
$(GENDIR)\pconf.h $(GENDIR)\ldconf.tr: $(TARGET_DEVS) $(GLOBJDIR)\genconf$(XE)
	$(GLOBJDIR)\genconf -n - $(TARGET_DEVS) -h $(GENDIR)\pconf.h -ol $(GENDIR)\ldconf.tr

# Link an MS executable.
$(GENDIR)\ldall.tr: $(MAKEFILE) $(GENDIR)\ldgs.tr $(GENDIR)\ldconf.tr
	echo /SUBSYSTEM:CONSOLE >$(GENDIR)\ldall.tr
	$(CP_) $(GENDIR)\ldall.tr+$(GENDIR)\ldgs.tr $(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gsargs.$(OBJ) >>$(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gconfig.$(OBJ) >>$(GENDIR)\ldall.tr
	echo $(GLOBJDIR)\gscdefs.$(OBJ) >>$(GENDIR)\ldall.tr
	$(CP_) $(GENDIR)\ldall.tr+$(GENDIR)\ldconf.tr $(GENDIR)\ldall.tr

!IF "$(PL_FONT_SCALER)" == "ufst"
# HACK in the AGFA STUFF here.

FONTLIB=$(GENDIR)\fontlib.tr


# I have no idea what NODEFAULTLIB means.
$(FONTLIB): $(MAKEFILE)
	echo /NODEFAULTLIB:LIBC.lib > $(FONTLIB)
        echo $(UFST_LIBDIR)\fco_lib.lib >>$(FONTLIB)
        echo $(UFST_LIBDIR)\if_lib.lib >>$(FONTLIB)
        echo $(UFST_LIBDIR)\tt_lib.lib >>$(FONTLIB)
!ELSE
FONTLIB=
!ENDIF

$(TARGET_XE)$(XE): $(GENDIR)\ldall.tr $(MAIN_OBJ) $(TOP_OBJ) $(LIBCTR) $(FONTLIB)
	$(LINK_SETUP)
	$(LINK) $(LCT) /OUT:$(TARGET_XE)$(XE) $(MAIN_OBJ) $(TOP_OBJ) @$(GENDIR)\ldall.tr @$(LIBCTR) @$(FONTLIB)
