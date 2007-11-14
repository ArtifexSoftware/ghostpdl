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

# The product-specific top-level makefile defines the following:
#	MAKEFILE, CCLD, COMMONDIR, CONFIG, DEVICE_DEVS,
#	GCFLAGS, GENDIR, GLSRCDIR, GLGENDIR, GLOBJDIR, PSD
#	TOP_OBJ, MAIN_OBJ, TARGET_DEVS, TARGET_XE
# It also must include the product-specific *.mak.

default: $(TARGET_XE)

# Platform specification
include $(COMMONDIR)/gccdefs.mak
include $(COMMONDIR)/unixdefs.mak
include $(COMMONDIR)/generic.mak

# HACK - allow pcl and xl to see gs header definitions

include $(GLSRCDIR)/lib.mak

# Configure for debugging
debug:
	$(MAKE) -f $(firstword $(MAKEFILE)) GENOPT='-DDEBUG' CFLAGS='-ggdb -g3 -O0 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)'

pg-fp-with-cov:
	$(MAKE) -f $(firstword $(MAKEFILE)) GENDIR=$(PGGENDIR) GENOPT='' CFLAGS='-g -pg -O2 -fprofile-arcs -ftest-coverage $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg -fprofile-arcs -ftest-coverage -static'

# Configure for profiling
pg-fp:
	$(MAKE) -f $(firstword $(MAKEFILE)) GENDIR=$(PGGENDIR) GENOPT='' CFLAGS='-g -pg -O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg -static'

pg-nofp:
	$(MAKE) -f $(firstword $(MAKEFILE)) GENDIR=$(PGGENDIR) GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -pg -O2 -msoft-float $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' FPU_TYPE=-1 XOBJS='$(GLOBJDIR)/gsfemu.o'

# Configure for debugging and no FPU (crude timing configuration)
nofp:
	$(MAKE) -f $(firstword $(MAKEFILE)) GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -O0 -msoft-float $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)' FPU_TYPE=-1 XOBJS='$(GLOBJDIR)/gsfemu.o'

# Configure for optimization.
product:
	$(MAKE) -f $(firstword $(MAKEFILE)) GENOPT='' GCFLAGS='$(GCFLAGS)' CFLAGS='-O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS)'

clean_gs:
	$(MAKE) -f $(GLSRCDIR)/ugcclib.mak \
	GLSRCDIR='$(GLSRCDIR)' GLGENDIR='$(GLGENDIR)' \
	GLOBJDIR='$(GLOBJDIR)' clean

ifeq ($(PSICFLAGS), -DPSI_INCLUDED)
# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs files are out of date.
$(GENDIR)/ldgs.tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	$(MAKE) \
	  GCFLAGS='$(GCFLAGS)' FPU_TYPE='$(FPU_TYPE)'\
	  CONFIG='$(CONFIG)' FEATURE_DEVS='$(FEATURE_DEVS)' \
	  XINCLUDE='$(XINCLUDE)' XLIBDIRS='$(XLIBDIRS)' XLIBDIR='$(XLIBDIR)' XLIBS='$(XLIBS)' \
          DEVICE_DEVS='$(DEVICE_DEVS) $(DD)bbox.dev' \
          DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= DEVICE_DEVS4= \
          DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
          DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
          DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
          DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	  STDLIBS=$(STDLIBS) \
	  SYNC=$(SYNC) \
	  BAND_LIST_STORAGE=memory BAND_LIST_COMPRESSOR=zlib \
	  ZSRCDIR=$(ZSRCDIR) ZGENDIR=$(ZGENDIR) ZOBJDIR=$(ZOBJDIR) ZLIB_NAME=$(ZLIB_NAME) SHARE_ZLIB=$(SHARE_ZLIB) \
	  JSRCDIR=$(JSRCDIR) JGENDIR=$(JGENDIR) JOBJDIR=$(JOBJDIR) \
	  GLSRCDIR='$(GLSRCDIR)' PSSRCDIR=$(PSSRCDIR) PSGENDIR=$(GENDIR)  PSD='$(GENDIR)/' \
	  GLGENDIR='$(GLGENDIR)' GLOBJDIR='$(GLOBJDIR)' PSLIBDIR=$(PSLIBDIR) \
	  EXPATSRCDIR=$(EXPATSRCDIR) SHARE_EXPAT=$(SHARE_EXPAT) \
	  EXPAT_CFLAGS=$(EXPAT_CFLAGS) \
	  ICCSRCDIR=$(ICCSRCDIR) IMDISRCDIR=$(IMDISRCDIR) \
          COMPILE_INITS=$(COMPILE_INITS) PCLXL_ROMFS_ARGS='$(PCLXL_ROMFS_ARGS)' PJL_ROMFS_ARGS='$(PJL_ROMFS_ARGS)' \
	  UFST_ROOT=$(UFST_ROOT) UFST_BRIDGE=$(UFST_BRIDGE) UFST_LIB_EXT=$(UFST_LIB_EXT) \
	  UFST_ROMFS_ARGS='$(UFST_ROMFS_ARGS)' \
	  PNGSRCDIR=$(PNGSRCDIR) \
	  SHARE_LIBPNG=$(SHARE_LIBPNG) PNGCCFLAGS=$(PNGCCFLAGS) \
	  PSOBJDIR=$(GENDIR) IJSSRCDIR=../gs/ijs \
	  -f $(GLSRCDIR)/unix-gcc.mak\
	  $(GLOBJDIR)/ld.tr \
	  $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gsfemu.o \
	  $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o $(GLOBJDIR)/iconfig.$(OBJ) \
	  $(GLOBJDIR)/iccinit$(COMPILE_INITS).$(OBJ)
	  cp $(GLOBJDIR)/ld.tr $(GENDIR)/ldgs.tr

