#    Copyright (C) 1989, 1996, 1997, 1998 Aladdin Enterprises.  All rights reserved.
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

# $Id$
# makefile for contributed device drivers.

# Define the name of this makefile.
CONTRIB_MAK=$(GLSRC)contrib.mak

###### --------------------------- Catalog -------------------------- ######

# The following drivers are user-contributed, and maintained (if at all)
# by users.  Please do not ask Aladdin about problems with these drivers.

# MS-DOS displays (note: not usable with Desqview/X):
# *	herc	Hercules Graphics display   [MS-DOS only]
# *	pe	Private Eye display
#   Unix and VMS:
# *	att3b1	AT&T 3b1/Unixpc monochrome display   [3b1 only]
# *	sonyfb	Sony Microsystems monochrome display   [Sony only]
# *	sunview  SunView window system   [SunOS only]
#   Platform-independent:
# *	sxlcrt	CRT sixels, e.g. for VT240-like terminals
# Printers:
# *	ap3250	Epson AP3250 printer
# *	appledmp  Apple Dot Matrix Printer (should also work with Imagewriter)
# *	bj10e	Canon BubbleJet BJ10e
# *	bj200	Canon BubbleJet BJ200
# *	bjc600   Canon Color BubbleJet BJC-600, BJC-4000 and BJC-70
#               also good for Apple printers like the StyleWriter 2x00
# *	bjc800   Canon Color BubbleJet BJC-800
# *     ccr     CalComp Raster format
# *	cdeskjet  H-P DeskJet 500C with 1 bit/pixel color
# *	cdjcolor  H-P DeskJet 500C with 24 bit/pixel color and
#		high-quality color (Floyd-Steinberg) dithering;
#		also good for DeskJet 540C and Citizen Projet IIc (-r200x300)
# *	cdjmono  H-P DeskJet 500C printing black only;
#		also good for DeskJet 510, 520, and 540C (black only)
# *	cdj500	H-P DeskJet 500C (same as cdjcolor)
# *	cdj550	H-P DeskJet 550C/560C/660C/660Cse
# *	cp50	Mitsubishi CP50 color printer
# *	declj250  alternate DEC LJ250 driver
# *	djet500c  H-P DeskJet 500C alternate driver
#		(does not work on 550C or 560C)
# *	dnj650c  H-P DesignJet 650C
#	epson	Epson-compatible dot matrix printers (9- or 24-pin)
# *	eps9mid  Epson-compatible 9-pin, interleaved lines
#		(intermediate resolution)
# *	eps9high  Epson-compatible 9-pin, interleaved lines
#		(triple resolution)
# *	epsonc	Epson LQ-2550 and Fujitsu 3400/2400/1200 color printers
# *	hl7x0   Brother HL 720 and HL 730 (HL 760 is PCL compliant)
# *	ibmpro  IBM 9-pin Proprinter
# *	imagen	Imagen ImPress printers
# *	iwhi	Apple Imagewriter in high-resolution mode
# *	iwlo	Apple Imagewriter in low-resolution mode
# *	iwlq	Apple Imagewriter LQ in 320 x 216 dpi mode
# *	jetp3852  IBM Jetprinter ink-jet color printer (Model #3852)
# *	la50	DEC LA50 printer
# *	la70	DEC LA70 printer
# *	la70t	DEC LA70 printer with low-resolution text enhancement
# *	la75	DEC LA75 printer
# *	la75plus DEC LA75plus printer
# *	lbp8	Canon LBP-8II laser printer
# *	lips3	Canon LIPS III laser printer in English (CaPSL) mode
# *	ln03	DEC LN03 printer
# *	lj250	DEC LJ250 Companion color printer
# +	lj4dith  H-P LaserJet 4 with Floyd-Steinberg dithering
# *	lp8000	Epson LP-8000 laser printer
# *     lq850   Epson LQ850 printer at 360 x 360 DPI resolution;
#               also good for Canon BJ300 with LQ850 emulation
# *	m8510	C.Itoh M8510 printer
# *	necp6	NEC P6/P6+/P60 printers at 360 x 360 DPI resolution
# *	nwp533  Sony Microsystems NWP533 laser printer   [Sony only]
# *	oki182	Okidata MicroLine 182
# *	okiibm	Okidata MicroLine IBM-compatible printers
# *	paintjet  alternate H-P PaintJet color printer
# *	pj	H-P PaintJet XL driver 
# *	pjetxl	alternate H-P PaintJet XL driver
# *	pjxl	H-P PaintJet XL color printer
# *	pjxl300  H-P PaintJet XL300 color printer;
#		also good for PaintJet 1200C and CopyJet
# *	r4081	Ricoh 4081 laser printer
# *	sj48	StarJet 48 inkjet printer
# *	sparc	SPARCprinter
# *	st800	Epson Stylus 800 printer
# *	stcolor	Epson Stylus Color
# *	t4693d2  Tektronix 4693d color printer, 2 bits per R/G/B component
# *	t4693d4  Tektronix 4693d color printer, 4 bits per R/G/B component
# *	t4693d8  Tektronix 4693d color printer, 8 bits per R/G/B component
# *	tek4696  Tektronix 4695/4696 inkjet plotter
# *	uniprint  Unified printer driver -- Configurable Color ESC/P-,
#		ESC/P2-, HP-RTL/PCL mono/color driver
# *	xes	Xerox XES printers (2700, 3700, 4045, etc.)
# Fax systems:
# *	dfaxhigh  DigiBoard, Inc.'s DigiFAX software format (high resolution)
# *	dfaxlow  DigiFAX low (normal) resolution
# Other raster file formats and devices:
# *	cif	CIF file format for VLSI
# *	inferno  Inferno bitmaps
# *	mgrmono  1-bit monochrome MGR devices
# *	mgrgray2  2-bit gray scale MGR devices
# *	mgrgray4  4-bit gray scale MGR devices
# *	mgrgray8  8-bit gray scale MGR devices
# *	mgr4	4-bit (VGA) color MGR devices
# *	mgr8	8-bit color MGR devices
# *	sgirgb	SGI RGB pixmap format

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

