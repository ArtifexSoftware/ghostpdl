/* Copyright (C) 2001-2018 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*$Id: gdevtxtw.c 7795 2007-03-23 13:56:11Z tim $ */
/* Device for Unicode (UTF-8 or UCS2) text extraction */
#include "memory_.h"
#include "string_.h"
#include "gp.h"			/* for gp_file_name_sizeof */
#include "gx.h"
#include "gserrors.h"
#include "gsparam.h"
#include "gsutil.h"
#include "gxdevice.h"
#include "gsdevice.h"		/* requires gsmatrix.h */
#include "gxfont.h"
#include "gxfont0.h"
#include "gstext.h"
#include "gxfcid.h"
#include "gxgstate.h"
#include "gxpath.h"
#include "gdevagl.h"
#include "gxdevsop.h"
#include "gzpath.h"
#include "gdevkrnlsclass.h" /* 'standard' built in subclasses, currently First/Last Page and obejct filter */

/* #define TRACE_TXTWRITE 1 */

extern single_glyph_list_t *SingleGlyphList;
extern double_glyph_list_t *DoubleGlyphList;
extern treble_glyph_list_t *TrebleGlyphList;
extern quad_glyph_list_t *QuadGlyphList;
/*
 * Define the structure used to return glyph width information.  Note that
 * there are two different sets of width information: real-number (x,y)
 * values, which give the true advance width, and an integer value, which
 * gives an X advance width for WMode = 0 or a Y advance width for WMode = 1.
 * The return value from txt_glyph_width() indicates which of these is/are
 * valid.
 */
typedef struct txt_glyph_width_s {
    double w;
    gs_point xy;
    gs_point v;				/* glyph origin shift */
} txt_glyph_width_t;
typedef struct txt_glyph_widths_s {
    txt_glyph_width_t Width;		/* unmodified, for Widths */
    txt_glyph_width_t real_width;	/* possibly modified, for rendering */
    bool replaced_v;
} txt_glyph_widths_t;

/* Structure to record the Unicode characters, the total width of the text
 * recorded, and various useful attributes such as the font, size, colour
 * rendering mode, writing mode etc. These are stored as a series of x-ordered
 * entries in a list, using the starting x co-ordinate.
 */
typedef struct text_list_entry_s {
    struct text_list_entry_s *previous;
    struct text_list_entry_s *next;

    gs_point start;
    gs_point end;
    gs_point FontBBox_bottomleft, FontBBox_topright;
    float *Widths;
    unsigned short *Unicode_Text;
    int Unicode_Text_Size;
    int render_mode;

    gs_matrix matrix;		/* Tm et al */

    gs_font *font;
    char *FontName;
    int wmode;			/* WMode of font */
    double PaintType0Width;
    double size;
} text_list_entry_t;

/* Structure to maintain a list of text fragments, ordered by X co-ordinate.
 * These structures are themselves maintained in a Y-ordered list.
 */
typedef struct page_text_list_s {
    struct page_text_list_s *previous;
    struct page_text_list_s *next;
    gs_point start;
    float MinY, MaxY;
    text_list_entry_t *x_ordered_list;
} page_text_list_t;

/* A simple structure to maintain the lists of text fragments, it is also
 * a convenient place to record the page number and anything else we may
 * want to record that is relevant to the page rather than the text.
 */
typedef struct page_text_s {
    int PageNum;
    page_text_list_t *y_ordered_list;
    text_list_entry_t *unsorted_text_list;
} page_text_t;

/* The custom sub-classed device structure */
typedef struct gx_device_txtwrite_s {
    gx_device_common;
    page_text_t PageData;
    char fname[gp_file_name_sizeof];	/* OutputFile */
    FILE *file;
    int TextFormat;
#ifdef TRACE_TXTWRITE
    FILE *DebugFile;
#endif
} gx_device_txtwrite_t;

/* Device procedures */
static dev_proc_open_device(txtwrite_open_device);
static dev_proc_close_device(txtwrite_close_device);
static dev_proc_output_page(txtwrite_output_page);
static dev_proc_fill_rectangle(txtwrite_fill_rectangle);
static dev_proc_get_params(txtwrite_get_params);
static dev_proc_put_params(txtwrite_put_params);
static dev_proc_fill_path(txtwrite_fill_path);
static dev_proc_stroke_path(txtwrite_stroke_path);
static dev_proc_text_begin(txtwrite_text_begin);
static dev_proc_strip_copy_rop(txtwrite_strip_copy_rop);
static dev_proc_dev_spec_op(txtwrite_dev_spec_op);


/* The device prototype */
#define X_DPI 72
#define Y_DPI 72

/* Define the text enumerator. */
typedef struct textw_text_enum_s {
    gs_text_enum_common;
    bool charproc_accum;
    bool cdevproc_callout;
    double cdevproc_result[10];
    float *Widths;
    unsigned short *TextBuffer;
    int TextBufferIndex;
    text_list_entry_t *text_state;
} textw_text_enum_t;
#define private_st_textw_text_enum()\
  extern_st(st_gs_text_enum);\
  gs_private_st_suffix_add0(st_textw_text_enum, textw_text_enum_t,\
    "textw_text_enum_t", textw_text_enum_enum_ptrs, textw_text_enum_reloc_ptrs,\
    st_gs_text_enum)

private_st_textw_text_enum();

const gx_device_txtwrite_t gs_txtwrite_device =
{
    /* Define the device as 8-bit gray scale to avoid computing halftones. */
    std_device_dci_body(gx_device_txtwrite_t, 0, "txtwrite",
                        DEFAULT_WIDTH_10THS * X_DPI / 10,
                        DEFAULT_HEIGHT_10THS * Y_DPI / 10,
                        X_DPI, Y_DPI,
                        1, 8, 255, 0, 256, 1),
    {txtwrite_open_device,
     NULL, /*gx_upright_get_initial_matrix,*/
     NULL, /*gx_default_sync_output,*/
     txtwrite_output_page,
     txtwrite_close_device,
     NULL, /*gx_default_gray_map_rgb_color,*/
     NULL, /*gx_default_gray_map_color_rgb,*/
     txtwrite_fill_rectangle,               /* Can't be NULL and there is no gx_default_fill_rectangle! */
     NULL, /*gx_default_tile_rectangle,*/
     NULL, /*gx_default_copy_mono,*/
     NULL, /*gx_default_copy_color,*/
     NULL, /*gx_default_draw_line,*/
     NULL, /*gx_default_get_bits,*/
     txtwrite_get_params,
     txtwrite_put_params,
     NULL, /*gx_default_map_cmyk_color,*/
     NULL, /*gx_default_get_xfont_procs,*/
     NULL, /*gx_default_get_xfont_device,*/
     NULL, /*gx_default_map_rgb_alpha_color,*/
     gx_page_device_get_page_device, /*gx_page_device_get_page_device,*/
     NULL,			/* get_alpha_bits */
     NULL, /*gx_default_copy_alpha,*/
     NULL,			/* get_band */
     NULL,			/* copy_rop */
     txtwrite_fill_path,
     txtwrite_stroke_path,
     NULL, /*gx_default_fill_mask,*/
     NULL, /*gx_default_fill_trapezoid,*/
     NULL, /*gx_default_fill_parallelogram,*/
     NULL, /*gx_default_fill_triangle,*/
     NULL, /*gx_default_draw_thin_line,*/
     NULL,                      /* begin image */
     NULL,			/* image_data */
     NULL,			/* end_image */
     NULL, /*gx_default_strip_tile_rectangle,*/
     txtwrite_strip_copy_rop,
     NULL,			/* get_clipping_box */
     NULL, /* txtwrite_begin_typed_image */
     NULL,			/* get_bits_rectangle */
     NULL, /*gx_default_map_color_rgb_alpha,*/
     gx_null_create_compositor,
     NULL,			/* get_hardware_params */
     txtwrite_text_begin,
     NULL,			/* finish_copydevice */
     NULL,			/* begin_transparency_group */
     NULL,			/* end_transparency_group */
     NULL,			/* begin_transparency_mask */
     NULL,			/* end_transparency_mask */
     NULL,			/* discard_transparency_layer */
     NULL,			/* get_color_mapping_procs */
     NULL,			/* get_color_comp_index */
     NULL,			/* encode_color */
     NULL,			/* decode_color */
     NULL,                      /* pattern manager */
     NULL,                      /* fill_rectangle_hl_color */
     NULL,                      /* include_color_space */
     NULL,                      /* fill_linear_color_scanline */
     NULL,                      /* fill_linear_color_trapezoid */
     NULL,                      /* fill_linear_color_triangle */
     NULL,                      /* update_spot_equivalent_colors */
     NULL,                      /* ret_devn_params */
     NULL,                      /* fillpage */
     NULL,                      /* push_transparency_state */
     NULL,                      /* pop_transparency_state */
     NULL,                      /* put_image */
     txtwrite_dev_spec_op,      /* dev_spec_op */
     NULL,                      /* copy_planes */
     NULL,                      /* get_profile */
     NULL,                      /* set_graphics_type_tag */
     NULL,                      /* strip_copy_rop2 */
     NULL                       /* strip_tile_rect_devn */
    },
    { 0 },			/* Page Data */
    { 0 },			/* Output Filename */
    0,				/* Output FILE * */
    3				/* TextFormat */
};

#ifndef gx_device_textw_DEFINED
#  define gx_device_textw_DEFINED
typedef struct gx_device_textwrite_s gx_device_textw;
#endif

static const gs_param_item_t txt_param_items[] = {
#define pi(key, type, memb) { key, type, offset_of(gx_device_txtwrite_t, memb) }
    pi("TextFormat", gs_param_type_int, TextFormat),
#undef pi
    gs_param_item_end
};

/* ---------------- Open/close/page ---------------- */

static int
txtwrite_open_device(gx_device * dev)
{
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;
    int code = 0;

    gx_device_fill_in_procs(dev);
    if (tdev->fname[0] == 0)
        return_error(gs_error_undefinedfilename);

    tdev->PageData.PageNum = 0;
    tdev->PageData.y_ordered_list = NULL;
    tdev->file = NULL;
#ifdef TRACE_TXTWRITE
    tdev->DebugFile = gp_fopen("/temp/txtw_dbg.txt", "wb+");
#endif

    code = install_internal_subclass_devices((gx_device **)&dev, NULL);
    return code;
}

static int
txtwrite_close_device(gx_device * dev)
{
    int code = 0;
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;

    if (tdev->file) {
        code = gx_device_close_output_file(dev, tdev->fname, tdev->file);
        tdev->file = 0;
    }

#ifdef TRACE_TXTWRITE
    fclose(tdev->DebugFile);
#endif
    return code;
}

/* Routine inspects horizontal lines of text to see if they can be collapsed
 * into a single line. This essentially detects superscripts and subscripts
 * as well as lines which are slightly mis-aligned.
 */
