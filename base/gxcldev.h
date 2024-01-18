/* Copyright (C) 2001-2024 Artifex Software, Inc.
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


/* Internal definitions for Ghostscript command lists. */

#ifndef gxcldev_INCLUDED
#  define gxcldev_INCLUDED

#include "gxclist.h"
#include "gsropt.h"
#include "gxht.h"		/* for gxdht.h */
#include "gxtmap.h"		/* ditto */
#include "gxdht.h"		/* for halftones */
#include "strimpl.h"		/* for compressed bitmaps */
#include "scfx.h"		/* ditto */
#include "srlx.h"		/* ditto */
#include "gsdcolor.h"

#ifdef HAVE_VALGRIND
#include "valgrind/memcheck.h"
#endif

/* ---------------- Commands ---------------- */

/* Define the compression modes for bitmaps. */
/*#define cmd_compress_none 0 *//* (implicit) */
#define cmd_compress_rle 1
#define cmd_compress_cfe 2
#define cmd_compress_const 3
#define cmd_mask_compress_any\
  ((1 << cmd_compress_rle) | (1 << cmd_compress_cfe) | (1 << cmd_compress_const))

/* Exported by gxclutil.c */
void clist_rle_init(stream_RLE_state *ss);
void clist_rld_init(stream_RLD_state *ss);
void clist_cfe_init(stream_CFE_state *ss, int width, gs_memory_t *mem);
void clist_cfd_init(stream_CFD_state *ss, int width, int height,
                    gs_memory_t *mem);

/* Write out a block of data of arbitrary size and content to a      */
/* specified band at the given offset PAST the last band in the page */
/* Used for various data such as the icc profile table and the       */
/* per band color_usage array                                        */
/*                                                                   */
/* If the size is not well defined, then the data itself should      */
/* contain information for length or logical end-of-data.            */
int cmd_write_pseudo_band(gx_device_clist_writer *cldev, unsigned char *pbuf,
                          int data_size, int pseudo_band_offset);
/*
 * A command always consists of an operation followed by operands;
 * the syntax of the operands depends on the operation.
 * In the operation definitions below:
 *      + (prefixed) means the operand is in the low 4 bits of the opcode.
 *      # means a variable-size operand encoded with the variable-size
 *         integer encoding.
 *      % means a variable-size operand encoded with the variable-size
 *         fixed coordinate encoding.
 *      $ means a color sized according to the device depth.
 *      <> means the operand size depends on other state information
 *         and/or previous operands.
 */
