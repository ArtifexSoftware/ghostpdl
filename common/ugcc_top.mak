#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# ugcc_top.mak
# Generic top-level makefile for Unix/gcc platforms.

# The product-specific top-level makefile defines the following:
#	MAKEFILE, CCLD, COMMONDIR, CONFIG, DEVICE_DEVS, GCFLAGS, GSDIR,
#	MAIN_OBJ, TARGET_DEVS, TARGET_XE
# It also must include the product-specific *.mak.

default: $(TARGET_XE)

# Platform specification
include $(COMMONDIR)/gccdefs.mak
include $(COMMONDIR)/unixdefs.mak
include $(COMMONDIR)/generic.mak

# Configure for debugging
debug:
	make GENOPT='-DDEBUG'

# Configure for profiling
pg-fp:
	make GENOPT='' CFLAGS='-pg -O $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' CCLEAF='$(CCC)'
pg-nofp:
	make GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-pg -O -msoft-float $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GSDIR)/gsfemu.o'

# Configure for debugging and no FPU (crude timing configuration)
nofp:
	make GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -O0 -msoft-float $(GCFLAGS) $(XCFLAGS)' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GSDIR)/gsfemu.o'

# Configure for optimization and no FPU (product configuration)
product:
	make GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-O -msoft-float $(GCFLAGS) $(XCFLAGS)' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GSDIR)/gsfemu.o'

# Build the required files in the GS directory.
# It's simplest always to build the floating point emulator,
# even though we don't always link it in.
ld$(CONFIG).tr: $(MAKEFILE)
	make -C $(GSDIR) \
	  GCFLAGS='$(GCFLAGS)' FPU_TYPE='$(FPU_TYPE)'\
	  CONFIG='$(CONFIG)' FEATURE_DEVS='$(FEATURE_DEVS)' \
	  DEVICE_DEVS='$(DEVICE_DEVS) bbox.dev' \
	  BAND_LIST_STORAGE=memory BAND_LIST_COMPRESSOR=zlib \
	  -f ugcclib.mak \
	  ld$(CONFIG).tr \
	  gsargs.o gsfemu.o gsnogc.o \
	  gconfig$(CONFIG).o gscdefs$(CONFIG).o
	cp $(GSDIR)/ld$(CONFIG).tr ld$(CONFIG).tr

# Build the configuration file.
pconf$(CONFIG).h ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GSDIR)/genconf$(XE)
	$(GSDIR)/genconf -n - $(TARGET_DEVS) -h pconf$(CONFIG).h -p "%s&s&&" -o ldconf$(CONFIG).tr

# Link a Unix executable.
$(TARGET_XE): ld$(CONFIG).tr ldconf$(CONFIG).tr $(MAIN_OBJ)
	$(ECHOGS_XE) -w ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(ECHOGS_XE) -a ldt.tr -n -s $(GSDIR)/gsargs.o $(GSDIR)/gsnogc.o $(GSDIR)/gconfig$(CONFIG).o $(GSDIR)/gscdefs$(CONFIG).o -s
	$(ECHOGS_XE) -a ldt.tr -n -s $(XOBJS) -s
	$(ECHOGS_XE) -w t.tr -n for f in -s
	cat ld$(CONFIG).tr >>t.tr
	echo \; do >>t.tr
	echo if \( test -f $(GSDIR)/\$$f \) then >>t.tr
	echo $(ECHOGS_XE) -a ldt.tr -q $(GSDIR)/ \$$f -x 205c >>t.tr
	echo else >>t.tr
	echo $(ECHOGS_XE) -a ldt.tr -q \$$f -x 205c >>t.tr
	echo fi >>t.tr
	echo done >>t.tr
	sh <t.tr
	cat ldconf$(CONFIG).tr >>ldt.tr
	$(ECHOGS_XE) -a ldt.tr -s - $(MAIN_OBJ) $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; sh <ldt.tr
