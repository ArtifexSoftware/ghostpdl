/* Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id$ */

/*
GhostScript Font API plug-in that allows fonts to be rendered by FreeType.
Started by Graham Asher, 6th June 2002.
*/

#include "stdio_.h"
#include "memory_.h"
#include "math_.h"
#include "errors.h"
#include "iplugin.h"
#include "ifapi.h"
#include "ghost.h"
#include "oper.h"
/*#include "gsstruct.h"*/
/*#include "gzstate.h"*/
/*#include "gxdevice.h"*/
#include "gxfont.h"
/*#include "gxfcache.h"*/
#include "bfont.h"
#include "gxfont42.h"
/*#include "gdevpsf.h"*/
#include "idict.h"
#include "write_t1.h"
#include "write_t2.h"

/* FreeType headers */
#include "freetype/freetype.h"
#include "freetype/ftincrem.h"
#include "freetype/ftglyph.h"
#include "freetype/ftoutln.h"

#include <assert.h>

/* Note: structure definitions here start with FF_, which stands for 'FAPI FreeType". */

typedef struct FF_server_
    {
    FAPI_server m_fapi_server;
    FT_Library m_freetype_library;
    FT_OutlineGlyph m_outline_glyph;
    FT_BitmapGlyph m_bitmap_glyph;
    } FF_server;

typedef struct FF_stream_info_
	{
	struct FT_StreamRec_ m_ft_stream;
	ref m_sfnt_array;
	int m_block_index;
	const unsigned char* m_block_data;
	unsigned long m_block_start;
	unsigned long m_block_length;
	} FF_stream_info;

static unsigned long stream_read(FT_Stream a_stream,unsigned long a_offset,unsigned char* a_buffer,unsigned long a_count)
	{
	int n;
	long bytes_read;
	FF_stream_info* info = (FF_stream_info*)a_stream;

	if (a_offset >= info->m_block_start && a_offset + a_count <= info->m_block_start + info->m_block_length)
		{
		memcpy(a_buffer,info->m_block_data + a_offset - info->m_block_start,a_count);
		return a_count;
		}

	if (a_offset <= info->m_block_start)
		{
		info->m_block_index = -1;
		info->m_block_start = info->m_block_length = 0;
		info->m_block_data = NULL;
		}

	bytes_read = 0;
	n = r_size(&info->m_sfnt_array);
	while (a_count > 0)
		{
		unsigned long available = 0;

		// get the block containing a_offset
		while (a_offset > info->m_block_start + info->m_block_length)
			{
			ref data;
			if (info->m_block_index == n - 1)
				return bytes_read;

			info->m_block_index++;
			info->m_block_start += info->m_block_length;
			array_get(&info->m_sfnt_array,info->m_block_index,&data);
			info->m_block_data = data.value.const_bytes;
			info->m_block_length = r_size(&data) & ~(uint)1; /* See Adobe Technical Note # 5012, section 4.2. */
			}
			
		available = info->m_block_start + info->m_block_length - a_offset;
		if (available > a_count)
			available = a_count;

		memcpy(a_buffer,info->m_block_data + a_offset - info->m_block_start,available);
		a_buffer += available;
		bytes_read += available;
		a_count -= available;
		}

	return bytes_read;
	}

static void stream_close(FT_Stream a_stream)
	{
	}

static FF_stream_info* new_stream_info(ref a_sfnt_array,unsigned long a_size)
	{
	FF_stream_info* info = (FF_stream_info*)malloc(sizeof(FF_stream_info));
	memset(info,0,sizeof(FF_stream_info));
	if (info)
		{
		info->m_ft_stream.size = a_size;
		info->m_ft_stream.read = stream_read;
		info->m_ft_stream.close = stream_close;
		info->m_sfnt_array = a_sfnt_array;
		info->m_block_index = -1;
		}
	return info;
	}

static void delete_stream_info(FF_stream_info* a_stream_info)
	{
	free(a_stream_info);
	}

typedef struct FF_face_
    {
    FT_Face m_ft_face;			                /* The FreeType typeface object. */
    FT_Incremental_InterfaceRec* m_ft_inc_int;  /* If non-null, the incremental interface object passed to FreeType. */
	FF_stream_info* m_stream_info;				/* If non-null, the custom stream used to access font data. */
	unsigned char* m_font_data;					/* Non-null if font data is owned by this object. */
    } FF_face;