###### ------------------- MS-DOS display devices ------------------- ######

### ------------------- The Hercules Graphics display ------------------- ###

herc_=$(GLOBJ)gdevherc.$(OBJ)
herc.dev: $(herc_)
	$(SETDEV) herc $(herc_)

$(GLOBJ)gdevherc.$(OBJ): $(GLSRC)gdevherc.c $(GDEV) $(dos__h)\
 $(gsmatrix_h) $(gxbitmap_h)
	$(GLCC) $(GLO_)gdevherc.$(OBJ) $(C_) $(GLSRC)gdevherc.c

### ---------------------- The Private Eye display ---------------------- ###
### Note: this driver was contributed by a user:                          ###
###   please contact narf@media-lab.media.mit.edu if you have questions.  ###

pe_=$(GLOBJ)gdevpe.$(OBJ)
pe.dev: $(pe_)
	$(SETDEV) pe $(pe_)

$(GLOBJ)gdevpe.$(OBJ): $(GLSRC)gdevpe.c $(GDEV) $(memory__h)
	$(GLCC) $(GLO_)gdevpe.$(OBJ) $(C_) $(GLSRC)gdevpe.c

###### ----------------------- Other displays ------------------------ ######

### -------------- The AT&T 3b1 Unixpc monochrome display --------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Andy Fyfe (andy@cs.caltech.edu) if you have questions.          ###

att3b1_=$(GLOBJ)gdev3b1.$(OBJ)
att3b1.dev: $(att3b1_)
	$(SETDEV) att3b1 $(att3b1_)

$(GLOBJ)gdev3b1.$(OBJ): $(GLSRC)gdev3b1.c $(GDEV)
	$(GLCC) $(GLO_)gdev3b1.$(OBJ) $(C_) $(GLSRC)gdev3b1.c

### ------------------- Sony NeWS frame buffer device ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Mike Smolenski (mike@intertech.com) if you have questions.     ###

# This is implemented as a 'printer' device.
sonyfb_=$(GLOBJ)gdevsnfb.$(OBJ)
sonyfb.dev: $(sonyfb_) page.dev
	$(SETPDEV) sonyfb $(sonyfb_)

$(GLOBJ)gdevsnfb.$(OBJ): $(GLSRC)gdevsnfb.c $(PDEVH)
	$(GLCC) $(GLO_)gdevsnfb.$(OBJ) $(C_) $(GLSRC)gdevsnfb.c

### ------------------------ The SunView device ------------------------ ###
### Note: this driver is maintained by a user: if you have questions,    ###
###       please contact Andreas Stolcke (stolcke@icsi.berkeley.edu).    ###

sunview_=$(GLOBJ)gdevsun.$(OBJ)
sunview.dev: $(sunview_)
	$(SETDEV) sunview $(sunview_)
	$(ADDMOD) sunview -lib suntool sunwindow pixrect

$(GLOBJ)gdevsun.$(OBJ): $(GLSRC)gdevsun.c $(GDEV) $(malloc__h)\
 $(gscdefs_h) $(gserrors_h) $(gsmatrix_h)
	$(GLCC) $(GLO_)gdevsun.$(OBJ) $(C_) $(GLSRC)gdevsun.c

### ------------------------- DEC sixel displays ------------------------ ###
### Note: this driver was contributed by a user: please contact           ###
###   Phil Keegstra (keegstra@tonga.gsfc.nasa.gov) if you have questions. ###

# This is a "printer" device, but it probably shouldn't be.
# I don't know why the implementor chose to do it this way.
sxlcrt_=$(GLOBJ)gdevln03.$(OBJ)
sxlcrt.dev: $(sxlcrt_) page.dev
	$(SETPDEV) sxlcrt $(sxlcrt_)

###### --------------- Memory-buffered printer devices --------------- ######

