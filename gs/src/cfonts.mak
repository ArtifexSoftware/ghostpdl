#    Copyright (C) 1992, 1995, 1996 Aladdin Enterprises.  All rights reserved.
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

# Makefile for compiling PostScript Type 1 fonts into C.
# For more information about fonts, consult the Fontmap file,
# and also fonts.txt.

# Edit the following 2 lines to reflect your environment.
OBJ=o
CCCF=gcc -c -O

CFONTS=.
FONT2C=font2c

# ---------------------------------------------------------------- #

# This file supports two slightly different font sets:
# the de facto commercial standard set of 35 PostScript fonts, and a slightly
# larger set distributed with the free version of the software.

fonts_standard_o: \
	AvantGarde_o Bookman_o Courier_o \
	Helvetica_o NewCenturySchlbk_o Palatino_o \
	TimesRoman_o Symbol_o ZapfChancery_o ZapfDingbats_o

fonts_standard_c: \
	AvantGarde_c Bookman_c Courier_c \
	Helvetica_c NewCenturySchlbk_c Palatino_c \
	TimesRoman_c Symbol_c ZapfChancery_c ZapfDingbats_c

fonts_free_o: fonts_standard_o \
	CharterBT_o Cyrillic_o Kana_o Utopia_o

fonts_free_c: fonts_standard_c \
	CharterBT_c Cyrillic_c Kana_c Utopia_c

# ---------------------------------------------------------------- #
#                                                                  #
#                         Standard 35 fonts                        #
#                                                                  #
# ---------------------------------------------------------------- #

# By convention, the names of the 35 standard compiled fonts use '0' for
# the foundry name.  This allows users to substitute different foundries
# without having to change this makefile.

# ---------------- Avant Garde ----------------

AvantGarde_c: $(CFONTS)/0agk.c $(CFONTS)/0agko.c $(CFONTS)/0agd.c \
	$(CFONTS)/0agdo.c

$(CFONTS)/0agk.c:
	$(FONT2C) AvantGarde-Book $(CFONTS)/0agk.c agk

$(CFONTS)/0agko.c:
	$(FONT2C) AvantGarde-BookOblique $(CFONTS)/0agko.c agko

$(CFONTS)/0agd.c:
	$(FONT2C) AvantGarde-Demi $(CFONTS)/0agd.c agd

$(CFONTS)/0agdo.c:
	$(FONT2C) AvantGarde-DemiOblique $(CFONTS)/0agdo.c agdo

AvantGarde_o: 0agk.$(OBJ) 0agko.$(OBJ) 0agd.$(OBJ) 0agdo.$(OBJ)

0agk.$(OBJ): $(CFONTS)/0agk.c $(CCFONT)
	$(CCCF) $(CFONTS)/0agk.c

0agko.$(OBJ): $(CFONTS)/0agko.c $(CCFONT)
	$(CCCF) $(CFONTS)/0agko.c

0agd.$(OBJ): $(CFONTS)/0agd.c $(CCFONT)
	$(CCCF) $(CFONTS)/0agd.c

0agdo.$(OBJ): $(CFONTS)/0agdo.c $(CCFONT)
	$(CCCF) $(CFONTS)/0agdo.c

# ---------------- Bookman ----------------

Bookman_c: $(CFONTS)/0bkl.c $(CFONTS)/0bkli.c $(CFONTS)/0bkd.c \
	$(CFONTS)/0bkdi.c

$(CFONTS)/0bkl.c:
	$(FONT2C) Bookman-Light $(CFONTS)/0bkl.c bkl

$(CFONTS)/0bkli.c:
	$(FONT2C) Bookman-LightItalic $(CFONTS)/0bkli.c bkli

$(CFONTS)/0bkd.c:
	$(FONT2C) Bookman-Demi $(CFONTS)/0bkd.c bkd

$(CFONTS)/0bkdi.c:
	$(FONT2C) Bookman-DemiItalic $(CFONTS)/0bkdi.c bkdi

Bookman_o: 0bkl.$(OBJ) 0bkli.$(OBJ) 0bkd.$(OBJ) 0bkdi.$(OBJ)

0bkl.$(OBJ): $(CFONTS)/0bkl.c $(CCFONT)
	$(CCCF) $(CFONTS)/0bkl.c

0bkli.$(OBJ): $(CFONTS)/0bkli.c $(CCFONT)
	$(CCCF) $(CFONTS)/0bkli.c

0bkd.$(OBJ): $(CFONTS)/0bkd.c $(CCFONT)
	$(CCCF) $(CFONTS)/0bkd.c

0bkdi.$(OBJ): $(CFONTS)/0bkdi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0bkdi.c

