# Copyright (C) 2001-2019 Artifex Software, Inc.
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
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
#
# Partial makefile common to all Unix configurations.
# This part of the makefile contains the linking steps.

# Define the name of this makefile.
UNIXLINK_MAK=$(GLSRC)unixlink.mak $(TOP_MAKEFILES)

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

GS_DOT_O=$(PSOBJ)gs.$(OBJ)

# Build a library archive for the entire interpreter.
# This is not used in a standard build.

# options for genconf, to *just* output the libs to link
# and search paths for them
CONFLIBSTR=-pl "&-l%s&s&&" -pL "&-L%s&s&&" -l

libgs_a_tr=$(GLOBJ)libgs_a.tr
GS_A=$(BINDIR)$(D)$(GS).a
$(GS_A): $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) \
         $(obj_tr) $(ECHOGS_XE) $(INT_ARCHIVE_ALL) $(INT_ALL) $(DEVS_ALL) \
         $(UNIXLINK_MAK)
	rm -f $(GS_A)
	$(ECHOGS_XE) -w $(libgs_a_tr) -n - $(AR) $(ARFLAGS) $(GS_A)
	$(ECHOGS_XE) -a $(libgs_a_tr) -n -s $(INT_ARCHIVE_ALL) -s
	$(ECHOGS_XE) -a $(libgs_a_tr) -n -s $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) -s
	cat $(obj_tr) >>$(libgs_a_tr)
	$(ECHOGS_XE) -a $(libgs_a_tr) -s -
	$(SH) <$(libgs_a_tr)
	$(RANLIB) $(GS_A)

GS_A_XE=$(BINDIR)$(D)$(GS)_aexe$(XE)
gs_a_xeld_tr=$(GLOBJ)$(D)$(GS)_aexeld.tr
gs_a_xeldt_tr=$(GLOBJ)$(D)$(GS)_aexeldt.tr
$(GS_A_XE): $(GS_A) $(GS_DOT_O)
	$(EXP)$(GENCONF_XE) $(gs_tr) -h $(GLGENDIR)$(D)unused.h $(CONFLIBSTR) $(gs_a_xeld_tr)
	$(ECHOGS_XE) -w $(gs_a_xeldt_tr) -n - $(CCLD) $(GS_LDFLAGS) -o $(GS_A_XE)
	$(ECHOGS_XE) -a $(gs_a_xeldt_tr) -n -s $(GS_DOT_O) -s
	$(ECHOGS_XE) -a $(gs_a_xeldt_tr) -n -s - $(GS_A) $(EXTRALIBS) $(STDLIBS) -s
	$(ECHOGS_XE) -a $(gs_a_xeldt_tr) -n -s -R $(gs_a_xeld_tr)
	$(SH) < $(gs_a_xeldt_tr)

libgpcl6_a_tr=$(GLOBJ)libgpcl6_a.tr
GPCL_A=$(BINDIR)$(D)$(PCL).a
$(GPCL_A): $(MAIN_OBJ) $(TOP_OBJ) $(XOBJS) \
           $(GLOBJDIR)/pclromfs$(COMPILE_INITS).$(OBJ) $(PCL_DEVS_ALL) \
           $(INT_ARCHIVE_SOME) $(pclobj_tr) $(ECHOGS_XE) $(DEVS_ALL) $(UNIXLINK_MAK)
	rm -f $(GPCL_A)
	$(ECHOGS_XE) -w $(libgpcl6_a_tr) -n - $(AR) $(ARFLAGS) $(GPCL_A)
	$(ECHOGS_XE) -a $(libgpcl6_a_tr) -n -s $(TOP_OBJ) $(INT_ARCHIVE_SOME) $(XOBJS)  -s
	$(ECHOGS_XE) -a $(libgpcl6_a_tr) -n -s $(PSOBJ)pclromfs$(COMPILE_INITS).$(OBJ) $(MAIN_OBJ) -s
	cat $(pclobj_tr) >>$(libgpcl6_a_tr)
	$(ECHOGS_XE) -a $(libgpcl6_a_tr) -s -
	$(SH) <$(libgpcl6_a_tr)
	$(RANLIB) $(GPCL_A)

