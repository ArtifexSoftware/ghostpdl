#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# watc_top.mak
# Generic top-level makefile for MS-DOS/Watcom platforms.

# The product-specific top-level makefile defines the following:
#	MAKEFILE, COMMONDIR, CONFIG, DEBUG, DEVICE_DEVS, GSDIR, MAIN_OBJ,
#	NOPRIVATE, TDEBUG, TARGET_DEVS, TARGET_XE, WCVERSION
# It also must include the product-specific *.mak.

# XE isn't defined yet.
default: $(TARGET_XE).exe

# Platform specification
!include $(COMMONDIR)\watcdefs.mak
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\generic.mak
!include $(GSDIR)\wccommon.mak
!include $(GSDIR)\version.mak

# Build the required files in the GS directory.
ld$(CONFIG).tr: $(MAKEFILE) $(ECHOGS_XE)
	echo WCVERSION=$(WCVERSION) >$(GSDIR)\_wm_temp.mak
	echo FEATURE_DEVS=$(FEATURE_DEVS) >>$(GSDIR)\_wm_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) bbox.dev >>$(GSDIR)\_wm_temp.mak
	echo BAND_LIST_STORAGE=memory >>$(GSDIR)\_wm_temp.mak
	echo BAND_LIST_COMPRESSOR=zlib >>$(GSDIR)\_wm_temp.mak
	echo !include watclib.mak >>$(GSDIR)\_wm_temp.mak
	cd >$(GSDIR)\_wm_dir.bat
	cd $(GSDIR)
	$(ECHOGS_XE) -w _wm_cdir.bat cd -s -r _wm_dir.bat
	wmakel -u -n -h -f _wm_temp.mak CONFIG=$(CONFIG) gsargs.$(OBJ) gsnogc.$(OBJ) >_wm_temp.bat
	call _wm_temp.bat
	wmakel -u -n -h -f _wm_temp.mak CONFIG=$(CONFIG) ld$(CONFIG).tr gconfig$(CONFIG).$(OBJ) gscdefs$(CONFIG).$(OBJ) >_wm_temp.bat
	call _wm_temp.bat
	call _wm_cdir.bat
	rem	Use type rather than copy to update the creation time
	type $(GSDIR)\ld$(CONFIG).tr >ld$(CONFIG).tr

# Build the configuration file.
pconf$(CONFIG).h ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GSDIR)\genconf$(XE)
	$(GSDIR)\genconf -n - $(TARGET_DEVS) -h pconf$(CONFIG).h -p FILE&s&ps -ol ldconf$(CONFIG).tr

# Link a Watcom executable.
ldt$(CONFIG).tr: $(MAKEFILE) ld$(CONFIG).tr ldconf$(CONFIG).tr
	echo SYSTEM DOS4G >ldt$(CONFIG).tr
	echo OPTION STUB=$(STUB) >>ldt$(CONFIG).tr
	echo OPTION STACK=16k >>ldt$(CONFIG).tr
	echo PATH $(GSDIR) >>ldt$(CONFIG).tr
	$(CP_) ldt$(CONFIG).tr+ld$(CONFIG).tr
	echo FILE gsargs.$(OBJ) >>ldt$(CONFIG).tr
	echo FILE gsnogc.$(OBJ) >>ldt$(CONFIG).tr
	echo FILE gconfig$(CONFIG).$(OBJ) >>ldt$(CONFIG).tr
	echo FILE gscdefs$(CONFIG).$(OBJ) >>ldt$(CONFIG).tr
	echo PATH . >>ldt$(CONFIG).tr
	$(CP_) ldt$(CONFIG).tr+ldconf$(CONFIG).tr

# Link the executable.
$(TARGET_XE)$(XE): ldt$(CONFIG).tr $(MAIN_OBJ)
	$(LINK) $(LCT) NAME $(TARGET_XE) OPTION MAP=$(TARGET_XE) FILE $(MAIN_OBJ) @ldt$(CONFIG).tr
