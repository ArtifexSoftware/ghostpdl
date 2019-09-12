/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  1305 Grant Avenue - Suite 200,
   Novato, CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/* psdcmykog device.
 *
 * This device produces 6 component psd files, Cyan, Magenta, Yellow, Key
 * (Black), "Artifex Orange" and "Artifex Green". It renders at the given
 * resolution, then subscales by a factor of 2 in each axis to achieve
 * simple anti-aliasing.
 *
 * The primary purpose of this device is to be a testable examplar for
 * various features added to Ghostscript during the development for 9.11.
 * It is (hopefully) simple enough to understand.
 *
 * Specifically, it is 1) a planar device, 2) a device with specific
 * raster alignment requirements, 3) a device that uses process page,
 * 4) a device that 'adjusts' the bandheight, and 5) a device that can
 * quickly skip processing for unused planes.
 *
 * To cover these in more detail:
 *
 *  1) The PSD file format is a planar one, that is the output requires
 *     all the C information, then all the M information, etc. Previously
 *     this has frequently been implemented within Ghostscript devices by
 *     rendering a 'chunky' image (CMYKCMYKCMYK etc) and then unpacking it
 *     afterwards. This device demonstrates the advantages of working direct
 *     in planar mode.
 *
 *  2) In order to swiftly perform the subsample, we would like to leverage
 *     SSE instructions (if they are available on the target CPU). To do this
 *     we require (or at least prefer) specific alignment of the buffers in
 *     which we work. The device now sets dev->pad and dev->log2_align_mod
 *     at startup, and any buffers created by the device will be allocated
 *     with this in mind.
 *
 *     Other devices may use SSE to perform halftoning, or repacking of data
 *     for their own devices.
 *
 *  3) Traditionally devices have called 'get_bits' or 'get_bits_rectangle'
 *     to retrieve the rendered bits for output. A new mechanism has been
 *     added to Ghostscript whereby output devices can work on whole 'bands'
 *     at once, with "in place" access to the buffered data. The primary
 *     benefit here (over and above possibly not needing another buffer) is
 *     that when multiple rendering threads are used (-dNumRenderThreads=x)
 *     any post processing (such as the subsample stage in this device, or
 *     the compression stage in the fpng device) will be performed on these
 *     background render threads too - a potentially significant speedup.
 *
 *  4) The code that performs the subsampling can be much simpler if we know
 *     in advance that the heights of the rendering bands will be a multiple
 *     of two. Similarly, devices that do JPEG (or similar macroblock based
 *     compression) might be simpler if they know that the rendering bands
 *     are a multiple of 8 (or maybe 16).
 *
 *  5) The internal band rendering code within Ghostscript can detect (within
 *     certain limits) whether a given colorant is used or not. In the case
 *     where a colorant isn't used at all, the device can short circuit its
 *     processing of that plane.
 */

// Set USE_SSE2 if we have SSE2 available to us.
// GS defines HAVE_SSE2 for windows builds.
#ifdef HAVE_SSE2
#define USE_SSE2
#endif

// Set USE_SSE3 if we have SSE3 available to us. Only has an effect if
// USE_SSE2 is also set.
//#define USE_SSE3

// For timing we might want to disable this device from actually producing
// any output.
//#define NO_OUTPUT

#include "std.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gxdevcli.h"
#include "gdevdevn.h"
#include "gdevprn.h"
#include "gsequivc.h"
#include "gdevdevnprn.h"
#include "gxgetbit.h"
#include "gxdevsop.h"
#include "gdevppla.h"
#include "gdevpsd.h"

/* And for the SSE code */
#ifdef USE_SSE4
#include <smmintrin.h>   // SIMD sse4
#endif
#ifdef USE_SSE3
#include <tmmintrin.h>   // SIMD sse3
#endif
#ifdef USE_SSE2
#include <emmintrin.h>   // SIMD sse2
#endif

/* First we have the device structure definition. */
struct gx_device_cmykog_s {
  gx_devn_prn_device_common;
};

typedef struct gx_device_cmykog_s gx_device_cmykog;

/* GC procedures
 *
 * In order to allow garbage collection to occur, we need to define a
 * structure descriptor for the device structure. This is a bit of
 * Ghostscript magic that knows how to enumerate/relocate pointers
 * within that structure.
 */

