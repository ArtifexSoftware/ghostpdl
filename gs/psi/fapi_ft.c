/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
 */
/* $Id$ */

/*
   GhostScript Font API plug-in that allows fonts to be rendered by FreeType.
   Started by Graham Asher, 6th June 2002.
 */

/* GhostScript headers. */
#include "stdio_.h"
#include "malloc_.h"
#include "ierrors.h"
#include "ifapi.h"
#include "write_t1.h"
#include "write_t2.h"
#include "math_.h"
#include "gserror.h"
#include "gsmemory.h"
#include "gsmalloc.h"
#include "gxfixed.h"
#include "gdebug.h"

/* FreeType headers */
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_INCREMENTAL_H
#include FT_GLYPH_H
#include FT_SYSTEM_H
#include FT_MODULE_H
#include FT_TRIGONOMETRY_H
#include FT_OUTLINE_H

/* Note: structure definitions here start with FF_, which stands for 'FAPI FreeType". */

typedef struct FF_server_s
{
    FAPI_server fapi_server;
    FT_Library freetype_library;
    FT_OutlineGlyph outline_glyph;
    FT_BitmapGlyph bitmap_glyph;
    gs_memory_t *mem;
    FT_Memory ftmemory;
} FF_server;

typedef struct FF_face_s
{
    FT_Face ft_face;

    /* Currently in force scaling/transform for this face */
    FT_Matrix ft_transform;
    FT_F26Dot6 width, height;
    FT_UInt horz_res;
    FT_UInt vert_res;
    
    /* If non-null, the incremental interface object passed to FreeType. */
    FT_Incremental_InterfaceRec *ft_inc_int;

    /* Non-null if font data is owned by this object. */
    unsigned char *font_data;
} FF_face;

/* Here we define the struct FT_Incremental that is used as an opaque type
 * inside FreeType. This structure has to have the tag FT_IncrementalRec_
 * to be compatible with the functions defined in FT_Incremental_FuncsRec. 
 */
typedef struct FT_IncrementalRec_
{
    FAPI_font *fapi_font;		/* The font. */

    /* If it is already in use glyph data is allocated on the heap. */
    unsigned char *glyph_data;		/* A one-shot buffer for glyph data. */
    size_t glyph_data_length;		/* Length in bytes of glyph_data. */
    bool glyph_data_in_use;		/* True if glyph_data is already in use. */

    FT_Incremental_MetricsRec glyph_metrics; /* Incremental glyph metrics supplied by Ghostscript. */
    unsigned long glyph_metrics_index;	/* contains data for this glyph index unless it is 0xFFFFFFFF. */
    FAPI_metrics_type metrics_type;	/* determines whether metrics are replaced, added, etc. */
} FT_IncrementalRec;


FT_CALLBACK_DEF( void* )
FF_alloc( FT_Memory memory, long size)
{
    gs_memory_t *mem = (gs_memory_t *)memory->user;
    
    return(gs_malloc (mem, size, 1, "FF_alloc"));
}

FT_CALLBACK_DEF( void* )
FF_realloc(FT_Memory memory, long cur_size, long new_size, void* block)
{
    gs_memory_t *mem = (gs_memory_t *)memory->user;
    void *tmp;
    
    if (cur_size == new_size) {
        return (block);
    }
    
    tmp = gs_malloc (mem, new_size, 1, "FF_realloc");
    
    
    if (tmp && block) {
        memcpy (tmp, block, min(cur_size, new_size));

        gs_free (mem, block, 0, 0, "FF_realloc");
    }
    
    return(tmp);
}

FT_CALLBACK_DEF( void )
FF_free(FT_Memory memory, void* block)
{
    gs_memory_t *mem = (gs_memory_t *)memory->user;
    gs_free (mem, block, 0, 0, "FF_free");
}


static FF_face *
new_face(FAPI_server *a_server, FT_Face a_ft_face, FT_Incremental_InterfaceRec *a_ft_inc_int, unsigned char *a_font_data)
{
    FF_server *s = (FF_server*)a_server;

    FF_face *face = (FF_face *)FF_alloc(s->ftmemory, sizeof(FF_face));
    if (face)
    {
        face->ft_face = a_ft_face;
        face->ft_inc_int = a_ft_inc_int;
        face->font_data = a_font_data;
    }
    return face;
}

static void
delete_face(FAPI_server *a_server, FF_face *a_face)
{
    if (a_face)
    {
        FF_server *s = (FF_server*)a_server;
        
        FT_Done_Face(a_face->ft_face);
        FF_free(s->ftmemory, a_face->ft_inc_int);
        FF_free(s->ftmemory, a_face->font_data);
        FF_free(s->ftmemory, a_face);
    }
}

static FT_IncrementalRec *
new_inc_int_info(FAPI_server *a_server, FAPI_font *a_fapi_font)
{
    FF_server *s = (FF_server*)a_server;

    FT_IncrementalRec *info = (FT_IncrementalRec*)FF_alloc(s->ftmemory, sizeof(FT_IncrementalRec));
    if (info)
    {
        info->fapi_font = a_fapi_font;
        info->glyph_data = NULL;
        info->glyph_data_length = 0;
        info->glyph_data_in_use = false;
        info->glyph_metrics_index = 0xFFFFFFFF;
        info->metrics_type = FAPI_METRICS_NOTDEF;
    }
    return info;
}

