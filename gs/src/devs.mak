#    Copyright (C) 1989, 1996, 1997 Aladdin Enterprises.  All rights reserved.
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

# makefile for device drivers.

# Define the name of this makefile.
DEVS_MAK=devs.mak

###### --------------------------- Catalog -------------------------- ######

# It is possible to build configurations with an arbitrary collection of
# device drivers, although some drivers are supported only on a subset
# of the target platforms.  The currently available drivers are:

# MS-DOS displays (note: not usable with Desqview/X):
#   MS-DOS EGA and VGA:
#	ega	EGA (640x350, 16-color)
#	vga	VGA (640x480, 16-color)
#   MS-DOS SuperVGA:
# *	ali	SuperVGA using Avance Logic Inc. chipset, 256-color modes
# *	atiw	ATI Wonder SuperVGA, 256-color modes
# *	s3vga	SuperVGA using S3 86C911 chip (e.g., Diamond Stealth board)
#	svga16	Generic SuperVGA in 800x600, 16-color mode
# *	tseng	SuperVGA using Tseng Labs ET3000/4000 chips, 256-color modes
# *	tvga	SuperVGA using Trident chipset, 256-color modes
#   ****** NOTE: The vesa device does not work with the Watcom (32-bit MS-DOS)
#   ****** compiler or executable.
#	vesa	SuperVGA with VESA standard API driver
#   MS-DOS other:
#	bgi	Borland Graphics Interface (CGA)  [MS-DOS only]
# *	herc	Hercules Graphics display   [MS-DOS only]
# *	pe	Private Eye display
# Other displays:
#   MS Windows:
#	mswindll  Microsoft Windows 3.1 DLL  [MS Windows only]
#	mswinprn  Microsoft Windows 3.0, 3.1 DDB printer  [MS Windows only]
#	mswinpr2  Microsoft Windows 3.0, 3.1 DIB printer  [MS Windows only]
#   OS/2:
# *	os2pm	OS/2 Presentation Manager   [OS/2 only]
# *	os2dll	OS/2 DLL bitmap             [OS/2 only]
# *	os2prn	OS/2 printer                [OS/2 only]
#   Unix and VMS:
#   ****** NOTE: For direct frame buffer addressing under SCO Unix or Xenix,
#   ****** edit the definition of EGAVGA below.
# *	att3b1	AT&T 3b1/Unixpc monochrome display   [3b1 only]
# *	lvga256  Linux vgalib, 256-color VGA modes  [Linux only]
# *	sonyfb	Sony Microsystems monochrome display   [Sony only]
# *	sunview  SunView window system   [SunOS only]
# +	vgalib	Linux PC with VGALIB   [Linux only]
#	x11	X Windows version 11, release >=4   [Unix and VMS only]
#	x11alpha  X Windows masquerading as a device with alpha capability
#	x11cmyk  X Windows masquerading as a 1-bit-per-plane CMYK device
#	x11gray2  X Windows as a 2-bit gray-scale device
#	x11mono  X Windows masquerading as a black-and-white device
#   Platform-independent:
# *	sxlcrt	CRT sixels, e.g. for VT240-like terminals
# Printers:
# *	ap3250	Epson AP3250 printer
# *	appledmp  Apple Dot Matrix Printer (should also work with Imagewriter)
#	bj10e	Canon BubbleJet BJ10e
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
# +	deskjet  H-P DeskJet and DeskJet Plus
#	djet500  H-P DeskJet 500; use -r600 for DJ 600 series
# *	djet500c  H-P DeskJet 500C alternate driver
#		(does not work on 550C or 560C)
# *	dnj650c  H-P DesignJet 650C
#	epson	Epson-compatible dot matrix printers (9- or 24-pin)
# *	eps9mid  Epson-compatible 9-pin, interleaved lines
#		(intermediate resolution)
# *	eps9high  Epson-compatible 9-pin, interleaved lines
#		(triple resolution)
# *	epsonc	Epson LQ-2550 and Fujitsu 3400/2400/1200 color printers
# *	ibmpro  IBM 9-pin Proprinter
# *	imagen	Imagen ImPress printers
# *	iwhi	Apple Imagewriter in high-resolution mode
# *	iwlo	Apple Imagewriter in low-resolution mode
# *	iwlq	Apple Imagewriter LQ in 320 x 216 dpi mode
# *	jetp3852  IBM Jetprinter ink-jet color printer (Model #3852)
# +	laserjet  H-P LaserJet
# *	la50	DEC LA50 printer
# *	la70	DEC LA70 printer
# *	la70t	DEC LA70 printer with low-resolution text enhancement
# *	la75	DEC LA75 printer
# *	la75plus DEC LA75plus printer
# *	lbp8	Canon LBP-8II laser printer
# *	lips3	Canon LIPS III laser printer in English (CaPSL) mode
# *	ln03	DEC LN03 printer
# *	lj250	DEC LJ250 Companion color printer
# +	ljet2p	H-P LaserJet IId/IIp/III* with TIFF compression
# +	ljet3	H-P LaserJet III* with Delta Row compression
# +	ljet3d	H-P LaserJet IIID with duplex capability
# +	ljet4	H-P LaserJet 4 (defaults to 600 dpi)
# +	lj4dith  H-P LaserJet 4 with Floyd-Steinberg dithering
# +	ljetplus  H-P LaserJet Plus
#	lj5mono  H-P LaserJet 5 & 6 family (PCL XL), bitmap:
#		see below for restrictions & advice
#	lj5gray  H-P LaserJet 5 & 6 family, gray-scale bitmap;
#		see below for restrictions & advice
# *	lp2563	H-P 2563B line printer
# *	lp8000	Epson LP-8000 laser printer
# *     lq850   Epson LQ850 printer at 360 x 360 DPI resolution;
#               also good for Canon BJ300 with LQ850 emulation
# *	m8510	C.Itoh M8510 printer
# *	necp6	NEC P6/P6+/P60 printers at 360 x 360 DPI resolution
# *	nwp533  Sony Microsystems NWP533 laser printer   [Sony only]
# *	oce9050  OCE 9050 printer
# *	oki182	Okidata MicroLine 182
# *	okiibm	Okidata MicroLine IBM-compatible printers
# *	paintjet  alternate H-P PaintJet color printer
# *	pj	H-P PaintJet XL driver 
# *	pjetxl	alternate H-P PaintJet XL driver
# *	pjxl	H-P PaintJet XL color printer
# *	pjxl300  H-P PaintJet XL300 color printer;
#		also good for PaintJet 1200C
#	(pxlmono) H-P black-and-white PCL XL printers (LaserJet 5 and 6 family)
#	(pxlcolor) H-P color PCL XL printers (none available yet)
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
# Fax file format:
#   ****** NOTE: all of these drivers adjust the page size to match
#   ****** one of the three CCITT standard sizes (U.S. letter with A4 width,
#   ****** A4, or B4).
#	faxg3	Group 3 fax, with EOLs but no header or EOD
#	faxg32d  Group 3 2-D fax, with EOLs but no header or EOD
#	faxg4	Group 4 fax, with EOLs but no header or EOD
#	tiffcrle  TIFF "CCITT RLE 1-dim" (= Group 3 fax with no EOLs)
#	tiffg3	TIFF Group 3 fax (with EOLs)
#	tiffg32d  TIFF Group 3 2-D fax
#	tiffg4	TIFF Group 4 fax
# High-level file formats:
#	epswrite  EPS output (like PostScript Distillery)
#	pdfwrite  PDF output (like Adobe Acrobat Distiller)
#	pswrite  PostScript output (like PostScript Distillery)
#	pxlmono  Black-and-white PCL XL
#	pxlcolor  Color PCL XL
# Other raster file formats and devices:
#	bit	Plain bits, monochrome
#	bitrgb	Plain bits, RGB
#	bitcmyk  Plain bits, CMYK
#	bmpmono	Monochrome MS Windows .BMP file format
#	bmp16	4-bit (EGA/VGA) .BMP file format
#	bmp256	8-bit (256-color) .BMP file format
#	bmp16m	24-bit .BMP file format
#	cgmmono  Monochrome (black-and-white) CGM -- LOW LEVEL OUTPUT ONLY
#	cgm8	8-bit (256-color) CGM -- DITTO
#	cgm24	24-bit color CGM -- DITTO
# *	cif	CIF file format for VLSI
#	jpeg	JPEG format, RGB output
#	jpeggray  JPEG format, gray output
#	miff24	ImageMagick MIFF format, 24-bit direct color, RLE compressed
# *	mgrmono  1-bit monochrome MGR devices
# *	mgrgray2  2-bit gray scale MGR devices
# *	mgrgray4  4-bit gray scale MGR devices
# *	mgrgray8  8-bit gray scale MGR devices
# *	mgr4	4-bit (VGA) color MGR devices
# *	mgr8	8-bit color MGR devices
#	pcxmono	PCX file format, monochrome (1-bit black and white)
#	pcxgray	PCX file format, 8-bit gray scale
#	pcx16	PCX file format, 4-bit planar (EGA/VGA) color
#	pcx256	PCX file format, 8-bit chunky color
#	pcx24b	PCX file format, 24-bit color (3 8-bit planes)
#	pcxcmyk PCX file format, 4-bit chunky CMYK color
#	pbm	Portable Bitmap (plain format)
#	pbmraw	Portable Bitmap (raw format)
#	pgm	Portable Graymap (plain format)
#	pgmraw	Portable Graymap (raw format)
#	pgnm	Portable Graymap (plain format), optimizing to PBM if possible
#	pgnmraw	Portable Graymap (raw format), optimizing to PBM if possible
#	pnm	Portable Pixmap (plain format) (RGB), optimizing to PGM or PBM
#		 if possible
#	pnmraw	Portable Pixmap (raw format) (RGB), optimizing to PGM or PBM
#		 if possible
#	ppm	Portable Pixmap (plain format) (RGB)
#	ppmraw	Portable Pixmap (raw format) (RGB)
#	pkm	Portable inKmap (plain format) (4-bit CMYK => RGB)
#	pkmraw	Portable inKmap (raw format) (4-bit CMYK => RGB)
#	pngmono	Monochrome Portable Network Graphics (PNG)
#	pnggray	8-bit gray Portable Network Graphics (PNG)
#	png16	4-bit color Portable Network Graphics (PNG)
#	png256	8-bit color Portable Network Graphics (PNG)
#	png16m	24-bit color Portable Network Graphics (PNG)
#	psmono	PostScript (Level 1) monochrome image
#	psgray	PostScript (Level 1) 8-bit gray image
#	sgirgb	SGI RGB pixmap format
#	tiff12nc  TIFF 12-bit RGB, no compression
#	tiff24nc  TIFF 24-bit RGB, no compression (NeXT standard format)
#	tifflzw  TIFF LZW (tag = 5) (monochrome)
#	tiffpack  TIFF PackBits (tag = 32773) (monochrome)

