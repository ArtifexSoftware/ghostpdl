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
# makefile for Artifex's device drivers.

# Define the name of this makefile.
DEVS_MAK=$(DEVSRC)devs.mak $(TOP_MAKEFILES)

DEVSRC=$(DEVSRCDIR)$(D)
DEVVEC=$(DEVSRC)vector
DEVVECSRC=$(DEVVEC)$(D)

DEVI_=$(DEVGENDIR) $(II)$(GLSRCDIR) $(II)$(GLGENDIR) $(II)$(DEVSRCDIR)
DEVF_=

DEVCCFLAGS=$(I_)$(DEVI_)$(_I) $(I_)$(DEVVEC)$(_I) $(DEVF_)
DEVCC=$(CC_) $(DEVCCFLAGS)
XPSDEVCC=$(CC_) $(XPSPRINTCFLAGS) $(DEVCCFLAGS)

DEVJCC=$(GLJCC)
DEVCCSHARED=$(GLCCSHARED)

# All device drivers depend on the following:
GDEVH=$(gserrors_h) $(gx_h) $(gxdevice_h)
GDEV=$(AK) $(ECHOGS_XE) $(GDEVH)

DEVOBJ=$(DEVOBJDIR)$(D)
DEVO_=$(O_)$(DEVOBJ)

DEVGEN=$(DEVGENDIR)$(D)

###### --------------------------- Overview -------------------------- ######

# It is possible to build Ghostscript with an arbitrary collection of device
# drivers, although some drivers are supported only on a subset of the
# target platforms.

# The catalog in this file, devs.mak, lists all the drivers that were
# written by Artifex, or by people working closely with Artifex, and for
# which Artifex is willing to take problem reports (although since
# Ghostscript is provided with NO WARRANTY and NO SUPPORT, we can't promise
# that we'll solve your problem).  Another file, contrib.mak, lists all the
# drivers contributed by other people that are distributed by Artifex with
# Ghostscript.  Note in particular that all drivers for color inkjets and
# other non-PostScript-capable color printers are in contrib.mak.

# If you haven't configured Ghostscript before, or if you want to add a
# driver that that isn't included in the catalogs (for which you have the
# source code), we suggest you skip to the "End of catalog" below and read
# the documentation there before continuing.

###### --------------------------- Catalog -------------------------- ######

# MS-DOS displays (note: not usable with Desqview/X):
# Other displays:
#	display		For use on any platform that supports DLLs
#   MS Windows:
#	mswinpr2	Microsoft Windows 3.0, 3.1 DIB printer  [MS Windows only]
#   Unix and VMS:
#	x11		X Windows version 11, release >=4   [Unix and VMS only]
#	x11alpha	X Windows masquerading as a device with alpha capability
#	x11cmyk		X Windows masquerading as a 1-bit-per-plane CMYK device
#	x11cmyk2 	X Windows as a 2-bit-per-plane CMYK device
#	x11cmyk4	X Windows as a 4-bit-per-plane CMYK device
#	x11cmyk8	X Windows as an 8-bit-per-plane CMYK device
#	x11gray2	X Windows as a 2-bit gray-scale device
#	x11gray4 	X Windows as a 4-bit gray-scale device
#	x11mono		X Windows masquerading as a black-and-white device
#	x11rg16x 	X Windows with G5/B5/R6 pixel layout for testing.
#	x11rg32x	X Windows with G11/B10/R11 pixel layout for testing.
# Printers:
# +	atx23		Practical Automation ATX-23 label printer
# +	atx24		Practical Automation ATX-24 label printer
# +	atx38		Practical Automation ATX-38 label printer
# +	itk24i		Practical Automation ITK-24i thermal kiosk printer
# +	itk38		Practical Automation ITK-38 thermal kiosk printer
# +	deskjet		H-P DeskJet and DeskJet Plus
#	djet500		H-P DeskJet 500; use -r600 for DJ 600 series
# +	fs600		Kyocera FS-600 (600 dpi)
# +	laserjet	H-P LaserJet
# +	ljet2p		H-P LaserJet IId/IIp/III* with TIFF compression
# +	ljet3		H-P LaserJet III* with Delta Row compression
# +	ljet3d		H-P LaserJet IIID with duplex capability
# +	ljet4		H-P LaserJet 4 (defaults to 600 dpi)
# +	ljet4d		H-P LaserJet 4 (defaults to 600 dpi) with duplex
# +	ljetplus	H-P LaserJet Plus
#	lj5mono		H-P LaserJet 5 & 6 family (PCL XL), bitmap:
#			see below for restrictions & advice
#	lj5gray		H-P LaserJet 5 & 6 family, gray-scale bitmap;
#			see below for restrictions & advice
# *	lp2563		H-P 2563B line printer
# *	oce9050		OCE 9050 printer
#	(pxlmono)	H-P black-and-white PCL XL printers (LaserJet 5 and 6 family)
#	(pxlcolor)	H-P color PCL XL printers (e.g. Color LaserJet 4500)
# Fax file format:
#   ****** NOTE: all of these drivers normally adjust the page size to match
#   ****** one of the three CCITT standard sizes (U.S. letter with A4 width,
#   ****** A4, or B4).  To suppress this, use -dAdjustWidth=0.
#	faxg3		Group 3 fax, with EOLs but no header or EOD
#	faxg32d		Group 3 2-D fax, with EOLs but no header or EOD
#	faxg4		Group 4 fax, with EOLs but no header or EOD
#	tiffcrle	TIFF "CCITT RLE 1-dim" (= Group 3 fax with no EOLs)
#	tiffg3		TIFF Group 3 fax (with EOLs)
#	tiffg32d	TIFF Group 3 2-D fax
#	tiffg4		TIFF Group 4 fax
# High-level file formats:
#	pdfwrite	PDF output (like Adobe Acrobat Distiller)
#	txtwrite	ASCII or Unicode text output
#	pxlmono 	Black-and-white PCL XL
#	pxlcolor	Color PCL XL
# Other raster file formats and devices:
#	bit		Plain bits, monochrome
#	bitrgb		Plain bits, RGB
#	bitcmyk		Plain bits, CMYK
#	bmpmono		Monochrome MS Windows .BMP file format
#	bmpgray		8-bit gray .BMP file format
#	bmpsep1		Separated 1-bit CMYK .BMP file format, primarily for testing
#	bmpsep8		Separated 8-bit CMYK .BMP file format, primarily for testing
#	bmp16		4-bit (EGA/VGA) .BMP file format
#	bmp256		8-bit (256-color) .BMP file format
#	bmp16m		24-bit .BMP file format
#	bmp32b		32-bit pseudo-.BMP file format
#	chameleon	Plain bits, rgb/mono/cmyk runtime configurable.
#	jpeg		JPEG format, RGB output
#	jpeggray	JPEG format, gray output
#	jpegcmyk	JPEG format, cmyk output
#	miff24		ImageMagick MIFF format, 24-bit direct color, RLE compressed
#	pamcmyk4 	Portable Arbitrary Map file format 4-bit CMYK
#	pamcmyk32	Portable Arbitrary Map file format 32-bit CMYK
#	pcxmono		PCX file format, monochrome (1-bit black and white)
#	pcxgray		PCX file format, 8-bit gray scale
#	pcx16		PCX file format, 4-bit planar (EGA/VGA) color
#	pcx256		PCX file format, 8-bit chunky color
#	pcx24b		PCX file format, 24-bit color (3 8-bit planes)
#	pcxcmyk		PCX file format, 4-bit chunky CMYK color
#	pbm		Portable Bitmap (plain format)
#	pbmraw		Portable Bitmap (raw format)
#	pgm		Portable Graymap (plain format)
#	pgmraw		Portable Graymap (raw format)
#	pgnm		Portable Graymap (plain format), optimizing to PBM if possible
#	pgnmraw		Portable Graymap (raw format), optimizing to PBM if possible
#	pnm		Portable Pixmap (plain format) (RGB), optimizing to PGM or PBM
#			if possible
#	pnmraw		Portable Pixmap (raw format) (RGB), optimizing to PGM or PBM
#			if possible
#       pnmcmyk         PAM 32-bit CMYK if colors, otherwise pgmraw.
#	ppm		Portable Pixmap (plain format) (RGB)
#	ppmraw		Portable Pixmap (raw format) (RGB)
#	pkm		Portable inKmap (plain format) (4-bit CMYK => RGB)
#	pkmraw		Portable inKmap (raw format) (4-bit CMYK => RGB)
#	pksm		Portable Separated map (plain format) (4-bit CMYK => 4 pages)
#	pksmraw		Portable Separated map (raw format) (4-bit CMYK => 4 pages)
# *	plan9bm		Plan 9 bitmap format
#	plan		PLANar device (24 bit RGB)
#	planm		PLANar device (1 bit Mono)
#	plang		PLANar device (8 bit Gray)
#	planc		PLANar device (32 bit CMYK)
#	plank		PLANar device (4 bit CMYK)
#	planr		PLANar device (3 bit RGB)
#	plib		PLanar Interleaved Band buffer device (24 bit RGB)
#	plibm		PLanar Interleaved Band buffer device (1 bit Mono)
#	plibg		PLanar Interleaved Band buffer device (8 bit Gray)
#	plibc		PLanar Interleaved Band buffer device (32 bit CMYK)
#	plibk		PLanar Interleaved Band buffer device (4 bit CMYK)
#	pngmono		Monochrome Portable Network Graphics (PNG)
#	pngmonod	Monochrome (error diffused) Portable Network Graphics (PNG)
#	pnggray		8-bit gray Portable Network Graphics (PNG)
#	png16		4-bit color Portable Network Graphics (PNG)
#	png256		8-bit color Portable Network Graphics (PNG)
#	png16m		24-bit color Portable Network Graphics (PNG)
#	pngalpha	32-bit RGBA color Portable Network Graphics (PNG)
#	tiffgray	TIFF 8-bit gray, no compression
#	tiff12nc	TIFF 12-bit RGB, no compression
#	tiff24nc 	TIFF 24-bit RGB, no compression (NeXT standard format)
#	tiff48nc	TIFF 48-bit RGB, no compression
#	tiff32nc	TIFF 32-bit CMYK
#	tiff64nc 	TIFF 64-bit CMYK
#	tiffsep		Creates tiffgray for each colorant plus a CMYK composite
#	tiffsep1	Creates halftoned tiff 1-bit per pixel for each colorant
#	tifflzw 	TIFF LZW (tag = 5) (monochrome)
#	tiffpack	TIFF PackBits (tag = 32773) (monochrome)
#	tiffscaled	TIFF (monochrome output, integer downsampled and dithered from grayscale rendering)
#	tiffscaled8	TIFF (greyscale output, integer downsampled and dithered from grayscale rendering)
#	tiffscaled24	TIFF (rgb output, integer downsampled and dithered from rgb rendering)
#	tiffscaled32	TIFF (cmyk output, integer downsampled and dithered from cmyk rendering)
#	tiffscaled4	TIFF (cmyk output, integer downsampled and dithered from cmyk rendering)

# Note that MS Windows-specific drivers are defined in pcwin.mak, not here,
# because they have special compilation requirements that require defining
# parameter macros not relevant to other platforms; the OS/2-specific
# drivers are there too, because they share some definitions.

# User-contributed drivers marked with * require hardware or software
# that is not available to Artifex Software Inc.  Please contact the
# original contributors, not Artifex Software Inc, if you have questions.
# Contact information appears in the driver entry below.
#
# Drivers marked with a + are maintained by Artifex Software Inc with
# the assistance of users, since Artifex Software Inc doesn't have access to
# the hardware for these either.

# If you add drivers, it would be nice if you kept each list
# in alphabetical order.

###### ----------------------- End of catalog ----------------------- ######

# As noted in gs.mak, DEVICE_DEVS and DEVICE_DEVS1..15 select the devices
# that should be included in a given configuration.  By convention, these
# are used as follows.  Each of these must be limited to about 6 devices
# so as not to overflow the 120 character limit on MS-DOS command lines.
#	DEVICE_DEVS - the default device, and any display devices.
#	DEVICE_DEVS1 - additional display devices if needed.
#	DEVICE_DEVS2 - dot matrix printers.
#	DEVICE_DEVS3 - H-P monochrome printers.
#	DEVICE_DEVS4 - H-P color printers.
#	DEVICE_DEVS5 - additional inkjet printers if needed.
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
#	DEVICE_DEVS16 - additional high-level and utility drivers
#	DEVICE_DEVS17 - (overflow for PC platforms)
#	DEVICE_DEVS18 - (ditto)
#	DEVICE_DEVS19 - (ditto)
#	DEVICE_DEVS20 - (ditto)
# Feel free to disregard this convention if it gets in your way.

# If you want to add a new device driver, the examples below should be
# enough of a guide to the correct form for the makefile rules.
# Note that all drivers other than displays must include page.dev in their
# dependencies and use $(SETPDEV) rather than $(SETDEV) in their rule bodies.

