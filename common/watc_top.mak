#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# watc_top.mak
# Generic top-level makefile for MS-DOS/Watcom platforms.

# The product-specific top-level makefile defines the following:
#	MAKEFILE, COMMONDIR, CONFIG, DEBUG, DEVICE_DEVS, GLSRCDIR, MAIN_OBJ,
#	NOPRIVATE, TDEBUG, TARGET_DEVS, TARGET_XE, WCVERSION
# It also must include the product-specific *.mak.

# XE isn't defined yet.
default: $(TARGET_XE).exe

# Define the ANSI-to-K&R dependency.  Borland C, Microsoft C and
# Watcom C all accept ANSI syntax, but we need to preconstruct ccf32.tr 
# to get around the limit on the maximum length of a command line.

AK=$(GLGENDIR)\ccf32.tr

# clean gs files using the watc.mak file in the gs directory
clean_gs:
	wmake -u -f $(GLSRCDIR)\watc.mak \
	GLSRCDIR=$(GLSRCDIR) GLGENDIR=$(GLGENDIR) \
	GLOBJDIR=$(GLOBJDIR) clean

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

!include $(COMMONDIR)\watcdefs.mak
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\generic.mak
!include $(GLSRCDIR)\wccommon.mak
!include $(GLSRCDIR)\version.mak

# Build the required files in the GS directory.
$(GLGENDIR)\ld$(CONFIG).tr: $(MAKEFILE) $(ECHOGS_XE)
	echo WCVERSION=$(WCVERSION) >$(GLGENDIR)\_wm_temp.mak
	echo GLSRCDIR=$(GLSRCDIR) >>$(GENDIR)\_wm_temp.mak
	echo GLGENDIR=$(GLGENDIR) >>$(GENDIR)\_wm_temp.mak
	echo GLOBJDIR=$(GLOBJDIR) >>$(GENDIR)\_wm_temp.mak
	echo PSRCDIR=$(PSRCDIR) >>$(GENDIR)\_wm_temp.mak
	echo PVERSION=$(PVERSION) >>$(GENDIR)\_wm_temp.mak
	echo JSRCDIR=$(JSRCDIR) >>$(GENDIR)\_wm_temp.mak
	echo JVERSION=$(JVERSION) >>$(GENDIR)\_wm_temp.mak
	echo ZSRCDIR=$(ZSRCDIR) >>$(GENDIR)\_wm_temp.mak
	echo FEATURE_DEVS=$(FEATURE_DEVS) >>$(GLGENDIR)\_wm_temp.mak
	echo DEVICE_DEVS=$(DEVICE_DEVS) $(DD)bbox.dev >>$(GLGENDIR)\_wm_temp.mak
	echo FPU_TYPE=$(FPU_TYPE) >>$(GENDIR)\_wm_temp.mak
	echo CPU_TYPE=$(CPU_TYPE) >>$(GENDIR)\_wm_temp.mak
	echo BAND_LIST_STORAGE=memory >>$(GLGENDIR)\_wm_temp.mak
	echo BAND_LIST_COMPRESSOR=zlib >>$(GLGENDIR)\_wm_temp.mak
	echo !include $(GLSRCDIR)\watclib.mak >>$(GLGENDIR)\_wm_temp.mak
	$(MAKE) -u -n -h -f $(GLGENDIR)\_wm_temp.mak CONFIG=$(CONFIG) $(GLOBJDIR)\gsargs.$(OBJ) $(GLOBJDIR)\gsnogc.$(OBJ) $(GLGENDIR)\arch.h >$(GLGENDIR)\_wm_temp.bat
	call $(GLGENDIR)\_wm_temp.bat
	$(MAKE) -u -n -h -f $(GLGENDIR)\_wm_temp.mak CONFIG=$(CONFIG) $(GLGENDIR)\ld$(CONFIG).tr $(GLOBJDIR)\gconfig$(CONFIG).$(OBJ) $(GLOBJDIR)\gscdefs$(CONFIG).$(OBJ) >$(GLGENDIR)\_wm_temp.bat
	call $(GLGENDIR)\_wm_temp.bat

# Build the configuration file.
$(GLGENDIR)\pconf$(CONFIG).h $(GLGENDIR)\ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GLOBJDIR)\genconf$(XE)
	$(GLOBJDIR)\genconf -n - $(TARGET_DEVS) -h $(GLGENDIR)\pconf$(CONFIG).h -e ~ -p FILE~s~ps -ol $(GLGENDIR)\ldconf$(CONFIG).tr

# Link a Watcom executable.
$(GLGENDIR)\ldt$(CONFIG).tr: $(MAKEFILE) $(GLGENDIR)\ld$(CONFIG).tr $(GLGENDIR)\ldconf$(CONFIG).tr
	echo OPTION STACK=32k >$(GLGENDIR)\ldt$(CONFIG).tr
!ifeq WAT32 0
	echo SYSTEM DOS4G >>$(GLGENDIR)\ldt$(CONFIG).tr
	echo OPTION STUB=$(STUB) >>$(GLGENDIR)\ldt$(CONFIG).tr
!endif
	type $(GLGENDIR)\ld$(CONFIG).tr >>$(GLGENDIR)\ldt$(CONFIG).tr
	echo FILE $(GLOBJDIR)\gsargs.$(OBJ) >>$(GLGENDIR)\ldt$(CONFIG).tr
	echo FILE $(GLOBJDIR)\gsnogc.$(OBJ) >>$(GLGENDIR)\ldt$(CONFIG).tr
	echo FILE $(GLOBJDIR)\gconfig$(CONFIG).$(OBJ) >>$(GLGENDIR)\ldt$(CONFIG).tr
	echo FILE $(GLOBJDIR)\gscdefs$(CONFIG).$(OBJ) >>$(GLGENDIR)\ldt$(CONFIG).tr
	type $(GLGENDIR)\ldconf$(CONFIG).tr >>$(GLGENDIR)\ldt$(CONFIG).tr

# Link the executable.
$(TARGET_XE)$(XE): $(GLGENDIR)\ldt$(CONFIG).tr $(MAIN_OBJ)
	$(LINK) $(LCT) NAME $(TARGET_XE) OPTION MAP=$(TARGET_XE) FILE $(MAIN_OBJ) @$(GLGENDIR)\ldt$(CONFIG).tr