# ---------------- Courier ----------------

Courier_c: $(CFONTS)/0crr.c $(CFONTS)/0cri.c $(CFONTS)/0crb.c \
	$(CFONTS)/0crbi.c

$(CFONTS)/0crr.c:
	$(FONT2C) Courier $(CFONTS)/0crr.c crr

$(CFONTS)/0cri.c:
	$(FONT2C) Courier-Italic $(CFONTS)/0cri.c cri

$(CFONTS)/0crb.c:
	$(FONT2C) Courier-Bold $(CFONTS)/0crb.c crb

$(CFONTS)/0crbi.c:
	$(FONT2C) Courier-BoldItalic $(CFONTS)/0crbi.c crbi

Courier_o: 0crr.$(OBJ) 0cri.$(OBJ) 0crb.$(OBJ) 0crbi.$(OBJ)

0crr.$(OBJ): $(CFONTS)/0crr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0crr.c

0cri.$(OBJ): $(CFONTS)/0cri.c $(CCFONT)
	$(CCCF) $(CFONTS)/0cri.c

0crb.$(OBJ): $(CFONTS)/0crb.c $(CCFONT)
	$(CCCF) $(CFONTS)/0crb.c

0crbi.$(OBJ): $(CFONTS)/0crbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0crbi.c

# ---------------- Helvetica ----------------

Helvetica_c: $(CFONTS)/0hvr.c $(CFONTS)/0hvro.c \
	$(CFONTS)/0hvb.c $(CFONTS)/0hvbo.c $(CFONTS)/0hvrrn.c \
	$(CFONTS)/0hvrorn.c $(CFONTS)/0hvbrn.c $(CFONTS)/0hvborn.c

$(CFONTS)/0hvr.c:
	$(FONT2C) Helvetica $(CFONTS)/0hvr.c hvr

$(CFONTS)/0hvro.c:
	$(FONT2C) Helvetica-Oblique $(CFONTS)/0hvro.c hvro

$(CFONTS)/0hvb.c:
	$(FONT2C) Helvetica-Bold $(CFONTS)/0hvb.c hvb

$(CFONTS)/0hvbo.c:
	$(FONT2C) Helvetica-BoldOblique $(CFONTS)/0hvbo.c hvbo

$(CFONTS)/0hvrrn.c:
	$(FONT2C) Helvetica-Narrow $(CFONTS)/0hvrrn.c hvrrn

$(CFONTS)/0hvrorn.c:
	$(FONT2C) Helvetica-Narrow-Oblique $(CFONTS)/0hvrorn.c hvrorn

$(CFONTS)/0hvbrn.c:
	$(FONT2C) Helvetica-Narrow-Bold $(CFONTS)/0hvbrn.c hvbrn

$(CFONTS)/0hvborn.c:
	$(FONT2C) Helvetica-Narrow-BoldOblique $(CFONTS)/0hvborn.c hvborn

Helvetica_o: 0hvr.$(OBJ) 0hvro.$(OBJ) 0hvb.$(OBJ) 0hvbo.$(OBJ) \
	0hvrrn.$(OBJ) 0hvrorn.$(OBJ) 0hvbrn.$(OBJ) 0hvborn.$(OBJ)

0hvr.$(OBJ): $(CFONTS)/0hvr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvr.c

0hvro.$(OBJ): $(CFONTS)/0hvro.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvro.c

0hvb.$(OBJ): $(CFONTS)/0hvb.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvb.c

0hvbo.$(OBJ): $(CFONTS)/0hvbo.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvbo.c

0hvrrn.$(OBJ): $(CFONTS)/0hvrrn.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvrrn.c

0hvrorn.$(OBJ): $(CFONTS)/0hvrorn.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvrorn.c

0hvbrn.$(OBJ): $(CFONTS)/0hvbrn.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvbrn.c

0hvborn.$(OBJ): $(CFONTS)/0hvborn.c $(CCFONT)
	$(CCCF) $(CFONTS)/0hvborn.c

# ---------------- New Century Schoolbook ----------------

NewCenturySchlbk_c: $(CFONTS)/0ncr.c $(CFONTS)/0ncri.c $(CFONTS)/0ncb.c \
	$(CFONTS)/0ncbi.c

$(CFONTS)/0ncr.c:
	$(FONT2C) NewCenturySchlbk-Roman $(CFONTS)/0ncr.c ncr

$(CFONTS)/0ncri.c:
	$(FONT2C) NewCenturySchlbk-Italic $(CFONTS)/0ncri.c ncri

$(CFONTS)/0ncb.c:
	$(FONT2C) NewCenturySchlbk-Bold $(CFONTS)/0ncb.c ncb

