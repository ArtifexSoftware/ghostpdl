/* Copyright (C) 1991, 1996, 1997 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/* gxclist.c */
/* Command list writing for Ghostscript. */
#include "memory_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gpcheck.h"
#include "gserrors.h"
#include "gxdevice.h"
#include "gxdevmem.h"		/* must precede gxcldev.h */
#include "gxcldev.h"
#include "gxclpath.h"
#include "gsparams.h"

#define cdev cwdev

/* Forward declarations of procedures */
private dev_proc_open_device(clist_open);
private dev_proc_output_page(clist_output_page);
private dev_proc_close_device(clist_close);
private dev_proc_get_band(clist_get_band);
private int clist_put_current_params(gx_device_clist_writer * cldev);

/* In gxclrect.c */
extern dev_proc_fill_rectangle(clist_fill_rectangle);
extern dev_proc_copy_mono(clist_copy_mono);
extern dev_proc_copy_color(clist_copy_color);
extern dev_proc_copy_alpha(clist_copy_alpha);
extern dev_proc_strip_tile_rectangle(clist_strip_tile_rectangle);
extern dev_proc_strip_copy_rop(clist_strip_copy_rop);

/* In gxclread.c */
extern dev_proc_get_bits(clist_get_bits);

/* The device procedures */
gx_device_procs gs_clist_device_procs =
{clist_open,
 gx_forward_get_initial_matrix,
 gx_default_sync_output,
 clist_output_page,
 clist_close,
 gx_forward_map_rgb_color,
 gx_forward_map_color_rgb,
 clist_fill_rectangle,
 gx_default_tile_rectangle,
 clist_copy_mono,
 clist_copy_color,
 gx_default_draw_line,
 clist_get_bits,
 gx_forward_get_params,
 gx_forward_put_params,
 gx_forward_map_cmyk_color,
 gx_forward_get_xfont_procs,
 gx_forward_get_xfont_device,
 gx_forward_map_rgb_alpha_color,
 gx_forward_get_page_device,
 gx_forward_get_alpha_bits,
 clist_copy_alpha,
 clist_get_band,
 gx_default_copy_rop,
 gx_default_fill_path,
 gx_default_stroke_path,
 gx_default_fill_mask,
 gx_default_fill_trapezoid,
 gx_default_fill_parallelogram,
 gx_default_fill_triangle,
 gx_default_draw_thin_line,
 gx_default_begin_image,
 gx_default_image_data,
 gx_default_end_image,
 clist_strip_tile_rectangle,
 clist_strip_copy_rop,
 gx_forward_get_clipping_box,
 gx_forward_get_hardware_params
};

/* ------ Define the command set and syntax ------ */

/* Define the clipping enable/disable opcodes. */
/* The path extensions initialize these to their proper values. */
byte cmd_opvar_disable_clip = 0xff;
byte cmd_opvar_enable_clip = 0xff;

#ifdef DEBUG
const char *cmd_op_names[16] =
{cmd_op_name_strings};
private const char *cmd_misc_op_names[16] =
{cmd_misc_op_name_strings};
const char **cmd_sub_op_names[16] =
{cmd_misc_op_names, 0, 0, 0, 0, 0, 0, 0,
 0, 0, 0, 0, 0, 0, 0, 0
};
private ulong far_data cmd_op_counts[256];
private ulong far_data cmd_op_sizes[256];
private ulong cmd_tile_reset, cmd_tile_found, cmd_tile_added;
extern ulong cmd_diffs[5];	/* in gxclpath.c */
private ulong cmd_same_band, cmd_other_band;
int
cmd_count_op(int op, uint size)
{
    cmd_op_counts[op]++;
    cmd_op_sizes[op] += size;
    if (gs_debug_c('L')) {
	const char **sub = cmd_sub_op_names[op >> 4];

	if (sub)
	    dprintf2(", %s(%u)\n", sub[op & 0xf], size);
	else
	    dprintf3(", %s %d(%u)\n", cmd_op_names[op >> 4], op & 0xf, size);
	fflush(dstderr);
    }
    return op;
}
void
cmd_uncount_op(int op, uint size)
{
    cmd_op_counts[op]--;
    cmd_op_sizes[op] -= size;
}
#endif

/* Initialization for imager state. */
/* The initial scale is arbitrary. */
const gs_imager_state clist_imager_state_initial =
{gs_imager_state_initial(300.0 / 72.0)};

/*
 * The buffer area (data, data_size) holds a bitmap cache when both writing
 * and reading.  The rest of the space is used for the command buffer and
 * band state bookkeeping when writing, and for the rendering buffer (image
 * device) when reading.  For the moment, we divide the space up
 * arbitrarily, except that we allocate less space for the bitmap cache if
 * the device doesn't need halftoning.
 *
 * All the routines for allocating tables in the buffer are idempotent, so
 * they can be used to check whether a given-size buffer is large enough.
 */

/*
 * Calculate the desired size for the tile cache.
 */
private uint
clist_tile_cache_size(const gx_device * target, uint data_size)
{
    uint bits_size =
    (data_size / 5) & -align_cached_bits_mod;	/* arbitrary */

    if ((gx_device_has_color(target) ? target->color_info.max_color :
	 target->color_info.max_gray) >= 31
	) {			/* No halftones -- cache holds only Patterns & characters. */
	bits_size -= bits_size >> 2;
    }
#define min_bits_size 1024
    if (bits_size < min_bits_size)
	bits_size = min_bits_size;
#undef min_bits_size
    return bits_size;
}