static
ENUM_PTRS_WITH(cmykog_device_enum_ptrs, gx_device_cmykog *pdev)
{
  (void)pdev; /* Silenced unused var warning */
  ENUM_PREFIX(st_gx_devn_prn_device, 0);
  /* Any extra pointers added to our device after the gx_devn_prn_device
   * fields would be enumerated here. */
  return 0;
}
ENUM_PTRS_END

static RELOC_PTRS_WITH(cmykog_device_reloc_ptrs, gx_device_cmykog *pdev)
{
  (void)pdev; /* Silenced unused var warning */
  RELOC_PREFIX(st_gx_devn_prn_device);
  /* Any extra pointers added to our device after the gx_devn_prn_device
   * fields would be relocated here. */
}
RELOC_PTRS_END

/* Even though cmykog_device_finalize is the same as gx_device_finalize, */
/* we need to implement it separately because st_composite_final */
/* declares all 3 procedures as private. */
static void
cmykog_device_finalize(const gs_memory_t *cmem, void *vpdev)
{
  gx_devn_prn_device_finalize(cmem, vpdev);
}

gs_private_st_composite_final(st_cmykog_device, gx_device_cmykog,
  "gx_device_cmykog", cmykog_device_enum_ptrs, cmykog_device_reloc_ptrs,
  cmykog_device_finalize);

/*--------------------------------------*/
/* Printer Driver Functions 		*/
/*--------------------------------------*/

/* Next, we have calls to open and close the device */

/* Open the printer */
static int
cmykog_open(gx_device * pdev)
{
  gx_device_cmykog *dev = (gx_device_cmykog *)pdev;
  unsigned char k;

  dev->color_info.separable_and_linear = GX_CINFO_SEP_LIN;
  dev->icc_struct->supports_devn = true;

  /* This device is a DeviceN color based device but unlike psdcmyk and
     tiffsep it can't change its color characteristics.  It remains as 
     a 6 color cmykog device.  Postsript files can change the maxseparations 
     (example Bug 696600 29-07E.PS)  The gdevdevn code will, if it 
     detects a change occured, close the device and reopen (see
     devn_printer_put_params) possibly resetting the color info.  tiffsep
     and psdcmyk, when reopened, will  reset their color characteristics based upon
     the content of the page, avoiding any issues upon the reopen.  Here we
     will need to make sure to set the color information to avoid problems. */
  pdev->color_info.max_components = 6;
  pdev->color_info.depth = pdev->color_info.num_components * 8;

  /* For the planar device we need to set up the bit depth of each plane.
   * For other devices this is handled in check_device_separable where
   * we compute the bit shift for the components etc. */
  for (k = 0; k < pdev->color_info.num_components; k++) {
      pdev->color_info.comp_bits[k] = 8;
  }

  /* Here we set the device so that any buffers set up will be aligned to a
   * multiple of 1<<5 = 32. This is so the SSE code can safely load 32 bytes
   * at a time from a scanline.
   *
   * See (2) at the top of this file. */
  dev->pad = 0; /* No additional padding requirements */
  dev->log2_align_mod = 5; /* But we do want lines aligned to a multiple of 32 */

  /* Finally, we open the device requesting the underlying buffers to be
   * planar, rather than chunky. See (1) at the top of this file. */
  return gdev_prn_open_planar(pdev, true);
}

/* Close the printer */
static int
cmykog_close(gx_device * pdev)
{
  return gdev_prn_close(pdev);
}

/* Next, we hook gxdso_adjust_bandheight to ensure that every band is an even
 * height. This takes care of 4) from the comments at the top of the file. */
static int
cmykog_dev_spec_op(gx_device *dev_, int op, void *data, int datasize)
{
  if (op == gxdso_adjust_bandheight) {
    /* Any band height is fine, as long as it is even */
    return datasize & ~1;
  }
  if (op == gxdso_get_dev_param) {
      int code;
      dev_param_req_t *request = (dev_param_req_t *)data;
      code = gdev_prn_get_param(dev_, request->Param, request->list);
      if (code != gs_error_undefined)
          return code;
  }
  if (op == gxdso_supports_devn) {
    return true;
  }
  return gdev_prn_dev_spec_op(dev_, op, data, datasize);
}

