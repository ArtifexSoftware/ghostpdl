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
# makefile for "local source" leptonica code.

LEPTINCLUDES=\
	$(I_)$(LEPTONICADIR)/src$(_I)\
	$(I_)$(GLSRCDIR)$(_I)\
	$(I_)$(GLGENDIR)$(_I)

LEPTCFLAGS_LOCAL=\
	-DLEPTONICA_INTERCEPT_ALLOC=1\
	-DHAVE_LIBJPEG=0\
	-DHAVE_LIBTIFF=0\
	-DHAVE_LIBPNG=0\
	-DHAVE_LIBZ=0\
	-DHAVE_LIBGIF=0\
	-DHAVE_LIBUNGIF=0\
	-DHAVE_LIBWEBP=0\
	-DHAVE_LIBWEBP_ANIM=0\
	-DHAVE_LIBJP2K=0

LEPTCC = $(CC) $(CCFLAGS) $(LEPTINCLUDES) $(LEPTCFLAGS_LOCAL) $(LEPTSUPPRESS)
LEPTOBJ = $(GLOBJDIR)$(D)leptonica_
LEPTO_ = $(O_)$(LEPTOBJ)

LEPTDEPS=\
	$(arch_h)\
	$(GLSRCDIR)/leptonica.mak\
	$(MAKEDIRS)

$(LEPTOBJ)adaptmap.$(OBJ) : $(LEPTONICADIR)/src/adaptmap.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)adaptmap.$(OBJ) $(C_) $(LEPTONICADIR)/src/adaptmap.c

$(LEPTOBJ)affine.$(OBJ) : $(LEPTONICADIR)/src/affine.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)affine.$(OBJ) $(C_) $(LEPTONICADIR)/src/affine.c

$(LEPTOBJ)affinecompose.$(OBJ) : $(LEPTONICADIR)/src/affinecompose.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)affinecompose.$(OBJ) $(C_) $(LEPTONICADIR)/src/affinecompose.c

$(LEPTOBJ)arrayaccess.$(OBJ) : $(LEPTONICADIR)/src/arrayaccess.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)arrayaccess.$(OBJ) $(C_) $(LEPTONICADIR)/src/arrayaccess.c

$(LEPTOBJ)baseline.$(OBJ) : $(LEPTONICADIR)/src/baseline.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)baseline.$(OBJ) $(C_) $(LEPTONICADIR)/src/baseline.c

$(LEPTOBJ)bbuffer.$(OBJ) : $(LEPTONICADIR)/src/bbuffer.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bbuffer.$(OBJ) $(C_) $(LEPTONICADIR)/src/bbuffer.c

$(LEPTOBJ)bilateral.$(OBJ) : $(LEPTONICADIR)/src/bilateral.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bilateral.$(OBJ) $(C_) $(LEPTONICADIR)/src/bilateral.c

$(LEPTOBJ)bilinear.$(OBJ) : $(LEPTONICADIR)/src/bilinear.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bilinear.$(OBJ) $(C_) $(LEPTONICADIR)/src/bilinear.c

$(LEPTOBJ)binarize.$(OBJ) : $(LEPTONICADIR)/src/binarize.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)binarize.$(OBJ) $(C_) $(LEPTONICADIR)/src/binarize.c

$(LEPTOBJ)binexpand.$(OBJ) : $(LEPTONICADIR)/src/binexpand.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)binexpand.$(OBJ) $(C_) $(LEPTONICADIR)/src/binexpand.c

$(LEPTOBJ)binreduce.$(OBJ) : $(LEPTONICADIR)/src/binreduce.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)binreduce.$(OBJ) $(C_) $(LEPTONICADIR)/src/binreduce.c

$(LEPTOBJ)blend.$(OBJ) : $(LEPTONICADIR)/src/blend.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)blend.$(OBJ) $(C_) $(LEPTONICADIR)/src/blend.c

$(LEPTOBJ)bmf.$(OBJ) : $(LEPTONICADIR)/src/bmf.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bmf.$(OBJ) $(C_) $(LEPTONICADIR)/src/bmf.c

$(LEPTOBJ)bmpio.$(OBJ) : $(LEPTONICADIR)/src/bmpio.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bmpio.$(OBJ) $(C_) $(LEPTONICADIR)/src/bmpio.c

$(LEPTOBJ)bmpiostub.$(OBJ) : $(LEPTONICADIR)/src/bmpiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bmpiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/bmpiostub.c

$(LEPTOBJ)bootnumgen1.$(OBJ) : $(LEPTONICADIR)/src/bootnumgen1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bootnumgen1.$(OBJ) $(C_) $(LEPTONICADIR)/src/bootnumgen1.c

$(LEPTOBJ)bootnumgen2.$(OBJ) : $(LEPTONICADIR)/src/bootnumgen2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bootnumgen2.$(OBJ) $(C_) $(LEPTONICADIR)/src/bootnumgen2.c

$(LEPTOBJ)bootnumgen3.$(OBJ) : $(LEPTONICADIR)/src/bootnumgen3.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bootnumgen3.$(OBJ) $(C_) $(LEPTONICADIR)/src/bootnumgen3.c

$(LEPTOBJ)bootnumgen4.$(OBJ) : $(LEPTONICADIR)/src/bootnumgen4.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bootnumgen4.$(OBJ) $(C_) $(LEPTONICADIR)/src/bootnumgen4.c

