# Copyright (C) 2001-2019 Artifex Software, Inc.
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
# Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
# CA 94945, U.S.A., +1(415)492-9861, for further information.
#
# makefile for zlib library code.
# Users of this makefile must define the following:
#	GSSRCDIR - the GS library source directory
#	ZSRCDIR - the source directory
#	ZGENDIR - the generated intermediate file directory
#	ZOBJDIR - the object directory
#	SHARE_ZLIB - 0 to compile zlib, 1 to share
#	ZLIB_NAME - if SHARE_ZLIB=1, the name of the shared library
#	ZAUXDIR - the directory for auxiliary objects

# This partial makefile compiles the zlib library for use in Ghostscript.
# You can get the source code for this library from:
#   http://www.gzip.org/zlib/
#   http://www.libpng.org/pub/png/src/
#	zlib-#.#.#.tar.gz or zlib###.zip
# Please see Ghostscript's `Make.htm' file for instructions about how to
# unpack these archives.
#
# When each version of Ghostscript is released, we copy the associated
# version of the zlib library to
#	ftp://ftp.cs.wisc.edu/ghost/3rdparty/
# for more convenient access.
#
# This makefile is known to work with zlib source version 1.2.1.
# It will only work with earlier versions (1.1.4) when SHARE_ZLIB=1.
# Note that there are obscure bugs in zlib versions before 1.1.3 that
# may cause the FlateDecode filter to produce an occasional ioerror
# and there is a serious security problem with 1.1.3: we strongly
# recommend using version 1.1.4 or later.

ZSRC=$(ZSRCDIR)$(D)
ZGEN=$(ZGENDIR)$(D)
ZOBJ=$(ZOBJDIR)$(D)
ZAUX=$(ZAUXDIR)$(D)
ZO_=$(O_)$(ZOBJ)
ZAO_=$(O_)$(ZAUX)

# We need D_, _D_, and _D because the OpenVMS compiler uses different
# syntax from other compilers.
# ZI_ and ZF_ are defined in gs.mak.
ZCCFLAGS=$(ZLIB_CFLAGS) $(I_)$(ZI_)$(_I) $(ZF_) $(D_)verbose$(_D_)-1$(_D)
ZCC=$(CC_) $(ZCCFLAGS)
ZCCAUXFLAGS=$(ZLIB_CFLAGS) $(I_)$(ZI_)$(_I) $(ZF_) $(D_)verbose$(_D_)-1$(_D)
ZCCAUX=$(CCAUX_) $(ZCCAUXFLAGS)

# Define the name of this makefile.
ZLIB_MAK=$(GLSRC)zlib.mak $(TOP_MAKEFILES)

z.clean : z.config-clean z.clean-not-config-clean

### WRONG.  MUST DELETE OBJ AND GEN FILES SELECTIVELY.
z.clean-not-config-clean :
	$(RM_) $(ZOBJ)*.$(OBJ)

z.config-clean :
	$(RMN_) $(ZGEN)zlib*.dev $(ZGEN)crc32*.dev

ZDEP=$(AK) $(ZLIB_MAK) $(MAKEDIRS)

# Code common to compression and decompression.

zlibc_=$(ZOBJ)zutil.$(OBJ)
$(ZGEN)zlibc.dev : $(ZLIB_MAK) $(ECHOGS_XE) $(zlibc_)
	$(SETMOD) $(ZGEN)zlibc $(zlibc_)

$(ZOBJ)zutil.$(OBJ) : $(ZSRC)zutil.c $(ZDEP)
	$(ZCC) $(ZO_)zutil.$(OBJ) $(C_) $(ZSRC)zutil.c

# Encoding (compression) code.

$(ZGEN)zlibe.dev : $(ZLIB_MAK) $(ZGEN)zlibe_$(SHARE_ZLIB).dev \
 $(MAKEDIRS)
	$(CP_) $(ZGEN)zlibe_$(SHARE_ZLIB).dev $(ZGEN)zlibe.dev