### --------------------- The Apple printer devices --------------------- ###
### Note: these drivers were contributed by users.                        ###
###   If you have questions about the DMP driver, please contact          ###
###	Mark Wedel (master@cats.ucsc.edu).                                ###
###   If you have questions about the Imagewriter drivers, please contact ###
###	Jonathan Luckey (luckey@rtfm.mlb.fl.us).                          ###
###   If you have questions about the Imagewriter LQ driver, please       ###
###	contact Scott Barker (barkers@cuug.ab.ca).                        ###

appledmp_=$(GLOBJ)gdevadmp.$(OBJ)

$(GLOBJ)gdevadmp.$(OBJ): $(GLSRC)gdevadmp.c $(PDEVH)
	$(GLCC) $(GLO_)gdevadmp.$(OBJ) $(C_) $(GLSRC)gdevadmp.c

appledmp.dev: $(appledmp_) page.dev
	$(SETPDEV) appledmp $(appledmp_)

iwhi.dev: $(appledmp_) page.dev
	$(SETPDEV) iwhi $(appledmp_)

iwlo.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlo $(appledmp_)

iwlq.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlq $(appledmp_)

### ------------ The Canon BubbleJet BJ10e and BJ200 devices ------------ ###

bj10e_=$(GLOBJ)gdevbj10.$(OBJ)

bj10e.dev: $(bj10e_) page.dev
	$(SETPDEV) bj10e $(bj10e_)

bj200.dev: $(bj10e_) page.dev
	$(SETPDEV) bj200 $(bj10e_)

$(GLOBJ)gdevbj10.$(OBJ): $(GLSRC)gdevbj10.c $(PDEVH)
	$(GLCC) $(GLO_)gdevbj10.$(OBJ) $(C_) $(GLSRC)gdevbj10.c

### ------------- The CalComp Raster Format ----------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Ernst Muellner (ernst.muellner@oenzl.siemens.de) if you have    ###
###       questions.                                                      ###

ccr_=$(GLOBJ)gdevccr.$(OBJ)
ccr.dev: $(ccr_) page.dev
	$(SETPDEV) ccr $(ccr_)

$(GLOBJ)gdevccr.$(OBJ): $(GLSRC)gdevccr.c $(PDEVH)
	$(GLCC) $(GLO_)gdevccr.$(OBJ) $(C_) $(GLSRC)gdevccr.c

### The H-P DeskJet, PaintJet, and DesignJet family color printer devices.###
### Note: there are two different 500C drivers, both contributed by users.###
###   If you have questions about the djet500c driver,                    ###
###       please contact AKayser@et.tudelft.nl.                           ###
###   If you have questions about the cdj* drivers,                       ###
###       please contact g.cameron@biomed.abdn.ac.uk.                     ###
###   If you have questions about the dnj560c driver,                     ###
###       please contact koert@zen.cais.com.                              ###
###   If you have questions about the lj4dith driver,                     ###
###       please contact Eckhard.Rueggeberg@ts.go.dlr.de.                 ###
###   If you have questions about the BJC600/BJC4000, BJC800, or ESCP     ###
###       drivers, please contact yves.arrouye@usa.net.                   ###

cdeskjet_=$(GLOBJ)gdevcdj.$(OBJ) $(HPPCL)

cdeskjet.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdeskjet $(cdeskjet_)

cdjcolor.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdjcolor $(cdeskjet_)

cdjmono.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdjmono $(cdeskjet_)

cdj500.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdj500 $(cdeskjet_)

cdj550.dev: $(cdeskjet_) page.dev
	$(SETPDEV) cdj550 $(cdeskjet_)

declj250.dev: $(cdeskjet_) page.dev
	$(SETPDEV) declj250 $(cdeskjet_)

dnj650c.dev: $(cdeskjet_) page.dev
	$(SETPDEV) dnj650c $(cdeskjet_)

lj4dith.dev: $(cdeskjet_) page.dev
	$(SETPDEV) lj4dith $(cdeskjet_)

pj.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pj $(cdeskjet_)

pjxl.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pjxl $(cdeskjet_)

# Note: the pjxl300 driver also works for the CopyJet.
pjxl300.dev: $(cdeskjet_) page.dev
	$(SETPDEV) pjxl300 $(cdeskjet_)

# Note: the BJC600 driver also works for the BJC4000.
bjc600.dev: $(cdeskjet_) page.dev
	$(SETPDEV) bjc600 $(cdeskjet_)

bjc800.dev: $(cdeskjet_) page.dev
	$(SETPDEV) bjc800 $(cdeskjet_)

escp.dev: $(cdeskjet_) page.dev
	$(SETPDEV) escp $(cdeskjet_)

# NB: you can also customise the build if required, using
# -DBitsPerPixel=<number> if you wish the default to be other than 24
# for the generic drivers (cdj500, cdj550, pjxl300, pjtest, pjxltest).
$(GLOBJ)gdevcdj.$(OBJ): $(GLSRC)gdevcdj.c $(std_h) $(PDEVH) $(GLSRC)gdevbjc.h\
 $(gsparam_h) $(gsstate_h) $(gxlum_h)\
 $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevcdj.$(OBJ) $(C_) $(GLSRC)gdevcdj.c

