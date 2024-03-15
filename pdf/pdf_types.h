/* Copyright (C) 2018-2024 Artifex Software, Inc.
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

#ifndef PDF_OBJ_TYPES
#define PDF_OBJ_TYPES

/* defines for various compile-time debugging flags.
 * These only emit any text on a debug build and when the relevant
 * flag is set and compiled.
 */
#ifndef DEBUG_PATTERN
#define DEBUG_PATTERN 0
#endif

#ifndef DEBUG_CONTEXT
#define DEBUG_CONTEXT 0
#endif

#ifndef DEBUG_GSAVE
#define DEBUG_GSAVE 0
#endif

#ifndef DEBUG_IMAGES
#define DEBUG_IMAGES 0
#endif

#ifndef DEBUG_TRANSPARENCY
#define DEBUG_TRANSPARENCY 0
#endif

#ifndef DEBUG_DEVICE
#define DEBUG_DEVICE 0
#endif

#ifndef DEBUG_CACHE
#define DEBUG_CACHE 0
#endif

#ifndef DEBUG_CACHE_FREE
#define DEBUG_CACHE_FREE 0
#endif

#ifndef PROBE_STREAMS
#define PROBE_STREAMS 0
#endif

#ifndef REFCNT_DEBUG
#define REFCNT_DEBUG 0
#endif

#ifndef CACHE_STATISTICS
#define CACHE_STATISTICS 0
#endif

#ifndef DEBUG_DICT
#define DEBUG_DICT 0
#endif

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
    PDF_FAST_KEYWORD = 'k',
    PDF_FONT = 'F',
    PDF_STREAM = 'S',
    /* The following aren't PDF object types, but are objects we either want to
     * reference count, or store on the stack.
     */
    PDF_XREF_TABLE = 'X',
    PDF_ARRAY_MARK = '[',
    PDF_DICT_MARK = '<',
    PDF_PROC_MARK = '{',
    PDF_CMAP = 'C',
    PDF_BUFFER = 'B',
    /* Lastly, for the benefit of duplicate colour space identification, we store either
     * a name for a colour space, or if there is no name, the context (we can get the
     * context from the name object if there is one). We need to be able to tell if a
     * pdf_obj is a name or a context.
     */
    PDF_CTX = 'c'
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

#define PDF_NAME_DECLARED_LENGTH 4096

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
    unsigned char data[PDF_NAME_DECLARED_LENGTH];
} pdf_string;

typedef struct pdf_name_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char data[PDF_NAME_DECLARED_LENGTH];
} pdf_name;

/* For storing arbitrary byte arrays where the length may be
   greater than PDF_NAME_DECLARED_LENGTH - prevents static
   alalysis tools complaining if we just used pdf_string
 */
typedef struct pdf_buffer_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char *data;
} pdf_buffer;

typedef enum pdf_key_e {
#include "pdf_tokens.h"
        TOKEN__LAST_KEY,
} pdf_key;

#define PDF_NULL_OBJ ((pdf_obj *)(uintptr_t)TOKEN_null)
#define PDF_TRUE_OBJ ((pdf_obj *)(uintptr_t)TOKEN_PDF_TRUE)
#define PDF_FALSE_OBJ ((pdf_obj *)(uintptr_t)TOKEN_PDF_FALSE)
#define PDF_TOKEN_AS_OBJ(token) ((pdf_obj *)(uintptr_t)(token))

typedef struct pdf_keyword_s {
    pdf_obj_common;
    uint32_t length;
    unsigned char data[PDF_NAME_DECLARED_LENGTH];
} pdf_keyword;

typedef struct pdf_array_s {
    pdf_obj_common;
    uint64_t size;
    pdf_obj **values;
} pdf_array;

typedef struct pdf_dict_entry_s {
    pdf_obj *key;
    pdf_obj *value;
} pdf_dict_entry;

typedef struct pdf_dict_s {
    pdf_obj_common;
    uint64_t size;
    uint64_t entries;
    pdf_dict_entry *list;
    bool dict_written;  /* Has dict been written (for pdfwrite) */
    bool is_sorted;     /* true if the dictionary has been sorted by Key, for faster searching */
} pdf_dict;

typedef struct pdf_stream_s {
    pdf_obj_common;
    pdf_dict *stream_dict;
    /* This is plain ugly. In order to deal with the (illegal) case where a stream
     * uses named objects which are not defined in the stream's Resources dictionary,
     * but *are* defined in one of the enclosing stream's Resources dictionary, we need
     * to be able to walk back up the chain of nested streams. We set/reset the parent_obj
     * in pdfi_interpret_stream_context, the value is only ever non-NULL if we are executing
     * the stream, and the stream has a parent context.
     */
    pdf_obj *parent_obj;
    gs_offset_t stream_offset;
    int64_t Length; /* Value of Length in dict, 0 if undefined.  non-zero means it's a stream */
    bool length_valid; /* True if Length and is_stream have been cached above */
    bool stream_written; /* Has stream been written (for pdfwrite) */
    bool is_marking;
} pdf_stream;

typedef struct pdf_indirect_ref_s {
    pdf_obj_common;
    uint64_t ref_object_num;
    uint32_t ref_generation_num;
    uint64_t highlevel_object_num;
    bool is_highlevelform; /* Used to tell that it's a high level form for pdfmark */
    bool is_marking; /* Are we in the middle of marking this? */
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
} xref_table_t;

#define UNREAD_BUFFER_SIZE 256

typedef struct pdf_c_stream_s {
    bool eof;
    stream *original;
    stream *s;
    uint32_t unread_size;
    char unget_buffer[UNREAD_BUFFER_SIZE];
} pdf_c_stream;

#ifndef inline
#define inline __inline
#endif /* inline */

#define pdfi_type_of(A) pdfi_type_of_imp((pdf_obj *)A)

static inline pdf_obj_type pdfi_type_of_imp(pdf_obj *obj)
{
    if ((uintptr_t)obj > TOKEN__LAST_KEY)
        return obj->type;
    else if ((uintptr_t)obj == TOKEN_PDF_TRUE || (uintptr_t)obj == TOKEN_PDF_FALSE)
        return PDF_BOOL;
    else if ((uintptr_t)obj == TOKEN_null)
        return PDF_NULL;
    else
        return PDF_FAST_KEYWORD;
}

static inline int pdf_object_num(pdf_obj *obj)
{
    if ((uintptr_t)obj > TOKEN__LAST_KEY)
        return obj->object_num;
    return 0;
}

#endif
