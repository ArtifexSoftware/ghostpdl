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

AK=ccf32.tr

# Platform specification
!include $(COMMONDIR)\msvcdefs.mak
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\generic.mak
!include $(GSDIR)\msvccmd.mak
!include $(GSDIR)\version.mak
!include $(GSDIR)\msvctail.mak

CCLEAFFLAGS=$(COMPILE_WITHOUT_FRAMES)

# Build the required files in the GS directory.
ld$(CONFIG).tr: $(MAKEFILE) $(AK)
	echo MSVC_VERSION=$(MSVC_VERSION) >$(GSDIR)\_vc_temp.mak
	echo DEVSTUDIO=$(DEVSTUDIO) >>$(GSDIR)\_vc_temp.mak
	echo FEATURE_DEVS=$(FEATURE_DEVS) >>$(GSDIR)\_vc_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) bbox.dev >>$(GSDIR)\_vc_temp.mak
	echo BAND_LIST_STORAGE=memory >>$(GSDIR)\_vc_temp.mak
	echo BAND_LIST_COMPRESSOR=zlib >>$(GSDIR)\_vc_temp.mak
	echo FPU_TYPE=$(FPU_TYPE) >>$(GSDIR)\_vc_temp.mak
	echo CPU_TYPE=$(CPU_TYPE) >>$(GSDIR)\_vc_temp.mak
	echo CONFIG=$(CONFIG) >>$(GSDIR)\_vc_temp.mak
	echo !include msvclib.mak >>$(GSDIR)\_vc_temp.mak
	rem --- Create/use BAT file since CD in a bat file is not effective here
	-cd >$(GSDIR)\_vc_dir.bat
	echo cd $(GSDIR) >$(GSDIR)\_vc_make.bat
	echo $(MAKE) /F _vc_temp.mak CONFIG=$(CONFIG) gsargs.$(OBJ) gsnogc.$(OBJ) echogs.exe >>$(GSDIR)\_vc_make.bat
	echo $(MAKE) /F _vc_temp.mak CONFIG=$(CONFIG) ld$(CONFIG).tr gconfig$(CONFIG).$(OBJ) gscdefs$(CONFIG).$(OBJ) >>$(GSDIR)\_vc_make.bat
	echo $(ECHOGS_XE) -w _wm_cdir.bat cd -s -r _vc_dir.bat >>$(GSDIR)\_vc_make.bat
	echo _wm_cdir.bat >>$(GSDIR)\_vc_make.bat
	call $(GSDIR)\_vc_make.bat
	rem --------------------
	del $(GSDIR)\_vc_temp.mak
	del $(GSDIR)\_vc_dir.bat
	del $(GSDIR)\_vc_make.bat
	del $(GSDIR)\_wm_cdir.bat
	rem Use type rather than copy to update the creation time
	type $(GSDIR)\ld$(CONFIG).tr >ld$(CONFIG).tr

# Build the configuration file.
pconf$(CONFIG).h ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GSDIR)\genconf$(XE)
	$(GSDIR)\genconf -n - $(TARGET_DEVS) -h pconf$(CONFIG).h -p &ps -ol ldconf$(CONFIG).tr

# Link an MS executable.
ldt$(CONFIG).tr: $(MAKEFILE) ld$(CONFIG).tr ldconf$(CONFIG).tr
	echo /SUBSYSTEM:CONSOLE >ldt$(CONFIG).tr
	$(CP_) ldt$(CONFIG).tr+ld$(CONFIG).tr
	echo $(GSDIR)\gsargs.$(OBJ) >>ldt$(CONFIG).tr
	echo $(GSDIR)\gsnogc.$(OBJ) >>ldt$(CONFIG).tr
	echo $(GSDIR)\gconfig$(CONFIG).$(OBJ) >>ldt$(CONFIG).tr
	echo $(GSDIR)\gscdefs$(CONFIG).$(OBJ) >>ldt$(CONFIG).tr
	$(CP_) ldt$(CONFIG).tr+ldconf$(CONFIG).tr

# Link the executable. Force LIB to ref GS 'coz ld$(CONFIG).tr obj's have no pathname
!ifdef LIB
OLD_LIB=$(LIB)
!endif

$(TARGET_XE)$(XE): ldt$(CONFIG).tr $(MAIN_OBJ) $(LIBCTR)
	rem Set LIB env var to alloc linker to find GS object files
	set LIB=$(GSDIR)
	$(LINK_SETUP)
	$(LINK) $(LCT) /OUT:$(TARGET_XE)$(XE) $(MAIN_OBJ) @ldt$(CONFIG).tr @$(LIBCTR)
	set LIB=$(OLD_LIB)

!ifdef OLD_LIB
! undef OLD_LIB
!endif
