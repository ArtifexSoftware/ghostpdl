/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Command list interpreter/rasterizer */
#include "memory_.h"
#include "gx.h"
#include "gp.h"                 /* for gp_fmode_rb */
#include "gpcheck.h"
#include "gserrors.h"
#include "gscdefs.h"            /* for image type table */
#include "gsbitops.h"
#include "gsparams.h"
#include "gsstate.h"            /* (should only be gs_gstate) */
#include "gstrans.h"		/* for gs_is_pdf14trans_compositor */
#include "gxdcolor.h"
#include "gxdevice.h"
#include "gscoord.h"            /* requires gsmatrix.h */
#include "gsdevice.h"           /* for gs_deviceinitialmatrix */
#include "gsiparm4.h"
#include "gxdevmem.h"           /* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gxcmap.h"
#include "gxcolor2.h"
#include "gxcspace.h"           /* for gs_color_space_type */
#include "gxdhtres.h"
#include "gxgetbit.h"
#include "gxpaint.h"            /* for gx_fill/stroke_params */
#include "gxpcolor.h"
#include "gxhttile.h"
#include "gxiparam.h"
#include "gximask.h"
#include "gzpath.h"
#include "gzcpath.h"
#include "gzacpath.h"
#include "stream.h"
#include "strimpl.h"
#include "gxcomp.h"
#include "gsserial.h"
#include "gxdhtserial.h"
#include "gzht.h"
#include "gxshade.h"
#include "gxshade4.h"
#include "gsicc_manage.h"
#include "gsicc.h"

extern_gx_device_halftone_list();
extern_gx_image_type_table();

/* We need color space types for constructing temporary color spaces. */
extern const gs_color_space_type gs_color_space_type_Indexed;

/* Print a bitmap for tracing */
#ifdef DEBUG
static void
cmd_print_bits(gs_memory_t *mem, const byte * data, int width, int height, int raster)
{
    int i, j;

    dmlprintf3(mem, "[L]width=%d, height=%d, raster=%d\n",
              width, height, raster);
    for (i = 0; i < height; i++) {
        const byte *row = data + i * raster;

        dmlprintf(mem, "[L]");
        for (j = 0; j < raster; j++)
            dmprintf1(mem, " %02x", row[j]);
        dmputc(mem, '\n');
    }
}
#else
#  define cmd_print_bits(mem, data, width, height, raster) DO_NOTHING
#endif

/* Get a variable-length integer operand. */
#define cmd_getw(var, p)\
  BEGIN\
    if ( *p < 0x80 ) var = *p++;\
    else { const byte *_cbp; var = cmd_get_w(p, &_cbp); p = _cbp; }\
  END

static long
cmd_get_w(const byte * p, const byte ** rp)
{
    int val = *p++ & 0x7f;
    int shift = 7;

    for (; val |= (int)(*p & 0x7f) << shift, *p++ > 0x7f; shift += 7);
    *rp = p;
    return val;
}

/* Get a variable-length fractional operand. */
#define cmd_getfrac(var, p)\
  BEGIN\
    if ( !(*p & 1) ) var = (*p++) << 24;\
    else { const byte *_cbp; var = cmd_get_frac31(p, &_cbp); p = _cbp; }\
  END
static frac31
cmd_get_frac31(const byte * p, const byte ** rp)
{
    frac31 val = (*p++ & 0xFE) << 24;
    int shift = 24 - 7;

    for (; val |= (frac31)(*p & 0xFE) << shift, *p++ & 1; shift -= 7);
    *rp = p;
    return val;
}

/*
 * Define the structure for keeping track of the command reading buffer.
 *
 * The ptr member is only used for passing the current pointer to, and
 * receiving an updated pointer from, commands implemented as separate
 * procedures: normally it is kept in a register.
 */
typedef struct command_buf_s {
    byte *data;                 /* actual buffer, guaranteed aligned */
    uint size;
    const byte *ptr;            /* next byte to be read (see above) */
    const byte *warn_limit;     /* refill warning point */
    const byte *end;            /* byte just beyond valid data */
    stream *s;                  /* for refilling buffer */
    int end_status;
} command_buf_t;

/* Set the end of a command buffer. */
static void
set_cb_end(command_buf_t *pcb, const byte *end)
{
    pcb->end = end;
    pcb->warn_limit = pcb->data + (pcb->size - cmd_largest_size + 1);
    if ( pcb->warn_limit > pcb->end )
        pcb->warn_limit = pcb->end;     /**** This is dangerous. Other places ****/
                                        /**** assume that the limit is a soft ****/
                                        /**** limit and should check 'end'    ****/
}

/* Read more data into a command buffer. */
static int
top_up_cbuf(command_buf_t *pcb, const byte **pcbp)
{
    uint nread;
    const byte *cbp = *pcbp;
    byte *cb_top = pcb->data + (pcb->end - cbp);
#   ifdef DEBUG
    stream_state *st = pcb->s->state;
#   endif

    if (pcb->end - cbp >= pcb->size) {
        errprintf(pcb->s->memory, "Clist I/O error: cbp past end of buffer\n");
        return (gs_error_ioerror);
    }

    if (seofp(pcb->s)) {
        /* Can't use offset_map, because s_close resets s->state. Don't top up. */
        pcb->end_status = pcb->s->end_status;
        return 0;
    }
#   ifdef DEBUG
    {
        int code = top_up_offset_map(st, pcb->data, cbp, pcb->end);

        if (code < 0)
            return code;
    }
#   endif
    memmove(pcb->data, cbp, pcb->end - cbp);
    nread = pcb->end - cb_top;
    pcb->end_status = sgets(pcb->s, cb_top, nread, &nread);
    if ( nread == 0 ) {
        /* No data for this band at all. */
        if (cb_top >= pcb->end) {
            /* should not happen */
            *pcbp = pcb->data;
            pcb->data[0] = cmd_opv_end_run;
            return_error(gs_error_ioerror);
        }
        *cb_top = cmd_opv_end_run;
        nread = 1;
    }
    set_cb_end(pcb, cb_top + nread);
    process_interrupts(pcb->s->memory);
    *pcbp = pcb->data;
    return 0;
}

/* Read data from the command buffer and stream. */
static const byte *
cmd_read_data(command_buf_t *pcb, byte *ptr, uint rsize, const byte *cbp)
{
    if (pcb->end - cbp >= rsize) {
        memmove(ptr, cbp, rsize);
        return cbp + rsize;
    } else {
        uint cleft = pcb->end - cbp;
        uint rleft = rsize - cleft;

        memmove(ptr, cbp, cleft);
        sgets(pcb->s, ptr + cleft, rleft, &rleft);
        return pcb->end;
    }
}
#define cmd_read(ptr, rsize, cbp)\
  cbp = cmd_read_data(&cbuf, ptr, rsize, cbp)

/* Read a fixed-size value from the command buffer. */
static inline const byte *
cmd_copy_value(void *pvar, int var_size, const byte *cbp)
{
    memcpy(pvar, cbp, var_size);
    return cbp + var_size;
}
#define cmd_get_value(var, cbp)\
  cbp = cmd_copy_value(&var, sizeof(var), cbp)

/*
 * Define a buffer structure to hold a serialized halftone. This is
 * used only if the serialized halftone is too large to fit into
 * the command buffer.
 */
typedef struct ht_buff_s {
    uint    ht_size, read_size;
    byte *  pcurr;
    byte *  pbuff;
} ht_buff_t;

/*
 * Render one band to a specified target device.  Note that if
 * action == setup, target may be 0.
 */
static int read_set_tile_size(command_buf_t *pcb, tile_slot *bits, bool for_pattern);
static int read_set_bits(command_buf_t *pcb, tile_slot *bits,
                          int compress, gx_clist_state *pcls,
                          gx_strip_bitmap *tile, tile_slot **pslot,
                          gx_device_clist_reader *cdev, gs_memory_t *mem);
static int read_set_misc2(command_buf_t *pcb, gs_gstate *pgs,
                           segment_notes *pnotes);
static int read_set_color_space(command_buf_t *pcb, gs_gstate *pgs,
                                 gx_device_clist_reader *cdev, gs_memory_t *mem);
static int read_begin_image(command_buf_t *pcb, gs_image_common_t *pic,
                             gs_color_space *pcs);
static int read_put_params(command_buf_t *pcb, gs_gstate *pgs,
                            gx_device_clist_reader *cdev,
                            gs_memory_t *mem);
static int read_create_compositor(command_buf_t *pcb, gs_memory_t *mem, gs_composite_t **ppcomp);
static int apply_create_compositor(gx_device_clist_reader *cdev, gs_gstate *pgs,
                                   gs_memory_t *mem, gs_composite_t *pcomp,
                                   int x0, int y0, gx_device **ptarget);
static int read_alloc_ht_buff(ht_buff_t *, uint, gs_memory_t *);
static int read_ht_segment(ht_buff_t *, command_buf_t *, gs_gstate *,
                            gx_device *, gs_memory_t *);

static const byte *cmd_read_rect(int, gx_cmd_rect *, const byte *);
static const byte *cmd_read_short_bits(command_buf_t *pcb, byte *data, int tot_bytes,
                                        int width_bytes, int height,
                                        uint raster, const byte *cbp);
static int cmd_select_map(cmd_map_index, cmd_map_contents,
                           gs_gstate *, int **,
                           frac **, uint *, gs_memory_t *);
static int cmd_create_dev_ht(gx_device_halftone **, gs_memory_t *);
static int cmd_resize_halftone(gx_device_halftone **, uint,
                                gs_memory_t *);
static int clist_decode_segment(gx_path *, int, fixed[6],
                                 gs_fixed_point *, int, int,
                                 segment_notes);
static int clist_do_polyfill(gx_device *, gx_path *,
                              const gx_drawing_color *,
                              gs_logical_operation_t);

static inline void
enqueue_compositor(gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last, gs_composite_t *pcomp)
{
    if (*ppcomp_last == NULL) {
        pcomp->prev = pcomp->next = NULL;
        *ppcomp_last = *ppcomp_first = pcomp;
    } else {
        (*ppcomp_last)->next = pcomp;
        pcomp->prev = *ppcomp_last;
        pcomp->next = NULL;
        *ppcomp_last = pcomp;
    }
}

#if 0 /* Appears unused - keep for a while. */
static inline gs_composite_t *
dequeue_last_compositor(gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last)
{
    gs_composite_t *pcomp = *ppcomp_last;

    if (*ppcomp_first == *ppcomp_last)
        *ppcomp_first = *ppcomp_last = NULL;
    else {
        *ppcomp_last = (*ppcomp_last)->prev;
        pcomp->prev = NULL;
        (*ppcomp_last)->next = NULL;
    }
    return pcomp;
}

static inline gs_composite_t *
dequeue_first_compositor(gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last)
{
    gs_composite_t *pcomp = *ppcomp_first;

    if (*ppcomp_first == *ppcomp_last)
        *ppcomp_first = *ppcomp_last = NULL;
    else {
        *ppcomp_first = (*ppcomp_first)->next;
        pcomp->next = NULL;
        (*ppcomp_last)->prev = NULL;
    }
    return pcomp;
}
#endif

static inline int
dequeue_compositor(gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last, gs_composite_t *pcomp)
{
    if (*ppcomp_last == *ppcomp_first) {
        if (*ppcomp_last == pcomp) {
            *ppcomp_last = *ppcomp_first = NULL;
            return 0;
        } else
            return_error(gs_error_unregistered); /* Must not happen. */
    } else {
        gs_composite_t *pcomp_next = pcomp->next, *pcomp_prev = pcomp->prev;

        if (*ppcomp_last == pcomp)
            *ppcomp_last = pcomp->prev;
        else
            pcomp_next->prev = pcomp_prev;
        if (*ppcomp_first == pcomp)
            *ppcomp_first = pcomp->next;
        else
            pcomp_prev->next = pcomp_next;
        pcomp->next = pcomp->prev = NULL;
        return 0;
    }
}

static inline void
free_compositor(gs_composite_t *pcomp, gs_memory_t *mem)
{
    gs_free_object(mem, pcomp, "free_compositor");
}

static inline bool
is_null_compositor_op(const byte *cbp, int *length)
{
    if (cbp[0] == cmd_opv_end_run) {
        *length = 1;
        return true;
    }
    return false;
}

static int
execute_compositor_queue(gx_device_clist_reader *cdev, gx_device **target, gx_device **tdev, gs_gstate *pgs,
                         gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last, gs_composite_t *pcomp_from,
                         int x0, int y0, gs_memory_t *mem, bool idle)
{
    while (pcomp_from != NULL) {
        gs_composite_t *pcomp = pcomp_from;
        int code;

        pcomp_from = pcomp->next;
        code = dequeue_compositor(ppcomp_first, ppcomp_last, pcomp);
        if (code < 0)
            return code;
        pcomp->idle |= idle;
        code = apply_create_compositor(cdev, pgs, mem, pcomp, x0, y0, target); /* Releases the compositor. */
        if (code < 0)
            return code;
        *tdev = *target;
    }
    return 0;
}

static void
mark_as_idle(gs_composite_t *pcomp_start, gs_composite_t *pcomp_end)
{
    gs_composite_t *pcomp = pcomp_start;

    while (pcomp != NULL) {
        pcomp->idle = true;
        if (pcomp == pcomp_end)
            break;
        pcomp = pcomp->next;
    }
}

static inline int
drop_compositor_queue(gs_composite_t **ppcomp_first, gs_composite_t **ppcomp_last,
                      gs_composite_t *pcomp_from, gs_memory_t *mem, int x0, int y0,
                      gs_gstate *pgs)
{
    gs_composite_t *pcomp;

    do {
        int code;

        pcomp = *ppcomp_last;
        if (pcomp == NULL)
            return 0;
        dequeue_compositor(ppcomp_first, ppcomp_last, *ppcomp_last);
        code = pcomp->type->procs.adjust_ctm(pcomp, x0, y0, pgs);
        if (code < 0)
            return code;
        free_compositor(pcomp, mem);
    } while (pcomp != pcomp_from);
    return 0;
}

static int
read_set_misc_map(byte cb, command_buf_t *pcb, gs_gstate *pgs, gs_memory_t *mem)
{
    const byte *cbp = pcb->ptr;
    frac *mdata;
    int *pcomp_num;
    uint count = 0;		/* quiet compiler */
    cmd_map_contents cont =
        (cmd_map_contents)(cb & 0x30) >> 4;
    int code;

    code = cmd_select_map(cb & 0xf, cont,
                          pgs,
                          &pcomp_num,
                          &mdata, &count, mem);

    if (code < 0)
        return code;
    /* Get component number if relevant */
    if (pcomp_num == NULL)
        cbp++;
    else {
        *pcomp_num = (int) *cbp++;
        if_debug1m('L', mem, " comp_num=%d", *pcomp_num);
    }
    if (cont == cmd_map_other) {
        cbp = cmd_read_data(pcb, (byte *)mdata, count, cbp);

#ifdef DEBUG
        if (gs_debug_c('L')) {
            uint i;

            for (i = 0; i < count / sizeof(*mdata); ++i)
                dmprintf1(mem, " 0x%04x", mdata[i]);
            dmputc(mem, '\n');
        }
    } else {
        if_debug0m('L', mem, " none\n");
#endif
    }
    /* Recompute the effective transfer, */
    /* in case this was a transfer map. */
    gx_gstate_set_effective_xfer(pgs);
    pcb->ptr = cbp;
    return 0;
}

