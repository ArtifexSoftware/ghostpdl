 /* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plchar.c */
/* PCL font handling library -- operations on individual characters */
#include "math_.h"
#include "memory_.h"
#include "stdio_.h"             /* for gdebug.h */
#include "gdebug.h"
#include "gserror.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsimage.h"
#include "gspaint.h"
#include "gspath.h"
#include "gsbittab.h"
#include "gxarith.h"            /* for igcd */
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"
#include "gscoord.h"
#include "gsstate.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxpath.h"
/* We really shouldn't need the following, but currently they are needed */
/* for pgs->path and penum->log2_current_scale in pl_tt_build_char. */
#include "gxfixed.h"
#include "gxchar.h"
#include "gxfcache.h"
#include "gxttf.h"
#include "gzstate.h"

/* Define whether to cache TrueType characters. */
/* This would only be disabled for debugging. */
#define CACHE_TRUETYPE_CHARS

/* Structure descriptors */
gs_private_st_ptrs1(st_pl_font_glyph, pl_font_glyph_t, "pl_font_glyph_t",
  pl_font_glyph_enum_ptrs, pl_font_glyph_reloc_ptrs, data);
gs_private_st_element(st_pl_font_glyph_element, pl_font_glyph_t,
  "pl_font_glyph_t[]",
  pl_font_glyph_elt_enum_ptrs, pl_font_glyph_elt_reloc_ptrs, st_pl_font_glyph);

int
pl_prepend_xl_dummy_header(gs_memory_t *mem, byte **ppheader)
{
    return 0;
}

int
pl_swap_header(byte *header, uint gifct)
{
    return 0;
}

/* ---------------- Utilities ---------------- */

/* Look up a glyph in a font.  Return a pointer to the glyph's slot */
/* (data != 0) or where it should be added (data == 0). */
pl_font_glyph_t *
pl_font_lookup_glyph(const pl_font_t *plfont, gs_glyph glyph)
{       uint size = plfont->glyphs.size;
        uint skip = plfont->glyphs.skip;
        uint index = glyph % size;
        pl_font_glyph_t *pfg;
        pl_font_glyph_t *result = 0;

        while ( (pfg = plfont->glyphs.table + index)->data ?
                pfg->glyph != glyph : pfg->glyph != 0
              )
          { if ( !pfg->data )
              result = pfg;
            index = (index >= skip ? index : index + size) - skip;
          }
        return (!pfg->data && result) ? result : pfg;
}

/* ---------------- Bitmap font support ---------------- */

/* Encode a character for a bitmap font.  This is simple, because */
/* bitmap fonts are always bound. */
static gs_glyph
pl_bitmap_encode_char(gs_font *pfont, gs_char chr, gs_glyph not_used)
{       return (gs_glyph)chr;
}

/* Get character existence and escapement for a bitmap font. */
/* This is simple for the same reason. */
static int
pl_bitmap_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{       
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

    pwidth->x = pwidth->y = 0;
    if ( !pwidth ) {
#ifdef DEBUG    
        dprintf("Warning should not call width function without width\n" );
#endif
        return (cdata == 0 ? 1 : 0);
    }
    if ( cdata == 0 ) { 
        return 1;
    }
    if ( cdata[0] == 0 ) { /* PCL XL characters don't have an escapement. */
            pwidth->x = pwidth->y = 0;
            return 0;
    }

    { 
        const byte *params = cdata + 6;
        pwidth->x = (plfont->header[13] ? /* variable pitch */
                     pl_get_int16(params + 8) * 0.25 :
                     pl_get_int16(params) /*lsb*/ + pl_get_int16(params + 4) /*width*/);
    }
    return 0;
}

static int
pl_bitmap_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    gs_point width;
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;
    /* never a vertical substitute */
    metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;
    /* no data - character not found */
    if ( cdata == 0 )
        return 1;
    /* We are not concerned about PCL XL characters */
    if ( cdata[0] == 0 )
        return 0;
    
    metrics[0] = pl_get_int16(cdata + 6);
    pl_bitmap_char_width(plfont, pgs, char_code, &width);
    metrics[2] = width.x;
    return 0;
}

/*
 * For pseudo-bolding, we have to "smear" a bitmap horizontally and
 * vertically by ORing together a rectangle of bits below and to the left of
 * each output bit.  We do this separately for horizontal and vertical
 * smearing.  Eventually, we will replace the current procedure, which takes
 * time proportional to W * H * (N + log2(N)), with one that is only
 * proportional to N (but takes W * N additional buffer space).
 */

/* Allocate the line buffer for bolding.  We need 2 + bold scan lines. */
static byte *
alloc_bold_lines(gs_memory_t *mem, uint width, int bold, client_name_t cname)
{       return gs_alloc_byte_array(mem, 2 + bold, bitmap_raster(width + bold),
                                   cname);
}

/* Merge one (aligned) scan line into another, for vertical smearing. */
static void
bits_merge(byte *dest, const byte *src, uint nbytes)
{       long *dp = (long *)dest;
        const long *sp = (const long *)src;
        uint n = (nbytes + sizeof(long) - 1) >> arch_log2_sizeof_long;

        for ( ; n >= 4; sp += 4, dp += 4, n -= 4 )
          dp[0] |= sp[0], dp[1] |= sp[1], dp[2] |= sp[2], dp[3] |= sp[3];
        for ( ; n; ++sp, ++dp, --n )
          *dp |= *sp;
}

/* Smear a scan line horizontally.  Note that the output is wider than */
/* the input by the amount of bolding (smear_width). */
static void
bits_smear_horizontally(byte *dest, const byte *src, uint width,
  uint smear_width)
{       uint bits_on = 0;
        const byte *sp = src;
        uint sbyte = *sp;
        byte *dp = dest;
        uint dbyte = sbyte;
        uint sdmask = 0x80;
        const byte *zp = src;
        uint zmask = 0x80;
        uint i = 0;

        /* Process the first smear_width bits. */
        { uint stop = min(smear_width, width);

          for ( ; i < stop; ++i ) {
            if ( sbyte & sdmask )
              bits_on++;
            else if ( bits_on )
              dbyte |= sdmask;
            if ( (sdmask >>= 1) == 0 )
              sdmask = 0x80, *dp++ = dbyte, dbyte = sbyte = *++sp;
          }
        }
                
        /* Process all but the last smear_width bits. */
        { for ( ; i < width; ++i ) {
            if ( sbyte & sdmask )
              bits_on++;
            else if ( bits_on )
              dbyte |= sdmask;
            if ( *zp & zmask )
              --bits_on;
            if ( (sdmask >>= 1) == 0 ) {
              sdmask = 0x80;
              *dp++ = dbyte;
on:           switch ( (dbyte = sbyte = *++sp) ) {
                case 0xff:
                  if ( width - i <= 8 )
                    break;
                  *dp++ = 0xff;
                  bits_on += 8 -
                    byte_count_bits[(*zp & (zmask - 1)) + (zp[1] & -zmask)];
                  ++zp;
                  i += 8;
                  goto on;
                case 0:
                  if ( bits_on || width - i <= 8 )
                    break;
                  *dp++ = 0;
                  /* We know there can't be any bits to be zeroed, */
                  /* because bits_on can't go negative. */
                  ++zp;
                  i += 8;
                  goto on;
                default:
                  ;
              }
            }
            if ( (zmask >>= 1) == 0 )
              zmask = 0x80, ++zp;
          }
        }

        /* Process the last smear_width bits. */
        /****** WRONG IF width < smear_width ******/
        { uint stop = width + smear_width;

          for ( ; i < stop; ++i ) {
            if ( bits_on )
              dbyte |= sdmask;
            if ( (sdmask >>= 1) == 0 )
              sdmask = 0x80, *dp++ = dbyte, dbyte = 0;
            if ( *zp & zmask )
              --bits_on;
            if ( (zmask >>= 1) == 0 )
              zmask = 0x80, ++zp;
          }
        }

        if ( sdmask != 0x80 )
          *dp = dbyte;
}

