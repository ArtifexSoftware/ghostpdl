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

# Id: watclib.mak 
# makefile for MS-DOS / Watcom C/C++ library testing.

libdefault: $(GLOBJ)gslib.exe
	%null

GS_DOCDIR=c:/gs
GS_LIB_DEFAULT=.;c:/gs\;c:/gs/fonts
SEARCH_HERE_FIRST=1
GS_INIT=gs_init.ps

DEBUG=1
TDEBUG=0
NOPRIVATE=1

GS=gslib

GLSRCDIR=.
GLGENDIR=.
GLOBJDIR=.

JSRCDIR=jpeg
JVERSION=6

PSRCDIR=libpng
PVERSION=96

ZSRCDIR=zlib

CONFIG=
CFLAGS=

# Allow predefinition of WCVERSION
# when using this makefile from inside another one.
!ifndef WCVERSION
WCVERSION=10.0
!endif
LIBPATHS=LIBPATH $(%WATCOM)\lib386 LIBPATH $(%WATCOM)\lib386\dos
STUB=$(%WATCOM)\binb\wstub.exe

CPU_TYPE=386
FPU_TYPE=0

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

watclib_1=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_iwatc.$(OBJ) $(GLOBJ)gp_msdos.$(OBJ)
watclib_2=$(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_dosfs.$(OBJ) $(GLOBJ)gp_dosfe.$(OBJ)
watclib__=$(watclib_1) $(watclib_2)
watclib_.dev: $(watclib__)
	$(SETMOD) watclib_ $(watclib_1)
	$(ADDMOD) watclib_ -obj $(watclib_2)

$(GLOBJ)gp_iwatc.$(OBJ): $(GLSRC)gp_iwatc.c $(stat__h) $(string__h)\
 $(gx_h) $(gp_h)
	$(GLCC) $(GLO_)gp_iwatc.$(OBJ) $(C_) $(GLSRC)gp_iwatc.c

BEGINFILES=*.err

LIB_ONLY=$(GLOBJ)gslib.obj $(GLOBJ)gsnogc.obj $(GLOBJ)gconfig.obj $(GLOBJ)gscdefs.obj
ll_tr=ll$(CONFIG).tr
$(ll_tr): $(MAKEFILE)
	echo SYSTEM DOS4G >$(ll_tr)
	echo OPTION STUB=$(STUB) >>$(ll_tr)
	echo OPTION STACK=12k >>$(ll_tr)
	echo FILE $(GLOBJ)gsnogc.obj >>$(ll_tr)
	echo FILE $(GLOBJ)gconfig.obj >>$(ll_tr)
	echo FILE $(GLOBJ)gscdefs.obj >>$(ll_tr)

$(GLOBJ)gslib.exe: $(LIB_ALL) $(LIB_ONLY) $(ld_tr) $(ll_tr)
	$(LINK) $(LCT) NAME gslib OPTION MAP=gslib FILE $(GLOBJ)gslib @$(ld_tr) @$(ll_tr)
