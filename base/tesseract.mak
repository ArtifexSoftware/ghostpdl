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
# makefile for "local source" tesseract code.

TESSINCLUDES=\
	$(I_)$(TESSERACTDIR)/include$(_I)\
	$(I_)$(TESSERACTDIR)/src/api$(_I)\
	$(I_)$(TESSERACTDIR)/src/arch$(_I)\
	$(I_)$(TESSERACTDIR)/src/ccmain$(_I)\
	$(I_)$(TESSERACTDIR)/src/ccstruct$(_I)\
	$(I_)$(TESSERACTDIR)/src/ccutil$(_I)\
	$(I_)$(TESSERACTDIR)/src/classify$(_I)\
	$(I_)$(TESSERACTDIR)/src/cutil$(_I)\
	$(I_)$(TESSERACTDIR)/src/dict$(_I)\
	$(I_)$(TESSERACTDIR)/src/lstm$(_I)\
	$(I_)$(TESSERACTDIR)/src/opencl$(_I)\
	$(I_)$(TESSERACTDIR)/src/textord$(_I)\
	$(I_)$(TESSERACTDIR)/src/training$(_I)\
	$(I_)$(TESSERACTDIR)/src/viewer$(_I)\
	$(I_)$(TESSERACTDIR)/src/wordrec$(_I)\
	$(I_)$(LEPTONICADIR)/src$(_I)\
	$(I_)$(GLSRCDIR)$(_I)\
	$(I_)$(GLGENDIR)$(_I)

# If we wanted to enable the legacy mode in tesseract, we'd:
#   remove  -DDISABLED_LEGACY_ENGINE from TESSCXX
#   set TESSERACT_LEGACY to include TESSERACT_LEGACY_OBJS

# We set -DCLUSTER when doing builds for our testing cluster. Unfortunately,
# this conflicts with Tesseract's use of a CLUSTER type. We work around this
# here by undefining CLUSTER for the tesseract portion of the build.

TESSCXX = $(CXX) $(TESSINCLUDES) $(TESSCXXFLAGS) $(CCFLAGS) -DTESSERACT_IMAGEDATA_AS_PIX -DTESSERACT_DISABLE_DEBUG_FONTS -DGRAPHICS_DISABLED -UCLUSTER -DDISABLED_LEGACY_ENGINE
TESSOBJ = $(GLOBJDIR)$(D)tesseract_
TESSO_ = $(O_)$(TESSOBJ)

