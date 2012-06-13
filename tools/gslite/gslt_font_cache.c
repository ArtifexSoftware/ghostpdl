/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include "gslt_font_int.h"

#include "gslt.h"
#include "gslt_font.h"

/*
 * The font cache is a gs_font_dir.
 * It has some parameters that need to be set,
 * do so here.
 */

gs_font_dir *
gslt_new_font_cache(gs_memory_t *mem)
{
    gs_font_dir *fontdir;

    int smax = 50;         /* number of scaled fonts */
    int bmax = 500000;     /* space for cached chars */
    int mmax = 200;        /* number of cached font/matrix pairs */
    int cmax = 5000;       /* number of cached chars */
    int upper = 32000;     /* max size of a single cached char */

    fontdir = gs_font_dir_alloc2_limits(mem, mem, smax, bmax, mmax, cmax, upper);
    if (!fontdir)
    {
        gs_throw(-1, "cannot gs_font_dir_alloc2_limits()");
        return NULL;
    }

    gs_setaligntopixels(fontdir, 1); /* no subpixels */
    gs_setgridfittt(fontdir, 3); /* see gx_ttf_outline for values */

    return fontdir;
}

void
gslt_free_font_cache(gs_memory_t *mem, gs_font_dir *fontdir)
{
    gs_free_object(mem, fontdir, "gs_font_dir");
}

/*
 * Find the offset and length of an SFNT table.
 * Return -1 if no table by the specified name is found.
 */

int
gslt_find_sfnt_table(gslt_font_t *xf, char *name, int *lengthp)
{
    byte *directory;
    int offset;
    int ntables;
    int i;

    if (xf->length < 12)
        return -1;

    if (!memcmp(xf->data, "ttcf", 4))
    {
        int nfonts = u32(xf->data + 8);
        if (xf->subfontid < 0 || xf->subfontid >= nfonts)
            return -1;
        offset = u32(xf->data + 12 + xf->subfontid * 4);
    }
    else
    {
        offset = 0;
    }

    ntables = u16(xf->data + offset + 4);
    if (xf->length < offset + 12 + ntables * 16)
        return -1;

    for (i = 0; i < ntables; i++)
    {
        byte *entry = xf->data + offset + 12 + i * 16;
        if (!memcmp(entry, name, 4))
        {
            if (lengthp)
                *lengthp = u32(entry + 12);
            return u32(entry + 8);
        }
    }

    return -1;
}

/*
 * Load and initialize a font struct from a file.
 */

gslt_font_t *
gslt_new_font(gs_memory_t *mem, gs_font_dir *fontdir, char *buf, int buflen, int subfontid)
{
    gslt_font_t *xf;
    int t;

    xf = (void*) gs_alloc_bytes(mem, sizeof(gslt_font_t), "gslt_font struct");
    if (!xf)
    {
        gs_throw(-1, "out of memory");
        return NULL;
    }

    xf->data = (byte*)buf;
    xf->length = buflen;
    xf->font = NULL;

    xf->subfontid = subfontid;
    xf->cmaptable = 0;
    xf->cmapsubcount = 0;
    xf->cmapsubtable = 0;
    xf->usepua = 0;

    xf->cffdata = 0;
    xf->cffend = 0;
    xf->gsubrs = 0;
    xf->subrs = 0;
    xf->charstrings = 0;

    if (memcmp(xf->data, "OTTO", 4) == 0)
        t = gslt_init_postscript_font(mem, fontdir, xf);
    else if (memcmp(xf->data, "\0\1\0\0", 4) == 0)
        t = gslt_init_truetype_font(mem, fontdir, xf);
    else if (memcmp(xf->data, "true", 4) == 0)
        t = gslt_init_truetype_font(mem, fontdir, xf);
    else if (memcmp(xf->data, "ttcf", 4) == 0)
        t = gslt_init_truetype_font(mem, fontdir, xf);
    else
    {
        gslt_free_font(mem, xf);
        gs_throw(-1, "not an opentype font");
        return NULL;
    }

    if (t < 0)
    {
        gslt_free_font(mem, xf);
        gs_rethrow(-1, "cannot init font");
        return NULL;
    }

    t = gslt_load_sfnt_cmap(xf);
    if (t < 0)
    {
        errprintf("warning: no cmap table found in font\n");
    }

    return xf;
}

void
gslt_free_font(gs_memory_t *mem, gslt_font_t *xf)
{
    gs_font_finalize(xf->font);
    gs_free_object(mem, xf->font, "font object");
    gs_free_object(mem, xf, "gslt_font struct");
}