int
clist_playback_band(clist_playback_action playback_action,
                    gx_device_clist_reader *cdev, stream *s,
                    gx_device *target, int x0, int y0, gs_memory_t * mem)
{
    byte *cbuf_storage;
    command_buf_t cbuf;
    /* data_bits is for short copy_* bits and copy_* compressed, */
    /* must be aligned */
    byte *data_bits = 0;
    const byte *cbp;
    int dev_depth;              /* May vary due to compositing devices */
    int dev_depth_bytes;
    int odd_delta_shift;
    int num_zero_bytes;
    gx_device *tdev;
    gx_clist_state state;
    gx_color_index *set_colors;
    gx_device_color *set_dev_colors;
    tile_slot *state_slot;
    gx_strip_bitmap state_tile; /* parameters for reading tiles */
    tile_slot tile_bits;        /* parameters of current tile */
    gs_int_point tile_phase, color_phase;
    gx_path path;
    bool in_path;
    gs_fixed_point ppos;
    gx_clip_path clip_path;
    bool use_clip;
    gx_clip_path *pcpath;
    gx_device_cpath_accum clip_accum;
    gs_fixed_rect target_box;
    struct _cas {
        bool lop_enabled;
        gx_device_color dcolor;
        gs_fixed_point fa_save;
    } clip_save;
    bool in_clip = false;
    gs_gstate gs_gstate;
    gx_device_color fill_color;
    gx_device_color stroke_color;
    float dash_pattern[cmd_max_dash];
    gx_fill_params fill_params;
    gx_stroke_params stroke_params;
#ifdef DEBUG
    gs_halftone_type halftone_type;
#endif
    union im_ {
        gs_image_common_t c;
        gs_data_image_t d;
        gs_image1_t i1;
        gs_image4_t i4;
    } image;
    gs_int_rect image_rect;
    gs_color_space *pcs = NULL;
    gx_image_enum_common_t *image_info;
    gx_image_plane_t planes[32];
    uint data_height;
    uint data_size;
    byte *data_on_heap;
    fixed vs[6];
    segment_notes notes;
    int data_x;
    int code = 0;
    ht_buff_t  ht_buff;
    gx_device *const orig_target = target;
    gx_device_clip clipper_dev;
    bool clipper_dev_open;
    patch_fill_state_t pfs;
    int op = 0;
    int plane_height = 0;

#ifdef DEBUG
    stream_state *st = s->state; /* Save because s_close resets s->state. */
#endif
    gs_composite_t *pcomp_first = NULL, *pcomp_last = NULL;
    tile_slot bits;             /* parameters for reading bits */

    /* pad the cbuf data area a bit (just in case) */
    if ((cbuf_storage = gs_alloc_bytes(mem, cbuf_size + sizeof(double),
                               "clist_playback_band(cbuf_storage)")) == NULL) {
        return_error(gs_error_VMerror);
    }
    cbuf.data = (byte *)cbuf_storage;
    cbuf.size = cbuf_size;
    cbuf.s = s;
    cbuf.end_status = 0;
    set_cb_end(&cbuf, cbuf.data + cbuf.size);
    cbp = cbuf.end;

    pfs.dev = NULL; /* Indicate "not initialized". */
    memset(&ht_buff, 0, sizeof(ht_buff));

    /* The following initializations are to quiet gcc warnings. */
    memset(&bits, 0, sizeof(bits));
    memset(&tile_bits, 0, sizeof(tile_bits));
    memset(&clip_save, 0, sizeof(clip_save));
    memset(&state_slot, 0, sizeof(state_slot));
    ppos.x = ppos.y = 0;

in:                             /* Initialize for a new page. */
    tdev = target;
    set_colors = state.colors;
    set_dev_colors = state.tile_color_devn;
    use_clip = false;
    pcpath = NULL;
    clipper_dev_open = false;
    notes = sn_none;
    data_x = 0;
    {
        static const gx_clist_state cls_initial = { cls_initial_values };

        state = cls_initial;
    }
    state_tile.id = gx_no_bitmap_id;
    state_tile.shift = state_tile.rep_shift = 0;
    state_tile.size.x = state_tile.size.y = 0;
    state_tile.num_planes = 1;
    tile_phase.x = color_phase.x = x0;
    tile_phase.y = color_phase.y = y0;
    gx_path_init_local(&path, mem);
    in_path = false;
    /*
     * Initialize the clipping region to the full page.
     * (Since we also initialize use_clip to false, this is arbitrary.)
     */
    {
        gs_fixed_rect cbox;

        gx_cpath_init_local(&clip_path, mem);
        cbox.p.x = 0;
        cbox.p.y = 0;
        cbox.q.x = cdev->width;
        cbox.q.y = cdev->height;
        gx_cpath_from_rectangle(&clip_path, &cbox);
    }
    if (target != 0)
        (*dev_proc(target, get_clipping_box))(target, &target_box);
    memset(&gs_gstate, 0, sizeof(gs_gstate));
    GS_STATE_INIT_VALUES_CLIST((&gs_gstate));
    code = gs_gstate_initialize(&gs_gstate, mem);
    gs_gstate.device = tdev;
    gs_gstate.view_clip = NULL; /* Avoid issues in pdf14 fill stroke */
    gs_gstate.clip_path = &clip_path;
    pcs = gs_cspace_new_DeviceGray(mem);
    if (pcs == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto out;
    }
    pcs->type->install_cspace(pcs, &gs_gstate);
    gs_gstate.color[0].color_space = pcs;
    rc_increment_cs(pcs);
    gs_gstate.color[1].color_space = pcs;
    rc_increment_cs(pcs);
    /* Remove the ICC link cache and replace with the device link cache
       so that we share the cache across bands */
    rc_decrement(gs_gstate.icc_link_cache,"clist_playback_band");
    gs_gstate.icc_link_cache = cdev->icc_cache_cl;
    /* Need to lock during the increment of the link cache */
    gx_monitor_enter(cdev->icc_cache_cl->lock);
    rc_increment(cdev->icc_cache_cl);
    gx_monitor_leave(cdev->icc_cache_cl->lock); /* let everyone run */
    if (code < 0)
        goto out;

    gs_gstate.line_params.dash.pattern = dash_pattern;
    if (tdev != 0) {
        gx_set_cmap_procs(&gs_gstate, tdev);
    }
    gx_gstate_setscreenphase(&gs_gstate, -x0, -y0, gs_color_select_all);
#ifdef DEBUG
    halftone_type = ht_type_none;
#endif
    fill_color.ccolor_valid = false;
    color_unset(&fill_color);
    data_bits = gs_alloc_bytes(mem, data_bits_size,
                               "clist_playback_band(data_bits)");
    if (data_bits == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto out;
    }
    while (code >= 0) {
        int compress;
        int depth = 0x7badf00d; /* Initialize against indeterminizm. */
        int raster = 0x7badf00d; /* Initialize against indeterminizm. */
        byte *source = NULL;  /* Initialize against indeterminizm. */
        gx_color_index colors[2];
        gx_color_index *pcolor;
        gx_device_color *pdcolor = NULL;
        gs_logical_operation_t log_op;

        /* Make sure the buffer contains a full command. */
        if (cbp >= cbuf.warn_limit) {
            if (cbuf.end_status < 0) {  /* End of file or error. */
                if (cbp >= cbuf.end) {
                    code = (cbuf.end_status == EOFC ? 0 :
                            gs_note_error(gs_error_ioerror));
                    break;
                }
            } else {
                code = top_up_cbuf(&cbuf, &cbp);
                if (code < 0)
                    goto top_up_failed;
            }
        }
        op = *cbp++;
#ifdef DEBUG
        if (gs_debug_c('L')) {
            const char *const *sub = cmd_sub_op_names[op >> 4];
            long offset = (long)clist_file_offset(st, cbp - 1 - cbuf.data);

            if (sub)
                dmlprintf1(mem, "[L]%s", sub[op & 0xf]);
            else
                dmlprintf2(mem, "[L]%s %d", cmd_op_names[op >> 4], op & 0xf);
            dmlprintf1(mem, "(offset=%ld):", offset);
        }
#endif
        switch (op >> 4) {
            case cmd_op_misc >> 4:
                switch (op) {
                    case cmd_opv_end_run:
                        if_debug0m('L', mem, "\n");
                        continue;
                    case cmd_opv_set_tile_size:
                        cbuf.ptr = cbp;
                        code = read_set_tile_size(&cbuf, &tile_bits,
                                    gx_device_is_pattern_clist((gx_device *)cdev));
                        cbp = cbuf.ptr;
                        if (code < 0)
                            goto out;
                        continue;
                    case cmd_opv_set_tile_phase:
                        cmd_getw(state.tile_phase.x, cbp);
                        cmd_getw(state.tile_phase.y, cbp);
                        if_debug2m('L', mem, " (%d,%d)\n",
                                   state.tile_phase.x,
                                   state.tile_phase.y);
                        goto set_phase;
                    case cmd_opv_set_tile_bits:
                        bits = tile_bits;
                        compress = 0;
                      stb:
                        cbuf.ptr = cbp;
                        code = read_set_bits(&cbuf, &bits, compress,
                                             &state, &state_tile, &state_slot,
                                             cdev, mem);
                        cbp = cbuf.ptr;
                        if (code < 0)
                            goto out;
                        goto stp;
                    case cmd_opv_set_bits:
                        compress = *cbp & 3;
                        bits.cb_depth = *cbp++ >> 2;
                        cmd_getw(bits.width, cbp);
                        cmd_getw(bits.height, cbp);
                        if_debug4m('L', mem, " compress=%d depth=%d size=(%d,%d)",
                                   compress, bits.cb_depth,
                                   bits.width, bits.height);
                        bits.cb_raster =
                            bitmap_raster(bits.width * bits.cb_depth);
                        bits.x_reps = bits.y_reps = 1;
                        bits.shift = bits.rep_shift = 0;
                        bits.num_planes = 1;
                        goto stb;
                    case cmd_opv_set_tile_color:
                        set_colors = state.tile_colors;
                        if_debug0m('L', mem, "\n");
                        continue;
                    case cmd_opv_set_misc:
                        {
                            uint cb = *cbp++;

                            switch (cb >> 6) {
                                case cmd_set_misc_lop >> 6:
                                    cmd_getw(state.lop, cbp);
                                    state.lop = (state.lop << 6) + (cb & 0x3f);
                                    if_debug1m('L', mem, " lop=0x%x\n", state.lop);
                                    if (state.lop_enabled)
                                        gs_gstate.log_op = state.lop;
                                    break;
                                case cmd_set_misc_data_x >> 6:
                                    if (cb & 0x20)
                                        cmd_getw(data_x, cbp);
                                    else
                                        data_x = 0;
                                    data_x = (data_x << 5) + (cb & 0x1f);
                                    if_debug1m('L', mem, " data_x=%d\n", data_x);
                                    break;
                                case cmd_set_misc_map >> 6:
                                    cbuf.ptr = cbp;
                                    code = read_set_misc_map(cb, &cbuf, &gs_gstate, mem);
                                    if (code < 0)
                                        goto out;
                                    cbp = cbuf.ptr;
                                    break;
                                case cmd_set_misc_halftone >> 6: {
                                    uint num_comp;
#ifdef DEBUG
                                    halftone_type = cb & 0x3f;
#endif
                                    cmd_getw(num_comp, cbp);
#ifdef DEBUG
                                    if_debug2m('L', mem, " halftone type=%d num_comp=%u\n",
                                               halftone_type, num_comp);
#endif
                                    code = cmd_resize_halftone(
                                                        &gs_gstate.dev_ht,
                                                        num_comp, mem);
                                    if (code < 0)
                                        goto out;
                                    break;
                                }
                                default:
                                    goto bad_op;
                            }
                        }
                        continue;
                    case cmd_opv_enable_lop:
                        state.lop_enabled = true;
                        gs_gstate.log_op = state.lop;
                        if_debug0m('L', mem, "\n");
                        continue;
                    case cmd_opv_disable_lop:
                        state.lop_enabled = false;
                        gs_gstate.log_op = lop_default;
                        if_debug0m('L', mem, "\n");
                        continue;
                    case cmd_opv_end_page:
                        if_debug0m('L', mem, "\n");
                        /*
                         * Do end-of-page cleanup, then reinitialize if
                         * there are more pages to come.
                         */
                        goto out;
                    case cmd_opv_delta_color0:
                        pcolor = &set_colors[0];
                        goto delta2_c;
                    case cmd_opv_delta_color1:
                        pcolor = &set_colors[1];
                      delta2_c:set_colors = state.colors;
                        /* See comments for cmd_put_color() in gxclutil.c. */
                        {
                            gx_color_index delta = 0;
                            uint data;

                            dev_depth = (tdev->color_info.depth <= 8*sizeof(gx_color_index) ?
                                         tdev->color_info.depth : 8*sizeof(gx_color_index));
                            dev_depth_bytes = (dev_depth + 7) >> 3;
                            switch (dev_depth_bytes) {
                                /* For cases with an even number of bytes */
                                case 8:
                                    data = *cbp++;
                                    delta = (((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f)) << 24) << 24;
                                    /* fall through */
                                case 6:
                                    data = *cbp++;
                                    delta |= (((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f)) << 16) << 16;
                                    /* fall through */
                                case 4:
                                    data = *cbp++;
                                    delta |= ((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f)) << 16;
                                    /* fall through */
                                case 2:
                                    data = *cbp++;
                                    delta |= ((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f));
                                    break;
                                /* For cases with an odd number of bytes */
                                case 7:
                                    data = *cbp++;
                                    delta = ((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f)) << 16;
                                    /* fall through */
                                case 5:
                                    data = *cbp++;
                                    delta |= ((gx_color_index)
                                        ((data & 0xf0) << 4) + (data & 0x0f));
                                    /* fall through */
                                case 3:
                                    data = *cbp++;
                                    odd_delta_shift = (dev_depth_bytes - 3) * 8;
                                    delta |= ((gx_color_index)
                                        ((data & 0xe0) << 3) + (data & 0x1f)) << odd_delta_shift;
                                    data = *cbp++;
                                    delta |= ((gx_color_index) ((data & 0xf8) << 2) + (data & 0x07))
                                                        << (odd_delta_shift + 11);
                            }
                            *pcolor += delta - cmd_delta_offsets[dev_depth_bytes];
                        }
                        if (sizeof(*pcolor) <= sizeof(ulong))
                            if_debug1m('L', mem, " 0x%lx\n", (ulong)*pcolor);
                        else
                            if_debug2m('L', mem, " 0x%8lx%08lx\n",
                                       (ulong)(*pcolor >> 8*(sizeof(*pcolor) - sizeof(ulong))), (ulong)*pcolor);
                        continue;
                    case cmd_opv_set_copy_color:
                        state.color_is_alpha = 0;
                        if_debug0m('L', mem, "\n");
                        continue;
                    case cmd_opv_set_copy_alpha:
                        state.color_is_alpha = 1;
                        if_debug0m('L', mem, "\n");
                        continue;
                    default:
                        goto bad_op;
                }
                /*NOTREACHED */
            case cmd_op_set_color0 >> 4:
                pcolor = &set_colors[0];
                goto set_color;
            case cmd_op_set_color1 >> 4:
                pcolor = &set_colors[1];
              set_color:set_colors = state.colors;
                /*
                 * We have a special case for gx_no_color_index. If the low
                 * order 4 bits are "cmd_no_color_index" then we really
                 * have a value of gx_no_color_index.  Otherwise the these
                 * bits indicate the number of low order zero bytes in the
                 * value.  See comments for cmd_put_color() in gxclutil.c.
                 */
                num_zero_bytes = op & 0x0f;

                if (num_zero_bytes == cmd_no_color_index)
                    *pcolor = gx_no_color_index;
                else {
                    gx_color_index color = 0;

                    dev_depth = (tdev->color_info.depth < 8*sizeof(gx_color_index) ?
                                 tdev->color_info.depth : 8*sizeof(gx_color_index));
                    dev_depth_bytes = (dev_depth + 7) >> 3;
                    switch (dev_depth_bytes - num_zero_bytes) {
                        case 8:
                            color = (gx_color_index) * cbp++;
                        case 7:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 6:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 5:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 4:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 3:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 2:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        case 1:
                            color = (color << 8) | (gx_color_index) * cbp++;
                        default:
                            break;
                    }
                    color <<= num_zero_bytes * 8;
                    *pcolor = color;
                }
                if (sizeof(*pcolor) <= sizeof(ulong))
                    if_debug1m('L', mem, " 0x%lx\n", (ulong)*pcolor);
                else
                    if_debug2m('L', mem, " 0x%8lx%08lx\n",
                               (ulong)(*pcolor >> 8*(sizeof(*pcolor) - sizeof(ulong))), (ulong)*pcolor);
                continue;
            case cmd_op_fill_rect >> 4:
            case cmd_op_tile_rect >> 4:
                cbp = cmd_read_rect(op, &state.rect, cbp);
                break;
            case cmd_op_fill_rect_short >> 4:
            case cmd_op_tile_rect_short >> 4:
                state.rect.x += *cbp + cmd_min_short;
                state.rect.width += cbp[1] + cmd_min_short;
                if (op & 0xf) {
                    state.rect.height += (op & 0xf) + cmd_min_dxy_tiny;
                    cbp += 2;
                } else {
                    state.rect.y += cbp[2] + cmd_min_short;
                    state.rect.height += cbp[3] + cmd_min_short;
                    cbp += 4;
                }
                break;
            case cmd_op_fill_rect_tiny >> 4:
            case cmd_op_tile_rect_tiny >> 4:
                if (op & 8)
                    state.rect.x += state.rect.width;
                else {
                    int txy = *cbp++;

                    state.rect.x += (txy >> 4) + cmd_min_dxy_tiny;
                    state.rect.y += (txy & 0xf) + cmd_min_dxy_tiny;
                }
                state.rect.width += (op & 7) + cmd_min_dw_tiny;
                break;
            case cmd_op_copy_mono_planes >> 4:
                cmd_getw(plane_height, cbp);
                if (plane_height == 0) {
                    /* We are doing a copy mono */
                    depth = 1;
                } else {
                    depth = tdev->color_info.depth;
                }
                if_debug1m('L', mem, " plane_height=0x%x", plane_height);
                goto copy;
            case cmd_op_copy_color_alpha >> 4:
                if (state.color_is_alpha) {
                    if (!(op & 8))
                        depth = *cbp++;
                } else
                    depth = tdev->color_info.depth;
                plane_height = 0;
              copy:cmd_getw(state.rect.x, cbp);
                cmd_getw(state.rect.y, cbp);
                if (op & cmd_copy_use_tile) {   /* Use the current "tile". */
#ifdef DEBUG
                    if (state_slot->index != state.tile_index) {
                        mlprintf2(mem, "state_slot->index = %d, state.tile_index = %d!\n",
                                  state_slot->index,
                                  state.tile_index);
                        code = gs_note_error(gs_error_ioerror);
                        goto out;
                    }
#endif
                    depth = state_slot->cb_depth;
                    state.rect.width = state_slot->width;
                    state.rect.height = state_slot->height;
                    if (state.rect.y + state.rect.height > cdev->height)
                        state.rect.height = cdev->height - state.rect.y;	/* clamp as writer did */
                    raster = state_slot->cb_raster;
                    source = (byte *) (state_slot + 1);
                } else {        /* Read width, height, bits. */
                    /* depth was set already. */
                    uint width_bits, width_bytes;
                    uint bytes;
                    uchar planes = 1;
                    uint plane_depth = depth;
                    uint pln;
                    byte compression = op & 3;
                    uint out_bytes;

                    cmd_getw(state.rect.width, cbp);
                    cmd_getw(state.rect.height, cbp);
                    if (plane_height != 0) {
                        planes = tdev->color_info.num_components;
                        plane_depth /= planes;
                    }
                    width_bits = state.rect.width * plane_depth;
                    bytes = clist_bitmap_bytes(width_bits,
                                               state.rect.height,
                                               op & 3, &width_bytes,
                                               (uint *)&raster);
                    if (planes > 1) {
                        out_bytes = raster * state.rect.height;
                        plane_height = state.rect.height;
                    } else {
                        out_bytes = bytes;
                    }
                    /* copy_mono and copy_color/alpha */
                    /* ensure that the bits will fit in a single buffer, */
                    /* even after decompression if compressed. */
#ifdef DEBUG
                    if (planes * out_bytes > data_bits_size) {
                        mlprintf6(mem, "bitmap size exceeds buffer!  width=%d raster=%d height=%d\n    file pos %"PRId64" buf pos %d/%d\n",
                                  state.rect.width, raster,
                                  state.rect.height,
                                  stell(s), (int)(cbp - cbuf.data),
                                  (int)(cbuf.end - cbuf.data));
                        code = gs_note_error(gs_error_ioerror);
                        goto out;
                    }
#endif
                    for (pln = 0; pln < planes; pln++) {
                        byte *plane_bits = data_bits + pln * plane_height * raster;

                        /* Fill the cbuf if needed to get the data for the plane */
                        if (cbp + out_bytes >= cbuf.warn_limit) {
                            code = top_up_cbuf(&cbuf, &cbp);
                            if (code < 0)
                                goto top_up_failed;
                        }
                        if (pln)
                            compression = *cbp++;

                        if (compression == cmd_compress_const) {
                            cbp = cmd_read_data(&cbuf, plane_bits, 1, cbp);
                            if (width_bytes > 0 && state.rect.height > 0)
                                memset(plane_bits+1, *plane_bits, width_bytes * state.rect.height - 1);

                        } else if (compression) {       /* Decompress the image data. */
                            stream_cursor_read r;
                            stream_cursor_write w;

                            /* We don't know the data length a priori, */
                            /* so to be conservative, we read */
                            /* the uncompressed size. */
                            uint cleft = cbuf.end - cbp;

                            if (cleft < bytes  && !cbuf.end_status) {
                                uint nread = cbuf_size - cleft;

#                               ifdef DEBUG
                                    code = top_up_offset_map(st, cbuf.data, cbp, cbuf.end);
                                    if (code < 0)
                                        goto top_up_failed;
#                               endif
                                memmove(cbuf.data, cbp, cleft);
                                cbuf.end_status = sgets(s, cbuf.data + cleft, nread, &nread);
                                set_cb_end(&cbuf, cbuf.data + cleft + nread);
                                cbp = cbuf.data;
                            }
                            r.ptr = cbp - 1;
                            r.limit = cbuf.end - 1;
                            w.ptr = plane_bits - 1;
                            w.limit = w.ptr + data_bits_size;
                            switch (compression) {
                                case cmd_compress_rle:
                                    {
                                        stream_RLD_state sstate;

                                        clist_rld_init(&sstate);
                                        /* The process procedure can't fail. */
                                        (*s_RLD_template.process)
                                            ((stream_state *)&sstate, &r, &w, true);
                                    }
                                    break;
                                case cmd_compress_cfe:
                                    {
                                        stream_CFD_state sstate;

                                        clist_cfd_init(&sstate,
                                        width_bytes << 3 /*state.rect.width */ ,
                                                       state.rect.height, mem);
                                        /* The process procedure can't fail. */
                                        (*s_CFD_template.process)
                                            ((stream_state *)&sstate, &r, &w, true);
                                        (*s_CFD_template.release)
                                            ((stream_state *)&sstate);
                                    }
                                    break;
                                default:
                                    goto bad_op;
                            }
                            cbp = r.ptr + 1;
                            if (pln == 0)
                                source = data_bits;
                        } else if ((state.rect.height > 1 && width_bytes != raster) ||
                                   (plane_height != 0)) {
                            cbp = cmd_read_short_bits(&cbuf, plane_bits, bytes, width_bytes,
                                                      state.rect.height, raster, cbp);
                            if (pln == 0)
                                source = data_bits;
                        } else {
                            /* Never used for planar data */
                            cmd_read(cbuf.data, bytes, cbp);
                            source = cbuf.data;
                        }
                    }
#ifdef DEBUG
                    if (gs_debug_c('L')) {
                        dmprintf2(mem, " depth=%d, data_x=%d\n",
                                  depth, data_x);
                        cmd_print_bits(mem, source, state.rect.width,
                                       state.rect.height, raster);
                    }
#endif
                }
                break;
            case cmd_op_delta_tile_index >> 4:
                state.tile_index += (int)(op & 0xf) - 8;
                goto sti;
            case cmd_op_set_tile_index >> 4:
                state.tile_index =
                    ((op & 0xf) << 8) + *cbp++;
              sti:state_slot =
                    (tile_slot *) (cdev->cache_chunk->data +
                                 cdev->tile_table[state.tile_index].offset);
                if_debug2m('L', mem, " index=%u offset=%lu\n",
                           state.tile_index,
                           cdev->tile_table[state.tile_index].offset);
                state_tile.data = (byte *) (state_slot + 1);
              stp:state_tile.size.x = state_slot->width;
                state_tile.size.y = state_slot->height;
                state_tile.raster = state_slot->cb_raster;
                state_tile.rep_width = state_tile.size.x /
                    state_slot->x_reps;
                state_tile.rep_height = state_tile.size.y /
                    state_slot->y_reps;
                state_tile.rep_shift = state_slot->rep_shift;
                state_tile.shift = state_slot->shift;
                state_tile.id = state_slot->id;
                state_tile.num_planes = state_slot->num_planes;
set_phase:      /*
                 * state.tile_phase is overloaded according to the command
                 * to which it will apply:
                 *      For fill_path, stroke_path, fill_triangle/p'gram,
                 * fill_mask, and (mask or CombineWithColor) images,
                 * it is pdcolor->phase.  For these operations, we
                 * precompute the color_phase values.
                 *      For strip_tile_rectangle and strip_copy_rop,
                 * it is the phase arguments of the call, used with
                 * state_tile.  For these operations, we precompute the
                 * tile_phase values.
                 *
                 * Note that control may get here before one or both of
                 * state_tile or dev_ht has been set.
                 */
                if (state_tile.size.x)
                    tile_phase.x =
                        (state.tile_phase.x + x0) % state_tile.size.x;
                if (gs_gstate.dev_ht && gs_gstate.dev_ht->lcm_width)
                    color_phase.x =
                        (state.tile_phase.x + x0) %
                        gs_gstate.dev_ht->lcm_width;
                /*
                 * The true tile height for shifted tiles is not
                 * size.y: see gxbitmap.h for the computation.
                 */
                if (state_tile.size.y) {
                    int full_height;

                    if (state_tile.shift == 0)
                        full_height = state_tile.size.y;
                    else
                        full_height = state_tile.rep_height *
                            (state_tile.rep_width /
                             igcd(state_tile.rep_shift,
                                  state_tile.rep_width));
                    tile_phase.y =
                        (state.tile_phase.y + y0) % full_height;
                }
                if (gs_gstate.dev_ht && gs_gstate.dev_ht->lcm_height)
                    color_phase.y =
                        (state.tile_phase.y + y0) %
                        gs_gstate.dev_ht->lcm_height;
                gx_gstate_setscreenphase(&gs_gstate,
                                         -(state.tile_phase.x + x0),
                                         -(state.tile_phase.y + y0),
                                         gs_color_select_all);
                continue;
            case cmd_op_misc2 >> 4:
                switch (op) {
                    case cmd_opv_set_fill_adjust:
                        cmd_get_value(gs_gstate.fill_adjust.x, cbp);
                        cmd_get_value(gs_gstate.fill_adjust.y, cbp);
                        if_debug2m('L', mem, " (%g,%g)\n",
                                   fixed2float(gs_gstate.fill_adjust.x),
                                   fixed2float(gs_gstate.fill_adjust.y));
                        continue;
                    case cmd_opv_set_ctm:
                        {
                            gs_matrix mat;

                            cbp = cmd_read_matrix(&mat, cbp);
                            if_debug6m('L', mem, " [%g %g %g %g %g %g]\n",
                                       mat.xx, mat.xy, mat.yx, mat.yy,
                                       mat.tx, mat.ty);
                            mat.tx -= x0;
                            mat.ty -= y0;
                            gs_gstate_setmatrix(&gs_gstate, &mat);
                        }
                        continue;
                    case cmd_opv_set_misc2:
                        cbuf.ptr = cbp;
                        code = read_set_misc2(&cbuf, &gs_gstate, &notes);
                        cbp = cbuf.ptr;
                        if (code < 0)
                            goto out;
                        continue;
                    case cmd_opv_set_dash:
                        {
                            int nb = *cbp++;
                            int n = nb & 0x3f;
                            float dot_length, offset;

                            cmd_get_value(dot_length, cbp);
                            cmd_get_value(offset, cbp);
                            memcpy(dash_pattern, cbp, n * sizeof(float));

                            gx_set_dash(&gs_gstate.line_params.dash,
                                        dash_pattern, n, offset,
                                        NULL);
                            gx_set_dash_adapt(&gs_gstate.line_params.dash,
                                              (nb & 0x80) != 0);
                            gx_set_dot_length(&gs_gstate.line_params,
                                              dot_length,
                                              (nb & 0x40) != 0);
#ifdef DEBUG
                            if (gs_debug_c('L')) {
                                int i;

                                dmprintf4(mem, " dot=%g(mode %d) adapt=%d offset=%g [",
                                         dot_length,
                                         (nb & 0x40) != 0,
                                         (nb & 0x80) != 0, offset);
                                for (i = 0; i < n; ++i)
                                    dmprintf1(mem, "%g ", dash_pattern[i]);
                                dmputs(mem, "]\n");
                            }
#endif
                            cbp += n * sizeof(float);
                        }
                        break;
                    case cmd_opv_enable_clip:
                        pcpath = (use_clip ? &clip_path : NULL);
                        if (pcpath) {
                            code = gx_cpath_ensure_path_list(pcpath);
                            if (code < 0)
                                goto out;
                        }
                        clipper_dev_open = false;
                        if_debug0m('L', mem, "\n");
                        break;
                    case cmd_opv_disable_clip:
                        pcpath = NULL;
                        clipper_dev_open = false;
                        if_debug0m('L', mem, "\n");
                        break;
                    case cmd_opv_begin_clip:
                        pcpath = NULL;
                        clipper_dev_open = false;
                        in_clip = true;
                        if_debug0m('L', mem, "\n");
                        code = gx_cpath_reset(&clip_path);
                        if (code < 0)
                            goto out;
                        gx_cpath_accum_begin(&clip_accum, mem, false);
                        gx_cpath_accum_set_cbox(&clip_accum,
                                                &target_box);
                        tdev = (gx_device *)&clip_accum;
                        clip_save.lop_enabled = state.lop_enabled;
                        clip_save.dcolor = fill_color;
                        clip_save.fa_save.x = gs_gstate.fill_adjust.x;
                        clip_save.fa_save.y = gs_gstate.fill_adjust.y;
                        /* clip_path should match fill_path, i.e., with fill_adjust applied	*/
                        /* If we get here with the fill_adjust = [0, 0], set it to [0.5, 0.5]i	*/
                        if (clip_save.fa_save.x == 0 || clip_save.fa_save.y == 0)
                            gs_gstate.fill_adjust.x = gs_gstate.fill_adjust.y = fixed_half;
                        /* temporarily set a solid color */
                        color_set_pure(&fill_color, (gx_color_index)1);
                        state.lop_enabled = false;
                        gs_gstate.log_op = lop_default;
                        break;
                    case cmd_opv_end_clip:
                        if_debug0m('L', mem, "\n");
                        gx_cpath_accum_end(&clip_accum, &clip_path);
                        tdev = target;
                        /*
                         * If the entire band falls within the clip
                         * path, no clipping is needed.
                         */
                        {
                            gs_fixed_rect cbox;

                            gx_cpath_inner_box(&clip_path, &cbox);
                            use_clip =
                                !(cbox.p.x <= target_box.p.x &&
                                  cbox.q.x >= target_box.q.x &&
                                  cbox.p.y <= target_box.p.y &&
                                  cbox.q.y >= target_box.q.y);
                        }
                        pcpath = (use_clip ? &clip_path : NULL);
                        if (pcpath) {
                            code = gx_cpath_ensure_path_list(pcpath);
                            if (code < 0)
                                goto out;
                        }
                        clipper_dev_open = false;
                        state.lop_enabled = clip_save.lop_enabled;
                        gs_gstate.log_op =
                            (state.lop_enabled ? state.lop :
                             lop_default);
                        fill_color = clip_save.dcolor;
                        /* restore the fill_adjust if it was changed by begin_clip */
                        gs_gstate.fill_adjust.x = clip_save.fa_save.x;
                        gs_gstate.fill_adjust.y = clip_save.fa_save.y;
                        in_clip = false;
                        break;
                    case cmd_opv_set_color_space:
                        cbuf.ptr = cbp;
                        code = read_set_color_space(&cbuf, &gs_gstate, cdev, mem);
                        pcs = gs_gstate.color[0].color_space;
                        cbp = cbuf.ptr;
                        if (code < 0) {
                            if (code == gs_error_rangecheck)
                                goto bad_op;
                            goto out;
                        }
                        break;
                    case cmd_op_fill_rect_hl:
                        {
                            gs_fixed_rect rect_hl;

                            cbp = cmd_read_rect(op & 0xf0, &state.rect, cbp);
                            if (fill_color.type != gx_dc_type_devn) {
                                if_debug0m('L', mem, "hl rect fill without devn color\n");
                                code = gs_note_error(gs_error_typecheck);
                                goto out;
                            }
                            if_debug4m('L', mem, " x=%d y=%d w=%d h=%d\n",
                                       state.rect.x, state.rect.y,
                                       state.rect.width,state.rect.height);
                            rect_hl.p.x = int2fixed(state.rect.x - x0);
                            rect_hl.p.y = int2fixed(state.rect.y - y0);
                            rect_hl.q.x = int2fixed(state.rect.width) + rect_hl.p.x;
                            rect_hl.q.y = int2fixed(state.rect.height) + rect_hl.p.y;
                            code = dev_proc(tdev, fill_rectangle_hl_color) (tdev,
                                                        &rect_hl, NULL,
                                                        &fill_color, NULL);
                        }
                        continue;
                    case cmd_opv_begin_image_rect:
                        cbuf.ptr = cbp;
                        code = read_begin_image(&cbuf, &image.c, pcs);
                        cbp = cbuf.ptr;
                        if (code < 0)
                            goto out;
                        {
                            uint diff;

                            cmd_getw(image_rect.p.x, cbp);
                            cmd_getw(image_rect.p.y, cbp);
                            cmd_getw(diff, cbp);
                            image_rect.q.x = image.d.Width - diff;
                            cmd_getw(diff, cbp);
                            image_rect.q.y = image.d.Height - diff;
                            if_debug4m('L', mem, " rect=(%d,%d),(%d,%d)",
                                       image_rect.p.x, image_rect.p.y,
                                       image_rect.q.x, image_rect.q.y);
                        }
                        goto ibegin;
                    case cmd_opv_begin_image:
                        cbuf.ptr = cbp;
                        code = read_begin_image(&cbuf, &image.c, pcs);
                        cbp = cbuf.ptr;
                        if (code < 0)
                            goto out;
                        image_rect.p.x = 0;
                        image_rect.p.y = 0;
                        image_rect.q.x = image.d.Width;
                        image_rect.q.y = image.d.Height;
                        if_debug2m('L', mem, " size=(%d,%d)",
                                  image.d.Width, image.d.Height);
ibegin:                 if_debug0m('L', mem, "\n");
                        {
                            /* Processing an image operation */
                            dev_proc(tdev, set_graphics_type_tag)(tdev, GS_IMAGE_TAG);/* FIXME: what about text bitmaps? */
                            image.i4.override_in_smask = 0;
                            code = (*dev_proc(tdev, begin_typed_image))
                                (tdev, &gs_gstate, NULL,
                                 (const gs_image_common_t *)&image,
                                 &image_rect, &fill_color, pcpath, mem,
                                 &image_info);
                        }
                        if (code < 0)
                            goto out;
                        break;
                    case cmd_opv_image_plane_data:
                        cmd_getw(data_height, cbp);
                        if (data_height == 0) {
                            if_debug0m('L', mem, " done image\n");
                            code = gx_image_end(image_info, true);
                            if (code < 0)
                                goto out;
                            continue;
                        }
                        {
                            uint flags;
                            int plane;
                            uint raster1 = 0xbaadf00d; /* Initialize against indeterminizm. */

                            cmd_getw(flags, cbp);
                            for (plane = 0;
                                 plane < image_info->num_planes;
                                 ++plane, flags >>= 1) {
                                if (flags & 1) {
                                    if (cbuf.end - cbp <
                                        2 * cmd_max_intsize(sizeof(uint))) {
                                        code = top_up_cbuf(&cbuf, &cbp);
                                        if (code < 0)
                                            goto top_up_failed;
                                    }
                                    cmd_getw(planes[plane].raster, cbp)                                ;
                                    if ((raster1 = planes[plane].raster) != 0)
                                        cmd_getw(data_x, cbp);
                                } else {
                                    planes[plane].raster = raster1;
                                }
                                planes[plane].data_x = data_x;
                            }
                        }
                        goto idata;
                    case cmd_opv_image_data:
                        cmd_getw(data_height, cbp);
                        if (data_height == 0) {
                            if_debug0m('L', mem, " done image\n");
                            code = gx_image_end(image_info, true);
                            if (code < 0)
                                goto out;
                            continue;
                        }
                        {
                            uint bytes_per_plane;
                            int plane;

                            cmd_getw(bytes_per_plane, cbp);
                            if_debug2m('L', mem, " height=%u raster=%u\n",
                                       data_height, bytes_per_plane);
                            for (plane = 0;
                                 plane < image_info->num_planes;
                                 ++plane
                                 ) {
                                planes[plane].data_x = data_x;
                                planes[plane].raster = bytes_per_plane;
                            }
                        }
idata:                  data_size = 0;
                        {
                            int plane;

                            for (plane = 0; plane < image_info->num_planes;
                                 ++plane)
                                data_size += planes[plane].raster;
                        }
                        data_size *= data_height;
                        data_on_heap = 0;
                        if (cbuf.end - cbp < data_size) {
                            code = top_up_cbuf(&cbuf, &cbp);
                            if (code < 0)
                                goto top_up_failed;
                        }
                        if (cbuf.end - cbp >= data_size) {
                            planes[0].data = cbp;
                            cbp += data_size;
                        } else {
                            uint cleft = cbuf.end - cbp;
                            uint rleft = data_size - cleft;
                            byte *rdata;

                            if (data_size > cbuf.end - cbuf.data) {
                                /* Allocate a separate buffer. */
                                rdata = data_on_heap =
                                    gs_alloc_bytes(mem, data_size,
                                                   "clist image_data");
                                if (rdata == 0) {
                                    code = gs_note_error(gs_error_VMerror);
                                    goto out;
                                }
                            } else
                                rdata = cbuf.data;
                            memmove(rdata, cbp, cleft);
                            if (sgets(s, rdata + cleft, rleft, &rleft) < 0) {
                                code = gs_note_error(gs_error_unregistered); /* Must not happen. */
                                goto out;
                            }
                            planes[0].data = rdata;
                            cbp = cbuf.end;     /* force refill */
                        }
                        {
                            int plane;
                            const byte *data = planes[0].data;

                            for (plane = 0;
                                 plane < image_info->num_planes;
                                 ++plane
                                 ) {
                                if (planes[plane].raster == 0)
                                    planes[plane].data = 0;
                                else {
                                    planes[plane].data = data;
                                    data += planes[plane].raster *
                                        data_height;
                                }
                            }
                        }
#ifdef DEBUG
                        if (gs_debug_c('L')) {
                            int plane;

                            for (plane = 0; plane < image_info->num_planes;
                                 ++plane)
                                if (planes[plane].data != 0)
                                    cmd_print_bits(mem,
                                                   planes[plane].data,
                                                   image_rect.q.x -
                                                   image_rect.p.x,
                                                   data_height,
                                                   planes[plane].raster);
                        }
#endif
                        code = gx_image_plane_data(image_info, planes,
                                                   data_height);
                        if (code < 0)
                            gx_image_end(image_info, false);
                        if (data_on_heap)
                            gs_free_object(mem, data_on_heap,
                                           "clist image_data");
                        data_x = 0;
                        if (code < 0)
                            goto out;
                        continue;
                    case cmd_opv_extend:
                        switch (*cbp++) {
                            case cmd_opv_ext_put_params:
                                if_debug0m('L', mem, "put_params\n");
                                cbuf.ptr = cbp;
                                code = read_put_params(&cbuf, &gs_gstate,
                                                        cdev, mem);
                                cbp = cbuf.ptr;
                                if (code > 0)
                                    break; /* empty list */
                                if (code < 0)
                                    goto out;
                                if (playback_action == playback_action_setup)
                                    goto out;
                                break;
                            case cmd_opv_ext_create_compositor:
                                if_debug0m('L', mem, " ext_create_compositor\n");
                                cbuf.ptr = cbp;
                                /*
                                 * The screen phase may have been changed during
                                 * the processing of masked images.
                                 */
                                gx_gstate_setscreenphase(&gs_gstate,
                                            -x0, -y0, gs_color_select_all);
                                cbp -= 2; /* Step back to simplify the cycle invariant below. */
                                for (;;) {
                                    /* This internal loop looks ahead for compositor commands and
                                       copies them into a temporary queue. Compositors, which do not paint something,
                                       are marked as idle and later executed with a reduced functionality
                                       for reducing time and memory expense. */
                                    int len;

                                    if (cbp >= cbuf.warn_limit) {
                                        code = top_up_cbuf(&cbuf, &cbp);
                                        if (code < 0)
                                            goto out;
                                    }
                                    if (cbp[0] == cmd_opv_extend && cbp[1] == cmd_opv_ext_create_compositor) {
                                        gs_composite_t *pcomp, *pcomp_opening;
                                        gs_compositor_closing_state closing_state;

                                        cbuf.ptr = cbp += 2;
                                        code = read_create_compositor(&cbuf, mem, &pcomp);
                                        if (code < 0)
                                            goto out;
                                        cbp = cbuf.ptr;
                                        if (pcomp == NULL)
                                            continue;
                                        if (gs_is_pdf14trans_compositor(pcomp) &&
                                            playback_action == playback_action_render_no_pdf14) {
                                            /* free the compositor object */
                                            gs_free_object(mem, pcomp, "read_create_compositor");
                                            pcomp = NULL;
                                            continue;
                                        }
                                        pcomp_opening = pcomp_last;
                                        closing_state = pcomp->type->procs.is_closing(pcomp, &pcomp_opening, tdev);
                                        switch(closing_state)
                                        {
                                        default:
                                            code = (int)closing_state;
                                            if (code >= 0)
                                                code = gs_note_error(gs_error_unregistered); /* Must not happen. */
                                            goto out;
                                        case COMP_ENQUEUE:
                                            /* Enqueue. */
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            break;
                                        case COMP_EXEC_IDLE:
                                            /* Execute idle. */
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            code = execute_compositor_queue(cdev, &target, &tdev,
                                                &gs_gstate, &pcomp_first, &pcomp_last, pcomp_opening, x0, y0, mem, true);
                                            if (code < 0)
                                                goto out;
                                            break;
                                        case COMP_EXEC_QUEUE:
                                            /* The opening command was executed. Execute the queue. */
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            code = execute_compositor_queue(cdev, &target, &tdev,
                                                &gs_gstate, &pcomp_first, &pcomp_last, pcomp_first, x0, y0, mem, false);
                                            if (code < 0)
                                                goto out;
                                            break;
                                        case COMP_REPLACE_PREV:
                                            /* Replace last compositors. */
                                            code = execute_compositor_queue(cdev, &target, &tdev,
                                                &gs_gstate, &pcomp_first, &pcomp_last, pcomp_opening, x0, y0, mem, true);
                                            if (code < 0)
                                                goto out;
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            break;
                                        case COMP_REPLACE_CURR:
                                            /* Replace specific compositor. */
                                            code = dequeue_compositor(&pcomp_first, &pcomp_last, pcomp_opening);
                                            if (code < 0)
                                                goto out;
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            free_compositor(pcomp_opening, mem);
                                            break;
                                        case COMP_DROP_QUEUE:
                                            /* Annihilate the last compositors. */
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            code = drop_compositor_queue(&pcomp_first, &pcomp_last, pcomp_opening, mem, x0, y0, &gs_gstate);
                                            if (code < 0)
                                                goto out;
                                            break;
                                        case COMP_MARK_IDLE:
                                            /* Mark as idle. */
                                            enqueue_compositor(&pcomp_first, &pcomp_last, pcomp);
                                            mark_as_idle(pcomp_opening, pcomp);
                                        }
                                    } else if (is_null_compositor_op(cbp, &len)) {
                                        cbuf.ptr = cbp += len;
                                    } else if (cbp[0] == cmd_opv_end_page) {
                                        /* End page, drop the queue. */
                                        code = execute_compositor_queue(cdev, &target, &tdev,
                                                &gs_gstate, &pcomp_first, &pcomp_last, pcomp_first, x0, y0, mem, true);
                                        if (code < 0)
                                            goto out;
                                        break;
                                    } else if (pcomp_last != NULL &&
                                            pcomp_last->type->procs.is_friendly(pcomp_last, cbp[0], cbp[1])) {
                                        /* Immediately execute friendly commands
                                           inside the compositor lookahead loop.
                                           Currently there are few friendly commands for the pdf14 compositor only
                                           due to the logic defined in c_pdf14trans_is_friendly.
                                           This code duplicates some code portions from the main loop,
                                           but we have no better idea with no slowdown to the main loop.
                                         */
                                        uint cb;

                                        switch (*cbp++) {
                                            case cmd_opv_extend:
                                                switch (*cbp++) {
                                                    case cmd_opv_ext_put_halftone:
                                                        {
                                                            uint    ht_size;

                                                            enc_u_getw(ht_size, cbp);
                                                            code = read_alloc_ht_buff(&ht_buff, ht_size, mem);
                                                            if (code < 0)
                                                                goto out;
                                                        }
                                                        break;
                                                    case cmd_opv_ext_put_ht_seg:
                                                        cbuf.ptr = cbp;
                                                        code = read_ht_segment(&ht_buff, &cbuf,
                                                                               &gs_gstate, tdev,
                                                                               mem);
                                                        cbp = cbuf.ptr;
                                                        if (code < 0)
                                                            goto out;
                                                        break;
                                                    default:
                                                        code = gs_note_error(gs_error_unregistered); /* Must not happen. */
                                                        goto out;
                                                }
                                                break;
                                            case cmd_opv_set_misc:
                                                cb = *cbp++;
                                                switch (cb >> 6) {
                                                    case cmd_set_misc_map >> 6:
                                                        cbuf.ptr = cbp;
                                                        code = read_set_misc_map(cb, &cbuf, &gs_gstate, mem);
                                                        if (code < 0)
                                                            goto out;
                                                        cbp = cbuf.ptr;
                                                        break;
                                                    default:
                                                        code = gs_note_error(gs_error_unregistered); /* Must not happen. */
                                                        goto out;
                                                }
                                                break;
                                            default:
                                                code = gs_note_error(gs_error_unregistered); /* Must not happen. */
                                                goto out;
                                        }
                                    } else {
                                        /* A drawing command, execute entire queue. */
                                        code = execute_compositor_queue(cdev, &target, &tdev,
                                            &gs_gstate, &pcomp_first, &pcomp_last, pcomp_first, x0, y0, mem, false);
                                        if (code < 0)
                                            goto out;
                                        break;
                                    }
                                }
                                if (pcomp_last != NULL) {
                                    code = gs_note_error(gs_error_unregistered);
                                    goto out;
                                }
                                break;
                            case cmd_opv_ext_put_halftone:
                                {
                                    uint    ht_size;

                                    if_debug0m('L', mem, " ext_put_halftone\n");
                                    enc_u_getw(ht_size, cbp);
                                    code = read_alloc_ht_buff(&ht_buff, ht_size, mem);
                                    if (code < 0)
                                        goto out;
                                }
                                break;
                            case cmd_opv_ext_put_ht_seg:
                                if_debug0m('L', mem, " ext_put_ht_seg\n");
                                cbuf.ptr = cbp;
                                code = read_ht_segment(&ht_buff, &cbuf,
                                                       &gs_gstate, tdev,
                                                       mem);
                                cbp = cbuf.ptr;
                                if (code < 0)
                                    goto out;
                                break;
                            case cmd_opv_ext_set_color_is_devn:
                                state.color_is_devn = true;
                                if_debug0m('L', mem, " ext_set_color_is_devn\n");
                                break;
                            case cmd_opv_ext_unset_color_is_devn:
                                state.color_is_devn = false;
                                if_debug0m('L', mem, " ext_unset_color_is_devn\n");
                                break;
                            case cmd_opv_ext_tile_rect_hl:
                                /* Strip tile with devn colors */
                                cbp = cmd_read_rect(op & 0xf0, &state.rect, cbp);
                                if_debug4m('L', mem, " x=%d y=%d w=%d h=%d\n",
                                           state.rect.x, state.rect.y,
                                           state.rect.width,state.rect.height);
                                code = (*dev_proc(tdev, strip_tile_rect_devn))
                                    (tdev, &state_tile,
                                     state.rect.x - x0, state.rect.y - y0,
                                     state.rect.width, state.rect.height,
                                     &(state.tile_color_devn[0]),
                                     &(state.tile_color_devn[1]),
                                     tile_phase.x, tile_phase.y);
                                break;
                            case cmd_opv_ext_put_fill_dcolor:
                                pdcolor = &fill_color;
                                goto load_dcolor;
                            case cmd_opv_ext_put_stroke_dcolor:
                                pdcolor = &stroke_color;
                                goto load_dcolor;
                            case cmd_opv_ext_put_tile_devn_color0:
                                pdcolor = &set_dev_colors[0];
                                goto load_dcolor;
                            case cmd_opv_ext_put_tile_devn_color1:
                                pdcolor = &set_dev_colors[1];
                    load_dcolor:{
                                    uint    color_size;
                                    int left, offset, l;
                                    const gx_device_color_type_t *  pdct;
                                    byte type_and_flag = *cbp++;
                                    byte is_continuation = type_and_flag & 0x80;

                                    if_debug0m('L', mem, " cmd_opv_ext_put_drawing_color\n");
                                    pdct = gx_get_dc_type_from_index(type_and_flag & 0x7F);
                                    if (pdct == 0) {
                                        code = gs_note_error(gs_error_rangecheck);
                                        goto out;
                                    }
                                    offset = 0;
                                    if (is_continuation)
                                        enc_u_getw(offset, cbp);
                                    enc_u_getw(color_size, cbp);
                                    left = color_size;
                                    if (!left) {
                                        /* We still need to call pdct->read because it may change dev_color.type -
                                           see gx_dc_null_read.*/
                                        code = pdct->read(pdcolor, &gs_gstate,
                                                          pdcolor, tdev, offset,
                                                          cbp, 0, mem);
                                        if (code < 0)
                                            goto out;
                                    }
                                    while (left) {
                                        if (cbuf.warn_limit - cbp < (int)left) {  /* cbp can be past warn_limit */
                                            code = top_up_cbuf(&cbuf, &cbp);
                                            if (code < 0)
                                                goto top_up_failed;
                                        }
                                        l = min(left, cbuf.end - cbp);
                                        code = pdct->read(pdcolor, &gs_gstate,
                                                          pdcolor, tdev, offset,
                                                          cbp, l, mem);
                                        if (code < 0)
                                            goto out;
                                        l = code;
                                        cbp += l;
                                        offset += l;
                                        left -= l;
                                    }
                                    code = gx_color_load(pdcolor, &gs_gstate,
                                                         tdev);
                                    if (code < 0)
                                        goto out;
                                }
                                break;
                            default:
                                goto bad_op;
                        }
                        break;
                    default:
                        goto bad_op;
                }
                continue;
            case cmd_op_segment >> 4:
                {
                    int i;
                    static const byte op_num_operands[] = {
                        cmd_segment_op_num_operands_values
                    };
                  rgapto:
                    if (!in_path) {
                        ppos.x = int2fixed(state.rect.x);
                        ppos.y = int2fixed(state.rect.y);
                        if_debug2m('L', mem, " (%d,%d)", state.rect.x,
                                   state.rect.y);
                        notes = sn_none;
                        in_path = true;
                    }
                    for (i = 0; i < op_num_operands[op & 0xf]; ++i) {
                        fixed v;
                        int b = *cbp;

                        switch (b >> 5) {
                            case 0:
                            case 1:
                                vs[i++] =
                                    ((fixed) ((b ^ 0x20) - 0x20) << 13) +
                                    ((int)cbp[1] << 5) + (cbp[2] >> 3);
                                if_debug1m('L', mem, " %g", fixed2float(vs[i - 1]));
                                cbp += 2;
                                v = (int)((*cbp & 7) ^ 4) - 4;
                                break;
                            case 2:
                            case 3:
                                v = (b ^ 0x60) - 0x20;
                                break;
                            case 4:
                            case 5:
                                /*
                                 * Without the following cast, C's
                                 * brain-damaged coercion rules cause the
                                 * result to be considered unsigned, and not
                                 * sign-extended on machines where
                                 * sizeof(long) > sizeof(int).
                                 */
                                v = (((b ^ 0xa0) - 0x20) << 8) + (int)*++cbp;
                                break;
                            case 6:
                                v = (b ^ 0xd0) - 0x10;
                                vs[i] =
                                    ((v << 8) + cbp[1]) << (_fixed_shift - 2);
                                if_debug1m('L', mem, " %g", fixed2float(vs[i]));
                                cbp += 2;
                                continue;
                            default /*case 7 */ :
                                v = (int)(*++cbp ^ 0x80) - 0x80;
                                for (b = 0; b < sizeof(fixed) - 3; ++b)
                                    v = (v << 8) + *++cbp;
                                break;
                        }
                        cbp += 3;
                        /* Absent the cast in the next statement, */
                        /* the Borland C++ 4.5 compiler incorrectly */
                        /* sign-extends the result of the shift. */
                        vs[i] = (v << 16) + (uint) (cbp[-2] << 8) + cbp[-1];
                        if_debug1m('L', mem, " %g", fixed2float(vs[i]));
                    }
                    if_debug0m('L', mem, "\n");
                    code = clist_decode_segment(&path, op, vs, &ppos,
                                                x0, y0, notes);
                    if (code < 0)
                        goto out;
                }
                continue;
            case cmd_op_path >> 4:
                {
                    gx_path fpath;
                    gx_path *ppath;

                    if (op == cmd_opv_rgapto)
                        goto rgapto;

                    ppath = &path;

                    if_debug0m('L', mem, "\n");
                    /* if in clip, flatten path first */
                    if (in_clip) {
                        gx_path_init_local(&fpath, mem);
                        code = gx_path_add_flattened_accurate(&path, &fpath,
                                             gs_currentflat_inline(&gs_gstate),
                                             gs_gstate.accurate_curves);
                        if (code < 0)
                            goto out;
                        ppath = &fpath;
                    }
                    switch (op) {
                        case cmd_opv_fill:
                            fill_params.rule = gx_rule_winding_number;
                            goto fill;
                        case cmd_opv_eofill:
                            fill_params.rule = gx_rule_even_odd;
                        fill:
                            fill_params.adjust = gs_gstate.fill_adjust;
                            fill_params.flatness = gs_gstate.flatness;
                            code = (*dev_proc(tdev, fill_path))(tdev, &gs_gstate, ppath,
                                                                &fill_params, &fill_color, pcpath);
                            break;
                        case cmd_opv_fill_stroke:
                            fill_params.rule = gx_rule_winding_number;
                            goto fill_stroke;
                        case cmd_opv_eofill_stroke:
                            fill_params.rule = gx_rule_even_odd;
                        fill_stroke:
                            fill_params.adjust = gs_gstate.fill_adjust;
                            fill_params.flatness = gs_gstate.flatness;
                            stroke_params.flatness = gs_gstate.flatness;
                            stroke_params.traditional = false;
                            code = (*dev_proc(tdev, fill_stroke_path))(tdev, &gs_gstate, ppath,
                                                                &fill_params, &fill_color,
                                                                &stroke_params, &stroke_color, pcpath);
                            /* if the color is a pattern, it may have had the "is_locked" flag set	*/
                            /* clear those now (see do_fill_stroke).					*/
                            if (gx_dc_is_pattern1_color(&stroke_color)) {
                                gs_id id = stroke_color.colors.pattern.p_tile->id;

                                code = gx_pattern_cache_entry_set_lock(&gs_gstate, id, false);
                                if (code < 0)
                                    return code;	/* unlock failed -- should not happen */
                            }
                            if (gx_dc_is_pattern1_color(&fill_color)) {
                                gs_id id = fill_color.colors.pattern.p_tile->id;

                                code = gx_pattern_cache_entry_set_lock(&gs_gstate, id, false);
                                if (code < 0)
                                    return code;	/* unlock failed -- should not happen */
                            }
                            break;
                        case cmd_opv_stroke:
                            stroke_params.flatness = gs_gstate.flatness;
                            stroke_params.traditional = false;
                            code = (*dev_proc(tdev, stroke_path))
                                                       (tdev, &gs_gstate,
                                                       ppath, &stroke_params,
                                                       &stroke_color, pcpath);
                            break;
                        case cmd_opv_polyfill:
                            code = clist_do_polyfill(tdev, ppath, &fill_color,
                                                     gs_gstate.log_op);
                            break;
                        case cmd_opv_fill_trapezoid:
                            {
                                gs_fixed_edge left, right;
                                fixed ybot, ytop;
                                int options, swap_axes, wh;
                                fixed x0f;
                                fixed y0f;
                                gx_device *ttdev = tdev;

                                if (pcpath != NULL && !clipper_dev_open) {
                                    gx_make_clip_device_on_stack(&clipper_dev, pcpath, tdev);
                                    clipper_dev_open = true;
                                }
                                if (clipper_dev_open)
                                    ttdev = (gx_device *)&clipper_dev;
                                /* Note that if we have transparency present, the clipper device may need to have
                                   its color information updated to be synced up with the target device.
                                   This can occur if we had fills of a path first with a transparency mask to get
                                   an XPS opacity followed by a fill with a transparency group. This occurs in
                                   the XPS gradient code */
                                if (tdev->color_info.num_components != ttdev->color_info.num_components){
                                    /* Reset the clipper device color information. Only worry about
                                       the information that is used in the trap code */
                                    ttdev->color_info.num_components = tdev->color_info.num_components;
                                    ttdev->color_info.depth = tdev->color_info.depth;
                                    ttdev->color_info.polarity = tdev->color_info.polarity;
                                    memcpy(&(ttdev->color_info.comp_bits),&(tdev->color_info.comp_bits),GX_DEVICE_COLOR_MAX_COMPONENTS);
                                    memcpy(&(ttdev->color_info.comp_shift),&(tdev->color_info.comp_shift),GX_DEVICE_COLOR_MAX_COMPONENTS);
                                }
                                cmd_getw(left.start.x, cbp);
                                cmd_getw(left.start.y, cbp);
                                cmd_getw(left.end.x, cbp);
                                cmd_getw(left.end.y, cbp);
                                cmd_getw(right.start.x, cbp);
                                cmd_getw(right.start.y, cbp);
                                cmd_getw(right.end.x, cbp);
                                cmd_getw(right.end.y, cbp);
                                cmd_getw(options, cbp);
                                if (!(options & 4)) {
                                    cmd_getw(ybot, cbp);
                                    cmd_getw(ytop, cbp);
                                } else
                                    ytop = ybot = 0; /* Unused, but quiet gcc warning. */
                                swap_axes = options & 1;
                                wh = swap_axes ? tdev->width : tdev->height;
                                x0f = int2fixed(swap_axes ? y0 : x0);
                                y0f = int2fixed(swap_axes ? x0 : y0);
                                left.start.x -= x0f;
                                left.start.y -= y0f;
                                left.end.x -= x0f;
                                left.end.y -= y0f;
                                right.start.x -= x0f;
                                right.start.y -= y0f;
                                right.end.x -= x0f;
                                right.end.y -= y0f;
                                if (options & 2) {
                                    uchar num_components = tdev->color_info.num_components;
                                    frac31 c[4][GX_DEVICE_COLOR_MAX_COMPONENTS], *cc[4];
                                    byte colors_mask, i, j, m = 1;
                                    gs_fill_attributes fa;
                                    gs_fixed_rect clip;
                                    fixed hh = int2fixed(swap_axes ? target->width : target->height);

                                    if (cbuf.end - cbp < 5 * cmd_max_intsize(sizeof(frac31))) {
                                        code = top_up_cbuf(&cbuf, &cbp);
                                        if (code < 0)
                                            goto top_up_failed;
                                    }
                                    cmd_getw(clip.p.x, cbp);
                                    cmd_getw(clip.p.y, cbp);
                                    cmd_getw(clip.q.x, cbp);
                                    cmd_getw(clip.q.y, cbp);
                                    clip.p.x -= x0f;
                                    clip.p.y -= y0f;
                                    clip.q.x -= x0f;
                                    clip.q.y -= y0f;
                                    if (clip.p.y < 0)
                                        clip.p.y = 0;
                                    if (clip.q.y > hh)
                                        clip.q.y = hh;
                                    fa.clip = &clip;
                                    fa.swap_axes = swap_axes;
                                    fa.ht = NULL;
                                    fa.lop = lop_default; /* fgixme: gs_gstate.log_op; */
                                    fa.ystart = ybot - y0f;
                                    fa.yend = ytop - y0f;
                                    cmd_getw(colors_mask, cbp);
                                    for (i = 0; i < 4; i++, m <<= 1) {
                                        if (colors_mask & m) {
                                            if (cbuf.end - cbp < num_components * cmd_max_intsize(sizeof(frac31))) {
                                                code = top_up_cbuf(&cbuf, &cbp);
                                                if (code < 0)
                                                    goto top_up_failed;
                                            }
                                            cc[i] = c[i];
                                            for (j = 0; j < num_components; j++)
                                                cmd_getfrac(c[i][j], cbp);
                                        } else
                                            cc[i] = NULL;
                                    }
                                    if (options & 4) {
#                                       if 1 /* Disable to debug gx_fill_triangle_small. */
                                        code = dev_proc(ttdev, fill_linear_color_triangle)(ttdev, &fa,
                                                        &left.start, &left.end, &right.start,
                                                        cc[0], cc[1], cc[2]);
#                                       else
                                        code = 0;
#                                       endif
                                        if (code == 0) {
                                            /* The target device didn't fill the trapezoid and
                                               requests a decomposition. Subdivide into smaller triangles : */
                                            if (pfs.dev == NULL)
                                                code = gx_init_patch_fill_state_for_clist(tdev, &pfs, mem);
                                            if (code >= 0) {
                                                pfs.dev = ttdev;
                                                pfs.rect = clip; /* fixme: eliminate 'clip'. */
                                                fa.pfs = &pfs;
                                                code = gx_fill_triangle_small(ttdev, &fa,
                                                            &left.start, &left.end, &right.start,
                                                            cc[0], cc[1], cc[2]);
                                            }
                                        }
                                    } else {
                                        code = dev_proc(ttdev, fill_linear_color_trapezoid)(ttdev, &fa,
                                                        &left.start, &left.end, &right.start, &right.end,
                                                        cc[0], cc[1], cc[2], cc[3]);
                                        if (code == 0) {
                                            /* Fixme : The target device didn't fill the trapezoid and
                                               requests a decomposition.
                                               Currently we never call it with 4 colors (see gxshade6.c)
                                               and 2 colors must not return 0 - see comment to
                                               dev_t_proc_fill_linear_color_trapezoid in gxdevcli.c .
                                               Must not happen. */
                                            code = gs_note_error(gs_error_unregistered);
                                        }
                                    }
                                } else
                                    code = gx_default_fill_trapezoid(ttdev, &left, &right,
                                        max(ybot - y0f, fixed_half),
                                        min(ytop - y0f, int2fixed(wh)), swap_axes,
                                        &fill_color, gs_gstate.log_op);
                            }
                           break;
                        default:
                            goto bad_op;
                    }
                    if (ppath != &path)
                        gx_path_free(ppath, "clist_render_band");
                }
                if (in_path) {  /* path might be empty! */
                    state.rect.x = fixed2int_var(ppos.x);
                    state.rect.y = fixed2int_var(ppos.y);
                    in_path = false;
                }
                gx_path_free(&path, "clist_render_band");
                gx_path_init_local(&path, mem);
                if (code < 0)
                    goto out;
                continue;
            default:
              bad_op:mlprintf5(mem, "Bad op %02x band y0 = %d file pos %"PRId64" buf pos %d/%d\n",
                 op, y0, stell(s), (int)(cbp - cbuf.data), (int)(cbuf.end - cbuf.data));
                {
                    const byte *pp;

                    for (pp = cbuf.data; pp < cbuf.end; pp += 10) {
                        dmlprintf1(mem, "%4d:", (int)(pp - cbuf.data));
                        dmprintf10(mem, " %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
                                  pp[0], pp[1], pp[2], pp[3], pp[4],
                                  pp[5], pp[6], pp[7], pp[8], pp[9]);
                    }
                }
                code = gs_note_error(gs_error_Fatal);
                goto out;
        }
        if_debug4m('L', mem, " x=%d y=%d w=%d h=%d\n",
                  state.rect.x, state.rect.y, state.rect.width,
                  state.rect.height);
        switch (op >> 4) {
            case cmd_op_fill_rect >> 4:
                if (state.rect.width == 0 && state.rect.height == 0 &&
                    state.rect.x == 0 && state.rect.y == 0) {
                    /* FIXME: This test should be unnecessary. Bug 692076
                     * is open pending a proper fix. */
                    code = (dev_proc(tdev, fillpage) == NULL ? 0 :
                            (*dev_proc(tdev, fillpage))(tdev, &gs_gstate,
                                                        &fill_color));
                    break;
                }
            case cmd_op_fill_rect_short >> 4:
            case cmd_op_fill_rect_tiny >> 4:
                if (!state.lop_enabled) {
                    code = (*dev_proc(tdev, fill_rectangle))
                        (tdev, state.rect.x - x0, state.rect.y - y0,
                         state.rect.width, state.rect.height,
                         state.colors[1]);
                    break;
                }
                source = NULL;
                data_x = 0;
                raster = 0;
                colors[0] = colors[1] = state.colors[1];
                log_op = state.lop;
                pcolor = colors;
         do_rop:if (plane_height == 0) {
                    code = (*dev_proc(tdev, strip_copy_rop))
                                (tdev, source, data_x, raster, gx_no_bitmap_id,
                                 pcolor, &state_tile,
                                 (state.tile_colors[0] == gx_no_color_index &&
                                  state.tile_colors[1] == gx_no_color_index ?
                                  NULL : state.tile_colors),
                                 state.rect.x - x0, state.rect.y - y0,
                                 state.rect.width - data_x, state.rect.height,
                                 tile_phase.x, tile_phase.y, log_op);
                } else {
                    code = (*dev_proc(tdev, strip_copy_rop2))
                                (tdev, source, data_x, raster, gx_no_bitmap_id,
                                 pcolor, &state_tile,
                                 (state.tile_colors[0] == gx_no_color_index &&
                                  state.tile_colors[1] == gx_no_color_index ?
                                  NULL : state.tile_colors),
                                 state.rect.x - x0, state.rect.y - y0,
                                 state.rect.width - data_x, state.rect.height,
                                 tile_phase.x, tile_phase.y, log_op,
                                 plane_height);
                     plane_height = 0;
                }
                data_x = 0;
                break;
            case cmd_op_tile_rect >> 4:
                if (state.rect.width == 0 && state.rect.height == 0 &&
                    state.rect.x == 0 && state.rect.y == 0) {
                    code = (*dev_proc(tdev, fillpage))(tdev, &gs_gstate, &fill_color);
                    break;
                }
            case cmd_op_tile_rect_short >> 4:
            case cmd_op_tile_rect_tiny >> 4:
                /* Currently we don't use lop with tile_rectangle. */
                code = (*dev_proc(tdev, strip_tile_rectangle))
                    (tdev, &state_tile,
                     state.rect.x - x0, state.rect.y - y0,
                     state.rect.width, state.rect.height,
                     state.tile_colors[0], state.tile_colors[1],
                     tile_phase.x, tile_phase.y);
                break;
            case cmd_op_copy_mono_planes >> 4:
                if (state.lop_enabled) {
                    pcolor = state.colors;
                    log_op = state.lop;
                    goto do_rop;
                }
                if ((op & cmd_copy_use_tile) || pcpath != NULL) {       /*
                                                                         * This call of copy_mono originated as a call
                                                                         * of fill_mask.
                                                                         */
                    code = gx_image_fill_masked
                        (tdev, source, data_x, raster, gx_no_bitmap_id,
                         state.rect.x - x0, state.rect.y - y0,
                         state.rect.width - data_x, state.rect.height,
                         &fill_color, 1, gs_gstate.log_op, pcpath);
                } else {
                    if (plane_height == 0) {
                        code = (*dev_proc(tdev, copy_mono))
                             (tdev, source, data_x, raster, gx_no_bitmap_id,
                              state.rect.x - x0, state.rect.y - y0,
                              state.rect.width - data_x, state.rect.height,
                              state.colors[0], state.colors[1]);
                    } else {
                        code = (*dev_proc(tdev, copy_planes))
                             (tdev, source, data_x, raster, gx_no_bitmap_id,
                              state.rect.x - x0, state.rect.y - y0,
                              state.rect.width - data_x, state.rect.height,
                              plane_height);
                    }
                }
                plane_height = 0;
                data_x = 0;
                break;
            case cmd_op_copy_color_alpha >> 4:
                if (state.color_is_alpha) {
/****** CAN'T DO ROP WITH ALPHA ******/
                    if (state.color_is_devn &&
                        dev_proc(tdev, copy_alpha_hl_color) != gx_default_no_copy_alpha_hl_color) { /* FIXME */
                        code = (*dev_proc(tdev, copy_alpha_hl_color))
                            (tdev, source, data_x, raster, gx_no_bitmap_id,
                             state.rect.x - x0, state.rect.y - y0,
                             state.rect.width - data_x, state.rect.height,
                             &fill_color, depth);
                    } else {
                        code = (*dev_proc(tdev, copy_alpha))
                            (tdev, source, data_x, raster, gx_no_bitmap_id,
                             state.rect.x - x0, state.rect.y - y0,
                             state.rect.width - data_x, state.rect.height,
                             state.colors[1], depth);
                    }
                } else {
                    if (state.lop_enabled) {
                        pcolor = NULL;
                        log_op = state.lop;
                        goto do_rop;
                    }
                    code = (*dev_proc(tdev, copy_color))
                        (tdev, source, data_x, raster, gx_no_bitmap_id,
                         state.rect.x - x0, state.rect.y - y0,
                         state.rect.width - data_x, state.rect.height);
                }
                data_x = 0;
                break;
            default:            /* can't happen */
                goto bad_op;
        }
    }
    /* Clean up before we exit. */
  out:
    if (ht_buff.pbuff != 0) {
        gs_free_object(mem, ht_buff.pbuff, "clist_playback_band(ht_buff)");
        ht_buff.pbuff = 0;
        ht_buff.pcurr = 0;
    }
    ht_buff.ht_size = 0;
    ht_buff.read_size = 0;

    if (pcomp_last != NULL) {
        int code1 = drop_compositor_queue(&pcomp_first, &pcomp_last, NULL, mem, x0, y0, &gs_gstate);

        if (code == 0)
            code = code1;
    }
    gx_cpath_free(&clip_path, "clist_render_band exit");
    gx_path_free(&path, "clist_render_band exit");
    if (gs_gstate.pattern_cache != NULL) {
        gx_pattern_cache_free(gs_gstate.pattern_cache);
        gs_gstate.pattern_cache = NULL;
    }
    /* The imager state release will decrement the icc link cache.  To avoid
       race conditions lock the cache */
    gx_monitor_enter(cdev->icc_cache_cl->lock);
    gs_gstate_release(&gs_gstate);
    gx_monitor_leave(cdev->icc_cache_cl->lock); /* done with increment, let everyone run */
    gs_free_object(mem, data_bits, "clist_playback_band(data_bits)");
    if (target != orig_target) {
        if (target->rc.ref_count != 1) {
            /* This can occur if we are coming from a pattern clist that
               includes transparency.  In this case, we do not want to
               free the compositor since it is really the main target that
               we are tiling into.  i.e. the tile itself does not have
               a pdf14 device, but rather we push a trans group, draw and
               then pop the group to properly blend */
            rc_decrement(target, "gxclrast(target compositor)");
        } else {
            /* Ref count was 1. close the device and then free it */
            if (target->is_open)
                dev_proc(target, close_device)(target);
            gs_free_object(target->memory, target, "gxclrast discard compositor");
        }
        target = orig_target;
    }
    if (code < 0) {
        if (pfs.dev != NULL)
            term_patch_fill_state(&pfs);
        gx_cpath_free(&clip_path, "clist_playback_band");
        if (pcpath != &clip_path)
            gx_cpath_free(pcpath, "clist_playback_band");
        return_error(code);
    }
    /* Check whether we have more pages to process. */
    if ((playback_action != playback_action_setup &&
        (cbp < cbuf.end || !seofp(s)) && (op != cmd_opv_end_page) )
        )
        goto in;
    if (pfs.dev != NULL)
        term_patch_fill_state(&pfs);
    gs_free_object(mem, pcs, "clist_playback_band(pcs)");
    gs_free_object(mem, cbuf_storage, "clist_playback_band(cbuf_storage)");
    gx_cpath_free(&clip_path, "clist_playback_band");
    if (pcpath != &clip_path)
        gx_cpath_free(pcpath, "clist_playback_band");
    return code;
top_up_failed:
    gx_cpath_free(&clip_path, "clist_playback_band");
    if (pcpath != &clip_path)
        gx_cpath_free(pcpath, "clist_playback_band");
    return code;
}

