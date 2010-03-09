#  Copyright (C) 2001-2010 Artifex Software, Inc.
#  All Rights Reserved.
#
#  This software is provided AS-IS with no warranty, either express or
#  implied.
#
#  This software is distributed under license and may not be copied, modified
#  or distributed except as expressly authorized under the terms of that
#  license.  Refer to licensing information at http://www.artifex.com/
#  or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
#  San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
#
# $Id$
# makefile for freetype as part of the monolithic gs build.
#
# Users of this makefile must define the following:
#	FTSRCDIR    - the source directory
#	FTGENDIR    - the generated intermediate file directory
#	FTOBJDIR    - the object file directory
#	FT_CFLAGS   - The include options for the freetype library
#	SHARE_FT - 0 to compile in freetype, 1 to link a shared library
#	FT_LIBS  - if SHARE_FT=1, the link options for the shared library

# (Rename directories.)
FTSRC=$(FTSRCDIR)$(D)src$(D)
FTGEN=$(FTGENDIR)$(D)
FTOBJ=$(FTOBJDIR)$(D)
FTO_=$(O_)$(FTOBJ)

# Define our local compiler alias
# we must define FT2_BUILD_LIBRARY to get internal declarations
FTCC=$(CC_) $(I_)$(FTSRCDIR)$(D)include$(_I) -DFT2_BUILD_LIBRARY

# Define the name of this makefile.
FT_MAK=$(GLSRC)freetype.mak

# file complements for each component
ft_autofit=\
	$(FTOBJ)afangles.$(OBJ) \
	$(FTOBJ)afcjk.$(OBJ) \
	$(FTOBJ)afdummy.$(OBJ) \
	$(FTOBJ)afglobal.$(OBJ) \
	$(FTOBJ)afhints.$(OBJ) \
	$(FTOBJ)afindic.$(OBJ) \
	$(FTOBJ)aflatin.$(OBJ) \
	$(FTOBJ)afloader.$(OBJ) \
	$(FTOBJ)afmodule.$(OBJ) \
	$(FTOBJ)afwarp.$(OBJ)

ft_base=\
	$(FTOBJ)ftadvanc.$(OBJ) \
	$(FTOBJ)ftcalc.$(OBJ) \
	$(FTOBJ)ftdbgmem.$(OBJ) \
	$(FTOBJ)ftgloadr.$(OBJ) \
	$(FTOBJ)ftobjs.$(OBJ) \
	$(FTOBJ)ftoutln.$(OBJ) \
	$(FTOBJ)ftrfork.$(OBJ) \
	$(FTOBJ)ftsnames.$(OBJ) \
	$(FTOBJ)ftstream.$(OBJ) \
	$(FTOBJ)fttrigon.$(OBJ) \
	$(FTOBJ)ftutil.$(OBJ) \
	$(FTOBJ)ftbbox.$(OBJ) \
	$(FTOBJ)ftbdf.$(OBJ) \
	$(FTOBJ)ftbitmap.$(OBJ) \
	$(FTOBJ)ftdebug.$(OBJ) \
	$(FTOBJ)ftgasp.$(OBJ) \
	$(FTOBJ)ftglyph.$(OBJ) \
	$(FTOBJ)ftgxval.$(OBJ) \
	$(FTOBJ)ftinit.$(OBJ) \
	$(FTOBJ)ftlcdfil.$(OBJ) \
	$(FTOBJ)ftmm.$(OBJ) \
	$(FTOBJ)ftotval.$(OBJ) \
	$(FTOBJ)ftpfr.$(OBJ) \
	$(FTOBJ)ftstroke.$(OBJ) \
	$(FTOBJ)ftsynth.$(OBJ) \
	$(FTOBJ)ftsystem.$(OBJ) \
	$(FTOBJ)fttype1.$(OBJ) \
	$(FTOBJ)ftwinfnt.$(OBJ) \
	$(FTOBJ)ftxf86.$(OBJ) \
	$(FTOBJ)ftpatent.$(OBJ)

ft_bdf=\
	$(FTOBJ)bdflib.$(OBJ) \
	$(FTOBJ)bdfdrivr.$(OBJ)

ft_cache=\
	$(FTOBJ)ftcbasic.$(OBJ) \
	$(FTOBJ)ft2ccache.$(OBJ) \
	$(FTOBJ)ftccmap.$(OBJ) \
	$(FTOBJ)ftcglyph.$(OBJ) \
	$(FTOBJ)ftcimage.$(OBJ) \
	$(FTOBJ)ftcmanag.$(OBJ) \
	$(FTOBJ)ftcmru.$(OBJ) \
	$(FTOBJ)ftcsbits.$(OBJ)

