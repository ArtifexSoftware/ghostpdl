/* Copyright (C) 2015-2018 Artifex Software, Inc.
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

#include "claptrap.h"
#include "claptrap-impl.h"

ClapTrap *ClapTrap_Init(gs_memory_t     *mem,
                        int              width,
                        int              height,
                        int              num_comps,
                        const int       *comp_order,
                        int              max_x_offset,
                        int              max_y_offset,
                        ClapTrap_LineFn *get_line,
                        void            *get_line_arg)
{
    ClapTrap *ct;

    ct = (ClapTrap *)gs_alloc_bytes(mem, sizeof(*ct), "ClapTrap");
    if (ct == NULL)
        return NULL;

    ct->width        = width;
    ct->height       = height;
    ct->num_comps    = num_comps;
    ct->comp_order   = comp_order;
    ct->max_x_offset = max_x_offset;
    ct->max_y_offset = max_y_offset;
    ct->get_line     = get_line;
    ct->get_line_arg = get_line_arg;
    ct->lines_in_buf = max_y_offset * 2 + 1;
    ct->lines_read   = 0;
    ct->y            = 0;
    ct->span         = width * num_comps;

    ct->linebuf      = gs_alloc_bytes(mem, ct->span * ct->lines_in_buf, "ClapTrap linebuf");
    ct->process      = gs_alloc_bytes(mem, ct->width * ct->lines_in_buf, "ClapTrap process");
    if (ct->linebuf == NULL || ct->process == NULL)
    {
        gs_free_object(mem, ct->linebuf, "ClapTrap linebuf");
        gs_free_object(mem, ct->process, "ClapTrap process");
        gs_free_object(mem, ct, "ClapTrap");
        return NULL;
    }

    return ct;
}

void ClapTrap_Fin(gs_memory_t *mem, ClapTrap *ct)
{
    if (ct)
    {
        gs_free_object(mem, ct->linebuf, "ClapTrap linebuf");
        gs_free_object(mem, ct->process, "ClapTrap process");
    }
    gs_free_object(mem, ct, "ClapTrap");
}