TESSDEPS=\
	$(arch_h)\
	$(GLSRCDIR)/tesseract.mak\
	$(GLGENDIR)/tesseract/version.h\
	$(TESSERACTDIR)/include/tesseract/baseapi.h\
	$(TESSERACTDIR)/include/tesseract/capi.h\
	$(TESSERACTDIR)/include/tesseract/ltrresultiterator.h\
	$(TESSERACTDIR)/include/tesseract/ocrclass.h\
	$(TESSERACTDIR)/include/tesseract/osdetect.h\
	$(TESSERACTDIR)/include/tesseract/pageiterator.h\
	$(TESSERACTDIR)/include/tesseract/publictypes.h\
	$(TESSERACTDIR)/include/tesseract/renderer.h\
	$(TESSERACTDIR)/include/tesseract/resultiterator.h\
	$(TESSERACTDIR)/include/tesseract/unichar.h\
	$(TESSERACTDIR)/src/arch/dotproduct.h\
	$(TESSERACTDIR)/src/arch/intsimdmatrix.h\
	$(TESSERACTDIR)/src/arch/simddetect.h\
	$(TESSERACTDIR)/src/ccmain/control.h\
	$(TESSERACTDIR)/src/ccmain/docqual.h\
	$(TESSERACTDIR)/src/ccmain/equationdetect.h\
	$(TESSERACTDIR)/src/ccmain/fixspace.h\
	$(TESSERACTDIR)/src/ccmain/mutableiterator.h\
	$(TESSERACTDIR)/src/ccmain/output.h\
	$(TESSERACTDIR)/src/ccmain/paragraphs.h\
	$(TESSERACTDIR)/src/ccmain/paragraphs_internal.h\
	$(TESSERACTDIR)/src/ccmain/paramsd.h\
	$(TESSERACTDIR)/src/ccmain/pgedit.h\
	$(TESSERACTDIR)/src/ccmain/reject.h\
	$(TESSERACTDIR)/src/ccmain/tesseractclass.h\
	$(TESSERACTDIR)/src/ccmain/tessvars.h\
	$(TESSERACTDIR)/src/ccmain/werdit.h\
	$(TESSERACTDIR)/src/ccstruct/blamer.h\
	$(TESSERACTDIR)/src/ccstruct/blobbox.h\
	$(TESSERACTDIR)/src/ccstruct/blobs.h\
	$(TESSERACTDIR)/src/ccstruct/blread.h\
	$(TESSERACTDIR)/src/ccstruct/boxread.h\
	$(TESSERACTDIR)/src/ccstruct/boxword.h\
	$(TESSERACTDIR)/src/ccstruct/ccstruct.h\
	$(TESSERACTDIR)/src/ccstruct/coutln.h\
	$(TESSERACTDIR)/src/ccstruct/detlinefit.h\
	$(TESSERACTDIR)/src/ccstruct/dppoint.h\
	$(TESSERACTDIR)/src/ccstruct/fontinfo.h\
	$(TESSERACTDIR)/src/ccstruct/imagedata.h\
	$(TESSERACTDIR)/src/ccstruct/linlsq.h\
	$(TESSERACTDIR)/src/ccstruct/matrix.h\
	$(TESSERACTDIR)/src/ccstruct/mod128.h\
	$(TESSERACTDIR)/src/ccstruct/normalis.h\
	$(TESSERACTDIR)/src/ccstruct/ocrblock.h\
	$(TESSERACTDIR)/src/ccstruct/ocrpara.h\
	$(TESSERACTDIR)/src/ccstruct/ocrrow.h\
	$(TESSERACTDIR)/src/ccstruct/otsuthr.h\
	$(TESSERACTDIR)/src/ccstruct/pageres.h\
	$(TESSERACTDIR)/src/ccstruct/params_training_featdef.h\
	$(TESSERACTDIR)/src/ccstruct/pdblock.h\
	$(TESSERACTDIR)/src/ccstruct/points.h\
	$(TESSERACTDIR)/src/ccstruct/polyaprx.h\
	$(TESSERACTDIR)/src/ccstruct/polyblk.h\
	$(TESSERACTDIR)/src/ccstruct/quadlsq.h\
	$(TESSERACTDIR)/src/ccstruct/quadratc.h\
	$(TESSERACTDIR)/src/ccstruct/quspline.h\
	$(TESSERACTDIR)/src/ccstruct/ratngs.h\
	$(TESSERACTDIR)/src/ccstruct/rect.h\
	$(TESSERACTDIR)/src/ccstruct/rejctmap.h\
	$(TESSERACTDIR)/src/ccstruct/seam.h\
	$(TESSERACTDIR)/src/ccstruct/split.h\
	$(TESSERACTDIR)/src/ccstruct/statistc.h\
	$(TESSERACTDIR)/src/ccstruct/stepblob.h\
	$(TESSERACTDIR)/src/ccstruct/werd.h\
	$(TESSERACTDIR)/src/ccutil/ambigs.h\
	$(TESSERACTDIR)/src/ccutil/bitvector.h\
	$(TESSERACTDIR)/src/ccutil/ccutil.h\
	$(TESSERACTDIR)/src/ccutil/clst.h\
	$(TESSERACTDIR)/src/ccutil/elst.h\
	$(TESSERACTDIR)/src/ccutil/elst2.h\
	$(TESSERACTDIR)/src/ccutil/errcode.h\
	$(TESSERACTDIR)/src/ccutil/fileerr.h\
	$(TESSERACTDIR)/src/ccutil/genericheap.h\
	$(TESSERACTDIR)/src/ccutil/host.h\
	$(TESSERACTDIR)/src/ccutil/indexmapbidi.h\
	$(TESSERACTDIR)/src/ccutil/kdpair.h\
	$(TESSERACTDIR)/src/ccutil/lsterr.h\
	$(TESSERACTDIR)/src/ccutil/object_cache.h\
	$(TESSERACTDIR)/src/ccutil/params.h\
	$(TESSERACTDIR)/src/ccutil/qrsequence.h\
	$(TESSERACTDIR)/src/ccutil/scanutils.h\
	$(TESSERACTDIR)/src/ccutil/sorthelper.h\
	$(TESSERACTDIR)/src/ccutil/tessdatamanager.h\
	$(TESSERACTDIR)/src/ccutil/tprintf.h\
	$(TESSERACTDIR)/src/ccutil/unicharcompress.h\
	$(TESSERACTDIR)/src/ccutil/unicharmap.h\
	$(TESSERACTDIR)/src/ccutil/unicharset.h\
	$(TESSERACTDIR)/src/ccutil/unicity_table.h\
	$(TESSERACTDIR)/src/ccutil/universalambigs.h\
	$(TESSERACTDIR)/src/classify/adaptive.h\
	$(TESSERACTDIR)/src/classify/classify.h\
	$(TESSERACTDIR)/src/classify/cluster.h\
	$(TESSERACTDIR)/src/classify/clusttool.h\
	$(TESSERACTDIR)/src/classify/featdefs.h\
	$(TESSERACTDIR)/src/classify/float2int.h\
	$(TESSERACTDIR)/src/classify/fpoint.h\
	$(TESSERACTDIR)/src/classify/intfeaturespace.h\
	$(TESSERACTDIR)/src/classify/intfx.h\
	$(TESSERACTDIR)/src/classify/intmatcher.h\
	$(TESSERACTDIR)/src/classify/intproto.h\
	$(TESSERACTDIR)/src/classify/kdtree.h\
	$(TESSERACTDIR)/src/classify/mf.h\
	$(TESSERACTDIR)/src/classify/mfdefs.h\
	$(TESSERACTDIR)/src/classify/mfoutline.h\
	$(TESSERACTDIR)/src/classify/mfx.h\
	$(TESSERACTDIR)/src/classify/normfeat.h\
	$(TESSERACTDIR)/src/classify/normmatch.h\
	$(TESSERACTDIR)/src/classify/ocrfeatures.h\
	$(TESSERACTDIR)/src/classify/outfeat.h\
	$(TESSERACTDIR)/src/classify/picofeat.h\
	$(TESSERACTDIR)/src/classify/protos.h\
	$(TESSERACTDIR)/src/classify/shapeclassifier.h\
	$(TESSERACTDIR)/src/classify/shapetable.h\
	$(TESSERACTDIR)/src/classify/tessclassifier.h\
	$(TESSERACTDIR)/src/classify/trainingsample.h\
	$(TESSERACTDIR)/src/cutil/bitvec.h\
	$(TESSERACTDIR)/src/cutil/oldlist.h\
	$(TESSERACTDIR)/src/dict/dawg.h\
	$(TESSERACTDIR)/src/dict/dawg_cache.h\
	$(TESSERACTDIR)/src/dict/dict.h\
	$(TESSERACTDIR)/src/dict/matchdefs.h\
	$(TESSERACTDIR)/src/dict/stopper.h\
	$(TESSERACTDIR)/src/dict/trie.h\
	$(TESSERACTDIR)/src/lstm/convolve.h\
	$(TESSERACTDIR)/src/lstm/fullyconnected.h\
	$(TESSERACTDIR)/src/lstm/functions.h\
	$(TESSERACTDIR)/src/lstm/input.h\
	$(TESSERACTDIR)/src/lstm/lstm.h\
	$(TESSERACTDIR)/src/lstm/lstmrecognizer.h\
	$(TESSERACTDIR)/src/lstm/maxpool.h\
	$(TESSERACTDIR)/src/lstm/network.h\
	$(TESSERACTDIR)/src/lstm/networkio.h\
	$(TESSERACTDIR)/src/lstm/networkscratch.h\
	$(TESSERACTDIR)/src/lstm/parallel.h\
	$(TESSERACTDIR)/src/lstm/plumbing.h\
	$(TESSERACTDIR)/src/lstm/recodebeam.h\
	$(TESSERACTDIR)/src/lstm/reconfig.h\
	$(TESSERACTDIR)/src/lstm/reversed.h\
	$(TESSERACTDIR)/src/lstm/series.h\
	$(TESSERACTDIR)/src/lstm/static_shape.h\
	$(TESSERACTDIR)/src/lstm/stridemap.h\
	$(TESSERACTDIR)/src/lstm/tfnetwork.h\
	$(TESSERACTDIR)/src/lstm/weightmatrix.h\
	$(TESSERACTDIR)/src/opencl/oclkernels.h\
	$(TESSERACTDIR)/src/opencl/openclwrapper.h\
	$(TESSERACTDIR)/src/textord/alignedblob.h\
	$(TESSERACTDIR)/src/textord/baselinedetect.h\
	$(TESSERACTDIR)/src/textord/bbgrid.h\
	$(TESSERACTDIR)/src/textord/blkocc.h\
	$(TESSERACTDIR)/src/textord/blobgrid.h\
	$(TESSERACTDIR)/src/textord/ccnontextdetect.h\
	$(TESSERACTDIR)/src/textord/cjkpitch.h\
	$(TESSERACTDIR)/src/textord/colfind.h\
	$(TESSERACTDIR)/src/textord/colpartition.h\
	$(TESSERACTDIR)/src/textord/colpartitiongrid.h\
	$(TESSERACTDIR)/src/textord/colpartitionset.h\
	$(TESSERACTDIR)/src/textord/devanagari_processing.h\
	$(TESSERACTDIR)/src/textord/drawtord.h\
	$(TESSERACTDIR)/src/textord/edgblob.h\
	$(TESSERACTDIR)/src/textord/edgloop.h\
	$(TESSERACTDIR)/src/textord/equationdetectbase.h\
	$(TESSERACTDIR)/src/textord/fpchop.h\
	$(TESSERACTDIR)/src/textord/gap_map.h\
	$(TESSERACTDIR)/src/textord/imagefind.h\
	$(TESSERACTDIR)/src/textord/linefind.h\
	$(TESSERACTDIR)/src/textord/makerow.h\
	$(TESSERACTDIR)/src/textord/oldbasel.h\
	$(TESSERACTDIR)/src/textord/pithsync.h\
	$(TESSERACTDIR)/src/textord/pitsync1.h\
	$(TESSERACTDIR)/src/textord/scanedg.h\
	$(TESSERACTDIR)/src/textord/sortflts.h\
	$(TESSERACTDIR)/src/textord/strokewidth.h\
	$(TESSERACTDIR)/src/textord/tabfind.h\
	$(TESSERACTDIR)/src/textord/tablefind.h\
	$(TESSERACTDIR)/src/textord/tablerecog.h\
	$(TESSERACTDIR)/src/textord/tabvector.h\
	$(TESSERACTDIR)/src/textord/textlineprojection.h\
	$(TESSERACTDIR)/src/textord/textord.h\
	$(TESSERACTDIR)/src/textord/topitch.h\
	$(TESSERACTDIR)/src/textord/tordmain.h\
	$(TESSERACTDIR)/src/textord/tovars.h\
	$(TESSERACTDIR)/src/textord/underlin.h\
	$(TESSERACTDIR)/src/textord/wordseg.h\
	$(TESSERACTDIR)/src/textord/workingpartset.h\
	$(TESSERACTDIR)/src/viewer/scrollview.h\
	$(TESSERACTDIR)/src/viewer/svmnode.h\
	$(TESSERACTDIR)/src/viewer/svutil.h\
	$(TESSERACTDIR)/src/wordrec/associate.h\
	$(TESSERACTDIR)/src/wordrec/chop.h\
	$(TESSERACTDIR)/src/wordrec/drawfx.h\
	$(TESSERACTDIR)/src/wordrec/findseam.h\
	$(TESSERACTDIR)/src/wordrec/language_model.h\
	$(TESSERACTDIR)/src/wordrec/lm_consistency.h\
	$(TESSERACTDIR)/src/wordrec/lm_pain_points.h\
	$(TESSERACTDIR)/src/wordrec/lm_state.h\
	$(TESSERACTDIR)/src/wordrec/outlines.h\
	$(TESSERACTDIR)/src/wordrec/params_model.h\
	$(TESSERACTDIR)/src/wordrec/plotedges.h\
	$(TESSERACTDIR)/src/wordrec/render.h\
	$(TESSERACTDIR)/src/wordrec/wordrec.h\
	$(MAKEDIRS)

$(GLGENDIR)/tesseract/version.h : $(ECHOGS_XE) $(GLSRCDIR)/tesseract.mak
	-mkdir $(GLGENDIR)$(D)tesseract
	$(ECHOGS_XE) -w $(GLGENDIR)/tesseract/version.h -x 23 define TESSERACT_VERSION_STR -x 2022 5.0.0-beta-gs -x 22


$(TESSOBJ)api_baseapi.$(OBJ) : $(TESSERACTDIR)/src/api/baseapi.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_baseapi.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/baseapi.cpp

$(TESSOBJ)api_altorenderer.$(OBJ) : $(TESSERACTDIR)/src/api/altorenderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_altorenderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/altorenderer.cpp

$(TESSOBJ)api_capi.$(OBJ) : $(TESSERACTDIR)/src/api/capi.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_capi.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/capi.cpp

$(TESSOBJ)api_hocrrenderer.$(OBJ) : $(TESSERACTDIR)/src/api/hocrrenderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_hocrrenderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/hocrrenderer.cpp

