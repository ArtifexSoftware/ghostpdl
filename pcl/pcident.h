/*
 * Copyright (C) 1998 Aladdin Enterprises.
 * All rights reserved.
 *
 * This file is part of Aladdin Ghostscript.
 *
 * Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
 * or distributor accepts any responsibility for the consequences of using it,
 * or for whether it serves any particular purpose or works at all, unless he
 * or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
 * License (the "License") for full details.
 *
 * Every copy of Aladdin Ghostscript must include a copy of the License,
 * normally in a plain ASCII text file named PUBLIC.  The License grants you
 * the right to copy, modify and redistribute Aladdin Ghostscript, but only
 * under certain conditions described in the License.  Among other things, the
 * License requires that the copyright notice and this notice be preserved on
 * all copies.
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

typedef ulong   pcl_gsid_t;

extern  pcl_gsid_t  pcl_next_id( void );

extern  void    pcl_init_id( void );

#endif  /* pcident_INCLUDED */