/* ---------------- Individual commands ---------------- */

/*
 * These single-use procedures implement a few large individual commands,
 * primarily for readability but also to avoid overflowing compilers'
 * optimization limits.  They all take the command buffer as their first
 * parameter (pcb), assume that the current buffer pointer is in pcb->ptr,
 * and update it there.
 */

static int
read_set_tile_size(command_buf_t *pcb, tile_slot *bits, bool for_pattern)
{
    const byte *cbp = pcb->ptr;
    uint rep_width, rep_height;
    uint pdepth;
    byte bd = *cbp++;

    bits->cb_depth = cmd_code_to_depth(bd);
    if (for_pattern)
        cmd_getw(bits->id, cbp);
    cmd_getw(rep_width, cbp);
    cmd_getw(rep_height, cbp);
    if (bd & 0x20) {
        cmd_getw(bits->x_reps, cbp);
        bits->width = rep_width * bits->x_reps;
    } else {
        bits->x_reps = 1;
        bits->width = rep_width;
    }
    if (bd & 0x40) {
        cmd_getw(bits->y_reps, cbp);
        bits->height = rep_height * bits->y_reps;
    } else {
        bits->y_reps = 1;
        bits->height = rep_height;
    }
    if (bd & 0x80)
        cmd_getw(bits->rep_shift, cbp);
    else
        bits->rep_shift = 0;
    if (bd & 0x10)
        bits->num_planes = *cbp++;
    else
        bits->num_planes = 1;
    if_debug7('L', " depth=%d size=(%d,%d), rep_size=(%d,%d), rep_shift=%d, num_planes=%d\n",
              bits->cb_depth, bits->width,
              bits->height, rep_width,
              rep_height, bits->rep_shift, bits->num_planes);
    bits->shift =
        (bits->rep_shift == 0 ? 0 :
         (bits->rep_shift * (bits->height / rep_height)) % rep_width);
    pdepth = bits->cb_depth;
    if (bits->num_planes != 1)
        pdepth /= bits->num_planes;
    bits->cb_raster = bitmap_raster(bits->width * pdepth);
    pcb->ptr = cbp;
    return 0;
}

