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
# makefile for libtiff.
# Users of this makefile must define the following:
#	TIFFSRCDIR    - the source directory
#	TIFFGEN       - the generated intermediate file directory
#	SHARE_LIBTIFF - 0 to compile libtiff, 1 to share
#	LIBTIFF_NAME  - if SHARE_LIBTIFF=1, the name of the shared library

# (Rename directories.)
TIFFSRC=$(TIFFSRCDIR)$(D)
TIFFCONF=$(TIFFCONFDIR)$(D)
TIFFGEN=$(TIFFGENDIR)$(D)
TIFFOBJ=$(TIFFOBJDIR)$(D)
TIFFO_=$(O_)$(TIFFOBJ)
JPEGGEN=$(JGENDIR)$(D)

TIFFCONFIG_H=$(TIFFCONF)libtiff$(D)tif_config$(TIFFCONFIG_SUFFIX).h
TIFFCONF_H=$(TIFFCONF)libtiff$(D)tiffconf$(TIFFCONFIG_SUFFIX).h

# Define the name of this makefile.
LIBTIFF_MAK=$(GLSRC)tiff.mak $(TOP_MAKEFILES)

TIFFCC=$(CC_) $(TIFF_CFLAGS) $(I_)$(TI_) $(II)$(JI_)$(_I) $(PF_)

TIFFDEP = $(AK) $(TIFFGEN)tif_config.h $(TIFFGEN)tiffconf.h $(LIBTIFF_MAK) $(MAKEDIRS)
gstiffio_h=$(GLSRC)gstiffio.h

tiff_1=$(TIFFOBJ)tif_aux.$(OBJ) $(TIFFOBJ)tif_close.$(OBJ) $(TIFFOBJ)tif_codec.$(OBJ) $(TIFFOBJ)tif_color.$(OBJ)
tiff_2=$(TIFFOBJ)tif_compress.$(OBJ) $(TIFFOBJ)tif_dir.$(OBJ) $(TIFFOBJ)tif_dirinfo.$(OBJ) $(TIFFOBJ)tif_dirread.$(OBJ)
tiff_3=$(TIFFOBJ)tif_dirwrite.$(OBJ) $(TIFFOBJ)tif_dumpmode.$(OBJ) $(TIFFOBJ)tif_error.$(OBJ) $(TIFFOBJ)tif_extension.$(OBJ)
tiff_4=$(TIFFOBJ)tif_fax3.$(OBJ) $(TIFFOBJ)tif_fax3sm.$(OBJ) $(TIFFOBJ)tif_flush.$(OBJ) $(TIFFOBJ)tif_getimage.$(OBJ)
tiff_5=$(TIFFOBJ)tif_jbig.$(OBJ) $(TIFFOBJ)tif_jpeg.$(OBJ) $(TIFFOBJ)tif_jpeg_12.$(OBJ) $(TIFFOBJ)tif_luv.$(OBJ) $(TIFFOBJ)tif_lzw.$(OBJ) $(TIFFOBJ)tif_webp.$(OBJ)
tiff_6=$(TIFFOBJ)tif_next.$(OBJ) $(TIFFOBJ)tif_ojpeg.$(OBJ) $(TIFFOBJ)tif_open.$(OBJ) $(TIFFOBJ)tif_packbits.$(OBJ)
tiff_7=$(TIFFOBJ)tif_pixarlog.$(OBJ) $(TIFFOBJ)tif_predict.$(OBJ) $(TIFFOBJ)tif_print.$(OBJ) $(TIFFOBJ)tif_read.$(OBJ)
tiff_8=$(TIFFOBJ)tif_strip.$(OBJ) $(TIFFOBJ)tif_swab.$(OBJ) $(TIFFOBJ)tif_thunder.$(OBJ) $(TIFFOBJ)tif_tile.$(OBJ)
# tiff_9=$(TIFFOBJ)tif_$(TIFFPLATFORM).$(OBJ) $(TIFFOBJ)tif_version.$(OBJ) $(TIFFOBJ)tif_warning.$(OBJ) $(TIFFOBJ)tif_write.$(OBJ)
tiff_9=$(TIFFOBJ)tif_version.$(OBJ) $(TIFFOBJ)tif_warning.$(OBJ) $(TIFFOBJ)tif_write.$(OBJ)
tiff_10=$(TIFFOBJ)tif_zip.$(OBJ)
tiff_11=$(TIFFOBJ)gstiffio.$(OBJ)

