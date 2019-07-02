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

/* Stub functions to allow mkromfs to build */

#include "windows_.h"
#include "stdio_.h"
#include "gp.h"
#include "memory_.h"
#include "stat_.h"

#include "gp_mswin.h"

FILE *
gp_open_printer_impl(gs_memory_t *mem,
                     const char  *fname,
                     int         *binary_mode,
                     int          (**close)(FILE *))
{
    return NULL;
}

FILE *
gp_open_scratch_file_impl(const gs_memory_t *mem,
                          const char        *prefix,
                                char        *fname,
                          const char        *mode,
                                int          remove)
{
    return NULL;
}
