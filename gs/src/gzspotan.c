/* Copyright (C) 2003 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/*$Id$ */
/* A spot analyzer device implementation. */
#include "gx.h"
#include "gserrors.h"
#include "gsdevice.h"
#include "gzspotan.h"
#include "gxfixed.h"
#include "gxdevice.h"
#include "memory_.h"
#include "vdtrace.h"
#include <assert.h>

#define VD_TRAP_N_COLOR RGB(128, 128, 0)
#define VD_TRAP_U_COLOR RGB(0, 0, 255)
#define VD_CONT_COLOR RGB(0, 255, 0)

public_st_device_spot_analyzer();
private_st_san_trap();
private_st_san_trap_contact();

private dev_proc_open_device(san_open);
private dev_proc_close_device(san_close);
private dev_proc_get_clipping_box(san_get_clipping_box);

/* The device descriptor */
/* Many of these procedures won't be called; they are set to NULL. */
private const gx_device_spot_analyzer gx_spot_analyzer_device =
{std_device_std_body(gx_device_spot_analyzer, 0, "spot analyzer",
		     0, 0, 1, 1),
 {san_open,
  NULL,
  NULL,
  NULL,
  san_close,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  gx_default_fill_path,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  san_get_clipping_box,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  gx_default_finish_copydevice,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL
 }
};

private int
san_open(register gx_device * dev)
{
    gx_device_spot_analyzer * const padev = (gx_device_spot_analyzer *)dev;

    padev->trap_buffer = 0;
    padev->cont_buffer = 0;
    padev->trap_buffer_count = padev->trap_buffer_max = 0;
    padev->cont_buffer_count = padev->cont_buffer_max = 0;
    return 0;
}

private int
san_close(gx_device * dev)
{
    gx_device_spot_analyzer * const padev = (gx_device_spot_analyzer *)dev;

    gs_free_object(padev->memory, padev->trap_buffer, "san_close");
    padev->trap_buffer = 0;
    gs_free_object(padev->memory, padev->cont_buffer, "san_close");
    padev->cont_buffer = 0;
    return 0;
}

void
san_get_clipping_box(gx_device * dev, gs_fixed_rect * pbox)
{
    pbox->p.x = min_int;
    pbox->p.y = min_int;
    pbox->q.x = max_int;
    pbox->q.y = max_int;
}

/* --------------------- Utilities ------------------------- */
/* fixme : use something like C++ patterns to generate same functions for various types. */

private const int buf_increment = 100;

private inline gx_san_trap *
trap_reserve(gx_device_spot_analyzer *padev)
{
    if (padev->trap_buffer_count == padev->trap_buffer_max) {
	gx_san_trap *buf = gs_alloc_struct_array(padev->memory, 
		padev->trap_buffer_max + buf_increment, gx_san_trap, 
		&st_san_trap, "trap_reserve");
	if (buf == NULL)
	    return NULL;
	if (padev->trap_buffer != NULL) {
	    memcpy(buf, padev->trap_buffer, sizeof(buf[0]) * padev->trap_buffer_max);
	    gs_free_object(padev->memory, padev->trap_buffer, "trap_reserve");
	}
	padev->trap_buffer = buf;
	padev->trap_buffer_max += buf_increment;
    }
    return &padev->trap_buffer[padev->trap_buffer_count++];
}

private inline gx_san_trap_contact *
cont_reserve(gx_device_spot_analyzer *padev)
{
    if (padev->cont_buffer_count == padev->cont_buffer_max) {
	gx_san_trap_contact *buf = gs_alloc_struct_array(padev->memory, 
		padev->cont_buffer_max + buf_increment, gx_san_trap_contact, 
		&st_san_trap_contact, "cont_reserve");
	if (buf == NULL)
	    return NULL;
	if (padev->cont_buffer != NULL) {
	    memcpy(buf, padev->cont_buffer, sizeof(buf[0]) * padev->cont_buffer_max);
	    gs_free_object(padev->memory, padev->cont_buffer, "cont_reserve");
	}
	padev->cont_buffer = buf;
	padev->cont_buffer_max += buf_increment;
    }
    return &padev->cont_buffer[padev->cont_buffer_count++];
}

private inline void
trap_unreserve(gx_device_spot_analyzer *padev, gx_san_trap *t)
{
    assert(t == &padev->trap_buffer[padev->trap_buffer_count - 1]);
    padev->trap_buffer_count--;
}