$(ZGEN)zlibe_1.dev : $(ZLIB_MAK) $(ZLIB_MAK) $(ECHOGS_XE) \
 $(MAKEDIRS)
	$(SETMOD) $(ZGEN)zlibe_1 -lib $(ZLIB_NAME)

zlibe_=$(ZOBJ)adler32.$(OBJ) $(ZOBJ)deflate.$(OBJ) \
	$(ZOBJ)compress.$(OBJ) $(ZOBJ)trees.$(OBJ) $(ZOBJ)crc32.$(OBJ)
$(ZGEN)zlibe_0.dev : $(ZLIB_MAK) $(ECHOGS_XE) $(ZGEN)zlibc.dev $(zlibe_) \
 $(MAKEDIRS)
	$(SETMOD) $(ZGEN)zlibe_0 $(zlibe_)
	$(ADDMOD) $(ZGEN)zlibe_0 -include $(ZGEN)zlibc.dev

$(ZOBJ)adler32.$(OBJ) : $(ZSRC)adler32.c $(ZDEP)
	$(ZCC) $(ZO_)adler32.$(OBJ) $(C_) $(ZSRC)adler32.c

$(ZOBJ)deflate.$(OBJ) : $(ZSRC)deflate.c $(ZDEP)
	$(ZCC) $(ZO_)deflate.$(OBJ) $(C_) $(ZSRC)deflate.c

# new file in zlib 1.2.x
$(ZOBJ)compress.$(OBJ) : $(ZSRC)compress.c $(ZDEP)
	$(ZCC) $(ZO_)compress.$(OBJ) $(C_) $(ZSRC)compress.c

$(ZOBJ)trees.$(OBJ) : $(ZSRC)trees.c $(ZDEP)
	$(ZCC) $(ZO_)trees.$(OBJ) $(C_) $(ZSRC)trees.c

# The zlib filters per se don't need crc32, but libpng versions starting
# with 0.90 do.

$(ZGEN)crc32.dev : $(ZLIB_MAK) $(ZGEN)crc32_$(SHARE_ZLIB).dev \
 $(MAKEDIRS)
	$(CP_) $(ZGEN)crc32_$(SHARE_ZLIB).dev $(ZGEN)crc32.dev

$(ZGEN)crc32_1.dev : $(ZLIB_MAK) $(ZLIB_MAK) $(ECHOGS_XE) \
 $(MAKEDIRS)
	$(SETMOD) $(ZGEN)crc32_1 -lib $(ZLIB_NAME)

$(ZGEN)crc32_0.dev : $(ZLIB_MAK) $(ECHOGS_XE) $(ZOBJ)crc32.$(OBJ) \
 $(MAKEDIRS)
	$(SETMOD) $(ZGEN)crc32_0 $(ZOBJ)crc32.$(OBJ)

# We have to compile crc32 without warnings, because it defines 32-bit
# constants that produces gcc warnings with -Wtraditional.
$(ZOBJ)crc32.$(OBJ) : $(ZSRC)crc32.c $(ZDEP)
	$(CC_NO_WARN) $(ZCCFLAGS) $(ZO_)crc32.$(OBJ) $(C_) $(ZSRC)crc32.c

# Decoding (decompression) code.

$(ZGEN)zlibd.dev : $(ZLIB_MAK) $(ZGEN)zlibd_$(SHARE_ZLIB).dev $(MAKEDIRS)
	$(CP_) $(ZGEN)zlibd_$(SHARE_ZLIB).dev $(ZGEN)zlibd.dev

$(ZGEN)zlibd_1.dev : $(ZLIB_MAK) $(ZLIB_MAK) $(ECHOGS_XE) \
 $(MAKEDIRS)
	$(SETMOD) $(ZGEN)zlibd_1 -lib $(ZLIB_NAME)

# zlibd[12]_ list the decompression source files for zlib 1.4.x
zlibd1_=$(ZOBJ)infblock.$(OBJ) $(ZOBJ)infcodes.$(OBJ) $(ZOBJ)inffast.$(OBJ)
zlibd2_=$(ZOBJ)inflate.$(OBJ) $(ZOBJ)inftrees.$(OBJ) $(ZOBJ)infutil.$(OBJ) $(ZOBJ)crc32.$(OBJ)

