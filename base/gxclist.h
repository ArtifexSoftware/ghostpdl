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


/* Command list definitions for Ghostscript. */
/* Requires gxdevice.h and gxdevmem.h */

#ifndef gxclist_INCLUDED
#  define gxclist_INCLUDED

#include "gscspace.h"
#include "gxband.h"
#include "gxbcache.h"
#include "gxclio.h"
#include "gxcolor2.h"		/* for pattern1_instance */
#include "gxdevbuf.h"
#include "gxgstate.h"
#include "gxrplane.h"
#include "gscms.h"
#include "gxcomp.h"

/*
 * A command list is essentially a compressed list of driver calls.
 * Command lists are used to record an image that must be rendered in bands
 * for high-resolution and/or limited-memory printers.
 *
 * Command lists work in two phases.  The first phase records driver calls,
 * sorting them according to the band(s) they affect.  The second phase
 * reads back the commands band-by-band to create the bitmap images.
 * When opened, a command list is in the recording state; it switches
 * automatically from recording to reading when its get_bits procedure
 * is called.  Currently, there is a hack to reopen the device after
 * each page is processed, in order to switch back to recording.
 */

/*
 * The command list contains both commands for particular bands (the vast
 * majority) and commands that apply to a range of bands.  In order to
 * synchronize the two, we maintain the following invariant for buffered
 * commands:
 *
 *      If there are any band-range commands in the buffer, they are the
 *      first commands in the buffer, before any specific-band commands.
 *
 * To maintain this invariant, whenever we are about to put an band-range
 * command in the buffer, we check to see if the buffer already has any
 * band-range commands in it, and if so, whether they are the last commands
 * in the buffer and are for the same range; if the answer to any of these
 * questions is negative, we flush the buffer.
 */

/* ---------------- Public structures ---------------- */

/*
 * Define a saved page object. This consists of information about the
 * page's clist and parameters and the num_copies parameter of
 * output_page. The current device's name, and color_info is saved to
 * allow gdev_prn_render_pages to make sure that the target device is
 * compatible.
 */
typedef struct gx_saved_page_s {
    char dname[32];		/* device name for checking */
    gx_device_color_info color_info;	/* also for checking */
    gs_graphics_type_tag_t	tag;
    /* Elements from gx_band_page_info that we need */
    char cfname[gp_file_name_sizeof];	/* command file name */
    char bfname[gp_file_name_sizeof];	/* block file name */
    const clist_io_procs_t *io_procs;
    uint tile_cache_size;	/* size of tile cache */
    size_t line_ptrs_offset;
    int num_planar_planes;
    int64_t bfile_end_pos;		/* ftell at end of bfile */
    gx_band_params_t band_params;  /* parameters used when writing band list */
                                /* (actual values, no 0s) */
    gs_memory_t *mem;		/* allocator for paramlist */
    /* device params are expected to include info to set the icc_struct and the devn_params */
    /* as well as color_info (if it is able to change as the 'bit' devices can).            */
    int paramlist_len;
    byte *paramlist;		/* serialized device param list */
    /* for DeviceN devices, we need the spot colors collected during parsing */
    int num_separations;
    int separation_name_sizes[GX_DEVICE_MAX_SEPARATIONS];
    byte *separation_names[GX_DEVICE_MAX_SEPARATIONS];	/* AKA Spot Color names */
} gx_saved_page;

/*
 * Define a saved page placed at a particular (X,Y) offset for rendering.
 */
typedef struct gx_placed_page_s {
    gx_saved_page *page;
    gs_int_point offset;
} gx_placed_page;

/* ---------------- Internal structures ---------------- */

/*
 * Currently, halftoning occurs during the first phase, producing calls
 * to tile_rectangle.  Both phases keep a cache of recently seen bitmaps
 * (halftone cells and characters), which allows writing only a short cache
 * index in the command list rather than the entire bitmap.
 *
 * We keep only a single cache for all bands, but since the second phase
 * reads the command lists for each band separately, we have to keep track
 * for each cache entry E and band B whether the definition of E has been
 * written into B's list.  We do this with a bit mask in each entry.
 *
 * Eventually, we will replace this entire arrangement with one in which
 * we pass the actual halftone screen (whitening order) to all bands
 * through the command list, and halftoning occurs on the second phase.
 * This not only will shrink the command list, but will allow us to apply
 * other rendering algorithms such as error diffusion in the second phase.
 */
