/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* XPS interpreter - truetype font support */

#include "ghostxps.h"

#include <gxfont.h>
#include "xpsfapi.h"

/*
 * Some extra TTF parsing magic that isn't covered by the graphics library.
 */

static inline int u16(const byte *p)
{
        return (p[0] << 8) | p[1];
}

static inline int u32(const byte *p)
{
        return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static const char *pl_mac_names[258] = {
    ".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl",
    "numbersign", "dollar", "percent", "ampersand", "quotesingle", "parenleft",
    "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash",
    "zero", "one", "two", "three", "four", "five", "six", "seven", "eight",
    "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at",
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
    "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft",
    "backslash", "bracketright", "asciicircum", "underscore", "grave", "a",
    "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p",
    "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar",
    "braceright", "asciitilde", "Adieresis", "Aring", "Ccedilla", "Eacute",
    "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex",
    "adieresis", "atilde", "aring", "ccedilla", "eacute", "egrave",
    "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis",
    "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde",
    "uacute", "ugrave", "ucircumflex", "udieresis", "dagger", "degree", "cent",
    "sterling", "section", "bullet", "paragraph", "germandbls", "registered",
    "copyright", "trademark", "acute", "dieresis", "notequal", "AE", "Oslash",
    "infinity", "plusminus", "lessequal", "greaterequal", "yen", "mu",
    "partialdiff", "summation", "product", "pi", "integral", "ordfeminine",
    "ordmasculine", "Omega", "ae", "oslash", "questiondown", "exclamdown",
    "logicalnot", "radical", "florin", "approxequal", "Delta", "guillemotleft",
    "guillemotright", "ellipsis", "nonbreakingspace", "Agrave", "Atilde",
    "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright",
    "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis",
    "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl",
    "daggerdbl", "periodcentered", "quotesinglbase", "quotedblbase",
    "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis",
    "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute",
    "Ocircumflex", "apple", "Ograve", "Uacute", "Ucircumflex", "Ugrave",
    "dotlessi", "circumflex", "tilde", "macron", "breve", "dotaccent", "ring",
    "cedilla", "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron",
    "scaron", "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute",
    "yacute", "Thorn", "thorn", "minus", "multiply", "onesuperior",
    "twosuperior", "threesuperior", "onehalf", "onequarter", "threequarters",
    "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla", "scedilla",
    "Cacute", "cacute", "Ccaron", "ccaron", "dcroat"
};

/*
 * A bunch of callback functions that the ghostscript
 * font machinery will call. The most important one
 * is the build_char function. These are specific to
 * truetype (loca/glyf) flavored opentypes.
 */

static unsigned int
xps_true_get_glyph_index(gs_font_type42 *pfont42, gs_glyph glyph)
{
    /* identity */
    return glyph;
}

static int
xps_true_callback_string_proc(gs_font_type42 *p42, ulong offset, uint length, const byte **pdata)
{
    xps_font_t *font = p42->client_data;
    if (offset + length > font->length)
    {
        *pdata = NULL;
        return gs_throw2(-1, "font data access out of bounds (offset=%lu size=%u)", offset, length);
    }
    *pdata = font->data + offset;
    return 0;
}

static gs_glyph
xps_true_callback_encode_char(gs_font *pfont, gs_char chr, gs_glyph_space_t spc)
{
    xps_font_t *font = pfont->client_data;
    int value;

    value = xps_encode_font_char(font, chr);
    if (value == 0)
        return GS_NO_GLYPH;
    return value;
}

static int
xps_true_callback_decode_glyph(gs_font *pfont, gs_glyph glyph, int ch, ushort *unicode_return, unsigned int length)
{
    xps_font_t *font = pfont->client_data;
    char *ur = (char *)unicode_return;
    int u;

    if (length == 0)
        return 2;
    u = xps_decode_font_char(font, glyph);
    /* Unfortunate assumptions in the pdfwrite code mea that we have to return the
     * value as a big-endian short, no matter what platform we are on
     */
    ur[1] = u & 0xff;
    ur[0] = u >> 8;
    return 2;
}

static int
xps_true_callback_glyph_name(gs_font *pfont, gs_glyph glyph, gs_const_string *pstr)
{
    /* This function is copied verbatim from plfont.c */

    int table_length;
    int table_offset;

    ulong format;
    int numGlyphs;
    uint glyph_name_index;
    const byte *postp; /* post table pointer */
    xps_font_t *font = pfont->client_data;

    if (glyph >= GS_MIN_GLYPH_INDEX) {
        glyph -= GS_MIN_GLYPH_INDEX;
    }

    /* guess if the font type is not truetype */
    if ( pfont->FontType != ft_TrueType )
    {
        glyph -= 29;
        if (glyph < 258 )
        {
            pstr->data = (byte*) pl_mac_names[glyph];
            pstr->size = strlen((char*)pstr->data);
            return 0;
        }
        else
        {
            return gs_throw1(-1, "glyph index %lu out of range", (ulong)glyph);
        }
    }

    table_offset = xps_find_sfnt_table((xps_font_t*)pfont->client_data, "post", &table_length);

    /* no post table */
    if (table_offset < 0)
        return gs_throw(-1, "no post table");

    /* this shoudn't happen but... */
    if ( table_length == 0 )
        return gs_throw(-1, "zero-size post table");

    ((gs_font_type42 *)pfont)->data.string_proc((gs_font_type42 *)pfont,
                                                table_offset, table_length, &postp);
    format = u32(postp);

    /* Format 1.0 (mac encoding) is a simple table see the TT spec.
     * We don't implement this because we don't see it in practice.
     * Format 2.5 is deprecated.
     * Format 3.0 means that there is no post data in the font file.
     * We see this a lot but can't do much about it.
     * The only format we support is 2.0.
     */
    if ( format != 0x20000 )
    {
        /* Invent a name if we don't know the table format. */
        char buf[32];
        gs_snprintf(buf, sizeof(buf), "glyph%d", (int)glyph);

        /* Ugly hackery. see comment below, after 'not mac' this ends up as a memory leak.
         * The PostScript interpreter adds the strings it creates to the PostScript name table
         * which is cleared and freed at EOJ. Presumably because these functions were
         * written with PostScript in mind, there is no provision for there not to be a
         * persistent copy of the name data, so we have to make one, which means it leaks.
         */
        pstr->size = strlen(buf);
        pstr->data = gs_alloc_bytes(pfont->memory, pstr->size + 1, "glyph to name");
        if ( pstr->data == 0 )
            return -1;

        memset((byte *)pstr->data, 0x00, pstr->size + 1);
        memcpy((byte *)pstr->data, buf, pstr->size);
        return 0;
    }

    /* skip over the post header */
    numGlyphs = (int)u16(postp + 32);
    if ((int)glyph > numGlyphs - 1)
    {
        return gs_throw1(-1, "glyph index %lu out of range", (ulong)glyph);
    }

    /* glyph name index starts at post + 34 each entry is 2 bytes */
    glyph_name_index = u16(postp + 34 + (glyph * 2));

    /* this shouldn't happen */
    if ( glyph_name_index > 0x7fff )
        return gs_throw(-1, "post table format error");

    /* mac easy */
    if ( glyph_name_index < 258 )
    {
        /* dmprintf2(pfont->memory, "glyph name (mac) %d = %s\n", glyph, pl_mac_names[glyph_name_index]); */
        pstr->data = (byte*) pl_mac_names[glyph_name_index];
        pstr->size = strlen((char*)pstr->data);
        return 0;
    }

    /* not mac */
    else
    {
        byte *mydata;

        /* and here's the tricky part */
        const byte *pascal_stringp = postp + 34 + (numGlyphs * 2);

        /* 0 - 257 lives in the mac table above */
        glyph_name_index -= 258;

        /* The string we want is the index'th pascal string,
         * so we "hop" to each length byte "index" times. */
        while (glyph_name_index > 0)
        {
            pascal_stringp += ((int)(*pascal_stringp)+1);
            glyph_name_index--;
        }

        /* length byte */
        pstr->size = (int)(*pascal_stringp);

        /* + 1 is for the length byte */
        pstr->data = pascal_stringp + 1;

        /* sanity check */
        if ( pstr->data + pstr->size > postp + table_length || pstr->data - 1 < postp)
            return gs_throw(-1, "data out of range");

        /* sigh - we have to allocate a copy of the data - by the
           time a high level device makes use of it the font data
           may be freed.  Track the allocated memory in our
           font 'wrapper' so we can free it when we free tha font wrapper.
         */
        mydata = gs_alloc_bytes(pfont->memory, pstr->size + 1, "glyph to name");
        if ( mydata == 0 )
            return -1;
        memcpy(mydata, pascal_stringp + 1, pstr->size);
        pstr->data = mydata;

        mydata[pstr->size] = 0;

        if (font->names == NULL) {
            font->names = (char **)gs_alloc_bytes(pfont->memory, 256 * sizeof (char *), "names storage");
            if (font->names == NULL) {
                gs_free_object(pfont->memory, (byte *)pstr->data, "free string on error");
                pstr->data = NULL;
                pstr->size = 0;
                return -1;
            }
            font->max_name_index = 255;
            font->next_name_index = 0;
            memset(font->names, 0x00, 256 * sizeof (char *));
        }
        if (font->next_name_index > font->max_name_index) {
            char **temp = NULL;
            temp = (char **)gs_alloc_bytes(pfont->memory, (font->max_name_index + 256) * sizeof (char *), "names storage");
            if (temp == NULL) {
                gs_free_object(pfont->memory, (byte *)pstr->data, "free string on error");
                pstr->data = NULL;
                pstr->size = 0;
                return -1;
            }
            memset(temp, 0x00, (font->max_name_index + 256) * sizeof (char *));
            memcpy(temp, font->names, font->max_name_index * sizeof(char *));
            gs_free_object(pfont->memory, (void *)font->names, "realloc names storage");
            font->names = temp;
            font->max_name_index += 256;
        }
        font->names[font->next_name_index++] = (char *)pstr->data;
        return 0;
    }
}

static int
xps_true_callback_build_char(gs_show_enum *penum, gs_gstate *pgs, gs_font *pfont,
        gs_char chr, gs_glyph glyph)
{
    gs_font_type42 *p42 = (gs_font_type42*)pfont;
    const gs_rect *pbbox;
    float sbw[4], w2[6];
    gs_fixed_point saved_adjust;
    int code;

    /* dmprintf1(pfont->memory, "build char ttf %d\n", glyph); */

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
            (gs_text_enum_t*)penum, (gs_font*)p42,
            gs_show_in_charpath(penum) != cpm_show);
    if (code < 0)
        return code;

    /* Indicate that dropout prevention should be enabled by setting
        fill_adjust to the special value -1. */
    saved_adjust = pgs->fill_adjust;
    pgs->fill_adjust.x = -1;
    pgs->fill_adjust.y = -1;

    code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
    if (code < 0)
        return code;

    pgs->fill_adjust = saved_adjust;

    return 0;
}

