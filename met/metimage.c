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

/*$Id$*/

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
#include "zipparse.h"

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
 */
private int
readdata(gs_memory_t *mem, zip_state_t *pzip, ST_Name ImageSource, byte **bufp, int *lenp)
{
    byte *buf;
    ulong len = 0; 
    zip_part_t *part = find_zip_part_by_name(pzip, ImageSource);

    if (part == NULL) 
	return mt_throw1(-1,"Image part not found %s", ImageSource);

    len = zip_part_length(part);

    buf = gs_alloc_bytes(mem, len, "xps_readdata buffer");
    if ( buf == 0 ) {
	return -1;
    }

    zip_part_seek(part, 0, 0);
    if ( len != zip_part_read(buf, len, part) ) {
	return mt_throw(-1, "zip_part_read len wrong");
    }

    *bufp = buf;
    *lenp = len;

#ifndef NO_PNG_TO_JPEG_HACK
    /* NB: this converts PNG to JPEGS on the fly using /tmp/foo.png and /tmp/foo.jpg 
     * NB: also converts TIFF to jpg 
     * NB: file name extension detection is weak, 
     * file data inspection would be much better.
     */

    {
	const char *ctif_convert = "/usr/bin/convert /tmp/foo.tif /tmp/foo.jpg";
	const char *ctif = "/tmp/foo.tif";
	const char *cpng_convert = "/usr/bin/convert /tmp/foo.png /tmp/foo.jpg";
	const char *cpng = "/tmp/foo.png";
	char *imgStr;
	char *convertStr;
	bool doit = false;

	if (0 == strncasecmp(&ImageSource[strlen(ImageSource) -5], "tiff", 4 )) {
	    imgStr = ctif;
	    convertStr = ctif_convert;
	    doit = true;
	}
#if 0 
	else 	if (0 == strncasecmp(&ImageSource[strlen(ImageSource) -4], "png", 3 )); {
	    imgStr = cpng;
	    convertStr = cpng_convert;
	    doit = true;
	}
#endif
	if (doit) {
	    FILE *in = fopen(imgStr, "w");
	    fwrite(buf, 1, len, in);
	    fclose(in);

	    system( convertStr );

	    in = fopen("/tmp/foo.jpg", "r");
	    if (in == NULL) 
		return mt_throw(-1, "/tmp/foo.jpg file open failed");
	    len = (fseek(in, 0L, SEEK_END), ftell(in));
	    rewind(in);
	    gs_free_object(mem, buf, "readdata");
	    buf = gs_alloc_bytes(mem, len, "readdata");
	    if (!buf)
		return -1;
	    fread(buf, 1, len, in);
	    fclose(in);    
	    *bufp = buf;
	    *lenp = len;
	}
    }
#endif 
    /* NB no deletion of images,
     * NB should process once and use many not read many 
     */

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
        return mt_rethrow(code, "jpeg_create_decompress failed");

    s_DCTD_template.init((stream_state*)&state);

    rp.ptr = rbuf - 1;
    rp.limit = rbuf + rlen - 1;

    /* read the header only by not having a write buffer */
    wp.ptr = 0;
    wp.limit = 0;

    code = s_DCTD_template.process(mem, (stream_state*)&state, &rp, &wp, true);
    if (code != 1)
        return mt_rethrow(code, "DCTD process failed");

    g_image->width = jddp.dinfo.output_width;
    g_image->height = jddp.dinfo.output_height;
    g_image->comps = jddp.dinfo.output_components;
    g_image->xres = jddp.dinfo.X_density;
    g_image->yres = jddp.dinfo.Y_density;
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
    if (code == EOFC)
	return 0;
    return mt_throw(code, "jpeg stream decode error");
}

