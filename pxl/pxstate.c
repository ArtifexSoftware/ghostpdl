/* Copyright (C) 1996, 1998 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pxstate.c */
/* State allocation/initialization/cleanup */

#include "stdio_.h"			/* std.h + NULL */
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "pxstate.h"

/* Import the initialization procedure table from pxmain.c. */
typedef int (*px_init_proc)(P1(px_state_t *));
extern const px_init_proc px_init_table[];

/* GC descriptors */
private_st_px_state();
#define pxs ((px_state_t *)vptr)
private ENUM_PTRS_BEGIN(px_state_enum_ptrs) {
	index -= px_state_num_ptrs + px_state_num_string_ptrs;
#define md(i,e)\
  if ( index < st_px_dict_max_ptrs )\
    return ENUM_SUPER_ELT(px_state_t, st_px_dict, e, 0);\
  index -= st_px_dict_max_ptrs;
	px_state_do_dicts(md)
#undef md
	return 0;
	}
#undef ENUM_DICT
#define mp(i,e) ENUM_PTR(i, px_state_t, e);
#define ms(i,e) ENUM_STRING_PTR(px_state_num_ptrs + i, px_state_t, e);
	px_state_do_ptrs(mp)
	px_state_do_string_ptrs(ms)
#undef mp
#undef ms
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(px_state_reloc_ptrs) {
#define mp(i,e) RELOC_PTR(px_state_t, e);
#define ms(i,e) RELOC_STRING_PTR(px_state_t, e);
#define md(i,e)\
  RELOC_SUPER_ELT(px_state_t, st_px_dict, e);
	px_state_do_ptrs(mp)
	px_state_do_string_ptrs(ms)
	px_state_do_dicts(md)
#undef mp
#undef ms
#undef md
} RELOC_PTRS_END
#undef pxs

/* Allocate a px_state_t. */
px_state_t *
px_state_alloc(gs_memory_t *memory)
{	px_state_t *pxs = gs_alloc_struct(memory, px_state_t, &st_px_state,
					  "px_state_alloc");
	px_gstate_t *pxgs = px_gstate_alloc(memory);

	if ( pxs == 0 || pxgs == 0 )
	  { gs_free_object(memory, pxgs, "px_gstate_alloc");
	    gs_free_object(memory, pxs, "px_state_alloc");
	    return 0;
	  }
	pxs->memory = memory;
	/* Clear all pointers known to GC. */
#define mp(i,e) pxs->e = 0;
#define ms(i,e) pxs->e.data = 0;
#define md(i,e) pl_dict_init(&pxs->e, pxs->memory, NULL);
	px_state_do_ptrs(mp)
	px_state_do_string_ptrs(ms)
	px_state_do_dicts(md)
#undef mp
#undef ms
#undef md
	pxs->pxgs = pxgs;
	pxgs->pxs = pxs;
	px_state_init(pxs, NULL);
	/* Run module initialization code. */
	{ const px_init_proc *init;
	  for ( init = px_init_table; *init; ++init )
	    (*init)(pxs);
	}
	return pxs;
}

/* Do one-time state initialization. */
/* There isn't much of this: most state is initialized per-session. */
void
px_state_init(px_state_t *pxs, gs_state *pgs)
{	pxs->pgs = pgs;
	px_gstate_init(pxs->pxgs, pgs);
	pxs->error_report = eErrorPage;	/* default before first session */
	pxs->end_page = px_default_end_page;
	pxs->data_source_open = false;
	px_dict_init(&pxs->stream_dict, pxs->memory, NULL);
	pxs->warning_length = 0;
}

/* Do one-time finalization at the end of execution. */
void
px_state_finit(px_state_t *pxs)
{	/* If streams persisted across sessions, we would release them here. */
#if 0
	px_dict_release(&pxs->stream_dict);
#endif
}