/*
This structure has to have the tag FT_IncrementalRec to be compatible with
the functions defined in FT_Incremental_FuncsRec.
*/
typedef struct FT_IncrementalRec_
    {
    FAPI_font* m_fapi_font;						/* The font. */
    unsigned char* m_glyph_data;				/* A one-shot buffer for glyph data. */
												/* If it is already in use glyph data is allocated on the heap. */
	size_t m_glyph_data_length;					/* Length in bytes of m_glyph_data. */
	bool m_glyph_data_in_use;					/* True if m_glyph_data is already in use. */
    } FT_IncrementalRec;

static FF_face* new_face(FT_Face a_ft_face,FT_Incremental_InterfaceRec* a_ft_inc_int,FF_stream_info* a_stream_info,
						 unsigned char* a_font_data)
    {
	FF_face* face = (FF_face *)malloc(sizeof(FF_face));
	if (face)
        {
	    face->m_ft_face = a_ft_face;
        face->m_ft_inc_int = a_ft_inc_int;
		face->m_stream_info = a_stream_info;
		face->m_font_data = a_font_data;
        }
    return face;
    }

static void delete_face(FF_face* a_face)
    {
    if (a_face)
        {
	    FT_Error ft_error = FT_Done_Face(a_face->m_ft_face);
        free(a_face->m_ft_inc_int);
		delete_stream_info(a_face->m_stream_info);
		free(a_face->m_font_data);
        free(a_face);
        }
    }

static FT_IncrementalRec* new_inc_int_info(FAPI_font* a_fapi_font)
	{
	FT_IncrementalRec* info = (FT_IncrementalRec*)malloc(sizeof(FT_IncrementalRec));
	if (info)
		{
		info->m_fapi_font = a_fapi_font;
		info->m_glyph_data = NULL;
		info->m_glyph_data_length = 0;
		info->m_glyph_data_in_use = false;
		}
	return info;
	}

static void delete_inc_int_info(FT_IncrementalRec* a_inc_int_info)
	{
	if (a_inc_int_info)
		{
		free(a_inc_int_info->m_glyph_data);
		free(a_inc_int_info);
		}
	}

static FT_Error get_fapi_glyph_data(FT_Incremental a_info,FT_UInt a_index,FT_Data* a_data)
	{
    FAPI_font* ff = a_info->m_fapi_font;
	ushort length = 0;

	/* Tell the FAPI interface that we need to decrypt the glyph data. */
	ff->need_decrypt = true;

	/* If m_glyph_data is already in use (as will happen for composite glyphs) create a new buffer on the heap. */
	if (a_info->m_glyph_data_in_use)
		{
		unsigned char* buffer = NULL;
		length = ff->get_glyph(ff,a_index,NULL,0);
		buffer = malloc(length);
		if (!buffer)
			return FT_Err_Out_Of_Memory;
		ff->get_glyph(ff,a_index,buffer,length);
		a_data->pointer = buffer;
		}
	else
		{
		/*
		Save ff->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
		make the deprecated Type 2 endchar ('seac') work, so that it can be restored
		if we need to try again with a longer buffer.
		*/
		const void* saved_char_data = ff->char_data;

		/* Get as much of the glyph data as possible into the buffer */
		length = ff->get_glyph(ff,a_index,a_info->m_glyph_data,(ushort)a_info->m_glyph_data_length);

		/* If the buffer was too small enlarge it and try again. */
		if (length > a_info->m_glyph_data_length)
			{
			a_info->m_glyph_data = realloc(a_info->m_glyph_data,length);
			if (!a_info->m_glyph_data)
				{
				a_info->m_glyph_data_length = 0;
				return FT_Err_Out_Of_Memory;
				}
			a_info->m_glyph_data_length = length;
			ff->char_data = saved_char_data;
			ff->get_glyph(ff,a_index,a_info->m_glyph_data,length);
			}

		/* Set the returned pointer and length. */
		a_data->pointer = a_info->m_glyph_data;

		a_info->m_glyph_data_in_use = true;
		}

	a_data->length = length;
	return 0;
	}

static void free_fapi_glyph_data(FT_Incremental a_info,FT_Data* a_data)
	{
	if (a_data->pointer == (const FT_Byte*)a_info->m_glyph_data)
		a_info->m_glyph_data_in_use = false;
	else
		free((FT_Byte*)a_data->pointer);
	} 