/*
 * Initialize the allocation for the tile cache.  Sets: tile_hash_mask,
 * tile_max_count, tile_table, chunk (structure), bits (structure).
 */
private int
clist_init_tile_cache(gx_device * dev, byte * init_data, ulong data_size)
{
    byte *data = init_data;
    uint bits_size = data_size;

    /*
     * Partition the bits area between the hash table and the actual
     * bitmaps.  The per-bitmap overhead is about 24 bytes; if the
     * average character size is 10 points, its bitmap takes about 24 +
     * 0.5 * 10/72 * xdpi * 10/72 * ydpi / 8 bytes (the 0.5 being a
     * fudge factor to account for characters being narrower than they
     * are tall), which gives us a guideline for the size of the hash
     * table.
     */
    uint avg_char_size =
    (uint) (dev->x_pixels_per_inch * dev->y_pixels_per_inch *
	    (0.5 * 10 / 72 * 10 / 72 / 8)) + 24;
    uint hc = bits_size / avg_char_size;
    uint hsize;

    while ((hc + 1) & hc)
	hc |= hc >> 1;		/* make mask (power of 2 - 1) */
    if (hc < 0xff)
	hc = 0xff;		/* make allowance for halftone tiles */
    else if (hc > 0xfff)
	hc = 0xfff;		/* cmd_op_set_tile_index has 12-bit operand */
    /* Make sure the tables will fit. */
    while (hc >= 3 && (hsize = (hc + 1) * sizeof(tile_hash)) >= bits_size)
	hc >>= 1;
    if (hc < 3)
	return_error(gs_error_rangecheck);
    cdev->tile_hash_mask = hc;
    cdev->tile_max_count = hc - (hc >> 2);
    cdev->tile_table = (tile_hash *) data;
    data += hsize;
    bits_size -= hsize;
    gx_bits_cache_chunk_init(&cdev->chunk, data, bits_size);
    gx_bits_cache_init(&cdev->bits, &cdev->chunk);
    return 0;
}

/*
 * Initialize the allocation for the bands.  Requires: target.  Sets:
 * page_band_height (=page_info.band_params.BandHeight), nbands.
 */
private int
clist_init_bands(gx_device * dev, uint data_size, int band_width,
		 int band_height)
{
    gx_device *target = cdev->target;
    int nbands;

    if (gdev_mem_data_size((gx_device_memory *) target, band_width,
			   band_height) > data_size
	)
	return_error(gs_error_rangecheck);
    cdev->page_band_height = band_height;
    nbands = (target->height + band_height - 1) / band_height;
    cdev->nbands = nbands;
#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dprintf4("[l]width=%d, band_width=%d, band_height=%d, nbands=%d\n",
		 target->width, band_width, band_height, nbands);
#endif
    return 0;
}

/*
 * Initialize the allocation for the band states, which are used only
 * when writing.  Requires: nbands.  Sets: states, cbuf, cend.
 */
private int
clist_init_states(gx_device * dev, byte * init_data, uint data_size)
{
    ulong state_size = cdev->nbands * (ulong) sizeof(gx_clist_state);

    /*
     * The +100 in the next line is bogus, but we don't know what the
     * real check should be. We're effectively assuring that at least 100
     * bytes will be available to buffer command operands.
     */
    if (state_size + sizeof(cmd_prefix) + cmd_largest_size + 100 > data_size)
	return_error(gs_error_rangecheck);
    cdev->states = (gx_clist_state *) init_data;
    cdev->cbuf = init_data + state_size;
    cdev->cend = init_data + data_size;
    return 0;
}

/*
 * Initialize all the data allocations.  Requires: target.  Sets:
 * page_tile_cache_size, page_info.band_params.BandWidth,
 * page_info.band_params.BandBufferSpace, + see above.
 */