# User-contributed drivers marked with * require hardware or software
# that is not available to Aladdin Enterprises.  Please contact the
# original contributors, not Aladdin Enterprises, if you have questions.
# Contact information appears in the driver entry below.
#
# Drivers marked with a + are maintained by Aladdin Enterprises with
# the assistance of users, since Aladdin Enterprises doesn't have access to
# the hardware for these either.

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

# As noted in gs.mak, DEVICE_DEVS and DEVICE_DEVS1..15 select the devices
# that should be included in a given configuration.  By convention, these
# are used as follows.  Each of these must be limited to about 10 devices
# so as not to overflow the 120 character limit on MS-DOS command lines.
#	DEVICE_DEVS - the default device, and any display devices.
#	DEVICE_DEVS1 - additional display devices if needed.
#	DEVICE_DEVS2 - dot matrix printers.
#	DEVICE_DEVS3 - H-P monochrome printers.
#	DEVICE_DEVS4 - H-P color printers.
#	DEVICE_DEVS5 - additional H-P printers if needed.
#	DEVICE_DEVS6 - other ink-jet and laser printers.
#	DEVICE_DEVS7 - fax file formats.
#	DEVICE_DEVS8 - PCX file formats.
#	DEVICE_DEVS9 - PBM/PGM/PPM file formats.
#	DEVICE_DEVS10 - black-and-white TIFF file formats.
#	DEVICE_DEVS11 - BMP and color TIFF file formats.
#	DEVICE_DEVS12 - PostScript image and 'bit' file formats.
#	DEVICE_DEVS13 - PNG file formats.
#	DEVICE_DEVS14 - CGM, JPEG, and MIFF file formats.
#	DEVICE_DEVS15 - high-level (PostScript and PDF) file formats.
# Feel free to disregard this convention if it gets in your way.

# If you want to add a new device driver, the examples below should be
# enough of a guide to the correct form for the makefile rules.
# Note that all drivers other than displays must include page.dev in their
# dependencies and use $(SETPDEV) rather than $(SETDEV) in their rule bodies.

# All device drivers depend on the following:
GDEV=$(AK) $(ECHOGS_XE) $(gserrors_h) $(gx_h) $(gxdevice_h)

# "Printer" drivers depend on the following:
PDEVH=$(AK) $(gdevprn_h)

# Define the header files for device drivers.  Every header file used by
# more than one device driver family must be listed here.
gdev8bcm_h=gdev8bcm.h
gdevpccm_h=gdevpccm.h
gdevpcfb_h=gdevpcfb.h $(dos__h)
gdevpcl_h=gdevpcl.h
gdevsvga_h=gdevsvga.h
gdevx_h=gdevx.h

###### ----------------------- Device support ----------------------- ######

# Provide a mapping between StandardEncoding and ISOLatin1Encoding.
gdevemap.$(OBJ): gdevemap.c $(AK) $(std_h)

# Implement dynamic color management for 8-bit mapped color displays.
gdev8bcm.$(OBJ): gdev8bcm.c $(AK) \
  $(gx_h) $(gxdevice_h) $(gdev8bcm_h)

###### ------------------- MS-DOS display devices ------------------- ######

# There are really only three drivers: an EGA/VGA driver (4 bit-planes,
# plane-addressed), a SuperVGA driver (8 bit-planes, byte addressed),
# and a special driver for the S3 chip.

# PC display color mapping
gdevpccm.$(OBJ): gdevpccm.c $(AK) \
  $(gx_h) $(gsmatrix_h) $(gxdevice_h) $(gdevpccm_h)

### ----------------------- EGA and VGA displays ----------------------- ###

# The shared MS-DOS makefile defines PCFBASM as either gdevegaa.$(OBJ)
# or an empty string.

gdevegaa.$(OBJ): gdevegaa.asm

# NOTE: for direct frame buffer addressing under SCO Unix or Xenix,
# change gdevevga to gdevsco in the following line.  Also, since
# SCO's /bin/as does not support the "out" instructions, you must build
# the gnu assembler and have it on your path as "as".
EGAVGA=gdevevga.$(OBJ) gdevpcfb.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)
#EGAVGA=gdevsco.$(OBJ) gdevpcfb.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)

gdevevga.$(OBJ): gdevevga.c $(GDEV) $(memory__h) $(gdevpcfb_h)
	$(CCD) gdevevga.c

gdevsco.$(OBJ): gdevsco.c $(GDEV) $(memory__h) $(gdevpcfb_h)

# Common code for MS-DOS and SCO.
gdevpcfb.$(OBJ): gdevpcfb.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gdevpccm_h) $(gdevpcfb_h) $(gsparam_h)
	$(CCD) gdevpcfb.c

# The EGA/VGA family includes EGA and VGA.  Many SuperVGAs in 800x600,
# 16-color mode can share the same code; see the next section below.

ega.dev: $(EGAVGA)
	$(SETDEV) ega $(EGAVGA)

vga.dev: $(EGAVGA)
	$(SETDEV) vga $(EGAVGA)

### ------------------------- SuperVGA displays ------------------------ ###

# SuperVGA displays in 16-color, 800x600 mode are really just slightly
# glorified VGA's, so we can handle them all with a single driver.
# The way to select them on the command line is with
#	-sDEVICE=svga16 -dDisplayMode=NNN
# where NNN is the display mode in decimal.  See use.txt for the modes
# for some popular display chipsets.

