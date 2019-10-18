# Copyright (C) 2001-2019 Artifex Software, Inc.
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
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
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

UNIX_DLL_MAK=$(GLSRC)unix-dll.mak $(TOP_MAKEFILES)

# Location for building shared object
SODIRPREFIX=so
SODEBUGDIRPREFIX=sodebug

# ------------------- Ghostscript shared object --------------------------- #

# Shared object names

# simple loader (no support for display device)
GSSOC_XENAME=$(GS_SO_BASE)c$(XE)
GSSOC_XE=$(BINDIR)/$(GSSOC_XENAME)
GSSOC=$(BINDIR)/$(GSSOC_XENAME)

PCLSOC_XENAME=$(PCL_SO_BASE)c$(XE)
PCLSOC_XE=$(BINDIR)/$(PCLSOC_XENAME)
PCLSOC=$(BINDIR)/$(PCLSOC_XENAME)

XPSSOC_XENAME=$(XPS_SO_BASE)c$(XE)
XPSSOC_XE=$(BINDIR)/$(XPSSOC_XENAME)
XPSSOC=$(BINDIR)/$(XPSSOC_XENAME)

GPDLSOC_XENAME=$(GPDL_SO_BASE)c$(XE)
GPDLSOC_XE=$(BINDIR)/$(GPDLSOC_XENAME)
GPDLSOC=$(BINDIR)/$(GPDLSOC_XENAME)

# loader suporting display device using Gtk+
GSSOX_XENAME=$(GS_SO_BASE)x$(XE)
GSSOX_XE=$(BINDIR)/$(GSSOX_XENAME)
GSSOX=$(BINDIR)/$(GSSOX_XENAME)

# shared library
GS_SONAME_BASE=lib$(GS_SO_BASE)
PCL_SONAME_BASE=lib$(PCL_SO_BASE)
XPS_SONAME_BASE=lib$(XPS_SO_BASE)
GPDL_SONAME_BASE=lib$(GPDL_SO_BASE)

# GNU/Linux
GS_SOEXT=$(SO_LIB_EXT)
GS_DLLEXT=$(DLL_EXT)

#GS_SONAME=$(GS_SONAME_BASE)$(GS_SOEXT)$(GS_DLLEXT)
#GS_SONAME_MAJOR=$(GS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(GS_DLLEXT)
#GS_SONAME_MAJOR_MINOR=$(GS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MINOR)$(GS_DLLEXT)

#PCL_SONAME=$(PCL_SONAME_BASE)$(GS_SOEXT)$(GS_DLLEXT)
#PCL_SONAME_MAJOR=$(PCL_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(GS_DLLEXT)
#PCL_SONAME_MAJOR_MINOR=$(PCL_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MINOR)$(GS_DLLEXT)

#XPS_SONAME=$(XPS_SONAME_BASE)$(GS_SOEXT)$(GS_DLLEXT)
#XPS_SONAME_MAJOR=$(XPS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(GS_DLLEXT)
#XPS_SONAME_MAJOR_MINOR=$(XPS_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MINOR)$(GS_DLLEXT)

#GPDL_SONAME=$(GPDL_SONAME_BASE)$(GS_SOEXT)$(GS_DLLEXT)
#GPDL_SONAME_MAJOR=$(GPDL_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(GS_DLLEXT)
#GPDL_SONAME_MAJOR_MINOR=$(GPDL_SONAME_BASE)$(GS_SOEXT)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MAJOR)$(SO_LIB_VERSION_SEPARATOR)$(GS_VERSION_MINOR)$(GS_DLLEXT)

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

PCL_SO=$(BINDIR)/$(PCL_SONAME)
PCL_SO_MAJOR=$(BINDIR)/$(PCL_SONAME_MAJOR)
PCL_SO_MAJOR_MINOR=$(BINDIR)/$(PCL_SONAME_MAJOR_MINOR)