static int merge_vertically(gx_device_txtwrite_t *tdev)
{
#ifdef TRACE_TXTWRITE
    text_list_entry_t *debug_x;
#endif
    page_text_list_t *y_list = tdev->PageData.y_ordered_list;

    while (y_list && y_list->next) {
        page_text_list_t *next = y_list->next;
        bool collision = false;
        float overlap = (y_list->start.y + y_list->MaxY) - (next->start.y + next->MinY);

        if (overlap >= (y_list->MaxY - y_list->MinY) / 4) {
            /* At least a 25% overlap, lets test for x collisions */
            text_list_entry_t *upper = y_list->x_ordered_list, *lower;
            while (upper && !collision) {
                lower = next->x_ordered_list;
                while (lower && !collision) {
                    if (upper->start.x >= lower->start.x) {
                        if (upper->start.x <= lower->end.x) {
                            /* Collision */
                            collision = true;
                            break;
                        }
                    } else {
                        if (upper->end.x > lower->start.x) {
                            /* Collision */
                            collision = true;
                            break;
                        }
                    }
                    lower = lower->next;
                }
                upper = upper->next;
            }
            if (!collision) {
                text_list_entry_t *from, *to, *new_order, *current;
                /* Consolidate y lists */
                to = y_list->x_ordered_list;
                from = next->x_ordered_list;
#ifdef TRACE_TXTWRITE
                fprintf(tdev->DebugFile, "\nConsolidating two horizontal lines, line 1:");
                debug_x = from;
                while (debug_x) {
                    fprintf(tdev->DebugFile, "\n\t");
                    fwrite(debug_x->Unicode_Text, sizeof(unsigned short), debug_x->Unicode_Text_Size, tdev->DebugFile);
                    debug_x = debug_x->next;
                }
                fprintf(tdev->DebugFile, "\nConsolidating two horizontal lines, line 2");
                debug_x = to;
                while (debug_x) {
                    fprintf(tdev->DebugFile, "\n\t");
                    fwrite(debug_x->Unicode_Text, sizeof(unsigned short), debug_x->Unicode_Text_Size, tdev->DebugFile);
                    debug_x = debug_x->next;
                }
#endif
                if (from->start.x < to->start.x) {
                    current = new_order = from;
                    from = from->next;
                } else {
                    current = new_order = to;
                    to = to->next;
                }
                while (to && from) {
                    if (to->start.x < from->start.x) {
                        current->next = to;
                        to->previous = current;
                        to = to->next;
                    } else {
                        current->next = from;
                        from->previous = current;
                        from = from->next;
                    }
                    current = current->next;
                }
                if (to) {
                    to->previous = current;
                    current->next = to;
                } else {
                    if (from) {
                        from->previous = current;
                        current->next = from;
                    }
                }
                y_list->x_ordered_list = new_order;
#ifdef TRACE_TXTWRITE
                fprintf(tdev->DebugFile, "\nAfter:");
                debug_x = new_order;
                while (debug_x) {
                    fprintf(tdev->DebugFile, "\n\t");
                    fwrite(debug_x->Unicode_Text, sizeof(unsigned short), debug_x->Unicode_Text_Size, tdev->DebugFile);
                    debug_x = debug_x->next;
                }
                fprintf(tdev->DebugFile, "\n");
#endif
                y_list->next = next->next;
                if (next->next)
                    next->next->previous = y_list;
                gs_free(tdev->memory, next, 1, sizeof(page_text_list_entry_t), "txtwrite free text list");
            } else
                y_list = next;
        } else
            y_list = next;
    }
    return 0;
}

/* Routine to merge horizontally adjacent text fragments. If the distance
 * between two horizontal fragments is small, then they are treated as one
 * frament of text, if its larger then we insert a space (and set the Width
 * entry appropriately). Otherwise we leave them as separate.
 */
static int merge_horizontally(gx_device_txtwrite_t *tdev)
{
#ifdef TRACE_TXTWRITE
    text_list_entry_t *debug_x;
#endif
    unsigned short UnicodeSpace = 0x20;
    page_text_list_t *y_list = tdev->PageData.y_ordered_list;

    while (y_list) {
        float average_width;
        text_list_entry_t *from, *to;
        from = y_list->x_ordered_list;
        to = from->next;

        while (from && to) {
            average_width = (from->end.x - from->start.x) / from->Unicode_Text_Size;

            if (to->start.x - from->end.x < average_width / 2) {
                /* consolidate fragments */
                unsigned short *NewText;
                float *NewWidths;

                NewText = (unsigned short *)gs_malloc(tdev->memory->stable_memory,
                    (from->Unicode_Text_Size + to->Unicode_Text_Size), sizeof(unsigned short), "txtwrite alloc working text buffer");
                NewWidths = (float *)gs_malloc(tdev->memory->stable_memory,
                    (from->Unicode_Text_Size + to->Unicode_Text_Size), sizeof(float), "txtwrite alloc Widths array");
                if (!NewText || !NewWidths) {
                    if (NewText)
                        gs_free(tdev->memory, NewText, from->Unicode_Text_Size + to->Unicode_Text_Size, sizeof (unsigned short), "free working text fragment");
                    /* ran out of memory, don't consolidate */
                    from = from->next;
                    to = to->next;
                } else {
#ifdef TRACE_TXTWRITE
                    fprintf(tdev->DebugFile, "Consolidating two horizontal fragments in one line, before:\n\t");
                    fwrite(from->Unicode_Text, sizeof(unsigned short), from->Unicode_Text_Size, tdev->DebugFile);
                    fprintf(tdev->DebugFile, "\n\t");
                    fwrite(to->Unicode_Text, sizeof(unsigned short), to->Unicode_Text_Size, tdev->DebugFile);
#endif
                    memcpy(NewText, from->Unicode_Text, from->Unicode_Text_Size * sizeof(unsigned short));
                    memcpy(&NewText[from->Unicode_Text_Size], to->Unicode_Text, to->Unicode_Text_Size * sizeof(unsigned short));
                    memcpy(NewWidths, from->Widths, from->Unicode_Text_Size * sizeof(float));
                    memcpy(&NewWidths[from->Unicode_Text_Size], to->Widths, to->Unicode_Text_Size * sizeof(float));
                    gs_free(tdev->memory, from->Unicode_Text, from->Unicode_Text_Size, sizeof (unsigned short), "free consolidated text fragment");
                    gs_free(tdev->memory, to->Unicode_Text, to->Unicode_Text_Size, sizeof (unsigned short), "free consolidated text fragment");
                    gs_free(tdev->memory, from->Widths, from->Unicode_Text_Size, sizeof (float), "free consolidated Widths array");
                    gs_free(tdev->memory, to->Widths, to->Unicode_Text_Size, sizeof (float), "free consolidated Widths array");
                    gs_free(tdev->memory, to->FontName, 1, strlen(from->FontName) + 1, "free FontName");
                    from->Unicode_Text = NewText;
                    from->Unicode_Text_Size += to->Unicode_Text_Size;
                    from->Widths = NewWidths;
#ifdef TRACE_TXTWRITE
                    fprintf(tdev->DebugFile, "After:\n\t");
                    fwrite(from->Unicode_Text, sizeof(unsigned short), from->Unicode_Text_Size, tdev->DebugFile);
#endif
                    from->end = to->end;
                    from->next = to->next;
                    if (from->next)
                        from->next->previous = from;
                    gs_free(tdev->memory, to, 1, sizeof(text_list_entry_t), "free consolidated fragment");
                    to = from->next;
                }
            } else {
                if (to->start.x - from->end.x < average_width *2){
                    unsigned short *NewText;
                    float *NewWidths;

                    NewText = (unsigned short *)gs_malloc(tdev->memory->stable_memory,
                        (from->Unicode_Text_Size + to->Unicode_Text_Size + 1), sizeof(unsigned short), "txtwrite alloc text state");
                    NewWidths = (float *)gs_malloc(tdev->memory->stable_memory,
                        (from->Unicode_Text_Size + to->Unicode_Text_Size + 1), sizeof(float), "txtwrite alloc Widths array");
                    if (!NewText || !NewWidths) {
                        if (NewText)
                            gs_free(tdev->memory, NewText, from->Unicode_Text_Size + to->Unicode_Text_Size, sizeof (unsigned short), "free working text fragment");
                        /* ran out of memory, don't consolidate */
                        from = from->next;
                        to = to->next;
                    } else {
                        memcpy(NewText, from->Unicode_Text, from->Unicode_Text_Size * sizeof(unsigned short));
                        memcpy(&NewText[from->Unicode_Text_Size], &UnicodeSpace, sizeof(unsigned short));
                        memcpy(&NewText[from->Unicode_Text_Size + 1], to->Unicode_Text, to->Unicode_Text_Size * sizeof(unsigned short));
                        memcpy(NewWidths, from->Widths, from->Unicode_Text_Size * sizeof(float));
                        NewWidths[from->Unicode_Text_Size] = to->start.x - from->end.x;
                        memcpy(&NewWidths[from->Unicode_Text_Size + 1], to->Widths, to->Unicode_Text_Size * sizeof(float));
                        gs_free(tdev->memory, from->Unicode_Text, from->Unicode_Text_Size, sizeof (unsigned short), "free consolidated text fragment");
                        gs_free(tdev->memory, to->Unicode_Text, to->Unicode_Text_Size, sizeof (unsigned short), "free consolidated text fragment");
                        gs_free(tdev->memory, from->Widths, from->Unicode_Text_Size, sizeof (float), "free consolidated Widths array");
                        gs_free(tdev->memory, to->Widths, to->Unicode_Text_Size, sizeof (float), "free consolidated Widths array");
                        gs_free(tdev->memory, to->FontName, 1, strlen(from->FontName) + 1, "free FontName");
                        from->Unicode_Text = NewText;
                        from->Unicode_Text_Size += to->Unicode_Text_Size + 1;
                        from->Widths = NewWidths;
                        from->end = to->end;
                        from->next = to->next;
                        if (from->next)
                            from->next->previous = from;
                        gs_free(tdev->memory, to, 1, sizeof(text_list_entry_t), "free consolidated fragment");
                        to = from->next;
                    }
                } else {
                    from = from->next;
                    to = to->next;
                }
            }
        }
        y_list = y_list->next;
    }
    return 0;
}

static int write_simple_text(unsigned short *text, int count, gx_device_txtwrite_t *tdev)
{
    switch(tdev->TextFormat) {
        case 2:
            fwrite(text, sizeof (unsigned short), count, tdev->file);
            break;
        case 3:
            {
                int i;
                unsigned short *UTF16 = (unsigned short *)text;
                unsigned char UTF8[3];

                for (i=0;i<count;i++) {
                    if (*UTF16 < 0x80) {
                        UTF8[0] = *UTF16 & 0xff;
                        fwrite (UTF8, sizeof(unsigned char), 1, tdev->file);
                    } else {
                        if (*UTF16 < 0x800) {
                            UTF8[0] = (*UTF16 >> 6) + 0xC0;
                            UTF8[1] = (*UTF16 & 0x3F) + 0x80;
                            fwrite (UTF8, sizeof(unsigned char), 2, tdev->file);
                        } else {
                            UTF8[0] = (*UTF16 >> 12) + 0xE0;
                            UTF8[1] = ((*UTF16 >> 6) & 0x3F) + 0x80;
                            UTF8[2] = (*UTF16 & 0x3F) + 0x80;
                            fwrite (UTF8, sizeof(unsigned char), 3, tdev->file);
                        }
                    }
                    UTF16++;
                }
            }
            break;
        default:
            return gs_note_error(gs_error_rangecheck);
            break;
    }
    return 0;
}

