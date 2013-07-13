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
# Partial makefile for Unix shared library target

# Useful make commands:
#  make so		make ghostscript as a shared object
#  make sodebug		make debug ghostscript as a shared object
#  make soinstall	install shared object ghostscript
#  make soclean		remove build files
#
# If you want to test the executable without installing:
#  export LD_LIBRARY_PATH=/insert-path-here/sobin
#  export GS_LIB=/insert-path-here/lib

# Location for building shared object
SODIRPREFIX=so
SODEBUGDIRPREFIX=sodebug

# ------------------- Ghostscript shared object --------------------------- #

# Shared object names

# simple loader (no support for display device)
GSSOC_XENAME=$(GS_SO_BASE)c$(XE)
GSSOC_XE=$(BINDIR)/$(GSSOC_XENAME)
GSSOC=$(BINDIR)/$(GSSOC_XENAME)

# loader suporting display device using Gtk+
GSSOX_XENAME=$(GS_SO_BASE)x$(XE)
GSSOX_XE=$(BINDIR)/$(GSSOX_XENAME)
GSSOX=$(BINDIR)/$(GSSOX_XENAME)

# shared library
GS_SONAME_BASE=lib$(GS_SO_BASE)

# GNU/Linux
GS_SOEXT=$(SO_LIB_EXT)
GS_DLLEXT=$(DLL_EXT)

GS_SONAME=$(GS_SONAME_BASE)$(GS_SOEXT)$(GS_DLLEXT)

GS_SONAME_MAJOR=$(GS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(GS_DLLEXT)

GS_SONAME_MAJOR_MINOR=$(GS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MINOR)$(GS_DLLEXT)

#LDFLAGS_SO=-shared -Wl,-soname=$(GS_SONAME_MAJOR)

# NOTE: the value of LD_SET_DT_SONAME for, for example, Solaris ld, must contain the
# trailing space to separation it from the value of the option. For GNU ld and
# similar linkers it must containt the trailing "=" 
# LDFLAGS_SO=-shared -Wl,$(LD_SET_DT_SONAME)$(LDFLAGS_SO_PREFIX)$(GS_SONAME_MAJOR)


# MacOS X
#GS_SOEXT=dylib
#GS_SONAME=$(GS_SONAME_BASE).$(GS_SOEXT)
#GS_SONAME_MAJOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR).$(GS_SOEXT)
#GS_SONAME_MAJOR_MINOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR).$(GS_SOEXT)
#LDFLAGS_SO=-dynamiclib -flat_namespace
#LDFLAGS_SO_MAC=-dynamiclib -install_name $(GS_SONAME_MAJOR_MINOR)
#LDFLAGS_SO=-dynamiclib -install_name $(FRAMEWORK_NAME)

GS_SO=$(BINDIR)/$(GS_SONAME)
GS_SO_MAJOR=$(BINDIR)/$(GS_SONAME_MAJOR) 
GS_SO_MAJOR_MINOR=$(BINDIR)/$(GS_SONAME_MAJOR_MINOR)

# Shared object is built by redefining GS_XE in a recursive make.

# Create symbolic links to the Ghostscript interpreter library

$(GS_SO): $(GS_SO_MAJOR)
	$(RM_) $(GS_SO)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO)

$(GS_SO_MAJOR): $(GS_SO_MAJOR_MINOR)
	$(RM_) $(GS_SO_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO_MAJOR)

# Build the small Ghostscript loaders, with Gtk+ and without
$(GSSOC_XE): $(GS_SO) $(PSSRC)$(SOC_LOADER)
	$(GLCC) -g -o $(GSSOC_XE) $(PSSRC)dxmainc.c \
	-L$(BINDIR) -l$(GS_SO_BASE)

$(GSSOX_XE): $(GS_SO) $(PSSRC)$(SOC_LOADER)
	$(GLCC) -g $(SOC_CFLAGS) -o $(GSSOX_XE) $(PSSRC)$(SOC_LOADER) \
	-L$(BINDIR) -l$(GS_SO_BASE) $(SOC_LIBS)