svga16.dev: $(EGAVGA)
	$(SETDEV) svga16 $(EGAVGA)

# More capable SuperVGAs have a wide variety of slightly differing
# interfaces, so we need a separate driver for each one.

SVGA=gdevsvga.$(OBJ) gdevpccm.$(OBJ) $(PCFBASM)

gdevsvga.$(OBJ): gdevsvga.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gsparam_h) $(gxarith_h) $(gdevpccm_h) $(gdevpcfb_h) $(gdevsvga_h)
	$(CCD) gdevsvga.c

# The SuperVGA family includes: Avance Logic Inc., ATI Wonder, S3,
# Trident, Tseng ET3000/4000, and VESA.

ali.dev: $(SVGA)
	$(SETDEV) ali $(SVGA)

atiw.dev: $(SVGA)
	$(SETDEV) atiw $(SVGA)

tseng.dev: $(SVGA)
	$(SETDEV) tseng $(SVGA)

tvga.dev: $(SVGA)
	$(SETDEV) tvga $(SVGA)

vesa.dev: $(SVGA)
	$(SETDEV) vesa $(SVGA)

# The S3 driver doesn't share much code with the others.

s3vga_=gdevs3ga.$(OBJ) gdevsvga.$(OBJ) gdevpccm.$(OBJ)
s3vga.dev: $(SVGA) $(s3vga_)
	$(SETDEV) s3vga $(SVGA)
	$(ADDMOD) s3vga -obj $(s3vga_)

gdevs3ga.$(OBJ): gdevs3ga.c $(GDEV) $(gdevpcfb_h) $(gdevsvga_h)
	$(CCD) gdevs3ga.c

### ------------ The BGI (Borland Graphics Interface) device ----------- ###

cgaf.$(OBJ): $(BGIDIR)\cga.bgi
	$(BGIDIR)\bgiobj /F $(BGIDIR)\cga

egavgaf.$(OBJ): $(BGIDIR)\egavga.bgi
	$(BGIDIR)\bgiobj /F $(BGIDIR)\egavga

# Include egavgaf.$(OBJ) for debugging only.
bgi_=gdevbgi.$(OBJ) cgaf.$(OBJ)
bgi.dev: $(bgi_)
	$(SETDEV) bgi $(bgi_)
	$(ADDMOD) bgi -lib $(LIBDIR)\graphics

gdevbgi.$(OBJ): gdevbgi.c $(GDEV) $(MAKEFILE) $(gxxfont_h)
	$(CCC) -DBGI_LIB="$(BGIDIRSTR)" gdevbgi.c

### ------------------- The Hercules Graphics display ------------------- ###

herc_=gdevherc.$(OBJ)
herc.dev: $(herc_)
	$(SETDEV) herc $(herc_)

gdevherc.$(OBJ): gdevherc.c $(GDEV) $(dos__h) $(gsmatrix_h) $(gxbitmap_h)
	$(CCC) gdevherc.c

### ---------------------- The Private Eye display ---------------------- ###
### Note: this driver was contributed by a user:                          ###
###   please contact narf@media-lab.media.mit.edu if you have questions.  ###

pe_=gdevpe.$(OBJ)
pe.dev: $(pe_)
	$(SETDEV) pe $(pe_)

gdevpe.$(OBJ): gdevpe.c $(GDEV) $(memory__h)

###### ----------------------- Other displays ------------------------ ######

### -------------------- The MS-Windows 3.n DLL ------------------------- ###

gsdll_h=gsdll.h

gdevmswn_h=gdevmswn.h $(GDEV)\
 $(dos__h) $(memory__h) $(string__h) $(windows__h)\
 gp_mswin.h

gdevmswn.$(OBJ): gdevmswn.c $(gdevmswn_h) $(gp_h) $(gpcheck_h) \
 $(gsdll_h) $(gsparam_h) $(gdevpccm_h)
	$(CCCWIN) gdevmswn.c

gdevmsxf.$(OBJ): gdevmsxf.c $(ctype__h) $(math__h) $(memory__h) $(string__h)\
 $(gdevmswn_h) $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(CCCWIN) gdevmsxf.c

# An implementation using a DIB filled by an image device.
gdevwdib.$(OBJ): gdevwdib.c $(gdevmswn_h) $(gsdll_h) $(gxdevmem_h)
	$(CCCWIN) gdevwdib.c

mswindll_=gdevmswn.$(OBJ) gdevmsxf.$(OBJ) gdevwdib.$(OBJ) \
  gdevemap.$(OBJ) gdevpccm.$(OBJ)
mswindll.dev: $(mswindll_)
	$(SETDEV) mswindll $(mswindll_)

### -------------------- The MS-Windows DDB 3.n printer ----------------- ###

mswinprn_=gdevwprn.$(OBJ) gdevmsxf.$(OBJ)
mswinprn.dev: $(mswinprn_)
	$(SETDEV) mswinprn $(mswinprn_)

gdevwprn.$(OBJ): gdevwprn.c $(gdevmswn_h) $(gp_h)
	$(CCCWIN) gdevwprn.c

### -------------------- The MS-Windows DIB 3.n printer ----------------- ###

mswinpr2_=gdevwpr2.$(OBJ)
mswinpr2.dev: $(mswinpr2_) page.dev
	$(SETPDEV) mswinpr2 $(mswinpr2_)

gdevwpr2.$(OBJ): gdevwpr2.c $(PDEVH) $(windows__h)\
 $(gdevpccm_h) $(gp_h) gp_mswin.h
	$(CCCWIN) gdevwpr2.c

### ------------------ OS/2 Presentation Manager device ----------------- ###

os2pm_=gdevpm.$(OBJ) gdevpccm.$(OBJ)
os2pm.dev: $(os2pm_)
	$(SETDEV) os2pm $(os2pm_)

os2dll_=gdevpm.$(OBJ) gdevpccm.$(OBJ)
os2dll.dev: $(os2dll_)
	$(SETDEV) os2dll $(os2dll_)

gdevpm.$(OBJ): gdevpm.c $(string__h)\
 $(gp_h) $(gpcheck_h)\
 $(gsdll_h) $(gserrors_h) $(gsexit_h) $(gsparam_h)\
 $(gx_h) $(gxdevice_h) $(gxdevmem_h)\
 $(gdevpccm_h) gdevpm.h

### --------------------------- The OS/2 printer ------------------------ ###

os2prn_=gdevos2p.$(OBJ)
os2prn.dev: $(os2prn_) page.dev
	$(SETPDEV) os2prn $(os2prn_)

os2prn.$(OBJ): os2prn.c $(gp_h)

### -------------- The AT&T 3b1 Unixpc monochrome display --------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Andy Fyfe (andy@cs.caltech.edu) if you have questions.          ###

att3b1_=gdev3b1.$(OBJ)
att3b1.dev: $(att3b1_)
	$(SETDEV) att3b1 $(att3b1_)

gdev3b1.$(OBJ): gdev3b1.c $(GDEV)

### ---------------------- Linux PC with vgalib ------------------------- ###
### Note: these drivers were contributed by users.                        ###
### For questions about the lvga256 driver, please contact                ###
###       Ludger Kunz (ludger.kunz@fernuni-hagen.de).                     ###
### For questions about the vgalib driver, please contact                 ###
###       Erik Talvola (talvola@gnu.ai.mit.edu).                          ###

lvga256_=gdevl256.$(OBJ)
lvga256.dev: $(lvga256_)
	$(SETDEV) lvga256 $(lvga256_)
	$(ADDMOD) lvga256 -lib vga vgagl

gdevl256.$(OBJ): gdevl256.c $(GDEV)

vgalib_=gdevvglb.$(OBJ) gdevpccm.$(OBJ)
vgalib.dev: $(vgalib_)
	$(SETDEV) vgalib $(vgalib_)
	$(ADDMOD) vgalib -lib vga

gdevvglb.$(OBJ): gdevvglb.c $(GDEV) $(gdevpccm_h) $(gsparam_h)

### ------------------- Sony NeWS frame buffer device ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Mike Smolenski (mike@intertech.com) if you have questions.     ###

# This is implemented as a 'printer' device.
sonyfb_=gdevsnfb.$(OBJ)
sonyfb.dev: $(sonyfb_) page.dev
	$(SETPDEV) sonyfb $(sonyfb_)

