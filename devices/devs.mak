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

gxfcopy_h=$(DEVSRC)gxfcopy.h

# Define the header files for device drivers.  Every header file used by
# more than one device driver family must be listed here.
gdev8bcm_h=$(DEVSRC)gdev8bcm.h
gdevcbjc_h=$(DEVSRC)gdevcbjc.h $(stream_h)

gdevpcl_h=$(DEVSRC)gdevpcl.h
gdevpsu_h=$(DEVVECSRC)gdevpsu.h
# Out of order
gdevdljm_h=$(DEVSRC)gdevdljm.h

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

$(DEVOBJ)gdevdsp.$(OBJ) : $(DEVSRC)gdevdsp.c $(string__h) $(gdevkrnlsclass_h)\
 $(gp_h) $(gpcheck_h) $(gdevpccm_h) $(gsparam_h) $(gsdevice_h)\
 $(GDEVH) $(gxdevmem_h) $(gdevdevn_h) $(gsequivc_h) $(gdevdsp_h) $(gdevdsp2_h) \
  $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevdsp.$(OBJ) $(C_) $(DEVSRC)gdevdsp.c

### -------------------------- The X11 device -------------------------- ###

# Please note that Artifex Software Inc does not support Ghostview.
# For more information about Ghostview, please contact Tim Theisen
# (ghostview@cs.wisc.edu).

gdevxcmp_h=$(DEVSRC)gdevxcmp.h
gdevx_h=$(DEVSRC)gdevx.h

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
 $(gxdevmem_h) $(gxgetbit_h) $(gxiparam_h) $(gxpath_h) $(gdevkrnlsclass_h) \
 $(DEVS_MAK) $(MAKEDIRS)
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

RINKJ_INCL=$(I_)$(RINKJ_SRCDIR)$(_I) $(I_)$(GLSRC)$(_I) $(I_)$(DEVOBJ)$(_I)
RINKJ_CC=$(CC_) $(RINKJ_INCL)

rinkj_byte_stream_h=$(RINKJ_SRC)rinkj-byte-stream.h
rinkj_core=$(RINKJ_OBJ)evenbetter-rll.$(OBJ) \
 $(RINKJ_OBJ)rinkj-byte-stream.$(OBJ) $(RINKJ_OBJ)rinkj-device.$(OBJ) \
 $(RINKJ_OBJ)rinkj-config.$(OBJ) $(RINKJ_OBJ)rinkj-dither.$(OBJ) \
 $(RINKJ_OBJ)rinkj-epson870.$(OBJ) $(RINKJ_OBJ)rinkj-screen-eb.$(OBJ)

$(rinkj_byte_stream_h):$(gp_h)

$(RINKJ_OBJ)evenbetter-rll.$(OBJ) : $(RINKJ_SRC)evenbetter-rll.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)evenbetter-rll.$(OBJ) $(C_) $(RINKJ_SRC)evenbetter-rll.c

$(RINKJ_OBJ)rinkj-byte-stream.$(OBJ) : $(RINKJ_SRC)rinkj-byte-stream.c \
 $(rinkj_byte_stream_h) $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-byte-stream.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-byte-stream.c

$(RINKJ_OBJ)rinkj-device.$(OBJ) : $(RINKJ_SRC)rinkj-device.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-device.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-device.c

$(RINKJ_OBJ)rinkj-config.$(OBJ) : $(RINKJ_SRC)rinkj-config.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-config.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-config.c

$(RINKJ_OBJ)rinkj-dither.$(OBJ) : $(RINKJ_SRC)rinkj-dither.c $(DEVS_MAK) $(MAKEDIRS)
	$(RINKJ_CC) $(RINKJ_O_)rinkj-dither.$(OBJ) $(C_) $(RINKJ_SRC)rinkj-dither.c

$(RINKJ_OBJ)rinkj-epson870.$(OBJ) : $(RINKJ_SRC)rinkj-epson870.c $(rinkj_byte_stream_h) $(DEVS_MAK) $(MAKEDIRS)
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

gdevpsdf_h=$(DEVVECSRC)gdevpsdf.h
gdevpsds_h=$(DEVVECSRC)gdevpsds.h

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

$(DEVOBJ)gdevtxtw.$(OBJ) : $(DEVVECSRC)gdevtxtw.c $(GDEV) $(gdevkrnlsclass_h) \
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
gdevpdfg_h=$(DEVVECSRC)gdevpdfg.h
gdevpdfo_h=$(DEVVECSRC)gdevpdfo.h
gdevpdfx_h=$(DEVVECSRC)gdevpdfx.h

