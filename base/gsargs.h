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


/* Command line argument list management */

#ifndef gsargs_INCLUDED
#  define gsargs_INCLUDED

#include "std.h"

/*
 * We need to handle recursion into @-files.
 * The following structures keep track of the state.
 * Defining a maximum argument length and a maximum nesting depth
 * decreases generality, but eliminates the need for dynamic allocation.
 */
#define arg_str_max 2048
#define arg_depth_max 11
typedef struct arg_source_s {
    bool is_file;
    union _u {
        struct _su {
            bool parsed;        /* true for "pushed-back" argument, not to be parsed again */
            bool decoded;       /* true if already converted via 'get_codepoint' */
            char *chars;        /* original string */
            gs_memory_t *memory;/* if non-0, free chars when done with it */
            const char *str;    /* string being read */
        } s;
        gp_file *file;
    } u;
} arg_source;
typedef struct arg_list_s {
    bool expand_ats;            /* if true, expand @-files */
    gp_file *(*arg_fopen) (const char *fname, void *fopen_data);
    void *fopen_data;
    int (*get_codepoint)(gp_file *file, const char **astr);
    gs_memory_t *memory;
    const char **argp;
    int argn;
    int depth;                  /* depth of @-files */
    char cstr[arg_str_max + 6]; /* +6 allows for long utf8 codepoints */
    arg_source sources[arg_depth_max];
} arg_list;

int codepoint_to_utf8(char *cstr, int rune);

/* Initialize an arg list. */
int arg_init(arg_list    *pal,
             const char **argv,
             int          argc,
             gp_file     *(*arg_fopen)(const char *fname, void *fopen_data),
             void        *fopen_data,
             int          (*get_codepoint)(gp_file *file, const char **astr),
             gs_memory_t *mem);

/*
 * Push a string onto an arg list.
 * This may also be used (once) to "unread" the last argument.
 * parsed implies that it's in gs internal format (utf8), unparsed implies
 * that it's in the format ready for get_codepoint to be called on it.
 * If mem != 0, it is used to free the string when we are done with it.
 * Return 0 on success, non-zero on failure
 */
int arg_push_memory_string(arg_list * pal, char *str, bool parsed, gs_memory_t * mem);

/* Push a string onto an arg_list.
 * As arg_push_memory_string, with the additional facility that a string
 * can be unparsed, but already decoded.
 */
int arg_push_decoded_memory_string(arg_list * pal, char *str, bool parsed, bool decoded, gs_memory_t * mem);

#define arg_push_string(pal, str, parsed)\
  arg_push_memory_string(pal, str, parsed, (gs_memory_t *)0);

/* Clean up an arg list before exiting. */
void arg_finit(arg_list * pal);

/*
 * Get the next arg from a list.
 * Note that these are not copied to the heap.
 */
/* returns:
 * >0 - valid argument
 *  0 - arguments exhausted
 * <0 - error condition
 * *argstr is *always* set: to the arg string if it is valid,
 * or to NULL otherwise
 */
int arg_next(arg_list * pal, const char **argstr, const gs_memory_t *errmem);

/* Copy an argument string to the heap. */
char *arg_copy(const char *str, gs_memory_t * mem);

/* Free an argument string previously copied to the heap. */
void arg_free(char *str, gs_memory_t * mem);

/* compare a match string against an encoded arg */
int arg_strcmp(arg_list *pal, const char *arg, const char *match);

#endif /* gsargs_INCLUDED */