ft_cff=\
	$(FTOBJ)cffobjs.$(OBJ) \
	$(FTOBJ)cffload.$(OBJ) \
	$(FTOBJ)cffgload.$(OBJ) \
	$(FTOBJ)cffparse.$(OBJ) \
	$(FTOBJ)cffcmap.$(OBJ) \
	$(FTOBJ)cffdrivr.$(OBJ)

ft_cid=\
	$(FTOBJ)cidparse.$(OBJ) \
	$(FTOBJ)cidload.$(OBJ) \
	$(FTOBJ)cidriver.$(OBJ) \
	$(FTOBJ)cidgload.$(OBJ) \
	$(FTOBJ)cidobjs.$(OBJ)

ft_gzip=$(FTOBJ)ftgzip.$(OBJ)

ft_lzw=$(FTOBJ)ftlzw.$(OBJ)

ft_pcf=\
	$(FTOBJ)pcfdrivr.$(OBJ) \
	$(FTOBJ)pcfread.$(OBJ) \
	$(FTOBJ)pcfutil.$(OBJ)

ft_pfr=\
	$(FTOBJ)pfrload.$(OBJ) \
	$(FTOBJ)pfrgload.$(OBJ) \
	$(FTOBJ)pfrcmap.$(OBJ) \
	$(FTOBJ)pfrdrivr.$(OBJ) \
	$(FTOBJ)pfrsbit.$(OBJ) \
	$(FTOBJ)pfrobjs.$(OBJ)

ft_psaux=\
	$(FTOBJ)psobjs.$(OBJ) \
	$(FTOBJ)t1decode.$(OBJ) \
	$(FTOBJ)t1cmap.$(OBJ) \
	$(FTOBJ)afmparse.$(OBJ) \
	$(FTOBJ)psconv.$(OBJ) \
	$(FTOBJ)psauxmod.$(OBJ)

ft_pshinter=\
	$(FTOBJ)pshrec.$(OBJ) \
	$(FTOBJ)pshglob.$(OBJ) \
	$(FTOBJ)pshmod.$(OBJ) \
	$(FTOBJ)pshalgo.$(OBJ)

ft_psnames=\
	$(FTOBJ)psmodule.$(OBJ) \
	$(FTOBJ)pspic.$(OBJ)

ft_raster=\
	$(FTOBJ)ftraster.$(OBJ) \
	$(FTOBJ)ftrend1.$(OBJ) \
	$(FTOBJ)rastpic.$(OBJ)

ft_smooth=\
	$(FTOBJ)ftgrays.$(OBJ) \
	$(FTOBJ)ftsmooth.$(OBJ) \
	$(FTOBJ)ftspic.$(OBJ)

ft_sfnt=\
	$(FTOBJ)sfobjs.$(OBJ) \
	$(FTOBJ)sfdriver.$(OBJ) \
	$(FTOBJ)ttcmap.$(OBJ) \
	$(FTOBJ)ttmtx.$(OBJ) \
	$(FTOBJ)ttpost.$(OBJ) \
	$(FTOBJ)ft2ttload.$(OBJ) \
	$(FTOBJ)ttsbit.$(OBJ) \
	$(FTOBJ)ttkern.$(OBJ) \
	$(FTOBJ)ttbdf.$(OBJ) \
	$(FTOBJ)sfntpic.$(OBJ)

ft_truetype=\
	$(FTOBJ)ttdriver.$(OBJ) \
	$(FTOBJ)ft2ttobjs.$(OBJ) \
	$(FTOBJ)ttpload.$(OBJ) \
	$(FTOBJ)ttgload.$(OBJ) \
	$(FTOBJ)ft2ttinterp.$(OBJ) \
	$(FTOBJ)ttgxvar.$(OBJ) \
	$(FTOBJ)ttpic.$(OBJ)

ft_type1=\
	$(FTOBJ)t1afm.$(OBJ) \
	$(FTOBJ)t1driver.$(OBJ) \
	$(FTOBJ)t1objs.$(OBJ) \
	$(FTOBJ)t1load.$(OBJ) \
	$(FTOBJ)t1gload.$(OBJ) \
	$(FTOBJ)t1parse.$(OBJ)

ft_type42=\
	$(FTOBJ)t42objs.$(OBJ) \
	$(FTOBJ)t42parse.$(OBJ) \
	$(FTOBJ)t42drivr.$(OBJ)

ft_winfonts=$(FTOBJ)winfnt.$(OBJ)

# instantiate the requested build option (shared or compiled in)
$(FTGEN)freetype.dev : $(TOP_MAKEFILES) $(FTGEN)freetype_$(SHARE_FT).dev
	$(CP_) $(FTGEN)freetype_$(SHARE_FT).dev $(FTGEN)freetype.dev