typedef struct {
    ulong offset;		/* writing: offset from cdev->data, */
    /*   0 means unused */
    /* reading: offset from cdev->chunk.data */
} tile_hash;

typedef struct {
    gx_cached_bits_common;
    /* To save space, instead of storing rep_width and rep_height, */
    /* we store width / rep_width and height / rep_height. */
    byte x_reps, y_reps;
    ushort rep_shift;
    ushort index;		/* index in table (hash table when writing) */
    ushort num_bands;		/* # of 1-bits in the band mask */
    byte num_planes;
    /* byte band_mask[]; */
#define ts_mask(pts) (byte *)((pts) + 1)
    /* byte bits[]; */
#define ts_bits(cldev,pts) (ts_mask(pts) + (cldev)->tile_band_mask_size)
} tile_slot;

/* Define the prefix on each command run in the writing buffer. */
typedef struct cmd_prefix_s cmd_prefix;
struct cmd_prefix_s {
    cmd_prefix *next;
    uint size;
    ulong id; /* Debug purpose only. */
};

/* Define the pointers for managing a list of command runs in the buffer. */
/* There is one of these for each band, plus one for band-range commands. */
typedef struct cmd_list_s {
    cmd_prefix *head, *tail;	/* list of commands for band */
} cmd_list;

/*
 * In order to keep the per-band state down to a reasonable size,
 * we store only a single set of the gs_gstate parameters;
 * for each parameter, each band has a flag that says whether that band
 * 'knows' the current value of the parameters.
 */
#define GS_STATE_INIT_VALUES_CLIST(s) GS_STATE_INIT_VALUES(s, 300.0 / 72.0)

/*
 * Define the main structure for holding command list state.
 * Unless otherwise noted, all elements are used in both the writing (first)
 * and reading (second) phase.
 */
typedef struct gx_clist_state_s gx_clist_state;

#define gx_device_clist_common_members\
        gx_device_forward_common;	/* (see gxdevice.h) */\
                /* Following must be set before writing or reading. */\
                /* See gx_device_clist_writer, below, for more that must be init'd */\
        /* gx_device *target; */	/* device for which commands */\
                                        /* are being buffered */\
        gx_device_buf_procs_t buf_procs;\
        gs_memory_t *bandlist_memory;	/* allocator for in-memory bandlist files */\
        byte *data;			/* buffer area */\
        uint data_size;			/* size of buffer */\
        gx_band_params_t band_params;	/* band buffering parameters */\
        bool do_not_open_or_close_bandfiles;	/* if true, do not open/close bandfiles */\
        dev_proc_dev_spec_op((*orig_spec_op)); /* Original dev spec op handler */\
                /* Following are used for both writing and reading. */\
        gx_bits_cache_chunk *cache_chunk;	/* the only chunk of bits */\
        gx_bits_cache bits;\
        uint tile_hash_mask;		/* size of tile hash table -1 */\
        uint tile_band_mask_size;	/* size of band mask preceding */\
                                        /* each tile in the cache */\
        tile_hash *tile_table;		/* table for tile cache: */\
                                        /* see tile_hash above */\
                                        /* (a hash table when writing) */\
        int ymin, ymax;			/* current band, <0 when writing */\
                /* Following are set when writing, read when reading. */\
        gx_band_page_info_t page_info;	/* page information */\
        int nbands;			/* # of bands */\
        int64_t trans_dev_icc_hash;     /* A special hash code for des color */\
        clist_icctable_t *icc_table;    /* Table that keeps track of ICC profiles.\
                                           It relates the hashcode to the cfile\
                                           file location. */\
        gsicc_link_cache_t *icc_cache_cl; /* Link cache */\
        int icc_cache_list_len;         /* Length of list of caches, one per rendering thread */\
        gsicc_link_cache_t **icc_cache_list  /* Link cache list */

/* Define a structure to hold where the ICC profiles are stored in the clist
   Profiles are added into psuedo bands of the clist, these are bands that exist beyond
   the edge of the normal band list.  A profile will occupy its own band.  The structure
   here is a table that relates the hash code of the ICC profile to the pseudoband.
   This table will be added at the end of the clist writing process.  */

