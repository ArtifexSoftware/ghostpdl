#    Copyright (C) 1996 Aladdin Enterprises.  All rights reserved.
#    Unauthorized use, copying, and/or distribution prohibited.

# makefile for PCL XL or PCL 5 interpreter, Unix/gcc platform.

# Note: the Linux link of only the imager library for PCL5 with DEBUG is:
#	text	data	bss	dec	hex	filename
#	379349 	77942  	3052   	460343 	70637  	pcl5

# Define generic configuration options.
GENOPT=-DDEBUG

# Define the directory with the Ghostscript source files.
GSDIR=..
#GSDIR=$(D)gs
GD=$(GSDIR)$(D)

# Define the names of the executable files.
PCL5=pcl5
PCLXL=pclxl
# Define the target.  For PCL5:
LANGUAGE=pcl5
CONFIG=5
TARGET_DEVS=pcl5c.dev hpgl2c.dev
TARGET_XE=$(PCL5)
# For PCL XL:
#LANGUAGE=pclxl
#CONFIG=6
#TARGET_DEVS=pclxl.dev
#TARGET_XE=$(PCLXL)

################ No changes needed below here ################

# Define Unix options.
#GCFLAGS=-Wall -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wwrite-strings
GCFLAGS=-Dconst= -Wall -Wpointer-arith -Wstrict-prototypes
# We use -O0 for debugging, because optimization confuses gdb.
CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS)
LDFLAGS=$(XLDFLAGS)
EXTRALIBS=
XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt SM ICE Xext X11

DEVICE_DEVS=x11.dev x11mono.dev x11alpha.dev x11cmyk.dev\
 djet500.dev ljet4.dev\
 pcxmono.dev\
 pbmraw.dev pgmraw.dev ppmraw.dev

MAKEFILE=ugccpcl.mak

# Define parameters for a Unix/gcc build.
CCC=gcc $(GENOPT) $(CFLAGS) -I. -I$(GSDIR) -c
CCLEAF=$(CCC) -fomit-frame-pointer
CCLD=gcc
CP_=cp -p
D=/
GS=gs
OBJ=o
RMN_=rm -f
TYPE=cat
XE=

.c.o:
	$(CCC) $*.c

include $(LANGUAGE).mak
include pcllib.mak

# Configure for profiling
pg-fp:
	make GENOPT='' CFLAGS='-pg -O $(GCFLAGS) -I$(GSDIR) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' CCLEAF='$(CCC)'
pg-nofp:
	make GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-pg -O -msoft-float $(GCFLAGS) -I$(GSDIR) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GD)gsfemu.o'

# Configure for debugging and no FPU (crude timing configuration)
nofp:
	make GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-g -O0 -msoft-float $(GCFLAGS) -I$(GSDIR) $(XCFLAGS)' XLIBS='Xt SM ICE Xext X11' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GD)gsfemu.o'

# Configure for optimization and no FPU (product configuration)
product:
	make GENOPT='' GCFLAGS='-msoft-float $(GCFLAGS)' CFLAGS='-O -msoft-float $(GCFLAGS) -I$(GSDIR) $(XCFLAGS)' XLIBS='Xt SM ICE Xext X11' FPU_TYPE=-1 CCLEAF='$(CCC)' XOBJS='$(GD)gsfemu.o'

# Build the required files in the GS directory.
# It's simplest always to build the floating point emulator,
# even though we don't always link it in.
ld$(LANGUAGE).tr: $(MAKEFILE) $(LANGUAGE).mak
	make -C $(GD) \
	  GCFLAGS='$(GCFLAGS)' FPU_TYPE='$(FPU_TYPE)'\
	  CONFIG='$(CONFIG)' FEATURE_DEVS='$(FEATURE_DEVS)' \
	  DEVICE_DEVS='$(DEVICE_DEVS)' \
	  cl_impl=clmem \
	  cl_filter_devs=zlib cl_filter_name=zlib \
	  -f ugcclib.mak \
	  ld$(CONFIG).tr gsfemu.o gsnogc.o gconfig$(CONFIG).o gscdefs$(CONFIG).o
	cp $(GD)ld$(CONFIG).tr ld$(LANGUAGE).tr

# Build the configuration file.
pconf$(CONFIG).h ldconf$(CONFIG).tr: $(TARGET_DEVS) $(GD)genconf$(XE)
	$(GD)genconf -n - $(TARGET_DEVS) -h pconf$(CONFIG).h -p "%s&s&&" -o ldconf$(CONFIG).tr

# Link a Unix executable.
$(TARGET_XE): ld$(LANGUAGE).tr ldconf$(CONFIG).tr $(LANGUAGE).$(OBJ)
	$(GD)echogs -w ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(TARGET_XE)
	$(GD)echogs -a ldt.tr -n -s $(GD)gsnogc.o $(GD)gconfig$(CONFIG).o $(GD)gscdefs$(CONFIG).o -s
	$(GD)echogs -a ldt.tr -n -s $(XOBJS) -s
	$(GD)echogs -w t.tr -n for f in -s
	cat ld$(LANGUAGE).tr >>t.tr
	echo \; do >>t.tr
	echo if \( test -f $(GD)\$$f \) then >>t.tr
	echo $(GD)echogs -a ldt.tr -q $(GD) \$$f -x 205c >>t.tr
	echo else >>t.tr
	echo $(GD)echogs -a ldt.tr -q \$$f -x 205c >>t.tr
	echo fi >>t.tr
	echo done >>t.tr
	sh <t.tr
	cat ldconf$(CONFIG).tr >>ldt.tr
	$(GD)echogs -a ldt.tr -s - $(LANGUAGE).$(OBJ) $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; sh <ldt.tr