typedef enum {
    cmd_op_misc              = 0x00, /* (see below) */
    cmd_opv_end_run          = 0x00, /* (nothing) */
    cmd_opv_set_tile_size    = 0x01, /* rs?(1)nry?(1)nrx?(1)depth(5, encoded), */
                                     /* rep_width#, rep_height#, */
                                     /* [, nreps_x#][, nreps_y #] */
                                     /* [, rep_shift#] */
    cmd_opv_set_tile_phase   = 0x02, /* x#, y# */
    cmd_opv_set_tile_bits    = 0x03, /* index#, offset#, <bits> */
    cmd_opv_set_bits         = 0x04, /* depth*4+compress, width#, height#, */
                                     /* index#, offset#, <bits> */
    cmd_opv_set_tile_color   = 0x05, /* (nothing; next set/delta_color */
                                     /* refers to tile) */
    cmd_opv_set_misc         = 0x06,
#define cmd_set_misc_lop      (0 << 6) /* 00: lop_lsb(6), lop_msb# */
#define cmd_set_misc_data_x   (1 << 6) /* 01: more(1)dx_lsb(5)[, dx_msb#] */
#define cmd_set_misc_map      (2 << 6) /* 10: contents(2)map_index(4) */
                                       /* [, n x frac] */
#define cmd_set_misc_halftone (3 << 6) /* 11: type(6), num_comp# */
    cmd_opv_enable_lop       = 0x07, /* (nothing) */
    cmd_opv_disable_lop      = 0x08, /* (nothing) */
    cmd_opv_set_screen_phaseT= 0x09, /* x#, y# */
    cmd_opv_set_screen_phaseS= 0x0a, /* x#, y# */
    cmd_opv_end_page         = 0x0b, /* (nothing) */
    cmd_opv_delta_color0     = 0x0c, /* See cmd_put_color in gxclutil.c */
    cmd_opv_delta_color1     = 0x0d, /* <<same as color0>> */
    cmd_opv_set_copy_color   = 0x0e, /* (nothing) */
    cmd_opv_set_copy_alpha   = 0x0f, /* (nothing) */
    cmd_op_set_color0        = 0x10, /* +n = number of low order zero bytes | */
#define cmd_no_color_index 15        /* +15 = transparent - "no color" */
    cmd_op_set_color1        = 0x20, /* <<same as color0>> */
    cmd_op_fill_rect         = 0x30, /* +dy2dh2, x#, w# | +0, rect# */
    cmd_op_fill_rect_short   = 0x40, /* +dh, dx, dw | +0, rect_short */
    cmd_op_fill_rect_tiny    = 0x50, /* +dw+0, rect_tiny | +dw+8 */
    cmd_op_tile_rect         = 0x60, /* +dy2dh2, x#, w# | +0, rect# */
    cmd_op_tile_rect_short   = 0x70, /* +dh, dx, dw | +0, rect_short */
    cmd_op_tile_rect_tiny    = 0x80, /* +dw+0, rect_tiny | +dw+8 */
    cmd_op_copy_mono_planes  = 0x90, /* +compress, plane_height, x#, y#, (w+data_x)#, */
                                     /* h#, <bits> | */
#define cmd_copy_use_tile 8          /* +8 (use tile), x#, y# | */
    cmd_op_copy_color_alpha  = 0xa0, /* (same as copy_mono, except: */
                                     /* if color, ignore ht_color; */
                                     /* if alpha & !use_tile, depth is */
                                     /*   first operand) */
    cmd_op_delta_tile_index  = 0xb0, /* +delta+8 */
    cmd_op_set_tile_index    = 0xc0, /* +index[11:8], index[7:0] */
    cmd_op_misc2             = 0xd0, /* (see below) */
    cmd_opv_set_bits_planar  = 0xd0, /* depth*4+compress, width#, height#, */
                                     /* num_planes, index#, offset#, <bits> */
    cmd_op_fill_rect_hl      = 0xd1, /* rect fill with devn color */
    cmd_opv_set_fill_adjust  = 0xd2, /* adjust_x/y(fixed) */
    cmd_opv_set_ctm          = 0xd3, /* [per sput/sget_matrix] */
    cmd_opv_set_color_space  = 0xd4, /* base(4)Indexed?(2)0(2) */
                                     /* [, hival#, table|map] */
    /*
     * cmd_opv_set_misc2_value is followed by a mask (a variable-length
     * integer), and then by parameter values for the parameters selected
     * by the mask.  See gxclpath.h for the "known" mask values.
     */
    /* cap_join:      0(2)cap(3)join(3) */
    /* cj_ac_sa:      0(3)curve_join+1(3)acc.curves(1)stroke_adj(1) */
    /* flatness:      (float) */
    /* line width:    (float) */
    /* miter limit:   (float) */
    /* op_bm_tk:      blend mode(5)text knockout(1)o.p.mode(1)o.p.(1) */
    /* segment notes: (byte) */
    /* opacity/shape: alpha(float)mask(TBD) */
    /* alpha:         <<verbatim copy from gs_gstate>> */
    cmd_opv_set_misc2        = 0xd5, /* mask#, selected parameters */
    cmd_opv_set_dash         = 0xd6, /* adapt(1)abs.dot(1)n(6), dot */
                                     /* length(float), offset(float), */
                                     /* n x (float) */
    cmd_opv_enable_clip      = 0xd7, /* (nothing) */
    cmd_opv_disable_clip     = 0xd8, /* (nothing) */
    cmd_opv_begin_clip       = 0xd9, /* fill_adjust.x#, fill_adjust.y# */
    cmd_opv_end_clip         = 0xda, /* (nothing) */
    cmd_opv_begin_image_rect = 0xdb, /* same as begin_image, followed by */
                                     /* x0#, w-x1#, y0#, h-y1# */
    cmd_opv_begin_image      = 0xdc, /* image_type_table index, */
                                     /* [per image type] */
    cmd_opv_image_data       = 0xdd, /* height# (premature EOD if 0), */
                                     /* raster#, <data> */
    cmd_opv_image_plane_data = 0xde, /* height# (premature EOD if 0), */
                                     /* flags# (0 = same raster & data_x, */
                                     /* 1 = new raster & data_x, lsb first), */
                                     /* [raster#, [data_x#,]]* <data> */
    cmd_opv_extend           = 0xdf, /* command, varies (see gx_cmd_ext_op below) */


#define cmd_misc2_op_name_strings\
  "set_bits_planar", "fill_hl_color", \
  "set_fill_adjust", "set_ctm",\
  "set_color_space", "set_misc2", "set_dash", "enable_clip",\
  "disable_clip", "begin_clip", "end_clip", "begin_image_rect",\
  "begin_image", "image_data", "image_plane_data", "extended"

    cmd_op_segment           = 0xe0, /* (see below) */
    cmd_opv_rmoveto          = 0xe0, /* dx%, dy% */
    cmd_opv_rlineto          = 0xe1, /* dx%, dy% */
    cmd_opv_hlineto          = 0xe2, /* dx% */
    cmd_opv_vlineto          = 0xe3, /* dy% */
    cmd_opv_rmlineto         = 0xe4, /* dx1%,dy1%, dx2%,dy2% */
    cmd_opv_rm2lineto        = 0xe5, /* dx1%,dy1%, dx2%,dy2%, dx3%,dy3% */
    cmd_opv_rm3lineto        = 0xe6, /* dx1%,dy1%, dx2%,dy2%, dx3%,dy3%, */
                                     /* [-dx2,-dy2 implicit] */
    cmd_opv_rrcurveto        = 0xe7, /* dx1%,dy1%, dx2%,dy2%, dx3%,dy3% */
    cmd_opv_min_curveto = cmd_opv_rrcurveto,
    cmd_opv_hvcurveto        = 0xe8, /* dx1%, dx2%,dy2%, dy3% */
    cmd_opv_vhcurveto        = 0xe9, /* dy1%, dx2%,dy2%, dx3% */
    cmd_opv_nrcurveto        = 0xea, /* dx2%,dy2%, dx3%,dy3% */
    cmd_opv_rncurveto        = 0xeb, /* dx1%,dy1%, dx2%,dy2% */
    cmd_opv_vqcurveto        = 0xec, /* dy1%, dx2%[,dy2=dx2 with sign */
                                     /* of dy1, dx3=dy1 with sign of dx2] */
    cmd_opv_hqcurveto        = 0xed, /* dx1%, [dx2=dy2 with sign */
                                     /* of dx1,]%dy2, [dy3=dx1 with sign */
                                     /* of dy2] */
    cmd_opv_scurveto         = 0xee, /* all implicit: previous op must have been */
                                     /* *curveto with one or more of dx/y1/3 = 0. */
                                     /* If h*: -dx3,dy3, -dx2,dy2, -dx1,dy1. */
                                     /* If v*: dx3,-dy3, dx2,-dy2, dx1,-dy1. */
    cmd_opv_max_curveto = cmd_opv_scurveto,
    cmd_opv_closepath        = 0xef, /* (nothing) */

#define cmd_segment_op_name_strings\
  "rmoveto", "rlineto", "hlineto", "vlineto",\
  "rmlineto", "rm2lineto", "rm3lineto", "rrcurveto",\
  "hvcurveto", "vhcurveto", "nrcurveto", "rncurveto",\
  "vqcurveto", "hqcurveto", "scurveto", "closepath"

    cmd_op_path              = 0xf0, /* (see below) */
    cmd_opv_fill             = 0xf0,
    cmd_opv_rgapto           = 0xf1, /* dx%, dy% */
    cmd_opv_lock_pattern     = 0xf2, /* lock, id */
    cmd_opv_eofill           = 0xf3,
    cmd_opv_fill_stroke      = 0xf4,
    cmd_opv_eofill_stroke    = 0xf5,
    cmd_opv_stroke           = 0xf6,
    /* UNUSED 0xf7 */
    /* UNUSED 0xf8 */
    cmd_opv_polyfill         = 0xf9,
    /* UNUSED 0xfa */
    /* UNUSED 0xfb */
    cmd_opv_fill_trapezoid   = 0xfc
    /* UNUSED 0xfd */
    /* UNUSED 0xfe */
    /* UNUSED 0xff */

#define cmd_path_op_name_strings\
  "fill", "rgapto", "lock_pattern", "eofill",\
  "fill_stroke", "eofill_stroke", "stroke", "?f7?",\
  "?f8?", "polyfill", "?fa?", "?fb?",\
  "fill_trapezoid", "?fd?", "?fe?", "?ff?"

/* unused cmd_op values: 0xf7, 0xf8, 0xfa, 0xfb, 0xfd, 0xfe, 0xff */
} gx_cmd_op;