private int
clist_init_data(gx_device * dev, byte * init_data, uint data_size)
{
    gx_device *target = cdev->target;
    const int band_width =
    cdev->page_info.band_params.BandWidth =
    (cdev->band_params.BandWidth ? cdev->band_params.BandWidth :
     target->width);
    int band_height = cdev->band_params.BandHeight;
    const uint band_space =
    cdev->page_info.band_params.BandBufferSpace =
    (cdev->band_params.BandBufferSpace ?
     cdev->band_params.BandBufferSpace : data_size);
    byte *data = init_data;
    uint size = band_space;
    uint bits_size;
    int code;

    if (band_height) {		/*
				 * The band height is fixed, so the band buffer requirement
				 * is completely determined.
				 */
	uint band_data_size =
	gdev_mem_data_size((gx_device_memory *) target,
			   band_width, band_height);

	if (band_data_size >= band_space)
	    return_error(gs_error_rangecheck);
	bits_size = min(band_space - band_data_size, data_size >> 1);
    } else {			/*
				 * Choose the largest band height that will fit in the
				 * rendering-time buffer.
				 */
	bits_size = clist_tile_cache_size(target, band_space);
	bits_size = min(bits_size, data_size >> 1);
	band_height = gdev_mem_max_height((gx_device_memory *) target,
					  band_width,
					  band_space - bits_size);
	if (band_height == 0)
	    return_error(gs_error_rangecheck);
    }
    code = clist_init_tile_cache(dev, data, bits_size);
    if (code < 0)
	return code;
    cdev->page_tile_cache_size = bits_size;
    data += bits_size;
    size -= bits_size;
    code = clist_init_bands(dev, size, band_width, band_height);
    if (code < 0)
	return code;
    return clist_init_states(dev, data, data_size - bits_size);
}
/*
 * Reset the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
private int
clist_reset(gx_device * dev)
{
    int code;
    int nbands;

    code = clist_init_data(dev, cdev->data, cdev->data_size);
    if (code < 0)
	return (cdev->permanent_error = code);

    /* Now initialize the rest of the state. */
    cdev->permanent_error = 0;
    nbands = cdev->nbands;
    cdev->ymin = cdev->ymax = -1;	/* render_init not done yet */
    memset(cdev->tile_table, 0, (cdev->tile_hash_mask + 1) *
	   sizeof(*cdev->tile_table));
    cdev->cnext = cdev->cbuf;
    cdev->ccl = 0;
    cdev->band_range_list.head = cdev->band_range_list.tail = 0;
    cdev->band_range_min = 0;
    cdev->band_range_max = nbands - 1;
    {
	int band;
	gx_clist_state *states = cdev->states;

	for (band = 0; band < nbands; band++, states++) {
	    static const gx_clist_state cls_initial =
	    {cls_initial_values};

	    *states = cls_initial;
	}
    }
    /*
     * Round up the size of the per-tile band mask so that the bits,
     * which follow it, stay aligned.
     */
    cdev->tile_band_mask_size =
	((nbands + (align_bitmap_mod * 8 - 1)) >> 3) &
	~(align_bitmap_mod - 1);
    /*
     * Initialize the all-band parameters to impossible values,
     * to force them to be written the first time they are used.
     */
    memset(&cdev->tile_params, 0, sizeof(cdev->tile_params));
    cdev->tile_depth = 0;
    cdev->tile_known_min = nbands;
    cdev->tile_known_max = -1;
    cdev->imager_state = clist_imager_state_initial;
    cdev->clip_path = NULL;
    cdev->clip_path_id = gs_no_id;
    cdev->color_space = 0;
    {
	int i;

	for (i = 0; i < countof(cdev->transfer_ids); ++i)
	    cdev->transfer_ids[i] = gs_no_id;
    }
    cdev->black_generation_id = gs_no_id;
    cdev->undercolor_removal_id = gs_no_id;
    cdev->device_halftone_id = gs_no_id;
    return 0;
}
/*
 * Initialize the device state (for writing).  This routine requires only
 * data, data_size, and target to be set, and is idempotent.
 */
private int
clist_init(gx_device * dev)
{
    int code = clist_reset(dev);

    if (code >= 0) {
	cdev->in_image = false;
	cdev->error_is_retryable = 0;
	cdev->driver_call_nesting = 0;
	cdev->ignore_lo_mem_warnings = 0;
    }
    return code;
}

/* (Re)init open band files for output (set block size, etc). */
private int			/* ret 0 ok, -ve error code */
clist_reinit_output_file(gx_device * dev)
{
    int code = 0;

    /* bfile needs to guarantee cmd_blocks for: 1 band range, nbands */
    /*  & terminating entry */
    int b_block = sizeof(cmd_block) * (cdev->nbands + 2);

    /* cfile needs to guarantee one writer buffer */
    /*  + one end_clip cmd (if during image's clip path setup) */
    /*  + an end_image cmd for each band (if during image) */
    /*  + end_cmds for each band and one band range */
    int c_block
    = cdev->cend - cdev->cbuf + 2 + cdev->nbands * 2 + (cdev->nbands + 1);

    /* All this is for partial page rendering's benefit, do do only */
    /* if partial page rendering is available */
    if (clist_test_VMerror_recoverable(cdev)) {
	if (cdev->page_bfile != 0)
	    code = clist_set_block_size(cdev->page_bfile, b_block);
	if (code >= 0 && cdev->page_cfile != 0)
	    code = clist_set_block_size(cdev->page_cfile, c_block);
    }
    return code;
}

/* Write out the current parameters that must be at the head of each page */
/* if async rendering is in effect */
private int
clist_emit_page_header(gx_device * dev)
{
    int code = 0;

    if ((cdev->disable_mask & clist_disable_pass_thru_params)) {
	do
	    if ((code = clist_put_current_params(cdev)) >= 0)
		break;
	while ((code = clist_try_recover_VMerror(cdev, code)) < 0);
	cdev->permanent_error = (code < 0) ? code : 0;
	if (cdev->permanent_error < 0)
	    cdev->error_is_retryable = 0;
    }
    return code;
}