/* Image a bitmap character, with or without bolding. */
int
image_bitmap_char(gs_image_enum *ienum, const gs_image_t *pim,
  const byte *bitmap_data, uint sraster, int bold, byte *bold_lines,
  gs_state *pgs)
{       uint dest_bytes = (pim->Width + 7) >> 3;
        gx_device *dev = pgs->device;
        void *iinfo;
        const byte *planes[1];
        int code;

        gx_set_dev_color(pgs);
        code = (*dev_proc(dev, begin_image))
          (dev, (const gs_imager_state *)pgs, pim, gs_image_format_chunky,
           NULL, gs_currentdevicecolor_inline(pgs), pgs->clip_path,
           pgs->memory, (gx_image_enum_common_t **)&iinfo);
        if ( code < 0 )
          return code;
        if ( bold )
          { /* Pass individual smeared lines. */
            uint src_width = pim->Width - bold;
            uint src_height = pim->Height - bold;
            uint dest_raster = bitmap_raster(pim->Width);
            int n1 = bold + 1;
#define merged_line(i) (bold_lines + ((i) % n1 + 1) * dest_raster)
            int y;

            planes[0] = bold_lines;
            for ( y = 0; y < pim->Height; ++y ) {
              int y0 = (y < bold ? 0 : y - bold);
              int y1 = min(y + 1, src_height);

              if ( y < src_height ) {
                bits_smear_horizontally(merged_line(y),
                                        bitmap_data + y * sraster,
                                        src_width, bold);
                { /* Now re-establish the invariant -- see below. */
                  int kmask = 1;

                  for ( ; (y & kmask) == kmask && y - kmask >= y0;
                        kmask = (kmask << 1) + 1
                      )
                    bits_merge(merged_line(y - kmask),
                               merged_line(y - (kmask >> 1)),
                               dest_bytes);
                }
              }

              /*
               * As of this point in the loop, we maintain the following
               * invariant to cache partial merges of groups of lines: for
               * each Y, y0 <= Y < y1, let K be the maximum k such that Y
               * mod 2^k = 0 and Y + 2^k < y1; then merged_line(Y) holds
               * the union of horizontally smeared source lines Y through
               * Y + 2^k - 1.  The idea behind this is similar to the idea
               * of quicksort.
               */

              { /* Now construct the output line. */
                bool first = true;
                int iy;

                for ( iy = y1 - 1; iy >= y0; --iy ) {
                  int kmask = 1;

                  while ( (iy & kmask) == kmask && iy - kmask >= y0 )
                    iy -= kmask, kmask <<= 1;
                  if ( first ) {
                    memcpy(bold_lines, merged_line(iy), dest_bytes);
                    first = false;
                  }
                  else
                    bits_merge(bold_lines, merged_line(iy), dest_bytes);
                }
              }


              code = (*dev_proc(dev, image_data))
                (dev, iinfo, planes, 0, dest_bytes, 1);
              if ( code != 0 )
                break;
            }
#undef merged_line
          }
        else
          { /* Pass the entire image at once. */
            planes[0] = bitmap_data;
            code = (*dev_proc(dev, image_data))
              (dev, iinfo, planes, 0, dest_bytes, pim->Height);
          }
        (*dev_proc(dev, end_image))(dev, iinfo, code >= 0);
        return code;
}


/* Render a character for a bitmap font. */
/* This handles both format 0 (PCL XL) and format 4 (PCL5 bitmap). */
static int
pl_bitmap_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph glyph)
{       pl_font_t *plfont = (pl_font_t *)pfont->client_data;
        const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;
        bool orient = plfont->orient;

        if ( cdata == 0 )
          return 0;
        { const byte *params;
          const byte *bitmap_data;
          int lsb, ascent;
          float delta_x;
          gs_image_t image;
          gs_image_enum *ienum;
          int code;
          uint bold;
          byte *bold_lines = 0;

          if ( cdata[0] == 0 )
            { /* PCL XL format */
              params = cdata + 2;
              bitmap_data = cdata + 10;
              delta_x = 0;      /* irrelevant */
              lsb = pl_get_int16(params);
              ascent = pl_get_int16(params + 2);
            }
          else
            { /* PCL5 format */
              params = cdata + 6;
              bitmap_data = cdata + 16;
              delta_x = (plfont->header[13] ? /* variable pitch */
                         pl_get_int16(params + 8) * 0.25 :
                         (short)(pl_get_int16(params) /*lsb*/ + pl_get_int16(params + 4)) /*width*/);
              lsb = pl_get_int16(params);
              ascent = pl_get_int16(params + 2);
            }
          ienum = gs_image_enum_alloc(pgs->memory, "pl_bitmap_build_char");
          if ( ienum == 0 )
            return_error(gs_error_VMerror);
          gs_image_t_init_mask(&image, true);
          image.Width = pl_get_uint16(params + 4);
          image.Height = pl_get_uint16(params + 6);
          /* Determine the amount of pseudo-bolding. */
          if ( plfont->bold_fraction != 0 ) { 
              bold = (uint)(2 * image.Height * plfont->bold_fraction + 0.5);
              bold_lines = alloc_bold_lines(pgs->memory, image.Width, bold,
                                            "pl_bitmap_build_char(bold_line)");
              if ( bold_lines == 0 )
                { code = gs_note_error(gs_error_VMerror);
                  goto out;
                }
              image.Width += bold;
              image.Height += bold;
              ascent += bold;
            }
          else
            bold = 0;

          gs_make_identity(&image.ImageMatrix);
          gs_matrix_rotate(&image.ImageMatrix, orient * -90, &image.ImageMatrix);
          image.ImageMatrix.tx -= lsb;
          image.ImageMatrix.ty += ascent;
          image.adjust = true;
          if (bold || orient != 0)
              code = gs_setcharwidth(penum, pgs, delta_x, 0);
          else {
              /* we use cache device for portrait bitmap fonts to
                 avoid image setup overhead.  */
              float m[6];
              m[0] = delta_x; m[1] = 0;
              m[2] = lsb; m[3] = image.Height - ascent;
              m[4] = image.Width + m[2];
              m[5] = -ascent;
              code = gs_setcachedevice(penum, pgs, m);
          }
          if ( code < 0 )
            return code;
#ifdef DEBUG          
          if ( gs_debug_c('B') ) {
              int i;
              int pixels = round_up(image.Width,8) * image.Height;
              dprintf7("bitmap font data chr=%ld, width=%d, height=%d, lsb=%d, ascent=%d, top offset=%d left offset=%d\n",
                       chr, image.Width, image.Height, lsb, ascent, pl_get_int16(params + 2), pl_get_int16(params));
              for ( i = 0; i < pixels; i++ ) {
                  if ( i % round_up(image.Width, 8) == 0 )
                      dprintf("\n");
                  dprintf1("%d", bitmap_data[i >> 3] & (128 >> (i & 7)) ? 1 : 0);
              }
              dprintf("\n");
          }
#endif
          code = image_bitmap_char(ienum, &image, bitmap_data,
                                   (image.Width - bold + 7) >> 3, bold,
                                   bold_lines, pgs);
out:      gs_free_object(pgs->memory, bold_lines,
                         "pl_bitmap_build_char(bold_lines)");
          gs_free_object(pgs->memory, ienum, "pl_bitmap_build_char");
          return (code < 0 ? code : 0);
        }
}

/* ---------------- TrueType font support ---------------- */

/* Look up a character in the TrueType character-to-TT-glyph map. */
/* Return a pointer to the glyph's slot (chr != gs_no_char) or where */
/* it should be added (chr == gs_no_char). */
pl_tt_char_glyph_t *
pl_tt_lookup_char(const pl_font_t *plfont, gs_glyph glyph)
{       uint size = plfont->char_glyphs.size;
        uint skip = plfont->char_glyphs.skip;
        uint index = glyph % size;
        pl_tt_char_glyph_t *ptcg;
        pl_tt_char_glyph_t *result = 0;

        while ( (ptcg = plfont->char_glyphs.table + index)->chr != gs_no_char ?
                ptcg->chr != glyph : ptcg->glyph
              )
          { if ( ptcg->chr == gs_no_char )
              result = ptcg;
            index = (index >= skip ? index : index + size) - skip;
          }
        return (result ? result : ptcg);
}

/* Get a string from a TrueType font. */
static int
pl_tt_string_proc(gs_font_type42 *pfont, ulong offset, uint length, const byte **pdata)
{       pl_font_t *plfont = pfont->client_data;

        *pdata = plfont->header + plfont->offsets.GT +
          (plfont->large_sizes ? 6 : 4) + offset;
        return 0;
}