$(LEPTOBJ)boxbasic.$(OBJ) : $(LEPTONICADIR)/src/boxbasic.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxbasic.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxbasic.c

$(LEPTOBJ)boxfunc1.$(OBJ) : $(LEPTONICADIR)/src/boxfunc1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxfunc1.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxfunc1.c

$(LEPTOBJ)boxfunc2.$(OBJ) : $(LEPTONICADIR)/src/boxfunc2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxfunc2.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxfunc2.c

$(LEPTOBJ)boxfunc3.$(OBJ) : $(LEPTONICADIR)/src/boxfunc3.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxfunc3.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxfunc3.c

$(LEPTOBJ)boxfunc4.$(OBJ) : $(LEPTONICADIR)/src/boxfunc4.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxfunc4.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxfunc4.c

$(LEPTOBJ)boxfunc5.$(OBJ) : $(LEPTONICADIR)/src/boxfunc5.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)boxfunc5.$(OBJ) $(C_) $(LEPTONICADIR)/src/boxfunc5.c

$(LEPTOBJ)bytearray.$(OBJ) : $(LEPTONICADIR)/src/bytearray.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)bytearray.$(OBJ) $(C_) $(LEPTONICADIR)/src/bytearray.c

$(LEPTOBJ)ccbord.$(OBJ) : $(LEPTONICADIR)/src/ccbord.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)ccbord.$(OBJ) $(C_) $(LEPTONICADIR)/src/ccbord.c

$(LEPTOBJ)classapp.$(OBJ) : $(LEPTONICADIR)/src/classapp.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)classapp.$(OBJ) $(C_) $(LEPTONICADIR)/src/classapp.c

$(LEPTOBJ)colorcontent.$(OBJ) : $(LEPTONICADIR)/src/colorcontent.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colorcontent.$(OBJ) $(C_) $(LEPTONICADIR)/src/colorcontent.c

$(LEPTOBJ)coloring.$(OBJ) : $(LEPTONICADIR)/src/coloring.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)coloring.$(OBJ) $(C_) $(LEPTONICADIR)/src/coloring.c

$(LEPTOBJ)colormap.$(OBJ) : $(LEPTONICADIR)/src/colormap.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colormap.$(OBJ) $(C_) $(LEPTONICADIR)/src/colormap.c

$(LEPTOBJ)colormorph.$(OBJ) : $(LEPTONICADIR)/src/colormorph.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colormorph.$(OBJ) $(C_) $(LEPTONICADIR)/src/colormorph.c

$(LEPTOBJ)colorquant1.$(OBJ) : $(LEPTONICADIR)/src/colorquant1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colorquant1.$(OBJ) $(C_) $(LEPTONICADIR)/src/colorquant1.c

$(LEPTOBJ)colorquant2.$(OBJ) : $(LEPTONICADIR)/src/colorquant2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colorquant2.$(OBJ) $(C_) $(LEPTONICADIR)/src/colorquant2.c

$(LEPTOBJ)colorseg.$(OBJ) : $(LEPTONICADIR)/src/colorseg.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colorseg.$(OBJ) $(C_) $(LEPTONICADIR)/src/colorseg.c

$(LEPTOBJ)colorspace.$(OBJ) : $(LEPTONICADIR)/src/colorspace.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)colorspace.$(OBJ) $(C_) $(LEPTONICADIR)/src/colorspace.c

$(LEPTOBJ)compare.$(OBJ) : $(LEPTONICADIR)/src/compare.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)compare.$(OBJ) $(C_) $(LEPTONICADIR)/src/compare.c

$(LEPTOBJ)conncomp.$(OBJ) : $(LEPTONICADIR)/src/conncomp.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)conncomp.$(OBJ) $(C_) $(LEPTONICADIR)/src/conncomp.c

$(LEPTOBJ)convertfiles.$(OBJ) : $(LEPTONICADIR)/src/convertfiles.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)convertfiles.$(OBJ) $(C_) $(LEPTONICADIR)/src/convertfiles.c

$(LEPTOBJ)convolve.$(OBJ) : $(LEPTONICADIR)/src/convolve.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)convolve.$(OBJ) $(C_) $(LEPTONICADIR)/src/convolve.c

$(LEPTOBJ)correlscore.$(OBJ) : $(LEPTONICADIR)/src/correlscore.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)correlscore.$(OBJ) $(C_) $(LEPTONICADIR)/src/correlscore.c

$(LEPTOBJ)dewarp1.$(OBJ) : $(LEPTONICADIR)/src/dewarp1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dewarp1.$(OBJ) $(C_) $(LEPTONICADIR)/src/dewarp1.c

$(LEPTOBJ)dewarp2.$(OBJ) : $(LEPTONICADIR)/src/dewarp2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dewarp2.$(OBJ) $(C_) $(LEPTONICADIR)/src/dewarp2.c

$(LEPTOBJ)dewarp3.$(OBJ) : $(LEPTONICADIR)/src/dewarp3.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dewarp3.$(OBJ) $(C_) $(LEPTONICADIR)/src/dewarp3.c

$(LEPTOBJ)dewarp4.$(OBJ) : $(LEPTONICADIR)/src/dewarp4.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dewarp4.$(OBJ) $(C_) $(LEPTONICADIR)/src/dewarp4.c

$(LEPTOBJ)dnabasic.$(OBJ) : $(LEPTONICADIR)/src/dnabasic.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dnabasic.$(OBJ) $(C_) $(LEPTONICADIR)/src/dnabasic.c

