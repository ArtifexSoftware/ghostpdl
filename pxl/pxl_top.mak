#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# pxl_top.mak
# Top-level platform-independent makefile for PCL XL

# This file must be preceded by pxl.mak.

#DEVICE_DEVS is defined in the platform-specific file.
DD='$(GLGENDIR)$(D)'
FEATURE_DEVS=$(DD)colimlib.dev $(DD)dps2lib.dev $(DD)path1lib.dev $(DD)patlib.dev $(DD)psl2cs.dev $(DD)rld.dev $(DD)roplib.dev $(DD)ttflib.dev  $(DD)cielib.dev

default: $(TARGET_XE)$(XE)
	echo Done.

clean: config-clean clean-not-config-clean

clean-not-config-clean: pl.clean-not-config-clean pxl.clean-not-config-clean
	$(RMN_) $(TARGET_XE)$(XE)

config-clean: pl.config-clean pxl.config-clean
	$(RMN_) *.tr $(GD)devs.tr$(CONFIG) $(GD)ld$(CONFIG).tr
	$(RMN_) $(PXLGEN)pconf$(CONFIG).h $(PXLGEN)pconfig.h
	$(RM_) $(PXLSRC)pxlver.h

#### Main program


PXLVERSION=1.20

$(PXLSRC)pxlver.h: $(PXLSRC)pxl_top.mak
	$(PXLGEN)echogs$(XE) -e .h -w $(PXLSRC)pxlver -n "#define PXLVERSION"
	$(PXLGEN)echogs$(XE) -e .h -a $(PXLSRC)pxlver -s -x 22 $(PXLVERSION) -x 22
	$(PXLGEN)echogs$(XE) -e .h -a $(PXLSRC)pxlver -n "#define PXLBUILDDATE"
	$(PXLGEN)echogs$(XE) -e .h -a $(PXLSRC)pxlver -s -x 22 -d -x 22

pxlver_h=$(PXLSRC)pxlver.h


# Note: we always compile the main program with -DDEBUG.
$(PXLOBJ)pxmain.$(OBJ): $(PXLSRC)pxmain.c $(AK)\
 $(malloc__h) $(math__h) $(memory__h) $(string__h)\
 $(gdebug_h) $(gp_h)\
 $(gsargs_h) $(gscdefs_h) $(gscoord_h) $(gsdevice_h) $(gserrors_h) $(gsgc_h)\
 $(gsio_h) $(gslib_h) $(gsmatrix_h) $(gsmemory_h) $(gspaint_h) $(gsparam_h)\
 $(gsstate_h) $(gsstruct_h) $(gstypes_h)\
 $(gxalloc_h) $(gxstate_h)\
 $(plmain_h) $(plparse_h) $(pjparse_h)\
 $(pxlver_h)\
 $(pxattr_h) $(pxerrors_h) $(pxparse_h) $(pxptable_h) $(pxstate_h) $(pxvalue_h)\
 $(PXLGEN)pconf$(CONFIG).h
	$(CP_) $(PXLGEN)pconf$(CONFIG).h $(PXLGEN)pconfig.h
	$(PXLCCC) $(PXLSRC)pxmain.c $(PXLO_)pxmain.$(OBJ)