GPCL_A_XE=$(BINDIR)$(D)$(PCL)_aexe$(XE)
gpcl_a_xeld_tr=$(GLOBJ)$(D)$(PCL)_aexeld.tr
gpcl_a_xeldt_tr=$(GLOBJ)$(D)$(PCL)_aexeldt.tr
$(GPCL_A_XE): $(GPCL_A) $(REALMAIN_OBJ)
	$(EXP)$(GENCONF_XE) $(pcl_tr) -h $(GLGENDIR)$(D)unused.h $(CONFLIBSTR) $(gpcl_a_xeld_tr)
	$(ECHOGS_XE) -w $(gpcl_a_xeldt_tr) -n - $(CCLD) $(GS_LDFLAGS) -o $(GPCL_A_XE)
	$(ECHOGS_XE) -a $(gpcl_a_xeldt_tr) -n -s $(REALMAIN_OBJ) -s
	$(ECHOGS_XE) -a $(gpcl_a_xeldt_tr) -n -s - $(GPCL_A) $(EXTRALIBS) $(STDLIBS) -s
	$(ECHOGS_XE) -a $(gpcl_a_xeldt_tr) -n -s -R $(gpcl_a_xeld_tr)
	$(SH) < $(gpcl_a_xeldt_tr)

libgxps_a_tr=$(GLOBJ)libgxps_a.tr
GXPS_A=$(BINDIR)$(D)$(XPS).a
$(GXPS_A): $(MAIN_OBJ) $(XPS_TOP_OBJS) $(XOBJS) \
           $(GLOBJDIR)/xpsromfs$(COMPILE_INITS).$(OBJ)  $(XPS_DEVS_ALL) \
           $(INT_ARCHIVE_SOME) $(xpsobj_tr) $(ECHOGS_XE) $(DEVS_ALL) $(UNIXLINK_MAK)
	rm -f $(GXPS_A)
	$(ECHOGS_XE) -w $(libgxps_a_tr) -n - $(AR) $(ARFLAGS) $(GXPS_A)
	$(ECHOGS_XE) -a $(libgxps_a_tr) -n -s $(XPS_TOP_OBJS) $(INT_ARCHIVE_SOME) $(XOBJS) -s
	$(ECHOGS_XE) -a $(libgxps_a_tr) -n -s $(PSOBJ)xpsromfs$(COMPILE_INITS).$(OBJ) $(MAIN_OBJ) -s
	cat $(xpsobj_tr) >> $(libgxps_a_tr)
	$(ECHOGS_XE) -a $(libgxps_a_tr) -s -
	$(SH) <$(libgxps_a_tr)
	$(RANLIB) $(GXPS_A)

GXPS_A_XE=$(BINDIR)$(D)$(XPS)_aexe$(XE)
gxps_a_xeld_tr=$(GLOBJ)$(D)$(XPS)_aexeld.tr
gxps_a_xeldt_tr=$(GLOBJ)$(D)$(XPS)_aexeldt.tr
$(GXPS_A_XE): $(GXPS_A) $(REALMAIN_OBJ)
	$(EXP)$(GENCONF_XE) $(xps_tr) -h $(GLGENDIR)$(D)unused.h $(CONFLIBSTR) $(gxps_a_xeld_tr)
	$(ECHOGS_XE) -w $(gxps_a_xeldt_tr) -n - $(CCLD) $(GS_LDFLAGS) -o $(GXPS_A_XE)
	$(ECHOGS_XE) -a $(gxps_a_xeldt_tr) -n -s $(REALMAIN_OBJ) -s
	$(ECHOGS_XE) -a $(gxps_a_xeldt_tr) -n -s - $(GXPS_A) $(EXTRALIBS) $(STDLIBS) -s
	$(ECHOGS_XE) -a $(gxps_a_xeldt_tr) -n -s -R $(gxps_a_xeld_tr)
	$(SH) < $(gxps_a_xeldt_tr)

libgpdl_tr=$(GLOBJ)libgpdl.tr
GPDL_A=$(BINDIR)$(D)$(GPDL).a
$(GPDL_A): $(GPDL_PSI_TOP_OBJS) $(PCL_PXL_TOP_OBJS) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) $(MAIN_OBJ) \
         $(XOBJS) $(GLOBJDIR)/pdlromfs$(COMPILE_INITS).$(OBJ) \
         $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c0.$(OBJ) \
         $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c1.$(OBJ) \
         $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c2.$(OBJ) \
         $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c3.$(OBJ) \
         $(PSINT_ARCHIVE_ALL) \
         $(pdlobj_tr) $(ECHOGS_XE) $(INT_ARCHIVE_ALL) $(INT_ALL) $(DEVS_ALL) \
         $(UNIXLINK_MAK)
	rm -f $(GPDL_A)
	$(ECHOGS_XE) -w $(libgpdl_tr) -n - $(AR) $(ARFLAGS) $(GPDL_A)
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GPDL_PSI_TOP_OBJS) $(PCL_PXL_TOP_OBJS) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) $(XOBJS) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GLOBJDIR)/pdlromfs$(COMPILE_INITS).$(OBJ) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c0.$(OBJ) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c1.$(OBJ) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c2.$(OBJ) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c3.$(OBJ) -s
	$(ECHOGS_XE) -a $(libgpdl_tr) -n -s $(MAIN_OBJ) -s
	cat $(pdlobj_tr) >>$(libgpdl_tr)
	$(ECHOGS_XE) -a $(libgpdl_tr) -s -
	$(SH) <$(libgpdl_tr)
	$(RANLIB) $(GPDL_A)

