/* Copyright (C) 1991, 1995, 1997 Aladdin Enterprises.  All rights reserved.
  
  This file is part of Aladdin Ghostscript.
  
  Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
  or distributor accepts any responsibility for the consequences of using it,
  or for whether it serves any particular purpose or works at all, unless he
  or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
  License (the "License") for full details.
  
  Every copy of Aladdin Ghostscript must include a copy of the License,
  normally in a plain ASCII text file named PUBLIC.  The License grants you
  the right to copy, modify and redistribute Aladdin Ghostscript, but only
  under certain conditions described in the License.  Among other things, the
  License requires that the copyright notice and this notice be preserved on
  all copies.
*/

/* zcontext.c */
/* Display PostScript context operators */
#include "memory_.h"
#include "ghost.h"
#include "gp.h"			/* for usertime */
#include "errors.h"
#include "oper.h"
#include "gsexit.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gxalloc.h"
#include "idict.h"
#include "igstate.h"
#include "icontext.h"
#include "istruct.h"
#include "dstack.h"
#include "estack.h"
#include "ostack.h"
#include "store.h"

/* Still needs:
   fork: copy gstate stack
   fork_done: do all necessary restores
 */

/* Scheduling hooks in interp.c */
extern int (*gs_interp_reschedule_proc)(P0());
extern int (*gs_interp_time_slice_proc)(P0());
extern int gs_interp_time_slice_ticks;

/* Per-context state stored in statics */
extern ref ref_binary_object_format;

/* Context state structure */
/* (defined in icontext.h) */
extern_st(st_ref_stack);
private_st_context_state();
#define pcst ((gs_context_state_t *)vptr)
private ENUM_PTRS_BEGIN(context_state_enum_ptrs) return 0;
	ENUM_REF(0, gs_context_state_t, dstack);
	ENUM_REF(1, gs_context_state_t, estack);
	ENUM_REF(2, gs_context_state_t, ostack);
	ENUM_PTR(3, gs_context_state_t, pgs);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(context_state_reloc_ptrs) {
	RELOC_REF(gs_context_state_t, dstack);
	RELOC_REF(gs_context_state_t, estack);
	RELOC_REF(gs_context_state_t, ostack);
	RELOC_PTR(gs_context_state_t, pgs);
} RELOC_PTRS_END
#undef pcst

/* Context structure */
typedef enum {
	cs_active,
	cs_done
} ctx_status;
typedef struct gs_context_s gs_context;
struct gs_context_s {
	gs_context_state_t state;	/* (must be first for subclassing) */
		/* Private state */
	ctx_status status;
	ulong index;
	bool detach;			/* true if a detach has been */
					/* executed for this context */
	gs_context *next;		/* next context with same status */
					/* (active, waiting on same lock, */
					/* waiting on same condition, */
					/* waiting to be destroyed) */
	gs_context *joiner;		/* context waiting on a join */
					/* for this one */
	gs_context *table_next;		/* hash table chain */
};
gs_private_st_suffix_add3(st_context, gs_context, "context",
  context_enum_ptrs, context_reloc_ptrs, st_context_state,
  next, joiner, table_next);

/* Context list structure */
typedef struct ctx_list_s {
	gs_context *head;
	gs_context *tail;
} ctx_list;

/* Condition structure */
typedef struct gs_condition_s {
	ctx_list waiting;	/* contexts waiting on this condition */
} gs_condition;
gs_private_st_ptrs2(st_condition, gs_condition, "conditiontype",
  condition_enum_ptrs, condition_reloc_ptrs, waiting.head, waiting.tail);

/* Lock structure */
typedef struct gs_lock_s {
	ctx_list waiting;		/* contexts waiting for this lock, */
					/* must be first for subclassing */
	gs_context *holder;		/* context holding the lock, if any */
} gs_lock;
gs_private_st_suffix_add1(st_lock, gs_lock, "locktype",
  lock_enum_ptrs, lock_reloc_ptrs, st_condition, holder);

