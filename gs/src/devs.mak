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


# makefile for Aladdin's device drivers.

# Define the name of this makefile.
DEVS_MAK=$(GLSRC)devs.mak

# All device drivers depend on the following:
GDEVH=$(gserrors_h) $(gx_h) $(gxdevice_h)
GDEV=$(AK) $(ECHOGS_XE) $(GDEVH)

###### --------------------------- Overview -------------------------- ######

# It is possible to build Ghostscript with an arbitrary collection of device
# drivers, although some drivers are supported only on a subset of the
# target platforms.

# The catalog in this file, devs.mak, lists all the drivers that were
# written by Aladdin, or by people working closely with Aladdin, and for
# which Aladdin is willing to take problem reports (although since
# Ghostscript is provided with NO WARRANTY and NO SUPPORT, we can't promise
# that we'll solve your problem).  Another file, contrib.mak, lists all the
# drivers contributed by other people that are distributed by Aladdin with
# Ghostscript.  Note in particular that all drivers for color inkjets and
# other non-PostScript-capable color printers are in contrib.mak.

# If you haven't configured Ghostscript before, or if you want to add a
# driver that that isn't included in the catalogs (for which you have the
# source code), we suggest you skip to the "End of catalog" below and read
# the documentation there before continuing.

###### --------------------------- Catalog -------------------------- ######

# MS-DOS displays (note: not usable with Desqview/X):
#   MS-DOS EGA and VGA:
#	ega	EGA (640x350, 16-color)
#	vga	VGA (640x480, 16-color)
#   MS-DOS SuperVGA:
# *	ali	SuperVGA using Avance Logic Inc. chipset, 256-color modes
# *	atiw	ATI Wonder SuperVGA, 256-color modes
# *	cirr	SuperVGA using Cirrus Logic CL-GD54XX chips, 256-color modes
# *	s3vga	SuperVGA using S3 86C911 chip (e.g., Diamond Stealth board)
#	svga16	Generic SuperVGA in 800x600, 16-color mode
# *	tseng	SuperVGA using Tseng Labs ET3000/4000 chips, 256-color modes
# *	tvga	SuperVGA using Trident chipset, 256-color modes
#   ****** NOTE: The vesa device does not work with the Watcom (32-bit MS-DOS)
#   ****** compiler or executable.
#	vesa	SuperVGA with VESA standard API driver
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
# *	lvga256  Linux vgalib, 256-color VGA modes  [Linux only]
# +	vgalib	Linux PC with VGALIB   [Linux only]
#	x11	X Windows version 11, release >=4   [Unix and VMS only]
#	x11alpha  X Windows masquerading as a device with alpha capability
#	x11cmyk  X Windows masquerading as a 1-bit-per-plane CMYK device
#	x11cmyk2  X Windows as a 2-bit-per-plane CMYK device
#	x11cmyk4  X Windows as a 4-bit-per-plane CMYK device
#	x11cmyk8  X Windows as an 8-bit-per-plane CMYK device
#	x11gray2  X Windows as a 2-bit gray-scale device
#	x11gray4  X Windows as a 4-bit gray-scale device
#	x11mono  X Windows masquerading as a black-and-white device
# Printers:
# +	deskjet  H-P DeskJet and DeskJet Plus
#	djet500  H-P DeskJet 500; use -r600 for DJ 600 series
# +	laserjet  H-P LaserJet
# +	ljet2p	H-P LaserJet IId/IIp/III* with TIFF compression
# +	ljet3	H-P LaserJet III* with Delta Row compression
# +	ljet3d	H-P LaserJet IIID with duplex capability
# +	ljet4	H-P LaserJet 4 (defaults to 600 dpi)
# +	ljetplus  H-P LaserJet Plus
#	lj5mono  H-P LaserJet 5 & 6 family (PCL XL), bitmap:
#		see below for restrictions & advice
#	lj5gray  H-P LaserJet 5 & 6 family, gray-scale bitmap;
#		see below for restrictions & advice
# *	lp2563	H-P 2563B line printer
# *	oce9050  OCE 9050 printer
#	(pxlmono) H-P black-and-white PCL XL printers (LaserJet 5 and 6 family)
#	(pxlcolor) H-P color PCL XL printers (none available yet)
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
#	jpeg	JPEG format, RGB output
#	jpeggray  JPEG format, gray output
#	miff24	ImageMagick MIFF format, 24-bit direct color, RLE compressed
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
# *	plan9bm  Plan 9 bitmap format
#	pngmono	Monochrome Portable Network Graphics (PNG)
#	pnggray	8-bit gray Portable Network Graphics (PNG)
#	png16	4-bit color Portable Network Graphics (PNG)
#	png256	8-bit color Portable Network Graphics (PNG)
#	png16m	24-bit color Portable Network Graphics (PNG)
#	psmono	PostScript (Level 1) monochrome image
#	psgray	PostScript (Level 1) 8-bit gray image
#	psrgb	PostScript (Level 2) 24-bit color image
#	tiff12nc  TIFF 12-bit RGB, no compression
#	tiff24nc  TIFF 24-bit RGB, no compression (NeXT standard format)
#	tifflzw  TIFF LZW (tag = 5) (monochrome)
#	tiffpack  TIFF PackBits (tag = 32773) (monochrome)

# Note that MS Windows-specific drivers are defined in msdevs.mak, not here,
# because they have special compilation requirements that require defining
# parameter macros not relevant to other platforms; the OS/2-specific
# drivers are there too, because they share some definitions.

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

# "Printer" drivers depend on the following:
PDEVH=$(AK) $(gdevprn_h)

# Define the header files for device drivers.  Every header file used by
# more than one device driver family must be listed here.
gdev8bcm_h=$(GLSRC)gdev8bcm.h
gdevpccm_h=$(GLSRC)gdevpccm.h
gdevpcfb_h=$(GLSRC)gdevpcfb.h $(dos__h)
gdevpcl_h=$(GLSRC)gdevpcl.h
gdevsvga_h=$(GLSRC)gdevsvga.h