#define cmd_op_name_strings\
  "(misc)", "set_color[0]", "set_color[1]", "fill_rect",\
  "fill_rect_short", "fill_rect_tiny", "tile_rect", "tile_rect_short",\
  "tile_rect_tiny", "copy_mono_planes", "copy_color_alpha", "delta_tile_index",\
  "set_tile_index", "(misc2)", "(segment)", "(path)"

#define cmd_misc_op_name_strings\
  "end_run", "set_tile_size", "set_tile_phase", "set_tile_bits",\
  "set_bits", "set_tile_color", "set_misc", "enable_lop",\
  "disable_lop", "set_screen_phaseT", "set_screen_phaseS", "end_page",\
  "delta2_color0", "delta2_color1", "set_copy_color", "set_copy_alpha",

#ifdef DEBUG
extern const char *const cmd_op_names[16];
extern const char *const *const cmd_sub_op_names[16];
#endif

/*
 * Define the size of the largest command, not counting any bitmap or
 * similar variable-length operands.
 * The variable-size integer encoding is little-endian.  The low 7 bits
 * of each byte contain data; the top bit is 1 for all but the last byte.
 */
#define cmd_max_intsize(siz)\
  (((siz) * 8 + 6) / 7)

/* NB: Assume that the largest size is for dash patterns. Larger that this
 *     need to be read from the cbuf in a loop.
 */
#define cmd_largest_size\
  (2 + sizeof(float)		/* dot_length */\
     + sizeof(float)           /* offset */\
     + (cmd_max_dash * sizeof(float))\
  )

/* ---------------- Command parameters ---------------- */

/* Rectangle */
typedef struct {
    int x, y, width, height;
} gx_cmd_rect;

/* Short rectangle */
typedef struct {
    byte dx, dwidth, dy, dheight;	/* dy and dheight are optional */
} gx_cmd_rect_short;

#define cmd_min_short (-128)
#define cmd_max_short 127
/* Tiny rectangle */
#define cmd_min_dw_tiny (-4)
#define cmd_max_dw_tiny 3
typedef struct {
    unsigned dx:4;
    unsigned dy:4;
} gx_cmd_rect_tiny;

#define cmd_min_dxy_tiny (-8)
#define cmd_max_dxy_tiny 7

