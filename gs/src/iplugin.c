/* Copyright (C) 1995, 1996, 1997, 1998, 1999, 2001 Aladdin Enterprises.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/*$Id$ */
/* Plugin manager */

#include "malloc_.h"
#include "string_.h"
#include "ghost.h"
#include "gxalloc.h"
#include "errors.h"
#include "ialloc.h"
#include "iplugin.h"
#include "icstate.h"

/*  Plugin list is being build in raw memory pool,
    because it is irrelevant to PS virtual memory.
    At least it must live during alloc_restore_all, allowing
    plugins to handle the finalization of objects they manage.
*/

extern_i_plugin_table();

private void *i_plugin_mem_alloc(i_plugin_client_memory *mem, unsigned int nbytes, const char *cname)
{   gs_raw_memory_t *mem_raw = (gs_raw_memory_t *)mem->client_data;
    return mem_raw->procs.alloc_bytes_immovable(mem_raw, nbytes, cname);
}

private void i_plugin_mem_free(i_plugin_client_memory *mem, void *data, const char *cname)
{   gs_raw_memory_t *mem_raw = (gs_raw_memory_t *)mem->client_data;
    mem_raw->procs.free_object(mem_raw, data, cname);
}

void i_plugin_make_memory(i_plugin_client_memory *mem, gs_raw_memory_t *mem_raw)
{   mem->client_data = mem_raw;
    mem->alloc = i_plugin_mem_alloc;
    mem->free  = i_plugin_mem_free;
}

int i_plugin_init(i_ctx_t *i_ctx_p)
{   gs_raw_memory_t *mem_raw = i_ctx_p->memory.current->parent;
    const i_plugin_instantiation_proc *p = i_plugin_table;
    i_plugin_holder *h;
    int code;
    i_plugin_client_memory client_mem;
    i_plugin_make_memory(&client_mem, mem_raw);
    for (; *p != 0; p++) {
        i_plugin_instance *instance = 0;
        code = (*p)(i_ctx_p, &client_mem, &instance);
        if (code != 0)
            return code;
        h = (i_plugin_holder *)gs_alloc_bytes_immovable(mem_raw, sizeof(i_plugin_holder), "plugin_holder");
        if (h == 0)
            return_error(e_Fatal);
        h->I = instance;
        h->next = i_ctx_p->plugin_list;
        i_ctx_p->plugin_list = h;
    }
    return 0;
}

void i_plugin_finit(gs_raw_memory_t *mem_raw, i_plugin_holder *list)
{   i_plugin_client_memory client_mem;
    i_plugin_make_memory(&client_mem, mem_raw);
    while (list != 0) {
        i_plugin_holder *h = list;
        list = h->next;
        h->I->d->finit(h->I, &client_mem);
        gs_free_object(mem_raw, h, "plugin_holder");
    }
}

i_plugin_holder * i_plugin_get_list(i_ctx_t *i_ctx_p)
{   return i_ctx_p->plugin_list;
}

i_plugin_instance *i_plugin_find(i_ctx_t *i_ctx_p, const char *type, const char *subtype)
{   i_plugin_holder *h = i_ctx_p->plugin_list;
    for (; h != 0; h = h->next) {
        i_plugin_instance *I = h->I;
        if (!strcmp(I->d->type, type) && !strcmp(I->d->subtype, subtype))
            return I;
    }
    return NULL;
}

/* todo: define plugin enumerator by 'type' */