static const FT_Incremental_FuncsRec TheFAPIIncrementalInterfaceFuncs =
    {
    get_fapi_glyph_data,
	free_fapi_glyph_data,
    NULL
    };

static FT_Incremental_InterfaceRec* new_inc_int(FAPI_font* a_fapi_font)
	{
	FT_Incremental_InterfaceRec* i = (FT_Incremental_InterfaceRec*)malloc(sizeof(FT_Incremental_InterfaceRec));
	if (i)
		{
		i->object = (FT_Incremental)new_inc_int_info(a_fapi_font);
		i->funcs = &TheFAPIIncrementalInterfaceFuncs;
		}
	if (!i->object)
		{
		free(i);
		i = NULL;
		}
	return i;
	}

static void delete_inc_int(FT_Incremental_InterfaceRec* a_inc_int)
	{
	if (a_inc_int)
		{
		delete_inc_int_info(a_inc_int->object);
		free(a_inc_int);
		}
	}

/*
Convert FreeType error codes to GhostScript ones.
Very rudimentary because most don't correspond.
*/
static int ft_to_gs_error(FT_Error a_error)
    {
    if (a_error)
        {
        if (a_error == FT_Err_Out_Of_Memory)
            return e_VMerror;
        else
            return e_invalidfont;
        }
    return 0;
    }

/*
Load a glyph and optionally rasterize it. Return its metrics in a_metrics.
If a_bitmap is true convert the glyph to a bitmap.
*/
static FAPI_retcode load_glyph(FAPI_font* a_fapi_font,const FAPI_char_ref *a_char_ref,
							   FAPI_metrics* a_metrics,FT_Glyph* a_glyph,bool a_bitmap)	 
	{
    FT_Error ft_error = 0;
	FF_face* face = (FF_face*)a_fapi_font->server_font_data;
	FT_Face ft_face = face->m_ft_face;
    int index = a_char_ref->char_code;

	/*
	Save a_fapi_font->char_data, which is set to null by FAPI_FF_get_glyph as part of a hack to
	make the deprecated Type 2 endchar ('seac') work, so that it can be restored
	after the first call to FT_Load_Glyph.
	*/
	const void* saved_char_data = a_fapi_font->char_data;

    if (!a_char_ref->is_glyph_index)
	    index = FT_Get_Char_Index(ft_face,index);

	/* Refresh the pointer to the FAPI_font held by the incremental interface. */
	if (face->m_ft_inc_int)
		face->m_ft_inc_int->object->m_fapi_font = a_fapi_font;

	/* Refresh data pointers used by Type 42 fonts. GhostScript might have moved any of these. */
	if (!a_fapi_font->is_type1)
		{
		gs_font_type42* f42 = (gs_font_type42*)a_fapi_font->client_font_data;
		font_data* fd = (font_data*)(f42->client_data);
		ref sfnts = fd->u.type42.sfnts;

		/* Refresh the pointer to the SFNT array and its current element held by the custom stream. */
		if (face->m_stream_info)
			{
			face->m_stream_info->m_sfnt_array = sfnts;
			if (face->m_stream_info->m_block_index >= 0)
				{
				ref data;
				array_get(&sfnts,face->m_stream_info->m_block_index,&data);
				face->m_stream_info->m_block_data = data.value.const_bytes;
				}
			}

		/* Refresh the pointer to the font data held in the FT_Face object. */
		else
			{
			ref data;
			array_get(&sfnts,0,&data);
			ft_face->stream->base = data.value.bytes;
			}
		}

    ft_error = FT_Load_Glyph(ft_face,index,FT_LOAD_MONOCHROME | FT_LOAD_NO_SCALE);
    if (!ft_error && a_metrics)
	    {
	    a_metrics->bbox_x0 = ft_face->glyph->metrics.horiBearingX;
	    a_metrics->bbox_y0 = ft_face->glyph->metrics.horiBearingY - ft_face->glyph->metrics.height;
	    a_metrics->bbox_x1 = a_metrics->bbox_x0 + ft_face->glyph->metrics.width;
	    a_metrics->bbox_y1 = a_metrics->bbox_y0 + ft_face->glyph->metrics.height;
	    a_metrics->escapement = ft_face->glyph->metrics.horiAdvance;
	    a_metrics->em_x = ft_face->units_per_EM;
	    a_metrics->em_y = ft_face->units_per_EM;
	    }

    // We have to load the glyph again, scale it correctly, and render it if we need a bitmap.
    if (!ft_error)
		{
		a_fapi_font->char_data = saved_char_data;
	    ft_error = FT_Load_Glyph(ft_face,index,a_bitmap ? FT_LOAD_MONOCHROME | FT_LOAD_RENDER: FT_LOAD_MONOCHROME);
		}
    if (!ft_error && a_glyph)
	    ft_error = FT_Get_Glyph(ft_face->glyph,a_glyph);
    return ft_to_gs_error(ft_error);
	} 

