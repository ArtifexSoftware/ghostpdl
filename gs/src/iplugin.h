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

#ifndef iplugin_INCLUDED
#define iplugin_INCLUDED

#ifndef i_ctx_t_DEFINED
#define i_ctx_t_DEFINED
typedef struct gs_context_state_s i_ctx_t;
#endif

#ifndef gs_raw_memory_t_DEFINED
#define gs_raw_memory_t_DEFINED
typedef struct gs_raw_memory_s gs_raw_memory_t;
#endif

typedef struct i_plugin_holder_s i_plugin_holder;
typedef struct i_plugin_instance_s i_plugin_instance;
typedef struct i_plugin_descriptor_s i_plugin_descriptor;
typedef struct i_plugin_client_memory_s i_plugin_client_memory;

struct i_plugin_descriptor_s { /* RTTI for plugins */
    const char *type;          /* Plugin type, such as "FAPI" */
    const char *subtype;       /* Plugin type, such as "UFST" */
    void (*finit)(P2(i_plugin_instance *instance, i_plugin_client_memory *mem)); /* Destructor & deallocator for the instance. */
};

struct i_plugin_instance_s {   /* Base class for various plugins */
    const i_plugin_descriptor *d;
};

struct i_plugin_holder_s { /* Forms list of plugins for plugin manager */
    i_plugin_holder *next;
    i_plugin_instance *I;
};

struct i_plugin_client_memory_s { /* must be copying */
    void *client_data;
    void *(*alloc)(i_plugin_client_memory *mem, unsigned int size, const char *id);
    void (*free)(i_plugin_client_memory *mem, void *data, const char *cname);
};

#define plugin_instantiation_proc(proc)\
  int proc(P3(i_ctx_t *, i_plugin_client_memory *client_mem, i_plugin_instance **instance))

#define extern_i_plugin_table()\
  typedef plugin_instantiation_proc((*i_plugin_instantiation_proc));\
  extern const i_plugin_instantiation_proc i_plugin_table[]

void i_plugin_make_memory(i_plugin_client_memory *mem, gs_raw_memory_t *mem_raw);
int i_plugin_init(P1(i_ctx_t *));
void i_plugin_finit(P2(gs_raw_memory_t *mem, i_plugin_holder *list));
i_plugin_instance *i_plugin_find(P3(i_ctx_t *i_ctx_p, const char *type, const char *subtype));
i_plugin_holder * i_plugin_get_list(P1(i_ctx_t *i_ctx_p));

#endif /* iplugin_INCLUDED */