/* Return the vertical substitute for a glyph, if it has one; */
/* otherwise return gs_no_glyph. */
static gs_glyph
pl_font_vertical_glyph(gs_glyph glyph, const pl_font_t *plfont)
{       long VT = plfont->offsets.VT;
        const byte *vtseg;
        uint i, len;

        if ( VT < 0 )
          return gs_no_glyph;
        vtseg = plfont->header + VT;
        if ( plfont->large_sizes )
          len = pl_get_uint32(vtseg + 2),
          i = 6;
        else
          len = pl_get_uint16(vtseg + 2),
          i = 4;
        len += i;
        for ( ; i < len; i += 4 )
          if ( glyph == pl_get_uint16(vtseg + i) )
            return pl_get_uint16(vtseg + i + 2);
        return gs_no_glyph;
}

/* get metrics with support for XL tt class 1 and 2 
 * pl overrides gstype42_default_get_metrics   
 */

int
pl_tt_get_metrics(gs_font_type42 * pfont, uint glyph_index,
                  gs_type42_metrics_options_t options,  float *sbw)
{    
    pl_font_t *plfont = pfont->client_data;
    const pl_font_glyph_t *pfg = 0;
    const byte *cdata = 0;
    int wmode = gs_type42_metrics_options_wmode(options);

    if ( plfont->glyphs.table != 0 ) {
        /* at least one caller calls before the glyph.table is valid, no chars yet 
         * test routes caller to gs_type42_default_get_metrics
         */
        pfg = pl_font_lookup_glyph(plfont, glyph_index);
        cdata = pfg->data;   

        if (cdata && (cdata[1] == 1 || cdata[1] == 2)) {
            double factor = 1.0 / pfont->data.unitsPerEm;
            uint width;
            int lsb;

            lsb = pl_get_int16(cdata + 4);
            width = pl_get_int16(cdata + 6);

            /* foo NB what about the top side bearing in class 2 ? */

            if (wmode) {
                /* NB BUG all other fonts work without this sign
                   change it should already be accounted for in the
                   character ctm */
                factor = -factor;       /* lsb and width go down the page */
                sbw[0] = 0, sbw[1] = lsb * factor;
                sbw[2] = 0, sbw[3] = width * factor;
            } else {
                sbw[0] = lsb * factor, sbw[1] = 0;
                sbw[2] = width * factor, sbw[3] = 0;
            }   
            return 0; /* tt class 1,2 */
        }
    }
    /* else call default implementation for tt class 0, incomplete font */
    /* first check for a vertical substitute if writing mode is
      vertical.  We unpleasantly replace the glyph_index parameter
      passed to procedure to be consist with the pl_tt_build_char()
      debacle */
    {
        pl_font_t *plfont = pfont->client_data;
        if (plfont->allow_vertical_substitutes) {
            gs_glyph vertical = pl_font_vertical_glyph(glyph_index, plfont);
            if ( vertical != gs_no_glyph )
                glyph_index = vertical;
        }
    }

    /* the graphics library does not do this correctly.  If there
       aren't two sets of metrics WMode should be ignored.  We work
       around that here. */

    if (wmode == 1) {
        const gs_type42_mtx_t *pmtx = &pfont->data.metrics[wmode];
        if (pmtx->length == 0) {
            wmode = 0;
        }
        else {
            if ( gs_debug_c('=') ) {
                dprintf("Found vertical metrics\n");
            }
        }
    }
    return gs_type42_default_get_metrics(pfont, glyph_index, wmode, sbw);
}


/* Get the outline data for a glyph in a downloaded TrueType font. */
int
pl_tt_get_outline(gs_font_type42 *pfont, uint index, gs_glyph_data_t *pdata)
{       
    pl_font_t *plfont = pfont->client_data;
    const pl_font_glyph_t *pfg = pl_font_lookup_glyph(plfont, index);
    const byte *cdata = pfg->data;

    if ( cdata == 0 ) { 
        /* undefined glyph */
        gs_glyph_data_from_null(pdata);
    }
    else { 
        uint desc_size  = (*cdata == 15 ? cdata[2] /* PCL5 */ : 0 /* PCL XL */);
        uint data_size = pl_get_uint16(cdata + 2 + desc_size);

        if ( data_size <= 4 ) { 
            /* empty outline */
            gs_glyph_data_from_null(pdata);
        } else if ( cdata[1] == 0) { 
            gs_glyph_data_from_bytes(pdata, 
                                     cdata,
                                     6 + desc_size, 
                                     data_size - 4, 
                                     NULL);
        } else if ( cdata[1] == 1) {        
            gs_glyph_data_from_bytes(pdata, 
                                     cdata,
                                     10, 
                                     data_size - 8, 
                                     NULL);
        } else if ( cdata[1] == 2) {  
            gs_glyph_data_from_bytes(pdata, 
                                     cdata,
                                     12, 
                                     data_size - 10, 
                                     NULL);
        }
    }
    return 0;
}

#define access(base, length, vptr)\
  (*pfont->data.string_proc)(pfont, (ulong)(base), length, &vptr)

/* Find a table in a TrueType font. */
/* Return the data offset of the table; store the length in *plen. */
/* If the table is missing, return 0. */
ulong
tt_find_table(gs_font_type42 *pfont, const char *tname, uint *plen)
{       const byte *OffsetTable;
        uint numTables;
        const byte *TableDirectory;
        uint i;
        ulong table_dir_offset = 0;

        access(0, 12, OffsetTable);
        access(table_dir_offset, 12, OffsetTable);
        numTables = pl_get_uint16(OffsetTable + 4);
        access(table_dir_offset + 12, numTables * 16, TableDirectory);
        for ( i = 0; i < numTables; ++i )
          { const byte *tab = TableDirectory + i * 16;
            if ( !memcmp(tab, tname, 4) )
              { if ( plen )
                  *plen = pl_get_uint32(tab + 12);
                return pl_get_uint32(tab + 8);
              }
          }
        return 0;
}

#ifndef gs_imager_state_DEFINED
#  define gs_imager_state_DEFINED
typedef struct gs_imager_state_s gs_imager_state;
#endif

/* Opaque type for a path */
#ifndef gx_path_DEFINED
#  define gx_path_DEFINED
typedef struct gx_path_s gx_path;
#endif

/*
 * Map a key through a cmap sub-table.  We export this so we can use
 * it someday for mapping between glyph vocabularies.  If the key is
 * not mappable, return gs_error_undefined; if the sub-table type is
 * unknown, return gs_error_invalidfont.
 */
int
pl_cmap_lookup(const uint key, const byte *table, uint *pvalue)
{       /* Dispatch according to the table type. */
        switch ( pl_get_uint16(table) )
          {
          case 0:
            {   /* Apple standard 1-to-1 mapping. */
                *pvalue = table[key + 6];
                if_debug2('J', "[J]%u => %u\n", key, *pvalue);
                break;
            }
          case 4:
            {   /* Microsoft/Adobe segmented mapping.  What a mess! */
                uint segCount2 = pl_get_uint16(table + 6);
                const byte *endCount = table + 14;
                const byte *startCount = endCount + segCount2 + 2;
                const byte *idDelta = startCount + segCount2;
                const byte *idRangeOffset = idDelta + segCount2;
                /*const byte *glyphIdArray = idRangeOffset + segCount2;*/
                uint i2;

                for ( i2 = 0; i2 < segCount2 - 3; i2 += 2 )
                  { int delta, roff;
                    uint start = pl_get_uint16(startCount + i2);
                    uint glyph;

                    if_debug4('J', "[J]start=%u end=%u delta=%d roff=%d\n",
                              start, pl_get_uint16(endCount + i2), pl_get_int16(idDelta + i2),
                              pl_get_int16(idRangeOffset + i2));
                    if ( key < start )
                      { if_debug1('J', "[J]%u out of range\n", key);
                        return_error(gs_error_undefined);
                      }
                    if ( key > pl_get_uint16(endCount + i2) )
                      continue;
                    delta = pl_get_int16(idDelta + i2);
                    roff = pl_get_int16(idRangeOffset + i2);
                    if ( roff == 0 )
                      { *pvalue = ( key + delta ) & 0xffff; /* mod 65536 */
                        if_debug2('J', "[J]%u => %u\n", key, *pvalue);
                        return 0;
                      }
                    glyph = pl_get_uint16(idRangeOffset + i2 + roff +
                                ((key - start) << 1));
                    *pvalue = (glyph == 0 ? 0 : glyph + delta);
                    if_debug2('J', "[J]%u => %u\n", key, *pvalue);
                    return 0;
                  }
                /*
                 * The TrueType documentation says that the last range is
                 * always supposed to end with 0xffff, so this shouldn't
                 * happen; however, in some real fonts, it does.
                 */
                if_debug1('J', "[J]%u out of range\n", key);
                return_error(gs_error_undefined);
            }
          case 6:
            {   /* Single interval lookup. */
                uint firstCode = pl_get_uint16(table + 6);
                uint entryCount = pl_get_uint16(table + 8);

                if ( key < firstCode || key >= firstCode + entryCount )
                  { if_debug1('J', "[J]%u out of range\n", key);
                    return_error(gs_error_undefined);
                  }
                *pvalue = pl_get_uint16(table + 10 + ((key - firstCode) << 1));
                if_debug2('J', "[J]%u => %u\n", key, *pvalue);
                break;
            }
          default:
            return_error(gs_error_invalidfont);
          }
        return 0;
}