###### ----------------------- Device support ----------------------- ######

# Implement dynamic color management for 8-bit mapped color displays.
$(GLOBJ)gdev8bcm.$(OBJ): $(GLSRC)gdev8bcm.c $(AK)\
 $(gx_h) $(gxdevice_h) $(gdev8bcm_h)
	$(GLCC) $(GLO_)gdev8bcm.$(OBJ) $(C_) $(GLSRC)gdev8bcm.c

# PC display color mapping
$(GLOBJ)gdevpccm.$(OBJ): $(GLSRC)gdevpccm.c $(AK)\
 $(gx_h) $(gsmatrix_h) $(gxdevice_h) $(gdevpccm_h)
	$(GLCC) $(GLO_)gdevpccm.$(OBJ) $(C_) $(GLSRC)gdevpccm.c

###### ------------------- MS-DOS display devices ------------------- ######

# There are really only three drivers: an EGA/VGA driver (4 bit-planes,
# plane-addressed), a SuperVGA driver (8 bit-planes, byte addressed),
# and a special driver for the S3 chip.

### ----------------------- EGA and VGA displays ----------------------- ###

# The shared MS-DOS makefile defines PCFBASM as either gdevegaa.$(OBJ)
# or an empty string.

$(GLOBJ)gdevegaa.$(OBJ): $(GLSRC)gdevegaa.asm
	$(GLCC) $(GLO_)gdevegaa.$(OBJ) $(C_) $(GLSRC)gdevegaa.c

# NOTE: for direct frame buffer addressing under SCO Unix or Xenix,
# change gdevevga to gdevsco in the following line.  Also, since
# SCO's /bin/as does not support the "out" instructions, you must build
# the gnu assembler and have it on your path as "as".
EGAVGA=$(GLOBJ)gdevevga.$(OBJ) $(GLOBJ)gdevpcfb.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ) $(PCFBASM)
#EGAVGA=$(GLOBJ)gdevsco.$(OBJ) $(GLOBJ)gdevpcfb.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ) $(PCFBASM)

#**************** $(CCD) gdevevga.c
$(GLOBJ)gdevevga.$(OBJ): $(GLSRC)gdevevga.c $(GDEV) $(memory__h) $(gdevpcfb_h)
	$(GLCC) $(GLO_)gdevevga.$(OBJ) $(C_) $(GLSRC)gdevevga.c

$(GLOBJ)gdevsco.$(OBJ): $(GLSRC)gdevsco.c $(GDEV) $(memory__h) $(gdevpcfb_h)
	$(GLCC) $(GLO_)gdevsco.$(OBJ) $(C_) $(GLSRC)gdevsco.c

# Common code for MS-DOS and SCO.
#**************** $(CCD) gdevpcfb.c
$(GLOBJ)gdevpcfb.$(OBJ): $(GLSRC)gdevpcfb.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gdevpccm_h) $(gdevpcfb_h) $(gsparam_h)
	$(GLCC) $(GLO_)gdevpcfb.$(OBJ) $(C_) $(GLSRC)gdevpcfb.c

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

SVGA=$(GLOBJ)gdevsvga.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ) $(PCFBASM)

#**************** $(CCD) gdevsvga.c
$(GLOBJ)gdevsvga.$(OBJ): $(GLSRC)gdevsvga.c $(GDEV) $(memory__h) $(gconfigv_h)\
 $(gsparam_h) $(gxarith_h) $(gdevpccm_h) $(gdevpcfb_h) $(gdevsvga_h)
	$(GLCC) $(GLO_)gdevsvga.$(OBJ) $(C_) $(GLSRC)gdevsvga.c

# The SuperVGA family includes: Avance Logic Inc., ATI Wonder, S3,
# Trident, Tseng ET3000/4000, and VESA.

ali.dev: $(SVGA)
	$(SETDEV) ali $(SVGA)

atiw.dev: $(SVGA)
	$(SETDEV) atiw $(SVGA)

cirr.dev: $(SVGA)
	$(SETDEV) cirr $(SVGA)

tseng.dev: $(SVGA)
	$(SETDEV) tseng $(SVGA)

tvga.dev: $(SVGA)
	$(SETDEV) tvga $(SVGA)

vesa.dev: $(SVGA)
	$(SETDEV) vesa $(SVGA)

# The S3 driver doesn't share much code with the others.

s3vga_=$(GLOBJ)gdevs3ga.$(OBJ) $(GLOBJ)gdevsvga.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
s3vga.dev: $(SVGA) $(s3vga_)
	$(SETDEV) s3vga $(SVGA)
	$(ADDMOD) s3vga -obj $(s3vga_)

#**************** $(CCD) gdevs3ga.c
$(GLOBJ)gdevs3ga.$(OBJ): $(GLSRC)gdevs3ga.c $(GDEV) $(gdevpcfb_h) $(gdevsvga_h)
	$(GLCC) $(GLO_)gdevs3ga.$(OBJ) $(C_) $(GLSRC)gdevs3ga.c

###### ----------------------- Other displays ------------------------ ######

### ---------------------- Linux PC with vgalib ------------------------- ###
### Note: these drivers were contributed by users.                        ###
### For questions about the lvga256 driver, please contact                ###
###       Ludger Kunz (ludger.kunz@fernuni-hagen.de).                     ###
### For questions about the vgalib driver, please contact                 ###
###       Erik Talvola (talvola@gnu.ai.mit.edu).                          ###

lvga256_=$(GLOBJ)gdevl256.$(OBJ)
lvga256.dev: $(lvga256_)
	$(SETDEV) lvga256 $(lvga256_)
	$(ADDMOD) lvga256 -lib vga vgagl

$(GLOBJ)gdevl256.$(OBJ): $(GLSRC)gdevl256.c $(GDEV)
	$(GLCC) $(GLO_)gdevl256.$(OBJ) $(C_) $(GLSRC)gdevl256.c