/* Global state */
typedef struct gs_scheduler_s {
  gs_context *current;
  long usertime_initial;	/* usertime when current started running */
  ctx_list active;
  gs_context *dead;
#define ctx_table_size 19
  gs_context *table[ctx_table_size];
} gs_scheduler_t;
private gs_scheduler_t *gs_scheduler;
#define ctx_current gs_scheduler->current
#define ctx_active gs_scheduler->active
#define ctx_dead gs_scheduler->dead
#define ctx_table gs_scheduler->table
private gs_gc_root_t gs_scheduler_root;
/* Structure definition */
gs_private_st_composite(st_scheduler, gs_scheduler_t, "gs_scheduler",
  scheduler_enum_ptrs, scheduler_reloc_ptrs);
private ENUM_PTRS_BEGIN(scheduler_enum_ptrs) {
  index -= 4;
  if ( index < ctx_table_size )
    ENUM_RETURN_PTR(gs_scheduler_t, table[index]);
  return 0;
}
  ENUM_PTR(0, gs_scheduler_t, current);
  ENUM_PTR(1, gs_scheduler_t, active.head);
  ENUM_PTR(2, gs_scheduler_t, active.tail);
  ENUM_PTR(3, gs_scheduler_t, dead);
ENUM_PTRS_END
private RELOC_PTRS_BEGIN(scheduler_reloc_ptrs) {
  RELOC_PTR(gs_scheduler_t, current);
  RELOC_PTR(gs_scheduler_t, active.head);
  RELOC_PTR(gs_scheduler_t, active.tail);
  RELOC_PTR(gs_scheduler_t, dead);
  { int i;
    for ( i = 0; i < ctx_table_size; ++i )
      RELOC_PTR(gs_scheduler_t, table[i]);
  }
} RELOC_PTRS_END

/* Forward references */
private int context_create(P2(gs_context **, gs_ref_memory_t *));
private long context_usertime(P0());
#define context_load(psched, pctx)\
  do {\
    if ( (pctx)->state.keep_usertime )\
      (psched)->usertime_initial = context_usertime();\
    context_state_load(&(pctx)->state);\
  } while (0)
#define context_store(psched, pctx)\
  do {\
    context_state_store(&(pctx)->state);\
    if ( (pctx)->state.keep_usertime )\
      (pctx)->state.usertime_total +=\
        context_usertime() - (psched)->usertime_initial;\
  } while (0)
private int context_param(P2(os_ptr, gs_context **));
#define check_context(op, vpc)\
  if ( (code = context_param(op, &vpc)) < 0 ) return code
private void context_destroy(P1(gs_context *));
private void stack_copy(P4(ref_stack *, const ref_stack *, uint, uint));
private int lock_acquire(P1(os_ptr));
private int lock_release(P1(ref *));

/* List manipulation macros */
#define add_last(pl,pc)\
  (((pl)->head == 0 ? ((pl)->head = pc) : ((pl)->tail->next = pc)),\
   (pl)->tail = pc, (pc)->next = 0)
#define add_last_all(pl,pcl)		/* pcl->head != 0 */\
  (((pl)->head == 0 ? ((pl)->head = (pcl)->head) :\
    ((pl)->tail->next = (pcl)->head)),\
   (pl)->tail = (pcl)->tail, (pcl)->head = (pcl)->tail = 0)

/* ------ Initialization ------ */

