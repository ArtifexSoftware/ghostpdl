/* Copyright (C) 2001-2023 Artifex Software, Inc.
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


/* Character writing "operators" for Ghostscript library */
#include "gx.h"
#include "gserrors.h"
#include "gsstruct.h"
#include "gsmatrix.h"		/* for gscoord.h */
#include "gscoord.h"		/* for gs_idtransform */
#include "gzstate.h"
#include "gxdevice.h"
#include "gxdevmem.h"
#include "gxchar.h"
#include "gxfont.h"

/* Structure descriptors */
extern_st(st_gs_show_enum);

/* ------ String writing operators ------ */

/* Free the contents of a show enumerator. */
void
gs_show_enum_release(gs_show_enum * penum, gs_memory_t * emem)
{
    if (penum->text.operation)	/* otherwise, never initialized */
        penum->procs->release((gs_text_enum_t *)penum, "gs_show_enum_release");
    if (emem != 0)
        gs_free_object(emem, penum, "gs_show_enum_release");
}

/* ------ Width/cache operators ------ */

/* setcachedevice */
/* The elements of pw are: wx, wy, llx, lly, urx, ury. */
/* Note that this returns 1 if we just set up the cache device. */
int
gs_setcachedevice_double(gs_show_enum *penum, gs_gstate *pgs, const double *pw)
{
    if (penum->pgs != pgs)
        return_error(gs_error_rangecheck);
    return gs_text_setcachedevice((gs_text_enum_t *)penum, pw);
}
/* The _float procedure is strictly for backward compatibility. */
int
gs_setcachedevice_float(gs_show_enum * penum, gs_gstate * pgs, const float *pw)
{
    double w[6];
    int i;

    for (i = 0; i < 6; ++i)
        w[i] = pw[i];
    return gs_setcachedevice_double(penum, pgs, w);
}

/* setcachedevice2 */
/* The elements of pw2 are: w0x, w0y, llx, lly, urx, ury, w1x, w1y, vx, vy. */
/* Note that this returns 1 if we just set up the cache device. */
int
gs_setcachedevice2_double(gs_show_enum * penum, gs_gstate * pgs,
                          const double *pw2)
{
    if (penum->pgs != pgs)
        return_error(gs_error_rangecheck);
    return gs_text_setcachedevice2((gs_text_enum_t *)penum, pw2);
}
/* The _float procedure is strictly for backward compatibility. */
int
gs_setcachedevice2_float(gs_show_enum * penum, gs_gstate * pgs, const float *pw2)
{
    double w2[10];
    int i;

    for (i = 0; i < 10; ++i)
        w2[i] = pw2[i];
    return gs_setcachedevice2_double(penum, pgs, w2);
}

/* setcharwidth */
/* Note that this returns 1 if the current show operation is */
/* non-displaying (stringwidth or cshow). */
int
gs_setcharwidth(gs_show_enum * penum, gs_gstate * pgs,
                double wx, double wy)
{
    double w[2];

    if (penum->pgs != pgs)
        return_error(gs_error_rangecheck);
    w[0] = wx, w[1] = wy;
    return gs_text_setcharwidth((gs_text_enum_t *)penum, w);
}

/* ------ Enumerator ------ */

/* Do the next step of a show (or stringwidth) operation */
int
gs_show_next(gs_show_enum * penum)
{
    return gs_text_process((gs_text_enum_t *)penum);
}

/*
 * Return true if we only need the width from the rasterizer
 * and can short-circuit the full rendering of the character,
 * false if we need the actual character bits.
 */
bool
gs_show_width_only(const gs_show_enum * penum)
{
    return gs_text_is_width_only((const gs_text_enum_t *)penum);
}

/* ------ Accessors ------ */

/* Return the current character for rendering. */
gs_char
gs_show_current_char(const gs_show_enum * penum)
{
    return gs_text_current_char((const gs_text_enum_t *)penum);
}

/* Return the current glyph for rendering. */
gs_glyph
gs_show_current_glyph(const gs_show_enum * penum)
{
    return gs_text_current_glyph((const gs_text_enum_t *)penum);
}

/* Return the width of the just-enumerated character (for cshow). */
int
gs_show_current_width(const gs_show_enum * penum, gs_point * ppt)
{
    return gs_text_current_width((const gs_text_enum_t *)penum, ppt);
}

/* Return the just-displayed character for kerning. */
gs_char
gs_kshow_previous_char(const gs_show_enum * penum)
{
    return gs_text_current_char((const gs_text_enum_t *)penum);
}

/* Return the about-to-be-displayed character for kerning. */
gs_char
gs_kshow_next_char(const gs_show_enum * penum)
{
    return penum->text.data.bytes[penum->index];
}

/* Return the accumulated width for stringwidth. */
void
gs_show_width(const gs_show_enum * penum, gs_point * ppt)
{
    gs_text_total_width((const gs_text_enum_t *)penum, ppt);
}

/* ------ Internal routines ------ */

#if 0
/*
 * Force the enumerator to be a gs_show_enum *, which the current
 * implementation code requires.
 */
static int
show_n_begin(gs_show_enum *penum, gs_gstate *pgs, int code, gs_text_enum_t *pte)
{
    if (code < 0)
        return code;
    if (gs_object_type(pgs->memory, pte) != &st_gs_show_enum) {
        /* Use the default implementation. */
        gx_device *dev = pgs->device;
        gs_text_params_t text;
        gs_memory_t *mem = pte->memory;
        dev_proc_text_begin((*text_begin)) = dev_proc(dev, text_begin);

        text = pte->text;
        gs_text_release(NULL, pte, "show_n_begin");
        /* Temporarily reset the text_begin procedure to the default. */
        set_dev_proc(dev, text_begin, gx_default_text_begin);
        code = gs_text_begin(pgs, &text, mem, &pte);
        set_dev_proc(dev, text_begin, text_begin);
        if (code < 0)
            return code;
    }
    /* Now we know pte points to a gs_show_enum. */
    *penum = *(gs_show_enum *)pte;
    gs_free_object(pgs->memory, pte, "show_n_begin");
    return code;
}
#endif