vgalib_=$(GLOBJ)gdevvglb.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
vgalib.dev: $(vgalib_)
	$(SETDEV2) vgalib $(vgalib_)
	$(ADDMOD) vgalib -lib vga

$(GLOBJ)gdevvglb.$(OBJ): $(GLSRC)gdevvglb.c $(GDEV) $(gdevpccm_h) $(gsparam_h)
	$(GLCC) $(GLO_)gdevvglb.$(OBJ) $(C_) $(GLSRC)gdevvglb.c

### -------------------------- The X11 device -------------------------- ###

# Aladdin Enterprises does not support Ghostview.  For more information
# about Ghostview, please contact Tim Theisen (ghostview@cs.wisc.edu).

gdevx_h=$(GLSRC)gdevx.h

# See the main makefile for the definition of XLIBS.
x11_=$(GLOBJ)gdevx.$(OBJ) $(GLOBJ)gdevxini.$(OBJ) $(GLOBJ)gdevxxf.$(OBJ) $(GLOBJ)gdevemap.$(OBJ)
x11.dev: $(x11_)
	$(SETDEV2) x11 $(x11_)
	$(ADDMOD) x11 -lib $(XLIBS)

# See the main makefile for the definition of XINCLUDE.
GDEVX=$(GDEV) $(GLSRC)x_.h $(GLSRC)gdevx.h $(MAKEFILE)
$(GLOBJ)gdevx.$(OBJ): $(GLSRC)gdevx.c $(GDEVX) $(math__h) $(memory__h)\
 $(gscoord_h) $(gsdevice_h) $(gsiparm2_h) $(gsmatrix_h) $(gsparam_h)\
 $(gxgetbit_h) $(gxiparam_h) $(gxpath_h)
	$(GLCC) $(XINCLUDE) $(GLO_)gdevx.$(OBJ) $(C_) $(GLSRC)gdevx.c

$(GLOBJ)gdevxini.$(OBJ): $(GLSRC)gdevxini.c $(GDEVX) $(math__h) $(memory__h) $(gserrors_h)
	$(GLCC) $(XINCLUDE) $(GLO_)gdevxini.$(OBJ) $(C_) $(GLSRC)gdevxini.c

$(GLOBJ)gdevxxf.$(OBJ): $(GLSRC)gdevxxf.c $(GDEVX) $(math__h) $(memory__h)\
 $(gsstruct_h) $(gsutil_h) $(gxxfont_h)
	$(GLCC) $(XINCLUDE) $(GLO_)gdevxxf.$(OBJ) $(C_) $(GLSRC)gdevxxf.c

# Alternate X11-based devices to help debug other drivers.
# x11alpha pretends to have 4 bits of alpha channel.
# x11cmyk pretends to be a CMYK device with 1 bit each of C,M,Y,K.
# x11cmyk2 pretends to be a CMYK device with 2 bits each of C,M,Y,K.
# x11cmyk4 pretends to be a CMYK device with 4 bits each of C,M,Y,K.
# x11cmyk8 pretends to be a CMYK device with 8 bits each of C,M,Y,K.
# x11gray2 pretends to be a 2-bit gray-scale device.
# x11gray4 pretends to be a 4-bit gray-scale device.
# x11mono pretends to be a black-and-white device.
x11alt_=$(x11_) $(GLOBJ)gdevxalt.$(OBJ)
x11alpha.dev: $(x11alt_)
	$(SETDEV2) x11alpha $(x11alt_)
	$(ADDMOD) x11alpha -lib $(XLIBS)

x11cmyk.dev: $(x11alt_)
	$(SETDEV2) x11cmyk $(x11alt_)
	$(ADDMOD) x11cmyk -lib $(XLIBS)

x11cmyk2.dev: $(x11alt_)
	$(SETDEV2) x11cmyk2 $(x11alt_)
	$(ADDMOD) x11cmyk2 -lib $(XLIBS)

x11cmyk4.dev: $(x11alt_)
	$(SETDEV2) x11cmyk4 $(x11alt_)
	$(ADDMOD) x11cmyk4 -lib $(XLIBS)

x11cmyk8.dev: $(x11alt_)
	$(SETDEV2) x11cmyk8 $(x11alt_)
	$(ADDMOD) x11cmyk8 -lib $(XLIBS)

x11gray2.dev: $(x11alt_)
	$(SETDEV2) x11gray2 $(x11alt_)
	$(ADDMOD) x11gray2 -lib $(XLIBS)

x11gray4.dev: $(x11alt_)
	$(SETDEV2) x11gray4 $(x11alt_)
	$(ADDMOD) x11gray4 -lib $(XLIBS)

x11mono.dev: $(x11alt_)
	$(SETDEV2) x11mono $(x11alt_)
	$(ADDMOD) x11mono -lib $(XLIBS)

$(GLOBJ)gdevxalt.$(OBJ): $(GLSRC)gdevxalt.c $(GDEVX) $(math__h) $(memory__h) $(gsparam_h)
	$(GLCC) $(XINCLUDE) $(GLO_)gdevxalt.$(OBJ) $(C_) $(GLSRC)gdevxalt.c

###### --------------- Memory-buffered printer devices --------------- ######

### ----------- The H-P DeskJet and LaserJet printer devices ----------- ###

### These are essentially the same device.
### NOTE: printing at full resolution (300 DPI) requires a printer
###   with at least 1.5 Mb of memory.  150 DPI only requires .5 Mb.
### Note that the lj4dith driver is included with the H-P color printer
###   drivers below.

HPPCL=$(GLOBJ)gdevpcl.$(OBJ)
HPMONO=$(GLOBJ)gdevdjet.$(OBJ) $(HPPCL)

$(GLOBJ)gdevpcl.$(OBJ): $(GLSRC)gdevpcl.c $(PDEVH) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevpcl.$(OBJ) $(C_) $(GLSRC)gdevpcl.c

