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
!include $(GLSRCDIR)\version.mak
!include $(GLSRCDIR)\lib.mak
!include $(GLSRCDIR)\msvctail.mak

FORCE:

# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs .c files are out of date.
$(GENDIR)/ldl$(CONFIG).tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	$(MAKE) /F $(GLSRCDIR)\msvclib.mak MSVC_VERSION="$(MSVC_VERSION)" \
	GLSRCDIR="$(GLSRCDIR)" \
	GLGENDIR="$(GLGENDIR)" GLOBJDIR="$(GLOBJDIR)" \
	PSRCDIR="$(PSRCDIR)" PVERSION="$(PVERSION)" \
	JSRCDIR="$(JSRCDIR)" JVERSION="$(JVERSION)" \
	ZSRCDIR="$(ZSRCDIR)" DEVSTUDIO="$(DEVSTUDIO)" \
	FEATURE_DEVS="$(FEATURE_DEVS)" DEVICE_DEVS="$(DEVICE_DEVS)" \
	BAND_LIST_STORAGE=memory BAND_LIST_COMPRESSOR=zlib \
	FPU_TYPE="$(FPU_TYPE)" CPU_TYPE="$(CPU_TYPE)" CONFIG="$(CONFIG)" \
	$(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\gsnogc.$(OBJ) $(GLOBJDIR)\echogs.exe \
	$(GLOBJDIR)\ld$(CONFIG).tr $(GLOBJDIR)\gconfig$(CONFIG).$(OBJ) \
	$(GLOBJDIR)\gscdefs$(CONFIG).$(OBJ)
	$(CP_) $(GENDIR)\ld$(CONFIG).tr >$(GENDIR)\ldl$(CONFIG).tr

# Build the configuration file.
$(GENDIR)\pconf$(CONFIG).h $(GENDIR)\ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GLOBJDIR)\genconf$(XE)
	$(GLOBJDIR)\genconf -n - $(TARGET_DEVS) -h $(GENDIR)\pconf$(CONFIG).h -ol $(GENDIR)\ldconf$(CONFIG).tr

# Link an MS executable.
$(GENDIR)\ldt$(CONFIG).tr: $(MAKEFILE) $(GENDIR)\ldl$(CONFIG).tr $(GENDIR)\ldconf$(CONFIG).tr
	echo /SUBSYSTEM:CONSOLE >$(GENDIR)\ldt$(CONFIG).tr
	$(CP_) $(GENDIR)\ldt$(CONFIG).tr+$(GENDIR)\ld$(CONFIG).tr $(GENDIR)\ldt$(CONFIG).tr
	echo $(GLOBJDIR)\gsargs.$(OBJ) >>$(GENDIR)\ldt$(CONFIG).tr
	echo $(GLOBJDIR)\gsnogc.$(OBJ) >>$(GENDIR)\ldt$(CONFIG).tr
	echo $(GLOBJDIR)\gconfig$(CONFIG).$(OBJ) >>$(GENDIR)\ldt$(CONFIG).tr
	echo $(GLOBJDIR)\gscdefs$(CONFIG).$(OBJ) >>$(GENDIR)\ldt$(CONFIG).tr
	$(CP_) $(GENDIR)\ldt$(CONFIG).tr+$(GENDIR)\ldconf$(CONFIG).tr $(GENDIR)\ldt$(CONFIG).tr

# Link the executable. Force LIB to ref GS 'coz ld$(CONFIG).tr obj's have no pathname
!ifdef LIB
OLD_LIB=$(LIB)
!endif

$(TARGET_XE)$(XE): $(GENDIR)\ldt$(CONFIG).tr $(MAIN_OBJ) $(LIBCTR)
	rem Set LIB env var to alloc linker to find GS object files
	set LIB=$(GLGENDIR)
	$(LINK_SETUP)
	$(LINK) $(LCT) /OUT:$(TARGET_XE)$(XE) $(MAIN_OBJ) @$(GENDIR)\ldt$(CONFIG).tr @$(LIBCTR)
	set LIB=$(OLD_LIB)

!ifdef OLD_LIB
! undef OLD_LIB
!endif
