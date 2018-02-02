# Copyright (C) 2001-2018 Artifex Software, Inc.
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
#
# makefile for contributed device drivers.

# Define the name of this makefile.
CONTRIB_MAK=$(DEVSRC)contrib.mak $(TOP_MAKEFILES)

###### --------------------------- Catalog -------------------------- ######

# The following drivers are user-contributed, and maintained (if at all) by
# users.  Please report problems in these drivers to their authors, whose
# e-mail addresses appear below: do not report them to mailing lists or
# mailboxes for general Ghostscript problems.

# Displays:
#   MS-DOS (note: not usable with Desqview/X):
#	pe	Private Eye display
#   Unix and VMS:
#	sonyfb	Sony Microsystems monochrome display   [Sony only]
# Printers:
#	ap3250	Epson AP3250 printer
#	bj10e	Canon BubbleJet BJ10e
#	bj200	Canon BubbleJet BJ200; also good for BJ300 in ProPrinter mode
#		(see comments in source code)
#	bjc600   Canon Color BubbleJet BJC-600, BJC-4000 and BJC-70
#               also good for Apple printers like the StyleWriter 2x00
#	bjc800   Canon Color BubbleJet BJC-800
#	ccr     CalComp Raster format
#	cdeskjet  H-P DeskJet 500C with 1 bit/pixel color
#	cdjcolor  H-P DeskJet 500C with 24 bit/pixel color and
#		high-quality color (Floyd-Steinberg) dithering;
#		also good for DeskJet 540C and Citizen Projet IIc (-r200x300)
#	cdjmono  H-P DeskJet 500C printing black only;
#		also good for DeskJet 510, 520, and 540C (black only)
#	cdj500	H-P DeskJet 500C (same as cdjcolor)
#	cdj550	H-P DeskJet 550C/560C/660C/660Cse
#	cljet5	H-P Color LaserJet 5/5M (see below for some notes)
#	cljet5c  H-P Color LaserJet 5/5M (see below for some notes)
#	coslw2p  CoStar LabelWriter II II/Plus
#	coslwxl  CoStar LabelWriter XL
#	declj250  alternate DEC LJ250 driver
#	djet500c  H-P DeskJet 500C alternate driver
#		(does not work on 550C or 560C)
#	dnj650c  H-P DesignJet 650C
#	epson	Epson-compatible dot matrix printers (9- or 24-pin)
#	eps9mid  Epson-compatible 9-pin, interleaved lines
#		(intermediate resolution)
#	eps9high  Epson-compatible 9-pin, interleaved lines
#		(triple resolution)
#	epsonc	Epson LQ-2550 and Fujitsu 3400/2400/1200 color printers
#	hl7x0   Brother HL 720 and HL 730 (HL 760 is PCL compliant);
#		also usable with the MFC6550MC Fax Machine.
#	ibmpro  IBM 9-pin Proprinter
#	imagen	Imagen ImPress printers
#	jetp3852  IBM Jetprinter ink-jet color printer (Model #3852)
#	lbp8	Canon LBP-8II laser printer
#	lips3	Canon LIPS III laser printer in English (CaPSL) mode
#	lj250	DEC LJ250 Companion color printer
#	lj3100sw H-P LaserJet 3100 (requires installed HP-Software)
#	lj4dith  H-P LaserJet 4 with Floyd-Steinberg dithering
#	lp8000	Epson LP-8000 laser printer
#	lq850   Epson LQ850 printer at 360 x 360 DPI resolution;
#               also good for Canon BJ300 with LQ850 emulation
#	lxm5700m Lexmark 5700 monotone
#	m8510	C.Itoh M8510 printer
#	necp6	NEC P6/P6+/P60 printers at 360 x 360 DPI resolution
#	nwp533  Sony Microsystems NWP533 laser printer   [Sony only]
#	oki182	Okidata MicroLine 182
#	okiibm	Okidata MicroLine IBM-compatible printers
#	paintjet  alternate H-P PaintJet color printer
#	photoex  Epson Stylus Color Photo, Photo EX, Photo 700
#	pj	H-P PaintJet XL driver 
#	pjetxl	alternate H-P PaintJet XL driver
#	pjxl	H-P PaintJet XL color printer
#	pjxl300  H-P PaintJet XL300 color printer;
#		also good for PaintJet 1200C and CopyJet
#	r4081	Ricoh 4081 laser printer
#	sj48	StarJet 48 inkjet printer
#	st800	Epson Stylus 800 printer
#	stcolor	Epson Stylus Color
#	t4693d2  Tektronix 4693d color printer, 2 bits per R/G/B component
#	t4693d4  Tektronix 4693d color printer, 4 bits per R/G/B component
#	t4693d8  Tektronix 4693d color printer, 8 bits per R/G/B component
#	tek4696  Tektronix 4695/4696 inkjet plotter
#	uniprint  Unified printer driver -- Configurable Color ESC/P-,
#		ESC/P2-, HP-RTL/PCL mono/color driver
# Fax systems:
#	cfax	SFF format for CAPI fax interface
#	dfaxhigh  DigiBoard, Inc.'s DigiFAX software format (high resolution)
#	dfaxlow  DigiFAX low (normal) resolution
# Other raster file formats and devices:
#	cif	CIF file format for VLSI
#	inferno  Inferno bitmaps
#	mgrmono  1-bit monochrome MGR devices
#	mgrgray2  2-bit gray scale MGR devices
#	mgrgray4  4-bit gray scale MGR devices
#	mgrgray8  8-bit gray scale MGR devices
#	mgr4	4-bit (VGA) color MGR devices
#	mgr8	8-bit color MGR devices

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