static int simple_text_output(gx_device_txtwrite_t *tdev)
{
    int chars_wide;
    float char_size, min_size, min_width_size;
#ifdef TRACE_TXTWRITE
    text_list_entry_t *debug_x;
#endif
    text_list_entry_t * x_entry;
    page_text_list_t *y_list;
    unsigned short UnicodeSpace = 0x20, UnicodeEOL[2] = {0x00D, 0x0a};

    merge_vertically(tdev);

    merge_horizontally(tdev);

    min_size = (float)tdev->width;
    /* Estimate maximum text density */
    y_list = tdev->PageData.y_ordered_list;
    while (y_list) {
        x_entry = y_list->x_ordered_list;
        while (x_entry) {
            if (x_entry->size < min_size)
                min_size = x_entry->size;
            x_entry = x_entry->next;
        }
        y_list = y_list->next;
    }

    min_width_size = min_size;
    y_list = tdev->PageData.y_ordered_list;
    while (y_list) {
        float width;

        x_entry = y_list->x_ordered_list;
        while (x_entry) {
            width = (x_entry->end.x - x_entry->start.x) / x_entry->Unicode_Text_Size;
            if (x_entry->next) {
                /* If we have following text, check to see if *not* using the newly calculated size would result in
                 * the end of the text going past the beginning of the following text. If it does then we must
                 * use the new minimum, regardless of how small it is!
                 */
                if (x_entry->start.x + ((x_entry->Unicode_Text_Size + 1) * min_width_size) > x_entry->next->start.x && width > 0)
                    min_width_size = width;
            } else {
                if (width < min_width_size && width >= (float)min_size * 0.75)
                    min_width_size = width;
            }
            x_entry = x_entry->next;
        }
        y_list = y_list->next;
    }

    min_size = min_width_size;
    chars_wide = (int)ceil(tdev->width / min_size);
    char_size = (float)tdev->width / (float)chars_wide;

    y_list = tdev->PageData.y_ordered_list;
    while (y_list) {
        float xpos = 0;
        x_entry = y_list->x_ordered_list;
        while (x_entry) {
            while (xpos < x_entry->start.x) {
                write_simple_text(&UnicodeSpace, 1, tdev);
                xpos += char_size;
            }
            write_simple_text(x_entry->Unicode_Text, x_entry->Unicode_Text_Size, tdev);
            xpos += x_entry->Unicode_Text_Size * char_size;
            if (x_entry->next) {
                x_entry = x_entry->next;
            } else {
                x_entry = NULL;
            }
        }
        write_simple_text((unsigned short *)&UnicodeEOL, 2, tdev);
        if (y_list->next) {
            y_list = y_list->next;
        } else {
            y_list = NULL;
        }
    }
    return 0;
}

static int escaped_Unicode (unsigned short Unicode, char *Buf)
{
    switch (Unicode)
    {
    case 0x3C: gs_sprintf(Buf, "&lt;"); break;
    case 0x3E: gs_sprintf(Buf, "&gt;"); break;
    case 0x26: gs_sprintf(Buf, "&amp;"); break;
    case 0x22: gs_sprintf(Buf, "&quot;"); break;
    case 0x27: gs_sprintf(Buf, "&apos;"); break;
    default:
        if (Unicode >= 32 && Unicode <= 127)
            gs_sprintf(Buf, "%c", Unicode);
        else
            gs_sprintf(Buf, "&#x%x;", Unicode);
        break;
    }

    return 0;
}

static int decorated_text_output(gx_device_txtwrite_t *tdev)
{
    int i;
    text_list_entry_t * x_entry, *next_x;
    char TextBuffer[512], Escaped[32];
    float xpos;
    page_text_list_t *y_list;
#ifdef TRACE_TXTWRITE
    text_list_entry_t *debug_x;
#endif

    if (tdev->TextFormat == 0) {
        fwrite("<page>\n", sizeof(unsigned char), 7, tdev->file);
        x_entry = tdev->PageData.unsorted_text_list;
        while (x_entry) {
            next_x = x_entry->next;
            gs_sprintf(TextBuffer, "<span bbox=\"%0.0f %0.0f %0.0f %0.0f\" font=\"%s\" size=\"%0.4f\">\n", x_entry->start.x, x_entry->start.y,
                x_entry->end.x, x_entry->end.y, x_entry->FontName,x_entry->size);
            fwrite(TextBuffer, 1, strlen(TextBuffer), tdev->file);
            xpos = x_entry->start.x;
            for (i=0;i<x_entry->Unicode_Text_Size;i++) {
                escaped_Unicode(x_entry->Unicode_Text[i], (char *)&Escaped);
                gs_sprintf(TextBuffer, "<char bbox=\"%0.0f %0.0f %0.0f %0.0f\" c=\"%s\"/>\n", xpos,
                    x_entry->start.y, xpos + x_entry->Widths[i], x_entry->end.y, Escaped);
                fwrite(TextBuffer, 1, strlen(TextBuffer), tdev->file);
                xpos += x_entry->Widths[i];
            }
            fwrite("</span>\n", sizeof(unsigned char), 8, tdev->file);

            x_entry = next_x;
        }
        fwrite("</page>\n", sizeof(unsigned char), 8, tdev->file);
    } else {

        merge_vertically(tdev);
        merge_horizontally(tdev);

        y_list = tdev->PageData.y_ordered_list;
        fwrite("<page>\n", sizeof(unsigned char), 7, tdev->file);
        /* Walk the list looking for 'blocks' */
        do {
            page_text_list_t *temp;
            page_text_t block;
            page_text_list_t *block_line;
            float BBox[4];

            memset(&block, 0x00, sizeof(page_text_t));
            memset(BBox, 0x00, sizeof(float) * 4);

            while (y_list) {
                if (block.y_ordered_list) {
                    text_list_entry_t *x_entry = y_list->x_ordered_list;

                    block_line = block.y_ordered_list;
                    while (x_entry) {
                        if (x_entry->start.x > BBox[2] || x_entry->end.x < BBox[0] ||
                            x_entry->start.y > (BBox[1] + (BBox[3] - BBox[1]))) {
                                ;
                        } else {
                            block_line->next = (page_text_list_t *)gs_malloc(tdev->memory->stable_memory, 1,
                                sizeof(page_text_list_t), "txtwrite alloc Y-list");
                            memset(block_line->next, 0x00, sizeof(page_text_list_t));
                            block_line = block_line->next;
                            block_line->x_ordered_list = x_entry;
                            if(x_entry->next)
                                x_entry->next->previous = x_entry->previous;
                            if (x_entry->previous)
                                x_entry->previous->next = x_entry->next;
                            else {
                                if (x_entry->next == 0x00) {
                                    /* remove Y entry */
                                    temp = y_list->next;
                                    if (y_list->previous)
                                        y_list->previous->next = y_list->next;
                                    if (y_list->next)
                                        y_list->next->previous = y_list->previous;
                                    else {
                                        if (y_list->previous == 0x00) {
                                            tdev->PageData.y_ordered_list = 0x00;
                                        }
                                    }
                                    gs_free(tdev->memory, y_list, 1, sizeof(page_text_list_t), "txtwrite free text list");
                                    if (tdev->PageData.y_ordered_list == y_list)
                                        tdev->PageData.y_ordered_list = temp;
                                    y_list = temp;
                                    x_entry = x_entry->next;
                                    continue;
                                }
                            }
                            if (block_line->x_ordered_list->start.x < BBox[0])
                                BBox[0] = block_line->x_ordered_list->start.x;
                            if (block_line->x_ordered_list->start.y < BBox[1])
                                BBox[1] = block_line->x_ordered_list->start.y;
                            if (block_line->x_ordered_list->end.x < BBox[2])
                                BBox[2] = block_line->x_ordered_list->end.x;
                            if (block_line->x_ordered_list->end.y + block_line->x_ordered_list->FontBBox_topright.y < BBox[3])
                                BBox[3] = block_line->x_ordered_list->end.y + block_line->x_ordered_list->FontBBox_topright.y;
                        }
                        x_entry = x_entry->next;
                    }
                } else {
                    block.y_ordered_list = block_line = (page_text_list_t *)gs_malloc(tdev->memory->stable_memory, 1,
                        sizeof(page_text_list_t), "txtwrite alloc Y-list");
                    memset(block_line, 0x00, sizeof(page_text_list_t));
                    block_line->x_ordered_list = y_list->x_ordered_list;
                    y_list->x_ordered_list = y_list->x_ordered_list->next;
                    if (y_list->x_ordered_list == 0x00) {
                        temp = y_list->next;
                        if (y_list->previous)
                            y_list->previous->next = y_list->next;
                        if (y_list->next)
                            y_list->next->previous = y_list->previous;
                        else {
                            if (y_list->previous == 0x00) {
                                tdev->PageData.y_ordered_list = 0x00;
                            }
                        }
                        gs_free(tdev->memory, y_list, 1, sizeof(page_text_list_t), "txtwrite free text list");
                        if (tdev->PageData.y_ordered_list == y_list)
                            tdev->PageData.y_ordered_list = temp;
                        y_list = temp;
                        continue;
                    }
                    block_line->x_ordered_list->next = block_line->x_ordered_list->previous = 0x00;
                    BBox[0] = block_line->x_ordered_list->start.x;
                    BBox[1] = block_line->x_ordered_list->start.y;
                    BBox[2] = block_line->x_ordered_list->end.x;
                    BBox[3] = block_line->x_ordered_list->end.y + block_line->x_ordered_list->FontBBox_topright.y;
                }
                if (y_list)
                    y_list = y_list->next;
            }
            /* FIXME - need to free the used memory in here */
            fwrite("<block>\n", sizeof(unsigned char), 8, tdev->file);
            block_line = block.y_ordered_list;
            while (block_line) {
                fwrite("<line>\n", sizeof(unsigned char), 7, tdev->file);
                x_entry = block_line->x_ordered_list;
                while(x_entry) {
                    gs_sprintf(TextBuffer, "<span bbox=\"%0.0f %0.0f %0.0f %0.0f\" font=\"%s\" size=\"%0.4f\">\n", x_entry->start.x, x_entry->start.y,
                        x_entry->end.x, x_entry->end.y, x_entry->FontName,x_entry->size);
                    fwrite(TextBuffer, 1, strlen(TextBuffer), tdev->file);
                    xpos = x_entry->start.x;
                    for (i=0;i<x_entry->Unicode_Text_Size;i++) {
                        escaped_Unicode(x_entry->Unicode_Text[i], (char *)&Escaped);
                        gs_sprintf(TextBuffer, "<char bbox=\"%0.0f %0.0f %0.0f %0.0f\" c=\"%s\"/>\n", xpos,
                            x_entry->start.y, xpos + x_entry->Widths[i], x_entry->end.y, Escaped);
                        fwrite(TextBuffer, 1, strlen(TextBuffer), tdev->file);
                        xpos += x_entry->Widths[i];
                    }
                    fwrite("</span>\n", sizeof(unsigned char), 8, tdev->file);
                    x_entry = x_entry->next;
                }
                fwrite("</line>\n", sizeof(unsigned char), 8, tdev->file);
                block_line = block_line->next;
            }
            fwrite("</block>\n", sizeof(unsigned char), 9, tdev->file);
            y_list = tdev->PageData.y_ordered_list;
        } while (y_list);

        fwrite("</page>\n", sizeof(unsigned char), 8, tdev->file);
    }
    return 0;
}