djet500c_=$(GLOBJ)gdevdjtc.$(OBJ) $(HPPCL)
djet500c.dev: $(djet500c_) page.dev
	$(SETPDEV) djet500c $(djet500c_)

$(GLOBJ)gdevdjtc.$(OBJ): $(GLSRC)gdevdjtc.c $(PDEVH) $(malloc__h) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevdjtc.$(OBJ) $(C_) $(GLSRC)gdevdjtc.c

### -------------------- The Mitsubishi CP50 printer -------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Michael Hu (michael@ximage.com) if you have questions.          ###

cp50_=$(GLOBJ)gdevcp50.$(OBJ)
cp50.dev: $(cp50_) page.dev
	$(SETPDEV) cp50 $(cp50_)

$(GLOBJ)gdevcp50.$(OBJ): $(GLSRC)gdevcp50.c $(PDEVH)
	$(GLCC) $(GLO_)gdevcp50.$(OBJ) $(C_) $(GLSRC)gdevcp50.c

### ----------------- The generic Epson printer device ----------------- ###
### Note: most of this code was contributed by users.  Please contact    ###
###       the following people if you have questions:                    ###
###   eps9mid - Guenther Thomsen (thomsen@cs.tu-berlin.de)               ###
###   eps9high - David Wexelblat (dwex@mtgzfs3.att.com)                  ###
###   ibmpro - James W. Birdsall (jwbirdsa@picarefy.picarefy.com)        ###

epson_=$(GLOBJ)gdevepsn.$(OBJ)

epson.dev: $(epson_) page.dev
	$(SETPDEV) epson $(epson_)

eps9mid.dev: $(epson_) page.dev
	$(SETPDEV) eps9mid $(epson_)

eps9high.dev: $(epson_) page.dev
	$(SETPDEV) eps9high $(epson_)

$(GLOBJ)gdevepsn.$(OBJ): $(GLSRC)gdevepsn.c $(PDEVH)
	$(GLCC) $(GLO_)gdevepsn.$(OBJ) $(C_) $(GLSRC)gdevepsn.c

### ----------------- The IBM Proprinter printer device ---------------- ###

ibmpro.dev: $(epson_) page.dev
	$(SETPDEV) ibmpro $(epson_)

### -------------- The Epson LQ-2550 color printer device -------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Dave St. Clair (dave@exlog.com) if you have questions.         ###

epsonc_=$(GLOBJ)gdevepsc.$(OBJ)
epsonc.dev: $(epsonc_) page.dev
	$(SETPDEV) epsonc $(epsonc_)

$(GLOBJ)gdevepsc.$(OBJ): $(GLSRC)gdevepsc.c $(PDEVH)
	$(GLCC) $(GLO_)gdevepsc.$(OBJ) $(C_) $(GLSRC)gdevepsc.c

### ------------- The Epson ESC/P 2 language printer devices ------------- ###
### Note: these drivers were contributed by users.                         ###
### For questions about the Stylus 800 and AP3250 drivers, please contact  ###
###        Richard Brown (rab@tauon.ph.unimelb.edu.au).                    ###
### For questions about the Stylus Color drivers, please contact           ###
###        Gunther Hess (gunther@elmos.de).                                ###

ESCP2=$(GLOBJ)gdevescp.$(OBJ)

$(GLOBJ)gdevescp.$(OBJ): $(GLSRC)gdevescp.c $(PDEVH)
	$(GLCC) $(GLO_)gdevescp.$(OBJ) $(C_) $(GLSRC)gdevescp.c

ap3250.dev: $(ESCP2) page.dev
	$(SETPDEV) ap3250 $(ESCP2)

st800.dev: $(ESCP2) page.dev
	$(SETPDEV) st800 $(ESCP2)

stcolor1_=$(GLOBJ)gdevstc.$(OBJ) $(GLOBJ)gdevstc1.$(OBJ) $(GLOBJ)gdevstc2.$(OBJ)
stcolor2_=$(GLOBJ)gdevstc3.$(OBJ) $(GLOBJ)gdevstc4.$(OBJ)
stcolor.dev: $(stcolor1_) $(stcolor2_) page.dev
	$(SETPDEV) stcolor $(stcolor1_)
	$(ADDMOD) stcolor -obj $(stcolor2_)

gdevstc_h=$(GLSRC)gdevstc.h $(gdevprn_h) $(gsparam_h) $(gsstate_h)

$(GLOBJ)gdevstc.$(OBJ): $(GLSRC)gdevstc.c $(gdevstc_h) $(PDEVH)
	$(GLCC) $(GLO_)gdevstc.$(OBJ) $(C_) $(GLSRC)gdevstc.c

