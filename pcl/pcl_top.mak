#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_top.mak
# Top-level platform-independent makefile for PCL5* et al

# This file must be preceded by pcl.mak.

default: $(TARGET_XE)$(XE)
	@echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pcl.clean-not-config-clean
	$(RM_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pcl.config-clean
	$(RMN_) *.tr $(GD)devs.tr $(GD)ld.tr
	$(RMN_) $(PCLGEN)pconf.h $(PCLGEN)pconfig.h

#### Implementation stub

$(PCLOBJ)pcimpl.$(OBJ): $(PCLSRC)pcimpl.c           \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PCLCCC) $(PCLSRC)pcimpl.c $(PCLO_)pcimpl.$(OBJ)
