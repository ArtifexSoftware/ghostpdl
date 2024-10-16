# Copyright (C) 2001-2024 Artifex Software, Inc.
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
#
# makefile for freetype as part of the monolithic gs build.
#
# Users of this makefile must define the following:
#	FTSRCDIR    - the source directory
#	FTGENDIR    - the generated intermediate file directory
#	FTOBJDIR    - the object file directory
#	FT_CFLAGS   - The include options for the freetype library
#	SHARE_FT - 0 to compile in freetype, 1 to link a shared library
#	FT_LIBS  - if SHARE_FT=1, the link options for the shared library
#	FT_LIB_PATH - if SHARE_FT=1, the path(s) for the shared library

# (Rename directories.)
FTSRC=$(FTSRCDIR)$(D)src$(D)
FTGEN=$(FTGENDIR)$(D)
FTOBJ=$(FTOBJDIR)$(D)
FTO_=$(O_)$(FTOBJ)

# Define our local compiler alias
# we must define FT2_BUILD_LIBRARY to get internal declarations
# If GS is using the system zlib, freetype should also do so,
FTCC=$(CC) $(I_)$(FTGEN)$(_I) $(I_)$(FTSRCDIR)$(D)include$(_I) $(FT_CFLAGS) \
     $(D_)FT_CONFIG_OPTIONS_H=\"$(FTCONFH)\"$(_D) $(D_)FT2_BUILD_LIBRARY$(_D) \
     $(D_)DARWIN_NO_CARBON$(_D) $(D_)FT_CONFIG_OPTION_SYSTEM_ZLIB$(_D) $(CCFLAGS)

# Define the name of this makefile.
FT_MAK=$(GLSRC)freetype.mak $(TOP_MAKEFILES)

# file complements for each component
ft_autofit=\
	$(FTOBJ)afcjk.$(OBJ) \
	$(FTOBJ)afdummy.$(OBJ) \
	$(FTOBJ)afglobal.$(OBJ) \
	$(FTOBJ)afhints.$(OBJ) \
	$(FTOBJ)afindic.$(OBJ) \
	$(FTOBJ)aflatin.$(OBJ) \
	$(FTOBJ)afloader.$(OBJ) \
	$(FTOBJ)afmodule.$(OBJ) \
	$(FTOBJ)afblue.$(OBJ) \
	$(FTOBJ)afranges.$(OBJ) \
	$(FTOBJ)afshaper.$(OBJ)

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
	$(FTOBJ)ftpatent.$(OBJ) \
	$(FTOBJ)ftmd5.$(OBJ) \
	$(FTOBJ)fthash.$(OBJ) \
        $(FTOBJ)ftpsprop.$(OBJ) \
        $(FTOBJ)ftfntfmt.$(OBJ)


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
	$(FTOBJ)cffdrivr.$(OBJ) \
	$(FTOBJ)cffcmap.$(OBJ) \
	$(FTOBJ)cffgload.$(OBJ) \
	$(FTOBJ)cffload.$(OBJ) \
	$(FTOBJ)cffobjs.$(OBJ) \
	$(FTOBJ)cffparse.$(OBJ)

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
	$(FTOBJ)psauxmod.$(OBJ) \
        $(FTOBJ)psft.$(OBJ) \
        $(FTOBJ)cffdecode.$(OBJ) \
        $(FTOBJ)psfont.$(OBJ) \
        $(FTOBJ)psblues.$(OBJ) \
        $(FTOBJ)psintrp.$(OBJ) \
        $(FTOBJ)pserror.$(OBJ) \
        $(FTOBJ)psstack.$(OBJ) \
        $(FTOBJ)pshints.$(OBJ) \
        $(FTOBJ)psarrst.$(OBJ) \
        $(FTOBJ)psread.$(OBJ)

ft_pshinter=\
	$(FTOBJ)pshrec.$(OBJ) \
	$(FTOBJ)pshglob.$(OBJ) \
	$(FTOBJ)pshmod.$(OBJ) \
	$(FTOBJ)pshalgo.$(OBJ)

ft_psnames=\
	$(FTOBJ)psmodule.$(OBJ)

ft_raster=\
	$(FTOBJ)ftraster.$(OBJ) \
	$(FTOBJ)ftrend1.$(OBJ)