gdevsnfb.$(OBJ): gdevsnfb.c $(PDEVH)

### ------------------------ The SunView device ------------------------ ###
### Note: this driver is maintained by a user: if you have questions,    ###
###       please contact Andreas Stolcke (stolcke@icsi.berkeley.edu).    ###

sunview_=gdevsun.$(OBJ)
sunview.dev: $(sunview_)
	$(SETDEV) sunview $(sunview_)
	$(ADDMOD) sunview -lib suntool sunwindow pixrect

gdevsun.$(OBJ): gdevsun.c $(GDEV) $(malloc__h)\
 $(gscdefs_h) $(gserrors_h) $(gsmatrix_h)

### -------------------------- The X11 device -------------------------- ###

# Aladdin Enterprises does not support Ghostview.  For more information
# about Ghostview, please contact Tim Theisen (ghostview@cs.wisc.edu).

# See the main makefile for the definition of XLIBS.
x11_=gdevx.$(OBJ) gdevxini.$(OBJ) gdevxxf.$(OBJ) gdevemap.$(OBJ)
x11.dev: $(x11_)
	$(SETDEV) x11 $(x11_)
	$(ADDMOD) x11 -lib $(XLIBS)

# See the main makefile for the definition of XINCLUDE.
GDEVX=$(GDEV) x_.h gdevx.h $(MAKEFILE)
gdevx.$(OBJ): gdevx.c $(GDEVX) $(math__h) $(memory__h) $(gsparam_h)
	$(CCC) $(XINCLUDE) gdevx.c

gdevxini.$(OBJ): gdevxini.c $(GDEVX) $(math__h) $(memory__h) $(gserrors_h)
	$(CCC) $(XINCLUDE) gdevxini.c

gdevxxf.$(OBJ): gdevxxf.c $(GDEVX) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(CCC) $(XINCLUDE) gdevxxf.c

# Alternate X11-based devices to help debug other drivers.
# x11alpha pretends to have 4 bits of alpha channel.
# x11cmyk pretends to be a CMYK device with 1 bit each of C,M,Y,K.
# x11gray2 pretends to be a 2-bit gray-scale device.
# x11mono pretends to be a black-and-white device.
x11alt_=$(x11_) gdevxalt.$(OBJ)
x11alpha.dev: $(x11alt_)
	$(SETDEV) x11alpha $(x11alt_)
	$(ADDMOD) x11alpha -lib $(XLIBS)

x11cmyk.dev: $(x11alt_)
	$(SETDEV) x11cmyk $(x11alt_)
	$(ADDMOD) x11cmyk -lib $(XLIBS)

x11gray2.dev: $(x11alt_)
	$(SETDEV) x11gray2 $(x11alt_)
	$(ADDMOD) x11gray2 -lib $(XLIBS)

x11mono.dev: $(x11alt_)
	$(SETDEV) x11mono $(x11alt_)
	$(ADDMOD) x11mono -lib $(XLIBS)

gdevxalt.$(OBJ): gdevxalt.c $(GDEVX) $(math__h) $(memory__h) $(gsparam_h)
	$(CCC) $(XINCLUDE) gdevxalt.c

### ------------------------- DEC sixel displays ------------------------ ###
### Note: this driver was contributed by a user: please contact           ###
###   Phil Keegstra (keegstra@tonga.gsfc.nasa.gov) if you have questions. ###

# This is a "printer" device, but it probably shouldn't be.
# I don't know why the implementor chose to do it this way.
sxlcrt_=gdevln03.$(OBJ)
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

appledmp_=gdevadmp.$(OBJ)

gdevadmp.$(OBJ): gdevadmp.c $(PDEVH)

appledmp.dev: $(appledmp_) page.dev
	$(SETPDEV) appledmp $(appledmp_)

iwhi.dev: $(appledmp_) page.dev
	$(SETPDEV) iwhi $(appledmp_)

iwlo.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlo $(appledmp_)

iwlq.dev: $(appledmp_) page.dev
	$(SETPDEV) iwlq $(appledmp_)

### ------------ The Canon BubbleJet BJ10e and BJ200 devices ------------ ###

bj10e_=gdevbj10.$(OBJ)

bj10e.dev: $(bj10e_) page.dev
	$(SETPDEV) bj10e $(bj10e_)

bj200.dev: $(bj10e_) page.dev
	$(SETPDEV) bj200 $(bj10e_)

gdevbj10.$(OBJ): gdevbj10.c $(PDEVH)

### ------------- The CalComp Raster Format ----------------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Ernst Muellner (ernst.muellner@oenzl.siemens.de) if you have    ###
###       questions.                                                      ###

ccr_=gdevccr.$(OBJ)
ccr.dev: $(ccr_) page.dev
	$(SETPDEV) ccr $(ccr_)

gdevccr.$(OBJ): gdevccr.c $(PDEVH)

### ----------- The H-P DeskJet and LaserJet printer devices ----------- ###

### These are essentially the same device.
### NOTE: printing at full resolution (300 DPI) requires a printer
###   with at least 1.5 Mb of memory.  150 DPI only requires .5 Mb.
### Note that the lj4dith driver is included with the H-P color printer
###   drivers below.

HPPCL=gdevpcl.$(OBJ)
HPMONO=gdevdjet.$(OBJ) $(HPPCL)

gdevpcl.$(OBJ): gdevpcl.c $(PDEVH) $(gdevpcl_h)

gdevdjet.$(OBJ): gdevdjet.c $(PDEVH) $(gdevpcl_h)

deskjet.dev: $(HPMONO) page.dev
	$(SETPDEV) deskjet $(HPMONO)

djet500.dev: $(HPMONO) page.dev
	$(SETPDEV) djet500 $(HPMONO)

laserjet.dev: $(HPMONO) page.dev
	$(SETPDEV) laserjet $(HPMONO)

ljetplus.dev: $(HPMONO) page.dev
	$(SETPDEV) ljetplus $(HPMONO)

### Selecting ljet2p provides TIFF (mode 2) compression on LaserJet III,
### IIIp, IIId, IIIsi, IId, and IIp. 

ljet2p.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet2p $(HPMONO)

### Selecting ljet3 provides Delta Row (mode 3) compression on LaserJet III,
### IIIp, IIId, IIIsi.

ljet3.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet3 $(HPMONO)

### Selecting ljet3d also provides duplex printing capability.

ljet3d.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet3d $(HPMONO)

### Selecting ljet4 also provides Delta Row compression on LaserJet IV series.

ljet4.dev: $(HPMONO) page.dev
	$(SETPDEV) ljet4 $(HPMONO)

lp2563.dev: $(HPMONO) page.dev
	$(SETPDEV) lp2563 $(HPMONO)

oce9050.dev: $(HPMONO) page.dev
	$(SETPDEV) oce9050 $(HPMONO)

### ------------------ The H-P LaserJet 5 and 6 devices ----------------- ###

### These drivers use H-P's new PCL XL printer language, like H-P's
### LaserJet 5 Enhanced driver for MS Windows.  We don't recommend using
### them:
###	- If you have a LJ 5L or 5P, which isn't a "real" LaserJet 5,
###	use the ljet4 driver instead.  (The lj5 drivers won't work.)
###	- If you have any other model of LJ 5 or 6, use the pxlmono
###	driver, which often produces much more compact output.

gdevpxat_h=gdevpxat.h
gdevpxen_h=gdevpxen.h
gdevpxop_h=gdevpxop.h

ljet5_=gdevlj56.$(OBJ) $(HPPCL)
lj5mono.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5mono $(ljet5_)

lj5gray.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5gray $(ljet5_)

gdevlj56.$(OBJ): gdevlj56.c $(PDEVH) $(gdevpcl_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h)

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
###       drivers, please contact Yves.Arrouye@imag.fr.                   ###

cdeskjet_=gdevcdj.$(OBJ) $(HPPCL)

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
gdevcdj.$(OBJ): gdevcdj.c $(std_h) $(PDEVH) gdevbjc.h\
 $(gsparam_h) $(gsstate_h) $(gxlum_h)\
 $(gdevpcl_h)