$(LEPTOBJ)dnafunc1.$(OBJ) : $(LEPTONICADIR)/src/dnafunc1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dnafunc1.$(OBJ) $(C_) $(LEPTONICADIR)/src/dnafunc1.c

$(LEPTOBJ)dnahash.$(OBJ) : $(LEPTONICADIR)/src/dnahash.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dnahash.$(OBJ) $(C_) $(LEPTONICADIR)/src/dnahash.c

$(LEPTOBJ)dwacomb.2.$(OBJ) : $(LEPTONICADIR)/src/dwacomb.2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dwacomb.2.$(OBJ) $(C_) $(LEPTONICADIR)/src/dwacomb.2.c

$(LEPTOBJ)dwacomblow.2.$(OBJ) : $(LEPTONICADIR)/src/dwacomblow.2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)dwacomblow.2.$(OBJ) $(C_) $(LEPTONICADIR)/src/dwacomblow.2.c

$(LEPTOBJ)edge.$(OBJ) : $(LEPTONICADIR)/src/edge.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)edge.$(OBJ) $(C_) $(LEPTONICADIR)/src/edge.c

$(LEPTOBJ)encoding.$(OBJ) : $(LEPTONICADIR)/src/encoding.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)encoding.$(OBJ) $(C_) $(LEPTONICADIR)/src/encoding.c

$(LEPTOBJ)enhance.$(OBJ) : $(LEPTONICADIR)/src/enhance.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)enhance.$(OBJ) $(C_) $(LEPTONICADIR)/src/enhance.c

$(LEPTOBJ)fhmtauto.$(OBJ) : $(LEPTONICADIR)/src/fhmtauto.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fhmtauto.$(OBJ) $(C_) $(LEPTONICADIR)/src/fhmtauto.c

$(LEPTOBJ)fhmtgenlow.1.$(OBJ) : $(LEPTONICADIR)/src/fhmtgenlow.1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fhmtgenlow.1.$(OBJ) $(C_) $(LEPTONICADIR)/src/fhmtgenlow.1.c

$(LEPTOBJ)fmorphauto.$(OBJ) : $(LEPTONICADIR)/src/fmorphauto.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fmorphauto.$(OBJ) $(C_) $(LEPTONICADIR)/src/fmorphauto.c

$(LEPTOBJ)fmorphgen.1.$(OBJ) : $(LEPTONICADIR)/src/fmorphgen.1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fmorphgen.1.$(OBJ) $(C_) $(LEPTONICADIR)/src/fmorphgen.1.c

$(LEPTOBJ)fmorphgenlow.1.$(OBJ) : $(LEPTONICADIR)/src/fmorphgenlow.1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fmorphgenlow.1.$(OBJ) $(C_) $(LEPTONICADIR)/src/fmorphgenlow.1.c

$(LEPTOBJ)fpix1.$(OBJ) : $(LEPTONICADIR)/src/fpix1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fpix1.$(OBJ) $(C_) $(LEPTONICADIR)/src/fpix1.c

$(LEPTOBJ)fpix2.$(OBJ) : $(LEPTONICADIR)/src/fpix2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)fpix2.$(OBJ) $(C_) $(LEPTONICADIR)/src/fpix2.c

$(LEPTOBJ)gifiostub.$(OBJ) : $(LEPTONICADIR)/src/gifiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)gifiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/gifiostub.c

$(LEPTOBJ)gplot.$(OBJ) : $(LEPTONICADIR)/src/gplot.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)gplot.$(OBJ) $(C_) $(LEPTONICADIR)/src/gplot.c

$(LEPTOBJ)graphics.$(OBJ) : $(LEPTONICADIR)/src/graphics.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)graphics.$(OBJ) $(C_) $(LEPTONICADIR)/src/graphics.c

$(LEPTOBJ)graymorph.$(OBJ) : $(LEPTONICADIR)/src/graymorph.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)graymorph.$(OBJ) $(C_) $(LEPTONICADIR)/src/graymorph.c

$(LEPTOBJ)grayquant.$(OBJ) : $(LEPTONICADIR)/src/grayquant.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)grayquant.$(OBJ) $(C_) $(LEPTONICADIR)/src/grayquant.c

$(LEPTOBJ)hashmap.$(OBJ) : $(LEPTONICADIR)/src/hashmap.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)hashmap.$(OBJ) $(C_) $(LEPTONICADIR)/src/hashmap.c

$(LEPTOBJ)heap.$(OBJ) : $(LEPTONICADIR)/src/heap.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)heap.$(OBJ) $(C_) $(LEPTONICADIR)/src/heap.c

$(LEPTOBJ)jbclass.$(OBJ) : $(LEPTONICADIR)/src/jbclass.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)jbclass.$(OBJ) $(C_) $(LEPTONICADIR)/src/jbclass.c

$(LEPTOBJ)jp2kheader.$(OBJ) : $(LEPTONICADIR)/src/jp2kheader.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)jp2kheader.$(OBJ) $(C_) $(LEPTONICADIR)/src/jp2kheader.c

$(LEPTOBJ)jp2kheaderstub.$(OBJ) : $(LEPTONICADIR)/src/jp2kheaderstub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)jp2kheaderstub.$(OBJ) $(C_) $(LEPTONICADIR)/src/jp2kheaderstub.c

$(LEPTOBJ)jp2kiostub.$(OBJ) : $(LEPTONICADIR)/src/jp2kiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)jp2kiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/jp2kiostub.c