/*
 * Initialize the ghostscript font machinery for a truetype
 * (type42 in postscript terminology) font.
 */

int
xps_init_truetype_font(xps_context_t *ctx, xps_font_t *font)
{
    int code = 0;

    font->font = (void*) gs_alloc_struct(ctx->memory, gs_font_type42, &st_gs_font_type42, "xps_font type42");
    if (!font->font)
        return gs_throw(gs_error_VMerror, "out of memory");

    /* no shortage of things to initialize */
    {
        gs_font_type42 *p42 = (gs_font_type42*) font->font;

        /* Common to all fonts: */

        p42->next = 0;
        p42->prev = 0;
        p42->memory = ctx->memory;

        p42->dir = ctx->fontdir; /* NB also set by gs_definefont later */
        p42->base = font->font; /* NB also set by gs_definefont later */
        p42->is_resource = false;
        gs_notify_init(&p42->notify_list, gs_memory_stable(ctx->memory));
        p42->id = gs_next_ids(ctx->memory, 1);

        p42->client_data = font; /* that's us */

        /* this is overwritten in grid_fit() */
        gs_make_identity(&p42->FontMatrix);
        gs_make_identity(&p42->orig_FontMatrix); /* NB ... original or zeroes? */

        p42->FontType = ft_TrueType;
        p42->BitmapWidths = false;
        p42->ExactSize = fbit_use_outlines;
        p42->InBetweenSize = fbit_use_outlines;
        p42->TransformedChar = fbit_use_outlines;
        p42->WMode = 0;
        p42->PaintType = 0;
        p42->StrokeWidth = 0;
        p42->is_cached = 0;

        p42->procs.define_font = gs_no_define_font;
        p42->procs.make_font = gs_no_make_font;
        p42->procs.font_info = gs_type42_font_info;
        p42->procs.same_font = gs_default_same_font;
        p42->procs.encode_char = xps_true_callback_encode_char;
        p42->procs.decode_glyph = xps_true_callback_decode_glyph;
        p42->procs.enumerate_glyph = gs_type42_enumerate_glyph;
        p42->procs.glyph_info = gs_type42_glyph_info;
        p42->procs.glyph_outline = gs_type42_glyph_outline;
        p42->procs.glyph_name = xps_true_callback_glyph_name;
        p42->procs.init_fstack = gs_default_init_fstack;
        p42->procs.next_char_glyph = gs_default_next_char_glyph;
        p42->procs.build_char = xps_true_callback_build_char;

        memset(p42->font_name.chars, 0, sizeof(p42->font_name.chars));
        xps_load_sfnt_name(font, (char*)p42->font_name.chars, sizeof(p42->font_name.chars));
        p42->font_name.size = strlen((char*)p42->font_name.chars);

        memset(p42->key_name.chars, 0, sizeof(p42->key_name.chars));
        strcpy((char*)p42->key_name.chars, (char*)p42->font_name.chars);
        p42->key_name.size = strlen((char*)p42->key_name.chars);

        /* Base font specific: */

        p42->FontBBox.p.x = 0;
        p42->FontBBox.p.y = 0;
        p42->FontBBox.q.x = 0;
        p42->FontBBox.q.y = 0;

        uid_set_UniqueID(&p42->UID, p42->id);

        p42->encoding_index = ENCODING_INDEX_UNKNOWN;
        p42->nearest_encoding_index = ENCODING_INDEX_ISOLATIN1;

        p42->FAPI = 0;
        p42->FAPI_font_data = 0;

        /* Type 42 specific: */

        p42->data.string_proc = xps_true_callback_string_proc;
        p42->data.proc_data = font;

        code = gs_type42_font_init(p42, font->subfontid);
        if (code < 0)
            return code;
        p42->data.get_glyph_index = xps_true_get_glyph_index;
    }

    if ((code = gs_definefont(ctx->fontdir, font->font)) < 0) {
        return(code);
    }

    code = xps_fapi_passfont (font->font, NULL, NULL, font->data, font->length);
    return code;
}