# shorter list of files for zlib 1.2.x
zlibd_=$(ZOBJ)inffast.$(OBJ) $(ZOBJ)inflate.$(OBJ) $(ZOBJ)inftrees.$(OBJ) $(ZOBJ)uncompr.$(OBJ)


$(ZGEN)zlibd_0.dev : $(ZLIB_MAK) $(ECHOGS_XE) $(ZGEN)zlibc.dev $(zlibd_) $(MAKEDIRS)
	$(SETMOD) $(ZGEN)zlibd_0 $(zlibd_)
	$(ADDMOD) $(ZGEN)zlibd_0 -include $(ZGEN)zlibc.dev

$(ZOBJ)infblock.$(OBJ) : $(ZSRC)infblock.c $(ZDEP)
	$(ZCC) $(ZO_)infblock.$(OBJ) $(C_) $(ZSRC)infblock.c

$(ZOBJ)infcodes.$(OBJ) : $(ZSRC)infcodes.c $(ZDEP)
	$(ZCC) $(ZO_)infcodes.$(OBJ) $(C_) $(ZSRC)infcodes.c

$(ZOBJ)inffast.$(OBJ) : $(ZSRC)inffast.c $(ZDEP)
	$(ZCC) $(ZO_)inffast.$(OBJ) $(C_) $(ZSRC)inffast.c

$(ZOBJ)inflate.$(OBJ) : $(ZSRC)inflate.c $(ZDEP)
	$(ZCC) $(ZO_)inflate.$(OBJ) $(C_) $(ZSRC)inflate.c

$(ZOBJ)inftrees.$(OBJ) : $(ZSRC)inftrees.c $(ZDEP)
	$(ZCC) $(ZO_)inftrees.$(OBJ) $(C_) $(ZSRC)inftrees.c

$(ZOBJ)infutil.$(OBJ) : $(ZSRC)infutil.c $(ZDEP)
	$(ZCC) $(ZO_)infutil.$(OBJ) $(C_) $(ZSRC)infutil.c

# new file in zlib 1.2.x
$(ZOBJ)uncompr.$(OBJ) : $(ZSRC)uncompr.c $(ZDEP)
	$(ZCC) $(ZO_)uncompr.$(OBJ) $(C_) $(ZSRC)uncompr.c

# Auxiliary objects

$(ZAUX)adler32.$(OBJ) : $(ZSRC)adler32.c $(ZDEP)
	$(ZCCAUX) $(ZAO_)adler32.$(OBJ) $(C_) $(ZSRC)adler32.c

$(ZAUX)deflate.$(OBJ) : $(ZSRC)deflate.c $(ZDEP)
	$(ZCCAUX) $(ZAO_)deflate.$(OBJ) $(C_) $(ZSRC)deflate.c

# new file in zlib 1.2.x
$(ZAUX)compress.$(OBJ) : $(ZSRC)compress.c $(ZDEP)
	$(ZCCAUX) $(ZAO_)compress.$(OBJ) $(C_) $(ZSRC)compress.c

$(ZAUX)trees.$(OBJ) : $(ZSRC)trees.c $(ZDEP)
	$(ZCCAUX) $(ZAO_)trees.$(OBJ) $(C_) $(ZSRC)trees.c

$(ZAUX)zutil.$(OBJ) : $(ZSRC)zutil.c $(ZDEP)
	$(ZCCAUX) $(ZAO_)zutil.$(OBJ) $(C_) $(ZSRC)zutil.c

# We have to compile crc32 without warnings, because it defines 32-bit
# constants that produces gcc warnings with -Wtraditional.
$(ZAUX)crc32.$(OBJ) : $(ZSRC)crc32.c $(ZDEP)
	$(CCAUX_NO_WARN) $(ZCCAUXFLAGS) $(ZAO_)crc32.$(OBJ) $(C_) $(ZSRC)crc32.c
