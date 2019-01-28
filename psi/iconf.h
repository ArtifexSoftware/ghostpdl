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


/* Collected imports for interpreter configuration and initialization */

#ifndef iconf_INCLUDED
#define iconf_INCLUDED

#include "stdpre.h"
#include "gxiodev.h"

/* ------ Imported data ------ */

/* Configuration information imported from iccinit[01].c */
extern const byte gs_init_string[];
extern const uint gs_init_string_sizeof;

/* Configuration information imported from iconf.c */
extern const byte gs_init_files[];
extern const uint gs_init_files_sizeof;
extern const byte gs_emulators[];
extern const uint gs_emulators_sizeof;

extern gx_io_device *i_io_device_table[];
extern const unsigned i_io_device_table_count;

#endif /* iconf_INCLUDED */
