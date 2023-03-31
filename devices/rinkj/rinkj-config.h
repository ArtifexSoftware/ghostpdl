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


#ifndef rinkj_config_h_INCLUDED
#define rinkj_config_h_INCLUDED

/* Support for reading Rinkj config files. */

char *
rinkj_strdup_size (const char *src, int size);

char *
rinkj_config_get (const char *config, const char *key);

char *
rinkj_config_keyval (const char *config, char **p_val, const char **p_next);

#endif
