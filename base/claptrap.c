/* Copyright (C) 2015-16 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "claptrap.h"
#include "claptrap-impl.h"

/* This is the actual guts of the per-pixel processing.
 * We use a static inline, so the compiler can optimise
 * out as many of the tests as possible. */
inline static void process_at_pixel(ClapTrap      * restrict ct,
                                    unsigned char * restrict buffer,
                                    int            x,
                                    int            clips_on_x,
                                    int            clips_on_y,
                                    int            first_comp,
                                    int            last_comp,
                                    int            prev_comp,
                                    int            comp,
                                    int            line_offset)
{
    /* We look at the pixel values on comp.
     * We look at the process values passed into us from prev_comp, and pass out
     * into comp.
     */

    /* Use local vars to avoid pointer aliasing */
    int            width        = ct->width;
    int            height       = ct->height;
    int            num_comps    = ct->num_comps;
    int            max_x_offset = ct->max_x_offset;
    int            max_y_offset = ct->max_y_offset;
    int            span         = ct->span;
    int            lines_in_buf = ct->lines_in_buf;
    unsigned char *linebuf      = ct->linebuf;
    unsigned char *process      = ct->process;
    int            y            = ct->y;
    /* Some offsets we will use repeatedly */
    int            o            = x * num_comps;
    int            oc           = o + comp;
    /* p != 0 if we need to be processed because a previous component shadows us.
     * If we're the first component then no one can shadow us. */
    int            p            = (first_comp ? 0 :
                                   process[line_offset + o + prev_comp]);
    /* Process flag for next component inherits from this one */
    int            np           = p;
    int            cx, cy, sx, sy, ex, ey, lo;
    unsigned char *pc;
    unsigned char *ppc;

    assert((first_comp != 1) ^ (prev_comp == -1));
    assert((last_comp != 1) ^ (comp == ct->comp_order[num_comps-1]));

    /* Work out the search region bounds */
    sy = y - max_y_offset;
    if (clips_on_y && sy < 0)
        sy = 0;
    ey = y + max_y_offset;
    if (clips_on_y && ey >= height)
        ey = height-1;
    sx = x - max_x_offset;
    if (clips_on_x && sx < 0)
        sx = 0;
    ex = x + max_x_offset;
    if (clips_on_x && ex >= width)
        ex = width-1;

    /* We only need to check for shadowing lower components if we're
     * not the last last component (!last_comp). We can only need to process
     * here if we are not the first component (!first_comp) and
     * if (p != 0) then we need to search for the maximum local value
     * of  this component. */
    if (!last_comp || (!first_comp && p))
    {
        int min_v, max_v, v;

        lo = sy % lines_in_buf;
        if (!first_comp)
            max_v = 0;
        if (!last_comp)
            min_v = 255;
        pc = &linebuf[lo * span + sx * num_comps + comp];
        for (cy = sy; cy <= ey; cy++)
        {
            ppc = pc;
            for (cx = sx; cx <= ex; cx++)
            {
                int cv = *ppc;
                ppc += num_comps;
                if (cx == x && cy == y)
                {
                    v = cv;
                    continue;
                }
                if (!first_comp && cv > max_v)
                    max_v = cv;
                if (!last_comp && cv < min_v)
                    min_v = cv;
            }
            pc += span;
            lo++;
            if (lo == lines_in_buf)
            {
                pc -= span * lines_in_buf;
            }
        }
        /* If we're not the last component, and we meet the criteria
         * the next component needs processing. */
        if (!last_comp && shadow_here(v, min_v, comp))
        {
            if (v > np)
                np = v;
        }

        /* Update the next components process flag if required */
        if (!last_comp)
            process[line_offset + oc] = np;

#ifdef SAVE_PROCESS_BUFFER
        if (!last_comp)
        {
            buffer[oc] = ct->process[line_offset + oc];
            return;
        }
#else
        if (!first_comp && p > v && trap_here(v, max_v, comp))
        {
            if (max_v < p)
                p = max_v;
            buffer[oc] = p;
            return;
        }
#endif
    }
    buffer[oc] = linebuf[line_offset + oc];
}