ft_smooth=\
	$(FTOBJ)ftgrays.$(OBJ) \
	$(FTOBJ)ftsmooth.$(OBJ)

ft_sfnt=\
	$(FTOBJ)ttmtx.$(OBJ) \
	$(FTOBJ)ttpost.$(OBJ) \
	$(FTOBJ)ft2ttload.$(OBJ) \
	$(FTOBJ)ttsbit.$(OBJ) \
	$(FTOBJ)ttkern.$(OBJ) \
	$(FTOBJ)ttbdf.$(OBJ) \
	$(FTOBJ)sfnt.$(OBJ) \
	$(FTOBJ)pngshim.$(OBJ) \
	$(FTOBJ)ttcpal.$(OBJ)


ft_truetype=\
	$(FTOBJ)ttdriver.$(OBJ) \
	$(FTOBJ)ft2ttobjs.$(OBJ) \
	$(FTOBJ)ttpload.$(OBJ) \
	$(FTOBJ)ttgload.$(OBJ) \
	$(FTOBJ)ft2ttinterp.$(OBJ) \
	$(FTOBJ)ttgxvar.$(OBJ)

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

ft_sdf=\
	$(FTOBJ)ftbsdf.$(OBJ) \
	$(FTOBJ)ftsdf.$(OBJ) \
	$(FTOBJ)ftsdfcommon.$(OBJ) \
	$(FTOBJ)ftsdfrend.$(OBJ) \

ft_svg=\
	$(FTOBJ)ftsvg.$(OBJ)

# instantiate the requested build option (shared or compiled in)
$(FTGEN)freetype.dev : $(FTGEN)freetype_$(SHARE_FT).dev $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(CP_) $(FTGEN)freetype_$(SHARE_FT).dev $(FTGEN)freetype.dev

# Define the shared version.
$(FTGEN)freetype_1.dev : $(TOP_MAKEFILES) $(FT_MAK) $(ECHOGS_XE) $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(SETMOD) $(FTGEN)freetype_1 -lib $(FT_LIBS)
	$(ADDMOD) $(FTGEN)freetype_1 -libpath $(FT_LIB_PATH)

# Define the non-shared version.
$(FTGEN)freetype_0.dev : $(FT_MAK) $(ECHOGS_XE) \
    $(ft_autofit) $(ft_base) $(ft_bdf) $(ft_cache) $(ft_cff) $(ft_cid) \
    $(ft_gzip) $(ft_lzw) $(ft_pcf) $(ft_pfr) $(ft_psaux) $(ft_pshinter) \
    $(ft_psnames) $(ft_raster) $(ft_smooth) $(ft_sfnt) $(ft_truetype) \
    $(ft_type1) $(ft_type42) $(ft_winfonts) $(ft_sdf) $(ft_svg) $(GENFTCONFH) $(MAKEDIRS)
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
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_sdf)
	$(ADDMOD) $(FTGEN)freetype_0 $(ft_svg)


# custom build rules for each source file

$(FTOBJ)afangles.$(OBJ) : $(FTSRC)autofit$(D)afangles.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afangles.$(OBJ) $(C_) $(FTSRC)autofit$(D)afangles.c

$(FTOBJ)afcjk.$(OBJ) : $(FTSRC)autofit$(D)afcjk.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afcjk.$(OBJ) $(C_) $(FTSRC)autofit$(D)afcjk.c

$(FTOBJ)afdummy.$(OBJ) : $(FTSRC)autofit$(D)afdummy.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afdummy.$(OBJ) $(C_) $(FTSRC)autofit$(D)afdummy.c

$(FTOBJ)afglobal.$(OBJ) : $(FTSRC)autofit$(D)afglobal.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afglobal.$(OBJ) $(C_) $(FTSRC)autofit$(D)afglobal.c

$(FTOBJ)afhints.$(OBJ) : $(FTSRC)autofit$(D)afhints.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afhints.$(OBJ) $(C_) $(FTSRC)autofit$(D)afhints.c

$(FTOBJ)afindic.$(OBJ) : $(FTSRC)autofit$(D)afindic.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afindic.$(OBJ) $(C_) $(FTSRC)autofit$(D)afindic.c