/*
 * Encoding for tile depth information.
 *
 * The cmd_opv_set_tile_size command code stores tile depth information
 * as part of the first byte following the command code. Throughout
 * the history of ghostscript, at least 3 different encodings have been used
 * here.
 *
 * Originally, we used 5 bits of the byte to hold 'depth-1'.
 *
 * Later, the DeviceN code required it to cope with depths of >32 bits, so
 * a new encoding was used that represented depth information either directly
 * (for depth <= 15), or as a multiple of 8. The high-order bit determined
 * which was the case; it was cleared if the depth is represented directly,
 * and set if the depth was represented as a multiple of 8.
 *
 * #define cmd_depth_to_code(d)    ((d) > 0xf ? 0x10 | ((d) >> 3) : (d))
 * #define cmd_code_to_depth(v)    \
 *    (((v) & 0x10) != 0 ? ((v) & 0xf) << 3 : (v) & 0xf)
 *
 * With the advent of the planar device, we needed to use one fewer bit in
 * the byte, so adopted the following encoding scheme:
 *   depth:  1  2 (3) 4 (5) (6) (7) 8  12  16  24  32  40  48  56  64
 *   value:  0  1 (2) 3 (4) (5) (6) 7   8   9  10  11  12  13  14  15
 * The numbers in brackets represent depths that are represented, but aren't
 * used by ghostscript (i.e. are available for future use).
 */
#define cmd_depth_to_code(d)    ((d) > 8 ? 8 | (((d)-5) >> 3) : ((d)-1))
#define cmd_code_to_depth(v)    \
    (((v) & 8) == 0 ? ((v) & 0x7)+1 : ((v) & 0x7 ? ((((v) & 7) << 3) + 8) : 12))

/*
 * When we write bitmaps, we remove raster padding selectively:
 *      - If the bitmap is compressed, we don't remove any padding;
 *      - If the width is <= 6 bytes, we remove all the padding;
 *      - If the bitmap is only 1 scan line high, we remove the padding;
 *      - If the bitmap is going to be replicated horizontally (see the
 *      definition of decompress_spread below), we remove the padding;
 *      - Otherwise, we remove the padding only from the last scan line.
 */
#define cmd_max_short_width_bytes 6
#define cmd_max_short_width_bits (cmd_max_short_width_bytes * 8)
/*
 * Determine the (possibly unpadded) width in bytes for writing a bitmap,
 * per the algorithm just outlined.  If compression_mask has any of the
 * cmd_mask_compress_any bits set, we assume the bitmap will be compressed.
 * Return the total size of the bitmap.
 */
uint clist_bitmap_bytes(uint width_bits, uint height,
                        int compression_mask,
                        uint * width_bytes, uint * raster);

/*
 * For halftone cells, we always write an unreplicated bitmap, but we
 * reserve cache space for the reading pass based on the replicated size.
 * See the clist_change_tile procedure for the algorithm that chooses the
 * replication factors.
 */

/* ---------------- Block file entries ---------------- */

typedef struct cmd_block_s {
    int band_min, band_max;
#define cmd_band_end (-1)	/* end of band file */
    int64_t pos;		/* starting position in cfile */
} cmd_block;

/* ---------------- Band state ---------------- */

/* Remember the current state of one band when writing or reading. */
struct gx_clist_state_s {
    gx_color_index colors[2];	/* most recent colors */
    gx_device_color_saved sdc;  /* last device color for this band */
    uint tile_index;		/* most recent tile index */
    gx_bitmap_id tile_id;	/* most recent tile id */
/* Since tile table entries may be deleted and/or moved at any time, */
/* the following is the only reliable way to check whether tile_index */
/* references a particular tile id: */
#define cls_has_tile_id(cldev, pcls, tid, offset_temp)\
  ((pcls)->tile_id == (tid) &&\
   (offset_temp = cldev->tile_table[(pcls)->tile_index].offset) != 0 &&\
   ((tile_slot *)(cldev->data + offset_temp))->id == (tid))
    gs_id pattern_id;		/* the last stored pattern id. */
    gs_int_point tile_phase;	/* most recent tile phase */
    gs_int_point screen_phase[2];	/* most recent screen phase */
    gx_color_index tile_colors[2];	/* most recent tile colors */
    gx_device_color tile_color_devn[2];  /* devn tile colors */
    gx_cmd_rect rect;		/* most recent rectangle */
    gs_logical_operation_t lop;	/* most recent logical op */
    short lop_enabled;		/* 0 = don't use lop, 1 = use lop, */
                                /* -1 is used internally */
    short clip_enabled;		/* 0 = don't clip, 1 = do clip, */
                                /* -1 is used internally */
    bool color_is_alpha;	/* for copy_color_alpha */
    bool color_is_devn;     /* more overload of copy_color_alpha for devn support */
    uint known;			/* flags for whether this band */
                                /* knows various misc. parameters */
    /* We assign 'known' flags here from the high end; */
    /* gxclpath.h assigns them from the low end. */
#define tile_params_known (1<<15)
#define begin_image_known (1<<14) /* gxclimag.c */
#define initial_known 0x3fff	/* exclude tile & image params */
    /* Following are only used when writing */
    cmd_list list;		/* list of commands for band */
    /* Following are set when writing, read when reading */
    gx_color_usage_t color_usage;
};

/* The initial values for a band state */
/*static const gx_clist_state cls_initial */
#define cls_initial_values\
         { gx_no_color_index, gx_no_color_index },\
        { gx_dc_type_none },\
        0, gx_no_bitmap_id, gs_no_id,\
         { 0, 0 }, { {0, 0}, {0, 0}}, { gx_no_color_index, gx_no_color_index },\
        { {NULL}, {NULL} },\
         { 0, 0, 0, 0 }, lop_default, 0, 0, 0, 0, initial_known,\
        { 0, 0 }, /* cmd_list */\
        { 0, /* or */\
          0, /* slow rop */\
          { { max_int, max_int }, /* p */ { min_int, min_int } /* q */ } /* trans_bbox */\
        } /* color_usage */