$(TESSOBJ)api_lstmboxrenderer.$(OBJ) : $(TESSERACTDIR)/src/api/lstmboxrenderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_lstmboxrenderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/lstmboxrenderer.cpp

$(TESSOBJ)api_pdfrenderer.$(OBJ) : $(TESSERACTDIR)/src/api/pdfrenderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_pdfrenderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/pdfrenderer.cpp

$(TESSOBJ)api_renderer.$(OBJ) : $(TESSERACTDIR)/src/api/renderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_renderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/renderer.cpp

$(TESSOBJ)api_wordstrboxrenderer.$(OBJ) : $(TESSERACTDIR)/src/api/wordstrboxrenderer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)api_wordstrboxrenderer.$(OBJ) $(C_) $(TESSERACTDIR)/src/api/wordstrboxrenderer.cpp

$(TESSOBJ)arch_intsimdmatrix.$(OBJ) : $(TESSERACTDIR)/src/arch/intsimdmatrix.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)arch_intsimdmatrix.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/intsimdmatrix.cpp

$(TESSOBJ)arch_simddetect.$(OBJ) : $(TESSERACTDIR)/src/arch/simddetect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)arch_simddetect.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/simddetect.cpp

$(TESSOBJ)ccmain_applybox.$(OBJ) : $(TESSERACTDIR)/src/ccmain/applybox.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_applybox.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/applybox.cpp

$(TESSOBJ)ccmain_control.$(OBJ) : $(TESSERACTDIR)/src/ccmain/control.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_control.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/control.cpp

$(TESSOBJ)ccmain_docqual.$(OBJ) : $(TESSERACTDIR)/src/ccmain/docqual.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_docqual.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/docqual.cpp

$(TESSOBJ)ccmain_equationdetect.$(OBJ) : $(TESSERACTDIR)/src/ccmain/equationdetect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_equationdetect.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/equationdetect.cpp

$(TESSOBJ)ccmain_linerec.$(OBJ) : $(TESSERACTDIR)/src/ccmain/linerec.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_linerec.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/linerec.cpp

$(TESSOBJ)ccmain_ltrresultiterator.$(OBJ) : $(TESSERACTDIR)/src/ccmain/ltrresultiterator.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_ltrresultiterator.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/ltrresultiterator.cpp

$(TESSOBJ)ccmain_mutableiterator.$(OBJ) : $(TESSERACTDIR)/src/ccmain/mutableiterator.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_mutableiterator.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/mutableiterator.cpp

$(TESSOBJ)ccmain_output.$(OBJ) : $(TESSERACTDIR)/src/ccmain/output.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_output.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/output.cpp

$(TESSOBJ)ccmain_osdetect.$(OBJ) : $(TESSERACTDIR)/src/ccmain/osdetect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_osdetect.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/osdetect.cpp

$(TESSOBJ)ccmain_pageiterator.$(OBJ) : $(TESSERACTDIR)/src/ccmain/pageiterator.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_pageiterator.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/pageiterator.cpp

$(TESSOBJ)ccmain_pagesegmain.$(OBJ) : $(TESSERACTDIR)/src/ccmain/pagesegmain.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_pagesegmain.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/pagesegmain.cpp

$(TESSOBJ)ccmain_pagewalk.$(OBJ) : $(TESSERACTDIR)/src/ccmain/pagewalk.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_pagewalk.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/pagewalk.cpp

$(TESSOBJ)ccmain_paragraphs.$(OBJ) : $(TESSERACTDIR)/src/ccmain/paragraphs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_paragraphs.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/paragraphs.cpp

$(TESSOBJ)ccmain_paramsd.$(OBJ) : $(TESSERACTDIR)/src/ccmain/paramsd.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_paramsd.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/paramsd.cpp

$(TESSOBJ)ccmain_par_control.$(OBJ) : $(TESSERACTDIR)/src/ccmain/par_control.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_par_control.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/par_control.cpp

$(TESSOBJ)ccmain_pgedit.$(OBJ) : $(TESSERACTDIR)/src/ccmain/pgedit.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_pgedit.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/pgedit.cpp

$(TESSOBJ)ccmain_reject.$(OBJ) : $(TESSERACTDIR)/src/ccmain/reject.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_reject.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/reject.cpp

$(TESSOBJ)ccmain_resultiterator.$(OBJ) : $(TESSERACTDIR)/src/ccmain/resultiterator.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_resultiterator.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/resultiterator.cpp

$(TESSOBJ)ccmain_superscript.$(OBJ) : $(TESSERACTDIR)/src/ccmain/superscript.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_superscript.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/superscript.cpp

$(TESSOBJ)ccmain_tessbox.$(OBJ) : $(TESSERACTDIR)/src/ccmain/tessbox.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_tessbox.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/tessbox.cpp

$(TESSOBJ)ccmain_tessedit.$(OBJ) : $(TESSERACTDIR)/src/ccmain/tessedit.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_tessedit.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/tessedit.cpp

$(TESSOBJ)ccmain_tesseractclass.$(OBJ) : $(TESSERACTDIR)/src/ccmain/tesseractclass.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_tesseractclass.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/tesseractclass.cpp

$(TESSOBJ)ccmain_tessvars.$(OBJ) : $(TESSERACTDIR)/src/ccmain/tessvars.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_tessvars.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/tessvars.cpp

$(TESSOBJ)ccmain_tfacepp.$(OBJ) : $(TESSERACTDIR)/src/ccmain/tfacepp.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_tfacepp.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/tfacepp.cpp

$(TESSOBJ)ccmain_thresholder.$(OBJ) : $(TESSERACTDIR)/src/ccmain/thresholder.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_thresholder.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/thresholder.cpp

$(TESSOBJ)ccmain_werdit.$(OBJ) : $(TESSERACTDIR)/src/ccmain/werdit.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_werdit.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/werdit.cpp

$(TESSOBJ)ccmain_adaptions.$(OBJ) : $(TESSERACTDIR)/src/ccmain/adaptions.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_adaptions.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/adaptions.cpp

$(TESSOBJ)ccmain_fixspace.$(OBJ) : $(TESSERACTDIR)/src/ccmain/fixspace.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_fixspace.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/fixspace.cpp

$(TESSOBJ)ccmain_fixxht.$(OBJ) : $(TESSERACTDIR)/src/ccmain/fixxht.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_fixxht.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/fixxht.cpp

$(TESSOBJ)ccmain_recogtraining.$(OBJ) : $(TESSERACTDIR)/src/ccmain/recogtraining.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccmain_recogtraining.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccmain/recogtraining.cpp

$(TESSOBJ)ccstruct_blamer.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/blamer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_blamer.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/blamer.cpp

$(TESSOBJ)ccstruct_blobbox.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/blobbox.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_blobbox.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/blobbox.cpp

$(TESSOBJ)ccstruct_blobs.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/blobs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_blobs.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/blobs.cpp

$(TESSOBJ)ccstruct_blread.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/blread.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_blread.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/blread.cpp

$(TESSOBJ)ccstruct_boxread.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/boxread.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_boxread.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/boxread.cpp

$(TESSOBJ)ccstruct_boxword.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/boxword.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_boxword.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/boxword.cpp

$(TESSOBJ)ccstruct_ccstruct.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/ccstruct.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_ccstruct.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/ccstruct.cpp

$(TESSOBJ)ccstruct_coutln.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/coutln.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_coutln.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/coutln.cpp

$(TESSOBJ)ccstruct_detlinefit.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/detlinefit.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_detlinefit.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/detlinefit.cpp

$(TESSOBJ)ccstruct_dppoint.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/dppoint.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_dppoint.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/dppoint.cpp

$(TESSOBJ)ccstruct_image.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/image.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_image.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/image.cpp

$(TESSOBJ)ccstruct_imagedata.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/imagedata.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_imagedata.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/imagedata.cpp

$(TESSOBJ)ccstruct_linlsq.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/linlsq.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_linlsq.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/linlsq.cpp

$(TESSOBJ)ccstruct_matrix.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/matrix.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_matrix.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/matrix.cpp

$(TESSOBJ)ccstruct_mod128.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/mod128.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_mod128.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/mod128.cpp

$(TESSOBJ)ccstruct_normalis.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/normalis.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_normalis.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/normalis.cpp

$(TESSOBJ)ccstruct_ocrblock.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/ocrblock.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_ocrblock.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/ocrblock.cpp

$(TESSOBJ)ccstruct_ocrpara.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/ocrpara.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_ocrpara.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/ocrpara.cpp

$(TESSOBJ)ccstruct_ocrrow.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/ocrrow.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_ocrrow.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/ocrrow.cpp

$(TESSOBJ)ccstruct_otsuthr.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/otsuthr.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_otsuthr.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/otsuthr.cpp

$(TESSOBJ)ccstruct_pageres.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/pageres.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_pageres.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/pageres.cpp

$(TESSOBJ)ccstruct_pdblock.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/pdblock.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_pdblock.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/pdblock.cpp

