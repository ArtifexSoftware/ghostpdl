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

/* pcsfont.c */
/* PCL5 soft font creation commands */
#include "memory_.h"
#include "stdio_.h"             /* needed for pl_load_tt_font */
#include "math_.h"
#include "gdebug.h"
#include "pcommand.h"
#include "pcfont.h"
#include "pcursor.h"
#include "pcpage.h"
#include "pcstate.h"
#include "pcfsel.h"
#include "pcparse.h"
#include "pjparse.h"
#include "pldict.h"
#include "plvalue.h"
#include "plmain.h" /* for high_level_device...
                      hopefully this will go away soon */
#include "gsbitops.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"

/* comment out to return an error instead of continuing when
   processing a corrupt download font. */
#define IGNORE_CORRUPT_FONT

/* Emulate bug in HP printer where component metrics are ignored. */
#define DISABLE_USE_MY_METRICS

/* Define the downloaded character data formats. */
typedef enum {
  pccd_bitmap = 4,
  pccd_intellifont = 10,
  pccd_truetype = 15
} pcl_character_format;

/* ------ Internal procedures ------ */

/* Delete a soft font.  If it is the currently selected font, or the */
/* current primary or secondary font, cause a new one to be chosen. */
static void
pcl_delete_soft_font(pcl_state_t *pcs, const byte *key, uint ksize,
  void *value)
{
    if ( value == NULL ) {
        if ( !pl_dict_find_no_stack(&pcs->soft_fonts, key, ksize, &value) )
            return;             /* not a defined font */
    }
    {
        pl_font_t *plfontp = (pl_font_t *)value;
        if ( pcs->font_selection[0].font == plfontp )
            pcs->font_selection[0].font = 0;
        if ( pcs->font_selection[1].font == plfontp )
            pcs->font_selection[1].font = 0;
        /* if this is permanent font we need to tell PJL we are
           removing it */
        if ( plfontp->storage & pcds_permanent )
            if ( pjl_proc_register_permanent_soft_font_deletion(pcs->pjls,
                       plfontp->params.pjl_font_number) > 0 )
                pcl_set_current_font_environment(pcs);
        pcs->font = pcs->font_selection[pcs->font_selected].font;
        pl_dict_undef_purge_synonyms(&pcs->soft_fonts, key, ksize);
    }
}

/* ------ Commands ------ */

static int /* ESC * c <id> D */
pcl_assign_font_id(pcl_args_t *pargs, pcl_state_t *pcs)
{       uint id = uint_arg(pargs);
        id_set_value(pcs->font_id, id);
        /* set state to use id's not strings */
        pcs->font_id_type = numeric_id;
        return 0;
}

/* Supports copy or assignment of resident fonts.  Copying a soft font
   involves a deep copy and a new font.  Copying a resident font
   creates a link to the original (source) font */

static int
pcl_make_resident_font_copy(pcl_state_t *pcs)
{
    pl_dict_enum_t dictp;
    gs_const_string key;
    void * value;
    bool found = false;

    /* first check for a duplicate key, if found remove it */
    if ( pl_dict_lookup(&pcs->built_in_fonts, current_font_id, current_font_id_size,
                        &value, false, (pl_dict_t **)0) )
        if ( pl_dict_undef(&pcs->built_in_fonts, current_font_id, current_font_id_size) == false )
                /* shouldn't fail */
                return -1;

    /* now search for the value */
    pl_dict_enum_begin(&pcs->built_in_fonts, &dictp);
    while ( pl_dict_enum_next(&dictp, &key, &value ) )
        if ( (void *)(pcs->font) == value ) {
            found = true;
            break;
        }
    if ( found == false )
        return -1;
    pl_dict_put_synonym(&pcs->built_in_fonts, key.data,
                        key.size, current_font_id,
                        current_font_id_size);
    return 0;
}


