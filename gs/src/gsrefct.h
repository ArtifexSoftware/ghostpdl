/* Copyright (C) 1993, 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* gsrefct.h */
/* Reference counting definitions */

#ifndef gsrefct_INCLUDED
#  define gsrefct_INCLUDED

/*
 * In many places below, a do {...} while (0) avoids problems with a possible
 * enclosing 'if'.
 */

/*
 * A reference-counted object must include the following header:
 *	rc_header rc;
 * The header need not be the first element of the object.
 */
typedef struct rc_header_s rc_header;
struct rc_header_s {
	long ref_count;
	gs_memory_t *memory;
#define rc_free_proc(proc)\
  void proc(P3(gs_memory_t *, void *, client_name_t))
	rc_free_proc((*free));
};

/* ------ Allocate/free ------ */

rc_free_proc(rc_free_struct_only);
#define rc_alloc_struct_n(vp, typ, pstyp, mem, errstat, cname, rcinit)\
  do\
   {	if ( ((vp) = gs_alloc_struct(mem, typ, pstyp, cname)) == 0 ) {\
	  errstat;\
	} else {\
	  (vp)->rc.ref_count = rcinit;\
	  (vp)->rc.memory = mem;\
	  (vp)->rc.free = rc_free_struct_only;\
	}\
   }\
  while (0)
#define rc_alloc_struct_0(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 0)
#define rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname)\
  rc_alloc_struct_n(vp, typ, pstype, mem, errstat, cname, 1)

#define rc_free_struct(vp, cname)\
  (*(vp)->rc.free)((vp)->rc.memory, (void *)(vp), cname)

/* ------ Reference counting ------ */

/* Increment a reference count. */
#define rc_increment(vp)\
  do { if ( (vp) != 0 ) (vp)->rc.ref_count++; } while (0)

/* Increment a reference count, allocating the structure if necessary. */
#define rc_allocate_struct(vp, typ, pstype, mem, errstat, cname)\
  do\
   { if ( (vp) != 0 )\
       (vp)->rc.ref_count++;\
     else\
       rc_alloc_struct_1(vp, typ, pstype, mem, errstat, cname);\
   }\
  while (0)

/* Guarantee that a structure is allocated and is not shared. */
#define rc_unshare_struct(vp, typ, pstype, mem, errstat, cname)\
  do\
   { if ( (vp) == 0 || (vp)->rc.ref_count > 1 || (vp)->rc.memory != (mem) )\
      {	typ *new;\
	rc_alloc_struct_1(new, typ, pstype, mem, errstat, cname);\
	if ( vp ) (vp)->rc.ref_count--;\
	(vp) = new;\
      }\
   }\
  while (0)

/* Adjust a reference count either up or down. */
#define rc_adjust_(vp, delta, cname, body)\
  do\
   { if ( (vp) != 0 && !((vp)->rc.ref_count += delta) )\
      {	rc_free_struct(vp, cname);\
	body;\
      }\
   }\
  while (0)
#define rc_adjust(vp, delta, cname)\
  rc_adjust_(vp, delta, cname, (vp) = 0)
#define rc_adjust_only(vp, delta, cname)\
  rc_adjust_(vp, delta, cname, DO_NOTHING)
#define rc_adjust_const(vp, delta, cname)\
  rc_adjust_only(vp, delta, cname)
#define rc_decrement(vp, cname)\
  rc_adjust(vp, -1, cname)
#define rc_decrement_only(vp, cname)\
  rc_adjust_only(vp, -1, cname)

/* Assign a pointer, adjusting reference counts. */
#define rc_assign(vpto, vpfrom, cname)\
  do\
   { if ( (vpto) != (vpfrom) )\
      {	rc_decrement_only(vpto, cname);\
	(vpto) = (vpfrom);\
	rc_increment(vpto);\
      }\
   }\
  while (0)
/* Adjust reference counts for assigning a pointer, */
/* but don't do the assignment.  We use this before assigning */
/* an entire structure containing reference-counted pointers. */
#define rc_pre_assign(vpto, vpfrom, cname)\
  do\
   { if ( (vpto) != (vpfrom) )\
      {	rc_decrement_only(vpto, cname);\
	rc_increment(vpfrom);\
      }\
   }\
  while (0)

#endif					/* gsrefct_INCLUDED */