/* Encode a character using the TrueType cmap tables. */
/* (We think this is never used for downloaded fonts.) */
static gs_glyph
pl_tt_cmap_encode_char(gs_font_type42 *pfont, ulong cmap_offset,
  uint cmap_len, gs_char chr)
{       const byte *cmap;
        const byte *cmap_sub;
        const byte *table;
        uint value;
        int code;

        access(cmap_offset, cmap_len, cmap);
        /* Since the Apple cmap format is of no help in determining */
        /* the encoding, look for a Microsoft table; but if we can't */
        /* find one, take the first one. */
        cmap_sub = cmap + 4;
        { uint i;
          for ( i = 0; i < pl_get_uint16(cmap + 2); ++i )
            { if_debug3('j', "[j]cmap %d: platform %u encoding %u\n",
                        i, pl_get_uint16(cmap_sub + i * 8), pl_get_uint16(cmap_sub + i * 8 + 2));
              if ( pl_get_uint16(cmap_sub + i * 8) == 3 )
                { cmap_sub += i * 8;
                  break;
                }
            }
        }
        { uint offset = cmap_offset + pl_get_uint32(cmap_sub + 4);
          access(offset, cmap_offset + cmap_len - offset, table);
        }
        code = pl_cmap_lookup((uint)chr, table, &value);
        return (code < 0 ? gs_no_glyph : value);
}

/* Encode a character using the map built for downloaded TrueType fonts. */
static gs_glyph
pl_tt_dynamic_encode_char(const gs_font_type42 *pfont, gs_char chr)
{       pl_font_t *plfont = pfont->client_data;
        const pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, chr);

        return (ptcg->chr == gs_no_char ? gs_no_glyph : ptcg->glyph);
}

/* Return the galley character for a character code, if any; */
/* otherwise return gs_no_char. */
/* Note that we return 0xffff for a character that is explicitly */
/* designated as undefined. */
static gs_char
pl_font_galley_character(gs_char chr, const pl_font_t *plfont)
{       long GC = plfont->offsets.GC;
        const byte *gcseg;
        uint b0, b1;
        uint i, len;
        uint default_char;

        if ( GC < 0 )
          return gs_no_char;
        gcseg = plfont->header + GC;
        if ( plfont->large_sizes )
          len = pl_get_uint32(gcseg + 2),
          i = 12;
        else
          len = pl_get_uint16(gcseg + 2),
          i = 10;
        if ( len != pl_get_uint16(gcseg + i - 2) * 6 + 6 ) /* bad data */
          return gs_no_char;
        default_char = pl_get_uint16(gcseg + i - 4); /* default character */
        len += i - 6;
        b0 = chr >> 8;
        b1 = chr & 0xff;
        for ( ; i < len; i += 6 )
          if ( b0 >= gcseg[i] && b0 <= gcseg[i + 1] &&
               b1 >= gcseg[i + 2] && b1 <= gcseg[i + 3]
             )
            return pl_get_uint16(gcseg + i + 4);
        return default_char;
}

/* NB major hack.  This is used by pl_decode_glyph; */
gs_char last_char = 0;

/* Encode a character for a TrueType font. */
/* What we actually return is the TT glyph index.  Note that */
/* we may return either gs_no_glyph or 0 for an undefined character. */
gs_glyph
pl_tt_encode_char(gs_font *pfont_generic, gs_char chr, gs_glyph not_used)
{       
        gs_font_type42 *pfont = (gs_font_type42 *)pfont_generic;
        uint cmap_len;
        ulong cmap_offset = tt_find_table(pfont, "cmap", &cmap_len);
        gs_glyph glyph = 
          (cmap_offset == 0 ?
            /* This is a downloaded font with no cmap. */
           pl_tt_dynamic_encode_char(pfont, chr) :
           pl_tt_cmap_encode_char(pfont, cmap_offset, cmap_len, chr));
        pl_font_t *plfont = pfont->client_data;
        pl_font_glyph_t *pfg;

        last_char = chr;

        if ( plfont->offsets.GC < 0 )
          return glyph;  /* no substitute */
        pfg = pl_font_lookup_glyph(plfont, glyph);
        /* If the character is missing, use the galley character instead. */
        if ( !pfg->data )
          { gs_char galley_char = pl_font_galley_character(chr, plfont);

            if ( galley_char != gs_no_char )
              { return
                  (galley_char == 0xffff ? 0 :
                   cmap_offset == 0 ?
                   pl_tt_dynamic_encode_char(pfont, galley_char) :
                   pl_tt_cmap_encode_char(pfont, cmap_offset, cmap_len,
                                          galley_char));
              }
          }
        return glyph;   /* no substitute */
}


/* Get metrics */
static int
pl_tt_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])
{
    gs_glyph unused_glyph = gs_no_glyph;
    gs_glyph glyph = pl_tt_encode_char(plfont->pfont, char_code, unused_glyph);
    if ( glyph == gs_no_glyph ) {
        return 1;
    }
    return gs_type42_get_metrics((gs_font_type42 *)plfont->pfont,
                                  glyph, metrics);
}

/* Get character existence and escapement for a TrueType font. */
static int
pl_tt_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{       gs_font *pfont = plfont->pfont;
        gs_char chr = char_code;
        gs_glyph unused_glyph = gs_no_glyph;
        gs_glyph glyph = pl_tt_encode_char(pfont, chr, unused_glyph);
        int code;
        float sbw[4];
        
        pwidth->x = pwidth->y = 0;

        /* Check for a vertical substitute. */
        if ( pfont->WMode & 1 ) {
            gs_glyph vertical = pl_font_vertical_glyph(glyph, plfont);
            if ( vertical != gs_no_glyph )
              glyph = vertical;
        }

        /* undefined character */
        if ( glyph == 0xffff || glyph == gs_no_glyph )
            return 1;

        code = gs_type42_get_metrics((gs_font_type42 *)pfont, glyph, sbw);
        if ( code < 0 )
            return code;
        /* character exists */
        pwidth->x = sbw[2];
        return 0;
}


