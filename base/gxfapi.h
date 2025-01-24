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

/* Font API support  */

#ifndef gxfapi_INCLUDED
#define gxfapi_INCLUDED

#include "gsmemory.h"
#include "gsccode.h"
#include "stdint_.h"
#include "gsgstate.h"
#include "gsfont.h"
#include "gstext.h"

typedef struct gs_font_base_s gs_font_base;

typedef int fracint;            /* A fractional integer with statically unknown number of fraction bits.
                                   The number of bits depends on plugin and is being specified in
                                   gs_fapi_server::frac_shift.
                                 */
typedef int gs_fapi_retcode;

typedef enum
{
    gs_fapi_font_feature_FontMatrix,
    gs_fapi_font_feature_UniqueID,
    gs_fapi_font_feature_BlueScale,
    gs_fapi_font_feature_Weight,
    gs_fapi_font_feature_ItalicAngle,
    gs_fapi_font_feature_IsFixedPitch,
    gs_fapi_font_feature_UnderLinePosition,
    gs_fapi_font_feature_UnderlineThickness,
    gs_fapi_font_feature_FontType,
    gs_fapi_font_feature_FontBBox,
    gs_fapi_font_feature_BlueValues_count,
    gs_fapi_font_feature_BlueValues,
    gs_fapi_font_feature_OtherBlues_count,
    gs_fapi_font_feature_OtherBlues,
    gs_fapi_font_feature_FamilyBlues_count,
    gs_fapi_font_feature_FamilyBlues,
    gs_fapi_font_feature_FamilyOtherBlues_count,
    gs_fapi_font_feature_FamilyOtherBlues,
    gs_fapi_font_feature_BlueShift,
    gs_fapi_font_feature_BlueFuzz,
    gs_fapi_font_feature_StdHW,
    gs_fapi_font_feature_StdVW,
    gs_fapi_font_feature_StemSnapH_count,
    gs_fapi_font_feature_StemSnapH,
    gs_fapi_font_feature_StemSnapV_count,
    gs_fapi_font_feature_StemSnapV,
    gs_fapi_font_feature_ForceBold,
    gs_fapi_font_feature_LanguageGroup,
    gs_fapi_font_feature_lenIV,
    gs_fapi_font_feature_GlobalSubrs_count,
    gs_fapi_font_feature_Subrs_count,
    gs_fapi_font_feature_Subrs_total_size,
    gs_fapi_font_feature_TT_size,
    /* Multiple Master specifics */
    gs_fapi_font_feature_DollarBlend,
    gs_fapi_font_feature_DollarBlend_length,
    gs_fapi_font_feature_BlendAxisTypes_count,
    gs_fapi_font_feature_BlendAxisTypes,
    gs_fapi_font_feature_BlendPrivate_count,
    gs_fapi_font_feature_BlendFontInfo_count,
    gs_fapi_font_feature_BlendFontBBox_length,
    gs_fapi_font_feature_BlendFontBBox,

    gs_fapi_font_feature_BlendBlueValues_length,
    gs_fapi_font_feature_BlendBlueValues_count,
    gs_fapi_font_feature_BlendBlueValues,
    gs_fapi_font_feature_BlendOtherBlues_length,
    gs_fapi_font_feature_BlendOtherBlues_count,
    gs_fapi_font_feature_BlendOtherBlues,
    gs_fapi_font_feature_BlendBlueScale_count,
    gs_fapi_font_feature_BlendBlueScale,
    gs_fapi_font_feature_BlendBlueShift_count,
    gs_fapi_font_feature_BlendBlueShift,
    gs_fapi_font_feature_BlendBlueFuzz_count,
    gs_fapi_font_feature_BlendBlueFuzz,
    gs_fapi_font_feature_BlendForceBold_count,
    gs_fapi_font_feature_BlendForceBold,
    gs_fapi_font_feature_BlendStdHW_length,
    gs_fapi_font_feature_BlendStdHW_count,
    gs_fapi_font_feature_BlendStdHW,
    gs_fapi_font_feature_BlendStdVW_length,
    gs_fapi_font_feature_BlendStdVW_count,
    gs_fapi_font_feature_BlendStdVW,
    gs_fapi_font_feature_BlendStemSnapH_length,
    gs_fapi_font_feature_BlendStemSnapH_count,
    gs_fapi_font_feature_BlendStemSnapH,
    gs_fapi_font_feature_BlendStemSnapV_length,
    gs_fapi_font_feature_BlendStemSnapV_count,
    gs_fapi_font_feature_BlendStemSnapV,

    gs_fapi_font_feature_WeightVector_count,
    gs_fapi_font_feature_WeightVector,
    gs_fapi_font_feature_BlendDesignPositionsArrays_count,
    gs_fapi_font_feature_BlendDesignPositionsArrayValue,
    gs_fapi_font_feature_BlendDesignMapArrays_count,
    gs_fapi_font_feature_BlendDesignMapSubArrays_count,
    gs_fapi_font_feature_BlendDesignMapArrayValue,
    /* End MM specifics */
    /* CharString emission */
    gs_fapi_font_feature_CharStrings_count,
    /* End CharString emission */
} gs_fapi_font_feature;