$(GLOBJ)gdevdjet.$(OBJ): $(GLSRC)gdevdjet.c $(PDEVH) $(gdevpcl_h)
	$(GLCC) $(GLO_)gdevdjet.$(OBJ) $(C_) $(GLSRC)gdevdjet.c

deskjet.dev: $(HPMONO) page.dev
	$(SETPDEV2) deskjet $(HPMONO)

djet500.dev: $(HPMONO) page.dev
	$(SETPDEV2) djet500 $(HPMONO)

laserjet.dev: $(HPMONO) page.dev
	$(SETPDEV2) laserjet $(HPMONO)

ljetplus.dev: $(HPMONO) page.dev
	$(SETPDEV2) ljetplus $(HPMONO)

### Selecting ljet2p provides TIFF (mode 2) compression on LaserJet III,
### IIIp, IIId, IIIsi, IId, and IIp. 

ljet2p.dev: $(HPMONO) page.dev
	$(SETPDEV2) ljet2p $(HPMONO)

### Selecting ljet3 provides Delta Row (mode 3) compression on LaserJet III,
### IIIp, IIId, IIIsi.

ljet3.dev: $(HPMONO) page.dev
	$(SETPDEV2) ljet3 $(HPMONO)

### Selecting ljet3d also provides duplex printing capability.

ljet3d.dev: $(HPMONO) page.dev
	$(SETPDEV2) ljet3d $(HPMONO)

### Selecting ljet4 also provides Delta Row compression on LaserJet IV series.

ljet4.dev: $(HPMONO) page.dev
	$(SETPDEV2) ljet4 $(HPMONO)

lp2563.dev: $(HPMONO) page.dev
	$(SETPDEV2) lp2563 $(HPMONO)

oce9050.dev: $(HPMONO) page.dev
	$(SETPDEV2) oce9050 $(HPMONO)

### ------------------ The H-P LaserJet 5 and 6 devices ----------------- ###

### These drivers use H-P's new PCL XL printer language, like H-P's
### LaserJet 5 Enhanced driver for MS Windows.  We don't recommend using
### them:
###	- If you have a LJ 5L or 5P, which isn't a "real" LaserJet 5,
###	use the ljet4 driver instead.  (The lj5 drivers won't work.)
###	- If you have any other model of LJ 5 or 6, use the pxlmono
###	driver, which often produces much more compact output.

gdevpxat_h=$(GLSRC)gdevpxat.h
gdevpxen_h=$(GLSRC)gdevpxen.h
gdevpxop_h=$(GLSRC)gdevpxop.h

ljet5_=$(GLOBJ)gdevlj56.$(OBJ) $(HPPCL)
lj5mono.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5mono $(ljet5_)

lj5gray.dev: $(ljet5_) page.dev
	$(SETPDEV) lj5gray $(ljet5_)

$(GLOBJ)gdevlj56.$(OBJ): $(GLSRC)gdevlj56.c $(PDEVH) $(gdevpcl_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h)
	$(GLCC) $(GLO_)gdevlj56.$(OBJ) $(C_) $(GLSRC)gdevlj56.c

###### ------------------- High-level file formats ------------------- ######

# Support for PostScript and PDF

gdevpsdf_h=$(GLSRC)gdevpsdf.h $(gdevvec_h) $(gsparam_h) $(scfx_h) $(strimpl_h)
gdevpsds_h=$(GLSRC)gdevpsds.h $(strimpl_h)
gdevpstr_h=$(GLSRC)gdevpstr.h

psdf_1=$(GLOBJ)gdevpsde.$(OBJ) $(GLOBJ)gdevpsdf.$(OBJ) $(GLOBJ)gdevpsdi.$(OBJ)
psdf_2=$(GLOBJ)gdevpsdp.$(OBJ) $(GLOBJ)gdevpsds.$(OBJ) $(GLOBJ)gdevpstr.$(OBJ)
psdf_3=$(GLOBJ)scfparam.$(OBJ) $(GLOBJ)sdcparam.$(OBJ) $(GLOBJ)sdeparam.$(OBJ)
psdf_=$(psdf_1) $(psdf_2) $(psdf_3)
psdf.dev: $(ECHOGS_XE) $(psdf_) vector.dev
	$(SETMOD) psdf $(psdf_1)
	$(ADDMOD) psdf -obj $(psdf_2)
	$(ADDMOD) psdf -obj $(psdf_3)
	$(ADDMOD) psdf -include vector

$(GLOBJ)gdevpsde.$(OBJ): $(GLSRC)gdevpsde.c $(GXERR) $(memory__h)\
 $(gsccode_h) $(gsmatrix_h) $(gxfixed_h) $(gxfont_h) $(gxfont1_h)\
 $(stream_h)\
 $(gdevpsdf_h) $(gdevpstr_h)
	$(GLCC) $(GLO_)gdevpsde.$(OBJ) $(C_) $(GLSRC)gdevpsde.c

$(GLOBJ)gdevpsdf.$(OBJ): $(GLSRC)gdevpsdf.c $(GXERR) $(string__h)\
 $(sa85x_h) $(scanchar_h) $(scfx_h) $(sstring_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)
	$(GLCC) $(GLO_)gdevpsdf.$(OBJ) $(C_) $(GLSRC)gdevpsdf.c

$(GLOBJ)gdevpsdi.$(OBJ): $(GLSRC)gdevpsdi.c $(GXERR) $(math__h)\
 $(gscspace_h)\
 $(scfx_h) $(sdct_h) $(slzwx_h) $(srlx_h) $(spngpx_h)\
 $(strimpl_h) $(szlibx_h)\
 $(gdevpsdf_h) $(gdevpsds_h)\
 $(GLGEN)jpeglib.h
	$(GLJCC) $(GLO_)gdevpsdi.$(OBJ) $(C_) $(GLSRC)gdevpsdi.c