djet500c_=gdevdjtc.$(OBJ) $(HPPCL)
djet500c.dev: $(djet500c_) page.dev
	$(SETPDEV) djet500c $(djet500c_)

gdevdjtc.$(OBJ): gdevdjtc.c $(PDEVH) $(malloc__h) $(gdevpcl_h)

### -------------------- The Mitsubishi CP50 printer -------------------- ###
### Note: this driver was contributed by a user: please contact           ###
###       Michael Hu (michael@ximage.com) if you have questions.          ###

cp50_=gdevcp50.$(OBJ)
cp50.dev: $(cp50_) page.dev
	$(SETPDEV) cp50 $(cp50_)

gdevcp50.$(OBJ): gdevcp50.c $(PDEVH)

### ----------------- The generic Epson printer device ----------------- ###
### Note: most of this code was contributed by users.  Please contact    ###
###       the following people if you have questions:                    ###
###   eps9mid - Guenther Thomsen (thomsen@cs.tu-berlin.de)               ###
###   eps9high - David Wexelblat (dwex@mtgzfs3.att.com)                  ###
###   ibmpro - James W. Birdsall (jwbirdsa@picarefy.picarefy.com)        ###

epson_=gdevepsn.$(OBJ)

epson.dev: $(epson_) page.dev
	$(SETPDEV) epson $(epson_)

eps9mid.dev: $(epson_) page.dev
	$(SETPDEV) eps9mid $(epson_)

eps9high.dev: $(epson_) page.dev
	$(SETPDEV) eps9high $(epson_)

gdevepsn.$(OBJ): gdevepsn.c $(PDEVH)

### ----------------- The IBM Proprinter printer device ---------------- ###

ibmpro.dev: $(epson_) page.dev
	$(SETPDEV) ibmpro $(epson_)

### -------------- The Epson LQ-2550 color printer device -------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Dave St. Clair (dave@exlog.com) if you have questions.         ###

epsonc_=gdevepsc.$(OBJ)
epsonc.dev: $(epsonc_) page.dev
	$(SETPDEV) epsonc $(epsonc_)

gdevepsc.$(OBJ): gdevepsc.c $(PDEVH)

### ------------- The Epson ESC/P 2 language printer devices ------------- ###
### Note: these drivers were contributed by users.                         ###
### For questions about the Stylus 800 and AP3250 drivers, please contact  ###
###        Richard Brown (rab@tauon.ph.unimelb.edu.au).                    ###
### For questions about the Stylus Color drivers, please contact           ###
###        Gunther Hess (gunther@elmos.de).                                ###

ESCP2=gdevescp.$(OBJ)

gdevescp.$(OBJ): gdevescp.c $(PDEVH)

ap3250.dev: $(ESCP2) page.dev
	$(SETPDEV) ap3250 $(ESCP2)

st800.dev: $(ESCP2) page.dev
	$(SETPDEV) st800 $(ESCP2)

stcolor1_=gdevstc.$(OBJ) gdevstc1.$(OBJ) gdevstc2.$(OBJ)
stcolor2_=gdevstc3.$(OBJ) gdevstc4.$(OBJ)
stcolor.dev: $(stcolor1_) $(stcolor2_) page.dev
	$(SETPDEV) stcolor $(stcolor1_)
	$(ADDMOD) stcolor -obj $(stcolor2_)

gdevstc.$(OBJ): gdevstc.c gdevstc.h $(PDEVH)

gdevstc1.$(OBJ): gdevstc1.c gdevstc.h $(PDEVH)

gdevstc2.$(OBJ): gdevstc2.c gdevstc.h $(PDEVH)

gdevstc3.$(OBJ): gdevstc3.c gdevstc.h $(PDEVH)

gdevstc4.$(OBJ): gdevstc4.c gdevstc.h $(PDEVH)

### --------------- Ugly/Update -> Unified Printer Driver ---------------- ###
### For questions about this driver, please contact:                       ###
###        Gunther Hess (gunther@elmos.de)                                 ###

uniprint_=gdevupd.$(OBJ)
uniprint.dev: $(uniprint_) page.dev
	$(SETPDEV) uniprint $(uniprint_)

gdevupd.$(OBJ): gdevupd.c $(PDEVH) $(gsparam_h)

### -------------- cdj850 - HP 850c Driver under development ------------- ###
### Since this driver is in the development-phase it is not distributed    ###
### with ghostscript, but it is available via anonymous ftp from:          ###
###                        ftp://bonk.ethz.ch                              ###
### For questions about this driver, please contact:                       ###
###       Uli Wortmann (E-Mail address inside the driver-package)          ###

cdeskjet8_=gdevcd8.$(OBJ) $(HPPCL)

cdj850.dev: $(cdeskjet8_) page.dev
	$(SETPDEV) cdj850 $(cdeskjet8_)

### ------------ The H-P PaintJet color printer device ----------------- ###
### Note: this driver also supports the DEC LJ250 color printer, which   ###
###       has a PaintJet-compatible mode, and the PaintJet XL.           ###
### If you have questions about the XL, please contact Rob Reiss         ###
###       (rob@moray.berkeley.edu).                                      ###

PJET=gdevpjet.$(OBJ) $(HPPCL)

gdevpjet.$(OBJ): gdevpjet.c $(PDEVH) $(gdevpcl_h)

lj250.dev: $(PJET) page.dev
	$(SETPDEV) lj250 $(PJET)

paintjet.dev: $(PJET) page.dev
	$(SETPDEV) paintjet $(PJET)

pjetxl.dev: $(PJET) page.dev
	$(SETPDEV) pjetxl $(PJET)

### -------------- Imagen ImPress Laser Printer device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Alan Millar (AMillar@bolis.sf-bay.org) if you have questions.  ###
### Set USE_BYTE_STREAM if using parallel interface;                     ###
### Don't set it if using 'ipr' spooler (default).                       ###
### You may also add -DA4 if needed for A4 paper.			 ###

imagen_=gdevimgn.$(OBJ)
imagen.dev: $(imagen_) page.dev
	$(SETPDEV) imagen $(imagen_)

gdevimgn.$(OBJ): gdevimgn.c $(PDEVH)
	$(CCC) gdevimgn.c			# for ipr spooler
#	$(CCC) -DUSE_BYTE_STREAM gdevimgn.c	# for parallel

### ------- The IBM 3852 JetPrinter color inkjet printer device -------- ###
### Note: this driver was contributed by users: please contact           ###
###       Kevin Gift (kgift@draper.com) if you have questions.           ###
### Note that the paper size that can be addressed by the graphics mode  ###
###   used in this driver is fixed at 7-1/2 inches wide (the printable   ###
###   width of the jetprinter itself.)                                   ###

jetp3852_=gdev3852.$(OBJ)
jetp3852.dev: $(jetp3852_) page.dev
	$(SETPDEV) jetp3852 $(jetp3852_)

gdev3852.$(OBJ): gdev3852.c $(PDEVH) $(gdevpcl_h)

### ---------- The Canon LBP-8II and LIPS III printer devices ---------- ###
### Note: these drivers were contributed by users.                       ###
### For questions about these drivers, please contact                    ###
###       Lauri Paatero, lauri.paatero@paatero.pp.fi                     ###

lbp8_=gdevlbp8.$(OBJ)
lbp8.dev: $(lbp8_) page.dev
	$(SETPDEV) lbp8 $(lbp8_)

lips3.dev: $(lbp8_) page.dev
	$(SETPDEV) lips3 $(lbp8_)

gdevlbp8.$(OBJ): gdevlbp8.c $(PDEVH)

### ----------- The DEC LN03/LA50/LA70/LA75 printer devices ------------ ###
### Note: this driver was contributed by users: please contact           ###
###       Ulrich Mueller (ulm@vsnhd1.cern.ch) if you have questions.     ###
### For questions about LA50 and LA75, please contact                    ###
###       Ian MacPhedran (macphed@dvinci.USask.CA).                      ###
### For questions about the LA70, please contact                         ###
###       Bruce Lowekamp (lowekamp@csugrad.cs.vt.edu).                   ###
### For questions about the LA75plus, please contact                     ###
###       Andre' Beck (Andre_Beck@IRS.Inf.TU-Dresden.de).                ###