static int
read_set_bits(command_buf_t *pcb, tile_slot *bits, int compress,
              gx_clist_state *pcls, gx_strip_bitmap *tile, tile_slot **pslot,
              gx_device_clist_reader *cdev, gs_memory_t *mem)
{
    const byte *cbp = pcb->ptr;
    uint rep_width = bits->width / bits->x_reps;
    uint rep_height = bits->height / bits->y_reps;
    uint index;
    ulong offset;
    uint width_bits;
    uint width_bytes;
    uint raster;
    uint bytes;
    byte *data;
    tile_slot *slot;
    uint depth = bits->cb_depth;

    if (bits->num_planes != 1)
        depth /= bits->num_planes;
    width_bits = rep_width * depth;

    bytes = clist_bitmap_bytes(width_bits, rep_height * bits->num_planes,
                               compress |
                               (rep_width < bits->width ?
                                decompress_spread : 0) |
                               decompress_elsewhere,
                               &width_bytes,
                               (uint *)&raster);

    cmd_getw(index, cbp);
    cmd_getw(offset, cbp);
    if_debug2m('L', mem, " index=%d offset=%lu\n", index, offset);
    pcls->tile_index = index;
    cdev->tile_table[pcls->tile_index].offset = offset;
    slot = (tile_slot *)(cdev->cache_chunk->data + offset);
    *pslot = slot;
    *slot = *bits;
    tile->data = data = (byte *)(slot + 1);
#ifdef DEBUG
    slot->index = pcls->tile_index;
#endif
    if (compress == cmd_compress_const) {
        cbp = cmd_read_data(pcb, data, 1, cbp);
        if (width_bytes > 0 && rep_height > 0)
            memset(data+1, *data, width_bytes * rep_height - 1);
    } else if (compress) {
        /*
         * Decompress the image data.  We'd like to share this code with the
         * similar code in copy_*, but right now we don't see how.
         */
        stream_cursor_read r;
        stream_cursor_write w;
        /*
         * We don't know the data length a priori, so to be conservative, we
         * read the uncompressed size.
         */
        uint cleft = pcb->end - cbp;

        if (cleft < bytes && !pcb->end_status) {
            uint nread = cbuf_size - cleft;
#   ifdef DEBUG
            stream_state *st = pcb->s->state;
#   endif

#           ifdef DEBUG
            {
                int code = top_up_offset_map(st, pcb->data, cbp, pcb->end);

                if (code < 0)
                    return code;
            }
#           endif
            memmove(pcb->data, cbp, cleft);
            pcb->end_status = sgets(pcb->s, pcb->data + cleft, nread, &nread);
            set_cb_end(pcb, pcb->data + cleft + nread);
            cbp = pcb->data;
        }
        r.ptr = cbp - 1;
        r.limit = pcb->end - 1;
        w.ptr = data - 1;
        w.limit = w.ptr + bytes;
        switch (compress) {
        case cmd_compress_rle:
            {
                stream_RLD_state sstate;

                clist_rld_init(&sstate);
                (*s_RLD_template.process)
                    ((stream_state *)&sstate, &r, &w, true);
            }
            break;
        case cmd_compress_cfe:
            {
                stream_CFD_state sstate;

                clist_cfd_init(&sstate,
                               width_bytes << 3 /*width_bits */ ,
                               rep_height, mem);
                (*s_CFD_template.process)
                    ((stream_state *)&sstate, &r, &w, true);
                (*s_CFD_template.release)
                    ((stream_state *)&sstate);
            }
            break;
        default:
            return_error(gs_error_unregistered);
        }
        cbp = r.ptr + 1;
    } else if (rep_height * bits->num_planes > 1 && width_bytes != bits->cb_raster) {
        cbp = cmd_read_short_bits(pcb, data, bytes,
                                  width_bytes, rep_height * bits->num_planes,
                                  bits->cb_raster, cbp);
    } else {
        cbp = cmd_read_data(pcb, data, bytes, cbp);
    }
    if (bits->width > rep_width)
        bits_replicate_horizontally(data,
                                    rep_width * depth, rep_height * bits->num_planes,
                                    bits->cb_raster,
                                    bits->width * depth,
                                    bits->cb_raster);
    if (bits->height > rep_height)
        bits_replicate_vertically(data,
                                  rep_height, bits->cb_raster,
                                  bits->height);
#ifdef DEBUG
    if (gs_debug_c('L'))
        cmd_print_bits(mem, data, bits->width, bits->height, bits->cb_raster);
#endif
    pcb->ptr = cbp;
    return 0;
}