$(GLOBJ)gdevstc1.$(OBJ): $(GLSRC)gdevstc1.c $(gdevstc_h) $(PDEVH)
	$(GLCC) $(GLO_)gdevstc1.$(OBJ) $(C_) $(GLSRC)gdevstc1.c

$(GLOBJ)gdevstc2.$(OBJ): $(GLSRC)gdevstc2.c $(gdevstc_h) $(PDEVH)
	$(GLCC) $(GLO_)gdevstc2.$(OBJ) $(C_) $(GLSRC)gdevstc2.c

$(GLOBJ)gdevstc3.$(OBJ): $(GLSRC)gdevstc3.c $(gdevstc_h) $(PDEVH)
	$(GLCC) $(GLO_)gdevstc3.$(OBJ) $(C_) $(GLSRC)gdevstc3.c

$(GLOBJ)gdevstc4.$(OBJ): $(GLSRC)gdevstc4.c $(gdevstc_h) $(PDEVH)
	$(GLCC) $(GLO_)gdevstc4.$(OBJ) $(C_) $(GLSRC)gdevstc4.c

### --------------- Ugly/Update -> Unified Printer Driver ---------------- ###
### For questions about this driver, please contact:                       ###
###        Gunther Hess (gunther@elmos.de)                                 ###

uniprint_=$(GLOBJ)gdevupd.$(OBJ)
uniprint.dev: $(uniprint_) page.dev
	$(SETPDEV) uniprint $(uniprint_)

$(GLOBJ)gdevupd.$(OBJ): $(GLSRC)gdevupd.c $(PDEVH) $(gsparam_h)
	$(GLCC) $(GLO_)gdevupd.$(OBJ) $(C_) $(GLSRC)gdevupd.c

### -------------- cdj850 - HP 850c Driver under development ------------- ###
### Since this driver is in the development-phase it is not distributed    ###
### with ghostscript, but it is available via anonymous ftp from:          ###
###                        ftp://bonk.ethz.ch                              ###
### For questions about this driver, please contact:                       ###
###       Uli Wortmann (E-Mail address inside the driver-package)          ###

cdeskjet8_=$(GLOBJ)gdevcd8.$(OBJ) $(HPPCL)

cdj850.dev: $(cdeskjet8_) page.dev
	$(SETPDEV) cdj850 $(cdeskjet8_)

### ------------ The H-P PaintJet color printer device ----------------- ###
### Note: this driver also supports the DEC LJ250 color printer, which   ###
###       has a PaintJet-compatible mode, and the PaintJet XL.           ###
### If you have questions about the XL, please contact Rob Reiss         ###
###       (rob@moray.berkeley.edu).                                      ###

PJET=$(GLOBJ)gdevpjet.$(OBJ) $(HPPCL)

$(GLOBJ)gdevpjet.$(OBJ): $(GLSRC)gdevpjet.c $(PDEVH) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevpjet.$(OBJ) $(C_) $(GLSRC)gdevpjet.c

lj250.dev: $(PJET) page.dev
	$(SETPDEV) lj250 $(PJET)

paintjet.dev: $(PJET) page.dev
	$(SETPDEV) paintjet $(PJET)

pjetxl.dev: $(PJET) page.dev
	$(SETPDEV) pjetxl $(PJET)

###--------------------- The Brother HL 7x0 printer --------------------- ### 
###                    This driver was contributed by a user :            ###
###  Please contact Pierre-Olivier Gaillard (pierre.gaillard@hol.fr)      ###
### if you have any questions.                                            ###

hl7x0_=$(GLOBJ)gdevhl7x.$(OBJ)
hl7x0.dev: $(hl7x0_) page.dev
	$(SETPDEV) hl7x0 $(hl7x0_)

$(GLOBJ)gdevhl7x.$(OBJ): $(GLSRC)gdevhl7x.c $(PDEVH) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevhl7x.$(OBJ) $(C_) $(GLSRC)gdevhl7x.c

### -------------- Imagen ImPress Laser Printer device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Alan Millar (AMillar@bolis.sf-bay.org) if you have questions.  ###
### Set USE_BYTE_STREAM if using parallel interface;                     ###
### Don't set it if using 'ipr' spooler (default).                       ###
### You may also add -DA4 if needed for A4 paper.			 ###

imagen_=$(GLOBJ)gdevimgn.$(OBJ)
imagen.dev: $(imagen_) page.dev
	$(SETPDEV) imagen $(imagen_)

# Uncomment the first line for the ipr spooler, the second line for parallel.
IMGN_OPT=
#IMGN_OPT=-DUSE_BYTE_STREAM
$(GLOBJ)gdevimgn.$(OBJ): $(GLSRC)gdevimgn.c $(PDEVH)
	$(GLCC) $(IMGN_OPT) $(GLO_)gdevimgn.$(OBJ) $(C_) $(GLSRC)gdevimgn.c

### ------- The IBM 3852 JetPrinter color inkjet printer device -------- ###
### Note: this driver was contributed by users: please contact           ###
###       Kevin Gift (kgift@draper.com) if you have questions.           ###
### Note that the paper size that can be addressed by the graphics mode  ###
###   used in this driver is fixed at 7-1/2 inches wide (the printable   ###
###   width of the jetprinter itself.)                                   ###