$(GLOBJ)gdevpsdp.$(OBJ): $(GLSRC)gdevpsdp.c $(GDEVH) $(string__h)\
 $(scfx_h) $(sdct_h) $(slzwx_h) $(srlx_h) $(strimpl_h) $(szlibx_h)\
 $(gdevpsdf_h) $(gdevpstr_h) $(GLGEN)jpeglib.h
	$(GLJCC) $(GLO_)gdevpsdp.$(OBJ) $(C_) $(GLSRC)gdevpsdp.c

$(GLOBJ)gdevpsds.$(OBJ): $(GLSRC)gdevpsds.c $(GX) $(memory__h)\
 $(gdevpsds_h) $(gserrors_h) $(gxdcconv_h)
	$(GLCC) $(GLO_)gdevpsds.$(OBJ) $(C_) $(GLSRC)gdevpsds.c

$(GLOBJ)gdevpstr.$(OBJ): $(GLSRC)gdevpstr.c\
 $(math__h) $(stdio__h) $(string__h)\
 $(gdevpstr_h) $(stream_h)
	$(GLCC) $(GLO_)gdevpstr.$(OBJ) $(C_) $(GLSRC)gdevpstr.c

# PostScript and EPS writers

pswrite_=$(GLOBJ)gdevps.$(OBJ) $(GLOBJ)scantab.$(OBJ) $(GLOBJ)sfilter2.$(OBJ)
epswrite.dev: $(ECHOGS_XE) $(pswrite_) psdf.dev
	$(SETDEV2) epswrite $(pswrite_)
	$(ADDMOD) epswrite -include psdf

pswrite.dev: $(ECHOGS_XE) $(pswrite_) psdf.dev
	$(SETDEV2) pswrite $(pswrite_)
	$(ADDMOD) pswrite -include psdf

$(GLOBJ)gdevps.$(OBJ): $(GLSRC)gdevps.c $(GDEV)\
 $(math__h) $(memory__h) $(time__h)\
 $(gscdefs_h) $(gscspace_h) $(gsline_h) $(gsparam_h) $(gsiparam_h) $(gsmatrix_h)\
 $(gxdcolor_h) $(gzpath_h)\
 $(sa85x_h) $(strimpl_h)\
 $(gdevpsdf_h) $(gdevpstr_h)
	$(GLCC) $(GLO_)gdevps.$(OBJ) $(C_) $(GLSRC)gdevps.c

# PDF writer
# Note that gs_pdfwr.ps will only actually be loaded if the configuration
# includes a PostScript interpreter.

pdfwrite1_=$(GLOBJ)gdevpdf.$(OBJ) $(GLOBJ)gdevpdfd.$(OBJ) $(GLOBJ)gdevpdfi.$(OBJ)
pdfwrite2_=$(GLOBJ)gdevpdfm.$(OBJ) $(GLOBJ)gdevpdfo.$(OBJ) $(GLOBJ)gdevpdfp.$(OBJ)
pdfwrite3_=$(GLOBJ)gdevpdft.$(OBJ) $(GLOBJ)gsflip.$(OBJ)
pdfwrite4_=$(GLOBJ)scantab.$(OBJ) $(GLOBJ)sfilter2.$(OBJ) $(GLOBJ)sstring.$(OBJ)
pdfwrite_=$(pdfwrite1_) $(pdfwrite2_) $(pdfwrite3_) $(pdfwrite4_)
pdfwrite.dev: $(ECHOGS_XE) $(pdfwrite_)\
 cmyklib.dev cfe.dev dcte.dev lzwe.dev rle.dev szlibe.dev psdf.dev
	$(SETDEV2) pdfwrite $(pdfwrite1_)
	$(ADDMOD) pdfwrite $(pdfwrite2_)
	$(ADDMOD) pdfwrite $(pdfwrite3_)
	$(ADDMOD) pdfwrite $(pdfwrite4_)
	$(ADDMOD) pdfwrite -ps gs_pdfwr
	$(ADDMOD) pdfwrite -include cmyklib cfe dcte lzwe rle szlibe psdf

gdevpdfx_h=$(GLSRC)gdevpdfx.h\
 $(gsparam_h) $(gxdevice_h) $(gxline_h) $(stream_h)\
 $(gdevpsdf_h) $(gdevpstr_h)

$(GLOBJ)gdevpdf.$(OBJ): $(GLSRC)gdevpdf.c $(GDEVH)\
 $(math__h) $(memory__h) $(string__h) $(time__h)\
 $(gp_h)\
 $(gdevpdfx_h) $(gscdefs_h)\
 $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)\
 $(scanchar_h) $(scfx_h) $(slzwx_h) $(sstring_h) $(strimpl_h) $(szlibx_h)
	$(GLCC) $(GLO_)gdevpdf.$(OBJ) $(C_) $(GLSRC)gdevpdf.c

$(GLOBJ)gdevpdfd.$(OBJ): $(GLSRC)gdevpdfd.c $(math__h)\
 $(gdevpdfx_h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxistate_h) $(gxpaint_h)\
 $(gzcpath_h) $(gzpath_h)
	$(GLCC) $(GLO_)gdevpdfd.$(OBJ) $(C_) $(GLSRC)gdevpdfd.c

$(GLOBJ)gdevpdfi.$(OBJ): $(GLSRC)gdevpdfi.c\
 $(math__h) $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfx_h)\
 $(gscie_h) $(gscolor2_h) $(gserrors_h) $(gsflip_h)\
 $(gxcspace_h) $(gxistate_h)\
 $(sa85x_h) $(scfx_h) $(sdct_h) $(slzwx_h) $(spngpx_h) $(srlx_h) $(strimpl_h)\
 $(szlibx_h)\
 $(GLGEN)jpeglib.h
	$(GLJCC) $(GLO_)gdevpdfi.$(OBJ) $(C_) $(GLSRC)gdevpdfi.c

