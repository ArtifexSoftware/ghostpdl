#    Copyright (C) 1994, 1995, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# Partial makefile common to all Unix and Desqview/X configurations.

# This is the very last part of the makefile for these configurations.
# Since Unix make doesn't have an 'include' facility, we concatenate
# the various parts of the makefile together by brute force (in tar_cat).

# Define a rule for building profiling configurations.
pg:
	make GENOPT='' CFLAGS='-pg -O $(GCFLAGS) $(XCFLAGS)' LDFLAGS='$(XLDFLAGS) -pg' XLIBS='Xt SM ICE Xext X11' CCLEAF='$(CCC)'

# Define a rule for building debugging configurations.
debug:
	make GENOPT='-DDEBUG' CFLAGS='-g -O $(GCFLAGS) $(XCFLAGS)'

# The rule for gconfigv.h is here because it is shared between Unix and
# DV/X environments.
gconfigv.h: unix-end.mak $(MAKEFILE) $(ECHOGS_XE)
	$(EXP)echogs -w gconfigv.h -x 23 define USE_ASM -x 2028 -q $(USE_ASM)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define USE_FPU -x 2028 -q $(FPU_TYPE)-0 -x 29
	$(EXP)echogs -a gconfigv.h -x 23 define EXTEND_NAMES 0$(EXTEND_NAMES)

# The following rules are equivalent to what tar_cat does.
# The rm -f is so that we don't overwrite a file that `make'
# may currently be reading from.
GENERIC_MAK_LIST=$(GS_MAK) $(LIB_MAK) $(INT_MAK) $(JPEG_MAK) $(LIBPNG_MAK) $(ZLIB_MAK) $(DEVS_MAK)
UNIX_MAK_LIST=dvx-gcc.mak unixansi.mak unix-cc.mak unix-gcc.mak

unix.mak: $(UNIX_MAK_LIST)

DVX_GCC_MAK=$(VERSION_MAK) dgc-head.mak dvx-head.mak $(GENERIC_MAK_LIST) dvx-tail.mak unix-end.mak
dvx-gcc.mak: $(DVX_GCC_MAK)
	rm -f dvx-gcc.mak
	$(CAT) $(DVX_GCC_MAK) >dvx-gcc.mak

UNIXANSI_MAK=$(VERSION_MAK) ansihead.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unixansi.mak: $(UNIXANSI_MAK)
	rm -f unixansi.mak
	$(CAT) $(UNIXANSI_MAK) >unixansi.mak

UNIX_CC_MAK=$(VERSION_MAK) cc-head.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unix-cc.mak: $(UNIX_CC_MAK)
	rm -f unix-cc.mak
	$(CAT) $(UNIX_CC_MAK) >unix-cc.mak

UNIX_GCC_MAK=$(VERSION_MAK) gcc-head.mak unixhead.mak $(GENERIC_MAK_LIST) unixtail.mak unix-end.mak
unix-gcc.mak: $(UNIX_GCC_MAK)
	rm -f unix-gcc.mak
	$(CAT) $(UNIX_GCC_MAK) >unix-gcc.mak

# Installation

TAGS:
	etags -t *.c *.h

install: install-exec install-scripts install-data

# The sh -c in the rules below is required because Ultrix's implementation
# of sh -e terminates execution of a command if any error occurs, even if
# the command traps the error with ||.

install-exec: $(GS)
	-mkdir $(bindir)
	$(INSTALL_PROGRAM) $(GS) $(bindir)/$(GS)

install-scripts: gsnd
	-mkdir $(scriptdir)
	sh -c 'for f in gsbj gsdj gsdj500 gslj gslp gsnd bdftops font2c \
pdf2dsc pdf2ps printafm ps2ascii ps2epsi ps2pdf wftopfa ;\
	do if ( test -f $$f ); then $(INSTALL_PROGRAM) $$f $(scriptdir)/$$f; fi;\
	done'

MAN1_PAGES=gs pdf2dsc pdf2ps ps2ascii ps2epsi ps2pdf
install-data: gs.1
	-mkdir $(mandir)
	-mkdir $(man1dir)
	sh -c 'for f in $(MAN1_PAGES) ;\
	do if ( test -f $$f.1 ); then $(INSTALL_DATA) $$f.1 $(man1dir)/$$f.$(man1ext); fi;\
	done'
	-mkdir $(datadir)
	-mkdir $(gsdir)
	-mkdir $(gsdatadir)
	sh -c 'for f in Fontmap \
cbjc600.ppd cbjc800.ppd *.upp \
gs_init.ps gs_btokn.ps gs_ccfnt.ps gs_cff.ps gs_cidfn.ps gs_cmap.ps \
gs_diskf.ps gs_dpnxt.ps gs_dps.ps gs_dps1.ps gs_dps2.ps gs_epsf.ps \
gs_fonts.ps gs_kanji.ps gs_lev2.ps \
gs_pfile.ps gs_res.ps gs_setpd.ps gs_statd.ps \
gs_ttf.ps gs_typ42.ps gs_type1.ps \
gs_dbt_e.ps gs_iso_e.ps gs_ksb_e.ps gs_std_e.ps gs_sym_e.ps \
acctest.ps align.ps bdftops.ps caption.ps decrypt.ps docie.ps \
font2c.ps gslp.ps impath.ps landscap.ps level1.ps lines.ps \
markhint.ps markpath.ps \
packfile.ps pcharstr.ps pfbtogs.ps ppath.ps prfont.ps printafm.ps \
ps2ai.ps ps2ascii.ps ps2epsi.ps ps2image.ps \
quit.ps showchar.ps showpage.ps stcinfo.ps stcolor.ps \
traceimg.ps traceop.ps type1enc.ps type1ops.ps uninfo.ps unprot.ps \
viewcmyk.ps viewgif.ps viewjpeg.ps viewpcx.ps viewpbm.ps viewps2a.ps \
winmaps.ps wftopfa.ps wrfont.ps zeroline.ps \
gs_l2img.ps gs_pdf.ps \
pdf2dsc.ps \
pdf_base.ps pdf_draw.ps pdf_font.ps pdf_main.ps pdf_sec.ps pdf_2ps.ps \
gs_mex_e.ps gs_mro_e.ps gs_pdf_e.ps gs_wan_e.ps \
gs_pdfwr.ps ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(gsdatadir)/$$f; fi;\
	done'
	-mkdir $(docdir)
	sh -c 'for f in COPYING NEWS PUBLIC README \
bug-form.txt c-style.txt current.txt devices.txt drivers.txt fonts.txt \
helpers.txt hershey.txt history1.txt history2.txt history3.txt humor.txt \
install.txt language.txt lib.txt make.txt new-user.txt \
ps2epsi.txt ps2pdf.txt psfiles.txt public.txt \
unix-lpr.txt use.txt xfonts.txt ;\
	do if ( test -f $$f ); then $(INSTALL_DATA) $$f $(docdir)/$$f; fi;\
	done'
	-mkdir $(exdir)
	for f in alphabet.ps chess.ps cheq.ps colorcir.ps escher.ps golfer.ps \
grayalph.ps snowflak.ps tiger.ps waterfal.ps \
ridt91.eps ;\
	do $(INSTALL_DATA) $$f $(exdir)/$$f ;\
	done
