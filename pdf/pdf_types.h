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

#ifndef PDF_OBJ_TYPES
#define PDF_OBJ_TYPES

/* defines for various compile-time debugging flags.
 * These only emit any text on a debug build and when the relevant
 * flag is set and compiled.
 */
#define DEBUG_PATTERN 0
#define DEBUG_CONTEXT 0
#define DEBUG_GSAVE 0
#define DEBUG_IMAGES 0
#define DEBUG_TRANSPARENCY 0
#define DEBUG_DEVICE 0
#define DEBUG_CACHE 0
#define DEBUG_CACHE_FREE 0
#define PROBE_STREAMS 0
#define REFCNT_DEBUG 0
#define CACHE_STATISTICS 0

#include "stdint_.h"    /* Various data types */
#include "scommon.h"    /* for gs_offset_t */

typedef enum pdf_obj_type_e {
    PDF_NULL = 'n',
    PDF_INT = 'i',
    PDF_REAL = 'f',
    PDF_STRING = 's',
    PDF_NAME = '/',
    PDF_ARRAY = 'a',
    PDF_DICT = 'd',
    PDF_INDIRECT = 'R',
    PDF_BOOL = 'b',
    PDF_KEYWORD = 'K',
    PDF_FONT = 'F',
    /* The following aren't PDF object types, but are objects we either want to
     * reference count, or store on the stack.
     */
    PDF_XREF_TABLE = 'X',
    PDF_ARRAY_MARK = '[',
    PDF_DICT_MARK = '<',
    PDF_PROC_MARK = '{',
    PDF_CMAP = 'C'
} pdf_obj_type;

#if REFCNT_DEBUG
#define pdf_obj_common \
    pdf_obj_type type;\
    char flags;\
    unsigned int refcnt;\
    void *ctx;\
    uint32_t object_num;\
    uint32_t generation_num;\
    uint32_t indirect_num;\
    uint16_t indirect_gen; \
    uint64_t UID
#else
#define pdf_obj_common \
    pdf_obj_type type;\
    char flags;\
    unsigned int refcnt;\
    /* We have to define ctx as a void * and cast it, because the pdf_context structure\
     * contains pdf_object members, so there's a circular dependency\
     */\
    void *ctx;\
    /* Technically object numbers can be any integer. The only documented limit\
     * architecturally is the fact that the linked list of free objects (ab)uses\
     * the 'offset' field of the xref to hold the object number of the next free\
     * object, this means object numbers can't be larger than 10 decimal digits.\
     * Unfortunately, that's 34 bits, so we use the Implementation Limits in\
     * Appendix H (Annexe C of the 2.0 spec) which limits object numbers to 23 bits.\
     * But we use 32 bits instead.\
     */\
    uint32_t object_num;\
    uint32_t generation_num;\
    /* We only use the indirect object number for creating decryption keys for strings\
     * for which we (currently at least) only need the bottom 3 bytes, but we keep\
     * a full 32-bits. For the generation number we only need the bottom 2 bytes\
     * so we keep that as a 16-bit value and truncate it on storage. If storage\
     * becomes a problem we could use 5 bytes to store the 2 values but to save one\
     * byte it does not seem worthwhile. Note! the indirect num is the object number\
     * of the object if it is an indirect object (so indirect_num == object_num) or,\
     * if the object is defined directly, its the object number of the enclosing\
     * indirect object. This is not the same thing as the /Parent entry in some dictionaries.\
     */\
    uint32_t indirect_num;\
    uint16_t indirect_gen
#endif

typedef struct pdf_obj_s {
    pdf_obj_common;
} pdf_obj;

typedef struct pdf_bool_s {
    pdf_obj_common;
    bool value;
} pdf_bool;

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
    PDF_INVALID_KEY,
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
    pdf_obj **values;
} pdf_array;

typedef struct pdf_dict_s {
    pdf_obj_common;
    uint64_t size;
    uint64_t entries;
    pdf_obj **keys;
    pdf_obj **values;
    gs_offset_t stream_offset;
    int64_t Length; /* Value of Length in dict, 0 if undefined.  non-zero means it's a stream */
    bool is_stream; /* True if it has a Length param */
    bool length_valid; /* True if Length and is_stream have been cached above */
} pdf_dict;

typedef struct pdf_indirect_ref_s {
    pdf_obj_common;
    uint64_t ref_object_num;
    uint32_t ref_generation_num;
    bool is_label; /* Used to tell that it's a label for pdfmark */
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
    bool eof;
    stream *original;
    stream *s;
    uint32_t unread_size;
    char unget_buffer[UNREAD_BUFFER_SIZE];
} pdf_stream;

#endif