static int
txtwrite_output_page(gx_device * dev, int num_copies, int flush)
{
    int code;
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;
    text_list_entry_t * x_entry, *next_x;
    page_text_list_t *y_list;
    gs_parsed_file_name_t parsed;
    const char *fmt;
    const short BOM = 0xFEFF;

    if (!tdev->file) {
        /* Either this is the first page, or we're doing one file per page */
        code = gx_device_open_output_file(dev, tdev->fname,
                true, false, &tdev->file); /* binary, sequential */
        if (code < 0)
            return code;
    }

    switch(tdev->TextFormat) {
        case 0:
        case 1:
            code = decorated_text_output(tdev);
            if (code < 0)
                return code;
            break;

        case 2:
            fwrite (&BOM, sizeof(unsigned short), 1, tdev->file);
            /* fall through */
        case 3:
            code = simple_text_output(tdev);
            if (code < 0)
                return code;
            break;

        default:
            return gs_note_error(gs_error_rangecheck);
            break;
    }

    code =  gx_default_output_page(dev, num_copies, flush);
    if (code < 0)
        return code;

    /* free the sorted fragment list! */
    y_list = tdev->PageData.y_ordered_list;
    while (y_list) {
        x_entry = y_list->x_ordered_list;
        while (x_entry) {
            gs_free(tdev->memory, x_entry->Unicode_Text, x_entry->Unicode_Text_Size, sizeof (usnigned short), "txtwrite free text fragment text buffer");
            gs_free(tdev->memory, x_entry->Widths, x_entry->Unicode_Text_Size, sizeof (float), "txtwrite free widths array");
            gs_free(tdev->memory, x_entry->FontName, 1, strlen(x_entry->FontName) + 1, "txtwrite free Font Name");
            if (x_entry->next) {
                x_entry = x_entry->next;
                gs_free(tdev->memory, x_entry->previous, 1, sizeof(text_list_entry_t), "txtwrite free text fragment");
            } else {
                gs_free(tdev->memory, x_entry, 1, sizeof(text_list_entry_t), "txtwrite free text fragment");
                x_entry = NULL;
            }
        }
        if (y_list->next) {
            y_list = y_list->next;
            gs_free(tdev->memory, y_list->previous, 1, sizeof(page_text_list_t), "txtwrite free text list");
        } else {
            gs_free(tdev->memory, y_list, 1, sizeof(page_text_list_t), "txtwrite free text list");
            y_list = NULL;
        }
    }
    tdev->PageData.y_ordered_list = NULL;

    /* free the unsorted fragment list */
    x_entry = tdev->PageData.unsorted_text_list;
    while (x_entry) {
        next_x = x_entry->next;
        gs_free(tdev->memory, x_entry->Unicode_Text, x_entry->Unicode_Text_Size, sizeof (usnigned short), "txtwrite free unsorted text fragment text buffer");
        gs_free(tdev->memory, x_entry->Widths, x_entry->Unicode_Text_Size, sizeof (float), "txtwrite free widths array");
        gs_free(tdev->memory, x_entry->FontName, 1, strlen(x_entry->FontName) + 1, "txtwrite free Font Name");
        gs_free(tdev->memory, x_entry, 1, sizeof(text_list_entry_t), "txtwrite free unsorted text fragment");
        x_entry = next_x;
    }
    tdev->PageData.unsorted_text_list = NULL;

    code = gx_parse_output_file_name(&parsed, &fmt, tdev->fname,
                                         strlen(tdev->fname), tdev->memory);

    if (code >= 0 && fmt) { /* file per page */
        code = gx_device_close_output_file(dev, tdev->fname, tdev->file);
        tdev->file = NULL;
    }
    return code;
}

/* ---------------- Low-level drawing ---------------- */

static int
txtwrite_fill_rectangle(gx_device * dev, int x, int y, int w, int h,
                    gx_color_index color)
{
    return 0;
}

/*static int
txtwrite_copy_alpha(gx_device * dev, const byte * data, int data_x,
                int raster, gx_bitmap_id id, int x, int y, int w, int h,
                gx_color_index color, int depth)
{
    return 0;
}

static int
txtwrite_copy_mono(gx_device * dev, const byte * data, int dx, int raster,
               gx_bitmap_id id, int x, int y, int w, int h,
               gx_color_index zero, gx_color_index one)
{
    return 0;
}
static int
txtwrite_copy_color(gx_device * dev, const byte * data,
                int data_x, int raster, gx_bitmap_id id,
                int x, int y, int width, int height)
{
    return 0;
}

static int
txtwrite_strip_tile_rectangle(gx_device * dev, const gx_strip_bitmap * tiles,
   int x, int y, int w, int h, gx_color_index color0, gx_color_index color1,
                          int px, int py)
{
    return 0;
}

static int
txtwrite_strip_copy_rop(gx_device * dev,
                    const byte * sdata, int sourcex, uint sraster,
                    gx_bitmap_id id,
                    const gx_color_index * scolors,
                    const gx_strip_bitmap * textures,
                    const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return 0;
}*/

/* ---------------- Parameters ---------------- */

static int txtwrite_get_param(gx_device *dev, char *Param, void *list)
{
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;
    gs_param_list * plist = (gs_param_list *)list;
    bool bool_T = true;

    if (strcmp(Param, "OutputFile") == 0) {
        gs_param_string ofns;

        ofns.data = (const byte *)tdev->fname,
        ofns.size = strlen(tdev->fname),
        ofns.persistent = false;
        return param_write_string(plist, "OutputFile", &ofns);
    }
    if (strcmp(Param, "WantsToUnicode") == 0) {
        return param_write_bool(plist, "WantsToUnicode", &bool_T);
    }
    if (strcmp(Param, "PreserveTrMode") == 0) {
        return param_write_bool(plist, "PreserveTrMode", &bool_T);
    }
    if (strcmp(Param, "HighLevelDevice") == 0) {
        return param_write_bool(plist, "HighLevelDevice", &bool_T);
    }
    return_error(gs_error_undefined);
}

static int
txtwrite_get_params(gx_device * dev, gs_param_list * plist)
{
    int code;
    bool bool_T = true;
    gs_param_string ofns;
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;

    code = gx_default_get_params(dev, plist);
    if (code < 0)
        return code;

    ofns.data = (const byte *)tdev->fname,
    ofns.size = strlen(tdev->fname),
    ofns.persistent = false;
    code = param_write_string(plist, "OutputFile", &ofns);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "WantsToUnicode", &bool_T);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "PreserveTrMode", &bool_T);
    if (code < 0)
        return code;

    code = param_write_bool(plist, "HighLevelDevice", &bool_T);
    if (code < 0)
        return code;

   code = gs_param_write_items(plist, tdev, NULL, txt_param_items);
   return code;
}

/* We implement put_params to ensure that we keep the important */
/* device parameters up to date, and to prevent an /undefined error */
static int
txtwrite_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_txtwrite_t *tdev = (gx_device_txtwrite_t *) dev;
    int ecode = 0;
    int code;
    const char *param_name;
    gs_param_string ofs;
    bool dummy;

    switch (code = param_read_string(plist, (param_name = "OutputFile"), &ofs)) {
        case 0:
            if (dev->LockSafetyParams &&
                    bytes_compare(ofs.data, ofs.size,
                        (const byte *)tdev->fname, strlen(tdev->fname))) {
                ecode = gs_note_error(gs_error_invalidaccess);
                goto ofe;
            }
            if (ofs.size >= gp_file_name_sizeof)
                ecode = gs_error_limitcheck;
            else
                break;
            goto ofe;
        default:
            ecode = code;
          ofe:param_signal_error(plist, param_name, ecode);
        /* fall through */
        case 1:
            ofs.data = 0;
            break;
    }

    if (ecode < 0)
        return ecode;

    code = param_read_int(plist, "TextFormat", &tdev->TextFormat);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "WantsToUnicode", &dummy);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "HighLevelDevice", &dummy);
    if (code < 0)
        return code;

    code = param_read_bool(plist, "PreserveTrMode", &dummy);
    if (code < 0)
        return code;

    code = gx_default_put_params(dev, plist);
    if (code < 0)
        return code;

    if (ofs.data != 0) {	/* Close the file if it's open. */
        if (tdev->file != 0) {
            fclose(tdev->file);
            tdev->file = 0;
        }
        memcpy(tdev->fname, ofs.data, ofs.size);
        tdev->fname[ofs.size] = 0;
    }
    return 0;
}

/* ---------------- Polygon drawing ---------------- */

/*static int
txtwrite_fill_trapezoid(gx_device * dev,
                    const gs_fixed_edge * left, const gs_fixed_edge * right,
                    fixed ybot, fixed ytop, bool swap_axes,
                    const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    return 0;
}

static int
txtwrite_fill_parallelogram(gx_device * dev,
                        fixed px, fixed py, fixed ax, fixed ay,
                        fixed bx, fixed by, const gx_device_color * pdevc,
                        gs_logical_operation_t lop)
{
    return 0;
}

static int
txtwrite_fill_triangle(gx_device * dev,
                   fixed px, fixed py, fixed ax, fixed ay, fixed bx, fixed by,
                   const gx_device_color * pdevc, gs_logical_operation_t lop)
{
    return 0;
}

static int
txtwrite_draw_thin_line(gx_device * dev,
                    fixed fx0, fixed fy0, fixed fx1, fixed fy1,
                    const gx_device_color * pdevc, gs_logical_operation_t lop,
                    fixed adjustx, fixed adjusty)
{
    return 0;
}*/

/* ---------------- High-level drawing ---------------- */

static int
txtwrite_fill_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
               const gx_fill_params * params, const gx_device_color * pdevc,
               const gx_clip_path * pcpath)
{
        return 0;
}

static int
txtwrite_stroke_path(gx_device * dev, const gs_gstate * pgs, gx_path * ppath,
                 const gx_stroke_params * params,
                 const gx_drawing_color * pdevc, const gx_clip_path * pcpath)
{
    return 0;
}

/* ------ Text imaging ------ */

static int
txtwrite_font_orig_matrix(const gs_font *font, gs_glyph cid, gs_matrix *pmat)
{
    int code;

    switch (font->FontType) {
    case ft_composite:                /* subfonts have their own FontMatrix */
    case ft_TrueType:
    case ft_CID_TrueType:
        /* The TrueType FontMatrix is 1 unit per em, which is what we want. */
        gs_make_identity(pmat);
        return 0;
    case ft_encrypted:
    case ft_encrypted2:
    case ft_CID_encrypted:
    case ft_user_defined:
    case ft_PCL_user_defined:
    case ft_GL2_stick_user_defined:
    case ft_GL2_531:
        /*
         * Type 1 fonts are supposed to use a standard FontMatrix of
         * [0.001 0 0 0.001 0 0], with a 1000-unit cell.  However,
         * Windows NT 4.0 creates Type 1 fonts, apparently derived from
         * TrueType fonts, that use a 2048-unit cell and corresponding
         * FontMatrix.  Also, some PS programs perform font scaling by
         * replacing FontMatrix like this :
         *
         *   /f12 /Times-Roman findfont
         *   copyfont          % (remove FID)
         *   dup /FontMatrix [0.012 0 0 0.012 0 0] put
         *   definefont
         *   /f12 1 selectfont
         *
         * Such fonts are their own "base font", but the orig_matrix
         * must still be set to 0.001, not 0.012 .
         *
         * The old code used a heuristic to detect and correct for this here.
         * Unfortunately it doesn't work properly when it meets a font
         * with FontMatrix like this :
         *
         *   /FontMatrix [1 2288 div 0 0 1 2288 div 0 0 ] def
         *
         * (the bug 686970). Also comparefiles\455690.pdf appears to
         * have similar problem. Therefore we added a support to lib/gs_fonts.ps,
         * src/zbfont.c, src/gsfont.c that provides an acces to the original
         * font via a special key OrigFont added to the font dictionary while definefont.
         * Now we work through this access with PS interpreter,
         * but keep the old heuristic for other clients.
         */
        {
            const gs_font *base_font = font;

            while (base_font->base != base_font)
                base_font = base_font->base;
            if (font->FontType == ft_user_defined ||
                font->FontType == ft_PCL_user_defined ||
                font->FontType == ft_GL2_stick_user_defined ||
                font->FontType == ft_GL2_531)
                *pmat = base_font->FontMatrix;
            else if (base_font->orig_FontMatrix.xx != 0 || base_font->orig_FontMatrix.xy != 0 ||
                base_font->orig_FontMatrix.yx != 0 || base_font->orig_FontMatrix.yy != 0)
                *pmat = base_font->orig_FontMatrix;
            else {
                /*  Must not happen with PS interpreter.
                    Provide a hewuristic for other clients.
                */
                if (base_font->FontMatrix.xx == 1.0/2048 &&
                    base_font->FontMatrix.xy == 0 &&
                    base_font->FontMatrix.yx == 0 &&
                    any_abs(base_font->FontMatrix.yy) == 1.0/2048
                    )
                    *pmat = base_font->FontMatrix;
                else
                    gs_make_scaling(0.001, 0.001, pmat);
            }
        }
        if (font->FontType == ft_CID_encrypted && cid != -1) {
            int fidx;

            if (cid < GS_MIN_CID_GLYPH)
                cid = GS_MIN_CID_GLYPH;
            code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                cid, NULL, &fidx);
            if (code < 0) {
                code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                (gs_glyph)GS_MIN_CID_GLYPH, NULL, &fidx);
            }
            if (code >= 0) {
                gs_matrix_multiply(&(gs_cid0_indexed_font(font, fidx)->FontMatrix),
                                pmat, pmat);
            }
        }
        return 0;
    default:
        return_error(gs_error_rangecheck);
    }
}

