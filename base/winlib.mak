# Copyright (C) 2001-2023 Artifex Software, Inc.
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
# Common makefile section for 32-bit MS Windows.

# This makefile must be acceptable to Microsoft Visual C++, Watcom C++,
# and Borland C++.  For this reason, the only conditional directives
# allowed are !if[n]def, !else, and !endif.
WINLIB_MAK=$(GLSRC)winlib.mak $(TOP_MAKEFILES)

# Note that built-in third-party libraries aren't available.

SHARE_FT=0
SHARE_JPEG=0
SHARE_LIBPNG=0
SHARE_LIBTIFF=0
SHARE_ZLIB=0
SHARE_JBIG2=0
SHARE_JPX=0
SHARE_LCMS=0
SHARE_LCUPS=0
SHARE_LCUPSI=0

SHARE_IJS=0
IJS_NAME=
IJSSRCDIR=ijs
IJSEXECTYPE=win

# Define the directory where the CUPS library sources are stored,

!ifndef LCUPSSRCDIR
SHARE_LCUPS=0
LCUPS_NAME=
LCUPSSRCDIR=cups
LCUPSBUILDTYPE=win
CUPS_CC=$(CC) $(CFLAGS) -DWIN32
!endif

!ifndef LCUPSISRCDIR
SHARE_LCUPSI=0
LCUPSI_NAME=
LCUPSISRCDIR=cups
CUPS_CC=$(CC) $(CFLAGS) -DWIN32
!endif

# Define the platform name.

!ifndef GSPLATFORM
!ifdef METRO
GSPLATFORM=metro_
!else
GSPLATFORM=mswin32_
!endif
!endif

# Define the auxiliary program dependency. We use this to
# preconstruct ccf32.tr to get around the limit on the maximum
# length of a command line.

AK=$(GLGENDIR)\ccf32.tr

# Define the syntax for command, object, and executable files.

NULL=

CMD=.bat
D_=-D
_D_=$(NULL)=
_D=
I_=-I
II=-I
_I=
NO_OP=@rem
# O_ and XE_ are defined separately for each compiler.
OBJ=obj
Q=
XE=.exe
XEAUX=.exe
PERCENTESCAPE=%
GENCONFLINECONT=

# Define generic commands.

# We have to use a batch file for the equivalent of cp,
# because the DOS COPY command copies the file write time, like cp -p.
# We also have to use a batch file for for the equivalent of rm -f,
# because the DOS ERASE command returns an error status if the file
# doesn't exist.
CP_=call $(GLSRCDIR)\cp.bat
RM_=call $(GLSRCDIR)\rm.bat
RMN_=call $(GLSRCDIR)\rm.bat

# Define the generic compilation flags.

PLATOPT=

# Define conditinal name for UFST bridge :
!ifdef UFST_ROOT
UFST_LIB_EXT=.lib
!endif

# Define conditinal for FreeType bridge :
!ifndef FT_BRIDGE
FT_BRIDGE = 0
!endif

# Which CMS are we using?
!ifndef WHICH_CMS
WHICH_CMS=lcms2mt
!endif

# Define the files to be removed by `make clean'.
# nmake expands macros when encountered, not when used,
# so this must precede the !include statements.

BEGINFILES=$(GLGENDIR)\ccf32.tr\
 $(GLOBJDIR)\*.res $(GLOBJDIR)\*.ico\
 $(BINDIR)\$(GSDLL).dll $(BINDIR)\$(GSCONSOLE).exe\
 $(BINDIR)\setupgs.exe $(BINDIR)\uninstgs.exe\
 $(GLOBJDIR)\cups\*.h $(GLOBJDIR)\*.h $(GLOBJDIR)\*.c $(AUXDIR)\*.sbr \
 $(AUXDIR)\*.pdb \
 $(BEGINFILES2)

# Include the generic makefiles.
#!include $(COMMONDIR)/pcdefs.mak
#!include $(COMMONDIR)/generic.mak
!include $(GLSRCDIR)\gs.mak

!if "$(OCR_VERSION)"=="1"
!include $(GLSRCDIR)\leptonica.mak
!include $(GLSRCDIR)\tesseract.mak
!endif