private int ctx_reschedule(P0());
private int ctx_time_slice(P0());
private void
zcontext_init(void)
{	gs_ref_memory_t *imem = iimemory_global;

	gs_scheduler = gs_alloc_struct((gs_memory_t *)imem, gs_scheduler_t,
				       &st_scheduler, "gs_scheduler");
	gs_register_struct_root((gs_memory_t *)imem, &gs_scheduler_root,
				(void **)&gs_scheduler, "gs_scheduler");
	ctx_current = 0;
	ctx_active.head = ctx_active.tail = 0;
	ctx_dead = 0;
	memset(ctx_table, 0, sizeof(ctx_table));
	/* Create an initial context. */
	if ( context_create(&ctx_current, imem) < 0 )
	  { lprintf("Can't create initial context!");
	    gs_abort();
	  }
	ctx_current->state.memory = gs_imemory;
	/* Hook into the interpreter. */
	gs_interp_reschedule_proc = ctx_reschedule;
	gs_interp_time_slice_proc = ctx_time_slice;
	gs_interp_time_slice_ticks = 100;
}

/* ------ Interpreter interface to scheduler ------ */

/* When an operator decides it is time to run a new context, */
/* it returns o_reschedule.  The interpreter saves all its state in */
/* memory, calls ctx_reschedule, and then loads the state from memory. */
private int
ctx_reschedule(void)
{	/* Save the state of the current context in ctx_current, */
	/* if any context is current at all. */
	if ( ctx_current != 0 )
	  context_store(gs_scheduler, ctx_current);
	/* If there are any dead contexts waiting to be released, */
	/* take care of that now. */
	while ( ctx_dead != 0 )
	  { gs_context *next = ctx_dead->next;

	    context_destroy(ctx_dead);
	    ctx_dead = next;
	  }
	/* Run the first ready context. */
	if ( ctx_active.head == 0 )
	  { lprintf("No context to run!");
	    return_error(e_Fatal);
	  }
	ctx_current = ctx_active.head;
	if ( !(ctx_active.head = ctx_active.head->next) )
	  ctx_active.tail = 0;
	/* Load the state of the new current context. */
	context_load(gs_scheduler, ctx_current);
	esfile_clear_cache();
	dict_set_top();		/* reload dict stack cache */
	return 0;
}

/* If the interpreter wants to time-slice, it saves its state, */
/* calls ctx_time_slice, and reloads its state. */
private int
ctx_time_slice(void)
{	if ( ctx_active.head == 0 )
	  return 0;
	add_last(&ctx_active, ctx_current);
	return ctx_reschedule();
}

/* ------ Context operators ------ */

private int fork_done(P1(os_ptr));

/* - currentcontext <context> */
private int
zcurrentcontext(register os_ptr op)
{	push(1);
	make_int(op, ctx_current->index);
	return 0;
}

/* <context> detach - */
private int
zdetach(register os_ptr op)
{	gs_context *pctx;
	int code;

	check_context(op, pctx);
	if ( pctx->joiner != 0 || pctx->detach )
	  return_error(e_invalidcontext);
	switch ( pctx->status )
	  {
	  case cs_active:
		pctx->detach = true;
		break;
	  case cs_done:
		context_destroy(pctx);
	  }
	pop(1);
	return 0;
}

