/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gslt_font_int.h"

#include "gslt.h"
#include "gslt_font.h"

/*
 * A bunch of callback functions that the ghostscript
 * font machinery will call. The most important one
 * is the build_char function. These are specific to
 * truetype (loca/glyf) flavored opentypes.
 */

static gs_glyph
gslt_true_callback_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t spc)
{
    gslt_font_t *xf = pfont->client_data;
    int value;
    value = gslt_encode_font_char(xf, chr);
    if (value == 0)
        return gs_no_glyph;
    return value;
}

static gs_char
gslt_true_callback_decode_glyph(gs_font *p42, gs_glyph glyph, int ch)
{
    return GS_NO_CHAR;
}

static int
gslt_true_callback_glyph_name(gs_font *pf, gs_glyph glyph, gs_const_string *pstr)
{
    return 0;
}

static int
gslt_true_callback_string_proc(gs_font_type42 *p42, ulong offset, uint length, const byte **pdata)
{
    /* NB bounds check offset + length - use gs_object_size for memory
       buffers - if file read should fail */
    gslt_font_t *xf = p42->client_data;
//    ulong adjusted_offset = offset + ttc_offset;
    *pdata = xf->data + offset;
    return 0;
}

static int
gslt_true_callback_build_char(gs_text_enum_t *ptextenum, gs_state *pgs, gs_font *pfont,
        gs_char chr, gs_glyph glyph)
{
    gs_show_enum *penum = (gs_show_enum*)ptextenum;
    gs_font_type42 *p42 = (gs_font_type42*)pfont;
    const gs_rect *pbbox;
    float sbw[4], w2[6];
    int code;

    code = gs_type42_get_metrics(p42, glyph, sbw);
    if (code < 0)
        return code;

    w2[0] = sbw[2];
    w2[1] = sbw[3];

    pbbox =  &p42->FontBBox;
    w2[2] = pbbox->p.x;
    w2[3] = pbbox->p.y;
    w2[4] = pbbox->q.x;
    w2[5] = pbbox->q.y;
    
    dprintf6("  bbox (%g %g) %g %g %g %g\n", w2[0], w2[1], w2[2], w2[3], w2[4], w2[5]);

    /* Expand the bbox when stroking */
    if ( pfont->PaintType )
    {
        float expand = max(1.415, gs_currentmiterlimit(pgs)) * gs_currentlinewidth(pgs) / 2;
        w2[2] -= expand, w2[3] -= expand;
        w2[4] += expand, w2[5] += expand;
    }

    if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
        return code;

    if ( (code = gs_setcachedevice(penum, pgs, w2)) < 0 )
        return code;

    code = gs_type42_append(glyph, pgs,
	    gx_current_path(pgs),
	    ptextenum, (gs_font*)p42,
	    gs_show_in_charpath(penum) != cpm_show);
    if (code < 0)
        return code;

    code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
    if (code < 0)
        return code;

    return 0;
}

/*
 * Initialize the ghostscript font machinery for a truetype
 * (type42 in postscript terminology) font.
 */

int
gslt_init_truetype_font(gs_memory_t *mem, gs_font_dir *fontdir, gslt_font_t *xf)
{
    xf->font = (void*) gs_alloc_struct(mem, gs_font_type42, &st_gs_font_type42, "gslt_font type42");
    if (!xf->font)
        return gs_throw(-1, "out of memory");

    /* no shortage of things to initialize */
    {
        gs_font_type42 *p42 = (gs_font_type42*) xf->font;

        /* Common to all fonts: */

        p42->next = 0;
        p42->prev = 0;
        p42->memory = mem;

        p42->dir = fontdir; /* NB also set by gs_definefont later */
        p42->base = xf->font; /* NB also set by gs_definefont later */
        p42->is_resource = false;
        gs_notify_init(&p42->notify_list, gs_memory_stable(mem));
        p42->id = gs_next_ids(mem, 1);

        p42->client_data = xf; /* that's us */

        gs_make_identity(&p42->FontMatrix);
        gs_make_identity(&p42->orig_FontMatrix); /* NB ... original or zeroes? */

        p42->FontType = ft_TrueType;
        p42->BitmapWidths = true;
        p42->ExactSize = fbit_use_outlines;
        p42->InBetweenSize = fbit_use_outlines;
        p42->TransformedChar = fbit_use_outlines;
        p42->WMode = 0;
        p42->PaintType = 0;
        p42->StrokeWidth = 0;

        p42->procs.init_fstack = gs_default_init_fstack;
        p42->procs.next_char_glyph = gs_default_next_char_glyph;
        p42->procs.glyph_name = gslt_true_callback_glyph_name;
        p42->procs.decode_glyph = gslt_true_callback_decode_glyph;
        p42->procs.define_font = gs_no_define_font;
        p42->procs.make_font = gs_no_make_font;
        p42->procs.font_info = gs_default_font_info;
        p42->procs.glyph_info = gs_default_glyph_info;
        p42->procs.glyph_outline = gs_no_glyph_outline;
        p42->procs.encode_char = gslt_true_callback_encode_char;
        p42->procs.build_char = gslt_true_callback_build_char;

        p42->font_name.size = 0;
        p42->key_name.size = 0;

        /* Base font specific: */

        p42->FontBBox.p.x = 0;
        p42->FontBBox.p.y = 0;
        p42->FontBBox.q.x = 0;
        p42->FontBBox.q.y = 0;

        uid_set_UniqueID(&p42->UID, p42->id);

        p42->encoding_index = ENCODING_INDEX_UNKNOWN;
        p42->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;

        p42->FAPI = 0;
        p42->FAPI_font_data = 0;

        /* Type 42 specific: */

        p42->data.string_proc = gslt_true_callback_string_proc;
        p42->data.proc_data = xf;
        gs_type42_font_init(p42, xf->subfontid);
    }

    gs_definefont(fontdir, xf->font);

    return 0;
}