# Define the shared version.
$(FTGEN)freetype_1.dev : $(TOP_MAKEFILES) $(FT_MAK) $(ECHOGS_XE)
	$(SETMOD) $(FTGEN)freetype_1 -link $(FT_LIBS)

# Define the non-shared version.
$(FTGEN)freetype_0.dev : $(TOP_MAKEFILES) $(FT_MAK) $(ECHOGS_XE) \
    $(ft_autofit) $(ft_base) $(ft_bdf) $(ft_cache) $(ft_cff) $(ft_cid) \
    $(ft_gzip) $(ft_lzw) $(ft_pcf) $(ft_pfr) $(ft_psaux) $(ft_pshinter) \
    $(ft_psnames) $(ft_raster) $(ft_smooth) $(ft_sfnt) $(ft_truetype) \
    $(ft_type1) $(ft_type42) $(ft_winfonts)
	$(SETMOD) $(FTGEN)freetype_0 $(ft_autofit)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_base)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_bdf)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_cache)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_cff)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_cid)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_gzip)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_lzw)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_pcf)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_pfr)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_psaux)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_pshinter)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_psnames)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_raster)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_smooth)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_sfnt)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_truetype)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_type1)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_type42)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_winfonts)


# custom build rules for each source file

$(FTOBJ)afangles.$(OBJ) : $(FTSRC)autofit$(D)afangles.c
	$(FTCC) $(FTO_)afangles.$(OBJ) $(C_) $(FTSRC)autofit$(D)afangles.c

$(FTOBJ)afcjk.$(OBJ) : $(FTSRC)autofit$(D)afcjk.c
	$(FTCC) $(FTO_)afcjk.$(OBJ) $(C_) $(FTSRC)autofit$(D)afcjk.c

$(FTOBJ)afdummy.$(OBJ) : $(FTSRC)autofit$(D)afdummy.c
	$(FTCC) $(FTO_)afdummy.$(OBJ) $(C_) $(FTSRC)autofit$(D)afdummy.c

$(FTOBJ)afglobal.$(OBJ) : $(FTSRC)autofit$(D)afglobal.c
	$(FTCC) $(FTO_)afglobal.$(OBJ) $(C_) $(FTSRC)autofit$(D)afglobal.c

$(FTOBJ)afhints.$(OBJ) : $(FTSRC)autofit$(D)afhints.c
	$(FTCC) $(FTO_)afhints.$(OBJ) $(C_) $(FTSRC)autofit$(D)afhints.c

$(FTOBJ)afindic.$(OBJ) : $(FTSRC)autofit$(D)afindic.c
	$(FTCC) $(FTO_)afindic.$(OBJ) $(C_) $(FTSRC)autofit$(D)afindic.c

$(FTOBJ)aflatin.$(OBJ) : $(FTSRC)autofit$(D)aflatin.c
	$(FTCC) $(FTO_)aflatin.$(OBJ) $(C_) $(FTSRC)autofit$(D)aflatin.c

$(FTOBJ)afloader.$(OBJ) : $(FTSRC)autofit$(D)afloader.c
	$(FTCC) $(FTO_)afloader.$(OBJ) $(C_) $(FTSRC)autofit$(D)afloader.c

$(FTOBJ)afmodule.$(OBJ) : $(FTSRC)autofit$(D)afmodule.c
	$(FTCC) $(FTO_)afmodule.$(OBJ) $(C_) $(FTSRC)autofit$(D)afmodule.c

$(FTOBJ)afwarp.$(OBJ) : $(FTSRC)autofit$(D)afwarp.c
	$(FTCC) $(FTO_)afwarp.$(OBJ) $(C_) $(FTSRC)autofit$(D)afwarp.c


$(FTOBJ)ftadvanc.$(OBJ) : $(FTSRC)base$(D)ftadvanc.c
	$(FTCC) $(FTO_)ftadvanc.$(OBJ) $(C_) $(FTSRC)base$(D)ftadvanc.c

$(FTOBJ)ftcalc.$(OBJ) : $(FTSRC)base$(D)ftcalc.c
	$(FTCC) $(FTO_)ftcalc.$(OBJ) $(C_) $(FTSRC)base$(D)ftcalc.c

$(FTOBJ)ftdbgmem.$(OBJ) : $(FTSRC)base$(D)ftdbgmem.c
	$(FTCC) $(FTO_)ftdbgmem.$(OBJ) $(C_) $(FTSRC)base$(D)ftdbgmem.c

$(FTOBJ)ftgloadr.$(OBJ) : $(FTSRC)base$(D)ftgloadr.c
	$(FTCC) $(FTO_)ftgloadr.$(OBJ) $(C_) $(FTSRC)base$(D)ftgloadr.c