/* <mark> <obj1> ... <objN> <proc> fork <context> */
private int
zfork(register os_ptr op)
{	uint mcount = ref_stack_counttomark(&o_stack);
	gs_context *pctx;
	int code;

	check_proc(*op);
	if ( mcount == 0 )
	  return_error(e_unmatchedmark);
	if ( gs_imemory.save_level )
	  return_error(e_invalidcontext);
	code = context_create(&pctx, iimemory_global);
	if ( code < 0 )
	  return code;
	/* Share local and global VM. */
	pctx->state.memory = gs_imemory;
	{ int i;
	  for ( i = 0; i < countof(gs_imemory.spaces.indexed); ++i )
	    if ( gs_imemory.spaces.indexed[i] != 0 )
	      gs_imemory.spaces.indexed[i]->num_contexts++;
	}
	/* Initialize the stacks. */
	{ ref_stack *dstack = r_ptr(&pctx->state.dstack, ref_stack);
	  uint count = ref_stack_count(&d_stack);

	  ref_stack_push(dstack, count - ref_stack_count(dstack));
	  stack_copy(dstack, &d_stack, count, 0);
	}
	{ ref_stack *estack = r_ptr(&pctx->state.estack, ref_stack);

	  ref_stack_push(estack, 3);
	  /* fork_done must be executed in both normal and error cases. */
	  make_mark_estack(estack->p - 2, es_other, fork_done);
	  make_oper(estack->p - 1, 0, fork_done);
	  *estack->p = *op;
	}
	{ ref_stack *ostack = r_ptr(&pctx->state.ostack, ref_stack);
	  uint count = mcount - 2;

	  ref_stack_push(ostack, count);
	  stack_copy(ostack, &o_stack, count, 1);
	}
	/****** COPY GSTATE STACK ******/
	pctx->state.binary_object_format = ref_binary_object_format;
	add_last(&ctx_active, pctx);
	pop(mcount - 1);
	op = osp;
	make_int(op, pctx->index);
	return 0;
}
/* This gets executed when a context terminates normally */
/* or when the stack is being unwound for an error termination. */
/****** MUST DO ALL RESTORES ******/
/****** WHAT IF invalidrestore? ******/
private int
fork_done(os_ptr op)
{	if ( ctx_current->detach )
	  { /*
	     * We can't free the context's memory yet, because the
	     * interpreter still has references to it.  Instead, queue the
	     * context to be freed the next time we reschedule.
	     */
	    context_store(gs_scheduler, ctx_current);
	    ctx_current->next = ctx_dead;
	    ctx_dead = ctx_current;
	    ctx_current = 0;
	  }
	else
	  { gs_context *pctx = ctx_current->joiner;

	    ctx_current->status = cs_done;
	    /* Schedule the context waiting to join this one, if any. */
	    if ( pctx != 0 )
	      add_last(&ctx_active, pctx);
	  }
	return o_reschedule;
}

/* <context> join <mark> <obj1> ... <objN> */
private int
zjoin(register os_ptr op)
{	gs_context *pctx;
	int code;

	check_context(op, pctx);
	if ( pctx->joiner != 0 || pctx->detach || pctx == ctx_current ||
	     pctx->state.memory.space_global !=
	       ctx_current->state.memory.space_global ||
	     pctx->state.memory.space_local !=
	       ctx_current->state.memory.space_local ||
	     gs_imemory.save_level != 0
	   )
	  return_error(e_invalidcontext);
	switch ( pctx->status )
	   {
	case cs_active:
		pctx->joiner = ctx_current;
		return o_reschedule;
	case cs_done:
	   {	const ref_stack *ostack =
		  r_ptr(&pctx->state.ostack, ref_stack);
		uint count = ref_stack_count(ostack);

		push(count);
		{ ref *rp = ref_stack_index(&o_stack, count);
		  make_mark(rp);
		}
		stack_copy(&o_stack, ostack, count, 0);
		context_destroy(pctx);
	   }
	   }
	return 0;
}

/* - yield - */
private int
zyield(register os_ptr op)
{	if ( ctx_active.head == 0 )
	  return 0;
	add_last(&ctx_active, ctx_current);
	return o_reschedule;
}

/* ------ Condition and lock operators ------ */

private int
  monitor_cleanup(P1(os_ptr)),
  monitor_release(P1(os_ptr)),
  await_lock(P1(os_ptr));

/* - condition <condition> */
private int
zcondition(register os_ptr op)
{	gs_condition *pcond =
	  ialloc_struct(gs_condition, &st_condition, "zcondition");

	if ( pcond == 0 )
	  return_error(e_VMerror);
	pcond->waiting.head = pcond->waiting.tail = 0;
	push(1);
	make_istruct(op, a_all, pcond);
	return 0;
}

/* - lock <lock> */
private int
zlock(register os_ptr op)
{	gs_lock *plock = ialloc_struct(gs_lock, &st_lock, "zlock");

	if ( plock == 0 )
	  return_error(e_VMerror);
	plock->holder = 0;
	plock->waiting.head = plock->waiting.tail = 0;
	push(1);
	make_istruct(op, a_all, plock);
	return 0;
}