static int /* ESC * c <fc_enum> F */
pcl_font_control(pcl_args_t *pargs, pcl_state_t *pcs)
{       
    gs_const_string key;
    void *value;
    pl_dict_enum_t denum;

    switch ( uint_arg(pargs) ) {
    case 0:
        /* Delete all soft fonts. */
        pcs->font = pcs->font_selection[pcs->font_selected].font;
        pl_dict_enum_stack_begin(&pcs->soft_fonts, &denum, false);
        while ( pl_dict_enum_next(&denum, &key, &value) ) {
            pcl_delete_soft_font(pcs, key.data, key.size, value);
            /* when deleting fonts we must restart the enumeration
               each time because the last deleted font may have had
               deleted synonyms which are not properly registered in
               the enumeration structure.  NB A similar change should
               be made for deleting all temporary fonts. */
            pl_dict_enum_stack_begin(&pcs->soft_fonts, &denum, false);
        }
        pl_dict_release(&pcs->soft_fonts);
        break;
    case 1:
	/* Delete all temporary soft fonts. */
	pl_dict_enum_stack_begin(&pcs->soft_fonts, &denum, false);
	while ( pl_dict_enum_next(&denum, &key, &value) )
	    if ( ((pl_font_t *)value)->storage == pcds_temporary )
		pcl_delete_soft_font(pcs, key.data, key.size, value);
        break;
    case 2:
	/* Delete soft font <font_id>. */
	pcl_delete_soft_font(pcs, current_font_id, current_font_id_size, NULL);
	/* decache the currently selected font in case we deleted it. */
	pcl_decache_font(pcs, -1);

        break;
    case 3:
        /* Delete character <font_id, character_code>. */
        if ( pl_dict_find_no_stack(&pcs->soft_fonts, current_font_id, current_font_id_size, &value) )
            pl_font_remove_glyph((pl_font_t *)value, pcs->character_code);
        return 0;
        
        break;
    case 4:
        /* Make soft font <font_id> temporary. */
        if ( pl_dict_find_no_stack(&pcs->soft_fonts, current_font_id, current_font_id_size, &value) )
            ((pl_font_t *)value)->storage = pcds_temporary;
        
        break;
    case 5:
        /* Make soft font <font_id> permanent. */
        if ( pl_dict_find_no_stack(&pcs->soft_fonts, current_font_id, current_font_id_size, &value) ) {
            ((pl_font_t *)value)->storage = pcds_permanent;
            ((pl_font_t *)value)->params.pjl_font_number = 
                pjl_proc_register_permanent_soft_font_addition(pcs->pjls);
        }
        break;
    case 6:
        {
            int code;
            if ( pcs->font == 0 ) { 
                code = pcl_recompute_font(pcs, false);
                if ( code < 0 )
                    return code;
            }
            if ( pcs->font->storage == pcds_internal ) {
                return pcl_make_resident_font_copy(pcs);
            } else {
                pl_font_t *plfont = pl_clone_font(pcs->font,
                                                  pcs->memory,
                                                  "pcl_font_control()");
                if ( plfont == 0 ) {
                    dprintf("pcsfont.c clone font FIXME\n");
                    return 0;
                }
                code = gs_definefont(pcs->font_dir, plfont->pfont);
                if ( code < 0 )
                    return code;
                pcl_delete_soft_font(pcs, current_font_id, current_font_id_size, NULL);
                plfont->storage = pcds_temporary;
                plfont->data_are_permanent = false;
                pl_dict_put(&pcs->soft_fonts, current_font_id, current_font_id_size, plfont);
            }
        }
        break;
    default:
        return 0;
    }
    return 0;
}