$(LEPTOBJ)jpegiostub.$(OBJ) : $(LEPTONICADIR)/src/jpegiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)jpegiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/jpegiostub.c

$(LEPTOBJ)kernel.$(OBJ) : $(LEPTONICADIR)/src/kernel.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)kernel.$(OBJ) $(C_) $(LEPTONICADIR)/src/kernel.c

$(LEPTOBJ)libversions.$(OBJ) : $(LEPTONICADIR)/src/libversions.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)libversions.$(OBJ) $(C_) $(LEPTONICADIR)/src/libversions.c

$(LEPTOBJ)list.$(OBJ) : $(LEPTONICADIR)/src/list.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)list.$(OBJ) $(C_) $(LEPTONICADIR)/src/list.c

$(LEPTOBJ)map.$(OBJ) : $(LEPTONICADIR)/src/map.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)map.$(OBJ) $(C_) $(LEPTONICADIR)/src/map.c

$(LEPTOBJ)morph.$(OBJ) : $(LEPTONICADIR)/src/morph.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)morph.$(OBJ) $(C_) $(LEPTONICADIR)/src/morph.c

$(LEPTOBJ)morphapp.$(OBJ) : $(LEPTONICADIR)/src/morphapp.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)morphapp.$(OBJ) $(C_) $(LEPTONICADIR)/src/morphapp.c

$(LEPTOBJ)morphdwa.$(OBJ) : $(LEPTONICADIR)/src/morphdwa.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)morphdwa.$(OBJ) $(C_) $(LEPTONICADIR)/src/morphdwa.c

$(LEPTOBJ)morphseq.$(OBJ) : $(LEPTONICADIR)/src/morphseq.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)morphseq.$(OBJ) $(C_) $(LEPTONICADIR)/src/morphseq.c

$(LEPTOBJ)numabasic.$(OBJ) : $(LEPTONICADIR)/src/numabasic.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)numabasic.$(OBJ) $(C_) $(LEPTONICADIR)/src/numabasic.c

$(LEPTOBJ)numafunc1.$(OBJ) : $(LEPTONICADIR)/src/numafunc1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)numafunc1.$(OBJ) $(C_) $(LEPTONICADIR)/src/numafunc1.c

$(LEPTOBJ)numafunc2.$(OBJ) : $(LEPTONICADIR)/src/numafunc2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)numafunc2.$(OBJ) $(C_) $(LEPTONICADIR)/src/numafunc2.c

$(LEPTOBJ)pageseg.$(OBJ) : $(LEPTONICADIR)/src/pageseg.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pageseg.$(OBJ) $(C_) $(LEPTONICADIR)/src/pageseg.c

$(LEPTOBJ)paintcmap.$(OBJ) : $(LEPTONICADIR)/src/paintcmap.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)paintcmap.$(OBJ) $(C_) $(LEPTONICADIR)/src/paintcmap.c

$(LEPTOBJ)partify.$(OBJ) : $(LEPTONICADIR)/src/partify.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)partify.$(OBJ) $(C_) $(LEPTONICADIR)/src/partify.c

$(LEPTOBJ)partition.$(OBJ) : $(LEPTONICADIR)/src/partition.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)partition.$(OBJ) $(C_) $(LEPTONICADIR)/src/partition.c

$(LEPTOBJ)pdfio1.$(OBJ) : $(LEPTONICADIR)/src/pdfio1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pdfio1.$(OBJ) $(C_) $(LEPTONICADIR)/src/pdfio1.c

$(LEPTOBJ)pdfio1stub.$(OBJ) : $(LEPTONICADIR)/src/pdfio1stub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pdfio1stub.$(OBJ) $(C_) $(LEPTONICADIR)/src/pdfio1stub.c

$(LEPTOBJ)pdfio2.$(OBJ) : $(LEPTONICADIR)/src/pdfio2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pdfio2.$(OBJ) $(C_) $(LEPTONICADIR)/src/pdfio2.c

$(LEPTOBJ)pdfio2stub.$(OBJ) : $(LEPTONICADIR)/src/pdfio2stub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pdfio2stub.$(OBJ) $(C_) $(LEPTONICADIR)/src/pdfio2stub.c

$(LEPTOBJ)pix1.$(OBJ) : $(LEPTONICADIR)/src/pix1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pix1.$(OBJ) $(C_) $(LEPTONICADIR)/src/pix1.c

$(LEPTOBJ)pix2.$(OBJ) : $(LEPTONICADIR)/src/pix2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pix2.$(OBJ) $(C_) $(LEPTONICADIR)/src/pix2.c

$(LEPTOBJ)pix3.$(OBJ) : $(LEPTONICADIR)/src/pix3.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pix3.$(OBJ) $(C_) $(LEPTONICADIR)/src/pix3.c

$(LEPTOBJ)pix4.$(OBJ) : $(LEPTONICADIR)/src/pix4.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pix4.$(OBJ) $(C_) $(LEPTONICADIR)/src/pix4.c

$(LEPTOBJ)pix5.$(OBJ) : $(LEPTONICADIR)/src/pix5.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pix5.$(OBJ) $(C_) $(LEPTONICADIR)/src/pix5.c

$(LEPTOBJ)pixabasic.$(OBJ) : $(LEPTONICADIR)/src/pixabasic.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixabasic.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixabasic.c