$(FTOBJ)aflatin.$(OBJ) : $(FTSRC)autofit$(D)aflatin.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)aflatin.$(OBJ) $(C_) $(FTSRC)autofit$(D)aflatin.c

$(FTOBJ)afloader.$(OBJ) : $(FTSRC)autofit$(D)afloader.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afloader.$(OBJ) $(C_) $(FTSRC)autofit$(D)afloader.c

$(FTOBJ)afmodule.$(OBJ) : $(FTSRC)autofit$(D)afmodule.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afmodule.$(OBJ) $(C_) $(FTSRC)autofit$(D)afmodule.c

$(FTOBJ)afwarp.$(OBJ) : $(FTSRC)autofit$(D)afwarp.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afwarp.$(OBJ) $(C_) $(FTSRC)autofit$(D)afwarp.c

$(FTOBJ)afblue.$(OBJ) : $(FTSRC)autofit$(D)afblue.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afblue.$(OBJ) $(C_) $(FTSRC)autofit$(D)afblue.c

$(FTOBJ)afranges.$(OBJ) : $(FTSRC)autofit$(D)afranges.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afranges.$(OBJ) $(C_) $(FTSRC)autofit$(D)afranges.c

$(FTOBJ)afshaper.$(OBJ) : $(FTSRC)autofit$(D)afshaper.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afshaper.$(OBJ) $(C_) $(FTSRC)autofit$(D)afshaper.c

$(FTOBJ)ftadvanc.$(OBJ) : $(FTSRC)base$(D)ftadvanc.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftadvanc.$(OBJ) $(C_) $(FTSRC)base$(D)ftadvanc.c

$(FTOBJ)ftcalc.$(OBJ) : $(FTSRC)base$(D)ftcalc.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcalc.$(OBJ) $(C_) $(FTSRC)base$(D)ftcalc.c

$(FTOBJ)ftdbgmem.$(OBJ) : $(FTSRC)base$(D)ftdbgmem.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftdbgmem.$(OBJ) $(C_) $(FTSRC)base$(D)ftdbgmem.c

$(FTOBJ)ftgloadr.$(OBJ) : $(FTSRC)base$(D)ftgloadr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftgloadr.$(OBJ) $(C_) $(FTSRC)base$(D)ftgloadr.c

$(FTOBJ)ftobjs.$(OBJ) : $(FTSRC)base$(D)ftobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftobjs.$(OBJ) $(C_) $(FTSRC)base$(D)ftobjs.c

$(FTOBJ)ftoutln.$(OBJ) : $(FTSRC)base$(D)ftoutln.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftoutln.$(OBJ) $(C_) $(FTSRC)base$(D)ftoutln.c

$(FTOBJ)ftrfork.$(OBJ) : $(FTSRC)base$(D)ftrfork.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftrfork.$(OBJ) $(C_) $(FTSRC)base$(D)ftrfork.c

$(FTOBJ)ftsnames.$(OBJ) : $(FTSRC)base$(D)ftsnames.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsnames.$(OBJ) $(C_) $(FTSRC)base$(D)ftsnames.c

$(FTOBJ)ftstream.$(OBJ) : $(FTSRC)base$(D)ftstream.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftstream.$(OBJ) $(C_) $(FTSRC)base$(D)ftstream.c

$(FTOBJ)fttrigon.$(OBJ) : $(FTSRC)base$(D)fttrigon.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)fttrigon.$(OBJ) $(C_) $(FTSRC)base$(D)fttrigon.c

$(FTOBJ)ftutil.$(OBJ) : $(FTSRC)base$(D)ftutil.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftutil.$(OBJ) $(C_) $(FTSRC)base$(D)ftutil.c

$(FTOBJ)ftbbox.$(OBJ) : $(FTSRC)base$(D)ftbbox.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftbbox.$(OBJ) $(C_) $(FTSRC)base$(D)ftbbox.c

$(FTOBJ)ftbdf.$(OBJ) : $(FTSRC)base$(D)ftbdf.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftbdf.$(OBJ) $(C_) $(FTSRC)base$(D)ftbdf.c

$(FTOBJ)ftbitmap.$(OBJ) : $(FTSRC)base$(D)ftbitmap.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftbitmap.$(OBJ) $(C_) $(FTSRC)base$(D)ftbitmap.c