/* if necessary, allocate a buffer to hold a serialized halftone */
static int
read_alloc_ht_buff(ht_buff_t * pht_buff, uint ht_size, gs_memory_t * mem)
{
    /* free the existing buffer, if any (usually none) */
    if (pht_buff->pbuff != 0) {
        gs_free_object(mem, pht_buff->pbuff, "read_alloc_ht_buff");
        pht_buff->pbuff = 0;
    }

    /*
     * If the serialized halftone fits in the command buffer, no
     * additional buffer is required.
     */
    if (ht_size > cbuf_ht_seg_max_size) {
        pht_buff->pbuff = gs_alloc_bytes(mem, ht_size, "read_alloc_ht_buff");
        if (pht_buff->pbuff == 0)
            return_error(gs_error_VMerror);
    }
    pht_buff->ht_size = ht_size;
    pht_buff->read_size = 0;
    pht_buff->pcurr = pht_buff->pbuff;
    return 0;
}

/* read a halftone segment; if it is the final segment, build the halftone */
static int
read_ht_segment(
    ht_buff_t *                 pht_buff,
    command_buf_t *             pcb,
    gs_gstate *           pgs,
    gx_device *                 dev,
    gs_memory_t *               mem )
{
    const byte *                cbp = pcb->ptr;
    const byte *                pbuff = 0;
    uint                        ht_size = pht_buff->ht_size, seg_size;
    int                         code = 0;

    /* get the segment size; refill command buffer if necessary */
    enc_u_getw(seg_size, cbp);
    if (pcb->warn_limit - cbp < (int)seg_size) { /* cbp can be past warn_limit */
        code = top_up_cbuf(pcb, &cbp);
        if (code < 0)
            return code;
        if (pcb->end - cbp < (int)seg_size) {
            emprintf(mem, " *** ht segment size doesn't fit in buffer ***\n");
            return_error(gs_error_unknownerror);
        }
    }

    if (pht_buff->pbuff == 0) {
        /* if not separate buffer, must be only one segment */
        if (seg_size != ht_size)
            return_error(gs_error_unknownerror);
        pbuff = cbp;
    } else {
        if (seg_size + pht_buff->read_size > pht_buff->ht_size)
            return_error(gs_error_unknownerror);
        memcpy(pht_buff->pcurr, cbp, seg_size);
        pht_buff->pcurr += seg_size;
        if ((pht_buff->read_size += seg_size) == ht_size)
            pbuff = pht_buff->pbuff;
    }

    /* if everything has been read, convert back to a halftone */
    if (pbuff != 0) {
        code = gx_ht_read_and_install(pgs, dev, pbuff, ht_size, mem);

        /* release any buffered information */
        if (pht_buff->pbuff != 0) {
            gs_free_object(mem, pht_buff->pbuff, "read_alloc_ht_buff");
            pht_buff->pbuff = 0;
            pht_buff->pcurr = 0;
        }
        pht_buff->ht_size = 0;
        pht_buff->read_size = 0;
    }

    /* update the command buffer ponter */
    pcb->ptr = cbp + seg_size;

    return code;
}