private inline void
cont_unreserve(gx_device_spot_analyzer *padev, gx_san_trap_contact *t)
{
    assert(t == &padev->cont_buffer[padev->cont_buffer_count - 1]);
    padev->cont_buffer_count--;
}

private inline gx_san_trap *
band_list_last(const gx_san_trap *list)
{
    /* Assuming a non-empty cyclic list, and the anchor points to the first element.  */
    return list->prev;
}

private inline gx_san_trap_contact *
cont_list_last(const gx_san_trap_contact *list)
{
    /* Assuming a non-empty cyclic list, and the anchor points to the first element.  */
    return list->prev;
}

private inline void
band_list_remove(gx_san_trap **list, gx_san_trap *t)
{
    /* Assuming a cyclic list, and the element is in it. */
    if (t->next == t) {
	*list = NULL;
    } else {
	if (*list == t)
	    *list = t->next;
	t->next->prev = t->prev;
	t->prev->next = t->next;
    }
    t->next = t->prev = NULL; /* Safety. */
}

private inline void
band_list_insert_last(gx_san_trap **list, gx_san_trap *t)
{
    /* Assuming a cyclic list. */
    if (*list == 0) {
	*list = t->next = t->prev = t;
    } else {
	gx_san_trap *last = band_list_last(*list);
	gx_san_trap *first = *list;

	t->next = first;
	t->prev = last;
	last->next = first->prev = t;
    }
}

private inline void
cont_list_insert_last(gx_san_trap_contact **list, gx_san_trap_contact *t)
{
    /* Assuming a cyclic list. */
    if (*list == 0) {
	*list = t->next = t->prev = t;
    } else {
	gx_san_trap_contact *last = cont_list_last(*list);
	gx_san_trap_contact *first = *list;

	t->next = first;
	t->prev = last;
	last->next = first->prev = t;
    }
}

private inline bool
trap_is_last(const gx_san_trap *list, const gx_san_trap *t)
{
    /* Assuming a non-empty cyclic list, and the anchor points to the first element.  */
    return t->next == list; 
}

private inline void
check_band_list(const gx_san_trap *list)
{
#if DEBUG
    if (list != NULL) {
	const gx_san_trap *t = list;

	while (t->next != list) {
	    assert(t->xrtop <= t->next->xltop);
	    t = t->next;
	}
    }
#endif
}

private void
try_unite_last_trap(gx_device_spot_analyzer *padev, fixed xlbot)
{
    if (padev->bot_band != NULL && padev->top_band != NULL) {
	gx_san_trap *last = band_list_last(padev->top_band);
	/* If the last trapeziod is a prolongation of its bottom contact, 
	   unite it and release the last trapezoid and the last contact. */
	if (last->lower != NULL && last->xrbot < xlbot && 
		(last->prev == last || last->prev->xrbot < last->xlbot)) {
	    gx_san_trap *t = last->lower->lower;

	    if (last->lower->next == last->lower &&
		    t->l == last->l && t->r == last->r) {
		if (padev->bot_current == t)
		    padev->bot_current = (t == band_list_last(padev->bot_band) ? NULL : t->next);
		band_list_remove(&padev->top_band, last);
		band_list_remove(&padev->bot_band, t);
		check_band_list(padev->bot_band);
		band_list_insert_last(&padev->top_band, t);
		t->ytop = last->ytop;
		t->xltop = last->xltop;
		t->xrtop = last->xrtop;
		vd_quad(t->xlbot, t->ybot, t->xrbot, t->ybot, 
			t->xrtop, t->ytop, t->xltop, t->ytop, 1, VD_TRAP_U_COLOR);
		trap_unreserve(padev, last);
		cont_unreserve(padev, last->lower);
		check_band_list(padev->top_band);
		check_band_list(padev->bot_band);
	    }
	}
    }
}

/* --------------------- Accessories ------------------------- */