$(TIFFSRC)libtiff$(D)tif_config.unix.h : $(TIFFSRC)libtiff$(D)tif_config.h.in $(LIBTIFF_MAK)
	cd $(TIFFSRC) && ./configure
	$(CP_) $(TIFFCONF)libtiff$(D)tif_config.h $(TIFFCONF)libtiff$(D)tif_config.unix.h

$(TIFFSRC)libtiff$(D)tiffconf.unix.h : $(TIFFSRC)libtiff$(D)tiffconf.h.in $(LIBTIFF_MAK)
	cd $(TIFFSRC) && ./configure
	$(CP_) $(TIFFCONF)libtiff$(D)tiffconf.h $(TIFFCONF)libtiff$(D)tiffconf.unix.h

$(TIFFOBJ)tif_aux.$(OBJ) : $(TIFFSRC)/libtiff/tif_aux.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_aux.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_aux.c

$(TIFFOBJ)tif_close.$(OBJ) : $(TIFFSRC)/libtiff/tif_close.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_close.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_close.c

$(TIFFOBJ)tif_codec.$(OBJ) : $(TIFFSRC)/libtiff/tif_codec.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_codec.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_codec.c

$(TIFFOBJ)tif_color.$(OBJ) : $(TIFFSRC)/libtiff/tif_color.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_color.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_color.c

$(TIFFOBJ)tif_compress.$(OBJ) : $(TIFFSRC)/libtiff/tif_compress.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_compress.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_compress.c

$(TIFFOBJ)tif_dir.$(OBJ) : $(TIFFSRC)/libtiff/tif_dir.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_dir.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_dir.c

$(TIFFOBJ)tif_dirinfo.$(OBJ) : $(TIFFSRC)/libtiff/tif_dirinfo.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_dirinfo.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_dirinfo.c

$(TIFFOBJ)tif_dirread.$(OBJ) : $(TIFFSRC)/libtiff/tif_dirread.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_dirread.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_dirread.c

$(TIFFOBJ)tif_dirwrite.$(OBJ) : $(TIFFSRC)/libtiff/tif_dirwrite.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_dirwrite.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_dirwrite.c

$(TIFFOBJ)tif_dumpmode.$(OBJ) : $(TIFFSRC)/libtiff/tif_dumpmode.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_dumpmode.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_dumpmode.c

$(TIFFOBJ)tif_error.$(OBJ) : $(TIFFSRC)/libtiff/tif_error.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_error.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_error.c

$(TIFFOBJ)tif_extension.$(OBJ) : $(TIFFSRC)/libtiff/tif_extension.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_extension.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_extension.c

$(TIFFOBJ)tif_fax3.$(OBJ) : $(TIFFSRC)/libtiff/tif_fax3.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_fax3.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_fax3.c

$(TIFFOBJ)tif_fax3sm.$(OBJ) : $(TIFFSRC)/libtiff/tif_fax3sm.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_fax3sm.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_fax3sm.c

$(TIFFOBJ)tif_flush.$(OBJ) : $(TIFFSRC)/libtiff/tif_flush.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_flush.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_flush.c

$(TIFFOBJ)tif_getimage.$(OBJ) : $(TIFFSRC)/libtiff/tif_getimage.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_getimage.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_getimage.c

$(TIFFOBJ)tif_jbig.$(OBJ) : $(TIFFSRC)/libtiff/tif_jbig.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_jbig.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_jbig.c

$(TIFFOBJ)tif_jpeg_12.$(OBJ) : $(TIFFSRC)/libtiff/tif_jpeg_12.c $(TIFFDEP) $(JGENDIR)/jconfig.h
	$(TIFFCC) $(TIFFO_)tif_jpeg_12.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_jpeg_12.c

$(TIFFOBJ)tif_jpeg.$(OBJ) : $(TIFFSRC)/libtiff/tif_jpeg.c $(TIFFDEP) $(JGENDIR)/jconfig.h
	$(TIFFCC) $(TIFFO_)tif_jpeg.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_jpeg.c

$(TIFFOBJ)tif_luv.$(OBJ) : $(TIFFSRC)/libtiff/tif_luv.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_luv.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_luv.c