ln03_=gdevln03.$(OBJ)
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

gdevln03.$(OBJ): gdevln03.c $(PDEVH)

# LA70 driver with low-resolution text enhancement.

la70t_=gdevla7t.$(OBJ)
la70t.dev: $(la70t_) page.dev
	$(SETPDEV) la70t $(la70t_)

gdevla7t.$(OBJ): gdevla7t.c $(PDEVH)

### -------------- The Epson LP-8000 laser printer device -------------- ###
### Note: this driver was contributed by a user: please contact Oleg     ###
###       Oleg Fat'yanov <faty1@rlem.titech.ac.jp> if you have questions.###

lp8000_=gdevlp8k.$(OBJ)
lp8000.dev: $(lp8000_) page.dev
	$(SETPDEV) lp8000 $(lp8000_)

gdevlp8k.$(OBJ): gdevlp8k.c $(PDEVH)

### -------------- The C.Itoh M8510 printer device --------------------- ###
### Note: this driver was contributed by a user: please contact Bob      ###
###       Smith <bob@snuffy.penfield.ny.us> if you have questions.       ###

m8510_=gdev8510.$(OBJ)
m8510.dev: $(m8510_) page.dev
	$(SETPDEV) m8510 $(m8510_)

gdev8510.$(OBJ): gdev8510.c $(PDEVH)

### -------------- 24pin Dot-matrix printer with 360DPI ---------------- ###
### Note: this driver was contributed by users.  Please contact:         ###
###    Andreas Schwab (schwab@ls5.informatik.uni-dortmund.de) for        ###
###      questions about the NEC P6;                                     ###
###    Christian Felsch (felsch@tu-harburg.d400.de) for                  ###
###      questions about the Epson LQ850.                                ###

dm24_=gdevdm24.$(OBJ)
gdevdm24.$(OBJ): gdevdm24.c $(PDEVH)

necp6.dev: $(dm24_) page.dev
	$(SETPDEV) necp6 $(dm24_)

lq850.dev: $(dm24_) page.dev
	$(SETPDEV) lq850 $(dm24_)

### ----------------- The Okidata MicroLine 182 device ----------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Maarten Koning (smeg@bnr.ca) if you have questions.            ###

oki182_=gdevo182.$(OBJ)
oki182.dev: $(oki182_) page.dev
	$(SETPDEV) oki182 $(oki182_)

gdevo182.$(OBJ): gdevo182.c $(PDEVH)

### ------------- The Okidata IBM compatible printer device ------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Charles Mack (chasm@netcom.com) if you have questions.         ###

okiibm_=gdevokii.$(OBJ)
okiibm.dev: $(okiibm_) page.dev
	$(SETPDEV) okiibm $(okiibm_)

gdevokii.$(OBJ): gdevokii.c $(PDEVH)

### ------------- The Ricoh 4081 laser printer device ------------------ ###
### Note: this driver was contributed by users:                          ###
###       please contact kdw@oasis.icl.co.uk if you have questions.      ###

r4081_=gdev4081.$(OBJ)
r4081.dev: $(r4081_) page.dev
	$(SETPDEV) r4081 $(r4081_)


gdev4081.$(OBJ): gdev4081.c $(PDEVH)

### -------------------- Sony NWP533 printer device -------------------- ###
### Note: this driver was contributed by a user: please contact Tero     ###
###       Kivinen (kivinen@joker.cs.hut.fi) if you have questions.       ###

nwp533_=gdevn533.$(OBJ)
nwp533.dev: $(nwp533_) page.dev
	$(SETPDEV) nwp533 $(nwp533_)

gdevn533.$(OBJ): gdevn533.c $(PDEVH)

### ------------------------- The SPARCprinter ------------------------- ###
### Note: this driver was contributed by users: please contact Martin    ###
###       Schulte (schulte@thp.uni-koeln.de) if you have questions.      ###
###       He would also like to hear from anyone using the driver.       ###
### Please consult the source code for additional documentation.         ###

sparc_=gdevsppr.$(OBJ)
sparc.dev: $(sparc_) page.dev
	$(SETPDEV) sparc $(sparc_)

gdevsppr.$(OBJ): gdevsppr.c $(PDEVH)

### ----------------- The StarJet SJ48 device -------------------------- ###
### Note: this driver was contributed by a user: if you have questions,  ###
###	                      .                                          ###
###       please contact Mats Akerblom (f86ma@dd.chalmers.se).           ###

sj48_=gdevsj48.$(OBJ)
sj48.dev: $(sj48_) page.dev
	$(SETPDEV) sj48 $(sj48_)

gdevsj48.$(OBJ): gdevsj48.c $(PDEVH)

### ----------------- Tektronix 4396d color printer -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karl Hakimian (hakimian@haney.eecs.wsu.edu)                    ###
###       if you have questions.                                         ###

t4693d_=gdev4693.$(OBJ)
t4693d2.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d2 $(t4693d_)

t4693d4.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d4 $(t4693d_)

t4693d8.dev: $(t4693d_) page.dev
	$(SETPDEV) t4693d8 $(t4693d_)

gdev4693.$(OBJ): gdev4693.c $(PDEVH)

### -------------------- Tektronix ink-jet printers -------------------- ###
### Note: this driver was contributed by a user: please contact          ###
###       Karsten Spang (spang@nbivax.nbi.dk) if you have questions.     ###

tek4696_=gdevtknk.$(OBJ)
tek4696.dev: $(tek4696_) page.dev
	$(SETPDEV) tek4696 $(tek4696_)

gdevtknk.$(OBJ): gdevtknk.c $(PDEVH) $(malloc__h)

### ----------------- The Xerox XES printer device --------------------- ###
### Note: this driver was contributed by users: please contact           ###
###       Peter Flass (flass@lbdrscs.bitnet) if you have questions.      ###

xes_=gdevxes.$(OBJ)
xes.dev: $(xes_) page.dev
	$(SETPDEV) xes $(xes_)

gdevxes.$(OBJ): gdevxes.c $(PDEVH)

###### ------------------------- Fax devices ------------------------- ######

### --------------- Generic PostScript system compatible fax ------------ ###

# This code doesn't work yet.  Don't even think about using it.

PSFAX=gdevpfax.$(OBJ)

psfax_=$(PSFAX)
psfax.dev: $(psfax_) page.dev
	$(SETPDEV) psfax $(psfax_)
	$(ADDMOD) psfax -iodev Fax

gdevpfax.$(OBJ): gdevpfax.c $(PDEVH) $(gsparam_h) $(gxiodev_h)

### ------------------------- The DigiFAX device ------------------------ ###
###    This driver outputs images in a format suitable for use with       ###
###    DigiBoard, Inc.'s DigiFAX software.  Use -sDEVICE=dfaxhigh for     ###
###    high resolution output, -sDEVICE=dfaxlow for normal output.        ###
### Note: this driver was contributed by a user: please contact           ###
###       Rick Richardson (rick@digibd.com) if you have questions.        ###

dfax_=gdevdfax.$(OBJ) gdevtfax.$(OBJ)

dfaxlow.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxlow $(dfax_)
	$(ADDMOD) dfaxlow -include cfe

dfaxhigh.dev: $(dfax_) page.dev
	$(SETPDEV) dfaxhigh $(dfax_)
	$(ADDMOD) dfaxhigh -include cfe

gdevdfax.$(OBJ): gdevdfax.c $(PDEVH) $(scfx_h) $(strimpl_h)

### --------------See under TIFF below for fax-format TIFF -------------- ###

###### ------------------- High-level file formats ------------------- ######

# Support for PostScript and PDF

gdevpsdf_h=gdevpsdf.h $(gdevvec_h) $(strimpl_h)
gdevpstr_h=gdevpstr.h

gdevpsdf.$(OBJ): gdevpsdf.c $(stdio__h) $(string__h)\
 $(gserror_h) $(gserrors_h) $(gsmemory_h) $(gsparam_h) $(gstypes_h)\
 $(gxdevice_h)\
 $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

gdevpstr.$(OBJ): gdevpstr.c $(math__h) $(stdio__h) $(string__h)\
 $(gdevpstr_h) $(stream_h)

# PostScript and EPS writers

