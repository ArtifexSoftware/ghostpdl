# Copyright (C) 2001-2025 Artifex Software, Inc.
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
# Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
# CA 94129, USA, for further information.
#
#
# Common interpreter makefile section for 32-bit MS Windows.

# This makefile must be acceptable to Microsoft Visual C++, Watcom C++,
# and Borland C++.  For this reason, the only conditional directives
# allowed are !if[n]def, !else, and !endif.


# Include the generic makefile.
!include $(PSSRCDIR)\int.mak

# Define the location of the NSIS makensis installer utility
!ifndef MAKENSIS_XE
!if $(BUILD_SYSTEM) == 64
MAKENSIS_XE="C:\Program Files (x86)\NSIS-3.0\makensis.exe"
!else
MAKENSIS_XE="C:\Program Files\NSIS-3.0\makensis.exe"
!endif
!endif

!ifdef WIN64
NSISTARGET=gs$(GS_VERSION)w64
VCREDIST=vcredist_x64.exe
!else
NSISTARGET=gs$(GS_VERSION)w32
VCREDIST=vcredist_x86.exe
!endif

# Define the C++ compiler invocation for library modules.
GLCPP=$(CPP) $(CO) $(I_)$(GLI_)$(_I)

# Define the compilation rule for Windows interpreter code.
# This requires PS*_ to be defined, so it has to come after int.mak.
PSCCWIN=$(CC_WX) $(CCWINFLAGS) $(I_)$(PSI_)$(_I) $(I_)$(DEVSRCDIR)$(_I) $(PSF_)

# Define the name of this makefile.
WININT_MAK=$(PSSRC)winint.mak $(TOP_MAKEFILES)

# Define the RCOMP switch for including INCDIR.
!if "$(INCDIR)"==""
i_INCDIR=
!else
i_INCDIR=-i$(INCDIR)
!endif


# ----------------------------- Main program ------------------------------ #

ICONS=$(GLGEN)gswin.ico $(GLGEN)gswin16.ico

GS_ALL=$(INT_ALL)\
  $(LIB_ALL) $(LIBCTR) $(ld_tr) $(GSDLL_OBJ).res $(PSSRC)$(GSDLL).def $(ICONS)

dwdll_h=$(PSSRC)dwdll.h $(iapi_h) $(windows__h)
dwimg_h=$(PSSRC)dwimg.h $(windows__h)
dwtrace_h=$(PSSRC)dwtrace.h
dwres_h=$(PSSRC)dwres.h
dwtext_h=$(PSSRC)dwtext.h $(windows__h)
dwreg_h=$(PSSRC)dwreg.h

# Make the icons from their text form.

$(GLGEN)gswin.ico: $(GLSRC)gswin.icx $(ECHOGS_XE) $(WININT_MAK)
	$(ECHOGS_XE) -wb $(GLGEN)gswin.ico -n -X -r $(GLSRC)gswin.icx

$(GLGEN)gswin16.ico: $(GLSRC)gswin16.icx $(ECHOGS_XE) $(WININT_MAK)
	$(ECHOGS_XE) -wb $(GLGEN)gswin16.ico -n -X -r $(GLSRC)gswin16.icx

# resources for short EXE loader (no dialogs)
$(GS_OBJ).res: $(PSSRC)dwmain.rc $(dwres_h) $(ICONS) $(WININT_MAK)
	$(ECHOGS_XE) -w $(PSGEN)_exe.rc -x 23 define -s gstext_ico $(GLGENDIR)\gswin.ico
	$(ECHOGS_XE) -a $(PSGEN)_exe.rc -x 23 define -s gsgraph_ico $(GLGENDIR)\gswin.ico
	$(ECHOGS_XE) -a $(PSGEN)_exe.rc -R $(PSSRC)dwmain.rc
	$(RCOMP) -dGS_DOT_VERSION=$(GS_DOT_VERSION) -dGS_VERSION_MAJOR=$(GS_VERSION_MAJOR) -dGS_VERSION_MINOR=$(GS_VERSION_MINOR) -dGS_VERSION_PATCH=$(GS_VERSION_PATCH)  -i$(PSSRCDIR) -i$(PSGENDIR) -i$(GLSRCDIR) $(i_INCDIR) -r $(RO_)$(GS_OBJ).res $(PSGEN)_exe.rc
	del $(PSGEN)_exe.rc