!include $(GLSRCDIR)\lib.mak
!include $(GLSRCDIR)\freetype.mak
!if "$(UFST_BRIDGE)"=="1"
!include $(UFST_ROOT)\fapiufst.mak
!endif
!include $(GLSRCDIR)\jpeg.mak
# zlib.mak must precede png.mak
!include $(GLSRCDIR)\zlib.mak
!include $(GLSRCDIR)\png.mak
!include $(GLSRCDIR)\tiff.mak
!include $(GLSRCDIR)\jbig2.mak
!include $(GLSRCDIR)\openjpeg.mak
!include $(GLSRCDIR)\cal.mak
!include $(GLSRCDIR)\ocr.mak

!include $(GLSRCDIR)\expat.mak
!include $(GLSRCDIR)\jpegxr.mak

!include $(GLSRCDIR)\$(WHICH_CMS).mak
!include $(GLSRCDIR)\ijs.mak
!include $(GLSRCDIR)\lcups.mak
!include $(GLSRCDIR)\lcupsi.mak
!include $(DEVSRCDIR)\extract.mak
!include $(DEVSRCDIR)\devs.mak
!include $(DEVSRCDIR)\dcontrib.mak
!include $(CONTRIBDIR)\contrib.mak

# Define the compilation rule for Windows devices.
# This requires GL*_ to be defined, so it has to come after lib.mak.
GLCCWIN=$(CC_WX) $(CCWINFLAGS) $(I_)$(GLI_)$(_I) $(GLF_)

!include $(GLSRCDIR)\winplat.mak
!include $(GLSRCDIR)\pcwin.mak

# Define abbreviations for the executable and DLL files.
GS_OBJ=$(GLOBJ)$(GS)
GSDLL_SRC=$(GLSRC)$(GSDLL)
GSDLL_OBJ=$(GLOBJ)$(GSDLL)

# -------------------------- Auxiliary files --------------------------- #

# No special gconfig_.h is needed.	/* This file deliberately left blank. */
$(gconfig__h): $(TOP_MAKEFILES) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfig__h) -x 2f2a20 This file deliberately left blank. -x 2a2f

# -------------------------------- Library -------------------------------- #

# The Windows Win32 platform

mswin32__=$(GLOBJ)gp_mswin.$(OBJ) $(GLOBJ)gp_wgetv.$(OBJ) $(GLOBJ)gp_wpapr.$(OBJ) \
 $(GLOBJ)gp_stdia.$(OBJ) $(GLOBJ)gp_utf8.$(OBJ) $(GLOBJ)gp_winfs.$(OBJ)
mswin32_inc=$(GLD)nosync.dev $(GLD)winplat.dev

$(GLGEN)mswin32_.dev:  $(mswin32__) $(ECHOGS_XE) $(mswin32_inc) $(WINLIB_MAK)
	$(SETMOD) $(GLGEN)mswin32_ $(mswin32__)
	$(ADDMOD) $(GLGEN)mswin32_ -include $(mswin32_inc)

$(GLOBJ)gp_mswin.$(OBJ): $(GLSRC)gp_mswin.c $(AK) $(gp_mswin_h) \
 $(ctype__h) $(dos__h) $(malloc__h) $(memory__h) $(pipe__h) \
 $(stdio__h) $(string__h) $(windows__h) \
 $(gx_h) $(gp_h) $(gpcheck_h) $(gpmisc_h) $(gserrors_h) $(gsexit_h) \
 $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_mswin.$(OBJ) $(C_) $(GLSRC)gp_mswin.c