pswrite1_=gdevps.$(OBJ) gdevpsdf.$(OBJ) gdevpstr.$(OBJ)
pswrite2_=scantab.$(OBJ) sfilter2.$(OBJ)
pswrite_=$(pswrite1_) $(pswrite2_)
epswrite.dev: $(ECHOGS_XE) $(pswrite_) vector.dev
	$(SETDEV) epswrite $(pswrite1_)
	$(ADDMOD) epswrite $(pswrite2_)
	$(ADDMOD) epswrite -include vector

pswrite.dev: $(ECHOGS_XE) $(pswrite_) vector.dev
	$(SETDEV) pswrite $(pswrite1_)
	$(ADDMOD) pswrite $(pswrite2_)
	$(ADDMOD) pswrite -include vector

gdevps.$(OBJ): gdevps.c $(GDEV) $(math__h) $(time__h)\
 $(gscdefs_h) $(gscspace_h) $(gsparam_h) $(gsiparam_h) $(gsmatrix_h)\
 $(gxdcolor_h)\
 $(sa85x_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

# PDF writer
# Note that gs_pdfwr.ps will only actually be loaded if the configuration
# includes a PostScript interpreter.

pdfwrite1_=gdevpdf.$(OBJ) gdevpdfd.$(OBJ) gdevpdfi.$(OBJ) gdevpdfm.$(OBJ)
pdfwrite2_=gdevpdfp.$(OBJ) gdevpdft.$(OBJ) gdevpsdf.$(OBJ) gdevpstr.$(OBJ)
pdfwrite3_=gsflip.$(OBJ) scantab.$(OBJ) sfilter2.$(OBJ) sstring.$(OBJ)
pdfwrite_=$(pdfwrite1_) $(pdfwrite2_) $(pdfwrite3_)
pdfwrite.dev: $(ECHOGS_XE) $(pdfwrite_) \
  cmyklib.dev cfe.dev dcte.dev lzwe.dev rle.dev vector.dev
	$(SETDEV) pdfwrite $(pdfwrite1_)
	$(ADDMOD) pdfwrite $(pdfwrite2_)
	$(ADDMOD) pdfwrite $(pdfwrite3_)
	$(ADDMOD) pdfwrite -ps gs_pdfwr
	$(ADDMOD) pdfwrite -include cmyklib cfe dcte lzwe rle vector

gdevpdfx_h=gdevpdfx.h $(gsparam_h) $(gxdevice_h) $(gxline_h) $(stream_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

gdevpdf.$(OBJ): gdevpdf.c $(math__h) $(memory__h) $(string__h) $(time__h)\
 $(gp_h)\
 $(gdevpdfx_h) $(gscdefs_h) $(gserrors_h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)\
 $(scanchar_h) $(scfx_h) $(slzwx_h) $(sstring_h) $(strimpl_h) $(szlibx_h)
	$(CCCZ) gdevpdf.c

gdevpdfd.$(OBJ): gdevpdfd.c $(math__h)\
 $(gdevpdfx_h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)

gdevpdfi.$(OBJ): gdevpdfi.c $(math__h) $(memory__h) $(gx_h) \
  $(gdevpdfx_h) $(gscie_h) $(gscolor2_h) $(gserrors_h) $(gsflip_h)\
  $(gxcspace_h) $(gxistate_h) \
  $(sa85x_h) $(scfx_h) $(srlx_h) $(strimpl_h)

gdevpdfm.$(OBJ): gdevpdfm.c $(memory__h) $(string__h) $(gx_h) \
  $(gdevpdfx_h) $(gserrors_h) $(gsutil_h) $(scanchar_h)

gdevpdfp.$(OBJ): gdevpdfp.c $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h)

gdevpdft.$(OBJ): gdevpdft.c $(math__h) $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(scommon_h)

# High-level PCL XL writer

pxl_=gdevpx.$(OBJ)
pxlmono.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV) pxlmono $(pxl_)
	$(ADDMOD) pxlmono -include vector

pxlcolor.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV) pxlcolor $(pxl_)
	$(ADDMOD) pxlcolor -include vector

gdevpx.$(OBJ): gdevpx.c $(math__h) $(memory__h) $(string__h)\
 $(gx_h) $(gsccolor_h) $(gsdcolor_h) $(gserrors_h)\
 $(gxcspace_h) $(gxdevice_h) $(gxpath_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevvec_h)\
 $(srlx_h) $(strimpl_h)

###### --------------------- Raster file formats --------------------- ######

### --------------------- The "plain bits" devices ---------------------- ###

bit_=gdevbit.$(OBJ)

bit.dev: $(bit_) page.dev
	$(SETPDEV) bit $(bit_)

bitrgb.dev: $(bit_) page.dev
	$(SETPDEV) bitrgb $(bit_)

bitcmyk.dev: $(bit_) page.dev
	$(SETPDEV) bitcmyk $(bit_)

gdevbit.$(OBJ): gdevbit.c $(PDEVH) $(gsparam_h) $(gxlum_h)

### ------------------------- .BMP file formats ------------------------- ###

bmp_=gdevbmp.$(OBJ) gdevpccm.$(OBJ)

gdevbmp.$(OBJ): gdevbmp.c $(PDEVH) $(gdevpccm_h)

bmpmono.dev: $(bmp_) page.dev
	$(SETPDEV) bmpmono $(bmp_)

bmp16.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16 $(bmp_)

bmp256.dev: $(bmp_) page.dev
	$(SETPDEV) bmp256 $(bmp_)

bmp16m.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16m $(bmp_)

### ------------- BMP driver that serves as demo of async rendering ---- ###
devasync_=gdevasyn.$(OBJ) gdevpccm.$(OBJ) gxsync.$(OBJ)

gdevasyn.$(OBJ): gdevasyn.c $(AK) $(stdio__h) $(gdevprna_h) $(gdevpccm_h)\
 $(gserrors_h) $(gpsync_h)

asynmono.dev: $(devasync_) page.dev async.dev
	$(SETPDEV) asynmono $(devasync_)
	$(ADDMOD) asynmono -include async


### -------------------------- CGM file format ------------------------- ###
### This driver is under development.  Use at your own risk.             ###
### The output is very low-level, consisting only of rectangles and      ###
### cell arrays.                                                         ###

cgm_=gdevcgm.$(OBJ) gdevcgml.$(OBJ)

gdevcgml_h=gdevcgml.h
gdevcgmx_h=gdevcgmx.h $(gdevcgml_h)

gdevcgm.$(OBJ): gdevcgm.c $(GDEV) $(memory__h)\
 $(gsparam_h) $(gdevpccm_h) $(gdevcgml_h)

gdevcgml.$(OBJ): gdevcgml.c $(memory__h) $(stdio__h)\
 $(gdevcgmx_h)

cgmmono.dev: $(cgm_)
	$(SETDEV) cgmmono $(cgm_)

cgm8.dev: $(cgm_)
	$(SETDEV) cgm8 $(cgm_)

cgm24.dev: $(cgm_)
	$(SETDEV) cgm24 $(cgm_)

### -------------------- The CIF file format for VLSI ------------------ ###
### Note: this driver was contributed by a user: please contact          ###
###       Frederic Petrot (petrot@masi.ibp.fr) if you have questions.    ###

cif_=gdevcif.$(OBJ)
cif.dev: $(cif_) page.dev
	$(SETPDEV) cif $(cif_)

gdevcif.$(OBJ): gdevcif.c $(PDEVH)

### ------------------------- JPEG file format ------------------------- ###

jpeg_=gdevjpeg.$(OBJ)

# RGB output
jpeg.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV) jpeg $(jpeg_)
	$(ADDMOD) jpeg -include sdcte

# Gray output
jpeggray.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV) jpeggray $(jpeg_)
	$(ADDMOD) jpeggray -include sdcte

gdevjpeg.$(OBJ): gdevjpeg.c $(stdio__h) $(PDEVH)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h) jpeglib.h

### ------------------------- MIFF file format ------------------------- ###
### Right now we support only 24-bit direct color, but we might add more ###
### formats in the future.                                               ###

miff_=gdevmiff.$(OBJ)

miff24.dev: $(miff_) page.dev
	$(SETPDEV) miff24 $(miff_)