$(LEPTOBJ)pixacc.$(OBJ) : $(LEPTONICADIR)/src/pixacc.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixacc.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixacc.c

$(LEPTOBJ)pixafunc1.$(OBJ) : $(LEPTONICADIR)/src/pixafunc1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixafunc1.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixafunc1.c

$(LEPTOBJ)pixafunc2.$(OBJ) : $(LEPTONICADIR)/src/pixafunc2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixafunc2.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixafunc2.c

$(LEPTOBJ)pixalloc.$(OBJ) : $(LEPTONICADIR)/src/pixalloc.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixalloc.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixalloc.c

$(LEPTOBJ)pixarith.$(OBJ) : $(LEPTONICADIR)/src/pixarith.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixarith.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixarith.c

$(LEPTOBJ)pixcomp.$(OBJ) : $(LEPTONICADIR)/src/pixcomp.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixcomp.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixcomp.c

$(LEPTOBJ)pixconv.$(OBJ) : $(LEPTONICADIR)/src/pixconv.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixconv.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixconv.c

$(LEPTOBJ)pixlabel.$(OBJ) : $(LEPTONICADIR)/src/pixlabel.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixlabel.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixlabel.c

$(LEPTOBJ)pixtiling.$(OBJ) : $(LEPTONICADIR)/src/pixtiling.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pixtiling.$(OBJ) $(C_) $(LEPTONICADIR)/src/pixtiling.c

$(LEPTOBJ)pngiostub.$(OBJ) : $(LEPTONICADIR)/src/pngiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pngiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/pngiostub.c

$(LEPTOBJ)pnmio.$(OBJ) : $(LEPTONICADIR)/src/pnmio.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pnmio.$(OBJ) $(C_) $(LEPTONICADIR)/src/pnmio.c

$(LEPTOBJ)pnmiostub.$(OBJ) : $(LEPTONICADIR)/src/pnmiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)pnmiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/pnmiostub.c

$(LEPTOBJ)projective.$(OBJ) : $(LEPTONICADIR)/src/projective.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)projective.$(OBJ) $(C_) $(LEPTONICADIR)/src/projective.c

$(LEPTOBJ)psio1.$(OBJ) : $(LEPTONICADIR)/src/psio1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)psio1.$(OBJ) $(C_) $(LEPTONICADIR)/src/psio1.c

$(LEPTOBJ)psio1stub.$(OBJ) : $(LEPTONICADIR)/src/psio1stub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)psio1stub.$(OBJ) $(C_) $(LEPTONICADIR)/src/psio1stub.c

$(LEPTOBJ)psio2.$(OBJ) : $(LEPTONICADIR)/src/psio2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)psio2.$(OBJ) $(C_) $(LEPTONICADIR)/src/psio2.c

$(LEPTOBJ)psio2stub.$(OBJ) : $(LEPTONICADIR)/src/psio2stub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)psio2stub.$(OBJ) $(C_) $(LEPTONICADIR)/src/psio2stub.c

$(LEPTOBJ)ptabasic.$(OBJ) : $(LEPTONICADIR)/src/ptabasic.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)ptabasic.$(OBJ) $(C_) $(LEPTONICADIR)/src/ptabasic.c

$(LEPTOBJ)ptafunc1.$(OBJ) : $(LEPTONICADIR)/src/ptafunc1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)ptafunc1.$(OBJ) $(C_) $(LEPTONICADIR)/src/ptafunc1.c

$(LEPTOBJ)ptafunc2.$(OBJ) : $(LEPTONICADIR)/src/ptafunc2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)ptafunc2.$(OBJ) $(C_) $(LEPTONICADIR)/src/ptafunc2.c

$(LEPTOBJ)ptra.$(OBJ) : $(LEPTONICADIR)/src/ptra.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)ptra.$(OBJ) $(C_) $(LEPTONICADIR)/src/ptra.c

$(LEPTOBJ)quadtree.$(OBJ) : $(LEPTONICADIR)/src/quadtree.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)quadtree.$(OBJ) $(C_) $(LEPTONICADIR)/src/quadtree.c

$(LEPTOBJ)queue.$(OBJ) : $(LEPTONICADIR)/src/queue.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)queue.$(OBJ) $(C_) $(LEPTONICADIR)/src/queue.c

$(LEPTOBJ)rank.$(OBJ) : $(LEPTONICADIR)/src/rank.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rank.$(OBJ) $(C_) $(LEPTONICADIR)/src/rank.c

$(LEPTOBJ)rbtree.$(OBJ) : $(LEPTONICADIR)/src/rbtree.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rbtree.$(OBJ) $(C_) $(LEPTONICADIR)/src/rbtree.c

$(LEPTOBJ)readfile.$(OBJ) : $(LEPTONICADIR)/src/readfile.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)readfile.$(OBJ) $(C_) $(LEPTONICADIR)/src/readfile.c

$(LEPTOBJ)regutils.$(OBJ) : $(LEPTONICADIR)/src/regutils.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)regutils.$(OBJ) $(C_) $(LEPTONICADIR)/src/regutils.c

$(LEPTOBJ)rop.$(OBJ) : $(LEPTONICADIR)/src/rop.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rop.$(OBJ) $(C_) $(LEPTONICADIR)/src/rop.c

$(LEPTOBJ)roplow.$(OBJ) : $(LEPTONICADIR)/src/roplow.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)roplow.$(OBJ) $(C_) $(LEPTONICADIR)/src/roplow.c