$(TESSOBJ)ccstruct_points.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/points.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_points.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/points.cpp

$(TESSOBJ)ccstruct_polyaprx.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/polyaprx.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_polyaprx.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/polyaprx.cpp

$(TESSOBJ)ccstruct_polyblk.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/polyblk.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_polyblk.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/polyblk.cpp

$(TESSOBJ)ccstruct_quadlsq.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/quadlsq.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_quadlsq.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/quadlsq.cpp

$(TESSOBJ)ccstruct_quspline.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/quspline.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_quspline.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/quspline.cpp

$(TESSOBJ)ccstruct_ratngs.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/ratngs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_ratngs.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/ratngs.cpp

$(TESSOBJ)ccstruct_rect.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/rect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_rect.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/rect.cpp

$(TESSOBJ)ccstruct_rejctmap.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/rejctmap.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_rejctmap.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/rejctmap.cpp

$(TESSOBJ)ccstruct_seam.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/seam.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_seam.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/seam.cpp

$(TESSOBJ)ccstruct_split.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/split.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_split.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/split.cpp

$(TESSOBJ)ccstruct_statistc.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/statistc.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_statistc.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/statistc.cpp

$(TESSOBJ)ccstruct_stepblob.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/stepblob.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_stepblob.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/stepblob.cpp

$(TESSOBJ)ccstruct_werd.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/werd.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_werd.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/werd.cpp

$(TESSOBJ)ccstruct_fontinfo.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/fontinfo.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_fontinfo.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/fontinfo.cpp

$(TESSOBJ)ccstruct_params_training_featdef.$(OBJ) : $(TESSERACTDIR)/src/ccstruct/params_training_featdef.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccstruct_params_training_featdef.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccstruct/params_training_featdef.cpp

$(TESSOBJ)classify_adaptive.$(OBJ) : $(TESSERACTDIR)/src/classify/adaptive.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_adaptive.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/adaptive.cpp

$(TESSOBJ)classify_adaptmatch.$(OBJ) : $(TESSERACTDIR)/src/classify/adaptmatch.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_adaptmatch.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/adaptmatch.cpp

$(TESSOBJ)classify_blobclass.$(OBJ) : $(TESSERACTDIR)/src/classify/blobclass.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_blobclass.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/blobclass.cpp

$(TESSOBJ)classify_classify.$(OBJ) : $(TESSERACTDIR)/src/classify/classify.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_classify.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/classify.cpp

$(TESSOBJ)classify_cluster.$(OBJ) : $(TESSERACTDIR)/src/classify/cluster.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_cluster.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/cluster.cpp

$(TESSOBJ)classify_clusttool.$(OBJ) : $(TESSERACTDIR)/src/classify/clusttool.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_clusttool.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/clusttool.cpp

$(TESSOBJ)classify_cutoffs.$(OBJ) : $(TESSERACTDIR)/src/classify/cutoffs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_cutoffs.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/cutoffs.cpp

$(TESSOBJ)classify_float2int.$(OBJ) : $(TESSERACTDIR)/src/classify/float2int.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_float2int.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/float2int.cpp

$(TESSOBJ)classify_featdefs.$(OBJ) : $(TESSERACTDIR)/src/classify/featdefs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_featdefs.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/featdefs.cpp

$(TESSOBJ)classify_fpoint.$(OBJ) : $(TESSERACTDIR)/src/classify/fpoint.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_fpoint.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/fpoint.cpp

$(TESSOBJ)classify_intfeaturespace.$(OBJ) : $(TESSERACTDIR)/src/classify/intfeaturespace.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_intfeaturespace.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/intfeaturespace.cpp

$(TESSOBJ)classify_intfx.$(OBJ) : $(TESSERACTDIR)/src/classify/intfx.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_intfx.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/intfx.cpp

$(TESSOBJ)classify_intmatcher.$(OBJ) : $(TESSERACTDIR)/src/classify/intmatcher.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_intmatcher.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/intmatcher.cpp

$(TESSOBJ)classify_intproto.$(OBJ) : $(TESSERACTDIR)/src/classify/intproto.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_intproto.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/intproto.cpp

$(TESSOBJ)classify_kdtree.$(OBJ) : $(TESSERACTDIR)/src/classify/kdtree.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_kdtree.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/kdtree.cpp

$(TESSOBJ)classify_mf.$(OBJ) : $(TESSERACTDIR)/src/classify/mf.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_mf.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/mf.cpp

$(TESSOBJ)classify_mfoutline.$(OBJ) : $(TESSERACTDIR)/src/classify/mfoutline.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_mfoutline.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/mfoutline.cpp

$(TESSOBJ)classify_mfx.$(OBJ) : $(TESSERACTDIR)/src/classify/mfx.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_mfx.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/mfx.cpp

$(TESSOBJ)classify_normfeat.$(OBJ) : $(TESSERACTDIR)/src/classify/normfeat.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_normfeat.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/normfeat.cpp

$(TESSOBJ)classify_normmatch.$(OBJ) : $(TESSERACTDIR)/src/classify/normmatch.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_normmatch.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/normmatch.cpp

$(TESSOBJ)classify_ocrfeatures.$(OBJ) : $(TESSERACTDIR)/src/classify/ocrfeatures.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_ocrfeatures.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/ocrfeatures.cpp

$(TESSOBJ)classify_outfeat.$(OBJ) : $(TESSERACTDIR)/src/classify/outfeat.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_outfeat.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/outfeat.cpp

$(TESSOBJ)classify_picofeat.$(OBJ) : $(TESSERACTDIR)/src/classify/picofeat.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_picofeat.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/picofeat.cpp

$(TESSOBJ)classify_protos.$(OBJ) : $(TESSERACTDIR)/src/classify/protos.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_protos.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/protos.cpp

$(TESSOBJ)classify_shapeclassifier.$(OBJ) : $(TESSERACTDIR)/src/classify/shapeclassifier.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_shapeclassifier.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/shapeclassifier.cpp

$(TESSOBJ)classify_shapetable.$(OBJ) : $(TESSERACTDIR)/src/classify/shapetable.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_shapetable.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/shapetable.cpp

$(TESSOBJ)classify_tessclassifier.$(OBJ) : $(TESSERACTDIR)/src/classify/tessclassifier.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_tessclassifier.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/tessclassifier.cpp

$(TESSOBJ)classify_trainingsample.$(OBJ) : $(TESSERACTDIR)/src/classify/trainingsample.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)classify_trainingsample.$(OBJ) $(C_) $(TESSERACTDIR)/src/classify/trainingsample.cpp

$(TESSOBJ)cutil_cutil_class.$(OBJ) : $(TESSERACTDIR)/src/cutil/cutil_class.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)cutil_cutil_class.$(OBJ) $(C_) $(TESSERACTDIR)/src/cutil/cutil_class.cpp

$(TESSOBJ)cutil_emalloc.$(OBJ) : $(TESSERACTDIR)/src/cutil/emalloc.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)cutil_emalloc.$(OBJ) $(C_) $(TESSERACTDIR)/src/cutil/emalloc.cpp

$(TESSOBJ)cutil_oldlist.$(OBJ) : $(TESSERACTDIR)/src/cutil/oldlist.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)cutil_oldlist.$(OBJ) $(C_) $(TESSERACTDIR)/src/cutil/oldlist.cpp

$(TESSOBJ)dict_context.$(OBJ) : $(TESSERACTDIR)/src/dict/context.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_context.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/context.cpp

$(TESSOBJ)dict_dawg.$(OBJ) : $(TESSERACTDIR)/src/dict/dawg.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_dawg.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/dawg.cpp

$(TESSOBJ)dict_dawg_cache.$(OBJ) : $(TESSERACTDIR)/src/dict/dawg_cache.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_dawg_cache.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/dawg_cache.cpp

$(TESSOBJ)dict_dict.$(OBJ) : $(TESSERACTDIR)/src/dict/dict.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_dict.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/dict.cpp

$(TESSOBJ)dict_permdawg.$(OBJ) : $(TESSERACTDIR)/src/dict/permdawg.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_permdawg.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/permdawg.cpp

$(TESSOBJ)dict_stopper.$(OBJ) : $(TESSERACTDIR)/src/dict/stopper.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_stopper.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/stopper.cpp

$(TESSOBJ)dict_trie.$(OBJ) : $(TESSERACTDIR)/src/dict/trie.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_trie.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/trie.cpp

$(TESSOBJ)dict_hyphen.$(OBJ) : $(TESSERACTDIR)/src/dict/hyphen.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)dict_hyphen.$(OBJ) $(C_) $(TESSERACTDIR)/src/dict/hyphen.cpp

$(TESSOBJ)textord_alignedblob.$(OBJ) : $(TESSERACTDIR)/src/textord/alignedblob.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_alignedblob.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/alignedblob.cpp

