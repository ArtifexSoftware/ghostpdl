/* Copyright (C) 1994, 1996, 1999 Aladdin Enterprises.  All rights reserved.
  
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
/* Declarations for transfer function & similar cache remapping */

#ifndef icolor_INCLUDED
#  define icolor_INCLUDED

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
int zcolor_remap_one(P5(i_ctx_t *, const ref *, gx_transfer_map *,
			const gs_state *, op_proc_t));

/* Reload a cache with entries in [0..1] after sampling. */
int zcolor_remap_one_finish(P1(i_ctx_t *));

/* Reload a cache with entries in [-1..1] after sampling. */
int zcolor_remap_one_signed_finish(P1(i_ctx_t *));

/* Recompute the effective transfer functions and invalidate the current */
/* color after cache reloading. */
int zcolor_reset_transfer(P1(i_ctx_t *));

/* Invalidate the current color after cache reloading. */
int zcolor_remap_color(P1(i_ctx_t *));

#endif /* icolor_INCLUDED */
