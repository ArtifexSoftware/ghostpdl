#    Copyright (C) 2000, Aladdin Enterprises.  All rights reserved.
# 
# This file is part of AFPL Ghostscript.
# 
# AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
# distributor accepts any responsibility for the consequences of using it, or
# for whether it serves any particular purpose or works at all, unless he or
# she says so in writing.  Refer to the Aladdin Free Public License (the
# "License") for full details.
# 
# Every copy of AFPL Ghostscript must include a copy of the License, normally
# in a plain ASCII text file named PUBLIC.  The License grants you the right
# to copy, modify and redistribute AFPL Ghostscript, but only under certain
# conditions described in the License.  Among other things, the License
# requires that the copyright notice and this notice be preserved on all
# copies.

# $Id$
# Partial makefile for Unix DLL (shared object) target


GSSO_XE=$(BINDIR)$(D)$(GS)_so$(XE)
GS_SONAME=lib$(GS).so
GS_SONAME_MAJOR=$(GS_SONAME).$(GS_VERSION_MAJOR)
GS_SONAME_MAJOR_MINOR= $(GS_SONAME).$(GS_VERSION_MAJOR).$(GS_VERSION_MINOR)
GS_SO=$(BINDIR)/$(GS_SONAME)
GS_SO_MAJOR=$(GS_SO).$(GS_VERSION_MAJOR)
GS_SO_MAJOR_MINOR=$(GS_SO_MAJOR).$(GS_VERSION_MINOR)


# --------------------------- Executable loader --------------------------- #

# build the small Ghostscript loader

$(GSSO_XE): $(GS_SO) $(GLSRC)dxmain.c
	$(GLCC) -g `gtk-config --cflags` -o $(GSSO_XE) $(GLSRC)dxmain.c -L$(BINDIR) -lgs `gtk-config --libs`
	

# ------------------- Ghostscript shared object --------------------------- #

# build the Ghostscript DLL (shared object)

$(GS_SO): $(GS_SO_MAJOR)
	-rm $(GS_SO)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO)

$(GS_SO_MAJOR): $(GS_SO_MAJOR_MINOR)
	-rm $(GS_SO_MAJOR)
	ln -s $(GS_SONAME_MAJOR_MINOR) $(GS_SO_MAJOR)

$(GS_SO_MAJOR_MINOR): $(ld_tr) $(ECHOGS_XE) $(XE_ALL)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(LDFLAGS) -shared -Wl,-soname,$(GS_SONAME_MAJOR) -o $(GS_SO_MAJOR_MINOR)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(PSOBJ)gs.$(OBJ) -s
	cat $(ld_tr) >>$(ldt_tr)
	$(ECHOGS_XE) -a $(ldt_tr) -s - $(EXTRALIBS) $(STDLIBS)
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; \
	XCFLAGS= XINCLUDE= XLDFLAGS= XLIBDIRS= XLIBS= \
	FEATURE_DEVS= DEVICE_DEVS= DEVICE_DEVS1= DEVICE_DEVS2= DEVICE_DEVS3= \
	DEVICE_DEVS4= DEVICE_DEVS5= DEVICE_DEVS6= DEVICE_DEVS7= DEVICE_DEVS8= \
	DEVICE_DEVS9= DEVICE_DEVS10= DEVICE_DEVS11= DEVICE_DEVS12= \
	DEVICE_DEVS13= DEVICE_DEVS14= DEVICE_DEVS15= DEVICE_DEVS16= \
	DEVICE_DEVS17= DEVICE_DEVS18= DEVICE_DEVS19= DEVICE_DEVS20= \
	$(SH) <$(ldt_tr)


# Define a rule for building shared object configurations.
DLLDEFS=CFLAGS='$(CFLAGS_SO) $(GCFLAGS) $(XDFLAGS)' STDIO_IMPLEMENTATION=c DISPLAY_DEV=$(DD)display.dev

so: dll

dll:
	$(MAKE) $(DLLDEFS) $(GSSO_XE)

dllclean: clean
	-rm $(GS_SO)
	-rm $(GS_SO_MAJOR)
	-rm $(GS_SO_MAJOR_MINOR)
	-rm $(GSSO_XE)
