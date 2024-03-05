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


/* setcolorscreen operator */
#include "ghost.h"
#include "memory_.h"
#include "oper.h"
#include "estack.h"
#include "gsstruct.h"		/* must precede igstate.h, */
                                        /* because of #ifdef in gsht.h */
#include "ialloc.h"
#include "igstate.h"
#include "gsmatrix.h"
#include "gxdevice.h"		/* for gzht.h */
#include "gzht.h"
#include "gsstate.h"
#include "iht.h"
#include "store.h"

/* Dummy spot function */
static float
spot_dummy(double x, double y)
{
    return (x + y) / 2;
}

/* <red_freq> ... <gray_proc> setcolorscreen - */
static int setcolorscreen_finish(i_ctx_t *);
static int setcolorscreen_cleanup(i_ctx_t *);
static int
zsetcolorscreen(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    gs_colorscreen_halftone cscreen;
    ref sprocs[4];
    gs_halftone *pht;
    gx_device_halftone *pdht;
    int i;
    int code = 0;
    int space = 0;
    gs_memory_t *mem;
    gs_memory_t *currmem = (gs_memory_t *)idmemory->current;

    for (i = 0; i < 4; i++) {
        os_ptr op1 = op - 9 + i * 3;
        int code = zscreen_params(op1, &cscreen.screens.indexed[i]);

        if (code < 0)
            return code;
        cscreen.screens.indexed[i].spot_function = spot_dummy;
        sprocs[i] = *op1;
        space = max(space, r_space_index(op1));
    }
    mem = (gs_memory_t *)idmemory->spaces_indexed[space];
    /* We must have the currentglobal consistent through the sampling process */
    ialloc_set_space(idmemory, (mem == (gs_memory_t *)idmemory->spaces.memories.named.global ? avm_global : avm_local));
    check_estack(9);		/* for sampling screens */
    rc_alloc_struct_0(pht, gs_halftone, &st_halftone,
                      mem, pht = 0, "setcolorscreen(halftone)");
    rc_alloc_struct_0(pdht, gx_device_halftone, &st_device_halftone,
                      mem, pdht = 0, "setcolorscreen(device halftone)");
    if (pht == 0 || pdht == 0)
        code = gs_note_error(gs_error_VMerror);
    else {
        pht->type = ht_type_colorscreen;
        pht->objtype = HT_OBJTYPE_DEFAULT;
        pht->params.colorscreen = cscreen;
        code = gs_sethalftone_prepare(igs, pht, pdht);
    }
    if (code >= 0) {		/* Schedule the sampling of the screens. */
        es_ptr esp0 = esp;	/* for backing out */

        esp += 9;
        make_mark_estack(esp - 8, es_other, setcolorscreen_cleanup);
        make_bool(esp - 7, (currmem == (gs_memory_t *)idmemory->spaces.memories.named.global));
        memcpy(esp - 6, sprocs, sizeof(ref) * 4);	/* procs */
        make_istruct(esp - 2, 0, pht);
        make_istruct(esp - 1, 0, pdht);
        make_op_estack(esp, setcolorscreen_finish);
        for (i = 0; i < 4; i++) {
            /* Shuffle the indices to correspond to */
            /* the component order. */
            code = zscreen_enum_init(i_ctx_p,
                                     &pdht->components[(i + 1) & 3].corder,
                                &pht->params.colorscreen.screens.indexed[i],
                                     &sprocs[i], 0, 0, space);
            if (code < 0) {
                esp = esp0;
                break;
            }
        }
    }
    if (code < 0) {
        gs_free_object(mem, pdht, "setcolorscreen(device halftone)");
        gs_free_object(mem, pht, "setcolorscreen(halftone)");
        return code;
    }
    pop(12);
    return o_push_estack;
}
/* Install the color screen after sampling. */
static int
setcolorscreen_finish(i_ctx_t *i_ctx_p)
{
    gx_device_halftone *pdht = r_ptr(esp, gx_device_halftone);
    int code;

    pdht->order = pdht->components[0].corder;
    code = gx_ht_install(igs, r_ptr(esp - 1, gs_halftone), pdht);
    if (code < 0) {
        /* We need the stack correct for setcolorscreen_cleanup() but we need it back
           where we started before we return the error.
         */
        es_ptr esp0 = esp;
        esp -= 8;
        setcolorscreen_cleanup(i_ctx_p);
        esp = esp0;
        return code;
    }
    istate->screen_procs.red   = esp[-5];
    istate->screen_procs.green = esp[-4];
    istate->screen_procs.blue  = esp[-3];
    istate->screen_procs.gray  = esp[-2];
    make_null(&istate->halftone);
    esp -= 8;
    setcolorscreen_cleanup(i_ctx_p);
    return o_pop_estack;
}
/* Clean up after installing the color screen. */
static int
setcolorscreen_cleanup(i_ctx_t *i_ctx_p)
{
    gs_halftone *pht = r_ptr(esp + 7, gs_halftone);
    gx_device_halftone *pdht = r_ptr(esp + 8, gx_device_halftone);
    bool global = (esp + 2)->value.boolval;

    gs_free_object(pdht->rc.memory, pdht,
                   "setcolorscreen_cleanup(device halftone)");
    gs_free_object(pht->rc.memory, pht,
                   "setcolorscreen_cleanup(halftone)");
    /* See bug #707007, explicitly freed structures on the stacks need to be made NULL */
    make_null(esp + 6);
    make_null(esp + 7);
    ialloc_set_space(idmemory, (global ? avm_global : avm_local));
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zht1_op_defs[] =
{
    {"<setcolorscreen", zsetcolorscreen},
                /* Internal operators */
    {"0%setcolorscreen_finish", setcolorscreen_finish},
    op_def_end(0)
};