/* Render a TrueType character. */
static int
pl_tt_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph orig_glyph)
{       gs_glyph glyph = orig_glyph;
#define pbfont ((gs_font_base *)pfont)
#define pfont42 ((gs_font_type42 *)pfont)
        int code;
        pl_font_t *plfont = (pl_font_t *)pfont->client_data;
        float bold_fraction = gs_show_in_charpath(penum) != cpm_show ? 0.0 : plfont->bold_fraction;
        uint bold_added;
        double scale;
        float sbw[4], w2[6];
        int ipx, ipy, iqx, iqy;
        gx_device_memory *pmdev;
        bool ctm_modified = false;
        bool bold_device_created = false;
        gs_matrix save_ctm;
#define isDownloaded(p42) ((p42)->data.proc_data == 0)
#ifdef CACHE_TRUETYPE_CHARS
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcachedevice(penum, pgs, w2)
#else
#  define tt_set_cache(penum, pgs, w2)\
     gs_setcharwidth(penum, pgs, w2[0], w2[1]);
#endif
        /* undefined */
        if ( glyph == gs_no_glyph )
            return 0;
        /* Get the metrics and set the cache device. */
        code = gs_type42_get_metrics(pfont42, glyph, sbw);
        if ( code < 0 )
          return code;
        w2[0] = sbw[2], w2[1] = sbw[3];

        /* Adjust the bounding box for stroking if needed. */

        { const gs_rect *pbbox = &pbfont->FontBBox;

          w2[2] = pbbox->p.x, w2[3] = pbbox->p.y;
          w2[4] = pbbox->q.x, w2[5] = pbbox->q.y;
          if ( pfont->PaintType )
            {   double expand = max(1.415, gs_currentmiterlimit(pgs)) *
                  gs_currentlinewidth(pgs) / 2;

                w2[2] -= expand, w2[3] -= expand;
                w2[4] += expand, w2[5] += expand;
            }
        }

        /* Establish a current point. */
        if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
            return code;

        {
            bool madesub = false;
            /* Check for a vertical substitute. */
            if ( plfont->allow_vertical_substitutes ) {
                pl_font_t *plfont = pfont->client_data;
                gs_glyph vertical = pl_font_vertical_glyph(glyph, plfont);
                if ( vertical != gs_no_glyph ) {
                    glyph = vertical;
                    madesub = true;
                }
            }
            /* now check for rotation.  This is the ringer, fonts with
               escapement 1 em get rotated.  If you hold an HP
               engineer's head close to your ear you can hear the
               ocean. */
            if ( (pfont->WMode & 1) && sbw[2] == 1.0 ) {
                /* save the ctm */
                gs_currentmatrix(pgs, &save_ctm);
                ctm_modified = true;
                /* magic numbers - we don't completelely understand
                   the translation magic used by HP.  This provides a
                   good approximation */
                gs_translate(pgs, 1.0/1.15, -(1.0 - 1.0/1.15));
                gs_rotate(pgs, 90);
            }
        }


        /*
         * If we want pseudo-bold, render untransformed to an intermediate
         * bitmap, smear it, and then transform it to produce the output.
         * This is really messy.
         */
        if ( bold_fraction == 0 )
          { code = tt_set_cache(penum, pgs, w2);
            if ( code < 0 )
              return code;
            bold_added = 0;
          }
        else
          { gs_matrix mat, smat;
            gs_rect sbox;

            code = gs_gsave(pgs);
            if ( code < 0 )
              return code;
            gs_currentmatrix(pgs, &mat);
            /* Determine an appropriate scale for the bitmap. */
            scale = max(fabs(mat.xx) + fabs(mat.yx),
                        fabs(mat.xy) + fabs(mat.yy));
            gs_make_scaling(scale, scale, &smat);
            sbox.p.x = w2[2], sbox.p.y = w2[3];
            sbox.q.x = w2[4], sbox.q.y = w2[5];
            gs_bbox_transform(&sbox, &smat, &sbox);
            ipx = (int)sbox.p.x, ipy = (int)sbox.p.y;
            iqx = (int)ceil(sbox.q.x), iqy = (int)ceil(sbox.q.y);
            /* Set up the memory device for the bitmap.  NB should check code. */
            gs_make_mem_mono_device_with_copydevice(&pmdev,
                                            pgs->memory, pgs->device);
            bold_device_created = true;
            /* due to rounding, bold added (integer) can be zero while
               bold fraction (float) is non zero in which case we add
               1 scan line.  We do not "0" bold simply because it is
               inconvenient to back out at this point.  We don't have
               any HP tests which would show measurable difference
               either way (0 or 1). */
            bold_added = max((int)(scale * bold_fraction * 2 + 0.5), 1);
            pmdev->width = iqx - ipx + bold_added;
            pmdev->height = iqy - ipy;
            pmdev->bitmap_memory = pgs->memory;
            code = (*dev_proc(pmdev, open_device))((gx_device *)pmdev);
            if ( code < 0 )
              { gs_grestore(pgs);
                return code;
              }
            /* NB we have no idea why opening the device doesn't set
               the is_open flag, but this meme is repeated in various
               parts of the code, if it isn't done closing the memory
               device will not have no effect (release bitmap and
               mask). */
            pmdev->is_open = true;
            /* Don't allow gs_setdevice to reset things. */
            gx_set_device_only(pgs, (gx_device *)pmdev);

            { gs_fixed_rect cbox;
              cbox.p.x = cbox.p.y = fixed_0;
              cbox.q.x = int2fixed(pmdev->width);
              cbox.q.y = int2fixed(pmdev->height);
              gx_clip_to_rectangle(pgs, &cbox);
            }
            /* Make sure we clear the entire bitmap. */
            memset(pmdev->base, 0, bitmap_raster(pmdev->width) * pmdev->height);
            gx_set_device_color_1(pgs); /* write 1's */
            smat.tx = -ipx;
            smat.ty = -ipy;
            gs_setmatrix(pgs, &smat);
          }
        code = gs_type42_append(glyph, pgs, pgs->path,
                                (gs_text_enum_t *)penum, pfont,
                                gs_show_in_charpath(penum) != cpm_show);
        if ( code >= 0 )
          code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
        if (ctm_modified)
            gs_setmatrix(pgs, &save_ctm);
        if ( bold_added )
          gs_grestore(pgs);

        if ( code < 0 || !bold_added )
          return (code < 0 ? code : 0);

        /* Now smear the bitmap and copy it to the destination. */

        { gs_image_t image;
          gs_image_enum *ienum =
            gs_image_enum_alloc(pgs->memory, "pl_tt_build_char");
          byte *bold_lines =
            alloc_bold_lines(pgs->memory, pmdev->width - bold_added, bold_added,
                             "pl_tt_build_char(bold_lines)");

          if ( ienum == 0 || bold_lines == 0 )
            { code = gs_note_error(gs_error_VMerror);
              goto out;
            }
          gs_image_t_init_mask(&image, true);
          image.Width = pmdev->width;
          image.Height = pmdev->height + bold_added;
          gs_make_scaling(scale, scale, &image.ImageMatrix);
          image.ImageMatrix.tx = -ipx;
          image.ImageMatrix.ty = -ipy;
          image.adjust = true;
          code = gs_setcharwidth(penum, pgs, w2[0], w2[1]);
          if ( code < 0 )
            goto out;
          code = image_bitmap_char(ienum, &image, pmdev->base,
                                   bitmap_raster(pmdev->width), bold_added,
                                   bold_lines, pgs);
out:      if (bold_device_created)
              gx_device_retain((gx_device *)pmdev, false);
          gs_free_object(pgs->memory, bold_lines, "pl_tt_build_char(bold_lines)");
          gs_free_object(pgs->memory, ienum, "pl_tt_build_char(image enum)");
        }
        return (code < 0 ? code : 0);
#undef pfont42
#undef pbfont
}

/* We don't have to do any character encoding, since Intellifonts are */
/* indexed by character code (if bound) or MSL code (if unbound). */
static gs_glyph
pl_intelli_encode_char(gs_font *pfont, gs_char pchr, gs_glyph not_used)
{       return (gs_glyph)pchr;
}

/* Define the structure of the Intellifont metrics. */
typedef struct intelli_metrics_s {
  byte charSymbolBox[4][2];
  byte charEscapementBox[4][2];
  byte halfLine[2];
  byte centerline[2];
} intelli_metrics_t;