$(TESSOBJ)textord_baselinedetect.$(OBJ) : $(TESSERACTDIR)/src/textord/baselinedetect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_baselinedetect.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/baselinedetect.cpp

$(TESSOBJ)textord_bbgrid.$(OBJ) : $(TESSERACTDIR)/src/textord/bbgrid.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_bbgrid.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/bbgrid.cpp

$(TESSOBJ)textord_blkocc.$(OBJ) : $(TESSERACTDIR)/src/textord/blkocc.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_blkocc.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/blkocc.cpp

$(TESSOBJ)textord_blobgrid.$(OBJ) : $(TESSERACTDIR)/src/textord/blobgrid.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_blobgrid.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/blobgrid.cpp

$(TESSOBJ)textord_ccnontextdetect.$(OBJ) : $(TESSERACTDIR)/src/textord/ccnontextdetect.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_ccnontextdetect.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/ccnontextdetect.cpp

$(TESSOBJ)textord_cjkpitch.$(OBJ) : $(TESSERACTDIR)/src/textord/cjkpitch.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_cjkpitch.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/cjkpitch.cpp

$(TESSOBJ)textord_colfind.$(OBJ) : $(TESSERACTDIR)/src/textord/colfind.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_colfind.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/colfind.cpp

$(TESSOBJ)textord_colpartition.$(OBJ) : $(TESSERACTDIR)/src/textord/colpartition.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_colpartition.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/colpartition.cpp

$(TESSOBJ)textord_colpartitionset.$(OBJ) : $(TESSERACTDIR)/src/textord/colpartitionset.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_colpartitionset.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/colpartitionset.cpp

$(TESSOBJ)textord_colpartitiongrid.$(OBJ) : $(TESSERACTDIR)/src/textord/colpartitiongrid.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_colpartitiongrid.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/colpartitiongrid.cpp

$(TESSOBJ)textord_devanagari_processing.$(OBJ) : $(TESSERACTDIR)/src/textord/devanagari_processing.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_devanagari_processing.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/devanagari_processing.cpp

$(TESSOBJ)textord_drawtord.$(OBJ) : $(TESSERACTDIR)/src/textord/drawtord.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_drawtord.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/drawtord.cpp

$(TESSOBJ)textord_edgblob.$(OBJ) : $(TESSERACTDIR)/src/textord/edgblob.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_edgblob.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/edgblob.cpp

$(TESSOBJ)textord_edgloop.$(OBJ) : $(TESSERACTDIR)/src/textord/edgloop.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_edgloop.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/edgloop.cpp

$(TESSOBJ)textord_fpchop.$(OBJ) : $(TESSERACTDIR)/src/textord/fpchop.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_fpchop.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/fpchop.cpp

$(TESSOBJ)textord_gap_map.$(OBJ) : $(TESSERACTDIR)/src/textord/gap_map.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_gap_map.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/gap_map.cpp

$(TESSOBJ)textord_imagefind.$(OBJ) : $(TESSERACTDIR)/src/textord/imagefind.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_imagefind.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/imagefind.cpp

$(TESSOBJ)textord_linefind.$(OBJ) : $(TESSERACTDIR)/src/textord/linefind.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_linefind.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/linefind.cpp

$(TESSOBJ)textord_makerow.$(OBJ) : $(TESSERACTDIR)/src/textord/makerow.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_makerow.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/makerow.cpp

$(TESSOBJ)textord_oldbasel.$(OBJ) : $(TESSERACTDIR)/src/textord/oldbasel.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_oldbasel.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/oldbasel.cpp

$(TESSOBJ)textord_pithsync.$(OBJ) : $(TESSERACTDIR)/src/textord/pithsync.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_pithsync.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/pithsync.cpp

$(TESSOBJ)textord_pitsync1.$(OBJ) : $(TESSERACTDIR)/src/textord/pitsync1.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_pitsync1.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/pitsync1.cpp

$(TESSOBJ)textord_scanedg.$(OBJ) : $(TESSERACTDIR)/src/textord/scanedg.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_scanedg.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/scanedg.cpp

$(TESSOBJ)textord_sortflts.$(OBJ) : $(TESSERACTDIR)/src/textord/sortflts.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_sortflts.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/sortflts.cpp

$(TESSOBJ)textord_strokewidth.$(OBJ) : $(TESSERACTDIR)/src/textord/strokewidth.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_strokewidth.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/strokewidth.cpp

$(TESSOBJ)textord_tabfind.$(OBJ) : $(TESSERACTDIR)/src/textord/tabfind.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tabfind.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tabfind.cpp

$(TESSOBJ)textord_tablefind.$(OBJ) : $(TESSERACTDIR)/src/textord/tablefind.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tablefind.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tablefind.cpp

$(TESSOBJ)textord_tabvector.$(OBJ) : $(TESSERACTDIR)/src/textord/tabvector.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tabvector.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tabvector.cpp

$(TESSOBJ)textord_tablerecog.$(OBJ) : $(TESSERACTDIR)/src/textord/tablerecog.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tablerecog.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tablerecog.cpp

$(TESSOBJ)textord_textlineprojection.$(OBJ) : $(TESSERACTDIR)/src/textord/textlineprojection.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_textlineprojection.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/textlineprojection.cpp

$(TESSOBJ)textord_textord.$(OBJ) : $(TESSERACTDIR)/src/textord/textord.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_textord.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/textord.cpp

$(TESSOBJ)textord_topitch.$(OBJ) : $(TESSERACTDIR)/src/textord/topitch.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_topitch.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/topitch.cpp

$(TESSOBJ)textord_tordmain.$(OBJ) : $(TESSERACTDIR)/src/textord/tordmain.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tordmain.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tordmain.cpp

$(TESSOBJ)textord_tospace.$(OBJ) : $(TESSERACTDIR)/src/textord/tospace.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tospace.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tospace.cpp

$(TESSOBJ)textord_tovars.$(OBJ) : $(TESSERACTDIR)/src/textord/tovars.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_tovars.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/tovars.cpp

$(TESSOBJ)textord_underlin.$(OBJ) : $(TESSERACTDIR)/src/textord/underlin.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_underlin.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/underlin.cpp

$(TESSOBJ)textord_wordseg.$(OBJ) : $(TESSERACTDIR)/src/textord/wordseg.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_wordseg.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/wordseg.cpp

$(TESSOBJ)textord_workingpartset.$(OBJ) : $(TESSERACTDIR)/src/textord/workingpartset.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_workingpartset.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/workingpartset.cpp

$(TESSOBJ)textord_equationdetectbase.$(OBJ) : $(TESSERACTDIR)/src/textord/equationdetectbase.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)textord_equationdetectbase.$(OBJ) $(C_) $(TESSERACTDIR)/src/textord/equationdetectbase.cpp

$(TESSOBJ)viewer_scrollview.$(OBJ) : $(TESSERACTDIR)/src/viewer/scrollview.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)viewer_scrollview.$(OBJ) $(C_) $(TESSERACTDIR)/src/viewer/scrollview.cpp

$(TESSOBJ)viewer_svmnode.$(OBJ) : $(TESSERACTDIR)/src/viewer/svmnode.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)viewer_svmnode.$(OBJ) $(C_) $(TESSERACTDIR)/src/viewer/svmnode.cpp

$(TESSOBJ)viewer_svutil.$(OBJ) : $(TESSERACTDIR)/src/viewer/svutil.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)viewer_svutil.$(OBJ) $(C_) $(TESSERACTDIR)/src/viewer/svutil.cpp

$(TESSOBJ)wordrec_chop.$(OBJ) : $(TESSERACTDIR)/src/wordrec/chop.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_chop.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/chop.cpp

$(TESSOBJ)wordrec_chopper.$(OBJ) : $(TESSERACTDIR)/src/wordrec/chopper.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_chopper.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/chopper.cpp

$(TESSOBJ)wordrec_findseam.$(OBJ) : $(TESSERACTDIR)/src/wordrec/findseam.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_findseam.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/findseam.cpp

$(TESSOBJ)wordrec_gradechop.$(OBJ) : $(TESSERACTDIR)/src/wordrec/gradechop.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_gradechop.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/gradechop.cpp

$(TESSOBJ)wordrec_tface.$(OBJ) : $(TESSERACTDIR)/src/wordrec/tface.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_tface.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/tface.cpp

$(TESSOBJ)wordrec_wordrec.$(OBJ) : $(TESSERACTDIR)/src/wordrec/wordrec.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_wordrec.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/wordrec.cpp

$(TESSOBJ)wordrec_associate.$(OBJ) : $(TESSERACTDIR)/src/wordrec/associate.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_associate.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/associate.cpp

$(TESSOBJ)wordrec_drawfx.$(OBJ) : $(TESSERACTDIR)/src/wordrec/drawfx.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_drawfx.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/drawfx.cpp

$(TESSOBJ)wordrec_language_model.$(OBJ) : $(TESSERACTDIR)/src/wordrec/language_model.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_language_model.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/language_model.cpp