static void
delete_inc_int_info(FAPI_server *a_server, FT_IncrementalRec *a_inc_int_info)
{
    FF_server *s = (FF_server*)a_server;
    if (a_inc_int_info)
    {
        FF_free(s->ftmemory, a_inc_int_info->glyph_data);
        FF_free(s->ftmemory, a_inc_int_info);
    }
}

static FT_Error
get_fapi_glyph_data(FT_Incremental a_info, FT_UInt a_index, FT_Data *a_data)
{
    FAPI_font *ff = a_info->fapi_font;
    int length = 0;
    
    /* Tell the FAPI interface that we need to decrypt the glyph data. */
    ff->need_decrypt = true;

    /* If glyph_data is already in use (as will happen for composite glyphs)
     * create a new buffer on the heap.
     */
    if (a_info->glyph_data_in_use)
    {
	unsigned char *buffer = NULL;
	length = ff->get_glyph(ff, a_index, NULL, 0);
	if (length == 65535)
	    return FT_Err_Invalid_Glyph_Index;
	buffer = gs_malloc ((gs_memory_t *)a_info->fapi_font->memory, length, 1, "get_fapi_glyph_data");
	if (!buffer)
	    return FT_Err_Out_Of_Memory;
	if (ff->get_glyph(ff, a_index, buffer, length) == 65535) {
	    gs_free ((gs_memory_t *)a_info->fapi_font->memory, buffer, 0, 0, "get_fapi_glyph_data");
	    return FT_Err_Invalid_Glyph_Index;
	}
	a_data->pointer = buffer;
    }
    else
    {
        /* Save ff->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
         * make the deprecated Type 2 endchar ('seac') work, so that it can be restored
         * if we need to try again with a longer buffer.
         */
        const void *saved_char_data = ff->char_data;

       /* Get as much of the glyph data as possible into the buffer */
       length = ff->get_glyph(ff, a_index, a_info->glyph_data, (ushort)a_info->glyph_data_length);
       if (length == -1) {
	   ff->char_data = saved_char_data;
	   return FT_Err_Invalid_File_Format;
       }

       /* If the buffer was too small enlarge it and try again. */
       if (length > a_info->glyph_data_length) {
            if (a_info->glyph_data) {
                gs_free ((gs_memory_t *)a_info->fapi_font->memory, a_info->glyph_data, 0, 0, "get_fapi_glyph_data");
            }
        
            a_info->glyph_data = gs_malloc ((gs_memory_t *)a_info->fapi_font->memory, length, 1, "get_fapi_glyph_data");
        
            if (!a_info->glyph_data) {
                a_info->glyph_data_length = 0;
                return FT_Err_Out_Of_Memory;
            }
            a_info->glyph_data_length = length;
            ff->char_data = saved_char_data;
            if ((ff->get_glyph(ff, a_index, a_info->glyph_data, length)) == -1)
		return FT_Err_Invalid_File_Format;
        }

        /* Set the returned pointer and length. */
        a_data->pointer = a_info->glyph_data;

        a_info->glyph_data_in_use = true;
    }

    a_data->length = length;
    return 0;
}

static void
free_fapi_glyph_data(FT_Incremental a_info, FT_Data *a_data)
{
    if (a_data->pointer == (const FT_Byte*)a_info->glyph_data)
        a_info->glyph_data_in_use = false;
    else
        gs_free((gs_memory_t *)a_info->fapi_font->memory, (FT_Byte*)a_data->pointer, 0, 0, "free_fapi_glyph_data");
}

static FT_Error
get_fapi_glyph_metrics(FT_Incremental a_info, FT_UInt a_glyph_index,
	FT_Bool bVertical, FT_Incremental_MetricsRec *a_metrics)
{
    /* FreeType will create synthetic vertical metrics, including a vertical
     * advance, if none is present. We don't want this, so if the font uses Truetype outlines
     * and the WMode is not 1 (vertical) we ignore the advance by setting it to 0
     */
    if (bVertical && !a_info->fapi_font->is_type1)
	a_metrics->advance = 0;

    if (a_info->glyph_metrics_index == a_glyph_index)
    {
	switch (a_info->metrics_type)
	{
	    case FAPI_METRICS_ADD:
		a_metrics->advance += a_info->glyph_metrics.advance;
		break;
	    case FAPI_METRICS_REPLACE_WIDTH:
		a_metrics->advance = a_info->glyph_metrics.advance;
		break;
	    case FAPI_METRICS_REPLACE:
		*a_metrics = a_info->glyph_metrics;
		/* We are replacing the horizontal metrics, so the vertical must be 0 */
		a_metrics->advance_v = 0;
		break;
	    default:
		/* This can't happen. */
		return FT_Err_Invalid_Argument;
	}
    }
    return 0;
}

static const
FT_Incremental_FuncsRec TheFAPIIncrementalInterfaceFuncs =
{
    get_fapi_glyph_data,
    free_fapi_glyph_data,
    get_fapi_glyph_metrics
};

static FT_Incremental_InterfaceRec *
new_inc_int(FAPI_server *a_server, FAPI_font *a_fapi_font)
{
    FF_server *s = (FF_server*)a_server;

    FT_Incremental_InterfaceRec *i =
	(FT_Incremental_InterfaceRec*) FF_alloc(s->ftmemory, sizeof(FT_Incremental_InterfaceRec));
    if (i)
    {
        i->object = (FT_Incremental)new_inc_int_info(a_server, a_fapi_font);
        i->funcs = &TheFAPIIncrementalInterfaceFuncs;
    }
    if (!i->object)
    {
        FF_free(s->ftmemory, i);
        i = NULL;
    }
    return i;
}