/* Open the device's bandfiles */
private int
clist_open_output_file(gx_device * dev)
{
    char fmode[4];
    int code;

    if (cdev->do_not_open_or_close_bandfiles)
	return 0;		/* external bandfile open/close managed externally */

    cdev->page_cfile = 0;	/* in case of failure */
    cdev->page_bfile = 0;	/* ditto */
    strcpy(fmode, "w+");
    strcat(fmode, gp_fmode_binary_suffix);
    cdev->page_cfname[0] = 0;	/* create a new file */
    cdev->page_bfname[0] = 0;	/* ditto */
    cdev->page_bfile_end_pos = 0;
    if ((code = clist_fopen(cdev->page_cfname, fmode, &cdev->page_cfile,
		 cdev->bandlist_memory, cdev->bandlist_memory, true)) < 0 ||
	(code = clist_fopen(cdev->page_bfname, fmode, &cdev->page_bfile,
		 cdev->bandlist_memory, cdev->bandlist_memory, true)) < 0 ||
	(code = clist_reinit_output_file(dev)) < 0
	) {
	clist_close_output_file(dev);
	cdev->permanent_error = code;
	cdev->error_is_retryable = 0;
    }
    return code;
}

/* Close the device by freeing the temporary files. */
/* Note that this does not deallocate the buffer. */
int
clist_close_output_file(gx_device * dev)
{
    if (cdev->page_cfile != NULL) {
	clist_fclose(cdev->page_cfile, cdev->page_cfname, true);
	cdev->page_cfile = NULL;
    }
    if (cdev->page_bfile != NULL) {
	clist_fclose(cdev->page_bfile, cdev->page_bfname, true);
	cdev->page_bfile = NULL;
    }
    return 0;
}

/* Open the device by initializing the device state and opening the */
/* scratch files. */
private int
clist_open(gx_device * dev)
{
    int code;
    int pages_remain;

    cdev->permanent_error = 0;
    code = clist_init(dev);
    if (code < 0)
	return code;
    code = clist_open_output_file(dev);
    if (code >= 0)
	code = clist_emit_page_header(dev);
    return code;
}

private int
clist_close(gx_device * dev)
{
    if (cdev->do_not_open_or_close_bandfiles)
	return 0;
    return clist_close_output_file(dev);
}

/* The output_page procedure should never be called! */
private int
clist_output_page(gx_device * dev, int num_copies, int flush)
{
    return_error(gs_error_Fatal);
}

/* Reset (or prepare to append to) the command list after printing a page. */
int
clist_finish_page(gx_device * dev, bool flush)
{
    int code;

    if (flush) {
	if (cdev->page_cfile != 0)
	    clist_rewind(cdev->page_cfile, true, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    clist_rewind(cdev->page_bfile, true, cdev->page_bfname);
	cdev->page_bfile_end_pos = 0;
    } else {
	if (cdev->page_cfile != 0)
	    clist_fseek(cdev->page_cfile, 0L, SEEK_END, cdev->page_cfname);
	if (cdev->page_bfile != 0)
	    clist_fseek(cdev->page_bfile, 0L, SEEK_END, cdev->page_bfname);
    }
    code = clist_init(dev);	/* reinitialize */
    if (code >= 0)
	code = clist_reinit_output_file(dev);
    if (code >= 0)
	code = clist_emit_page_header(dev);

    return code;
}

/* Print statistics. */
#ifdef DEBUG
void
cmd_print_stats(void)
{
    int ci, cj;

    dprintf3("[l]counts: reset = %lu, found = %lu, added = %lu\n",
	     cmd_tile_reset, cmd_tile_found, cmd_tile_added);
    dprintf5("     diff 2.5 = %lu, 3 = %lu, 4 = %lu, 2 = %lu, >4 = %lu\n",
	     cmd_diffs[0], cmd_diffs[1], cmd_diffs[2], cmd_diffs[3],
	     cmd_diffs[4]);
    dprintf2("     same_band = %lu, other_band = %lu\n",
	     cmd_same_band, cmd_other_band);
    for (ci = 0; ci < 0x100; ci += 0x10) {
	const char **sub = cmd_sub_op_names[ci >> 4];

	if (sub != 0) {
	    dprintf1("[l]  %s =", cmd_op_names[ci >> 4]);
	    for (cj = ci; cj < ci + 0x10; cj += 2)
		dprintf6("\n\t%s = %lu(%lu), %s = %lu(%lu)",
			 sub[cj - ci],
			 cmd_op_counts[cj], cmd_op_sizes[cj],
			 sub[cj - ci + 1],
			 cmd_op_counts[cj + 1], cmd_op_sizes[cj + 1]);
	} else {
	    ulong tcounts = 0, tsizes = 0;

	    for (cj = ci; cj < ci + 0x10; cj++)
		tcounts += cmd_op_counts[cj],
		    tsizes += cmd_op_sizes[cj];
	    dprintf3("[l]  %s (%lu,%lu) =\n\t",
		     cmd_op_names[ci >> 4], tcounts, tsizes);
	    for (cj = ci; cj < ci + 0x10; cj++)
		if (cmd_op_counts[cj] == 0)
		    dputs(" -");
		else
		    dprintf2(" %lu(%lu)", cmd_op_counts[cj],
			     cmd_op_sizes[cj]);
	}
	dputs("\n");
    }
}
#endif /* DEBUG */

/* ------ Writing ------ */

/* Utilities */

/* Write the commands for one band or band range. */
private int			/* ret 0 all ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_band(gx_device_clist_writer * cldev, int band_min, int band_max,
	       cmd_list * pcl, byte cmd_end)
{
    const cmd_prefix *cp = pcl->head;
    int code_b = 0;
    int code_c = 0;

    if (cp != 0 || cmd_end != cmd_opv_end_run) {
	clist_file_ptr cfile = cldev->page_cfile;
	clist_file_ptr bfile = cldev->page_bfile;
	cmd_block cb;
	byte end = cmd_count_op(cmd_end, 1);

	if (cfile == 0 || bfile == 0)
	    return_error(gs_error_ioerror);
	cb.band_min = band_min;
	cb.band_max = band_max;
	cb.pos = clist_ftell(cfile);
	if_debug3('l', "[l]writing for bands (%d,%d) at %ld\n",
		  band_min, band_max, cb.pos);
	clist_fwrite_chars(&cb, sizeof(cb), bfile);
	if (cp != 0) {
	    pcl->tail->next = 0;	/* terminate the list */
	    for (; cp != 0; cp = cp->next) {
#ifdef DEBUG
		if ((const byte *)cp < cldev->cbuf ||
		    (const byte *)cp >= cldev->cend ||
		    cp->size > cldev->cend - (const byte *)cp
		    ) {
		    lprintf1("cmd_write_band error at 0x%lx\n", (ulong) cp);
		    return_error(gs_error_Fatal);
		}
#endif
		clist_fwrite_chars(cp + 1, cp->size, cfile);
	    }
	    pcl->head = pcl->tail = 0;
	}
	clist_fwrite_chars(&end, 1, cfile);
	process_interrupts();
	code_b = clist_ferror_code(bfile);
	code_c = clist_ferror_code(cfile);
	if (code_b < 0)
	    return_error(code_b);
	if (code_c < 0)
	    return_error(code_c);
    }
    return code_b | code_c;
}

