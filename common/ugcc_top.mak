#  Copyright (C) 1997-2007 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# ugcc_top.mak
# Generic top-level makefile for Unix/gcc platforms.
#
# The product-specific top-level makefile defines the following:
#	MAKEFILE, CCLD, COMMONDIR, CONFIG, DEVICE_DEVS,
#	GCFLAGS, GENDIR, GLSRCDIR, GLGENDIR, GLOBJDIR, PSD
#	TOP_OBJ, REALMAIN_OBJ, MAIN_OBJ, TARGET_DEVS, TARGET_XE
# It also must include the product-specific *.mak.

# Platform specification
include $(COMMONDIR)/gccdefs.mak
include $(COMMONDIR)/unixdefs.mak
include $(COMMONDIR)/generic.mak

# these gems were plucked from the old ugcclib.mak.  The CC options
# seem to be out of sync.

BINDIR=./libobj
GLD=$(GLGENDIR)/
CCFLAGS=$(GENOPT) $(CFLAGS)
CC_=$(CC) $(CCFLAGS)
CCAUX=$(CC)
CC_NO_WARN=$(CC_) -Wno-cast-qual -Wno-traditional
CC_SHARED=$(CC_)

# Which CMS are we using?
# Options are currently lcms or lcms2

WHICH_CMS=lcms

include $(GLSRCDIR)/unixhead.mak
include $(GLSRCDIR)/gs.mak
include $(GLSRCDIR)/lib.mak
include $(PSSRCDIR)/int.mak
include $(GLSRCDIR)/jpeg.mak
# zlib.mak must precede png.mak
include $(GLSRCDIR)/zlib.mak
include $(GLSRCDIR)/png.mak
include $(GLSRCDIR)/jbig2.mak
include $(GLSRCDIR)/icclib.mak
include $(GLSRCDIR)/$(WHICH_CMS).mak
include $(GLSRCDIR)/ijs.mak
include $(GLSRCDIR)/tiff.mak
include $(GLSRCDIR)/devs.mak
include $(GLSRCDIR)/contrib.mak
include $(GLSRCDIR)/unix-aux.mak
include $(GLSRCDIR)/unix-end.mak
include $(GLSRCDIR)/version.mak
include $(GLSRCDIR)/freetype.mak

UGCC_TOP_DIR:
	@if test ! -d $(GLGENDIR); then mkdir $(GLGENDIR); fi

# Configure for debugging
pdl-debug: UGCC_TOP_DIR
	$(MAKE) -f $(firstword $(MAKEFILE)) GENOPT='-DDEBUG' CFLAGS='-ggdb -g3 -O0 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)' pdl-default

pdl-pg-with-cov: UGCC_TOP_DIR
	$(MAKE) -f $(firstword $(MAKEFILE)) GENDIR=$(PGGENDIR) GENOPT='' CFLAGS='-g -pg -O2 -fprofile-arcs -ftest-coverage $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg -fprofile-arcs -ftest-coverage' pdl-default

# Configure for profiling
pdl-pg: UGCC_TOP_DIR
	$(MAKE) -f $(firstword $(MAKEFILE)) GENDIR=$(PGGENDIR) GENOPT='' CFLAGS='-g -pg -O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' pdl-default

# Configure for optimization.
pdl-product: UGCC_TOP_DIR
	$(MAKE) -f $(firstword $(MAKEFILE)) GENOPT='' GCFLAGS='$(GCFLAGS)' CFLAGS='-O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)' pdl-default

pdl-product-lib: UGCC_TOP_DIR
	$(MAKE) -f $(firstword $(MAKEFILE)) GENOPT='' GCFLAGS='$(GCFLAGS)' CFLAGS='-O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)' lib

pdl-clean:
	$(RMN_) $(GENDIR)/*.dev $(GENDIR)/devs*.tr $(GENDIR)/gconfig*.h
	$(RMN_) $(GENDIR)/gconfx*.h $(GENDIR)/j*.h
	$(RMN_) $(GENDIR)/c*.tr $(GENDIR)/o*.tr $(GENDIR)/l*.tr
	$(RMN_) $(GENDIR)/*.$(OBJ)
	$(RMN_) $(GENDIR)/*.h
	$(RMN_) $(GENDIR)/*.c
	$(RMN_) $(GENDIR)/*.a
	$(RMN_) $(GENDIR)/mkromfs
	$(RMN_) $(GENARCH_XE)
	$(RMN_) $(ECHOGS_XE)
	$(RMN_) $(GENCONF_XE)
	$(RMN_) $(TARGET_XE)$(XE)


# Build the configuration file.
$(GENDIR)/pconf.h $(GENDIR)/ldconf.tr: $(TARGET_DEVS) $(GLOBJDIR)/genconf$(XE)
	$(GLOBJDIR)/genconf -n - $(TARGET_DEVS) -h $(GENDIR)/pconf.h -p "%s&s&&" -o $(GENDIR)/ldconf.tr

# Create a library
$(TARGET_LIB): $(ld_tr) $(GENDIR)/ldconf.tr $(MAIN_OBJ) $(TOP_OBJ) $(XOBJS) $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(AR) $(ARFLAGS)  $@
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ) $(XOBJS) -s
	cat $(GENDIR)/ldt.tr $(GENDIR)/ldconf.tr | grep ".o" >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ) $(MAIN_OBJ)
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; sh <$(GENDIR)/ldall.tr

ifeq ($(PSICFLAGS), -DPSI_INCLUDED)
# Link a Unix executable.  NB - XOBS is not concatenated to the link
# list here.  It seems to have been done earlier on unlike the
# standalone pcl build below.
$(TARGET_XE): $(ld_tr) $(GENDIR)/ldconf.tr $(REALMAIN_OBJ) $(MAIN_OBJ) $(TOP_OBJ) $(XOBJS) $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ) -s
	cat $(ld_tr) $(GENDIR)/ldconf.tr >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ) $(REALMAIN_OBJ) $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	sh <$(GENDIR)/ldall.tr
else
# Link a Unix executable.
$(TARGET_XE): $(ld_tr) $(GENDIR)/ldconf.tr $(REALMAIN_OBJ) $(MAIN_OBJ) $(TOP_OBJ) $(XOBJS) $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ) $(XOBJS) -s
	cat $(ld_tr) $(GENDIR)/ldconf.tr >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(GLOBJDIR)/gsromfs$(COMPILE_INITS).$(OBJ) $(REALMAIN_OBJ) $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	sh <$(GENDIR)/ldall.tr
endif
