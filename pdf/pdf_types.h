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

#define REFCNT_DEBUG 0

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
    /* The following aren't PDF object types, but are objects we either want to
     * reference count, or store on the stack.
     */
    PDF_XREF_TABLE = 'X',
    PDF_ARRAY_MARK = '[',
    PDF_DICT_MARK = '>',
} pdf_obj_type;

#if REFCNT_DEBUG
#define pdf_obj_common \
    pdf_obj_type type;\
    char flags;\
    unsigned int refcnt;\
    gs_memory_t *memory;                /* memory allocator to use */\
    uint64_t object_num;\
    uint32_t generation_num;\
    uint64_t UID
#else
#define pdf_obj_common \
    pdf_obj_type type;\
    char flags;\
    unsigned int refcnt;\
    gs_memory_t *memory;                /* memory allocator to use */\
    uint64_t object_num;\
    uint32_t generation_num
#endif

typedef struct pdf_obj_s {
    pdf_obj_common;
} pdf_obj;

typedef struct pdf_num_s {
    pdf_obj_common;
    union {
        /* Acrobat (up to PDF version 1.7) limits ints to 32-bits, we choose to use 64 */
        int64_t i;
        double d;
    }value;
} pdf_num;

typedef struct pdf_string_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char *data;
} pdf_string;

typedef struct pdf_name_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char *data;
} pdf_name;

typedef enum pdf_key_e {
    PDF_NOT_A_KEYWORD,
    PDF_OBJ,
    PDF_ENDOBJ,
    PDF_STREAM,
    PDF_ENDSTREAM,
    PDF_XREF,
    PDF_STARTXREF,
    PDF_TRAILER,
}pdf_key;

typedef struct pdf_keyword_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char *data;
    pdf_key key;
} pdf_keyword;

typedef struct pdf_array_s {
    pdf_obj_common;
    uint64_t size;
    uint64_t entries;
    pdf_obj **values;
} pdf_array;

typedef struct pdf_dict_s {
    pdf_obj_common;
    uint64_t size;
    uint64_t entries;
    pdf_obj **keys;
    pdf_obj **values;
    gs_offset_t stream_offset;
} pdf_dict;

typedef struct pdf_indirect_ref_s {
    pdf_obj_common;
    uint64_t ref_object_num;
    uint32_t ref_generation_num;
} pdf_indirect_ref;

typedef struct pdf_obj_cache_entry_s {
    void *next;
    void *previous;
    pdf_obj *o;
}pdf_obj_cache_entry;

/* The compressed and uncompressed xref entries are identical, they only differ
 * in the names used for the variables. Its simply less confusing not to overload
 * the names.
 */
typedef struct xref_entry_s {
    bool compressed;                /* true if object is in a compressed object stream */
    bool free;                      /* true if this is a free entry */
    uint64_t object_num;            /* Object number */

    union u_s {
        struct uncompressed_s {
            uint32_t generation_num;        /* Generation number. */
            gs_offset_t offset;             /* File offset. */
        }uncompressed;
        struct compressed_s {
            uint64_t compressed_stream_num; /* compressed stream object number if compressed */
            uint32_t object_index;          /* Index of object in compressed stream */
        }compressed;
    }u;
    pdf_obj_cache_entry *cache;     /* Pointer to cache entry if cached, or NULL if not */
} xref_entry;

typedef struct xref_s {
    pdf_obj_common;
    uint64_t xref_size;
    xref_entry *xref;
} xref_table;

#define UNREAD_BUFFER_SIZE 256

typedef struct pdf_stream_s {
    stream *s;
    uint32_t unread_size;
    char unget_buffer[UNREAD_BUFFER_SIZE];
} pdf_stream;

#endif