$(FTOBJ)ftdebug.$(OBJ) : $(FTSRC)base$(D)ftdebug.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftdebug.$(OBJ) $(C_) $(FTSRC)base$(D)ftdebug.c

$(FTOBJ)ftgasp.$(OBJ) : $(FTSRC)base$(D)ftgasp.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftgasp.$(OBJ) $(C_) $(FTSRC)base$(D)ftgasp.c

$(FTOBJ)ftglyph.$(OBJ) : $(FTSRC)base$(D)ftglyph.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftglyph.$(OBJ) $(C_) $(FTSRC)base$(D)ftglyph.c

$(FTOBJ)ftgxval.$(OBJ) : $(FTSRC)base$(D)ftgxval.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftgxval.$(OBJ) $(C_) $(FTSRC)base$(D)ftgxval.c

$(FTOBJ)ftinit.$(OBJ) : $(FTSRC)base$(D)ftinit.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftinit.$(OBJ) $(C_) $(FTSRC)base$(D)ftinit.c

$(FTOBJ)ftlcdfil.$(OBJ) : $(FTSRC)base$(D)ftlcdfil.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftlcdfil.$(OBJ) $(C_) $(FTSRC)base$(D)ftlcdfil.c

$(FTOBJ)ftmm.$(OBJ) : $(FTSRC)base$(D)ftmm.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftmm.$(OBJ) $(C_) $(FTSRC)base$(D)ftmm.c

$(FTOBJ)ftotval.$(OBJ) : $(FTSRC)base$(D)ftotval.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftotval.$(OBJ) $(C_) $(FTSRC)base$(D)ftotval.c

$(FTOBJ)ftpfr.$(OBJ) : $(FTSRC)base$(D)ftpfr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftpfr.$(OBJ) $(C_) $(FTSRC)base$(D)ftpfr.c

$(FTOBJ)ftstroke.$(OBJ) : $(FTSRC)base$(D)ftstroke.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftstroke.$(OBJ) $(C_) $(FTSRC)base$(D)ftstroke.c

$(FTOBJ)ftsynth.$(OBJ) : $(FTSRC)base$(D)ftsynth.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsynth.$(OBJ) $(C_) $(FTSRC)base$(D)ftsynth.c

$(FTOBJ)ftsystem.$(OBJ) : $(FTSRC)base$(D)ftsystem.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsystem.$(OBJ) $(C_) $(FTSRC)base$(D)ftsystem.c

$(FTOBJ)fttype1.$(OBJ) : $(FTSRC)base$(D)fttype1.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)fttype1.$(OBJ) $(C_) $(FTSRC)base$(D)fttype1.c

$(FTOBJ)ftwinfnt.$(OBJ) : $(FTSRC)base$(D)ftwinfnt.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftwinfnt.$(OBJ) $(C_) $(FTSRC)base$(D)ftwinfnt.c

$(FTOBJ)ftpatent.$(OBJ) : $(FTSRC)base$(D)ftpatent.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftpatent.$(OBJ) $(C_) $(FTSRC)base$(D)ftpatent.c

$(FTOBJ)ftmd5.$(OBJ) : $(FTSRC)base$(D)md5.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftmd5.$(OBJ) $(C_) $(FTSRC)base$(D)md5.c

$(FTOBJ)fthash.$(OBJ) : $(FTSRC)base$(D)fthash.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)fthash.$(OBJ) $(C_) $(FTSRC)base$(D)fthash.c

$(FTOBJ)ftpsprop.$(OBJ) : $(FTSRC)base$(D)ftpsprop.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftpsprop.$(OBJ) $(C_) $(FTSRC)base$(D)ftpsprop.c

$(FTOBJ)ftfntfmt.$(OBJ) : $(FTSRC)base$(D)ftfntfmt.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftfntfmt.$(OBJ) $(C_) $(FTSRC)base$(D)ftfntfmt.c

$(FTOBJ)bdflib.$(OBJ) : $(FTSRC)bdf$(D)bdflib.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)bdflib.$(OBJ) $(C_) $(FTSRC)bdf$(D)bdflib.c

$(FTOBJ)bdfdrivr.$(OBJ) : $(FTSRC)bdf$(D)bdfdrivr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)bdfdrivr.$(OBJ) $(C_) $(FTSRC)bdf$(D)bdfdrivr.c