$(FTOBJ)ftobjs.$(OBJ) : $(FTSRC)base$(D)ftobjs.c
	$(FTCC) $(FTO_)ftobjs.$(OBJ) $(C_) $(FTSRC)base$(D)ftobjs.c

$(FTOBJ)ftoutln.$(OBJ) : $(FTSRC)base$(D)ftoutln.c
	$(FTCC) $(FTO_)ftoutln.$(OBJ) $(C_) $(FTSRC)base$(D)ftoutln.c

$(FTOBJ)ftrfork.$(OBJ) : $(FTSRC)base$(D)ftrfork.c
	$(FTCC) $(FTO_)ftrfork.$(OBJ) $(C_) $(FTSRC)base$(D)ftrfork.c

$(FTOBJ)ftsnames.$(OBJ) : $(FTSRC)base$(D)ftsnames.c
	$(FTCC) $(FTO_)ftsnames.$(OBJ) $(C_) $(FTSRC)base$(D)ftsnames.c

$(FTOBJ)ftstream.$(OBJ) : $(FTSRC)base$(D)ftstream.c
	$(FTCC) $(FTO_)ftstream.$(OBJ) $(C_) $(FTSRC)base$(D)ftstream.c

$(FTOBJ)fttrigon.$(OBJ) : $(FTSRC)base$(D)fttrigon.c
	$(FTCC) $(FTO_)fttrigon.$(OBJ) $(C_) $(FTSRC)base$(D)fttrigon.c

$(FTOBJ)ftutil.$(OBJ) : $(FTSRC)base$(D)ftutil.c
	$(FTCC) $(FTO_)ftutil.$(OBJ) $(C_) $(FTSRC)base$(D)ftutil.c

$(FTOBJ)ftbbox.$(OBJ) : $(FTSRC)base$(D)ftbbox.c
	$(FTCC) $(FTO_)ftbbox.$(OBJ) $(C_) $(FTSRC)base$(D)ftbbox.c

$(FTOBJ)ftbdf.$(OBJ) : $(FTSRC)base$(D)ftbdf.c
	$(FTCC) $(FTO_)ftbdf.$(OBJ) $(C_) $(FTSRC)base$(D)ftbdf.c

$(FTOBJ)ftbitmap.$(OBJ) : $(FTSRC)base$(D)ftbitmap.c
	$(FTCC) $(FTO_)ftbitmap.$(OBJ) $(C_) $(FTSRC)base$(D)ftbitmap.c

$(FTOBJ)ftdebug.$(OBJ) : $(FTSRC)base$(D)ftdebug.c
	$(FTCC) $(FTO_)ftdebug.$(OBJ) $(C_) $(FTSRC)base$(D)ftdebug.c

$(FTOBJ)ftgasp.$(OBJ) : $(FTSRC)base$(D)ftgasp.c
	$(FTCC) $(FTO_)ftgasp.$(OBJ) $(C_) $(FTSRC)base$(D)ftgasp.c

$(FTOBJ)ftglyph.$(OBJ) : $(FTSRC)base$(D)ftglyph.c
	$(FTCC) $(FTO_)ftglyph.$(OBJ) $(C_) $(FTSRC)base$(D)ftglyph.c

$(FTOBJ)ftgxval.$(OBJ) : $(FTSRC)base$(D)ftgxval.c
	$(FTCC) $(FTO_)ftgxval.$(OBJ) $(C_) $(FTSRC)base$(D)ftgxval.c

$(FTOBJ)ftinit.$(OBJ) : $(FTSRC)base$(D)ftinit.c
	$(FTCC) $(FTO_)ftinit.$(OBJ) $(C_) $(FTSRC)base$(D)ftinit.c

$(FTOBJ)ftlcdfil.$(OBJ) : $(FTSRC)base$(D)ftlcdfil.c
	$(FTCC) $(FTO_)ftlcdfil.$(OBJ) $(C_) $(FTSRC)base$(D)ftlcdfil.c

$(FTOBJ)ftmm.$(OBJ) : $(FTSRC)base$(D)ftmm.c
	$(FTCC) $(FTO_)ftmm.$(OBJ) $(C_) $(FTSRC)base$(D)ftmm.c

$(FTOBJ)ftotval.$(OBJ) : $(FTSRC)base$(D)ftotval.c
	$(FTCC) $(FTO_)ftotval.$(OBJ) $(C_) $(FTSRC)base$(D)ftotval.c

$(FTOBJ)ftpfr.$(OBJ) : $(FTSRC)base$(D)ftpfr.c
	$(FTCC) $(FTO_)ftpfr.$(OBJ) $(C_) $(FTSRC)base$(D)ftpfr.c