typedef enum
{
    gs_fapi_glyph_invalid_format = -1,
    gs_fapi_glyph_invalid_index = -2
} gs_fapi_glyph_error;

typedef enum
{
    gs_fapi_metrics_notdef,
    gs_fapi_metrics_add,        /* Add to native glyph width. */
    gs_fapi_metrics_replace_width,      /* Replace the native glyph width. */
    gs_fapi_metrics_replace     /* Replace the native glyph width and lsb. */
} gs_fapi_metrics_type;

typedef struct
{
    gs_char client_char_code;       /* Debug purpose. */
    gs_glyph char_codes[4];
    int char_codes_count;
    bool is_glyph_index;        /* true if char_code contains glyph index */
    const unsigned char *char_name;     /* to be used exclusively with char_code. */
    unsigned int char_name_length;
    gs_fapi_metrics_type metrics_type;
    fracint sb_x, sb_y, aw_x, aw_y;     /* replaced PS metrics. */
    int metrics_scale;          /* Scale for replaced PS metrics.
                                   Zero means "em box size". */
} gs_fapi_char_ref;


typedef struct
{
    int platform_id;
    int encoding_id;
} gs_fapi_ttf_cmap_request;

#define GS_FAPI_NUM_TTF_CMAP_REQ 10

typedef struct gs_fapi_font_s gs_fapi_font;
struct gs_fapi_font_s
{
    /* server's data : */
    void *server_font_data;
    bool need_decrypt;
    /* client's data : */
    const gs_memory_t *memory;
    const char *font_file_path;
    const char *full_font_buf;  /* If the font data is entirely contained in a memory buffer */
    int full_font_buf_len;
    int subfont;
    bool is_type1;              /* Only for non-disk fonts; dirty for disk fonts. */
    bool is_cid;
    bool is_outline_font;
    bool is_mtx_skipped;        /* Ugly. Only UFST needs. */
    bool is_vertical;
    bool metrics_only;  /* can save the expense of loading entire glyph */
    gs_fapi_ttf_cmap_request ttf_cmap_req[GS_FAPI_NUM_TTF_CMAP_REQ]; /* Lets client request a specific cmap to be set. Also, a couple of fallbacks */
    gs_fapi_ttf_cmap_request ttf_cmap_selected;                      /* If the request contains multiple cmaps, this records which was actually selected */
    void *client_ctx_p;
    void *client_font_data;
    void *client_font_data2;
    const void *char_data;
    int char_data_len;
    float embolden;

    int (*get_word) (gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned short *ret);

    int (*get_long) (gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, unsigned long *ret);

    int (*get_float) (gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, float *ret);

    int (*get_name) (gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *buffer, int len);

    int (*get_proc) (gs_fapi_font *ff, gs_fapi_font_feature var_id, int index, char *Buffer);

    int (*get_gsubr) (gs_fapi_font *ff, int index, byte *buf, int buf_length);