/* Used when writing out the table at the end of the clist writing.
   Table is written to maxband + 1 */

typedef struct clist_icc_serial_entry_s clist_icc_serial_entry_t;

struct clist_icc_serial_entry_s {
    int64_t hashcode;              /* A hash code for the icc profile */
    int64_t file_position;        /* File position in cfile of the profile with header */
    int size;
};

typedef struct clist_icctable_entry_s clist_icctable_entry_t;

struct clist_icctable_entry_s {
    clist_icc_serial_entry_t serial_data;
    clist_icctable_entry_t *next;  /* The next entry in the table */
    cmm_profile_t *icc_profile;    /* The profile.  In non-gc memory. This is
                                      written out at the end of the writer phase */
    bool render_is_valid;          /* Since the profile is written out at the
                                      end and is shared, we can't guarantee that
                                      this entry will stay the same through
                                      the profile's life.  Hence we store it here
                                      and set it just when we do the writing */
};

#define private_st_clist_icctable_entry()\
  gs_private_st_ptrs1(st_clist_icctable_entry,\
                clist_icctable_entry_t, "clist_icctable_entry",\
                clist_icctable_entry_enum_ptrs, clist_icctable_entry_reloc_ptrs, next)

typedef struct clist_icctable_s clist_icctable_t;

struct clist_icctable_s {
    int tablesize;
    gs_memory_t *memory;
    clist_icctable_entry_t *head;
    clist_icctable_entry_t *final;
};

void
clist_icc_table_finalize(const gs_memory_t *memory, void * vptr);

#define private_st_clist_icctable()\
  gs_private_st_ptrs2_final(st_clist_icctable,\
                clist_icctable_t, "clist_icctable",\
                clist_icctable_enum_ptrs, clist_icctable_reloc_ptrs, clist_icc_table_finalize, head, final)

typedef struct gx_device_clist_common_s {
    gx_device_clist_common_members;
} gx_device_clist_common;

#define clist_band_height(cldev) ((cldev)->page_info.band_params.BandHeight)
#define clist_cfname(cldev) ((cldev)->page_info.cfname)
#define clist_cfile(cldev) ((cldev)->page_info.cfile)
#define clist_bfname(cldev) ((cldev)->page_info.bfname)
#define clist_bfile(cldev) ((cldev)->page_info.bfile)

/* Define the length of the longest dash pattern we are willing to store. */
/* (Strokes with longer patterns are converted to fills.) */
#define cmd_max_dash 11

/* Define a clist cropping buffer,
   which represents a cropping stack element while clist writing. */
typedef struct clist_writer_cropping_buffer_s clist_writer_cropping_buffer_t;

struct clist_writer_cropping_buffer_s {
    int cropping_min, cropping_max;
    uint mask_id, temp_mask_id;
    clist_writer_cropping_buffer_t *next;
};

#define private_st_clist_writer_cropping_buffer()\
  gs_private_st_ptrs1(st_clist_writer_cropping_buffer,\
                clist_writer_cropping_buffer_t, "clist_writer_transparency_buffer",\
                clist_writer_cropping_buffer_enum_ptrs, clist_writer_cropping_buffer_reloc_ptrs, next)

/* Define the state of a band list when writing. */
typedef struct clist_icc_color_s {
    int64_t icc_hash;           /* hash code for icc profile */
    byte icc_num_components;   /* needed to avoid having to read icc data early */
    bool is_lab;               /* also needed early */
    gsicc_profile_t default_match;     /* used by gsicc_get_link early for usefastcolor mode */
    gsicc_colorbuffer_t data_cs;
} clist_icc_color_t;

typedef struct clist_color_space_s {
    byte byte1;			/* see cmd_opv_set_color_space in gxclpath.h */
    gs_id id;			/* space->id for comparisons */
    clist_icc_color_t icc_info; /* Data needed to enable delayed icc reading */
    const gs_color_space *space;
} clist_color_space_t;