/* Define the size of the command buffer used for reading. */
#define cbuf_size 4096
/* This is needed to split up operations with a large amount of data, */
/* primarily large copy_ operations. */
#define data_bits_size 4096

/* ---------------- Driver procedures ---------------- */

/* In gxclrect.c */
dev_proc_fillpage(clist_fillpage);
dev_proc_fill_rectangle(clist_fill_rectangle);
dev_proc_copy_mono(clist_copy_mono);
dev_proc_copy_color(clist_copy_color);
dev_proc_copy_alpha(clist_copy_alpha);
dev_proc_strip_tile_rectangle(clist_strip_tile_rectangle);
dev_proc_strip_tile_rect_devn(clist_strip_tile_rect_devn);
dev_proc_strip_copy_rop2(clist_strip_copy_rop2);
dev_proc_fill_trapezoid(clist_fill_trapezoid);
dev_proc_fill_linear_color_trapezoid(clist_fill_linear_color_trapezoid);
dev_proc_fill_linear_color_triangle(clist_fill_linear_color_triangle);
dev_proc_dev_spec_op(clist_dev_spec_op);
dev_proc_copy_planes(clist_copy_planes);
dev_proc_fill_rectangle_hl_color(clist_fill_rectangle_hl_color);
dev_proc_copy_alpha_hl_color(clist_copy_alpha_hl_color);
dev_proc_process_page(clist_process_page);

/* In gxclimag.c */
dev_proc_fill_mask(clist_fill_mask);
dev_proc_begin_typed_image(clist_begin_typed_image);
dev_proc_composite(clist_composite);

/* In gxclread.c */
dev_proc_get_bits_rectangle(clist_get_bits_rectangle);

/* ---------------- Driver procedure support ---------------- */

/*
 * The procedures and macros defined here are used when writing
 * (gxclist.c, gxclbits.c, gxclimag.c, gxclpath.c, gxclrect.c).
 * Note that none of the cmd_put_xxx procedures do VMerror recovery;
 * they convert low-memory warnings to VMerror errors.
 */

/* ------ Exported by gxclist.c ------ */

/* Write out device parameters. */
int cmd_put_params(gx_device_clist_writer *, gs_param_list *);

/* Conditionally keep command statistics. */
/* #define COLLECT_STATS_CLIST */

#ifdef COLLECT_STATS_CLIST
int cmd_count_op(int op, uint size, const gs_memory_t *mem);
int cmd_count_extended_op(int op, uint size, const gs_memory_t *mem);
void cmd_uncount_op(int op, uint size);
void cmd_print_stats(const gs_memory_t *);
#  define cmd_count_add1(v) (v++)
#else
#  define cmd_count_op(op, size, mem) (op)
#  define cmd_count_extended_op(op, size, mem) (op)
#  define cmd_uncount_op(op, size) DO_NOTHING
#  define cmd_count_add1(v) DO_NOTHING
#endif

/* Add a command to the appropriate band list, */
/* and allocate space for its data. */
byte *cmd_put_list_op(gx_device_clist_writer * cldev, cmd_list * pcl, uint size);

/* Add a extended op command to the appropriate band list, */
/* and allocate space for its data. */
byte *cmd_put_list_extended_op(gx_device_clist_writer * cldev, cmd_list * pcl, int op, uint size);

/* Request a space in the buffer.
   Writes out the buffer if necessary.
   Returns the size of available space. */
int cmd_get_buffer_space(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size);

#ifdef DEBUG
void clist_debug_op(gs_memory_t *mem, const unsigned char *op_ptr);
byte *cmd_put_op(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size);
#else
#define clist_debug_op(mem, op) do { } while (0)
#  define cmd_put_op(cldev, pcls, size)\
     cmd_put_list_op(cldev, &(pcls)->list, size)
#  define cmd_put_extended_op(cldev, pcls, op, size)\
     cmd_put_list_extended_op(cldev, &(pcls)->list, op, size)
#endif
/* Call cmd_put_op and update stats if no error occurs. */
static inline int
set_cmd_put_op(byte **dp, gx_device_clist_writer * cldev,
               gx_clist_state * pcls, int op, uint csize)
{
    *dp = cmd_put_op(cldev, pcls, csize);

    if (*dp == NULL)
        return (cldev)->error_code;
    **dp = cmd_count_op(op, csize, cldev->memory);

    if (gs_debug_c('L')) {
        clist_debug_op(cldev->memory, *dp);
        dmlprintf1(cldev->memory, "[%u]\n", csize);
    }

    return 0;
}
/* Call cmd_put_extended_op and update stats if no error occurs. */
static inline int
set_cmd_put_extended_op(byte **dp, gx_device_clist_writer * cldev,
                        gx_clist_state * pcls, int op, uint csize)
{
    *dp = cmd_put_op(cldev, pcls, csize);

    if (*dp == NULL)
        return (cldev)->error_code;
    **dp = cmd_opv_extend;
    (*dp)[1] = cmd_count_extended_op(op, csize, cldev->memory);

    if (gs_debug_c('L')) {
        clist_debug_op(cldev->memory, *dp);
        dmlprintf1(cldev->memory, "[%u]\n", csize);
    }

    return 0;
}

