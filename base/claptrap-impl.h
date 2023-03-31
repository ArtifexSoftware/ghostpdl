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

#ifndef CLAPTRAP_IMPL_H
#define CLAPTRAP_IMPL_H

#include "claptrap.h"

/* The API for this is that the caller requests trapped scanlines.
 * We rely on being able to request untrapped scanlines from a source.
 * Because we need to look ahead (and behind) of the current position
 * to calculate whether trapping is needed at a given point, we use a
 * rolling buffer to hold several scanlines.
 *
 * First we examine the buffered data to calculate a 'process map'.
 * The process map says whether we need to consider 'smearing'
 * component i at position(x, y). If we ever consider trapping
 * component i, we will also consider trapping component i+1.
 *
 * We need to have processed line y+max_y_offset in order to be
 * sure that the process map for line y is complete.
 */

struct ClapTrap
{
    ClapTrap_LineFn *get_line;
    void            *get_line_arg;
    int              width;
    int              height;
    int              num_comps;
    const int       *comp_order;
    int              max_x_offset;
    int              max_y_offset;
    int              lines_in_buf; /* How many lines we can store in linebuf (2*max_y_offset+1) */
    unsigned char   *linebuf;
    int              lines_read; /* How many lines we have read into linebuf so far */
    int              y; /* Which output line we are on */
    int              span;
    unsigned char   *process;
};

/* Evaluate whether a given component should 'shadow' components
 * under it (i.e. if the component plane 'comp' was printed
 * offset, would it noticably change the amount of the colours
 * under it that could be seen) */
inline static int shadow_here(int v, int min_v, int comp)
{
    return (min_v < 0.8 * v && min_v < v - 16);
}

/* Given that component comp might be exposed by a higher
 * plane being offset, would the value exposed here be
 * noticably different from the values around it? */
inline static int trap_here(int v, int max_v, int comp)
{
    return (v < 0.8 * max_v);
}

#endif /* CLAPTRAP_IMPL_H */