static void
delete_inc_int(FAPI_server *a_server, FT_Incremental_InterfaceRec *a_inc_int)
{
    FF_server *s = (FF_server*)a_server;
    if (a_inc_int)
    {
        delete_inc_int_info(a_server, a_inc_int->object);
        FF_free(s->ftmemory, a_inc_int);
    }
}

/* Convert FreeType error codes to GhostScript ones.
 * Very rudimentary because most don't correspond.
 */
static int
ft_to_gs_error(FT_Error a_error)
{
    if (a_error)
    {
	if (a_error == FT_Err_Out_Of_Memory)
	    return e_VMerror;
	else
	    return e_unknownerror;
    }
    return 0;
}

/* Load a glyph and optionally rasterize it. Return its metrics in a_metrics.
 * If a_bitmap is true convert the glyph to a bitmap.
 */
static FAPI_retcode
load_glyph(FAPI_font *a_fapi_font, const FAPI_char_ref *a_char_ref,
	FAPI_metrics *a_metrics, FT_Glyph *a_glyph, bool a_bitmap)
{
    FT_Error ft_error = 0;
    FF_face *face = (FF_face*)a_fapi_font->server_font_data;
    FT_Face ft_face = face->ft_face;
    int index = a_char_ref->char_code;

    /* Save a_fapi_font->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
     * make the deprecated Type 2 endchar ('seac') work, so that it can be restored
     * after the first call to FT_Load_Glyph.
     */
    const void *saved_char_data = a_fapi_font->char_data;

    if (!a_char_ref->is_glyph_index)
    {
	if (ft_face->num_charmaps)
	    index = FT_Get_Char_Index(ft_face, index);
	else
	{
	    /* If there are no character maps and no glyph index, loading the glyph will still work
	     * properly if both glyph data and metrics are supplied by the incremental interface.
	     * In that case we use a dummy glyph index which will be passed
	     * back to FAPI_FF_get_glyph by get_fapi_glyph_data.
	     *
	     * Type 1 fonts don't use the code and can appear to FreeType to have only one glyph,
	     * so we have to set the index to 0.
	     *
	     * For other font types, FAPI_FF_get_glyph requires the character code
	     * when getting data.
	     */
	    if (a_fapi_font->is_type1)
		index = 0;
	    else
		index = a_char_ref->char_code;
	}
    }

    /* Refresh the pointer to the FAPI_font held by the incremental interface. */
    if (face->ft_inc_int)
	face->ft_inc_int->object->fapi_font = a_fapi_font;

    /* Store the overriding metrics if they have been supplied. */
    if (face->ft_inc_int && a_char_ref->metrics_type != FAPI_METRICS_NOTDEF)
    {
	FT_Incremental_MetricsRec *m = &face->ft_inc_int->object->glyph_metrics;
	m->bearing_x = a_char_ref->sb_x >> 16;
	m->bearing_y = a_char_ref->sb_y >> 16;
	m->advance = a_char_ref->aw_x >> 16;
	face->ft_inc_int->object->glyph_metrics_index = index;
	face->ft_inc_int->object->metrics_type = a_char_ref->metrics_type;
    }
    else
	if (face->ft_inc_int)
	    /* Make sure we don't leave this set to the last value, as we may then use inappropriate metrics values */
	    face->ft_inc_int->object->glyph_metrics_index = 0xFFFFFFFF;
    
    /* We have to load the glyph, scale it correctly, and render it if we need a bitmap. */
    if (!ft_error)
    {
        /* We disable loading bitmaps because if we allow it then FreeType invents metrics for them, which messes up our glyph positioning */
        /* Also the bitmaps tend to look somewhat different (though more readable) than FreeType's rendering. By disabling them we */
        /* maintain consistency better.  (FT_LOAD_NO_BITMAP) */
        a_fapi_font->char_data = saved_char_data;
        if (!a_fapi_font->is_type1)
            ft_error = FT_Load_Glyph(ft_face, index, a_bitmap ? FT_LOAD_MONOCHROME | FT_LOAD_RENDER | FT_LOAD_NO_BITMAP : FT_LOAD_MONOCHROME | FT_LOAD_NO_BITMAP);
	else {
            /* Current FreeType hinting for type 1 fonts is so poor we are actually better off without it (fewer files render incorrectly) (FT_LOAD_NO_HINTING) */
            ft_error = FT_Load_Glyph(ft_face, index, a_bitmap ? FT_LOAD_MONOCHROME | FT_LOAD_RENDER | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP: FT_LOAD_MONOCHROME | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
	    if (ft_error == FT_Err_Invalid_File_Format) 
		return index+1;
	}
    }

    if (ft_error == FT_Err_Too_Many_Hints || ft_error == FT_Err_Invalid_Argument || ft_error == FT_Err_Too_Many_Function_Defs || ft_error == FT_Err_Invalid_Glyph_Index) {
        a_fapi_font->char_data = saved_char_data;
        ft_error = FT_Load_Glyph(ft_face, index, a_bitmap ? FT_LOAD_MONOCHROME | FT_LOAD_RENDER | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP: FT_LOAD_MONOCHROME | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP);
    }
    /* Previously we interpreted the glyph unscaled, and derived the metrics from that. Now we only interpret it
     * once, and work out the metrics from the scaled/hinted outline.
     */
    if (!ft_error && a_metrics)
    {
        FT_Long hx;
        FT_Long hy;
        FT_Long w;
        FT_Long h;
        FT_Long hadv;
        FT_Long vadv;

        /* In order to get the metrics in the form we need them, we have to remove the size scaling
         * the resolution scaling, and convert to points.
         */
        hx = (FT_Long)((((double)ft_face->glyph->metrics.horiBearingX * ft_face->units_per_EM / face->width) / face->horz_res) * 72);
        hy = (FT_Long)((((double)ft_face->glyph->metrics.horiBearingY * ft_face->units_per_EM / face->height) / face->vert_res) * 72);

        w = (FT_Long)((((double)ft_face->glyph->metrics.width * ft_face->units_per_EM / face->width) / face->horz_res) * 72);
        h = (FT_Long)((((double)ft_face->glyph->metrics.height * ft_face->units_per_EM / face->height) / face->vert_res) * 72);

        hadv = (FT_Long)((((double)ft_face->glyph->metrics.horiAdvance * ft_face->units_per_EM / face->width) / face->horz_res) * 72);

	/* Ugly. FreeType creates verticla metrics for TT fonts, normally we override them in the 
	 * metrics callbacks, but those only work for incremental interface fonts, and TrueType fonts
	 * loaded as CIDFont replacements are not incrementally handled. So here, if its a CIDFont, and
	 * its not type 1 outlines, and its not a vertical mode fotn, ignore the advance.
	 */
	if(!a_fapi_font->is_type1 && a_fapi_font->is_cid && !a_fapi_font->is_vertical)
	    vadv = 0;
	else
	    vadv = (FT_Long)((((double)ft_face->glyph->metrics.vertAdvance * ft_face->units_per_EM / face->height) / face->vert_res) * 72);
        
        a_metrics->bbox_x0 = hx;
        a_metrics->bbox_y0 = hy - h;
        a_metrics->bbox_x1 = a_metrics->bbox_x0 + w;
        a_metrics->bbox_y1 = a_metrics->bbox_y0 + h;
        a_metrics->escapement = hadv;
        a_metrics->v_escapement = vadv;
        a_metrics->em_x = ft_face->units_per_EM;
        a_metrics->em_y = ft_face->units_per_EM;
    }

    if (!ft_error && a_glyph)
        ft_error = FT_Get_Glyph(ft_face->glyph, a_glyph);

    if (ft_error == FT_Err_Too_Many_Hints) {
#ifdef DEBUG
	if (gs_debug_c('1')) {
            emprintf1(a_fapi_font->memory,
                      "TrueType glyph %d uses more instructions than the declared maximum in the font. Continuing, ignoring broken glyph\n",
                      a_char_ref->char_code);
        }
#endif
	ft_error = 0;
    }
    if (ft_error == FT_Err_Invalid_Argument) {
#ifdef DEBUG
	if (gs_debug_c('1')) {
	    emprintf1(a_fapi_font->memory,
                      "TrueType parsing error in glyph %d in the font. Continuing, ignoring broken glyph\n",
                      a_char_ref->char_code);
        }
#endif
	ft_error = 0;
    }
    if (ft_error == FT_Err_Too_Many_Function_Defs) {
#ifdef DEBUG
	if (gs_debug_c('1')) {
	    emprintf1(a_fapi_font->memory,
                      "TrueType instruction error in glyph %d in the font. Continuing, ignoring broken glyph\n",
                      a_char_ref->char_code);
        }
#endif
	ft_error = 0;
    }
    if (ft_error == FT_Err_Invalid_Glyph_Index) {
#ifdef DEBUG
	if (gs_debug_c('1')) {
	    emprintf1(a_fapi_font->memory,
                      "FreeType is unable to find the glyph %d in the font. Continuing, ignoring missing glyph\n",
                      a_char_ref->char_code);
        }
#endif
	ft_error = 0;
    }
    return ft_to_gs_error(ft_error);
} 

/*
 * Ensure that the rasterizer is open.
 *
 * In the case of FreeType this means creating the FreeType library object.
 */
static FAPI_retcode
ensure_open(FAPI_server *a_server, const byte *server_param, int server_param_size)
{
    FF_server *s = (FF_server*)a_server;
    if (!s->freetype_library)
    {
        FT_Error ft_error;
        
        /* As we want FT to use our memory management, we cannot use the convenience of
         * FT_Init_FreeType(), we have to do each stage "manually"
         */
        s->ftmemory = gs_malloc (s->mem, sizeof(*s->ftmemory), 1, "ensure_open");
        if (!s->ftmemory) {
            return(-1);
        }
        
        s->ftmemory->user = s->mem;
        s->ftmemory->alloc = FF_alloc;
        s->ftmemory->free = FF_free;
        s->ftmemory->realloc = FF_realloc;
        
        ft_error = FT_New_Library(s->ftmemory, &s->freetype_library);
        if (ft_error) {
            gs_free (s->mem, s->ftmemory, 0, 0, "ensure_open");
        }
        else {
            FT_Add_Default_Modules(s->freetype_library);
        }

	if (ft_error)
	    return ft_to_gs_error(ft_error);
    }
    return 0;
}

#if 0 /* Not currently used */
static void
transform_concat(FT_Matrix *a_A, const FT_Matrix *a_B)
{
    FT_Matrix result = *a_B;
    FT_Matrix_Multiply(a_A, &result);
    *a_A = result;
}

/* Create a transform representing an angle defined as a vector. */
static void
make_rotation(FT_Matrix *a_transform, const FT_Vector *a_vector)
{
    FT_Fixed length, cos, sin;
    if (a_vector->x >= 0 && a_vector->y == 0)
    {
	a_transform->xx = a_transform->yy = 65536;
	a_transform->xy = a_transform->yx = 0;
	return;
    }

    length = FT_Vector_Length((FT_Vector*)a_vector);
    cos = FT_DivFix(a_vector->x, length);
    sin = FT_DivFix(a_vector->y, length);
    a_transform->xx = a_transform->yy = cos;
    a_transform->xy = -sin;
    a_transform->yx = sin;
}
#endif /* Not currently used */

/* Divide a transformation into a scaling part and a rotation-and-shear part.
 * The scaling part is used for setting the pixel size for hinting.
 */
static void
transform_decompose(FT_Matrix *a_transform, FT_Fixed *a_x_scale, FT_Fixed *a_y_scale)
{
    double scalex, scaley, fact = 1.0;
    FT_Matrix ftscale_mat;
    
    scalex = hypot ((double)a_transform->xx, (double)a_transform->xy) / 65536.0;
    scaley = hypot ((double)a_transform->yx, (double)a_transform->yy) / 65536.0;
    
    /* FT clamps the width and height to a lower limit of 1.0 units
     * (note: as FT stores it in 64ths of a unit, that is 64)
     * So if either the width or the height are <1.0 here, we scale
     * the width and height appropriately, and then compensate using
     * the "final" matrix for FT
     */
    /* We use 1 1/64th to calculate the scale, so that we *guarantee* the
     * scalex/y we calculate will be >64 after rounding.
     */
    if (scalex > scaley)
    {
        if (scaley < 1.0)
	{
	    fact = 1.016 / scaley;
	    scaley = scaley * fact;
	    scalex = scalex * fact;
	}
    }
    else
    {
        if (scalex < 1.0)
	{
	    fact = 1.016 / scalex;
	    scalex = scalex * fact;
	    scaley = scaley * fact;
	}
    }
    
    ftscale_mat.xx = (FT_Fixed)(((1.0 / scalex)) * 65536.0);
    ftscale_mat.xy = 0;
    ftscale_mat.yx = 0;
    ftscale_mat.yy = (FT_Fixed)(((1.0 / scaley)) * 65536.0);
    
    FT_Matrix_Multiply (a_transform, &ftscale_mat);
    memcpy(a_transform, &ftscale_mat, sizeof(FT_Matrix));
        
    /* Return values ready scaled for FT */
    *a_x_scale = (FT_Fixed)(scalex * 64);
    *a_y_scale = (FT_Fixed)(scaley * 64);
}

/*
 * Open a font and set its size.
 */
static FAPI_retcode
get_scaled_font(FAPI_server *a_server, FAPI_font *a_font,
	const FAPI_font_scale *a_font_scale,
	const char *a_map,
	FAPI_descendant_code a_descendant_code)
{
    FF_server *s = (FF_server*)a_server;
    FF_face *face = (FF_face*)a_font->server_font_data;
    FT_Error ft_error = 0;

    /* dpf("get_scaled_font enter: is_type1=%d is_cid=%d font_file_path='%s' a_descendant_code=%d\n",
       a_font->is_type1, a_font->is_cid, a_font->font_file_path ? a_font->font_file_path : "", a_descendant_code); */

    /* If this font is the top level font of an embedded CID type 0 font (font type 9)
     * do nothing. See the comment in FAPI_prepare_font. The descendant fonts are
     * passed in individually.
     */
    if (a_font->is_cid && a_font->is_type1 && a_font->font_file_path == NULL &&
	    (a_descendant_code == FAPI_TOPLEVEL_BEGIN ||
	     a_descendant_code == FAPI_TOPLEVEL_COMPLETE))
    {
	/* dpf("get_scaled_font return 0\n"); */
	return 0;
    }

    /* Create the face if it doesn't already exist. */
    if (!face)
    {
	FT_Face ft_face = NULL;
	FT_Parameter ft_param;
	FT_Incremental_InterfaceRec *ft_inc_int = NULL;
	unsigned char *own_font_data = NULL;

	/* dpf("get_scaled_font creating face\n"); */

	/* Load a typeface from a file. */
	if (a_font->font_file_path)
	{
	    ft_error = FT_New_Face(s->freetype_library, a_font->font_file_path, a_font->subfont, &ft_face);
	    if (!ft_error && ft_face)
		ft_error = FT_Select_Charmap(ft_face, ft_encoding_unicode);
	}

	/* Load a typeface from a representation in GhostScript's memory. */
	else
	{
	    FT_Open_Args open_args;
	    open_args.flags = FT_OPEN_MEMORY;

	    if (a_font->is_type1)
	    {
		long length;
		int type = a_font->get_word(a_font, FAPI_FONT_FEATURE_FontType, 0);

		/* Tell the FAPI interface that we need to decrypt the /Subrs data. */
		a_font->need_decrypt = true;

		/*
		   Serialise a type 1 font in PostScript source form, or
		   a Type 2 font in binary form, so that FreeType can read it.
		 */
		if (type == 1)
		    length = FF_serialize_type1_font(a_font, NULL, 0);
		else
		    length = FF_serialize_type2_font(a_font, NULL, 0);
		open_args.memory_base = own_font_data = FF_alloc(s->ftmemory, length);
		if (!open_args.memory_base)
		    return e_VMerror;
		if (type == 1)
		    open_args.memory_size = FF_serialize_type1_font(a_font, own_font_data, length);
		else
		    open_args.memory_size = FF_serialize_type2_font(a_font, own_font_data, length);
		if (open_args.memory_size != length)
		    return_error(e_unregistered); /* Must not happen. */
		ft_inc_int = new_inc_int(a_server, a_font);
		if (!ft_inc_int)
		{
		    FF_free (s->ftmemory, own_font_data);
		    return e_VMerror;
		}
	    }

	    /* It must be type 42 (see code in FAPI_FF_get_glyph in zfapi.c). */
	    else 
	    {
		/* Get the length of the TrueType data. */
		open_args.memory_size = a_font->get_long(a_font, FAPI_FONT_FEATURE_TT_size, 0);
		if (open_args.memory_size == 0)
		    return e_invalidfont;

		/* Load the TrueType data into a single buffer. */
		open_args.memory_base = own_font_data = FF_alloc(s->ftmemory, open_args.memory_size);
		if (!own_font_data)
		    return e_VMerror;
		if (a_font->serialize_tt_font(a_font, own_font_data, open_args.memory_size))
		    return e_invalidfont;

		/* We always load incrementally. */
		ft_inc_int = new_inc_int(a_server, a_font);
		if (!ft_inc_int)
		{
		    FF_free(s->ftmemory, own_font_data);
		    return e_VMerror;
		}
	    }

	    if (ft_inc_int)
	    {
		open_args.flags = (FT_UInt)(open_args.flags | FT_OPEN_PARAMS);
		ft_param.tag = FT_PARAM_TAG_INCREMENTAL;
		ft_param.data = ft_inc_int;
		open_args.num_params = 1;
		open_args.params = &ft_param;
	    }
	    ft_error = FT_Open_Face(s->freetype_library, &open_args, a_font->subfont, &ft_face);
	}

	if (ft_face)
	{
	    face = new_face(a_server, ft_face, ft_inc_int, own_font_data);
	    if (!face)
	    {
		FF_free(s->ftmemory, own_font_data);
		FT_Done_Face(ft_face);
		delete_inc_int(a_server, ft_inc_int);
		return e_VMerror;
	    }
	    a_font->server_font_data = face;
	}
	else
	    a_font->server_font_data = NULL;	
    }

    /* Set the point size and transformation.
     * The matrix is scaled by the shift specified in the server, 16,
     * so we divide by 65536 when converting to a gs_matrix.
     */
    if (face)
    {
        /* Convert the GS transform into an FT transform.
         * Ignore the translation elements because they contain very large values
         * derived from the current transformation matrix and so are of no use.
         */
        face->ft_transform.xx = a_font_scale->matrix[0];
        face->ft_transform.xy = a_font_scale->matrix[2];
        face->ft_transform.yx = a_font_scale->matrix[1];
        face->ft_transform.yy = a_font_scale->matrix[3];
        
        face->horz_res = a_font_scale->HWResolution[0] >> 16;
        face->vert_res = a_font_scale->HWResolution[1] >> 16;
        
        /* Split the transform into scale factors and a rotation-and-shear
         * transform.
         */
        transform_decompose(&face->ft_transform, &face->width, &face->height);
		
        ft_error = FT_Set_Char_Size(face->ft_face, face->width, face->height,
                face->horz_res, face->vert_res);
        
        if (ft_error)
        {
            delete_face(a_server, face);
            a_font->server_font_data = NULL;
            return ft_to_gs_error(ft_error);
        }

	/* Concatenate the transform to a reflection around (y=0) so that it
	 * produces a glyph that is upside down in FreeType terms, with its
	 * first row at the bottom. That is what GhostScript needs.
	 */

	FT_Set_Transform(face->ft_face, &face->ft_transform, NULL);
    }

    /* dpf("get_scaled_font return %d\n", a_font->server_font_data ? 0 : -1); */
    return a_font->server_font_data ? 0 : -1;
}

/*
 * Return the name of a resource which maps names to character codes. Do this
 * by setting a_decoding_id to point to a null-terminated string. The resource
 * is in the 'decoding' directory in the directory named by /GenericResourceDir
 * in lib/gs_res.ps.
 */
static FAPI_retcode
get_decodingID(FAPI_server *a_server, FAPI_font *a_font, const char** a_decoding_id)
{
    *a_decoding_id = "Unicode";
    return 0;
}

/*
 * Get the font bounding box in font units.
 */
static FAPI_retcode
get_font_bbox(FAPI_server *a_server, FAPI_font *a_font, int a_box[4])
{
    FF_face *face = (FF_face*)a_font->server_font_data;
    a_box[0] = face->ft_face->bbox.xMin;
    a_box[1] = face->ft_face->bbox.yMin;
    a_box[2] = face->ft_face->bbox.xMax;
    a_box[3] = face->ft_face->bbox.yMax;
    return 0;
}

/*
 * Return a boolean value in a_proportional stating whether the font is proportional
 * or fixed-width.
 */
static FAPI_retcode
get_font_proportional_feature(FAPI_server *a_server, FAPI_font *a_font, bool *a_proportional)
{
    *a_proportional = true;
    return 0;
}

/* Convert the character name in a_char_ref.char_name to a character code or
 * glyph index and put it in a_char_ref.char_code, setting
 * a_char_ref.is_glyph_index as appropriate. If this is possible set a_result
 * to true, otherwise set it to false.  The return value is a standard error
 * return code.
 */
static FAPI_retcode
can_retrieve_char_by_name(FAPI_server *a_server, FAPI_font *a_font, FAPI_char_ref *a_char_ref, bool *a_result)
{
    FF_face *face = (FF_face*)a_font->server_font_data;
    char name[128];
    if (FT_HAS_GLYPH_NAMES(face->ft_face) && a_char_ref->char_name_length < sizeof(name))
    {
	memcpy(name, a_char_ref->char_name, a_char_ref->char_name_length);
	name[a_char_ref->char_name_length] = 0;
	a_char_ref->char_code = FT_Get_Name_Index(face->ft_face, name);
	*a_result = a_char_ref->char_code != 0;
	if (*a_result)
	    a_char_ref->is_glyph_index = true;
    }
    else
	*a_result = false;
    return 0;
}

/*
 * Return non-zero if the metrics can be replaced.
 */
static FAPI_retcode
can_replace_metrics(FAPI_server *a_server, FAPI_font *a_font, FAPI_char_ref *a_char_ref, int *a_result)
{
    /* Replace metrics only if the metrics are supplied in font units. */
    *a_result = 1;
    return 0;
}

/*
 * Retrieve the metrics of a_char_ref and put them in a_metrics.
 */
static FAPI_retcode
get_char_width(FAPI_server *a_server, FAPI_font *a_font, FAPI_char_ref *a_char_ref, FAPI_metrics *a_metrics)
{
    return load_glyph(a_font, a_char_ref, a_metrics, NULL, false);
}

static FAPI_retcode get_fontmatrix(FAPI_server *server, gs_matrix *m)
{
    m->xx = 1.0;
    m->xy = 0.0;
    m->yx = 0.0;
    m->yy = 1.0;
    m->tx = 0.0;
    m->ty = 0.0;
    return 0;
}

/*
 * Rasterize the character a_char and return its metrics. Do not return the
 * bitmap but store this. It can be retrieved by a subsequent call to
 * get_char_raster.
 */
static FAPI_retcode
get_char_raster_metrics(FAPI_server *a_server, FAPI_font *a_font,
	FAPI_char_ref *a_char_ref, FAPI_metrics *a_metrics)
{
    FF_server *s = (FF_server*)a_server;
    FAPI_retcode error = load_glyph(a_font, a_char_ref, a_metrics, (FT_Glyph*)&s->bitmap_glyph, true);
    return error;
}

/*
 * Return the bitmap created by the last call to get_char_raster_metrics.
 */
static FAPI_retcode
get_char_raster(FAPI_server *a_server, FAPI_raster *a_raster)
{
    FF_server *s = (FF_server*)a_server;
    if (!s->bitmap_glyph)
	return_error(e_unregistered); /* Must not happen. */
    a_raster->p = s->bitmap_glyph->bitmap.buffer;
    a_raster->width = s->bitmap_glyph->bitmap.width;
    a_raster->height = s->bitmap_glyph->bitmap.rows;
    a_raster->line_step = s->bitmap_glyph->bitmap.pitch;
    a_raster->orig_x = s->bitmap_glyph->left * 16;
    a_raster->orig_y = s->bitmap_glyph->top * 16;
    a_raster->left_indent = a_raster->top_indent = a_raster->black_height = a_raster->black_width = 0;
    return 0;
}

/*
 * Create an outline for the character a_char and return its metrics. Do not
 * return the outline but store this.
 * It can be retrieved by a subsequent call to get_char_outline.
 */
static FAPI_retcode
get_char_outline_metrics(FAPI_server *a_server, FAPI_font *a_font,
	FAPI_char_ref *a_char_ref, FAPI_metrics *a_metrics)
{
    FF_server *s = (FF_server*)a_server;
    return load_glyph(a_font, a_char_ref, a_metrics, (FT_Glyph*)&s->outline_glyph, false);
}

typedef struct FF_path_info_s
{
    FAPI_path *path;
    FracInt x;
    FracInt y;
} FF_path_info;

static int move_to(const FT_Vector *aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info*)aObject;

    /* FAPI expects that co-ordinates will be as implied by frac_shift
     * in our case 16.16 fixed precision. True for 'low level' FT 
     * routines (apparently), it isn't true for these routines where
     * FT returns a 26.6 format. Rescale to 16.16 so that FAPI will
     * be able to convert to GS co-ordinates properly.
     */
    p->x = aTo->x << 10;
    p->y = aTo->y << 10;

    return p->path->moveto(p->path, p->x, p->y) ? -1 : 0;
}