static int
read_set_misc2(command_buf_t *pcb, gs_gstate *pgs, segment_notes *pnotes)
{
    const byte *cbp = pcb->ptr;
    uint mask, cb;

    cmd_getw(mask, cbp);
    if (mask & cap_join_known) {
        cb = *cbp++;
        pgs->line_params.start_cap = (gs_line_cap)((cb >> 3) & 7);
        pgs->line_params.join = (gs_line_join)(cb & 7);
        if_debug2m('L', pgs->memory, " start_cap=%d join=%d\n",
                   pgs->line_params.start_cap, pgs->line_params.join);
        cb = *cbp++;
        pgs->line_params.end_cap = (gs_line_cap)((cb >> 3) & 7);
        pgs->line_params.dash_cap = (gs_line_cap)(cb & 7);
        if_debug2m('L', pgs->memory, "end_cap=%d dash_cap=%d\n",
                   pgs->line_params.end_cap, pgs->line_params.dash_cap);
    }
    if (mask & cj_ac_sa_known) {
        cb = *cbp++;
        pgs->line_params.curve_join = ((cb >> 2) & 7) - 1;
        pgs->accurate_curves = (cb & 2) != 0;
        pgs->stroke_adjust = cb & 1;
        if_debug3m('L', pgs->memory, " CJ=%d AC=%d SA=%d\n",
                   pgs->line_params.curve_join, pgs->accurate_curves,
                   pgs->stroke_adjust);
    }
    if (mask & flatness_known) {
        cmd_get_value(pgs->flatness, cbp);
        if_debug1m('L', pgs->memory, " flatness=%g\n", pgs->flatness);
    }
    if (mask & line_width_known) {
        float width;

        cmd_get_value(width, cbp);
        if_debug1m('L', pgs->memory, " line_width=%g\n", width);
        gx_set_line_width(&pgs->line_params, width);
    }
    if (mask & miter_limit_known) {
        float limit;

        cmd_get_value(limit, cbp);
        if_debug1m('L', pgs->memory, " miter_limit=%g\n", limit);
        gx_set_miter_limit(&pgs->line_params, limit);
    }
    if (mask & op_bm_tk_known) {
        cb = *cbp++;
        pgs->blend_mode = cb >> 3;
        pgs->text_knockout = cb & 1;
        /* the following usually have no effect; see gxclpath.c */
        cb = *cbp++;
        pgs->overprint_mode = (cb >> 2) & 1;
        pgs->stroke_overprint = (cb >> 1) & 1;
        pgs->overprint = cb & 1;
        cb = *cbp++;
        pgs->renderingintent = cb;
        if_debug6m('L', pgs->memory, " BM=%d TK=%d OPM=%d OP=%d op=%d RI=%d\n",
                   pgs->blend_mode, pgs->text_knockout, pgs->overprint_mode,
                   pgs->stroke_overprint, pgs->overprint, pgs->renderingintent);
    }
    if (mask & segment_notes_known) {
        cb = *cbp++;
        *pnotes = (segment_notes)(cb & 0x3f);
        if_debug1m('L', pgs->memory, " notes=%d\n", *pnotes);
    }
    if (mask & opacity_alpha_known) {
        cmd_get_value(pgs->opacity.alpha, cbp);
        if_debug1m('L', pgs->memory, " opacity.alpha=%g\n", pgs->opacity.alpha);
    }
    if (mask & shape_alpha_known) {
        cmd_get_value(pgs->shape.alpha, cbp);
        if_debug1m('L', pgs->memory, " shape.alpha=%g\n", pgs->shape.alpha);
    }
    if (mask & alpha_known) {
        cmd_get_value(pgs->alpha, cbp);
        if_debug1m('L', pgs->memory, " alpha=%u\n", pgs->alpha);
    }
    pcb->ptr = cbp;
    return 0;
}

