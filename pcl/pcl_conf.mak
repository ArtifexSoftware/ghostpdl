#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pcl_conf.mak
# Top-level makefile configuration for PCL5* -- contains only macro def'ns
# This is broken out (instead of being lumped into pcl_top.mak) for the
# benefit of make programs (e.g. MS nmake) which define macros "eagerly."
# This enables definition of platform-independent config info before use.

# Language and configuration.  These are actually platform-independent,
# but we define them here just to keep all parameters in one place.
CONFIG=5
TARGET_DEVS=$(PCLOBJDIR)\pcl5c.dev $(PCLOBJDIR)\hpgl2c.dev

# Main file's name
MAIN_OBJ=$(PCLOBJDIR)\pcmain.$(OBJ)

# GS options
#DEVICE_DEVS is defined in the platform-specific file.
FEATURE_DEVS=dps2lib.dev path1lib.dev patlib.dev rld.dev roplib.dev ttflib.dev