/* Obtain a spot analyzer device. */
int
gx_san__obtain(gs_memory_t *mem, gx_device_spot_analyzer **ppadev)
{
    gx_device_spot_analyzer *padev;
    int code;

    if (*ppadev != 0) {
	(*ppadev)->lock++;
	return 0;
    }
    padev = gs_alloc_struct(mem, gx_device_spot_analyzer, 
		&st_device_spot_analyzer, "gx_san__obtain");
    if (padev == 0)
	return_error(gs_error_VMerror);
    gx_device_init((gx_device *)padev, (const gx_device *)&gx_spot_analyzer_device,
		   mem, false);
    code = gs_opendevice((gx_device *)padev);
    if (code < 0) {
	gs_free_object(mem, padev, "gx_san__obtain");
	return code;
    }
    padev->lock = 1;
    *ppadev = padev;
    return 0;
}

void
gx_san__release(gx_device_spot_analyzer **ppadev)
{
    gx_device_spot_analyzer *padev = *ppadev;

    if (padev == NULL) {
	eprintf("Extra call to gx_san__release.");
	return;
    }
    if(--padev->lock < 0) {
	eprintf("Wrong lock to gx_san__release.");
	return;
    }
    if (padev->lock == 0) {
	*ppadev = NULL;
	rc_decrement(padev, "gx_san__release");
    }
}

/* Start accumulating a path. */
void 
gx_san_begin(gx_device_spot_analyzer *padev)
{
    padev->bot_band = NULL;
    padev->top_band = NULL;
    padev->bot_current = NULL;
    padev->trap_buffer_count = 0;
    padev->cont_buffer_count = 0;
}

/* Store a tarpezoid. */
/* Assumes an Y-band scanning order with increasing X inside a band. */
int
gx_san_trap_store(gx_device_spot_analyzer *padev, 
    fixed ybot, fixed ytop, fixed xlbot, fixed xrbot, fixed xltop, fixed xrtop,
    const segment *r, const segment *l)
{
    gx_san_trap *last;

    if (padev->top_band != NULL && padev->top_band->ytop != ytop) {
	try_unite_last_trap(padev, max_int);
	/* Step to a new band. */
	padev->bot_band = padev->bot_current = padev->top_band;
	padev->top_band = NULL;
    }
    if (padev->bot_band != NULL && padev->bot_band->ytop != ybot) {
	/* The Y-projection of the spot is not contiguous. */
	padev->top_band = NULL;
    }
    if (padev->top_band != NULL)
	try_unite_last_trap(padev, xlbot);
    check_band_list(padev->bot_band);
    check_band_list(padev->top_band);
    /* Make new trapeziod. */
    last = trap_reserve(padev);
    if (last == NULL)
	return_error(gs_error_VMerror);
    last->ybot = ybot;
    last->ytop = ytop;
    last->xlbot = xlbot;
    last->xrbot = xrbot;
    last->xltop = xltop;
    last->xrtop = xrtop;
    last->l = l;
    last->r = r;
    last->lower = 0;
    vd_quad(last->xlbot, last->ybot, last->xrbot, last->ybot, 
	    last->xrtop, last->ytop, last->xltop, last->ytop, 1, VD_TRAP_N_COLOR);
    band_list_insert_last(&padev->top_band, last);
    check_band_list(padev->top_band);
    while (padev->bot_current != NULL && padev->bot_current->xrtop < xlbot)
	padev->bot_current = (trap_is_last(padev->bot_band, padev->bot_current)
				    ? NULL : padev->bot_current->next);
    if (padev->bot_current != 0) {
	gx_san_trap *t = padev->bot_current;
	gx_san_trap *bot_last = band_list_last(padev->bot_band);

	while(t->xltop <= xrbot) {
	    gx_san_trap_contact *cont = cont_reserve(padev);

	    if (cont == NULL)
		return_error(gs_error_VMerror);
	    cont->lower = t;
	    cont->upper = last;
	    vd_bar((t->xltop + t->xrtop + t->xlbot + t->xrbot) / 4, (t->ytop + t->ybot) / 2,
		   (last->xltop + last->xrtop + last->xlbot + last->xrbot) / 4, 
		   (last->ytop + last->ybot) / 2, 0, VD_CONT_COLOR);
	    cont_list_insert_last(&last->lower, cont);
	    if (t == bot_last)
		break;
	    t = t->next;
	}
    }
    return 0;
}


/* Finish accumulating a path. */
void 
gx_san_end(const gx_device_spot_analyzer *padev)
{
}

/* Generate stems. */
int 
gx_san_generate_stems(gx_device_spot_analyzer *padev, 
		void *client_data,
		int (*handler)(void *client_data, gx_san_sect *ss))
{
    return 0;
}