XPS_SO=$(BINDIR)/$(XPS_SONAME)
XPS_SO_MAJOR=$(BINDIR)/$(XPS_SONAME_MAJOR)
XPS_SO_MAJOR_MINOR=$(BINDIR)/$(XPS_SONAME_MAJOR_MINOR)

GPDL_SO=$(BINDIR)/$(GPDL_SONAME)
GPDL_SO_MAJOR=$(BINDIR)/$(GPDL_SONAME_MAJOR)
GPDL_SO_MAJOR_MINOR=$(BINDIR)/$(GPDL_SONAME_MAJOR_MINOR)

# Shared object is built by redefining GS_XE in a recursive make.

# Create symbolic links to the Ghostscript interpreter library

$(GS_SO): $(GS_SO_MAJOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(GS_SO)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO)

$(GS_SO_MAJOR): $(GS_SO_MAJOR_MINOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(GS_SO_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO_MAJOR)

$(PCL_SO): $(PCL_SO_MAJOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(PCL_SO)
	ln -s $(PCL_SONAME_MAJOR_MINOR) $(PCL_SO)

$(PCL_SO_MAJOR): $(PCL_SO_MAJOR_MINOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(PCL_SO_MAJOR)
	ln -s $(PCL_SONAME_MAJOR_MINOR) $(PCL_SO_MAJOR)

$(XPS_SO): $(XPS_SO_MAJOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(XPS_SO)
	ln -s $(XPS_SONAME_MAJOR_MINOR) $(XPS_SO)

$(XPS_SO_MAJOR): $(XPS_SO_MAJOR_MINOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(XPS_SO_MAJOR)
	ln -s $(XPS_SONAME_MAJOR_MINOR) $(XPS_SO_MAJOR)

$(GPDL_SO): $(GPDL_SO_MAJOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(GPDL_SO)
	ln -s $(GPDL_SONAME_MAJOR_MINOR) $(GPDL_SO)

$(GPDL_SO_MAJOR): $(GPDL_SO_MAJOR_MINOR) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(RM_) $(GPDL_SO_MAJOR)
	ln -s $(GPDL_SONAME_MAJOR_MINOR) $(GPDL_SO_MAJOR)


gs-so-links-subtarget: $(GS_SO) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(NO_OP)

gpcl6-so-links-subtarget: $(PCL_SO) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(NO_OP)

gxps-so-links-subtarget: $(XPS_SO) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(NO_OP)

gpdl-so-links-subtarget: $(GPDL_SO) $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(NO_OP)

# dummy for when only GS source is available
-so-links-subtarget: $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(NO_OP)

# Build the small Ghostscript loaders, with Gtk+ and without
$(GSSOC_XE): gs-so-links-subtarget $(PSSRC)dxmainc.c $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(GLCC) $(GLO_)dxmainc.$(OBJ) $(C_) $(PSSRC)dxmainc.c
	$(GLCC) -L$(BINDIR) $(LDFLAGS) $(O_) $(GSSOC_XE) $(GLOBJ)dxmainc.$(OBJ) -l$(GS_SO_BASE)

$(GSSOX_XE): gs-so-links-subtarget $(PSSRC)$(SOC_LOADER).c $(UNIX_DLL_MAK) $(MAKEDIRS)
	$(GLCC) $(SOC_CFLAGS) $(GLO_)$(SOC_LOADER).$(OBJ) $(C_) $(PSSRC)$(SOC_LOADER).c
	$(GLCC) -L$(BINDIR) $(LDFLAGS) $(O_) $(GSSOX_XE) $(GLOBJ)$(SOC_LOADER).$(OBJ) -l$(GS_SO_BASE) $(SOC_LIBS)

$(PCLSOC_XE): gpcl6-so-links-subtarget $(UNIX_DLL_MAK) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) $(MAKEDIRS)
	$(GLCC) -L$(BINDIR) $(LDFLAGS) $(O_) $(PCLSOC_XE) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) -l$(PCL_SO_BASE)

$(XPSSOC_XE): gxps-so-links-subtarget $(UNIX_DLL_MAK) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) $(MAKEDIRS)
	$(GLCC) -L$(BINDIR) $(LDFLAGS) $(O_) $(XPSSOC_XE) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) -l$(XPS_SO_BASE)

$(GPDLSOC_XE): gpdl-so-links-subtarget $(UNIX_DLL_MAK) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) $(MAKEDIRS)
	$(GLCC) -L$(BINDIR) $(LDFLAGS) $(O_) $(GPDLSOC_XE) $(PLOBJ)$(REALMAIN_SRC).$(OBJ) -l$(GPDL_SO_BASE)

gpcl6-so-loader: $(PCLSOC_XE)
	$(NO_OP)

gxps-so-loader: $(XPSSOC_XE)
	$(NO_OP)

gpdl-so-loader: $(GPDLSOC_XE)
	$(NO_OP)

# dummy for when only GS source is available
-so-loader:
	$(NO_OP)


gs-so-strip:
	$(STRIP_XE) $(STRIP_XE_OPTS) $(GS_XE)

gpcl6-so-strip:
	$(STRIP_XE) $(STRIP_XE_OPTS) $(GPCL_XE)

gxps-so-strip:
	$(STRIP_XE) $(STRIP_XE_OPTS) $(GXPS_XE)

gpdl-so-strip:
	$(STRIP_XE)$(STRIP_XE_OPTS)  $(GPDL_XE)

# dummy for when only GS source is available
-so-strip:
	$(NO_OP)


# ------------------------- Recursive make targets ------------------------- #

SODEFS=\
 GS_DOT_O= \
 REALMAIN_OBJ= \
 GS_XE=$(BINDIR)/$(GS_SONAME_MAJOR_MINOR) \
 GPCL_XE=$(BINDIR)/$(PCL_SONAME_MAJOR_MINOR) \
 GXPS_XE=$(BINDIR)/$(XPS_SONAME_MAJOR_MINOR) \
 GPDL_XE=$(BINDIR)/$(GPDL_SONAME_MAJOR_MINOR) \
 DISPLAY_DEV=$(DD)display.dev \
 BUILDDIRPREFIX=$(BUILDDIRPREFIX)

SODEFS_FINAL=\
 DISPLAY_DEV=$(DD)display.dev\
 BUILDDIRPREFIX=$(BUILDDIRPREFIX)

# This is a bit nasty; because of the directory name rewriting that happens
# on a recursive build, we have to recurse twice before we are sure that
# all the targets are correct.

# Normal shared object
so:
	@if test -z "$(MAKE) $(SUB_MAKE_OPTION)" -o -z "`$(MAKE) $(SUB_MAKE_OPTION) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SUB_MAKE_OPTION) so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

so-only:
	@if test -z "$(MAKE) $(SUB_MAKE_OPTION)" -o -z "`$(MAKE) $(SUB_MAKE_OPTION) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SUB_MAKE_OPTION) so-only-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)
	$(MAKE) $(SUB_MAKE_OPTION) gs-so-links-subtarget \
                                   $(PCL_TARGET)-so-links-subtarget \
                                   $(XPS_TARGET)-so-links-subtarget \
                                   $(GPDL_TARGET)-so-links-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)
	

