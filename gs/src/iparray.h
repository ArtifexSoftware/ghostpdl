/* Copyright (C) 1994, 1999 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Packed array constructor for Ghostscript */
/* Requires ipacked.h, istack.h */

#ifndef iparray_INCLUDED
#  define iparray_INCLUDED

/*
 * The only reason to put this in a separate header is that it requires
 * both ipacked.h and istack.h; putting it in either one would make it
 * depend on the other one.  There must be a better way....
 */

/* Procedures implemented in zpacked.c */

/* Make a packed array from the top N elements of a stack. */
int make_packed_array(P5(ref *, ref_stack_t *, uint, gs_dual_memory_t *,
			 client_name_t));

#endif /* iparray_INCLUDED */
