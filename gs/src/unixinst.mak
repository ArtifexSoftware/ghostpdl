#    Copyright (C) 1997, 1998 Aladdin Enterprises.  All rights reserved.
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


# Partial makefile common to all Unix and Desqview/X configurations,
# containing the `install' targets.
# This is the very last part of the makefile for these configurations.

install: install-exec install-scripts install-data

# The sh -c in the rules below is required because Ultrix's implementation
# of sh -e terminates execution of a command if any error occurs, even if
# the command traps the error with ||.

# We include mkdirs for datadir, gsdir, and gsdatadir in all 3 install
# rules, just in case bindir or scriptdir is a subdirectory of any of these.

install-exec: $(GS_XE)
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	-mkdir $(bindir)
	$(INSTALL_PROGRAM) $(GS_XE) $(bindir)/$(GS)

install-scripts: gsnd
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	-mkdir $(scriptdir)
	sh -c 'for f in \
gsbj gsdj gsdj500 gslj gslp gsnd \
bdftops dvipdf font2c \
pdf2dsc pdf2ps pf2afm printafm ps2ascii ps2epsi ps2pdf ps2ps wftopfa ;\
	do if ( test -f $$f ); then $(INSTALL_PROGRAM) $$f $(scriptdir)/$$f; fi;\
	done'

MAN1_PAGES=gs pdf2dsc pdf2ps ps2ascii ps2epsi ps2pdf ps2ps
install-data: gs.1
	-mkdir $(mandir)
	-mkdir $(man1dir)
	sh -c 'for f in $(MAN1_PAGES) ;\
	do if ( test -f $$f.1 ); then $(INSTALL_DATA) $$f.1 $(man1dir)/$$f.$(man1ext); fi;\
	done'
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	sh -c 'for f in \
Fontmap \
cbjc600.ppd cbjc800.ppd *.upp \
gs_init.ps gs_btokn.ps gs_ccfnt.ps gs_cff.ps gs_cidfn.ps gs_cmap.ps \
gs_diskf.ps gs_dpnxt.ps gs_dps.ps gs_dps1.ps gs_dps2.ps gs_epsf.ps \
gs_fonts.ps gs_kanji.ps gs_lev2.ps gs_ll3.ps \
gs_pfile.ps gs_res.ps gs_setpd.ps gs_statd.ps \
gs_ttf.ps gs_typ32.ps gs_typ42.ps gs_type1.ps \
gs_dbt_e.ps gs_iso_e.ps gs_ksb_e.ps gs_std_e.ps gs_sym_e.ps \
acctest.ps align.ps bdftops.ps caption.ps decrypt.ps docie.ps \
font2c.ps font2pcl.ps gslp.ps impath.ps landscap.ps level1.ps lines.ps \
markhint.ps markpath.ps \
packfile.ps pcharstr.ps pf2afm.ps ppath.ps prfont.ps printafm.ps \
ps2ai.ps ps2ascii.ps ps2epsi.ps ps2image.ps \
quit.ps showchar.ps showpage.ps stcinfo.ps stcolor.ps \
traceimg.ps traceop.ps type1enc.ps type1ops.ps uninfo.ps unprot.ps \
viewcmyk.ps viewgif.ps viewjpeg.ps viewpcx.ps viewpbm.ps viewps2a.ps \
winmaps.ps wftopfa.ps wrfont.ps zeroline.ps \
gs_l2img.ps \
pdf2dsc.ps \
pdf_base.ps pdf_draw.ps pdf_font.ps pdf_main.ps pdf_ops.ps pdf_sec.ps \
gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps \
gs_pdfwr.ps ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(gsdatadir)/$$f; fi;\
	done'
	-mkdir $(docdir)
	sh -c 'for f in \
COPYING NEWS PUBLIC README \
bug-form.txt c-style.txt current.txt devices.txt drivers.txt fonts.txt \
helpers.txt hershey.txt history1.txt history2.txt history3.txt humor.txt \
install.txt language.txt lib.txt make.txt new-user.txt \
ps2epsi.txt ps2pdf.txt psfiles.txt public.txt \
unix-lpr.txt use.txt xfonts.txt ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(docdir)/$$f; fi;\
	done'
	-mkdir $(exdir)
	for f in \
alphabet.ps chess.ps cheq.ps colorcir.ps escher.ps golfer.ps \
grayalph.ps snowflak.ps tiger.ps vasarely.ps waterfal.ps \
ridt91.eps ;\
	do $(INSTALL_DATA) $$f $(exdir)/$$f ;\
	done