$(CFONTS)/0ncbi.c:
	$(FONT2C) NewCenturySchlbk-BoldItalic $(CFONTS)/0ncbi.c ncbi

NewCenturySchlbk_o: 0ncr.$(OBJ) 0ncri.$(OBJ) 0ncb.$(OBJ) 0ncbi.$(OBJ)

0ncr.$(OBJ): $(CFONTS)/0ncr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0ncr.c

0ncri.$(OBJ): $(CFONTS)/0ncri.c $(CCFONT)
	$(CCCF) $(CFONTS)/0ncri.c

0ncb.$(OBJ): $(CFONTS)/0ncb.c $(CCFONT)
	$(CCCF) $(CFONTS)/0ncb.c

0ncbi.$(OBJ): $(CFONTS)/0ncbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0ncbi.c

# ---------------- Palatino ----------------

Palatino_c: $(CFONTS)/0plr.c $(CFONTS)/0plri.c $(CFONTS)/0plb.c \
	$(CFONTS)/0plbi.c

$(CFONTS)/0plr.c:
	$(FONT2C) Palatino-Roman $(CFONTS)/0plr.c plr

$(CFONTS)/0plri.c:
	$(FONT2C) Palatino-Italic $(CFONTS)/0plri.c plri

$(CFONTS)/0plb.c:
	$(FONT2C) Palatino-Bold $(CFONTS)/0plb.c plb

$(CFONTS)/0plbi.c:
	$(FONT2C) Palatino-BoldItalic $(CFONTS)/0plbi.c plbi

Palatino_o: 0plr.$(OBJ) 0plri.$(OBJ) 0plb.$(OBJ) 0plbi.$(OBJ)

0plr.$(OBJ): $(CFONTS)/0plr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0plr.c

0plri.$(OBJ): $(CFONTS)/0plri.c $(CCFONT)
	$(CCCF) $(CFONTS)/0plri.c

0plb.$(OBJ): $(CFONTS)/0plb.c $(CCFONT)
	$(CCCF) $(CFONTS)/0plb.c

0plbi.$(OBJ): $(CFONTS)/0plbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0plbi.c

# ---------------- Times Roman ----------------

TimesRoman_c: $(CFONTS)/0tmr.c $(CFONTS)/0tmri.c $(CFONTS)/0tmb.c \
	$(CFONTS)/0tmbi.c

$(CFONTS)/0tmr.c:
	$(FONT2C) Times-Roman $(CFONTS)/0tmr.c tmr

$(CFONTS)/0tmri.c:
	$(FONT2C) Times-Italic $(CFONTS)/0tmri.c tmri

$(CFONTS)/0tmb.c:
	$(FONT2C) Times-Bold $(CFONTS)/0tmb.c tmb

$(CFONTS)/0tmbi.c:
	$(FONT2C) Times-BoldItalic $(CFONTS)/0tmbi.c tmbi

TimesRoman_o: 0tmr.$(OBJ) 0tmri.$(OBJ) 0tmb.$(OBJ) 0tmbi.$(OBJ)

0tmr.$(OBJ): $(CFONTS)/0tmr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0tmr.c

0tmri.$(OBJ): $(CFONTS)/0tmri.c $(CCFONT)
	$(CCCF) $(CFONTS)/0tmri.c

0tmb.$(OBJ): $(CFONTS)/0tmb.c $(CCFONT)
	$(CCCF) $(CFONTS)/0tmb.c

0tmbi.$(OBJ): $(CFONTS)/0tmbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0tmbi.c

# ---------------- Symbol ----------------

Symbol_c: $(CFONTS)/0syr.c

$(CFONTS)/0syr.c:
	$(FONT2C) Symbol $(CFONTS)/0syr.c syr

Symbol_o: 0syr.$(OBJ)

0syr.$(OBJ): $(CFONTS)/0syr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0syr.c

# ---------------- Zapf Chancery ----------------

ZapfChancery_c: $(CFONTS)/0zcmi.c

$(CFONTS)/0zcmi.c:
	$(FONT2C) ZapfChancery-MediumItalic $(CFONTS)/0zcmi.c zcmi

ZapfChancery_o: 0zcmi.$(OBJ)

0zcmi.$(OBJ): $(CFONTS)/0zcmi.c $(CCFONT)
	$(CCCF) $(CFONTS)/0zcmi.c

# ---------------- Zapf Dingbats ----------------

ZapfDingbats_c: $(CFONTS)/0zdr.c

$(CFONTS)/0zdr.c:
	$(FONT2C) ZapfDingbats $(CFONTS)/0zdr.c zdr

ZapfDingbats_o: 0zdr.$(OBJ)

