#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
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

# $Id$
# Common interpreter makefile section for 32-bit MS Windows.

# This makefile must be acceptable to Microsoft Visual C++, Watcom C++,
# and Borland C++.  For this reason, the only conditional directives
# allowed are !if[n]def, !else, and !endif.


# Include the generic makefile.
!include $(PSSRCDIR)\int.mak

# Define the C++ compiler invocation for library modules.
GLCPP=$(CPP) $(CO) $(I_)$(GLI_)$(_I)

# Define the compilation rule for Windows interpreter code.
# This requires PS*_ to be defined, so it has to come after int.mak.
PSCCWIN=$(CC_WX) $(CCWINFLAGS) $(I_)$(PSI_)$(_I) $(PSF_)

# ----------------------------- Main program ------------------------------ #

# We would like to put the icons into GLGENDIR, but that would require
# fiddling with the .rc files in ways I don't understand.

ICONS=gsgraph.ico gstext.ico

GS_ALL=$(INT_ALL) $(INTASM)\
  $(LIB_ALL) $(LIBCTR) lib.tr $(ld_tr) $(GSDLL_OBJ).res $(GSDLL).def $(ICONS)

dwdll_h=$(GLSRC)dwdll.h
dwimg_h=$(GLSRC)dwimg.h
dwmain_h=$(GLSRC)dwmain.h
dwtext_h=$(GLSRC)dwtext.h

# Make the icons from their text form.

gsgraph.ico: $(GLSRC)gsgraph.icx $(ECHOGS_XE)
	$(ECHOGS_XE) -wb gsgraph.ico -n -X -r $(GLSRC)gsgraph.icx

gstext.ico: $(GLSRC)gstext.icx $(ECHOGS_XE)
	$(ECHOGS_XE) -wb gstext.ico -n -X -r $(GLSRC)gstext.icx

# resources for short EXE loader (no dialogs)
$(GS_OBJ).res: $(GLSRC)dwmain.rc $(dwmain_h) $(ICONS)
	$(RCOMP) -i$(INCDIR) -r -fo$(GS_OBJ).res $(GLSRC)dwmain.rc

# resources for main program (includes dialogs)
$(GSDLL_OBJ).res: $(GLSRC)gsdll32.rc $(gp_mswin_h) $(ICONS)
	$(RCOMP) -i$(INCDIR) -r -fo$(GSDLL_OBJ).res $(GLSRC)gsdll32.rc


# Modules for small EXE loader.

DWOBJ=$(GLOBJ)dwdll.obj $(GLOBJ)dwimg.obj $(GLOBJ)dwmain.obj $(GLOBJ)dwtext.obj $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj

$(GLOBJ)dwdll.obj: $(GLSRC)dwdll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwdll.obj -c $(GLSRC)dwdll.cpp

$(GLOBJ)dwimg.obj: dwimg.cpp $(AK) $(dwmain_h) $(dwdll_h) $(dwtext_h) $(dwimg_h)\
 $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwimg.obj -c $(GLSRC)dwimg.cpp

$(GLOBJ)dwmain.obj: dwmain.cpp $(AK) $(dwdll_h) $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwmain.obj -c $(GLSRC)dwmain.cpp

$(GLOBJ)dwtext.obj: dwtext.cpp $(AK) $(dwtext_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwtext.obj -c $(GLSRC)dwtext.cpp

# Modules for big EXE

DWOBJNO = $(GLOBJ)dwnodll.obj $(GLOBJ)dwimg.obj $(GLOBJ)dwmain.obj $(GLOBJ)dwtext.obj

$(GLOBJ)dwnodll.obj: $(GLSRC)dwnodll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwnodll.obj -c $(GLSRC)dwnodll.cpp

# Compile gsdll.c, the main program of the DLL.

$(GLOBJ)gsdll.obj: $(GLSRC)gsdll.c $(AK) $(gsdll_h) $(ghost_h)
	$(PSCCWIN) $(COMPILE_FOR_DLL) $(GLO_)gsdll.$(OBJ) -c $(GLSRC)gsdll.c

# Modules for console mode EXEs

OBJC=$(GLOBJ)dwmainc.obj $(GLOBJ)dwdllc.obj $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj
OBJCNO=$(GLOBJ)dwmainc.obj $(GLOBJ)dwnodllc.obj

$(GLOBJ)dwmainc.obj: $(GLSRC)dwmainc.cpp $(AK) $(dwmain_h) $(dwdll_h) $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwmainc.obj -c $(GLSRC)dwmainc.cpp

$(GLOBJ)dwdllc.obj: $(GLSRC)dwdll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwdllc.obj -c $(GLSRC)dwdll.cpp

$(GLOBJ)dwnodllc.obj: $(GLSRC)dwnodll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwnodllc.obj -c $(GLSRC)dwnodll.cpp

# end of winint.mak