FORCE:

else


# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs files are out of date.
$(GENDIR)/ldgs.tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	$(MAKE) \
	  GCFLAGS='$(GCFLAGS)' FPU_TYPE='$(FPU_TYPE)'\
	  CONFIG='$(CONFIG)' FEATURE_DEVS='$(FEATURE_DEVS)' \
	  XINCLUDE='$(XINCLUDE)' XLIBDIRS='$(XLIBDIRS)' XLIBDIR='$(XLIBDIR)' XLIBS='$(XLIBS)' \
          DEVICE_DEVS='$(DEVICE_DEVS) $(DD)bbox.dev' \
	  STDLIBS=$(STDLIBS) \
	  SYNC=$(SYNC) \
	  BAND_LIST_STORAGE=memory BAND_LIST_COMPRESSOR=zlib \
	  ZSRCDIR=$(ZSRCDIR) ZGENDIR=$(ZGENDIR) ZOBJDIR=$(ZOBJDIR) ZLIB_NAME=$(ZLIB_NAME) SHARE_ZLIB=$(SHARE_ZLIB) \
	  JSRCDIR=$(JSRCDIR) JGENDIR=$(JGENDIR) JOBJDIR=$(JOBJDIR) \
	  GLSRCDIR='$(GLSRCDIR)' PSSRCDIR=$(PSSRCDIR) PSGENDIR=$(GENDIR) PSD='$(GENDIR)/' \
	  GLGENDIR='$(GLGENDIR)' GLOBJDIR='$(GLOBJDIR)' PSLIBDIR=$(PSLIBDIR) \
	  EXPATSRCDIR=$(EXPATSRCDIR) SHARE_EXPAT=$(SHARE_EXPAT) \
	  EXPAT_CFLAGS=$(EXPAT_CFLAGS) \
	  ICCSRCDIR=$(ICCSRCDIR) IMDISRCDIR=$(IMDISRCDIR) \
          COMPILE_INITS=$(COMPILE_INITS) PCLXL_ROMFS_ARGS='$(PCLXL_ROMFS_ARGS)' PJL_ROMFS_ARGS='$(PJL_ROMFS_ARGS)' \
	  UFST_ROOT=$(UFST_ROOT) UFST_BRIDGE=$(UFST_BRIDGE) UFST_LIB_EXT=$(UFST_LIB_EXT) \
	  UFST_ROMFS_ARGS='$(UFST_ROMFS_ARGS)' \
	  UFST_CFLAGS='$(UFST_CFLAGS)' \
	  PNGSRCDIR=$(PNGSRCDIR) \
	  SHARE_LIBPNG=$(SHARE_LIBPNG) PNGCCFLAGS=$(PNGCCFLAGS) \
	  PSOBJDIR=$(GENDIR) IJSSRCDIR=../gs/ijs \
	  -f $(GLSRCDIR)/ugcclib.mak \
	  $(GLOBJDIR)/ld.tr \
	  $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gsfemu.o \
	  $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o
	  cp $(GLOBJDIR)/ld.tr $(GENDIR)/ldgs.tr

FORCE:

endif

# Build the configuration file.
$(GENDIR)/pconf.h $(GENDIR)/ldconf.tr: $(TARGET_DEVS) $(GLOBJDIR)/genconf$(XE)
	$(GLOBJDIR)/genconf -n - $(TARGET_DEVS) -h $(GENDIR)/pconf.h -p "%s&s&&" -o $(GENDIR)/ldconf.tr

# Create a library
$(TARGET_LIB): $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr $(MAIN_OBJ) $(TOP_OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(AR) $(ARFLAGS)  $@
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ) $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o -s
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(XOBJS) -s
	cat $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr | grep ".o" >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(MAIN_OBJ)
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; sh <$(GENDIR)/ldall.tr

ifeq ($(PSICFLAGS), -DPSI_INCLUDED)
# Link a Unix executable.
$(TARGET_XE): $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr $(MAIN_OBJ) $(TOP_OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ)  -s
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(XOBJS) -s
	cat $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	sh <$(GENDIR)/ldall.tr
else
# Link a Unix executable.
$(TARGET_XE): $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr $(MAIN_OBJ) $(TOP_OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldall.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(TOP_OBJ) $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gconfig.o $(GLOBJDIR)/gscdefs.o -s
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -n -s $(XOBJS) -s
	cat $(GENDIR)/ldgs.tr $(GENDIR)/ldconf.tr >>$(GENDIR)/ldall.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldall.tr -s - $(MAIN_OBJ) $(EXTRALIBS) $(STDLIBS)
	sh <$(GENDIR)/ldall.tr
endif