/* Add a command for all bands or a range of bands. */
byte *cmd_put_range_op(gx_device_clist_writer * cldev, int band_min,
                       int band_max, uint size);

/* Call cmd_put_all/range_op and update stats if no error occurs. */
static inline int
set_cmd_put_range_op(byte **dp, gx_device_clist_writer * cldev,
                     int op, int bmin, int bmax, uint csize)
{
    *dp = cmd_put_range_op(cldev, bmin, bmax, csize);
    if (*dp == NULL)
        return (cldev)->error_code;
    **dp = cmd_count_op(op, csize, (cldev)->memory);

    if (gs_debug_c('L')) {
        clist_debug_op(cldev->memory, *dp);
        dmlprintf1(cldev->memory, "[%u]\n", csize);
    }

    return 0;
}
#define set_cmd_put_all_op(dp, cldev, op, csize)\
  set_cmd_put_range_op(dp, cldev, op, 0, (cldev)->nbands - 1, csize)
static inline int
set_cmd_put_range_extended_op(byte **dp, gx_device_clist_writer * cldev,
                     int op, int bmin, int bmax, uint csize)
{
    *dp = cmd_put_range_op(cldev, bmin, bmax, csize);
    if (*dp == NULL)
        return (cldev)->error_code;
    **dp = cmd_opv_extend;
    (*dp)[1] = cmd_count_extended_op(op, csize, (cldev)->memory);

    if (gs_debug_c('L')) {
        clist_debug_op(cldev->memory, *dp);
        dmlprintf1(cldev->memory, "[%u]\n", csize);
    }

    return 0;
}
#define set_cmd_put_all_extended_op(dp, cldev, op, csize)\
  set_cmd_put_range_extended_op(dp, cldev, op, 0, (cldev)->nbands - 1, csize)

/* Shorten the last allocated command. */
/* Note that this does not adjust the statistics. */
#define cmd_shorten_list_op(cldev, pcls, delta)\
  ((pcls)->tail->size -= (delta), (cldev)->cnext -= (delta))
#define cmd_shorten_op(cldev, pcls, delta)\
  cmd_shorten_list_op(cldev, &(pcls)->list, delta)

/* Write out the buffered commands, and reset the buffer. */
/* Return 0 if OK, 1 if OK with low-memory warning, */
/* or the usual negative error code. */
int cmd_write_buffer(gx_device_clist_writer * cldev, byte cmd_end);

/* End a page by flushing the buffer and terminating the command list. */
int clist_end_page(gx_device_clist_writer *);

/* Compute the # of bytes required to represent a variable-size integer. */
/* (This works for negative integers also; they are written as though */
/* they were unsigned.) */
int cmd_size_w(uint);

#define w1byte(w) (!((w) & ~0x7f))
#define w2byte(w) (!((w) & ~0x3fff))
#define cmd_sizew(w)\
  (w1byte(w) ? 1 : w2byte(w) ? 2 : cmd_size_w((uint)(w)))
#define cmd_size2w(wx,wy)\
  (w1byte((wx) | (wy)) ? 2 :\
   cmd_size_w((uint)(wx)) + cmd_size_w((uint)(wy)))
#define cmd_sizexy(xy) cmd_size2w((xy).x, (xy).y)
#define cmd_sizew_max ((sizeof(uint) * 8 + 6) / 7)

/* Put a variable-size integer in the buffer. */
byte *cmd_put_w(uint, byte *);

#define cmd_putw(w,dp)\
  (w1byte(w) ? (**dp = w, ++(*dp)) :\
   w2byte(w) ? (**dp = (w) | 0x80, (*dp)[1] = (w) >> 7, (*dp) += 2) :\
   (*dp = cmd_put_w((uint)(w), *dp)))
#define cmd_put2w(wx,wy,dp)\
  (w1byte((wx) | (wy)) ? ((*dp)[0] = (wx), (*dp)[1] = (wy), (*dp) += 2) :\
   (*dp = cmd_put_w((uint)(wy), cmd_put_w((uint)(wx), *dp))))
#define cmd_putxy(xy,dp) cmd_put2w((xy).x, (xy).y, dp)

int cmd_size_frac31(register frac31 w);
byte * cmd_put_frac31(register frac31 w, register byte * dp);

/* Put out a command to set a color. */
typedef struct {
    byte set_op;
    byte delta_op;
    bool tile_color;
    bool devn;
} clist_select_color_t;

extern const clist_select_color_t
      clist_select_color0, clist_select_color1, clist_select_tile_color0,
      clist_select_tile_color1, clist_select_devn_color0,
      clist_select_devn_color1;

/* See comments in gxclutil.c */
int cmd_put_color(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                  const clist_select_color_t * select,
                  gx_color_index color, gx_color_index * pcolor);

extern const gx_color_index cmd_delta_offsets[];	/* In gxclutil.c */

#define cmd_set_color0(dev, pcls, color0)\
  cmd_put_color(dev, pcls, &clist_select_color0, color0, &(pcls)->colors[0])
#define cmd_set_color1(dev, pcls, color1)\
  cmd_put_color(dev, pcls, &clist_select_color1, color1, &(pcls)->colors[1])

/* Put out a command to set the tile colors. */
int cmd_set_tile_colors(gx_device_clist_writer *cldev, gx_clist_state * pcls,
                        gx_color_index color0, gx_color_index color1);