GPDL_A_XE=$(BINDIR)$(D)$(GPDL)_aexe$(XE)
gpdl_a_xeld_tr=$(GLOBJ)$(D)$(GPDL)_aexeld.tr
gpdl_a_xeldt_tr=$(GLOBJ)$(D)$(GPDL)_aexeldt.tr
$(GPDL_A_XE): $(GPDL_A) $(REALMAIN_OBJ)
	$(EXP)$(GENCONF_XE) $(gpdl_tr) -h $(GLGENDIR)$(D)unused.h $(CONFLIBSTR) $(gpdl_a_xeld_tr)
	$(ECHOGS_XE) -w $(gpdl_a_xeldt_tr) -n - $(CCLD) $(GS_LDFLAGS) -o $(GPDL_A_XE)
	$(ECHOGS_XE) -a $(gpdl_a_xeldt_tr) -n -s $(REALMAIN_OBJ) -s
	$(ECHOGS_XE) -a $(gpdl_a_xeldt_tr) -n -s - $(GPDL_A) $(EXTRALIBS) $(STDLIBS) -s
	$(ECHOGS_XE) -a $(gpdl_a_xeldt_tr) -n -s -R $(gpdl_a_xeld_tr)
	$(SH) < $(gpdl_a_xeldt_tr)


# Here is the final link step.  The stuff with LD_RUN_PATH is for SVR4
# systems with dynamic library loading; I believe it's harmless elsewhere.
# The resetting of the environment variables to empty strings is for SCO Unix,
# which has limited environment space.
ldt_tr=$(PSOBJ)ldt.tr

$(GS_XE): $(ld_tr) $(gs_tr) $(ECHOGS_XE) $(XE_ALL) $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) \
          $(UNIXLINK_MAK)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(GS_LDFLAGS) -o $(GS_XE)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) $(GS_DOT_O) -s
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
            $(INT_ARCHIVE_SOME) $(UNIXLINK_MAK)
	$(ECHOGS_XE) -w $(pclldt_tr) -n - $(CCLD) $(PCL_LDFLAGS) $(XLIBDIRS) -o $(GPCL_XE)
	$(ECHOGS_XE) -a $(pclldt_tr) -n -x 20
	$(ECHOGS_XE) -a $(pclldt_tr) -n -s $(TOP_OBJ) $(INT_ARCHIVE_SOME) $(XOBJS) -s
	cat $(pclld_tr) >> $(pclldt_tr)
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
             $(INT_ARCHIVE_SOME) $(UNIXLINK_MAK)
	$(ECHOGS_XE) -w $(xpsldt_tr) -n - $(CCLD) $(XPS_LDFLAGS) $(XLIBDIRS) -o $(GXPS_XE)
	$(ECHOGS_XE) -a $(xpsldt_tr) -n -s $(XPS_TOP_OBJS) $(INT_ARCHIVE_SOME) $(XOBJS) -s
	cat $(xpsld_tr) >> $(xpsldt_tr)
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
	     $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c0.$(OBJ) \
	     $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c1.$(OBJ) \
	     $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c2.$(OBJ) \
	     $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c3.$(OBJ) \
             $(PSINT_ARCHIVE_ALL) $(UNIXLINK_MAK)
	$(ECHOGS_XE) -w $(gpdlldt_tr) -n - $(CCLD) $(PDL_LDFLAGS) $(XLIBDIRS) -o $(GPDL_XE)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s $(GPDL_PSI_TOP_OBJS) $(PCL_PXL_TOP_OBJS) $(PSI_TOP_OBJ) $(XPS_TOP_OBJ) $(XOBJS) -s
	cat $(gpdlld_tr) >> $(gpdlldt_tr)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS).$(OBJ)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c0.$(OBJ)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c1.$(OBJ)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c2.$(OBJ)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -n -s - $(GLOBJDIR)/pdlromfs$(COMPILE_INITS)c3.$(OBJ)
	$(ECHOGS_XE) -a $(gpdlldt_tr) -s - $(REALMAIN_OBJ) $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
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

$(APITEST_XE): $(ld_tr) $(ECHOGS_XE) $(XE_ALL) $(PSOBJ)gsromfs$(COMPILE_INITS).$(OBJ) $(PSOBJ)apitest.$(OBJ) \
               $(UNIXLINK_MAK)
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