struct gx_device_clist_writer_s {
    gx_device_clist_common_members;	/* (must be first) */
    int error_code;		/* error returned by cmd_put_op */
    gx_clist_state *states;	/* current state of each band */
    byte *cbuf;			/* start of command buffer */
    byte *cnext;		/* next slot in command buffer */
    byte *cend;			/* end of command buffer */
    cmd_list *ccl;		/* &clist_state.list of last command */
    cmd_list *band_range_list;	/* list of band-range commands */
    int band_range_min, band_range_max;		/* range for list */
    uint tile_max_size;		/* max size of a single tile (bytes) */
    uint tile_max_count;	/* max # of hash table entries */
    gx_strip_bitmap tile_params;	/* current tile parameters */
    int tile_depth;		/* current tile depth */
    int tile_known_min, tile_known_max;  /* range of bands that knows the */
                                /* current tile parameters */
    /*
     * NOTE: we must not set the line_params.dash.pattern member of the
     * gs_gstate to point to the dash_pattern member of the writer
     * state (except transiently), since this would confuse the
     * garbage collector.
     */
    gs_gstate gs_gstate;	        /* current values of gs_gstate params */
    bool pdf14_needed;		/* if true then not page level opaque mode */
                                /* above set when not at page level with no SMask or when */
                                /* the page level BM, shape or opacity alpha needs tranaparency */
    int pdf14_trans_group_level;/* 0 when at page level group -- push increments, pop decrements */
                                /* -1 when PUSH_DEVICE not yet performed to prevent spurious ops */
    int pdf14_smask_level;	/* 0 when at SMask None -- push increments, pop decrements */
    bool page_pdf14_needed;	/* save page level pdf14_needed state */

    float dash_pattern[cmd_max_dash];	/* current dash pattern */
    const gx_clip_path *clip_path;	/* current clip path, */
                                /* only non-transient for images */
    gs_id clip_path_id;		/* id of current clip path */
    clist_color_space_t color_space; /* only used for non-mask images */
    gs_id transfer_ids[4];	/* ids of transfer maps */
    gs_id black_generation_id;	/* id of black generation map */
    gs_id undercolor_removal_id;	/* id of u.c.r. map */
    gs_id device_halftone_id;	/* id of device halftone */
    gs_id image_enum_id;	/* non-0 if we are inside an image */
                                /* that we are passing through */
    int permanent_error;		/* if < 0, error only cleared by clist_reset() */
    int ignore_lo_mem_warnings;	/* ignore warnings from clist file/mem */
            /* Following must be set before writing */
    int disable_mask;		/* mask of routines to disable clist_disable_xxx */
    gs_pattern1_instance_t *pinst; /* Used when it is a pattern clist. */
    int cropping_min, cropping_max;
    int save_cropping_min, save_cropping_max;
    bool cropping_saved; /* for clist_fill_path only. */
    int cropping_level;
    clist_writer_cropping_buffer_t *cropping_stack;
    ulong ins_count;
    uint mask_id_count;
    uint mask_id;
    uint temp_mask_id; /* Mask id of a mask of an image with SMask. */
    bool is_fillpage;
    gx_device_color_info clist_color_info; /* color information to be used during clist writing.
                                           It may be different than the target device if we
                                           are in a transparency group.  Since the fill rect
                                           commands use the forward procs and we have no
                                           access to the graphic state information in those
                                           routines, this is the logical place to put this
                                           information */
    bool op_fill_active;   /* Needed so we know state during clist writing */
    bool op_stroke_active; /* Needed so we know state during clist writing  */

};

/* Bits for gx_device_clist_writer.disable_mask. Bit set disables behavior */
#define clist_disable_fill_path	(1 << 0)
#define clist_disable_stroke_path (1 << 1)
#define clist_disable_hl_image (1 << 2)
#define clist_disable_complex_clip (1 << 3)
#define clist_disable_nonrect_hl_image (1 << 4)
#define clist_disable_pass_thru_params (1 << 5)	/* disable EXCEPT at top of page */
#define clist_disable_copy_alpha (1 << 6) /* target does not support copy_alpha */

typedef struct clist_render_thread_control_s clist_render_thread_control_t;