/* Merge the bounding box of a character into the composite box, */
/* and set the escapement.  Return true if the character is defined. */
static bool
pl_intelli_merge_box(float wbox[6], const pl_font_t *plfont, gs_glyph glyph)
{       const byte *cdata = pl_font_lookup_glyph(plfont, glyph)->data;

        if ( cdata == 0 )
          return false;
        wbox[1] = 0;
        if ( cdata[3] == 4 )
          { /* Compound character.  Merge the component boxes; */
            /* use the compound character's escapement. */
            bool found = false;
            uint i;

            for ( i = 0; i < cdata[6]; ++i )
              found |= pl_intelli_merge_box(wbox, plfont,
                                            pl_get_uint16(cdata + 8 + i * 6));
            wbox[0] = pl_get_int16(cdata + 4);
            return found;
          }
        /* Non-compound character. */
        cdata += 4;             /* skip PCL character header */
        { const intelli_metrics_t *metrics =
            (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
          int llx = pl_get_int16(metrics->charSymbolBox[0]);
          int lly = pl_get_int16(metrics->charSymbolBox[1]);
          int urx = pl_get_int16(metrics->charSymbolBox[2]);
          int ury = pl_get_int16(metrics->charSymbolBox[3]);

          wbox[0] = pl_get_int16(metrics->charEscapementBox[2]) -
            pl_get_int16(metrics->charEscapementBox[0]);
          wbox[2] = min(wbox[2], llx);
          wbox[3] = min(wbox[3], lly);
          wbox[4] = max(wbox[4], urx);
          wbox[5] = max(wbox[5], ury);
        }
        return true;
}

/* Do the work for rendering an Intellifont character. */
/* The caller has done the setcachedevice. */
static int
pl_intelli_show_char(gs_state *pgs, const pl_font_t *plfont, gs_glyph glyph)
{    
    int code;
    const byte *cdata;
    pl_font_glyph_t *font_glyph;
    const intelli_metrics_t *metrics;
    int *xBuffer, *yBuffer;
    client_name_t cname = (client_name_t)"pl_intelli_show_char";
    font_glyph = pl_font_lookup_glyph(plfont, glyph);
    cdata = font_glyph->data;

    if ( cdata == 0 ) {
	if_debug1('1', "[1] no character data for glyph %ld\n",glyph);
	return 0;
    }
    if ( cdata[3] == 4 ) { /* Compound character */
	gs_matrix save_ctm;
	int i;
	gs_currentmatrix(pgs, &save_ctm);
	for ( i = 0; i < cdata[6]; ++i ) {
	    const byte *edata = cdata + 8 + i * 6;
	    floatp x_offset = pl_get_int16(edata + 2);
	    floatp y_offset = pl_get_int16(edata + 4);
	    gs_translate(pgs, x_offset, y_offset);
	    code = pl_intelli_show_char(pgs, plfont, pl_get_uint16(edata));
	    gs_setmatrix(pgs, &save_ctm);
	    if ( code < 0 )
		return code;
	}
	return 0;
    } /* compound character */

    /* not compound character */
    {
	const byte *outlines;
	uint num_loops;
	uint i;
	cdata += 4;		/* skip PCL character header */
	outlines = cdata + pl_get_uint16(cdata + 6);
	num_loops = pl_get_uint16(outlines);

	if_debug2('1', "[1]ifont glyph %lu: loops=%u\n",(ulong)glyph, num_loops);

        if (num_loops == 0)
            return -1;

	for ( i = 0; i < num_loops; ++i ) {
	    const byte *xyc = cdata + pl_get_uint16(outlines + 4 + i * 8);
	    uint num_points;
	    uint num_aux_points;
	    const byte *x_coords, *y_coords, *x_coords_last;
	    const byte *x_aux_coords, *y_aux_coords, *x_aux_coords_last;
	    int llx, lly, urx, ury; /* character bounding box */
	    int x, y;
	    int xAux, yAux;
	    int *xLimit, *yLimit, *xScan, *yScan, *xLast;
	    int pointBufferSize;
            uint sz;

	    num_points = pl_get_uint16(xyc);
	    num_aux_points = pl_get_uint16(xyc + 2);

	    x_coords = xyc + 4;
	    y_coords = x_coords + num_points * 2;
	    x_coords_last = y_coords;

	    metrics = (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
	    llx = pl_get_int16(metrics->charSymbolBox[0]);
	    lly = pl_get_int16(metrics->charSymbolBox[1]);
	    urx = pl_get_int16(metrics->charSymbolBox[2]);
	    ury = pl_get_int16(metrics->charSymbolBox[3]);

	    pointBufferSize = num_points; /* allocate enough to hold all points */
	    if ( num_aux_points != 0xffff ) {
		pointBufferSize += num_aux_points;
		x_aux_coords = y_coords + num_points * 2;
		y_aux_coords = x_aux_coords + num_aux_points;
		x_aux_coords_last = y_coords;
	    }
	    else {
		x_aux_coords = NULL;
		y_aux_coords = NULL;
		x_aux_coords_last = NULL;
	    }

            sz = pointBufferSize * sizeof(int);

            if (i == 0) {
                xBuffer = (int *)gs_alloc_bytes(pgs->memory, sz, cname);
                yBuffer = (int *)gs_alloc_bytes(pgs->memory, sz, cname);
            } else {
                /* NB we don't have a font that tests this yet */
                xBuffer = (int *)gs_resize_object(pgs->memory, xBuffer, sz, cname);
                yBuffer = (int *)gs_resize_object(pgs->memory, yBuffer, sz, cname);
            }

	    if (xBuffer == NULL || yBuffer == NULL) {
		if( xBuffer != NULL)
		    gs_free_object(pgs->memory, xBuffer, "x point buffer");
		if( yBuffer != NULL)
		    gs_free_object(pgs->memory, yBuffer, "y point buffer");
		if_debug1('1', "[1]cannot allocate point buffers %i\n",pointBufferSize * sizeof(int));
		return_error(gs_error_VMerror);
	    }

	    xLimit = xBuffer + pointBufferSize;
	    yLimit = yBuffer + pointBufferSize;
	    xLast = NULL;

	    if_debug2('1', "[1]num_points=%u num_aux_points=%u\n", num_points, num_aux_points);

	    /* collect the points in the buffers, since we need to clean them up later */
	    /* only points inside the bounding box are allowed */
	    /* aux points are points inserted between two points, making the outline smoother */
	    /* the aux points could be used for curve fitting, but we add line segments */
	    for ( xScan = xBuffer, yScan = yBuffer; x_coords < x_coords_last; x_coords += 2, y_coords += 2 ) {
		x = pl_get_uint16(x_coords) & 0x3fff;
		y = pl_get_uint16(y_coords) & 0x3fff;

		if_debug4('1', "[1]%s (%d,%d) %s\n",
			  (*x_coords & 0x80 ? " line" : "curve"), x, y,
			  (*y_coords & 0x80 ? " line" : "curve"));

		if (xScan > xBuffer)  { /* not first point, therefore aux is possible */
		    if ( x_aux_coords < x_aux_coords_last &&!(*x_coords & 0x80) ) { /* use an aux point */
			/* The auxiliary dx and dy values are signed. */
			int dx = (*x_aux_coords++ ^ 0x80) - 0x80;
			int dy = (*y_aux_coords++ ^ 0x80) - 0x80;

			if_debug2('1', "[1]... aux (%d,%d)\n", dx, dy);

			xAux = (x + *(xScan-1)) / 2 + dx;
			yAux = (y + *(yScan-1)) / 2 + dy;
			if ((xAux >= llx && xAux <= urx) && (yAux >= lly && yAux <= ury)) { /* aux point is inside bounding box */
			    *xScan++ = xAux;
			    *yScan++ = yAux;
			} /* end point inside bounding box */
			/* what do points outside the bounding box mean? */
		    } /* use an aux point */
		} /* not first point */

		if ( (x >= llx && x <= urx) && (y >= lly && y <= ury) ) { /* point inside bounding box */
		    *xScan++ = x;
		    *yScan++ = y;
		} /* point inside bounding box */
	    } /* for num_points - first time through */

	    if ( num_aux_points != 0xffff ) 
		xLast = xScan;
	    else
		xLast = xScan - 1; /* discard the last point */

	    xScan = xBuffer;
	    yScan = yBuffer;
	    if (xLast > xBuffer) {
		code = gs_moveto(pgs, (floatp)*xScan++, (floatp)*yScan++);
		if ( code < 0 )
		    goto cleanup;
	    }

	    for (; xScan < xLast; ) {
		code = gs_lineto(pgs, (floatp)*xScan++, (floatp)*yScan++);
		if ( code < 0 )
		    goto cleanup;
	    }
	    /* close the path of this loop */
	    code = gs_closepath(pgs);
	    if ( code < 0 )
                break;

	} /* for num_loops */
    
cleanup:
        gs_free_object(pgs->memory, xBuffer, "x point buffer");
        gs_free_object(pgs->memory, yBuffer, "y point buffer");
    } /* end not compound */
    return code;
}

/* Get character existence and escapement for an Intellifont. */
static int
pl_intelli_char_width(const pl_font_t *plfont, const void *pgs, uint char_code, gs_point *pwidth)
{       
        const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;
        int wx;

        if ( !pwidth )
          return (cdata == 0 ? 1 : 0);
        if ( cdata == 0 )
          { pwidth->x = pwidth->y = 0;
            return 1;
          }
        switch ( cdata[3] )
          {
          case 3:               /* non-compound character */
            cdata += 4;         /* skip PCL character header */
            { const intelli_metrics_t *metrics =
                (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));
              wx =
                pl_get_int16(metrics->charEscapementBox[2]) -
                pl_get_int16(metrics->charEscapementBox[0]);
            }
            break;
          case 4:               /* compound character */
            wx = pl_get_int16(cdata + 4);
            break;
          default:              /* shouldn't happen */
            pwidth->x = pwidth->y = 0;
            return 0;
          }
        pwidth->x = (floatp)wx / 8782.0;

#ifdef DEBUG
        {
            pl_font_glyph_t *cglyph = pl_font_lookup_glyph(plfont, char_code);
            if_debug1('1', "[1] glyph %ld\n", cglyph->glyph);
            if_debug2('1', "[1] intelli width of %d %f\n", char_code, pwidth->x);
        }
#endif
        return 0;
}

static int
pl_intelli_char_metrics(const pl_font_t *plfont, const void *pgs, uint char_code, float metrics[4])

{
    gs_point width;
    const byte *cdata = pl_font_lookup_glyph(plfont, char_code)->data;

    metrics[0] = metrics[1] = metrics[2] = metrics[3] = 0;

    if ( cdata == 0 ) { 
        return 1;
    }

    /* compound */
    if ( cdata[3] == 4 ) {
        dprintf("warning compound intellifont metrics not supported" );
        return 0;
    }

    cdata += 4;
    
    {
        const intelli_metrics_t *intelli_metrics =
            (const intelli_metrics_t *)(cdata + pl_get_uint16(cdata + 2));

        /* NB probably not right */
        /* never a vertical substitute, doesn't yet handle compound characters */
        metrics[0] = (float)pl_get_int16(intelli_metrics->charSymbolBox[0]);
        metrics[0] /= 8782.0;
        pl_intelli_char_width(plfont, pgs, char_code, &width);
        metrics[2] = width.x;
        return 0;
    }
}

/* Render a character for an Intellifont. */
static int
pl_intelli_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char chr, gs_glyph glyph)
{       const pl_font_t *plfont = (const pl_font_t *)pfont->client_data;
        float wbox[6];
        int code;

        wbox[0] = wbox[1] = 0;
        wbox[2] = wbox[3] = 65536.0;
        wbox[4] = wbox[5] = -65536.0;
        if ( !pl_intelli_merge_box(wbox, plfont, glyph) )
          { wbox[2] = wbox[3] = wbox[4] = wbox[5] = 0;
            code = gs_setcachedevice(penum, pgs, wbox);
            return (code < 0 ? code : 0);
          }
        code = gs_setcachedevice(penum, pgs, wbox);
        if ( code < 0 )
          return code;
        code = pl_intelli_show_char(pgs, plfont, glyph);
        if ( code < 0 )
          return code;
        /* Since we don't take into account which side of the loops is */
        /* outside, we take the easy way out.... */
        code = gs_eofill(pgs);
        return (code < 0 ? code : 0);
}


