/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.

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


/* Page queue implementation */

/* Initial version 2/1/98 by John Desrosiers (soho@crl.com) */
/* 7/17/98 L. Peter Deutsch (ghost@aladdin.com) edited to conform to
   Ghostscript coding standards */
/* 8/7/98 ghost@aladdin.com fixed bugs in #define st_... statements */
/* 11/3/98 ghost@aladdin.com further updates for coding standards,
   moves definition of page queue structure to gxpageq.c */

#ifndef gxpageq_INCLUDED
# define gxpageq_INCLUDED

#include "gsmemory.h"
#include "gxband.h"
#include "gxsync.h"

/* --------------- Data type definitions --------------------- */

typedef enum {
    GX_PAGE_QUEUE_ACTION_PARTIAL_PAGE,
    GX_PAGE_QUEUE_ACTION_FULL_PAGE,
    GX_PAGE_QUEUE_ACTION_COPY_PAGE,
    GX_PAGE_QUEUE_ACTION_TERMINATE
} gx_page_queue_action_t;

/*
 * Define the structure used to manage a page queue.
 * A page queue is a monitor-locked FIFO which holds completed command
 * list files ready for rendering.
 */
#ifndef gx_page_queue_DEFINED
# define gx_page_queue_DEFINED
typedef struct gx_page_queue_s gx_page_queue_t;
#endif

/*
 * Define a page queue entry object.
 */
typedef struct gx_page_queue_entry_s gx_page_queue_entry_t;
struct gx_page_queue_entry_s {
    gx_band_page_info_t page_info;
    gx_page_queue_action_t action;	/* action code */
    int num_copies;		/* number of copies to render */
    gx_page_queue_entry_t *next;		/* link to next in queue */
    gx_page_queue_t *queue;	/* link to queue the entry is in */
};

#define private_st_gx_page_queue_entry()\
  gs_private_st_ptrs2(st_gx_page_queue_entry, gx_page_queue_entry_t,\
    "gx_page_queue_entry",\
    gx_page_queue_entry_enum_ptrs, gx_page_queue_entry_reloc_ptrs,\
    next, queue)

/* -------------- Public Procedure Declaraions --------------------- */

/* Allocate a page queue. */
gx_page_queue_t *gx_page_queue_alloc(P1(gs_memory_t *mem));

/*
 * Allocate and initialize a page queue entry.
 * All page queue entries must be allocated by this routine.
 */
/* rets ptr to allocated object, 0 if VM error */
gx_page_queue_entry_t *
gx_page_queue_entry_alloc(P1(
    gx_page_queue_t * queue	/* queue that entry is being alloc'd for */
    ));

/*
 * Free a page queue entry.
 * All page queue entries must be destroyed by this routine.
 */
void gx_page_queue_entry_free(P1(
    gx_page_queue_entry_t * entry	/* entry to free up */
    ));

/*
 * Free the page_info resources held by the pageq entry.  Used to free
 * pages' clist, typically after rendering.  Note that this routine is NOT
 * called implicitly by gx_page_queue_entry_free, since page clist may be
 * managed separately from page queue entries.  However, unless you are
 * managing clist separately, you must call this routine before freeing the
 * pageq entry itself (via gx_page_queue_entry_free), or you will leak
 * memory (lots).
 */
void gx_page_queue_entry_free_page_info(P1(
    gx_page_queue_entry_t * entry	/* entry to free up */
    ));

/*
 * Initialize a page queue; this must be done before it can be used.
 * This routine allocates & inits various necessary structures and will
 * fail if insufficient memory is available.
 */
/* -ve error code, or 0 */
int gx_page_queue_init(P2(
    gx_page_queue_t * queue,	/* page queue to init */
    gs_memory_t * memory	/* allocator for dynamic memory */
    ));

/*
 * Destroy a page queue which was initialized by gx_page_queue_init.
 * Any page queue entries in the queue are released and destroyed;
 * dynamic allocations are released.
 */
void gx_page_queue_dnit(P1(
    gx_page_queue_t * queue	/* page queue to dnit */
    ));

/*
 * If there are any pages in queue, wait until one of them finishes
 * rendering.  Typically called by writer's out-of-memory error handlers
 * that want to wait until some memory has been freed.
 */
/* rets 0 if no pages were waiting for rendering, 1 if actually waited */
int gx_page_queue_wait_one_page(P1(
    gx_page_queue_t * queue	/* queue to wait on */
    ));

/*
 * Wait until all (if any) pages in queue have finished rendering. Typically
 * called by writer operations which need to drain the page queue before
 * continuing.
 */
void gx_page_queue_wait_until_empty(P1(
    gx_page_queue_t * queue		/* page queue to wait on */
    ));

/*
 * Add a pageq queue entry to the end of the page queue. If an unsatisfied
 * reader thread has an outstanding gx_page_queue_start_deque(), wake it up.
 */
void gx_page_queue_enqueue(P1(
    gx_page_queue_entry_t * entry	/* entry to add */
    ));

/*
 * Allocate & construct a pageq entry, then add to the end of the pageq as
 * in gx_page_queue_enqueue. If unable to allocate a new pageq entry, uses
 * the pre-allocated reserve entry held in the pageq. When using the reserve
 * pageq entry, wait until enough pages have been rendered to allocate a new
 * reserve for next time -- this should always succeed & returns eFatal if not.
 * Unless the reserve was used, does not wait for any rendering to complete.
 * Typically called by writer when it has a (partial) page ready for rendering.
 */
/* rets 0 ok, gs_error_Fatal if error */
int gx_page_queue_add_page(P4(
    gx_page_queue_t * queue,		/* page queue to add to */
    gx_page_queue_action_t action,		/* action code to queue */
    const gx_band_page_info_t * page_info,	/* bandinfo incl. bandlist */
    int page_count		/* # of copies to print if final "print,"
				   /* 0 if partial page, -1 if cancel */
    ));

/*
 * Retrieve the least-recently added queue entry from the pageq. If no
 * entry is available, waits on a signal from gx_page_queue_enqueue. Must
 * eventually be followed by a call to gx_page_queue_finish_dequeue for the
 * same pageq entry.
 * Even though the pageq is actually removed from the pageq, a mark is made in
 * the pageq to indicate that the pageq is not "empty" until the
 * gx_page_queue_finish_dequeue; this is for the benefit of
 * gx_page_queue_wait_???, since the completing the current page's rendering
 * may free more memory.
 * Typically called by renderer thread loop, which looks like:
    do {
	gx_page_queue_start_deqeueue(...);
	render_retrieved_entry(...);
	gx_page_queue_finish_dequeue(...);
    } while (some condition);
 */
gx_page_queue_entry_t *		/* removed entry */
gx_page_queue_start_dequeue(P1(
    gx_page_queue_t * queue	/* page queue to retrieve from */
    ));

/*
 * Free the pageq entry and its associated band list data, then signal any
 * waiting threads.  Typically used to indicate completion of rendering the
 * pageq entry.  Note that this is different from gx_page_queue_entry_free,
 * which does not free the band list data (a separate call of
 * gx_page_queue_entry_free_page_info is required).
 */
void gx_page_queue_finish_dequeue(P1(
    gx_page_queue_entry_t * entry  /* entry that was retrieved to delete */
    ));

#endif /*!defined(gxpageq_INCLUDED) */