/* <lock> <proc> monitor - */
private int
zmonitor(register os_ptr op)
{	gs_lock *plock;
	int code;

	check_stype(op[-1], st_lock);
	check_proc(*op);
	plock = r_ptr(op - 1, gs_lock);
	if ( plock->holder == ctx_current ||
	     (gs_imemory.save_level != 0 &&
	      plock->holder != 0 &&
	      plock->holder->state.memory.space_local ==
	        ctx_current->state.memory.space_local)
	   )
	  return_error(e_invalidcontext);
	/*
	 * We push on the e-stack:
	 *	The lock object
	 *	An e-stack mark with monitor_cleanup, to release the lock
	 *	  in case of an error
	 *	monitor_release, to release the lock in the normal case
	 *	The procedure to execute
	 */
	check_estack(4);
	code = lock_acquire(op - 1);
	if ( code != 0 )
	  { /* We didn't acquire the lock.  Re-execute this later. */
	    push_op_estack(zmonitor);
	    return code;	/* o_reschedule */
	  }
	*++esp = op[-1];
	push_mark_estack(es_other, monitor_cleanup);
	push_op_estack(monitor_release);
	*++esp = *op;
	pop(2);
	return code;
}
/* Release the monitor lock when unwinding for an error or exit. */
private int
monitor_cleanup(os_ptr op)
{	int code = lock_release(esp);
	--esp;
	return code;
}
/* Release the monitor lock when the procedure completes. */
private int
monitor_release(os_ptr op)
{	--esp;
	return monitor_cleanup(op);
}

/* <condition> notify - */
private int
znotify(register os_ptr op)
{	gs_condition *pcond;

	check_stype(*op, st_condition);
	pcond = r_ptr(op, gs_condition);
	pop(1); op--;
	if ( pcond->waiting.head == 0 )		/* nothing to do */
	  return 0;
	add_last_all(&ctx_active, &pcond->waiting);
	return zyield(op);
}

/* <lock> <condition> wait - */
private int
zwait(register os_ptr op)
{	gs_lock *plock;
	gs_condition *pcond;

	check_stype(op[-1], st_lock);
	plock = r_ptr(op - 1, gs_lock);
	check_stype(*op, st_condition);
	pcond = r_ptr(op, gs_condition);
	if ( plock->holder != ctx_current ||
	     (gs_imemory.save_level != 0 &&
	      (r_space(op - 1) == avm_local || r_space(op) == avm_local))
	   )
	  return_error(e_invalidcontext);
	check_estack(1);
	lock_release(op - 1);
	add_last(&pcond->waiting, ctx_current);
	push_op_estack(await_lock);
	return o_reschedule;
}
/* When the condition is signaled, wait for acquiring the lock. */
private int
await_lock(os_ptr op)
{	int code = lock_acquire(op - 1);

	if ( code == 0 )
	  { pop(2);
	    return code;
	  }
	/* We didn't acquire the lock.  Re-execute the wait. */
	push_op_estack(await_lock);
	return code;		/* o_reschedule */
}

/* ------ Miscellaneous operators ------ */

/* - usertime <int> */
private int
zusertime_context(register os_ptr op)
{	long utime = context_usertime();

	push(1);
	if ( !ctx_current->state.keep_usertime ) {
	  /*
	   * This is the first time this context has executed usertime:
	   * we must track its execution time from now on.
	   */
	  gs_scheduler->usertime_initial = utime;
	  ctx_current->state.keep_usertime = true;
	}
	make_int(op, ctx_current->state.usertime_total + utime -
		 gs_scheduler->usertime_initial);
	return 0;
}

/* ------ Internal routines ------ */