$(GLOBJ)gdevpdfm.$(OBJ): $(GLSRC)gdevpdfm.c\
 $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h) $(gsutil_h) $(scanchar_h)
	$(GLCC) $(GLO_)gdevpdfm.$(OBJ) $(C_) $(GLSRC)gdevpdfm.c

$(GLOBJ)gdevpdfo.$(OBJ): $(GLSRC)gdevpdfo.c $(memory__h) $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(sstring_h) $(strimpl_h)
	$(GLCC) $(GLO_)gdevpdfo.$(OBJ) $(C_) $(GLSRC)gdevpdfo.c

$(GLOBJ)gdevpdfp.$(OBJ): $(GLSRC)gdevpdfp.c $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h)
	$(GLCC) $(GLO_)gdevpdfp.$(OBJ) $(C_) $(GLSRC)gdevpdfp.c

$(GLOBJ)gdevpdft.$(OBJ): $(GLSRC)gdevpdft.c\
 $(math__h) $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(scommon_h)
	$(GLCC) $(GLO_)gdevpdft.$(OBJ) $(C_) $(GLSRC)gdevpdft.c

# High-level PCL XL writer

pxl_=$(GLOBJ)gdevpx.$(OBJ)
pxlmono.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV2) pxlmono $(pxl_)
	$(ADDMOD) pxlmono -include vector

pxlcolor.dev: $(pxl_) $(GDEV) vector.dev
	$(SETDEV2) pxlcolor $(pxl_)
	$(ADDMOD) pxlcolor -include vector

$(GLOBJ)gdevpx.$(OBJ): $(GLSRC)gdevpx.c\
 $(math__h) $(memory__h) $(string__h)\
 $(gx_h) $(gsccolor_h) $(gsdcolor_h) $(gserrors_h)\
 $(gxcspace_h) $(gxdevice_h) $(gxpath_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevvec_h)\
 $(srlx_h) $(strimpl_h)
	$(GLCC) $(GLO_)gdevpx.$(OBJ) $(C_) $(GLSRC)gdevpx.c

###### --------------------- Raster file formats --------------------- ######

### --------------------- The "plain bits" devices ---------------------- ###

bit_=$(GLOBJ)gdevbit.$(OBJ)

bit.dev: $(bit_) page.dev
	$(SETPDEV2) bit $(bit_)

bitrgb.dev: $(bit_) page.dev
	$(SETPDEV2) bitrgb $(bit_)

bitcmyk.dev: $(bit_) page.dev
	$(SETPDEV2) bitcmyk $(bit_)

$(GLOBJ)gdevbit.$(OBJ): $(GLSRC)gdevbit.c $(PDEVH)\
 $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)
	$(GLCC) $(GLO_)gdevbit.$(OBJ) $(C_) $(GLSRC)gdevbit.c

### ------------------------- .BMP file formats ------------------------- ###

gdevbmp_h=$(GLSRC)gdevbmp.h

bmp_=$(GLOBJ)gdevbmp.$(OBJ) $(GLOBJ)gdevbmpc.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)

$(GLOBJ)gdevbmp.$(OBJ): $(GLSRC)gdevbmp.c $(PDEVH) $(gdevbmp_h) $(gdevpccm_h)
	$(GLCC) $(GLO_)gdevbmp.$(OBJ) $(C_) $(GLSRC)gdevbmp.c

$(GLOBJ)gdevbmpc.$(OBJ): $(GLSRC)gdevbmpc.c $(PDEVH) $(gdevbmp_h)
	$(GLCC) $(GLO_)gdevbmpc.$(OBJ) $(C_) $(GLSRC)gdevbmpc.c

bmpmono.dev: $(bmp_) page.dev
	$(SETPDEV) bmpmono $(bmp_)

bmp16.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16 $(bmp_)

bmp256.dev: $(bmp_) page.dev
	$(SETPDEV) bmp256 $(bmp_)

bmp16m.dev: $(bmp_) page.dev
	$(SETPDEV) bmp16m $(bmp_)

### ------------- BMP driver that serves as demo of async rendering ---- ###

bmpa_=$(GLOBJ)gdevbmpa.$(OBJ) $(GLOBJ)gdevbmpc.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)

$(GLOBJ)gdevbmpa.$(OBJ): $(GLSRC)gdevbmpa.c $(AK) $(stdio__h)\
 $(gdevbmp_h) $(gdevprna_h) $(gdevpccm_h) $(gserrors_h) $(gpsync_h)
	$(GLCC) $(GLO_)gdevbmpa.$(OBJ) $(C_) $(GLSRC)gdevbmpa.c

bmpamono.dev: $(bmpa_) page.dev async.dev
	$(SETPDEV) bmpamono $(bmpa_)
	$(ADDMOD) bmpamono -include async

### -------------------------- CGM file format ------------------------- ###
### This driver is under development.  Use at your own risk.             ###
### The output is very low-level, consisting only of rectangles and      ###
### cell arrays.                                                         ###

cgm_=$(GLOBJ)gdevcgm.$(OBJ) $(GLOBJ)gdevcgml.$(OBJ)

gdevcgml_h=$(GLSRC)gdevcgml.h
gdevcgmx_h=$(GLSRC)gdevcgmx.h $(gdevcgml_h)

$(GLOBJ)gdevcgm.$(OBJ): $(GLSRC)gdevcgm.c $(GDEV) $(memory__h)\
 $(gsparam_h) $(gdevpccm_h) $(gdevcgml_h)
	$(GLCC) $(GLO_)gdevcgm.$(OBJ) $(C_) $(GLSRC)gdevcgm.c

$(GLOBJ)gdevcgml.$(OBJ): $(GLSRC)gdevcgml.c $(memory__h) $(stdio__h)\
 $(gdevcgmx_h)
	$(GLCC) $(GLO_)gdevcgml.$(OBJ) $(C_) $(GLSRC)gdevcgml.c

cgmmono.dev: $(cgm_)
	$(SETDEV) cgmmono $(cgm_)

