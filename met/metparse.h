/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001, 2005 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$Id$*/

/* metro parser interface */

#ifndef metparse_INCLUDED
#  define metparse_INCLUDED

#include "metstate.h" /* nb this should be separated out - reconsider */

/* the size of the input buffer to be used by the parser */
#define MET_PARSER_MIN_INPUT_SIZE 8192

typedef struct met_parser_state_s met_parser_state_t;

typedef struct data_element_s {
    char debug_info[1024];
    void *data;
} data_element_t;

struct met_parser_state_s {
    gs_memory_t *memory;
    int error_code; /* error detected in a callback */
    int depth;
    void *parser;
    data_element_t data_stack[20]; /* nb should not have depth limitation */
    int stack_top;
    met_state_t *mets; /* nb this should be separated out - reconsider */
};

/* Allocate a parser state. */
met_parser_state_t *met_process_alloc(gs_memory_t *memory);

/* Release a parser state. */
void met_process_release(met_parser_state_t *st);

/* Initialize the parser state. */
void met_process_init(met_parser_state_t *st);

/* Process a buffer of metro/XML. */
int met_process(met_parser_state_t *st, met_state_t *pxs,
                void *pzip, stream_cursor_read *pr);

/* shutdown the parser */
int met_process_shutdown(met_parser_state_t *st);

#endif				/* metparse_INCLUDED */