static int /* ESC ) s <count> W */
pcl_font_header(pcl_args_t *pargs, pcl_state_t *pcs)
{       uint count = uint_arg(pargs);
        const byte *data = arg_data(pargs);
        pcl_font_header_t *pfh = (pcl_font_header_t *)data;
        uint desc_size;
        pl_font_scaling_technology_t fst;
        gs_memory_t *mem = pcs->memory;
        pl_font_t *plfont;
        byte *header;
        int code;
        bool has_checksum;
        if ( count < 64 && pfh->HeaderFormat != pcfh_bitmap)
          return e_Range; /* pcfh_bitmap defaults short headers to 0 except underline position = 5; */ 
        desc_size =
          (pfh->FontDescriptorSize[0] << 8) + pfh->FontDescriptorSize[1];
        /* Dispatch on the header format. */
        switch ( pfh->HeaderFormat )
          {
          case pcfh_bitmap:
          case pcfh_resolution_bitmap:
            fst = plfst_bitmap;
            has_checksum = false;
            break;
          case pcfh_intellifont_bound:
          case pcfh_intellifont_unbound:
            fst = plfst_Intellifont;
            /* intellifonts do have a checksum but we have seen
                several fonts in tests that don't follow the
                documentation.  It is possible intellifonts use a
                different offset than truetype to begin the summation.
                We have not investigated. */
            has_checksum = false;
            break;
          case pcfh_truetype:
          case pcfh_truetype_large:
            fst = plfst_TrueType;
            has_checksum = true;
            break;
          default:
#ifdef IGNORE_CORRUPT_FONT
              return 0;
#else
              return_error(gs_error_invalidfont);
#endif

          }

        /* Fonts should include a final byte that makes the sum of the
           bytes in the font 0 (mod 256).  The hp documentation says
           byte 64 and up should contribute to the checksum.  All
           known examples indicate byte 64 (index 63 of the array
           below) is not included. */
        if (has_checksum) {
            ulong sum = 0;
            int i;
            for (i = count - 1; i >= 64; i--) {
                sum += data[i];
                sum %= 256;
            }

            if (sum != 0) {
                dprintf1("corrupt font sum=%ld\n", sum);
                return e_Range;
            }
        }
        /* Delete any previous font with this ID. */
        pcl_delete_soft_font(pcs, current_font_id, current_font_id_size, NULL);
        /* Create the generic font information. */
        plfont = pl_alloc_font(mem, "pcl_font_header(pl_font_t)");
        header = gs_alloc_bytes(mem, count, "pcl_font_header(header)");
        if ( plfont == 0 || header == 0 )
          return_error(e_Memory);
        memcpy(header, data, count);
        plfont->storage = pcds_temporary;
        plfont->data_are_permanent = false;
        if (fst == plfst_Intellifont) {
            uint gifct_offset;
            if ( pfh->HeaderFormat == pcfh_intellifont_bound )
                gifct_offset = 78;
            else {
                gifct_offset = 78 + 8; /* + 8 for the character complement */
                /* copy in the compliment while we are here. */
                memcpy(plfont->character_complement, &data[78], 8);
            }
        }
        plfont->header = header;
        plfont->header_size = count;
        plfont->is_xl_format = false;
        plfont->scaling_technology = fst;
        plfont->font_type = (pl_font_type_t)pfh->FontType;
        plfont->font_file = (char *)0;
        code = pl_font_alloc_glyph_table(plfont, 256, mem,
                                         "pcl_font_header(char table)");
        if ( code < 0 )
          return code;
        /* Create the actual font. */
        switch ( fst )
          {
          case plfst_bitmap:
          {
            gs_font_base *pfont;
bitmap:     pfont = gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                                    "pcl_font_header(bitmap font)");

            if ( pfont == 0 )
              return_error(e_Memory);
            code = pl_fill_in_font((gs_font *)pfont, plfont, pcs->font_dir,
                                   mem, "nameless_font");
            if ( code < 0 )
              return code;
            pl_fill_in_bitmap_font(pfont, gs_next_ids(mem, 1));
            /* Extract parameters from the font header. */
            if ( pfh->HeaderFormat == pcfh_resolution_bitmap )
              {
#define pfhx ((const pcl_resolution_bitmap_header_t *)pfh)
                plfont->resolution.x = pl_get_uint16(pfhx->XResolution);
                plfont->resolution.y = pl_get_uint16(pfhx->YResolution);
#undef pfhx
              }
            /* note that we jump to the bitmap label for type 16 - so
               called truetype large which can also be bitmap but its
               resolution field is filled in when scanning the
               segments (see pl_font_scan_segments() the resolution
               does not show up in the font header. */
            else if ( pfh->HeaderFormat == pcfh_bitmap )
                plfont->resolution.x = plfont->resolution.y = 300;
            { ulong pitch_1024th_dots =
                ((ulong)pl_get_uint16(pfh->Pitch) << 8) + pfh->PitchExtended;
              floatp pitch_cp = (floatp)
                (pitch_1024th_dots / 1024.0     /* dots */
                 / plfont->resolution.x /* => inches */
                 * 7200.0);

              pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
            }
            {
              uint height_quarter_dots = pl_get_uint16(pfh->Height);
              plfont->params.height_4ths =
                (uint)(floor((double)height_quarter_dots * 72.0 /
                             (double)(plfont->resolution.x) + 0.5));
            }
            break;
          }
          case plfst_TrueType:
          {
            gs_font_type42 *pfont;
            { static const pl_font_offset_errors_t errors = {
                gs_error_invalidfont, 0
              };
              code =
                pl_font_scan_segments(mem, plfont, 70, desc_size, (ulong)count - 2,
                                      pfh->HeaderFormat == pcfh_truetype_large,
                                      &errors);
              if ( code < 0 )
                return code;
              /* truetype large format 16 can be truetype or bitmap -
                 absurd */
              if ( ( pfh->HeaderFormat == pcfh_truetype_large ) &&
                   ( plfont->scaling_technology == plfst_bitmap ) )
                  goto bitmap;
                                                                  
            }
            pfont = gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42,
                                    "pcl_font_header(truetype font)");
            if ( pfont == 0 )
              return_error(e_Memory);

            { uint num_chars = pl_get_uint16(pfh->LastCode);

              if ( num_chars < 20 )
                num_chars = 20;
              else if ( num_chars > 300 )
                num_chars = 300;
              code = pl_tt_alloc_char_glyphs(plfont, num_chars, mem,
                                             "pcl_font_header(char_glyphs)");
              if ( code < 0 )
                return code;
            }
            code = pl_fill_in_font((gs_font *)pfont, plfont, pcs->font_dir,
                                   mem, "nameless_font");
            if ( code < 0 )
              return code;
            pl_fill_in_tt_font(pfont, NULL, gs_next_ids(mem, 1));
            { uint pitch_cp =
                pl_get_uint16(pfh->Pitch) * 1000.0 / pfont->data.unitsPerEm;
              pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
            }
          }
            break;
          case plfst_Intellifont:
          {
            gs_font_base *pfont =
              gs_alloc_struct(mem, gs_font_base, &st_gs_font_base,
                              "pcl_font_header(bitmap font)");

            if ( pfont == 0 )
              return_error(e_Memory);
            code = pl_fill_in_font((gs_font *)pfont, plfont, pcs->font_dir,
                                   mem, "nameless_font");
            if ( code < 0 )
              return code;
            pl_fill_in_intelli_font(pfont, gs_next_ids(mem, 1));
            { uint pitch_cp =
                pl_get_uint16(pfh->Pitch) * 1000.0 / 8782.0;
              pl_fp_set_pitch_cp(&plfont->params, pitch_cp);
            }
            break;
          }
          default:
            return_error(gs_error_invalidfont); /* can't happen */
          }
        /* Extract common parameters from the font header. */
        plfont->params.symbol_set = pl_get_uint16(pfh->SymbolSet);
        plfont->params.proportional_spacing = pfh->Spacing;
        plfont->params.style = (pfh->StyleMSB << 8) + pfh->StyleLSB;
        plfont->params.stroke_weight = /* signed byte */
          (int)(pfh->StrokeWeight ^ 0x80) - 0x80;
        plfont->params.typeface_family =
          (pfh->TypefaceMSB << 8) + pfh->TypefaceLSB;
        pl_dict_put(&pcs->soft_fonts, current_font_id,
                    current_font_id_size, plfont);
        plfont->pfont->procs.define_font = gs_no_define_font;
        return gs_definefont(pcs->font_dir, plfont->pfont);
}