$(TIFFOBJ)tif_lzw.$(OBJ) : $(TIFFSRC)/libtiff/tif_lzw.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_lzw.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_lzw.c

$(TIFFOBJ)tif_webp.$(OBJ) : $(TIFFSRC)/libtiff/tif_webp.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_webp.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_webp.c

$(TIFFOBJ)tif_next.$(OBJ) : $(TIFFSRC)/libtiff/tif_next.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_next.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_next.c

$(TIFFOBJ)tif_ojpeg.$(OBJ) : $(TIFFSRC)/libtiff/tif_ojpeg.c $(jconfig_h) $(TIFFDEP)
	$(TIFFCC) $(I_)$(GLI_) $(TIFFO_)tif_ojpeg.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_ojpeg.c

$(TIFFOBJ)tif_open.$(OBJ) : $(TIFFSRC)/libtiff/tif_open.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_open.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_open.c

$(TIFFOBJ)tif_packbits.$(OBJ) : $(TIFFSRC)/libtiff/tif_packbits.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_packbits.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_packbits.c

$(TIFFOBJ)tif_pixarlog.$(OBJ) : $(TIFFSRC)/libtiff/tif_pixarlog.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_pixarlog.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_pixarlog.c

$(TIFFOBJ)tif_predict.$(OBJ) : $(TIFFSRC)/libtiff/tif_predict.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_predict.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_predict.c

$(TIFFOBJ)tif_print.$(OBJ) : $(TIFFSRC)/libtiff/tif_print.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_print.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_print.c

$(TIFFOBJ)tif_read.$(OBJ) : $(TIFFSRC)/libtiff/tif_read.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_read.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_read.c

$(TIFFOBJ)tif_strip.$(OBJ) : $(TIFFSRC)/libtiff/tif_strip.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_strip.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_strip.c

$(TIFFOBJ)tif_swab.$(OBJ) : $(TIFFSRC)/libtiff/tif_swab.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_swab.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_swab.c

$(TIFFOBJ)tif_thunder.$(OBJ) : $(TIFFSRC)/libtiff/tif_thunder.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_thunder.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_thunder.c

$(TIFFOBJ)tif_tile.$(OBJ) : $(TIFFSRC)/libtiff/tif_tile.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_tile.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_tile.c

$(TIFFOBJ)tif_version.$(OBJ) : $(TIFFSRC)/libtiff/tif_version.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_version.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_version.c

$(TIFFOBJ)tif_warning.$(OBJ) : $(TIFFSRC)/libtiff/tif_warning.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_warning.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_warning.c

$(TIFFOBJ)tif_write.$(OBJ) : $(TIFFSRC)/libtiff/tif_write.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_write.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_write.c

$(TIFFOBJ)tif_zip.$(OBJ) : $(TIFFSRC)/libtiff/tif_zip.c $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)tif_zip.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_zip.c

# tif_win32.c include <windows.h> and needed to be compiled with non-ansi extensions.
# so it has a different compiler flag compared to other tif_$(TIFFPLATFORM).c .
# We also have this target before tif_$(TIFFPLATFORM).c for this reason.
# $(TIFFOBJ)tif_win32.$(OBJ) : $(TIFFSRC)/libtiff/tif_win32.c $(TIFFDEP)
#	$(CC) $(I_)$(TI_) $(II)$(JI_)$(_I) $(PF_) $(TIFFO_)tif_win32.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_win32.c

## Generic target:
## we may need to add targets for openvms, mac classics, os2, etc later. For the time
## being only win32 and unix (including mac os x) are supported.
#$(TIFFOBJ)tif_$(TIFFPLATFORM).$(OBJ) : $(TIFFSRC)/libtiff/tif_$(TIFFPLATFORM).c $(TIFFDEP)
#	$(TIFFCC) $(TIFFO_)tif_$(TIFFPLATFORM).$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_$(TIFFPLATFORM).c

#$(TIFFOBJ)tif_unix.$(OBJ) : $(TIFFSRC)/libtiff/tif_unix.c $(TIFFDEP)
#	$(TIFFCC) $(TIFFO_)tif_unix.$(OBJ) $(C_) $(TIFFSRC)/libtiff/tif_unix.c

