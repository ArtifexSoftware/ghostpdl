#    Copyright (C) 1995, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# makefile for Unix / gcc library testing.

GLSRCDIR=.
GLGENDIR=./debugobj
GLOBJDIR=./debugobj

#include $(COMMONDIR)/gccdefs.mak
#include $(COMMONDIR)/unixdefs.mak
#include $(COMMONDIR)/generic.mak
include $(GLSRCDIR)/version.mak

gsdir = /usr/local/share/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)
GS_DOCDIR=$(gsdatadir)/doc
GS_LIB_DEFAULT=$(gsdatadir):$(gsdir)/fonts
SEARCH_HERE_FIRST=1
GS_INIT=gs_init.ps

#GENOPT=-DDEBUG
GENOPT=
GS=gslib

JSRCDIR=jpeg
JVERSION=6
# DON'T SET THIS TO 1!
SHARE_JPEG=0
JPEG_NAME=jpeg

PSRCDIR=libpng
PVERSION=96
SHARE_LIBPNG=1
LIBPNG_NAME=png

ZSRCDIR=zlib
SHARE_ZLIB=1
ZLIB_NAME=z

CONFIG=

CC=gcc
CCLD=$(CC)

GCFLAGS=-Wall -Wcast-qual -Wpointer-arith -Wstrict-prototypes -Wwrite-strings -fno-common
XCFLAGS=
CFLAGS=-g -O $(GCFLAGS) $(XCFLAGS)
LDFLAGS=$(XLDFLAGS)
EXTRALIBS=
XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt Xext X11

FPU_TYPE=1

FEATURE_DEVS=dps2lib.dev psl2cs.dev cielib.dev imasklib.dev patlib.dev htxlib.dev roplib.dev devcmap.dev
COMPILE_INITS=0
BAND_LIST_STORAGE=file
BAND_LIST_COMPRESSOR=zlib
FILE_IMPLEMENTATION=stdio
DEVICE_DEVS=x11cmyk.dev x11mono.dev x11.dev x11alpha.dev djet500.dev pbmraw.dev pgmraw.dev ppmraw.dev bbox.dev
DEVICE_DEVS1=
DEVICE_DEVS2=
DEVICE_DEVS3=
DEVICE_DEVS4=
DEVICE_DEVS5=
DEVICE_DEVS6=
DEVICE_DEVS7=
DEVICE_DEVS8=
DEVICE_DEVS9=
DEVICE_DEVS10=
DEVICE_DEVS11=
DEVICE_DEVS12=
DEVICE_DEVS13=
DEVICE_DEVS14=
DEVICE_DEVS15=

MAKEFILE=$(GLSRCDIR)/ugcclib.mak

AK=
CCFLAGS=$(GENOPT) $(CFLAGS)
CC_=$(CC) $(CCFLAGS)
CCAUX=$(CC)
CC_LEAF=$(CC_)
# When using gcc, CCA2K isn't needed....
CCA2K=$(CC)

include $(GLSRCDIR)/unixhead.mak

include $(GLSRCDIR)/gs.mak
include $(GLSRCDIR)/lib.mak
include $(GLSRCDIR)/jpeg.mak
# zlib.mak must precede libpng.mak
include $(GLSRCDIR)/zlib.mak
include $(GLSRCDIR)/libpng.mak
include $(GLSRCDIR)/devs.mak
include $(GLSRCDIR)/contrib.mak

# Following is from unixtail.mak, we have a different link step.
unix__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_unix.$(OBJ) $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_unifn.$(OBJ)
unix_.dev: $(unix__) nosync.dev
	$(SETMOD) unix_ $(unix__) -include nosync

$(GLOBJ)gp_unix.$(OBJ): $(GLSRC)gp_unix.c $(AK)\
 $(pipe__h) $(string__h) $(time__h)\
 $(gx_h) $(gsexit_h) $(gp_h)
	$(GLCC) $(GLO_)gp_unix.$(OBJ) $(C_) $(GLSRC)gp_unix.c

sysv__=$(GLOBJ)gp_getnv.$(OBJ) $(GLOBJ)gp_nofb.$(OBJ) $(GLOBJ)gp_unix.$(OBJ) $(GLOBJ)gp_unifs.$(OBJ) $(GLOBJ)gp_unifn.$(OBJ) $(GLOBJ)gp_sysv.$(OBJ)
sysv_.dev: $(sysv__)
	$(SETMOD) sysv_ $(sysv__)

$(GLOBJ)gp_sysv.$(OBJ): $(GLSRC)gp_sysv.c $(time__h) $(AK)
	$(GLCC) $(GLO_)gp_sysv.$(OBJ) $(C_) $(GLSRC)gp_sysv.c

# Auxiliary programs

$(ANSI2KNR_XE): $(GLSRC)ansi2knr.c
	$(CCA2K) $(O_)$(ANSI2KNR_XE) $(GLSRC)ansi2knr.c

$(ECHOGS_XE): $(GLSRC)echogs.c $(AK)
	$(CCAUX) $(O_)$(ECHOGS_XE) $(GLSRC)echogs.c

# On the RS/6000 (at least), compiling genarch.c with gcc with -O
# produces a buggy executable.
$(GENARCH_XE): $(GLSRC)genarch.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENARCH_XE) $(GLSRC)genarch.c

$(GENCONF_XE): $(GLSRC)genconf.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENCONF_XE) $(GLSRC)genconf.c

$(GENDEV_XE): $(GLSRC)gendev.c $(AK) $(stdpre_h)
	$(CCAUX) $(O_)$(GENDEV_XE) $(GLSRC)gendev.c

INCLUDE=/usr/include
$(gconfig__h): $(MAKEFILE) $(ECHOGS_XE)
	$(ECHOGS_XE) -w $(gconfig__h) -x 2f2a -s This file was generated automatically. -s -x 2a2f
	if ( test -f $(INCLUDE)/dirent.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_DIRENT_H; else true; fi
	if ( test -f $(INCLUDE)/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/dir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_DIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/ndir.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_NDIR_H; else true; fi
	if ( test -f $(INCLUDE)/sys/time.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIME_H; else true; fi
	if ( test -f $(INCLUDE)/sys/times.h ); then $(ECHOGS_XE) -a $(gconfig__h) -x 23 define HAVE_SYS_TIMES_H; else true; fi

LIB_ONLY=$(GLOBJ)gslib.$(OBJ) $(GLOBJ)gsnogc.$(OBJ) $(GLOBJ)gconfig.$(OBJ) $(GLOBJ)gscdefs.$(OBJ)
ldt_tr=$(GLOBJ)ldt.tr
$(GS_XE): $(ld_tr) $(ECHOGS_XE) $(LIB_ALL) $(DEVS_ALL) $(LIB_ONLY)
	$(ECHOGS_XE) -w $(ldt_tr) -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GS_XE)
	$(ECHOGS_XE) -a $(ldt_tr) -n -s $(LIB_ONLY) -s
	cat $(ld_tr) >>$(ldt_tr)
	$(ECHOGS_XE) -a $(ldt_tr) -s - $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; $(SH) <$(ldt_tr)

include $(GLSRCDIR)/unix-end.mak