static int /* ESC * c <char_code> E */
pcl_character_code(pcl_args_t *pargs, pcl_state_t *pcs)
{       
    pcs->character_code = uint_arg(pargs);
    return 0;
}

static int /* ESC ( s <count> W */
pcl_character_data(pcl_args_t *pargs, pcl_state_t *pcs)
{       
    uint count = uint_arg(pargs);
    uint font_data_size = count;
    const byte *data = arg_data(pargs);
    void *value;
    pl_font_t *plfont;
    pcl_font_header_format_t format;
    byte *char_data = 0;
    int code;

    if ( !pl_dict_find_no_stack(&pcs->soft_fonts, current_font_id,
                                current_font_id_size, &value) )
        return 0;               /* font not found */

    plfont = ((pl_font_t *)value);

    if ( count < 4 || data[2] > count - 2 )
        return e_Range;
    if ( data[1] ) /* continuation */ {
        /* Check that we are continuing data - we know this is the
           case if the previous download character command byte count
           is smaller than the data indicated calculating the space
           for the glyph */
        if ( (pcs->soft_font_char_data == 0) )
            return e_Range;
        /* NB we only enable this for uncompressed bitmap
           characters for now, since we don't have real world
           examples for the other font file formats.  */
        if ( data[0] != pccd_bitmap && data[3] != 1 ) {
            dprintf("continuation not implemented for this font type\n");
            return e_Unimplemented;
        }
        /* append the new data to the new object */
        memcpy(pcs->soft_font_char_data + pcs->soft_font_count, data + 2, count - 2);
        /* update the continuation count */
        pcs->soft_font_count += (count -2);
        return 0;
    } else {
        pcs->soft_font_count = 0;
        pcs->soft_font_char_data = 0;
    }
    format = (pcl_font_header_format_t)
        ((const pcl_font_header_t *)plfont->header)->HeaderFormat;
    switch ( data[0] )
        {
        case pccd_bitmap:
            { uint width, height;
            if ( data[2] != 14 ||
                 (format != pcfh_bitmap &&
                  format != pcfh_resolution_bitmap &&
                  format != pcfh_truetype_large)
                 )
                return e_Range;

            width = pl_get_uint16(data + 10);
            if (width < 1  || width > 16384)
                return e_Range;
            height = pl_get_uint16(data + 12);
            if (height < 1 || height > 16384)
                return e_Range;

            switch ( data[3] ) {
            case 1:             /* uncompressed bitmap */
                font_data_size = 16 + (((width + 7) >> 3) * height);
                break;
            case 2:             /* compressed bitmap */
                { uint y = 0;
                const byte *src = data + 16;
                const byte *end = data + count;
                uint width_bytes = (width + 7) >> 3;
                byte *row;

                char_data = gs_alloc_bytes(pcs->memory, 16 + width_bytes * height,
                                           "pcl_character_data(compressed bitmap)");
                if ( char_data == 0 )
                    return_error(e_Memory);
                memcpy(char_data, data, 16);
                memset(char_data + 16, 0, width_bytes * height);
                row = char_data + 16;
                while ( src < end && y < height ) { /* Read the next compressed row. */
                    uint x;
                    int color = 0;
                    uint reps = *src++;
                    for ( x = 0; src < end && x < width; color ^= 1 ) { /* Read the next run. */
                        uint rlen = *src++;

                        if ( rlen > width - x )
                            return e_Range;  /* row overrun */
                        if ( color ) { /* Set the run to black. */
                            char *data = (char *)row;
                            while ( rlen-- ) {
                                data[x >> 3] |= (128 >> (x & 7));
                                x++;
                            }
                        }
                        else
                            x += rlen;
                    }
                    row += width_bytes;
                    ++y;
                    /* Replicate the row if needed. */
                    for ( ; reps > 0 && y < height;
                          --reps, ++y, row += width_bytes
                          )
                        memcpy(row, row - width_bytes, width_bytes);
                }
                } break;
            default:
                return e_Range;
            }
            }
            break;
        case pccd_intellifont:
            if ( data[2] != 2 ||
                 (format != pcfh_intellifont_bound &&
                  format != pcfh_intellifont_unbound)
                 )
                return e_Range;
            switch ( data[3] )
                {
                case 3:         /* non-compound character */
                    /* See TRM Figure 11-16 (p. 11-67) for the following. */
                    if ( count < 14 )
                        return e_Range;
                    { 
                        uint data_size = pl_get_uint16(data + 4);
                        uint contour_offset = pl_get_uint16(data + 6);
                        uint metric_offset = pl_get_uint16(data + 8);
                        uint outline_offset = pl_get_uint16(data + 10);
                        uint xy_offset = pl_get_uint16(data + 12);
                        /* The contour data excludes 4 initial bytes of header */
                        /* and 2 final bytes of padding/checksum. */
                        if ( data_size != count - 6 ||
                             contour_offset < 10 ||
                             metric_offset < contour_offset ||
                             outline_offset < metric_offset ||
                             xy_offset < outline_offset ||
                             xy_offset > count - 6
                             )
                            return e_Range;
                    }
                    break;
                case 4:         /* compound character */
                    /* See TRM Figure 11-18 (p. 11-68) and 11-19 (p. 11-73) */
                    /* for the following. */
                    if ( count < 8 )
                        return e_Range;
                    { /*
                       * TRM Figure 11-18 is wrong: the number of components
                       * is a byte, not a 16-bit quantity.  (It is identified
                       * correctly as a UB on p. 11-70, however.)
                       */
                        uint num_components = data[6];
                        if ( count != 8 + num_components * 6 + 2 )
                            return e_Range;
                    }
                    break;
                default:
                    return e_Range;
                }
            break;
        case pccd_truetype:
            if ( format != pcfh_truetype && format != pcfh_truetype_large )
                return e_Range;
            break;
        default:
            return e_Range;
        }
    /* Register the character. */
    /**** FREE PREVIOUS DEFINITION ****/
    /* Compressed bitmaps have already allocated and filled in */
    /* the character data structure. */
    if ( char_data == 0 ) { 
        char_data = gs_alloc_bytes(pcs->memory, font_data_size,
                                   "pcl_character_data");
        if ( char_data == 0 )
            return_error(e_Memory);
        memset(char_data, 0, font_data_size);
        /* if count > font_data_size extra data is ignored */
        memcpy(char_data, data, min(count, font_data_size) );
        /* NB we only handle continuation for uncompressed bitmap characters */
        if ( data[0] == pccd_bitmap && 
             data[3] == 1 &&
             font_data_size > count /* expecting continuation */
             ) {
            pcs->soft_font_char_data = char_data;
            pcs->soft_font_count = count;
        } else {
            pcs->soft_font_char_data = 0;
            pcs->soft_font_count = 0;
        }
    }
    /* get and set the orientation field */
    {
        pcl_font_header_t *header = (pcl_font_header_t *)plfont->header;
        plfont->orient = header->Orientation;
    }
    code = pl_font_add_glyph(plfont, pcs->character_code, char_data);
    if (code < 0)
        return code;
#ifdef DISABLE_USE_MY_METRICS
    if (data[0] == pccd_truetype)
        code = pl_font_disable_composite_metrics(plfont, pcs->character_code);
#endif /* DISABLE_USE_MY_METRICS */
    return code;
#undef plfont
}

