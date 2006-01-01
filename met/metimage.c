/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id: */

/* image related elements */

/* element constructor */

#include "memory_.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gp.h"
#include "metcomplex.h"
#include "metelement.h"
#include "metutil.h"
#include "math_.h"
#include "metimage.h"
#include "ctype_.h"
#include "gsimage.h"
#include "gscspace.h"
#include "gsptype1.h"
#include <stdlib.h> /* nb for atof */
#include "strimpl.h"
#include "scommon.h"
#include "jpeglib_.h"           /* for jpeg filter */
#include "sdct.h"
#include "sjpeg.h"


private int
ImageBrush_cook(void **ppdata, met_state_t *ms, const char *el, const char **attr)
{
    CT_ImageBrush *aImageBrush = 
        (CT_ImageBrush *)gs_alloc_bytes(ms->memory,
                                     sizeof(CT_ImageBrush),
                                       "ImageBrush_cook");
    int i;

    memset(aImageBrush, 0, sizeof(CT_ImageBrush));

#define MYSET(field, value)                                                   \
    met_cmp_and_set((field), attr[i], attr[i+1], (value))

    /* parse attributes, filling in the zeroed out C struct */
    for(i = 0; attr[i]; i += 2) {
        if (!MYSET(&aImageBrush->Key, "Key"))
            ;
        else if (!MYSET(&aImageBrush->Transform, "Transform"))
            ;
        else if (!MYSET(&aImageBrush->Viewbox, "Viewbox"))
            ;
        else if (!MYSET(&aImageBrush->Viewport, "Viewport"))
            ;
        else if (!MYSET(&aImageBrush->Fill, "Fill"))
            ;
        else if (!MYSET(&aImageBrush->TileMode, "TileMode"))
            ;
        else if (!MYSET(&aImageBrush->ViewboxUnits, "ViewboxUnits"))
            ;
        else if (!MYSET(&aImageBrush->ViewportUnits, "ViewportUnits"))
            ;
        else if (!MYSET(&aImageBrush->ImageSource, "ImageSource"))
            ;
        else {
            dprintf2(ms->memory, "unsupported attribute %s=%s\n",
                     attr[i], attr[i+1]);
        }
    }

    /* copy back the data for the parser. */
    *ppdata = aImageBrush;
    return 0;
}

private int
stream_error(stream_state * st, const char *str)
{
    dprintf1(st->memory, "stream error %s\n", str );
    return 0;
}

/* temporary code to read raster format.  reads the whole works.  NB
   uses stdio FILE, needs to read from zip archive. */
private int
readdata(gs_memory_t *mem, ST_Name ImageSource, byte **bufp, int *lenp)
{
    FILE *in;
    byte *buf;
    ulong len;
    
    /* open the resource */
    if ((in = fopen(ImageSource, gp_fmode_rb)) == NULL) {
        dprintf1(mem, "%s not found\n", ImageSource);
        return -1;
    }

    /* get the whole thing for now */
    len = (fseek(in, 0L, SEEK_END), ftell(in));

    /* nb error checking */
    rewind(in);
    buf = gs_alloc_bytes(mem, len, "readdata");
    if (!buf)
        return -1;

    fread(buf, 1, len, in);
    fclose(in);
    *bufp = buf;
    *lenp = len;
    return 0;
}

private int
decodejpeg(gs_memory_t *mem, byte *rbuf, int rlen, met_image_t *g_image)
{
    jpeg_decompress_data jddp;
    stream_DCT_state state;
    stream_cursor_read rp;
    stream_cursor_write wp;
    int code;
    int wlen;
    byte *wbuf;
    /* NB this is placed before set defaults when the memory parameter
       brain damage is removed */
    state.memory = mem;

    s_DCTD_template.set_defaults((stream_state*)&state);

    state.template = &s_DCTD_template;
    state.report_error = stream_error;
    state.min_left = 0;
    state.error_string[0] = 0;

    state.jpeg_memory = mem;
    state.data.decompress = &jddp;

    jddp.template = s_DCTD_template;
    jddp.memory = mem;
    jddp.scanline_buffer = NULL;

    if ((code = gs_jpeg_create_decompress(&state)) < 0)
        return -1;

    s_DCTD_template.init((stream_state*)&state);

    rp.ptr = rbuf - 1;
    rp.limit = rbuf + rlen - 1;

    /* read the header only by not having a write buffer */
    wp.ptr = 0;
    wp.limit = 0;

    code = s_DCTD_template.process(mem, (stream_state*)&state, &rp, &wp, true);
    if (code != 1)
        return -1;

    g_image->width = jddp.dinfo.output_width;
    g_image->height = jddp.dinfo.output_height;
    g_image->comps = jddp.dinfo.output_components;
    g_image->bits = 8;

    wlen = jddp.dinfo.output_width *
        jddp.dinfo.output_height *
        jddp.dinfo.output_components;

    wbuf = gs_alloc_bytes(mem, wlen, "decodejpeg");
    if (!wbuf)
        return -1;

    g_image->samples = wbuf;

    wp.ptr = wbuf - 1;
    wp.limit = wbuf + wlen - 1;

    code = s_DCTD_template.process(mem, (stream_state*)&state, &rp, &wp, true);
    if (code != EOFC)
        return -1;
    return code;
}

