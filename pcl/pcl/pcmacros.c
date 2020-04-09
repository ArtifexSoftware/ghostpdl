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


/* pcmacros.c */
/* PCL5 macro commands */
#include "stdio_.h"             /* std.h + NULL */
#include "pcommand.h"
#include "pgmand.h"
#include "pcstate.h"
#include "pcparse.h"

/* ---------------- Macro execution ---------------- */

#ifdef DEBUG
static void
pcl_dump_current_macro(pcl_state_t * pcs, const char *msg)
{
    int i;

    dmputs(pcs->memory, msg);
    dmputs(pcs->memory, " ");
    for (i = 0; i < current_macro_id_size; i++)
        dmprintf1(pcs->memory, "%02x", current_macro_id[i]);
    dmputs(pcs->memory, "\n");
    return;
}
#endif

/* Execute a macro.  We're willing to use C recursion for this because */
/* the PCL5 specification only allows 2 levels of call. */
static int
do_copies(pcl_state_t * psaved, pcl_state_t * pcs, pcl_copy_operation_t copy)
{
    const pcl_init_t **init = pcl_init_table;
    int code = 0;

    for (; *init && code >= 0; ++init)
        if ((*init)->do_copy)
            code = (*(*init)->do_copy) (psaved, pcs, copy);
    return code;
}
int
pcl_execute_macro(const pcl_macro_t * pmac, pcl_state_t * pcs,
                  pcl_copy_operation_t before, pcl_reset_type_t reset,
                  pcl_copy_operation_t after)
{
    pcl_parser_state_t state;
    hpgl_parser_state_t gstate;
    pcl_state_t saved;
    stream_cursor_read r;
    int code;

#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_dump_current_macro(pcs, "Starting macro execution");
    }
#endif

    if (before) {
        memcpy(&saved, pcs, sizeof(*pcs));
        code = do_copies(&saved, pcs, before);
        if (code < 0)
            return code;
        pcs->saved = &saved;
    }
    if (reset) {
        code = pcl_do_resets(pcs, reset);
        if (code < 0)
            return code;
    }
    state.definitions = pcs->pcl_commands;
    state.hpgl_parser_state = &gstate;
    code = pcl_process_init(&state, pcs);
    if (code < 0)
        return code;
    r.ptr = (const byte *)(pmac + 1) - 1;
    r.limit = (const byte *)pmac + (gs_object_size(pcs->memory, pmac) - 1);
    pcs->macro_level++;
    code = pcl_process(&state, pcs, &r);
    pcs->macro_level--;
    if (after) {
        int errcode = do_copies(&saved, pcs, after);
        if (errcode < 0)
            return errcode;
        memcpy(pcs, &saved, sizeof(*pcs));
    }
    if (code < 0)
        return code;
#ifdef DEBUG
    if (gs_debug_c('i')) {
        pcl_dump_current_macro(pcs, "Finished macro execution");
    }
#endif

    return code;
}

/* ---------------- Commands ---------------- */

/* Define the macro control operations. */
enum
{
    macro_end_definition = 1,
    macro_execute = 2,
    macro_call = 3,
    macro_delete_temporary = 7
};