$(FTOBJ)ftcbasic.$(OBJ) : $(FTSRC)cache$(D)ftcbasic.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcbasic.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcbasic.c

$(FTOBJ)ft2ccache.$(OBJ) : $(FTSRC)cache$(D)ftccache.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ft2ccache.$(OBJ) $(C_) $(FTSRC)cache$(D)ftccache.c

$(FTOBJ)ftccmap.$(OBJ) : $(FTSRC)cache$(D)ftccmap.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftccmap.$(OBJ) $(C_) $(FTSRC)cache$(D)ftccmap.c

$(FTOBJ)ftcglyph.$(OBJ) : $(FTSRC)cache$(D)ftcglyph.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcglyph.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcglyph.c

$(FTOBJ)ftcimage.$(OBJ) : $(FTSRC)cache$(D)ftcimage.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcimage.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcimage.c

$(FTOBJ)ftcmanag.$(OBJ) : $(FTSRC)cache$(D)ftcmanag.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcmanag.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcmanag.c

$(FTOBJ)ftcmru.$(OBJ) : $(FTSRC)cache$(D)ftcmru.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcmru.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcmru.c

$(FTOBJ)ftcsbits.$(OBJ) : $(FTSRC)cache$(D)ftcsbits.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftcsbits.$(OBJ) $(C_) $(FTSRC)cache$(D)ftcsbits.c

$(FTOBJ)cff.$(OBJ) : $(FTSRC)cff$(D)cff.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cff.$(OBJ) $(C_) $(FTSRC)cff$(D)cff.c

$(FTOBJ)cffcmap.$(OBJ) : $(FTSRC)cff$(D)cffcmap.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffcmap.$(OBJ) $(C_) $(FTSRC)cff$(D)cffcmap.c

$(FTOBJ)cffdrivr.$(OBJ) : $(FTSRC)cff$(D)cffdrivr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffdrivr.$(OBJ) $(C_) $(FTSRC)cff$(D)cffdrivr.c

$(FTOBJ)cffgload.$(OBJ) : $(FTSRC)cff$(D)cffgload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffgload.$(OBJ) $(C_) $(FTSRC)cff$(D)cffgload.c

$(FTOBJ)cffload.$(OBJ) : $(FTSRC)cff$(D)cffload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffload.$(OBJ) $(C_) $(FTSRC)cff$(D)cffload.c

$(FTOBJ)cffobjs.$(OBJ) : $(FTSRC)cff$(D)cffobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffobjs.$(OBJ) $(C_) $(FTSRC)cff$(D)cffobjs.c

$(FTOBJ)cffparse.$(OBJ) : $(FTSRC)cff$(D)cffparse.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffparse.$(OBJ) $(C_) $(FTSRC)cff$(D)cffparse.c

$(FTOBJ)cidparse.$(OBJ) : $(FTSRC)cid$(D)cidparse.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cidparse.$(OBJ) $(C_) $(FTSRC)cid$(D)cidparse.c

$(FTOBJ)cidload.$(OBJ) : $(FTSRC)cid$(D)cidload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cidload.$(OBJ) $(C_) $(FTSRC)cid$(D)cidload.c

$(FTOBJ)cidriver.$(OBJ) : $(FTSRC)cid$(D)cidriver.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cidriver.$(OBJ) $(C_) $(FTSRC)cid$(D)cidriver.c

$(FTOBJ)cidgload.$(OBJ) : $(FTSRC)cid$(D)cidgload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cidgload.$(OBJ) $(C_) $(FTSRC)cid$(D)cidgload.c

$(FTOBJ)cidobjs.$(OBJ) : $(FTSRC)cid$(D)cidobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cidobjs.$(OBJ) $(C_) $(FTSRC)cid$(D)cidobjs.c

$(FTOBJ)ftgzip.$(OBJ) : $(FTSRC)gzip$(D)ftgzip.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftgzip.$(OBJ) $(C_) $(FTSRC)gzip$(D)ftgzip.c

$(FTOBJ)ftlzw.$(OBJ) : $(FTSRC)lzw$(D)ftlzw.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftlzw.$(OBJ) $(C_) $(FTSRC)lzw$(D)ftlzw.c