/**
Ensure that the rasterizer is open.

In the case of FreeType this means creating the FreeType library object.
*/
static FAPI_retcode ensure_open(FAPI_server* a_server)
	{
	FF_server* s = (FF_server*)a_server;
	if (!s->m_freetype_library)
		{
		FT_Error ft_error = FT_Init_FreeType(&s->m_freetype_library);
		if (ft_error)
			return ft_to_gs_error(ft_error);
		}
	return 0;
	}

/**
Open a font and set its transformation matrix to a_matrix, which is a 6-element array representing an affine
transform, and its resolution in dpi to a_resolution, which contains horizontal and vertical components.
*/
static FAPI_retcode get_scaled_font(FAPI_server* a_server,FAPI_font* a_font,int a_subfont,const FracInt a_matrix[6],
									 const FracInt a_resolution[2],const char* a_map,bool a_vertical)
	{
	FF_server* s = (FF_server*)a_server;
	FF_face* face = (FF_face*)a_font->server_font_data;
	FT_Error ft_error = 0;

	/* Create the face if it doesn't already exist. */
	if (!face)
		{
		FT_Face ft_face = NULL;
		FT_Parameter ft_param;
        FT_Incremental_InterfaceRec* ft_inc_int = NULL;
		FF_stream_info* stream_info = NULL;
		unsigned char* own_font_data = NULL;

		/* Load a typeface from a file. */
		if (a_font->font_file_path)
			{
			ft_error = FT_New_Face(s->m_freetype_library,a_font->font_file_path,a_subfont,&ft_face);
			if (!ft_error && ft_face)
				ft_error = FT_Select_Charmap(ft_face,ft_encoding_unicode);
			}
		
		/* Load a typeface from a representation in GhostScript's memory. */
		else
			{
			FT_Open_Args open_args;

			if (a_font->is_type1)
				{
				long length;
				int type = a_font->get_word(a_font,FAPI_FONT_FEATURE_FontType,0);

				/* Tell the FAPI interface that we need to decrypt the /Subrs data. */
				a_font->need_decrypt = true;

				/*
				Serialise a type 1 font in PostScript source form, or
				a Type 2 font in binary form, so that FreeType can read it.
				*/
				if (type == 1)
					length = FF_serialize_type1_font(a_font,NULL,0);
				else
					length = FF_serialize_type2_font(a_font,NULL,0);
				open_args.memory_base = own_font_data = malloc(length);
				if (!open_args.memory_base)
					return e_VMerror;
				if (type == 1)
					open_args.memory_size = FF_serialize_type1_font(a_font,own_font_data,length);
				else
					open_args.memory_size = FF_serialize_type2_font(a_font,own_font_data,length);
				assert(open_args.memory_size == length);
				open_args.flags = ft_open_memory;
				ft_inc_int = new_inc_int(a_font);
				if (!ft_inc_int)
					{
					free(own_font_data);
					return e_VMerror;
					}
				}
			/* It must be type 42 (see code in FAPI_FF_get_glyph in zfapi.c). */
			else 
				{
				gs_font_type42* f42 = (gs_font_type42*)a_font->client_font_data;
				ref* dictionary = (ref*)a_font->client_font_data2;
				font_data* fd = (font_data*)(f42->client_data);
				ref sfnts = fd->u.type42.sfnts;
				ref* glyph_directory = NULL;

				/*
				If the data is in a single array element it can be used directly, but the pointer to
				it in the FreeType face will need to be refreshed every time a FAPI function is called
				in case GhostScript has moved the data.

				If the data is in multiple array elements we access it via a custom FreeType stream that
				reads the data from the correct array element or elements.
				*/
				if (r_size(&sfnts) == 1)
					{
					ref data;
					array_get(&sfnts,0,&data);

					/* Get a pointer to the TrueType data. */
					open_args.memory_base = data.value.const_bytes;
					open_args.memory_size = r_size(&data) & ~(uint)1; /* See Adobe Technical Note # 5012, section 4.2. */
					if (open_args.memory_size == 0)
						return e_invalidfont;

					open_args.flags = ft_open_memory;
					}
				else
					{
					/* Get the length of the TrueType data. */
					open_args.memory_size = a_font->get_long(a_font,FAPI_FONT_FEATURE_TT_size,0);
					if (open_args.memory_size == 0)
						return e_invalidfont;
					stream_info = new_stream_info(sfnts,open_args.memory_size);
					if (!stream_info)
						return e_VMerror;
					open_args.flags = ft_open_stream;
					}

				/* If there is a glyph directory we load incrementally. */
				if (dict_find_string(dictionary,"GlyphDirectory",&glyph_directory) > 0)
					{
					ft_inc_int = new_inc_int(a_font);
                    if (!ft_inc_int)
				        return e_VMerror;
					}
				}

			if (ft_inc_int)
				{
				open_args.flags = (FT_Open_Flags)(open_args.flags | ft_open_params);
				ft_param.tag = FT_PARAM_TAG_INCREMENTAL;
				ft_param.data = ft_inc_int;
				open_args.num_params = 1;
				open_args.params = &ft_param;
				}
			ft_error = FT_Open_Face(s->m_freetype_library,&open_args,a_subfont,&ft_face);
			}

		if (ft_face)
			{
			face = new_face(ft_face,ft_inc_int,stream_info,own_font_data);
			if (!face)
                {
				FT_Done_Face(ft_face);
				delete_inc_int(ft_inc_int);
				return e_VMerror;
                }
			a_font->server_font_data = face;
			}
		else
			a_font->server_font_data = NULL;	
		}

	/*
	Set the point size and transformation.
	The matrix is scaled by the shift specified in the server, 16.
	*/
	if (face)
		{
		double width = a_matrix[0] / 65536.0;
		double height = -a_matrix[3] / 65536.0;
		FT_Matrix ft_matrix;
		ft_error = FT_Set_Char_Size(face->m_ft_face,a_matrix[0] >> 10,-a_matrix[3] >> 10,
									a_resolution[0] >> 16,a_resolution[1] >> 16);

		/*+ check error code. */

		ft_matrix.xx = (FT_Fixed)(a_matrix[0] / width);
		ft_matrix.xy = (FT_Fixed)(a_matrix[1] / width);
		ft_matrix.yx = (FT_Fixed)(a_matrix[2] / height);
		ft_matrix.yy = (FT_Fixed)(a_matrix[3] / height);

		/*
		Set the transform, ignoring the translation elements, 4 and 5, for now, because they contain very large values
		derived from the current transformation matrix and so are of no use.

		The transformation inverts the glyph, producing a glyph that is upside down in FreeType terms, with its
		first row at the bottom. That is what GhostScript needs. 
		*/
		FT_Set_Transform(face->m_ft_face,&ft_matrix,NULL);
		}

	return a_font->server_font_data ? 0 : -1;
	}