/* Write out the buffered commands, and reset the buffer. */
private int			/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
cmd_write_buffer(gx_device_clist_writer * cldev, byte cmd_end)
{
    int nbands = cldev->nbands;
    gx_clist_state *pcls;
    int band;
    int warning;
    int code = cmd_write_band(cldev, cldev->band_range_min,
			      cldev->band_range_max,
			      &cldev->band_range_list, cmd_opv_end_run);


    warning = code;
    for (band = 0, pcls = cldev->states;
	 code >= 0 && band < nbands; band++, pcls++
	) {
	code = cmd_write_band(cldev, band, band, &pcls->list, cmd_end);
	warning |= code;
    }
    cldev->cnext = cldev->cbuf;
    cldev->ccl = 0;
    cldev->band_range_list.head = cldev->band_range_list.tail = 0;
#ifdef DEBUG
    if (gs_debug_c('l'))
	cmd_print_stats();
#endif
    return_check_interrupt(code != 0 ? code : warning);
}
/* End a page by flushing the buffer and terminating the command list. */
int				/* ret 0 all-ok, -ve error code, or +1 ok w/low-mem warning */
clist_end_page(gx_device_clist_writer * cldev)
{
    int code = cmd_write_buffer(cldev, cmd_opv_end_page);
    cmd_block cb;
    int ecode = 0;

    if (code >= 0) {		/* Write the terminating entry in the block file. */
	/* Note that because of copypage, there may be many such entries. */
	cb.band_min = cb.band_max = cmd_band_end;
	cb.pos
	    = cldev->page_cfile == 0 ? 0 : clist_ftell(cldev->page_cfile);
	clist_fwrite_chars(&cb, sizeof(cb), cldev->page_bfile);
	code = clist_ferror_code(cldev->page_bfile);
    }
    if (code >= 0) {
	ecode |= code;
	cldev->page_bfile_end_pos = clist_ftell(cldev->page_bfile);
    }
    if (code < 0)
	ecode = code;

    /* reset block size to 0 to release reserve memory if mem files */
    if (cldev->page_bfile != 0)
	clist_set_block_size(cldev->page_bfile, 0);
    if (cldev->page_cfile != 0)
	clist_set_block_size(cldev->page_cfile, 0);

#ifdef DEBUG
    if (gs_debug_c('l') | gs_debug_c(':'))
	dprintf2("[l]clist_end_page at cfile=%ld, bfile=%ld\n",
		 cb.pos, cldev->page_bfile_end_pos);
#endif
    return ecode;
}

/* Recover recoverable VM error if possible without flushing */
int				/* ret -ve err, >= 0 if recovered w/# = cnt pages left in page queue */
clist_try_recover_VMerror(gx_device_clist_writer * cldev,
			  int old_error_code)
{
    int code = old_error_code;
    int pages_remain;

    if (!clist_test_VMerror_recoverable(cldev)
	|| !cldev->error_is_retryable
	|| old_error_code != gs_error_VMerror)
	return old_error_code;

    /* Do some rendering, return if enough memory is now free */
    do {
	pages_remain = (*cldev->free_up_bandlist_memory) ((gx_device *) cldev, false);
	if (pages_remain < 0) {
	    code = pages_remain;	/* abort, error or interrupt req */
	    break;
	}
	if (clist_reinit_output_file((gx_device *) cldev) == 0) {
	    code = pages_remain;	/* got enough memory to continue */
	    break;
	}
    } while (pages_remain);

    if_debug1('L', "[L]soft flush of command list, status: %d\n", code);
    return code;
}

