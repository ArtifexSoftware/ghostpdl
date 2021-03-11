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


/* pxvalue.h */
/* Value definitions for PCL XL parser */

#ifndef pxvalue_INCLUDED
#  define pxvalue_INCLUDED

#include "stdint_.h"
#include "gstypes.h"
#include "pxattr.h"

/* ---------------- Values on stack ---------------- */

/* Define masks for checking data types of attribute values. */
typedef enum
{
    /* Structure */
    pxd_scalar = 1,
    pxd_xy = 2,
    pxd_box = 4,
    pxd_array = 8,
    pxd_structure = 0xf,
    /* Representation */
    pxd_ubyte = 0x10,
    pxd_uint16 = 0x20,
    pxd_uint32 = 0x40,
    pxd_sint16 = 0x80,
    pxd_sint32 = 0x100,
    pxd_any_int =
        pxd_ubyte | pxd_uint16 | pxd_uint32 | pxd_sint16 | pxd_sint32,
    pxd_real32 = 0x200,
    pxd_any_real = pxd_real32,
    pxd_representation = 0x3f0,
    /* Byte ordering for arrays */
    pxd_big_endian = 0x400,
    /* Heap allocation for arrays */
    pxd_on_heap = 0x800
} px_data_type_t;

typedef float real;

/* Define a type for values on the stack. */
typedef struct px_array_s
{
    uint size;                  /* number of elements, not length in bytes */
    const byte *data;
} px_array_t;

typedef struct
{
    px_data_type_t type;
    px_attribute_t attribute;
    union v_
    {
        int32_t i;
        int32_t ia[4];          /* ia[0] aliases i */
        real r;
        real ra[4];             /* ra[0] aliases r */
        px_array_t array;
    } value;
} px_value_t;

#define real_value(pv, n)\
  ((pv)->type & pxd_any_real ? (pv)->value.ra[n] : (real)(pv)->value.ia[n])

#define set_box_value(x1, y1, x2, y2, pv)\
  do {\
    const px_value_t *pv_ = (pv);\
    if ( pv_->type & pxd_any_real )\
      x1 = pv_->value.ra[0], y1 = pv_->value.ra[1],\
        x2 = pv_->value.ra[2], y2 = pv_->value.ra[3];\
    else\
      x1 = pv_->value.ia[0], y1 = pv_->value.ia[1],\
        x2 = pv_->value.ia[2], y2 = pv_->value.ia[3];\
  } while ( 0 )

#define value_size(pv)\
  ((pv)->type & pxd_ubyte ? 1 : (pv)->type & (pxd_uint16 | pxd_sint16) ? 2 : 4)

#define array_value_size(pav)\
  ((pav)->value.array.size * value_size(pav))

/* ---------------- Procedures ---------------- */

/* Get numeric values from the input or an array. */
uint uint16at(const byte * p, bool big_endian);

int sint16at(const byte * p, bool big_endian);

int32_t uint32at(const byte * p, bool big_endian);

int32_t sint32at(const byte * p, bool big_endian);

real real32at(const byte * p, bool big_endian);

/* Get an element from an array. */
/* The caller does all index and type checking. */
int32_t integer_elt(const px_value_t * pav, uint index);

real real_elt(const px_value_t * pav, uint index);

#endif /* pxvalue_INCLUDED */
