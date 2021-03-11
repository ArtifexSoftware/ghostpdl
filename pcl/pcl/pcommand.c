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


/* pcommand.c - Utilities for PCL 5 commands */

#include "std.h"
#include "memory_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsmatrix.h"
#include "gxstate.h"
#include "gsdevice.h"
#include "pcindxed.h"
#include "pcommand.h"
#include "pcparse.h"
#include "pcstate.h"
#include "pcparam.h"
#include "pcident.h"
#include "pgmand.h"             /* temporary */
/*
 * Get the command argument as an int, uint, or float.
 */
int
int_value(const pcl_value_t * pv)
{
    return (int)(value_is_neg(pv) ? -(int)pv->i : pv->i);
}

uint
uint_value(const pcl_value_t * pv)
{
    return pv->i;
}

float
float_value(const pcl_value_t * pv)
{
    return (value_is_float(pv) ?
            (float)(value_is_neg(pv) ? -(int)pv->i - pv->fraction
                    : pv->i + pv->fraction)
            : (float)int_value(pv));
}

/*
 * "put" parameters to the device.
 */
static int
transfer_param1(gs_c_param_list * alist, pcl_state_t * pcs)
{
    gs_c_param_list_read(alist);

    return gs_gstate_putdeviceparams(pcs->pgs, gs_currentdevice(pcs->pgs),
                                      (gs_param_list *) alist);
}

/*
 * Set a Boolean parameter.
 */
int
put_param1_bool(pcl_state_t * pcs, gs_param_name pkey, bool value)
{
    gs_c_param_list list;
    int code;

    gs_c_param_list_write(&list, pcs->memory);
    code = param_write_bool((gs_param_list *) & list, pkey, &value);
    if (code >= 0)
        code = transfer_param1(&list, pcs);

    gs_c_param_list_release(&list);
    return code;
}

/*
 * Set a float parameter.
 */
int
put_param1_float(pcl_state_t * pcs, gs_param_name pkey, double value)
{
    gs_c_param_list list;
    float fval = value;
    int code;

    gs_c_param_list_write(&list, pcs->memory);
    code = param_write_float((gs_param_list *) & list, pkey, &fval);
    if (code >= 0)
        code = transfer_param1(&list, pcs);

    gs_c_param_list_release(&list);
    return code;
}

/*
 * Set an integer parameter.
 */
int
put_param1_int(pcl_state_t * pcs, gs_param_name pkey, int value)
{
    gs_c_param_list list;
    int code;

    gs_c_param_list_write(&list, pcs->memory);
    code = param_write_int((gs_param_list *) & list, pkey, &value);
    if (code >= 0)
        code = transfer_param1(&list, pcs);

    gs_c_param_list_release(&list);
    return code;
}

/*
 * Set a parameter consisting of an array of two floats. This is used to pass
 * the paper size parameter to the device.
 */
int
put_param1_float_array(pcl_state_t * pcs, gs_param_name pkey, float pf[2]
    )
{
    gs_c_param_list list;
    gs_param_float_array pf_array;
    int code = 0;

    pf_array.data = pf;
    pf_array.size = 2;
    pf_array.persistent = false;

    gs_c_param_list_write(&list, pcs->memory);
    code = param_write_float_array((gs_param_list *) & list, pkey, &pf_array);
    if (code >= 0)
        code = transfer_param1(&list, pcs);

    gs_c_param_list_release(&list);
    return code;
}

int
put_param1_string(pcl_state_t * pcs, gs_param_name pkey, const char *str)
{
    gs_c_param_list list;
    gs_param_string paramstr;
    int code;

    gs_c_param_list_write(&list, pcs->memory);
    param_string_from_string(paramstr, str);
    code = param_write_string((gs_param_list *) & list, pkey, &paramstr);
    if (code >= 0)
        code = transfer_param1(&list, pcs);

    gs_c_param_list_release(&list);
    return code;
}