jetp3852_=$(GLOBJ)gdev3852.$(OBJ)
jetp3852.dev: $(jetp3852_) page.dev
	$(SETPDEV) jetp3852 $(jetp3852_)

$(GLOBJ)gdev3852.$(OBJ): $(GLSRC)gdev3852.c $(PDEVH) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdev3852.$(OBJ) $(C_) $(GLSRC)gdev3852.c

### ---------- The Canon LBP-8II and LIPS III printer devices ---------- ###
### Note: these drivers were contributed by users.                       ###
### For questions about these drivers, please contact                    ###
###       Lauri Paatero, lauri.paatero@paatero.pp.fi                     ###

lbp8_=$(GLOBJ)gdevlbp8.$(OBJ)
lbp8.dev: $(lbp8_) page.dev
	$(SETPDEV) lbp8 $(lbp8_)

lips3.dev: $(lbp8_) page.dev
	$(SETPDEV) lips3 $(lbp8_)

$(GLOBJ)gdevlbp8.$(OBJ): $(GLSRC)gdevlbp8.c $(PDEVH)
	$(GLCC) $(GLO_)gdevlbp8.$(OBJ) $(C_) $(GLSRC)gdevlbp8.c

### ----------- The DEC LN03/LA50/LA70/LA75 printer devices ------------ ###
### Note: this driver was contributed by users: please contact           ###
###       Ulrich Mueller (ulm@vsnhd1.cern.ch) if you have questions.     ###
### For questions about LA50 and LA75, please contact                    ###
###       Ian MacPhedran (macphed@dvinci.USask.CA).                      ###
### For questions about the LA70, please contact                         ###
###       Bruce Lowekamp (lowekamp@csugrad.cs.vt.edu).                   ###
### For questions about the LA75plus, please contact                     ###
###       Andre' Beck (Andre_Beck@IRS.Inf.TU-Dresden.de).                ###

ln03_=$(GLOBJ)gdevln03.$(OBJ)
ln03.dev: $(ln03_) page.dev
	$(SETPDEV) ln03 $(ln03_)

la50.dev: $(ln03_) page.dev
	$(SETPDEV) la50 $(ln03_)

la70.dev: $(ln03_) page.dev
	$(SETPDEV) la70 $(ln03_)

la75.dev: $(ln03_) page.dev
	$(SETPDEV) la75 $(ln03_)

la75plus.dev: $(ln03_) page.dev
	$(SETPDEV) la75plus $(ln03_)

$(GLOBJ)gdevln03.$(OBJ): $(GLSRC)gdevln03.c $(PDEVH)
	$(GLCC) $(GLO_)gdevln03.$(OBJ) $(C_) $(GLSRC)gdevln03.c

# LA70 driver with low-resolution text enhancement.

la70t_=$(GLOBJ)gdevla7t.$(OBJ)
la70t.dev: $(la70t_) page.dev
	$(SETPDEV) la70t $(la70t_)

$(GLOBJ)gdevla7t.$(OBJ): $(GLSRC)gdevla7t.c $(PDEVH)
	$(GLCC) $(GLO_)gdevla7t.$(OBJ) $(C_) $(GLSRC)gdevla7t.c

### -------------- The Epson LP-8000 laser printer device -------------- ###
### Note: this driver was contributed by a user: please contact Oleg     ###
###       Oleg Fat'yanov <faty1@rlem.titech.ac.jp> if you have questions.###

lp8000_=$(GLOBJ)gdevlp8k.$(OBJ)
lp8000.dev: $(lp8000_) page.dev
	$(SETPDEV) lp8000 $(lp8000_)

$(GLOBJ)gdevlp8k.$(OBJ): $(GLSRC)gdevlp8k.c $(PDEVH)
	$(GLCC) $(GLO_)gdevlp8k.$(OBJ) $(C_) $(GLSRC)gdevlp8k.c

### -------------- The C.Itoh M8510 printer device --------------------- ###
### Note: this driver was contributed by a user: please contact Bob      ###
###       Smith <bob@snuffy.penfield.ny.us> if you have questions.       ###

m8510_=$(GLOBJ)gdev8510.$(OBJ)
m8510.dev: $(m8510_) page.dev
	$(SETPDEV) m8510 $(m8510_)

$(GLOBJ)gdev8510.$(OBJ): $(GLSRC)gdev8510.c $(PDEVH)
	$(GLCC) $(GLO_)gdev8510.$(OBJ) $(C_) $(GLSRC)gdev8510.c

### -------------- 24pin Dot-matrix printer with 360DPI ---------------- ###
### Note: this driver was contributed by users.  Please contact:         ###
###    Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de) for        ###
###      questions about the NEC P6;                                     ###
###    Christian Felsch (felsch@tu-harburg.d400.de) for                  ###
###      questions about the Epson LQ850.                                ###

