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



/* this is the ps interpreter interface to the native font
   enumeration code. it calls the platform-specific routines
   to obtain an additional set of entries that can be added
   to the Fontmap to reference fonts stored on the system.
 */

#include "memory_.h"
#include "string_.h"
#include <stdlib.h>
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "gsmalloc.h"
#include "ialloc.h"
#include "iname.h"
#include "iutil.h"
#include "store.h"
#include "gp.h"

typedef struct fontenum_s {
        char *fontname, *path;
        struct fontenum_s *next;
} fontenum_t;

/* .getnativefonts [ [<name> <path>] ... ] */
static int
z_fontenum(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    void *enum_state = NULL;
    int code = 0;
    int e,elements, e2;
    char *fontname, *path;
    fontenum_t *r, *results;
    ref array;
    uint length;
    uint length2;
    byte *nfname, *nfpath;

    enum_state = gp_enumerate_fonts_init(imemory);
    if (enum_state == NULL) {
      /* put false on the stack and return */
      push(1);
      make_bool(op, false);
      goto all_done;
    }

    r = results = gs_malloc(imemory->non_gc_memory, 1, sizeof(fontenum_t), "fontenum list");
    if (!r) {
        code = gs_note_error(gs_error_VMerror);
        goto done;
    }
    r->next = NULL;
    elements = 0;
    while((code = gp_enumerate_fonts_next(enum_state, &fontname, &path )) > 0) {
        if (fontname == NULL || path == NULL) {
            code = gs_note_error(gs_error_ioerror);
            goto done;
        }

        length = strlen(fontname) + 1;
        r->fontname = gs_malloc(imemory->non_gc_memory, length, 1, "native font name");
        if (r->fontname) {
            memcpy(r->fontname, fontname, length);

            r->next = gs_malloc(imemory->non_gc_memory, 1, sizeof(fontenum_t), "fontenum list");
            length2 = strlen(path) + 1;
            r->path = gs_malloc(imemory->non_gc_memory, length2, 1, "native font path");
            if (r->next == NULL || r->path == NULL) {
                gs_free(imemory->non_gc_memory, r->fontname, length, 1, "native font name");
                gs_free(imemory->non_gc_memory, r->path, length2, 1, "native font path");
                gs_free(imemory->non_gc_memory, r->next, sizeof(fontenum_t), 1, "fontenum list");
            }
            else {
                memcpy(r->path, path, length2);
                r = r->next;
                elements += 1;
            }
        }
    }

    gp_enumerate_fonts_free(enum_state);
    enum_state = NULL;

    if ((code = ialloc_ref_array(&array, a_all | icurrent_space, elements, "native fontmap")) >= 0) {
        r = results;
        for (e = e2 = 0; e < elements; e++) {
            ref mapping;

            if ((code = ialloc_ref_array(&mapping, a_all | icurrent_space, 2, "native font mapping")) >= 0) {
                length = strlen(r->fontname);
                length2 = strlen(r->path);
                nfname = ialloc_string(length, "native font name");
                nfpath = ialloc_string(length2, "native font path");
                if (nfname == NULL || nfpath == NULL) {
                    ifree_string(nfname, length, "native font name");
                    ifree_string(nfpath, length2, "native font name");
                }
                else {
                   memcpy(nfname, r->fontname, length);
                   make_string(&(mapping.value.refs[0]), a_all | icurrent_space, length, nfname);
                   memcpy(nfpath, r->path, length2);
                   make_string(&(mapping.value.refs[1]), a_all | icurrent_space, length2, nfpath);

                   ref_assign(&(array.value.refs[e2]), &mapping);
                   e2++;
                }
            }
            results = r;
            r = r->next;

            gs_free(imemory->non_gc_memory, results->fontname,
                    strlen(results->fontname) + 1, 1, "native font name");
            gs_free(imemory->non_gc_memory, results->path,
                    strlen(results->path) + 1, 1, "native font path");
            gs_free(imemory->non_gc_memory, results, 1,
                    sizeof(fontenum_t), "fontenum list");
        }
    }
    else {
        while (elements--) {
            r = results->next;
            gs_free(imemory->non_gc_memory, results->fontname,
                    strlen(results->fontname) + 1, 1, "native font name");
            gs_free(imemory->non_gc_memory, results->path,
                    strlen(results->path) + 1, 1, "native font path");
            gs_free(imemory->non_gc_memory, results, 1,
                    sizeof(fontenum_t), "fontenum list");
            results = r;
        }
    }

done:
    if (enum_state)
        gp_enumerate_fonts_free(enum_state);
    if (code >= 0) {
        push(2);
        ref_assign(op-1, &array);
        make_bool(op, true);
    }
all_done:
    return code;
}

/* Match the above routines to their postscript filter names.
   This is how our static routines get called externally. */
const op_def zfontenum_op_defs[] = {
    {"0.getnativefonts", z_fontenum},
    op_def_end(0)
};
