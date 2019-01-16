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


/* Supporting procedures for 'save' recording. */

#ifndef idosave_INCLUDED
#  define idosave_INCLUDED

#include "imemory.h"

/*
 * Structure for saved change chain for save/restore.  Because of the
 * garbage collector, we need to distinguish the cases where the change
 * is in a static object, a dynamic ref, or a dynamic struct.
 */
typedef struct alloc_change_s alloc_change_t;
struct alloc_change_s {
    alloc_change_t *next;
    ref_packed *where;
    ref contents;
#define AC_OFFSET_STATIC (-2)	/* static object */
#define AC_OFFSET_REF (-1)	/* dynamic ref */
#define AC_OFFSET_ALLOCATED (-3) /* a newly allocated ref array */
    short offset;		/* if >= 0, offset within struct */
};

/*
 * Save a change that must be undone by restore.  We have to pass the
 * pointer to the containing object to alloc_save_change for two reasons:
 *
 *      - We need to know which VM the containing object is in, so we can
 * know on which chain of saved changes to put the new change.
 *
 *      - We need to know whether the object is an array of refs (which
 * includes dictionaries) or a struct, so we can properly trace and
 * relocate the pointer to it from the change record during garbage
 * collection.
 */

int alloc_save_change(gs_dual_memory_t *dmem, const ref *pcont,
                      ref_packed *ptr, client_name_t cname);
int alloc_save_change_in(gs_ref_memory_t *mem, const ref *pcont,
                         ref_packed *ptr, client_name_t cname);
/* Remove an AC_OFFSET_ALLOCATED element. */
void alloc_save_remove(gs_ref_memory_t *mem, ref_packed *obj, client_name_t cname);
/* Allocate a structure for recording an allocation event. */
int alloc_save_change_alloc(gs_ref_memory_t *mem, client_name_t cname, alloc_change_t **pcp);

#endif /* idosave_INCLUDED */