/* Define the state of a band list when reading. */
/* For normal rasterizing, pages and num_pages are both 0. */
typedef struct gx_device_clist_reader_s {
    gx_device_clist_common_members;	/* (must be first) */
    gx_render_plane_t yplane;		/* current plane, index = -1 */
                                        /* means all planes */
    const gx_placed_page *pages;
    gx_color_usage_t *color_usage_array; /* per band color_usage */
    int num_pages;
    void *offset_map; /* Just against collecting the map as garbage. */
    int num_render_threads;		/* number of threads being used */
    clist_render_thread_control_t *render_threads;	/* array of threads */
    byte *main_thread_data;		/* saved data pointer of main thread */
    int curr_render_thread;		/* index into array */
    int thread_lookahead_direction;	/* +1 or -1 */
    int next_band;			/* may be < 0 or >= num bands when no more remain to render */
    struct gx_device_clist_reader_s *orig_clist_device;
                                        /* This is NULL, unless we're in a worker thread for clist
                                         * rendering, in which case it's a pointer back to the
                                         * original clist device. */
} gx_device_clist_reader;

union gx_device_clist_s {
    gx_device_clist_common common;
    gx_device_clist_reader reader;
    gx_device_clist_writer writer;
};

extern_st(st_device_clist);
#define public_st_device_clist()	/* in gxclist.c */\
  gs_public_st_complex_only(st_device_clist, gx_device_clist,\
    "gx_device_clist", 0, device_clist_enum_ptrs, device_clist_reloc_ptrs,\
    gx_device_finalize)
#define st_device_clist_max_ptrs\
  (st_device_forward_max_ptrs + st_gs_gstate_num_ptrs + 4)

#define CLIST_IS_WRITER(cdev) ((cdev)->common.ymin < 0)

/* setup before opening clist device */
#define clist_init_params(xclist, xdata, xdata_size, xtarget, xbuf_procs, xband_params, xexternal, xmemory, xdisable, pageusestransparency, pageusesoverprint)\
    BEGIN\
        (xclist)->common.data = (xdata);\
        (xclist)->common.data_size = (xdata_size);\
        (xclist)->common.target = (xtarget);\
        (xclist)->common.buf_procs = (xbuf_procs);\
        (xclist)->common.band_params = (xband_params);\
        (xclist)->common.do_not_open_or_close_bandfiles = (xexternal);\
        (xclist)->common.bandlist_memory = (xmemory);\
        (xclist)->writer.disable_mask = (xdisable);\
        (xclist)->writer.page_uses_transparency = (pageusestransparency);\
        (xclist)->writer.page_uses_overprint = (pageusesoverprint);\
        (xclist)->writer.pinst = NULL;\
    END

void clist_initialize_device_procs(gx_device *dev);

void clist_init_io_procs(gx_device_clist *pclist_dev, bool in_memory);

/* Reset (or prepare to append to) the command list after printing a page. */
int clist_finish_page(gx_device * dev, bool flush);

/* Close the band files and delete their contents. */
int clist_close_output_file(gx_device *dev);

/*
 * Close and delete the contents of the band files associated with a
 * page_info structure (a page that has been separated from the device).
 */
int clist_close_page_info(gx_band_page_info_t *ppi);

/* Define the abstract type for a printer device. */
typedef struct gx_device_printer_s gx_device_printer;

/* Do device setup from params passed in the command list. */
int clist_setup_params(gx_device *dev);

/*
 * Render a rectangle to a client-supplied image.  This implements
 * gdev_prn_render_rectangle for devices that are using banding.
 *
 * Note that clist_render_rectangle only guarantees to render *at least* the
 * requested rectangle to bdev, offset by (-prect->p.x, -prect->p.y):
 * anything it does to avoid rendering regions outside the rectangle is
 * merely an optimization.  If the client really wants the output clipped to
 * some rectangle smaller than ((0, 0), (bdev->width, bdev->height)), it
 * must set up a clipping device.
 */
int clist_render_rectangle(gx_device_clist *cdev,
                           const gs_int_rect *prect, gx_device *bdev,
                           const gx_render_plane_t *render_plane,
                           bool clear);

/* Optimization of PDF 1.4 transparency requires a trans_bbox for each band */
/* This function updates the clist writer states with the bbox provided. */
void clist_update_trans_bbox(gx_device_clist_writer *dev, gs_int_rect *bbox);