    int (*get_subr) (gs_fapi_font *ff, int index, byte *buf, int buf_length);

    int (*get_raw_subr) (gs_fapi_font *ff, int index, byte *buf, int buf_length);

    int (*get_glyph) (gs_fapi_font *ff, gs_glyph char_code, byte *buf, int buf_length);

    int (*serialize_tt_font) (gs_fapi_font *ff, void *buf, int buf_size);

    int (*retrieve_tt_font) (gs_fapi_font *ff, void **buf, int *buf_size);

    int (*get_charstring) (gs_fapi_font *ff, int index, byte *buf, ushort buf_length);

    int (*get_charstring_name) (gs_fapi_font *ff, int index, byte *buf, ushort buf_length);

    int (*get_glyphdirectory_data) (gs_fapi_font *ff, int char_code, const byte **ptr);

    int (*get_glyphname_or_cid) (gs_text_enum_t *penum, gs_font_base *pbfont,
                                 gs_string *charstring, gs_string *name,
                                 gs_glyph ccode, gs_string *enc_char_name,
                                  char *font_file_path, gs_fapi_char_ref *cr,
                                 bool bCID);

    int (*fapi_get_metrics) (gs_fapi_font *ff, gs_string *char_name, gs_glyph cid, double *m, bool vertical);

    int (*fapi_set_cache) (gs_text_enum_t *penum,
                           const gs_font_base *pbfont,
                           const gs_string *char_name, gs_glyph cid,
                           const double pwidth[2], const gs_rect *pbbox,
                           const double Metrics2_sbw_default[4],
                           bool *imagenow);
};

#define GS_FAPI_WEIGHTVECTOR_MAX 16

typedef struct gs_fapi_face_s gs_fapi_face;
struct gs_fapi_face_s
{
    gs_id font_id;
    gs_matrix ctm;
    gs_log2_scale_point log2_scale;
    bool align_to_pixels;
    float HWResolution[2];
    struct {
        int count;
        float values[GS_FAPI_WEIGHTVECTOR_MAX];
    } WeightVector;
};

typedef struct gs_fapi_path_s gs_fapi_path;
struct gs_fapi_path_s
{
    void *olh;                  /* Client's data. */
    int shift;
    int gs_error;
    int (*moveto) (gs_fapi_path *, int64_t, int64_t);
    int (*lineto) (gs_fapi_path *, int64_t, int64_t);
    int (*curveto) (gs_fapi_path *, int64_t, int64_t, int64_t, int64_t,
                    int64_t, int64_t);
    int (*closepath) (gs_fapi_path *);
};

typedef struct gs_fapi_font_scale_s
{
    fracint matrix[6];
    fracint HWResolution[2];
    int subpixels[2];
    bool align_to_pixels;
} gs_fapi_font_scale;

typedef struct gs_fapi_metrics_s
{
    int bbox_x0, bbox_y0, bbox_x1, bbox_y1;     /* design units */
    int escapement;             /* design units */
    int v_escapement;           /* design units */
    int em_x, em_y;             /* design units */
} gs_fapi_metrics;

typedef struct
{                               /* 1bit/pixel only, rows are byte-aligned. */
    void *p;
    int width, height, line_step;
    int orig_x, orig_y;         /* origin, 1/16s pixel */
    int left_indent, top_indent;
    int black_width, black_height;
} gs_fapi_raster;

typedef enum gs_fapi_descendant_code_s
{                               /* Possible values are descendant font indices and 4 ones defined below. */
    gs_fapi_descendant_prepared = -1,   /* See FAPI_prepare_font in zfapi.c . */
    gs_fapi_toplevel_prepared = -2,
    gs_fapi_toplevel_begin = -3,
    gs_fapi_toplevel_complete = -4
} gs_fapi_descendant_code;

/* interrogate scaler about style simulation capabilities
 * for a given font
 */
typedef enum gs_fapi_style_s
{
    gs_fapi_style_bold = 1
} gs_fapi_style;

/* interrogate scaler about style simulation capabilities
 * for a given font
 */
