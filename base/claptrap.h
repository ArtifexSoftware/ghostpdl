/* Copyright (C) 2001-2019 Artifex Software, Inc.
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

#ifndef CLAPTRAP_H
#define CLAPTRAP_H

#include "std.h"
#include "stdpre.h"
#include "gsmemory.h"

#ifndef inline
#define inline __inline
#endif /* inline */

typedef struct ClapTrap ClapTrap;

typedef int (ClapTrap_LineFn)(void *arg, unsigned char *buf);

ClapTrap *ClapTrap_Init(gs_memory_t     *mem,
                        int              width,
                        int              height,
                        int              num_comps,
                        const int       *comp_order,
                        int              max_x_offset,
                        int              max_y_offset,
                        ClapTrap_LineFn *get_line,
                        void            *get_line_arg);

void ClapTrap_Fin(gs_memory_t *mem,
                  ClapTrap *trapper);

int ClapTrap_GetLine(ClapTrap      * gs_restrict trapper,
                     unsigned char * gs_restrict buffer);

int ClapTrap_GetLinePlanar(ClapTrap       * gs_restrict trapper,
                           unsigned char ** gs_restrict buffer);

#endif /* CLAPTRAP_H */