int ClapTrap_GetLine(ClapTrap      * restrict ct,
                     unsigned char * restrict buffer)
{
    int max_y;
    int l_margin;
    int r_margin;
    int comp_idx;
    int prev_comp;
    int comp;
    int x;
    int line_offset;

    /* Read in as many lines as we need */
    max_y = ct->y + ct->max_y_offset;
    if (max_y > ct->height-1)
        max_y = ct->height-1;
    while (ct->lines_read <= max_y)
    {
        int bufpos = ct->span * (ct->lines_read % ct->lines_in_buf);
        int code = ct->get_line(ct->get_line_arg, &ct->linebuf[bufpos]);
        if (code < 0)
            return code;
        memset(&ct->process[bufpos], 0, ct->span);
        ct->lines_read++;
    }

    /* Now we have enough information to calculate the process map for the next line of data */
    l_margin = ct->max_x_offset;
    r_margin = ct->width - ct->max_x_offset;
    if (r_margin < 0)
    {
        r_margin = 0;
        l_margin = 0;
    }
    line_offset = (ct->y % ct->lines_in_buf) * ct->span;
    if (ct->y < ct->max_y_offset || ct->y >= ct->height - ct->max_y_offset)
    {
        /* Some of our search area is off the end of the bitmap. We must be careful. */
        comp = ct->comp_order[0];
        for (x = 0; x < l_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 1, 1, 0, -1, comp, line_offset);
        }
        for (; x < r_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 0, 1, 1, 0, -1, comp, line_offset);
        }
        for (; x < ct->width; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 1, 1, 0, -1, comp, line_offset);
        }
        for (comp_idx = 1; comp_idx < ct->num_comps-1; comp_idx++)
        {
            prev_comp = comp;
            comp = ct->comp_order[comp_idx];
            for (x = 0; x < l_margin; x++)
            {
                process_at_pixel(ct, buffer, x, 1, 1, 0, 0, prev_comp, comp, line_offset);
            }
            for (; x < r_margin; x++)
            {
                process_at_pixel(ct, buffer, x, 0, 1, 0, 0, prev_comp, comp, line_offset);
            }
            for (; x < ct->width; x++)
            {
                process_at_pixel(ct, buffer, x, 1, 1, 0, 0, prev_comp, comp, line_offset);
            }
        }
        prev_comp = comp;
        comp = ct->comp_order[comp_idx];
        for (x = 0; x < l_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 1, 0, 1, prev_comp, comp, line_offset);
        }
        for (; x < r_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 0, 1, 0, 1, prev_comp, comp, line_offset);
        }
        for (; x < ct->width; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 1, 0, 1, prev_comp, comp, line_offset);
        }
    }
    else
    {
        /* Our search area never clips on y at least. */
        comp = ct->comp_order[0];
        for (x = 0; x < l_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 0, 1, 0, -1, comp, line_offset);
        }
        for (; x < r_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 0, 0, 1, 0, -1, comp, line_offset);
        }
        for (; x < ct->width; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 0, 1, 0, -1, comp, line_offset);
        }
        for (comp_idx = 1; comp_idx < ct->num_comps-1; comp_idx++)
        {
            prev_comp = comp;
            comp = ct->comp_order[comp_idx];
            for (x = 0; x < l_margin; x++)
            {
                process_at_pixel(ct, buffer, x, 1, 0, 0, 0, prev_comp, comp, line_offset);
            }
            for (; x < r_margin; x++)
            {
                process_at_pixel(ct, buffer, x, 0, 0, 0, 0, prev_comp, comp, line_offset);
            }
            for (; x < ct->width; x++)
            {
                process_at_pixel(ct, buffer, x, 1, 0, 0, 0, prev_comp, comp, line_offset);
            }
        }
        prev_comp = comp;
        comp = ct->comp_order[comp_idx];
        for (x = 0; x < l_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 0, 0, 1, prev_comp, comp, line_offset);
        }
        for (; x < r_margin; x++)
        {
            process_at_pixel(ct, buffer, x, 0, 0, 0, 1, prev_comp, comp, line_offset);
        }
        for (; x < ct->width; x++)
        {
            process_at_pixel(ct, buffer, x, 1, 0, 0, 1, prev_comp, comp, line_offset);
        }
    }
    ct->y++;
    if (ct->y == ct->height)
    {
        ct->y = 0;
        ct->lines_read = 0;
    }

    return 0;
}