/* template for casting data to command */
typedef struct alphanumeric_data_s {
  byte operation;
  byte string_id[512];
} alphanumeric_data_t;

typedef enum resource_type_enum { 
    macro_resource,
    font_resource
} resource_type_t;
    
/* look up a macro in the macro dictionary, if it is not found search
   on disk and add the macro to the macro dictionary */
static void *
pcl_find_resource(pcl_state_t *pcs,
                  const byte string_id[],
                  int string_id_size,
                  resource_type_t resource_type
                  )
{
    pl_dict_t *dict = (resource_type == macro_resource ? 
                       &pcs->macros :
                       &pcs->soft_fonts);
    void *value = NULL;
f:  if ( pl_dict_find(dict,
                      string_id,
                      string_id_size,
                      &value) )
        return value;
    {
        /* max alpha name + NULL */
        char alphaname[512 + 1];
        long int size;
        int c, code;
        for ( c = 0; c < string_id_size; c++ )
            alphaname[c] = string_id[c];
        alphaname[c] = '\0';
        size = pjl_proc_get_named_resource_size(pcs->pjls, alphaname);
        if ( size == 0 )
            return NULL;
        /* allocate enough space for the macro data and the header
           which indicates the storage type */
        value = gs_alloc_bytes(pcs->memory,
                               size + sizeof(pcl_macro_t),
                               "disk macro");
        if ( value == NULL )
            return NULL;
        ((pcl_macro_t *)value)->storage = pcds_permanent;
        if ( pjl_proc_get_named_resource(pcs->pjls, alphaname,
                (byte *)value + sizeof(pcl_macro_t) ) < 0 )
            return NULL;
        code = pl_dict_put(dict,
                           string_id,
                           string_id_size,
                           value);
        if ( code < 0 )
            return NULL;
        /* now find the entry just placed in the dictionary. should not fail */
        goto f;
    }
    return value;
}

