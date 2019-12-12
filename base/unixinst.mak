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
# Partial makefile common to all Unix and Desqview/X configurations,
# containing the `install' targets.
# This is the very last part of the makefile for these configurations.

UNIXINST_MAK=$(GLSRC)unixinst.mak $(TOP_MAKEFILES)

install: install-gs install-$(PCL) install-$(XPS)

# The sh -c in the rules below is required because Ultrix's implementation
# of sh -e terminates execution of a command if any error occurs, even if
# the command traps the error with ||.

# We include mkdirs for datadir, gsdir, and gsdatadir in all 3 install
# rules, just in case bindir or scriptdir is a subdirectory of any of these.

install-exec-bindir:
	-mkdir -p $(DESTDIR)$(bindir)

install-exec: $(GS_XE) install-exec-bindir $(UNIXINST_MAK) $(MAKEDIRS)
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)
	$(INSTALL_PROGRAM) $(GS_XE) $(DESTDIR)$(bindir)/$(GS)$(XE)

install-gs: install-exec install-scripts install-data $(INSTALL_SHARED) $(INSTALL_CONTRIB)
	$(NO_OP)

install-gpcl6: $(GPCL_XE) install-exec-bindir $(UNIXINST_MAK) $(MAKEDIRS)
	$(INSTALL_PROGRAM) $(GPCL_XE) $(DESTDIR)$(bindir)/$(PCL)$(XE)

install-gxps: $(GXPS_XE) install-exec-bindir $(UNIXINST_MAK) $(MAKEDIRS)
	$(INSTALL_PROGRAM) $(GXPS_XE) $(DESTDIR)$(bindir)/$(XPS)$(XE)

# dummy install taget if we don't have pcl and xps available
install-:
	$(NO_OP)

install-no_gpcl6:
	$(NO_OP)

install-no_gxps:
	$(NO_OP)

install-scripts: $(PSLIBDIR)/gsnd $(UNIXINST_MAK) $(MAKEDIRS)
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)
	-mkdir -p $(DESTDIR)$(scriptdir)
	$(SH) -c 'for f in \
gsbj gsdj gsdj500 gslj gslp gsnd \
bdftops dvipdf eps2eps \
pdf2dsc pdf2ps pf2afm pfbtopfa pphs printafm \
ps2ascii ps2epsi ps2pdf ps2pdf12 ps2pdf13 ps2pdf14 ps2pdfwr ps2ps ps2ps2 \
fixmswrd.pl lprsetup.sh pj-gs.sh pv.sh sysvlp.sh unix-lpr.sh ;\
	do if ( test -f $(PSLIBDIR)/$$f ); then \
	  (cat $(PSLIBDIR)/$$f | sed -e "s/GS_EXECUTABLE=gs/GS_EXECUTABLE=$(GS)/" > $(PSOBJDIR)/$$f); \
	  $(INSTALL_PROGRAM) $(PSOBJDIR)/$$f $(DESTDIR)$(scriptdir)/$$f; \
	fi;\
	done'

PSRESDIR=$(PSLIBDIR)/../Resource
ICCRESDIR=$(PSLIBDIR)/../iccprofiles
PSDOCDIR=$(PSLIBDIR)/../doc
PSEXDIR=$(PSLIBDIR)/../examples
PSMANDIR=$(PSLIBDIR)/../man

install-data: install-libdata install-resdata$(COMPILE_INITS) install-iccdata$(COMPILE_INITS) install-doc install-man

# There's no point in providing a complete dependency list: we include
# one file from each subdirectory just as a sanity check.