gdevmiff.$(OBJ): gdevmiff.c $(PDEVH)

### --------------------------- MGR devices ---------------------------- ###
### Note: these drivers were contributed by a user: please contact       ###
###       Carsten Emde (carsten@ce.pr.net.ch) if you have questions.     ###

MGR=gdevmgr.$(OBJ) gdevpccm.$(OBJ)

gdevmgr.$(OBJ): gdevmgr.c $(PDEVH) $(gdevpccm_h) gdevmgr.h

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

### ------------------------- PCX file formats ------------------------- ###

pcx_=gdevpcx.$(OBJ) gdevpccm.$(OBJ)

gdevpcx.$(OBJ): gdevpcx.c $(PDEVH) $(gdevpccm_h) $(gxlum_h)

pcxmono.dev: $(pcx_) page.dev
	$(SETPDEV) pcxmono $(pcx_)

pcxgray.dev: $(pcx_) page.dev
	$(SETPDEV) pcxgray $(pcx_)

pcx16.dev: $(pcx_) page.dev
	$(SETPDEV) pcx16 $(pcx_)

pcx256.dev: $(pcx_) page.dev
	$(SETPDEV) pcx256 $(pcx_)

pcx24b.dev: $(pcx_) page.dev
	$(SETPDEV) pcx24b $(pcx_)

pcxcmyk.dev: $(pcx_) page.dev
	$(SETPDEV) pcxcmyk $(pcx_)

# The 2-up PCX device is here only as an example, and for testing.
pcx2up.dev: $(LIB_MAK) $(ECHOGS_XE) gdevp2up.$(OBJ) page.dev pcx256.dev
	$(SETPDEV) pcx2up gdevp2up.$(OBJ)
	$(ADDMOD) pcx2up -include pcx256

gdevp2up.$(OBJ): gdevp2up.c $(AK)\
 $(gdevpccm_h) $(gdevprn_h) $(gxclpage_h)

### ------------------- Portable Bitmap file formats ------------------- ###
### For more information, see the pbm(5), pgm(5), and ppm(5) man pages.  ###

pxm_=gdevpbm.$(OBJ)

gdevpbm.$(OBJ): gdevpbm.c $(PDEVH) $(gscdefs_h) $(gxlum_h)

### Portable Bitmap (PBM, plain or raw format, magic numbers "P1" or "P4")

pbm.dev: $(pxm_) page.dev
	$(SETPDEV) pbm $(pxm_)

pbmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pbmraw $(pxm_)

### Portable Graymap (PGM, plain or raw format, magic numbers "P2" or "P5")

pgm.dev: $(pxm_) page.dev
	$(SETPDEV) pgm $(pxm_)

pgmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pgmraw $(pxm_)

# PGM with automatic optimization to PBM if this is possible.

pgnm.dev: $(pxm_) page.dev
	$(SETPDEV) pgnm $(pxm_)

pgnmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pgnmraw $(pxm_)

### Portable Pixmap (PPM, plain or raw format, magic numbers "P3" or "P6")

ppm.dev: $(pxm_) page.dev
	$(SETPDEV) ppm $(pxm_)

ppmraw.dev: $(pxm_) page.dev
	$(SETPDEV) ppmraw $(pxm_)

# PPM with automatic optimization to PGM or PBM if possible.

pnm.dev: $(pxm_) page.dev
	$(SETPDEV) pnm $(pxm_)

pnmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pnmraw $(pxm_)

### Portable inKmap (CMYK internally, converted to PPM=RGB at output time)

pkm.dev: $(pxm_) page.dev
	$(SETPDEV) pkm $(pxm_)

pkmraw.dev: $(pxm_) page.dev
	$(SETPDEV) pkmraw $(pxm_)

### --------------- Portable Network Graphics file format --------------- ###
### Requires libpng 0.81 and zlib 0.95 (or more recent versions).         ###
### See libpng.mak and zlib.mak for more details.                         ###

png_=gdevpng.$(OBJ) gdevpccm.$(OBJ)

gdevpng.$(OBJ): gdevpng.c $(gdevprn_h) $(gdevpccm_h) $(gscdefs_h) $(PSRC)png.h
	$(CCCP) gdevpng.c

pngmono.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) pngmono  $(png_)
	$(ADDMOD) pngmono  -include libpng

pnggray.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) pnggray  $(png_)
	$(ADDMOD) pnggray  -include libpng

png16.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png16  $(png_)
	$(ADDMOD) png16  -include libpng

png256.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png256  $(png_)
	$(ADDMOD) png256  -include libpng

png16m.dev: libpng.dev $(png_) page.dev
	$(SETPDEV) png16m  $(png_)
	$(ADDMOD) png16m  -include libpng

### ---------------------- PostScript image format ---------------------- ###
### These devices make it possible to print Level 2 files on a Level 1    ###
###   printer, by converting them to a bitmap in PostScript format.       ###

ps_=gdevpsim.$(OBJ)

gdevpsim.$(OBJ): gdevpsim.c $(PDEVH)

psmono.dev: $(ps_) page.dev
	$(SETPDEV) psmono $(ps_)

psgray.dev: $(ps_) page.dev
	$(SETPDEV) psgray $(ps_)

# Someday there will be RGB and CMYK variants....

### -------------------------- SGI RGB pixmaps -------------------------- ###

sgirgb_=gdevsgi.$(OBJ)

gdevsgi.$(OBJ): gdevsgi.c $(PDEVH) gdevsgi.h

sgirgb.dev: $(sgirgb_) page.dev
	$(SETPDEV) sgirgb $(sgirgb_)

### -------------------- Plain or TIFF fax encoding --------------------- ###
###    Use -sDEVICE=tiffg3 or tiffg4 and				  ###
###	  -r204x98 for low resolution output, or			  ###
###	  -r204x196 for high resolution output				  ###
###    These drivers recognize 3 page sizes: letter, A4, and B4.	  ###

gdevtifs_h=gdevtifs.h

tfax_=gdevtfax.$(OBJ)
tfax.dev: $(tfax_) cfe.dev lzwe.dev rle.dev tiffs.dev
	$(SETMOD) tfax $(tfax_)
	$(ADDMOD) tfax -include cfe lzwe rle tiffs

gdevtfax.$(OBJ): gdevtfax.c $(PDEVH)\
 $(gdevtifs_h) $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h)

### Plain G3/G4 fax with no header

faxg3.dev: tfax.dev
	$(SETDEV) faxg3 -include tfax

faxg32d.dev: tfax.dev
	$(SETDEV) faxg32d -include tfax

faxg4.dev: tfax.dev
	$(SETDEV) faxg4 -include tfax

### ---------------------------- TIFF formats --------------------------- ###

tiffs_=gdevtifs.$(OBJ)
tiffs.dev: $(tiffs_) page.dev
	$(SETMOD) tiffs $(tiffs_)
	$(ADDMOD) tiffs -include page

gdevtifs.$(OBJ): gdevtifs.c $(PDEVH) $(stdio__h) $(time__h) \
 $(gdevtifs_h) $(gscdefs_h) $(gstypes_h)

# Black & white, G3/G4 fax

tiffcrle.dev: tfax.dev
	$(SETDEV) tiffcrle -include tfax

tiffg3.dev: tfax.dev
	$(SETDEV) tiffg3 -include tfax

tiffg32d.dev: tfax.dev
	$(SETDEV) tiffg32d -include tfax

tiffg4.dev: tfax.dev
	$(SETDEV) tiffg4 -include tfax

# Black & white, LZW compression

tifflzw.dev: tfax.dev
	$(SETDEV) tifflzw -include tfax

# Black & white, PackBits compression

tiffpack.dev: tfax.dev
	$(SETDEV) tiffpack -include tfax

# RGB, no compression

tiffrgb_=gdevtfnx.$(OBJ)

tiff12nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV) tiff12nc $(tiffrgb_)
	$(ADDMOD) tiff12nc -include tiffs

tiff24nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV) tiff24nc $(tiffrgb_)
	$(ADDMOD) tiff24nc -include tiffs

gdevtfnx.$(OBJ): gdevtfnx.c $(PDEVH) $(gdevtifs_h)