static int /* ESC & n <count> W [operation][string ID] */
pcl_alphanumeric_id_data(pcl_args_t *pargs, pcl_state_t *pcs)
{
        uint count = uint_arg(pargs);
        const alphanumeric_data_t *alpha_data = 
          (const alphanumeric_data_t *)arg_data(pargs);
        int string_id_size = (count - 1); /* size of id data size - operation size */
        if ( count == 0 ) 
          return 0;
        if ( count < 1 || count > 512 ) 
          return e_Range;
        switch ( alpha_data->operation )
          {
          case 0:
            /* Set the current font id to the given string id. */
            {
              char *new_id = (char *)gs_alloc_bytes(pcs->memory, string_id_size,
                                            "pcl_alphanumeric_id_data");
              if ( new_id == 0 ) 
                return_error(e_Memory);
              /* release the previous id, if necessary */
              if ( pcs->alpha_font_id.id )
                gs_free_object( pcs->memory,
                                pcs->alpha_font_id.id,
                                "pcl_free_string_id" );
              /* copy in the new id from the data */
              memcpy(new_id, alpha_data->string_id, string_id_size);
              /* set new id and size */
              pcs->alpha_font_id.id = (byte *)new_id;
              pcs->alpha_font_id.size = string_id_size;
              /* now set up the state to use string id's */
              pcs->font_id_type = string_id;
            }
          break;
          case 1:
            {
              /* Associates the current font's font id to the font
                 with the string id.  We simply create an alias entry
                 for the current font id entry.  HAS - FIXME. ... */
              void *value;
              if ( !pl_dict_find_no_stack(&pcs->soft_fonts,
                                          alpha_data->string_id,
                                          string_id_size,
                                          &value) )
                return 0;
              pl_dict_put_synonym(&pcs->soft_fonts, alpha_data->string_id,
                               string_id_size, current_font_id,
                               current_font_id_size);
            }
          break;
          case 2:
            {
              /* Select the fonts referred to by the String ID as
                 primary.  Same as font id selection but uses the
                 string key instead of a numerical key */
              void *value;
              pcl_font_selection_t *pfs = &pcs->font_selection[primary];
              if ( !pl_dict_find_no_stack(&pcs->soft_fonts,
                                          alpha_data->string_id,
                                          string_id_size,
                                          &value) )
                return 1;
              /* NB wrong */
              pcl_set_id_parameters(pcs, pfs, (pl_font_t *)value, 0);
              pcl_decache_font(pcs, -1);
            }
          break;
          case 3:
            {
              /* same as case 2 but sets secondary font */
              void *value;
              pcl_font_selection_t *pfs = &pcs->font_selection[secondary];
              if ( !pl_dict_find_no_stack(&pcs->soft_fonts,
                                          alpha_data->string_id,
                                          string_id_size,
                                          &value) )
                  return 1;
              /* NB wrong */
              pcl_set_id_parameters(pcs, pfs, (pl_font_t *)value, 0);
              pcl_decache_font(pcs, -1);
            }
          break;
          case 4:
            {
              /* sets the current macro id to the string id */
              char *new_id = (char *)gs_alloc_bytes(pcs->memory, string_id_size,
                                            "pcl_alphanumeric_id_data");
              if ( new_id == 0 ) 
                return_error(e_Memory);
              /* release the previous id, if necessary */
              if ( pcs->alpha_macro_id.id )
                gs_free_object( pcs->memory,
                                pcs->alpha_macro_id.id,
                                "pcl_free_string_id" );
              /* copy in the new id from the data */
              memcpy(new_id, alpha_data->string_id, string_id_size);
              /* set new id and size */
              pcs->alpha_macro_id.id = (byte *)new_id;
              pcs->alpha_macro_id.size = string_id_size;
              /* now set up the state to use string id's */
              pcs->macro_id_type = string_id;
            }
            break;
          case 5:
            /* associates current macro id to the supplied string id */
            {
              void *value;
              value = pcl_find_resource(pcs, alpha_data->string_id, string_id_size, macro_resource);
              if ( !value )
                  return 0;
              pl_dict_put_synonym(&pcs->macros, alpha_data->string_id, string_id_size,
                                  current_macro_id, current_macro_id_size);
            }
            break;
          case 20:
            /* deletes the font association named by the current Font ID */
            if ( pcs->font_id_type == string_id )
              pcl_delete_soft_font(pcs, current_font_id, current_font_id_size, NULL);
            break;
          case 21:
            /* deletes the macro association named the the current macro id */
            if ( pcs->macro_id_type == string_id )
              pl_dict_undef(&pcs->macros, current_macro_id, current_macro_id_size);
            break;
          case 100:
            /* media select */

            /* this is not sufficiently specified in the PCL
               comparison guide and interacts with the control panel
               so we do not implement it completely.  We have verified
               the following occurs: */

              pcl_end_page_if_marked(pcs);
              pcl_home_cursor(pcs);
          default:
            return e_Range;
          }
        return 0;
}
        