static int line_to(const FT_Vector *aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info*)aObject;

    /* See move_to() above */
    p->x = aTo->x << 10;
    p->y = aTo->y << 10;

    return p->path->lineto(p->path, p->x, p->y) ? -1 : 0;
}

static int conic_to(const FT_Vector *aControl, const FT_Vector *aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info*)aObject;
    floatp x, y, Controlx, Controly, Control1x, Control1y, Control2x, Control2y;

    /* More complivated than above, we need to do arithmetic on the
     * co-ordinates, so we want them as floats and we will convert the
     * result into 16.16 fixed precision for FAPI
     *
     * NB this code is funcitonally the same as the original, but I don't believe
     * the comment (below) to be what the code is actually doing....
     *
     * Convert a quadratic spline to a cubic. Do this by changing the three points
     * A, B and C to A, 1/3(B,A), 1/3(B,C), C - that is, the two cubic control points are
     * a third of the way from the single quadratic control point to the end points. This
     * gives the same curve as the original quadratic.
     */

    x = aTo->x / 64;
    p->x = float2fixed(x) << 8;
    y = aTo->y / 64;
    p->y = float2fixed(y) << 8;
    Controlx = aControl->x / 64;
    Controly = aControl->y / 64;

    Control1x = float2fixed((x + Controlx * 2) / 3) << 8;
    Control1y = float2fixed((y + Controly * 2) / 3) << 8;
    Control2x = float2fixed((x + Controlx * 2) / 3) << 8;
    Control2y = float2fixed((y + Controly * 2) / 3) << 8;

    return p->path->curveto(p->path, (FracInt)Control1x,
	    (FracInt)Control1y,
	    (FracInt)Control2x,
	    (FracInt)Control2y,
	    p->x, p->y) ? -1 : 0;
}