$(TESSOBJ)wordrec_lm_consistency.$(OBJ) : $(TESSERACTDIR)/src/wordrec/lm_consistency.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_lm_consistency.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/lm_consistency.cpp

$(TESSOBJ)wordrec_lm_pain_points.$(OBJ) : $(TESSERACTDIR)/src/wordrec/lm_pain_points.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_lm_pain_points.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/lm_pain_points.cpp

$(TESSOBJ)wordrec_lm_state.$(OBJ) : $(TESSERACTDIR)/src/wordrec/lm_state.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_lm_state.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/lm_state.cpp

$(TESSOBJ)wordrec_outlines.$(OBJ) : $(TESSERACTDIR)/src/wordrec/outlines.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_outlines.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/outlines.cpp

$(TESSOBJ)wordrec_pieces.$(OBJ) : $(TESSERACTDIR)/src/wordrec/pieces.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_pieces.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/pieces.cpp

$(TESSOBJ)wordrec_params_model.$(OBJ) : $(TESSERACTDIR)/src/wordrec/params_model.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_params_model.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/params_model.cpp

$(TESSOBJ)wordrec_plotedges.$(OBJ) : $(TESSERACTDIR)/src/wordrec/plotedges.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_plotedges.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/plotedges.cpp

$(TESSOBJ)wordrec_render.$(OBJ) : $(TESSERACTDIR)/src/wordrec/render.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_render.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/render.cpp

$(TESSOBJ)wordrec_segsearch.$(OBJ) : $(TESSERACTDIR)/src/wordrec/segsearch.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_segsearch.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/segsearch.cpp

$(TESSOBJ)wordrec_wordclass.$(OBJ) : $(TESSERACTDIR)/src/wordrec/wordclass.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)wordrec_wordclass.$(OBJ) $(C_) $(TESSERACTDIR)/src/wordrec/wordclass.cpp

$(TESSOBJ)ccutil_ambigs.$(OBJ) : $(TESSERACTDIR)/src/ccutil/ambigs.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_ambigs.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/ambigs.cpp

$(TESSOBJ)ccutil_ccutil.$(OBJ) : $(TESSERACTDIR)/src/ccutil/ccutil.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_ccutil.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/ccutil.cpp

$(TESSOBJ)ccutil_clst.$(OBJ) : $(TESSERACTDIR)/src/ccutil/clst.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_clst.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/clst.cpp

$(TESSOBJ)ccutil_elst2.$(OBJ) : $(TESSERACTDIR)/src/ccutil/elst2.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_elst2.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/elst2.cpp

$(TESSOBJ)ccutil_elst.$(OBJ) : $(TESSERACTDIR)/src/ccutil/elst.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_elst.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/elst.cpp

$(TESSOBJ)ccutil_errcode.$(OBJ) : $(TESSERACTDIR)/src/ccutil/errcode.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_errcode.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/errcode.cpp

$(TESSOBJ)ccutil_serialis.$(OBJ) : $(TESSERACTDIR)/src/ccutil/serialis.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_serialis.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/serialis.cpp

$(TESSOBJ)ccutil_scanutils.$(OBJ) : $(TESSERACTDIR)/src/ccutil/scanutils.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_scanutils.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/scanutils.cpp

$(TESSOBJ)ccutil_tessdatamanager.$(OBJ) : $(TESSERACTDIR)/src/ccutil/tessdatamanager.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_tessdatamanager.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/tessdatamanager.cpp

$(TESSOBJ)ccutil_tprintf.$(OBJ) : $(TESSERACTDIR)/src/ccutil/tprintf.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_tprintf.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/tprintf.cpp

$(TESSOBJ)ccutil_unichar.$(OBJ) : $(TESSERACTDIR)/src/ccutil/unichar.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_unichar.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/unichar.cpp

$(TESSOBJ)ccutil_unicharcompress.$(OBJ) : $(TESSERACTDIR)/src/ccutil/unicharcompress.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_unicharcompress.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/unicharcompress.cpp

$(TESSOBJ)ccutil_unicharmap.$(OBJ) : $(TESSERACTDIR)/src/ccutil/unicharmap.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_unicharmap.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/unicharmap.cpp

$(TESSOBJ)ccutil_unicharset.$(OBJ) : $(TESSERACTDIR)/src/ccutil/unicharset.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_unicharset.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/unicharset.cpp

$(TESSOBJ)ccutil_params.$(OBJ) : $(TESSERACTDIR)/src/ccutil/params.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_params.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/params.cpp

$(TESSOBJ)ccutil_bitvector.$(OBJ) : $(TESSERACTDIR)/src/ccutil/bitvector.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_bitvector.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/bitvector.cpp

$(TESSOBJ)ccutil_indexmapbidi.$(OBJ) : $(TESSERACTDIR)/src/ccutil/indexmapbidi.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)ccutil_indexmapbidi.$(OBJ) $(C_) $(TESSERACTDIR)/src/ccutil/indexmapbidi.cpp

$(TESSOBJ)lstm_convolve.$(OBJ) : $(TESSERACTDIR)/src/lstm/convolve.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_convolve.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/convolve.cpp

$(TESSOBJ)lstm_fullyconnected.$(OBJ) : $(TESSERACTDIR)/src/lstm/fullyconnected.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_fullyconnected.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/fullyconnected.cpp

$(TESSOBJ)lstm_functions.$(OBJ) : $(TESSERACTDIR)/src/lstm/functions.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_functions.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/functions.cpp

$(TESSOBJ)lstm_input.$(OBJ) : $(TESSERACTDIR)/src/lstm/input.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_input.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/input.cpp

$(TESSOBJ)lstm_lstm.$(OBJ) : $(TESSERACTDIR)/src/lstm/lstm.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_lstm.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/lstm.cpp

$(TESSOBJ)lstm_lstmrecognizer.$(OBJ) : $(TESSERACTDIR)/src/lstm/lstmrecognizer.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_lstmrecognizer.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/lstmrecognizer.cpp

$(TESSOBJ)lstm_maxpool.$(OBJ) : $(TESSERACTDIR)/src/lstm/maxpool.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_maxpool.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/maxpool.cpp

$(TESSOBJ)lstm_network.$(OBJ) : $(TESSERACTDIR)/src/lstm/network.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_network.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/network.cpp

$(TESSOBJ)lstm_networkio.$(OBJ) : $(TESSERACTDIR)/src/lstm/networkio.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_networkio.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/networkio.cpp

$(TESSOBJ)lstm_parallel.$(OBJ) : $(TESSERACTDIR)/src/lstm/parallel.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_parallel.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/parallel.cpp

$(TESSOBJ)lstm_plumbing.$(OBJ) : $(TESSERACTDIR)/src/lstm/plumbing.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_plumbing.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/plumbing.cpp

$(TESSOBJ)lstm_recodebeam.$(OBJ) : $(TESSERACTDIR)/src/lstm/recodebeam.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_recodebeam.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/recodebeam.cpp

$(TESSOBJ)lstm_reconfig.$(OBJ) : $(TESSERACTDIR)/src/lstm/reconfig.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_reconfig.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/reconfig.cpp

$(TESSOBJ)lstm_reversed.$(OBJ) : $(TESSERACTDIR)/src/lstm/reversed.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_reversed.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/reversed.cpp

$(TESSOBJ)lstm_series.$(OBJ) : $(TESSERACTDIR)/src/lstm/series.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_series.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/series.cpp

$(TESSOBJ)lstm_stridemap.$(OBJ) : $(TESSERACTDIR)/src/lstm/stridemap.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_stridemap.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/stridemap.cpp

$(TESSOBJ)lstm_tfnetwork.$(OBJ) : $(TESSERACTDIR)/src/lstm/tfnetwork.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_tfnetwork.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/tfnetwork.cpp

$(TESSOBJ)lstm_weightmatrix.$(OBJ) : $(TESSERACTDIR)/src/lstm/weightmatrix.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)lstm_weightmatrix.$(OBJ) $(C_) $(TESSERACTDIR)/src/lstm/weightmatrix.cpp

$(TESSOBJ)arch_dotproduct.$(OBJ) : $(TESSERACTDIR)/src/arch/dotproduct.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSO_)arch_dotproduct.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/dotproduct.cpp

$(TESSOBJ)arch_dotproductavx.$(OBJ): $(TESSERACTDIR)/src/arch/dotproductavx.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSAVX) $(TESSO_)arch_dotproductavx.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/dotproductavx.cpp

$(TESSOBJ)arch_dotproductfma.$(OBJ): $(TESSERACTDIR)/src/arch/dotproductfma.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSFMA) $(TESSO_)arch_dotproductfma.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/dotproductfma.cpp

$(TESSOBJ)arch_dotproductsse.$(OBJ): $(TESSERACTDIR)/src/arch/dotproductsse.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSSSE41) $(TESSO_)arch_dotproductsse.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/dotproductsse.cpp