/* If recoverable VM error, flush & try to recover it */
int				/* ret 0 ok, else -ve error */
clist_try_recover_VMerror_flush(gx_device_clist_writer * cldev,
				int old_error_code)
{
    int free_code = 0;
    int reset_code = 0;
    int code;

    /* If the device has the ability to render partial pages, flush
     * out the bandlist, and reset the writing state. Then, get the
     * device to render this band. When done, see if there's now enough
     * memory to satisfy the minimum low-memory guarantees. If not, 
     * get the device to render some more. If there's nothing left to
     * render & still insufficient memory, declare an error condition.
     */
    if (!clist_test_VMerror_recoverable(cldev)
	|| old_error_code != gs_error_VMerror)
	return old_error_code;	/* sorry, don't have any means to recover this error */
    free_code = (*cldev->free_up_bandlist_memory) ((gx_device *) cldev, true);

    /* Reset the state of bands to "don't know anything" */
    reset_code = clist_reset((gx_device *) cldev);
    if (reset_code >= 0)
	reset_code = clist_open_output_file((gx_device *) cldev);
    if (reset_code >= 0
	&& (cldev->disable_mask & clist_disable_pass_thru_params))
	reset_code = clist_put_current_params(cldev);
    if (reset_code < 0) {
	cldev->permanent_error = reset_code;
	cldev->error_is_retryable = 0;
    }
    code = reset_code < 0 ? reset_code : (free_code < 0 ? old_error_code : 0);
    if_debug1('L', "[L]hard flush of command list, status: %d\n", code);
    return code;
}

/* Add a command to the appropriate band list, */
/* and allocate space for its data. */
/* Return the pointer to the data area. */
/* If an error or warning occurs, set cldev->error_code and return 0. */
#define cmd_headroom (sizeof(cmd_prefix) + arch_align_ptr_mod)
byte *
cmd_put_list_op(gx_device_clist_writer * cldev, cmd_list * pcl, uint size)
{
    byte *dp = cldev->cnext;

    if (size + cmd_headroom > cldev->cend - dp) {
	if ((cldev->error_code =
	     cmd_write_buffer(cldev, cmd_opv_end_run)) != 0) {
	    if (cldev->error_code < 0)
		cldev->error_is_retryable = 0;	/* hard error */
	    else {
		/* upgrade lo-mem warning into an error */
		if (!cldev->ignore_lo_mem_warnings)
		    cldev->error_code = gs_error_VMerror;
		cldev->error_is_retryable = 1;
	    }
	    return 0;
	} else
	    return cmd_put_list_op(cldev, pcl, size);
    }
    if (cldev->ccl == pcl) {	/* We're adding another command for the same band. */
	/* Tack it onto the end of the previous one. */
	cmd_count_add1(cmd_same_band);
#ifdef DEBUG
	if (pcl->tail->size > dp - (byte *) (pcl->tail + 1)) {
	    lprintf1("cmd_put_list_op error at 0x%lx\n", (ulong) pcl->tail);
	}
#endif
	pcl->tail->size += size;
    } else {			/* Skip to an appropriate alignment boundary. */
	/* (We assume the command buffer itself is aligned.) */
	cmd_prefix *cp =
	(cmd_prefix *) (dp +
			((cldev->cbuf - dp) & (arch_align_ptr_mod - 1)));

	cmd_count_add1(cmd_other_band);
	dp = (byte *) (cp + 1);
	if (pcl->tail != 0) {
#ifdef DEBUG
	    if (pcl->tail < pcl->head ||
		pcl->tail->size > dp - (byte *) (pcl->tail + 1)
		) {
		lprintf1("cmd_put_list_op error at 0x%lx\n",
			 (ulong) pcl->tail);
	    }
#endif
	    pcl->tail->next = cp;
	} else
	    pcl->head = cp;
	pcl->tail = cp;
	cldev->ccl = pcl;
	cp->size = size;
    }
    cldev->cnext = dp + size;
    return dp;
}
#ifdef DEBUG
byte *
cmd_put_op(gx_device_clist_writer * cldev, gx_clist_state * pcls, uint size)
{
    if_debug3('L', "[L]band %d: size=%u, left=%u",
	      (int)(pcls - cldev->states),
	      size, (uint) (cldev->cend - cldev->cnext));
    return cmd_put_list_op(cldev, &pcls->list, size);
}
#endif

/* Add a command for a range of bands. */
byte *
cmd_put_range_op(gx_device_clist_writer * cldev, int band_min, int band_max,
		 uint size)
{
    if_debug4('L', "[L]band range(%d,%d): size=%u, left=%u",
	      band_min, band_max, size,
	      (uint) (cldev->cend - cldev->cnext));
    if (cldev->ccl != 0 &&
	(cldev->ccl != &cldev->band_range_list ||
	 band_min != cldev->band_range_min ||
	 band_max != cldev->band_range_max)
	) {
	if ((cldev->error_code = cmd_write_buffer(cldev, cmd_opv_end_run)) != 0) {
	    if (cldev->error_code < 0)
		cldev->error_is_retryable = 0;	/* hard error */
	    else {
		/* upgrade lo-mem warning into an error */
		cldev->error_code = gs_error_VMerror;
		cldev->error_is_retryable = 1;
	    }
	    return 0;
	}
	cldev->band_range_min = band_min;
	cldev->band_range_max = band_max;
    }
    return cmd_put_list_op(cldev, &cldev->band_range_list, size);
}

