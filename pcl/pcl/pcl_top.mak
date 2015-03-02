# Copyright (C) 2001-2012 Artifex Software, Inc.
# All Rights Reserved.
#
# This software is provided AS-IS with no warranty, either express or
# implied.
#
# This software is distributed under license and may not be copied,
# modified or distributed except as expressly authorized under the terms
# of the license contained in the file LICENSE in this distribution.
#
# Refer to licensing information at http://www.artifex.com or contact
# Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
# CA  94903, U.S.A., +1(415)492-9861, for further information.
#

# pcl_top.mak
# Top-level platform-independent makefile for PCL5* et al

# This file must be preceded by pcl.mak.

#default: $(TARGET_XE)$(XE)
#	@echo Done.

#clean: config-clean clean-not-config-clean

#clean-not-config-clean: pl.clean-not-config-clean pcl.clean-not-config-clean
#	$(RM_) $(TARGET_XE)$(XE)

#config-clean: pl.config-clean pcl.config-clean
#	$(RMN_) *.tr $(GD)devs.tr $(GD)ld.tr
#	$(RMN_) $(PCLGEN)pconf.h $(PCLGEN)pconfig.h

#### Implementation stub

$(PCLOBJ)pcimpl.$(OBJ): $(PCLSRC)pcimpl.c           \
                        $(AK)                       \
                        $(memory__h)                \
                        $(scommon_h)                \
                        $(gxdevice_h)               \
                        $(pltop_h)
	$(PCLCCC) $(PCLSRC)pcimpl.c $(PCLO_)pcimpl.$(OBJ)
