/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* plfont.c */
/* PCL font handling library -- operations on entire fonts */
#include "memory_.h"
#include "stdio_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gschar.h"
#include "gsutil.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "plfont.h"
#include "plvalue.h"

/* agfa stuff */
#undef true
#undef false
#undef frac_bits
#include "cgconfig.h"           /* this must be first  */
#include "ufstport.h"           /* this must be second */
#include "shareinc.h"
#include "strmio.h"

#if UFST_VERSION_MAJOR >= 6 && UFST_VERSION_MINOR >= 2
#undef true
#undef false

#define false FALSE
#define true TRUE
#define UNICODE UFST_UNICODE
#endif

/* Structure descriptors */
private_st_pl_font();

/* Define accessors for unaligned, big-endian quantities. */
#define u16(bptr) pl_get_uint16(bptr)
#define s16(bptr) pl_get_int16(bptr)
/**** ASSUME uint >= 32 BITS ****/
#define u32(bptr) (uint)pl_get_uint32(bptr)

/* ---------------- Library callbacks ---------------- */

/* Fill in AGFA MicroType font boilerplate. */
void
pl_fill_in_mt_font(gs_font_base * pfont, SW16 data, long unique_id)
{
    gs_make_identity(&pfont->FontMatrix);
    pfont->FontType = ft_MicroType;
    pfont->BitmapWidths = true;
    pfont->ExactSize = fbit_use_outlines;
    pfont->InBetweenSize = fbit_use_outlines;
    pfont->TransformedChar = fbit_use_outlines;

    pfont->FontBBox.p.x = pfont->FontBBox.p.y =
        pfont->FontBBox.q.x = pfont->FontBBox.q.y = 0;
    uid_set_UniqueID(&pfont->UID, unique_id | (data << 16));
    pfont->encoding_index = 1;          /****** WRONG ******/
    pfont->nearest_encoding_index = 1;          /****** WRONG ******/
    pl_mt_init_procs(pfont);
}

/* Load a built-in AGFA MicroType font */
int
pl_load_mt_font(SW16 handle, gs_font_dir * pdir, gs_memory_t * mem,
                long unique_id, pl_font_t ** pplfont)
{
    gs_font_base *pfont = gs_alloc_struct(mem, gs_font_base,
                                          &st_gs_font_base,
                                          "pl_mt_load_font(gs_font_base)");
    pl_font_t *plfont = pl_alloc_font(mem, "pl_mt_load_font(pl_font_t)");
    int code;

    if (pfont == 0 || plfont == 0)
        code = gs_note_error(gs_error_VMerror);
    else {                      /* Initialize general font boilerplate. */
        code =
            pl_fill_in_font((gs_font *) pfont, plfont, pdir, mem,
                            "illegal_font");
        if (code >= 0) {        /* Initialize MicroType font boilerplate. */
            plfont->header = 0;
            plfont->header_size = 0;
            plfont->scaling_technology = plfst_MicroType;
            plfont->font_type = plft_Unicode;
            plfont->large_sizes = true;
            plfont->is_xl_format = false;
            plfont->allow_vertical_substitutes = false;
            pl_fill_in_mt_font(pfont, handle, unique_id);
            code = gs_definefont(pdir, (gs_font *) pfont);
        }
    }
    if (code < 0) {
        gs_free_object(mem, plfont, "pl_mt_load_font(pl_font_t)");
        gs_free_object(mem, pfont, "pl_mt_load_font(gs_font_base)");
        return code;
    }
    *pplfont = plfont;
    return 0;
}
