#    Copyright (C) 1997, 2000 Aladdin Enterprises.  All rights reserved.
# 
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# $RCSfile$ $Revision$
# Common tail section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections
# edited 2000-06-05 by lpd to handle empty INCDIR specially.


# -------------------------- Auxiliary programs --------------------------- #

!if "$(INCDIR)"==""
IINCDIR=
!else
IINCDIR=/I$(INCDIR)
!endif

# This also creates the subdirectories since this (hopefully) will be the
# first need. Too bad nmake doesn't have .BEFORE symbolic target.
$(GLGENDIR)\ccf32.tr: $(TOP_MAKEFILES)
	-mkdir $(GLGENDIR)
	-mkdir $(BINDIR)
	echo $(GENOPT) $(IINCDIR) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ > $(GLGENDIR)\ccf32.tr

$(ECHOGS_XE): $(GLSRC)echogs.c
	$(CCAUX_SETUP)
	$(CCAUX) $(GLSRC)echogs.c /Fo$(GLOBJ)echogs.obj /Fe$(ECHOGS_XE) $(CCAUX_TAIL)

# Don't create genarch if it's not needed
!ifdef GENARCH_XE
$(GENARCH_XE): $(GLSRC)genarch.c $(GENARCH_DEPS) $(GLGENDIR)\ccf32.tr
	$(CCAUX_SETUP)
	$(CCAUX) @$(GLGENDIR)\ccf32.tr /Fo$(GLOBJ)genarch.obj /Fe$(GENARCH_XE) $(GLSRC)genarch.c $(CCAUX_TAIL)
!endif

$(GENCONF_XE): $(GLSRC)genconf.c $(GENCONF_DEPS)
	$(CCAUX_SETUP)
	$(CCAUX) $(GLSRC)genconf.c /Fo$(GLOBJ)genconf.obj /Fe$(GENCONF_XE) $(CCAUX_TAIL)

$(GENDEV_XE): $(GLSRC)gendev.c $(GENDEV_DEPS)
	$(CCAUX_SETUP)
	$(CCAUX) $(GLSRC)gendev.c /Fo$(GLOBJ)gendev.obj /Fe$(GENDEV_XE) $(CCAUX_TAIL)

$(GENHT_XE): $(GLSRC)genht.c $(GENHT_DEPS)
	$(CCAUX_SETUP)
	$(CCAUX) $(GENHT_CFLAGS) $(GLSRC)genht.c /Fo$(GLOBJ)genht.obj /Fe$(GENHT_XE) $(CCAUX_TAIL)

# PSSRC and PSOBJ aren't defined yet, so we spell out the definitions.
$(GENINIT_XE): $(PSSRCDIR)$(D)geninit.c $(GENINIT_DEPS)
	$(CCAUX_SETUP)
	$(CCAUX) $(PSSRCDIR)$(D)geninit.c /Fo$(PSOBJDIR)$(D)geninit.obj /Fe$(GENINIT_XE) $(CCAUX_TAIL)

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

LIBCTR=$(GLGEN)libc32.tr

$(LIBCTR): $(TOP_MAKEFILES)
        echo $(LIBD)shell32.lib >$(LIBCTR)
        echo $(LIBD)comdlg32.lib >>$(LIBCTR)
        echo $(LIBD)gdi32.lib >>$(LIBCTR)
        echo $(LIBD)user32.lib >>$(LIBCTR)
        echo $(LIBD)winspool.lib >>$(LIBCTR)
	echo $(LIBD)advapi32.lib >>$(LIBCTR)

# end of msvctail.mak
