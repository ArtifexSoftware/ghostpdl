#    Copyright (C) 1997, 1998, 1999 Aladdin Enterprises.  All rights reserved.
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


# Common interpreter makefile section for 32-bit MS Windows.

# This makefile must be acceptable to Microsoft Visual C++, Watcom C++,
# and Borland C++.  For this reason, the only conditional directives
# allowed are !if[n]def, !else, and !endif.


# Include the generic makefile.
!include $(PSSRCDIR)\int.mak
!include $(PSSRCDIR)\cfonts.mak

# Define the C++ compiler invocation for library modules.
GLCPP=$(CPP) $(CO) $(I_)$(GLI_)$(_I)

# Define the compilation rule for Windows interpreter code.
# This requires PS*_ to be defined, so it has to come after int.mak.
PSCCWIN=$(CC_WX) $(CCWINFLAGS) $(I_)$(PSI_)$(_I) $(PSF_)

# Define the name of this makefile.
WININT_MAK=$(PSSRC)winint.mak

# ----------------------------- Main program ------------------------------ #

ICONS=$(GLGEN)gsgraph.ico $(GLGEN)gstext.ico

GS_ALL=$(INT_ALL) $(INTASM)\
  $(LIB_ALL) $(LIBCTR) $(GLGEN)lib.tr $(ld_tr) $(GSDLL_OBJ).res $(GLSRC)$(GSDLL).def $(ICONS)

dwdll_h=$(GLSRC)dwdll.h $(gsdllwin_h)
dwimg_h=$(GLSRC)dwimg.h
dwmain_h=$(GLSRC)dwmain.h
dwtext_h=$(GLSRC)dwtext.h

# Make the icons from their text form.

$(GLGEN)gsgraph.ico: $(GLSRC)gsgraph.icx $(ECHOGS_XE) $(WININT_MAK)
	$(ECHOGS_XE) -wb $(GLGEN)gsgraph.ico -n -X -r $(GLSRC)gsgraph.icx

$(GLGEN)gstext.ico: $(GLSRC)gstext.icx $(ECHOGS_XE) $(WININT_MAK)
	$(ECHOGS_XE) -wb $(GLGEN)gstext.ico -n -X -r $(GLSRC)gstext.icx

# resources for short EXE loader (no dialogs)
$(GS_OBJ).res: $(GLSRC)dwmain.rc $(dwmain_h) $(ICONS) $(WININT_MAK)
	$(ECHOGS_XE) -w $(GLGEN)_exe.rc -x 23 define -s gstext_ico $(GLGENDIR)\gstext.ico
	$(ECHOGS_XE) -a $(GLGEN)_exe.rc -x 23 define -s gsgraph_ico $(GLGENDIR)\gsgraph.ico
	$(ECHOGS_XE) -a $(GLGEN)_exe.rc -R $(GLSRC)dwmain.rc
	$(RCOMP) -I$(GLSRCDIR) -i$(INCDIR)$(_I) -r $(RO_)$(GS_OBJ).res $(GLGEN)_exe.rc
	del $(GLGEN)_exe.rc

# resources for main program (includes dialogs)
$(GSDLL_OBJ).res: $(GLSRC)gsdll32.rc $(gp_mswin_h) $(ICONS) $(WININT_MAK)
	$(ECHOGS_XE) -w $(GLGEN)_dll.rc -x 23 define -s gstext_ico $(GLGENDIR)\gstext.ico
	$(ECHOGS_XE) -a $(GLGEN)_dll.rc -x 23 define -s gsgraph_ico $(GLGENDIR)\gsgraph.ico
	$(ECHOGS_XE) -a $(GLGEN)_dll.rc -R $(GLSRC)gsdll32.rc
	$(RCOMP) -I$(GLSRCDIR) -i$(INCDIR)$(_I) -r $(RO_)$(GSDLL_OBJ).res $(GLGEN)_dll.rc
	del $(GLGEN)_dll.rc


# Modules for small EXE loader.

DWOBJ=$(GLOBJ)dwdll.obj $(GLOBJ)dwimg.obj $(GLOBJ)dwmain.obj $(GLOBJ)dwtext.obj $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj

$(GLOBJ)dwdll.obj: $(GLSRC)dwdll.cpp $(AK)\
 $(dwdll_h) $(gsdll_h) $(gsdllwin_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwdll.obj $(C_) $(GLSRC)dwdll.cpp

$(GLOBJ)dwimg.obj: $(GLSRC)dwimg.cpp $(AK)\
 $(dwmain_h) $(dwdll_h) $(dwtext_h) $(dwimg_h)\
 $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwimg.obj $(C_) $(GLSRC)dwimg.cpp

$(GLOBJ)dwmain.obj: $(GLSRC)dwmain.cpp $(AK)\
 $(dwdll_h) $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwmain.obj $(C_) $(GLSRC)dwmain.cpp

$(GLOBJ)dwtext.obj: $(GLSRC)dwtext.cpp $(AK) $(dwtext_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwtext.obj $(C_) $(GLSRC)dwtext.cpp

# Modules for big EXE

DWOBJNO = $(GLOBJ)dwnodll.obj $(GLOBJ)dwimg.obj $(GLOBJ)dwmain.obj $(GLOBJ)dwtext.obj

$(GLOBJ)dwnodll.obj: $(GLSRC)dwnodll.cpp $(AK)\
 $(dwdll_h) $(gsdll_h) $(gsdllwin_h)
	$(GLCPP) $(COMPILE_FOR_EXE) $(GLO_)dwnodll.obj $(C_) $(GLSRC)dwnodll.cpp

# Compile gsdll.c, the main program of the DLL.

$(GLOBJ)gsdll.obj: $(GLSRC)gsdll.c $(AK) $(gsdll_h) $(ghost_h)
	$(PSCCWIN) $(COMPILE_FOR_DLL) $(GLO_)gsdll.$(OBJ) $(C_) $(GLSRC)gsdll.c

# Modules for console mode EXEs

OBJC=$(GLOBJ)dwmainc.obj $(GLOBJ)dwdllc.obj $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj
OBJCNO=$(GLOBJ)dwmainc.obj $(GLOBJ)dwnodllc.obj

$(GLOBJ)dwmainc.obj: $(GLSRC)dwmainc.cpp $(AK) $(dwmain_h) $(dwdll_h) $(gscdefs_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwmainc.obj $(C_) $(GLSRC)dwmainc.cpp

$(GLOBJ)dwdllc.obj: $(GLSRC)dwdll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwdllc.obj $(C_) $(GLSRC)dwdll.cpp

$(GLOBJ)dwnodllc.obj: $(GLSRC)dwnodll.cpp $(AK) $(dwdll_h) $(gsdll_h)
	$(GLCPP) $(COMPILE_FOR_CONSOLE_EXE) $(GLO_)dwnodllc.obj $(C_) $(GLSRC)dwnodll.cpp

# end of winint.mak
