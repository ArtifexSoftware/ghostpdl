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


/* Declarations for transfer function & similar cache remapping */

#ifndef icolor_INCLUDED
#  define icolor_INCLUDED

#include "iref.h"
#include "gsgstate.h"
#include "gxtmap.h"

/*
 * Define the number of stack slots needed for zcolor_remap_one.
 * The client is responsible for doing check_e/ostack or the equivalent
 * before calling zcolor_remap_one.
 */
extern const int zcolor_remap_one_ostack;
extern const int zcolor_remap_one_estack;

/*
 * Schedule the sampling and reloading of a cache.  Note that if
 * zcolor_remap_one recognize the procedure as being of a special form, it
 * may not schedule anything, but it still returns o_push_estack.  (This is
 * a change as of release 5.95; formerly, it returned 0 in this case.)
 */
int zcolor_remap_one(i_ctx_t *, const ref *, gx_transfer_map *,
                     const gs_gstate *, op_proc_t);

int transfer_remap_red_finish(i_ctx_t *i_ctx_p);
int transfer_remap_green_finish(i_ctx_t *i_ctx_p);
int transfer_remap_blue_finish(i_ctx_t *i_ctx_p);
int transfer_remap_gray_finish(i_ctx_t *);
int transfer_remap_one_finish(i_ctx_t *i_ctx_p);
int setblackgeneration_remap_one_finish(i_ctx_t *);
/* Reload a cache with entries in [0..1] after sampling. */
int zcolor_remap_one_finish(i_ctx_t *);

int setundercolor_remap_one_signed_finish(i_ctx_t *);
/* Reload a cache with entries in [-1..1] after sampling. */
int zcolor_remap_one_signed_finish(i_ctx_t *);

/* Recompute the effective transfer functions and invalidate the current */
/* color after cache reloading. */
int zcolor_reset_transfer(i_ctx_t *);

/* Invalidate the current color after cache reloading. */
int zcolor_remap_color(i_ctx_t *);

#endif /* icolor_INCLUDED */