/* initilialize the parser states */
int
pcl_do_registrations(pcl_state_t * pcs, pcl_parser_state_t * pst)
{
    const pcl_init_t **init;
    int code;

    /* initialize gl/2 command counter */
    hpgl_init_command_index(&pst->hpgl_parser_state, pcs->memory);
    pcs->parse_data = pst->hpgl_parser_state;
    /* initialize pcl's command counter */
    code = pcl_init_command_index(pst, pcs);
    if (code < 0) {
        if (pst->hpgl_parser_state != NULL)
            gs_free_object(pcs->memory, pst->hpgl_parser_state, "hpgl_init_command_index");
        return code;
    }
    for (init = pcl_init_table; *init; ++init) {
        if ((*init)->do_registration) {
            code = (*(*init)->do_registration) (pst, pcs->memory);
            if (code < 0) {
                lprintf1("Error %d during initialization!\n", code);
                return code;
            }
        }
    }
    return 0;
}

/* Similar to pcl_do_resets() but doesn't teminate loop immediately after
error. */
static int
pcl_do_resets_permanent(pcl_state_t * pcs)
{
    const pcl_init_t **init = pcl_init_table;
    int code = 0;

    for (; *init; ++init) {
        if ((*init)->do_reset) {
            int code2 = (*(*init)->do_reset) (pcs, pcl_reset_permanent);
            if (code2 < 0 && code >= 0) {
                code = code2;
            }
        }
    }
    return code;
}

/*
 * Run the reset code of all the modules.
 */
int
pcl_do_resets(pcl_state_t * pcs, pcl_reset_type_t type)
{
    const pcl_init_t **init = pcl_init_table;
    int code = 0;
    if (type == pcl_reset_permanent) {
        /* We do not want early termination of the loop. */
        return pcl_do_resets_permanent(pcs);
    }

    for (; *init && code >= 0; ++init) {
        if ((*init)->do_reset)
            code = (*(*init)->do_reset) (pcs, type);
    }
    if (code < 0) {
        (void) pcl_do_resets_permanent(pcs);
    }
    return code;
}

/*
 * "Cold start" initialization of the graphic state. This is provided as a
 * special routine to avoid (as much as possible) order depedencies in the
 * various reset routines used by individual modules. Some of the values
 * selected may be subsequently overridden by the reset routines; this code
 * just attempts to set them to reasonable values.
 */
void
pcl_init_state(pcl_state_t * pcs, gs_memory_t * pmem)
{
    /* some elementary fields */
    pcs->memory = pmem;
    pcs->num_copies = 1;
    pcs->output_bin = 1;
    pcs->perforation_skip = 1;

    pcs->font_id_type = numeric_id;
    pcs->macro_id_type = numeric_id;

    pcs->rotate_patterns = true;
    pcs->source_transparent = true;
    pcs->pattern_transparent = true;

    pcs->logical_op = 252;

    pcs->monochrome_mode = 0;
    pcs->render_mode = 3;

    pcs->next_id = 8UL;
    pcl_init_gstate_stk(pcs);
    pcs->configure_appletalk = 0;
    pcs->uom_cp = 7200L / 300L;
    pcs->palette_stack = 0;
    pcs->pdflt_palette = 0;
    pcs->pdflt_frgrnd = 0;
    pcs->pdflt_ht = 0;
    pcs->page_marked = false;
    pcs->cursor_moved = false;
    pcl_cs_base_init(pcs);
    pcl_cs_indexed_init(pcs);

    pcs->cap.x = pcs->cap.y = 0;
    pcs->vmi_cp = 0;
    pcs->halftone_set = false;
    pcs->ppaper_type_table = 0;

    pcs->hpgl_mode = -1;
}

#ifdef DEBUG
void
pcl_debug_dump_data(gs_memory_t *mem, const byte *d, int len)
{
    int i;

    dmprintf(mem, "BEGIN RAW DATA\n");
    for (i = 0; i < len; i++) {
        dmprintf2(mem, "%02x%c",
                  d[i],
                  (i % 16 == 15 || i == len - 1) ? '\n' : ' ');
    }
    dmprintf(mem, "END RAW DATA\n");
    return;
}

#endif
