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
# Partial makefile common to all Unix configurations.
# This part of the makefile contains the linking steps.

# Define the name of this makefile.
UNIXLINK_MAK=$(GLSRC)unixlink.mak

# The following prevents GNU make from constructing argument lists that
# include all environment variables, which can easily be longer than
# brain-damaged system V allows.

.NOEXPORT:

# ----------------------------- Main program ------------------------------ #

### Interpreter main program

INT_ARCHIVE_SOME=$(GLOBJ)gconfig.$(OBJ) $(GLOBJ)gscdefs.$(OBJ)

PSINT_ARCHIVE_ALL=$(PSOBJ)imainarg.$(OBJ) $(PSOBJ)imain.$(OBJ) $(GLOBJ)iconfig.$(OBJ)

INT_ARCHIVE_ALL=$(PSINT_ARCHIVE_ALL) $(INT_ARCHIVE_SOME)

XE_ALL=$(PSOBJ)gs.$(OBJ) $(INT_ARCHIVE_ALL) $(INT_ALL) $(DEVS_ALL)

# Build a library archive for the entire interpreter.
# This is not used in a standard build.
liar_tr=$(GLOBJ)liar.tr
GS_A=$(GS).a
$(GS_A): $(obj_tr) $(ECHOGS_XE) $(INT_ARCHIVE_ALL) $(INT_ALL) $(DEVS_ALL)
	rm -f $(GS_A)
	$(ECHOGS_XE) -w $(liar_tr) -n - $(AR) $(ARFLAGS) $(GS_A)
	$(ECHOGS_XE) -a $(liar_tr) -n -s $(INT_ARCHIVE_ALL) -s
	cat $(obj_tr) >>$(liar_tr)
	$(ECHOGS_XE) -a $(liar_tr) -s -
	$(SH) <$(liar_tr)
	$(RANLIB) $(GS_A)

# Here is the final link step.  The stuff with LD_RUN_PATH is for SVR4
# systems with dynamic library loading; I believe it's harmless elsewhere.
# The resetting of the environment variables to empty strings is for SCO Unix,
# which has limited environment space.
ldt_tr=$(PSOBJ)ldt.tr

$(GS_XE): $(ld_tr) $(gs_tr) $(ECHOGS_XE) $(XE_ALL) $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(LDFLAGS) -o $(GS_XE)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) $(PSOBJ)gs.$(OBJ) -s
	cat $(gsld_tr) >> $(ldt_tr)
	$(ECHOGS_XE) -a $(ldt_tr) -s - $(EXTRALIBS) $(STDLIBS)
	if [ x$(XLIBDIR) != x ]; then LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; fi; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	PSI_FEATURE_DEVS= FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	DEVICE_DEVS_EXTRA= \
	$(SH) <$(ldt_tr)

.gssubtarget: $(GS_XE)
	$(NO_OP)


pclldt_tr=$(PSOBJ)pclldt.tr
$(GPCL_XE): $(ld_tr) $(pcl_tr) $(REALMAIN_OBJ) $(MAIN_OBJ) $(TOP_OBJ) $(XOBJS) \
            $(GLOBJDIR)/pclromfs$(COMPILE_INITS).$(OBJ) \
            $(INT_ARCHIVE_SOME)
	$(ECHOGS_XE) -w $(pclldt_tr) -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GPCL_XE)
	$(ECHOGS_XE) -a $(pclldt_tr) -n -x 20
	cat $(ld_tr) >> $(pclldt_tr)
	$(ECHOGS_XE) -a $(pclldt_tr) -n -s $(TOP_OBJ) $(INT_ARCHIVE_SOME) $(XOBJS) -s
	cat $(pcl_tr) >> $(pclldt_tr)
	$(ECHOGS_XE) -a $(pclldt_tr) -n -s - $(GLOBJDIR)/pclromfs$(COMPILE_INITS).$(OBJ) $(REALMAIN_OBJ) $(MAIN_OBJ)
	$(ECHOGS_XE) -a $(pclldt_tr) -s - $(EXTRALIBS) $(STDLIBS)
	if [ x$(XLIBDIR) != x ]; then LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; fi; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	PCL_FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	DEVICE_DEVS_XETRA= \
	sh <$(pclldt_tr)

.pcl6subtarget: $(GPCL_XE)
	$(NO_OP)

xpsldt_tr=$(PSOBJ)xpsldt.tr
$(GXPS_XE): $(ld_tr) $(xps_tr) $(REALMAIN_OBJ) $(MAIN_OBJ) $(XPS_TOP_OBJS) \
             $(XOBJS) $(GLOBJDIR)/xpsromfs$(COMPILE_INITS).$(OBJ) \
             $(INT_ARCHIVE_SOME)
	$(ECHOGS_XE) -w $(xpsldt_tr) -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GXPS_XE)
	$(ECHOGS_XE) -a $(xpsldt_tr) -n -s $(XPS_TOP_OBJS) $(INT_ARCHIVE_SOME) $(XOBJS) -s
	cat $(ld_tr) >> $(xpsldt_tr)
	cat $(xps_tr) >> $(xpsldt_tr)
	$(ECHOGS_XE) -a $(xpsldt_tr) -s - $(GLOBJDIR)/xpsromfs$(COMPILE_INITS).$(OBJ) $(REALMAIN_OBJ) $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	if [ x$(XLIBDIR) != x ]; then LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; fi; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	PCL_FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	DEVICE_DEVS_EXTRA= \
	sh <$(xpsldt_tr)

.xpssubtarget: $(GXPS_XE)
	$(NO_OP)

gpdlldt_tr=$(PSOBJ)gpdlldt.tr
$(GPDL_XE): $(ld_tr) $(gpdl_tr) $(INT_ARCHIVE_ALL) $(REALMAIN_OBJ) $(MAIN_OBJ) \
             $(GPDL_PSI_TOP_OBJS) $(PCL_PXL_TOP_OBJS) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) \
             $(XOBJS) $(GLOBJDIR)/pdlromfs$(COMPILE_INITS).$(OBJ) \
             $(PSINT_ARCHIVE_ALL)
	$(ECHOGS_XE) -w $(gpdlldt_tr) -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GPDL_XE)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s $(GPDL_PSI_TOP_OBJS) $(PCL_PXL_TOP_OBJS) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) $(XOBJS) -s
	cat $(gpdlld_tr) >> $(gpdlldt_tr)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS).$(OBJ) $(REALMAIN_OBJ) $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	if [ x$(XLIBDIR) != x ]; then LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; fi; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	PCL_FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	DEVICE_DEVS_EXTRA= \
	sh <$(gpdlldt_tr)

.gpdlsubtarget: $(GPDL_XE)
	$(NO_OP)


APITEST_XE=$(BINDIR)$(D)apitest$(XE)

apitest: $(APITEST_XE)

$(APITEST_XE): $(ld_tr) $(ECHOGS_XE) $(XE_ALL) $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) $(PSOBJ)apitest.$(OBJ)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(LDFLAGS) -o $(APITEST_XE)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) $(PSOBJ)apitest.$(OBJ) -s
	cat $(ld_tr) >>$(ldt_tr)
	$(ECHOGS_XE) -a $(ldt_tr) -s - $(EXTRALIBS) $(STDLIBS)
	if [ x$(XLIBDIR) != x ]; then LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; fi; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	DEVICE_DEVS_EXTRA= \
	$(SH) <$(ldt_tr)