so-only-stripped:
	$(MAKE) $(SUB_MAKE_OPTION) so-only-stripped-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)
	$(MAKE) $(SUB_MAKE_OPTION) gs-so-links-subtarget \
                                   $(PCL_TARGET)-so-links-subtarget \
                                   $(XPS_TARGET)-so-links-subtarget \
                                   $(GPDL_TARGET)-so-links-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

# Debug shared object
so-onlydebug:
	@if test -z "$(MAKE) $(SUB_MAKE_OPTION)" -o -z "`$(MAKE) $(SUB_MAKE_OPTION) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SUB_MAKE_OPTION) so-only-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

sodebug:
	@if test -z "$(MAKE) $(SUB_MAKE_OPTION)" -o -z "`$(MAKE) $(SUB_MAKE_OPTION) --version 2>&1 | grep GNU`";\
	  then echo "Warning: this target requires gmake";\
	fi
	$(MAKE) $(SUB_MAKE_OPTION) CFLAGS_STANDARD="$(CFLAGS_DEBUG)" so-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

so-only-subtarget:
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 directories
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 $(AUXDIR)/echogs$(XEAUX) $(AUXDIR)/genarch$(XEAUX)
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) GENOPT='$(GENOPT)' GS_LDFLAGS='$(LDFLAGS) $(GS_LDFLAGS_SO)'\
         PCL_LDFLAGS='$(LDFLAGS) $(PCL_LDFLAGS_SO)' XPS_LDFLAGS='$(LDFLAGS) $(XPS_LDFLAGS_SO)' \
         PDL_LDFLAGS='$(LDFLAGS) $(PDL_LDFLAGS_SO)' CFLAGS='$(CFLAGS_STANDARD) $(CFLAGS_SO) \
         $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)