/* Initialization */
static int
pcsfont_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{               /* Register commands */
        DEFINE_CLASS('*')
          {'c', 'D',
             PCL_COMMAND("Assign Font ID", pcl_assign_font_id,
                         pca_neg_error|pca_big_error)},
          {'c', 'F',
             PCL_COMMAND("Font Control", pcl_font_control,
                         pca_neg_error|pca_big_error)},
        END_CLASS
        DEFINE_CLASS_COMMAND_ARGS(')', 's', 'W', "Font Header",
                                  pcl_font_header, pca_bytes)
        DEFINE_CLASS_COMMAND_ARGS('*', 'c', 'E', "Character Code",
                                  pcl_character_code, pca_neg_error|pca_big_ok)
        DEFINE_CLASS_COMMAND_ARGS( '(', 's', 'W', "Character Data",
                                  pcl_character_data, pca_bytes)
        DEFINE_CLASS_COMMAND_ARGS('&', 'n', 'W', "Alphanumeric ID Data",
                                  pcl_alphanumeric_id_data, pca_bytes)
        return 0;
}
static void
pcsfont_do_reset(pcl_state_t *pcs, pcl_reset_type_t type)
{       
    if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) ) { 
            pcs->soft_font_char_data = 0;
            pcs->soft_font_count = 0;
            id_set_value(pcs->font_id, 0);
            pcs->character_code = 0;
            pcs->font_id_type = numeric_id;
            if ( (type & pcl_reset_printer) != 0 ) { 
                pcl_args_t args;
                arg_set_uint(&args, 1); /* delete temporary fonts */
                pcl_font_control(&args, pcs);
                if ( pcs->alpha_font_id.id != 0 )
                  gs_free_object(pcs->memory,
                                 pcs->alpha_font_id.id,
                                 "pcsfont_do_reset");
              }
            pcs->alpha_font_id.id = 0;
    }
}
static int
pcsfont_do_copy(pcl_state_t *psaved, const pcl_state_t *pcs,
  pcl_copy_operation_t operation)
{       if ( operation & pcl_copy_after )
          { /* Don't restore the soft font set. */
            /**** MUST HANDLE POSSIBILITY THAT CURRENT FONT WAS DELETED. ****/
            psaved->soft_fonts = pcs->soft_fonts;
          }
        return 0;
}
const pcl_init_t pcsfont_init = {
  pcsfont_do_registration, pcsfont_do_reset, pcsfont_do_copy
};
