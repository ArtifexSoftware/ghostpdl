#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# ugcc_top.mak
# Generic top-level makefile for Unix/gcc platforms.

# The product-specific top-level makefile defines the following:
#	MAKEFILE, CCLD, COMMONDIR, CONFIG, DEVICE_DEVS, GCFLAGS, GENDIR,
#	GLSRCDIR, GLGENDIR, GLOBJDIR,
#	MAIN_OBJ, TARGET_DEVS, TARGET_XE
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
	make -f $(MAKEFILE) GENOPT='-DDEBUG' CFLAGS='-g -O0 $(GCFLAGS) $(XCFLAGS)'

# Configure for profiling
pg-fp:
	make -f $(MAKEFILE) GENOPT='' CFLAGS='-g -pg -O2 $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg'
pg-nofp:
	make -f $(MAKEFILE) GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -pg -O2 -msoft-float $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' FPU_TYPE=-1 XOBJS='$(GLOBJDIR)/gsfemu.o'

# Configure for debugging and no FPU (crude timing configuration)
nofp:
	make -f $(MAKEFILE) GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -O0 -msoft-float $(GCFLAGS) $(XCFLAGS)' FPU_TYPE=-1 XOBJS='$(GLOBJDIR)/gsfemu.o'

# Configure for optimization.
product:
	make -f $(MAKEFILE) GENOPT='' GCFLAGS='$(GCFLAGS)' CFLAGS='-O2 $(GCFLAGS) $(XCFLAGS)'

clean_gs:
	make -f $(GLSRCDIR)/ugcclib.mak \
	GLSRCDIR='$(GLSRCDIR)' GLGENDIR='$(GLGENDIR)' \
	GLOBJDIR='$(GLOBJDIR)' clean
	
# Build the required GS library files.  It's simplest always to build
# the floating point emulator, even though we don't always link it in.
# HACK * HACK * HACK - we force this make to occur since we have no
# way to determine if gs files are out of date.
$(GENDIR)/ldl$(CONFIG).tr: FORCE
	-mkdir $(GLGENDIR)
	-mkdir $(GLOBJDIR)
	make \
	  GCFLAGS='$(GCFLAGS)' FPU_TYPE='$(FPU_TYPE)'\
	  CONFIG='$(CONFIG)' FEATURE_DEVS='$(FEATURE_DEVS)' \
	  XINCLUDE='$(XINCLUDE)' XLIBDIRS='$(XLIBDIRS)' XLIBDIR='$(XLIBDIR)' XLIBS='$(XLIBS)' \
          DEVICE_DEVS='$(DEVICE_DEVS) $(DD)bbox.dev' \
	  BAND_LIST_STORAGE=memory BAND_LIST_COMPRESSOR=zlib \
	  ZSRCDIR=$(ZSRCDIR) ZGENDIR=$(ZGENDIR) ZOBJDIR=$(ZOBJDIR) ZLIB_NAME=$(ZLIB_NAME) SHARE_ZLIB=$(SHARE_ZLIB) \
	  GLSRCDIR='$(GLSRCDIR)' \
	  GLGENDIR='$(GLGENDIR)' GLOBJDIR='$(GLOBJDIR)' \
	  -f $(GLSRCDIR)/ugcclib.mak \
	  $(GLOBJDIR)/ld$(CONFIG).tr \
	  $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gsfemu.o $(GLOBJDIR)/gsnogc.o \
	  $(GLOBJDIR)/gconfig$(CONFIG).o $(GLOBJDIR)/gscdefs$(CONFIG).o
	cp $(GLOBJDIR)/ld$(CONFIG).tr $(GENDIR)/ldl$(CONFIG).tr

FORCE:

# Build the configuration file.
$(GENDIR)/pconf$(CONFIG).h $(GENDIR)/ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GLOBJDIR)/genconf$(XE)
	$(GLOBJDIR)/genconf -n - $(TARGET_DEVS) -h $(GENDIR)/pconf$(CONFIG).h -p "%s&s&&" -o $(GENDIR)/ldconf$(CONFIG).tr

# Link a Unix executable.
$(TARGET_XE): $(GENDIR)/ldl$(CONFIG).tr $(GENDIR)/ldconf$(CONFIG).tr $(MAIN_OBJ)
	$(ECHOGS_XE) -w $(GENDIR)/ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a $(GENDIR)/ldt.tr -n -s $(GLOBJDIR)/gsargs.o $(GLOBJDIR)/gsnogc.o $(GLOBJDIR)/gconfig$(CONFIG).o $(GLOBJDIR)/gscdefs$(CONFIG).o -s
	$(ECHOGS_XE) -a $(GENDIR)/ldt.tr -n -s $(XOBJS) -s
	cat $(GENDIR)/ldl$(CONFIG).tr $(GENDIR)/ldconf$(CONFIG).tr >>$(GENDIR)/ldt.tr
	$(ECHOGS_XE) -a $(GENDIR)/ldt.tr -s - $(MAIN_OBJ) $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; sh <$(GENDIR)/ldt.tr