/* Put out a command to set the tile phase. */
int
cmd_set_tile_phase_generic(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                   int px, int py, bool all_bands);
int cmd_set_tile_phase(gx_device_clist_writer *cldev, gx_clist_state * pcls,
                       int px, int py);
/* Put out a command to set the screen phase. */
int
cmd_set_screen_phase_generic(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                             int px, int py, gs_color_select_t color_select, bool all_bands);
int
cmd_set_screen_phase(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                     int px, int py, gs_color_select_t color_select);

/* Enable or disable the logical operation. */
int cmd_put_enable_lop(gx_device_clist_writer *, gx_clist_state *, int);
#define cmd_do_enable_lop(cldev, pcls, enable)\
  ( (pcls)->lop_enabled == ((enable) ^ 1) &&\
    cmd_put_enable_lop(cldev, pcls, enable) < 0 ?\
      (cldev)->error_code : 0 )
#define cmd_enable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 1)
#define cmd_disable_lop(cldev, pcls)\
  cmd_do_enable_lop(cldev, pcls, 0)

/* Enable or disable clipping. */
int cmd_put_enable_clip(gx_device_clist_writer *, gx_clist_state *, int);

#define cmd_do_enable_clip(cldev, pcls, enable)\
  ( (pcls)->clip_enabled == ((enable) ^ 1) &&\
    cmd_put_enable_clip(cldev, pcls, enable) < 0 ?\
      (cldev)->error_code : 0 )
#define cmd_enable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 1)
#define cmd_disable_clip(cldev, pcls)\
  cmd_do_enable_clip(cldev, pcls, 0)

/* Write a command to set the logical operation. */
int cmd_set_lop(gx_device_clist_writer *, gx_clist_state *,
                gs_logical_operation_t);

/* Disable (if default) or enable the logical operation, setting it if */
/* needed. */
int cmd_update_lop(gx_device_clist_writer *, gx_clist_state *,
                   gs_logical_operation_t);

/*
 * For dividing up an operation into bands, use the control pattern :
 *
 *   cmd_rects_enum_t re;
 *   RECT_ENUM_INIT(re, ry, rheight);
 *   do {
 *	RECT_STEP_INIT(re);
 *	... process rectangle x, y, width, height in band pcls ...
 *
 *	........
 *   } while ((re.y += re.height) < re.yend);
 *
 * Note that RECT_STEP_INIT(re) sets re.height.  It is OK for the code that
 * processes each band to reset height to a smaller (positive) value; the
 * vertical subdivision code in copy_mono, copy_color, and copy_alpha makes
 * use of this.  The band processing code may `continue' (to reduce nesting
 * of conditionals).
 *
 * If a put_params call fails, the device will be left in a closed state,
 * but higher-level code won't notice this fact.  We flag this by setting
 * permanent_error, which prevents writing to the command list.
 */

typedef struct cmd_rects_enum_s {
        int y;
        int height;
        int yend;
        int band_height;
        int band;
        gx_clist_state *pcls;
        int band_end;
        int rect_nbands;
} cmd_rects_enum_t;

#define RECT_ENUM_INIT(re, yvar, heightvar)\
        re.y = yvar;\
        re.height = heightvar;\
        re.yend = re.y + re.height;\
        re.band_height = cdev->page_info.band_params.BandHeight;\
        re.rect_nbands = (re.yend - re.y + re.band_height - 1) / re.band_height;

#define RECT_STEP_INIT(re)\
            re.band = re.y / re.band_height;\
            re.pcls = cdev->states + re.band;\
            re.band_end = (re.band + 1) * re.band_height;\
            re.height = min(re.band_end, re.yend) - re.y;

/* Read a transformation matrix. */
const byte *cmd_read_matrix(gs_matrix * pmat, const byte * cbp);

/* ------ Exported by gxclrect.c ------ */

/* Put out a fill or tile rectangle command. */
int cmd_write_rect_cmd(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                       int op, int x, int y, int width, int height);
/* Put out a fill with a devn color */
int cmd_write_rect_hl_cmd(gx_device_clist_writer * cldev,
                             gx_clist_state * pcls, int op, int x, int y,
                             int width, int height, bool extended_command);
/* Put out a fill or tile rectangle command for fillpage. */
int cmd_write_page_rect_cmd(gx_device_clist_writer * cldev, int op);

/* ------ Exported by gxclbits.c ------ */

/*
 * Put a bitmap in the buffer, compressing if appropriate.
 * pcls == 0 means put the bitmap in all bands.
 * Return <0 if error, otherwise the compression method.
 * A return value of gs_error_limitcheck means that the bitmap was too big
 * to fit in the command reading buffer.
 * Note that this leaves room for the command and initial arguments,
 * but doesn't fill them in.
 *
 * If decompress_elsewhere is set in the compression_mask, it is OK
 * to write out a compressed bitmap whose decompressed size is too large
 * to fit in the command reading buffer.  (This is OK when reading a
 * cached bitmap, but not a bitmap for a one-time copy operation.)
 */
#define decompress_elsewhere 0x100
/*
 * If decompress_spread is set, the decompressed data will be spread out
 * for replication, so we drop all the padding even if the width is
 * greater than cmd_max_short_width_bytes (see above).
 */
#define decompress_spread 0x200

/* clist_copy_mono and clist_copy_color have a max_size, but tiles to the */
/* cache do not (clist_change_bits and clist_change_tile).		  */
#define allow_large_bitmap 0x400

