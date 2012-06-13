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
# Partial makefile for MacOS X/Darwin shared object target

# Useful make commands:
#  make framework	make ghostscript as a MacOS X framework
#  make framework_install install the framework
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
GSSOC_XENAME=$(GS)c$(XE)
GSSOC_XE=$(BINDIR)/$(GSSOC_XENAME)
GSSOC=$(BINDIR)/$(GSSOC_XENAME)

# shared library
SOPREF=lib
#SOSUF=.so
SOSUF=.dylib

GS_SONAME_BASE=$(SOPREF)$(GS)
GS_SONAME=$(GS_SONAME_BASE)$(SOSUF)
GS_SONAME_MAJOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR)$(SOSUF)
GS_SONAME_MAJOR_MINOR=$(GS_SONAME_BASE).$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)$(SOSUF)
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

# Build the small Ghostscript loaders
# it would be nice if we could link to the framework instead

$(GSSOC_XE): $(GS_SO) $(PSSRC)dxmainc.c
	$(GLCC) -g -o $(GSSOC_XE) $(PSSRC)dxmainc.c -L$(BINDIR) -l$(GS)

# ------------------------- Recursive make targets ------------------------- #

# Not that for the framwework build we need to set -install_name
# differently in unix-dll.mak. We could pass it here under LDFLAGS
# but that breaks the .dylib build in favor of the Framework, and
# throws an error linking the example client so the build doesn't
# complete cleanly. There should probably be a separate .dylib target
# if we're going to build them at all. We should also be passing 
# compatibility versions.

SODEFS=LDFLAGS='$(LDFLAGS) $(CFLAGS_SO) -dynamiclib'\
 GS_XE=$(BINDIR)/$(GS_SONAME_MAJOR_MINOR)\
 STDIO_IMPLEMENTATION=c\
 DISPLAY_DEV=$(DD)display.dev\
 BUILDDIRPREFIX=$(BUILDDIRPREFIX)

# This is a bit nasty; because of the directory name rewriting that happens
# on a recursive build, we have to recurse twice before we are sure that
# all the targets are correct.

# Normal shared object
so:
	$(MAKE) so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

so-subtarget:
	$(MAKE) $(SODEFS) CFLAGS='$(CFLAGS_STANDARD) $(CFLAGS_SO) $(GCFLAGS) $(XCFLAGS)' prefix=$(prefix) $(GSSOC) $(GSSOX)

# Debug shared object
sodebug:
	$(MAKE) so-subtarget BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

sodebug-subtarget:
	$(MAKE) $(SODEFS) GENOPT='-DDEBUG' CFLAGS='$(CFLAGS_DEBUG) $(CFLAGS_SO) $(GCFLAGS) $(XCFLAGS)' $(GSSOC) $(GSSOX)

install-so:
	$(MAKE) install-so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

install-so-subtarget: so-subtarget
	-mkdir $(prefix)
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	-mkdir $(bindir)
	-mkdir $(libdir)
	$(INSTALL_PROGRAM) $(GSSOC) $(bindir)/$(GSSOC_XENAME)
	$(INSTALL_PROGRAM) $(GSSOX) $(bindir)/$(GSSOX_XENAME)
	$(INSTALL_PROGRAM) $(BINDIR)/$(GS_SONAME_MAJOR_MINOR) $(libdir)/$(GS_SONAME_MAJOR_MINOR)
	$(RM_) $(libdir)/$(GS_SONAME)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(libdir)/$(GS_SONAME)
	$(RM_) $(libdir)/$(GS_SONAME_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(libdir)/$(GS_SONAME_MAJOR)

soinstall:
	$(MAKE) soinstall-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

soinstall-subtarget: install-so-subtarget install-scripts install-data

GS_FRAMEWORK=$(BINDIR)/$(FRAMEWORK_NAME)$(FRAMEWORK_EXT)

framework: so lib/Info-macos.plist
	rm -rf $(GS_FRAMEWORK)
	-mkdir $(GS_FRAMEWORK)
	-mkdir $(GS_FRAMEWORK)/Versions
	-mkdir $(GS_FRAMEWORK)/Versions/$(GS_DOT_VERSION)
	-mkdir $(GS_FRAMEWORK)/Versions/$(GS_DOT_VERSION)/Headers
	-mkdir $(GS_FRAMEWORK)/Versions/$(GS_DOT_VERSION)/Resources
	(cd $(GS_FRAMEWORK)/Versions; ln -s $(GS_DOT_VERSION) Current)
	(cd $(GS_FRAMEWORK); \
	ln -s Versions/Current/Headers . ;\
	ln -s Versions/Current/Resources ;\
	ln -s Versions/Current/man . ;\
	ln -s Versions/Current/doc . ;\
	ln -s Versions/Current/$(FRAMEWORK_NAME) . )
	pwd
	cp psi/iapi.h psi/ierrors.h base/gdevdsp.h $(GS_FRAMEWORK)/Headers/
	cp lib/Info-macos.plist $(GS_FRAMEWORK)/Resources/
	cp -r lib $(GS_FRAMEWORK)/Resources/
	cp $(BINDIR)/$(GS_SONAME_MAJOR_MINOR) $(GS_FRAMEWORK)/Versions/Current/$(FRAMEWORK_NAME)
	cp -r man $(GS_FRAMEWORK)/Versions/Current
	cp -r doc $(GS_FRAMEWORK)/Versions/Current

framework_install : framework
	rm -rf $(prefix)
	cp -r $(GS_FRAMEWORK) $(prefix)

soclean:
	$(MAKE) soclean-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

soclean-subtarget:
	$(MAKE) $(SODEFS) clean
	$(RM_) $(BINDIR)/$(GS_SONAME)
	$(RM_) $(BINDIR)/$(GS_SONAME_MAJOR)
	$(RM_) $(GSSOC)
	$(RM_) $(GSSOX)

# End of unix-dll.mak