/*
 * Special version of txtwrite_font_orig_matrix(), that considers FDArray font's FontMatrix too.
 * Called only by txt_glyph_width().
 * 'cid' is only consulted if 'font' is a CIDFontType 0 CID font.
 */
static int
glyph_orig_matrix(const gs_font *font, gs_glyph cid, gs_matrix *pmat)
{
    int code = txtwrite_font_orig_matrix(font, cid, pmat);
    if (code >= 0) {
        if (font->FontType == ft_CID_encrypted) {
            int fidx;

            if (cid < GS_MIN_CID_GLYPH)
                cid = GS_MIN_CID_GLYPH;
            code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                cid, NULL, &fidx);
            if (code < 0) {
                code = ((gs_font_cid0 *)font)->cidata.glyph_data((gs_font_base *)font,
                                (gs_glyph)GS_MIN_CID_GLYPH, NULL, &fidx);
            }
            if (code >= 0) {
                gs_matrix_multiply(&(gs_cid0_indexed_font(font, fidx)->FontMatrix),
                                pmat, pmat);
            }
        }
    }
    return code;
}

/*
 * Compute the cached values in the text processing state from the text
 * parameters, current_font, and pgs->ctm.  Return either an error code (<
 * 0) or a mask of operation attributes that the caller must emulate.
 * Currently the only such attributes are TEXT_ADD_TO_ALL_WIDTHS and
 * TEXT_ADD_TO_SPACE_WIDTH.  Note that this procedure fills in all the
 * values in ppts->values, not just the ones that need to be set now.
 */
static int
transform_delta_inverse(const gs_point *pdelta, const gs_matrix *pmat,
                        gs_point *ppt)
{
    int code = gs_distance_transform_inverse(pdelta->x, pdelta->y, pmat, ppt);
    gs_point delta;

    if (code < 0)
        return code;
    if (ppt->y == 0)
        return 0;
    /* Check for numerical fuzz. */
    code = gs_distance_transform(ppt->x, 0.0, pmat, &delta);
    if (code < 0)
        return 0;                /* punt */
    if (fabs(delta.x - pdelta->x) < 0.01 && fabs(delta.y - pdelta->y) < 0.01) {
        /* Close enough to y == 0: device space error < 0.01 pixel. */
        ppt->y = 0;
    }
    return 0;
}

static
float txt_calculate_text_size(gs_gstate *pgs, gs_font *ofont,
                              const gs_matrix *pfmat, gs_matrix *smat, gs_matrix *tmat,
                              gs_font *font, gx_device *pdev)
{
    gs_matrix orig_matrix;
    double
        sx = pdev->HWResolution[0] / 72.0,
        sy = pdev->HWResolution[1] / 72.0;
    float size;

    /* Get the original matrix of the base font. */

    txtwrite_font_orig_matrix(ofont, -1, &orig_matrix);
    /* Compute the scaling matrix and combined matrix. */

    gs_matrix_invert(&orig_matrix, smat);
    gs_matrix_multiply(smat, pfmat, smat);
    *tmat = ctm_only(pgs);
    tmat->tx = tmat->ty = 0;
    gs_matrix_multiply(smat, tmat, tmat);

    /* Try to find a reasonable size value. */

    size = hypot(tmat->yx, tmat->yy) / sy;
    if (size < 0.01)
        size = hypot(tmat->xx, tmat->xy) / sx;
    if (size < 0.01)
        size = 1;

    return(size);
}

static int
txt_update_text_state(text_list_entry_t *ppts,
                      const textw_text_enum_t *penum,
                      gs_font *ofont, const gs_matrix *pfmat)
{
    gx_device *const pdev = penum->dev;
    gs_font *font = penum->current_font;
    gs_fixed_point cpt;
    gs_matrix smat, tmat;
    float size;
    int mask = 0;
    int code = gx_path_current_point(penum->path, &cpt);

    if (code < 0)
        return code;

    size = txt_calculate_text_size(penum->pgs, ofont, pfmat, &smat, &tmat, penum->current_font, pdev);
    /* Check for spacing parameters we can handle, and transform them. */

    if (penum->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
        if (penum->current_font->WMode == 0) {
            gs_point pt;

            code = transform_delta_inverse(&penum->text.delta_all, &smat, &pt);
            if (code < 0 || pt.y != 0)
                mask |= TEXT_ADD_TO_ALL_WIDTHS;
        }
        else
            mask |= TEXT_ADD_TO_ALL_WIDTHS;
    }

    if (penum->text.operation & TEXT_ADD_TO_SPACE_WIDTH) {
        gs_point pt;

        code = transform_delta_inverse(&penum->text.delta_space, &smat, &pt);
        if (code < 0 || pt.y != 0 || penum->text.space.s_char != 32)
            mask |= TEXT_ADD_TO_SPACE_WIDTH;
    }
    /* Store the updated values. */

    tmat.xx /= size;
    tmat.xy /= size;
    tmat.yx /= size;
    tmat.yy /= size;
    tmat.tx += fixed2float(cpt.x);
    tmat.ty += fixed2float(cpt.y);

    ppts->size = size;
    ppts->matrix = tmat;
    ppts->render_mode = penum->pgs->text_rendering_mode;
    ppts->FontName = (char *)gs_malloc(pdev->memory->stable_memory, 1,
        font->font_name.size + 1, "txtwrite alloc font name");
    if (!ppts->FontName)
        return gs_note_error(gs_error_VMerror);
    memcpy(ppts->FontName, font->font_name.chars, font->font_name.size);
    ppts->FontName[font->font_name.size] = 0x00;
    ppts->render_mode = font->WMode;

    if (font->PaintType == 2 && penum->pgs->text_rendering_mode == 0)
    {
        gs_gstate *pgs = penum->pgs;
        gs_font *font = penum->current_font;
        double scaled_width = font->StrokeWidth != 0 ? font->StrokeWidth : 0.001;
        double saved_width = pgs->line_params.half_width;
        /*
         * See stream_to_text in gdevpdfu.c re the computation of
         * the scaling value.
         */
        double scale = 72.0 / pdev->HWResolution[1];

        if (font->FontMatrix.yy != 0)
            scaled_width *= fabs(font->orig_FontMatrix.yy) * size * tmat.yy * scale;
        else
            scaled_width *= fabs(font->orig_FontMatrix.xy) * size * tmat.xy * scale;

        ppts->render_mode = 1;
        ppts->PaintType0Width = scaled_width;

        pgs->line_params.half_width = scaled_width / 2;
        if (code < 0)
            return code;

        pgs->line_params.half_width = saved_width;
    }
    return (code < 0 ? code : mask);
}

static int
store_glyph_width(txt_glyph_width_t *pwidth, int wmode, const gs_matrix *scale,
                  const gs_glyph_info_t *pinfo)
{
    double w, v;

    gs_distance_transform(pinfo->width[wmode].x, pinfo->width[wmode].y, scale, &pwidth->xy);
    if (wmode)
        w = pwidth->xy.y, v = pwidth->xy.x;
    else
        w = pwidth->xy.x, v = pwidth->xy.y;
    if (v != 0)
        return 1;
    pwidth->w = w;
    gs_distance_transform(pinfo->v.x, pinfo->v.y, scale, &pwidth->v);
    return 0;
}

static int
get_missing_width(gs_font *font, int wmode, const gs_matrix *scale_c,
                    txt_glyph_widths_t *pwidths)
{
    gs_font_info_t finfo;
    int code;

    code = font->procs.font_info((gs_font *)font, NULL,
                                  FONT_INFO_MISSING_WIDTH, &finfo);
    if (code < 0)
        return code;
    if (wmode) {
        gs_distance_transform(0.0, -finfo.MissingWidth, scale_c, &pwidths->real_width.xy);
        pwidths->Width.xy.x = 0;
        pwidths->Width.xy.y = pwidths->real_width.xy.y;
        pwidths->Width.w = pwidths->real_width.w =
                pwidths->Width.xy.y;
        pwidths->Width.v.x = - pwidths->Width.xy.y / 2;
        pwidths->Width.v.y = - pwidths->Width.xy.y;
    } else {
        gs_distance_transform(finfo.MissingWidth, 0.0, scale_c, &pwidths->real_width.xy);
        pwidths->Width.xy.x = pwidths->real_width.xy.x;
        pwidths->Width.xy.y = 0;
        pwidths->Width.w = pwidths->real_width.w =
                pwidths->Width.xy.x;
        pwidths->Width.v.x = pwidths->Width.v.y = 0;
    }
    /*
     * Don't mark the width as known, just in case this is an
     * incrementally defined font.
     */
    return 1;
}

/*
 * Get the widths (unmodified from the copied font,
 * and possibly modified from the original font) of a given glyph.
 * Return 1 if the width was defaulted to MissingWidth.
 * Return TEXT_PROCESS_CDEVPROC if a CDevProc callout is needed.
 * cdevproc_result != NULL if we restart after a CDevProc callout.
 */