# "Printer" drivers depend on the following:
PDEVH=$(AK) $(gdevprn_h)

gxfcopy_h=$(DEVSRC)gxfcopy.h $(gsccode_h)

# Define the header files for device drivers.  Every header file used by
# more than one device driver family must be listed here.
gdev8bcm_h=$(DEVSRC)gdev8bcm.h
gdevcbjc_h=$(DEVSRC)gdevcbjc.h $(stream_h)

gdevpcl_h=$(DEVSRC)gdevpcl.h
gdevpsu_h=$(DEVVECSRC)gdevpsu.h
# Out of order
gdevdljm_h=$(DEVSRC)gdevdljm.h $(gdevpcl_h)

GDEVLDFJB2CC=$(CC_) $(I_)$(DEVI_) $(II)$(LDF_JB2I_)$(_I) $(JB2CF_) $(GLF_)
GDEVLWFJPXCC=$(CC_) $(I_)$(DEVI_) $(II)$(LWF_JPXI_)$(_I) $(JPXCF_) $(GLF_)
GDEVLWFJB2JPXCC=$(CC_) $(I_)$(DEVI_)  $(II)$(LDF_JB2I_) $(II)$(LWF_JPXI_)$(_I) $(JB2CF_) $(JPXCF_) $(GLF_)

###### ----------------------- Device support ----------------------- ######

# Implement dynamic color management for 8-bit mapped color displays.
$(DEVOBJ)gdev8bcm.$(OBJ) : $(DEVSRC)gdev8bcm.c $(AK)\
 $(gx_h) $(gxdevice_h) $(gdev8bcm_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdev8bcm.$(OBJ) $(C_) $(DEVSRC)gdev8bcm.c

# Generate Canon BJC command sequences.
$(DEVOBJ)gdevcbjc.$(OBJ) : $(DEVSRC)gdevcbjc.c $(AK)\
 $(std_h) $(stream_h) $(gdevcbjc_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcbjc.$(OBJ) $(C_) $(DEVSRC)gdevcbjc.c

# Support for writing PostScript (high- or low-level).
$(DEVOBJ)gdevpsu.$(OBJ) : $(DEVVECSRC)gdevpsu.c $(GX) $(GDEV) $(math__h) $(time__h)\
 $(stat__h) $(unistd__h)\
 $(gdevpsu_h) $(gscdefs_h) $(gxdevice_h)\
 $(spprint_h) $(stream_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsu.$(OBJ) $(C_) $(DEVVECSRC)gdevpsu.c

### ------------------ Display device for DLL platforms ----------------- ###

display_=$(DEVOBJ)gdevdsp.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ) $(GLOBJ)gdevdevn.$(OBJ) \
	 $(GLOBJ)gsequivc.$(OBJ) $(DEVOBJ)gdevdcrd.$(OBJ)
$(DD)display.dev : $(display_) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)display $(display_)

$(DEVOBJ)gdevdsp.$(OBJ) : $(DEVSRC)gdevdsp.c $(string__h)\
 $(gp_h) $(gpcheck_h) $(gdevpccm_h) $(gsparam_h) $(gsdevice_h)\
 $(GDEVH) $(gxdevmem_h) $(gdevdevn_h) $(gsequivc_h) $(gdevdsp_h) $(gdevdsp2_h) \
  $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdsp.$(OBJ) $(C_) $(DEVSRC)gdevdsp.c

### -------------------------- The X11 device -------------------------- ###

# Please note that Artifex Software Inc does not support Ghostview.
# For more information about Ghostview, please contact Tim Theisen
# (ghostview@cs.wisc.edu).

gdevxcmp_h=$(DEVSRC)gdevxcmp.h
gdevx_h=$(DEVSRC)gdevx.h $(gdevbbox_h) $(gdevxcmp_h)

# See the main makefile for the definition of XLIBDIRS and XLIBS.
x11_=$(DEVOBJ)gdevx.$(OBJ) $(DEVOBJ)gdevxcmp.$(OBJ) $(DEVOBJ)gdevxini.$(OBJ)\
 $(DEVOBJ)gdevxres.$(OBJ) $(DEVOBJ)gsparamx.$(OBJ)
$(DD)x11_.dev : $(x11_) $(GLD)bboxutil.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)x11_ $(x11_)
	$(ADDMOD) $(DD)x11_ -link $(XLIBDIRS)
	$(ADDMOD) $(DD)x11_ -lib $(XLIBS)
	$(ADDMOD) $(DD)x11_ -include $(GLD)bboxutil

$(DD)x11.dev : $(DD)x11_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11 -include $(DD)x11_

# See the main makefile for the definition of XINCLUDE.
GDEVX=$(GDEV) $(x__h) $(gdevx_h) $(TOP_MAKEFILES)
$(DEVOBJ)gdevx.$(OBJ) : $(DEVSRC)gdevx.c $(GDEVX) $(math__h) $(memory__h)\
 $(gscoord_h) $(gsdevice_h) $(gsiparm2_h) $(gsmatrix_h) $(gsparam_h)\
 $(gxdevmem_h) $(gxgetbit_h) $(gxiparam_h) $(gxpath_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCCSHARED) $(XINCLUDE) $(DEVO_)gdevx.$(OBJ) $(C_) $(DEVSRC)gdevx.c

$(DEVOBJ)gdevxcmp.$(OBJ) : $(DEVSRC)gdevxcmp.c $(GDEVX) $(math__h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCCSHARED) $(XINCLUDE) $(DEVO_)gdevxcmp.$(OBJ) $(C_) $(DEVSRC)gdevxcmp.c

$(DEVOBJ)gdevxini.$(OBJ) : $(DEVSRC)gdevxini.c $(GDEVX) $(memory__h)\
 $(gserrors_h) $(gsparamx_h) $(gxdevmem_h) $(gdevbbox_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCCSHARED) $(XINCLUDE) $(DEVO_)gdevxini.$(OBJ) $(C_) $(DEVSRC)gdevxini.c

# We have to compile gdevxres without warnings, because there is a
# const/non-const cast required by the X headers that we can't work around.
$(DEVOBJ)gdevxres.$(OBJ) : $(DEVSRC)gdevxres.c $(std_h) $(x__h)\
 $(gsmemory_h) $(gstypes_h) $(gxdevice_h) $(gdevx_h) $(DEVS_MAK) $(MAKEDIRS)
	$(CC_NO_WARN) $(GLCCFLAGS) $(XINCLUDE) $(DEVO_)gdevxres.$(OBJ) $(C_) $(DEVSRC)gdevxres.c

# Alternate X11-based devices to help debug other drivers.
# x11alpha pretends to have 4 bits of alpha channel.
# x11cmyk pretends to be a CMYK device with 1 bit each of C,M,Y,K.
# x11cmyk2 pretends to be a CMYK device with 2 bits each of C,M,Y,K.
# x11cmyk4 pretends to be a CMYK device with 4 bits each of C,M,Y,K.
# x11cmyk8 pretends to be a CMYK device with 8 bits each of C,M,Y,K.
# x11gray2 pretends to be a 2-bit gray-scale device.
# x11gray4 pretends to be a 4-bit gray-scale device.
# x11mono pretends to be a black-and-white device.
# x11rg16x pretends to be a G5/B5/R6 color device.
# x11rg16x pretends to be a G11/B10/R11 color device.
x11alt_=$(DEVOBJ)gdevxalt.$(OBJ)
$(DD)x11alt_.dev : $(x11alt_) $(DD)x11_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)x11alt_ $(x11alt_)
	$(ADDMOD) $(DD)x11alt_ -include $(DD)x11_

$(DD)x11alpha.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11alpha -include $(DD)x11alt_

$(DD)x11cmyk.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11cmyk -include $(DD)x11alt_

$(DD)x11cmyk2.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11cmyk2 -include $(DD)x11alt_

$(DD)x11cmyk4.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11cmyk4 -include $(DD)x11alt_

$(DD)x11cmyk8.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11cmyk8 -include $(DD)x11alt_

$(DD)x11gray2.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11gray2 -include $(DD)x11alt_

$(DD)x11gray4.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11gray4 -include $(DD)x11alt_

$(DD)x11mono.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11mono -include $(DD)x11alt_

$(DD)x11rg16x.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11rg16x -include $(DD)x11alt_

$(DD)x11rg32x.dev : $(DD)x11alt_.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)x11rg32x -include $(DD)x11alt_

$(DEVOBJ)gdevxalt.$(OBJ) : $(DEVSRC)gdevxalt.c $(GDEVX) $(math__h) $(memory__h)\
 $(gsdevice_h) $(gsparam_h) $(gsstruct_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCCSHARED) $(XINCLUDE) $(DEVO_)gdevxalt.$(OBJ) $(C_) $(DEVSRC)gdevxalt.c

### Shared library object supporting X11.
### NON PORTABLE, ONLY UNIX WITH GCC SUPPORT

$(DEVOBJ)X11.so : $(x11alt_) $(x11_) $(DEVS_MAK) $(MAKEDIRS)
	$(CCLD) $(LDFLAGS) -shared -o $(DEVOBJ)X11.so $(x11alt_) $(x11_) -L/usr/X11R6/lib -lXt -lSM -lICE -lXext -lX11 $(XLIBDIRS)

###### --------------- Memory-buffered printer devices --------------- ######

### ---------------- Practical Automation label printers ---------------- ###

atx_=$(DEVOBJ)gdevatx.$(OBJ)

$(DD)atx23.dev : $(atx_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)atx23 $(atx_)

$(DD)atx24.dev : $(atx_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)atx24 $(atx_)

$(DD)atx38.dev : $(atx_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)atx38 $(atx_)

$(DD)itk24i.dev : $(atx_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)itk24i $(atx_)

$(DD)itk38.dev : $(atx_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)itk38 $(atx_)

$(DEVOBJ)gdevatx.$(OBJ) : $(DEVSRC)gdevatx.c $(PDEVH) $(math__h) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevatx.$(OBJ) $(C_) $(DEVSRC)gdevatx.c

### ----------- The H-P DeskJet and LaserJet printer devices ----------- ###

### These are essentially the same device.
### NOTE: printing at full resolution (300 DPI) requires a printer
###   with at least 1.5 Mb of memory.  150 DPI only requires .5 Mb.
### Note that the lj4dith driver is included with the H-P color printer
###   drivers below.
### For questions about the fs600 device, please contact                  ###
### Peter Schildmann (peter.schildmann@etechnik.uni-rostock.de).          ###

HPPCL=$(DEVOBJ)gdevpcl.$(OBJ)
HPDLJM=$(DEVOBJ)gdevdljm.$(OBJ) $(HPPCL)
HPMONO=$(DEVOBJ)gdevdjet.$(OBJ) $(HPDLJM)

$(DEVOBJ)gdevpcl.$(OBJ) : $(DEVSRC)gdevpcl.c $(PDEVH) $(math__h) $(gdevpcl_h)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpcl.$(OBJ) $(C_) $(DEVSRC)gdevpcl.c

$(DEVOBJ)gdevdljm.$(OBJ) : $(DEVSRC)gdevdljm.c $(PDEVH) $(gdevdljm_h) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdljm.$(OBJ) $(C_) $(DEVSRC)gdevdljm.c

$(DEVOBJ)gdevdjet.$(OBJ) : $(DEVSRC)gdevdjet.c $(PDEVH) $(gdevdljm_h) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdjet.$(OBJ) $(C_) $(DEVSRC)gdevdjet.c

$(DD)deskjet.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)deskjet $(HPMONO)

$(DD)djet500.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)djet500 $(HPMONO)

$(DD)fs600.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)fs600 $(HPMONO)

$(DD)laserjet.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)laserjet $(HPMONO)

$(DD)ljetplus.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljetplus $(HPMONO)

### Selecting ljet2p provides TIFF (mode 2) compression on LaserJet III,
### IIIp, IIId, IIIsi, IId, and IIp.

$(DD)ljet2p.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljet2p $(HPMONO)

### Selecting ljet3 provides Delta Row (mode 3) compression on LaserJet III,
### IIIp, IIId, IIIsi.

$(DD)ljet3.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljet3 $(HPMONO)

### Selecting ljet3d also provides duplex printing capability.

$(DD)ljet3d.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljet3d $(HPMONO)

### Selecting ljet4 or ljet4d also provides Delta Row compression on
### LaserJet IV series.

$(DD)ljet4.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljet4 $(HPMONO)

$(DD)ljet4d.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ljet4d $(HPMONO)

$(DD)lp2563.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)lp2563 $(HPMONO)

$(DD)oce9050.dev : $(HPMONO) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)oce9050 $(HPMONO)

### ------------------ The H-P LaserJet 5 and 6 devices ----------------- ###

### These drivers use H-P's new PCL XL printer language, like H-P's
### LaserJet 5 Enhanced driver for MS Windows.  We don't recommend using
### them:
###	- If you have a LJ 5L or 5P, which isn't a "real" LaserJet 5,
###	use the ljet4 driver instead.  (The lj5 drivers won't work.)
###	- If you have any other model of LJ 5 or 6, use the pxlmono
###	driver, which often produces much more compact output.


