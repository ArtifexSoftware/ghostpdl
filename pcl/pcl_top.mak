#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_top.mak
# Top-level platform-independent makefile for PCL5* et al

# This file must be preceded by pcl.mak.

#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS=dps2lib.dev path1lib.dev patlib.dev rld.dev roplib.dev ttflib.dev

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pcl.clean-not-config-clean
	$(RM_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pcl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld$(CONFIG).tr
	$(RMN_) $(PCLGEN)pconf$(CONFIG).h $(PCLGEN)pconfig.h

#### Main program

# Note: we always compile the main program with -DDEBUG.
$(PCLOBJ)pcmain.$(OBJ): $(PCLSRC)pcmain.c $(AK) $(malloc__h) $(math__h) $(memory__h) $(stdio__h)\
 $(gdebug_h) $(gp_h)\
 $(gdevbbox_h)\
 $(gscdefs_h) $(gscoord_h) $(gsgc_h) $(gslib_h) $(gsmatrix_h)\
 $(gspaint_h) $(gspath_h) $(gsstate_h)\
 $(gxalloc_h) $(gxdevice_h)\
 $(scommon_h)\
 $(plmain_h)\
 $(pjparse_h)\
 $(pcparse_h) $(pcstate_h) $(PCLGEN)pconf$(CONFIG).h
	$(CP_) $(PCLGEN)pconf$(CONFIG).h $(PCLGEN)pconfig.h
	$(PCLCCC) $(PCLSRC)pcmain.c $(PCLO_)pcmain.$(OBJ)