opdfread_h=$(DEVGEN)opdfread.h

$(DEVGEN)opdfread_1.h : $(PACKPS_XE) $(DEVVECSRC)opdfread.ps
	$(EXP)$(PACKPS_XE) -d -c -n opdfread_ps -o $(DEVGEN)opdfread_1.h $(DEVVECSRC)opdfread.ps

$(DEVGEN)opdfread_.h : $(PACKPS_XE) $(DEVVECSRC)opdfread.ps
	$(EXP)$(PACKPS_XE) -c -n opdfread_ps -o $(DEVGEN)opdfread_.h $(DEVVECSRC)opdfread.ps

$(DEVGEN)opdfread.h : $(DEVGEN)opdfread_$(DEBUG_OPDFREAD).h
	$(CP_) $(DEVGEN)opdfread_$(DEBUG_OPDFREAD).h $(opdfread_h)

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

gdevpsf_h=$(DEVVECSRC)gdevpsf.h

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
gdevpdtx_h=$(DEVVECSRC)gdevpdtx.h
gdevpdtb_h=$(DEVVECSRC)gdevpdtb.h
gdevpdtd_h=$(DEVVECSRC)gdevpdtd.h
gdevpdtf_h=$(DEVVECSRC)gdevpdtf.h
gdevpdti_h=$(DEVVECSRC)gdevpdti.h
gdevpdts_h=$(DEVVECSRC)gdevpdts.h
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

$(DD)psdrgb16.dev : $(DEVS_MAK) $(psd_) $(GLD)page.dev $(GDEV)
	$(SETDEV) $(DD)psdrgb16 $(psd_)

$(DD)psdcmyk16.dev : $(DEVS_MAK) $(psd_) $(GLD)page.dev $(GDEV)
	$(SETDEV) $(DD)psdcmyk16 $(psd_)

$(DEVOBJ)gdevpsd.$(OBJ) : $(DEVSRC)gdevpsd.c $(PDEVH) $(math__h)\
 $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gsparam_h) $(gxlum_h)\
 $(gstypes_h) $(gxdcconv_h) $(gdevdevn_h) $(gxdevsop_h) $(gsequivc_h)\
 $(gscms_h) $(gsicc_cache_h) $(gsicc_manage_h) $(gxgetbit_h)\
 $(gdevppla_h) $(gxiodev_h) $(gdevpsd_h) $(gxdevsop_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevpsd.$(OBJ) $(C_) $(DEVSRC)gdevpsd.c

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

minftrsz_h=$(DEVSRC)minftrsz.h
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
 $(stdint__h) $(gdevfax_h) $(gdevtifs_h) $(gdevkrnlsclass_h) \
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
 $(gsicc_cache_h) $(gdevkrnlsclass_h) $(gscms_h) $(DEVS_MAK) $(MAKEDIRS)
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

$(DEVOBJ)gdevtsep_0.$(OBJ) : $(DEVSRC)gdevtsep.c $(PDEVH) $(stdint__h)\
 $(gdevtifs_h) $(gdevdevn_h) $(gxdevsop_h) $(gsequivc_h) $(stdio__h) $(ctype__h)\
 $(gxdht_h) $(gxiodev_h) $(gxdownscale_h) $(gzht_h)\
 $(gxgetbit_h) $(gdevppla_h) $(gp_h) $(gstiffio_h) $(gsicc_h)\
 $(gscms_h) $(gsicc_cache_h) $(gxdevsop_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(I_)$(TI_)$(_I) $(DEVO_)gdevtsep_0.$(OBJ) $(C_) $(DEVSRC)gdevtsep.c

$(DEVOBJ)gdevtsep_1.$(OBJ) : $(DEVSRC)gdevtsep.c $(PDEVH) $(stdint__h)\
 $(gdevtifs_h) $(gdevdevn_h) $(gxdevsop_h) $(gsequivc_h) $(stdio__h) $(ctype__h)\
 $(gxdht_h) $(gxiodev_h) $(gxdownscale_h) $(gzht_h)\
 $(gxgetbit_h) $(gdevppla_h) $(gp_h) $(gstiffio_h) $(gsicc_h) $(cal_h)\
 $(gscms_h) $(gsicc_cache_h) $(gxdevsop_h) $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(D_)WITH_CAL$(_D) $(I_)$(CALSRCDIR)$(_I) $(I_)$(TI_)$(_I) $(DEVO_)gdevtsep_1.$(OBJ) $(C_) $(DEVSRC)gdevtsep.c

$(DEVOBJ)gdevtsep.$(OBJ) : $(DEVOBJ)gdevtsep_$(WITH_CAL).$(OBJ)
	$(CP_) $(DEVOBJ)gdevtsep_$(WITH_CAL).$(OBJ) $(DEVOBJ)gdevtsep.$(OBJ)
			 

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
$(DD)cups.dev : $(lcups_dev) $(lcupsi_dev) $(cups_) $(GDEV) $(GLD)page.dev \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)cups $(cups_)
	$(ADDMOD) $(DD)cups -include $(lcups_dev)
	$(ADDMOD) $(DD)cups -include $(lcupsi_dev)
	$(ADDMOD) $(DD)cups -include $(GLD)page
$(DD)pwgraster.dev : $(lcups_dev) $(lcupsi_dev) $(cups_) $(GDEV) $(GLD)page.dev \
 $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)pwgraster $(cups_)
	$(ADDMOD) $(DD)pwgraster -include $(lcups_dev)
	$(ADDMOD) $(DD)pwgraster -include $(lcupsi_dev)
	$(ADDMOD) $(DD)pwgraster -include $(GLD)page

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