static int
read_set_color_space(command_buf_t *pcb, gs_gstate *pgs,
                     gx_device_clist_reader *cdev, gs_memory_t *mem)
{
    const byte *cbp = pcb->ptr;
    byte b = *cbp++;
    int index = b >> 4;
    gs_color_space *pcs;
    int code = 0;
    cmm_profile_t *picc_profile;
    clist_icc_color_t icc_information;

    if_debug3m('L', mem, " %d%s%s\n", index,
               (b & 8 ? " (indexed)" : ""),
               (b & 4 ? "(proc)" : ""));
    /* They all store the ICC information.  Even if it is NULL
       it is used in the ICC case to avoid reading from the
       serialized profile data which is stored elsewhere in the
       clist.  Hence we avoid jumping around in the file. */
    memcpy(&icc_information, cbp, sizeof(clist_icc_color_t));
    cbp = cbp + sizeof(clist_icc_color_t);
    switch (index) {
    case gs_color_space_index_DeviceGray:
        pcs = gs_cspace_new_DeviceGray(mem);
        break;
    case gs_color_space_index_DeviceRGB:
        pcs = gs_cspace_new_DeviceRGB(mem);
        break;
    case gs_color_space_index_DeviceCMYK:
        pcs = gs_cspace_new_DeviceCMYK(mem);
        break;
    case gs_color_space_index_ICC:
        /* build the color space object */
        code = gs_cspace_build_ICC(&pcs, NULL, mem);
        /* Don't bother getting the ICC stuff from the clist yet */
        picc_profile = gsicc_profile_new(NULL, cdev->memory, NULL, 0);
        if (picc_profile == NULL)
            return gs_rethrow(-1, "Failed to find ICC profile during clist read");
        picc_profile->num_comps = icc_information.icc_num_components;
        picc_profile->hashcode = icc_information.icc_hash;
        picc_profile->hash_is_valid = true;
        picc_profile->islab = icc_information.is_lab;
        picc_profile->default_match = icc_information.default_match;
        picc_profile->data_cs = icc_information.data_cs;
        /* Store the clist reader address in the profile
           structure so that we can get to the buffer
           data if we really neeed it.  Ideally, we
           will use a cached link and only access this once. */
        picc_profile->dev = (gx_device*) cdev;
        /* Assign it to the colorspace */
        code = gsicc_set_gscs_profile(pcs, picc_profile, mem);
        /* And we no longer need our reference to the profile */
        gsicc_adjust_profile_rc(picc_profile, -1, "read_set_color_space");
        break;
    default:
        code = gs_note_error(gs_error_rangecheck);      /* others are NYI */
        goto out;
    }

    if (pcs == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto out;
    }

    if (b & 8) {
        bool use_proc = (b & 4) != 0;
        int hival;
        int num_values;
        byte *data;
        uint data_size;
        gs_color_space *pcs_indexed;

        pcs_indexed = gs_cspace_alloc(mem, &gs_color_space_type_Indexed);
        if (pcs_indexed == 0) {
            rc_decrement_cs(pcs, "read_set_color_space");
            code = gs_note_error(gs_error_VMerror);
            goto out;
        }
        pcs_indexed->base_space = pcs;
        pcs = pcs_indexed;
        pcs->params.indexed.use_proc = 0;
        pcs->params.indexed.lookup.table.data = 0;
        pcs->params.indexed.lookup.table.size = 0;
        cmd_getw(hival, cbp);
        pcs->params.indexed.n_comps = gs_color_space_num_components(pcs->base_space);
        num_values = (hival + 1) * pcs->params.indexed.n_comps;
        if (use_proc) {
            gs_indexed_map *map;

            code = alloc_indexed_map(&map, num_values, mem, "indexed map");
            if (code < 0) {
                rc_decrement_cs(pcs, "read_set_color_space");
                goto out;
            }
            map->proc.lookup_index = lookup_indexed_map;
            pcs->params.indexed.lookup.map = map;
            data = (byte *)map->values;
            data_size = num_values * sizeof(map->values[0]);
        } else {
            byte *table = gs_alloc_string(mem, num_values, "color_space indexed table");

            if (table == 0) {
                code = gs_note_error(gs_error_VMerror);
                rc_decrement_cs(pcs, "read_set_color_space");
                goto out;
            }
            pcs->params.indexed.lookup.table.data = table;
            pcs->params.indexed.lookup.table.size = num_values;
            data = table;
            data_size = num_values;
        }
        cbp = cmd_read_data(pcb, data, data_size, cbp);
        pcs->params.indexed.hival = hival;
        pcs->params.indexed.use_proc = use_proc;
    }

    /* Release reference to old color space before installing new one. */
    if (pgs->color[0].color_space != NULL)
        rc_decrement_only_cs(pgs->color[0].color_space, "read_set_color_space");
    pgs->color[0].color_space = pcs;
out:
    pcb->ptr = cbp;
    return code;
}

static int
read_begin_image(command_buf_t *pcb, gs_image_common_t *pic,
                 gs_color_space *pcs)
{
    uint index = *(pcb->ptr)++;
    const gx_image_type_t *image_type = gx_image_type_table[index];
    stream s;
    int code;

    /* This is sloppy, but we don't have enough information to do better. */
    code = top_up_cbuf(pcb, &pcb->ptr);
    if (code < 0)
        return code;
    s_init(&s, NULL);
    sread_string(&s, pcb->ptr, pcb->end - pcb->ptr);
    code = image_type->sget(pic, &s, pcs);
    pcb->ptr = sbufptr(&s);
    return code;
}

static int
read_put_params(command_buf_t *pcb, gs_gstate *pgs,
                gx_device_clist_reader *cdev, gs_memory_t *mem)
{
    const byte *cbp = pcb->ptr;
    gs_c_param_list param_list;
    uint cleft;
    uint rleft;
    bool alloc_data_on_heap = false;
    byte *param_buf;
    uint param_length;
    int code;

    cmd_get_value(param_length, cbp);
    if_debug1m('L', mem, " length=%d\n", param_length);
    if (param_length == 0) {
        code = 1;               /* empty list */
        goto out;
    }

    /* Make sure entire serialized param list is in cbuf */
    /* + force void* alignment */
    code = top_up_cbuf(pcb, &cbp);
    if (code < 0)
        return code;
    if (pcb->end - cbp >= param_length) {
        param_buf = (byte *)cbp;
        cbp += param_length;
    } else {
        /* NOTE: param_buf must be maximally aligned */
        param_buf = gs_alloc_bytes(mem, param_length,
                                   "clist put_params");
        if (param_buf == 0) {
            code = gs_note_error(gs_error_VMerror);
            goto out;
        }
        alloc_data_on_heap = true;
        cleft = pcb->end - cbp;
        rleft = param_length - cleft;
        memmove(param_buf, cbp, cleft);
        pcb->end_status = sgets(pcb->s, param_buf + cleft, rleft, &rleft);
        cbp = pcb->end;  /* force refill */
    }

    /*
     * Create a gs_c_param_list & expand into it.
     * NB that gs_c_param_list doesn't copy objects into
     * it, but rather keeps *pointers* to what's passed.
     * That's OK because the serialized format keeps enough
     * space to hold expanded versions of the structures,
     * but this means we cannot deallocate source buffer
     * until the gs_c_param_list is deleted.
     */
    gs_c_param_list_write(&param_list, mem);
    code = gs_param_list_unserialize
        ( (gs_param_list *)&param_list, param_buf );
    if (code >= 0 && code != param_length)
        code = gs_error_unknownerror;  /* must match */
    if (code >= 0) {
        gs_c_param_list_read(&param_list);
        code = gs_gstate_putdeviceparams(pgs, (gx_device *)cdev,
                                         (gs_param_list *)&param_list);
    }
    gs_c_param_list_release(&param_list);
    if (alloc_data_on_heap)
        gs_free_object(mem, param_buf, "clist put_params");

out:
    pcb->ptr = cbp;
    return code;
}

/*
 * Read a "create_compositor" command, and execute the command.
 *
 * This code assumes that a the largest create compositor command,
 * including the compositor name size, is smaller than the data buffer
 * size. This assumption is inherent in the basic design of the coding
 * and the de-serializer interface, as no length field is provided.
 *
 * At the time of this writing, no compositor violates this assumption.
 * The largest create_compositor is currently 1275 bytes, while the command
 * data buffer is 4096 bytes.
 *
 * In the event that this assumption is violated, a change in the encoding
 * would be called for.
 *
 * See comment in gdevp14.c c_pdf14trans_read PDF14_BEGIN_TRANS_MASK case.
 */
extern_gs_find_compositor();

static int
read_create_compositor(
    command_buf_t *pcb,  gs_memory_t *mem, gs_composite_t **ppcomp)
{
    const byte *                cbp = pcb->ptr;
    int                         comp_id = 0, code = 0;
    const gs_composite_type_t * pcomp_type = 0;

    /* fill the command buffer (see comment above) */
    if (pcb->end - cbp < MAX_CLIST_COMPOSITOR_SIZE + sizeof(comp_id)) {
        code = top_up_cbuf(pcb, &cbp);
        if (code < 0)
            return code;
    }

    /* find the appropriate compositor method vector */
    comp_id = *cbp++;
    if ((pcomp_type = gs_find_compositor(comp_id)) == 0)
        return_error(gs_error_unknownerror);

    /* de-serialize the compositor */
    code = pcomp_type->procs.read(ppcomp, cbp, pcb->end - cbp, mem);

    /* If we read more than the maximum expected, return a rangecheck error */
    if ( code > MAX_CLIST_COMPOSITOR_SIZE )
        return_error(gs_error_rangecheck);

    if (code > 0)
        cbp += code;
    pcb->ptr = cbp;
    return code;
}