0zdr.$(OBJ): $(CFONTS)/0zdr.c $(CCFONT)
	$(CCCF) $(CFONTS)/0zdr.c

# ---------------------------------------------------------------- #
#                                                                  #
#                         Additional fonts                         #
#                                                                  #
# ---------------------------------------------------------------- #

# ---------------- Bitstream Charter ----------------

CharterBT_c: $(CFONTS)/bchr.c $(CFONTS)/bchri.c $(CFONTS)/bchb.c \
	$(CFONTS)/bchbi.c

$(CFONTS)/bchr.c:
	$(FONT2C) Charter-Roman $(CFONTS)/bchr.c chr

$(CFONTS)/bchri.c:
	$(FONT2C) Charter-Italic $(CFONTS)/bchri.c chri

$(CFONTS)/bchb.c:
	$(FONT2C) Charter-Bold $(CFONTS)/bchb.c chb

$(CFONTS)/bchbi.c:
	$(FONT2C) Charter-BoldItalic $(CFONTS)/bchbi.c chbi

CharterBT_o: bchr.$(OBJ) bchri.$(OBJ) bchb.$(OBJ) bchbi.$(OBJ)

bchr.$(OBJ): $(CFONTS)/bchr.c $(CCFONT)
	$(CCCF) $(CFONTS)/bchr.c

bchri.$(OBJ): $(CFONTS)/bchri.c $(CCFONT)
	$(CCCF) $(CFONTS)/bchri.c

bchb.$(OBJ): $(CFONTS)/bchb.c $(CCFONT)
	$(CCCF) $(CFONTS)/bchb.c

bchbi.$(OBJ): $(CFONTS)/bchbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/bchbi.c

# ---------------- Cyrillic ----------------

Cyrillic_c: $(CFONTS)/fcyr.c $(CFONTS)/fcyri.c

$(CFONTS)/fcyr.c:
	$(FONT2C) Cyrillic $(CFONTS)/fcyr.c fcyr

$(CFONTS)/fcyri.c:
	$(FONT2C) Cyrillic-Italic $(CFONTS)/fcyri.c fcyri

Cyrillic_o: fcyr.$(OBJ) fcyri.$(OBJ)

fcyr.$(OBJ): $(CFONTS)/fcyr.c $(CCFONT)
	$(CCCF) $(CFONTS)/fcyr.c

fcyri.$(OBJ): $(CFONTS)/fcyri.c $(CCFONT)
	$(CCCF) $(CFONTS)/fcyri.c

# ---------------- Kana ----------------

Kana_c: $(CFONTS)/fhirw.c $(CFONTS)/fkarw.c

$(CFONTS)/fhirw.c:
	$(FONT2C) Calligraphic-Hiragana $(CFONTS)/fhirw.c fhirw

$(CFONTS)/fkarw.c:
	$(FONT2C) Calligraphic-Katakana $(CFONTS)/fkarw.c fkarw

Kana_o: fhirw.$(OBJ) fkarw.$(OBJ)

fhirw.$(OBJ): $(CFONTS)/fhirw.c $(CCFONT)
	$(CCCF) $(CFONTS)/fhirw.c

fkarw.$(OBJ): $(CFONTS)/fkarw.c $(CCFONT)
	$(CCCF) $(CFONTS)/fkarw.c

# ---------------- Utopia ----------------

Utopia_c: $(CFONTS)/putr.c $(CFONTS)/putri.c $(CFONTS)/putb.c \
	$(CFONTS)/putbi.c

$(CFONTS)/putr.c:
	$(FONT2C) Utopia-Regular $(CFONTS)/putr.c utr

$(CFONTS)/putri.c:
	$(FONT2C) Utopia-Italic $(CFONTS)/putri.c utri

$(CFONTS)/putb.c:
	$(FONT2C) Utopia-Bold $(CFONTS)/putb.c utb

$(CFONTS)/putbi.c:
	$(FONT2C) Utopia-BoldItalic $(CFONTS)/putbi.c utbi

Utopia_o: putr.$(OBJ) putri.$(OBJ) putb.$(OBJ) putbi.$(OBJ)

putr.$(OBJ): $(CFONTS)/putr.c $(CCFONT)
	$(CCCF) $(CFONTS)/putr.c

putri.$(OBJ): $(CFONTS)/putri.c $(CCFONT)
	$(CCCF) $(CFONTS)/putri.c

putb.$(OBJ): $(CFONTS)/putb.c $(CCFONT)
	$(CCCF) $(CFONTS)/putb.c

putbi.$(OBJ): $(CFONTS)/putbi.c $(CCFONT)
	$(CCCF) $(CFONTS)/putbi.c