# instead of the platform specific files above, we include our own which stubs out
# the platform specific code, and routes via the Ghostscript I/O functions.
$(TIFFOBJ)gstiffio_0.$(OBJ) : $(GLSRC)gstiffio.c $(gstiffio_h) $(PDEVH) $(stdint__h) $(stdio__h) $(time__h)\
    $(gscdefs_h) $(gstypes_h) $(stream_h) $(strmio_h) $(malloc__h) $(TIFFDEP)
	$(TIFFCC) $(TIFFO_)gstiffio_0.$(OBJ) $(D_)SHARE_LIBTIFF=$(SHARE_LIBTIFF) $(C_) $(GLSRC)gstiffio.c

$(TIFFOBJ)gstiffio_1.$(OBJ) : $(GLSRC)gstiffio.c $(gstiffio_h) $(PDEVH) $(stdint__h) $(stdio__h) $(time__h)\
    $(gscdefs_h) $(gstypes_h) $(stream_h) $(strmio_h) $(malloc__h) $(LIBTIFF_MAK) $(MAKEDIRS)
	$(TIFFCC) $(TIFFO_)gstiffio_1.$(OBJ) $(D_)SHARE_LIBTIFF=$(SHARE_LIBTIFF) $(C_) $(GLSRC)gstiffio.c

$(TIFFOBJ)gstiffio.$(OBJ) : $(TIFFOBJ)gstiffio_$(SHARE_LIBTIFF).$(OBJ) $(LIBTIFF_MAK) $(MAKEDIRS)
	$(CP_) $(TIFFOBJ)gstiffio_$(SHARE_LIBTIFF).$(OBJ) $(TIFFOBJ)gstiffio.$(OBJ)

$(TIFFGEN)tif_config.h: $(TIFFCONFIG_H) $(LIBTIFF_MAK) $(MAKEDIRS)
	$(CP_) $(TIFFCONFIG_H) $(TIFFGEN)tif_config.h
	
$(TIFFGEN)tiffconf.h: $(TIFFCONF_H) $(LIBTIFF_MAK) $(MAKEDIRS)
	$(CP_) $(TIFFCONF_H) $(TIFFGEN)tiffconf.h
	
# Define the version of libtiff.dev that we are actually using.
$(TIFFGEN)libtiff.dev : $(TIFFGEN)libtiff_$(SHARE_LIBTIFF).dev $(LIBTIFF_MAK) $(MAKEDIRS)
	$(CP_) $(TIFFGEN)libtiff_$(SHARE_LIBTIFF).dev $(TIFFGEN)libtiff.dev


# Define the shared version.
$(TIFFGEN)libtiff_1.dev : $(LIBTIFF_MAK) $(ECHOGS_XE) $(JPEGGEN)jpegd.dev $(JPEGGEN)jpege.dev \
    $(tiff_11) $(MAKEDIRS)
	$(SETMOD) $(TIFFGEN)libtiff_1 $(tiff_11)
	$(ADDMOD) $(TIFFGEN)libtiff_1 -lib $(LIBTIFF_NAME)
	$(ADDMOD) $(TIFFGEN)libtiff_1 -include $(JPEGGEN)jpegd.dev
	$(ADDMOD) $(TIFFGEN)libtiff_1 -include $(JPEGGEN)jpege.dev

# Define the non-shared version.
$(TIFFGEN)libtiff_0.dev : $(LIBTIFF_MAK) $(ECHOGS_XE) \
    $(tiff_1) $(tiff_2) $(tiff_3) $(tiff_4) $(tiff_5) \
    $(tiff_6) $(tiff_7) $(tiff_8) $(tiff_9) $(tiff_10) $(tiff_11) \
    $(JPEGGEN)jpegd.dev $(JPEGGEN)jpege.dev $(MAKEDIRS)
	$(SETMOD) $(TIFFGEN)libtiff_0 $(tiff_1)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_2)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_3)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_4)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_5)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_6)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_7)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_8)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_9)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_10)
	$(ADDMOD) $(TIFFGEN)libtiff_0 $(tiff_11)
	$(ADDMOD) $(TIFFGEN)libtiff_0 -include $(JPEGGEN)jpegd.dev
	$(ADDMOD) $(TIFFGEN)libtiff_0 -include $(JPEGGEN)jpege.dev

