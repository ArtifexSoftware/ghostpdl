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

# makefile for PNG (Portable Network Graphics) code.

# This partial makefile compiles the png library for use in the Ghostscript
# PNG drivers.  You can get the source code for this library from:
#   ftp://swrinde.nde.swri.edu/pub/png/src/
# The makefile is known to work with the following library versions:
# 0.89, 0.90, 0.95, and 0.96.  NOTE: the archive for libpng 0.95 may
# be inconsistent: if you have compilation problems, use an older version.
# Please see Ghostscript's `make.txt' file for instructions about how to
# unpack these archives.
#
# The specification for the PNG file format is available from:
#   http://www.group42.com/png.htm
#   http://www.w3.org/pub/WWW/TR/WD-png

# Define the name of this makefile.
LIBPNG_MAK=libpng.mak

# PSRCDIR is defined in the platform-specific makefile, not here,
# as the directory where the PNG library sources are stored.
#PSRCDIR=libpng
# PVERSION is defined in the platform-specific makefile, not here,
# as the libpng version number ("89", "90", "95", or "96").
#PVERSION=96

PSRC=$(PSRCDIR)$(D)
# CCCP is defined in gs.mak.
#CCCP=$(CCC) -I$(PSRCDIR) -I$(ZSRCDIR)

# We keep all of the PNG code in a separate directory so as not to
# inadvertently mix it up with Aladdin Enterprises' own code.
PDEP=$(AK)

png_1=png.$(OBJ) pngmem.$(OBJ) pngerror.$(OBJ)
png_2=pngtrans.$(OBJ) pngwrite.$(OBJ) pngwtran.$(OBJ) pngwutil.$(OBJ)

# libpng modules

png.$(OBJ): $(PSRC)png.c $(PDEP)
	$(CCCP) $(PSRC)png.c

# version 0.89 uses pngwio.c
pngwio.$(OBJ): $(PSRC)pngwio.c $(PDEP)
	$(CCCP) $(PSRC)pngwio.c

pngmem.$(OBJ): $(PSRC)pngmem.c $(PDEP)
	$(CCCP) $(PSRC)pngmem.c

pngerror.$(OBJ): $(PSRC)pngerror.c $(PDEP)
	$(CCCP) $(PSRC)pngerror.c

pngtrans.$(OBJ): $(PSRC)pngtrans.c $(PDEP)
	$(CCCP) $(PSRC)pngtrans.c

pngwrite.$(OBJ): $(PSRC)pngwrite.c $(PDEP)
	$(CCCP) $(PSRC)pngwrite.c

pngwtran.$(OBJ): $(PSRC)pngwtran.c $(PDEP)
	$(CCCP) $(PSRC)pngwtran.c

pngwutil.$(OBJ): $(PSRC)pngwutil.c $(PDEP)
	$(CCCP) $(PSRC)pngwutil.c

# Define the version of libpng.dev that we are actually using.
libpng.dev: $(MAKEFILE) libpng_$(SHARE_LIBPNG).dev
	$(CP_) libpng_$(SHARE_LIBPNG).dev libpng.dev

# Define the shared version of libpng.
# Note that it requires libz, which must be searched *after* libpng.
libpng_1.dev: $(MAKEFILE) $(LIBPNG_MAK) $(ECHOGS_XE) zlibe.dev
	$(SETMOD) libpng_1 -lib $(LIBPNG_NAME)
	$(ADDMOD) libpng_1 -include zlibe

# Define the non-shared version of libpng.
libpng_0.dev: $(LIBPNG_MAK) $(ECHOGS_XE) $(png_1) $(png_2)\
 zlibe.dev libpng$(PVERSION).dev
	$(SETMOD) libpng_0 $(png_1)
	$(ADDMOD) libpng_0 $(png_2)
	$(ADDMOD) libpng_0 -include zlibe libpng$(PVERSION)

libpng89.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ)
	$(SETMOD) libpng89 pngwio.$(OBJ)

libpng90.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng90 pngwio.$(OBJ) -include crc32

libpng95.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng95 pngwio.$(OBJ) -include crc32

libpng96.dev: $(LIBPNG_MAK) $(ECHOGS_XE) pngwio.$(OBJ) crc32.dev
	$(SETMOD) libpng96 pngwio.$(OBJ) -include crc32
