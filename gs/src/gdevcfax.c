/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

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

/*$Id$ */
/* SFF format writer for CAPI fax devices */
#include "gdevprn.h"
#include "strimpl.h"
#include "scfx.h"
#include "gdevfax.h"
 
/* The device descriptor */
private dev_proc_print_page(cfax_print_page);
const gx_device_fax gs_cfax_device = {
    FAX_DEVICE_BODY(gx_device_fax, gdev_fax_std_procs, "cfax", cfax_print_page)
};

/* ---------------- SFF output ----------------- */

private void
cfax_byte(uint c, FILE * file)
{
    fputc(c & 0xff, file);
}

private void
cfax_word(ushort c, FILE * file)
{
    cfax_byte(c & 0xff, file);
    cfax_byte(c >> 8, file);
}

private void
cfax_dword(ulong c, FILE * file)
{
    cfax_byte(c & 0xff, file);
    cfax_byte(c >> 8, file);
    cfax_byte(c >> 16, file);
    cfax_byte(c >> 24, file);
}

private void
cfax_doc_hdr(FILE * file)
{
    cfax_byte('S', file);
    cfax_byte('f', file);
    cfax_byte('f', file);
    cfax_byte('f', file);
    cfax_byte(1, file);
    cfax_byte(0, file);
    cfax_word(0, file);
    cfax_word(0, file);
    cfax_word(20, file);
    cfax_dword(0, file);
    cfax_dword(0, file);
}

private void
cfax_page_hdr(gx_device_printer * pdev, FILE * file)
{
    cfax_byte(254, file);
    cfax_byte(16, file);
    cfax_byte((pdev->y_pixels_per_inch < 100 ? 0 : 1), file);
    cfax_byte(0, file);
    cfax_byte(0, file);
    cfax_byte(0, file);
    cfax_word(pdev->width, file);
    cfax_word(pdev->height, file);
    cfax_dword(0, file);
    cfax_dword(0, file);
}

private void
cfax_doc_end(FILE * file)
{
    cfax_byte(254, file);
    cfax_byte(0, file);
}

/* Send the page to the printer. */
private int
cfax_stream_print_page(gx_device_printer * pdev, FILE * prn_stream,
		       const stream_template * temp, stream_state * ss)
{
    gs_memory_t *mem = &gs_memory_default;
    stream_cursor_read r;
    stream_cursor_write w;
    int in_size = gdev_mem_bytes_per_scan_line((gx_device *) pdev);
    int code = 0, lnum, nbytes, i;
    byte *in;
    byte *out;
    byte *actual_data;

    /* If the file is 'nul', don't even do the writes. */
    bool nul = !strcmp(pdev->fname, "nul");

#undef OUT_SIZE
#define OUT_SIZE 1000

    /* Initialize the common part of the encoder state. */
    ss->template = temp;
    ss->memory = mem;

    /* Allocate the buffers. */
    in = gs_alloc_bytes(mem, temp->min_in_size + in_size + 1, "cfax_stream_print_page(in)");
    out = gs_alloc_bytes(mem, OUT_SIZE, "cfax_stream_print_page(out)");
    if (in == 0 || out == 0) {
	code = gs_note_error(gs_error_VMerror);
	goto done;
    }
    /* Process the image. */
    for (lnum = 0; lnum < pdev->height; lnum++) {
	code = (*temp->init) (ss);
	if (code < 0)
	    return_error(gs_error_limitcheck);
	gdev_prn_get_bits(pdev, lnum, in, &actual_data);
	r.ptr = actual_data - 1;
	r.limit = actual_data + in_size;
	w.ptr = out - 1;
	w.limit = w.ptr + OUT_SIZE - 1;
	(*temp->process) (ss, &r, &w, 1 /* always last line */ );
	nbytes = w.ptr + 1 - out;
	if (!nul) {
	    if (nbytes > 0) {
		if (nbytes < 217) {
		    cfax_byte(nbytes, prn_stream);
		    for (i = 0; i < nbytes; i++)
			cfax_byte(out[i], prn_stream);
		} else {
		    cfax_byte(0, prn_stream);
		    cfax_word(nbytes, prn_stream);
		    for (i = 0; i < nbytes; i++)
			cfax_byte(out[i], prn_stream);
		}
	    } else {
		cfax_byte(218, prn_stream);
	    }
	}
	if (temp->release != 0)
	    (*temp->release) (ss);
    }

  done:
    gs_free_object(mem, out, "cfax_stream_print_page(out)");
    gs_free_object(mem, in, "cfax_stream_print_page(in)");
    return code;
}

/* Begin a capi fax page. */
private int
cfax_begin_page(gx_device_printer * pdev, FILE * fp, int width)
{
    /* Patch the width to reflect fax page width adjustment. */
    int save_width = pdev->width;

    pdev->width = width;
    if (gdev_prn_file_is_new(pdev)) {
	cfax_doc_hdr(fp);
    }
    cfax_page_hdr(pdev, fp);

    pdev->width = save_width;
    return 0;
}

/* End a capi fax page. */
private int
cfax_end_page(gx_device_printer * pdev, FILE * fp)
{
    cfax_doc_end(fp);
    return 0;
}

/* Print an capi fax (sff-encoded) page. */
private int
cfax_print_page(gx_device_printer * pdev, FILE * prn_stream)
{
    stream_CFE_state state;
    int code;

    gdev_fax_init_fax_state(&state, (gx_device_fax *)pdev);
    state.EndOfLine = false;
    state.EndOfBlock = false;
    state.EncodedByteAlign = true;
    state.FirstBitLowOrder = true;
    state.K = 0;
    cfax_begin_page(pdev, prn_stream, state.Columns);
    code = cfax_stream_print_page(pdev, prn_stream, &s_CFE_template,
				  (stream_state *) & state);
    cfax_end_page(pdev, prn_stream);
    return code;
}