static int cubic_to(const FT_Vector *aControl1, const FT_Vector *aControl2, const FT_Vector *aTo, void *aObject)
{
    FF_path_info *p = (FF_path_info*)aObject;
    unsigned long Control1x, Control1y, Control2x, Control2y;

    /* See move_to() above */
    p->x = aTo->x << 10;
    p->y = aTo->y << 10;

    Control1x = aControl1->x << 10;
    Control1y = aControl1->y << 10;
    Control2x = aControl2->x << 10;
    Control2y = aControl2->y << 10;
    return p->path->curveto(p->path, Control1x, Control1y, Control2x, Control2y, p->x, p->y) ? -1 : 0;

    p->x = aTo->x;
    p->y = aTo->y;
    return p->path->curveto(p->path, aControl1->x, aControl1->y, aControl2->x, aControl2->y, aTo->x, aTo->y) ? -1 : 0;
}

static const FT_Outline_Funcs TheFtOutlineFuncs =
{
    move_to,
    line_to,
    conic_to,
    cubic_to,
    0,
    0
};

/*
 * Return the outline created by the last call to get_char_outline_metrics.
 */
static FAPI_retcode
get_char_outline(FAPI_server *a_server, FAPI_path *a_path)
{
    FF_server *s = (FF_server*)a_server;
    FF_path_info p;
    FT_Error ft_error = 0;
    p.path = a_path;
    p.x = 0;
    p.y = 0;
    ft_error = FT_Outline_Decompose(&s->outline_glyph->outline, &TheFtOutlineFuncs, &p);
    if (a_path->gs_error == 0)
        a_path->closepath(a_path);
    return ft_to_gs_error(ft_error);
}

