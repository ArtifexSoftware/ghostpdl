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

/* $Id: gslt_font_api_test.c 2490 2006-08-08 22:33:47Z giles $ */
/* gslt OpenType font library example client */

#include <stdlib.h>
#include <stdio.h>


#include "gslt.h"
#include "gslt_font.h"

/*
 * Read a file from disk into memory.
 */

int readfile(char *filename, char **datap, int *lengthp)
{
    FILE *fp;
    int t, n;
    char *p;

    fp = fopen(filename, "rb");
    if (!fp) {
        printf("cannot open font file\n");
        return 1;
    }
    t = fseek(fp, 0, 2);
    if (t < 0)
    {
        fclose(fp);
        printf("cannot seek in font file\n");
        return 1;
    }

    n = ftell(fp);
    if (n < 0)
    {
        fclose(fp);
        printf("cannot tell in font file\n");
        return 1;
    }

    t = fseek(fp, 0, 0);
    if (t < 0)
    {
        fclose(fp);
        printf("cannot seek in font file\n");
        return 1;
    }

    p = malloc(n);
    if (!p)
    {
        fclose(fp);
        printf("out of memory\n");
        return 1;
    }

    t = fread(p, 1, n, fp);
    if (t != n)
    {
        free(p);
        fclose(fp);
        printf("cannot read font file data\n");
        return 1;
    }

    t = fclose(fp);
    if (t < 0)
    {
      printf("cannot close font file\n");
        /* ... continue anyway */
    }

    *lengthp = n;
    *datap = p;

    return 0;
}

/*
 * Test program.
 */

static int mymoveto(void *ctx, float x, float y)
{
    x *= 1000; y *= 1000;
    printf("%g %g moveto\n", x, y);
    return 0;
}

static int mylineto(void *ctx, float x, float y)
{
    x *= 1000; y *= 1000;
    printf("%g %g lineto\n", x, y);
    return 0;
}

static int mycurveto(void *ctx, float x0, float y0, float x1, float y1, float x2, float y2)
{
    x0 *= 1000; y0 *= 1000;
    x1 *= 1000; y1 *= 1000;
    x2 *= 1000; y2 *= 1000;
    printf("%g %g  %g %g  %g %g curveto\n", x0, y0, x1, y1, x2, y2);
    return 0;
}

static int myclosepath(void *ctx)
{
    printf("closepath\n");
    return 0;
}

int
main(int argc, const char *argv[])
{
    gs_memory_t *mem;
    gx_device *dev;
    gs_state *pgs;
    gs_font_dir *cache;
    gslt_font_t *font;
    gslt_outline_walker_t walk;
    gslt_glyph_bitmap_t slot;
    gslt_glyph_metrics_t mtx;
    gs_matrix ctm;
    int code;
    char *s;
    char *devicename;
    char *filename;
    int i, k, n, pid, eid, best;
    int subfontid;
    char *buf;
    int len;

    char *text;

    devicename = getenv("DEVICE");
    if (!devicename)
        devicename = "nullpage";

    mem = gslt_init_library();
    dev = gslt_init_device(mem, devicename);
    pgs = gslt_init_state(mem, dev);

    if (argc < 2)
    {
        filename = "/Users/tor/src/work/gslite/TrajanPro-Regular.otf";
        // filename = "/Users/tor/src/work/gslite/GenR102.TTF";
        // return gs_throw(1, "usage: gslt_font_api_test font.otf [subfontid]");
    }
    else
    {
        filename = argv[1];
    }

    subfontid = 0;
    if (argc == 3)
        subfontid = atoi(argv[2]);

    printf("Loading font '%s' subfont %d.\n", filename, subfontid);

    n = readfile(filename, &buf, &len);
    if (n < 0) {
        printf("cannot read font file '%s'", filename);
        return 1;
    }

    /*
     * Set up ghostscript library
     */

    // gslt_get_device_param(mem, dev, "Name");
    gslt_set_device_param(mem, dev, "OutputFile", "-");

    /*
     * Create a font cache
     */

    cache = gslt_new_font_cache(mem);
    if (!cache) {
        printf("cannot create font cache\n");
        return 1;
    }

    /*
     * Create the font and select an encoding
     */

    font = gslt_new_font(mem, cache, buf, len, subfontid);
    if (!font) {
        printf("cannot create font");
        return 1;
    }

    static struct { int pid, eid; } xps_cmap_list[] =
    {
        { 3, 10 },	/* Unicode with surrogates */
        { 3, 1 },	/* Unicode without surrogates */
        { 3, 5 },	/* Wansung */
        { 3, 4 },	/* Big5 */
        { 3, 3 },	/* Prc */
        { 3, 2 },	/* ShiftJis */
        { 3, 0 },	/* Symbol */
        // { 0, * }, -- Unicode (deprecated)
        { 1, 0 },
        { -1, -1 },
    };

    n = gslt_count_font_encodings(font);
    best = -1;
    for (k = 0; xps_cmap_list[k].pid != -1; k++)
    {
        for (i = 0; i < n; i++)
        {
            gslt_identify_font_encoding(font, i, &pid, &eid);
            if (pid == xps_cmap_list[k].pid && eid == xps_cmap_list[k].eid)
                goto found_cmap;
        }
    }
    gs_throw(-1, "could not find a suitable cmap");
    return 1;

found_cmap:
    printf("found a cmap to use %d %d\n", pid, eid);
    gslt_select_font_encoding(font, i);

    /*
     * Test outline extraction.
     */

    printf("walking the outline\n");

    walk.user = NULL;
    walk.moveto = mymoveto;
    walk.lineto = mylineto;
    walk.curveto = mycurveto;
    walk.closepath = myclosepath;

    code = gslt_outline_font_glyph(pgs, font, gslt_encode_font_char(font, 'I'), &walk);
    if (code < 0)
        printf("error in gslt_outline_font_glyph\n");

    /*
     * Test bitmap rendering.
     */

    printf("getting bitmaps\n");

    text = "Pack my box with five dozen liquor jugs!";

text = "This";

    ctm.xx = 100.0;
    ctm.xy = 0.0;
    ctm.yx = 0.0;
    ctm.yy = 100.0;
    ctm.tx = ctm.ty = 0.0;

    for (s = text; *s; s++)
    {
        int gid = gslt_encode_font_char(font, *s);

        if (s == text)
            gid = 2119;

        printf("char '%c' -> glyph %d\n", *s, gid);

        code = gslt_measure_font_glyph(pgs, font, gid, &mtx);
        if (code < 0)
        {
            printf("error in gslt_measure_font_glyph\n");
        }

        printf("glyph %3d: hadv=%f vadv=%f vorg=%f ", gid, mtx.hadv, mtx.vadv, mtx.vorg);

        code = gslt_render_font_glyph(pgs, font, &ctm, gid, &slot);
        if (code < 0)
        {
            printf("error in gslt_render_font_glyph\n");
            return 1;
        }

        printf(" -> %dx%d+(%d,%d)\n",
                slot.w, slot.h,
                slot.lsb, slot.top);

        gslt_release_font_glyph(mem, &slot);
    }

    /*
     * Clean up.
     */
    gslt_free_font(mem, font);
    gslt_free_font_cache(mem, cache);
    free(buf);

    gslt_free_state(mem, pgs);
    gslt_free_device(mem, dev);
    gslt_free_library(mem);

    gslt_alloc_print_leaks();

    return 0;
}
