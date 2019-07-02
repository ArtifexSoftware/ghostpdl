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


/* Common platform-specific routines for OS/2 */

#ifndef gp_os2_INCLUDED
#  define gp_os2_INCLUDED

#include "std.h"

/* If queue_name is NULL, list available queues */
/* If strlen(queue_name)==0, return default queue and driver name */
/* If queue_name supplied, return driver_name */
/* returns 0 if OK, non-zero for error */
int pm_find_queue(const gs_memory_t *mem, char *queue_name, char *driver_name);

/* Spool file to queue */
/* Return 0 if successful, non-zero if error. */
/* If filename is NULL, return 0 if spool queue is valid, non-zero if error */
int pm_spool(const gs_memory_t *mem, const char *filename, const char *queue);

#endif /* gp_os2_INCLUDED */
