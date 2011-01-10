#
# "$Id: Makefile 9120 2010-04-23 18:56:34Z mike $"
#
#   Filter makefile for the Common UNIX Printing System (CUPS).
#
#   Copyright 2007-2010 by Apple Inc.
#   Copyright 1997-2006 by Easy Software Products.
#
#   These coded instructions, statements, and computer programs are the
#   property of Apple Inc. and are protected by Federal copyright
#   law.  Distribution and use rights are outlined in the file "LICENSE.txt"
#   which should have been included with this file.  If this file is
#   file is missing or damaged, see the license at "http://www.cups.org/".
#
#   This file is subject to the Apple OS-Developed Software exception.
#

include ../Makedefs


LIBTARGETS =	\
		$(LIBCUPSIMAGE) \
		libcupsimage.a \
		$(LIB32CUPSIMAGE) \
		$(LIB64CUPSIMAGE)
TARGETS	=	\
		$(LIBTARGETS) \

IMAGEOBJS =	image-bmp.o image-colorspace.o image-gif.o image-jpeg.o \
		image-photocd.o image-pix.o image-png.o image-pnm.o \
		image-sgi.o image-sgilib.o image-sun.o image-tiff.o \
		image-zoom.o image.o error.o interpret.o raster.o
IMAGE32OBJS =	$(IMAGEOBJS:.o=.32.o)
IMAGE64OBJS =	$(IMAGEOBJS:.o=.64.o)
OBJS	=	$(IMAGEOBJS)


#
# Make all targets...
#

all:	$(TARGETS)


#
# Make library targets...
#

libs:		$(LIBTARGETS)


#
# Clean all object files...
#

clean:
	$(RM) $(OBJS) $(TARGETS)
	$(RM) libcupsimage.so libcupsimage.sl libcupsimage.dylib
	$(RM) -r 32bit 64bit


#
# Update dependencies (without system header dependencies...)
#

depend:
	touch Dependencies.tmp
	makedepend -Y -I.. -fDependencies.tmp $(OBJS:.o=.c) >/dev/null 2>&1
	$(RM) Dependencies
	cp Dependencies.tmp Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.32.o: \1\.c /' Dependencies.tmp >>Dependencies
	sed -E -e '1,$$s/^([^.]+)\.o:/\1\.64.o: \1\.c /' Dependencies.tmp >>Dependencies
	$(RM) Dependencies.tmp


#
# Install all targets...
#

install:	all install-data install-headers install-libs install-exec


#
# Install data files...
#

install-data:


#
# Install programs...
#

install-exec:


#
# Install headers...
#

install-headers:
	$(INSTALL_DIR) -m 755 $(INCLUDEDIR)/cups
	$(INSTALL_DATA) image.h $(INCLUDEDIR)/cups


#
# Install libraries...
#

install-libs: $(INSTALLSTATIC) $(INSTALL32) $(INSTALL64)
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) $(LIBCUPSIMAGE) $(LIBDIR)
	-if test $(LIBCUPSIMAGE) = "libcupsimage.so.2" -o $(LIBCUPSIMAGE) = "libcupsimage.sl.2"; then \
		$(RM) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/`basename $(LIBCUPSIMAGE) .2`; \
	fi
	-if test $(LIBCUPSIMAGE) = "libcupsimage.2.dylib"; then \
		$(RM) $(LIBDIR)/libcupsimage.dylib; \
		$(LN) $(LIBCUPSIMAGE) $(LIBDIR)/libcupsimage.dylib; \
	fi
	if test "x$(SYMROOT)" != "x"; then \
		$(INSTALL_DIR) $(SYMROOT); \
		cp $(LIBCUPSIMAGE) $(SYMROOT); \
	fi

installstatic:
	$(INSTALL_DIR) -m 755 $(LIBDIR)
	$(INSTALL_LIB) -m 755 libcupsimage.a $(LIBDIR)
	$(RANLIB) $(LIBDIR)/libcupsimage.a
	$(CHMOD) 555 $(LIBDIR)/libcupsimage.a

install32bit:
	$(INSTALL_DIR) -m 755 $(LIB32DIR)
	$(INSTALL_LIB) 32bit/libcupsimage.so.2 $(LIB32DIR)/libcupsimage.so.2
	$(LN) libcupsimage.so.2 $(LIB32DIR)/libcupsimage.so