/*
 * This routine will check to see if the color component name  match those
 * that are available amoung the current device's color components.
 *
 * Parameters:
 *   dev - pointer to device data structure.
 *   pname - pointer to name (zero termination not required)
 *   nlength - length of the name
 *
 * This routine returns a positive value (0 to n) which is the device colorant
 * number if the name is found.  It returns GX_DEVICE_COLOR_MAX_COMPONENTS if
 * the colorant is not being used due to a SeparationOrder device parameter.
 * It returns a negative value if not found.
 */
static int
cmykog_get_color_comp_index(gx_device * dev, const char * pname,
                                int name_size, int component_type)
{
    int index;
    gx_device_cmykog *pdev = (gx_device_cmykog *)dev;

    if (strncmp(pname, "None", name_size) == 0)
        return -1;

    index = devn_get_color_comp_index(dev,
                &(pdev->devn_params), &(pdev->equiv_cmyk_colors),
                pname, name_size, component_type, NO_AUTO_SPOT_COLORS);
    return index;
}

/* Set parameters.  We don't allow setting SeparationOrder, so we */
/* throw an error if that is present.                             */
static int
cmykog_put_params(gx_device * pdev, gs_param_list * plist)
{
    int code;
    gs_param_string_array sona;         /* SeparationOrder names array */
    gs_param_name param_name;

    sona.data = 0;
    switch (code = param_read_name_array(plist, (param_name = "SeparationOrder"), &sona)) {
      default:
        param_signal_error(plist, param_name, code);
        return_error(code);
      case 1:
        sona.data = 0;          /* mark as not filled */
      case 0:
        break;			/* sona.data and sona.size were set */
    }
    if (sona.data != 0) {
        param_signal_error(plist, "SeparationOrder", gs_error_undefined);
        return_error(gs_error_undefined);
    }
    return gx_devn_prn_put_params(pdev, plist);
}

/* Next we have the code and structures required to support process_page
 * operation. See 3) in the comments at the top of the file.
 *
 * First, we have a structure full of information. A pointer to this is
 * passed to every process_page function:
 */
typedef struct cmykog_process_arg_s {
  gp_file *spot_file[GX_DEVICE_COLOR_MAX_COMPONENTS];
  char spot_name[GX_DEVICE_COLOR_MAX_COMPONENTS][gp_file_name_sizeof];
  int dev_raster;
} cmykog_process_arg_t;

/* Next we have a structure that describes each 'buffer' of data. There will
 * be one of these created for each background rendering thread. The
 * process_fn fills in the details, and the output_fn then outputs based on
 * it. */
typedef struct cmykog_process_buffer_s {
  int w;
  int h;
  gs_get_bits_params_t params;
  gx_color_usage_t color_usage;
} cmykog_process_buffer_t;

/* This function is called once per rendering thread to set up the buffer
 * that will be used in future calls. */
static int
cmykog_init_buffer(void *arg_, gx_device *dev_, gs_memory_t *memory, int w, int h, void **bufferp)
{
#ifdef INCLUDE_EXAMPLE_CODE /* disabled by default to prevent compiler warnings */
  /* arg and dev here are unused, currently, but here as an example */
  cmykog_process_arg_t *arg = (cmykog_process_arg_t *)arg_;
  gx_device_cmykog *dev = (gx_device_cmykog *)dev_;
#endif
  cmykog_process_buffer_t *buffer;

  *bufferp = NULL;
  buffer = (cmykog_process_buffer_t *)gs_alloc_bytes(memory, sizeof(*buffer), "cmykog_init_buffer");
  if (buffer == NULL)
    return_error(gs_error_VMerror);
  memset(buffer, 0, sizeof(*buffer));

  *bufferp = buffer;
  return 0;
}

/* This function is called once per rendering thread after rendering
 * completes to free the buffer allocated in the init function above. */
static void
cmykog_free_buffer(void *arg_, gx_device *dev_, gs_memory_t *memory, void *buffer_)
{
#ifdef INCLUDE_EXAMPLE_CODE /* disabled by default to prevent compiler warnings */
  /* arg and dev here are unused, currently, but here as an example */
  cmykog_process_arg_t *arg = (cmykog_process_arg_t *)arg_;
  gx_device_cmykog *dev = (gx_device_cmykog *)dev_;
#endif
  cmykog_process_buffer_t *buffer = (cmykog_process_buffer_t *)buffer_;

  if (buffer) {
    gs_free_object(memory, buffer, "cmykog_init_buffer");
  }
}