###### ------------------- MS-DOS display devices ------------------- ######

### ---------------------- The Private Eye display ---------------------- ###
### Note: this driver was contributed by a user:                          ###
###   please contact narf@media-lab.media.mit.edu if you have questions.  ###

pe_=$(DEVOBJ)gdevpe.$(OBJ)
$(DD)pe.dev : $(pe_) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)pe $(pe_)

$(DEVOBJ)gdevpe.$(OBJ) : $(DEVSRC)gdevpe.c $(GDEV) $(memory__h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpe.$(OBJ) $(C_) $(DEVSRC)gdevpe.c

###### ----------------------- Other displays ------------------------ ######

### ------------------- Sony NeWS frame buffer device ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Mike Smolenski (mike@intertech.com) if you have questions.     ###

# This is implemented as a 'printer' device.
sonyfb_=$(DEVOBJ)gdevsnfb.$(OBJ)
$(DD)sonyfb.dev : $(sonyfb_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)sonyfb $(sonyfb_)

$(DEVOBJ)gdevsnfb.$(OBJ) : $(DEVSRC)gdevsnfb.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevsnfb.$(OBJ) $(C_) $(DEVSRC)gdevsnfb.c

###### --------------- Memory-buffered printer devices --------------- ######

### ------------ The Canon BubbleJet BJ10e and BJ200 devices ------------ ###

bj10e_=$(DEVOBJ)gdevbj10.$(OBJ)

$(DD)bj10e.dev : $(bj10e_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bj10e $(bj10e_)

$(DD)bj200.dev : $(bj10e_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bj200 $(bj10e_)

$(DEVOBJ)gdevbj10.$(OBJ) : $(DEVSRC)gdevbj10.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbj10.$(OBJ) $(C_) $(DEVSRC)gdevbj10.c

### ------------- The CalComp Raster Format ----------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Ernst Muellner (ernst.muellner@oenzl.siemens.de) if you have    ###
###       questions.                                                      ###

ccr_=$(DEVOBJ)gdevccr.$(OBJ)
$(DD)ccr.dev : $(ccr_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ccr $(ccr_)

$(DEVOBJ)gdevccr.$(OBJ) : $(DEVSRC)gdevccr.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevccr.$(OBJ) $(C_) $(DEVSRC)gdevccr.c

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
###   The BJC600/BJC4000, BJC800, and ESCP were originally contributed    ###
###       by yves.arrouye@usa.net, but he no longer answers questions     ###
###       about them.                                                     ###

cdeskjet_=$(DEVOBJ)gdevcdj.$(OBJ) $(HPPCL)

$(DD)cdeskjet.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cdeskjet $(cdeskjet_)

$(DD)cdjcolor.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cdjcolor $(cdeskjet_)

$(DD)cdjmono.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cdjmono $(cdeskjet_)

$(DD)cdj500.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cdj500 $(cdeskjet_)

$(DD)cdj550.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cdj550 $(cdeskjet_)

$(DD)declj250.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)declj250 $(cdeskjet_)

$(DD)dnj650c.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)dnj650c $(cdeskjet_)

$(DD)lj4dith.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj4dith $(cdeskjet_)

$(DD)pj.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pj $(cdeskjet_)

$(DD)pjxl.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pjxl $(cdeskjet_)

# Note: the pjxl300 driver also works for the CopyJet.
$(DD)pjxl300.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pjxl300 $(cdeskjet_)

# Note: the BJC600 driver also works for the BJC4000.
$(DD)bjc600.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjc600 $(cdeskjet_)

$(DD)bjc800.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)bjc800 $(cdeskjet_)

$(DD)escp.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)escp $(cdeskjet_)

