#    Copyright (C) 1991, 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# makefile for MS-DOS / Watcom C/C++ library testing.

libdefault: $(GLOBJ)gslib.exe
	%null

GS_DOCDIR=c:/gs
GS_LIB_DEFAULT=.;c:/gs\;c:/gs/fonts
SEARCH_HERE_FIRST=1
GS_INIT=gs_init.ps

!ifndef DEBUG
DEBUG=1
!endif

!ifndef TDEBUG
TDEBUG=1
!endif

!ifndef NOPRIVATE
NOPRIVATE=1
!endif

GS=gslib

!ifndef GLSRCDIR
GLSRCDIR=.
!endif

!ifndef GLGENDIR
GLGENDIR=.
!endif

!ifndef GLOBJDIR
GLOBJDIR=.
!endif


!ifndef JSRCDIR
JSRCDIR=jpeg
!endif

!ifndef JVERSION
JVERSION=6
!endif


!ifndef PSRCDIR
PSRCDIR=libpng
!endif

!ifndef PVERSION
PVERSION=96
!endif


!ifndef ZSRCDIR
ZSRCDIR=zlib
!endif


!ifndef CONFIG
CONFIG=
!endif

CFLAGS=

# Allow predefinition of WCVERSION
# when using this makefile from inside another one.
!ifndef WCVERSION
WCVERSION=10.0
!endif

LIBPATHS=LIBPATH $(%WATCOM)\lib386 LIBPATH $(%WATCOM)\lib386\nt

!ifndef CPU_TYPE
CPU_TYPE=386
!endif

!ifndef FPU_TYPE
FPU_TYPE=0
!endif


PLATFORM=watclib_
MAKEFILE=watclib.mak
PLATOPT=

!include $(GLSRCDIR)\wccommon.mak

# Allow predefinition of selectable options
# when using this makefile from inside another one.
!ifndef FEATURE_DEVS
FEATURE_DEVS=patlib.dev path1lib.dev hsblib.dev
!endif

!ifndef DEVICE_DEVS
DEVICE_DEVS=vga.dev
!endif

!ifndef COMPILE_INITS
COMPILE_INITS=0
!endif

!ifndef BAND_LIST_STORAGE
BAND_LIST_STORAGE=file
!endif

!ifndef BAND_LIST_COMPRESSOR
BAND_LIST_COMPRESSOR=zlib
!endif

!ifndef FILE_IMPLEMENTATION
FILE_IMPLEMENTATION=stdio
!endif

!include $(GLSRCDIR)\wctail.mak
!include $(GLSRCDIR)\devs.mak
!include $(GLSRCDIR)\contrib.mak

$(GLOBJ)gp_iwatc.$(OBJ): $(GLSRC)gp_iwatc.c $(stat__h) $(string__h)\
 $(gx_h) $(gp_h)
	$(GLCC) $(GLO_)gp_iwatc.$(OBJ) $(C_) $(GLSRC)gp_iwatc.c

$(GLOBJ)gp_ntfs.$(OBJ): $(GLSRC)gp_ntfs.c $(AK)\
 $(dos__h) $(memory__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gsstruct_h) $(gstypes_h) $(gsutil_h)
	$(GLCC) $(GLO_)gp_ntfs.$(OBJ) $(C_) $(GLSRC)gp_ntfs.c

$(GLOBJ)gp_win32.$(OBJ): $(GLSRC)gp_win32.c $(AK)\
 $(dos__h) $(stdio__h) $(string__h) $(windows__h)\
 $(gp_h) $(gsmemory_h) $(gstypes_h)
	$(GLCC) $(GLO_)gp_win32.$(OBJ) $(C_) $(GLSRC)gp_win32.c

# The Watcom C platform

!ifeq WAT32 0
watclib_1=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_iwatc.$(OBJ) $(GLOBJ)gp_msdos.$(OBJ)
watclib_2=$(GLOBJ)gp_dosfb.$(OBJ) $(GLOBJ)gp_dosfs.$(OBJ) $(GLOBJ)gp_dosfe.$(OBJ)
watclib__=$(watclib_1) $(watclib_2)
watclib_.dev: $(watclib__) nosync.dev
	$(SETMOD) watclib_ $(watclib_1)
	$(ADDMOD) watclib_ -obj $(watclib_2)
	$(ADDMOD) watclib_ -include nosync
!else
watclib_1=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_iwatc.$(OBJ) $(GLOBJ)gp_win32.$(OBJ)
watclib_2=$(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_ntfs.$(OBJ) $(GLOBJ)gxsync.$(OBJ)
watclib__=$(watclib_1) $(watclib_2)
watclib_.dev: $(watclib__)
	$(SETMOD) watclib_ $(watclib_1)
	$(ADDMOD) watclib_ -obj $(watclib_2)
!endif

BEGINFILES=*.err

LIB_ONLY=$(GLOBJ)gslib.obj $(GLOBJ)gsnogc.obj $(GLOBJ)gconfig.obj $(GLOBJ)gscdefs.obj
ll_tr=ll$(CONFIG).tr

$(ll_tr): $(MAKEFILE)
	echo OPTION STACK=64k >$(ll_tr)
!ifeq WAT32 0
	echo SYSTEM DOS4G >>$(ll_tr)
	echo OPTION STUB=$(STUB) >>$(ll_tr)
!endif
	echo FILE $(GLOBJ)gsnogc.obj >>$(ll_tr)
	echo FILE $(GLOBJ)gconfig.obj >>$(ll_tr)
	echo FILE $(GLOBJ)gscdefs.obj >>$(ll_tr)

$(GLOBJ)gslib.exe: $(LIB_ALL) $(LIB_ONLY) $(ld_tr) $(ll_tr)
	$(LINK) $(LCT) NAME gslib OPTION MAP=gslib FILE $(GLOBJ)gslib @$(ld_tr) @$(ll_tr)