$(FTOBJ)ftstroke.$(OBJ) : $(FTSRC)base$(D)ftstroke.c
	$(FTCC) $(FTO_)ftstroke.$(OBJ) $(C_) $(FTSRC)base$(D)ftstroke.c

$(FTOBJ)ftsynth.$(OBJ) : $(FTSRC)base$(D)ftsynth.c
	$(FTCC) $(FTO_)ftsynth.$(OBJ) $(C_) $(FTSRC)base$(D)ftsynth.c

$(FTOBJ)ftsystem.$(OBJ) : $(FTSRC)base$(D)ftsystem.c
	$(FTCC) $(FTO_)ftsystem.$(OBJ) $(C_) $(FTSRC)base$(D)ftsystem.c

$(FTOBJ)fttype1.$(OBJ) : $(FTSRC)base$(D)fttype1.c
	$(FTCC) $(FTO_)fttype1.$(OBJ) $(C_) $(FTSRC)base$(D)fttype1.c

$(FTOBJ)ftwinfnt.$(OBJ) : $(FTSRC)base$(D)ftwinfnt.c
	$(FTCC) $(FTO_)ftwinfnt.$(OBJ) $(C_) $(FTSRC)base$(D)ftwinfnt.c

$(FTOBJ)ftxf86.$(OBJ) : $(FTSRC)base$(D)ftxf86.c
	$(FTCC) $(FTO_)ftxf86.$(OBJ) $(C_) $(FTSRC)base$(D)ftxf86.c

$(FTOBJ)ftpatent.$(OBJ) : $(FTSRC)base$(D)ftpatent.c
	$(FTCC) $(FTO_)ftpatent.$(OBJ) $(C_) $(FTSRC)base$(D)ftpatent.c

$(FTOBJ)bdflib.$(OBJ) : $(FTSRC)bdf$(D)bdflib.c
	$(FTCC) $(FTO_)bdflib.$(OBJ) $(C_) $(FTSRC)bdf$(D)bdflib.c

$(FTOBJ)bdfdrivr.$(OBJ) : $(FTSRC)bdf$(D)bdfdrivr.c
	$(FTCC) $(FTO_)bdfdrivr.$(OBJ) $(C_) $(FTSRC)bdf$(D)bdfdrivr.c

$(FTOBJ)ftcbasic.$(OBJ) : $(FTSRC)cache$(D)ftcbasic.c
	$(FTCC) $(FTO_)ftcbasic.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcbasic.c

$(FTOBJ)ft2ccache.$(OBJ) : $(FTSRC)cache$(D)ftccache.c
	$(FTCC) $(FTO_)ft2ccache.$(OBJ) $(C_) $(FTSRC)cache$(D)ftccache.c

$(FTOBJ)ftccmap.$(OBJ) : $(FTSRC)cache$(D)ftccmap.c
	$(FTCC) $(FTO_)ftccmap.$(OBJ) $(C_) $(FTSRC)cache$(D)ftccmap.c

$(FTOBJ)ftcglyph.$(OBJ) : $(FTSRC)cache$(D)ftcglyph.c
	$(FTCC) $(FTO_)ftcglyph.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcglyph.c

$(FTOBJ)ftcimage.$(OBJ) : $(FTSRC)cache$(D)ftcimage.c
	$(FTCC) $(FTO_)ftcimage.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcimage.c

$(FTOBJ)ftcmanag.$(OBJ) : $(FTSRC)cache$(D)ftcmanag.c
	$(FTCC) $(FTO_)ftcmanag.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcmanag.c

$(FTOBJ)ftcmru.$(OBJ) : $(FTSRC)cache$(D)ftcmru.c
	$(FTCC) $(FTO_)ftcmru.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcmru.c

$(FTOBJ)ftcsbits.$(OBJ) : $(FTSRC)cache$(D)ftcsbits.c
	$(FTCC) $(FTO_)ftcsbits.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcsbits.c

$(FTOBJ)cffobjs.$(OBJ) : $(FTSRC)cff$(D)cffobjs.c
	$(FTCC) $(FTO_)cffobjs.$(OBJ) $(C_) $(FTSRC)cff$(D)cffobjs.c

$(FTOBJ)cffload.$(OBJ) : $(FTSRC)cff$(D)cffload.c
	$(FTCC) $(FTO_)cffload.$(OBJ) $(C_) $(FTSRC)cff$(D)cffload.c

$(FTOBJ)cffgload.$(OBJ) : $(FTSRC)cff$(D)cffgload.c
	$(FTCC) $(FTO_)cffgload.$(OBJ) $(C_) $(FTSRC)cff$(D)cffgload.c

$(FTOBJ)cffparse.$(OBJ) : $(FTSRC)cff$(D)cffparse.c
	$(FTCC) $(FTO_)cffparse.$(OBJ) $(C_) $(FTSRC)cff$(D)cffparse.c

