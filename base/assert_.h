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

/* A wrapper for providing assert() in ghosctscript code. Any changes
 * required for systems that don't provide an ANSI compatible assert.h
 * header can go in here. */

/* Avoid multiple inclusion - the ANSI assert system specifically does not
 * do this. */
#ifndef _ASSERT_H
#define _ASSERT_H

/* Our makefiles only provide DEBUG, not NDEBUG. To make up for this, we
 * map the absence of DEBUG to mean the presence of NDEBUG here. */
#if !defined(DEBUG) && !defined(NDEBUG)
#    define NDEBUG
#endif

/* We'd like the ability to force assert checking on even in release builds.
 * To do this without requiring NDEBUG and DEBUG to be explicitly set/unset
 * we add a new define, FORCE_ASSERT_CHECKING. */
#ifdef FORCE_ASSERT_CHECKING
#    undef NDEBUG
#endif

/* Any system specific magic to cope with the non-existence of the assert.h
 * header should go in here. */
#include <assert.h>

#endif
