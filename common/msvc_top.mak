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

# Platform-specific files that need to be deleted when 'nmake clean'
CLEAN_PLATFORM_FILES=$(TARGET_XE).ilk $(TARGET_XE).pdb

# Define the ANSI-to-K&R dependency.  Borland C, Microsoft C and
# Watcom C all accept ANSI syntax, but we need to preconstruct ccf32.tr 
# to get around the limit on the maximum length of a command line.

AK=$(GLGENDIR)\ccf32.tr

# Define names of utility programs
AUXGENDIR=$(GLGENDIR)
AUXGEN=$(AUXGENDIR)$(D)
ANSI2KNR_XE=$(AUXGEN)ansi2knr$(XEAUX)
ECHOGS_XE=$(AUXGEN)echogs$(XEAUX)
GENARCH_XE=$(AUXGEN)genarch$(XEAUX)
GENCONF_XE=$(AUXGEN)genconf$(XEAUX)
GENDEV_XE=$(AUXGEN)gendev$(XEAUX)
GENINIT_XE=$(AUXGEN)geninit$(XEAUX)


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
!include $(GLSRCDIR)\msvctail.mak

# Build the required files in the GS directory.
$(GENDIR)/ldl$(CONFIG).tr: $(MAKEFILE) $(AK)
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	echo MSVC_VERSION=$(MSVC_VERSION) >$(GENDIR)\_vc_temp.mak
	echo GLSRCDIR=$(GLSRCDIR) >>$(GENDIR)\_vc_temp.mak
	echo GLGENDIR=$(GLGENDIR) >>$(GENDIR)\_vc_temp.mak
	echo GLOBJDIR=$(GLOBJDIR) >>$(GENDIR)\_vc_temp.mak
	echo PSRCDIR=$(PSRCDIR) >>$(GENDIR)\_vc_temp.mak
	echo PVERSION=$(PVERSION) >>$(GENDIR)\_vc_temp.mak
	echo JSRCDIR=$(JSRCDIR) >>$(GENDIR)\_vc_temp.mak
	echo JVERSION=$(JVERSION) >>$(GENDIR)\_vc_temp.mak
	echo ZSRCDIR=$(ZSRCDIR) >>$(GENDIR)\_vc_temp.mak
	echo DEVSTUDIO=$(DEVSTUDIO) >>$(GENDIR)\_vc_temp.mak
	echo FEATURE_DEVS=$(FEATURE_DEVS) >>$(GENDIR)\_vc_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) bbox.dev >>$(GENDIR)\_vc_temp.mak
	echo BAND_LIST_STORAGE=memory >>$(GENDIR)\_vc_temp.mak
	echo BAND_LIST_COMPRESSOR=zlib >>$(GENDIR)\_vc_temp.mak
	echo FPU_TYPE=$(FPU_TYPE) >>$(GENDIR)\_vc_temp.mak
	echo CPU_TYPE=$(CPU_TYPE) >>$(GENDIR)\_vc_temp.mak
	echo CONFIG=$(CONFIG) >>$(GENDIR)\_vc_temp.mak
	echo !include $(GLSRCDIR)\msvclib.mak >>$(GENDIR)\_vc_temp.mak
	$(MAKE) /F $(GENDIR)\_vc_temp.mak CONFIG=$(CONFIG) $(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\gsnogc.$(OBJ) $(GLOBJDIR)\echogs.exe
	$(MAKE) /F $(GENDIR)\_vc_temp.mak CONFIG=$(CONFIG) $(GLOBJDIR)\ld$(CONFIG).tr $(GLOBJDIR)\gconfig$(CONFIG).$(OBJ) $(GLOBJDIR)\gscdefs$(CONFIG).$(OBJ)
	rem --------------------
	del $(GENDIR)\_vc_temp.mak
	rem Use type rather than copy to update the creation time
	type $(GENDIR)\ld$(CONFIG).tr >$(GENDIR)\ldl$(CONFIG).tr

# Build the configuration file.
$(GENDIR)\pconf$(CONFIG).h $(GENDIR)\ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GLOBJDIR)\genconf$(XE)
	$(GLOBJDIR)\genconf -n - $(TARGET_DEVS) -h $(GENDIR)\pconf$(CONFIG).h -p &ps -ol $(GENDIR)\ldconf$(CONFIG).tr

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