/* Create a context. */
private int
context_create(gs_context **ppctx, gs_ref_memory_t *mem)
{	gs_context *pctx;
	int code;
	long ctx_index;
	gs_context **pte;

	pctx = gs_alloc_struct((gs_memory_t *)mem, gs_context, &st_context,
			       "context_create");
	if ( pctx == 0 )
	  return_error(e_VMerror);
	code = context_state_alloc(&pctx->state, mem);
	if ( code < 0 )
	  { gs_free_object((gs_memory_t *)mem, pctx, "context_create");
	    return code;
	  }
	ctx_index = gs_next_ids(1);
	pctx->status = cs_active;
	pctx->index = ctx_index;
	pctx->detach = false;
	pctx->next = 0;
	pctx->joiner = 0;
	pte = &ctx_table[ctx_index % ctx_table_size];
	pctx->table_next = *pte;
	*pte = pctx;
	*ppctx = pctx;
	return 0;
}

/* Check a context ID.  Note that we do not check for context validity. */
private int
context_param(os_ptr op, gs_context **ppctx)
{	gs_context *pctx;
	long index;

	check_type(*op, t_integer);
	index = op->value.intval;
	if ( index < 0 )
	  return_error(e_invalidcontext);
	pctx = ctx_table[index % ctx_table_size];
	for ( ; ; pctx = pctx->table_next )
	  { if ( pctx == 0 )
	      return_error(e_invalidcontext);
	    if ( pctx->index == index )
	      break;
	  }
	*ppctx = pctx;
	return 0;
}

/* Read the usertime as a single value. */
private long
context_usertime(void)
{	long secs_ns[2];

	gp_get_usertime(secs_ns);
	return secs_ns[0] * 1000 + secs_ns[1] / 1000000;
}

/* Destroy a context. */
private void
context_destroy(gs_context *pctx)
{	gs_context **ppctx = &ctx_table[pctx->index % ctx_table_size];

	while ( *ppctx != pctx )
	  ppctx = &(*ppctx)->table_next;
	*ppctx = (*ppctx)->table_next;
	context_state_free(&pctx->state, iimemory);
	ifree_object(pctx, "context_destroy");
}

/* Copy the top elements of one stack to another. */
/* Note that this does not push the elements: */
/* the destination stack must have enough space preallocated. */
private void
stack_copy(ref_stack *to, const ref_stack *from, uint count, uint from_index)
{	long i;

	for ( i = (long)count - 1; i >= 0; --i )
	  *ref_stack_index(to, i) = *ref_stack_index(from, i + from_index);
}

/* Acquire a lock.  Return 0 if acquired, o_reschedule if not. */
private int
lock_acquire(os_ptr op)
{	gs_lock *plock = r_ptr(op, gs_lock);

	if ( plock->holder == 0 )
	  { plock->holder = ctx_current;
	    return 0;
	  }
	add_last(&plock->waiting, ctx_current);
	return o_reschedule;
}

/* Release a lock.  Return 0 if OK, e_invalidcontext if not. */
private int
lock_release(ref *op)
{	gs_lock *plock = r_ptr(op, gs_lock);

	if ( plock->holder == ctx_current )
	  { plock->holder = 0;
	    add_last_all(&ctx_active, &plock->waiting);
	    return 0;
	  }
	return_error(e_invalidcontext);
}

/* ------ Initialization procedure ------ */

BEGIN_OP_DEFS(zcontext_op_defs) {
	{"0condition", zcondition},
	{"0currentcontext", zcurrentcontext},
	{"1detach", zdetach},
	{"2fork", zfork},
	{"1join", zjoin},
	{"0lock", zlock},
	{"2monitor", zmonitor},
	{"1notify", znotify},
	{"2wait", zwait},
	{"0yield", zyield},
		/* Note that the following replace prior definitions */
		/* in the indicated files: */
	{"0usertime", zusertime_context},	/* zmisc.c */
		/* Internal operators */
	{"0%fork_done", fork_done},
	{"0%monitor_cleanup", monitor_cleanup},
	{"0%monitor_release", monitor_release},
	{"2%await_lock", await_lock},
END_OP_DEFS(zcontext_init) }