$(GLOBJ)gp_winfs.$(OBJ): $(GLSRC)gp_winfs.c $(AK) $(gp_mswin_h) \
 $(memory__h) $(stdio__h) $(windows__h) $(gp_h) $(gserrors_h) \
 $(gserrors_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_winfs.$(OBJ) $(C_) $(GLSRC)gp_winfs.c

$(AUX)gp_winfs.$(OBJ): $(GLSRC)gp_winfs.c $(AK) $(gp_mswin_h) \
 $(memory__h) $(stdio__h) $(windows__h) $(gp_h) $(gserrors_h) \
 $(WINLIB_MAK)
	$(GLCCAUX) $(AUXO_)gp_winfs.$(OBJ) $(C_) $(GLSRC)gp_winfs.c

$(AUX)gp_winfs2.$(OBJ): $(GLSRC)gp_winfs2.c $(AK) $(gp_mswin_h) \
 $(memory__h) $(stdio__h) $(windows__h) $(gp_h) $(gserrors_h) \
 $(WINLIB_MAK)
	$(GLCCAUX) $(AUXO_)gp_winfs2.$(OBJ) $(C_) $(GLSRC)gp_winfs2.c

$(GLOBJ)gp_wgetv.$(OBJ): $(GLSRC)gp_wgetv.c $(AK) $(gscdefs_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_wgetv.$(OBJ) $(C_) $(GLSRC)gp_wgetv.c

$(GLOBJ)gp_wpapr.$(OBJ): $(GLSRC)gp_wpapr.c $(AK) $(gp_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_wpapr.$(OBJ) $(C_) $(GLSRC)gp_wpapr.c

$(GLOBJ)gp_stdia.$(OBJ): $(GLSRC)gp_stdia.c $(AK)\
  $(stdio__h) $(time__h) $(unistd__h) $(gx_h) $(gp_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_stdia.$(OBJ) $(C_) $(GLSRC)gp_stdia.c

# The Metro platform
!ifdef METRO
METRO_OBJS=$(GLOBJ)winrtsup.$(OBJ)

$(GLOBJ)winrtsup.$(OBJ): $(GLSRCDIR)/winrtsup.cpp $(WINLIB_MAK)
	$(GLCCWIN) /EHsc $(GLO_)winrtsup.$(OBJ) $(C_) $(GLSRCDIR)/winrtsup.cpp
!else
METRO_OBJS=
!endif


metro__=$(GLOBJ)gp_mswin.$(OBJ) $(GLOBJ)gp_wgetv.$(OBJ) $(GLOBJ)gp_wpapr.$(OBJ)\
  $(GLOBJ)gp_stdia.$(OBJ) $(METRO_OBJS)
metro_inc=$(GLD)nosync.dev $(GLD)winplat.dev

$(GLGEN)metro_.dev:  $(metro__) $(ECHOGS_XE) $(metro_inc) $(WINLIB_MAK)
	$(SETMOD) $(GLGEN)metro_ $(metro__)
	$(ADDMOD) $(GLGEN)metro_ -include $(metro_inc)


# Define MS-Windows handles (file system) as a separable feature.

mshandle_=$(GLOBJ)gp_mshdl.$(OBJ)
$(GLD)mshandle.dev: $(ECHOGS_XE) $(mshandle_) $(WINLIB_MAK)
	$(SETMOD) $(GLD)mshandle $(mshandle_)
	$(ADDMOD) $(GLD)mshandle -iodev handle

$(GLOBJ)gp_mshdl.$(OBJ): $(GLSRC)gp_mshdl.c $(AK)\
 $(ctype__h) $(errno__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gxiodev_h) $(gserrors_h) $(WINLIB_MAK)
	$(GLCC) $(GLO_)gp_mshdl.$(OBJ) $(C_) $(GLSRC)gp_mshdl.c

# Define MS-Windows printer (file system) as a separable feature.

msprinter_=$(GLOBJ)gp_msprn.$(OBJ)

$(GLD)msprinter.dev: $(msprinter_) $(WINLIB_MAK)
	$(SETMOD) $(GLD)msprinter $(msprinter_)
	$(ADDMOD) $(GLD)msprinter -iodev printer

$(GLOBJ)gp_msprn.$(OBJ): $(GLSRC)gp_msprn.c $(AK)\
 $(ctype__h) $(errno__h) $(stdio__h) $(string__h)\
 $(gsmemory_h) $(gstypes_h) $(gxiodev_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_msprn.$(OBJ) $(C_) $(GLSRC)gp_msprn.c

# Define MS-Windows polling as a separable feature
# because it is not needed by the gslib.
mspoll_=$(GLOBJ)gp_mspol.$(OBJ)
$(GLD)mspoll.dev: $(ECHOGS_XE) $(mspoll_) $(WINLIB_MAK)
	$(SETMOD) $(GLD)mspoll $(mspoll_)

$(GLOBJ)gp_mspol.$(OBJ): $(GLSRC)gp_mspol.c $(AK)\
 $(gx_h) $(gp_h) $(gpcheck_h) $(WINLIB_MAK)
	$(GLCCWIN) $(GLO_)gp_mspol.$(OBJ) $(C_) $(GLSRC)gp_mspol.c

# end of winlib.mak