$(FTOBJ)pcfdrivr.$(OBJ) : $(FTSRC)pcf$(D)pcfdrivr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pcfdrivr.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfdrivr.c

$(FTOBJ)pcfread.$(OBJ) : $(FTSRC)pcf$(D)pcfread.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pcfread.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfread.c

$(FTOBJ)pcfutil.$(OBJ) : $(FTSRC)pcf$(D)pcfutil.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pcfutil.$(OBJ) $(C_) $(FTSRC)pcf$(D)pcfutil.c

$(FTOBJ)pfrload.$(OBJ) : $(FTSRC)pfr$(D)pfrload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrload.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrload.c

$(FTOBJ)pfrgload.$(OBJ) : $(FTSRC)pfr$(D)pfrgload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrgload.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrgload.c

$(FTOBJ)pfrcmap.$(OBJ) : $(FTSRC)pfr$(D)pfrcmap.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrcmap.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrcmap.c

$(FTOBJ)pfrdrivr.$(OBJ) : $(FTSRC)pfr$(D)pfrdrivr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrdrivr.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrdrivr.c

$(FTOBJ)pfrsbit.$(OBJ) : $(FTSRC)pfr$(D)pfrsbit.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrsbit.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrsbit.c

$(FTOBJ)pfrobjs.$(OBJ) : $(FTSRC)pfr$(D)pfrobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pfrobjs.$(OBJ) $(C_) $(FTSRC)pfr$(D)pfrobjs.c

$(FTOBJ)psobjs.$(OBJ) : $(FTSRC)psaux$(D)psobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psobjs.$(OBJ) $(C_) $(FTSRC)psaux$(D)psobjs.c

$(FTOBJ)t1decode.$(OBJ) : $(FTSRC)psaux$(D)t1decode.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1decode.$(OBJ) $(C_) $(FTSRC)psaux$(D)t1decode.c

$(FTOBJ)t1cmap.$(OBJ) : $(FTSRC)psaux$(D)t1cmap.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1cmap.$(OBJ) $(C_) $(FTSRC)psaux$(D)t1cmap.c

$(FTOBJ)afmparse.$(OBJ) : $(FTSRC)psaux$(D)afmparse.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)afmparse.$(OBJ) $(C_) $(FTSRC)psaux$(D)afmparse.c

$(FTOBJ)psconv.$(OBJ) : $(FTSRC)psaux$(D)psconv.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psconv.$(OBJ) $(C_) $(FTSRC)psaux$(D)psconv.c

$(FTOBJ)psauxmod.$(OBJ) : $(FTSRC)psaux$(D)psauxmod.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psauxmod.$(OBJ) $(C_) $(FTSRC)psaux$(D)psauxmod.c

$(FTOBJ)psft.$(OBJ) : $(FTSRC)psaux$(D)psft.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psft.$(OBJ) $(C_) $(FTSRC)psaux$(D)psft.c

$(FTOBJ)cffdecode.$(OBJ) : $(FTSRC)psaux$(D)cffdecode.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)cffdecode.$(OBJ) $(C_) $(FTSRC)psaux$(D)cffdecode.c

$(FTOBJ)psfont.$(OBJ) : $(FTSRC)psaux$(D)psfont.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psfont.$(OBJ) $(C_) $(FTSRC)psaux$(D)psfont.c

$(FTOBJ)psblues.$(OBJ) : $(FTSRC)psaux$(D)psblues.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psblues.$(OBJ) $(C_) $(FTSRC)psaux$(D)psblues.c

$(FTOBJ)psintrp.$(OBJ) : $(FTSRC)psaux$(D)psintrp.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psintrp.$(OBJ) $(C_) $(FTSRC)psaux$(D)psintrp.c

$(FTOBJ)pserror.$(OBJ) : $(FTSRC)psaux$(D)pserror.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pserror.$(OBJ) $(C_) $(FTSRC)psaux$(D)pserror.c

$(FTOBJ)psstack.$(OBJ) : $(FTSRC)psaux$(D)psstack.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psstack.$(OBJ) $(C_) $(FTSRC)psaux$(D)psstack.c

$(FTOBJ)pshints.$(OBJ) : $(FTSRC)psaux$(D)pshints.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pshints.$(OBJ) $(C_) $(FTSRC)psaux$(D)pshints.c