# ------------------------- Recursive make targets ------------------------- #

SODEFS=\
 GS_XE=$(BINDIR)/$(GS_SONAME_MAJOR_MINOR)\
 DISPLAY_DEV=$(DD)display.dev\
 STDIO_IMPLEMENTATION=c\
 BUILDDIRPREFIX=$(BUILDDIRPREFIX)

SODEFS_FINAL=\
 DISPLAY_DEV=$(DD)display.dev\
 STDIO_IMPLEMENTATION=c\
 BUILDDIRPREFIX=$(BUILDDIRPREFIX)

# This is a bit nasty; because of the directory name rewriting that happens
# on a recursive build, we have to recurse twice before we are sure that
# all the targets are correct.

# Normal shared object
so:
	@if test -z "$(MAKE)" -o -z "`$(MAKE) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

# Debug shared object
sodebug:
	@if test -z "$(MAKE)" -o -z "`$(MAKE) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) so-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

so-subtarget:
	$(MAKE) $(SODEFS) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 directories
	$(MAKE) $(SODEFS) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 $(AUXDIR)/echogs$(XEAUX) $(AUXDIR)/genarch$(XEAUX)
	$(MAKE) $(SODEFS) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS) $(LDFLAGS_SO)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(CFLAGS_SO) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)'\
	 prefix=$(prefix)
	$(MAKE) $(SODEFS_FINAL) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 $(GSSOC_XE) $(GSSOX_XE)

install-so:
	$(MAKE) install-so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

install-sodebug:
	$(MAKE) install-so-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

install-so-subtarget: so-subtarget
	-mkdir -p $(DESTDIR)$(prefix)
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)
	-mkdir -p $(DESTDIR)$(bindir)
	-mkdir -p $(DESTDIR)$(libdir)
	-mkdir -p $(DESTDIR)$(gsincludedir)
	$(INSTALL_PROGRAM) $(GSSOC) $(DESTDIR)$(bindir)/$(GSSOC_XENAME)
	$(INSTALL_PROGRAM) $(GSSOX) $(DESTDIR)$(bindir)/$(GSSOX_XENAME)
	$(INSTALL_PROGRAM) $(BINDIR)/$(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR_MINOR)
	$(RM_) $(DESTDIR)$(libdir)/$(GS_SONAME)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME)
	$(RM_) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(DESTDIR)$(libdir)/$(GS_SONAME_MAJOR)
	$(INSTALL_DATA) $(PSSRC)iapi.h $(DESTDIR)$(gsincludedir)iapi.h
	$(INSTALL_DATA) $(PSSRC)ierrors.h $(DESTDIR)$(gsincludedir)ierrors.h
	$(INSTALL_DATA) $(DEVSRC)gdevdsp.h $(DESTDIR)$(gsincludedir)gdevdsp.h

soinstall:
	$(MAKE) soinstall-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

sodebuginstall:
	$(MAKE) soinstall-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

soinstall-subtarget: install-so install-scripts install-data $(INSTALL_SHARED) $(INSTALL_CONTRIB)

# Clean targets
soclean:
	$(MAKE) BUILDDIRPREFIX=$(SODIRPREFIX) clean-so-subtarget

clean-so-subtarget:
	$(MAKE) $(SODEFS) clean-so-subsubtarget

clean-so-subsubtarget: clean
	$(RM_) $(BINDIR)/$(GS_SONAME)
	$(RM_) $(BINDIR)/$(GS_SONAME_MAJOR)
	$(RM_) $(GSSOC)
	$(RM_) $(GSSOX)
	$(RMN_) -r $(BINDIR) $(GLGENDIR) $(GLOBJDIR) $(PSGENDIR) $(PSOBJDIR)

sodebugclean:
	$(MAKE)  BUILDDIRPREFIX=$(SODEBUGDIRPREFIX) clean-sodebug-subtarget

clean-sodebug-subtarget:
	$(MAKE) $(SODEFS) clean-so-subsubtarget

# End of unix-dll.mak
