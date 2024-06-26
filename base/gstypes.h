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


/* Miscellaneous common types for Ghostscript library */

#ifndef gstypes_INCLUDED
#  define gstypes_INCLUDED

#include "stdpre.h"

/*
 * Define a type used internally for unique IDs of various kinds
 * (primarily, but not exclusively, character and halftone bitmaps).
 * These IDs bear no relation to any other ID space; we generate them all
 * ourselves.
 */
typedef ulong gs_id;

#define gs_no_id 0L

/*
 * Define a sensible representation of a string, as opposed to
 * the C char * type (which can't store arbitrary data, represent
 * substrings, or perform concatenation without destroying aliases).
 *
 * If a byte * pointer P is the result of allocating a string of size N,
 * then any substring of [P .. P+N) is a valid gs_string, i.e., any
 * gs_string S is valid (until the string is deallocated) if it has P <=
 * S.data and S.data + S.size <= P + N.
 */
#define GS_STRING_COMMON\
    byte *data;\
    uint size
typedef struct gs_string_s {
    GS_STRING_COMMON;
} gs_string;
#define GS_CONST_STRING_COMMON\
    const byte *data;\
    uint size
typedef struct gs_const_string_s {
    GS_CONST_STRING_COMMON;
} gs_const_string;
typedef struct gs_param_string_s {
    GS_CONST_STRING_COMMON;
    bool persistent;
} gs_param_string;

/*
 * Since strings are allocated differently from ordinary objects, define a
 * structure that can reference either a string or a byte object.  If bytes
 * == 0, data and size are the same as for a gs_string.  If bytes != 0, data
 * and size point within the object addressed by bytes (i.e., the bytes
 * member plays the role of P in the consistency condition given for
 * gs_string above).  Thus in either case, code can process the string using
 * only data and size: bytes is only relevant for garbage collection.
 *
 * Note: for garbage collection purposes, the string_common members must
 * come first.
 */
typedef struct gs_bytestring_s {
    GS_STRING_COMMON;
    byte *bytes;		/* see above */
} gs_bytestring;
typedef struct gs_const_bytestring_s {
    GS_CONST_STRING_COMMON;
    const byte *bytes;		/* see above */
} gs_const_bytestring;

#define gs_bytestring_from_string(pbs, dat, siz)\
  ((pbs)->data = (dat), (pbs)->size = (siz), (pbs)->bytes = 0)
#define gs_bytestring_from_bytes(pbs, byts, offset, siz)\
  ((pbs)->data = ((pbs)->bytes = (byts)) + (offset), (pbs)->size = (siz))

/*
 * Define types for Cartesian points.
 */
typedef struct gs_point_s {
    double x, y;
} gs_point;
typedef struct gs_int_point_s {
    int x, y;
} gs_int_point;

/*
 * Define a scale for oversampling.  Clients don't actually use this,
 * but this seemed like the handiest place for it.
 */
typedef struct gs_log2_scale_point_s {
    int x, y;
} gs_log2_scale_point;

/*
 * Define types for rectangles in the Cartesian plane.
 * Note that rectangles are half-open, i.e.: their width is
 * q.x-p.x and their height is q.y-p.y; they include the points
 * (x,y) such that p.x<=x<q.x and p.y<=y<q.y.
 */
typedef struct gs_rect_s {
    gs_point p, q;		/* origin point, corner point */
} gs_rect;
typedef struct gs_int_rect_s {
    gs_int_point p, q;
} gs_int_rect;

/*
 * Define a type for a floating-point parameter range.  Note that unlike
 * the intervals for gs_rect and gs_int_rect, these intervals are closed
 * (i.e., they represent rmin <= x <= rmax, not rmin <= x < rmax).
 */
typedef struct gs_range_s {
    float rmin, rmax;
} gs_range_t;

#endif /* gstypes_INCLUDED */