/* Write a variable-size positive integer. */
int
cmd_size_w(register uint w)
{
    register int size = 1;

    while (w > 0x7f)
	w >>= 7, size++;
    return size;
}
byte *
cmd_put_w(register uint w, register byte * dp)
{
    while (w > 0x7f)
	*dp++ = w | 0x80, w >>= 7;
    *dp = w;
    return dp + 1;
}

/* Define the encodings of the different settable colors. */
const clist_select_color_t
      clist_select_color0 =
{cmd_op_set_color0, cmd_opv_delta2_color0, 0}, clist_select_color1 =
{cmd_op_set_color1, cmd_opv_delta2_color1, 0}, clist_select_tile_color0 =
{cmd_op_set_color0, cmd_opv_delta2_color0, 1}, clist_select_tile_color1 =
{cmd_op_set_color1, cmd_opv_delta2_color1, 1};
int				/* ret 0 all ok, -ve error */
cmd_put_color(gx_device_clist_writer * cldev, gx_clist_state * pcls,
	      const clist_select_color_t * select,
	      gx_color_index color, gx_color_index * pcolor)
{
    byte *dp;
    long diff = (long)color - (long)(*pcolor);
    byte op, op_delta2;
    int code;

    if (diff == 0)
	return 0;
    if (select->tile_color) {
	code = set_cmd_put_op(dp, cldev, pcls, cmd_opv_set_tile_color, 1);
	if (code < 0)
	    return code;
    }
    op = select->set_op;
    op_delta2 = select->delta2_op;
    if (color == gx_no_color_index) {	/*
					 * We must handle this specially, because it may take more
					 * bytes than the color depth.
					 */
	code = set_cmd_put_op(dp, cldev, pcls, op + 15, 1);
	if (code < 0)
	    return code;
    } else {
	long delta;
	byte operand;

	switch ((cldev->color_info.depth + 15) >> 3) {
	    case 5:
		if (!((delta = diff + cmd_delta1_32_bias) &
		      ~cmd_delta1_32_mask) &&
		    (operand =
		     (byte) ((delta >> 23) + ((delta >> 18) & 1))) != 0 &&
		    operand != 15
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  (byte) (op + operand), 2);
		    if (code < 0)
			return code;
		    dp[1] = (byte) (((delta >> 10) & 0300) +
				    (delta >> 5) + delta);
		    break;
		}
		if (!((delta = diff + cmd_delta2_32_bias) &
		      ~cmd_delta2_32_mask)
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
		    if (code < 0)
			return code;
		    dp[1] = (byte) ((delta >> 20) + (delta >> 16));
		    dp[2] = (byte) ((delta >> 4) + delta);
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 5);
		if (code < 0)
		    return code;
		*++dp = (byte) (color >> 24);
		goto b3;
	    case 4:
		if (!((delta = diff + cmd_delta1_24_bias) &
		      ~cmd_delta1_24_mask) &&
		    (operand = (byte) (delta >> 16)) != 0 &&
		    operand != 15
		    ) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  (byte) (op + operand), 2);
		    if (code < 0)
			return code;
		    dp[1] = (byte) ((delta >> 4) + delta);
		    break;
		}
		if (!((delta = diff + cmd_delta2_24_bias) &
		      ~cmd_delta2_24_mask)
		    ) {
		    code
			= set_cmd_put_op(dp, cldev, pcls, op_delta2, 3);
		    if (code < 0)
			return code;
		    dp[1] = ((byte) (delta >> 13) & 0xf8) +
			((byte) (delta >> 11) & 7);
		    dp[2] = (byte) (((delta >> 3) & 0xe0) + delta);
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 4);
		if (code < 0)
		    return code;
	      b3:*++dp = (byte) (color >> 16);
		goto b2;
	    case 3:
		code = set_cmd_put_op(dp, cldev, pcls, op, 3);
		if (code < 0)
		    return code;
	      b2:*++dp = (byte) (color >> 8);
		goto b1;
	    case 2:
		if (diff >= -7 && diff < 7) {
		    code = set_cmd_put_op(dp, cldev, pcls,
					  op + (int)diff + 8, 1);
		    if (code < 0)
			return code;
		    break;
		}
		code = set_cmd_put_op(dp, cldev, pcls, op, 2);
		if (code < 0)
		    return code;
	      b1:dp[1] = (byte) color;
	}
    }
    *pcolor = color;
    return 0;
}

/* Put out a command to set the tile colors. */
int				/* ret 0 all ok, -ve error */
cmd_set_tile_colors(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		    gx_color_index color0, gx_color_index color1)
{
    if (color0 != pcls->tile_colors[0]) {
	int code = cmd_put_color(cldev, pcls,
				 &clist_select_tile_color0,
				 color0, &pcls->tile_colors[0]);

	if (code != 0)
	    return code;
    }
    if (color1 != pcls->tile_colors[1]) {
	int code = cmd_put_color(cldev, pcls,
				 &clist_select_tile_color1,
				 color1, &pcls->tile_colors[1]);

	if (code != 0)
	    return code;
    }
    return 0;
}

