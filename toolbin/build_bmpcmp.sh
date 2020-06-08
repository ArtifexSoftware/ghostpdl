# set to location of ghostpdl
export baseDirectory=~/Artifex

# First configure tiff not to use jpeg, jbig, lzma
cd $baseDirectory/ghostpdl/tiff
./configure --disable-jpeg --disable-old-jpeg --disable-jbig --disable-lzma
mv $baseDirectory/ghostpdl/tiff/libtiff/tif_config.h $baseDirectory/ghostpdl/toolbin

# Also set up configure for png
# Just use pre-built. Had issues getting this setup
cp $baseDirectory/ghostpdl/libpng/scripts/pnglibconf.h.prebuilt $baseDirectory/ghostpdl/toolbin/pnglibconf.h

cc -I$baseDirectory/ghostpdl/toolbin \
-I$baseDirectory/ghostpdl/libpng \
-I$baseDirectory/ghostpdl/tiff/libtiff \
-I$baseDirectory/ghostpdl/zlib \
-I$baseDirectory/ghostpdl/lcms2mt/include \
-o bmpcmp -DHAVE_LIBPNG -DHAVE_LIBTIFF -DCOLOR_MANAGED \
-UOJEPG_SUPPORT -UJPEG_SUPPORT \
 $baseDirectory/ghostpdl/toolbin/bmpcmp.c \
 $baseDirectory/ghostpdl/libpng/png.c \
 $baseDirectory/ghostpdl/libpng/pngerror.c \
 $baseDirectory/ghostpdl/libpng/pngget.c \
 $baseDirectory/ghostpdl/libpng/pngmem.c \
 $baseDirectory/ghostpdl/libpng/pngpread.c \
 $baseDirectory/ghostpdl/libpng/pngread.c \
 $baseDirectory/ghostpdl/libpng/pngrio.c \
 $baseDirectory/ghostpdl/libpng/pngrtran.c \
 $baseDirectory/ghostpdl/libpng/pngrutil.c \
 $baseDirectory/ghostpdl/libpng/pngset.c \
 $baseDirectory/ghostpdl/libpng/pngtrans.c \
 $baseDirectory/ghostpdl/libpng/pngwio.c \
 $baseDirectory/ghostpdl/libpng/pngwrite.c \
 $baseDirectory/ghostpdl/libpng/pngwtran.c \
 $baseDirectory/ghostpdl/libpng/pngwutil.c \
 $baseDirectory/ghostpdl/zlib/adler32.c \
 $baseDirectory/ghostpdl/zlib/crc32.c \
 $baseDirectory/ghostpdl/zlib/infback.c \
 $baseDirectory/ghostpdl/zlib/inflate.c \
 $baseDirectory/ghostpdl/zlib/uncompr.c \
 $baseDirectory/ghostpdl/zlib/compress.c \
 $baseDirectory/ghostpdl/zlib/deflate.c \
 $baseDirectory/ghostpdl/zlib/inffast.c \
 $baseDirectory/ghostpdl/zlib/inftrees.c \
 $baseDirectory/ghostpdl/zlib/trees.c \
 $baseDirectory/ghostpdl/zlib/zutil.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_aux.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_close.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_codec.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_color.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_compress.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_dir.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_dirinfo.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_dirread.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_dirwrite.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_dumpmode.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_error.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_extension.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_fax3.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_fax3sm.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_flush.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_getimage.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_jbig.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_jpeg.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_jpeg_12.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_luv.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_lzma.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_lzw.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_next.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_ojpeg.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_open.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_packbits.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_pixarlog.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_predict.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_print.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_read.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_strip.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_swab.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_thunder.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_tile.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_unix.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_version.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_warning.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_webp.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_write.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_zip.c \
 $baseDirectory/ghostpdl/tiff/libtiff/tif_zstd.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsalpha.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmscam02.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmscgats.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmscnvrt.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmserr.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsgamma.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsgmt.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmshalf.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsintrp.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsio0.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsio1.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmslut.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsmd5.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsmtrx.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsnamed.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsopt.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmspack.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmspcs.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsplugin.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsps2.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmssamp.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmssm.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmstypes.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsvirt.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmswtpnt.c \
 $baseDirectory/ghostpdl/lcms2mt/src/cmsxform.c \
 -lm 2>&1 -o $baseDirectory/ghostpdl/toolbin/bmpcmp

# clean up tiff directory so gs still builds