$(DEVOBJ)gdevpdfimg.$(OBJ) : $(DEVSRC)gdevpdfimg.c $(AK) $(gdevkrnlsclass_h) \
  $(DEVS_MAK) $(MAKEDIRS) $(arch_h) $(stdint__h) $(gdevprn_h) $(gxdownscale_h) \
  $(stream_h) $(spprint_h) $(time__h) $(smd5_h) $(sstring_h) $(strimpl_h) \
  $(slzwx_h) $(szlibx_h) $(jpeglib__h) $(sdct_h) $(srlx_h) $(gsicc_cache_h) $(sjpeg_h)
	$(DEVCC) $(DEVO_)gdevpdfimg.$(OBJ) $(C_) $(DEVSRC)gdevpdfimg.c

### -------- URF device --------------------- ###
urf=$(DEVOBJ)gdevurf.$(OBJ)
$(DD)urfgray.dev : $(urf) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)urfgray $(urf)
	$(ADDMOD) $(DD)urfgray -include $(GLD)page

$(DD)urfrgb.dev : $(urf) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)urfrgb $(urf)
	$(ADDMOD) $(DD)urfrgb -include $(GLD)page

$(DD)urfcmyk.dev : $(urf) $(GLD)page.dev $(GDEV) $(DEVS_MAK) $(MAKEDIRS)
	$(SETPDEV2) $(DD)urfcmyk $(urf)
	$(ADDMOD) $(DD)urfcmyk -include $(GLD)page

$(DEVOBJ)gdevurf.$(OBJ) : $(URFSRCDIR)$(D)gdevurf.c $(AK) $(PDEVH) \
 $(gsparam_h) $(gdevdcrd_h) $(gscrd_h) $(gscrdp_h) $(gxlum_h) $(gxdcconv_h)\
 $(gsutil_h) $(DEVS_MAK) $(MAKEDIRS)
	$(DEVCC) $(DEVO_)gdevurf.$(OBJ) $(C_) $(URFSRCDIR)$(D)gdevurf.c