typedef enum gs_fapi_font_info_s
{
    gs_fapi_font_info_name = 1,
    gs_fapi_font_info_bbox = 2,
    gs_fapi_font_info_pitch = 3,
    gs_fapi_font_info_uid = 4,
    gs_fapi_font_info_design_units = 5
} gs_fapi_font_info;

typedef struct gs_fapi_server_descriptor_s gs_fapi_server_descriptor;
typedef struct gs_fapi_server_instance_s gs_fapi_server_instance;

struct gs_fapi_server_descriptor_s
{
    const char *type;
    const char *subtype;
    void (*finit) (gs_fapi_server **server);
};

struct gs_fapi_server_instance_s
{                               /* Base class for various plugins */
    const gs_fapi_server_descriptor *d;
};


struct gs_fapi_server_s
{
    gs_fapi_server_instance ig;
    void *client_ctx_p;
    int frac_shift;             /* The number of fractional bits in coordinates. */
    gs_fapi_face face;
    gs_fapi_font ff;
    int max_bitmap;
    bool use_outline;
    bool transform_outline;
    gs_matrix outline_mat;
    uint grid_fit;
    gs_matrix initial_FontMatrix;       /* Font Matrix at the time the font is defined */
    /* Used to use the stored 'OrigFont' entry but */
    /* this did not change f a font was defined    */
    /* using an existing base font */



    gs_fapi_retcode(*ensure_open) (gs_fapi_server *server, const char *param, int param_size);
    gs_fapi_retcode(*get_scaled_font) (gs_fapi_server *server, gs_fapi_font *ff, const gs_fapi_font_scale *scale, const char *xlatmap, gs_fapi_descendant_code dc);
    gs_fapi_retcode(*get_decodingID) (gs_fapi_server *server, gs_fapi_font *ff, const char **decodingID);
    gs_fapi_retcode(*get_font_bbox) (gs_fapi_server *server, gs_fapi_font *ff, int BBox[4], int unitsPerEm[2]);
    gs_fapi_retcode(*get_font_proportional_feature) (gs_fapi_server *server, gs_fapi_font *ff, bool *bProportional);
    gs_fapi_retcode(*can_retrieve_char_by_name) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c, int *result);
    gs_fapi_retcode(*can_replace_metrics) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c, int *result);
    gs_fapi_retcode(*can_simulate_style) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_style style, void *style_data);
    gs_fapi_retcode(*get_fontmatrix) (gs_fapi_server *server, gs_matrix *m);
    gs_fapi_retcode(*get_char_width) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c, gs_fapi_metrics *metrics);
    gs_fapi_retcode(*get_char_raster_metrics) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c, gs_fapi_metrics *metrics);
    gs_fapi_retcode(*get_char_raster) (gs_fapi_server *server, gs_fapi_raster *r);
    gs_fapi_retcode(*get_char_outline_metrics) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_char_ref *c, gs_fapi_metrics *metrics);
    gs_fapi_retcode(*get_char_outline) (gs_fapi_server *server, gs_fapi_path *p);
    gs_fapi_retcode(*release_char_data) (gs_fapi_server *server);

    gs_fapi_retcode(*release_typeface) (gs_fapi_server *server, void *server_font_data);
    gs_fapi_retcode(*check_cmap_for_GID) (gs_fapi_server *server, uint *index);
    gs_fapi_retcode(*get_font_info) (gs_fapi_server *server, gs_fapi_font *ff, gs_fapi_font_info item, int index, void *data, int *datalen);
    gs_fapi_retcode(*set_mm_weight_vector) (gs_fapi_server *server, gs_fapi_font *ff, float *wvector, int length);

    /*  Some people get confused with terms "font cache" and "character cache".
       "font cache" means a cache for scaled font objects, which mainly
       keep the font header information and rules for adjusting it to specific raster.
       "character cahce" is a cache for specific character outlines and/or character rasters.
     */
    /*  get_scaled_font opens a typeface with a server and scales it according to CTM and HWResolution.
       This creates a server's scaled font object.
       Since UFST doesn't provide a handle to this object,
       we need to build the data for it and call this function whenever scaled font data may change.
       The server must cache scaled fonts internally.
       Note that FreeType doesn't provide internal font cache,
       so the bridge must do.
     */
    /*  Due to the interpreter fallback with CDevProc,
       get_char_raster_metrics leaves some data kept by the server,
       so taht get_char_raster uses them and release_char_data releases them.
       Therefore calls from GS to these functions must not
       interfer with different characters.
     */
};