$(TESSOBJ)arch_dotproductneon.$(OBJ): $(TESSERACTDIR)/src/arch/dotproductneon.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSSNEON) $(TESSO_)arch_dotproductneon.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/dotproductneon.cpp

$(TESSOBJ)arch_intsimdmatrixavx2.$(OBJ): $(TESSERACTDIR)/src/arch/intsimdmatrixavx2.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSAVX2) $(TESSO_)arch_intsimdmatrixavx2.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/intsimdmatrixavx2.cpp

$(TESSOBJ)arch_intsimdmatrixsse.$(OBJ): $(TESSERACTDIR)/src/arch/intsimdmatrixsse.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSSSE41) $(TESSO_)arch_intsimdmatrixsse.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/intsimdmatrixsse.cpp

$(TESSOBJ)arch_intsimdmatrixneon.$(OBJ): $(TESSERACTDIR)/src/arch/intsimdmatrixneon.cpp $(TESSDEPS)
	$(TESSCXX) $(TESSNEON) $(TESSO_)arch_intsimdmatrixneon.$(OBJ) $(C_) $(TESSERACTDIR)/src/arch/intsimdmatrixneon.cpp

# Targets needed for lstm engine
TESSERACT_OBJS_1=\
	$(TESSOBJ)api_altorenderer.$(OBJ)\
	$(TESSOBJ)api_baseapi.$(OBJ)\
	$(TESSOBJ)api_capi.$(OBJ)\
	$(TESSOBJ)api_hocrrenderer.$(OBJ)\
	$(TESSOBJ)api_lstmboxrenderer.$(OBJ)\
	$(TESSOBJ)api_pdfrenderer.$(OBJ)\
	$(TESSOBJ)api_renderer.$(OBJ)\
	$(TESSOBJ)api_wordstrboxrenderer.$(OBJ)\
	$(TESSOBJ)arch_intsimdmatrix.$(OBJ)\
	$(TESSOBJ)arch_simddetect.$(OBJ)\
	$(TESSOBJ)ccmain_applybox.$(OBJ)\
	$(TESSOBJ)ccmain_control.$(OBJ)\
	$(TESSOBJ)ccmain_linerec.$(OBJ)\
	$(TESSOBJ)ccmain_ltrresultiterator.$(OBJ)\
	$(TESSOBJ)ccmain_mutableiterator.$(OBJ)\
	$(TESSOBJ)ccmain_output.$(OBJ)\
	$(TESSOBJ)ccmain_pageiterator.$(OBJ)\
	$(TESSOBJ)ccmain_pagesegmain.$(OBJ)\
	$(TESSOBJ)ccmain_pagewalk.$(OBJ)\
	$(TESSOBJ)ccmain_paragraphs.$(OBJ)\
	$(TESSOBJ)ccmain_paramsd.$(OBJ)\
	$(TESSOBJ)ccmain_pgedit.$(OBJ)\
	$(TESSOBJ)ccmain_reject.$(OBJ)\
	$(TESSOBJ)ccmain_resultiterator.$(OBJ)\
	$(TESSOBJ)ccmain_tessedit.$(OBJ)\
	$(TESSOBJ)ccmain_tesseractclass.$(OBJ)\
	$(TESSOBJ)ccmain_tessvars.$(OBJ)\
	$(TESSOBJ)ccmain_thresholder.$(OBJ)\
	$(TESSOBJ)ccmain_werdit.$(OBJ)\


TESSERACT_OBJS_2 = \
	$(TESSOBJ)ccstruct_blamer.$(OBJ)\
	$(TESSOBJ)ccstruct_blobbox.$(OBJ)\
	$(TESSOBJ)ccstruct_blobs.$(OBJ)\
	$(TESSOBJ)ccstruct_blread.$(OBJ)\
	$(TESSOBJ)ccstruct_boxread.$(OBJ)\
	$(TESSOBJ)ccstruct_boxword.$(OBJ)\
	$(TESSOBJ)ccstruct_ccstruct.$(OBJ)\
	$(TESSOBJ)ccstruct_coutln.$(OBJ)\
	$(TESSOBJ)ccstruct_detlinefit.$(OBJ)\
	$(TESSOBJ)ccstruct_dppoint.$(OBJ)\
	$(TESSOBJ)ccstruct_image.$(OBJ)\
	$(TESSOBJ)ccstruct_imagedata.$(OBJ)\
	$(TESSOBJ)ccstruct_linlsq.$(OBJ)\
	$(TESSOBJ)ccstruct_matrix.$(OBJ)\
	$(TESSOBJ)ccstruct_mod128.$(OBJ)\
	$(TESSOBJ)ccstruct_normalis.$(OBJ)\
	$(TESSOBJ)ccstruct_ocrblock.$(OBJ)\
	$(TESSOBJ)ccstruct_ocrpara.$(OBJ)\
	$(TESSOBJ)ccstruct_ocrrow.$(OBJ)\
	$(TESSOBJ)ccstruct_otsuthr.$(OBJ)\
	$(TESSOBJ)ccstruct_pageres.$(OBJ)\
	$(TESSOBJ)ccstruct_pdblock.$(OBJ)\
	$(TESSOBJ)ccstruct_points.$(OBJ)\
	$(TESSOBJ)ccstruct_polyaprx.$(OBJ)\
	$(TESSOBJ)ccstruct_polyblk.$(OBJ)\
	$(TESSOBJ)ccstruct_quadlsq.$(OBJ)\
	$(TESSOBJ)ccstruct_quspline.$(OBJ)\
	$(TESSOBJ)ccstruct_ratngs.$(OBJ)\
	$(TESSOBJ)ccstruct_rect.$(OBJ)\
	$(TESSOBJ)ccstruct_rejctmap.$(OBJ)\
	$(TESSOBJ)ccstruct_seam.$(OBJ)\
	$(TESSOBJ)ccstruct_split.$(OBJ)\
	$(TESSOBJ)ccstruct_statistc.$(OBJ)\
	$(TESSOBJ)ccstruct_stepblob.$(OBJ)\
	$(TESSOBJ)ccstruct_werd.$(OBJ)

TESSERACT_OBJS_3=\
	$(TESSOBJ)classify_classify.$(OBJ)\
	$(TESSOBJ)dict_context.$(OBJ)\
	$(TESSOBJ)dict_dawg.$(OBJ)\
	$(TESSOBJ)dict_dawg_cache.$(OBJ)\
	$(TESSOBJ)dict_dict.$(OBJ)\
	$(TESSOBJ)dict_permdawg.$(OBJ)\
	$(TESSOBJ)dict_stopper.$(OBJ)\
	$(TESSOBJ)dict_trie.$(OBJ)\
	$(TESSOBJ)textord_alignedblob.$(OBJ)\
	$(TESSOBJ)textord_baselinedetect.$(OBJ)\
	$(TESSOBJ)textord_bbgrid.$(OBJ)\
	$(TESSOBJ)textord_blkocc.$(OBJ)\
	$(TESSOBJ)textord_blobgrid.$(OBJ)\
	$(TESSOBJ)textord_ccnontextdetect.$(OBJ)\
	$(TESSOBJ)textord_cjkpitch.$(OBJ)\
	$(TESSOBJ)textord_colfind.$(OBJ)\
	$(TESSOBJ)textord_colpartition.$(OBJ)\
	$(TESSOBJ)textord_colpartitionset.$(OBJ)\
	$(TESSOBJ)textord_colpartitiongrid.$(OBJ)\
	$(TESSOBJ)textord_devanagari_processing.$(OBJ)\
	$(TESSOBJ)textord_drawtord.$(OBJ)\
	$(TESSOBJ)textord_edgblob.$(OBJ)\
	$(TESSOBJ)textord_edgloop.$(OBJ)\
	$(TESSOBJ)textord_fpchop.$(OBJ)\
	$(TESSOBJ)textord_gap_map.$(OBJ)\
	$(TESSOBJ)textord_imagefind.$(OBJ)\
	$(TESSOBJ)textord_linefind.$(OBJ)\
	$(TESSOBJ)textord_makerow.$(OBJ)\
	$(TESSOBJ)textord_oldbasel.$(OBJ)\
	$(TESSOBJ)textord_pithsync.$(OBJ)\
	$(TESSOBJ)textord_pitsync1.$(OBJ)\
	$(TESSOBJ)textord_scanedg.$(OBJ)\
	$(TESSOBJ)textord_sortflts.$(OBJ)\
	$(TESSOBJ)textord_strokewidth.$(OBJ)\
	$(TESSOBJ)textord_tabfind.$(OBJ)\
	$(TESSOBJ)textord_tablefind.$(OBJ)\
	$(TESSOBJ)textord_tablerecog.$(OBJ)\
	$(TESSOBJ)textord_tabvector.$(OBJ)\
	$(TESSOBJ)textord_textlineprojection.$(OBJ)\
	$(TESSOBJ)textord_textord.$(OBJ)\
	$(TESSOBJ)textord_topitch.$(OBJ)\
	$(TESSOBJ)textord_tordmain.$(OBJ)\
	$(TESSOBJ)textord_tospace.$(OBJ)\
	$(TESSOBJ)textord_tovars.$(OBJ)\
	$(TESSOBJ)textord_underlin.$(OBJ)\
	$(TESSOBJ)textord_wordseg.$(OBJ)\
	$(TESSOBJ)textord_workingpartset.$(OBJ)\

