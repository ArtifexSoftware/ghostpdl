#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_top.mak
# Top-level platform-independent makefile for PCL5* et al

# This file must be preceded by pcl.mak.

#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS    = $(DD)dps2lib.dev   \
                  $(DD)path1lib.dev  \
                  $(DD)patlib.dev    \
                  $(DD)rld.dev       \
                  $(DD)roplib.dev    \
                  $(DD)ttflib.dev    \
                  $(DD)colimlib.dev  \
                  $(DD)cielib.dev    \
                  $(DD)htxlib.dev    \
	          $(DD)pipe.dev      \
                  $(DD)devcmap.dev

default: $(TARGET_XE)$(XE)
	@echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pcl.clean-not-config-clean
	$(RM_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pcl.config-clean
	$(RMN_) *.tr $(GD)devs.tr $(GD)ld.tr
	$(RMN_) $(PCLGEN)pconf.h $(PCLGEN)pconfig.h
	$(RM_) $(PCLSRC)pclver.h

#### Implementation stub

$(PCLOBJ)pcimpl.$(OBJ): $(PCLSRC)pcimpl.c           \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PCLCCC) $(PCLSRC)pcimpl.c $(PCLO_)pcimpl.$(OBJ)
