#    Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL XL interpreter, MS-DOS / Watcom C platform.

# ------------------------------- Options ------------------------------- #

# Define generic configuration options.
DEBUG=1
TDEBUG=0
NOPRIVATE=0

# Define the directory with the Ghostscript source files.
#GSDIR=..
GSDIR=\gs
GD=$(GSDIR)\\

# Define the names of the executable files.
PCL5=pcl5
PCLXL=pclxl
# Define the target.  For PCL5:
#LANGUAGE=pcl5
#CONFIG=5
#TARGET_DEVS=pcl5c.dev hpgl2c.dev
#TARGET_XE=$(PCL5)
# For PCL XL:
LANGUAGE=pclxl
CONFIG=6
TARGET_DEVS=pclxl.dev
TARGET_XE=$(PCLXL)

# Define Watcom options.

DEVICE_DEVS=vga.dev djet500.dev ljet4.dev pcxmono.dev pcxgray.dev

MAKEFILE=watcpcl.mak

# Define parameters for a Watcom build.
GS=gs386
WCVERSION=10.0

PLATOPT=-i=$(GSDIR)

!include $(GSDIR)\wccommon.mak
!include $(GSDIR)\version.mak

.c.obj:
	$(CCC) $<

!include $(LANGUAGE).mak
!include pcllib.mak

# Build the required files in the GS directory.
ld$(LANGUAGE).tr: $(MAKEFILE) $(LANGUAGE).mak $(GSDIR)\echogs$(XE)
	echo WCVERSION=$(WCVERSION) >$(GSDIR)\_wm_temp.mak
	echo FEATURE_DEVS=$(FEATURE_DEVS) >>$(GSDIR)\_wm_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) >>$(GSDIR)\_wm_temp.mak
	echo BAND_LIST_STORAGE=memory >>$(GSDIR)\_wm_temp.mak
	echo BAND_LIST_COMPRESSOR=zlib >>$(GSDIR)\_wm_temp.mak
	echo !include watclib.mak >>$(GSDIR)\_wm_temp.mak
	cd >$(GSDIR)\_wm_dir.bat
	cd $(GSDIR)
	echogs$(XE) -w _wm_cdir.bat cd -s -r _wm_dir.bat
	wmakel -u -n -h -f _wm_temp.mak CONFIG=$(CONFIG) gdevbbox.$(OBJ) gsargs.$(OBJ) gsnogc.$(OBJ) >_wm_temp.bat
	call _wm_temp.bat
	wmakel -u -n -h -f _wm_temp.mak CONFIG=$(CONFIG) ld$(CONFIG).tr gconfig$(CONFIG).$(OBJ) gscdefs$(CONFIG).$(OBJ) >_wm_temp.bat
	call _wm_temp.bat
	call _wm_cdir.bat
	rem	Use type rather than copy to update the creation time
	type $(GSDIR)\ld$(CONFIG).tr >ld$(LANGUAGE).tr

# Build the configuration file.
pconf$(CONFIG).h ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GSDIR)\genconf$(XE)
	$(GSDIR)\genconf -n - $(TARGET_DEVS) -h pconf$(CONFIG).h -p FILE&s&ps -ol ldconf$(CONFIG).tr

# Link a Watcom executable.
ldt.tr: $(MAKEFILE) ld$(LANGUAGE).tr ldconf$(CONFIG).tr
	echo SYSTEM DOS4G >ldt.tr
	echo OPTION STUB=$(STUB) >>ldt.tr
	echo OPTION STACK=16k >>ldt.tr
	echo PATH $(GSDIR) >>ldt.tr
	$(CP_) ldt.tr+ld$(LANGUAGE).tr
	echo FILE gdevbbox.$(OBJ) >>ldt.tr
	echo FILE gsargs.$(OBJ) >>ldt.tr
	echo FILE gsnogc.$(OBJ) >>ldt.tr
	echo FILE gconfig$(CONFIG).$(OBJ) >>ldt.tr
	echo FILE gscdefs$(CONFIG).$(OBJ) >>ldt.tr
	echo PATH . >>ldt.tr
	$(CP_) ldt.tr+ldconf$(CONFIG).tr

# Link the executable.
$(TARGET_XE)$(XE): ldt.tr $(LANGUAGE).$(OBJ)
	$(LINK) $(LCT) NAME $(TARGET_XE) OPTION MAP=$(TARGET_XE) FILE $(LANGUAGE).$(OBJ) @ldt.tr
