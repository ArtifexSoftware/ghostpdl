# $Id: contrib.mak,v 1.28 2002/10/16 23:26:33 giles Exp $

# Makefile for device drivers which are not in the original GPL GhostScript
# distributiion

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#

# Define the name of this makefile.
CONTRIB_MAK=$(CONTRIBDIR)$(D)contrib.mak $(TOP_MAKEFILES)
CONTRIBSRC=$(CONTRIBDIR)$(D)

# Almost all device drivers depend on the following:
CONTDEV=$(AK) $(ECHOGS_XE) $(GDEVH)

###### --------------------------- Catalog -------------------------- ######

# The following drivers are user-contributed, and maintained (if at all) by
# users.  Please report problems in these drivers to their authors, whose
# e-mail addresses appear below: do not report them to mailing lists or
# mailboxes for general Ghostscript problems.

# Displays:
#
#       None currently
#
# Printers:
#
#	bj10v	 Canon BubbleJet BJ10v/BJ15v (japanese)
#	bj10vh	 Canon BubbleJet BJ10v/BJ15v/BJ35v (japanese)
#	bjc880j	 Canon Color BubbleJet BJC-880J (japanese)
#       bjccolor Canon BJC-250, BJC-250ex, BJC-1000, ... Floyd-Steinberg
#                dithering
#       bjccmyk  Canon BJC-250, BJC-250ex, BJC-1000, ... GhostScript
#	         standard dithering
#       bjcgray  Canon BJC-250, BJC-250ex, BJC-1000, ... Grayscale mode
#       bjcmono  Canon BJC-250, BJC-250ex, BJC-1000, ... Monochrome mode
#       cdj1600  HP DeskJet 1600
#       cdj670   HP DeskJet 670
#       cdj850   HP DeskJet 850
#       cdj880   HP DeskJet 880
#       cdj890   HP DeskJet 890
#       cdj970   HP DeskJet 970
#	cdnj500  HP DesignJet 500
#	chp2200  HP Business Inkjet 2200
#       dl2100   DEC DL2100
#	escpage  Epson ESC/Page driver for Ghostscript (japanese)
#	fmpr	 Fujitsu FMPR (japanese)
#	fmlbp	 Fujitsu FMLBP2xx Page Printer (japanese)
#	gdi	 Samsung's old driver for their SmartGDI laser printers:
#		 ML-4500, ML-2xx, ML-1xxx, ML-5080, ML-6040, ... and
#		 Lexmark E210, same as "samsunggdi"
#       hl1240   Brother HL-1240 and compatible 600-dpi PCL-5 printers
#       hl1250   Brother HL-1250 and compatible 1200x600-dpi PCL-5 printers
#	hpdj1120c HP DeskJet 1120 ("pcl3" driver)
#	hpdj310  HP DeskJet 310 ("pcl3" driver)
#	hpdj320  HP DeskJet 320 ("pcl3" driver)
#	hpdj340  HP DeskJet 340 ("pcl3" driver)
#	hpdj400  HP DeskJet 400 ("pcl3" driver)
#	hpdj500  HP DeskJet 500 ("pcl3" driver)
#	hpdj500c HP DeskJet 500c ("pcl3" driver)
#	hpdj510  HP DeskJet 510 ("pcl3" driver)
#	hpdj520  HP DeskJet 520 ("pcl3" driver)
#	hpdj540  HP DeskJet 540 ("pcl3" driver)
#	hpdj550c HP DeskJet 550c ("pcl3" driver)
#	hpdj560c HP DeskJet 560c ("pcl3" driver)
#	hpdj600  HP DeskJet 600 ("pcl3" driver)
#	hpdj660c HP DeskJet 660c ("pcl3" driver)
#	hpdj670c HP DeskJet 670c ("pcl3" driver)
#	hpdj680c HP DeskJet 680c ("pcl3" driver)
#	hpdj690c HP DeskJet 690c ("pcl3" driver)
#	hpdj850c HP DeskJet 850c ("pcl3" driver)
#	hpdj855c HP DeskJet 855c ("pcl3" driver)
#	hpdj870c HP DeskJet 870c ("pcl3" driver)
#	hpdj890c HP DeskJet 890c ("pcl3" driver)
#	hpdjplus HP DeskJet Plus ("pcl3" driver)
#	hpdjportable HP DeskJet Portable ("pcl3" driver)
#	jj100    Star JJ-100 (japanese)
#       la50     DEC LA50
#       la70     DEC LA70
#       la75     DEC LA75
#       la75plus DEC LA75+
#	lbp310	 Canon LBP-310 (japanese)
#	lbp320	 Canon LBP-320 Pro/LBP-350 (japanese)
#       lex2050  Lexmark 2050
#       lex3200  Lexmark 3200
#       lex5700  Lexmark 5700
#       lex7000  Lexmark 7000, Lexmark IJ900, Compaq A900, Z51
#	lips2p	 Canon LIPS-II+ (japanese)
#	lips4v	 Canon LIPS IV vector mode driver
#	lips4	 Canon LIPS IV raster mode driver
#       ln03     DEC LN03
#       lxm3200  Lexmark 3200, Z31, Z12
#	md1xMono Alps MD-1000/1300/1500 (monochrome mode, japanese)
#	md2k	 Alps MD-2000/2010/4000/1000/1300/1500
#	md50Mono Alps MD-5000, Oki DP-5000 (monochrome mode, japanese)
#	md50Eco	 Alps MD-5000, Oki DP-5000 (economy mode, japanese)
#	md5k	 Alps MD-5000, Oki DP-5000
#	mj500c	 Epson Stylus Color IIs, 200, 1500 (japanese)
#	mj700v2c Epson Stylus, Epson MachJet (japanese)
#	mj6000c	 Epson Stylus Color 400, 800, 1520 (japanese)
#	mj8000c	 Epson Stylus Color 3000 (japanese)
#	ml600	 Okidata Microline 600CL/620CL (japanese)
#	npdl     NEC MultiWriter, PC-PR1000/2000 (japanese)
#       oki4w    Okidata OkiPage 4w+
#	opvp	 Gluecode for Vector Driver API
#	pcl3     PCL-3(+) printers: Many, especially older HP inkjets,
#		 non-HP inkjets as Sharp AJ, Xerox DocuPrint, ...
#	pr1000	 NEC PC-PR 1000 (japanese)
#	pr1000_4 NEC PC-PR 1000/4 (japanese)
#	pr150	 NEC PC-PR 150 (japanese)
#	pr201	 NEC PC-PR 201 (japanese)
#	rpdl     Ricoh RPDL I/II/III/IV drver for Ghostscript (japanese)
#	samsunggdi Samsung's old driver for their SmartGDI laser printers:
#		 ML-4500, ML-2xx, ML-1xxx, ML-5080, ML-6040, ... and
#		 Lexmark E210, same as "gdi"
#       xes      Xerox XES (2700, 3700, 4045, etc.)
#
# Fax systems:
#
#       None currently
#
# Special features
#
#	uniprint/: Some additional .upp files
#

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