private int
met_PaintPattern(const gs_client_color *pcc, gs_state *pgs)
{
    const gs_client_pattern *ppat = gs_getpattern(pcc);
    const met_pattern_t *pmpat = ppat->client_data;
    const met_image_t *pmim = pmpat->raster_image;
    const byte *pdata = pmim->samples;
    gs_memory_t *mem = gs_state_memory(pgs);
    gs_image_enum *penum;
    gs_color_space color_space;
    gs_image_t image;
    int num_components = 3; /* NB */
    int code;
    /* should be just save the ctm */
    gs_gsave(pgs);
    gs_scale(pgs, pmim->xres/96.0, pmim->yres/96.0);
    gs_cspace_init_DeviceRGB(mem, &color_space);
    gs_image_t_init(&image, &color_space);
    image.ColorSpace = &color_space;
    image.BitsPerComponent = pmim->bits;
    image.Width = pmim->width;
    image.Height = pmim->height;
    penum = gs_image_enum_alloc(mem, "met_PaintPattern");
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
            dprintf(mem, "underflow or overlow in image data\n");
            return -1;
        }
    }
    gs_image_cleanup(penum);
    gs_free_object(mem, penum, "px_paint_pattern");
    gs_grestore(pgs);
    return 0;
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
    
    code = readdata(mem, ms->pzip, ImageSource, &rbuf, &rlen);
    if (code < 0)
        return mt_rethrow(code, "read image data failed");
    if (rbuf[0] == 0xff && rbuf[1] == 0xd8) {
        code = decodejpeg(mem, rbuf, rlen, metpat->raster_image);
	if (code) 
	    return mt_throw(code, "decodejpeg failed");
    }
    else if (memcmp(rbuf, "\211PNG\r\n\032\n", 8) == 0) {
	code = mt_decode_png(mem, rbuf, rlen, metpat->raster_image);
	if (code)
	    return mt_rethrow(code, "decode_png failed");
    } else {
        return mt_throw1(-1, "unknown image file format %s", ImageSource);
    }
    gs_pattern1_init(&gspat);
    uid_set_UniqueID(&gspat.uid, gs_next_ids(mem, 1));
    gspat.PaintType = 1;
    gspat.TilingType = 1;
    // gspat.BBox = metpat->Viewbox;
    gspat.BBox.p.x = 0;
    gspat.BBox.p.y = 0;
    gspat.BBox.q.x = metpat->Viewbox.q.x - metpat->Viewbox.p.x;
    gspat.BBox.q.y = metpat->Viewbox.q.y - metpat->Viewbox.p.y;

    gspat.XStep = metpat->Viewbox.q.x - metpat->Viewbox.p.x; 
    gspat.YStep = metpat->Viewbox.q.y - metpat->Viewbox.p.y;
    gspat.PaintProc = met_PaintPattern;
    gspat.client_data = (void *)metpat;
    {
        gs_matrix mat;
        gs_color_space cs;
        gs_state *pgs = ms->pgs;
        gs_rect vbox = metpat->Viewbox;
        gs_rect vport = metpat->Viewport;
        gs_gsave(pgs);
        /* resolution scaling is done when the pattern is painted */
        /* translate the viewbox to the origin, scale the viewbox to
           the viewport and translate the viewbox back. */
        gs_make_translation(vport.p.x, vport.p.y, &mat);
        {
            double scalex = (vport.q.x - vport.p.x) / (vbox.q.x - vbox.p.x);
            double scaley = (vport.q.y - vport.p.y) / (vbox.q.y - vbox.p.y);
            gs_matrix_scale(&mat, scalex, scaley, &mat);
        }
        gs_matrix_translate(mem, &mat, -vbox.p.x, vbox.p.y, &mat);

        /* nb defaults to RGB */
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

    if (aImageBrush->Viewbox) {
        char viewbox[strlen(aImageBrush->Viewbox)];
        strcpy(viewbox, aImageBrush->Viewbox);
        /* nb wasteful */
        char *args[strlen(aImageBrush->Viewbox)];
        char **pargs = args;
        met_split(viewbox, args, is_Data_delimeter);
        pat->Viewbox.p.x = atof(pargs[0]);
        pat->Viewbox.p.y = atof(pargs[1]);
        pat->Viewbox.q.x = pat->Viewbox.p.x + atof(pargs[2]);
        pat->Viewbox.q.y = pat->Viewbox.p.y + atof(pargs[3]);
    }
    if (aImageBrush->Viewport) {
        /* do ever feel like your doing the same thing over and over
           again... */
        char viewport[strlen(aImageBrush->Viewport)];
        strcpy(viewport, aImageBrush->Viewport);
        /* nb wasteful */
        char *args[strlen(aImageBrush->Viewbox)];
        char **pargs = args;
        met_split(viewport, args, is_Data_delimeter);
        pat->Viewport.p.x = atof(pargs[0]);
        pat->Viewport.p.y = atof(pargs[1]);
        pat->Viewport.q.x = pat->Viewport.p.x + atof(pargs[2]);
        pat->Viewport.q.y = pat->Viewport.p.y + atof(pargs[3]);
    }

    if (aImageBrush->Transform) {
        char transform[strlen(aImageBrush->Transform)];
        char *args[strlen(aImageBrush->Transform)];
        strcpy(transform, aImageBrush->Transform);
        char **pargs = args;
        met_split(transform, args, is_Data_delimeter);
        pat->Transform.xx = atof(pargs[0]);
        pat->Transform.xy = atof(pargs[1]);
        pat->Transform.yx = atof(pargs[2]);
        pat->Transform.yy = atof(pargs[3]);
        pat->Transform.tx = atof(pargs[4]);
        pat->Transform.ty = atof(pargs[5]);
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