/**
Return the name of a resource which maps names to character codes. Do this by setting a_decoding_id
to point to a null-terminated string. The resource is in the 'decoding' directory in the directory named by
/GenericResourceDir in \lib\gs_res.ps.
*/
static FAPI_retcode get_decodingID(FAPI_server* a_server,FAPI_font* a_font,const char** a_decoding_id)
	{
	*a_decoding_id = "Unicode";
	return 0;
	}

/**
Get the font bounding box in font units.
*/
static FAPI_retcode get_font_bbox(FAPI_server* a_server,FAPI_font* a_font,int a_box[4])
	{
	FF_face* face = (FF_face*)a_font->server_font_data;
	a_box[0] = face->m_ft_face->bbox.xMin;
	a_box[1] = face->m_ft_face->bbox.yMin;
	a_box[2] = face->m_ft_face->bbox.xMax;
	a_box[3] = face->m_ft_face->bbox.yMax;
	return 0;
	}

/**
DOCUMENTATION NEEDED

I think this function returns a boolean value in a_proportional stating whether the font is proportional
or fixed-width.
*/
static FAPI_retcode get_font_proportional_feature(FAPI_server* a_server,FAPI_font* a_font,int subfont,
												   bool* a_proportional)
	{
	*a_proportional = true;
	return 0;
	}