### --------------------- The Apple printer devices --------------------- ###
### Note: these drivers were contributed by users.                        ###
###                                                                       ###
### Maintained by Mike Galatean contact via https://bugs.ghostscript.com  ###
###                                                                       ###

appledmp_=$(DEVOBJ)gdevadmp.$(OBJ)

$(DEVOBJ)gdevadmp.$(OBJ) : $(CONTRIBSRC)gdevadmp.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevadmp.$(OBJ) $(C_) $(CONTRIBSRC)gdevadmp.c

$(DD)appledmp.dev : $(appledmp_) $(DD)page.dev $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)appledmp $(appledmp_)

$(DD)iwhi.dev : $(appledmp_) $(DD)page.dev $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)iwhi $(appledmp_)

$(DD)iwlo.dev : $(appledmp_) $(DD)page.dev $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)iwlo $(appledmp_)

$(DD)iwlq.dev : $(appledmp_) $(DD)page.dev $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)iwlq $(appledmp_)


### ----------------- The BJC-210/240/250/250ex/265/1000 ---------------- ###

###
### For questions about the driver, mailto://szaszg@hu.inter.net
### http://bjc250gs.sourceforge.net
###

bjc_h=$(CONTRIBSRC)gdevbjc_.h

bjc_=$(DEVOBJ)gdevbjc_.$(OBJ) $(DEVOBJ)gdevbjca.$(OBJ)

