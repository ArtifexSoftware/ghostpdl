#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pxl_top.mak
# Top-level platform-independent makefile for PCL XL

# This file must be preceded by pxl.mak.

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld.tr
	$(RMN_) $(PXLGEN)pconf.h $(PXLGEN)pconfig.h
	$(RM_) $(PXLSRC)pxlver.h

#### Implementation stub
# Note: we always compile the main program with -DDEBUG.
$(PXLOBJ)pximpl.$(OBJ): $(PXLSRC)pximpl.c $(AK)\
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PXLCCC) $(PXLSRC)pximpl.c $(PXLO_)pximpl.$(OBJ)