$(LEPTOBJ)rotate.$(OBJ) : $(LEPTONICADIR)/src/rotate.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rotate.$(OBJ) $(C_) $(LEPTONICADIR)/src/rotate.c

$(LEPTOBJ)rotateam.$(OBJ) : $(LEPTONICADIR)/src/rotateam.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rotateam.$(OBJ) $(C_) $(LEPTONICADIR)/src/rotateam.c

$(LEPTOBJ)rotateorth.$(OBJ) : $(LEPTONICADIR)/src/rotateorth.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rotateorth.$(OBJ) $(C_) $(LEPTONICADIR)/src/rotateorth.c

$(LEPTOBJ)rotateshear.$(OBJ) : $(LEPTONICADIR)/src/rotateshear.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)rotateshear.$(OBJ) $(C_) $(LEPTONICADIR)/src/rotateshear.c

$(LEPTOBJ)runlength.$(OBJ) : $(LEPTONICADIR)/src/runlength.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)runlength.$(OBJ) $(C_) $(LEPTONICADIR)/src/runlength.c

$(LEPTOBJ)sarray1.$(OBJ) : $(LEPTONICADIR)/src/sarray1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)sarray1.$(OBJ) $(C_) $(LEPTONICADIR)/src/sarray1.c

$(LEPTOBJ)sarray2.$(OBJ) : $(LEPTONICADIR)/src/sarray2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)sarray2.$(OBJ) $(C_) $(LEPTONICADIR)/src/sarray2.c

$(LEPTOBJ)scale1.$(OBJ) : $(LEPTONICADIR)/src/scale1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)scale1.$(OBJ) $(C_) $(LEPTONICADIR)/src/scale1.c

$(LEPTOBJ)scale2.$(OBJ) : $(LEPTONICADIR)/src/scale2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)scale2.$(OBJ) $(C_) $(LEPTONICADIR)/src/scale2.c

$(LEPTOBJ)seedfill.$(OBJ) : $(LEPTONICADIR)/src/seedfill.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)seedfill.$(OBJ) $(C_) $(LEPTONICADIR)/src/seedfill.c

$(LEPTOBJ)sel1.$(OBJ) : $(LEPTONICADIR)/src/sel1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)sel1.$(OBJ) $(C_) $(LEPTONICADIR)/src/sel1.c

$(LEPTOBJ)sel2.$(OBJ) : $(LEPTONICADIR)/src/sel2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)sel2.$(OBJ) $(C_) $(LEPTONICADIR)/src/sel2.c

$(LEPTOBJ)selgen.$(OBJ) : $(LEPTONICADIR)/src/selgen.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)selgen.$(OBJ) $(C_) $(LEPTONICADIR)/src/selgen.c

$(LEPTOBJ)shear.$(OBJ) : $(LEPTONICADIR)/src/shear.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)shear.$(OBJ) $(C_) $(LEPTONICADIR)/src/shear.c

$(LEPTOBJ)skew.$(OBJ) : $(LEPTONICADIR)/src/skew.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)skew.$(OBJ) $(C_) $(LEPTONICADIR)/src/skew.c

$(LEPTOBJ)spixio.$(OBJ) : $(LEPTONICADIR)/src/spixio.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)spixio.$(OBJ) $(C_) $(LEPTONICADIR)/src/spixio.c

$(LEPTOBJ)stack.$(OBJ) : $(LEPTONICADIR)/src/stack.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)stack.$(OBJ) $(C_) $(LEPTONICADIR)/src/stack.c

$(LEPTOBJ)stringcode.$(OBJ) : $(LEPTONICADIR)/src/stringcode.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)stringcode.$(OBJ) $(C_) $(LEPTONICADIR)/src/stringcode.c

$(LEPTOBJ)strokes.$(OBJ) : $(LEPTONICADIR)/src/strokes.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)strokes.$(OBJ) $(C_) $(LEPTONICADIR)/src/strokes.c

$(LEPTOBJ)sudoku.$(OBJ) : $(LEPTONICADIR)/src/sudoku.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)sudoku.$(OBJ) $(C_) $(LEPTONICADIR)/src/sudoku.c

$(LEPTOBJ)textops.$(OBJ) : $(LEPTONICADIR)/src/textops.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)textops.$(OBJ) $(C_) $(LEPTONICADIR)/src/textops.c

$(LEPTOBJ)tiffiostub.$(OBJ) : $(LEPTONICADIR)/src/tiffiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)tiffiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/tiffiostub.c

$(LEPTOBJ)utils1.$(OBJ) : $(LEPTONICADIR)/src/utils1.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)utils1.$(OBJ) $(C_) $(LEPTONICADIR)/src/utils1.c

$(LEPTOBJ)utils2.$(OBJ) : $(LEPTONICADIR)/src/utils2.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)utils2.$(OBJ) $(C_) $(LEPTONICADIR)/src/utils2.c

$(LEPTOBJ)warper.$(OBJ) : $(LEPTONICADIR)/src/warper.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)warper.$(OBJ) $(C_) $(LEPTONICADIR)/src/warper.c

$(LEPTOBJ)webpiostub.$(OBJ) : $(LEPTONICADIR)/src/webpiostub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)webpiostub.$(OBJ) $(C_) $(LEPTONICADIR)/src/webpiostub.c

$(LEPTOBJ)writefile.$(OBJ) : $(LEPTONICADIR)/src/writefile.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)writefile.$(OBJ) $(C_) $(LEPTONICADIR)/src/writefile.c

