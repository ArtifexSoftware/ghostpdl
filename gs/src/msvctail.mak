#    Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
# 
# This file is part of Aladdin Ghostscript.
# 
# Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
# or distributor accepts any responsibility for the consequences of using it,
# or for whether it serves any particular purpose or works at all, unless he
# or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
# License (the "License") for full details.
# 
# Every copy of Aladdin Ghostscript must include a copy of the License,
# normally in a plain ASCII text file named PUBLIC.  The License grants you
# the right to copy, modify and redistribute Aladdin Ghostscript, but only
# under certain conditions described in the License.  Among other things, the
# License requires that the copyright notice and this notice be preserved on
# all copies.

# 
# Common tail section for Microsoft Visual C++ 4.x/5.x,
# Windows NT or Windows 95 platform.
# Created 1997-05-22 by L. Peter Deutsch from msvc4/5 makefiles.
# edited 1997-06-xx by JD to factor out interpreter-specific sections


# -------------------------- Auxiliary programs --------------------------- #

ccf32.tr: $(MAKEFILE)
	echo $(GENOPT) /I$(INCDIR) -DCHECK_INTERRUPTS -D_Windows -D__WIN32__ > ccf32.tr

# Don't create genarch if it's not needed
!ifdef GENARCH_XE
$(GENARCH_XE): $(GSLRC)genarch.c $(stdpre_h) $(iref_h) ccf32.tr
	$(CCAUX_SETUP)
	$(CCAUX) @ccf32.tr $(GLSRC)genarch.c $(CCAUX_TAIL)
!endif

# -------------------------------- Library -------------------------------- #

# See winlib.mak

# ----------------------------- Main program ------------------------------ #

LIBCTR=libc32.tr

$(LIBCTR): $(MAKEFILE)
        echo $(LIBDIR)\shell32.lib >$(LIBCTR)
        echo $(LIBDIR)\comdlg32.lib >>$(LIBCTR)
        echo $(LIBDIR)\gdi32.lib >>$(LIBCTR)
        echo $(LIBDIR)\user32.lib >>$(LIBCTR)
        echo $(LIBDIR)\winspool.lib >>$(LIBCTR)
	echo $(LIBDIR)\advapi32.lib >>$(LIBCTR)

# end of msvctail.mak