/* Make a clist device for accumulating. Used for pattern-clist as well as */
/* for pdf14 pages that are too large to be done in page mode.             */
gx_device_clist *
clist_make_accum_device(gs_memory_t *mem, gx_device *target, const char *dname, void *base, int space,
                        gx_device_buf_procs_t *buf_procs, gx_band_params_t *band_params,
                        bool use_memory_clist, bool uses_transparency,
                        gs_pattern1_instance_t *pinst);

/* Retrieve total size for cfile and bfile. */
int clist_data_size(const gx_device_clist *cdev, int select);
/* Get command list data. */
int clist_get_data(const gx_device_clist *cdev, int select, int64_t offset, byte *buf, int length);
/* Put command list data. */
int clist_put_data(const gx_device_clist *cdev, int select, int64_t offset, const byte *buf, int length);

/* Write out the array of color usage entries (one per band) */
int clist_write_color_usage_array(gx_device_clist_writer *cldev);

/* Write out simulated overprint CMYK equiv. values for spot colors */
int clist_write_op_equiv_cmyk_colors(gx_device_clist_writer *cldev,
    equivalent_cmyk_color_params *op_equiv_cmyk);

/* get the color_usage summary over a Y range from the clist writer states */
/* Not expected to be used */
int clist_writer_color_usage(gx_device_clist_writer *cldev, int y, int height,
                     gx_color_usage_t *color_usage, int *range_start);

/* ICC table prototypes */

/* Write out the table of profile entries */
int clist_icc_writetable(gx_device_clist_writer *cldev);

/* Write out the profile to the clist */
int64_t clist_icc_addprofile(gx_device_clist_writer *cdev, cmm_profile_t *iccprofile, int *iccsize);

/* Seach the table to see if we already have a profile in the cfile */
bool clist_icc_searchtable(gx_device_clist_writer *cdev, int64_t hashcode);

/* Add another entry into the icc profile table */
int clist_icc_addentry(gx_device_clist_writer *cdev, int64_t hashcode,
                       cmm_profile_t *icc_profile);

/* Free the table and its entries */
int clist_free_icc_table(clist_icctable_t *icc_table, gs_memory_t *memory);

/* Generic read function used with ICC and could be used with others.
   A different of this and clist_get_data is that here we reset the
   cfile position when we are done and this only reads from the cfile
   not the bfile or cfile */

int clist_read_chunk(gx_device_clist_reader *crdev, int64_t position, int size, unsigned char *buf);

/* Exports from gxclread used by the multi-threading logic */

/* Initialize for reading. */
int clist_render_init(gx_device_clist *dev);

int
clist_close_writer_and_init_reader(gx_device_clist *cldev);

void
clist_select_render_plane(gx_device *dev, int y, int height,
                          gx_render_plane_t *render_plane, int index);

int clist_rasterize_lines(gx_device *dev, int y, int lineCount,
                                  gx_device *bdev,
                                  const gx_render_plane_t *render_plane,
                                  int *pmy);

/* Enable multi threaded rendering. Returns > 0 if supported, < 0 if single threaded */
int
clist_enable_multi_thread_render(gx_device *dev);

/* Shutdown render threads and free up the related memory */
void
clist_teardown_render_threads(gx_device *dev);

/* Minimum BufferSpace needed when writing the clist */
/* This is an exported function because it is used to set up render threads */
/* and in clist_init_states to make sure the buffer is large enough */
size_t clist_minimum_buffer(int nbands);

#ifdef DEBUG
#define clist_debug_rect clist_debug_rect_imp
void clist_debug_rect_imp(int x, int y, int width, int height);
#define clist_debug_image_rect clist_debug_image_rect_imp
void clist_debug_image_rect_imp(int x, int y, int width, int height);
#define clist_debug_set_ctm clist_debug_set_ctm_imp
void clist_debug_set_ctm_imp(const gs_matrix *m);
#else
#define clist_debug_rect (void)
#define clist_debug_image_rect (void)
#define clist_debug_set_ctm (void)
#endif

/* Cropping by Y is necessary when the shading path is smaller than shading.
   In this case the clipping path is written into the path's bands only.
   Thus bands outside the shading path are not clipped,
   but the shading may paint into them, so use this macro to crop them.

   Besides that, cropping by Y is necessary when a transparency compositor
   is installed over clist writer. Transparency compositors change the number
   of device color components, so transparency group's elements
   must not paint to bands that are not covered by the transparency bbox
   to prevent a failure when clist reader recieves a wrong number of color components.
 */