$(FTOBJ)psarrst.$(OBJ) : $(FTSRC)psaux$(D)psarrst.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psarrst.$(OBJ) $(C_) $(FTSRC)psaux$(D)psarrst.c

$(FTOBJ)psread.$(OBJ) : $(FTSRC)psaux$(D)psread.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psread.$(OBJ) $(C_) $(FTSRC)psaux$(D)psread.c

$(FTOBJ)pshrec.$(OBJ) : $(FTSRC)pshinter$(D)pshrec.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pshrec.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshrec.c

$(FTOBJ)pshglob.$(OBJ) : $(FTSRC)pshinter$(D)pshglob.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pshglob.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshglob.c

$(FTOBJ)pshmod.$(OBJ) : $(FTSRC)pshinter$(D)pshmod.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pshmod.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshmod.c

$(FTOBJ)pshalgo.$(OBJ) : $(FTSRC)pshinter$(D)pshalgo.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pshalgo.$(OBJ) $(C_) $(FTSRC)pshinter$(D)pshalgo.c

$(FTOBJ)psmodule.$(OBJ) : $(FTSRC)psnames$(D)psmodule.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)psmodule.$(OBJ) $(C_) $(FTSRC)psnames$(D)psmodule.c

$(FTOBJ)ftraster.$(OBJ) : $(FTSRC)raster$(D)ftraster.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftraster.$(OBJ) $(C_) $(FTSRC)raster$(D)ftraster.c

$(FTOBJ)ftrend1.$(OBJ) : $(FTSRC)raster$(D)ftrend1.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftrend1.$(OBJ) $(C_) $(FTSRC)raster$(D)ftrend1.c

$(FTOBJ)raster.$(OBJ) : $(FTSRC)raster$(D)raster.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)raster.$(OBJ) $(C_) $(FTSRC)raster$(D)raster.c

$(FTOBJ)ftgrays.$(OBJ) : $(FTSRC)smooth$(D)ftgrays.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftgrays.$(OBJ) $(C_) $(FTSRC)smooth$(D)ftgrays.c

$(FTOBJ)ftsmooth.$(OBJ) : $(FTSRC)smooth$(D)ftsmooth.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsmooth.$(OBJ) $(C_) $(FTSRC)smooth$(D)ftsmooth.c

$(FTOBJ)smooth.$(OBJ) : $(FTSRC)smooth$(D)smooth.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)smooth.$(OBJ) $(C_) $(FTSRC)smooth$(D)smooth.c

$(FTOBJ)ttmtx.$(OBJ) : $(FTSRC)sfnt$(D)ttmtx.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttmtx.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttmtx.c

$(FTOBJ)ttpost.$(OBJ) : $(FTSRC)sfnt$(D)ttpost.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttpost.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttpost.c

$(FTOBJ)ft2ttload.$(OBJ) : $(FTSRC)sfnt$(D)ttload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ft2ttload.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttload.c

$(FTOBJ)ttsbit.$(OBJ) : $(FTSRC)sfnt$(D)ttsbit.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttsbit.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttsbit.c

$(FTOBJ)ttkern.$(OBJ) : $(FTSRC)sfnt$(D)ttkern.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttkern.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttkern.c

$(FTOBJ)ttbdf.$(OBJ) : $(FTSRC)sfnt$(D)ttbdf.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttbdf.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttbdf.c

$(FTOBJ)sfnt.$(OBJ) : $(FTSRC)sfnt$(D)sfnt.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)sfnt.$(OBJ) $(C_) $(FTSRC)sfnt$(D)sfnt.c

$(FTOBJ)pngshim.$(OBJ) : $(FTSRC)sfnt$(D)pngshim.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)pngshim.$(OBJ) $(C_) $(FTSRC)sfnt$(D)pngshim.c

$(FTOBJ)ttcpal.$(OBJ) : $(FTSRC)sfnt$(D)ttcpal.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttcpal.$(OBJ) $(C_) $(FTSRC)sfnt$(D)ttcpal.c

$(FTOBJ)ttdriver.$(OBJ) : $(FTSRC)truetype$(D)ttdriver.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttdriver.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttdriver.c

