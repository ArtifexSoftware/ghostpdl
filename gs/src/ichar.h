/* Copyright (C) 1994, 1996, 1997 Aladdin Enterprises.  All rights reserved.
  
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

/* ichar.h */
/* Shared definitions for text operators */
/* Requires gxchar.h */

/*
 * All the character rendering operators use the execution stack
 * for loop control -- see estack.h for details.
 * The information pushed by these operators is as follows:
 *	the enumerator (t_struct, a gs_show_enum);
 *	a slot for the procedure for kshow or cshow (probably t_array) or
 *		the string or array for [x][y]show (t_string or t_array);
 *	a slot for the string/array index for [x][y]show (t_integer);
 *	a slot for the saved o-stack depth for cshow or stringwidth,
 *		and for error recovery (t_integer);
 *	a slot for the saved d-stack depth ditto (t_integer);
 *	a slot for the saved gstate level ditto (t_integer);
 *	the procedure to be called at the end of the enumeration
 *		(t_operator, but called directly, not by the interpreter);
 *	the usual e-stack mark (t_null).
 */
#define snumpush 8
#define esenum(ep) r_ptr(ep, gs_show_enum)
#define senum esenum(esp)
#define esslot(ep) ((ep)[-1])
#define sslot esslot(esp)
#define essindex(ep) ((ep)[-2])
#define ssindex essindex(esp)
#define esodepth(ep) ((ep)[-3])
#define sodepth esodepth(esp)
#define esddepth(ep) ((ep)[-4])
#define sddepth esddepth(esp)
#define esgslevel(ep) ((ep)[-5])
#define sgslevel esgslevel(esp)
#define eseproc(ep) ((ep)[-6])
#define seproc eseproc(esp)

/* Procedures exported by zchar.c for zchar1.c, zchar2.c, and/or zcharcid.c. */
gs_show_enum *op_show_find(P0());
int op_show_setup(P2(os_ptr, gs_show_enum **));
int op_show_enum_setup(P2(os_ptr, gs_show_enum **));
void op_show_finish_setup(P3(gs_show_enum *, int, op_proc_p));
int op_show_continue(P1(os_ptr));
int op_show_continue_dispatch(P2(os_ptr, int));
int op_show_free(P1(int));