int cmd_put_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                 const byte * data, uint width_bits, uint height,
                 uint raster, int op_size, int compression_mask,
                 byte ** pdp, uint * psize);

/*
 * Put out commands for a color map (transfer function, black generation, or
 * undercolor removal).  If pid != 0, write the map only if its ID differs
 * from the current one, and update the saved ID in the case.
 */
typedef enum {
    cmd_map_transfer = 0,	/* all transfer functions */
    cmd_map_transfer_0,		/* transfer[0] */
    cmd_map_transfer_1,		/* transfer[1] */
    cmd_map_transfer_2,		/* transfer[2] */
    cmd_map_transfer_3,		/* transfer[3] */
    cmd_map_black_generation,
    cmd_map_undercolor_removal
} cmd_map_index;
typedef enum {
    cmd_map_none = 0,		/* no map, use default */
    cmd_map_identity,		/* identity map */
    cmd_map_other		/* other map */
} cmd_map_contents;
int cmd_put_color_map(gx_device_clist_writer * cldev,
                      cmd_map_index map_index, int comp_num,
                      const gx_transfer_map * map, gs_id * pid);

/*
 * Change tiles for clist_tile_rectangle.  (We make this a separate
 * procedure primarily for readability.)
 */
int clist_change_tile(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                      const gx_strip_bitmap * tiles, int depth);

/*
 * Change "tile" for clist_copy_*.  Only uses tiles->{data, id, raster,
 * rep_width, rep_height}.  tiles->[rep_]shift must be zero.
 */
int clist_change_bits(gx_device_clist_writer * cldev, gx_clist_state * pcls,
                      const gx_strip_bitmap * tiles, int depth);

/* ------ Exported by gxclimag.c ------ */

/*
 * Write out any necessary color mapping data.
 */
int cmd_put_color_mapping(gx_device_clist_writer * cldev,
                                  const gs_gstate * pgs);
/*
 * Add commands to represent a full (device) halftone.
 * (This routine should probably be in some other module.)
 */
int cmd_put_halftone(gx_device_clist_writer * cldev,
                     const gx_device_halftone * pdht);

/* ------ Exported by gxclrast.c for gxclread.c ------ */

/*
 * Define whether we are actually rendering a band, or just executing
 * the put_params that occurs at the beginning of each page.
 */
typedef enum {
    playback_action_render,
    playback_action_render_no_pdf14,
    playback_action_setup
} clist_playback_action;

/* Play back and rasterize one band. */
int clist_playback_band(clist_playback_action action,
                        gx_device_clist_reader *cdev,
                        stream *s, gx_device *target,
                        int x0, int y0, gs_memory_t *mem);

/* Playback the band file, taking the indicated action w/ its contents. */
int clist_playback_file_bands(clist_playback_action action,
                          gx_device_clist_reader *crdev,
                          gx_band_page_info_t *page_info, gx_device *target,
                          int band_first, int band_last, int x0, int y0);
#ifdef DEBUG
int64_t clist_file_offset(const stream_state *st, uint buffer_offset);
void top_up_offset_map(stream_state * st, const byte *buf, const byte *ptr, const byte *end);
void offset_map_next_data_out_of_band(stream_state *st);
void clist_debug_op(gs_memory_t *mem, const unsigned char *op_ptr);
void adjust_offset_map_for_skipped_data(stream_state *st, uint buffer_offset, uint skipped);
#endif

int clist_writer_push_no_cropping(gx_device_clist_writer *cdev);
int clist_writer_push_cropping(gx_device_clist_writer *cdev, int ry, int rheight);
int clist_writer_pop_cropping(gx_device_clist_writer *cdev);
int clist_writer_check_empty_cropping_stack(gx_device_clist_writer *cdev);
int clist_read_icctable(gx_device_clist_reader *crdev);
int clist_read_color_usage_array(gx_device_clist_reader *crdev);
int clist_read_op_equiv_cmyk_colors(gx_device_clist_reader *crdev,
    equivalent_cmyk_color_params *op_equiv);

/* Special write out for the serialized icc profile table */

int cmd_write_icctable(gx_device_clist_writer * cldev, unsigned char *pbuf, int data_size);

/* Enumeration of psuedo band offsets for extra c-list data.
   This includes the ICC profile table and and color_usage and
   may later include compressed image data */

typedef enum {
    COLOR_USAGE_OFFSET = 1,
    SPOT_EQUIV_COLORS = 2,
    ICC_TABLE_OFFSET = 3

} psuedoband_offset;

/* Enumeration of compositor actions for band cropping. */

typedef enum {
    ALLBANDS = 0,
    PUSHCROP,
    POPCROP,
    CURRBANDS,
    SAMEAS_PUSHCROP_BUTNOPUSH
} cl_crop_action;

/* Valgrind helper macro */
#ifdef HAVE_VALGRIND
#define CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev) \
do {\
    gx_device_clist_writer *CLDEV = (gx_device_clist_writer *)(cldev);\
    if (CLDEV->ccl != NULL)\
        VALGRIND_CHECK_MEM_IS_DEFINED(&CLDEV->ccl->tail[1], CLDEV->ccl->tail->size);\
} while (0 == 1)
#else
#define CMD_CHECK_LAST_OP_BLOCK_DEFINED(cldev) do { } while (0)
#endif

#endif /* gxcldev_INCLUDED */