static int
txt_glyph_widths(gs_font *font, int wmode, gs_glyph glyph,
                 gs_font *orig_font, txt_glyph_widths_t *pwidths,
                 const double cdevproc_result[10])
{
    gs_font *ofont = orig_font;
    gs_glyph_info_t info;
    gs_matrix scale_c, scale_o;
    int code, rcode = 0;
    gs_point v;
    int allow_cdevproc_callout = (orig_font->FontType == ft_CID_TrueType
                || orig_font->FontType == ft_CID_encrypted
                ? GLYPH_INFO_CDEVPROC : 0); /* fixme : allow more font types. */

    if (ofont->FontType == ft_composite)
        return_error(gs_error_unregistered); /* Must not happen. */
    code = glyph_orig_matrix((const gs_font *)font, glyph, &scale_c);
    if (code < 0)
        return code;
    code = glyph_orig_matrix(ofont, glyph, &scale_o);
    if (code < 0)
        return code;
    gs_matrix_scale(&scale_c, 1000.0, 1000.0, &scale_c);
    gs_matrix_scale(&scale_o, 1000.0, 1000.0, &scale_o);
    pwidths->Width.v.x = pwidths->Width.v.y = 0;
    pwidths->real_width.v.x = pwidths->real_width.v.y = 0;
    pwidths->replaced_v = false;
    if (glyph == GS_NO_GLYPH)
        return get_missing_width(font, wmode, &scale_c, pwidths);
    code = font->procs.glyph_info((gs_font *)font, glyph, NULL,
                                    GLYPH_INFO_WIDTH0 |
                                    (GLYPH_INFO_WIDTH0 << wmode) |
                                    GLYPH_INFO_OUTLINE_WIDTHS |
                                    (GLYPH_INFO_VVECTOR0 << wmode),
                                    &info);
    /* For CID fonts the PDF spec requires the x-component of v-vector
       to be equal to half glyph width, and AR5 takes it from W, DW.
       So make a compatibe data here.
     */
    if (font->FontType != ft_PCL_user_defined && font->FontType != ft_GL2_stick_user_defined &&
        font->FontType != ft_GL2_531
        && (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode)))) {
        code = get_missing_width(font, wmode, &scale_c, pwidths);
        if (code < 0)
            v.y = 0;
        else
            v.y = pwidths->Width.v.y;
        if (wmode && (ofont->FontType == ft_CID_encrypted ||
            ofont->FontType == ft_CID_TrueType)) {
            txt_glyph_widths_t widths1;

            if (get_missing_width(font, 0, &scale_c, &widths1) < 0)
                v.x = 0;
            else
                v.x = widths1.Width.w / 2;
        } else
            v.x = pwidths->Width.v.x;
    } else if (code < 0)
        return code;
    else {
        code = store_glyph_width(&pwidths->Width, wmode, &scale_c, &info);
        if (code < 0)
            return code;
        rcode |= code;
        if (info.members  & (GLYPH_INFO_VVECTOR0 << wmode))
            gs_distance_transform(info.v.x, info.v.y, &scale_c, &v);
        else
            v.x = v.y = 0;
        if (wmode && (ofont->FontType == ft_CID_encrypted ||
            ofont->FontType == ft_CID_TrueType)) {
            if (info.members & (GLYPH_INFO_WIDTH0 << wmode)) {
                gs_point xy;

                gs_distance_transform(info.width[0].x, info.width[0].y, &scale_c, &xy);
                v.x = xy.x / 2;
            } else {
                txt_glyph_widths_t widths1;

                if (get_missing_width(font, 0, &scale_c, &widths1) < 0)
                    v.x = 0;
                else
                    v.x = widths1.Width.w / 2;
            }
        }
    }
    pwidths->Width.v = v;
    /* Skip only if not paralel to the axis. */
    if (code > 0 && ofont->FontType != ft_CID_encrypted &&
            ofont->FontType != ft_CID_TrueType)
        pwidths->Width.xy.x = pwidths->Width.xy.y = pwidths->Width.w = 0;
    if (cdevproc_result == NULL) {
        info.members = 0;
        code = ofont->procs.glyph_info(ofont, glyph, NULL,
                                            (GLYPH_INFO_WIDTH0 << wmode) |
                                            (GLYPH_INFO_VVECTOR0 << wmode) |
                                            allow_cdevproc_callout,
                                            &info);
        /* fixme : Move this call before cfont->procs.glyph_info. */
        if (info.members & GLYPH_INFO_CDEVPROC) {
            if (allow_cdevproc_callout)
                return TEXT_PROCESS_CDEVPROC;
        else
            return_error(gs_error_rangecheck);
        }
    } else {
        info.width[0].x = cdevproc_result[0];
        info.width[0].y = cdevproc_result[1];
        info.width[1].x = cdevproc_result[6];
        info.width[1].y = cdevproc_result[7];
        info.v.x = (wmode ? cdevproc_result[8] : 0);
        info.v.y = (wmode ? cdevproc_result[9] : 0);
        info.members = (GLYPH_INFO_WIDTH0 << wmode) |
                       (wmode ? GLYPH_INFO_VVECTOR1 : 0);
        code = 0;
    }
    if (code == gs_error_undefined || !(info.members & (GLYPH_INFO_WIDTH0 << wmode)))
        pwidths->real_width = pwidths->Width;
    else if (code < 0)
        return code;
    else {
        if ((info.members & (GLYPH_INFO_VVECTOR0 | GLYPH_INFO_VVECTOR1)) != 0)
            pwidths->replaced_v = true;
        else
            info.v.x = info.v.y = 0;
        code = store_glyph_width(&pwidths->real_width, wmode, &scale_o, &info);
        if (code < 0)
            return code;
        rcode |= code;
        gs_distance_transform(info.v.x, info.v.y, &scale_o, &pwidths->real_width.v);
    }
    return rcode;
}

static void
txt_char_widths_to_uts(gs_font *font /* may be NULL for non-Type3 */,
                       txt_glyph_widths_t *pwidths)
{
    if (font && (font->FontType == ft_user_defined ||
        font->FontType == ft_PCL_user_defined ||
        font->FontType == ft_GL2_stick_user_defined ||
        font->FontType == ft_GL2_531)) {
        gs_matrix *pmat = &font->orig_FontMatrix;

        pwidths->Width.xy.x *= pmat->xx; /* formula simplified based on wy in glyph space == 0 */
        pwidths->Width.xy.y  = 0.0; /* WMode == 0 for PDF Type 3 fonts */
        gs_distance_transform(pwidths->real_width.xy.x, pwidths->real_width.xy.y, pmat, &pwidths->real_width.xy);
    } else {
        /*
         * For other font types:
         * - PDF design->text space is a simple scaling by 0.001.
         * - The Width.xy.x/y that should be zeroed-out per 5.3.3 "Text Space Details" is already 0.
         */
        pwidths->Width.xy.x /= 1000.0;
        pwidths->Width.xy.y /= 1000.0;
        pwidths->real_width.xy.x /= 1000.0;
        pwidths->real_width.xy.y /= 1000.0;
    }
}

/* Simple routine to update the current point by the accumulated width of the
 * text.
 */
static int
txt_shift_text_currentpoint(textw_text_enum_t *penum, gs_point *wpt)
{
    gs_gstate *pgs;
    extern_st(st_gs_gstate);

    if (gs_object_type(penum->dev->memory, penum->pgs) != &st_gs_gstate) {
        /* Probably never happens. Not sure though. */
        return_error(gs_error_unregistered);
    }
    pgs = (gs_gstate *)penum->pgs;
    return gs_moveto_aux(penum->pgs, gx_current_path(pgs),
                              fixed2float(penum->origin.x) + wpt->x,
                              fixed2float(penum->origin.y) + wpt->y);
}

/* Try to convert glyph names/character codes to Unicode. We first try to see
 * if we have any Unicode information either from a ToUnicode CMap or GlyphNames2Unicode
 * table. If that fails we look at the glyph name to see if it starts 'uni'
 * in which case we assume the remainder of the name is the Unicode value. If
 * its not a glyph of that form then we search a bunch of tables whcih map standard
 * glyph names to Unicode code points. If that fails we finally just return the character code.
 */
static int get_unicode(textw_text_enum_t *penum, gs_font *font, gs_glyph glyph, gs_char ch, unsigned short *Buffer)
{
    int code;
    gs_const_string gnstr;
    unsigned short fallback = ch;
    ushort *unicode = NULL;
    int length;

    length = font->procs.decode_glyph((gs_font *)font, glyph, ch, NULL, 0);
    if (length == 0) {
        code = font->procs.glyph_name(font, glyph, &gnstr);
        if (code >= 0 && gnstr.size == 7) {
            if (!memcmp(gnstr.data, "uni", 3)) {
                static const char *hexdigits = "0123456789ABCDEF";
                char *d0 = strchr(hexdigits, gnstr.data[3]);
                char *d1 = strchr(hexdigits, gnstr.data[4]);
                char *d2 = strchr(hexdigits, gnstr.data[5]);
                char *d3 = strchr(hexdigits, gnstr.data[6]);

                if (d0 != NULL && d1 != NULL && d2 != NULL && d3 != NULL) {
                    *Buffer++ = ((d0 - hexdigits) << 12) + ((d1 - hexdigits) << 8) + ((d2 - hexdigits) << 4) + (d3 - hexdigits);
                    return 1;
                }
            }
        }
        if (length == 0) {
            single_glyph_list_t *sentry = (single_glyph_list_t *)&SingleGlyphList;
            double_glyph_list_t *dentry = (double_glyph_list_t *)&DoubleGlyphList;
            treble_glyph_list_t *tentry = (treble_glyph_list_t *)&TrebleGlyphList;
            quad_glyph_list_t *qentry = (quad_glyph_list_t *)&QuadGlyphList;

            /* Search glyph to single Unicode value table */
            while (sentry->Glyph != 0) {
                if (sentry->Glyph[0] < gnstr.data[0]) {
                    sentry++;
                    continue;
                }
                if (sentry->Glyph[0] > gnstr.data[0]){
                    break;
                }
                if (strlen(sentry->Glyph) == gnstr.size) {
                    if(memcmp(gnstr.data, sentry->Glyph, gnstr.size) == 0) {
                        *Buffer = sentry->Unicode;
                        return 1;
                    }
                }
                sentry++;
            }

            /* Search glyph to double Unicode value table */
            while (dentry->Glyph != 0) {
                if (dentry->Glyph[0] < gnstr.data[0]) {
                    dentry++;
                    continue;
                }
                if (dentry->Glyph[0] > gnstr.data[0]){
                    break;
                }
                if (strlen(dentry->Glyph) == gnstr.size) {
                    if(memcmp(gnstr.data, dentry->Glyph, gnstr.size) == 0) {
                        memcpy(Buffer, dentry->Unicode, 2);
                        return 2;
                    }
                }
                dentry++;
            }

            /* Search glyph to triple Unicode value table */
            while (tentry->Glyph != 0) {
                if (tentry->Glyph[0] < gnstr.data[0]) {
                    tentry++;
                    continue;
                }
                if (tentry->Glyph[0] > gnstr.data[0]){
                    break;
                }
                if (strlen(tentry->Glyph) == gnstr.size) {
                    if(memcmp(gnstr.data, tentry->Glyph, gnstr.size) == 0) {
                        memcpy(Buffer, tentry->Unicode, 3);
                        return 3;
                    }
                }
                tentry++;
            }

            /* Search glyph to quadruple Unicode value table */
            while (qentry->Glyph != 0) {
                if (qentry->Glyph[0] < gnstr.data[0]) {
                    qentry++;
                    continue;
                }
                if (qentry->Glyph[0] > gnstr.data[0]){
                    break;
                }
                if (strlen(qentry->Glyph) == gnstr.size) {
                    if(memcmp(gnstr.data, qentry->Glyph, gnstr.size) == 0) {
                        memcpy(Buffer, qentry->Unicode, 4);
                        return 4;
                    }
                }
                qentry++;
            }
        }
        *Buffer = fallback;
        return 1;
    } else {
        char *b, *u;
        int l = length - 1;

        unicode = (ushort *)gs_alloc_bytes(penum->dev->memory, length, "temporary Unicode array");
        length = font->procs.decode_glyph((gs_font *)font, glyph, ch, unicode, length);
#if ARCH_IS_BIG_ENDIAN
        memcpy(Buffer, unicode, length);
#else
        b = (char *)Buffer;
        u = (char *)unicode;
        while (l >= 0) {
            *b++ = *(u + l);
            l--;
        }

#endif
        gs_free_object(penum->dev->memory, unicode, "free temporary unicode buffer");
        return length / sizeof(short);
    }
}

/* Routines to enumerate each glyph/character code in turn, find its width
 * so that we can update the current point and find the end of the text, convert
 * to Unicode if at all possible, and store some state such as the font, colour
 * text rendering mode, writing mode, etc.
 */