gdevpxut_h=$(DEVSRC)gdevpxut.h


$(DEVOBJ)gdevpxut.$(OBJ) : $(DEVSRC)gdevpxut.c $(math__h) $(string__h)\
 $(gx_h) $(gxdevcli_h) $(stream_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevpxut_h) $(GDEV) \
  $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpxut.$(OBJ) $(C_) $(DEVSRC)gdevpxut.c

ljet5_=$(DEVOBJ)gdevlj56.$(OBJ) $(DEVOBJ)gdevpxut.$(OBJ) $(HPPCL)
$(DD)lj5mono.dev : $(ljet5_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj5mono $(ljet5_)

$(DD)lj5gray.dev : $(ljet5_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)lj5gray $(ljet5_)

$(DEVOBJ)gdevlj56.$(OBJ) : $(DEVSRC)gdevlj56.c $(PDEVH) $(gdevpcl_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevpxut_h) $(stream_h) \
  $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevlj56.$(OBJ) $(C_) $(DEVSRC)gdevlj56.c

### -------------------- The ijs client ----------------- ###

ijs_=$(DEVOBJ)gdevijs.$(OBJ)

#$(IJSOBJ)ijs.$(OBJ) $(IJSOBJ)ijs_client.$(OBJ) \
# $(IJSOBJ)ijs_exec_$(IJSEXECTYPE).$(OBJ)

$(DD)ijs.dev : $(ijs_) $(GLD)page.dev $(DD)ijslib.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)ijs $(ijs_)
	$(ADDMOD) $(DD)ijs -include $(GLD)ijslib

$(DEVOBJ)gdevijs.$(OBJ) : $(DEVSRC)gdevijs.c $(PDEVH) $(unistd__h) $(gp_h)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(CC_) $(I_)$(DEVI_) $(II)$(IJSI_)$(_I) $(II)$(IJSI_)$(D)..$(_I) \
            $(GLF_) $(DEVO_)gdevijs.$(OBJ) $(C_) $(DEVSRC)gdevijs.c

# Please see ijs.mak for the Makefile fragment which builds the IJS
# library.


### -------------------------- The rinkj device ------------------------ ###

RINKJ_SRCDIR=$(DEVSRC)rinkj
RINKJ_SRC=$(RINKJ_SRCDIR)$(D)
RINKJ_OBJ=$(DEVOBJ)
RINKJ_O_=$(O_)$(RINKJ_OBJ)

RINKJ_INCL=$(I_)$(RINKJ_SRCDIR)$(_I)
RINKJ_CC=$(CC_) $(RINKJ_INCL)

rinkj_core=$(RINKJ_OBJ)evenbetter-rll.$(OBJ) \
 $(RINKJ_OBJ)rinkj-byte-stream.$(OBJ) $(RINKJ_OBJ)rinkj-device.$(OBJ) \
 $(RINKJ_OBJ)rinkj-config.$(OBJ) $(RINKJ_OBJ)rinkj-dither.$(OBJ) \
 $(RINKJ_OBJ)rinkj-epson870.$(OBJ) $(RINKJ_OBJ)rinkj-screen-eb.$(OBJ)

$(RINKJ_OBJ)evenbetter-rll.$(OBJ) : $(RINKJ_SRC)evenbetter-rll.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)evenbetter-rll.$(OBJ) $(C_) $(RINKJ_SRC)evenbetter-rll.c

$(RINKJ_OBJ)rinkj-byte-stream.$(OBJ) : $(RINKJ_SRC)rinkj-byte-stream.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-byte-stream.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-byte-stream.c

$(RINKJ_OBJ)rinkj-device.$(OBJ) : $(RINKJ_SRC)rinkj-device.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-device.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-device.c

$(RINKJ_OBJ)rinkj-config.$(OBJ) : $(RINKJ_SRC)rinkj-config.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-config.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-config.c

$(RINKJ_OBJ)rinkj-dither.$(OBJ) : $(RINKJ_SRC)rinkj-dither.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-dither.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-dither.c

$(RINKJ_OBJ)rinkj-epson870.$(OBJ) : $(RINKJ_SRC)rinkj-epson870.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-epson870.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-epson870.c

$(RINKJ_OBJ)rinkj-screen-eb.$(OBJ) : $(RINKJ_SRC)rinkj-screen-eb.c $(DEVS_MAK) $(MAKEDIRS) $(gserrors_h)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-screen-eb.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-screen-eb.c

rinkj_=$(DEVOBJ)gdevrinkj.$(OBJ) $(rinkj_core)

$(DD)rinkj.dev : $(rinkj_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)rinkj $(rinkj_)

$(DEVOBJ)gdevrinkj.$(OBJ) : $(DEVSRC)gdevrinkj.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gxdcconv_h) $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevrinkj.$(OBJ) $(C_) $(DEVSRC)gdevrinkj.c


###### ------------------- High-level file formats ------------------- ######

# Support for PostScript and PDF

gdevpsdf_h=$(DEVVECSRC)gdevpsdf.h $(gdevvec_h) $(gsparam_h)\
 $(sa85x_h) $(scfx_h) $(spsdf_h) $(strimpl_h)
gdevpsds_h=$(DEVVECSRC)gdevpsds.h $(strimpl_h) $(gsiparam_h)

psdf_1=$(DEVOBJ)gdevpsdi.$(OBJ) $(DEVOBJ)gdevpsdp.$(OBJ)
psdf_2=$(DEVOBJ)gdevpsds.$(OBJ) $(DEVOBJ)gdevpsdu.$(OBJ)
psdf_3=$(DEVOBJ)scfparam.$(OBJ) $(DEVOBJ)sdcparam.$(OBJ) $(DEVOBJ)sdeparam.$(OBJ)
psdf_4=$(DEVOBJ)spprint.$(OBJ) $(DEVOBJ)spsdf.$(OBJ) $(DEVOBJ)sstring.$(OBJ)
psdf_5=$(DEVOBJ)gsparamx.$(OBJ)
psdf_=$(psdf_1) $(psdf_2) $(psdf_3) $(psdf_4) $(psdf_5)
psdf_inc1=$(GLD)vector.dev $(GLD)pngp.dev $(GLD)seexec.dev
psdf_inc2=$(GLD)sdcte.dev $(GLD)slzwe.dev $(GLD)szlibe.dev
psdf_inc=$(psdf_inc1) $(psdf_inc2)
$(DD)psdf.dev : $(ECHOGS_XE) $(psdf_) $(psdf_inc) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)psdf $(psdf_1)
	$(ADDMOD) $(DD)psdf -obj $(psdf_2)
	$(ADDMOD) $(DD)psdf -obj $(psdf_3)
	$(ADDMOD) $(DD)psdf -obj $(psdf_4)
	$(ADDMOD) $(DD)psdf -obj $(psdf_5)
	$(ADDMOD) $(DD)psdf -include $(psdf_inc1)
	$(ADDMOD) $(DD)psdf -include $(psdf_inc2)

$(DEVOBJ)gdevpsdi.$(OBJ) : $(DEVVECSRC)gdevpsdi.c $(GXERR)\
 $(jpeglib__h) $(math__h) $(stdio__h)\
 $(gscspace_h)\
 $(scfx_h) $(slzwx_h) $(spngpx_h)\
 $(strimpl_h) $(szlibx_h) $(sisparam_h)\
 $(gdevpsdf_h) $(gdevpsds_h) $(gxdevmem_h) $(gxcspace_h) $(gxparamx_h)\
 $(sjbig2_luratech_h) $(sjpx_luratech_h) $(gsicc_manage_h) $(DEVS_MAK) $(MAKEDIRS)
	$(GDEVLWFJB2JPXCC) $(DEVO_)gdevpsdi.$(OBJ) $(C_) $(DEVVECSRC)gdevpsdi.c

$(DEVOBJ)gdevpsdp.$(OBJ) : $(DEVVECSRC)gdevpsdp.c $(GDEVH)\
 $(string__h) $(jpeglib__h)\
 $(scfx_h) $(sdct_h) $(slzwx_h) $(srlx_h) $(strimpl_h) $(szlibx_h)\
 $(gsparamx_h) $(gsutil_h) $(gdevpsdf_h)\
 $(sjbig2_luratech_h) $(sjpx_luratech_h) $(DEVS_MAK) $(MAKEDIRS)
	$(GDEVLWFJB2JPXCC) $(DEVO_)gdevpsdp.$(OBJ) $(C_) $(DEVVECSRC)gdevpsdp.c

$(DEVOBJ)gdevpsds.$(OBJ) : $(DEVVECSRC)gdevpsds.c $(GX) $(memory__h)\
 $(gserrors_h) $(gxdcconv_h) $(gdevpsds_h) $(gxbitmap_h)\
 $(gxcspace_h) $(gsdcolor_h) $(gscspace_h) $(gxdevcli_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsds.$(OBJ) $(C_) $(DEVVECSRC)gdevpsds.c

$(DEVOBJ)gdevpsdu.$(OBJ) : $(DEVVECSRC)gdevpsdu.c $(GXERR)\
 $(jpeglib__h) $(memory__h) $(stdio__h)\
 $(sa85x_h) $(scfx_h) $(sdct_h) $(sjpeg_h) $(strimpl_h)\
 $(gdevpsdf_h) $(spprint_h) $(gsovrc_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVJCC) $(DEVO_)gdevpsdu.$(OBJ) $(C_) $(DEVVECSRC)gdevpsdu.c

# Plain text writer

gdevagl_h=$(DEVVECSRC)gdevagl.h

txtwrite_=$(DEVOBJ)gdevtxtw.$(OBJ) $(DEVOBJ)gdevagl.$(OBJ)

$(DD)txtwrite.dev : $(ECHOGS_XE) $(txtwrite_) $(GDEV)\
 $(gdevagl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)txtwrite $(txtwrite_)

$(DEVOBJ)gdevtxtw.$(OBJ) : $(DEVVECSRC)gdevtxtw.c $(GDEV)\
  $(memory__h) $(string__h) $(gp_h) $(gsparam_h) $(gsutil_h) \
  $(gsdevice_h) $(gxfont_h) $(gxfont0_h) $(gstext_h) $(gxfcid_h)\
  $(gxgstate_h) $(gxpath_h) $(gdevagl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevtxtw.$(OBJ) $(C_) $(DEVVECSRC)gdevtxtw.c

$(DEVOBJ)gdevagl.$(OBJ) : $(DEVVECSRC)gdevagl.c $(GDEV)\
 $(gdevagl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevagl.$(OBJ) $(C_) $(DEVVECSRC)gdevagl.c


################ BEGIN PDF WRITER ################

# We reserve slots here for gdevpdfa...z, just in case we need them.
pdfwrite1_=$(DEVOBJ)gdevpdf.$(OBJ) $(DEVOBJ)gdevpdfb.$(OBJ)
pdfwrite2_=$(DEVOBJ)gdevpdfc.$(OBJ) $(DEVOBJ)gdevpdfd.$(OBJ) $(DEVOBJ)gdevpdfe.$(OBJ)
pdfwrite3_=$(DEVOBJ)gdevpdfg.$(OBJ)
pdfwrite4_=$(DEVOBJ)gdevpdfi.$(OBJ) $(DEVOBJ)gdevpdfj.$(OBJ) $(DEVOBJ)gdevpdfk.$(OBJ)
pdfwrite5_=$(DEVOBJ)gdevpdfm.$(OBJ)
pdfwrite6_=$(DEVOBJ)gdevpdfo.$(OBJ) $(DEVOBJ)gdevpdfp.$(OBJ) $(DEVOBJ)gdevpdft.$(OBJ)
pdfwrite7_=$(DEVOBJ)gdevpdfr.$(OBJ)
pdfwrite8_=$(DEVOBJ)gdevpdfu.$(OBJ) $(DEVOBJ)gdevpdfv.$(OBJ) $(DEVOBJ)gdevagl.$(OBJ)
pdfwrite9_=$(DEVOBJ)gsflip.$(OBJ)
pdfwrite10_=$(DEVOBJ)scantab.$(OBJ) $(DEVOBJ)sfilter2.$(OBJ)
pdfwrite_=$(pdfwrite1_) $(pdfwrite2_) $(pdfwrite3_) $(pdfwrite4_)\
 $(pdfwrite5_) $(pdfwrite6_) $(pdfwrite7_) $(pdfwrite8_) $(pdfwrite9_)\
 $(pdfwrite10_) $(pdfwrite11_)

# Since ps2write and eps2write are actually "clones" of pdfwrite,
# we just depend on it.
# Also note gs_pdfwr.ps depends on pdfwrite being available. So
# if this is changed to allow (e)ps2write to be built in without
# pdfwrite, then gs_pdfwr.ps will need to be changed to check for
# the three devices, rather than just pdfwrite.
$(DD)ps2write.dev : $(DD)pdfwrite.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)ps2write
	$(ADDMOD) $(DD)ps2write -include $(DD)pdfwrite.dev

$(DD)eps2write.dev : $(DD)pdfwrite.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)eps2write
	$(ADDMOD) $(DD)eps2write -include $(DD)pdfwrite.dev

# Note that for ps2pdf operation, we need to parse DSC comments to set
# the Orientation (Page dict /Rotate value). This is not part of the
# pdfwrite device, but part of the PS interpreter so that the pdfwrite
# device can be used with other top level interpreters (such as PCL).
$(DD)pdfwrite.dev : $(ECHOGS_XE) $(pdfwrite_)\
 $(GLD)cmyklib.dev $(GLD)cfe.dev $(GLD)lzwe.dev\
 $(GLD)rle.dev $(GLD)sdcte.dev $(GLD)sdeparam.dev $(GLD)smd5.dev\
 $(GLD)szlibe.dev $(GLD)psdf.dev $(GLD)sarc4.dev $(DD)pdtext.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)pdfwrite $(pdfwrite1_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite2_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite3_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite4_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite5_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite6_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite7_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite8_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite9_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite10_)
	$(ADDMOD) $(DD)pdfwrite $(pdfwrite11_)
	$(ADDMOD) $(DD)pdfwrite -include $(GLD)cmyklib $(GLD)cfe $(GLD)lzwe
	$(ADDMOD) $(DD)pdfwrite -include $(GLD)rle $(GLD)sdcte $(GLD)sdeparam
	$(ADDMOD) $(DD)pdfwrite -include $(GLD)smd5 $(GLD)szlibe $(GLD)sarc4.dev
	$(ADDMOD) $(DD)pdfwrite -include $(GLD)psdf
	$(ADDMOD) $(DD)pdfwrite -include $(DD)pdtext