$(DEVOBJ)gdevbjc_.$(OBJ) : $(CONTRIBSRC)gdevbjc_.c $(PDEVH) $(bjc_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbjc_.$(OBJ) $(C_) $(CONTRIBSRC)gdevbjc_.c

$(DEVOBJ)gdevbjca.$(OBJ) : $(CONTRIBSRC)gdevbjca.c $(PDEVH) $(bjc_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbjca.$(OBJ) $(C_) $(CONTRIBSRC)gdevbjca.c

$(DD)bjcmono.dev : $(bjc_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjcmono $(bjc_)

$(DD)bjcgray.dev : $(bjc_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjcgray $(bjc_)

$(DD)bjccmyk.dev : $(bjc_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjccmyk $(bjc_)

$(DD)bjccolor.dev : $(bjc_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjccolor $(bjc_)


### -------------- cdj850 - HP 850c Driver under development ------------- ###

cdeskjet8_=$(DEVOBJ)gdevcd8.$(OBJ) $(HPPCL)

# Author: Uli Wortmann (uliw@erdw.ethz.ch), Martin Gerbershagen (ger@ulm.temic.de)
# Printer: HP 670
$(DD)cdj670.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj670 $(cdeskjet8_)

# Author: Uli Wortmann (uliw@erdw.ethz.ch)
# Printer: HP 850
$(DD)cdj850.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj850 $(cdeskjet8_)

# Author: Uli Wortmann (uliw@erdw.ethz.ch), Martin Gerbershagen (ger@ulm.temic.de)
# Printer: HP 890
$(DD)cdj890.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj890 $(cdeskjet8_)

# Author: Uli Wortmann (uliw@erdw.ethz.ch), Martin Gerbershagen (ger@ulm.temic.de)
# Printer: HP 1600
$(DD)cdj1600.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj1600 $(cdeskjet8_)

$(DEVOBJ)gdevcd8.$(OBJ) : $(CONTRIBSRC)gdevcd8.c $(PDEVH) $(math__h)\
 $(gsparam_h) $(gxlum_h) $(gdevpcl_h) \
 $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcd8.$(OBJ) $(C_) $(CONTRIBSRC)gdevcd8.c


### -------------- cdj880 - HP 880c Driver under development ------------- ###
### Since this driver is in the development phase it is not distributed    ###
### with Ghostscript, but is available from                                ###
###   http://www.proaxis.com/~mgelhaus/linux/software/hp880c/hp880c.html   ###

### NOTE:  Depends on the presence of the cdj850 section above.            ###

# Author: Matthew Gelhaus (mgelhaus@proaxis.com)
# Printer: HP 880c
$(DD)cdj880.dev : $(cdeskjet8_) $(DD)page.dev $(CONTDEV) \
                  $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj880 $(cdeskjet8_)


### ------------- cdj970 - HP 970Cxi Driver under development ------------- ###

cdeskjet9_=$(DEVOBJ)gdevdj9.$(OBJ) $(HPPCL)

# Author: Rene Harsch (rene@harsch.net)
# Printer: HP 970Cxi
$(DD)cdj970.dev : $(cdeskjet9_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdj970 $(cdeskjet9_)

$(DEVOBJ)gdevdj9.$(OBJ) : $(CONTRIBSRC)gdevdj9.c $(PDEVH) $(math__h) $(string__h)\
 $(gsparam_h) $(gxlum_h) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdj9.$(OBJ) $(C_) $(CONTRIBSRC)gdevdj9.c


### -------------- cdnj500 - HP DesignJet 500 ------------- ###

### NOTE:  Same as chp2200 (some PJL and CRD changes).

$(DD)cdnj500.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cdnj500 $(cdeskjet8_)


### -------------- chp2200 - HP Business Inkjet 2200 ------------- ###

### NOTE:  Depends on the presence of the cdj850 section.

$(DD)chp2200.dev : $(cdeskjet8_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)chp2200 $(cdeskjet8_)


### ---------------- The Samsung SmartGDI laser printer devices --------- ###
### Note : this driver is used by Samsung SmartGDI compatible printers.   ###
###

GDIMONO=$(DEVOBJ)gdevgdi.$(OBJ) $(HPPCL)

$(DD)gdi.dev : $(GDIMONO) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)gdi $(GDIMONO)

$(DD)samsunggdi.dev : $(GDIMONO) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)samsunggdi $(GDIMONO)

$(DEVOBJ)gdevgdi.$(OBJ) : $(CONTRIBSRC)gdevgdi.c $(PDEVH) $(gdevpcl_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevgdi.$(OBJ) $(C_) $(CONTRIBSRC)gdevgdi.c


### ---------------- The Brother HL-1240/HL-1250 printers --------------- ###
### These printers are ljet4-compatible, plus support for source tray     ###
### selection and special 1200x600 dpi mode.                              ###

hl1250_=$(DEVOBJ)gdevhl12.$(OBJ) $(HPDLJM)
$(DD)hl1250.dev : $(hl1250_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hl1250 $(hl1250_)

$(DD)hl1240.dev : $(hl1250_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hl1240 $(hl1250_)

# Author: Marek Michalkiewicz <marekm@linux.org.pl>
# Printer: Brother HL-1250 (may work with some other models too)
$(DEVOBJ)gdevhl12.$(OBJ) : $(CONTRIBSRC)gdevhl12.c $(PDEVH) $(gdevdljm_h) \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevhl12.$(OBJ) $(C_) $(CONTRIBSRC)gdevhl12.c


### -------- The DEC LN03/DL2100/LA50/LA70/LA75 printer devices -------- ###

ln03_=$(DEVOBJ)gdevln03.$(OBJ)

# Author: Ulrich Mueller (ulm@vsnhd1.cern.ch)
# Printer: DEC LN03
$(DD)ln03.dev : $(ln03_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ln03 $(ln03_)

# Author: Nick Brown (nick.brown@coe.int)
# Printer: DEClaser 2100
$(DD)dl2100.dev : $(ln03_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)dl2100 $(ln03_)

# Author: Ian MacPhedran (macphed@dvinci.USask.CA)
# Printer: DEC LA50
$(DD)la50.dev : $(ln03_) $(DD)page.dev \
                          $(CONTDEV)  $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)la50 $(ln03_)

# Author: Bruce Lowekamp (lowekamp@csugrad.cs.vt.edu)
# Printer: DEC LA70
$(DD)la70.dev : $(ln03_) $(DD)page.dev \
                         $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)la70 $(ln03_)

# Author: Ian MacPhedran (macphed@dvinci.USask.CA)
# Printer: DEC LA75
$(DD)la75.dev : $(ln03_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)la75 $(ln03_)

# Author: Andre' Beck (Andre_Beck@IRS.Inf.TU-Dresden.de)
# Printer: DEC LA75plus
$(DD)la75plus.dev : $(ln03_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)la75plus $(ln03_)

$(DEVOBJ)gdevln03.$(OBJ) : $(CONTRIBSRC)gdevln03.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevln03.$(OBJ) $(C_) $(CONTRIBSRC)gdevln03.c


### -------------------------- EPSON Page/Color -------------------------- ###
###
###

ESCV_DIR=$(CONTRIBSRC)eplaser
ESCV_SRC=$(ESCV_DIR)$(D)
escv_opts=-DGS_VERSION_MAJOR=$(GS_VERSION_MAJOR)

escv_=$(DEVOBJ)gdevescv.$(OBJ)

$(DEVOBJ)gdevescv.$(OBJ) : $(ESCV_SRC)gdevescv.c $(ESCV_SRC)gdevescv.h $(PDEVH) $(time__h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevescv.$(OBJ) $(C_) $(escv_opts) $(ESCV_SRC)gdevescv.c

$(DD)alc1900.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc1900 $(escv_)

$(DD)alc2000.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc2000 $(escv_)

$(DD)alc4000.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc4000 $(escv_)

$(DD)alc4100.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc4100 $(escv_)

$(DD)alc8500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc8500 $(escv_)

$(DD)alc8600.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc8600 $(escv_)

$(DD)alc9100.dev : $(escv_) $(DD)page.dev \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)alc9100 $(escv_)

$(DD)lp3000c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp3000c $(escv_)

$(DD)lp8000c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8000c $(escv_)

$(DD)lp8200c.dev : $(escv_) $(DD)page.dev \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8200c $(escv_)

$(DD)lp8300c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8300c $(escv_)

$(DD)lp8500c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8500c $(escv_)

$(DD)lp8800c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8800c $(escv_)

$(DD)lp9000c.dev : $(escv_) $(DD)page.dev \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9000c $(escv_)

$(DD)lp9200c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9200c $(escv_)

$(DD)lp9500c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9500c $(escv_)

$(DD)lp9800c.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9800c $(escv_)

$(DD)lps6500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lps6500 $(escv_)

$(DD)epl2050.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl2050 $(escv_)

$(DD)epl2050p.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl2050p $(escv_)

$(DD)epl2120.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl2120 $(escv_)

$(DD)epl2500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl2500 $(escv_)

$(DD)epl2750.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl2750 $(escv_)

$(DD)epl5800.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl5800 $(escv_)

$(DD)epl5900.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl5900 $(escv_)

$(DD)epl6100.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl6100 $(escv_)

$(DD)epl6200.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epl6200 $(escv_)

$(DD)lp1800.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp1800 $(escv_)

$(DD)lp1900.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp1900 $(escv_)

$(DD)lp2200.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp2200 $(escv_)

$(DD)lp2400.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp2400 $(escv_)

$(DD)lp2500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp2500 $(escv_)

$(DD)lp7500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp7500 $(escv_)

$(DD)lp7700.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp7700 $(escv_)

$(DD)lp7900.dev : $(escv_) $(DD)page.dev \
                          $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp7900 $(escv_)

$(DD)lp8100.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8100 $(escv_)

$(DD)lp8300f.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8300f $(escv_)

$(DD)lp8400f.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8400f $(escv_)

$(DD)lp8600.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8600 $(escv_)

$(DD)lp8600f.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8600f $(escv_)

$(DD)lp8700.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8700 $(escv_)

$(DD)lp8900.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8900 $(escv_)

$(DD)lp9000b.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9000b $(escv_)

$(DD)lp9100.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9100 $(escv_)

$(DD)lp9200b.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9200b $(escv_)

$(DD)lp9300.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9300 $(escv_)

$(DD)lp9400.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9400 $(escv_)

$(DD)lp9600.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9600 $(escv_)

$(DD)lp9600s.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp9600s $(escv_)

$(DD)lps4500.dev : $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lps4500 $(escv_)

$(DD)eplcolor.dev: $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)eplcolor $(escv_)

$(DD)eplmono.dev: $(escv_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)eplmono $(escv_)

# ------ The Lexmark 5700 and 7000 devices ------ #

lex7000_=$(DEVOBJ)gdevlx7.$(OBJ)
$(DD)lex7000.dev : $(lex7000_) $(DD)page.dev \
                           $(CONTDEV) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lex7000 $(lex7000_)

lex5700_=$(DEVOBJ)gdevlx7.$(OBJ)
$(DD)lex5700.dev : $(lex5700_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lex5700 $(lex5700_)

lex3200_=$(DEVOBJ)gdevlx7.$(OBJ)
$(DD)lex3200.dev : $(lex3200_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lex3200 $(lex3200_)

lex2050_=$(DEVOBJ)gdevlx7.$(OBJ)
$(DD)lex2050.dev : $(lex2050_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lex2050 $(lex2050_)

$(DEVOBJ)gdevlx7.$(OBJ) : $(CONTRIBSRC)gdevlx7.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlx7.$(OBJ) $(C_) $(CONTRIBSRC)gdevlx7.c


### ----------- Lexmark 3200 device ------------ ###

lxm3200_=$(DEVOBJ)gdevlx32.$(OBJ)

$(DEVOBJ)gdevlx32.$(OBJ) : $(CONTRIBSRC)gdevlx32.c $(PDEVH) $(gsparam_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlx32.$(OBJ) $(C_) $(CONTRIBSRC)gdevlx32.c

$(DD)lxm3200.dev : $(lxm3200_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lxm3200 $(lxm3200_)


### -------------- The Canon LIPS IV laser printer device -------------- ###
###                                                                      ###
LIPS_DIR=$(CONTRIBSRC)lips4
LIPS_SRC=$(LIPS_DIR)$(D)
LIPS_OPT=-DGS_VERSION_MAJOR=$(GS_VERSION_MAJOR) $(I_)$(LIPS_SRC) $(II)$(PSSRC)$(_I)

$(DEVOBJ)gdevlprn.$(OBJ) : $(LIPS_SRC)gdevlprn.c $(LIPS_SRC)gdevlprn.h\
 $(gdevprn_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlprn.$(OBJ) $(LIPS_OPT) $(C_) $(LIPS_SRC)gdevlprn.c

lipsr_=$(DEVOBJ)gdevl4r.$(OBJ) $(DEVOBJ)gdevlips.$(OBJ) $(DEVOBJ)gdevlprn.$(OBJ)

$(DEVOBJ)gdevl4r.$(OBJ) : $(LIPS_SRC)gdevl4r.c $(LIPS_SRC)gdevlips.h $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevl4r.$(OBJ) $(LIPS_OPT) $(C_)\
 $(LIPS_SRC)gdevl4r.c

$(DEVOBJ)gdevlips.$(OBJ) : $(GX) $(LIPS_SRC)gdevlips.c $(std_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlips.$(OBJ) $(LIPS_OPT) $(C_) $(LIPS_SRC)gdevlips.c

$(DD)lips4.dev : $(lipsr_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lips4 $(lipsr_)

lipsv_=$(DEVOBJ)gdevl4v.$(OBJ) $(DEVOBJ)gdevlips.$(OBJ)

$(DD)lips4v.dev : $(ECHOGS_XE) $(lipsv_) $(DD)vector.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)lips4v $(lipsv_)
	$(ADDMOD) $(DD)lips4v -include $(GLD)vector

$(DEVOBJ)gdevl4v.$(OBJ) : $(LIPS_SRC)gdevl4v.c $(LIPS_SRC)gdevlips.h $(GDEV)\
 $(math__h) $(gscspace_h) $(gsutil_h) $(gsparam_h) $(gsmatrix_h) $(gdevvec_h)\
 $(ghost_h) $(gzstate_h) $(igstate_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevl4v.$(OBJ) $(LIPS_OPT) $(C_)\
 $(LIPS_SRC)gdevl4v.c

### --------------- Some extra devices: lips2p, bjc880j ---------------- ###

$(DD)lips2p.dev : $(lipsr_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lips2p $(lipsr_)

$(DD)bjc880j.dev : $(lipsr_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjc880j $(lipsr_)


### ------------ ALPS MD series printer devices ------------ ###

md2k_=$(DEVOBJ)gdevmd2k.$(OBJ)

$(DD)md2k.dev : $(md2k_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)md2k $(md2k_)

$(DD)md5k.dev : $(md2k_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)md5k $(md2k_)

$(DEVOBJ)gdevmd2k.$(OBJ) : $(CONTRIBSRC)gdevmd2k.c $(PDEVH) $(gsparam_h) \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevmd2k.$(OBJ) $(C_) $(CONTRIBSRC)gdevmd2k.c


### ----------------- The Okidata OkiPage 4w+ device ------------------- ###

oki4w_=$(DEVOBJ)gdevop4w.$(OBJ)
$(DD)oki4w.dev : $(oki4w_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)oki4w $(oki4w_)

# Author: Ivan Schreter (ivan@shadow.sk)
# Printer: Okidata OkiPage 4w+
$(DEVOBJ)gdevop4w.$(OBJ) : $(CONTRIBSRC)gdevop4w.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevop4w.$(OBJ) $(C_) $(CONTRIBSRC)gdevop4w.c


### ---------------- OpenPrinting Vector Printer Driver ---------------- ###
###                                                                      ###
OPVP_DIR=$(CONTRIBSRC)opvp
OPVP_SRC=$(OPVP_DIR)$(D)
OPVP_OPT=-DGS_VERSION_MAJOR=$(GS_VERSION_MAJOR) $(I_)$(PSSRC)$(_I)

opvp_=$(DEVOBJ)gdevopvp.$(OBJ)

$(DEVOBJ)gdevopvp.$(OBJ) : $(OPVP_SRC)gdevopvp.c $(OPVP_SRC)opvp_common.h\
 $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevopvp.$(OBJ) $(OPVP_OPT) $(C_) $(OPVP_SRC)gdevopvp.c

$(DD)opvp.dev : $(opvp_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)opvp $(opvp_)

$(DD)oprp.dev : $(opvp_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)oprp $(opvp_)


### Driver "pcl3" for various PCL-3+ printers #################################
#
# Author: Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
#         Germany, e-mail: Martin.Lottermoser@t-online.de.
#
# Read the file README in the pcl3 distribution for general installation
# instructions. If you don't have a distribution, visit
# http://home.t-online.de/home/Martin.Lottermoser/pcl3.html.
#
# If you are including both, hpdj and pcl3, in the compilation, you should
# disable the rules for pclcomp.$(OBJ) and pagecount.$(OBJ) in the hpdj
# section. The versions of these files contained in the pcl3 distribution are
# binary compatible with what hpdj 2.6 expects.
#
# You can add compilation options to the variable "pcl3_options" below.
#
#   NDEBUG
#	Define this to remove certain runtime checks from the code which might
#	then become slightly faster. Don't define this for pcl3 beta releases
#	or ghostscript test releases.
#
#   PCL3_MEDIA_FILE
#	Define this to be a C string containing the absolute path name of a
#	media configuration file if you want the "unspec" subdevice to use this
#	file instead of its default HP DeskJet 850C/855C/870C/890C media
#	configuration. An example definition for UNIX systems is:
#
#	    pcl3_options=-DPCL3_MEDIA_FILE='"$(gsdatadir)/pcl3.mcf"'
#
#       A detailed description of the meaning and the syntax of this file can
#       be found in the reference page gs-pcl3(1). Briefly, it tells the
#	driver which media sizes your printer supports and how. You can
#       override this at run time with the option "-sMediaConfigurationFile".
#
# The variable "eprn_fs_options" is intended for optimization options used only
# when compiling eprnfs.c. Choose the combination producing the fastest code.
# You should leave this variable empty unless (a) you definitely know which
# options produce the fastest code from eprnfs.c, and (b) ghostscript does not
# use the same options anyway.
#
# You should also look at the section below containing the rules for the *.dev
# files in order to find out which values you can add to the DEVICE_DEVS*
# variables in the platform-specific make file. You should always add at least
# the "pcl3" device but you may add others; the latter are shortcuts to the
# subdevices of pcl3.
#
#
# Remarks for Microsoft Windows
# =============================
# When compiling on Microsoft Windows and unless you are using Cygwin, you must
# define the preprocessor symbol "EPRN_NO_PAGECOUNTFILE" and either remove the
# dependency of pcl3 on pagecount.obj or replace pagecount.c with an empty
# file.
#
###############################################################################

# Compilation options
pcl3_options=
eprn_fs_options=

# Version of the pcl3 rules: $Revision: 1.29 $.

#==============================================================================

# eprn rules
# This is the part you need if you are developing another eprn-based driver.

# Complete set of compilation options
eprn_opts=-DGS_REVISION=$(GS_REVISION) $(pcl3_options) $(I_)$(PSSRCDIR)$(_I)

# Directory with eprn's source code as seen from the gs directory. The name
# does not include a trailing path separator.
eprn_dir=$(CONTRIBSRC)pcl3$(D)eprn
# Same as above but with a separator
eprn_src=$(eprn_dir)$(D)

# List of all eprn object files
eprn_=$(DEVOBJ)gdeveprn.$(OBJ) $(DEVOBJ)pagecount.$(OBJ) \
  $(DEVOBJ)mediasize.$(OBJ) $(DEVOBJ)eprnparm.$(OBJ) $(DEVOBJ)eprnrend.$(OBJ) \
  $(DEVOBJ)eprnfs.$(OBJ)

# List of eprn headers needed when gdeveprn.h is included:
eprn_headers=$(eprn_src)mediasize.h $(eprn_src)gdeveprn.h $(PDEVH)

#------------------------------------------------------------------------------

# Rules for individual files

$(DEVOBJ)mediasize.$(OBJ) : $(std_h) \
  $(eprn_src)mediasize.c $(eprn_src)mediasize.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_src)mediasize.c

$(DEVOBJ)gdeveprn.$(OBJ) : $(eprn_headers) $(eprn_src)pagecount.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_src)gdeveprn.c

$(DEVOBJ)eprnparm.$(OBJ) : $(eprn_src)eprnparm.c $(eprn_headers) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_src)eprnparm.c

$(DEVOBJ)eprnrend.$(OBJ) : $(eprn_src)eprnrend.c $(eprn_headers) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_src)eprnrend.c

$(DEVOBJ)eprnfs.$(OBJ) : $(eprn_src)eprnfs.c $(eprn_headers) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_fs_options) \
	  $(eprn_src)eprnfs.c

# File also used by hpdj:
$(DEVOBJ)pagecount.$(OBJ) : $(std_h) \
  $(eprn_src)pagecount.c $(eprn_src)pagecount.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(eprn_opts) $(eprn_src)pagecount.c

#==============================================================================

# pcl3 rules

# Complete set of compilation options
pcl3_opts=-DGS_REVISION=$(GS_REVISION) -I$(eprn_dir) $(pcl3_options)

# Root directory of pcl3 distribution as seen from the gs directory. The
# variable includes a trailing path separator.
pcl3_root=$(CONTRIBSRC)pcl3$(D)

# Directory with pcl3's source code (name includes a trailing path separator)
pcl3_src=$(pcl3_root)src$(D)

# Headers used by most pcl3 files
pcl3_headers=$(eprn_headers) \
  $(pcl3_src)pclgen.h $(pcl3_src)pclcap.h $(pcl3_src)pclsize.h

$(pcl3_src)pclgen.h:$(gp_h)

# List of all object files needed for pcl3
pcl3_=$(eprn_) $(DEVOBJ)gdevpcl3.$(OBJ) $(DEVOBJ)pclcap.$(OBJ) \
  $(DEVOBJ)pclgen.$(OBJ) $(DEVOBJ)pclcomp.$(OBJ) $(DEVOBJ)pclsize.$(OBJ)

#------------------------------------------------------------------------------

# Rules for individual files

$(DEVOBJ)pclgen.$(OBJ) : $(pcl3_src)pclgen.c $(pcl3_src)pclgen.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(pcl3_opts) $(pcl3_src)pclgen.c

$(DEVOBJ)pclsize.$(OBJ) : $(pcl3_src)pclsize.c $(eprn_src)mediasize.h \
	  $(pcl3_src)pclsize.h $(pcl3_src)pclgen.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(pcl3_opts) $(pcl3_src)pclsize.c

$(DEVOBJ)pclcap.$(OBJ) : $(pcl3_src)pclcap.c $(pcl3_headers) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(pcl3_opts) $(pcl3_src)pclcap.c

$(DEVOBJ)gdevpcl3.$(OBJ) : $(pcl3_src)gdevpcl3.c $(pcl3_headers) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(pcl3_opts) $(pcl3_src)gdevpcl3.c

# File also used by hpdj:
$(DEVOBJ)pclcomp.$(OBJ) : $(pcl3_src)pclcomp.c $(pcl3_src)pclgen.h \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(C_) $(O_)$@ $(pcl3_opts) $(pcl3_src)pclcomp.c

#------------------------------------------------------------------------------

# Device rules. The targets here (e.g., "$(DD)pcl3.dev") are the values you
# can add to the DEVICE_DEVS* variables in the platform-specific make file.

# The generic pcl3 device with selectable subdevices
$(DD)pcl3.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pcl3 $(pcl3_)

# Fixed devices for specific printers
$(DD)hpdjplus.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdjplus $(pcl3_)
$(DD)hpdjportable.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdjportable $(pcl3_)
$(DD)hpdj310.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj310 $(pcl3_)
$(DD)hpdj320.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj320 $(pcl3_)
$(DD)hpdj340.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj340 $(pcl3_)
$(DD)hpdj400.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj400 $(pcl3_)
$(DD)hpdj500.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj500 $(pcl3_)
$(DD)hpdj500c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj500c $(pcl3_)
$(DD)hpdj510.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj510 $(pcl3_)
$(DD)hpdj520.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj520 $(pcl3_)
$(DD)hpdj540.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj540 $(pcl3_)
$(DD)hpdj550c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj550c $(pcl3_)
$(DD)hpdj560c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj560c $(pcl3_)
$(DD)hpdj600.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj600 $(pcl3_)
$(DD)hpdj660c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj660c $(pcl3_)
$(DD)hpdj670c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj670c $(pcl3_)
$(DD)hpdj680c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj680c $(pcl3_)
$(DD)hpdj690c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj690c $(pcl3_)
$(DD)hpdj850c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj850c $(pcl3_)
$(DD)hpdj855c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj855c $(pcl3_)
$(DD)hpdj870c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj870c $(pcl3_)
$(DD)hpdj890c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj890c $(pcl3_)
$(DD)hpdj1120c.dev : $(pcl3_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hpdj1120c $(pcl3_)

#------------------------------------------------------------------------------

# The pcl3opts command.
# This is deliberately not included in the normal compilation because (a) the
# rule is unlikely to work and (b) the code is unlikely to compile on any but
# UNIX systems.

pcl3opts: $(BINDIR)$(D)pcl3opts$(XE) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
pcl3opts_=$(pcl3_src)pcl3opts.c $(pcl3_src)pclscan.c $(eprn_src)mediasize.c \
	$(pcl3_src)pclsize.c

$(BINDIR)$(D)pcl3opts$(XE): $(pcl3opts_) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(CC_) -o $@ -I$(eprn_src) $(pcl3opts_)
	gencat $(DEVOBJ)pcl3opts-en.cat $(pcl3_src)pcl3opts-en.msg
	#gencat $(DEVOBJ)pcl3opts-de.cat $(pcl3_src)pcl3opts-de.msg

#------------------------------------------------------------------------------

# Installation rules (work only for UNIX). Note that the PostScript examples
# and the PPD files are not copied.
pcl3-install:
	for f in gs-pcl3 pcl3opts; do	\
	  $(INSTALL_DATA) $(pcl3_root)doc$(D)$$f.1 $(DESTDIR)$(man1dir)/$$f.$(man1ext) \
	    || exit 1;	\
	  $(INSTALL_DATA) $(pcl3_root)doc$(D)$$f.html $(DESTDIR)$(docdir) || exit 1; done
	$(INSTALL_PROGRAM) $(BINDIR)$(D)pcl3opts$(XE) $(DESTDIR)$(bindir)


### ----------------- The Xerox XES printer device --------------------- ###

xes_=$(DEVOBJ)gdevxes.$(OBJ)
$(DD)xes.dev : $(xes_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)xes $(xes_)

# Author: Peter Flass (flass@lbdrscs.bitnet)
# Printer: Xerox XES (2700, 3700, 4045, etc.)
$(DEVOBJ)gdevxes.$(OBJ) : $(CONTRIBSRC)gdevxes.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevxes.$(OBJ) $(C_) $(CONTRIBSRC)gdevxes.c

#########################################################################
### --------------------Japanese printer addons --------------------- ###
#########################################################################

JAPDIR=$(CONTRIBSRC)japanese
JAPSRC=$(JAPDIR)$(D)

### ----------------- The NEC PC-PR201 printer device ----------------- ###

pr201_=$(DEVOBJ)gdevp201.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)pr201.dev : $(pr201_) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pr201 $(pr201_)

$(DD)pr150.dev : $(pr201_) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pr150 $(pr201_)

$(DD)pr1000.dev : $(pr201_) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pr1000 $(pr201_)

$(DD)pr1000_4.dev : $(pr201_) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pr1000_4 $(pr201_)

$(DEVOBJ)gdevp201.$(OBJ) : $(JAPSRC)gdevp201.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevp201.$(OBJ) $(C_) $(JAPSRC)gdevp201.c

### ----------------- The Star JJ-100 printer device ----------------- ###

jj100_=$(DEVOBJ)gdevj100.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)jj100.dev : $(jj100_) $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)jj100 $(jj100_)

$(DEVOBJ)gdevj100.$(OBJ) : $(JAPSRC)gdevj100.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(O_)$@ $(C_) $(JAPSRC)gdevj100.c

### ----------------- The Canon BubbleJet BJ10v device ----------------- ###

bj10v_=$(DEVOBJ)gdev10v.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)bj10v.dev : $(bj10v_) \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bj10v $(bj10v_)

$(DD)bj10vh.dev : $(bj10v_) \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bj10vh $(bj10v_)

# Uncomment the following line if you are using MS-DOS on PC9801 series.

$(DEVOBJ)gdev10v.$(OBJ) : $(JAPSRC)gdev10v.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(O_)$@ $(C_) $(JAPSRC)gdev10v.c
#	$(DEVCC) -DPC9801 $(O_)$@ $(C_) $(JAPSRC)gdev10v.c


##
## EPSON MachJet driver
##

mj700v2c_=$(DEVOBJ)gdevmjc.$(OBJ) $(HPPCL)

$(DEVOBJ)gdevmjc.$(OBJ) : $(JAPSRC)gdevmjc.c $(JAPSRC)gdevmjc.h $(PDEVH) $(gdevpcl_h) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevmjc.$(OBJ) $(C_) $(JAPSRC)gdevmjc.c

$(DD)mj700v2c.dev : $(mj700v2c_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mj700v2c $(mj700v2c_)

$(DD)mj500c.dev : $(mj700v2c_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mj500c $(mj700v2c_)

$(DD)mj6000c.dev : $(mj700v2c_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mj6000c $(mj700v2c_)

$(DD)mj8000c.dev : $(mj700v2c_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mj8000c $(mj700v2c_)

### ----------------- The Fujitsu FMPR printer device ----------------- ###

fmpr_=$(DEVOBJ)gdevfmpr.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)fmpr.dev : $(fmpr_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)fmpr $(fmpr_)

$(DEVOBJ)gdevfmpr.$(OBJ) : $(JAPSRC)gdevfmpr.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevfmpr.$(OBJ) $(C_) $(JAPSRC)gdevfmpr.c

### --------------- The Fujitsu FMLBP-2xx printer device --------------- ###

fmlbp_=$(DEVOBJ)gdevfmlbp.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)fmlbp.dev : $(fmlbp_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)fmlbp $(fmlbp_)

$(DEVOBJ)gdevfmlbp.$(OBJ) : $(JAPSRC)gdevfmlbp.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DFMLBP_NOADJUST_MARGIN $(O_)$@ $(C_) $(JAPSRC)gdevfmlbp.c

### ------ The OKI MICROLINE620CL (IPL) printer device ------- ###
#
#  by N.Tagawa
#
#  $Id: gdevml6.mak,v 1.2 1998/03/16 13:32:33 tagawa Exp $
#

ml6_=$(DEVOBJ)gdevml6.$(OBJ) $(DEVOBJ)gdevprn.$(OBJ)

$(DD)ml600.dev : $(ml6_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ml600 $(ml6_)

$(DEVOBJ)gdevml6.$(OBJ) : $(JAPSRC)gdevml6.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(O_)$@ $(C_) $(JAPSRC)gdevml6.c


#----------------------------------------------------------------------#

lbp3x0_=$(DEVOBJ)gdevlbp3.$(OBJ)

$(DD)lbp310.dev :$(lbp3x0_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lbp310 $(lbp3x0_)

$(DD)lbp320.dev :$(lbp3x0_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lbp320 $(lbp3x0_)

$(DEVOBJ)gdevlbp3.$(OBJ) : $(JAPSRC)gdevlbp3.c $(PDEVH)
	$(DEVCC) $(O_)$@ $(C_) $(JAPSRC)gdevlbp3.c

### --- The NEC NPDL language printer device ------ ###

npdl_=$(DEVOBJ)gdevnpdl.$(OBJ) $(DEVOBJ)gdevlprn.$(OBJ)

$(DEVOBJ)gdevnpdl.$(OBJ) : $(JAPSRC)gdevnpdl.c $(LIPS_SRC)gdevlprn.h $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevnpdl.$(OBJ) $(LIPS_OPT) $(C_) $(JAPSRC)gdevnpdl.c

$(DD)npdl.dev : $(npdl_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)npdl $(npdl_)

### ------- EPSON ESC/Page printer device ----------------- ###

escpage_=$(DEVOBJ)gdevespg.$(OBJ) $(DEVOBJ)gdevlprn.$(OBJ) $(DEVOBJ)gdevlips.$(OBJ)

$(DEVOBJ)gdevespg.$(OBJ) : $(JAPSRC)gdevespg.c $(LIPS_SRC)gdevlprn.h $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) -DA4 $(DEVO_)gdevespg.$(OBJ) $(LIPS_OPT) $(C_) $(JAPSRC)gdevespg.c

$(DD)escpage.dev : $(escpage_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)escpage $(escpage_)

$(DD)lp2000.dev : $(escpage_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp2000 $(escpage_)

### --- The RICOH RPDL language printer device ------ ###

rpdl_=$(DEVOBJ)gdevrpdl.$(OBJ) $(DEVOBJ)gdevlprn.$(OBJ)
$(DEVOBJ)gdevrpdl.$(OBJ) : $(JAPSRC)gdevrpdl.c $(LIPS_SRC)gdevlprn.h $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevrpdl.$(OBJ) $(LIPS_OPT) $(C_) $(JAPSRC)gdevrpdl.c

$(DD)rpdl.dev : $(rpdl_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)rpdl $(rpdl_)

### ---------- RICOH RPDL IV(600dpi) printer devices ---------- ###
#rpdl_=$(DEVOBJ)gdevrpdl.$(OBJ)
#$(DEVOBJ)gdevrpdl.$(OBJ) : $(JAPSRC)gdevrpdl.c \
#                           $(CONTRIB_MAK) $(MAKEDIRS)
#	$(DEVCC) $(DEVO_)gdevrpdl.$(OBJ) $(C_) $(JAPSRC)gdevrpdl.c
#
#$(DD)nx100f.dev : $(rpdl_) $(DD)page.dev \
#                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
#	$(SETPDEV2) $(DD)nx100f $(rpdl_)
#
#$(DD)nx100v.dev : $(rpdl_) $(DD)page.dev \
#                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
#	$(SETPDEV2) $(DD)nx100v $(rpdl_)

### ------------ The ALPS Micro Dry printer devices ------------ ###

alps_=$(DEVOBJ)gdevalps.$(OBJ)

$(DD)md50Mono.dev : $(alps_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)md50Mono $(alps_)

$(DD)md50Eco.dev : $(alps_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)md50Eco $(alps_)

$(DD)md1xMono.dev : $(alps_) $(DD)page.dev \
                           $(CONTDEV) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)md1xMono $(alps_)

$(DEVOBJ)gdevalps.$(OBJ) : $(JAPSRC)gdevalps.c $(PDEVH) \
                           $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(O_)$@ $(C_) $(JAPSRC)gdevalps.c

#########################################################################
#########################################################################

### ----------------- Additional .upp files ---------------- ###

extra-upp-install: install-libdata
	for f in $(CONTRIBSRC)uniprint$(D)*.upp; do \
	    $(INSTALL_DATA) $$f $(DESTDIR)$(gsdatadir)$(D)lib || exit 1; \
	done

### ------------ Build/Install special features ------------ ###

install-contrib-extras: extra-upp-install