#define crop_fill_y(cdev, ry, rheight)\
    BEGIN\
        if (ry < cdev->cropping_min) {\
            rheight = ry + rheight - cdev->cropping_min;\
            ry = cdev->cropping_min;\
        }\
        if (ry + rheight > cdev->cropping_max)\
            rheight = cdev->cropping_max - ry;\
    END

#define crop_fill(dev, x, y, w, h)\
    BEGIN\
        if ( x < 0 )\
            w += x, x = 0;\
        fit_fill_w(dev, x, w);\
        crop_fill_y(dev, y, h);\
    END

#define crop_copy_y(cdev, data, data_x, raster, id, ry, rheight)\
    BEGIN\
        if (ry < cdev->cropping_min) {\
            rheight = ry + rheight - cdev->cropping_min;\
            data += (cdev->cropping_min - ry) * raster;\
            id = gx_no_bitmap_id;\
            ry = cdev->cropping_min;\
        }\
        if (ry + rheight > cdev->cropping_max)\
            rheight = cdev->cropping_max - ry;\
    END

#define crop_copy(dev, data, data_x, raster, id, x, y, w, h)\
    BEGIN\
        if ( x < 0 )\
            w += x, data_x -= x, x = 0;\
        fit_fill_w(dev, x, w);\
        crop_copy_y(dev, data, data_x, raster, id, y, h);\
    END

/* Devices that are expected to be able to 'mutate' into being clist
 * devices have to be constructed in a particular way. They have to
 * have to have the "header" be padded out to a given size (so the clist
 * fields can all fit in at the top), and then the device specific
 * fields can follow this.
 * These fields should follow gx_device_common. */
#define gx_device_clist_mutatable_common\
    byte skip[max(sizeof(gx_device_memory), sizeof(gx_device_clist)) -\
              sizeof(gx_device) + sizeof(double) /* padding */];\
    long buffer_space;	          /* amount of space for clist buffer, */\
                                  /* 0 means not using clist */\
    byte *buf;	                  /* buffer for rendering */\
    gs_memory_t *buffer_memory;   /* allocator for command list */\
    gs_memory_t *bandlist_memory; /* allocator for bandlist files */\
    uint clist_disable_mask;      /* mask of clist options to disable */\
    gx_device_procs orig_procs	/* original (std_)procs */


#define GX_CLIST_MUTATABLE_DEVICE_DEFAULTS \
    { 0 },   /* skip */\
    0,       /* buffer_space */\
    NULL,    /* buf */\
    0,       /* buffer_memory */\
    0,       /* bandlist_memory */\
    0,       /* clist_disable_mask */\
    { NULL } /* orig_procs */

typedef struct {
    gx_device_common;
    gx_device_clist_mutatable_common;
} gx_device_clist_mutatable;

extern_st(st_device_clist_mutatable);
#define public_st_device_clist_mutatable()	/* in gxclist.c */\
  gs_public_st_complex_only(st_device_clist_mutatable,\
    gx_device_clist_mutatable, "gx_device_clist_mutatable", 0, \
    device_clist_mutatable_enum_ptrs, device_clist_mutatable_reloc_ptrs,\
    gx_device_finalize)
#define st_device_clist_mutatable_max_ptrs\
  (st_device_clist_mutable_max_ptrs)

/* A useful check to determine if a device that can be mutated to be a
 * clist has undergone such a mutation. */
#define CLIST_MUTATABLE_HAS_MUTATED(pdev) \
    (((gx_device_clist_mutatable *)(pdev))->buffer_space != 0)

int
clist_mutate_to_clist(gx_device_clist_mutatable  *pdev,
                      gs_memory_t                *buffer_memory,
                      byte                      **the_memory,
                const gdev_space_params          *space_params,
                      bool                        bufferSpace_is_exact,
                const gx_device_buf_procs_t      *buf_procs,
                      dev_proc_dev_spec_op(dev_spec_op),
                      size_t                      min_buffer_space);

#endif /* gxclist_INCLUDED */