dm24_=$(GLOBJ)gdevdm24.$(OBJ)
necp6.dev: $(dm24_) page.dev
	$(SETPDEV) necp6 $(dm24_)

lq850.dev: $(dm24_) page.dev
	$(SETPDEV) lq850 $(dm24_)

$(GLOBJ)gdevdm24.$(OBJ): $(GLSRC)gdevdm24.c $(PDEVH)
	$(GLCC) $(GLO_)gdevdm24.$(OBJ) $(C_) $(GLSRC)gdevdm24.c

### ----------------- The Okidata MicroLine 182 device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Maarten Koning (smeg@bnr.ca) if you have questions.            ###

oki182_=$(GLOBJ)gdevo182.$(OBJ)
oki182.dev: $(oki182_) page.dev
	$(SETPDEV) oki182 $(oki182_)

$(GLOBJ)gdevo182.$(OBJ): $(GLSRC)gdevo182.c $(PDEVH)
	$(GLCC) $(GLO_)gdevo182.$(OBJ) $(C_) $(GLSRC)gdevo182.c

### ------------- The Okidata IBM compatible printer device ------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Charles Mack (chasm@netcom.com) if you have questions.         ###

okiibm_=$(GLOBJ)gdevokii.$(OBJ)
okiibm.dev: $(okiibm_) page.dev
	$(SETPDEV) okiibm $(okiibm_)

$(GLOBJ)gdevokii.$(OBJ): $(GLSRC)gdevokii.c $(PDEVH)
	$(GLCC) $(GLO_)gdevokii.$(OBJ) $(C_) $(GLSRC)gdevokii.c

### ------------- The Ricoh 4081 laser printer device ------------------ ###
### Note: this driver was contributed by users:                          ###
###       please contact kdw@oasis.icl.co.uk if you have questions.      ###

r4081_=$(GLOBJ)gdev4081.$(OBJ)
r4081.dev: $(r4081_) page.dev
	$(SETPDEV) r4081 $(r4081_)


$(GLOBJ)gdev4081.$(OBJ): $(GLSRC)gdev4081.c $(PDEVH)
	$(GLCC) $(GLO_)gdev4081.$(OBJ) $(C_) $(GLSRC)gdev4081.c

### -------------------- Sony NWP533 printer device -------------------- ###
### Note: this driver was contributed by a user: please contact Tero     ###
###       Kivinen (kivinen@joker.cs.hut.fi) if you have questions.       ###

nwp533_=$(GLOBJ)gdevn533.$(OBJ)
nwp533.dev: $(nwp533_) page.dev
	$(SETPDEV) nwp533 $(nwp533_)

$(GLOBJ)gdevn533.$(OBJ): $(GLSRC)gdevn533.c $(PDEVH)
	$(GLCC) $(GLO_)gdevn533.$(OBJ) $(C_) $(GLSRC)gdevn533.c

### ------------------------- The SPARCprinter ------------------------- ###
### Note: this driver was contributed by users: please contact Martin    ###
###       Schulte (schulte@thp.uni-koeln.de) if you have questions.      ###
###       He would also like to hear from anyone using the driver.       ###
### Please consult the source code for additional documentation.         ###

sparc_=$(GLOBJ)gdevsppr.$(OBJ)
sparc.dev: $(sparc_) page.dev
	$(SETPDEV) sparc $(sparc_)

$(GLOBJ)gdevsppr.$(OBJ): $(GLSRC)gdevsppr.c $(PDEVH)
	$(GLCC) $(GLO_)gdevsppr.$(OBJ) $(C_) $(GLSRC)gdevsppr.c

### ----------------- The StarJet SJ48 device -------------------------- ###
### Note: this driver was contributed by a user: if you have questions,  ###
###	                      .                                          ###
###       please contact Mats Akerblom (f86ma@dd.chalmers.se).           ###

sj48_=$(GLOBJ)gdevsj48.$(OBJ)
sj48.dev: $(sj48_) page.dev
	$(SETPDEV) sj48 $(sj48_)

$(GLOBJ)gdevsj48.$(OBJ): $(GLSRC)gdevsj48.c $(PDEVH)
	$(GLCC) $(GLO_)gdevsj48.$(OBJ) $(C_) $(GLSRC)gdevsj48.c

### ----------------- Tektronix 4396d color printer -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karl Hakimian (hakimian@haney.eecs.wsu.edu)                    ###
###       if you have questions.                                         ###

t4693d_=$(GLOBJ)gdev4693.$(OBJ)
t4693d2.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d2 $(t4693d_)

t4693d4.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d4 $(t4693d_)

t4693d8.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d8 $(t4693d_)

$(GLOBJ)gdev4693.$(OBJ): $(GLSRC)gdev4693.c $(PDEVH)
	$(GLCC) $(GLO_)gdev4693.$(OBJ) $(C_) $(GLSRC)gdev4693.c

### -------------------- Tektronix ink-jet printers -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karsten Spang (spang@nbivax.nbi.dk) if you have questions.     ###

