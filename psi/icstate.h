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


/* Externally visible context state */

#ifndef icstate_INCLUDED
#  define icstate_INCLUDED

#include "imemory.h"
#include "iref.h"
#include "idsdata.h"
#include "iesdata.h"
#include "iosdata.h"
#include "stream.h"
#include "opdef.h"
#include "gsgstate.h"
#include "files.h"
#include "interp.h"

struct gs_context_state_s {
    gs_gstate *pgs;
    gs_dual_memory_t memory;
    int language_level;
    ref array_packing;		/* t_boolean */
    ref binary_object_format;	/* t_integer */
    long nv_page_count;    /* non-decreasing page counter for /PageCount */
                           /* It's updated only by currentsystemparams .*/
    long rand_state;		/* (not in Red Book) */
    long usertime_0[2];         /* initial value first time usertime was called */
    bool usertime_inited;       /* has usertime been called yet? */
    /* View clipping is handled in the graphics state. */
    ref error_object;		/* t__invalid or error object from operator */
    ref userparams;		/* t_dictionary */
    int scanner_options;	/* derived from userparams */
    bool LockFilePermissions;	/* accessed from userparams */
    bool starting_arg_file;	/* starting a file specified in command line. */
    bool RenderTTNotdef;	/* accessed from userparams */
    gs_file_path_ptr lib_path;	/* library search list (GS_LIB) */
    ref stdio[3];		/* t_file */
    stream *invalid_file_stream;/* An invalid file object (stable memory) */
    op_array_table op_array_table_global; /* Global operator table */
    op_array_table op_array_table_local;  /* Local operator table */
    int time_slice_ticks;                 /* Ticks before next slice */
    gs_offset_t uel_position;   /* The file position at which we last hit UEL */

    /* Put the stacks at the end to minimize other offsets. */
    dict_stack_t dict_stack;
    exec_stack_t exec_stack;
    op_stack_t op_stack;
    struct i_plugin_holder_s *plugin_list;
};
extern const long rand_state_initial; /* in zmath.c */

/*
 * We make st_context_state public because zcontext.c must subclass it.
 */
/*extern_st(st_context_state); *//* in icontext.h */
#define public_st_context_state()	/* in icontext.c */\
  gs_public_st_complex_only(st_context_state, gs_context_state_t,\
    "gs_context_state_t", context_state_clear_marks,\
    context_state_enum_ptrs, context_state_reloc_ptrs, 0)

#endif /* icstate_INCLUDED */