# resources for main program (includes dialogs)
$(GSDLL_OBJ).res: $(PSSRC)gsdll32.rc $(gp_mswin_h) $(ICONS) $(WININT_MAK)
	$(ECHOGS_XE) -w $(PSGEN)_dll.rc -x 23 define -s gstext_ico $(GLGENDIR)\gswin.ico
	$(ECHOGS_XE) -a $(PSGEN)_dll.rc -x 23 define -s gsgraph_ico $(GLGENDIR)\gswin.ico
	$(ECHOGS_XE) -a $(PSGEN)_dll.rc -R $(PSSRC)gsdll32.rc
	$(RCOMP) -dGS_DOT_VERSION=$(GS_DOT_VERSION) -dGS_VERSION_MAJOR=$(GS_VERSION_MAJOR) -dGS_VERSION_MINOR=$(GS_VERSION_MINOR) -dGS_VERSION_PATCH=$(GS_VERSION_PATCH)  -i$(PSSRCDIR) -i$(PSGENDIR) -i$(GLSRCDIR) $(i_INCDIR) -r $(RO_)$(GSDLL_OBJ).res $(PSGEN)_dll.rc
	del $(PSGEN)_dll.rc


# Modules for big EXE

!if $(DEBUG)
DWTRACE=$(GLOBJ)dwtrace.obj
!else
DWTRACE=
!endif


DWOBJNO = $(PSOBJ)dwnodll.obj $(GLOBJ)dwimg.obj $(DWTRACE) $(PSOBJ)dwmain.obj \
$(GLOBJ)dwtext.obj $(GLOBJ)dwreg.obj