$(FTOBJ)ft2ttobjs.$(OBJ) : $(FTSRC)truetype$(D)ttobjs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ft2ttobjs.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttobjs.c

$(FTOBJ)ttpload.$(OBJ) : $(FTSRC)truetype$(D)ttpload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttpload.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttpload.c

$(FTOBJ)ttgload.$(OBJ) : $(FTSRC)truetype$(D)ttgload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttgload.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttgload.c

$(FTOBJ)ft2ttinterp.$(OBJ) : $(FTSRC)truetype$(D)ttinterp.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ft2ttinterp.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttinterp.c

$(FTOBJ)ttgxvar.$(OBJ) : $(FTSRC)truetype$(D)ttgxvar.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ttgxvar.$(OBJ) $(C_) $(FTSRC)truetype$(D)ttgxvar.c

$(FTOBJ)truetype.$(OBJ) : $(FTSRC)truetype$(D)truetype.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)truetype.$(OBJ) $(C_) $(FTSRC)truetype$(D)truetype.c

$(FTOBJ)t1afm.$(OBJ) : $(FTSRC)type1$(D)t1afm.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1afm.$(OBJ) $(C_) $(FTSRC)type1$(D)t1afm.c

$(FTOBJ)t1driver.$(OBJ) : $(FTSRC)type1$(D)t1driver.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1driver.$(OBJ) $(C_) $(FTSRC)type1$(D)t1driver.c

$(FTOBJ)t1objs.$(OBJ) : $(FTSRC)type1$(D)t1objs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1objs.$(OBJ) $(C_) $(FTSRC)type1$(D)t1objs.c

$(FTOBJ)t1load.$(OBJ) : $(FTSRC)type1$(D)t1load.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1load.$(OBJ) $(C_) $(FTSRC)type1$(D)t1load.c

$(FTOBJ)t1gload.$(OBJ) : $(FTSRC)type1$(D)t1gload.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1gload.$(OBJ) $(C_) $(FTSRC)type1$(D)t1gload.c

$(FTOBJ)t1parse.$(OBJ) : $(FTSRC)type1$(D)t1parse.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t1parse.$(OBJ) $(C_) $(FTSRC)type1$(D)t1parse.c

$(FTOBJ)t42objs.$(OBJ) : $(FTSRC)type42$(D)t42objs.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t42objs.$(OBJ) $(C_) $(FTSRC)type42$(D)t42objs.c

$(FTOBJ)t42parse.$(OBJ) : $(FTSRC)type42$(D)t42parse.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t42parse.$(OBJ) $(C_) $(FTSRC)type42$(D)t42parse.c

$(FTOBJ)t42drivr.$(OBJ) : $(FTSRC)type42$(D)t42drivr.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)t42drivr.$(OBJ) $(C_) $(FTSRC)type42$(D)t42drivr.c

$(FTOBJ)winfnt.$(OBJ) : $(FTSRC)winfonts$(D)winfnt.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)winfnt.$(OBJ) $(C_) $(FTSRC)winfonts$(D)winfnt.c

$(FTOBJ)ftbsdf.$(OBJ) : $(FTSRC)sdf$(D)ftbsdf.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftbsdf.$(OBJ) $(C_) $(FTSRC)sdf$(D)ftbsdf.c

$(FTOBJ)ftsdf.$(OBJ) : $(FTSRC)sdf$(D)ftsdf.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsdf.$(OBJ) $(C_) $(FTSRC)sdf$(D)ftsdf.c

$(FTOBJ)ftsdfcommon.$(OBJ) : $(FTSRC)sdf$(D)ftsdfcommon.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsdfcommon.$(OBJ) $(C_) $(FTSRC)sdf$(D)ftsdfcommon.c

$(FTOBJ)ftsdfrend.$(OBJ) : $(FTSRC)sdf$(D)ftsdfrend.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsdfrend.$(OBJ) $(C_) $(FTSRC)sdf$(D)ftsdfrend.c

$(FTOBJ)ftsvg.$(OBJ) : $(FTSRC)svg$(D)ftsvg.c $(FT_MAK) $(GENFTCONFH) $(MAKEDIRS)
	$(FTCC) $(FTO_)ftsvg.$(OBJ) $(C_) $(FTSRC)svg$(D)ftsvg.c