static int
txtwrite_process_cmap_text(gs_text_enum_t *pte)
{
    textw_text_enum_t *const penum = (textw_text_enum_t *)pte;
    unsigned int rcode = 0;
    gs_text_enum_t scan = *(gs_text_enum_t *)pte;

    /* Composite font using a CMap */
    for ( ; ; ) {
        gs_glyph glyph;
        int font_code, code;
        gs_font *subfont;
        gs_char chr;
        txt_glyph_widths_t widths;
        gs_matrix m3;
        gs_point wanted;	/* user space */
        gs_point dpt = {0,0};

        font_code = scan.orig_font->procs.next_char_glyph
                (&scan, &chr, &glyph);

        subfont = scan.fstack.items[scan.fstack.depth].font;

        switch (font_code) {
            case 0:		/* no font change */
            case 1:		/* font change */
                code = txt_glyph_widths(subfont, scan.orig_font->WMode, glyph, (gs_font *)subfont, &widths,
                        penum->cdevproc_callout ? penum->cdevproc_result : NULL);
                if (code == TEXT_PROCESS_CDEVPROC) {
                    penum->cdevproc_callout = true;
                    pte->returned.current_glyph = glyph;
                    scan.returned.current_glyph = glyph;
                    pte->current_font = subfont;
                    scan.current_font = subfont;
                    rcode = TEXT_PROCESS_CDEVPROC;
                    break;
                }
                else {
                    penum->cdevproc_callout = false;
                    pte->index = scan.index;
                }
                code = gs_matrix_multiply(&subfont->FontMatrix, &pte->orig_font->FontMatrix, &m3);
                if (code < 0)
                    return code;
                code = txt_update_text_state(penum->text_state, (textw_text_enum_t *)pte, pte->orig_font, &m3);
                if (code < 0)
                    return code;
                txt_char_widths_to_uts(pte->orig_font, &widths); /* convert design->text space */
                gs_distance_transform(widths.real_width.xy.x * penum->text_state->size,
                          widths.real_width.xy.y * penum->text_state->size,
                          &penum->text_state->matrix, &wanted);
                pte->returned.total_width.x += wanted.x;
                pte->returned.total_width.y += wanted.y;
                penum->Widths[pte->index - 1] = wanted.x;

                if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
                    gs_point tpt;

                    gs_distance_transform(pte->text.delta_all.x, pte->text.delta_all.y,
                              &ctm_only(pte->pgs), &tpt);
                    dpt.x += tpt.x;
                    dpt.y += tpt.y;
                }
                if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH && chr == pte->text.space.s_char) {
                    gs_point tpt;

                    gs_distance_transform(pte->text.delta_space.x, pte->text.delta_space.y,
                              &ctm_only(pte->pgs), &tpt);
                    dpt.x += tpt.x;
                    dpt.y += tpt.y;
                }
                pte->returned.total_width.x += dpt.x;
                pte->returned.total_width.y += dpt.y;

                penum->TextBufferIndex += get_unicode(penum, (gs_font *)pte->orig_font, glyph, chr, &penum->TextBuffer[penum->TextBufferIndex]);
                penum->Widths[pte->index - 1] += dpt.x;
                break;
            case 2:		/* end of string */
                return 0;
            default:	        /* error */
                return font_code;
        }
        if (rcode || pte->index >= pte->text.size)
            break;
    }
    return rcode;
}

static int
txtwrite_process_plain_text(gs_text_enum_t *pte)
{
    /* one byte regular font */
    textw_text_enum_t *const penum = (textw_text_enum_t *)pte;
    gs_font *font = pte->orig_font;
    const gs_glyph *gdata = NULL;
    gs_glyph glyph;
    gs_char ch = 0;
    int i, code;
    uint operation = pte->text.operation;
    txt_glyph_widths_t widths;
    gs_point wanted;	/* user space */
    gs_point dpt = {0,0};

    for (i=0;i<pte->text.size;i++) {
        if (operation & (TEXT_FROM_STRING | TEXT_FROM_BYTES)) {
            ch = pte->text.data.bytes[pte->index++];
        } else if (operation & (TEXT_FROM_CHARS | TEXT_FROM_SINGLE_CHAR)) {
            ch = pte->text.data.chars[pte->index++];
        } else if (operation & (TEXT_FROM_GLYPHS | TEXT_FROM_SINGLE_GLYPH)) {
            if (operation & TEXT_FROM_GLYPHS) {
                gdata = pte->text.data.glyphs + (pte->index++ * sizeof (gs_glyph));
            } else {
                gdata = &pte->text.data.d_glyph;
            }
        }
        glyph = (gdata == NULL ? pte->orig_font->procs.encode_char(pte->orig_font, ch, GLYPH_SPACE_NAME)
                           : *gdata);

        code = txt_glyph_widths(font, font->WMode, glyph, (gs_font *)font, &widths, NULL);
        if (code < 0)
            return code;

        penum->cdevproc_callout = false;
        code = txt_update_text_state(penum->text_state, (textw_text_enum_t *)pte, pte->orig_font, &font->FontMatrix);
        if (code < 0)
            return code;

        txt_char_widths_to_uts(pte->orig_font, &widths); /* convert design->text space */
        gs_distance_transform(widths.real_width.xy.x * penum->text_state->size,
                          widths.real_width.xy.y * penum->text_state->size,
                          &penum->text_state->matrix, &wanted);
        pte->returned.total_width.x += wanted.x;
        pte->returned.total_width.y += wanted.y;
        penum->Widths[pte->index - 1] = wanted.x;

        if (pte->text.operation & TEXT_ADD_TO_ALL_WIDTHS) {
            gs_point tpt;

            gs_distance_transform(pte->text.delta_all.x, pte->text.delta_all.y,
                              &ctm_only(pte->pgs), &tpt);
            dpt.x += tpt.x;
            dpt.y += tpt.y;
        }
        if (pte->text.operation & TEXT_ADD_TO_SPACE_WIDTH && ch == pte->text.space.s_char) {
            gs_point tpt;

            gs_distance_transform(pte->text.delta_space.x, pte->text.delta_space.y,
                              &ctm_only(pte->pgs), &tpt);
            dpt.x += tpt.x;
            dpt.y += tpt.y;
        }
        pte->returned.total_width.x += dpt.x;
        pte->returned.total_width.y += dpt.y;

        penum->TextBufferIndex += get_unicode(penum, (gs_font *)pte->orig_font, glyph, ch, &penum->TextBuffer[penum->TextBufferIndex]);
        penum->Widths[pte->index - 1] += dpt.x;
    }
    return 0;
}

/* Routine to add the accumulated text, and its recorded properties to our
 * lists. We maintain a list of text on a per-page basis. Each fragment is
 * sorted by Y co-ordinate, then by X co-ordinate, and stored that way.
 * Eventually we will want to merge 'adjacent' fragments with the same
 * properties, at least when outputting a simple representation. We won't
 * do this for languages which don't read left/right or right/left though.
 */
static int
txt_add_sorted_fragment(gx_device_txtwrite_t *tdev, textw_text_enum_t *penum)
{
    if (!tdev->PageData.y_ordered_list) {
        /* first entry, no need to sort, just store it */
        tdev->PageData.y_ordered_list = (page_text_list_t *)gs_malloc(tdev->memory->stable_memory, 1,
            sizeof(page_text_list_t), "txtwrite alloc Y list entry");
        if (!tdev->PageData.y_ordered_list)
            return gs_note_error(gs_error_VMerror);
        memset(tdev->PageData.y_ordered_list, 0x00, sizeof(page_text_list_t));
        tdev->PageData.y_ordered_list->x_ordered_list = penum->text_state;
        tdev->PageData.y_ordered_list->next = tdev->PageData.y_ordered_list->previous = NULL;
        tdev->PageData.y_ordered_list->start = penum->text_state->start;
    } else {
        page_text_list_t *Y_List = tdev->PageData.y_ordered_list;

        while (Y_List->next && Y_List->start.y < penum->text_state->start.y)
            Y_List = Y_List->next;

        if (Y_List->start.y == penum->text_state->start.y) {
            /* Already have text at this y-position */
            text_list_entry_t *X_List = Y_List->x_ordered_list;

            while (X_List->next && X_List->start.x < penum->text_state->start.x)
                X_List = X_List->next;

            if (X_List->start.x > penum->text_state->start.x) {
                /* Insert before */
                penum->text_state->next = X_List;
                penum->text_state->previous = X_List->previous;
                X_List->previous = penum->text_state;
                if (!penum->text_state->previous)
                    /* New head of list */
                    Y_List->x_ordered_list = penum->text_state;
                else
                    penum->text_state->previous->next = penum->text_state;
            } else {
                /* Insert after */
                penum->text_state->previous = X_List;
                penum->text_state->next = X_List->next;
                X_List->next = penum->text_state;
                if (penum->text_state->next)
                    penum->text_state->next->previous = penum->text_state;
            }
            if (penum->text_state->FontBBox_bottomleft.y < Y_List->MinY)
                Y_List->MinY = penum->text_state->FontBBox_bottomleft.y;
            if (penum->text_state->FontBBox_bottomleft.y > Y_List->MaxY)
                Y_List->MaxY = penum->text_state->FontBBox_bottomleft.y;
            if (penum->text_state->FontBBox_topright.y < Y_List->MinY)
                Y_List->MinY = penum->text_state->FontBBox_topright.y;
            if (penum->text_state->FontBBox_topright.y > Y_List->MaxY)
                Y_List->MaxY = penum->text_state->FontBBox_topright.y;
        } else {
            /* New y-position, make a Y list new record */
            page_text_list_t *Y_Entry = (page_text_list_t *)gs_malloc(tdev->memory->stable_memory, 1,
                sizeof(page_text_list_t), "txtwrite alloc Y-list");
            if (!Y_Entry)
                return gs_note_error(gs_error_VMerror);

            Y_Entry->x_ordered_list = penum->text_state;
            Y_Entry->start = penum->text_state->start;
            if (penum->text_state->FontBBox_bottomleft.y > penum->text_state->FontBBox_topright.y) {
                Y_Entry->MinY = penum->text_state->FontBBox_topright.y;
                Y_Entry->MaxY = penum->text_state->FontBBox_bottomleft.y;
            } else {
                Y_Entry->MaxY = penum->text_state->FontBBox_topright.y;
                Y_Entry->MinY = penum->text_state->FontBBox_bottomleft.y;
            }

            if (Y_List->start.y > penum->text_state->start.y) {
                /* Insert before */
                Y_Entry->next = Y_List;
                Y_Entry->previous = Y_List->previous;
                Y_List->previous = Y_Entry;
                if (!Y_Entry->previous)
                    /* New head of list */
                    tdev->PageData.y_ordered_list = Y_Entry;
                else
                    ((page_text_list_t *)Y_Entry->previous)->next = Y_Entry;
            } else {
                /* Insert after */
                Y_Entry->next = Y_List->next;
                Y_Entry->previous = Y_List;
                Y_List->next = Y_Entry;
                if (Y_Entry->next)
                    ((page_text_list_t *)(Y_Entry->next))->previous = Y_Entry;
            }
        }
    }
    penum->text_state = NULL;
    return 0;
}

