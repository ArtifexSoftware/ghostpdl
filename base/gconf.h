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

/* Wrapper for gconfig.h or a substitute. */

/*
 * NOTA BENE: This file, unlike all other header files, must *not* have
 * double-inclusion protection, since it is used in peculiar ways.
 */

/*
 * Since not all C preprocessors implement #include with a non-quoted
 * argument, we arrange things so that we can still compile with such
 * compilers as long as GCONFIG_H isn't defined.
 */

#ifndef GCONFIG_H
#  include "gconfig.h"
#else
#  include GCONFIG_H
#endif
