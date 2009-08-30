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
/*$Id$ */

/* pxstream.c */
/* PCL XL user-defined stream operators */

#include "memory_.h"
#include "gsmemory.h"
#include "scommon.h"
#include "pxoper.h"
#include "pxstate.h"
#include "pxparse.h"
#include "pxptable.h"

/* ---------------- Internal procedures ---------------- */

/* Tag a stream name with its character width. */
static int
tag_stream_name(const px_value_t *psnv, gs_string *pstr,
  gs_memory_t *mem, client_name_t cname)
{       uint size = array_value_size(psnv);
        byte *str = gs_alloc_string(mem, size + 1, cname);

        if ( str == 0 )
          return_error(errorInsufficientMemory);
        str[0] = value_size(psnv);
        memcpy(str + 1, psnv->value.array.data, size);
        pstr->data = str;
        pstr->size = size + 1;
        return 0;
}

/* ---------------- Operators ---------------- */

const byte apxBeginStream[] = {
  pxaStreamName, 0, 0
};
int
pxBeginStream(px_args_t *par, px_state_t *pxs)
{       int code = tag_stream_name(par->pv[0], &pxs->stream_name, pxs->memory,
                                   "pxBeginStream(name)");

        if ( code < 0 )
          return code;
        pxs->stream_def.size = 0;
        pl_dict_undef(&pxs->stream_dict, pxs->stream_name.data,
                      pxs->stream_name.size);
        return 0;
}

const byte apxReadStream[] = {
  pxaStreamDataLength, 0, 0
};
int
pxReadStream(px_args_t *par, px_state_t *pxs)
{       ulong len = par->pv[0]->value.i;
        ulong copy = min(len - par->source.position, par->source.available);
        uint old_size = pxs->stream_def.size;
        byte *str;

        if ( copy == 0 )
          return pxNeedData;
        if ( old_size == 0 )
          str = gs_alloc_bytes(pxs->memory, copy, "pxReadStream");
        else
          str = gs_resize_object(pxs->memory, pxs->stream_def.data,
                                 old_size + copy, "pxReadStream");
        if ( str == 0 )
          return_error(errorInsufficientMemory);
        memcpy(str + old_size, par->source.data, copy);
        pxs->stream_def.data = str;
        pxs->stream_def.size = old_size + copy;
        par->source.data += copy;
        par->source.available -= copy;
        return ((par->source.position += copy) == len ? 0 : pxNeedData);
}

const byte apxEndStream[] = {0, 0};
int
pxEndStream(px_args_t *par, px_state_t *pxs)
{       int code = pl_dict_put(&pxs->stream_dict, pxs->stream_name.data,
                               pxs->stream_name.size, pxs->stream_def.data);

        gs_free_string(pxs->memory, pxs->stream_name.data,
                       pxs->stream_name.size, "pxEndStream(name)");
        return (code < 0 ? gs_note_error(errorInsufficientMemory) : 0);
}

const byte apxRemoveStream[] = {
  pxaStreamName, 0, 0
};

int
pxRemoveStream(px_args_t *par, px_state_t *pxs)
{       
    gs_string str;
    void *def;
    int code = tag_stream_name(par->pv[0], &str, pxs->memory,
                               "pxExecStream(name)");
    if ( code < 0 )
        return code;
    { 
        bool found = pl_dict_find(&pxs->stream_dict, str.data, str.size,
                                  &def);
        if ( !found )
            return_error(errorStreamUndefined);
        pl_dict_undef(&pxs->stream_dict, str.data, str.size);
        gs_free_string(pxs->memory, str.data, str.size,
                       "pxRemoveStream(name)");
    }
    return 0;
}

const byte apxExecStream[] = {
  pxaStreamName, 0, 0
};
int
pxExecStream(px_args_t *par, px_state_t *pxs)
{       gs_string str;
        void *def;
        const byte *def_data;
        uint def_size;
        bool big_endian;
        const byte *start;
        px_parser_state_t *pst = par->parser;
        px_parser_state_t st;
        stream_cursor_read r;
        int code = tag_stream_name(par->pv[0], &str, pxs->memory,
                                   "pxExecStream(name)");

        if ( code < 0 )
          return code;

        if ( pxs->stream_level > 32 )
            return_error(errorStreamNestingFull);

        { bool found = pl_dict_find(&pxs->stream_dict, str.data, str.size,
                                    &def);

          gs_free_string(pxs->memory, str.data, str.size,
                         "pxExecStream(name)");
          if ( !found )
            return_error(errorStreamUndefined);
        }
        def_data = def;
        def_size = gs_object_size(pxs->memory, def);
        /* We do all the syntax checking for streams here, rather than */
        /* in ReadStream or EndStream, for simplicity. */
        switch ( def_data[0] )
          {
          case '(': big_endian = true; break;
          case ')': big_endian = false; break;
          default: return_error(errorUnsupportedBinding);
          }
        if ( def_size < 16 ||
             strncmp(def_data + 1, " HP-PCL XL", 10)
           )
          return_error(errorUnsupportedClassName);
        /* support protocol level 1, 2 and 3 */
        if ( strncmp(def_data + 11, ";1;", 3) && 
             strncmp(def_data + 11, ";2;", 3) &&
             strncmp(def_data + 11, ";3;", 3) )
            return_error(errorUnsupportedProtocol);
        start = memchr(def_data + 14, '\n', def_size - 14);
        if ( !start )
          return_error(errorIllegalStreamHeader);
        st.memory = pxs->memory;
        px_process_init(&st, big_endian);
        st.macro_state = pst->macro_state | ptsExecStream;
        st.last_operator = pst->last_operator;
        r.ptr = start;
        r.limit = def_data + def_size - 1;
        pxs->stream_level++;
        code = px_process(&st, pxs, &r);
        pxs->stream_level--;
        pst->macro_state = st.macro_state & ~ptsExecStream;
        if ( code < 0 )
          { /* Set the operator counts for error reporting. */
            pst->parent_operator_count = pst->operator_count;
            pst->operator_count = st.operator_count;
            pst->last_operator = st.last_operator;
          }
        return code;
}