TESSERACT_OBJS_4=\
	$(TESSOBJ)viewer_scrollview.$(OBJ)\
	$(TESSOBJ)viewer_svmnode.$(OBJ)\
	$(TESSOBJ)viewer_svutil.$(OBJ)\
	$(TESSOBJ)wordrec_tface.$(OBJ)\
	$(TESSOBJ)wordrec_wordrec.$(OBJ)\
	$(TESSOBJ)ccutil_ccutil.$(OBJ)\
	$(TESSOBJ)ccutil_clst.$(OBJ)\
	$(TESSOBJ)ccutil_elst.$(OBJ)\
	$(TESSOBJ)ccutil_elst2.$(OBJ)\
	$(TESSOBJ)ccutil_errcode.$(OBJ)\
	$(TESSOBJ)ccutil_params.$(OBJ)\
	$(TESSOBJ)ccutil_scanutils.$(OBJ)\
	$(TESSOBJ)ccutil_serialis.$(OBJ)\
	$(TESSOBJ)ccutil_tessdatamanager.$(OBJ)\
	$(TESSOBJ)ccutil_tprintf.$(OBJ)\
	$(TESSOBJ)ccutil_unichar.$(OBJ)\
	$(TESSOBJ)ccutil_unicharcompress.$(OBJ)\
	$(TESSOBJ)ccutil_unicharmap.$(OBJ)\
	$(TESSOBJ)ccutil_unicharset.$(OBJ)\
	$(TESSOBJ)lstm_convolve.$(OBJ)\
	$(TESSOBJ)lstm_fullyconnected.$(OBJ)\
	$(TESSOBJ)lstm_functions.$(OBJ)\
	$(TESSOBJ)lstm_input.$(OBJ)\
	$(TESSOBJ)lstm_lstm.$(OBJ)\
	$(TESSOBJ)lstm_lstmrecognizer.$(OBJ)\
	$(TESSOBJ)lstm_maxpool.$(OBJ)\
	$(TESSOBJ)lstm_network.$(OBJ)\
	$(TESSOBJ)lstm_networkio.$(OBJ)\
	$(TESSOBJ)lstm_parallel.$(OBJ)\
	$(TESSOBJ)lstm_plumbing.$(OBJ)\
	$(TESSOBJ)lstm_recodebeam.$(OBJ)\
	$(TESSOBJ)lstm_reconfig.$(OBJ)\
	$(TESSOBJ)lstm_reversed.$(OBJ)\
	$(TESSOBJ)lstm_series.$(OBJ)\
	$(TESSOBJ)lstm_stridemap.$(OBJ)\
	$(TESSOBJ)lstm_tfnetwork.$(OBJ)\
	$(TESSOBJ)lstm_weightmatrix.$(OBJ)\
	$(TESSOBJ)arch_dotproduct.$(OBJ)\
	$(TESSOBJ)arch_dotproductavx.$(OBJ)\
	$(TESSOBJ)arch_intsimdmatrixavx2.$(OBJ)\
	$(TESSOBJ)arch_dotproductfma.$(OBJ)\
	$(TESSOBJ)arch_dotproductsse.$(OBJ)\
	$(TESSOBJ)arch_dotproductneon.$(OBJ)\
	$(TESSOBJ)arch_intsimdmatrixsse.$(OBJ)\
	$(TESSOBJ)arch_intsimdmatrixneon.$(OBJ)

# Targets needed for TESSERACT_LEGACY
TESSERACT_LEGACY_OBJS=\
	$(TESSOBJ)ccmain_adaptions.$(OBJ)\
	$(TESSOBJ)ccmain_docqual.$(OBJ)\
	$(TESSOBJ)ccmain_equationdetect.$(OBJ)\
	$(TESSOBJ)ccmain_fixspace.$(OBJ)\
	$(TESSOBJ)ccmain_fixxht.$(OBJ)\
	$(TESSOBJ)ccmain_osdetect.$(OBJ)\
	$(TESSOBJ)ccmain_par_control.$(OBJ)\
	$(TESSOBJ)ccmain_recogtraining.$(OBJ)\
	$(TESSOBJ)ccmain_superscript.$(OBJ)\
	$(TESSOBJ)ccmain_tessbox.$(OBJ)\
	$(TESSOBJ)ccmain_tfacepp.$(OBJ)\
	$(TESSOBJ)ccstruct_fontinfo.$(OBJ)\
	$(TESSOBJ)ccstruct_params_training_featdef.$(OBJ)\
	$(TESSOBJ)ccutil_ambigs.$(OBJ)\
	$(TESSOBJ)ccutil_bitvector.$(OBJ)\
	$(TESSOBJ)ccutil_indexmapbidi.$(OBJ)\
	$(TESSOBJ)classify_adaptive.$(OBJ)\
	$(TESSOBJ)classify_adaptmatch.$(OBJ)\
	$(TESSOBJ)classify_blobclass.$(OBJ)\
	$(TESSOBJ)classify_cluster.$(OBJ)\
	$(TESSOBJ)classify_clusttool.$(OBJ)\
	$(TESSOBJ)classify_cutoffs.$(OBJ)\
	$(TESSOBJ)classify_featdefs.$(OBJ)\
	$(TESSOBJ)classify_float2int.$(OBJ)\
	$(TESSOBJ)classify_fpoint.$(OBJ)\
	$(TESSOBJ)classify_intfeaturespace.$(OBJ)\
	$(TESSOBJ)classify_intfx.$(OBJ)\
	$(TESSOBJ)classify_intmatcher.$(OBJ)\
	$(TESSOBJ)classify_intproto.$(OBJ)\
	$(TESSOBJ)classify_kdtree.$(OBJ)\
	$(TESSOBJ)classify_mf.$(OBJ)\
	$(TESSOBJ)classify_mfoutline.$(OBJ)\
	$(TESSOBJ)classify_mfx.$(OBJ)\
	$(TESSOBJ)classify_normfeat.$(OBJ)\
	$(TESSOBJ)classify_normmatch.$(OBJ)\
	$(TESSOBJ)classify_ocrfeatures.$(OBJ)\
	$(TESSOBJ)classify_outfeat.$(OBJ)\
	$(TESSOBJ)classify_picofeat.$(OBJ)\
	$(TESSOBJ)classify_protos.$(OBJ)\
	$(TESSOBJ)classify_shapeclassifier.$(OBJ)\
	$(TESSOBJ)classify_shapetable.$(OBJ)\
	$(TESSOBJ)classify_tessclassifier.$(OBJ)\
	$(TESSOBJ)classify_trainingsample.$(OBJ)\
	$(TESSOBJ)cutil_oldlist.$(OBJ)\
	$(TESSOBJ)dict_hyphen.$(OBJ)\
	$(TESSOBJ)textord_equationdetectbase.$(OBJ)\
	$(TESSOBJ)wordrec_associate.$(OBJ)\
	$(TESSOBJ)wordrec_chop.$(OBJ)\
	$(TESSOBJ)wordrec_chopper.$(OBJ)\
	$(TESSOBJ)wordrec_drawfx.$(OBJ)\
	$(TESSOBJ)wordrec_findseam.$(OBJ)\
	$(TESSOBJ)wordrec_gradechop.$(OBJ)\
	$(TESSOBJ)wordrec_language_model.$(OBJ)\
	$(TESSOBJ)wordrec_lm_consistency.$(OBJ)\
	$(TESSOBJ)wordrec_lm_pain_points.$(OBJ)\
	$(TESSOBJ)wordrec_lm_state.$(OBJ)\
	$(TESSOBJ)wordrec_outlines.$(OBJ)\
	$(TESSOBJ)wordrec_params_model.$(OBJ)\
	$(TESSOBJ)wordrec_pieces.$(OBJ)\
	$(TESSOBJ)wordrec_plotedges.$(OBJ)\
	$(TESSOBJ)wordrec_render.$(OBJ)\
	$(TESSOBJ)wordrec_segsearch.$(OBJ)\
	$(TESSOBJ)wordrec_wordclass.$(OBJ)


#TESSERACT_LEGACY=$(TESSERACT_LEGACY_OBJS)
TESSERACT_LEGACY=

TESS_ROMFS_ARGS=\
	-c -P $(GLSRCDIR)$(D)..$(D) tessdata$(D)*
