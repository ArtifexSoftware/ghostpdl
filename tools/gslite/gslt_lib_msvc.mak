# gslite library makefile.

MAKEFILE=gslt_lib_msvc.mak
MSVC_VERSION=8

# Debugging options.  For production system DEBUG=0 and TDEBUG=0.
DEBUG=1
TDEBUG=1

DEVSTUDIO=
NOPRIVATE=0

COMPILE_INITS=0


# this is where we build everything.  The ghostscript makefiles will
# need all of these set to work properly.
GENDIR=.\obj
GLGENDIR=.\obj
GLOBJDIR=.\obj

# location of ghostscript source code.
GLSRCDIR=..\..\gs\base

# sigh.  will we ever fix this?  Definitions to force gx_color_index
# to 64 bits and define GSLITE.

XCFLAGS=/DGSLITE

DD=$(GLGENDIR)

# output devices - none required.
DEVICES=

# required features in addition to libcore.dev.  NB This should be
# pruned.
FEATURES=\
	$(DD)\page.dev $(DD)\gsnogc.dev $(DD)\ttflib.dev $(DD)\sdctd.dev\
	$(DD)\psf1lib.dev $(DD)\psf2lib.dev\
	$(DD)\path1lib.dev $(DD)\shadelib.dev $(DD)\cidlib.dev\
	$(DD)\psl3lib.dev $(DD)\slzwd.dev $(DD)\szlibd.dev\
	$(DD)\cmaplib.dev $(DD)\libpng.dev

# jpeg
JSRCDIR=..\..\gs\jpeg
JGENDIR=$(GENDIR)
JOBJDIR=$(GENDIR)

# zlib
ZSRCDIR=..\..\gs\zlib
ZGENDIR=$(GENDIR)
ZOBJDIR=$(GENDIR)

# lib png
PNGSRCDIR=..\..\gs\libpng
PNGGENDIR=$(GENDIR)
PNGOBJDIR=$(GENDIR)

# indicate we do not want use system shared libraries for zlib, png,
# and jpeg.
SHARE_ZLIB=0
SHARE_LIBPNG=0
SHARE_JPEG=0

STDLIBS=-lm

GSLITE_LIB=libgslt.dll
all: $(GSLITE_LIB)

# invoke the gs *library* makefile to build needed
# devices, objects and configuration files.
GSLIB_PARTS=$(GLOBJDIR)\ld.tr
$(GSLIB_PARTS): $(MAKEFILE) 
	-mkdir $(GLOBJDIR)
	$(MAKE) \
	  DEBUG=$(DEBUG) \
	  TDEBUG=$(TDEBUG) \
	  MSVC_VERSION=$(MSVC_VERSION) \
	  DEVSTUDIO=$(DEVSTUDIO) \
	  GCFLAGS=$(GCFLAGS) \
	  XCFLAGS=$(XCFLAGS) \
	  STDLIBS=$(STDLIBS) \
	  FEATURE_DEVS="$(FEATURES)"\
          DEVICE_DEVS="$(DEVICES)"\
	  GLSRCDIR=$(GLSRCDIR) GLGENDIR=$(GLGENDIR) GLOBJDIR=$(GLOBJDIR) \
	  JSRCDIR=$(JSRCDIR) JGENDIR=$(JGENDIR) JOBJDIR=$(JOBJDIR)\
	  ZSRCDIR=$(ZSRCDIR) ZGENDIR=$(ZGENDIR) ZOBJDIR=$(ZOBJDIR)\
	  PNGSRCDIR=$(PNGSRCDIR) PNGGENDIR=$(PNGGENDIR) PNGOBJDIR=$(PNGOBJDIR)\
	  TIFFSRCDIR=$(TIFFSRCDIR) TIFFCONFIG_SUFFIX=$(TIFFCONFIG_SUFFIX) TIFFPLATFORM=$(TIFFPLATFORM)\
	  PSD="$(GENDIR)/"\
	  SHARE_ZLIB=$(SHARE_ZLIB) SHARE_LIBPNG=$(SHARE_LIBPNG) SHARE_JPEG=$(SHARE_JPEG)\
	  /f $(GLSRCDIR)\msvclib.mak \
	  $(GLOBJDIR)\ld.tr \
	  $(GLOBJDIR)\gsargs.obj \
	  $(GLOBJDIR)\gconfig.obj $(GLOBJDIR)\gscdefs.obj \
	  $(GLOBJDIR)\jpeglib_.h


# ---------------------------------
# settings needed for building the gslite objects of gslt.mak

GSLTSRCDIR=.


# common definitions - CC_, I_ etc.
COMMONDIR=..\..\common
!include $(COMMONDIR)\pcdefs.mak
!include $(COMMONDIR)\msvcdefs.mak
!include $(GLSRCDIR)\msvccmd.mak

# include for gslt sources
!include gslt.mak

# ---------------------------------
# put gslite and graphics library objects in one library



# windows clutter.
MSXLIBS=shell32.lib comdlg32.lib gdi32.lib user32.lib winspool.lib advapi32.lib
# gs library config and miscellany.
GSXLIBS=$(GLOBJDIR)\gsargs.obj $(GLOBJDIR)\gconfig.obj $(GLOBJDIR)\gscdefs.obj

$(GSLITE_LIB): $(GSLIB_PARTS) $(GSLT_OBJS) $(GSXLIBS) $(MAKEFILE)
	link -dll -out:$(GSLITE_LIB) -def:gslt.def \
	-implib:gsltimp.lib $(GSLT_OBJS) $(MSXLIBS) $(GSXLIBS) @obj\ld.tr 

# the dependency on GSLIB_LIB is needed because the c compiler line
# @ccf32.tr is a needed side effect from building the library.
$(GSLTOBJ)gslt_image_test.$(OBJ) : $(GSLITE_LIB) $(GSLTSRC)gslt_image_test.c
	$(GSLT_CC) $(GSLTO_)gslt_image_test.$(OBJ) $(C_) $(GSLTSRC)gslt_image_test.c

$(GSLTOBJ)gslt_font_test.$(OBJ) : $(GSLITE_LIB) $(GSLTSRC)gslt_font_test.c
	$(GSLT_CC) $(GSLTO_)gslt_font_test.$(OBJ) $(C_) $(GSLTSRC)gslt_font_test.c

gslt_image_test.exe: $(GSLTOBJ)gslt_image_test.$(OBJ) $(GSLT_OBJS) $(GSLITE_LIB)
	link $(GSLTOBJ)gslt_image_test.$(OBJ) gsltimp.lib

gslt_font_test.exe: $(GSLTOBJ)gslt_font_test.$(OBJ) $(GSLT_OBJS) $(GSLITE_LIB)
	link $(GSLTOBJ)gslt_font_test.$(OBJ) gsltimp.lib


check: gslt_font_test.exe gslt_image_test.exe
	gslt_font_test ..\..\urwfonts\CenturySchL-Bold.ttf
	gslt_image_test tiger.jpg

clean:
	del *.pnm *.pbm *.lib *.exe obj\*.obj *.obj obj\*.tr