cgm8.dev: $(cgm_)
	$(SETDEV) cgm8 $(cgm_)

cgm24.dev: $(cgm_)
	$(SETDEV) cgm24 $(cgm_)

### ------------------------- JPEG file format ------------------------- ###

jpeg_=$(GLOBJ)gdevjpeg.$(OBJ)

# RGB output
jpeg.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV2) jpeg $(jpeg_)
	$(ADDMOD) jpeg -include sdcte

# Gray output
jpeggray.dev: $(jpeg_) sdcte.dev page.dev
	$(SETPDEV2) jpeggray $(jpeg_)
	$(ADDMOD) jpeggray -include sdcte

$(GLOBJ)gdevjpeg.$(OBJ): $(GLSRC)gdevjpeg.c $(stdio__h) $(PDEVH)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h) $(GLGEN)jpeglib.h
	$(GLCC) $(GLO_)gdevjpeg.$(OBJ) $(C_) $(GLSRC)gdevjpeg.c

### ------------------------- MIFF file format ------------------------- ###
### Right now we support only 24-bit direct color, but we might add more ###
### formats in the future.                                               ###

miff_=$(GLOBJ)gdevmiff.$(OBJ)

miff24.dev: $(miff_) page.dev
	$(SETPDEV) miff24 $(miff_)

$(GLOBJ)gdevmiff.$(OBJ): $(GLSRC)gdevmiff.c $(PDEVH)
	$(GLCC) $(GLO_)gdevmiff.$(OBJ) $(C_) $(GLSRC)gdevmiff.c

### ------------------------- PCX file formats ------------------------- ###

pcx_=$(GLOBJ)gdevpcx.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)

$(GLOBJ)gdevpcx.$(OBJ): $(GLSRC)gdevpcx.c $(PDEVH) $(gdevpccm_h) $(gxlum_h)
	$(GLCC) $(GLO_)gdevpcx.$(OBJ) $(C_) $(GLSRC)gdevpcx.c

pcxmono.dev: $(pcx_) page.dev
	$(SETPDEV2) pcxmono $(pcx_)

pcxgray.dev: $(pcx_) page.dev
	$(SETPDEV2) pcxgray $(pcx_)

pcx16.dev: $(pcx_) page.dev
	$(SETPDEV2) pcx16 $(pcx_)

pcx256.dev: $(pcx_) page.dev
	$(SETPDEV2) pcx256 $(pcx_)

pcx24b.dev: $(pcx_) page.dev
	$(SETPDEV2) pcx24b $(pcx_)

pcxcmyk.dev: $(pcx_) page.dev
	$(SETPDEV2) pcxcmyk $(pcx_)

# The 2-up PCX device is here only as an example, and for testing.

pcx2up.dev: $(LIB_MAK) $(ECHOGS_XE) gdevp2up.$(OBJ) page.dev pcx256.dev
	$(SETPDEV) pcx2up $(GLOBJ)gdevp2up.$(OBJ)
	$(ADDMOD) pcx2up -include pcx256

$(GLOBJ)gdevp2up.$(OBJ): $(GLSRC)gdevp2up.c $(AK)\
 $(gdevpccm_h) $(gdevprn_h) $(gxclpage_h)
	$(GLCC) $(GLO_)gdevp2up.$(OBJ) $(C_) $(GLSRC)gdevp2up.c

### ------------------- Portable Bitmap file formats ------------------- ###
### For more information, see the pbm(5), pgm(5), and ppm(5) man pages.  ###

pxm_=$(GLOBJ)gdevpbm.$(OBJ)

$(GLOBJ)gdevpbm.$(OBJ): $(GLSRC)gdevpbm.c $(PDEVH) $(gscdefs_h) $(gxlum_h)
	$(GLCC) $(GLO_)gdevpbm.$(OBJ) $(C_) $(GLSRC)gdevpbm.c

### Portable Bitmap (PBM, plain or raw format, magic numbers "P1" or "P4")

pbm.dev: $(pxm_) page.dev
	$(SETPDEV2) pbm $(pxm_)

pbmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) pbmraw $(pxm_)

### Portable Graymap (PGM, plain or raw format, magic numbers "P2" or "P5")

pgm.dev: $(pxm_) page.dev
	$(SETPDEV2) pgm $(pxm_)

pgmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) pgmraw $(pxm_)

# PGM with automatic optimization to PBM if this is possible.

pgnm.dev: $(pxm_) page.dev
	$(SETPDEV2) pgnm $(pxm_)

pgnmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) pgnmraw $(pxm_)

### Portable Pixmap (PPM, plain or raw format, magic numbers "P3" or "P6")

ppm.dev: $(pxm_) page.dev
	$(SETPDEV2) ppm $(pxm_)

ppmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) ppmraw $(pxm_)

# PPM with automatic optimization to PGM or PBM if possible.

pnm.dev: $(pxm_) page.dev
	$(SETPDEV2) pnm $(pxm_)

pnmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) pnmraw $(pxm_)

### Portable inKmap (CMYK internally, converted to PPM=RGB at output time)

pkm.dev: $(pxm_) page.dev
	$(SETPDEV2) pkm $(pxm_)

pkmraw.dev: $(pxm_) page.dev
	$(SETPDEV2) pkmraw $(pxm_)

### Plan 9 bitmap format

plan9bm.dev: $(pxm_) page.dev
	$(SETPDEV2) plan9bm $(pxm_)

### --------------- Portable Network Graphics file format --------------- ###
### Requires libpng 0.81 and zlib 0.95 (or more recent versions).         ###
### See libpng.mak and zlib.mak for more details.                         ###

png_=$(GLOBJ)gdevpng.$(OBJ) $(GLOBJ)gdevpccm.$(OBJ)
libpng_dev=$(PNGGENDIR)$(D)libpng.dev
png_i_=-include $(PNGGENDIR)$(D)libpng