/* These functions takes a plane of width*height 8 bit values, and
 * averages groups of 4 pixels 'in-place' together to give a plane of
 * (width>>1)*(height>>1). Also, the values are 'inverted' (each byte
 * subtracted from 255) as the psd format requires.
 * Both input and output share the same raster.
 *
 * The first is a simple C version, the second a version using SSE
 * instructions for speed.
 */
#ifndef USE_SSE2
static void average_plane(byte *image, int width, int height, int raster)
{
  int x, y;
  byte *imageOut = image;

  for (y = 0; y < height; y += 2) {
    byte *in = image;
    byte *out = imageOut;
    for (x = 0; x < width; x += 2) {
      int v;

      v  = in[raster];
      v += *in++;
      v += in[raster];
      v += *in++;
      *out++ = 255 ^ (v>>2);
    }
    image += raster * 2;
    imageOut += raster;
  }
}
#else
static void average_plane(byte* image, int width, int height, int raster)
{
  int x, y;
  __m128i mm0, mm1;
  __m128i mm2, mm3;
  __m128i invert = _mm_set1_epi8 ( -1 );
#ifdef USE_SSE3
  __m128i maskL, maskR;

  maskL.m128i_u8[ 0] = 0x00;
  maskL.m128i_u8[ 1] = 0x80;
  maskL.m128i_u8[ 2] = 0x02;
  maskL.m128i_u8[ 3] = 0x80;
  maskL.m128i_u8[ 4] = 0x04;
  maskL.m128i_u8[ 5] = 0x80;
  maskL.m128i_u8[ 6] = 0x06;
  maskL.m128i_u8[ 7] = 0x80;
  maskL.m128i_u8[ 8] = 0x08;
  maskL.m128i_u8[ 9] = 0x80;
  maskL.m128i_u8[10] = 0x0A;
  maskL.m128i_u8[11] = 0x80;
  maskL.m128i_u8[12] = 0x0C;
  maskL.m128i_u8[13] = 0x80;
  maskL.m128i_u8[14] = 0x0E;
  maskL.m128i_u8[15] = 0x80;
  maskR.m128i_u8[ 0] = 0x01;
  maskR.m128i_u8[ 1] = 0x80;
  maskR.m128i_u8[ 2] = 0x03;
  maskR.m128i_u8[ 3] = 0x80;
  maskR.m128i_u8[ 4] = 0x05;
  maskR.m128i_u8[ 5] = 0x80;
  maskR.m128i_u8[ 6] = 0x07;
  maskR.m128i_u8[ 7] = 0x80;
  maskR.m128i_u8[ 8] = 0x09;
  maskR.m128i_u8[ 9] = 0x80;
  maskR.m128i_u8[10] = 0x0B;
  maskR.m128i_u8[11] = 0x80;
  maskR.m128i_u8[12] = 0x0D;
  maskR.m128i_u8[13] = 0x80;
  maskR.m128i_u8[14] = 0x0F;
  maskR.m128i_u8[15] = 0x80;
#else
  __m128i mask = _mm_set1_epi16 ( 255 );
#endif

  for (y = 0; y < height; y += 2) {
    __m128i *out = (__m128i *)(image + (y>>1) * raster);
    const __m128i *in = (__m128i *)(image + y * raster);

    for (x = 0; x < width; x += 32) {
      mm0 = _mm_load_si128(in);          //mm0 = HHhhGGggFFffEEeeDDddCCccBBbbAAaa
#ifdef USE_SSE3
      mm1 = _mm_shuffle_epi8(mm0, maskL);//mm1 = 00hh00gg00ff00ee00dd00cc00bb00aa
      mm0 = _mm_shuffle_epi8(mm0, maskH);//mm0 = 00HH00GG00FF00EE00DD00CC00BB00AA
#else
      mm1 = _mm_and_si128(mask, mm0);    //mm1 = 00hh00gg00ff00ee00dd00cc00bb00aa
      mm0 = _mm_andnot_si128(mask, mm0); //mm0 = HH00GG00FF00EE00DD00CC00BB00AA00
      mm0 = _mm_srli_epi16(mm0, 8);      //mm0 = 00HH00GG00FF00EE00DD00CC00BB00AA
#endif
      mm0 = _mm_add_epi16(mm1, mm0);
      mm3 = _mm_load_si128((__m128i*)(((char *)in) + raster));
                                         //mm3 = PPppOOooNNnnMMmmLLllKKkkJJjjIIii
      in++;
#ifdef USE_SSE3
      mm1 = _mm_shuffle_epi8(mm3, maskL);//mm1 = 00pp00oo00nn00mm00ll00kk00jj00ii
      mm3 = _mm_shuffle_epi8(mm3, maskH);//mm3 = 00PP00OO00NN00MM00LL00KK00JJ00II
#else
      mm1 = _mm_and_si128(mask, mm3);    //mm1 = 00pp00oo00nn00mm00ll00kk00jj00ii
      mm3 = _mm_andnot_si128(mask, mm3); //mm3 = PP00OO00NN00MM00LL00KK00JJ00II00
      mm3 = _mm_srli_epi16(mm3, 8);      //mm3 = 00PP00OO00NN00MM00LL00KK00JJ00II
#endif
      mm3 = _mm_add_epi16(mm1, mm3);
      mm2 = _mm_add_epi16(mm0, mm3);

      mm0 = _mm_load_si128(in);          //mm0 = HHhhGGggFFffEEeeDDddCCccBBbbAAaa
#ifdef USE_SSE3
      mm1 = _mm_shuffle_epi8(mm0, maskL);//mm1 = 00hh00gg00ff00ee00dd00cc00bb00aa
      mm0 = _mm_shuffle_epi8(mm0, maskH);//mm0 = 00HH00GG00FF00EE00DD00CC00BB00AA
#else
      mm1 = _mm_and_si128(mask, mm0);    //mm1 = 00hh00gg00ff00ee00dd00cc00bb00aa
      mm0 = _mm_andnot_si128(mask, mm0); //mm0 = HH00GG00FF00EE00DD00CC00BB00AA00
      mm0 = _mm_srli_epi16(mm0, 8);      //mm0 = 00HH00GG00FF00EE00DD00CC00BB00AA
#endif
      mm0 = _mm_add_epi16(mm1, mm0);
      mm3 = _mm_load_si128((__m128i*)(((char *)in) + raster));
                                         //mm3 = PPppOOooNNnnMMmmLLllKKkkJJjjIIii
      in++;
#ifdef USE_SSE3
      mm1 = _mm_shuffle_epi8(mm3, maskL);//mm1 = 00pp00oo00nn00mm00ll00kk00jj00ii
      mm3 = _mm_shuffle_epi8(mm3, maskH);//mm3 = 00PP00OO00NN00MM00LL00KK00JJ00II
#else
      mm1 = _mm_and_si128(mask, mm3);    //mm1 = 00pp00oo00nn00mm00ll00kk00jj00ii
      mm3 = _mm_andnot_si128(mask, mm3); //mm3 = PP00OO00NN00MM00LL00KK00JJ00II00
      mm3 = _mm_srli_epi16(mm3, 8);      //mm3 = 00PP00OO00NN00MM00LL00KK00JJ00II
#endif
      mm3 = _mm_add_epi16(mm1, mm3);
      mm3 = _mm_add_epi16(mm0, mm3);

      mm2 = _mm_srli_epi16(mm2, 2);
      mm3 = _mm_srli_epi16(mm3, 2);

      // Recombine
      mm2 = _mm_packus_epi16(mm2, mm3);

      // Invert
      mm2 = _mm_xor_si128(mm2, invert);

      _mm_store_si128(out++, mm2);
    }
  }
}
#endif

