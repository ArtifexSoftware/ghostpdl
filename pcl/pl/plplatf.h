/* Copyright (C) 2001-2012 Artifex Software, Inc.
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


/* plplatf.h   Platform-related utils */

#ifndef plplatf_INCLUDED
#   define plplatf_INCLUDED

/* ------------- Platform de/init --------- */
void pl_platform_init(FILE * debug_out);

void pl_platform_dnit(int exit_status);

/*----- The following is declared here, but must be implemented by client ----*/
/* Terminate execution */
void pl_exit(int exit_status);

#endif /* plplatf_INCLUDED */