install-libdata: 
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)
	-mkdir -p $(DESTDIR)$(gsdatadir)/lib
	$(SH) -c 'for f in \
$(EXTRA_INIT_FILES) Fontmap.GS \
ht_ccsto.ps \
acctest.ps align.ps bdftops.ps \
caption.ps cid2code.ps docie.ps \
errpage.ps font2pcl.ps gslp.ps gsnup.ps image-qa.ps \
jispaper.ps landscap.ps lines.ps \
mkcidfm.ps PDFA_def.ps PDFX_def.ps pdf_info.ps \
pf2afm.ps pfbtopfa.ps ppath.ps \
pphs.ps \
prfont.ps printafm.ps \
ps2ai.ps ps2ascii.ps ps2epsi.ps rollconv.ps \
stcinfo.ps stcolor.ps stocht.ps \
traceimg.ps traceop.ps uninfo.ps \
viewcmyk.ps viewgif.ps viewjpeg.ps viewmiff.ps \
viewpcx.ps viewpbm.ps viewps2a.ps \
winmaps.ps zeroline.ps \
pdf2dsc.ps ;\
	do if ( test -f $(PSLIBDIR)/$$f ); then $(INSTALL_DATA) $(PSLIBDIR)/$$f $(DESTDIR)$(gsdatadir)/lib; fi;\
	done'
	$(SH) -c 'for f in $(PSLIBDIR)/gs_*.ps $(PSLIBDIR)/pdf*.ps;\
	do $(INSTALL_DATA) $$f $(DESTDIR)$(gsdatadir)/lib ;\
	done'
	$(SH) -c 'for f in $(PSLIBDIR)/*.ppd $(PSLIBDIR)/*.rpd $(PSLIBDIR)/*.upp $(PSLIBDIR)/*.xbm $(PSLIBDIR)/*.xpm;\
	do $(INSTALL_DATA) $$f $(DESTDIR)$(gsdatadir)/lib ;\
	done'

# install the default resource files
# copy in every category (directory) but CVS
RES_CATEGORIES=`ls $(PSRESDIR) | grep -v CVS` 
install-resdata0 : $(PSRESDIR)/Decoding/Unicode
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)/Resource
	$(SH) -c 'for dir in $(RES_CATEGORIES); do \
	  rdir=$(DESTDIR)$(gsdatadir)/Resource/$$dir ; \
	  test -d $$rdir || mkdir -p $$rdir ; \
	  for file in $(PSRESDIR)/$$dir/*; do \
	    if test -f $$file; then $(INSTALL_DATA) $$file $$rdir ; fi \
	  done \
	done'

# install default iccprofiles
install-iccdata0 : $(ICCRESDIR)
	-mkdir -p $(DESTDIR)$(datadir)
	-mkdir -p $(DESTDIR)$(gsdir)
	-mkdir -p $(DESTDIR)$(gsdatadir)/iccprofiles
	$(SH) -c 'for file in $(ICCRESDIR)/*; do \
	    if test -f $$file; then $(INSTALL_DATA) $$file $(DESTDIR)$(gsdatadir)/iccprofiles ; fi \
	done'

#COMPILE_INITS=1 don't need Resources, nor ICC

install-resdata1 :

install-iccdata1 :

# install html documentation
DOC_PAGES=index.html API.htm C-style.htm Develop.htm DLL.htm Fonts.htm Install.htm Lib.htm \
          News.htm Psfiles.htm Readme.htm sample_downscale_device.htm Source.htm \
          thirdparty.htm Use.htm WhatIsGS.htm Commprod.htm Deprecated.htm \
          Devices.htm Drivers.htm History9.htm Language.htm Make.htm Ps2epsi.htm \
          Ps-style.htm Release.htm SavedPages.htm subclass.htm Unix-lpr.htm \
          VectorDevices.htm gs-style.css index.js pscet_status.txt style.css \
          gdevds32.c COPYING \
          GS9_Color_Management.pdf

DOC_PAGE_IMAGES=Artifex_logo.png  favicon.png  ghostscript_logo.png  hamburger-light.png  x-light.png