# Dependencies:
$(GLSRC)gstiffio.h:$(GLSRC)gdevprn.h
$(GLSRC)gstiffio.h:$(GLSRC)string_.h
$(GLSRC)gstiffio.h:$(GLSRC)gsstrtok.h
$(GLSRC)gstiffio.h:$(GLSRC)gxclthrd.h
$(GLSRC)gstiffio.h:$(GLSRC)gxclpage.h
$(GLSRC)gstiffio.h:$(GLSRC)gxclist.h
$(GLSRC)gstiffio.h:$(GLSRC)gxgstate.h
$(GLSRC)gstiffio.h:$(GLSRC)gxline.h
$(GLSRC)gstiffio.h:$(GLSRC)gstrans.h
$(GLSRC)gstiffio.h:$(GLSRC)gsht1.h
$(GLSRC)gstiffio.h:$(GLSRC)math_.h
$(GLSRC)gstiffio.h:$(GLSRC)gdevp14.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcolor2.h
$(GLSRC)gstiffio.h:$(GLSRC)gxpcolor.h
$(GLSRC)gstiffio.h:$(GLSRC)gdevdevn.h
$(GLSRC)gstiffio.h:$(GLSRC)gsequivc.h
$(GLSRC)gstiffio.h:$(GLSRC)gx.h
$(GLSRC)gstiffio.h:$(GLSRC)gxblend.h
$(GLSRC)gstiffio.h:$(GLSRC)gxclipsr.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcomp.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdcolor.h
$(GLSRC)gstiffio.h:$(GLSRC)gdebug.h
$(GLSRC)gstiffio.h:$(GLSRC)gxmatrix.h
$(GLSRC)gstiffio.h:$(GLSRC)gxbitfmt.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdevbuf.h
$(GLSRC)gstiffio.h:$(GLSRC)gxband.h
$(GLSRC)gstiffio.h:$(GLSRC)gscolor2.h
$(GLSRC)gstiffio.h:$(GLSRC)gscindex.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdevice.h
$(GLSRC)gstiffio.h:$(GLSRC)gsht.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcpath.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdevmem.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdevcli.h
$(GLSRC)gstiffio.h:$(GLSRC)gxpcache.h
$(GLSRC)gstiffio.h:$(GLSRC)gsptype1.h
$(GLSRC)gstiffio.h:$(GLSRC)gxtext.h
$(GLSRC)gstiffio.h:$(GLSRC)gscie.h
$(GLSRC)gstiffio.h:$(GLSRC)gstext.h
$(GLSRC)gstiffio.h:$(GLSRC)gsnamecl.h
$(GLSRC)gstiffio.h:$(GLSRC)gstparam.h
$(GLSRC)gstiffio.h:$(GLSRC)gxstate.h
$(GLSRC)gstiffio.h:$(GLSRC)gspcolor.h
$(GLSRC)gstiffio.h:$(GLSRC)gxfcache.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcspace.h
$(GLSRC)gstiffio.h:$(GLSRC)gsropt.h
$(GLSRC)gstiffio.h:$(GLSRC)gsfunc.h
$(GLSRC)gstiffio.h:$(GLSRC)gsmalloc.h
$(GLSRC)gstiffio.h:$(GLSRC)gxrplane.h
$(GLSRC)gstiffio.h:$(GLSRC)gxctable.h
$(GLSRC)gstiffio.h:$(GLSRC)gsuid.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcmap.h
$(GLSRC)gstiffio.h:$(GLSRC)gsimage.h
$(GLSRC)gstiffio.h:$(GLSRC)gsdcolor.h
$(GLSRC)gstiffio.h:$(GLSRC)gxdda.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcvalue.h
$(GLSRC)gstiffio.h:$(GLSRC)gsfont.h
$(GLSRC)gstiffio.h:$(GLSRC)gxfmap.h
$(GLSRC)gstiffio.h:$(GLSRC)gxiclass.h
$(GLSRC)gstiffio.h:$(GLSRC)gxftype.h
$(GLSRC)gstiffio.h:$(GLSRC)gxfrac.h
$(GLSRC)gstiffio.h:$(GLSRC)gscms.h
$(GLSRC)gstiffio.h:$(GLSRC)gscspace.h
$(GLSRC)gstiffio.h:$(GLSRC)gxpath.h
$(GLSRC)gstiffio.h:$(GLSRC)gxbcache.h
$(GLSRC)gstiffio.h:$(GLSRC)gsdevice.h
$(GLSRC)gstiffio.h:$(GLSRC)gxarith.h
$(GLSRC)gstiffio.h:$(GLSRC)gxstdio.h
$(GLSRC)gstiffio.h:$(GLSRC)gspenum.h
$(GLSRC)gstiffio.h:$(GLSRC)gxhttile.h
$(GLSRC)gstiffio.h:$(GLSRC)gsrect.h
$(GLSRC)gstiffio.h:$(GLSRC)gslparam.h
$(GLSRC)gstiffio.h:$(GLSRC)gsxfont.h
$(GLSRC)gstiffio.h:$(GLSRC)gxclio.h
$(GLSRC)gstiffio.h:$(GLSRC)gsiparam.h
$(GLSRC)gstiffio.h:$(GLSRC)gsdsrc.h
$(GLSRC)gstiffio.h:$(GLSRC)gsio.h
$(GLSRC)gstiffio.h:$(GLSRC)gxbitmap.h
$(GLSRC)gstiffio.h:$(GLSRC)gsmatrix.h
$(GLSRC)gstiffio.h:$(GLSRC)gscpm.h
$(GLSRC)gstiffio.h:$(GLSRC)gxfixed.h
$(GLSRC)gstiffio.h:$(GLSRC)gsrefct.h
$(GLSRC)gstiffio.h:$(GLSRC)gsparam.h
$(GLSRC)gstiffio.h:$(GLSRC)gp.h
$(GLSRC)gstiffio.h:$(GLSRC)gsccolor.h
$(GLSRC)gstiffio.h:$(GLSRC)gsstruct.h
$(GLSRC)gstiffio.h:$(GLSRC)gxsync.h
$(GLSRC)gstiffio.h:$(GLSRC)gsutil.h
$(GLSRC)gstiffio.h:$(GLSRC)gsstrl.h
$(GLSRC)gstiffio.h:$(GLSRC)gdbflags.h
$(GLSRC)gstiffio.h:$(GLSRC)srdline.h
$(GLSRC)gstiffio.h:$(GLSRC)gserrors.h
$(GLSRC)gstiffio.h:$(GLSRC)scommon.h
$(GLSRC)gstiffio.h:$(GLSRC)memento.h
$(GLSRC)gstiffio.h:$(GLSRC)vmsmath.h
$(GLSRC)gstiffio.h:$(GLSRC)gscsel.h
$(GLSRC)gstiffio.h:$(GLSRC)gsbitmap.h
$(GLSRC)gstiffio.h:$(GLSRC)gsfname.h
$(GLSRC)gstiffio.h:$(GLSRC)gsstype.h
$(GLSRC)gstiffio.h:$(GLSRC)stat_.h
$(GLSRC)gstiffio.h:$(GLSRC)gxtmap.h
$(GLSRC)gstiffio.h:$(GLSRC)gsmemory.h
$(GLSRC)gstiffio.h:$(GLSRC)gpsync.h
$(GLSRC)gstiffio.h:$(GLSRC)memory_.h
$(GLSRC)gstiffio.h:$(GLSRC)gpgetenv.h
$(GLSRC)gstiffio.h:$(GLSRC)gslibctx.h
$(GLSRC)gstiffio.h:$(GLSRC)gscdefs.h
$(GLSRC)gstiffio.h:$(GLSRC)gs_dll_call.h
$(GLSRC)gstiffio.h:$(GLSRC)stdio_.h
$(GLSRC)gstiffio.h:$(GLSRC)gscompt.h
$(GLSRC)gstiffio.h:$(GLSRC)gxcindex.h
$(GLSRC)gstiffio.h:$(GLSRC)gsgstate.h
$(GLSRC)gstiffio.h:$(GLSRC)stdint_.h
$(GLSRC)gstiffio.h:$(GLSRC)gssprintf.h
$(GLSRC)gstiffio.h:$(GLSRC)gsccode.h
$(GLSRC)gstiffio.h:$(GLSRC)std.h
$(GLSRC)gstiffio.h:$(GLSRC)gstypes.h
$(GLSRC)gstiffio.h:$(GLSRC)stdpre.h
$(GLSRC)gstiffio.h:$(GLGEN)arch.h
