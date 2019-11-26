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
# makefile for PNG (Portable Network Graphics) code.
# Users of this makefile must define the following:
#	ZSRCDIR - the zlib source directory
#	ZGENDIR - the zlib generated intermediate file directory
#	ZOBJDIR - the zlib object directory
#	PNGSRCDIR - the source directory
#	PNGGENDIR - the generated intermediate file directory
#	PNGOBJDIR - the object directory
#	SHARE_LIBPNG - 0 to compile libpng, 1 to share
#	LIBPNG_NAME - if SHARE_LIBPNG=1, the name of the shared library
#       PNG_CFLAGS

# This partial makefile compiles the png library for use in the Ghostscript
# PNG drivers.  You can get the source code for this library from:
#   http://www.libpng.org/pub/png/src/
#   ftp://swrinde.nde.swri.edu/pub/png/src/
#   ftp://ftp.uu.net/graphics/png/src/
#   http://libpng.sourceforge.net/
# Please see Ghostscript's `Make.htm' file for instructions about how to
# unpack these archives.
#
# When each version of Ghostscript is released, we copy the associated
# version of the png library to
#	ftp://ftp.cs.wisc.edu/ghost/3rdparty/
# for more convenient access.
#
# this makefile should work with libpng versions 0.90 and above
#
# NOTE: the archive for libpng 0.95 may be inconsistent: if you have
# compilation problems, use a newer version.
#
# Please see Ghostscript's Make.htm file for instructions about how to
# unpack these archives.
#
# The specification for the PNG file format is available from:
#   http://www.group42.com/png.htm
#   http://www.w3.org/pub/WWW/TR/WD-png

# (Rename directories.)
PNGSRC=$(PNGSRCDIR)$(D)
PNGGEN=$(PNGGENDIR)$(D)
PNGOBJ=$(PNGOBJDIR)$(D)
PNGO_=$(O_)$(PNGOBJ)
PZGEN=$(ZGENDIR)$(D)

# PI_ and PF_ are defined in gs.mak.
# NB: we can't use the normal $(CC_) here because msvccmd.mak
# adds /Za which conflicts with the libpng 1.5.x source.
PNGCC=$(CC) $(CFLAGS) $(PNG_CFLAGS) $(I_)$(PI_)$(_I) $(I_)$(PNGGENDIR)$(_I) $(PF_) \
$(D_)PNG_NO_ASSEMBLER_CODE$(_D) $(D_)PNG_INTEL_SSE_OPT=0$(_D) \
$(D_)PNG_INTEL_SSE_IMPLEMENTATION=0$(_D) $(D_)PNG_ARM_NEON_IMPLEMENTATION=0$(_D)

# Define the name of this makefile.
LIBPNG_MAK=$(GLSRC)png.mak $(TOP_MAKEFILES)

png.clean : png.config-clean png.clean-not-config-clean

### WRONG.  MUST DELETE OBJ AND GEN FILES SELECTIVELY.
png.clean-not-config-clean :
	$(RM_) $(PNGOBJ)*.$(OBJ)

pnglibconf_h=$(PNGGENDIR)$(D)pnglibconf.h

png.config-clean :
	$(RM_) $(pnglibconf_h)
	$(RM_) $(PNGGEN)lpg*.dev

$(pnglibconf_h) : $(PNGSRC)scripts$(D)pnglibconf.h.prebuilt $(TOP_MAKEFILES) $(MAKEDIRS)
	$(CP_)  $(PNGSRC)scripts$(D)pnglibconf.h.prebuilt $(pnglibconf_h)

PDEP=$(AK) $(pnglibconf_h) $(LIBPNG_MAK) $(MAKEDIRS)

png_1=$(PNGOBJ)png.$(OBJ) $(PNGOBJ)pngmem.$(OBJ) $(PNGOBJ)pngerror.$(OBJ) $(PNGOBJ)pngset.$(OBJ)
png_2=$(PNGOBJ)pngtrans.$(OBJ) $(PNGOBJ)pngwrite.$(OBJ) $(PNGOBJ)pngwtran.$(OBJ) $(PNGOBJ)pngwutil.$(OBJ) $(PNGOBJ)pngwio.$(OBJ)
png_3=$(PNGOBJ)pngread.$(OBJ) $(PNGOBJ)pngrutil.$(OBJ) $(PNGOBJ)pngrtran.$(OBJ) $(PNGOBJ)pngrio.$(OBJ) $(PNGOBJ)pngget.$(OBJ)

# libpng modules

$(PNGOBJ)png.$(OBJ) : $(PNGSRC)png.c $(PDEP)
	$(PNGCC) $(PNGO_)png.$(OBJ) $(C_) $(PNGSRC)png.c

