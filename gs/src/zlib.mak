#    Copyright (C) 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for zlib library code.

# This partial makefile compiles the zlib library for use in Ghostscript.
# You can get the source code for this library from:
#   ftp://ftp.uu.net/pub/archiving/zip/zlib/zlib104.zip   (zlib 1.0.4)
#		or zlib-1.0.4.tar.gz
# Please see Ghostscript's `make.txt' file for instructions about how to
# unpack these archives.

# Define the name of this makefile.
ZLIB_MAK=zlib.mak

# ZSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the zlib sources are stored.
#ZSRCDIR=zlib
ZSRC=$(ZSRCDIR)$(D)
# We would like to define
#CCCZ=$(CCC) -I$(ZSRCDIR) -Dverbose=-1
# but the Watcom C compiler has strange undocumented restrictions on what can
# follow a -D=, and it doesn't allow negative numbers.  Instead, we define
# (in gs.mak):
#CCCZ=$(CCC) -I. -I$(ZSRCDIR)
# and handle the definition of verbose in a different, more awkward way.

# We keep all of the zlib code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
ZDEP=$(AK)

# Contrary to what some portability bigots assert as fact, C compilers are
# not consistent about where they start searching for #included files:
# some always start by looking in the same directory as the .c file being
# compiled, before using the search path specified with -I on the command
# line, while others do not do this.  For this reason, we must explicitly
# copy and then delete all the .c files, because they need to obtain our
# modified version of zutil.h.  We must also copy all header files that
# reference zutil.h directly or indirectly.

# Code common to compression and decompression.

zlibc_=zutil.$(OBJ)
zlibc.dev: $(ZLIB_MAK) $(ECHOGS_XE) $(zlibc_)
	$(SETMOD) zlibc $(zlibc_)

zutil.h: $(ZSRC)zutil.h $(ECHOGS_XE)
	$(EXP)echogs -w zutil.h -x 23 define verbose -s - -1
	$(EXP)echogs -a zutil.h -+R $(ZSRC)zutil.h

zutil.$(OBJ): $(ZSRC)zutil.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)zutil.c .
	$(CCCZ) zutil.c
	$(RM_) zutil.c

# Encoding (compression) code.

deflate.h: $(ZSRC)deflate.h zutil.h
	$(CP_) $(ZSRC)deflate.h .

zlibe.dev: $(MAKEFILE) zlibe_$(SHARE_ZLIB).dev
	$(CP_) zlibe_$(SHARE_ZLIB).dev zlibe.dev

zlibe_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) zlibe_1 -lib $(ZLIB_NAME)

zlibe_=adler32.$(OBJ) deflate.$(OBJ) trees.$(OBJ)
zlibe_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) zlibc.dev $(zlibe_)
	$(SETMOD) zlibe_0 $(zlibe_)
	$(ADDMOD) zlibe_0 -include zlibc

adler32.$(OBJ): $(ZSRC)adler32.c $(ZDEP)
	$(CP_) $(ZSRC)adler32.c .
	$(CCCZ) adler32.c
	$(RM_) adler32.c

deflate.$(OBJ): $(ZSRC)deflate.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)deflate.c .
	$(CCCZ) deflate.c
	$(RM_) deflate.c

trees.$(OBJ): $(ZSRC)trees.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)trees.c .
	$(CCCZ) trees.c
	$(RM_) trees.c

# The zlib filters per se don't need crc32, but libpng versions starting
# with 0.90 do.

crc32.dev: $(MAKEFILE) crc32_$(SHARE_ZLIB).dev
	$(CP_) crc32_$(SHARE_ZLIB).dev crc32.dev

crc32_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) crc32_1 -lib $(ZLIB_NAME)

crc32_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) crc32.$(OBJ)
	$(SETMOD) crc32_0 crc32.$(OBJ)

crc32.$(OBJ): $(ZSRC)crc32.c $(ZDEP) deflate.h
	$(CP_) $(ZSRC)crc32.c .
	$(CCCZ) crc32.c
	$(RM_) crc32.c

# Decoding (decompression) code.

zlibd.dev: $(MAKEFILE) zlibd_$(SHARE_ZLIB).dev
	$(CP_) zlibd_$(SHARE_ZLIB).dev zlibd.dev

zlibd_1.dev: $(MAKEFILE) $(ZLIB_MAK) $(ECHOGS_XE)
	$(SETMOD) zlibd_1 -lib $(ZLIB_NAME)

zlibd1_=infblock.$(OBJ) infcodes.$(OBJ) inffast.$(OBJ)
zlibd2_=inflate.$(OBJ) inftrees.$(OBJ) infutil.$(OBJ)
zlibd_ = $(zlibd1_) $(zlibd2_)
zlibd_0.dev: $(ZLIB_MAK) $(ECHOGS_XE) zlibc.dev $(zlibd_)
	$(SETMOD) zlibd_0 $(zlibd1_)
	$(ADDMOD) zlibd_0 -obj $(zlibd2_)
	$(ADDMOD) zlibd_0 -include zlibc

infblock.$(OBJ): $(ZSRC)infblock.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infblock.c .
	$(CCCZ) infblock.c
	$(RM_) infblock.c

infcodes.$(OBJ): $(ZSRC)infcodes.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infcodes.c .
	$(CCCZ) infcodes.c
	$(RM_) infcodes.c

inffast.$(OBJ): $(ZSRC)inffast.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inffast.c .
	$(CCCZ) inffast.c
	$(RM_) inffast.c

inflate.$(OBJ): $(ZSRC)inflate.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inflate.c .
	$(CCCZ) inflate.c
	$(RM_) inflate.c

inftrees.$(OBJ): $(ZSRC)inftrees.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)inftrees.c .
	$(CCCZ) inftrees.c
	$(RM_) inftrees.c

infutil.$(OBJ): $(ZSRC)infutil.c $(ZDEP) zutil.h
	$(CP_) $(ZSRC)infutil.c .
	$(CCCZ) infutil.c
	$(RM_) infutil.c
