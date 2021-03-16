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


/* pxparse.h */
/* Definitions and interface for PCL XL parser */
/* Requires gsmemory.h */

#ifndef pxparse_INCLUDED
#  define pxparse_INCLUDED

#include "pxoper.h"

/* ---------------- Definitions ---------------- */

/*
 * Define the minimum parser input buffer size.  The input buffer can
 * contain as much or as little data as the client wishes to supply;
 * the parser will always consume all of it.
 *
 * The minimum input buffer size is determined by the largest minimum size
 * required by the data decompression methods for bitmaps, which is currently
 * 1 byte for the RLE method.  Aside from that, the minimum size demanded
 * by the language itself is the size of the largest fixed-size language
 * token, which is 17 bytes (real32 or int32 box).  However, there is no
 * requirement that the client provide a buffer of this size: the parser
 * will buffer data internally if necessary.
 */
#define px_parser_max_token_size 1024
#define px_parser_min_input_size px_parser_max_token_size

/* Define the parser macro-states (used only for checking). */
#define ptsInSession		0x01
#define ptsInPage		0x02
#define ptsData			0x04    /* read data, want attribute */
#define ptsReadingStream	  0x08
#define ptsReadingRastPattern	  0x10
#define ptsReadingChar		  0x18
#define ptsReadingFont		  0x20
#define ptsReadingImage		  0x28
#define ptsReadingScan		  0x30
#define ptsReading		0x38    /* reading state */
#define ptsExecStream		0x40    /* executing a stream */
#define ptsInitial 0

/* Define the parser state. */
#ifndef px_parser_state_t_DEFINED
#  define px_parser_state_t_DEFINED
typedef struct px_parser_state_s px_parser_state_t;
#endif
#define max_stack max_px_args   /* must not exceed 256 */
struct px_parser_state_s
{
    /* Set at initialization */
    gs_memory_t *memory;
    bool big_endian;
    /* Updated dynamically, for error reporting only */
    long operator_count;
    long parent_operator_count;
    int /*px_tag_t */ last_operator;    /* pxtNull if none */
    /* Updated dynamically */
    int saved_count;            /* amount of leftover input in saved */
    uint data_left;             /* amount left to read of data or array */
    uint macro_state;           /* mask for macro-state */
    int stack_count;
        px_operator_proc((*data_proc)); /* operator awaiting data, if any */
    byte saved[px_parser_max_token_size];
    px_value_t stack[max_stack + 2];
    px_args_t args;
    byte attribute_indices[px_attribute_next];  /* indices of attrs on stack */
};

/* Define an abstract type for the state. */
#ifndef px_state_DEFINED
#  define px_state_DEFINED
typedef struct px_state_s px_state_t;
#endif

/* ---------------- Procedural interface ---------------- */

/* Allocate a parser state. */
px_parser_state_t *px_process_alloc(gs_memory_t * memory);

/* Release a parser state. */
void px_process_release(px_parser_state_t * st);

/* Initialize the parser state. */
void px_process_init(px_parser_state_t * st, bool big_endian);

/* Process a buffer of PCL XL commands. */
int px_process(px_parser_state_t * st, px_state_t * pxs,
               stream_cursor_read * pr);

/* unfortunately we have to export this for pass through mode, other
   commands do not need to know how much data is left to parse. */
uint px_parser_data_left(px_parser_state_t * pxp);

#endif /* pxparse_INCLUDED */