# NB: you can also customise the build if required, using
# -DBitsPerPixel=<number> if you wish the default to be other than 24
# for the generic drivers (cdj500, cdj550, pjxl300, pjtest, pjxltest).

gdevbjc_h=$(DEVSRC)gdevbjc.h

$(DEVOBJ)gdevcdj.$(OBJ) : $(DEVSRC)gdevcdj.c $(std_h) $(PDEVH)\
 $(gsparam_h) $(gsstate_h) $(gxlum_h)\
 $(gdevbjc_h) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcdj.$(OBJ) $(C_) $(DEVSRC)gdevcdj.c

djet500c_=$(DEVOBJ)gdevdjtc.$(OBJ) $(HPPCL)
$(DD)djet500c.dev : $(djet500c_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)djet500c $(djet500c_)

$(DEVOBJ)gdevdjtc.$(OBJ) : $(DEVSRC)gdevdjtc.c $(PDEVH) $(malloc__h) $(gdevpcl_h) \
 $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdjtc.$(OBJ) $(C_) $(DEVSRC)gdevdjtc.c

### -------------------- The H-P Color LaserJet 5/5M -------------------- ###

### There are two different drivers for this device.
### For questions about the cljet5/cljet5pr (more general) driver, contact
###	Jan Stoeckenius <jan@orimp.com>
### For questions about the cljet5c (simple) driver, contact
###	Henry Stiles <henrys@meerkat.dimensional.com>
### Note that this is a long-edge-feed device, so the default page size is
### wider than it is high.  To print portrait pages, specify the page size
### explicitly, e.g. -c letter or -c a4 on the command line.

cljet5_=$(DEVOBJ)gdevclj.$(OBJ) $(HPPCL)