/* This is the function that does the bulk of the processing for the device.
 * This will be called back from the process_page call after each 'band'
 * has been drawn. */
static int
cmykog_process(void *arg_, gx_device *dev_, gx_device *bdev, const gs_int_rect *rect, void *buffer_)
{
  cmykog_process_arg_t *arg = (cmykog_process_arg_t *)arg_;
  gx_device_cmykog *dev = (gx_device_cmykog *)dev_;
  cmykog_process_buffer_t *buffer = (cmykog_process_buffer_t *)buffer_;
  int code, ignore_start;
  unsigned char i;
  int w = rect->q.x - rect->p.x;
  int h = rect->q.y - rect->p.y;
  gs_int_rect my_rect;

  /* We call 'get_bits_rectangle' to retrieve pointers to each plane of data
   * for the supplied rectangle.
   *
   * Note that 'rect' as supplied to this function gives the position on the
   * page, where 'my_rect' is the equivalent rectangle in the current band.
   *
   * No copying is done here; 6 pointers are filled in, one for each plane.
   */
  buffer->params.options = GB_COLORS_NATIVE | GB_ALPHA_NONE | GB_PACKING_PLANAR | GB_RETURN_POINTER | GB_ALIGN_ANY | GB_OFFSET_0 | GB_RASTER_ANY;
  my_rect.p.x = 0;
  my_rect.p.y = 0;
  my_rect.q.x = w;
  my_rect.q.y = h;
  code = dev_proc(bdev, get_bits_rectangle)(bdev, &my_rect, &buffer->params, NULL);
  if (code < 0)
    return code;

  /* Now we check to see which of those planes was actually used. See
   * section 5) of the comment at the top of the file. */
  (void)gdev_prn_color_usage((gx_device *)dev, rect->p.y, h,
                             &buffer->color_usage, &ignore_start);

  /* Now, based on the pointers retrieved above, we can process the bands
   * data. We are guaranteed that all the bands will have an even height,
   * except possibly for the last one on the page. For simplicity we just
   * drop any 'single' pixels on the end of rows or on the bottom of the
   * page before trying to perform the subsampling. */
  w &= ~1;
  h &= ~1;
  for (i = 0; i < dev->color_info.num_components; i++) {
    /* Only process the plane if it's used */
    if ((buffer->color_usage.or>>i) & 1)
      average_plane(buffer->params.data[i], w, h, arg->dev_raster);
  }

  buffer->w = w>>1;
  buffer->h = h>>1;

  return code;
}