# Dependencies:
$(DEVSRC)gxfcopy.h:$(GLSRC)gsfont.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gxfcopy.h:$(GLSRC)scommon.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gsstype.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gsmemory.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gslibctx.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gxfcopy.h:$(GLSRC)stdio_.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gsgstate.h
$(DEVSRC)gxfcopy.h:$(GLSRC)stdint_.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gssprintf.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gsccode.h
$(DEVSRC)gxfcopy.h:$(GLSRC)std.h
$(DEVSRC)gxfcopy.h:$(GLSRC)gstypes.h
$(DEVSRC)gxfcopy.h:$(GLSRC)stdpre.h
$(DEVSRC)gxfcopy.h:$(GLGEN)arch.h
$(DEVSRC)gdev8bcm.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdev8bcm.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gstext.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscms.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gp.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevpcl.h:$(GLSRC)srdline.h
$(DEVSRC)gdevpcl.h:$(GLSRC)scommon.h
$(DEVSRC)gdevpcl.h:$(GLSRC)memento.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevpcl.h:$(GLSRC)stat_.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevpcl.h:$(GLSRC)memory_.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevpcl.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevpcl.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevpcl.h:$(GLSRC)std.h
$(DEVSRC)gdevpcl.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevpcl.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevpcl.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpsu.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpsu.h:$(GLGEN)arch.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gdevprn.h
$(DEVSRC)gdevdljm.h:$(GLSRC)string_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsstrtok.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxclthrd.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxclpage.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevdljm.h:$(DEVSRC)gdevpcl.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxline.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevdljm.h:$(GLSRC)math_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gx.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gdebug.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxband.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsht.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscie.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gstext.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscms.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsio.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gp.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsutil.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsstrl.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gdbflags.h
$(DEVSRC)gdevdljm.h:$(GLSRC)srdline.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gserrors.h
$(DEVSRC)gdevdljm.h:$(GLSRC)scommon.h
$(DEVSRC)gdevdljm.h:$(GLSRC)memento.h
$(DEVSRC)gdevdljm.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevdljm.h:$(GLSRC)stat_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevdljm.h:$(GLSRC)memory_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevdljm.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevdljm.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevdljm.h:$(GLSRC)std.h
$(DEVSRC)gdevdljm.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevdljm.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevdljm.h:$(GLGEN)arch.h
$(DEVSRC)gdevxcmp.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevxcmp.h:$(GLSRC)x_.h
$(DEVSRC)gdevxcmp.h:$(GLSRC)std.h
$(DEVSRC)gdevxcmp.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevxcmp.h:$(GLGEN)arch.h
$(DEVSRC)gdevx.h:$(GLSRC)gdevbbox.h
$(DEVSRC)gdevx.h:$(DEVSRC)gdevxcmp.h
$(DEVSRC)gdevx.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevx.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevx.h:$(GLSRC)gstext.h
$(DEVSRC)gdevx.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevx.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevx.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevx.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevx.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevx.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevx.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevx.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevx.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevx.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevx.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevx.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevx.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevx.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevx.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevx.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevx.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevx.h:$(GLSRC)gscms.h
$(DEVSRC)gdevx.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevx.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevx.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevx.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevx.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevx.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevx.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevx.h:$(GLSRC)x_.h
$(DEVSRC)gdevx.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevx.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevx.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevx.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevx.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevx.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevx.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevx.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevx.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevx.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevx.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevx.h:$(GLSRC)gp.h
$(DEVSRC)gdevx.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevx.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevx.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevx.h:$(GLSRC)srdline.h
$(DEVSRC)gdevx.h:$(GLSRC)scommon.h
$(DEVSRC)gdevx.h:$(GLSRC)memento.h
$(DEVSRC)gdevx.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevx.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevx.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevx.h:$(GLSRC)stat_.h
$(DEVSRC)gdevx.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevx.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevx.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevx.h:$(GLSRC)memory_.h
$(DEVSRC)gdevx.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevx.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevx.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevx.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevx.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevx.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevx.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevx.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevx.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevx.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevx.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevx.h:$(GLSRC)std.h
$(DEVSRC)gdevx.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevx.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevx.h:$(GLGEN)arch.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevpxut.h:$(GLSRC)scommon.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gdevpxat.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gdevpxen.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gdevpxop.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevpxut.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevpxut.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevpxut.h:$(GLSRC)std.h
$(DEVSRC)gdevpxut.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevpxut.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevpxut.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpsdf.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpsdf.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpsds.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpsds.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdfc.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdfc.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdfc.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdfc.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdfg.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdfg.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdfg.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdfg.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdfo.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdfo.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)smd5.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsmd5.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdfo.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdfo.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdfx.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdfx.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdfx.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfcid.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gstype1.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfont42.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxcid.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpsf.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpsf.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdt.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdt.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdt.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdt.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtx.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtx.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtx.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtx.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtx.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtb.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdtb.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtb.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtb.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtb.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtb.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtd.h:$(DEVVECSRC)gdevpdtb.h
$(DEVVECSRC)gdevpdtd.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdtd.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtd.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtd.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtd.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtd.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtf.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdtf.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtf.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtf.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtf.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtf.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdti.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdti.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdti.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdti.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdti.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdti.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdts.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdts.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdts.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdts.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdts.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdts.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpdts.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpdtf.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtt.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtt.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtt.h:$(GLGEN)arch.h
$(DEVVECSRC)gdevpdtw.h:$(DEVVECSRC)gdevpdtx.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfcmap.h
$(DEVVECSRC)gdevpdtw.h:$(DEVVECSRC)gdevpdt.h
$(DEVVECSRC)gdevpdtw.h:$(DEVVECSRC)gdevpdfx.h
$(DEVVECSRC)gdevpdtw.h:$(DEVVECSRC)gdevpsdf.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gdevvec.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxgstate.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxline.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gstrans.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)sarc4.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsht1.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfont.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gdevbbox.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)math_.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)scfx.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gdevp14.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcolor2.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)spprint.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)spsdf.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxpcolor.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gdevdevn.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsequivc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gspath.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxblend.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxclipsr.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcomp.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxdcolor.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcid.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxmatrix.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsgdata.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxbitfmt.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)shc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsgcache.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscolor2.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfapi.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscindex.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsnotify.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsfcmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxdevice.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsht.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcpath.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)sa85x.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxiparam.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)sa85d.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxdevmem.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxdevcli.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxpcache.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsptype1.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxtext.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscie.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gstext.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsnamecl.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gstparam.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxstate.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gspcolor.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfcache.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)stream.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcspace.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsropt.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsfunc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsmalloc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxrplane.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxctable.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxiodev.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsuid.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxhldevc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)strimpl.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsimage.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsdcolor.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxdda.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcvalue.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsfont.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxiclass.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxftype.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfrac.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscms.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscspace.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxpath.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxbcache.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsdevice.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxarith.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxstdio.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gspenum.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxhttile.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsrect.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gslparam.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsxfont.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsiparam.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsdsrc.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsio.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxbitmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsmatrix.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscpm.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxfixed.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsrefct.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsparam.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gp.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsccolor.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsstruct.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxsync.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)srdline.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)scommon.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)memento.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)vmsmath.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscsel.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsbitmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsfname.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsstype.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)stat_.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxtmap.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsmemory.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gpsync.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)memory_.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gpgetenv.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gslibctx.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscdefs.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gs_dll_call.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)stdio_.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gscompt.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsbittab.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gxcindex.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsgstate.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)stdint_.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gssprintf.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gsccode.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)std.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)gstypes.h
$(DEVVECSRC)gdevpdtw.h:$(GLSRC)stdpre.h
$(DEVVECSRC)gdevpdtw.h:$(GLGEN)arch.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxline.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevbmp.h:$(GLSRC)math_.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxband.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsht.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscie.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gstext.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscms.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsio.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gp.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevbmp.h:$(GLSRC)srdline.h
$(DEVSRC)gdevbmp.h:$(GLSRC)scommon.h
$(DEVSRC)gdevbmp.h:$(GLSRC)memento.h
$(DEVSRC)gdevbmp.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevbmp.h:$(GLSRC)stat_.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevbmp.h:$(GLSRC)memory_.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevbmp.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevbmp.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevbmp.h:$(GLSRC)std.h
$(DEVSRC)gdevbmp.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevbmp.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevbmp.h:$(GLGEN)arch.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdevdevnprn.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdevprn.h
$(DEVSRC)gdevpsd.h:$(GLSRC)string_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsstrtok.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxclthrd.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxclpage.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxline.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevpsd.h:$(GLSRC)math_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gx.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdebug.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxband.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsht.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscie.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gstext.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscms.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsio.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gp.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsutil.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsstrl.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gdbflags.h
$(DEVSRC)gdevpsd.h:$(GLSRC)srdline.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gserrors.h
$(DEVSRC)gdevpsd.h:$(GLSRC)scommon.h
$(DEVSRC)gdevpsd.h:$(GLSRC)memento.h
$(DEVSRC)gdevpsd.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevpsd.h:$(GLSRC)stat_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevpsd.h:$(GLSRC)memory_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevpsd.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevpsd.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevpsd.h:$(GLSRC)std.h
$(DEVSRC)gdevpsd.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevpsd.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevpsd.h:$(GLGEN)arch.h
$(DEVSRC)minftrsz.h:$(GLSRC)std.h
$(DEVSRC)minftrsz.h:$(GLSRC)stdpre.h
$(DEVSRC)minftrsz.h:$(GLGEN)arch.h
$(DEVSRC)gdevfax.h:$(GLSRC)gdevprn.h
$(DEVSRC)gdevfax.h:$(GLSRC)string_.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsstrtok.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxclthrd.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxclpage.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxline.h
$(DEVSRC)gdevfax.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevfax.h:$(GLSRC)math_.h
$(DEVSRC)gdevfax.h:$(GLSRC)scfx.h
$(DEVSRC)gdevfax.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevfax.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevfax.h:$(GLSRC)gx.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevfax.h:$(GLSRC)gdebug.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxband.h
$(DEVSRC)gdevfax.h:$(GLSRC)shc.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsht.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscie.h
$(DEVSRC)gdevfax.h:$(GLSRC)gstext.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevfax.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevfax.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscms.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevfax.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevfax.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsio.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevfax.h:$(GLSRC)gp.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsutil.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsstrl.h
$(DEVSRC)gdevfax.h:$(GLSRC)gdbflags.h
$(DEVSRC)gdevfax.h:$(GLSRC)srdline.h
$(DEVSRC)gdevfax.h:$(GLSRC)gserrors.h
$(DEVSRC)gdevfax.h:$(GLSRC)scommon.h
$(DEVSRC)gdevfax.h:$(GLSRC)memento.h
$(DEVSRC)gdevfax.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevfax.h:$(GLSRC)stat_.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevfax.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevfax.h:$(GLSRC)memory_.h
$(DEVSRC)gdevfax.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevfax.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevfax.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevfax.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevfax.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsbittab.h
$(DEVSRC)gdevfax.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevfax.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevfax.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevfax.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevfax.h:$(GLSRC)std.h
$(DEVSRC)gdevfax.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevfax.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevfax.h:$(GLGEN)arch.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gdevprn.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdownscale.h
$(DEVSRC)gdevtifs.h:$(GLSRC)string_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsstrtok.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxclthrd.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxclpage.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxclist.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxgstate.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxline.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gstrans.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxgetbit.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsht1.h
$(DEVSRC)gdevtifs.h:$(GLSRC)math_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gdevp14.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcolor2.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxpcolor.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gdevdevn.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsequivc.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gx.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxblend.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxclipsr.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcomp.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdcolor.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gdebug.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxmatrix.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxbitfmt.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdevbuf.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxband.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscolor2.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscindex.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdevice.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsht.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcpath.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdevmem.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdevcli.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxpcache.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsptype1.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxtext.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscie.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gstext.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsnamecl.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gstparam.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxstate.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gspcolor.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxfcache.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcspace.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsropt.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsfunc.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsmalloc.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxrplane.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxctable.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsuid.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcmap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsimage.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsdcolor.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxdda.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcvalue.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsfont.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxfmap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxiclass.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxftype.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxfrac.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscms.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscspace.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxpath.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxbcache.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsdevice.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxarith.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxstdio.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gspenum.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxhttile.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsrect.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gslparam.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsxfont.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxclio.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsiparam.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsdsrc.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsio.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxbitmap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsmatrix.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscpm.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxfixed.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsrefct.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsparam.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gp.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsccolor.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsstruct.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxsync.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsutil.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsstrl.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gdbflags.h
$(DEVSRC)gdevtifs.h:$(GLSRC)srdline.h
$(DEVSRC)gdevtifs.h:$(GLSRC)claptrap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gserrors.h
$(DEVSRC)gdevtifs.h:$(GLSRC)scommon.h
$(DEVSRC)gdevtifs.h:$(GLSRC)memento.h
$(DEVSRC)gdevtifs.h:$(GLSRC)vmsmath.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscsel.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsbitmap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsfname.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsstype.h
$(DEVSRC)gdevtifs.h:$(GLSRC)stat_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxtmap.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsmemory.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gpsync.h
$(DEVSRC)gdevtifs.h:$(GLSRC)memory_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gpgetenv.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gslibctx.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscdefs.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gs_dll_call.h
$(DEVSRC)gdevtifs.h:$(GLSRC)ctype_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)stdio_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gscompt.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gxcindex.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsgstate.h
$(DEVSRC)gdevtifs.h:$(GLSRC)stdint_.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gssprintf.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gsccode.h
$(DEVSRC)gdevtifs.h:$(GLSRC)std.h
$(DEVSRC)gdevtifs.h:$(GLSRC)gstypes.h
$(DEVSRC)gdevtifs.h:$(GLSRC)stdpre.h
$(DEVSRC)gdevtifs.h:$(GLGEN)arch.h