/* ---------------- Internal initialization ---------------- */

/* Initialize the procedures for a bitmap font. */
void
pl_bitmap_init_procs(gs_font_base *pfont)
{       pfont->procs.encode_char = (void *)pl_bitmap_encode_char; /* FIX ME (void *) */
        pfont->procs.build_char = (void *)pl_bitmap_build_char; /* FIX ME (void *) */
#define plfont ((pl_font_t *)pfont->client_data)
        plfont->char_width = pl_bitmap_char_width;
        plfont->char_metrics = pl_bitmap_char_metrics;
#undef plfont
}

/* Initialize the procedures for a TrueType font. */
void
pl_tt_init_procs(gs_font_type42 *pfont)
{  
    pfont->procs.encode_char = (void *)pl_tt_encode_char; /* FIX ME (void *) */
    pfont->procs.build_char = (void *)pl_tt_build_char; /* FIX ME (void *) */
    pfont->data.string_proc = pl_tt_string_proc;
#define plfont ((pl_font_t *)pfont->client_data)
    plfont->char_width = pl_tt_char_width;
    plfont->char_metrics = pl_tt_char_metrics;
#undef plfont
}

uint
pl_tt_get_glyph_index(gs_font_type42 *pfont42, gs_glyph glyph)
{
    /* identity */
    return glyph;
}

/* Finish initializing a TrueType font. */
void
pl_tt_finish_init(gs_font_type42 *pfont, bool downloaded)
{       float upem = pfont->data.unitsPerEm;
        ulong head = tt_find_table(pfont, "head", NULL);
        const byte *hdata;

        pfont->data.get_glyph_index = pl_tt_get_glyph_index;
        if ( downloaded )
          pfont->data.get_outline = pl_tt_get_outline;
        /* Set the FontBBox. */
        access(head, 44, hdata);
        pfont->FontBBox.p.x = pl_get_int16(hdata + 36) / upem;
        pfont->FontBBox.p.y = pl_get_int16(hdata + 38) / upem;
        pfont->FontBBox.q.x = pl_get_int16(hdata + 40) / upem;
        pfont->FontBBox.q.y = pl_get_int16(hdata + 42) / upem;
#ifdef DEBUG
        if ( gs_debug_c('m') )
          { const byte *OffsetTable;
            uint numTables;
            const byte *TableDirectory;
            uint i;

            access(0, 12, OffsetTable);
            numTables = pl_get_uint16(OffsetTable + 4);
            access(12, numTables * 16, TableDirectory);
            for ( i = 0; i < numTables; ++i )
              { const byte *tab = TableDirectory + i * 16;
                dprintf6("%c%c%c%c offset = %lu length = %lu\n",
                         tab[0], tab[1], tab[2], tab[3],
                         (ulong)pl_get_uint32(tab + 8), (ulong)pl_get_uint32(tab + 12));
              }
          }
#endif
        /* override default get metrics */
        pfont->data.get_metrics = pl_tt_get_metrics;
}

void
pl_intelli_init_procs(gs_font_base *pfont)
{       pfont->procs.encode_char = (void *)pl_intelli_encode_char;  /* FIX ME (void *) */
        pfont->procs.build_char = (void *)pl_intelli_build_char; /* FIX ME (void *) */
#define plfont ((pl_font_t *)pfont->client_data)
        plfont->char_width = pl_intelli_char_width;
        plfont->char_metrics = pl_intelli_char_metrics;
#undef plfont
}
/* ---------------- Public procedures ---------------- */

/* Allocate the glyph table. */
int
pl_font_alloc_glyph_table(pl_font_t *plfont, uint num_glyphs, gs_memory_t *mem,
  client_name_t cname)
{       uint size = num_glyphs + (num_glyphs >> 2) + 5;
        pl_font_glyph_t *glyphs =
          gs_alloc_struct_array(mem, size, pl_font_glyph_t,
                                &st_pl_font_glyph_element, cname);

        if ( glyphs == 0 )
          return_error(gs_error_VMerror);
        { uint i;
          for ( i = 0; i < size; ++i )
            glyphs[i].glyph = 0, glyphs[i].data = 0;
        }
        plfont->glyphs.table = glyphs;
        plfont->glyphs.used = 0;
        plfont->glyphs.limit = num_glyphs;
        plfont->glyphs.size = size;
        plfont->glyphs.skip = size * 2 / 3;
        while ( igcd(plfont->glyphs.skip, size) > 1 )
          plfont->glyphs.skip++;
        return 0;
}

/* Expand the glyph table. */
static int
expand_glyph_table(pl_font_t *plfont, gs_memory_t *mem)
{       pl_glyph_table_t old_table;
        int code;
        uint i;

        old_table = plfont->glyphs;
        code = pl_font_alloc_glyph_table(plfont, old_table.size, mem,
                                         "expand_glyph_table(new table)");
        if ( code < 0 )
          return code;
        for ( i = 0; i < old_table.size; ++i )
          if ( old_table.table[i].data )
            *pl_font_lookup_glyph(plfont, old_table.table[i].glyph) =
              old_table.table[i];
        gs_free_object(mem, old_table.table, "expand_glyph_table(old table)");
        plfont->glyphs.used = old_table.used;
        return 0;
}