static FAPI_retcode release_char_data(FAPI_server *a_server)
{
    FF_server *s = (FF_server*)a_server;
    FT_Done_Glyph(&s->outline_glyph->root);
    FT_Done_Glyph(&s->bitmap_glyph->root);
    s->outline_glyph = NULL;
    s->bitmap_glyph = NULL;
    return 0;
}

static FAPI_retcode
release_typeface(FAPI_server *a_server, void *a_server_font_data)
{
    FF_face *face = (FF_face*)a_server_font_data;
    delete_face(a_server, face);
    return 0;
}

static FAPI_retcode
check_cmap_for_GID(FAPI_server *server, uint *index)
{
    FF_face *face = (FF_face*)(server->ff.server_font_data);
    FT_Face ft_face = face->ft_face;

    *index = FT_Get_Char_Index(ft_face, *index);
    return 0;
}

static void gs_freetype_destroy(i_plugin_instance *a_instance, i_plugin_client_memory *a_memory);

static const i_plugin_descriptor TheFreeTypeDescriptor =
{
    "FAPI",
    "FreeType",
    gs_freetype_destroy
};

static const FAPI_server TheFreeTypeServer =
{
    { &TheFreeTypeDescriptor },
    16, /* frac_shift */
    {gs_no_id},
    {0},
    {1, 0, 0, 1, 0, 0},
    ensure_open,
    get_scaled_font,
    get_decodingID,
    get_font_bbox,
    get_font_proportional_feature,
    can_retrieve_char_by_name,
    can_replace_metrics,
    get_fontmatrix,
    get_char_width,
    get_char_raster_metrics,
    get_char_raster,
    get_char_outline_metrics,
    get_char_outline,
    release_char_data,
    release_typeface,
    check_cmap_for_GID
};