install-doc: $(PSDOCDIR)/News.htm
	-mkdir -p $(DESTDIR)$(docdir)
	-mkdir -p $(DESTDIR)$(docdir)/images
	$(SH) -c 'for f in $(DOC_PAGES) ;\
	do if ( test -f $(PSDOCDIR)/$$f ); then $(INSTALL_DATA) $(PSDOCDIR)/$$f $(DESTDIR)$(docdir); fi;\
	done'
	$(SH) -c 'for f in $(DOC_PAGE_IMAGES) ;\
	do if ( test -f $(PSDOCDIR)/images/$$f ); then $(INSTALL_DATA) $(PSDOCDIR)/images/$$f $(DESTDIR)$(docdir)/images; fi;\
	done'

# install the man pages for each locale
MAN_LCDIRS=. de
MAN1_LINKS_PS2PS=eps2eps
MAN1_LINKS_PS2PDF=ps2pdf12 ps2pdf13 ps2pdf14
MAN1_LINKS_GSLP=gsbj gsdj gsdj500 gslj
install-man: $(PSMANDIR)/gs.1
	$(SH) -c 'test -d $(DESTDIR)$(mandir) || mkdir -p $(DESTDIR)$(mandir)'
	$(SH) -c 'for d in $(MAN_LCDIRS) ;\
	do man1dir=$(DESTDIR)$(mandir)/$$d/man$(man1ext) ;\
	  ( test -d $$man1dir || mkdir -p $$man1dir ) ;\
	  for f in $(PSMANDIR)/$$d/*.1 ;\
	    do $(INSTALL_DATA) $$f $$man1dir ;\
	    if ( test -f $$man1dir/ps2ps.$(man1ext) ) ;\
	      then for f in $(MAN1_LINKS_PS2PS) ;\
	        do ( cd $$man1dir; rm -f $$f.$(man1ext) ;\
			  ln -s ps2ps.$(man1ext) $$f.$(man1ext) ) ;\
	      done ;\
	    fi ;\
	    if ( test -f $$man1dir/ps2pdf.$(man1ext) ) ;\
	      then for f in $(MAN1_LINKS_PS2PDF) ;\
	        do ( cd $$man1dir; rm -f $$f.$(man1ext) ;\
			  ln -s ps2pdf.$(man1ext) $$f.$(man1ext) ) ;\
	      done ;\
	    fi ;\
            if ( test -f $$man1dir/gslp.$(man1ext) ) ;\
	      then for f in $(MAN1_LINKS_GSLP) ;\
	        do ( cd $$man1dir; rm -f $$f.$(man1ext) ;\
			  ln -s gslp.$(man1ext) $$f.$(man1ext) ) ;\
	      done ;\
	    fi ;\
	  done ;\
	done'

# install the example files
install-examples:
	-mkdir -p $(DESTDIR)$(exdir)
	for f in \
        alphabet.ps colorcir.ps escher.ps grayalph.ps snowflak.ps \
        text_graph_image_cmyk_rgb.pdf transparency_example.ps waterfal.ps \
        annots.pdf doretree.ps golfer.eps ridt91.eps text_graphic_image.pdf \
        tiger.eps vasarely.ps;\
	do $(INSTALL_DATA) $(PSEXDIR)/$$f $(DESTDIR)$(exdir) ;\
	done
	-mkdir -p $(DESTDIR)$(exdir)/cjk
	for f in \
all_ac1.ps all_aj1.ps all_ak1.ps gscjk_ac.ps gscjk_aj.ps iso2022.ps \
all_ag1.ps all_aj2.ps article9.ps gscjk_ag.ps gscjk_ak.ps iso2022v.ps ;\
	do $(INSTALL_DATA) $(PSEXDIR)/cjk/$$f $(DESTDIR)$(exdir)/cjk ;\
	done

install-shared: $(GS_SHARED_OBJS)
	-mkdir -p $(DESTDIR)$(gssharedir)
	$(SH) -c 'for obj in $(GS_SHARED_OBJS); do \
	    $(INSTALL_PROGRAM) $$obj $(DESTDIR)$(gssharedir)/; done'