$(FTOBJ)cffcmap.$(OBJ) : $(FTSRC)cff$(D)cffcmap.c
	$(FTCC) $(FTO_)cffcmap.$(OBJ) $(C_) $(FTSRC)cff$(D)cffcmap.c

$(FTOBJ)cffdrivr.$(OBJ) : $(FTSRC)cff$(D)cffdrivr.c
	$(FTCC) $(FTO_)cffdrivr.$(OBJ) $(C_) $(FTSRC)cff$(D)cffdrivr.c

$(FTOBJ)cidparse.$(OBJ) : $(FTSRC)cid$(D)cidparse.c
	$(FTCC) $(FTO_)cidparse.$(OBJ) $(C_) $(FTSRC)cid$(D)cidparse.c

$(FTOBJ)cidload.$(OBJ) : $(FTSRC)cid$(D)cidload.c
	$(FTCC) $(FTO_)cidload.$(OBJ) $(C_) $(FTSRC)cid$(D)cidload.c

$(FTOBJ)cidriver.$(OBJ) : $(FTSRC)cid$(D)cidriver.c
	$(FTCC) $(FTO_)cidriver.$(OBJ) $(C_) $(FTSRC)cid$(D)cidriver.c

$(FTOBJ)cidgload.$(OBJ) : $(FTSRC)cid$(D)cidgload.c
	$(FTCC) $(FTO_)cidgload.$(OBJ) $(C_) $(FTSRC)cid$(D)cidgload.c

$(FTOBJ)cidobjs.$(OBJ) : $(FTSRC)cid$(D)cidobjs.c
	$(FTCC) $(FTO_)cidobjs.$(OBJ) $(C_) $(FTSRC)cid$(D)cidobjs.c

$(FTOBJ)ftgzip.$(OBJ) : $(FTSRC)gzip$(D)ftgzip.c
	$(FTCC) $(FTO_)ftgzip.$(OBJ) $(C_) $(FTSRC)gzip$(D)ftgzip.c

$(FTOBJ)ftlzw.$(OBJ) : $(FTSRC)lzw$(D)ftlzw.c
	$(FTCC) $(FTO_)ftlzw.$(OBJ) $(C_) $(FTSRC)lzw$(D)ftlzw.c

$(FTOBJ)pcfdrivr.$(OBJ) : $(FTSRC)pcf$(D)pcfdrivr.c
	$(FTCC) $(FTO_)pcfdrivr.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfdrivr.c

$(FTOBJ)pcfread.$(OBJ) : $(FTSRC)pcf$(D)pcfread.c
	$(FTCC) $(FTO_)pcfread.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfread.c

$(FTOBJ)pcfutil.$(OBJ) : $(FTSRC)pcf$(D)pcfutil.c
	$(FTCC) $(FTO_)pcfutil.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfutil.c

$(FTOBJ)pfrload.$(OBJ) : $(FTSRC)pfr$(D)pfrload.c
	$(FTCC) $(FTO_)pfrload.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrload.c

$(FTOBJ)pfrgload.$(OBJ) : $(FTSRC)pfr$(D)pfrgload.c
	$(FTCC) $(FTO_)pfrgload.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrgload.c

$(FTOBJ)pfrcmap.$(OBJ) : $(FTSRC)pfr$(D)pfrcmap.c
	$(FTCC) $(FTO_)pfrcmap.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrcmap.c

$(FTOBJ)pfrdrivr.$(OBJ) : $(FTSRC)pfr$(D)pfrdrivr.c
	$(FTCC) $(FTO_)pfrdrivr.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrdrivr.c

$(FTOBJ)pfrsbit.$(OBJ) : $(FTSRC)pfr$(D)pfrsbit.c
	$(FTCC) $(FTO_)pfrsbit.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrsbit.c

$(FTOBJ)pfrobjs.$(OBJ) : $(FTSRC)pfr$(D)pfrobjs.c
	$(FTCC) $(FTO_)pfrobjs.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrobjs.c

$(FTOBJ)psobjs.$(OBJ) : $(FTSRC)psaux$(D)psobjs.c
	$(FTCC) $(FTO_)psobjs.$(OBJ) $(C_) $(FTSRC)psaux$(D)psobjs.c

$(FTOBJ)t1decode.$(OBJ) : $(FTSRC)psaux$(D)t1decode.c
	$(FTCC) $(FTO_)t1decode.$(OBJ) $(C_) $(FTSRC)psaux$(D)t1decode.c

$(FTOBJ)t1cmap.$(OBJ) : $(FTSRC)psaux$(D)t1cmap.c
	$(FTCC) $(FTO_)t1cmap.$(OBJ) $(C_) $(FTSRC)psaux$(D)t1cmap.c

