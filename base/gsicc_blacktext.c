/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/*  Handling of color spaces stored during replacement of all
 *  text with black.
*/

#include "gsmemory.h"
#include "gsstruct.h"
#include "gzstate.h"
#include "gsicc_blacktext.h"

/* gsicc_blacktext_state_t is going to be storing GCed items
   (color spaces and client colors) and so will need to be GCed */
gs_private_st_ptrs4(st_blacktext_state, gsicc_blacktext_state_t,
    "gsicc_blacktext_state", blacktext_state_enum_ptrs,
    blacktext_state_reloc_ptrs, pcs[0], pcs[1], pcc[0], pcc[1]);

static void
rc_gsicc_blacktext_state_free(gs_memory_t *mem, void *ptr_in,
    client_name_t cname)
{
    gsicc_blacktext_state_t *state = (gsicc_blacktext_state_t*)ptr_in;

    rc_decrement_cs(state->pcs[0], "rc_gsicc_blacktext_state_free");
    rc_decrement_cs(state->pcs[1], "rc_gsicc_blacktext_state_free");

    gs_free_object(mem->stable_memory, state,
        "rc_gsicc_blacktext_state_free");
}

gsicc_blacktext_state_t*
gsicc_blacktext_state_new(gs_memory_t *memory)
{
    gsicc_blacktext_state_t *result;

    result = gs_alloc_struct(memory->stable_memory, gsicc_blacktext_state_t,
        &st_blacktext_state, "gsicc_blacktext_state_new");
    if (result == NULL)
        return NULL;
    rc_init_free(result, memory->stable_memory, 1, rc_gsicc_blacktext_state_free);
    result->memory = memory;
    result->pcs[0] = NULL;
    result->pcs[1] = NULL;
    result->pcc[0] = NULL;
    result->pcc[1] = NULL;

    return result;
}

void
gsicc_restore_black_text(gs_gstate *pgs)
{
    gsicc_blacktext_state_t *state = pgs->black_text_state;
    int code;

    if (state == NULL)
        return;

    /* Make sure state and original are same fill_color condition */
    if (state->rc.ref_count == 1) {
        if ((state->is_fill && pgs->is_fill_color) ||
            (!state->is_fill && !pgs->is_fill_color)) {

            if ((code = gs_setcolorspace_only(pgs, pgs->black_text_state->pcs[0])) >= 0) {
                /* current client color is gray.  no need to decrement */
                pgs->color[0].ccolor = pgs->black_text_state->pcc[0];
                pgs->color[0].ccolor->paint.values[0] = pgs->black_text_state->value[0];
            }
            gs_swapcolors_quick(pgs);
            if ((code = gs_setcolorspace_only(pgs, pgs->black_text_state->pcs[1])) >= 0) {
                pgs->color[0].ccolor = pgs->black_text_state->pcc[1];
                pgs->color[0].ccolor->paint.values[0] = pgs->black_text_state->value[1];

            }
            gs_swapcolors_quick(pgs);

        } else {

            if ((code = gs_setcolorspace_only(pgs, pgs->black_text_state->pcs[1])) >= 0) {
                pgs->color[0].ccolor = pgs->black_text_state->pcc[1];
                pgs->color[0].ccolor->paint.values[0] = pgs->black_text_state->value[1];
            }
            gs_swapcolors_quick(pgs);
            if ((code = gs_setcolorspace_only(pgs, pgs->black_text_state->pcs[0])) >= 0) {
                pgs->color[0].ccolor = pgs->black_text_state->pcc[0];
                pgs->color[0].ccolor->paint.values[0] = pgs->black_text_state->value[0];
            }
            gs_swapcolors_quick(pgs);
        }
        gx_unset_dev_color(pgs);
        gx_unset_alt_dev_color(pgs);
    }
    rc_decrement(state, "gsicc_restore_black_text");
    pgs->black_text_state = NULL;
}