gdevpdfb_h=$(DEVVECSRC)gdevpdfb.h
gdevpdfc_h=$(DEVVECSRC)gdevpdfc.h
gdevpdfg_h=$(DEVVECSRC)gdevpdfg.h $(gscspace_h)
gdevpdfo_h=$(DEVVECSRC)gdevpdfo.h $(gsparam_h)
gdevpdfx_h=$(DEVVECSRC)gdevpdfx.h\
 $(gsparam_h) $(gsuid_h) $(gxdevice_h) $(gxfont_h) $(gxline_h)\
 $(spprint_h) $(stream_h) $(gdevpsdf_h) $(gxdevmem_h) $(sarc4_h)

opdfread_h=$(DEVGEN)opdfread.h

$(DEVGEN)opdfread.h : $(PACKPS_XE) $(DEVVECSRC)opdfread.ps
	$(EXP)$(PACKPS_XE) -c -n opdfread_ps -o $(opdfread_h) $(DEVVECSRC)opdfread.ps

$(DEVOBJ)gdevpdf.$(OBJ) : $(DEVVECSRC)gdevpdf.c $(GDEVH)\
 $(fcntl__h) $(memory__h) $(string__h) $(time__h) $(unistd__h) $(gp_h)\
 $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h) $(smd5_h) $(sarc4_h)\
 $(gdevpdfb_h) $(gscms_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdf.$(OBJ) $(C_) $(DEVVECSRC)gdevpdf.c

$(DEVOBJ)gdevpdfb.$(OBJ) : $(DEVVECSRC)gdevpdfb.c\
 $(string__h) $(gx_h)\
 $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gserrors_h) $(gxcspace_h) $(gxdcolor_h) $(gxpcolor_h) $(gxhldevc_h)\
 $(gsptype1_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfb.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfb.c

$(DEVOBJ)gdevpdfc.$(OBJ) : $(DEVVECSRC)gdevpdfc.c $(GXERR) $(math__h) $(memory__h)\
 $(gdevpdfc_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gscie_h) $(gscindex_h) $(gscspace_h) $(gscdevn_h) $(gscsepr_h) $(gsicc_h)\
 $(sstring_h) $(stream_h) $(strimpl_h) $(gxcspace_h) $(gxcdevn_h) $(gscspace_h)\
 $(gsicc_manage_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfc.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfc.c

$(DEVOBJ)gdevpdfd.$(OBJ) : $(DEVVECSRC)gdevpdfd.c $(math__h) $(memory__h)\
 $(gx_h) $(gxdevice_h) $(gxfixed_h) $(gxgstate_h) $(gxpaint_h)\
 $(gxcoord_h) $(gxdevmem_h) $(gxcolor2_h) $(gxhldevc_h)\
 $(gsstate_h) $(gserrors_h) $(gsptype2_h) $(gsshade_h)\
 $(gzpath_h) $(gzcpath_h) $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gsutil_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfd.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfd.c

$(DEVOBJ)gdevpdfe.$(OBJ) : $(DEVVECSRC)gdevpdfe.c\
 $(gx_h) $(gserrors_h) $(string__h) $(time__h) $(stream_h) $(gp_h) $(smd5_h) $(gscdefs_h)\
 $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdtf_h) $(ConvertUTF_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfe.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfe.c

$(DEVOBJ)gdevpdfg.$(OBJ) : $(DEVVECSRC)gdevpdfg.c $(GXERR) $(math__h) $(string__h)\
 $(memory__h) $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gsfunc0_h) $(gsstate_h) $(gxdcolor_h) $(gxpcolor_h) $(gxcolor2_h) $(gsptype2_h)\
 $(gxbitmap_h) $(gxdht_h) $(gxfarith_h) $(gxfmap_h) $(gxht_h) $(gxgstate_h)\
 $(gzht_h) $(gsicc_manage_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfg.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfg.c

$(DEVOBJ)gdevpdfi.$(OBJ) : $(DEVVECSRC)gdevpdfi.c $(memory__h) $(math__h)\
 $(gx_h)\
 $(gserrors_h) $(gsdevice_h) $(gsflip_h) $(gsiparm4_h) $(gsstate_h) $(gscolor2_h)\
 $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdfo_h)\
 $(gxcspace_h) $(gximage3_h) $(gximag3x_h) $(gxdcolor_h) $(gxpcolor_h)\
 $(gxhldevc_h) $(gsicc_manage_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfi.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfi.c

$(DEVOBJ)gdevpdfj.$(OBJ) : $(DEVVECSRC)gdevpdfj.c\
 $(memory__h) $(string__h) $(gx_h) $(gserrors_h)\
 $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gxcspace_h)\
 $(gsiparm4_h) $(gdevpsds_h) $(spngpx_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVJCC) $(DEVO_)gdevpdfj.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfj.c

$(DEVOBJ)gdevpdfk.$(OBJ) : $(DEVVECSRC)gdevpdfk.c $(GXERR) $(math__h) $(memory__h)\
 $(gdevpdfc_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gsicc_h) $(gxcie_h) $(gxcspace_h)\
 $(stream_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfk.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfk.c

$(DEVOBJ)gdevpdfm.$(OBJ) : $(DEVVECSRC)gdevpdfm.c\
 $(math__h) $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfo_h) $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(szlibx_h) $(slzwx_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfm.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfm.c

$(DEVOBJ)gdevpdfo.$(OBJ) : $(DEVVECSRC)gdevpdfo.c $(memory__h) $(string__h)\
 $(gx_h)\
 $(gdevpdfo_h) $(gdevpdfx_h) $(gserrors_h) $(gsparam_h) $(gsutil_h)\
 $(sa85x_h) $(sarc4_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfo.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfo.c

$(DEVOBJ)gdevpdfp.$(OBJ) : $(DEVVECSRC)gdevpdfp.c $(memory__h) $(string__h) $(gx_h)\
 $(gdevpdfo_h) $(gdevpdfg_h) $(gdevpdfx_h) $(gserrors_h) $(gsparamx_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfp.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfp.c

$(DEVOBJ)gdevpdfr.$(OBJ) : $(DEVVECSRC)gdevpdfr.c $(memory__h) $(string__h)\
 $(gx_h)\
 $(gdevpdfo_h) $(gdevpdfx_h) $(gserrors_h) $(gsutil_h)\
 $(scanchar_h) $(sstring_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfr.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfr.c

$(DEVOBJ)gdevpdft.$(OBJ) : $(DEVVECSRC)gdevpdft.c $(string__h)\
 $(gx_h) $(gserrors_h) $(gstrans_h) $(gscolor2_h) $(gzstate_h)\
 $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdfo_h) $(gsccolor_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdft.$(OBJ) $(C_) $(DEVVECSRC)gdevpdft.c

$(DEVOBJ)gdevpdfu.$(OBJ) : $(DEVVECSRC)gdevpdfu.c $(GXERR)\
 $(jpeglib__h) $(memory__h) $(string__h)\
 $(gdevpdfo_h) $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdtd_h) $(gscdefs_h)\
 $(gsdsrc_h) $(gsfunc_h) $(gsfunc3_h)\
 $(sa85x_h) $(scfx_h) $(sdct_h) $(slzwx_h) $(spngpx_h)\
 $(srlx_h) $(sarc4_h) $(smd5_h) $(sstring_h) $(strimpl_h) $(szlibx_h)\
 $(strmio_h) $(sjbig2_luratech_h) $(sjpx_luratech_h)\
 $(opdfread_h) $(gdevagl_h) $(gs_mro_e_h) $(gs_mgl_e_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(GDEVLWFJB2JPXCC) $(DEVO_)gdevpdfu.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfu.c

$(DEVOBJ)gdevpdfv.$(OBJ) : $(DEVVECSRC)gdevpdfv.c $(GXERR) $(math__h) $(string__h)\
 $(gdevpdfg_h) $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gscindex_h) $(gscoord_h) $(gsiparm3_h) $(gsmatrix_h) $(gsptype2_h)\
 $(gxcolor2_h) $(gxdcolor_h) $(gxpcolor_h) $(gxshade_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdfv.$(OBJ) $(C_) $(DEVVECSRC)gdevpdfv.c

# ---------------- Font writing ---------------- #
# This is not really a library facility, but one piece of interpreter test
# code uses it.

# Support for PostScript and PDF font writing

gdevpsf_h=$(DEVVECSRC)gdevpsf.h $(gsccode_h) $(gsgdata_h)

psf_1=$(DEVOBJ)gdevpsf1.$(OBJ) $(DEVOBJ)gdevpsf2.$(OBJ) $(DEVOBJ)gdevpsfm.$(OBJ)
psf_2=$(DEVOBJ)gdevpsft.$(OBJ) $(DEVOBJ)gdevpsfu.$(OBJ) $(DEVOBJ)gdevpsfx.$(OBJ)
psf_3=$(DEVOBJ)spsdf.$(OBJ)
psf_=$(psf_1) $(psf_2) $(psf_3)
$(DD)psf.dev : $(DEV_MAK) $(ECHOGS_XE) $(psf_) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)psf $(psf_1)
	$(ADDMOD) $(DD)psf -obj $(psf_2)
	$(ADDMOD) $(DD)psf -obj $(psf_3)

$(DEVOBJ)gdevpsf1.$(OBJ) : $(DEVVECSRC)gdevpsf1.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsccode_h) $(gsmatrix_h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont1_h) $(gxmatrix_h) $(gxtype1_h)\
 $(sfilter_h) $(sstring_h) $(stream_h) $(strimpl_h)\
 $(gdevpsf_h) $(spprint_h) $(spsdf_h) $(math_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsf1.$(OBJ) $(C_) $(DEVVECSRC)gdevpsf1.c

$(DEVOBJ)gdevpsf2.$(OBJ) : $(DEVVECSRC)gdevpsf2.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h) $(gxarith_h) $(gsutil_h)\
 $(gsccode_h) $(gscencs_h) $(gscrypt1_h) $(gsmatrix_h)\
 $(gxfcid_h) $(gxfixed_h) $(gxfont_h) $(gxfont1_h)\
 $(stream_h) $(gdevpsf_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsf2.$(OBJ) $(C_) $(DEVVECSRC)gdevpsf2.c

$(DEVOBJ)gdevpsfm.$(OBJ) : $(DEVVECSRC)gdevpsfm.c $(AK) $(gx_h)\
 $(gserrors_h) $(gdevpsf_h) $(gxfcmap_h) $(spprint_h) $(spsdf_h) $(stream_h)\
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsfm.$(OBJ) $(C_) $(DEVVECSRC)gdevpsfm.c

$(DEVOBJ)gdevpsft.$(OBJ) : $(DEVVECSRC)gdevpsft.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gscencs_h) $(gsmatrix_h) $(gsutil_h)\
 $(gxfcid_h) $(gxfont_h) $(gxfont42_h) $(gxttf_h)\
 $(spprint_h) $(stream_h) $(gdevpsf_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsft.$(OBJ) $(C_) $(DEVVECSRC)gdevpsft.c

$(DEVOBJ)gdevpsfu.$(OBJ) : $(DEVVECSRC)gdevpsfu.c $(AK) $(gx_h)\
 $(gserrors_h) $(memory__h) $(gsmatrix_h) $(gxfont_h) $(gdevpsf_h)\
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsfu.$(OBJ) $(C_) $(DEVVECSRC)gdevpsfu.c

$(DEVOBJ)gdevpsfx.$(OBJ) : $(DEVVECSRC)gdevpsfx.c $(AK) $(gx_h)\
 $(gserrors_h) $(math__h) $(memory__h)\
 $(gxfixed_h) $(gxfont_h) $(gxfont1_h) $(gxmatrix_h) $(gxtype1_h)\
 $(stream_h) $(gdevpsf_h) $(gxgstate_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsfx.$(OBJ) $(C_) $(DEVVECSRC)gdevpsfx.c

# ---------------- Font copying ---------------- #

# This facility is not included in the core library.  Currently it is used
# only by pdfwrite.

fcopy_=$(DEVOBJ)gxfcopy.$(OBJ)
$(GLD)fcopy.dev : $(ECHOGS_XE) $(fcopy_) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(GLD)fcopy $(fcopy_)

$(DEVOBJ)gxfcopy.$(OBJ) : $(DEVSRC)gxfcopy.c $(memory__h) $(AK) $(gx_h)\
 $(gserrors_h) $(gscencs_h) $(gsline_h) $(gspaint_h) $(gspath_h) $(gsstruct_h)\
 $(gsutil_h) $(gschar_h) $(gxfont_h) $(gxfont1_h) $(gxfont42_h) $(gxchar_h)\
 $(gxfcid_h) $(gxfcopy_h) $(gxfcache_h) $(gxgstate_h) $(gxtext_h) $(gxtype1_h)\
 $(smd5_h) $(gzstate_h) $(gdevpsf_h) $(stream_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gxfcopy.$(OBJ) $(C_) $(DEVSRC)gxfcopy.c

######## pdfwrite text

# The text facilities for the PDF writer are so large and complex that
# we give them their own module name and (for the new code) file name prefix.
# However, logically they are part of pdfwrite and cannot be used separately.

$(DD)pdtext.dev : $(DD)pdxtext.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)pdtext -include $(DD)pdxtext

# For a code roadmap, see gdevpdtx.h.

gdevpdt_h=$(DEVVECSRC)gdevpdt.h
gdevpdtx_h=$(DEVVECSRC)gdevpdtx.h $(gdevpdt_h)
gdevpdtb_h=$(DEVVECSRC)gdevpdtb.h $(gdevpdtx_h)
gdevpdtd_h=$(DEVVECSRC)gdevpdtd.h $(gdevpdtb_h) $(gdevpdtx_h)
gdevpdtf_h=$(DEVVECSRC)gdevpdtf.h $(gdevpdtx_h)
gdevpdti_h=$(DEVVECSRC)gdevpdti.h $(gdevpdt_h)
gdevpdts_h=$(DEVVECSRC)gdevpdts.h $(gsmatrix_h)
gdevpdtt_h=$(DEVVECSRC)gdevpdtt.h
gdevpdtv_h=$(DEVVECSRC)gdevpdtv.h
gdevpdtw_h=$(DEVVECSRC)gdevpdtw.h
whitelst_h=$(DEVVECSRC)whitelst.h

# We reserve space for all of a..z, just in case.
pdxtext_ab=$(DEVOBJ)gdevpdt.$(OBJ) $(DEVOBJ)gdevpdtb.$(OBJ)
pdxtext_cde=$(DEVOBJ)gdevpdtc.$(OBJ) $(DEVOBJ)gdevpdtd.$(OBJ) $(DEVOBJ)gdevpdte.$(OBJ)
pdxtext_fgh=$(DEVOBJ)gdevpdtf.$(OBJ)
pdxtext_ijk=$(DEVOBJ)gdevpdti.$(OBJ)
pdxtext_lmn=
pdxtext_opq=
pdxtext_rst=$(DEVOBJ)gdevpdts.$(OBJ) $(DEVOBJ)gdevpdtt.$(OBJ)
pdxtext_uvw=$(DEVOBJ)gdevpdtv.$(OBJ) $(DEVOBJ)gdevpdtw.$(OBJ) $(DEVOBJ)whitelst.$(OBJ)
pdxtext_xyz=
pdxtext_=$(pdxtext_ab) $(pdxtext_cde) $(pdxtext_fgh) $(pdxtext_ijk)\
 $(pdxtext_lmn) $(pdxtext_opq) $(pdxtext_rst) $(pdxtext_uvw) $(pdxtext_xyz)\
 $(DEVOBJ)gsfont0c.$(OBJ)
$(DD)pdxtext.dev : $(pdxtext_) $(GDEV)\
 $(GLD)fcopy.dev $(GLD)psf.dev $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)pdxtext $(pdxtext_ab)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_cde)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_fgh)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_ijk)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_lmn)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_opq)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_rst)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_uvw)
	$(ADDMOD) $(DD)pdxtext $(pdxtext_xyz)
	$(ADDMOD) $(DD)pdxtext $(DEVOBJ)gsfont0c.$(OBJ)
	$(ADDMOD) $(DD)pdxtext -include $(GLD)fcopy $(GLD)psf