$(FTOBJ)afmparse.$(OBJ) : $(FTSRC)psaux$(D)afmparse.c
	$(FTCC) $(FTO_)afmparse.$(OBJ) $(C_) $(FTSRC)psaux$(D)afmparse.c

$(FTOBJ)psconv.$(OBJ) : $(FTSRC)psaux$(D)psconv.c
	$(FTCC) $(FTO_)psconv.$(OBJ) $(C_) $(FTSRC)psaux$(D)psconv.c

$(FTOBJ)psauxmod.$(OBJ) : $(FTSRC)psaux$(D)psauxmod.c
	$(FTCC) $(FTO_)psauxmod.$(OBJ) $(C_) $(FTSRC)psaux$(D)psauxmod.c

$(FTOBJ)pshrec.$(OBJ) : $(FTSRC)pshinter$(D)pshrec.c
	$(FTCC) $(FTO_)pshrec.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshrec.c

$(FTOBJ)pshglob.$(OBJ) : $(FTSRC)pshinter$(D)pshglob.c
	$(FTCC) $(FTO_)pshglob.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshglob.c

$(FTOBJ)pshmod.$(OBJ) : $(FTSRC)pshinter$(D)pshmod.c
	$(FTCC) $(FTO_)pshmod.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshmod.c

$(FTOBJ)pshalgo.$(OBJ) : $(FTSRC)pshinter$(D)pshalgo.c
	$(FTCC) $(FTO_)pshalgo.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshalgo.c

$(FTOBJ)psmodule.$(OBJ) : $(FTSRC)psnames$(D)psmodule.c
	$(FTCC) $(FTO_)psmodule.$(OBJ) $(C_) $(FTSRC)psnames$(D)psmodule.c

$(FTOBJ)pspic.$(OBJ) : $(FTSRC)psnames$(D)pspic.c
	$(FTCC) $(FTO_)pspic.$(OBJ) $(C_) $(FTSRC)psnames$(D)pspic.c

$(FTOBJ)ftraster.$(OBJ) : $(FTSRC)raster$(D)ftraster.c
	$(FTCC) $(FTO_)ftraster.$(OBJ) $(C_) $(FTSRC)raster$(D)ftraster.c

$(FTOBJ)ftrend1.$(OBJ) : $(FTSRC)raster$(D)ftrend1.c
	$(FTCC) $(FTO_)ftrend1.$(OBJ) $(C_) $(FTSRC)raster$(D)ftrend1.c

$(FTOBJ)rastpic.$(OBJ) : $(FTSRC)raster$(D)rastpic.c
	$(FTCC) $(FTO_)rastpic.$(OBJ) $(C_) $(FTSRC)raster$(D)rastpic.c

$(FTOBJ)ftgrays.$(OBJ) : $(FTSRC)smooth$(D)ftgrays.c
	$(FTCC) $(FTO_)ftgrays.$(OBJ) $(C_) $(FTSRC)smooth$(D)ftgrays.c

$(FTOBJ)ftsmooth.$(OBJ) : $(FTSRC)smooth$(D)ftsmooth.c
	$(FTCC) $(FTO_)ftsmooth.$(OBJ) $(C_) $(FTSRC)smooth$(D)ftsmooth.c

$(FTOBJ)ftspic.$(OBJ) : $(FTSRC)smooth$(D)ftspic.c
	$(FTCC) $(FTO_)ftspic.$(OBJ) $(C_) $(FTSRC)smooth$(D)ftspic.c

$(FTOBJ)sfobjs.$(OBJ) : $(FTSRC)sfnt$(D)sfobjs.c
	$(FTCC) $(FTO_)sfobjs.$(OBJ) $(C_) $(FTSRC)sfnt$(D)sfobjs.c

$(FTOBJ)sfdriver.$(OBJ) : $(FTSRC)sfnt$(D)sfdriver.c
	$(FTCC) $(FTO_)sfdriver.$(OBJ) $(C_) $(FTSRC)sfnt$(D)sfdriver.c

$(FTOBJ)ttcmap.$(OBJ) : $(FTSRC)sfnt$(D)ttcmap.c
	$(FTCC) $(FTO_)ttcmap.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttcmap.c

$(FTOBJ)ttmtx.$(OBJ) : $(FTSRC)sfnt$(D)ttmtx.c
	$(FTCC) $(FTO_)ttmtx.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttmtx.c

$(FTOBJ)ttpost.$(OBJ) : $(FTSRC)sfnt$(D)ttpost.c
	$(FTCC) $(FTO_)ttpost.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttpost.c