so-only-stripped-subtarget: so-only-subtarget
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) gs-so-strip $(PCL_TARGET)-so-strip $(XPS_TARGET)-so-strip $(GPDL_TARGET)-so-strip

so-subtarget: so-only-subtarget
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS_FINAL) GENOPT='$(GENOPT)' LDFLAGS='$(LDFLAGS)'\
	 CFLAGS='$(CFLAGS_STANDARD) $(GCFLAGS) $(AC_CFLAGS) $(XCFLAGS)' prefix=$(prefix)\
	 $(GSSOC_XE) $(GSSOX_XE) $(PCL_TARGET)-so-loader $(XPS_TARGET)-so-loader $(GPDL_TARGET)-so-loader

install-so:
	$(MAKE) $(SUB_MAKE_OPTION) install-so-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

install-sodebug:
	$(MAKE) $(SUB_MAKE_OPTION) install-so-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

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
	$(INSTALL_DATA) $(GLSRC)gserrors.h $(DESTDIR)$(gsincludedir)gserrors.h
	$(INSTALL_DATA) $(DEVSRC)gdevdsp.h $(DESTDIR)$(gsincludedir)gdevdsp.h

soinstall:
	$(MAKE) $(SUB_MAKE_OPTION) soinstall-subtarget BUILDDIRPREFIX=$(SODIRPREFIX)

sodebuginstall:
	$(MAKE) $(SUB_MAKE_OPTION) soinstall-subtarget GENOPT='-DDEBUG' BUILDDIRPREFIX=$(SODEBUGDIRPREFIX)

soinstall-subtarget: install-so install-scripts install-data $(INSTALL_SHARED) $(INSTALL_CONTRIB)

# Clean targets
soclean:
	$(MAKE) $(SUB_MAKE_OPTION) BUILDDIRPREFIX=$(SODIRPREFIX) clean-so-subtarget

clean-so-subtarget:
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) clean-so-subsubtarget

clean-so-subsubtarget: clean
	$(RM_) $(BINDIR)/$(GS_SONAME)
	$(RM_) $(BINDIR)/$(GS_SONAME_MAJOR)
	$(RM_) $(GSSOC)
	$(RM_) $(GSSOX)
	$(RMN_) -r $(BINDIR) $(GLGENDIR) $(GLOBJDIR) $(PSGENDIR) $(PSOBJDIR)

sodebugclean:
	$(MAKE) $(SUB_MAKE_OPTION)  BUILDDIRPREFIX=$(SODEBUGDIRPREFIX) clean-sodebug-subtarget

clean-sodebug-subtarget:
	$(MAKE) $(SUB_MAKE_OPTION) $(SODEFS) clean-so-subsubtarget

# End of unix-dll.mak