$(DEVOBJ)gdevpdt.$(OBJ) : $(DEVVECSRC)gdevpdt.c $(gx_h) $(gxpath_h) $(memory__h)\
 $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdtf_h) $(gdevpdti_h) $(gdevpdtx_h) $(gdevpdt_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdt.$(OBJ) $(C_) $(DEVVECSRC)gdevpdt.c

$(DEVOBJ)gdevpdtb.$(OBJ) : $(DEVVECSRC)gdevpdtb.c $(memory__h) $(ctype__h) $(string__h)\
 $(memory__h) $(ctype__h) $(string__h) $(gx_h) $(gserrors_h) $(gsutil_h) $(gxfcid_h)\
 $(gxfcopy_h) $(gxfont_h) $(gxfont42_h) $(gdevpsf_h) $(gdevpdfx_h) $(gdevpdfo_h)\
 $(gdevpdtb_h) $(gdevpdfg_h) $(gdevpdtf_h) $(smd5_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtb.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtb.c

$(DEVOBJ)gdevpdtc.$(OBJ) : $(DEVVECSRC)gdevpdtc.c $(gx_h) $(memory__h) $(string__h)\
 $(gserrors_h) $(gxfcmap_h) $(gxfont_h) $(gxfont0_h) $(gxfont0c_h)\
 $(gzpath_h) $(gxchar_h) $(gdevpsf_h) $(gdevpdfx_h) $(gdevpdtx_h)\
 $(gdevpdtd_h) $(gdevpdtf_h) $(gdevpdts_h) $(gdevpdtt_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtc.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtc.c

$(DEVOBJ)gdevpdte.$(OBJ) : $(DEVVECSRC)gdevpdte.c $(gx_h) $(math__h) $(memory__h) $(string__h)\
 $(gserrors_h) $(gsutil_h) $(gxfcmap_h) $(gxfcopy_h) $(gxfont_h) \
 $(gxfont0_h) $(gxfont0c_h) $(gxpath_h) $(gdevpsf_h) $(gdevpdfx_h) \
 $(gdevpdfg_h)  $(gdevpdfo_h) $(gdevpdtx_h) $(gdevpdtd_h) $(gdevpdtf_h) $(gdevpdts_h) \
 $(gdevpdtt_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdte.$(OBJ) $(C_) $(DEVVECSRC)gdevpdte.c

$(DEVOBJ)gdevpdtd.$(OBJ) : $(DEVVECSRC)gdevpdtd.c $(math__h) $(memory__h) $(gx_h)\
 $(gserrors_h) $(gsrect_h) $(gscencs_h)\
 $(gdevpdfo_h) $(gdevpdfx_h)\
 $(gdevpdtb_h) $(gdevpdtd_h) $(gdevpdtf_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtd.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtd.c

$(DEVOBJ)gdevpdtf.$(OBJ) : $(DEVVECSRC)gdevpdtf.c $(gx_h) $(memory__h)\
 $(string__h) $(gserrors_h) $(gsutil_h)\
 $(gxfcache_h) $(gxfcid_h) $(gxfcmap_h) $(gxfcopy_h) $(gxfont_h) $(gxfont1_h)\
 $(gdevpsf_h) $(gdevpdfx_h) $(gdevpdtb_h) $(gdevpdtd_h) $(gdevpdtf_h) $(gdevpdtw_h)\
 $(gdevpdti_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtf.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtf.c

$(DEVOBJ)gdevpdti.$(OBJ) : $(DEVVECSRC)gdevpdti.c $(memory__h) $(string__h) $(gx_h)\
 $(gserrors_h) $(gsutil_h)\
 $(gdevpdfx_h) $(gdevpdfg_h)\
 $(gdevpdtf_h) $(gdevpdti_h) $(gdevpdts_h) $(gdevpdtw_h) $(gdevpdtt_h) $(gdevpdfo_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdti.$(OBJ) $(C_) $(DEVVECSRC)gdevpdti.c

$(DEVOBJ)gdevpdts.$(OBJ) : $(DEVVECSRC)gdevpdts.c $(gx_h) $(math__h) $(memory__h)\
 $(gserrors_h) $(gdevpdfx_h) $(gdevpdfg_h) $(gdevpdtx_h) $(gdevpdtf_h)\
 $(gdevpdts_h) $(gdevpdtt_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdts.$(OBJ) $(C_) $(DEVVECSRC)gdevpdts.c

$(DEVOBJ)gdevpdtt.$(OBJ) : $(DEVVECSRC)gdevpdtt.c $(gx_h) $(math__h) $(string__h)\
 $(gserrors_h) $(gsencs_h) $(gscedata_h) $(gsmatrix_h) $(gzstate_h)\
 $(gxfcache_h) $(gxfont_h) $(gxfont0_h) $(gxfcid_h) $(gxfcopy_h)\
 $(gxfcmap_h) $(gxpath_h) $(gxchar_h) $(gxstate_h) $(gdevpdfx_h) $(gdevpdfg_h)\
 $(gdevpdfo_h) $(gdevpdtx_h) $(gdevpdtd_h) $(gdevpdtf_h) $(gdevpdts_h) $(gdevpdtt_h)\
 $(gdevpdti_h) $(gxhldevc_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtt.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtt.c

$(DEVOBJ)gdevpdtv.$(OBJ) : $(DEVVECSRC)gdevpdtv.c $(gx_h) $(gdevpdtv_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtv.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtv.c

$(DEVOBJ)gdevpdtw.$(OBJ) : $(DEVVECSRC)gdevpdtw.c $(gx_h) $(gserrors_h) $(memory__h)\
 $(gxfcmap_h) $(gxfont_h) $(gxfcopy_h) $(gscencs_h)\
 $(gdevpsf_h) $(gdevpdfx_h) $(gdevpdfo_h)\
 $(gdevpdtd_h) $(gdevpdtf_h) $(gdevpdti_h) $(gdevpdtw_h) $(gdevpdtv_h) $(sarc4_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpdtw.$(OBJ) $(C_) $(DEVVECSRC)gdevpdtw.c

$(DEVOBJ)whitelst.$(OBJ) : $(DEVVECSRC)whitelst.c $(whitelst_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)whitelst.$(OBJ) $(C_) $(DEVVECSRC)whitelst.c

################ END PDF WRITER ################

# High-level PCL XL writer

pxl_=$(DEVOBJ)gdevpx.$(OBJ) $(DEVOBJ)gdevpxut.$(OBJ) $(HPPCL)
$(DD)pxlmono.dev : $(pxl_) $(GDEV) $(GLD)vector.dev \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)pxlmono $(pxl_)
	$(ADDMOD) $(DD)pxlmono -include $(GLD)vector

$(DD)pxlcolor.dev : $(pxl_) $(GDEV) $(GLD)vector.dev \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)pxlcolor $(pxl_)
	$(ADDMOD) $(DD)pxlcolor -include $(GLD)vector

$(DEVOBJ)gdevpx.$(OBJ) : $(DEVVECSRC)gdevpx.c\
 $(math__h) $(memory__h) $(string__h)\
 $(gx_h) $(gsccolor_h) $(gsdcolor_h) $(gxiparam_h) $(gserrors_h)\
 $(gxcspace_h) $(gxdevice_h) $(gxpath_h) $(gdevmrop_h)\
 $(gdevpxat_h) $(gdevpxen_h) $(gdevpxop_h) $(gdevpxut_h) $(gdevvec_h)\
 $(srlx_h) $(strimpl_h) $(jpeglib__h) $(sdct_h) $(sjpeg_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpx.$(OBJ) $(C_) $(DEVVECSRC)gdevpx.c

# XPS writer. Uses libtiff for all images

libtiff_dev=$(TIFFGENDIR)$(D)libtiff.dev
tiff_i_=-include $(TIFFGENDIR)$(D)libtiff

xpswrite_=$(DEVOBJ)gdevxps.$(OBJ)
$(DD)xpswrite.dev : $(xpswrite_) $(GDEV) $(GLD)vector.dev \
$(libtiff_dev) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)xpswrite $(xpswrite_)
	$(ADDMOD) $(DD)xpswrite -include $(GLD)vector $(tiff_i_)

$(DEVOBJ)gdevxps_1.$(OBJ) : $(DEVVECSRC)gdevxps.c $(gdevvec_h) \
$(string__h) $(stdio__h) $(libtiff_dev) $(gx_h) $(gserrors_h) \
$(gxpath_h) $(gzcpath_h) $(stream_h) \
$(stdint__h) $(gdevtifs_h) $(gsicc_create_h) $(gsicc_cache_h) \
$(gximdecode_h) $(DEVS_MAK) $(MAKEDIRS)
	$(XPSDEVCC) $(I_)$(TI_)$(_I) $(GLO_)gdevxps_1.$(OBJ) $(C_) $(DEVVECSRC)gdevxps.c

$(DEVOBJ)gdevxps_0.$(OBJ) : $(DEVVECSRC)gdevxps.c $(gdevvec_h) \
$(string__h) $(stdio__h) $(libtiff_dev) $(gx_h) $(gserrors_h) \
$(gxpath_h) $(gzcpath_h) $(stream_h) $(zlib_h) \
$(stdint__h) $(gdevtifs_h) $(gsicc_create_h) $(gsicc_cache_h) \
$(gximdecode_h) $(DEVS_MAK) $(MAKEDIRS)
	$(XPSDEVCC) $(I_)$(TI_)$(_I) $(GLO_)gdevxps_0.$(OBJ) $(C_) $(DEVVECSRC)gdevxps.c

$(DEVOBJ)gdevxps.$(OBJ) : $(DEVOBJ)gdevxps_$(SHARE_ZLIB).$(OBJ) $(DEVS_MAK) $(MAKEDIRS)
	$(CP_) $(DEVOBJ)gdevxps_$(SHARE_ZLIB).$(OBJ) $(DEVOBJ)gdevxps.$(OBJ)

###### --------------------- Raster file formats --------------------- ######

### --------------------- The "plain bits" devices ---------------------- ###

# This device also exercises the driver CRD facilities, which is why it
# needs some additional files.

bit_=$(DEVOBJ)gdevbit.$(OBJ) $(DEVOBJ)gdevdcrd.$(OBJ)

$(DD)bit.dev : $(bit_) $(GLD)page.dev $(GLD)cielib.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bit $(bit_)
	$(ADDMOD) $(DD)bit -include $(GLD)cielib

$(DD)bitrgb.dev : $(bit_) $(GLD)page.dev $(GLD)cielib.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bitrgb $(bit_)
	$(ADDMOD) $(DD)bitrgb -include $(GLD)cielib

$(DD)bitcmyk.dev : $(bit_) $(GLD)page.dev $(GLD)cielib.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bitcmyk $(bit_)
	$(ADDMOD) $(DD)bitcmyk -include $(GLD)cielib

$(DD)bitrgbtags.dev : $(bit_) $(GLD)page.dev $(GLD)cielib.dev\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bitrgbtags $(bit_)
	$(ADDMOD) $(DD)bitrgbtags -include $(GLD)cielib

$(DEVOBJ)gdevbit.$(OBJ) : $(DEVSRC)gdevbit.c $(PDEVH)\
 $(gsparam_h) $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gxlum_h) $(gxdcconv_h)\
 $(gsutil_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbit.$(OBJ) $(C_) $(DEVSRC)gdevbit.c

### --------------------- The chameleon device ---------------------- ###

chameleon_=$(DEVOBJ)gdevchameleon.$(OBJ) $(DEVOBJ)gdevdcrd.$(OBJ)

$(DD)chameleon.dev : $(chameleon_) $(GLD)page.dev $(GLD)cielib.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)chameleon $(chameleon_)
	$(ADDMOD) $(DD)chameleon -include $(GLD)cielib

$(DEVOBJ)gdevchameleon.$(OBJ) : $(DEVSRC)gdevchameleon.c $(PDEVH)\
 $(gsparam_h) $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gxlum_h) $(gxdcconv_h)\
 $(gsutil_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevchameleon.$(OBJ) $(C_) $(DEVSRC)gdevchameleon.c

### ------------------------- .BMP file formats ------------------------- ###

gdevbmp_h=$(DEVSRC)gdevbmp.h

bmp_=$(DEVOBJ)gdevbmp.$(OBJ) $(DEVOBJ)gdevbmpc.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ)

$(DEVOBJ)gdevbmp.$(OBJ) : $(DEVSRC)gdevbmp.c $(PDEVH) $(gdevbmp_h) $(gdevpccm_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbmp.$(OBJ) $(C_) $(DEVSRC)gdevbmp.c

$(DEVOBJ)gdevbmpc.$(OBJ) : $(DEVSRC)gdevbmpc.c $(PDEVH) $(gdevbmp_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevbmpc.$(OBJ) $(C_) $(DEVSRC)gdevbmpc.c

$(DD)bmpmono.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmpmono $(bmp_)

$(DD)bmpgray.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmpgray $(bmp_)

$(DD)bmpsep1.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmpsep1 $(bmp_)

$(DD)bmpsep8.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmpsep8 $(bmp_)

$(DD)bmp16.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmp16 $(bmp_)

$(DD)bmp256.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmp256 $(bmp_)

$(DD)bmp16m.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmp16m $(bmp_)

$(DD)bmp32b.dev : $(bmp_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)bmp32b $(bmp_)

### --------------------------- The XCF device ------------------------- ###

xcf_=$(DEVOBJ)gdevxcf.$(OBJ)

$(DD)xcf.dev : $(xcf_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)xcf $(xcf_)

$(DD)xcfcmyk.dev : $(xcf_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)xcfcmyk $(xcf_)

$(DEVOBJ)gdevxcf.$(OBJ) : $(DEVSRC)gdevxcf.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gxdcconv_h) $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevxcf.$(OBJ) $(C_) $(DEVSRC)gdevxcf.c

### --------------------------- The PSD device ------------------------- ###

psd_=$(DEVOBJ)gdevpsd.$(OBJ) $(GLOBJ)gdevdevn.$(OBJ) $(GLOBJ)gsequivc.$(OBJ)
gdevpsd_h=$(DEVSRC)gdevpsd.h

$(DD)psdrgb.dev : $(psd_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)psdrgb $(psd_)

$(DD)psdcmyk.dev : $(psd_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)psdcmyk $(psd_)

$(DEVOBJ)gdevpsd.$(OBJ) : $(DEVSRC)gdevpsd.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gxdevsop_h) $(gsequivc_h)\
 $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) $(gxgetbit_h)\
 $(gdevppla_h) $(gxiodev_h) $(gdevpsd_h) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsd.$(OBJ) $(C_) $(DEVSRC)gdevpsd.c

### --------------------------- The GPRF device ------------------------- ###

gprf_=$(DEVOBJ)gdevgprf.$(OBJ) $(GLOBJ)gdevdevn.$(OBJ) $(GLOBJ)gsequivc.$(OBJ) $(DEVOBJ)gdevppla.$(OBJ)

$(DD)gprf.dev : $(gprf_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV) $(DD)gprf $(gprf_)

$(DEVOBJ)gdevgprf_1.$(OBJ) : $(DEVSRC)gdevgprf.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gsequivc_h) \
 $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) $(gxgetbit_h)\
 $(gdevppla_h) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevgprf_1.$(OBJ) $(II)$(ZI_)$(_I) $(C_) $(DEVSRC)gdevgprf.c

$(DEVOBJ)gdevgprf_0.$(OBJ) : $(DEVSRC)gdevgprf.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gsequivc_h) $(zlib_h)\
 $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) $(gxgetbit_h)\
 $(gdevppla_h) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevgprf_0.$(OBJ) $(II)$(ZI_)$(_I) $(C_) $(DEVSRC)gdevgprf.c

$(DEVOBJ)gdevgprf.$(OBJ) : $(DEVOBJ)gdevgprf_$(SHARE_ZLIB).$(OBJ) $(DEVS_MAK) $(MAKEDIRS)
	$(CP_) $(DEVOBJ)gdevgprf_$(SHARE_ZLIB).$(OBJ) $(DEVOBJ)gdevgprf.$(OBJ)

### ----------------------- The permutation device --------------------- ###

perm_=$(DEVOBJ)gdevperm.$(OBJ)

$(DD)perm.dev : $(perm_) $(GLD)page.dev $(GDEV)
	$(SETDEV) $(DD)perm $(perm_)

$(DEVOBJ)gdevperm.$(OBJ) : $(DEVSRC)gdevperm.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevperm.$(OBJ) $(C_) $(DEVSRC)gdevperm.c

### ------------------------ JBIG2 testing device ---------------------- ###

gdevjbig2_=$(DEVOBJ)gdevjbig2.$(OBJ)

$(DD)gdevjbig2.dev : $(gdevjbig2_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)gdevjbig2 $(gdevjbig2_)

$(DEVOBJ)gdevjbig2.$(OBJ) : $(DEVSRC)gdevjbig2.c $(PDEVH)\
 $(stream_h) $(strimpl_h) $(sjbig2_luratech_h) $(DEVS_MAK) $(MAKEDIRS)
	$(GDEVLDFJB2CC) $(DEVO_)gdevjbig2.$(OBJ) $(C_) $(DEVSRC)gdevjbig2.c

### ------------------------ JPX testing device ----------------------
###

gdevjpx_=$(DEVOBJ)gdevjpx.$(OBJ)

$(DD)jpxrgb.dev : $(gdevjpx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpxrgb $(gdevjpx_)

$(DD)jpxgray.dev : $(gdevjpx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpxgray $(gdevjpx_)

$(DD)jpxcmyk.dev : $(gdevjpx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpxcmyk $(gdevjpx_)

$(DEVOBJ)gdevjpx.$(OBJ) : $(DEVSRC)gdevjpx.c $(PDEVH)\
 $(stream_h) $(strimpl_h) $(sjpx_luratech_h) $(DEVS_MAK) $(MAKEDIRS)
	$(GDEVLWFJPXCC) $(DEVO_)gdevjpx.$(OBJ) $(C_) $(DEVSRC)gdevjpx.c

### ------------------------- JPEG file format ------------------------- ###

jpeg_=$(DEVOBJ)gdevjpeg.$(OBJ)

# RGB output
$(DD)jpeg.dev : $(jpeg_) $(GLD)sdcte.dev $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpeg $(jpeg_)
	$(ADDMOD) $(DD)jpeg -include $(GLD)sdcte

# Gray output
$(DD)jpeggray.dev : $(jpeg_) $(GLD)sdcte.dev $(GLD)page.dev\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpeggray $(jpeg_)
	$(ADDMOD) $(DD)jpeggray -include $(GLD)sdcte

# CMYK output
$(DD)jpegcmyk.dev : $(jpeg_) $(GLD)sdcte.dev $(GLD)page.dev\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)jpegcmyk $(jpeg_)
	$(ADDMOD) $(DD)jpegcmyk -include $(GLD)sdcte

$(DEVOBJ)gdevjpeg.$(OBJ) : $(DEVSRC)gdevjpeg.c $(PDEVH)\
 $(stdio__h) $(jpeglib__h)\
 $(sdct_h) $(sjpeg_h) $(stream_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevjpeg.$(OBJ) $(C_) $(DEVSRC)gdevjpeg.c

### ------------------------- MIFF file format ------------------------- ###
### Right now we support only 24-bit direct color, but we might add more ###
### formats in the future.                                               ###

miff_=$(DEVOBJ)gdevmiff.$(OBJ)

$(DD)miff24.dev : $(miff_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)miff24 $(miff_)

$(DEVOBJ)gdevmiff.$(OBJ) : $(DEVSRC)gdevmiff.c $(PDEVH) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevmiff.$(OBJ) $(C_) $(DEVSRC)gdevmiff.c

### ------------------------- PCX file formats ------------------------- ###

pcx_=$(DEVOBJ)gdevpcx.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ)

$(DEVOBJ)gdevpcx.$(OBJ) : $(DEVSRC)gdevpcx.c $(PDEVH) $(gdevpccm_h) $(gxlum_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpcx.$(OBJ) $(C_) $(DEVSRC)gdevpcx.c

$(DD)pcxmono.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcxmono $(pcx_)

$(DD)pcxgray.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcxgray $(pcx_)

$(DD)pcx16.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcx16 $(pcx_)

$(DD)pcx256.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcx256 $(pcx_)

$(DD)pcx24b.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcx24b $(pcx_)

$(DD)pcxcmyk.dev : $(pcx_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pcxcmyk $(pcx_)

### ------------------- Portable Bitmap file formats ------------------- ###
### For more information, see the pam(5), pbm(5), pgm(5), and ppm(5)     ###
### man pages.                                                           ###

pxm_=$(DEVOBJ)gdevpbm.$(OBJ) $(GLOBJ)gdevppla.$(OBJ) $(GLOBJ)gdevmpla.$(OBJ)

$(DEVOBJ)gdevpbm.$(OBJ) : $(DEVSRC)gdevpbm.c $(PDEVH)\
 $(gdevmpla_h) $(gdevplnx_h) $(gdevppla_h)\
 $(gscdefs_h) $(gscspace_h) $(gxgetbit_h) $(gxiparam_h) $(gxlum_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpbm.$(OBJ) $(C_) $(DEVSRC)gdevpbm.c

### Portable Bitmap (PBM, plain or raw format, magic numbers "P1" or "P4")

$(DD)pbm.dev : $(pxm_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pbm $(pxm_)

$(DD)pbmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pbmraw $(pxm_)

### Portable Graymap (PGM, plain or raw format, magic numbers "P2" or "P5")

$(DD)pgm.dev : $(pxm_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pgm $(pxm_)

$(DD)pgmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pgmraw $(pxm_)

# PGM with automatic optimization to PBM if this is possible.

$(DD)pgnm.dev : $(pxm_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pgnm $(pxm_)

$(DD)pgnmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pgnmraw $(pxm_)

### Portable Pixmap (PPM, plain or raw format, magic numbers "P3" or "P6")

$(DD)ppm.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ppm $(pxm_)

$(DD)ppmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)ppmraw $(pxm_)

# PPM with automatic optimization to PGM or PBM if possible.

$(DD)pnm.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pnm $(pxm_)

$(DD)pnmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pnmraw $(pxm_)

$(DD)pnmcmyk.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pnmcmyk $(pxm_)

### Portable inKmap (CMYK internally, converted to PPM=RGB at output time)

$(DD)pkm.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pkm $(pxm_)

$(DD)pkmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pkmraw $(pxm_)

### Portable Separated map (CMYK internally, produces 4 monobit pages)

$(DD)pksm.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pksm $(pxm_)

$(DD)pksmraw.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pksmraw $(pxm_)

### Plan 9 bitmap format

$(DD)plan9bm.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plan9bm $(pxm_)

### Portable Arbitrary Map (PAM, magic number "P7", CMYK)

$(DD)pamcmyk4.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pamcmyk4 $(pxm_)

$(DD)pamcmyk32.dev : $(pxm_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pamcmyk32 $(pxm_)

# Keep the older (non-descriptive) name in case it is being used
$(DD)pam.dev : $(pxm_) $(GLD)page.dev $(GDEV)  $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pam $(pxm_)

### --------------- Portable Network Graphics file format --------------- ###
### Requires libpng 0.81 and zlib 0.95 (or more recent versions).         ###
### See png.mak and zlib.mak for more details.                         ###

png_=$(DEVOBJ)gdevpng.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ)
libpng_dev=$(PNGGENDIR)$(D)libpng.dev
png_i_=-include $(PNGGENDIR)$(D)libpng

$(DEVOBJ)gdevpng.$(OBJ) : $(DEVSRC)gdevpng.c\
 $(gdevprn_h) $(gdevpccm_h) $(gscdefs_h) $(png__h) $(DEVS_MAK) $(MAKEDIRS)
	$(CC_) $(I_)$(DEVI_) $(II)$(PI_)$(_I) $(PCF_) $(GLF_) $(DEVO_)gdevpng.$(OBJ) $(C_) $(DEVSRC)gdevpng.c

$(DD)pngmono.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pngmono $(png_)
	$(ADDMOD) $(DD)pngmono $(png_i_)

$(DD)pngmonod.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pngmonod $(png_)
	$(ADDMOD) $(DD)pngmonod $(png_i_)

$(DD)pnggray.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pnggray $(png_)
	$(ADDMOD) $(DD)pnggray $(png_i_)

$(DD)png16.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)png16 $(png_)
	$(ADDMOD) $(DD)png16 $(png_i_)

$(DD)png256.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)png256 $(png_)
	$(ADDMOD) $(DD)png256 $(png_i_)

$(DD)png16m.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)png16m $(png_)
	$(ADDMOD) $(DD)png16m $(png_i_)

$(DD)png48.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)png48 $(png_)
	$(ADDMOD) $(DD)png48 $(png_i_)

$(DD)pngalpha.dev : $(libpng_dev) $(png_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pngalpha $(png_)
	$(ADDMOD) $(DD)pngalpha $(png_i_)

### --------------- Portable Network Graphics file format --------------- ###
### Requires zlib 0.95 (or more recent versions).                         ###
### See zlib.mak for more details.                                        ###

fpng_=$(DEVOBJ)gdevfpng.$(OBJ) $(DEVOBJ)gdevpccm.$(OBJ)

$(DEVOBJ)gdevfpng_0.$(OBJ) : $(DEVSRC)gdevfpng.c\
 $(gdevprn_h) $(gxdevsop_h) $(gdevpccm_h) $(gscdefs_h) $(zlib_h) $(DEVS_MAK) $(MAKEDIRS)
	$(CC_) $(I_)$(DEVI_) $(II)$(ZI_)$(_I) $(PCF_) $(GLF_) $(DEVO_)gdevfpng_0.$(OBJ) $(C_) $(DEVSRC)gdevfpng.c

$(DEVOBJ)gdevfpng_1.$(OBJ) : $(DEVSRC)gdevfpng.c\
 $(gdevprn_h) $(gdevpccm_h) $(gscdefs_h) $(DEVS_MAK) $(MAKEDIRS)
	$(CC_) $(I_)$(DEVI_) $(II)$(ZI_)$(_I) $(PCF_) $(GLF_) $(DEVO_)gdevfpng_1.$(OBJ) $(C_) $(DEVSRC)gdevfpng.c

$(DEVOBJ)gdevfpng.$(OBJ) : $(DEVOBJ)gdevfpng_$(SHARE_ZLIB).$(OBJ) $(DEVS_MAK) $(MAKEDIRS)
	$(CP_) $(DEVOBJ)gdevfpng_$(SHARE_ZLIB).$(OBJ) $(DEVOBJ)gdevfpng.$(OBJ)

$(DD)fpng.dev : $(fpng_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)fpng $(fpng_)
	$(ADDMOD) $(DD)fpng $(fpng_i_)

### ---------------------- PostScript image format ---------------------- ###
### These devices make it possible to print monochrome Level 2 files on a ###
###   Level 1 printer, by converting them to a bitmap in PostScript       ###
###   format.  They also can convert big, complex color PostScript files  ###
###   to (often) smaller and more easily printed bitmaps.                 ###

psim_=$(DEVOBJ)gdevpsim.$(OBJ) $(DEVOBJ)gdevpsu.$(OBJ)

$(DEVOBJ)gdevpsim.$(OBJ) : $(DEVSRC)gdevpsim.c $(PDEVH)\
 $(gdevpsu_h)\
 $(sa85x_h) $(srlx_h) $(stream_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsim.$(OBJ) $(C_) $(DEVSRC)gdevpsim.c

### --- Minimum Feature Size support functions --- ###

# Required by fax and 1bpp tiff functions. The grouping of functions
# within files means it is also pulled in for color/cmyk tiff functions
# too.

minftrsz_h=$(DEVSRC)minftrsz.h $(std_h)
minftrsz_=$(minftrsz_h) $(DEVOBJ)minftrsz.$(OBJ)

$(DEVOBJ)minftrsz.$(OBJ) : $(DEVSRC)minftrsz.c $(minftrsz_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)minftrsz.$(OBJ) $(C_) $(DEVSRC)minftrsz.c


### ---------------- Fax encoding ---------------- ###

# By default, these drivers recognize 3 page sizes -- (U.S.) letter, A4, and
# B4 -- and adjust the page width to the nearest legal value for real fax
# systems (1728 or 2048 pixels).  To suppress this, set the device parameter
# AdjustWidth to 0 (e.g., -dAdjustWidth=0 on the command line).

gdevfax_h=$(DEVSRC)gdevfax.h

fax_=$(DEVOBJ)gdevfax.$(OBJ) $(DEVOBJ)minftrsz.$(OBJ)
$(DD)fax.dev : $(libtiff_dev) $(fax_) $(GLD)cfe.dev $(minftrsz_h)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)fax $(fax_)
	$(ADDMOD) $(DD)fax -include $(GLD)cfe $(tiff_i_)

$(DEVOBJ)gdevfax.$(OBJ) : $(DEVSRC)gdevfax.c $(PDEVH)\
 $(gdevfax_h) $(scfx_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevfax.$(OBJ) $(C_) $(DEVSRC)gdevfax.c

$(DD)faxg3.dev : $(libtiff_dev) $(DD)fax.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)faxg3 -include $(DD)fax
	$(ADDMOD) $(DD)faxg3 $(tiff_i_)

$(DD)faxg32d.dev : $(libtiff_dev) $(DD)fax.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)faxg32d -include $(DD)fax
	$(ADDMOD) $(DD)faxg32d $(tiff_i_)

$(DD)faxg4.dev : $(libtiff_dev) $(DD)fax.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)faxg4 -include $(DD)fax
	$(ADDMOD) $(DD)faxg4 $(tiff_i_)

### -------------------- Plain or TIFF fax encoding --------------------- ###
###    Use -sDEVICE=tiffg3 or tiffg4 and				  ###
###	  -r204x98 for low resolution output, or			  ###
###	  -r204x196 for high resolution output				  ###

gdevtifs_h=$(DEVSRC)gdevtifs.h

tfax_=$(DEVOBJ)gdevtfax.$(OBJ) $(DEVOBJ)minftrsz.$(OBJ)
$(DD)tfax.dev : $(libtiff_dev) $(tfax_) $(GLD)cfe.dev\
 $(GLD)lzwe.dev $(GLD)rle.dev $(DD)fax.dev $(DD)tiffs.dev $(minftrsz_h)\
 $(gstiffio_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)tfax $(tfax_)
	$(ADDMOD) $(DD)tfax -include $(GLD)cfe $(GLD)lzwe $(GLD)rle
	$(ADDMOD) $(DD)tfax -include $(DD)fax $(DD)tiffs $(tiff_i_)

$(DEVOBJ)gdevtfax.$(OBJ) : $(DEVSRC)gdevtfax.c $(PDEVH)\
 $(stdint__h) $(gdevfax_h) $(gdevtifs_h)\
 $(scfx_h) $(slzwx_h) $(srlx_h) $(strimpl_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(I_)$(TI_)$(_I) $(DEVO_)gdevtfax.$(OBJ) $(C_) $(DEVSRC)gdevtfax.c

### ---------------------------- TIFF formats --------------------------- ###

tiffs_=$(DEVOBJ)gdevtifs.$(OBJ) $(DEVOBJ)minftrsz.$(OBJ)

tiffgray_=$(DEVOBJ)gdevtsep.$(OBJ) $(GLOBJ)gsequivc.$(OBJ) $(DEVOBJ)minftrsz.$(OBJ)

tiffsep_=$(tiffgray_) $(GLOBJ)gdevdevn.$(OBJ) $(GLOBJ)gsequivc.$(OBJ) \
$(GLOBJ)gdevppla.$(OBJ)

$(DD)tiffs.dev : $(libtiff_dev) $(tiffs_) $(GLD)page.dev\
 $(minftrsz_) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)tiffs $(tiffs_)
	$(ADDMOD) $(DD)tiffs -include $(GLD)page $(tiff_i_)

$(DEVOBJ)gdevtifs.$(OBJ) : $(DEVSRC)gdevtifs.c $(PDEVH) $(stdint__h) $(stdio__h) $(time__h)\
 $(gdevtifs_h) $(gscdefs_h) $(gstypes_h) $(stream_h) $(strmio_h) $(gstiffio_h)\
 $(gsicc_cache_h) $(gscms_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(I_)$(DEVI_) $(II)$(TI_)$(_I) $(DEVO_)gdevtifs.$(OBJ) $(C_) $(DEVSRC)gdevtifs.c

# Black & white, G3/G4 fax
# NOTE: see under faxg* above regarding page width adjustment.

$(DD)tiffcrle.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tiffcrle -include $(DD)tfax
	$(ADDMOD) $(DD)tiffcrle $(tiff_i_)

$(DD)tiffg3.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tiffg3 -include $(DD)tfax
	$(ADDMOD) $(DD)tiffg3 $(tiff_i_)

$(DD)tiffg32d.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tiffg32d -include $(DD)tfax
	$(ADDMOD) $(DD)tiffg32d $(tiff_i_)

$(DD)tiffg4.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tiffg4 -include $(DD)tfax
	$(ADDMOD) $(DD)tiffg4 $(tiff_i_)

# Black & white, LZW compression

$(DD)tifflzw.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tifflzw -include $(DD)tfax
	$(ADDMOD) $(DD)tifflzw $(tiff_i_)

# Black & white, PackBits compression

$(DD)tiffpack.dev : $(libtiff_dev) $(DD)tfax.dev $(minftrsz_)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)tiffpack -include $(DD)tfax
	$(ADDMOD) $(DD)tiffpack $(tiff_i_)

# TIFF Gray, no compression

$(DD)tiffgray.dev : $(libtiff_dev) $(tiffgray_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffgray $(tiffgray_)
	$(ADDMOD) $(DD)tiffgray -include $(DD)tiffs $(tiff_i_)

$(DEVOBJ)gdevtsep.$(OBJ) : $(DEVSRC)gdevtsep.c $(PDEVH) $(stdint__h)\
 $(gdevtifs_h) $(gdevdevn_h) $(gxdevsop_h) $(gsequivc_h) $(stdio__h) $(ctype__h)\
 $(gxgetbit_h) $(gdevppla_h) $(gp_h) $(gstiffio_h) $(gsicc_h)\
 $(gscms_h) $(gsicc_cache_h) $(gxdevsop_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(I_)$(TI_)$(_I) $(DEVO_)gdevtsep.$(OBJ) $(C_) $(DEVSRC)gdevtsep.c

# TIFF Scaled (downscaled gray -> mono), configurable compression

tiffscaled_=$(tiffsep_)

$(DD)tiffscaled.dev : $(libtiff_dev) $(tiffscaled_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffscaled $(tiffscaled_)
	$(ADDMOD) $(DD)tiffscaled -include $(DD)tiffs $(tiff_i_)

# TIFF Scaled 8 (downscaled gray -> gray), configurable compression

tiffscaled8_=$(tiffsep_)

$(DD)tiffscaled8.dev : $(libtiff_dev) $(tiffscaled8_)\
 $(DD)tiffs.dev $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffscaled8 $(tiffscaled8_)
	$(ADDMOD) $(DD)tiffscaled8 -include $(DD)tiffs $(tiff_i_)

# TIFF Scaled 24 (downscaled rgb -> rgb), configurable compression

tiffscaled24_=$(tiffsep_)

$(DD)tiffscaled24.dev : $(libtiff_dev) $(tiffscaled24_)\
 $(DD)tiffs.dev $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffscaled24 $(tiffscaled8_)
	$(ADDMOD) $(DD)tiffscaled24 -include $(DD)tiffs $(tiff_i_)

# TIFF Scaled 32 (downscaled cmyk -> cmyk), configurable compression

tiffscaled32_=$(tiffsep_)

$(DD)tiffscaled32.dev : $(libtiff_dev) $(tiffscaled32_)\
 $(DD)tiffs.dev $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffscaled32 $(tiffscaled8_)
	$(ADDMOD) $(DD)tiffscaled32 -include $(DD)tiffs $(tiff_i_)

# TIFF Scaled 4 (downscaled cmyk -> cmyk), configurable compression

tiffscaled4_=$(tiffsep_)

$(DD)tiffscaled4.dev : $(libtiff_dev) $(tiffscaled4_)\
 $(DD)tiffs.dev $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffscaled4 $(tiffscaled8_)
	$(ADDMOD) $(DD)tiffscaled4 -include $(DD)tiffs $(tiff_i_)

# TIFF RGB, no compression

tiffrgb_=$(DEVOBJ)gdevtfnx.$(OBJ) $(DEVOBJ)minftrsz.$(OBJ)

$(DD)tiff12nc.dev : $(libtiff_dev) $(tiffrgb_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiff12nc $(tiffrgb_)
	$(ADDMOD) $(DD)tiff12nc -include $(DD)tiffs $(tiff_i_)

$(DD)tiff24nc.dev : $(libtiff_dev) $(tiffrgb_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiff24nc $(tiffrgb_)
	$(ADDMOD) $(DD)tiff24nc -include $(DD)tiffs $(tiff_i_)

$(DD)tiff48nc.dev : $(libtiff_dev) $(tiffrgb_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiff48nc $(tiffrgb_)
	$(ADDMOD) $(DD)tiff48nc -include $(DD)tiffs $(tiff_i_)

$(DEVOBJ)gdevtfnx.$(OBJ) : $(DEVSRC)gdevtfnx.c $(PDEVH) $(stdint__h)\
 $(gdevtifs_h) $(gscms_h) $(gstiffio_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(I_)$(TI_)$(_I) $(DEVO_)gdevtfnx.$(OBJ) $(C_) $(DEVSRC)gdevtfnx.c

# TIFF CMYK, no compression

$(DD)tiff32nc.dev : $(libtiff_dev) $(tiffgray_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiff32nc $(tiffgray_)
	$(ADDMOD) $(DD)tiff32nc -include $(DD)tiffs $(tiff_i_)

$(DD)tiff64nc.dev : $(libtiff_dev) $(tiffgray_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiff64nc $(tiffgray_)
	$(ADDMOD) $(DD)tiff64nc -include $(DD)tiffs $(tiff_i_)

#
# Create separation files (tiffgray) plus CMYK composite (tiff32nc)

$(DD)tiffsep.dev : $(libtiff_dev) $(tiffsep_) $(DD)tiffs.dev\
 $(minftrsz_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffsep $(tiffsep_)
	$(ADDMOD) $(DD)tiffsep -include $(DD)tiffs $(tiff_i_)

#
# Create separation files (tiff 1-bit)

$(DD)tiffsep1.dev : $(tiffsep_) $(DD)tiffs.dev $(minftrsz_h)\
 $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)tiffsep1 $(tiffsep_)
	$(ADDMOD) $(DD)tiffsep1 -include $(DD)tiffs

#
# PLANar device

plan_=$(DEVOBJ)gdevplan.$(OBJ) $(DEVOBJ)gdevppla.$(OBJ) $(DEVOBJ)gdevmpla.$(OBJ)

$(DEVOBJ)gdevplan.$(OBJ) : $(DEVSRC)gdevplan.c $(PDEVH)\
 $(gdevmpla_h) $(gdevplnx_h) $(gdevppla_h)\
 $(gscdefs_h) $(gscspace_h) $(gxgetbit_h) $(gxiparam_h) $(gxlum_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevplan.$(OBJ) $(C_) $(DEVSRC)gdevplan.c

$(DD)plan.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plan $(plan_)

$(DD)plang.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plang $(plan_)

$(DD)planm.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)planm $(plan_)

$(DD)planc.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)planc $(plan_)

$(DD)plank.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plank $(plan_)

$(DD)planr.dev : $(plan_) $(GLD)page.dev $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)planr $(plan_)

#
# PLanar Interlaced Buffer device

plib_=$(DEVOBJ)gdevplib.$(OBJ) $(GLOBJ)gdevppla.$(OBJ) $(GLOBJ)gdevmpla.$(OBJ)

$(DEVOBJ)gdevplib.$(OBJ) : $(DEVSRC)gdevplib.c $(PDEVH)\
 $(gdevmpla_h) $(gdevplnx_h) $(gdevppla_h)\
 $(gscdefs_h) $(gscspace_h) $(gxgetbit_h) $(gxiparam_h) $(gxlum_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevplib.$(OBJ) $(C_) $(DEVSRC)gdevplib.c

$(DD)plib.dev : $(plib_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plib $(plib_)

$(DD)plibg.dev : $(plib_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plibg $(plib_)

$(DD)plibm.dev : $(plib_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plibm $(plib_)

$(DD)plibc.dev : $(plib_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plibc $(plib_)

$(DD)plibk.dev : $(plib_) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)plibk $(plib_)

# ink coverage device  --  a device that records the ink coverage
# on each page, and discards the page.
$(DD)inkcov.dev : $(ECHOGS_XE) $(LIB_MAK) $(DEVOBJ)gdevicov.$(OBJ) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)inkcov $(DEVOBJ)gdevicov.$(OBJ)

$(DD)ink_cov.dev : $(ECHOGS_XE) $(LIB_MAK) $(DEVOBJ)gdevicov.$(OBJ) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETDEV2) $(DD)ink_cov $(DEVOBJ)gdevicov.$(OBJ)

$(DEVOBJ)gdevicov.$(OBJ) : $(DEVSRC)gdevicov.c $(AK) \
  $(arch_h) $(gdevprn_h) $(stdio__h)  $(stdint__h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevicov.$(OBJ) $(C_) $(DEVSRC)gdevicov.c


### ------------------------------- CUPS ------------------------------- ###
lcups_dev=$(LCUPSGENDIR)$(D)lcups.dev
lcupsi_dev=$(LCUPSIGENDIR)$(D)lcupsi.dev

cups_=$(DEVOBJ)gdevcups.$(OBJ)
$(DD)cups.dev : $(lcups_dev) $(lcupsi_dev) $(cups_) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cups $(cups_)
	$(ADDMOD) $(DD)cups -include $(lcups_dev)
	$(ADDMOD) $(DD)cups -include $(lcupsi_dev)
$(DD)pwgraster.dev : $(lcups_dev) $(lcupsi_dev) $(cups_) $(GDEV) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pwgraster $(cups_)
	$(ADDMOD) $(DD)pwgraster -include $(lcups_dev)
	$(ADDMOD) $(DD)pwgraster -include $(lcupsi_dev)

$(DEVOBJ)gdevcups.$(OBJ) : $(LCUPSSRCDIR)$(D)gdevcups.c $(std_h) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(CUPS_CC) $(DEVO_)gdevcups.$(OBJ) $(C_) $(CFLAGS) $(CUPSCFLAGS) \
	    $(I_)$(GLSRC) \
	    $(I_)$(DEVSRC) \
            $(I_)$(DEVOBJ) $(I_)$(LCUPSSRCDIR)$(D)libs \
            $(LCUPSSRCDIR)$(D)gdevcups.c

### ---------------------------- Tracing -------------------------------- ###

# A tracing device, also an example of a high-level device.

$(DEVOBJ)gdevtrac.$(OBJ) : $(DEVSRC)gdevtrac.c $(AK) $(gx_h)\
 $(gserrors_h) $(gscspace_h)\
 $(gxdevice_h) $(gxdht_h) $(gxfont_h) $(gxiparam_h) $(gxgstate_h)\
 $(gxpaint_h) $(gxtmap_h) $(gzcpath_h) $(gzpath_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevtrac.$(OBJ) $(C_) $(DEVSRC)gdevtrac.c

$(DD)tracedev.dev : $(GDEV) $(DEVOBJ)gdevtrac.$(OBJ) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETMOD) $(DD)tracedev -dev2 tr_mono tr_rgb tr_cmyk
	$(ADDMOD) $(DD)tracedev -obj $(DEVOBJ)gdevtrac.$(OBJ)

###@@@--------------- PSDCMYKOG device --------------------------###

psdcmykog_=$(DEVOBJ)gdevcmykog.$(OBJ)

$(DD)psdcmykog.dev : $(GDEV) $(psdcmykog_) $(DD)page.dev $(DD)psdcmyk.dev \
  $(GLOBJ)gdevdevn.$(OBJ) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV) $(DD)psdcmykog $(psdcmykog_)

$(DEVOBJ)gdevcmykog.$(OBJ) : $(DEVSRC)gdevcmykog.c $(GDEV) \
 $(GDEVH) $(gdevdevn_h) $(gsequivc_h) $(gxdevsop_h) $(gdevdevnprn_h) $(gdevpsd_h) \
 $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevcmykog.$(OBJ) $(C_) $(DEVSRC)gdevcmykog.c

### -------- PDF as an image downscaled device --------------------- ###
$(DD)pdfimage8.dev : $(DEVOBJ)gdevpdfimg.$(OBJ) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pdfimage8 $(DEVOBJ)gdevpdfimg.$(OBJ)
	$(ADDMOD) $(DD)pdfimage8 -include $(GLD)page

$(DD)pdfimage24.dev : $(DEVOBJ)gdevpdfimg.$(OBJ) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pdfimage24 $(DEVOBJ)gdevpdfimg.$(OBJ)
	$(ADDMOD) $(DD)pdfimage24 -include $(GLD)page

$(DD)pdfimage32.dev : $(DEVOBJ)gdevpdfimg.$(OBJ) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pdfimage32 $(DEVOBJ)gdevpdfimg.$(OBJ)
	$(ADDMOD) $(DD)pdfimage32 -include $(GLD)page

$(DD)PCLm.dev : $(DEVOBJ)gdevpdfimg.$(OBJ) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)PCLm $(DEVOBJ)gdevpdfimg.$(OBJ)
	$(ADDMOD) $(DD)PCLm -include $(GLD)page

$(DEVOBJ)gdevpdfimg.$(OBJ) : $(DEVSRC)gdevpdfimg.c $(AK) \
  $(DEVS_MAK) $(MAKEDIRS) $(arch_h) $(stdint__h) $(gdevprn_h) $(gxdownscale_h) \
  $(stream_h) $(spprint_h) $(time__h) $(smd5_h) $(sstring_h) $(strimpl_h) \
  $(slzwx_h) $(szlibx_h) $(jpeglib__h) $(sdct_h) $(srlx_h) $(gsicc_cache_h) $(sjpeg_h)
	$(DEVCC) $(DEVO_)gdevpdfimg.$(OBJ) $(C_) $(DEVSRC)gdevpdfimg.c