$(PSOBJ)dwnodll.obj: $(PSSRC)dwnodll.c $(AK)\
 $(dwdll_h) $(iapi_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwnodll.obj $(C_) $(PSSRC)dwnodll.c

# Compile gsdll.c, the main program of the DLL.

$(PSOBJ)gsdll.obj: $(PSSRC)gsdll.c $(AK) $(iapi_h) $(ghost_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_DLL) $(PSO_)gsdll.$(OBJ) $(C_) $(PSSRC)gsdll.c

$(GLOBJ)gp_msdll.obj: $(GLSRC)gp_msdll.c $(AK) $(iapi_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_DLL) $(GLO_)gp_msdll.$(OBJ) $(C_) $(GLSRC)gp_msdll.c

# Modules for console mode EXEs

OBJC=$(PSOBJ)dwmainc.obj $(PSOBJ)dwdllc.obj $(GLOBJ)gscdefs.obj\
!ifdef METRO
 $(GLOBJ)gp_wgetv.obj \
!else
 $(GLOBJ)gp_wgetv.obj $(GLOBJ)dwimg.obj $(DWTRACE) $(GLOBJ)dwreg.obj
!endif

OBJCNO=$(PSOBJ)dwmainc.obj $(PSOBJ)dwnodllc.obj $(GLOBJ)dwimg.obj $(DWTRACE) $(GLOBJ)dwreg.obj

$(PSOBJ)dwmainc.obj: $(PSSRC)dwmainc.c $(AK) $(windows__h) $(fcntl__h) $(unistd__h) \
  $(iapi_h) $(vdtrace_h) $(gdevdsp_h) $(dwdll_h) $(dwimg_h) $(dwtrace_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_CONSOLE_EXE) $(PSO_)dwmainc.obj $(C_) $(PSSRC)dwmainc.c

$(PSOBJ)dwdllc.obj: $(PSSRC)dwdll.c $(AK) $(dwdll_h) $(iapi_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_CONSOLE_EXE) $(PSO_)dwdllc.obj $(C_) $(PSSRC)dwdll.c

$(PSOBJ)dwnodllc.obj: $(PSSRC)dwnodll.c $(AK) $(dwdll_h) $(iapi_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_CONSOLE_EXE) $(PSO_)dwnodllc.obj $(C_) $(PSSRC)dwnodll.c


# Modules for small EXE loader.

DWOBJ=$(PSOBJ)dwdll.obj $(GLOBJ)dwimg.obj $(DWTRACE) $(PSOBJ)dwmain.obj \
$(PSOBJ)dwtext.obj $(GLOBJ)gscdefs.obj $(GLOBJ)gp_wgetv.obj $(PSOBJ)dwreg.obj

$(PSOBJ)dwdll.obj: $(PSSRC)dwdll.c $(AK)\
 $(dwdll_h) $(iapi_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwdll.obj $(C_) $(PSSRC)dwdll.c

$(PSOBJ)dwimg.obj: $(PSSRC)dwimg.c $(AK)\
 $(dwres_h) $(dwdll_h) $(dwtext_h) $(dwimg_h) $(gdevdsp_h) $(stdio__h) \
 $(gscdefs_h) $(dwreg_h) $(WININT_MAK)
        $(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwimg.obj $(C_) $(PSSRC)dwimg.c

$(PSOBJ)dwtrace.obj: $(PSSRC)dwtrace.c $(AK)\
 $(dwimg_h) $(dwtrace_h)\
 $(gscdefs_h) $(stdpre_h) $(gsdll_h) $(vdtrace_h) $(WININT_MAK)
        $(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwtrace.obj $(C_) $(PSSRC)dwtrace.c

$(PSOBJ)dwmain.obj: $(PSSRC)dwmain.c $(AK)  $(windows__h) \
 $(iapi_h) $(vdtrace_h) $(dwres_h) $(dwdll_h) $(dwtext_h) $(dwimg_h) $(dwtrace_h) \
 $(dwreg_h) $(gdevdsp_h) $(WININT_MAK)
	$(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwmain.obj $(C_) $(PSSRC)dwmain.c

$(PSOBJ)dwtext.obj: $(PSSRC)dwtext.c $(AK) $(dwtext_h) $(WININT_MAK)
        $(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwtext.obj $(C_) $(PSSRC)dwtext.c

$(PSOBJ)dwreg.obj: $(PSSRC)dwreg.c $(AK) $(dwreg_h) $(WININT_MAK)
        $(PSCCWIN) $(COMPILE_FOR_EXE) $(PSO_)dwreg.obj $(C_) $(PSSRC)dwreg.c


# ------------ Windows version of the .locale_to_utf8 operator ------------ #

zwinutf8_=$(PSOBJ)zwinutf8.$(OBJ)
$(PSD)winutf8.dev : $(MAKEFILE) $(ECHOGS_XE) $(zwinutf8_) $(WININT_MAK)
	$(SETMOD) $(PSD)winutf8 $(zwinutf8_)
	$(ADDMOD) $(PSD)winutf8 -oper zwinutf8

$(PSOBJ)zwinutf8.$(OBJ) : $(PSSRC)zwinutf8.c $(OP)\
 $(ghost_h) $(oper_h) $(iutil_h) $(store_h) $(windows__h) $(WININT_MAK)
	$(PSCCWIN) $(PSO_)zwinutf8.$(OBJ) $(C_) $(PSSRC)zwinutf8.c

# -------------------- NSIS Installer -------------------------------- #
nsis: $(PSSRC)nsisinst.nsi $(GSCONSOLE_XE) $(GS_ALL) $(GS_XE) $(GSDLL_DLL) $(BINDIR)\$(GSDLL).lib \
      "$(VCINSTALLDIR)Redist\MSVC\$(MS_TOOLSET_VERSION)\$(VCREDIST)" \
      $(WININT_MAK)
	$(CP_) "$(VCINSTALLDIR)Redist\MSVC\$(MS_TOOLSET_VERSION)\$(VCREDIST)" .
	$(MAKENSIS_XE) -NOCD -DVCREDIST=$(VCREDIST) -DTARGET=$(NSISTARGET) -DVERSION=$(GS_DOT_VERSION) -DCOMPILE_INITS=$(COMPILE_INITS) $(PSSRC)nsisinst.nsi
	-del $(VCREDIST)
!if defined(KEYFILE) && defined(KEYPWORD) && defined(TIMESTAMP)
        signtool sign -f $(KEYFILE) /p $(KEYPWORD) /t $(TIMESTAMP) $(NSISTARGET)$(XE)
!endif

# -------------------- Distribution source archive ------------------- #
# This creates a zip file containing the files needed to build
# ghostscript on MS-Windows.  We don't distribute this zip file,
# but use it to build the executable distribution.
#
# The MS-Windows build process for a release is
#  gzip -d ghostscript-N.NN.tar.gz
#  tar -xvf ghostscript-N.NN.tar
#  cd ghostscript-N.NN
#  nmake -f psi/msvc32.mak srczip
#  cd gsN.NN
#  nmake -f psi/msvc32.mak
#  nmake -f psi/msvc32.mak archive

gs$(GS_VERSION)src.zip:
	-rmdir /s /q gs$(GS_DOT_VERSION)
	-del temp.zip
	zip -r -X temp.zip LICENSE Resource arch base conrib cups doc examples expat freetype iccprofiles ijs jbig2dec jpeg jpegxr lcms lcms2 lib libpng man openjpeg psi tiff toolbin trio zlib -x ".svn/*" -x "*/.svn/*" -x "*/*/.svn/*" -x "*/*/*/.svn/*" -x "*/*/*/*/.svn/*" -x "*/*/*/*/*/.svn/*"
	mkdir gs$(GS_DOT_VERSION)
	cd gs$(GS_DOT_VERSION)
	unzip -a ../temp.zip
	cd ..
	zip -9 -r -X gs$(GS_VERSION)src.zip gs$(GS_DOT_VERSION)

srczip: gs$(GS_VERSION)src.zip

# end of winint.mak