/* Allocate the TrueType character to glyph index map. */
int
pl_tt_alloc_char_glyphs(pl_font_t *plfont, uint num_chars, gs_memory_t *mem,
  client_name_t cname)
{       uint size = num_chars + (num_chars >> 2) + 5;
        pl_tt_char_glyph_t *char_glyphs =
          (pl_tt_char_glyph_t *)
          gs_alloc_byte_array(mem, size, sizeof(pl_tt_char_glyph_t), cname);

        if ( char_glyphs == 0 )
          return_error(gs_error_VMerror);
        { uint i;
          for ( i = 0; i < size; ++i )
            char_glyphs[i].chr = gs_no_char, char_glyphs[i].glyph = 0;
        }
        plfont->char_glyphs.table = char_glyphs;
        plfont->char_glyphs.used = 0;
        plfont->char_glyphs.limit = num_chars;
        plfont->char_glyphs.size = size;
        plfont->char_glyphs.skip = size * 2 / 3;
        while ( igcd(plfont->char_glyphs.skip, size) > 1 )
          plfont->char_glyphs.skip++;
        return 0;
}

/* Expand the character to glyph index map. */
static int
expand_char_glyph_table(pl_font_t *plfont, gs_memory_t *mem)
{       pl_tt_char_glyph_table_t old_table;
        int code;
        uint i;

        old_table = plfont->char_glyphs;
        code = pl_tt_alloc_char_glyphs(plfont, old_table.size, mem,
                                       "expand_char_glyphs(new table)");
        if ( code < 0 )
          return code;
        for ( i = 0; i < old_table.size; ++i )
          if ( old_table.table[i].chr != gs_no_char )
            *pl_tt_lookup_char(plfont, old_table.table[i].chr) =
              old_table.table[i];
        gs_free_object(mem, old_table.table, "expand_char_glyphs(old table)");
        plfont->char_glyphs.used = old_table.used;
        return 0;
}

/* Add a glyph to a font.  Return -1 if the table is full. */
typedef struct font_glyph_s {
  gs_font *font;
  gs_glyph glyph;
} font_glyph_t;
static bool
match_font_glyph(const gs_memory_t *mem, cached_char *cc, void *vpfg)
{       const font_glyph_t *pfg = vpfg;
        return (cc->pair->font == pfg->font && cc->code == pfg->glyph);
}
int
pl_font_add_glyph(pl_font_t *plfont, gs_glyph glyph, const byte *cdata)
{       gs_font *pfont = plfont->pfont;
        gs_glyph key = glyph;
        pl_tt_char_glyph_t *ptcg = 0;
        pl_font_glyph_t *pfg;
        /*
         * If this is a downloaded TrueType font, the "glyph" is actually
         * a character code, and the actual TrueType glyph index is in the
         * character header.  In this case, the character data must be either
         * a PCL5 format 15 or a PCL XL format 1 downloaded character.
         */
tcg:    if ( plfont->char_glyphs.table )
          { ptcg = pl_tt_lookup_char(plfont, key);
            if ( ptcg->chr == gs_no_char &&
                 plfont->char_glyphs.used >= plfont->char_glyphs.limit
               )
              { /* Table is full, try to expand it. */
                int code = expand_char_glyph_table(plfont, pfont->memory);
                if ( code < 0 )
                  return code;
                goto tcg;
              }
            /* get glyph id from character download */
            if ( cdata[0] == 1 )
                /* pxl truetype format 1, 
                 * class 0 at offset 4, class 1 at offset 8 or class 2 at 10  */
                key = pl_get_uint16(cdata + ((cdata[1] == 0) ? 4 : ((cdata[1] == 1) ? 8 : 10)));
            else
                /* pcl truetype format 15 */ 
                key = pl_get_uint16(cdata + cdata[2] + 4);
          }
fg:     pfg = pl_font_lookup_glyph(plfont, key);
        if ( pfg->data != 0 )
          { /* Remove the glyph from the character cache. */
            font_glyph_t match_fg;

            match_fg.font = pfont;
            match_fg.glyph = key;
            gx_purge_selected_cached_chars(pfont->dir, match_font_glyph,
                                           &match_fg);
            gs_free_object(pfont->memory, (void *)pfg->data,
                           "pl_font_add_glyph(old data)");
          }
        else
          { if ( plfont->glyphs.used >= plfont->glyphs.limit )
              { /* Table is full, try to expand it. */
                int code = expand_glyph_table(plfont, pfont->memory);
                if ( code < 0 )
                  return code;
                goto fg;
              }
            plfont->glyphs.used++;
          }
        if ( ptcg )
          { if ( ptcg->chr == gs_no_char )
              plfont->char_glyphs.used++;
            ptcg->chr = glyph;
            ptcg->glyph = key;
          }
        pfg->glyph = key;
        pfg->data = cdata;
        return 0;
}

static bool
is_composite(byte *pgdata)
{
    return (pl_get_int16(pgdata) == -1);
}

int
pl_font_disable_composite_metrics(pl_font_t *plfont, gs_glyph glyph)
{
    gs_glyph key = glyph;
    gs_font_type42 *pfont = plfont->pfont;
    pl_font_glyph_t *pfg;
    gs_glyph_data_t glyph_data;
    int code;

    /* This is the unusual way of looking up a glyph thanks to the pcl
       font wrapper format.  It is documented in several other places
       in this file.  If a char_glyphs table is not available it is
       not a downloadedd TT font wrapper so we do nothing. */
    if (plfont->char_glyphs.table) { 
        pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, key);
        if ( ptcg->chr == gs_no_char )
            return 0;
        key = ptcg->glyph;
    } else {
        return -1;
    }

    /* should never fail as this procedure is called only after a
       glyph has been successfully added. */
    code = pfont->data.get_outline(pfont, key, &glyph_data);
    if (code < 0)
        return code;

    /* null glyph */
    if (glyph_data.bits.data == 0)
        return 0;
    
    /* the glyph is guaranteed by the langauges to be have a reasonable
       header (enough to test for a composite glyph and do the tests
       below for components) so a UMR or overflow is not possible but
       it would be better to add checks.  The component parser below  */
    if (!is_composite(glyph_data.bits.data))
        return 0;

    /* enumerate the components and rewrite the flags field to not use
       metrics from the component.  Similar to the enumeration code in
       gstype42.c */
    {
        uint flags;
        byte *next_component = glyph_data.bits.data + 10;
        do {
            gs_matrix_fixed mat;
            byte *last_component = next_component;
            memset(&mat, 0, sizeof(mat)); /* arbitrary */
            gs_type42_parse_component(&next_component, &flags, &mat, NULL, plfont->pfont, &mat);
            if (flags & TT_CG_USE_MY_METRICS)
                /* bit 9 of the flags is the "use my metrics" flag
                   which is bit 1 of byte 0 big endian wise */
                last_component[0] &= ~(1 << 1);
        } while (flags & TT_CG_MORE_COMPONENTS);
    }
    return 0;
}
/* Remove a glyph from a font.  Return 1 if the glyph was present. */
int
pl_font_remove_glyph(pl_font_t *plfont, gs_glyph glyph)
{       gs_font *pfont = plfont->pfont;
        gs_glyph key = glyph;
        pl_font_glyph_t *pfg;

        /* See above regarding downloaded TrueType fonts. */
        if ( plfont->char_glyphs.table )
          { pl_tt_char_glyph_t *ptcg = pl_tt_lookup_char(plfont, key);
            if ( ptcg->chr == gs_no_char )
              return 0;
            key = ptcg->glyph;
            ptcg->chr = gs_no_char;
            ptcg->glyph = 1;    /* mark as deleted */
            plfont->char_glyphs.used--;
          }
        /* may not have a glyph table in case of cloned resident */
        if ( plfont->glyphs.table == 0 )
            return 0;
        pfg = pl_font_lookup_glyph(plfont, key);
        if ( pfg->data == 0 )
          return 0;             /* character not defined */
        { /* Remove the glyph from the character cache. */
          font_glyph_t match_fg;

          match_fg.font = pfont;
          match_fg.glyph = key;
          gx_purge_selected_cached_chars(pfont->dir, match_font_glyph,
                                         &match_fg);
          gs_free_object(pfont->memory, (void *)pfg->data,
                         "pl_font_remove_glyph(data)");
        }
        pfg->data = 0;
        pfg->glyph = 1;         /* mark as deleted */
        plfont->glyphs.used--;
        return 1;
}