$(DD)cljet5.dev : $(DEVS_MAK) $(cljet5_) $(GLD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cljet5 $(cljet5_)

# The cljet5pr driver has hacks for trying to handle page rotation.
# The hacks only work with one special PCL interpreter.  Don't use it!
$(DD)cljet5pr.dev : $(DEVS_MAK) $(cljet5_) $(GLD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cljet5pr $(cljet5_)

$(DEVOBJ)gdevclj.$(OBJ) : $(DEVSRC)gdevclj.c $(math__h) $(PDEVH)\
 $(gx_h) $(gsparam_h) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevclj.$(OBJ) $(C_) $(DEVSRC)gdevclj.c

cljet5c_=$(DEVOBJ)gdevcljc.$(OBJ) $(HPPCL)
$(DD)cljet5c.dev : $(DEVS_MAK) $(cljet5c_) $(GLD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cljet5c $(cljet5c_)

$(DEVOBJ)gdevcljc.$(OBJ) : $(DEVSRC)gdevcljc.c $(math__h) $(PDEVH) $(gdevpcl_h) \
 $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcljc.$(OBJ) $(C_) $(DEVSRC)gdevcljc.c

### --------------- The H-P LaserJet 3100 software device --------------- ###

### NOTE: This driver requires installed HP-Software to print.            ###
###       It can be used with smbclient to print from an UNIX box to a    ###
###       LaserJet 3100 printer attached to a MS-Windows box.             ###
### NOTE: this driver was contributed by a user: please contact           ###
###       Ulrich Schmid (uschmid@mail.hh.provi.de) if you have questions. ###

lj3100sw_=$(DEVOBJ)gdevl31s.$(OBJ) $(DEVOBJ)gdevmeds.$(OBJ)
$(DD)lj3100sw.dev : $(lj3100sw_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj3100sw $(lj3100sw_)

gdevmeds_h=$(DEVSRC)gdevmeds.h $(gdevprn_h)

$(DEVOBJ)gdevl31s.$(OBJ) : $(DEVSRC)gdevl31s.c $(gdevmeds_h) $(PDEVH) \
 $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevl31s.$(OBJ) $(C_) $(DEVSRC)gdevl31s.c

$(DEVOBJ)gdevmeds.$(OBJ) : $(DEVSRC)gdevmeds.c $(AK) $(gdevmeds_h) \
 $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevmeds.$(OBJ) $(C_) $(DEVSRC)gdevmeds.c

### ------ CoStar LabelWriter II II/Plus device ------ ###
### Contributed by Mike McCauley mikem@open.com.au     ###

coslw_=$(DEVOBJ)gdevcslw.$(OBJ)

$(DD)coslw2p.dev : $(coslw_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)coslw2p $(coslw_)

$(DD)coslwxl.dev : $(coslw_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)coslwxl $(coslw_)

$(DEVOBJ)gdevcslw.$(OBJ) : $(DEVSRC)gdevcslw.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcslw.$(OBJ) $(C_) $(DEVSRC)gdevcslw.c

### ----------------- The generic Epson printer device ----------------- ###
### Note: most of this code was contributed by users.  Please contact    ###
###       the following people if you have questions:                    ###
###   eps9mid - Guenther Thomsen (thomsen@cs.tu-berlin.de)               ###
###   eps9high - David Wexelblat (dwex@mtgzfs3.att.com)                  ###
###   ibmpro - James W. Birdsall (jwbirdsa@picarefy.picarefy.com)        ###

epson_=$(DEVOBJ)gdevepsn.$(OBJ)

$(DD)epson.dev : $(epson_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epson $(epson_)

$(DD)eps9mid.dev : $(epson_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)eps9mid $(epson_)

$(DD)eps9high.dev : $(epson_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)eps9high $(epson_)

$(DEVOBJ)gdevepsn.$(OBJ) : $(DEVSRC)gdevepsn.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevepsn.$(OBJ) $(C_) $(DEVSRC)gdevepsn.c

### ----------------- The IBM Proprinter printer device ---------------- ###

$(DD)ibmpro.dev : $(epson_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ibmpro $(epson_)

### -------------- The Epson LQ-2550 color printer device -------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Dave St. Clair (dave@exlog.com) if you have questions.         ###

epsonc_=$(DEVOBJ)gdevepsc.$(OBJ)
$(DD)epsonc.dev : $(epsonc_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)epsonc $(epsonc_)

$(DEVOBJ)gdevepsc.$(OBJ) : $(DEVSRC)gdevepsc.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevepsc.$(OBJ) $(C_) $(DEVSRC)gdevepsc.c

### ------------- The Epson ESC/P 2 language printer devices ------------- ###
### Note: these drivers were contributed by users.                         ###
### For questions about the Stylus 800 and AP3250 drivers, please contact  ###
###        Richard Brown (rab@tauon.ph.unimelb.edu.au).                    ###
### For questions about the Stylus Color drivers, please contact           ###
###        Gunther Hess (gunther@elmos.de).                                ###

ESCP2=$(DEVOBJ)gdevescp.$(OBJ)

$(DEVOBJ)gdevescp.$(OBJ) : $(DEVSRC)gdevescp.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevescp.$(OBJ) $(C_) $(DEVSRC)gdevescp.c

$(DD)ap3250.dev : $(ESCP2) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ap3250 $(ESCP2)

$(DD)st800.dev : $(ESCP2) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)st800 $(ESCP2)

stcolor1_=$(DEVOBJ)gdevstc.$(OBJ) $(DEVOBJ)gdevstc1.$(OBJ) $(DEVOBJ)gdevstc2.$(OBJ)
stcolor2_=$(DEVOBJ)gdevstc3.$(OBJ) $(DEVOBJ)gdevstc4.$(OBJ)
$(DD)stcolor.dev : $(stcolor1_) $(stcolor2_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)stcolor $(stcolor1_)
	$(ADDMOD) $(DD)stcolor -obj $(stcolor2_)

gdevstc_h=$(DEVSRC)gdevstc.h $(gdevprn_h) $(gsparam_h) $(gsstate_h)

$(DEVOBJ)gdevstc.$(OBJ) : $(DEVSRC)gdevstc.c $(gdevstc_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevstc.$(OBJ) $(C_) $(DEVSRC)gdevstc.c

$(DEVOBJ)gdevstc1.$(OBJ) : $(DEVSRC)gdevstc1.c $(gdevstc_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevstc1.$(OBJ) $(C_) $(DEVSRC)gdevstc1.c

$(DEVOBJ)gdevstc2.$(OBJ) : $(DEVSRC)gdevstc2.c $(gdevstc_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevstc2.$(OBJ) $(C_) $(DEVSRC)gdevstc2.c

$(DEVOBJ)gdevstc3.$(OBJ) : $(DEVSRC)gdevstc3.c $(gdevstc_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevstc3.$(OBJ) $(C_) $(DEVSRC)gdevstc3.c

$(DEVOBJ)gdevstc4.$(OBJ) : $(DEVSRC)gdevstc4.c $(gdevstc_h) $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevstc4.$(OBJ) $(C_) $(DEVSRC)gdevstc4.c

### --------------- Ugly/Update -> Unified Printer Driver ---------------- ###
### For questions about this driver, please contact:                       ###
###        Gunther Hess (gunther@elmos.de)                                 ###

uniprint_=$(DEVOBJ)gdevupd.$(OBJ)
$(DD)uniprint.dev : $(uniprint_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)uniprint $(uniprint_)

$(DEVOBJ)gdevupd.$(OBJ) : $(DEVSRC)gdevupd.c $(PDEVH) $(gsparam_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevupd.$(OBJ) $(C_) $(DEVSRC)gdevupd.c

### ------------ The H-P PaintJet color printer device ----------------- ###
### Note: this driver also supports the DEC LJ250 color printer, which   ###
###       has a PaintJet-compatible mode, and the PaintJet XL.           ###
### If you have questions about the XL, please contact Rob Reiss         ###
###       (rob@moray.berkeley.edu).                                      ###

PJET=$(DEVOBJ)gdevpjet.$(OBJ) $(HPPCL)

$(DEVOBJ)gdevpjet.$(OBJ) : $(DEVSRC)gdevpjet.c $(PDEVH) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpjet.$(OBJ) $(C_) $(DEVSRC)gdevpjet.c

$(DD)lj250.dev : $(PJET) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj250 $(PJET)

$(DD)paintjet.dev : $(PJET) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)paintjet $(PJET)

$(DD)pjetxl.dev : $(PJET) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)pjetxl $(PJET)

###--------------------- The Brother HL 7x0 printer --------------------- ### 
### Note: this driver was contributed by users: please contact            ###
###       Pierre-Olivier Gaillard (pierre.gaillard@hol.fr)                ###
###         for questions about the basic driver;                         ###
###       Ross Martin (ross@ross.interwrx.com, martin@walnut.eas.asu.edu) ###
###         for questions about usage with the MFC6550MC Fax Machine.     ###

hl7x0_=$(DEVOBJ)gdevhl7x.$(OBJ)
$(DD)hl7x0.dev : $(hl7x0_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)hl7x0 $(hl7x0_)

$(DEVOBJ)gdevhl7x.$(OBJ) : $(DEVSRC)gdevhl7x.c $(PDEVH) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevhl7x.$(OBJ) $(C_) $(DEVSRC)gdevhl7x.c

### -------------- Imagen ImPress Laser Printer device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Alan Millar (AMillar@bolis.sf-bay.org) if you have questions.  ###
### Set USE_BYTE_STREAM if using parallel interface;                     ###
### Don't set it if using 'ipr' spooler (default).                       ###
### You may also add -DA4 if needed for A4 paper.			 ###

imagen_=$(DEVOBJ)gdevimgn.$(OBJ)
$(DD)imagen.dev : $(imagen_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)imagen $(imagen_)

# Uncomment the first line for the ipr spooler, the second line for parallel.
IMGN_OPT=
#IMGN_OPT=-DUSE_BYTE_STREAM
$(DEVOBJ)gdevimgn.$(OBJ) : $(DEVSRC)gdevimgn.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(IMGN_OPT) $(DEVO_)gdevimgn.$(OBJ) $(C_) $(DEVSRC)gdevimgn.c

### ------- The IBM 3852 JetPrinter color inkjet printer device -------- ###
### Note: this driver was contributed by users: please contact           ###
###       Kevin Gift (kgift@draper.com) if you have questions.           ###
### Note that the paper size that can be addressed by the graphics mode  ###
###   used in this driver is fixed at 7-1/2 inches wide (the printable   ###
###   width of the jetprinter itself.)                                   ###

jetp3852_=$(DEVOBJ)gdev3852.$(OBJ)
$(DD)jetp3852.dev : $(jetp3852_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)jetp3852 $(jetp3852_)

$(DEVOBJ)gdev3852.$(OBJ) : $(DEVSRC)gdev3852.c $(PDEVH) $(gdevpcl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdev3852.$(OBJ) $(C_) $(DEVSRC)gdev3852.c

### ---------- The Canon LBP-8II and LIPS III printer devices ---------- ###
### Note: these drivers were contributed by users.                       ###
### For questions about these drivers, please contact                    ###
###       Lauri Paatero, lauri.paatero@paatero.pp.fi                     ###

lbp8_=$(DEVOBJ)gdevlbp8.$(OBJ)
$(DD)lbp8.dev : $(lbp8_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lbp8 $(lbp8_)

$(DD)lips3.dev : $(lbp8_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lips3 $(lbp8_)

$(DEVOBJ)gdevlbp8.$(OBJ) : $(DEVSRC)gdevlbp8.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlbp8.$(OBJ) $(C_) $(DEVSRC)gdevlbp8.c

### -------------- The Epson LP-8000 laser printer device -------------- ###
### Note: this driver was contributed by a user: please contact Oleg     ###
###       Oleg Fat'yanov <faty1@rlem.titech.ac.jp> if you have questions.###

lp8000_=$(DEVOBJ)gdevlp8k.$(OBJ)
$(DD)lp8000.dev : $(lp8000_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lp8000 $(lp8000_)

$(DEVOBJ)gdevlp8k.$(OBJ) : $(DEVSRC)gdevlp8k.c $(PDEVH)  $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlp8k.$(OBJ) $(C_) $(DEVSRC)gdevlp8k.c

### -------------- The C.Itoh M8510 printer device --------------------- ###
### Note: this driver was contributed by a user: please contact Bob      ###
###       Smith <bob@snuffy.penfield.ny.us> if you have questions.       ###

m8510_=$(DEVOBJ)gdev8510.$(OBJ)
$(DD)m8510.dev : $(m8510_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)m8510 $(m8510_)

$(DEVOBJ)gdev8510.$(OBJ) : $(DEVSRC)gdev8510.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdev8510.$(OBJ) $(C_) $(DEVSRC)gdev8510.c

### -------------- 24pin Dot-matrix printer with 360DPI ---------------- ###
### Note: this driver was contributed by users.  Please contact:         ###
###    Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de) for        ###
###      questions about the NEC P6;                                     ###
###    Christian Felsch (felsch@tu-harburg.d400.de) for                  ###
###      questions about the Epson LQ850.                                ###

dm24_=$(DEVOBJ)gdevdm24.$(OBJ)
$(DD)necp6.dev : $(dm24_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)necp6 $(dm24_)

$(DD)lq850.dev : $(dm24_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lq850 $(dm24_)

$(DEVOBJ)gdevdm24.$(OBJ) : $(DEVSRC)gdevdm24.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdm24.$(OBJ) $(C_) $(DEVSRC)gdevdm24.c

### ----------------- Lexmark 5700 printer ----------------------------- ###
### Note: this driver was contributed by users.  Please contact:         ###
###   Stephen Taylor (setaylor@ma.ultranet.com) if you have questions.   ###

lxm5700m_=$(DEVOBJ)gdevlxm.$(OBJ)
$(DD)lxm5700m.dev : $(lxm5700m_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lxm5700m $(lxm5700m_)

$(DEVOBJ)gdevlxm.$(OBJ) : $(DEVSRC)gdevlxm.c $(PDEVH) $(gsparams_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlxm.$(OBJ) $(C_) $(DEVSRC)gdevlxm.c

### ----------------- The Okidata MicroLine 182 device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Maarten Koning (smeg@bnr.ca) if you have questions.            ###

oki182_=$(DEVOBJ)gdevo182.$(OBJ)
$(DD)oki182.dev : $(oki182_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)oki182 $(oki182_)

$(DEVOBJ)gdevo182.$(OBJ) : $(DEVSRC)gdevo182.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevo182.$(OBJ) $(C_) $(DEVSRC)gdevo182.c

### ------------- The Okidata IBM compatible printer device ------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Charles Mack (chasm@netcom.com) if you have questions.         ###

okiibm_=$(DEVOBJ)gdevokii.$(OBJ)
$(DD)okiibm.dev : $(okiibm_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)okiibm $(okiibm_)

$(DEVOBJ)gdevokii.$(OBJ) : $(DEVSRC)gdevokii.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevokii.$(OBJ) $(C_) $(DEVSRC)gdevokii.c

### ------------------ The Epson Stylus Photo devices ------------------ ###
### This driver was contributed by a user: please contact                ###
###	Zoltan Kocsi (zoltan@bendor.com.au) if you have questions.       ###

photoex_=$(DEVOBJ)gdevphex.$(OBJ)
$(DD)photoex.dev : $(photoex_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)photoex $(photoex_)

$(DEVOBJ)gdevphex.$(OBJ) : $(DEVSRC)gdevphex.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevphex.$(OBJ) $(C_) $(DEVSRC)gdevphex.c

### ------------- The Ricoh 4081 laser printer device ------------------ ###
### Note: this driver was contributed by users:                          ###
###       please contact kdw@oasis.icl.co.uk if you have questions.      ###

r4081_=$(DEVOBJ)gdev4081.$(OBJ)
$(DD)r4081.dev : $(r4081_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)r4081 $(r4081_)


$(DEVOBJ)gdev4081.$(OBJ) : $(DEVSRC)gdev4081.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdev4081.$(OBJ) $(C_) $(DEVSRC)gdev4081.c

### -------------------- Sony NWP533 printer device -------------------- ###
### Note: this driver was contributed by a user: please contact Tero     ###
###       Kivinen (kivinen@joker.cs.hut.fi) if you have questions.       ###

nwp533_=$(DEVOBJ)gdevn533.$(OBJ)
$(DD)nwp533.dev : $(nwp533_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)nwp533 $(nwp533_)

$(DEVOBJ)gdevn533.$(OBJ) : $(DEVSRC)gdevn533.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevn533.$(OBJ) $(C_) $(DEVSRC)gdevn533.c

### ----------------- The StarJet SJ48 device -------------------------- ###
### Note: this driver was contributed by a user: if you have questions,  ###
###	                      .                                          ###
###       please contact Mats Akerblom (f86ma@dd.chalmers.se).           ###

sj48_=$(DEVOBJ)gdevsj48.$(OBJ)
$(DD)sj48.dev : $(sj48_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)sj48 $(sj48_)

$(DEVOBJ)gdevsj48.$(OBJ) : $(DEVSRC)gdevsj48.c $(PDEVH)
	$(DEVCC) $(DEVO_)gdevsj48.$(OBJ) $(C_) $(DEVSRC)gdevsj48.c

### ----------------- Tektronix 4396d color printer -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karl Hakimian (hakimian@haney.eecs.wsu.edu)                    ###
###       if you have questions.                                         ###

t4693d_=$(DEVOBJ)gdev4693.$(OBJ)
$(DD)t4693d2.dev : $(t4693d_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)t4693d2 $(t4693d_)

$(DD)t4693d4.dev : $(t4693d_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)t4693d4 $(t4693d_)

$(DD)t4693d8.dev : $(t4693d_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)t4693d8 $(t4693d_)

$(DEVOBJ)gdev4693.$(OBJ) : $(DEVSRC)gdev4693.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdev4693.$(OBJ) $(C_) $(DEVSRC)gdev4693.c

### -------------------- Tektronix ink-jet printers -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karsten Spang (spang@nbivax.nbi.dk) if you have questions.     ###

tek4696_=$(DEVOBJ)gdevtknk.$(OBJ)
$(DD)tek4696.dev : $(tek4696_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)tek4696 $(tek4696_)

$(DEVOBJ)gdevtknk.$(OBJ) : $(DEVSRC)gdevtknk.c $(PDEVH) $(malloc__h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevtknk.$(OBJ) $(C_) $(DEVSRC)gdevtknk.c

###### ------------------------- Fax devices ------------------------- ######

### ------------------------- CAPI fax devices -------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Peter Schaefer <peter.schaefer@gmx.de> if you have questions.   ###

cfax_=$(DEVOBJ)gdevcfax.$(OBJ)

$(DD)cfax.dev : $(cfax_) $(DD)fax.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)cfax $(cfax_)
	$(ADDMOD) $(DD)cfax -include $(DD)fax

$(DEVOBJ)gdevcfax.$(OBJ) : $(DEVSRC)gdevcfax.c $(PDEVH)\
 $(gdevfax_h) $(scfx_h) $(strimpl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcfax.$(OBJ) $(C_) $(DEVSRC)gdevcfax.c

### ------------------------- The DigiFAX device ------------------------ ###
###    This driver outputs images in a format suitable for use with       ###
###    DigiBoard, Inc.'s DigiFAX software.  Use -sDEVICE=dfaxhigh for     ###
###    high resolution output, -sDEVICE=dfaxlow for normal output.        ###
### Note: this driver was contributed by a user: please contact           ###
###       Rick Richardson (rick@digibd.com) if you have questions.        ###

dfax_=$(DEVOBJ)gdevdfax.$(OBJ)

$(DD)dfaxlow.dev : $(dfax_) $(DD)tfax.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)dfaxlow $(dfax_)
	$(ADDMOD) $(DEVGEN)dfaxlow -include $(DD)tfax

$(DD)dfaxhigh.dev : $(dfax_) $(DD)tfax.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)dfaxhigh $(dfax_)
	$(ADDMOD) $(DEVGEN)dfaxhigh -include $(DD)tfax

$(DEVOBJ)gdevdfax.$(OBJ) : $(DEVSRC)gdevdfax.c $(PDEVH)\
 $(gdevfax_h) $(gdevtfax_h) $(scfx_h) $(strimpl_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdfax.$(OBJ) $(C_) $(DEVSRC)gdevdfax.c

###### --------------------- Raster file formats --------------------- ######

### -------------------- The CIF file format for VLSI ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Frederic Petrot (petrot@masi.ibp.fr) if you have questions.    ###

cif_=$(DEVOBJ)gdevcif.$(OBJ)
$(DD)cif.dev : $(cif_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)cif $(cif_)

$(DEVOBJ)gdevcif.$(OBJ) : $(DEVSRC)gdevcif.c $(PDEVH) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcif.$(OBJ) $(C_) $(DEVSRC)gdevcif.c

### ------------------------- Inferno bitmaps -------------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Russ Cox <rsc@plan9.bell-labs.com> if you have questions.      ###

inferno_=$(DEVOBJ)gdevifno.$(OBJ)
$(DD)inferno.dev : $(inferno_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)inferno $(inferno_)

$(DEVOBJ)gdevifno.$(OBJ) : $(DEVSRC)gdevifno.c $(PDEVH)\
 $(gsparam_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevifno.$(OBJ) $(C_) $(DEVSRC)gdevifno.c

### --------------------------- MGR devices ---------------------------- ###
### Note: these drivers were contributed by a user: please contact       ###
###       Carsten Emde (ce@ceag.ch) if you have questions.               ###

MGR=$(DEVOBJ)gdevmgr.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ)

gdevmgr_h= $(DEVSRC)gdevmgr.h

$(DEVOBJ)gdevmgr.$(OBJ) : $(DEVSRC)gdevmgr.c $(PDEVH)\
 $(gdevmgr_h) $(gdevpccm_h) $(CONTRIB_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevmgr.$(OBJ) $(C_) $(DEVSRC)gdevmgr.c

$(DD)mgrmono.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgrmono $(MGR)

$(DD)mgrgray2.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgrgray2 $(MGR)

$(DD)mgrgray4.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgrgray4 $(MGR)

$(DD)mgrgray8.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgrgray8 $(MGR)

$(DD)mgr4.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgr4 $(MGR)

$(DD)mgr8.dev : $(MGR) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)mgr8 $(MGR)

#########################################################################
### --------------------Japanese printer addons --------------------- ###
#########################################################################

### These drivers are based on patches on existing device drivers in the
### src/ directory, therefore they are not in addons/

$(DD)ljet4pjl.dev : $(HPMONO) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ljet4pjl $(HPMONO)

$(DD)lj4dithp.dev : $(cdeskjet_) $(DD)page.dev $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj4dithp $(cdeskjet_)

$(DD)dj505j.dev : $(cdeskjet_) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)dj505j $(cdeskjet_)

$(DD)picty180.dev : $(cdeskjet_) $(CONTRIB_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)picty180 $(cdeskjet_)

#########################################################################
