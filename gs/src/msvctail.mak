#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
# This software is licensed to a single customer by Artifex Software Inc.
# under the terms of a specific OEM agreement.

# msvctail.mak
# Common tail section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections


# -------------------------- Auxiliary programs --------------------------- #

ccf32.tr: $(MAKEFILE)
	echo $(GENOPT) /I$(INCDIR) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ > ccf32.tr

# Don't create genarch if it's not needed
!ifdef GENARCH_XE
$(GENARCH_XE): genarch.c $(stdpre_h) $(iref_h) ccf32.tr
	$(CCAUX_SETUP)
	$(CCAUX) @ccf32.tr genarch.c $(CCAUX_TAIL)
!endif

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

LIBCTR=libc32.tr

$(LIBCTR): $(MAKEFILE) $(ECHOGS_XE)
        echo $(LIBDIR)\shell32.lib >$(LIBCTR)
        echo $(LIBDIR)\comdlg32.lib >>$(LIBCTR)
        echo $(LIBDIR)\gdi32.lib >>$(LIBCTR)
        echo $(LIBDIR)\user32.lib >>$(LIBCTR)
        echo $(LIBDIR)\winspool.lib >>$(LIBCTR)

# end of msvctail.mak
