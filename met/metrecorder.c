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

/* record and playback cooked elements */

#include "metrecorder.h"
#include "gserror.h"
#include "string_.h"
#include "metutil.h"

typedef struct metrecord_s metrecord_t;

struct metrecord_s {
    data_element_t *data;
    struct metrecord_s *next;
    /* NB should be debugging */
    bool open;
    char *element;
    int depth;

};

/* NB static fixme */

private metrecord_t *head = NULL;
private metrecord_t *tail = NULL;

int
met_record(gs_memory_t *mem, const data_element_t *data, const char *xmlel, bool open, int depth)
{
    metrecord_t *node = (metrecord_t *)gs_alloc_bytes(mem, 
                                    sizeof(metrecord_t), "met record node");
    data_element_t *el = (data_element_t *)gs_alloc_bytes(mem,
                                    sizeof(met_element_t), "met data element");
    void *cooked_data = gs_alloc_bytes(mem,
                                       gs_object_size(mem, data->cooked_data), "met record cooked data");

    if (!node || !data || !el) {
        return gs_throw(-1, "memory error recording");
    }
    
    *el = *data;
    memcpy(cooked_data, data->cooked_data,
           gs_object_size(mem, data->cooked_data));
    el->cooked_data = cooked_data;
    node->data = el;
    node->open = open;
    node->element = met_strdup(mem, xmlel);
    node->depth = depth;
    if (head == NULL) {
        head = tail = node;
    } else {
        tail = tail->next = node;
    }
    return 0;
}

int
met_playback(met_state_t *ms)
{
    metrecord_t *node = ms->current_resource;
    int code = 0;

    if (node == NULL) {
        return gs_throw(-1, "playback list is empty");
    }
    
    do {
        data_element_t *data = node->data;
        met_element_procs_t *procs = data->met_element_procs;
        met_selector sel = data->sel;
        
        switch (sel) {
        case met_cook:
            /* nb should clean up but shouldn't happen */
            return gs_throw(-1, "fatal cooking in playback");
            break;

        case met_action:
            if (gs_debug_c('i')) dprintf2("%d:opening %s\n", node->depth, node->element);
            code = (*procs->action)(data->cooked_data, ms);
            break;
        case met_done:
            if (gs_debug_c('i')) dprintf2("%d:closing %s\n", node->depth, node->element);
            code = (*procs->done)(data->cooked_data, ms);
            break;
        default:
            /* nb should clean up but shouldn't happen */
            return gs_throw(-1, "playback error unknown operation");
        }

        if (code < 0)
            return gs_rethrow(code, "playback error");
        if (node == tail)
            break;
        node = node->next;
    } while (1);

    return code;
}

int
met_store(met_state_t *ms)
{
    int code = 0;
    if (code == 0) {
        ms->current_resource = head;
        head = NULL;
        return code;
    }
    return gs_throw(code, "failed to store resource");
}
