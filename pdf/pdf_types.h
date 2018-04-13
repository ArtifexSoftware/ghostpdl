/* Copyright (C) 2001-2018 Artifex Software, Inc.
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

#include "stdint_.h"    /* Various data types */
#include "scommon.h"    /* for gs_offset_t */

#ifndef PDF_OBJ_TYPES
#define PDF_OBJ_TYPES
typedef enum pdf_obj_type_e {
    PDF_NULL = 'n',
	PDF_INT = 'i',
	PDF_REAL = 'f',
	PDF_STRING = 's',
	PDF_NAME = '/',
	PDF_ARRAY = 'a',
	PDF_DICT = 'd',
	PDF_INDIRECT = 'R',
    PDF_TRUE = '1',
    PDF_FALSE = '0',
    PDF_KEYWORD = 'K',
} pdf_obj_type_t;

typedef pdf_obj_type_t pdf_obj_type;

typedef struct pdf_obj_s {
    char type;
    char flags;
    unsigned int refcnt;
    gs_memory_t *memory;                /* memory allocator to use */
    uint64_t object_num;
    uint32_t generation_num;
} pdf_obj_t;

typedef pdf_obj_t pdf_obj;

typedef struct pdf_num_s {
    pdf_obj object;
    union {
        /* Acrobat (up to PDF version 1.7) limits ints to 32-bits, we choose to use 64 */
        int64_t i;
        double d;
    }u;
} pdf_num_t;

typedef pdf_num_t pdf_num;

typedef struct pdf_string_s {
    pdf_obj object;
    uint32_t length;
    unsigned char *data;
} pdf_string_t;

typedef pdf_string_t pdf_string;

typedef struct pdf_name_s {
    pdf_obj object;
    uint32_t length;
    unsigned char *data;
} pdf_name_t;

typedef pdf_name_t pdf_name;

typedef enum pdf_key_e {
    PDF_NOT_A_KEYWORD,
    PDF_OBJ,
    PDF_ENDOBJ,
    PDF_STREAM,
    PDF_ENDSTREAM,
    PDF_XREF,
    PDF_STARTXREF,
    PDF_TRAILER,
}pdf_key_t;

typedef pdf_key_t pdf_key;

typedef struct pdf_keyword_s {
    pdf_obj object;
    uint32_t length;
    unsigned char *data;
    pdf_key key;
} pdf_keyword_t;

typedef pdf_keyword_t pdf_keyword;

typedef struct pdf_array_s {
    pdf_obj object;
    uint64_t size;
    uint64_t entries;
    pdf_obj **values;
} pdf_array_t;

typedef pdf_array_t pdf_array;

typedef struct pdf_dict_s {
    pdf_obj object;
    uint64_t size;
    uint64_t entries;
    pdf_obj **keys;
    pdf_obj **values;
    gs_offset_t stream;
} pdf_dict_t;

typedef pdf_dict_t pdf_dict;

typedef struct pdf_indirect_ref_s {
    pdf_obj object;
    uint64_t object_num;
    uint32_t generation_num;
} pdf_indirect_ref_t;

typedef pdf_indirect_ref_t pdf_indirect_ref;

typedef struct xref_entry_s {
    bool compressed;                /* true if object is in a compressed object stream */
    bool free;                      /* true if this is a free entry */
    uint64_t object_num;            /* Object number or compressed stream object number if compressed */
    uint32_t generation_num;        /* Generation number. Objects in compressed streams have generation of 0 */
    gs_offset_t offset;             /* File offset. Index of object in compressed stream */
    pdf_obj *object;                /* Pointer to object if cached, or NULL if not */
} xref_entry_t;

typedef xref_entry_t xref_entry;

#endif