static int
txt_add_fragment(gx_device_txtwrite_t *tdev, textw_text_enum_t *penum)
{
    text_list_entry_t *unsorted_entry, *t;

    /* Create a duplicate entry for the unsorted list */
    unsorted_entry = (text_list_entry_t *)gs_malloc(tdev->memory->stable_memory, 1,
            sizeof(text_list_entry_t), "txtwrite alloc sorted text state");
    if (!unsorted_entry)
        return gs_note_error(gs_error_VMerror);

    /* Calculate the start and end points of the text */
    penum->text_state->start.x = fixed2float(penum->origin.x);
    penum->text_state->start.y = fixed2float(penum->origin.y);
    penum->text_state->end.x = penum->text_state->start.x + penum->returned.total_width.x;
    penum->text_state->end.y = penum->text_state->start.y + penum->returned.total_width.y;
    penum->text_state->Unicode_Text_Size = penum->TextBufferIndex;

    *unsorted_entry = *(penum->text_state);

    /* Update the saved text state with the acccumulated Unicode data */
    /* The working buffer (penum->TextBuffer) is freed in the text_release method */
    penum->text_state->Unicode_Text = (unsigned short *)gs_malloc(tdev->memory->stable_memory,
        penum->TextBufferIndex, sizeof(unsigned short), "txtwrite alloc text buffer");
    if (!penum->text_state->Unicode_Text)
        return gs_note_error(gs_error_VMerror);
    memcpy(penum->text_state->Unicode_Text, penum->TextBuffer, penum->TextBufferIndex * sizeof(unsigned short));

    penum->text_state->Widths = (float *)gs_malloc(tdev->memory->stable_memory,
        penum->TextBufferIndex, sizeof(float), "txtwrite alloc widths array");
    if (!penum->text_state->Widths)
        return gs_note_error(gs_error_VMerror);
    memcpy(penum->text_state->Widths, penum->Widths, penum->TextBufferIndex * sizeof(float));

    unsorted_entry->Unicode_Text = (unsigned short *)gs_malloc(tdev->memory->stable_memory,
        penum->TextBufferIndex, sizeof(unsigned short), "txtwrite alloc sorted text buffer");
    if (!unsorted_entry->Unicode_Text)
        return gs_note_error(gs_error_VMerror);
    memcpy(unsorted_entry->Unicode_Text, penum->TextBuffer, penum->TextBufferIndex * sizeof(unsigned short));

    unsorted_entry->Widths = (float *)gs_malloc(tdev->memory->stable_memory,
        penum->TextBufferIndex, sizeof(float), "txtwrite alloc widths array");
    if (!unsorted_entry->Widths)
        return gs_note_error(gs_error_VMerror);
    memcpy(unsorted_entry->Widths, penum->Widths, penum->TextBufferIndex * sizeof(float));

    unsorted_entry->FontName = (char *)gs_malloc(tdev->memory->stable_memory,
        (strlen(penum->text_state->FontName) + 1), sizeof(unsigned char), "txtwrite alloc sorted text buffer");
    if (!unsorted_entry->FontName)
        return gs_note_error(gs_error_VMerror);
    memcpy(unsorted_entry->FontName, penum->text_state->FontName, strlen(penum->text_state->FontName) * sizeof(unsigned char));
    unsorted_entry->FontName[strlen(penum->text_state->FontName)] = 0x00;

    /* First add one entry to the unsorted list */
    if (!tdev->PageData.unsorted_text_list) {
        tdev->PageData.unsorted_text_list = unsorted_entry;
        unsorted_entry->next = unsorted_entry->previous = NULL;
    } else {
        t = tdev->PageData.unsorted_text_list;
        while (t->next)
            t = t->next;
        t->next = unsorted_entry;
        unsorted_entry->next = NULL;
        unsorted_entry->previous = t;
    }

    /* Then add the other entry to the sorted list */
    return txt_add_sorted_fragment(tdev, penum);
}

/* This routine selects whether the text needs to be handled as regular glyphs
 * and character codes, or as CIDs, depending on the font type. This is required
 * because there are ways that regular text can be handled that aren't possible
 * with CIDFonts.
 */
static int
textw_text_process(gs_text_enum_t *pte)
{
    textw_text_enum_t *const penum = (textw_text_enum_t *)pte;
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) pte->dev;
    gs_font *font = pte->orig_font;
    gs_font_base *font_base = (gs_font_base *)pte->current_font;
    int code = 0;

    if (pte->text.size == 0)
        return 0;

    if (!penum->TextBuffer) {
        /* We can get up to 4 Unicode points per glyph, and a glyph can be
         * be represented by as little as one byte. So we make a very large
         * temporary buffer to hold the Unicode string as we assemble it. When
         * we copy it to the text fragment we will allocate only as many bytes
         * as are required to hold the actual nukmber of Unicode values we
         * decoded, and this temporary buffer will be discarded.
         */
        penum->TextBuffer = (unsigned short *)gs_malloc(tdev->memory->stable_memory,
            pte->text.size * 4, sizeof(unsigned short), "txtwrite temporary text buffer");
        if (!penum->TextBuffer)
            return gs_note_error(gs_error_VMerror);
        penum->Widths = (float *)gs_malloc(tdev->memory->stable_memory,
            pte->text.size, sizeof(float), "txtwrite temporary widths array");
        if (!penum->Widths)
            return gs_note_error(gs_error_VMerror);
    }
    {
        switch (font->FontType) {
        case ft_CID_encrypted:
        case ft_CID_TrueType:
        case ft_composite:
              code = txtwrite_process_cmap_text(pte);
            break;
        case ft_encrypted:
        case ft_encrypted2:
        case ft_TrueType:
        case ft_user_defined:
        case ft_PCL_user_defined:
        case ft_GL2_stick_user_defined:
        case ft_GL2_531:
            code = txtwrite_process_plain_text(pte);
            break;
        default:
	  return_error(gs_error_rangecheck);
            break;
        }
        if (code == 0) {
            if (font_base->FontBBox.p.x != font_base->FontBBox.q.x ||
                font_base->FontBBox.p.y != font_base->FontBBox.q.y) {
                gs_point p0, p1, p2, p3;
                gs_matrix m;

                m = ctm_only(pte->pgs);
                m.tx = m.ty = fixed2float(0);
                gs_matrix_multiply(&font_base->FontMatrix, &m, &m);
                gs_point_transform(font_base->FontBBox.p.x, font_base->FontBBox.p.y, &m, &p0);
                gs_point_transform(font_base->FontBBox.p.x, font_base->FontBBox.q.y, &m, &p1);
                gs_point_transform(font_base->FontBBox.q.x, font_base->FontBBox.p.y, &m, &p2);
                gs_point_transform(font_base->FontBBox.q.x, font_base->FontBBox.q.y, &m, &p3);
                penum->text_state->FontBBox_bottomleft.x = min(min(p0.x, p1.x), min(p1.x, p2.x));
                penum->text_state->FontBBox_topright.x = max(max(p0.x, p1.x), max(p1.x, p2.x));
                penum->text_state->FontBBox_bottomleft.y = min(min(p0.y, p1.y), min(p1.y, p2.y));
                penum->text_state->FontBBox_topright.y = max(max(p0.y, p1.y), max(p1.y, p2.y));
            }
            code = txt_shift_text_currentpoint(penum, &penum->returned.total_width);
            if (code != 0)
                return code;

            code = txt_add_fragment(tdev, penum);
        }
    }
    return code;
}

/* Begin processing text. */

/* Define the auxiliary procedures for text processing. */
static int
textw_text_resync(gs_text_enum_t *pte, const gs_text_enum_t *pfrom)
{
    return gs_text_resync(pte, pfrom);
}
static bool
textw_text_is_width_only(const gs_text_enum_t *pte)
{
    return false;
}
static int
textw_text_current_width(const gs_text_enum_t *pte, gs_point *pwidth)
{
    return gs_text_current_width(pte, pwidth);
}
static int
textw_text_set_cache(gs_text_enum_t *pte, const double *pw,
                   gs_text_cache_control_t control)
{
    textw_text_enum_t *const penum = (textw_text_enum_t *)pte;

    switch (control) {
        case TEXT_SET_CHAR_WIDTH:
        case TEXT_SET_CACHE_DEVICE:
            return gs_text_set_cache(pte, pw, control);
        case TEXT_SET_CACHE_DEVICE2:
            if (penum->cdevproc_callout) {
                memcpy(penum->cdevproc_result, pw, sizeof(penum->cdevproc_result));
                return 0;
            }
            return gs_text_set_cache(pte, pw, control);
        default:
            return_error(gs_error_rangecheck);
    }
    return 0;
}

static int
textw_text_retry(gs_text_enum_t *pte)
{
    return gs_text_retry(pte);
}
static void
textw_text_release(gs_text_enum_t *pte, client_name_t cname)
{
    textw_text_enum_t *const penum = (textw_text_enum_t *)pte;
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) pte->dev;

    /* Free the working buffer where the Unicode was assembled from the enumerated text */
    if (penum->TextBuffer)
        gs_free(tdev->memory, penum->TextBuffer, 1, penum->TextBufferIndex, "txtwrite free temporary text buffer");
    if (penum->Widths)
        gs_free(tdev->memory, penum->Widths, sizeof(float), pte->text.size, "txtwrite free temporary widths array");
    /* If this is copied away when we complete the text enumeration succesfully, then
     * we set the pointer to NULL, if we get here with it non-NULL , then there was
     * an error.
     */
    if (penum->text_state)
        gs_free(tdev->memory, penum->text_state, 1, sizeof(penum->text_state), "txtwrite free text state");

    gs_text_release(pte, cname);
}

/* This is the list of methods for the text enumerator */
static const gs_text_enum_procs_t textw_text_procs = {
    textw_text_resync, textw_text_process,
    textw_text_is_width_only, textw_text_current_width,
    textw_text_set_cache, textw_text_retry,
    textw_text_release
};

/* This device method gets called by the interpreter to deal with text. It
 * must create a text enumerator, which contains the methods for dealing with
 * the text itself. The interpreter will use the enumerator methods to deal with
 * the text, it won't refer to the device methods again for this text.
 */
static int
txtwrite_text_begin(gx_device * dev, gs_gstate * pgs,
                const gs_text_params_t * text, gs_font * font,
                gx_path * path, const gx_device_color * pdcolor,
                const gx_clip_path * pcpath,
                gs_memory_t * mem, gs_text_enum_t ** ppenum)
{
    gx_device_txtwrite_t *const tdev = (gx_device_txtwrite_t *) dev;
    textw_text_enum_t *penum;
    int code;

    /* If this is a stringwidth, we must let the default graphics library code handle it
     * in case there is no current point (this can happen if this is the first operation
     * we get, the current font is a CIDFont, and its descendant is a substitute type 1
     * font). If there is no current point we throw an error in text_process and that
     * messes up all the font handling. The test below is copied from pdfwrite
     * (gdev_pdf_text_begin) and seems to do the job.
     */
    if ((!(text->operation & TEXT_DO_DRAW) && pgs->text_rendering_mode != 3)
                    || path == 0 || !path_position_valid(path))
            return gx_default_text_begin(dev, pgs, text, font, path, pdcolor,
                                         pcpath, mem, ppenum);
    /* Allocate and initialize one of our text enumerators. */
    rc_alloc_struct_1(penum, textw_text_enum_t, &st_textw_text_enum, mem,
                      return_error(gs_error_VMerror), "gdev_textw_text_begin");
    penum->rc.free = rc_free_text_enum;
    penum->charproc_accum = false;
    penum->cdevproc_callout = false;
    penum->returned.total_width.x = penum->returned.total_width.y = 0;
    penum->TextBuffer = NULL;
    penum->TextBufferIndex = 0;
    penum->Widths = NULL;
    /* The enumerator's text_release method frees this memory */
    penum->text_state = (text_list_entry_t *)gs_malloc(tdev->memory->stable_memory, 1,
            sizeof(text_list_entry_t), "txtwrite alloc text state");
    if (!penum->text_state)
        return gs_note_error(gs_error_VMerror);
    memset(penum->text_state, 0x00, sizeof(text_list_entry_t));

    code = gs_text_enum_init((gs_text_enum_t *)penum, &textw_text_procs,
                             dev, pgs, text, font, path, pdcolor, pcpath, mem);
    if (code < 0) {
        /* Belt and braces; I'm not certain this is required, but its safe */
        gs_free(tdev->memory, penum->text_state, 1, sizeof(text_list_entry_t), "txtwrite free text state");
        penum->text_state = NULL;
        gs_free_object(mem, penum, "textwrite_text_begin");
        return code;
    }

    code = gx_path_current_point(penum->path, &penum->origin);
    if (code != 0)
       return code;

    *ppenum = (gs_text_enum_t *)penum;

    return 0;
}

static int
txtwrite_strip_copy_rop(gx_device * dev,
                    const byte * sdata, int sourcex, uint sraster,
                    gx_bitmap_id id,
                    const gx_color_index * scolors,
                    const gx_strip_bitmap * textures,
                    const gx_color_index * tcolors,
                    int x, int y, int w, int h,
                    int phase_x, int phase_y, gs_logical_operation_t lop)
{
    return 0;
}

int
txtwrite_dev_spec_op(gx_device *pdev, int dev_spec_op, void *data, int size)
{
    switch (dev_spec_op) {
        case gxdso_get_dev_param:
            {
                int code;
                dev_param_req_t *request = (dev_param_req_t *)data;
                code = txtwrite_get_param(pdev, request->Param, request->list);
                if (code != gs_error_undefined)
                    return code;
            }
    }
    return gx_default_dev_spec_op(pdev, dev_spec_op, data, size);
}