$(FTOBJ)ft2ttload.$(OBJ) : $(FTSRC)sfnt$(D)ttload.c
	$(FTCC) $(FTO_)ft2ttload.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttload.c

$(FTOBJ)ttsbit.$(OBJ) : $(FTSRC)sfnt$(D)ttsbit.c
	$(FTCC) $(FTO_)ttsbit.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttsbit.c

$(FTOBJ)ttkern.$(OBJ) : $(FTSRC)sfnt$(D)ttkern.c
	$(FTCC) $(FTO_)ttkern.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttkern.c

$(FTOBJ)ttbdf.$(OBJ) : $(FTSRC)sfnt$(D)ttbdf.c
	$(FTCC) $(FTO_)ttbdf.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttbdf.c

$(FTOBJ)sfntpic.$(OBJ) : $(FTSRC)sfnt$(D)sfntpic.c
	$(FTCC) $(FTO_)sfntpic.$(OBJ) $(C_) $(FTSRC)sfnt$(D)sfntpic.c

$(FTOBJ)ttdriver.$(OBJ) : $(FTSRC)truetype$(D)ttdriver.c
	$(FTCC) $(FTO_)ttdriver.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttdriver.c

$(FTOBJ)ft2ttobjs.$(OBJ) : $(FTSRC)truetype$(D)ttobjs.c
	$(FTCC) $(FTO_)ft2ttobjs.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttobjs.c

$(FTOBJ)ttpload.$(OBJ) : $(FTSRC)truetype$(D)ttpload.c
	$(FTCC) $(FTO_)ttpload.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttpload.c

$(FTOBJ)ttgload.$(OBJ) : $(FTSRC)truetype$(D)ttgload.c
	$(FTCC) $(FTO_)ttgload.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttgload.c

$(FTOBJ)ft2ttinterp.$(OBJ) : $(FTSRC)truetype$(D)ttinterp.c
	$(FTCC) $(FTO_)ft2ttinterp.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttinterp.c

$(FTOBJ)ttgxvar.$(OBJ) : $(FTSRC)truetype$(D)ttgxvar.c
	$(FTCC) $(FTO_)ttgxvar.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttgxvar.c

$(FTOBJ)ttpic.$(OBJ) : $(FTSRC)truetype$(D)ttpic.c
	$(FTCC) $(FTO_)ttpic.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttpic.c

$(FTOBJ)t1afm.$(OBJ) : $(FTSRC)type1$(D)t1afm.c
	$(FTCC) $(FTO_)t1afm.$(OBJ) $(C_) $(FTSRC)type1$(D)t1afm.c

$(FTOBJ)t1driver.$(OBJ) : $(FTSRC)type1$(D)t1driver.c
	$(FTCC) $(FTO_)t1driver.$(OBJ) $(C_) $(FTSRC)type1$(D)t1driver.c

$(FTOBJ)t1objs.$(OBJ) : $(FTSRC)type1$(D)t1objs.c
	$(FTCC) $(FTO_)t1objs.$(OBJ) $(C_) $(FTSRC)type1$(D)t1objs.c

$(FTOBJ)t1load.$(OBJ) : $(FTSRC)type1$(D)t1load.c
	$(FTCC) $(FTO_)t1load.$(OBJ) $(C_) $(FTSRC)type1$(D)t1load.c

$(FTOBJ)t1gload.$(OBJ) : $(FTSRC)type1$(D)t1gload.c
	$(FTCC) $(FTO_)t1gload.$(OBJ) $(C_) $(FTSRC)type1$(D)t1gload.c

$(FTOBJ)t1parse.$(OBJ) : $(FTSRC)type1$(D)t1parse.c
	$(FTCC) $(FTO_)t1parse.$(OBJ) $(C_) $(FTSRC)type1$(D)t1parse.c

$(FTOBJ)t42objs.$(OBJ) : $(FTSRC)type42$(D)t42objs.c
	$(FTCC) $(FTO_)t42objs.$(OBJ) $(C_) $(FTSRC)type42$(D)t42objs.c

$(FTOBJ)t42parse.$(OBJ) : $(FTSRC)type42$(D)t42parse.c
	$(FTCC) $(FTO_)t42parse.$(OBJ) $(C_) $(FTSRC)type42$(D)t42parse.c

$(FTOBJ)t42drivr.$(OBJ) : $(FTSRC)type42$(D)t42drivr.c
	$(FTCC) $(FTO_)t42drivr.$(OBJ) $(C_) $(FTSRC)type42$(D)t42drivr.c

$(FTOBJ)winfnt.$(OBJ) : $(FTSRC)winfonts$(D)winfnt.c
	$(FTCC) $(FTO_)winfnt.$(OBJ) $(C_) $(FTSRC)winfonts$(D)winfnt.c