private int
met_PaintPattern(const gs_client_color *pcc, gs_state *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    const met_pattern_t *pmpat = ppat->client_data;
    const met_image_t *pmim = pmpat->raster_image;
    const byte *pdata = pmim->samples;
    gs_image_enum *penum;
    gs_color_space color_space;
    gs_image_t image;
    int num_components = 3; /* NB */
    int code;

    gs_cspace_init_DeviceRGB(gs_state_memory(pgs), &color_space);
    gs_image_t_init(&image, &color_space);
    image.ColorSpace = &color_space;
    image.BitsPerComponent = pmim->bits;
    image.Width = pmim->width;
    image.Height = pmim->height;
    penum = gs_image_enum_alloc(gs_state_memory(pgs), "met_PaintPattern");
    if (!penum)
        return -1;
    if ((code = gs_image_init(penum, &image, false, pgs)) < 0)
        return code;
    {
        uint used;
        uint depth = image.BitsPerComponent * pmim->comps;
        /* NB - rounding alignement ?? */
        uint imbytes =
            ((image.Width * depth + 7) / 8) * image.Height;

        if ((code = gs_image_next(penum, pdata, imbytes, &used)) < 0)
            return code;
        if (imbytes != used) {
            dprintf(gs_state_memory(pgs), "image data error\n");
            return -1;
        }
    }
    return -0;
}

/* NB for the demo we render and set the pattern in the graphics
   state */
private int
make_pattern(ST_Name ImageSource, met_pattern_t *metpat, met_state_t *ms)
{
    byte *rbuf; /* raster buffer */
    int rlen;
    int code;
    gs_client_pattern gspat;
    gs_client_color gscolor;
    gs_memory_t *mem = ms->memory;
    code = readdata(mem, ImageSource, &rbuf, &rlen);
    if (code < 0)
        return code;
    if (rbuf[0] == 0xff && rbuf[1] == 0xd8)
        decodejpeg(mem, rbuf, rlen, metpat->raster_image);
    else if (memcmp(rbuf, "\211PNG\r\n\032\n", 8) == 0) {
        dprintf(mem, "png not implemented\n");
        return -1;
    } else {
        dprintf(mem, "unknown file format");
        return -1;
    }
    gs_pattern1_init(&gspat);
    uid_set_UniqueID(&gspat.uid, gs_next_ids(mem, 1));
    gspat.PaintType = 1;
    gspat.TilingType = 1;
    gspat.BBox = metpat->Viewbox;
    gspat.XStep = metpat->Viewbox.q.x - metpat->Viewbox.p.x;
    gspat.YStep = metpat->Viewbox.q.y - metpat->Viewbox.p.y;
    gspat.PaintProc = met_PaintPattern;
    gspat.client_data = (void *)metpat;
    {
        gs_matrix mat;
        gs_color_space cs;
        gs_state *pgs = ms->pgs;
        gs_gsave(pgs);
        /* NB figure out the pattern matrix */
        gs_make_identity(&mat);
        gs_setmatrix(pgs, &mat);
        gs_cspace_init_DeviceRGB(mem, &cs);
        gs_setcolorspace(pgs, &cs);
        gs_makepattern(&gscolor, &gspat, &mat, pgs, NULL);
        gs_grestore(pgs);
        gs_setpattern(pgs, &gscolor);
    }
    /* !!!! NB !!!! */
    extern bool patternset;
    patternset = true;
    return -1;

}



private bool is_Data_delimeter(char b) 
{
    return (b == ',') || (isspace(b));
}

/* action associated with this element */
private int
ImageBrush_action(void *data, met_state_t *ms)
{
    /* create the pattern dictionary entry */
    CT_ImageBrush *aImageBrush = data;
    met_pattern_t *pat = 
        (met_pattern_t *)gs_alloc_bytes(ms->memory,
        sizeof(met_pattern_t), "ImageBrush_action");
    met_image_t *pim =  (met_image_t *)gs_alloc_bytes(ms->memory,
        sizeof(met_image_t), "ImageBrush_action");

    if (!pat || !pim)
        return -1;

    pat->raster_image = pim;
    /* nb gcc'isms ahead */
    if (aImageBrush->Transform) {
        char transstring[strlen(aImageBrush->Transform)];
        strcpy(transstring, aImageBrush->Transform);
        /* nb wasteful */
        char *args[strlen(aImageBrush->Transform)];
        char **pargs = args;
        met_split(transstring, args, is_Data_delimeter);
        /* nb checking */
        pat->Transform.xx = atof(pargs[0]);
        pat->Transform.xy = atof(pargs[1]);
        pat->Transform.yx = atof(pargs[2]);
        pat->Transform.yy = atof(pargs[3]);
        pat->Transform.tx = atof(pargs[4]);
        pat->Transform.ty = atof(pargs[5]);
    }
    {
        char viewstring[strlen(aImageBrush->Viewbox)];
        strcpy(viewstring, aImageBrush->Viewbox);
        /* nb wasteful */
        char *args[strlen(aImageBrush->Viewbox)];
        char **pargs = args;
        met_split(viewstring, args, is_Data_delimeter);
        pat->Viewbox.p.x = atof(pargs[0]);
        pat->Viewbox.p.y = atof(pargs[1]);
        pat->Viewbox.q.x = pat->Viewbox.p.x + atof(pargs[2]);
        pat->Viewbox.q.y = pat->Viewbox.p.y + atof(pargs[3]);
    }
    make_pattern(aImageBrush->ImageSource, pat, ms);
    
    return 0;
}

private int
ImageBrush_done(void *data, met_state_t *ms)
{
        
    gs_free_object(ms->memory, data, "ImageBrush_done");
    return 0; /* incomplete */
}


const met_element_t met_element_procs_ImageBrush = {
    "ImageBrush",
    ImageBrush_cook,
    ImageBrush_action,
    ImageBrush_done
};

