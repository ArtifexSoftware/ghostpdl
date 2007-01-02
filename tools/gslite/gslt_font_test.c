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

/* $Id: gslt_font_api_test.c 2490 2006-08-08 22:33:47Z giles $ */
/* gslt OpenType font library example client */

#include <stdlib.h>
#include <stdio.h>

#include <gserror.h>

#include "gslt.h"
#include "gslt_font.h"

/* these are not strictly needed */
extern void gs_erasepage(gs_state *pgs);
extern void gs_moveto(gs_state *pgs, double, double);
extern void gs_output_page(gs_state *pgs, int, int);
extern char gs_debug[];

/*
 * Read a file from disk into memory.
 */

int readfile(char *filename, char **datap, int *lengthp)
{
    FILE *fp;
    int t, n;
    char *p;

    fp = fopen(filename, "rb");
    if (!fp)
        return gs_throw(-1, "cannot open font file");

    t = fseek(fp, 0, 2);
    if (t < 0)
    {
        fclose(fp);
        return gs_throw(-1, "cannot seek in font file");
    }

    n = ftell(fp);
    if (n < 0)
    {
        fclose(fp);
        return gs_throw(-1, "cannot tell in font file");
    }

    t = fseek(fp, 0, 0);
    if (t < 0)
    {
        fclose(fp);
        return gs_throw(-1, "cannot seek in font file");
    }

    p = malloc(n);
    if (!p)
    {
        fclose(fp);
        return gs_throw(-1, "out of memory");
    }

    t = fread(p, 1, n, fp);
    if (t != n)
    {
        free(p);
        fclose(fp);
        return gs_throw(-1, "cannot read font file data");
    }

    t = fclose(fp);
    if (t < 0)
    {
        gs_throw(-1, "cannot close font file");
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
    int i, n, pid, eid, best;
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
	// return gs_throw(1, "usage: gslt_font_api_test font.otf");
    }
    else
    {
        filename = argv[argc-1];
    }
    
    n = readfile(filename, &buf, &len);
    if (n < 0)
        return gs_rethrow1(1, "cannot read font file '%s'", filename);

    gs_debug['k'] = 1; /* debug character cache machinery */

    /*
     * Set up ghostscript library
     */

    // gslt_get_device_param(mem, dev, "Name");
    gslt_set_device_param(mem, dev, "OutputFile", "-");

    // so we see what device calls are made
    gs_erasepage(pgs);
    gs_moveto(pgs, 5.0, 20.0);

    /*
     * Create a font cache
     */

    cache = gslt_new_font_cache(mem);
    if (!cache)
        return gs_rethrow(1, "cannot create font cache");

    /*
     * Create the font and select an encoding
     */

    font = gslt_new_font(mem, cache, buf, len, 0);
    if (!font)
        return gs_rethrow(1, "cannot create font");

    n = gslt_count_font_encodings(font);
    for (best = 0, i = 0; i < n; i++)
    {
        gslt_identify_font_encoding(font, i, &pid, &eid);
	printf("encoding %d/%d = ( %d %d )\n", i + 1, n, pid, eid);
        if (pid == 3 && eid == 0)
            best = i;
    }
    gslt_select_font_encoding(font, best);

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

text = "ii";

    ctm.xx = 100.0;
    ctm.xy = 0.0;
    ctm.yx = 0.0;
    ctm.yy = 100.0;
    ctm.tx = ctm.ty = 0.0;

    for (s = text; *s; s++)
    {
        int gid = gslt_encode_font_char(font, *s);

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

    ctm.xx = 0.0;
    ctm.xy = 100.0;
    ctm.yx = -100.0;
    ctm.yy = 0.0;
    ctm.tx = ctm.ty = 0.0;

	gslt_release_font_glyph(mem, &slot);
    }

    /*
     * Clean up.
     */

    // show device.
    gs_output_page(pgs, 1, 1);

    gslt_free_font(mem, font);
    gslt_free_font_cache(mem, cache);
    free(buf);

    gslt_free_state(mem, pgs);
    gslt_free_device(mem, dev);
    gslt_free_library(mem);

    gslt_alloc_print_leaks();

    return 0;
}