static int apply_create_compositor(gx_device_clist_reader *cdev, gs_gstate *pgs,
                                   gs_memory_t *mem, gs_composite_t *pcomp,
                                   int x0, int y0, gx_device **ptarget)
{
    gx_device *tdev = *ptarget;
    int code;

    code = pcomp->type->procs.adjust_ctm(pcomp, x0, y0, pgs);
    if (code < 0)
        return code;
    /*
     * Apply the compositor to the target device; note that this may
     * change the target device.
     */
    code = dev_proc(tdev, create_compositor)(tdev, &tdev, pcomp, pgs, mem, (gx_device*) cdev);
    if (code >= 0 && tdev != *ptarget) {
        /* If we created a new compositor here, then that new compositor should
         * become the device to which we send all future drawing requests. If
         * the above create_compositor call found an existing compositor
         * already in the chain of devices (such as might happen when we are
         * playing back a clist based pattern, and the top device is a clip
         * device that forwards to a pdf14 device), then we'll just reuse
         * that one. We do not want to send new drawing operations to the
         * compositor, as that will sidestep the clipping. We therefore check
         * the reference count to see if this is a new device or not. */
        if (tdev->rc.ref_count == 1)
            *ptarget = tdev;
    }
    if (code < 0)
        return code;

    /* Perform any updates for the clist device required */
    code = pcomp->type->procs.clist_compositor_read_update(pcomp,
                                        (gx_device *)cdev, tdev, pgs, mem);
    if (code < 0)
        return code;

    /* free the compositor object */
    gs_free_object(mem, pcomp, "read_create_compositor");

    return code;
}

/* ---------------- Utilities ---------------- */

/* Read and unpack a short bitmap */
/*
 * The 'raster' in the dest buffer may be larger than the 'width_bytes'
 * in the src, so after reading we memmove data down to the proper
 * alignment from the last line backwards.
 * THIS RELIES on width_bytes <= raster to work.
 */
static const byte *
cmd_read_short_bits(command_buf_t *pcb, byte *data, int tot_bytes,
                    int width_bytes, int height, uint raster, const byte *cbp)
{
    /* Note the following may read from the file past the end of the buffer */
    /* leaving cbp at pcb->end. No further reading using cbp can be done    */
    /* without top_up_cbuf to reload the buffer.                            */
    cbp = cmd_read_data(pcb, data, tot_bytes, cbp);

    /* if needed, adjust buffer contents for dest raster > width_bytes */
    if (width_bytes < raster) {
        const byte *pdata = data /*src*/ + width_bytes * height;
        byte *udata = data /*dest*/ + height * raster;

        while (--height > 0) {	/* don't need to move the first line to itself */
            udata -= raster, pdata -= width_bytes;
            switch (width_bytes) {
                default:
                    memmove(udata, pdata, width_bytes);
                    break;
                case 6:
                    udata[5] = pdata[5];
                case 5:
                    udata[4] = pdata[4];
                case 4:
                    udata[3] = pdata[3];
                case 3:
                    udata[2] = pdata[2];
                case 2:
                    udata[1] = pdata[1];
                case 1:
                    udata[0] = pdata[0];
                case 0:;            /* shouldn't happen */
            }
        }
    }
    return cbp;
}

/* Read a rectangle. */
static const byte *
cmd_read_rect(int op, gx_cmd_rect * prect, const byte * cbp)
{
    cmd_getw(prect->x, cbp);
    if (op & 0xf)
        prect->y += ((op >> 2) & 3) - 2;
    else {
        cmd_getw(prect->y, cbp);
    }
    cmd_getw(prect->width, cbp);
    if (op & 0xf)
        prect->height += (op & 3) - 2;
    else {
        cmd_getw(prect->height, cbp);
    }
    return cbp;
}

/*
 * Select a map for loading with data.
 *
 * This routine has three outputs:
 *   *pmdata - points to the map data.
 *   *pcomp_num - points to a component number if the map is a transfer
 *               map which has been set via the setcolortransfer operator.
 *               A. value of NULL indicates that no component number is to
 *               be sent for this map.
 *   *pcount - the size of the map (in bytes).
 */
static int
cmd_select_map(cmd_map_index map_index, cmd_map_contents cont,
               gs_gstate * pgs, int ** pcomp_num, frac ** pmdata,
               uint * pcount, gs_memory_t * mem)
{
    gx_transfer_map *map;
    gx_transfer_map **pmap;
    const char *cname;

    *pcomp_num = NULL;          /* Only used for color transfer maps */
    switch (map_index) {
        case cmd_map_transfer:
            if_debug0m('L', mem, " transfer");
            rc_unshare_struct(pgs->set_transfer.gray, gx_transfer_map,
                &st_transfer_map, mem, return_error(gs_error_VMerror),
                "cmd_select_map(default_transfer)");
            map = pgs->set_transfer.gray;
            /* Release all current maps */
            rc_decrement(pgs->set_transfer.red, "cmd_select_map(red)");
            pgs->set_transfer.red = NULL;
            pgs->set_transfer.red_component_num = -1;
            rc_decrement(pgs->set_transfer.green, "cmd_select_map(green)");
            pgs->set_transfer.green = NULL;
            pgs->set_transfer.green_component_num = -1;
            rc_decrement(pgs->set_transfer.blue, "cmd_select_map(blue)");
            pgs->set_transfer.blue = NULL;
            pgs->set_transfer.blue_component_num = -1;
            goto transfer2;
        case cmd_map_transfer_0:
            pmap = &pgs->set_transfer.red;
            *pcomp_num = &pgs->set_transfer.red_component_num;
            goto transfer1;
        case cmd_map_transfer_1:
            pmap = &pgs->set_transfer.green;
            *pcomp_num = &pgs->set_transfer.green_component_num;
            goto transfer1;
        case cmd_map_transfer_2:
            pmap = &pgs->set_transfer.blue;
            *pcomp_num = &pgs->set_transfer.blue_component_num;
            goto transfer1;
        case cmd_map_transfer_3:
            pmap = &pgs->set_transfer.gray;
            *pcomp_num = &pgs->set_transfer.gray_component_num;
transfer1:  if_debug1m('L', mem, " transfer[%d]", (int)(map_index - cmd_map_transfer_0));
            rc_unshare_struct(*pmap, gx_transfer_map, &st_transfer_map, mem,
                return_error(gs_error_VMerror), "cmd_select_map(transfer)");
            map = *pmap;

transfer2:  if (cont != cmd_map_other) {
                gx_set_identity_transfer(map);
                *pmdata = 0;
                *pcount = 0;
                return 0;
            }
            break;
        case cmd_map_black_generation:
            if_debug0m('L', mem, " black generation");
            pmap = &pgs->black_generation;
            cname = "cmd_select_map(black generation)";
            goto alloc;
        case cmd_map_undercolor_removal:
            if_debug0m('L', mem, " undercolor removal");
            pmap = &pgs->undercolor_removal;
            cname = "cmd_select_map(undercolor removal)";
alloc:      if (cont == cmd_map_none) {
                rc_decrement(*pmap, cname);
                *pmap = 0;
                *pmdata = 0;
                *pcount = 0;
                return 0;
            }
            rc_unshare_struct(*pmap, gx_transfer_map, &st_transfer_map,
                              mem, return_error(gs_error_VMerror), cname);
            map = *pmap;
            if (cont == cmd_map_identity) {
                gx_set_identity_transfer(map);
                *pmdata = 0;
                *pcount = 0;
                return 0;
            }
            break;
        default:
            *pmdata = 0;
            return 0;
    }
    map->proc = gs_mapped_transfer;
    *pmdata = map->values;
    *pcount = sizeof(map->values);
    return 0;
}

/* Create a device halftone for the imager if necessary. */
static int
cmd_create_dev_ht(gx_device_halftone **ppdht, gs_memory_t *mem)
{
    gx_device_halftone *pdht = *ppdht;

    if (pdht == 0) {
        rc_header rc;

        rc_alloc_struct_1(pdht, gx_device_halftone, &st_device_halftone, mem,
                          return_error(gs_error_VMerror),
                          "cmd_create_dev_ht");
        rc = pdht->rc;
        memset(pdht, 0, sizeof(*pdht));
        pdht->rc = rc;
        *ppdht = pdht;
    }
    return 0;
}

/* Resize the halftone components array if necessary. */
static int
cmd_resize_halftone(gx_device_halftone **ppdht, uint num_comp,
                    gs_memory_t * mem)
{
    int code = cmd_create_dev_ht(ppdht, mem);
    gx_device_halftone *pdht = *ppdht;

    if (code < 0)
        return code;
    if (num_comp != pdht->num_comp) {
        gx_ht_order_component *pcomp;

        /*
         * We must be careful not to shrink or free the components array
         * before releasing any relevant elements.
         */
        if (num_comp < pdht->num_comp) {
            uint i;

            /* Don't release the default order. */
            for (i = pdht->num_comp; i-- > num_comp;)
                if (pdht->components[i].corder.bit_data != pdht->order.bit_data)
                    gx_ht_order_release(&pdht->components[i].corder, mem, true);
            if (num_comp == 0) {
                gs_free_object(mem, pdht->components, "cmd_resize_halftone");
                pcomp = 0;
            } else {
                pcomp = gs_resize_object(mem, pdht->components, num_comp,
                                         "cmd_resize_halftone");
                if (pcomp == 0) {
                    pdht->num_comp = num_comp;  /* attempt consistency */
                    return_error(gs_error_VMerror);
                }
            }
        } else {
            /* num_comp > pdht->num_comp */
            if (pdht->num_comp == 0)
                pcomp = gs_alloc_struct_array(mem, num_comp,
                                              gx_ht_order_component,
                                              &st_ht_order_component_element,
                                              "cmd_resize_halftone");
            else
                pcomp = gs_resize_object(mem, pdht->components, num_comp,
                                         "cmd_resize_halftone");
            if (pcomp == 0)
                return_error(gs_error_VMerror);
            memset(&pcomp[pdht->num_comp], 0,
                   sizeof(*pcomp) * (num_comp - pdht->num_comp));
        }
        pdht->num_comp = num_comp;
        pdht->components = pcomp;
    }
    return 0;
}

/* ------ Path operations ------ */

/* Decode a path segment. */
static int
clist_decode_segment(gx_path * ppath, int op, fixed vs[6],
                 gs_fixed_point * ppos, int x0, int y0, segment_notes notes)
{
    fixed px = ppos->x - int2fixed(x0);
    fixed py = ppos->y - int2fixed(y0);
    int code;

#define A vs[0]
#define B vs[1]
#define C vs[2]
#define D vs[3]
#define E vs[4]
#define F vs[5]

    switch (op) {
        case cmd_opv_rmoveto:
            code = gx_path_add_point(ppath, px += A, py += B);
            break;
        case cmd_opv_rlineto:
            code = gx_path_add_line_notes(ppath, px += A, py += B, notes);
            break;
        case cmd_opv_rgapto:
            code = gx_path_add_gap_notes(ppath, px += A, py += B, notes);
            break;
        case cmd_opv_hlineto:
            code = gx_path_add_line_notes(ppath, px += A, py, notes);
            break;
        case cmd_opv_vlineto:
            code = gx_path_add_line_notes(ppath, px, py += A, notes);
            break;
        case cmd_opv_rmlineto:
            if ((code = gx_path_add_point(ppath, px += A, py += B)) < 0)
                break;
            code = gx_path_add_line_notes(ppath, px += C, py += D, notes);
            break;
        case cmd_opv_rm2lineto:
            if ((code = gx_path_add_point(ppath, px += A, py += B)) < 0 ||
                (code = gx_path_add_line_notes(ppath, px += C, py += D,
                                               notes)) < 0
                )
                break;
            code = gx_path_add_line_notes(ppath, px += E, py += F, notes);
            break;
        case cmd_opv_rm3lineto:
            if ((code = gx_path_add_point(ppath, px += A, py += B)) < 0 ||
                (code = gx_path_add_line_notes(ppath, px += C, py += D,
                                               notes)) < 0 ||
                (code = gx_path_add_line_notes(ppath, px += E, py += F,
                                               notes)) < 0
                )
                break;
            code = gx_path_add_line_notes(ppath, px -= C, py -= D, notes);
            break;
        case cmd_opv_rrcurveto: /* a b c d e f => a b a+c b+d a+c+e b+d+f */
rrc:        E += (C += A);
            F += (D += B);
curve:      code = gx_path_add_curve_notes(ppath, px + A, py + B,
                                           px + C, py + D,
                                           px + E, py + F, notes);
            px += E, py += F;
            break;
        case cmd_opv_hvcurveto: /* a b c d => a 0 a+b c a+b c+d */
hvc:        F = C + D, D = C, E = C = A + B, B = 0;
            goto curve;
        case cmd_opv_vhcurveto: /* a b c d => 0 a b a+c b+d a+c */
vhc:        E = B + D, F = D = A + C, C = B, B = A, A = 0;
            goto curve;
        case cmd_opv_nrcurveto: /* a b c d => 0 0 a b a+c b+d */
            F = B + D, E = A + C, D = B, C = A, B = A = 0;
            goto curve;
        case cmd_opv_rncurveto: /* a b c d => a b a+c b+d a+c b+d */
            F = D += B, E = C += A;
            goto curve;
        case cmd_opv_vqcurveto: /* a b => VH a b TS(a,b) TS(b,a) */
            if ((A ^ B) < 0)
                C = -B, D = -A;
            else
                C = B, D = A;
            goto vhc;
        case cmd_opv_hqcurveto: /* a b => HV a TS(a,b) b TS(b,a) */
            if ((A ^ B) < 0)
                D = -A, C = B, B = -B;
            else
                D = A, C = B;
            goto hvc;
        case cmd_opv_scurveto: /* (a b c d e f) => */
            {
                fixed a = A, b = B;

                /* See gxclpath.h for details on the following. */
                if (A == 0) {
                    /* Previous curve was vh or vv */
                    A = E - C, B = D - F, C = C - a, D = b - D, E = a, F = -b;
                } else {
                    /* Previous curve was hv or hh */
                    A = C - E, B = F - D, C = a - C, D = D - b, E = -a, F = b;
                }
            }
            goto rrc;
        case cmd_opv_closepath:
            if ((code = gx_path_close_subpath(ppath)) < 0)
                return code;;
            if ((code = gx_path_current_point(ppath, (gs_fixed_point *) vs)) < 0)
                return code;;
            px = A, py = B;
            break;
        default:
            return_error(gs_error_rangecheck);
    }
#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
    ppos->x = px + int2fixed(x0);
    ppos->y = py + int2fixed(y0);
    return code;
}

/*
 * Execute a polyfill -- either a fill_parallelogram or a fill_triangle.
 *
 * Note that degenerate parallelograms or triangles may collapse into
 * a single line or point.  We must check for this so we don't try to
 * access non-existent segments.
 */
static int
clist_do_polyfill(gx_device *dev, gx_path *ppath,
                  const gx_drawing_color *pdcolor,
                  gs_logical_operation_t lop)
{
    const subpath *psub = ppath->first_subpath;
    const segment *pseg1;
    const segment *pseg2;
    int code;

    if (psub && (pseg1 = psub->next) != 0 && (pseg2 = pseg1->next) != 0) {
        fixed px = psub->pt.x, py = psub->pt.y;
        fixed ax = pseg1->pt.x - px, ay = pseg1->pt.y - py;
        fixed bx, by;
        /*
         * We take advantage of the fact that the parameter lists for
         * fill_parallelogram and fill_triangle are identical.
         */
        dev_proc_fill_parallelogram((*fill));

        /* close_path of 3 point triangle adds 4th point, detected here.*/
        /* close_path on parallelogram adds 5th point also ignored. */
        if (pseg2->next && !(px == pseg2->next->pt.x && py == pseg2->next->pt.y)) {
            /* Parallelogram */
            fill = dev_proc(dev, fill_parallelogram);
            bx = pseg2->pt.x - pseg1->pt.x;
            by = pseg2->pt.y - pseg1->pt.y;
        } else {
            /* Triangle */
            fill = dev_proc(dev, fill_triangle);
            bx = pseg2->pt.x - px;
            by = pseg2->pt.y - py;
        }
        code = fill(dev, px, py, ax, ay, bx, by, pdcolor, lop);
    } else
        code = 0;
    gx_path_new(ppath);
    return code;
}