static int                      /* ESC & f <mc_enum> X */
pcl_macro_control(pcl_args_t * pargs, pcl_state_t * pcs)
{
    int i = int_arg(pargs);
    gs_const_string key;
    void *value;
    pl_dict_enum_t denum;

    if (i == macro_end_definition) {
        if (pcs->defining_macro) {      /* The parser has included everything through this command */
            /* in the macro_definition, so we can finish things up. */
            int code;

            code = pl_dict_put(&pcs->macros, current_macro_id,
                               current_macro_id_size, pcs->macro_definition);
            pcs->defining_macro = false;
            pcs->macro_definition = 0;
#ifdef DEBUG
            if (gs_debug_c('i')) {
                pcl_dump_current_macro(pcs, "[i] Ending Macro");
            }
#endif
            return code;
        }
    } else if (pcs->defining_macro)
        return 0;               /* don't execute other macro operations */
    else if (i == macro_execute || i == macro_call) {   /*
                                                         * TRM 12-9 says that "two levels of nesting are allowed",
                                                         * which means 3 levels of macros (1 non-nested, 2 nested).
                                                         */
        if (pcs->macro_level > 2)
            return 0;
    } else if (pcs->macro_level)
        return e_Range;         /* macro operations not allowed inside macro */
    switch (i) {
        case 0:
            {                   /* Start defining <macro_id>. */
                pcl_macro_t *pmac;

                pmac = (pcl_macro_t *)
                    gs_alloc_bytes(pcs->memory, sizeof(pcl_macro_t),
                                   "begin macro definition");
                if (pmac == 0)
                    return_error(e_Memory);
                pmac->storage = pcds_temporary;
                pcs->macro_definition = (byte *) pmac;
                pcs->defining_macro = true;
#ifdef DEBUG
                if (gs_debug_c('i')) {
                    pcl_dump_current_macro(pcs, "[i] Starting Macro");
                }
#endif
                return 0;
            }
        case macro_end_definition:     /* 1 */
            {                   /* Stop defining macro.  Outside a macro, this is an error. */
                return e_Range;
            }
        case macro_execute:    /* 2 */
            {                   /* Execute <macro_id>. */
                void *value;

                if (!pl_dict_find
                    (&pcs->macros, current_macro_id, current_macro_id_size,
                     &value)
                    )
                    return 0;
                return pcl_execute_macro((const pcl_macro_t *)value, pcs,
                                         pcl_copy_none, pcl_reset_none,
                                         pcl_copy_none);
            }
        case macro_call:       /* 3 */
            {                   /* Call <macro_id>, saving and restoring environment. */
                void *value;

                if (!pl_dict_find
                    (&pcs->macros, current_macro_id, current_macro_id_size,
                     &value)
                    )
                    return 0;
                return pcl_execute_macro((const pcl_macro_t *)value, pcs,
                                         pcl_copy_before_call, pcl_reset_none,
                                         pcl_copy_after_call);
            }
        case 4:
            {                   /* Define <macro_id> as automatic overlay. */
                pcs->overlay_macro_id = pcs->macro_id;
                pcs->overlay_enabled = true;
                return 0;
            }
        case 5:
            {                   /* Cancel automatic overlay. */
                pcs->overlay_enabled = false;
                return 0;
            }
        case 6:
            {                   /* Delete all macros. */
                pl_dict_release(&pcs->macros);
                return 0;
            }
        case macro_delete_temporary:   /* 7 */
            {                   /* Delete temporary macros. */
                pl_dict_enum_stack_begin(&pcs->macros, &denum, false);
                while (pl_dict_enum_next(&denum, &key, &value))
                    if (((pcl_macro_t *) value)->storage == pcds_temporary)
                        pl_dict_undef_purge_synonyms(&pcs->macros, key.data,
                                                     key.size);
                return 0;
            }
        case 8:
            {                   /* Delete <macro_id>. */
                pl_dict_undef_purge_synonyms(&pcs->macros, current_macro_id,
                                             current_macro_id_size);
                return 0;
            }
        case 9:
            {                   /* Make <macro_id> temporary. */
                if (pl_dict_find
                    (&pcs->macros, current_macro_id, current_macro_id_size,
                     &value))
                    ((pcl_macro_t *) value)->storage = pcds_temporary;
                return 0;
            }
        case 10:
            {                   /* Make <macro_id> permanent. */
                if (pl_dict_find
                    (&pcs->macros, current_macro_id, current_macro_id_size,
                     &value))
                    ((pcl_macro_t *) value)->storage = pcds_permanent;
                return 0;
            }
        default:
            return e_Range;
    }
}

static int                      /* ESC & f <id> Y */
pcl_assign_macro_id(pcl_args_t * pargs, pcl_state_t * pcs)
{
    uint id = uint_arg(pargs);

    id_set_value(pcs->macro_id, id);
    pcs->macro_id_type = numeric_id;
    return 0;
}

/* Initialization */
static int
pcmacros_do_registration(pcl_parser_state_t * pcl_parser_state,
                         gs_memory_t * mem)
{                               /* Register commands */
    DEFINE_CLASS('&') {
    'f', 'X',
            PCL_COMMAND("Macro Control", pcl_macro_control,
                            pca_neg_error | pca_big_error | pca_in_macro)}, {
    'f', 'Y',
            PCL_COMMAND("Assign Macro ID", pcl_assign_macro_id,
                            pca_neg_error | pca_big_error)},
        END_CLASS return 0;
}
static int
pcmacros_do_reset(pcl_state_t * pcs, pcl_reset_type_t type)
{
    if (type & (pcl_reset_initial | pcl_reset_printer)) {
        int code = 0;

        pcs->overlay_enabled = false;
        pcs->macro_level = 0;
        pcs->defining_macro = false;
        pcs->saved = 0;
        pcs->macro_definition = 0;

        if (type & pcl_reset_initial)
            pl_dict_init(&pcs->macros, pcs->memory, NULL);
        else {
            pcl_args_t args;

            arg_set_uint(&args, macro_delete_temporary);
            code = pcl_macro_control(&args, pcs);
            if (pcs->alpha_macro_id.id != 0)
                gs_free_object(pcs->memory,
                               pcs->alpha_macro_id.id, "pcmacros_do_reset");
            if (code < 0)
                return code;
        }
    }

    if (type &
        (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay |
         pcl_reset_permanent)) {
        pcs->alpha_macro_id.size = 0;
        pcs->macro_id_type = numeric_id;
        id_set_value(pcs->macro_id, 0);
        pcs->alpha_macro_id.id = 0;
    }
    if (type & pcl_reset_permanent) {
        gs_free_object(pcs->memory, pcs->macro_definition, "begin macro definition");
        pl_dict_release(&pcs->macros);
    }

    return 0;
}
static int
pcmacros_do_copy(pcl_state_t * psaved, const pcl_state_t * pcs,
                 pcl_copy_operation_t operation)
{
    if (operation & pcl_copy_after) {   /* Don't restore the macro dictionary. */
        psaved->macros = pcs->macros;
    }
    return 0;
}
const pcl_init_t pcmacros_init = {
    pcmacros_do_registration, pcmacros_do_reset, pcmacros_do_copy
};