/* The font type 10 (ft_CID_user_defined) must not pass to FAPI. */
#define FAPI_ISCIDFONT(basefont) (basefont->FontType == ft_CID_encrypted ||\
                         basefont->FontType == ft_CID_user_defined || \
                         basefont->FontType == ft_CID_TrueType)


#define FAPI_ISTYPE1GLYPHDATA(basefont) (pbfont->FontType == ft_encrypted || \
                                basefont->FontType == ft_encrypted2 ||\
                                basefont->FontType == ft_CID_encrypted)

typedef int (*gs_fapi_get_server_param_callback) (gs_fapi_server *I,
                                                  const char *subtype,
                                                  char **server_param,
                                                  int *server_param_size);

typedef int (*gs_fapi_server_init_func) (gs_memory_t *mem,
                                         gs_fapi_server **server);

#define fapi_init_func(proc)\
  int proc(gs_memory_t *mem, gs_fapi_server **server)

/* Convienence function which uses a client's "prototype" gs_fapi_font
 * to fill in the actual fapi font for a gs_font, and sets the opaque
 * pointer for the client's private data (ctx_ptr).
 */
void gs_fapi_set_servers_client_data(gs_memory_t *mem,
                                     const gs_fapi_font *ff_proto,
                                     void *ctx_ptr);

/* Retrieve and initialize a FAPI server by name */
int gs_fapi_find_server(gs_memory_t *mem, const char *name,
                        gs_fapi_server **server,
                        gs_fapi_get_server_param_callback
                        get_server_param_cb);

gs_fapi_server **gs_fapi_get_server_list(gs_memory_t *mem);

/* Check if a FAPI server is available by name (server). If
 * server == NULL it just checks whether FAPI is there at all
 */
bool gs_fapi_available(gs_memory_t *mem, char *server);

int gs_fapi_get_metrics_count(gs_fapi_font *ff);

int
gs_fapi_prepare_font(gs_font *pfont, gs_fapi_server *I, int subfont, const char *font_file_path,
                     gs_string *full_font_buf, const char *xlatmap, const char **decodingID);

int
gs_fapi_finish_render(gs_font *pfont, gs_gstate *pgs, gs_text_enum_t *penum, gs_fapi_server *I);

int
gs_fapi_do_char(gs_font *pfont, gs_gstate *pgs, gs_text_enum_t *penum, char *font_file_path,
                bool bBuildGlyph, gs_string *charstring, gs_string *glyphname,
                gs_char chr, gs_glyph index, int subfont);

/* "General purpose" call to allow the client to interrogate
 * the font through the FAPI server - WARNING: depends on the
 * capabilities of the server.
 * The caller needs to pass in memory (data) large enough (data_len)
 * to hold the requested information - data == NULL, or data_len too
 * small will see data_len set to the required amount, and an error
 * returned.
 */
int
gs_fapi_get_font_info(gs_font *pfont, gs_fapi_font_info item, int index,
                      void *data, int *data_len);

int
gs_fapi_passfont(gs_font *pfont, int subfont, char *font_file_path,
                 gs_string *full_font_buf, char *fapi_request, char *xlatmap,
                 char **fapi_id, char **decodingID, gs_fapi_get_server_param_callback get_server_param_cb);

int gs_fapi_init(gs_memory_t *mem);

void gs_fapi_finit(gs_memory_t *mem);


void gx_fapi_get_ulp_character_data(byte **header, byte **character_data);

void gx_fapi_bits_smear_horizontally(byte *dest, const byte *src, uint width, uint smear_width);

void gx_fapi_bits_merge(byte *dest, const byte *src, uint nbytes);
#endif /* gxfapi_INCLUDED */