/* Put out a command to set the tile phase. */
int				/* ret 0 all ok, -ve error */
cmd_set_tile_phase(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		   int px, int py)
{
    int pcsize;
    byte *dp;
    int code;

    pcls->tile_phase.x = px;
    pcls->tile_phase.y = py;
    pcsize = 1 + cmd_sizexy(pcls->tile_phase);
    code =
	set_cmd_put_op(dp, cldev, pcls, (byte) cmd_opv_set_tile_phase, pcsize);
    if (code < 0)
	return code;
    ++dp;
    cmd_putxy(pcls->tile_phase, dp);
    return 0;
}

/* Write a command to enable or disable the logical operation. */
int				/* ret 0 all ok, -ve error */
cmd_put_enable_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		   int enable)
{
    byte *dp;
    int code = set_cmd_put_op(dp, cldev, pcls,
			      (byte) (enable ? cmd_opv_enable_lop :
				      cmd_opv_disable_lop),
			      1);

    if (code < 0)
	return code;
    pcls->lop_enabled = enable;
    return 0;
}

/* Write a command to enable or disable clipping. */
/* This routine is only called if the path extensions are included. */
int				/* ret 0 all ok, -ve error */
cmd_put_enable_clip(gx_device_clist_writer * cldev, gx_clist_state * pcls,
		    int enable)
{
    byte *dp;
    int code = set_cmd_put_op(dp, cldev, pcls,
			      (byte) (enable ? cmd_opvar_enable_clip :
				      cmd_opvar_disable_clip),
			      1);

    if (code < 0)
	return code;
    pcls->clip_enabled = enable;
    return 0;
}

/* Write a command to set the logical operation. */
int				/* ret 0 all ok, -ve error */
cmd_set_lop(gx_device_clist_writer * cldev, gx_clist_state * pcls,
	    gs_logical_operation_t lop)
{
    byte *dp;
    uint lop_msb = lop >> 6;

    int code = set_cmd_put_op(dp, cldev, pcls,
			      cmd_opv_set_misc, 2 + cmd_size_w(lop_msb));

    if (code < 0)
	return code;
    dp[1] = cmd_set_misc_lop + (lop & 0x3f);
    cmd_put_w(lop_msb, dp + 2);
    pcls->lop = lop;
    return 0;
}

/* Write a parameter list */
int				/* ret 0 all ok, -ve error */
cmd_put_params(gx_device_clist_writer * cldev,
	       gs_param_list * param_list)
{				/* NB open for READ */
    byte *dp;
    int code;
    byte local_buf[512];	/* arbitrary */
    int param_length = 0;

    /* Get serialized list's length + try to get it into local var if it fits. */
    param_length = code =
	gs_param_list_serialize(param_list, local_buf, sizeof(local_buf));

    if (param_length > 0) {	/* Get cmd buffer space for serialized */
	code = set_cmd_put_all_op(dp, cldev, cmd_opv_put_params,
				  1 + sizeof(unsigned) + param_length);

	if (code < 0)
	    return code;

	/* write param list to cmd list: needs to all fit in cmd buffer */
	if_debug1('l', "[l]put_params, length=%d\n", param_length);
	++dp;
	memcpy(dp, &param_length, sizeof(unsigned));
	dp += sizeof(unsigned);

	if (param_length > sizeof(local_buf)) {
	    int old_param_length = param_length;

	    param_length = code
		= gs_param_list_serialize(param_list, dp, old_param_length);
	    if (param_length >= 0)
		code = old_param_length != param_length
		    ? code = gs_error_unknownerror : 0;
	    if (code < 0) {	/* error serializing: back out by writing a 0-length parm list */
		memset(dp - sizeof(unsigned), 0, sizeof(unsigned));

		cmd_shorten_list_op(cldev, &cldev->band_range_list, old_param_length);
	    }
	} else
	    memcpy(dp, local_buf, param_length);	/* did this when computing length */
    }
    return code;
}

/* Write the target device's current parameter list */
private int			/* ret 0 all ok, -ve error */
clist_put_current_params(gx_device_clist_writer * cldev)
{
    gx_device *target = cldev->target;
    gs_c_param_list param_list;
    int code;

    /* To avoid attempts to write to unallocated memory, no attempt */
    /* is made to write to the command list if permanent_error is set. */
    /* This typically happens if a previous put_params failed. */
    if (cldev->permanent_error)
	return cldev->permanent_error;
    gs_c_param_list_write(&param_list, cldev->memory);
    code = (*dev_proc(target, get_params))
	(target, (gs_param_list *) & param_list);
    if (code >= 0) {
	gs_c_param_list_read(&param_list);
	code = cmd_put_params(cldev, (gs_param_list *) & param_list);
    }
    gs_c_param_list_release(&param_list);

    return code;
}


/* ---------------- Driver interface ---------------- */

private int
clist_get_band(gx_device * dev, int y, int *band_start)
{
    int band_height = cdev->page_band_height;
    int start;

    if (y < 0)
	y = 0;
    else if (y >= dev->height)
	y = dev->height;
    *band_start = start = y - y % band_height;
    return min(dev->height - start, band_height);
}