install64bit:
	$(INSTALL_DIR) -m 755 $(LIB64DIR)
	$(INSTALL_LIB) 64bit/libcupsimage.so.2 $(LIB64DIR)/libcupsimage.so.2
	$(LN) libcupsimage.so.2 $(LIB64DIR)/libcupsimage.so


#
# Uninstall all targets...
#

uninstall: $(UNINSTALL32) $(UNINSTALL64)
	$(RM) $(LIBDIR)/libcupsimage.2.dylib
	$(RM) $(LIBDIR)/libcupsimage.a
	$(RM) $(LIBDIR)/libcupsimage.dylib
	$(RM) $(LIBDIR)/libcupsimage_s.a
	$(RM) $(LIBDIR)/libcupsimage.sl
	$(RM) $(LIBDIR)/libcupsimage.sl.2
	$(RM) $(LIBDIR)/libcupsimage.so
	$(RM) $(LIBDIR)/libcupsimage.so.2
	-$(RMDIR) $(LIBDIR)
	$(RM) $(INCLUDEDIR)/cups/image.h
	-$(RMDIR) $(INCLUDEDIR)/cups

uninstall32bit:
	$(RM) $(LIB32DIR)/libcupsimage.so
	$(RM) $(LIB32DIR)/libcupsimage.so.2
	-$(RMDIR) $(LIB32DIR)

uninstall64bit:
	$(RM) $(LIB64DIR)/libcupsimage.so
	$(RM) $(LIB64DIR)/libcupsimage.so.2
	-$(RMDIR) $(LIB64DIR)


#
# libcupsimage.so.2, libcupsimage.sl.2
#

libcupsimage.so.2 libcupsimage.sl.2:	$(IMAGEOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(IMAGEOBJS) $(DSOLIBS) \
		-L../cups $(LINKCUPS) -lm
	$(RM) `basename $@ .2`
	$(LN) $@ `basename $@ .2`


#
# 32bit/libcupsimage.so.2
#

32bit/libcupsimage.so.2:	$(IMAGE32OBJS)
	echo Linking 32-bit $@...
	-mkdir 32bit
	$(DSO) $(ARCH32FLAGS) $(DSO32FLAGS) -o $@ $(IMAGE32OBJS) $(DSOLIBS) \
		-L../cups/32bit $(LINKCUPS) -lm


#
# 64bit/libcupsimage.so.2
#

64bit/libcupsimage.so.2:	$(IMAGE64OBJS)
	echo Linking 64-bit $@...
	-mkdir 64bit
	$(DSO) $(ARCH64FLAGS) $(DSO64FLAGS) -o $@ $(IMAGE64OBJS) $(DSOLIBS) \
		-L../cups/64bit $(LINKCUPS) -lm


#
# libcupsimage.2.dylib
#

libcupsimage.2.dylib:	$(IMAGEOBJS) $(LIBCUPSIMAGEORDER)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ \
		-install_name $(libdir)/$@ \
		-current_version 2.3.0 \
		-compatibility_version 2.0.0 \
		-sectorder __TEXT __text $(LIBCUPSIMAGEORDER) \
		$(IMAGEOBJS) $(DSOLIBS) -L../cups $(LINKCUPS) -lm
	$(RM) libcupsimage.dylib
	$(LN) $@ libcupsimage.dylib


#
# libcupsimage_s.a
#

libcupsimage_s.a:	$(IMAGEOBJS) libcupsimage_s.exp
	echo Linking $@...
	$(DSO) $(DSOFLAGS) -Wl,-berok,-bexport:libcupsimage_s.exp \
		-o libcupsimage_s.o $(IMAGEOBJS) $(DSOLIBS) -lm
	$(RM) $@
	$(AR) $(ARFLAGS) $@ libcupsimage_s.o


#
# libcupsimage.la
#

libcupsimage.la:       $(IMAGEOBJS)
	echo Linking $@...
	$(DSO) $(ARCHFLAGS) $(DSOFLAGS) -o $@ $(IMAGEOBJS:.o=.lo) $(DSOLIBS) \
		-L../cups $(LINKCUPS) \
		-rpath $(LIBDIR) -version-info 2:3


#
# libcupsimage.a
#

libcupsimage.a:	$(IMAGEOBJS)
	echo Archiving $@...
	$(RM) $@
	$(AR) $(ARFLAGS) $@ $(IMAGEOBJS)
	$(RANLIB) $@


#
# Dependencies...
#

include Dependencies


#
# End of "$Id: Makefile 9120 2010-04-23 18:56:34Z mike $".
#
