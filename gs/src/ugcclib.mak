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

include version.mak

gsdir = /usr/local/share/ghostscript
gsdatadir = $(gsdir)/$(GS_DOT_VERSION)
GS_DOCDIR=$(gsatadir)/doc
GS_LIB_DEFAULT=$(gsdatadir):$(gsdir)/fonts
SEARCH_HERE_FIRST=1
GS_INIT=gs_init.ps

GENOPT=-DDEBUG
GS=gslib

JSRCDIR=jpeg-6a
JVERSION=6

PSRCDIR=libpng
PVERSION=96
SHARE_LIBPNG=0
LIBPNG_NAME=png

ZSRCDIR=zlib
SHARE_ZLIB=0
ZLIB_NAME=z

CONFIG=

CC=gcc
CCLD=$(CC)

#GCFLAGS=-Wall -Wpointer-arith -Wstrict-prototypes -Wwrite-strings
GCFLAGS=-Dconst= -Wall -Wpointer-arith -Wstrict-prototypes
CFLAGS=-g -O0 $(GCFLAGS) $(XCFLAGS)
LDFLAGS=$(XLDFLAGS)
EXTRALIBS=
XINCLUDE=-I/usr/local/X/include
XLIBDIRS=-L/usr/X11/lib
XLIBDIR=
XLIBS=Xt Xext X11

FPU_TYPE=1

FEATURE_DEVS=dps2lib.dev psl2cs.dev cielib.dev patlib.dev
COMPILE_INITS=0
BAND_LIST_STORAGE=file
BAND_LIST_COMPRESSOR=zlib
FILE_IMPLEMENTATION=stdio
DEVICE_DEVS=x11cmyk.dev x11.dev x11alpha.dev x11mono.dev\
 djet500.dev\
 pbmraw.dev pgmraw.dev ppmraw.dev
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

MAKEFILE=ugcclib.mak

AK=
CCAUX=$(CC)
CCC=$(CC) $(CCFLAGS) -c
CCLEAF=$(CCC)
# When using gcc, CCA2K isn't needed....
CCA2K=$(CC)

include unixhead.mak

include gs.mak
include lib.mak
include jpeg.mak
include libpng.mak
include zlib.mak
include devs.mak

# Following is from unixtail.mak, we have a different link step.
unix__=gp_nofb.o gp_unix.o gp_unifs.o gp_unifn.o
unix_.dev: $(unix__)
	$(SETMOD) unix_ $(unix__)

gp_unix.o: gp_unix.c $(AK) $(string__h) $(gx_h) $(gsexit_h) $(gp_h) \
  $(time__h)

sysv__=gp_nofb.o gp_unix.o gp_unifs.o gp_unifn.o gp_sysv.o
sysv_.dev: $(sysv__)
	$(SETMOD) sysv_ $(sysv__)

gp_sysv.o: gp_sysv.c $(time__h) $(AK)

ansi2knr: ansi2knr.c $(stdio__h) $(string__h) $(malloc__h)
	$(CCA2K) $(O)ansi2knr ansi2knr.c

echogs: echogs.c
	$(CCAUX) $(O)echogs echogs.c

genarch: genarch.c $(stdpre_h)
	$(CCAUX) $(O)genarch genarch.c

genconf: genconf.c $(stdpre_h)
	$(CCAUX) $(O)genconf genconf.c

geninit: geninit.c $(stdio__h) $(string__h)
	$(CCAUX) $(O)geninit geninit.c

INCLUDE=/usr/include
gconfig_.h: unixtail.mak echogs
	./echogs -w gconfig_.h -x 2f2a -s This file was generated automatically. -s -x 2a2f
	sh -c 'if ( test -f $(INCLUDE)/dirent.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_DIRENT_H; fi'
	sh -c 'if ( test -f $(INCLUDE)/ndir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_NDIR_H; fi'
	sh -c 'if ( test -f $(INCLUDE)/sys/dir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_DIR_H; fi'
	sh -c 'if ( test -f $(INCLUDE)/sys/ndir.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_NDIR_H; fi'
	sh -c 'if ( test -f $(INCLUDE)/sys/time.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_TIME_H; fi'
	sh -c 'if ( test -f $(INCLUDE)/sys/times.h ); then ./echogs -a gconfig_.h -x 23 define HAVE_SYS_TIMES_H; fi'

LIB_ONLY=gslib.o gsnogc.o gconfig.o gscdefs.o
$(GS): $(ld_tr) echogs $(LIB_ALL) $(DEVS_ALL) $(LIB_ONLY)
	./echogs -w ldt.tr -n - $(CCLD) $(LDFLAGS) $(XLIBDIRS) -o $(GS)
	./echogs -a ldt.tr -n -s $(LIB_ONLY) -s
	cat $(ld_tr) >>ldt.tr
	./echogs -a ldt.tr -s - $(EXTRALIBS) -lm
	LD_RUN_PATH=$(XLIBDIR); export LD_RUN_PATH; $(SH) <ldt.tr

# Following is from unix-end.mak, we omit the install and tar_cat rules.
pg:
	make GENOPT='' CFLAGS='-pg -O $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' CCLEAF='$(CCC)'

gconfigv.h: $(MAKEFILE) echogs
	$(EXP)echogs -w gconfigv.h -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)