plugin_instantiation_proc(gs_fapi_ft_instantiate);

int gs_fapi_ft_instantiate( i_plugin_client_memory *a_memory, i_plugin_instance **a_plugin_instance)
{
    FF_server *server = (FF_server*) a_memory->alloc(a_memory, sizeof (FF_server), "FF_server");
    if (!server)
	return e_VMerror;
    memset(server, 0, sizeof(*server));
    server->mem = a_memory->client_data;
    server->fapi_server = TheFreeTypeServer;
    *a_plugin_instance = &server->fapi_server.ig;
    return 0;
}

static void gs_freetype_destroy(i_plugin_instance *a_plugin_instance, i_plugin_client_memory *a_memory)
{
    FF_server *server = (FF_server *)a_plugin_instance;
    
    FT_Done_Glyph(&server->outline_glyph->root);
    FT_Done_Glyph(&server->bitmap_glyph->root);
    
    /* As with initialization: since we're supplying memory management to
     * FT, we cannot just to use FT_Done_FreeType (), we have to use
     * FT_Done_Library () and then discard the memory ourselves
     */
    FT_Done_Library(server->freetype_library);
    gs_free (server->mem, server->ftmemory, 0, 0, "gs_freetype_destroy");

    a_memory->free(a_memory, server, "FF_server");
}