/**
Convert the character name in a_char_ref.char_name to a character code or glyph index and put it in a_char_ref.char_code,
setting a_char_ref.is_glyph_index as appropriate. If this is possible set a_result to true, otherwise set it to false.
The return value is a standard error return code.
*/
static FAPI_retcode can_retrieve_char_by_name(FAPI_server* a_server,FAPI_font* a_font,FAPI_char_ref* a_char_ref,
											   bool* a_result)

    /*
     * fixme :
     * This function was not intended to do anything besides returning a bool.
     * Should move the conversion to elswhere. (igorm)
     */
	{
	FF_face* face = (FF_face*)a_font->server_font_data;
	char name[128];
	if (FT_HAS_GLYPH_NAMES(face->m_ft_face) && a_char_ref->char_name_length < sizeof(name))
		{
		memcpy(name,a_char_ref->char_name,a_char_ref->char_name_length);
		name[a_char_ref->char_name_length] = 0;
		a_char_ref->char_code = FT_Get_Name_Index(face->m_ft_face,name);
		*a_result = a_char_ref->char_code != 0;
		if (*a_result)
			a_char_ref->is_glyph_index = true;
		}
	else
		*a_result = false;
	return 0;
	}
/**
Say whether we can replace metrics. Actually we can't.
*/
private FAPI_retcode can_replace_metrics(FAPI_server *server, FAPI_font *ff, FAPI_char_ref *c, int *result)
{   
    *result = 0;
    return 0;
}

/**
Retrieve the metrics of a_char_ref and put them in a_metrics.
*/
static FAPI_retcode get_char_width(FAPI_server *a_server,FAPI_font *a_font,FAPI_char_ref *a_char_ref,
									FAPI_metrics *a_metrics)
	{
	return load_glyph(a_font,a_char_ref,a_metrics,NULL,false);
	}

/**
Rasterize the character a_char and return its metrics. Do not return the bitmap but store this. It can be retrieved by
a subsequent call to get_char_raster.
*/
static FAPI_retcode get_char_raster_metrics(FAPI_server* a_server,FAPI_font* a_font,FAPI_char_ref* a_char_ref,
											 FAPI_metrics* a_metrics)
	{
	FF_server* s = (FF_server*)a_server;
	FAPI_retcode error = load_glyph(a_font,a_char_ref,a_metrics,(FT_Glyph*)&s->m_bitmap_glyph,true);
	return error;
	}

/**
Return the bitmap created by the last call to get_char_raster_metrics.
*/
static FAPI_retcode get_char_raster(FAPI_server *a_server,FAPI_raster *a_raster)
	{
	FF_server* s = (FF_server*)a_server;
	assert(s->m_bitmap_glyph);
	a_raster->p = s->m_bitmap_glyph->bitmap.buffer;
	a_raster->width = s->m_bitmap_glyph->bitmap.width;
	a_raster->height = s->m_bitmap_glyph->bitmap.rows;
	a_raster->line_step = s->m_bitmap_glyph->bitmap.pitch;
	a_raster->orig_x = s->m_bitmap_glyph->left * 16;
	a_raster->orig_y = s->m_bitmap_glyph->top * 16;
	return 0;
	}

/**
Create an outline for the character a_char and return its metrics. Do not return the outline but store this.
It can be retrieved by a subsequent call to get_char_outline.
*/
static FAPI_retcode get_char_outline_metrics(FAPI_server *a_server,FAPI_font *a_font,FAPI_char_ref *a_char_ref,
											  FAPI_metrics *a_metrics)
	{
	FF_server* s = (FF_server*)a_server;
	return load_glyph(a_font,a_char_ref,a_metrics,(FT_Glyph*)&s->m_outline_glyph,false);
	}

typedef struct FF_path_info_
	{
	FAPI_path* m_path;
	FracInt m_x;
	FracInt m_y;
	} FF_path_info;

static int move_to(FT_Vector* aTo,void* aObject)
	{
	FF_path_info* p = (FF_path_info*)aObject;
	p->m_x = aTo->x;
	p->m_y = aTo->y;
	return p->m_path->moveto(p->m_path,aTo->x,aTo->y) ? -1 : 0;
	}

