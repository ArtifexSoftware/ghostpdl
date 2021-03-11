/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* pcident.h - object identification mechanism for PCL */

#ifndef pcident_INCLUDED
#define pcident_INCLUDED

#include "gx.h"

/*
 * Various "graphic attribute" objects created by PCL are used in the graphic
 * state: patterns, color spaces, halftones, and rendering dictionaries. Unlike
 * PostScript, these objects also have an existence outside of the graphic state.
 * When graphic objects are rendered, it is necessary to determine the set of
 * attribute objects they should use, and which of these is currently installed
 * in the graphic state.
 *
 * There is no way to do this directly in the graphic library, as its attribute
 * objects do not carry any identifiers (they provide no benefit in a PostScript
 * setting). Hence, we pair the graphic library objects with PCL objects, and
 * assign identifiers to the latter. So long as the two objects are kept in a
 * one-to-one relationship, the identifiers can be used as identifiers of the
 * graphic library objects as well.
 *
 * Though objects of different types can in principle be assigned the same
 * identifier, for simplicity the current code assigns unique identifiers for
 * all objects. These identifiers are unsigned longs. They are assigned
 * consecutively beginning from 8 at boot time. In the unlikely event that
 * they should reach 0 once more, they will restart at 16 * 1024 * 1024; this
 * should prevent overlap with codes that are assigned to statically allocated
 * objects at boot time.
 */

typedef ulong pcl_gsid_t;

/* Define an opaque type for the PCL state. */
#ifndef pcl_state_DEFINED
#  define pcl_state_DEFINED
typedef struct pcl_state_s pcl_state_t;
#endif

pcl_gsid_t pcl_next_id(pcl_state_t * pcs);

#endif /* pcident_INCLUDED */