$(PNGOBJ)pngwio.$(OBJ) : $(PNGSRC)pngwio.c $(PDEP)
	$(PNGCC) $(PNGO_)pngwio.$(OBJ) $(C_) $(PNGSRC)pngwio.c

$(PNGOBJ)pngmem.$(OBJ) : $(PNGSRC)pngmem.c $(PDEP)
	$(PNGCC) $(PNGO_)pngmem.$(OBJ) $(C_) $(PNGSRC)pngmem.c

$(PNGOBJ)pngerror.$(OBJ) : $(PNGSRC)pngerror.c $(PDEP)
	$(PNGCC) $(PNGO_)pngerror.$(OBJ) $(C_) $(PNGSRC)pngerror.c

$(PNGOBJ)pngset.$(OBJ) : $(PNGSRC)pngset.c $(PDEP)
	$(PNGCC) $(PNGO_)pngset.$(OBJ) $(C_) $(PNGSRC)pngset.c

$(PNGOBJ)pngtrans.$(OBJ) : $(PNGSRC)pngtrans.c $(PDEP)
	$(PNGCC) $(PNGO_)pngtrans.$(OBJ) $(C_) $(PNGSRC)pngtrans.c

$(PNGOBJ)pngwrite.$(OBJ) : $(PNGSRC)pngwrite.c $(PDEP)
	$(PNGCC) $(PNGO_)pngwrite.$(OBJ) $(C_) $(PNGSRC)pngwrite.c

$(PNGOBJ)pngwtran.$(OBJ) : $(PNGSRC)pngwtran.c $(PDEP)
	$(PNGCC) $(PNGO_)pngwtran.$(OBJ) $(C_) $(PNGSRC)pngwtran.c

$(PNGOBJ)pngwutil.$(OBJ) : $(PNGSRC)pngwutil.c $(PDEP)
	$(PNGCC) $(PNGO_)pngwutil.$(OBJ) $(C_) $(PNGSRC)pngwutil.c

$(PNGOBJ)pngread.$(OBJ) : $(PNGSRC)pngread.c $(PDEP)
	$(PNGCC) $(PNGO_)pngread.$(OBJ) $(C_) $(PNGSRC)pngread.c

$(PNGOBJ)pngrutil.$(OBJ) : $(PNGSRC)pngrutil.c $(PDEP)
	$(PNGCC) $(PNGO_)pngrutil.$(OBJ) $(C_) $(PNGSRC)pngrutil.c

$(PNGOBJ)pngrtran.$(OBJ) : $(PNGSRC)pngrtran.c $(PDEP)
	$(PNGCC) $(PNGO_)pngrtran.$(OBJ) $(C_) $(PNGSRC)pngrtran.c

$(PNGOBJ)pngrio.$(OBJ) : $(PNGSRC)pngrio.c $(PDEP)
	$(PNGCC) $(PNGO_)pngrio.$(OBJ) $(C_) $(PNGSRC)pngrio.c

$(PNGOBJ)pngget.$(OBJ) : $(PNGSRC)pngget.c $(PDEP)
	$(PNGCC) $(PNGO_)pngget.$(OBJ) $(C_) $(PNGSRC)pngget.c


# Define the version of libpng.dev that we are actually using.
$(PNGGEN)libpng.dev : $(PNGGEN)libpng_$(SHARE_LIBPNG).dev $(LIBPNG_MAK) $(MAKEDIRS)
	$(CP_) $(PNGGEN)libpng_$(SHARE_LIBPNG).dev $(PNGGEN)libpng.dev

# Define the shared version of libpng.
# Note that it requires libz, which must be searched *after* libpng.
$(PNGGEN)libpng_1.dev : $(ECHOGS_XE) $(PZGEN)zlibe.dev \
 $(LIBPNG_MAK) $(MAKEDIRS)
	$(SETMOD) $(PNGGEN)libpng_1 -lib $(LIBPNG_NAME)
	$(ADDMOD) $(PNGGEN)libpng_1 -include $(PZGEN)zlibe.dev

# Define the non-shared version of libpng.
$(PNGGEN)libpng_0.dev : $(LIBPNG_MAK) $(ECHOGS_XE) $(png_1) $(png_2) $(png_3)\
 $(PZGEN)zlibe.dev $(PNGOBJ)pngwio.$(OBJ) $(PZGEN)crc32.dev $(MAKEDIRS)
	$(SETMOD) $(PNGGEN)libpng_0 $(png_1)
	$(ADDMOD) $(PNGGEN)libpng_0 $(png_2)
	$(ADDMOD) $(PNGGEN)libpng_0 $(png_3)
	$(ADDMOD) $(PNGGEN)libpng_0 $(PNGOBJ)pngwio.$(OBJ)
	$(ADDMOD) $(PNGGEN)libpng_0 -include $(PZGEN)zlibe.dev $(PZGEN)crc32.dev