static int line_to(FT_Vector* aTo,void* aObject)
	{
	FF_path_info* p = (FF_path_info*)aObject;
	p->m_x = aTo->x;
	p->m_y = aTo->y;
	return p->m_path->lineto(p->m_path,aTo->x,aTo->y) ? -1 : 0;
	}

static int conic_to(FT_Vector* aControl,FT_Vector* aTo,void* aObject)
	{
	FF_path_info* p = (FF_path_info*)aObject;
	p->m_x = aTo->x;
	p->m_y = aTo->y;
	/*
	Convert a quadratic spline to a cubic. Do this by changing the three points
	A, B and C to A, 1/3(B,A), 1/3(B,C), C - that is, the two cubic control points are
	a third of the way from the single quadratic control point to the end points. This
	gives an exact approximation to the original quadratic.
	*/
	return p->m_path->curveto(p->m_path,(p->m_x + aControl->x * 2) / 3,
						      (p->m_y + aControl->y * 2) / 3,
						      (aTo->x + aControl->x * 2) / 3,
						      (aTo->y + aControl->y * 2) / 3,
						      aTo->x,aTo->y) ? -1 : 0;
	}

static int cubic_to(FT_Vector* aControl1,FT_Vector* aControl2,FT_Vector* aTo,void* aObject)
	{
	FF_path_info* p = (FF_path_info*)aObject;
	p->m_x = aTo->x;
	p->m_y = aTo->y;
	return p->m_path->curveto(p->m_path,aControl1->x,aControl1->y,aControl2->x,aControl2->y,aTo->x,aTo->y) ? -1 : 0;
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

/**
Return the outline created by the last call to get_char_outline_metrics.
*/
static FAPI_retcode get_char_outline(FAPI_server *a_server,FAPI_path *a_path)
	{
	FF_server* s = (FF_server*)a_server;
	FF_path_info p = { 0, 0, 0 };
	FT_Error ft_error;

	p.m_path = a_path;
	ft_error = FT_Outline_Decompose(&s->m_outline_glyph->outline,&TheFtOutlineFuncs,&p);
	a_path->closepath(a_path);
	return ft_to_gs_error(ft_error);
	}

static FAPI_retcode release_char_data(FAPI_server *a_server)
	{
	FF_server* s = (FF_server*)a_server;
	FT_Done_Glyph(&s->m_outline_glyph->root);
	FT_Done_Glyph(&s->m_bitmap_glyph->root);
	s->m_outline_glyph = NULL;
	s->m_bitmap_glyph = NULL;
	return 0;
	}

static FAPI_retcode release_typeface(FAPI_server* a_server,void* a_server_font_data)
	{
	FF_face* face = (FF_face*)a_server_font_data;
	delete_face(face);
	return 0;
	}

static void gs_freetype_destroy(i_plugin_instance* a_instance,i_plugin_client_memory* a_memory);

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
    ensure_open,
    get_scaled_font,
    get_decodingID,
    get_font_bbox,
    get_font_proportional_feature,
    can_retrieve_char_by_name,
    can_replace_metrics,
    get_char_width,
    get_char_raster_metrics,
    get_char_raster,
    get_char_outline_metrics,
    get_char_outline,
    release_char_data,
    release_typeface
	};

plugin_instantiation_proc(gs_fapi_ft_instantiate);

int gs_fapi_ft_instantiate(i_ctx_t *a_context,
							i_plugin_client_memory *a_memory,
							i_plugin_instance **a_plugin_instance)
	{
	FF_server *server = (FF_server *)a_memory->alloc(a_memory,
		sizeof(FF_server),"FF_server");
    if (!server)
        return e_VMerror;
    memset(server,0,sizeof(*server));
    server->m_fapi_server = TheFreeTypeServer;
	*a_plugin_instance = &server->m_fapi_server.ig;
    return 0;
	}

static void gs_freetype_destroy(i_plugin_instance *a_plugin_instance,i_plugin_client_memory *a_memory)
	{
	FF_server *server = (FF_server *)a_plugin_instance;
	assert(server->m_fapi_server.ig.d == &TheFreeTypeDescriptor);
	FT_Done_Glyph(&server->m_outline_glyph->root);
	FT_Done_Glyph(&server->m_bitmap_glyph->root);
	FT_Done_FreeType(server->m_freetype_library);
    a_memory->free(a_memory,server,"FF_server");
	}
