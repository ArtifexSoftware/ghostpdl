/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Common output syntax and parameters for PostScript and PDF writers */

#ifndef spsdf_INCLUDED
#  define spsdf_INCLUDED

#include "gsparam.h"

/* Define an opaque type for streams. */
#ifndef stream_DEFINED
#  define stream_DEFINED
typedef struct stream_s stream;
#endif

/* ---------------- Symbolic data printing ---------------- */

/* Print a PostScript string in the most efficient form. */
#define PRINT_BINARY_OK 1
#define PRINT_ASCII85_OK 2
#define PRINT_HEX_NOT_OK 4
void s_write_ps_string(P4(stream * s, const byte * str, uint size,
			  int print_ok));

/*
 * Create a stream that just keeps track of how much has been written
 * to it.  We use this for measuring data that will be stored rather
 * than written to an actual stream.
 */
int s_alloc_position_stream(P2(stream ** ps, gs_memory_t * mem));

/*
 * Create/release a parameter list for printing (non-default) filter
 * parameters.  This should probably migrate to a lower level....
 */
typedef struct param_printer_params_s {
    const char *prefix;		/* before entire object, if any params */
    const char *suffix;		/* after entire object, if any params */
    const char *item_prefix;	/* before each param */
    const char *item_suffix;	/* after each param */
    int print_ok;
} param_printer_params_t;
/*
 * The implementation structure should be opaque, but there are a few
 * clients that need to be able to stack-allocate it.
 */
typedef struct printer_param_list_s {
    gs_param_list_common;
    stream *strm;
    param_printer_params_t params;
    bool any;
} printer_param_list_t;
#define private_st_printer_param_list()	/* in spsdf.c */\
  gs_private_st_ptrs1(st_printer_param_list, printer_param_list_t,\
    "printer_param_list_t", printer_plist_enum_ptrs, printer_plist_reloc_ptrs,\
    strm)

#define param_printer_params_default_values 0, 0, 0, "\n", 0
extern const param_printer_params_t param_printer_params_default;
int s_alloc_param_printer(P4(gs_param_list ** pplist,
			     const param_printer_params_t * ppp, stream * s,
			     gs_memory_t * mem));
void s_free_param_printer(P1(gs_param_list * plist));
/* Initialize or release a list without allocating or freeing it. */
int s_init_param_printer(P3(printer_param_list_t *prlist,
			    const param_printer_params_t * ppp, stream * s));
void s_release_param_printer(P1(printer_param_list_t *prlist));


#endif /* spsdf_INCLUDED */