tek4696_=$(GLOBJ)gdevtknk.$(OBJ)
tek4696.dev: $(tek4696_) page.dev
	$(SETPDEV) tek4696 $(tek4696_)

$(GLOBJ)gdevtknk.$(OBJ): $(GLSRC)gdevtknk.c $(PDEVH) $(malloc__h)
	$(GLCC) $(GLO_)gdevtknk.$(OBJ) $(C_) $(GLSRC)gdevtknk.c

### ----------------- The Xerox XES printer device --------------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Peter Flass (flass@lbdrscs.bitnet) if you have questions.      ###

xes_=$(GLOBJ)gdevxes.$(OBJ)
xes.dev: $(xes_) page.dev
	$(SETPDEV) xes $(xes_)

$(GLOBJ)gdevxes.$(OBJ): $(GLSRC)gdevxes.c $(PDEVH)
	$(GLCC) $(GLO_)gdevxes.$(OBJ) $(C_) $(GLSRC)gdevxes.c

###### ------------------------- Fax devices ------------------------- ######

### ------------------------- The DigiFAX device ------------------------ ###
###    This driver outputs images in a format suitable for use with       ###
###    DigiBoard, Inc.'s DigiFAX software.  Use -sDEVICE=dfaxhigh for     ###
###    high resolution output, -sDEVICE=dfaxlow for normal output.        ###
### Note: this driver was contributed by a user: please contact           ###
###       Rick Richardson (rick@digibd.com) if you have questions.        ###

dfax_=$(GLOBJ)gdevdfax.$(OBJ) $(GLOBJ)gdevtfax.$(OBJ)

dfaxlow.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxlow $(dfax_)
	$(ADDMOD) dfaxlow -include cfe

dfaxhigh.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxhigh $(dfax_)
	$(ADDMOD) dfaxhigh -include cfe

$(GLOBJ)gdevdfax.$(OBJ): $(GLSRC)gdevdfax.c $(PDEVH) $(scfx_h) $(strimpl_h)
	$(GLCC) $(GLO_)gdevdfax.$(OBJ) $(C_) $(GLSRC)gdevdfax.c

###### --------------------- Raster file formats --------------------- ######

### -------------------- The CIF file format for VLSI ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Frederic Petrot (petrot@masi.ibp.fr) if you have questions.    ###

cif_=$(GLOBJ)gdevcif.$(OBJ)
cif.dev: $(cif_) page.dev
	$(SETPDEV) cif $(cif_)

$(GLOBJ)gdevcif.$(OBJ): $(GLSRC)gdevcif.c $(PDEVH)
	$(GLCC) $(GLO_)gdevcif.$(OBJ) $(C_) $(GLSRC)gdevcif.c

### ------------------------- Inferno bitmaps -------------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Russ Cox <rsc@plan9.bell-labs.com> if you have questions.      ###

inferno_=$(GLOBJ)gdevifno.$(OBJ)
inferno.dev: $(inferno_) page.dev
	$(SETPDEV) inferno $(inferno_)

$(GLOBJ)gdevifno.$(OBJ): $(GLSRC)gdevifno.c $(PDEVH) $(gxlum_h)
	$(GLCC) $(GLO_)gdevifno.$(OBJ) $(C_) $(GLSRC)gdevifno.c

### --------------------------- MGR devices ---------------------------- ###
### Note: these drivers were contributed by a user: please contact       ###
###       Carsten Emde (carsten@ce.pr.net.ch) if you have questions.     ###

MGR=$(GLOBJ)gdevmgr.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)

$(GLOBJ)gdevmgr.$(OBJ): $(GLSRC)gdevmgr.c $(PDEVH)\
 $(gdevpccm_h) $(GLSRC)gdevmgr.h
	$(GLCC) $(GLO_)gdevmgr.$(OBJ) $(C_) $(GLSRC)gdevmgr.c

mgrmono.dev: $(MGR) page.dev
	$(SETPDEV) mgrmono $(MGR)

mgrgray2.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray2 $(MGR)

mgrgray4.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray4 $(MGR)

mgrgray8.dev: $(MGR) page.dev
	$(SETPDEV) mgrgray8 $(MGR)

mgr4.dev: $(MGR) page.dev
	$(SETPDEV) mgr4 $(MGR)

mgr8.dev: $(MGR) page.dev
	$(SETPDEV) mgr8 $(MGR)

### -------------------------- SGI RGB pixmaps -------------------------- ###

sgirgb_=$(GLOBJ)gdevsgi.$(OBJ)
sgirgb.dev: $(sgirgb_) page.dev
	$(SETPDEV) sgirgb $(sgirgb_)

$(GLOBJ)gdevsgi.$(OBJ): $(GLSRC)gdevsgi.c $(PDEVH) $(GLSRC)gdevsgi.h
	$(GLCC) $(GLO_)gdevsgi.$(OBJ) $(C_) $(GLSRC)gdevsgi.c
