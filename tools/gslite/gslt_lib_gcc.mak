# gslite library makefile.
MAKEFILE=gslt_lib_gcc.mak

# needed to tell how to build the shared library 
PLATFORM := $(shell uname)

# this is where we build everything.  The ghostscript makefiles will
# need all of these set to work properly.
GENDIR=./obj
GLGENDIR=./obj
GLOBJDIR=./obj

# location of ghostscript source code.
GLSRCDIR=../../gs/src

# sigh.  will we ever fix this?
GX_COLOR_INDEX_DEFINE=-DGX_COLOR_INDEX_TYPE="unsigned long long"

# set up flags.  NB refactor duplication - see gccdefs.mak included
# below and figure out how best to set up GCFLAGS, XCFLAGS, and
# .*FLAGS

GCFLAGS=-DGSLITE -Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings -DNDEBUG  $(GX_COLOR_INDEX_DEFINE) -fPIC -fno-common
XCFLAGS=-DGSLITE -Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings -DNDEBUG  $(GX_COLOR_INDEX_DEFINE) -fPIC -fno-common

DD=$(GLGENDIR)/

# output devices - none required.
DEVICES=

# required features in addition to libcore.dev.  NB This should be
# pruned.
FEATURES=\
	$(DD)page.dev $(DD)gsnogc.dev $(DD)ttflib.dev $(DD)sdctd.dev\
	$(DD)psf1lib.dev $(DD)psf2lib.dev\
	$(DD)path1lib.dev $(DD)shadelib.dev $(DD)cidlib.dev\
	$(DD)psl3lib.dev $(DD)slzwd.dev $(DD)szlibd.dev\
	$(DD)cmaplib.dev $(DD)libpng.dev

# jpeg
JSRCDIR=../../gs/jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)

# zlib
ZSRCDIR=../../gs/zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)

# lib png
PNGSRCDIR=../../gs/libpng
PNGGENDIR=$(GENDIR)
PNGOBJDIR=$(GENDIR)

SHARE_ZLIB=0
SHARE_LIBPNG=0
SHARE_JPEG=0

# ill-named extra libraries.
STDLIBS=-lm

# shared library NB - needs soname.
GSLITE_LIB=libgslt.so

all: $(GSLITE_LIB)

# gs library config and miscellany.
GSXLIBS=$(GLOBJDIR)/gsargs.$(OBJ) $(GLOBJDIR)/gconfig.$(OBJ) $(GLOBJDIR)/gscdefs.$(OBJ)

# invoke the gs *library* makefile to build needed
# devices, objects and configuration files.
GSLIB_PARTS=$(GLOBJDIR)/ld.tr
$(GSLIB_PARTS): $(MAKEFILE)
	-mkdir $(GLOBJDIR)
	$(MAKE) \
	  GCFLAGS='$(GCFLAGS)' \
	  STDLIBS="$(STDLIBS)" \
	  FEATURE_DEVS="$(FEATURES)"\
          DEVICE_DEVS="$(DEVICES)"\
	  GLSRCDIR=$(GLSRCDIR) GLGENDIR=$(GLGENDIR) GLOBJDIR=$(GLOBJDIR) \
	  JSRCDIR=$(JSRCDIR) JGENDIR=$(JGENDIR) JOBJDIR=$(JOBJDIR)\
	  ZSRCDIR=$(ZSRCDIR) ZGENDIR=$(ZGENDIR) ZOBJDIR=$(ZOBJDIR)\
	  PNGSRCDIR=$(PNGSRCDIR) PNGGENDIR=$(PNGGENDIR) PNGOBJDIR=$(PNGOBJDIR)\
          PSD='$(GENDIR)/'\
	  SHARE_ZLIB=$(SHARE_ZLIB) SHARE_LIBPNG=$(SHARE_LIBPNG) SHARE_JPEG=$(SHARE_JPEG)\
	  -f $(GLSRCDIR)/ugcclib.mak \
	  $(GLOBJDIR)/ld.tr \
	  $(GSXLIBS) \
	  $(GLOBJDIR)/jpeglib_.h

# ---------------------------------
# settings needed for building the gslite objects of gslt.mak

GSLTSRCDIR=.

# common definitions - CC_, I_ etc.
COMMONDIR=../../common
include $(COMMONDIR)/unixdefs.mak
include $(COMMONDIR)/gccdefs.mak

# include for gslt sources
include gslt.mak

# ---------------------------------
# put gslite and graphics library objects in one library

# ---------------------------------
# put gslite and graphics library objects in one library

LIBS = -lgslt -lpthread -L./

# NB missing soname, version business.
$(GSLITE_LIB): $(GSLIB_PARTS) $(GSLT_OBJS) $(MAKEFILE)
ifeq ($(PLATFORM), Darwin)
	gcc -dynamiclib -o libgslt.dylib $(shell cat obj/ld.tr | sed 's/\\//g') $(GSXLIBS) $(GSLT_OBJS) $(STDLIBS)
else
	gcc -shared $(shell cat obj/ld.tr | sed 's/\\//g') $(GSXLIBS) $(GSLT_OBJS) -o $(GSLITE_LIB) $(STDLIBS)
endif

$(GSLTOBJ)gslt_image_test.$(OBJ) : $(GSLITE_LIB) $(GSLTSRC)gslt_image_test.c
	$(GSLT_CC) $(GSLTO_)gslt_image_test.$(OBJ) $(C_) $(GSLTSRC)gslt_image_test.c

$(GSLTOBJ)gslt_font_test.$(OBJ) : $(GSLITE_LIB) $(GSLTSRC)gslt_font_test.c
	$(GSLT_CC) $(GSLTO_)gslt_font_test.$(OBJ) $(C_) $(GSLTSRC)gslt_font_test.c

$(GSLTOBJ)gslt_image_threads_test.$(OBJ) : $(GSLITE_LIB) $(GSLTSRC)gslt_image_threads_test.c
	$(GSLT_CC) $(GSLTO_)gslt_image_threads_test.$(OBJ) $(C_) $(GSLTSRC)gslt_image_threads_test.c

gslt_image_test: $(GSLTOBJ)gslt_image_test.$(OBJ) $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

gslt_font_test: $(GSLTOBJ)gslt_font_test.$(OBJ) $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)

gslt_image_threads_test: $(GSLTOBJ)gslt_image_threads_test.$(OBJ) $(gslt_OBJS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS) $(LIBS)


check: $(GSLITE_LIB) gslt_image_test gslt_font_test gslt_image_threads_test
	LD_LIBRARY_PATH=. ./gslt_image_test tiger.jpg
	LD_LIBRARY_PATH=. ./gslt_image_threads_test tiger.jpg
	LD_LIBRARY_PATH=. ./gslt_font_test ../../urwfonts/CenturySchL-Bold.ttf

clean:
	$(RM) *.pnm *.pbm *.so *.o *~ obj/* gslt_image_test gslt_font_test gslt_image_threads_test

.PHONY: clean check all