$(GLOBJ)gdevpng.$(OBJ): $(GLSRC)gdevpng.c\
 $(gdevprn_h) $(gdevpccm_h) $(gscdefs_h) $(PNGSRC)png.h
	$(CC_) $(I_)$(GLI_) $(II)$(PI_)$(_I) $(GLF_) $(GLO_)gdevpng.$(OBJ) $(C_) $(GLSRC)gdevpng.c

pngmono.dev: $(libpng_dev) $(png_) page.dev
	$(SETPDEV2) pngmono $(png_)
	$(ADDMOD) pngmono $(png_i_)

pnggray.dev: $(libpng_dev) $(png_) page.dev
	$(SETPDEV2) pnggray $(png_)
	$(ADDMOD) pnggray $(png_i_)

png16.dev: $(libpng_dev) $(png_) page.dev
	$(SETPDEV2) png16 $(png_)
	$(ADDMOD) png16 $(png_i_)

png256.dev: $(libpng_dev) $(png_) page.dev
	$(SETPDEV2) png256 $(png_)
	$(ADDMOD) png256 $(png_i_)

png16m.dev: $(libpng_dev) $(png_) page.dev
	$(SETPDEV2) png16m $(png_)
	$(ADDMOD) png16m $(png_i_)

### ---------------------- PostScript image format ---------------------- ###
### These devices make it possible to print monochrome Level 2 files on a ###
###   Level 1 printer, by converting them to a bitmap in PostScript       ###
###   format.  They also can convert big, complex color PostScript files  ###
###   to (often) smaller and more easily printed bitmaps.                 ###

# Monochrome, Level 1 output

psim_=$(GLOBJ)gdevpsim.$(OBJ)

$(GLOBJ)gdevpsim.$(OBJ): $(GLSRC)gdevpsim.c $(PDEVH)
	$(GLCC) $(GLO_)gdevpsim.$(OBJ) $(C_) $(GLSRC)gdevpsim.c

psmono.dev: $(psim_) page.dev
	$(SETPDEV2) psmono $(psim_)

psgray.dev: $(psim_) page.dev
	$(SETPDEV2) psgray $(psim_)

# RGB, Level 2 output

psci_=$(GLOBJ)gdevpsci.$(OBJ)

$(GLOBJ)gdevpsci.$(OBJ): $(GLSRC)gdevpsci.c $(PDEVH)\
 $(srlx_h) $(stream_h) $(strimpl_h)
	$(GLCC) $(GLO_)gdevpsci.$(OBJ) $(C_) $(GLSRC)gdevpsci.c

psrgb.dev: $(psci_) page.dev
	$(SETPDEV2) psrgb $(psci_)

### -------------------- Plain or TIFF fax encoding --------------------- ###
###    Use -sDEVICE=tiffg3 or tiffg4 and				  ###
###	  -r204x98 for low resolution output, or			  ###
###	  -r204x196 for high resolution output				  ###
###    These drivers recognize 3 page sizes: letter, A4, and B4.	  ###

gdevtifs_h=$(GLSRC)gdevtifs.h

tfax_=$(GLOBJ)gdevtfax.$(OBJ)
tfax.dev: $(tfax_) cfe.dev lzwe.dev rle.dev tiffs.dev
	$(SETMOD) tfax $(tfax_)
	$(ADDMOD) tfax -include cfe lzwe rle tiffs

$(GLOBJ)gdevtfax.$(OBJ): $(GLSRC)gdevtfax.c $(PDEVH)\
 $(gdevtifs_h) $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h)
	$(GLCC) $(GLO_)gdevtfax.$(OBJ) $(C_) $(GLSRC)gdevtfax.c

### Plain G3/G4 fax with no header

faxg3.dev: tfax.dev
	$(SETDEV2) faxg3 -include tfax

faxg32d.dev: tfax.dev
	$(SETDEV2) faxg32d -include tfax

faxg4.dev: tfax.dev
	$(SETDEV2) faxg4 -include tfax

### ---------------------------- TIFF formats --------------------------- ###

tiffs_=$(GLOBJ)gdevtifs.$(OBJ)
tiffs.dev: $(tiffs_) page.dev
	$(SETMOD) tiffs $(tiffs_)
	$(ADDMOD) tiffs -include page

$(GLOBJ)gdevtifs.$(OBJ): $(GLSRC)gdevtifs.c $(PDEVH) $(stdio__h) $(time__h)\
 $(gdevtifs_h) $(gscdefs_h) $(gstypes_h)
	$(GLCC) $(GLO_)gdevtifs.$(OBJ) $(C_) $(GLSRC)gdevtifs.c

# Black & white, G3/G4 fax

tiffcrle.dev: tfax.dev
	$(SETDEV2) tiffcrle -include tfax

tiffg3.dev: tfax.dev
	$(SETDEV2) tiffg3 -include tfax

tiffg32d.dev: tfax.dev
	$(SETDEV2) tiffg32d -include tfax

tiffg4.dev: tfax.dev
	$(SETDEV2) tiffg4 -include tfax

# Black & white, LZW compression

tifflzw.dev: tfax.dev
	$(SETDEV2) tifflzw -include tfax

# Black & white, PackBits compression

tiffpack.dev: tfax.dev
	$(SETDEV2) tiffpack -include tfax

# RGB, no compression

tiffrgb_=$(GLOBJ)gdevtfnx.$(OBJ)

tiff12nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV2) tiff12nc $(tiffrgb_)
	$(ADDMOD) tiff12nc -include tiffs

tiff24nc.dev: $(tiffrgb_) tiffs.dev
	$(SETPDEV2) tiff24nc $(tiffrgb_)
	$(ADDMOD) tiff24nc -include tiffs

$(GLOBJ)gdevtfnx.$(OBJ): $(GLSRC)gdevtfnx.c $(PDEVH) $(gdevtifs_h)
	$(GLCC) $(GLO_)gdevtfnx.$(OBJ) $(C_) $(GLSRC)gdevtfnx.c