static void
write_plane(gp_file *file, byte *data, int w, int h, int raster)
{
#ifndef NO_OUTPUT
  for (; h > 0; h--) {
      gp_fwrite(data, 1, w, file);
      data += raster;
  }
#endif
}

static const char empty[64] = {
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

static void
write_empty_plane(gp_file *file, int w, int h)
{
#ifndef NO_OUTPUT
  w *= h;
  while (w > 0) {
    h = w;
    if (h > 64)
        h = 64;
    gp_fwrite(empty, 1, h, file);
    w -= h;
  }
#endif
}

/* This function is called back from process_page for each band (in order)
 * after the process_fn has completed. All we need to do is to output
 * the contents of the buffer. */
static int
cmykog_output(void *arg_, gx_device *dev_, void *buffer_)
{
  cmykog_process_arg_t *arg = (cmykog_process_arg_t *)arg_;
  gx_device_cmykog *dev = (gx_device_cmykog *)dev_;
  cmykog_process_buffer_t *buffer = (cmykog_process_buffer_t *)buffer_;
  unsigned char i;
  int w = buffer->w;
  int h = buffer->h;
  int raster = arg->dev_raster;

  /* Output each plane in turn to file */
  for (i = 0; i < dev->color_info.num_components; i++) {
    /* The data will only have been processed if the plane is used. */
    if ((buffer->color_usage.or>>i) & 1)
      write_plane(arg->spot_file[i], buffer->params.data[i], w, h, raster);
    else
      write_empty_plane(arg->spot_file[i], w, h);
  }

  return 0;
}

/* This, finally is the main entry point that triggers printing for the
 * device. */
static int
cmykog_print_page(gx_device_printer * pdev, gp_file * prn_stream)
{
  gx_device_cmykog * pdevn = (gx_device_cmykog *) pdev;
  int ncomp = pdevn->color_info.num_components;
  cmykog_process_arg_t *arg;
  gx_process_page_options_t options;
  int code, i;
  psd_write_ctx *psd_ctx;

  if (!psd_allow_multiple_pages(pdev)) {
     emprintf(pdev->memory, "Use of the %%d format is required to output more than one page to PSD\nSee doc/Devices.htm#PSD for details\n\n");
     code = gs_note_error(gs_error_ioerror);
  }
  else {
    if ((arg = (cmykog_process_arg_t *)gs_alloc_bytes(pdev->memory,
                                          sizeof(cmykog_process_arg_t),
                                          "cmykog_print_page arg")) == NULL)
        return_error(gs_error_VMerror);

    memset(arg, 0, sizeof(cmykog_process_arg_t));
    if ((psd_ctx = (psd_write_ctx *)gs_alloc_bytes(pdev->memory,
                                          sizeof(psd_write_ctx),
                                          "cmykog_print_page psd_ctx")) == NULL) {
        gs_free_object(pdev->memory, arg, "cmykog_print_page arg");
        return_error(gs_error_VMerror);
    }

    /* Calculate the raster that will be used for each bands data;
     * gx_device_raster_plane takes care of any alignment or padding
     * required. */
    arg->dev_raster = gx_device_raster_plane((gx_device *)pdev, NULL);

#ifndef NO_OUTPUT
    /* Output the psd headers */
    code = psd_setup(psd_ctx, (gx_devn_prn_device *)pdevn,
                     prn_stream, pdev->width>>1, pdev->height>>1);
    if (code < 0)
        return code;

    code = psd_write_header(psd_ctx, (gx_devn_prn_device *)pdevn);
    if (code < 0)
        return code;

    /* We will output the 0th plane direct to the target file. We open
     * temporary files here, where the data for planes 1-5 will be put.
     * We will then copy this data into the target file at the end. */
    arg->spot_file[0] = prn_stream;
    for(i = 1; i < ncomp; i++) {
      arg->spot_file[i] = gp_open_scratch_file_rm(pdev->memory, gp_scratch_file_name_prefix, &(arg->spot_name[i][0]), "w+b");
      if (arg->spot_file[i] == NULL) {
        code = gs_error_invalidfileaccess;
        goto prn_done;
      }
    }
#endif

    /* Kick off the actual hard work */
    options.init_buffer_fn = cmykog_init_buffer;
    options.free_buffer_fn = cmykog_free_buffer;
    options.process_fn = cmykog_process;
    options.output_fn = cmykog_output;
    options.arg = arg;
    options.options = 0;
    code = dev_proc(pdev, process_page)((gx_device *)pdev, &options);

#ifndef NO_OUTPUT
    /* Now collate the temporary files. */
    for (i = 1; i < ncomp; i++) {
      char tmp[4096];
      int n;
      gp_fseek(arg->spot_file[i], 0, SEEK_SET);
      while (!gp_feof(arg->spot_file[i])) {
        n = gp_fread(tmp, 1, 4096, arg->spot_file[i]);
        gp_fwrite(tmp, 1, n, prn_stream);
      }
    }
    /* If Ghostscript knows that all 6 planes aren't used, it may
     * truncate ncomp. Write the extra planes as appropriate. */
    for (; i < pdev->color_info.max_components; i++) {
      write_empty_plane(prn_stream, pdev->width>>1, pdev->height>>1);
    }
#endif

prn_done:

#ifndef NO_OUTPUT
    /* Close the temporary files. */
    for(i = 1; i < ncomp; i++) {
      if (arg->spot_file[i] != NULL)
        gp_fclose(arg->spot_file[i]);
      if(arg->spot_name[i][0])
        unlink(arg->spot_name[i]);
    }
#endif
    gs_free_object(pdev->memory, psd_ctx, "cmykog_print_page psd_ctx");
    gs_free_object(pdev->memory, arg, "cmykog_print_page arg");
  }
  return code;
}

/* Finally, the device definition itself */

#define device_procs(get_color_mapping_procs)\
{       cmykog_open,			/* open device */\
        gx_default_get_initial_matrix,	/* initialize matrix */\
        NULL,                           /* sync_output */\
        gdev_prn_bg_output_page,        /* output_page */\
        cmykog_close,                   /* close */\
        NULL,                           /* map_rgb_color - not used */\
        NULL,                           /* map_color_rgb - not used */\
        NULL,                           /* fill_rectangle */\
        NULL,                           /* tile_rectangle */\
        NULL,                           /* copy_mono */\
        NULL,                           /* copy_color */\
        NULL,                           /* draw_line */\
        NULL,                           /* get_bits */\
        gx_devn_prn_get_params,         /* get_params */\
        cmykog_put_params,              /* put_params */\
        NULL,                           /* map_cmyk_color - not used */\
        NULL,                           /* get_xfont_procs */\
        NULL,                           /* get_xfont_device */\
        NULL,                           /* map_rgb_alpha_color */\
        gx_page_device_get_page_device, /* get_page_device */\
        NULL,                           /* get_alpha_bits */\
        NULL,                           /* copy_alpha */\
        NULL,                           /* get_band */\
        NULL,                           /* copy_rop */\
        NULL,                           /* fill_path */\
        NULL,                           /* stroke_path */\
        NULL,                           /* fill_mask */\
        NULL,                           /* fill_trapezoid */\
        NULL,                           /* fill_parallelogram */\
        NULL,                           /* fill_triangle */\
        NULL,                           /* draw_thin_line */\
        NULL,                           /* begin_image */\
        NULL,                           /* image_data */\
        NULL,                           /* end_image */\
        NULL,                           /* strip_tile_rectangle */\
        NULL,                           /* strip_copy_rop */\
        NULL,                           /* get_clipping_box */\
        NULL,                           /* begin_typed_image */\
        NULL,                           /* get_bits_rectangle */\
        NULL,                           /* map_color_rgb_alpha */\
        NULL,                           /* create_compositor */\
        NULL,                           /* get_hardware_params */\
        NULL,                           /* text_begin */\
        NULL,                           /* finish_copydevice */\
        NULL,                           /* begin_transparency_group */\
        NULL,                           /* end_transparency_group */\
        NULL,                           /* begin_transparency_mask */\
        NULL,                           /* end_transparency_mask */\
        NULL,                           /* discard_transparency_layer */\
        gx_devn_prn_get_color_mapping_procs,/* get_color_mapping_procs */\
        cmykog_get_color_comp_index,    /* get_color_comp_index */\
        gx_devn_prn_encode_color,       /* encode_color */\
        gx_devn_prn_decode_color,       /* decode_color */\
        NULL,                           /* pattern_manage */\
        NULL,                           /* fill_rectangle_hl_color */\
        NULL,				/* include_color_space */\
        NULL,				/* fill_linear_color_scanline */\
        NULL,				/* fill_linear_color_trapezoid */\
        NULL,				/* fill_linear_color_triangle */\
        NULL,                           /* update_spot_equivalent_colors */\
        gx_devn_prn_ret_devn_params,    /* ret_devn_params */\
        NULL,                           /* fillpage */\
        NULL,                           /* push_transparency_state */\
        NULL,                           /* pop_transparency_state */\
        NULL,                           /* put_image */\
        cmykog_dev_spec_op                 /* dev_spec_op */\
}

fixed_colorant_name DevCMYKOGComponents[] = {
  "Cyan",
  "Magenta",
  "Yellow",
  "Black",
  "Artifex Orange",
  "Artifex Green",
  0               /* List terminator */
};

#define CMYKOG_DEVICE(procs, dname, ncomp, pol, depth, mg, mc, cn, xdpi, ydpi)\
    std_device_full_body_type_extended(gx_device_cmykog, &procs, dname,\
          &st_cmykog_device,\
          (int)((long)(DEFAULT_WIDTH_10THS) * (xdpi) / 10),\
          (int)((long)(DEFAULT_HEIGHT_10THS) * (ydpi) / 10),\
          xdpi, ydpi,\
          ncomp,                /* MaxComponents */\
          ncomp,                /* NumComp */\
          pol,                  /* Polarity */\
          depth, 0,             /* Depth, GrayIndex */\
          mg, mc,               /* MaxGray, MaxColor */\
          mg + 1, mc + 1,       /* DitherGray, DitherColor */\
          GX_CINFO_SEP_LIN,     /* Linear & Separable */\
          cn,                   /* Process color model name */\
          0, 0,                 /* offsets */\
          0, 0, 0, 0            /* margins */\
        ),\
        prn_device_body_rest_(cmykog_print_page)

/*
 * PSDCMYKOG 8bits
 */
static const gx_device_procs cmykog_procs = device_procs(get_cmykog_spot_color_mapping_procs);

const gx_device_cmykog gs_psdcmykog_device =
{
  CMYKOG_DEVICE(cmykog_procs, "psdcmykog", 6, GX_CINFO_POLARITY_SUBTRACTIVE, 48, 255, 255, "DeviceCMYK", 600, 600),
  /* device specific parameters */
  { 8,                        /* Bits per color - must match ncomp, depth, etc. above */
    DevCMYKOGComponents,      /* Names of color model colorants */
    4,                        /* Number colorants for CMYK */
    6,                        /* MaxSeparations */
    -1,                       /* PageSpotColors has not been specified */
    {0},                      /* SeparationNames */
    0,                        /* SeparationOrder names */
    {0, 1, 2, 3, 4, 5, 6, 7 } /* Initial component SeparationOrder */
  }
};