$(LEPTOBJ)zlibmem.$(OBJ) : $(LEPTONICADIR)/src/zlibmem.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)zlibmem.$(OBJ) $(C_) $(LEPTONICADIR)/src/zlibmem.c

$(LEPTOBJ)zlibmemstub.$(OBJ) : $(LEPTONICADIR)/src/zlibmemstub.c $(LEPTDEPS)
	$(LEPTCC) $(LEPTO_)zlibmemstub.$(OBJ) $(C_) $(LEPTONICADIR)/src/zlibmemstub.c

LEPTONICA_OBJS=\
	$(LEPTOBJ)adaptmap.$(OBJ)\
	$(LEPTOBJ)affine.$(OBJ)\
	$(LEPTOBJ)affinecompose.$(OBJ)\
	$(LEPTOBJ)arrayaccess.$(OBJ)\
	$(LEPTOBJ)baseline.$(OBJ)\
	$(LEPTOBJ)bbuffer.$(OBJ)\
	$(LEPTOBJ)bilateral.$(OBJ)\
	$(LEPTOBJ)bilinear.$(OBJ)\
	$(LEPTOBJ)binarize.$(OBJ)\
	$(LEPTOBJ)binexpand.$(OBJ)\
	$(LEPTOBJ)binreduce.$(OBJ)\
	$(LEPTOBJ)blend.$(OBJ)\
	$(LEPTOBJ)bmf.$(OBJ)\
	$(LEPTOBJ)bmpio.$(OBJ)\
	$(LEPTOBJ)bmpiostub.$(OBJ)\
	$(LEPTOBJ)bootnumgen1.$(OBJ)\
	$(LEPTOBJ)bootnumgen2.$(OBJ)\
	$(LEPTOBJ)bootnumgen3.$(OBJ)\
	$(LEPTOBJ)bootnumgen4.$(OBJ)\
	$(LEPTOBJ)boxbasic.$(OBJ)\
	$(LEPTOBJ)boxfunc1.$(OBJ)\
	$(LEPTOBJ)boxfunc2.$(OBJ)\
	$(LEPTOBJ)boxfunc3.$(OBJ)\
	$(LEPTOBJ)boxfunc4.$(OBJ)\
	$(LEPTOBJ)boxfunc5.$(OBJ)\
	$(LEPTOBJ)bytearray.$(OBJ)\
	$(LEPTOBJ)ccbord.$(OBJ)\
	$(LEPTOBJ)classapp.$(OBJ)\
	$(LEPTOBJ)colorcontent.$(OBJ)\
	$(LEPTOBJ)coloring.$(OBJ)\
	$(LEPTOBJ)colormap.$(OBJ)\
	$(LEPTOBJ)colormorph.$(OBJ)\
	$(LEPTOBJ)colorquant1.$(OBJ)\
	$(LEPTOBJ)colorquant2.$(OBJ)\
	$(LEPTOBJ)colorseg.$(OBJ)\
	$(LEPTOBJ)colorspace.$(OBJ)\
	$(LEPTOBJ)compare.$(OBJ)\
	$(LEPTOBJ)conncomp.$(OBJ)\
	$(LEPTOBJ)convertfiles.$(OBJ)\
	$(LEPTOBJ)convolve.$(OBJ)\
	$(LEPTOBJ)correlscore.$(OBJ)\
	$(LEPTOBJ)dewarp1.$(OBJ)\
	$(LEPTOBJ)dewarp2.$(OBJ)\
	$(LEPTOBJ)dewarp3.$(OBJ)\
	$(LEPTOBJ)dewarp4.$(OBJ)\
	$(LEPTOBJ)dnabasic.$(OBJ)\
	$(LEPTOBJ)dnafunc1.$(OBJ)\
	$(LEPTOBJ)dnahash.$(OBJ)\
	$(LEPTOBJ)dwacomb.2.$(OBJ)\
	$(LEPTOBJ)dwacomblow.2.$(OBJ)\
	$(LEPTOBJ)edge.$(OBJ)\
	$(LEPTOBJ)encoding.$(OBJ)\
	$(LEPTOBJ)enhance.$(OBJ)\
	$(LEPTOBJ)fhmtauto.$(OBJ)\
	$(LEPTOBJ)fhmtgenlow.1.$(OBJ)\
	$(LEPTOBJ)fmorphauto.$(OBJ)\
	$(LEPTOBJ)fmorphgen.1.$(OBJ)\
	$(LEPTOBJ)fmorphgenlow.1.$(OBJ)\
	$(LEPTOBJ)fpix1.$(OBJ)\
	$(LEPTOBJ)fpix2.$(OBJ)\
	$(LEPTOBJ)gifiostub.$(OBJ)\
	$(LEPTOBJ)gplot.$(OBJ)\
	$(LEPTOBJ)graphics.$(OBJ)\
	$(LEPTOBJ)graymorph.$(OBJ)\
	$(LEPTOBJ)grayquant.$(OBJ)\
	$(LEPTOBJ)hashmap.$(OBJ)\
	$(LEPTOBJ)heap.$(OBJ)\
	$(LEPTOBJ)jbclass.$(OBJ)\
	$(LEPTOBJ)jp2kheader.$(OBJ)\
	$(LEPTOBJ)jp2kheaderstub.$(OBJ)\
	$(LEPTOBJ)jp2kiostub.$(OBJ)\
	$(LEPTOBJ)jpegiostub.$(OBJ)\
	$(LEPTOBJ)kernel.$(OBJ)\
	$(LEPTOBJ)libversions.$(OBJ)\
	$(LEPTOBJ)list.$(OBJ)\
	$(LEPTOBJ)map.$(OBJ)\
	$(LEPTOBJ)morph.$(OBJ)\
	$(LEPTOBJ)morphapp.$(OBJ)\
	$(LEPTOBJ)morphdwa.$(OBJ)\
	$(LEPTOBJ)morphseq.$(OBJ)\
	$(LEPTOBJ)numabasic.$(OBJ)\
	$(LEPTOBJ)numafunc1.$(OBJ)\
	$(LEPTOBJ)numafunc2.$(OBJ)\
	$(LEPTOBJ)pageseg.$(OBJ)\
	$(LEPTOBJ)paintcmap.$(OBJ)\
	$(LEPTOBJ)partify.$(OBJ)\
	$(LEPTOBJ)partition.$(OBJ)\
	$(LEPTOBJ)pdfio1.$(OBJ)\
	$(LEPTOBJ)pdfio1stub.$(OBJ)\
	$(LEPTOBJ)pdfio2.$(OBJ)\
	$(LEPTOBJ)pdfio2stub.$(OBJ)\
	$(LEPTOBJ)pix1.$(OBJ)\
	$(LEPTOBJ)pix2.$(OBJ)\
	$(LEPTOBJ)pix3.$(OBJ)\
	$(LEPTOBJ)pix4.$(OBJ)\
	$(LEPTOBJ)pix5.$(OBJ)\
	$(LEPTOBJ)pixabasic.$(OBJ)\
	$(LEPTOBJ)pixacc.$(OBJ)\
	$(LEPTOBJ)pixafunc1.$(OBJ)\
	$(LEPTOBJ)pixafunc2.$(OBJ)\
	$(LEPTOBJ)pixalloc.$(OBJ)\
	$(LEPTOBJ)pixarith.$(OBJ)\
	$(LEPTOBJ)pixcomp.$(OBJ)\
	$(LEPTOBJ)pixconv.$(OBJ)\
	$(LEPTOBJ)pixlabel.$(OBJ)\
	$(LEPTOBJ)pixtiling.$(OBJ)\
	$(LEPTOBJ)pngiostub.$(OBJ)\
	$(LEPTOBJ)pnmio.$(OBJ)\
	$(LEPTOBJ)pnmiostub.$(OBJ)\
	$(LEPTOBJ)projective.$(OBJ)\
	$(LEPTOBJ)psio1.$(OBJ)\
	$(LEPTOBJ)psio1stub.$(OBJ)\
	$(LEPTOBJ)psio2.$(OBJ)\
	$(LEPTOBJ)psio2stub.$(OBJ)\
	$(LEPTOBJ)ptabasic.$(OBJ)\
	$(LEPTOBJ)ptafunc1.$(OBJ)\
	$(LEPTOBJ)ptafunc2.$(OBJ)\
	$(LEPTOBJ)ptra.$(OBJ)\
	$(LEPTOBJ)quadtree.$(OBJ)\
	$(LEPTOBJ)queue.$(OBJ)\
	$(LEPTOBJ)rank.$(OBJ)\
	$(LEPTOBJ)rbtree.$(OBJ)\
	$(LEPTOBJ)readfile.$(OBJ)\
	$(LEPTOBJ)regutils.$(OBJ)\
	$(LEPTOBJ)rop.$(OBJ)\
	$(LEPTOBJ)roplow.$(OBJ)\
	$(LEPTOBJ)rotate.$(OBJ)\
	$(LEPTOBJ)rotateam.$(OBJ)\
	$(LEPTOBJ)rotateorth.$(OBJ)\
	$(LEPTOBJ)rotateshear.$(OBJ)\
	$(LEPTOBJ)runlength.$(OBJ)\
	$(LEPTOBJ)sarray1.$(OBJ)\
	$(LEPTOBJ)sarray2.$(OBJ)\
	$(LEPTOBJ)scale1.$(OBJ)\
	$(LEPTOBJ)scale2.$(OBJ)\
	$(LEPTOBJ)seedfill.$(OBJ)\
	$(LEPTOBJ)sel1.$(OBJ)\
	$(LEPTOBJ)sel2.$(OBJ)\
	$(LEPTOBJ)selgen.$(OBJ)\
	$(LEPTOBJ)shear.$(OBJ)\
	$(LEPTOBJ)skew.$(OBJ)\
	$(LEPTOBJ)spixio.$(OBJ)\
	$(LEPTOBJ)stack.$(OBJ)\
	$(LEPTOBJ)stringcode.$(OBJ)\
	$(LEPTOBJ)sudoku.$(OBJ)\
	$(LEPTOBJ)textops.$(OBJ)\
	$(LEPTOBJ)tiffiostub.$(OBJ)\
	$(LEPTOBJ)utils1.$(OBJ)\
	$(LEPTOBJ)utils2.$(OBJ)\
	$(LEPTOBJ)warper.$(OBJ)\
	$(LEPTOBJ)webpiostub.$(OBJ)\
	$(LEPTOBJ)writefile.$(OBJ)\
	$(LEPTOBJ)zlibmem.$(OBJ)\
	$(LEPTOBJ)zlibmemstub.$(OBJ)

#	$(LEPTOBJ)strokes.$(OBJ)\
