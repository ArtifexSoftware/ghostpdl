/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Miscellaneous LanguageLevel 3 operators */
#include "ghost.h"
#include "gscspace.h"		/* for gscolor2.h */
#include "gsmatrix.h"		/* ditto */
#include "gsclipsr.h"
#include "gscolor2.h"
#include "gscssub.h"
#include "oper.h"
#include "igstate.h"
#include "store.h"

/* - clipsave - */
private int
zclipsave(i_ctx_t *i_ctx_p)
{
    return gs_clipsave(igs);
}

/* - cliprestore - */
private int
zcliprestore(i_ctx_t *i_ctx_p)
{
    return gs_cliprestore(igs);
}

/* <proc1> <proc2> .eqproc <bool> */
/*
 * Test whether two procedures are equal to depth 10.
 * This is the equality test used by idiom recognition in 'bind'.
 */
#define MAX_DEPTH 10		/* depth is per Adobe specification */
typedef struct ref2_s {
    ref proc1, proc2;
} ref2_t;
private int
zeqproc(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    ref2_t stack[MAX_DEPTH + 1];
    ref2_t *top = stack;

    make_array(&stack[0].proc1, 0, 1, op - 1);
    make_array(&stack[0].proc2, 0, 1, op);
    for (;;) {
	long i;

	if (r_size(&top->proc1) == 0) {
	    /* Finished these arrays, go up to next level. */
	    if (top == stack) {
		/* We're done matching: it succeeded. */
		make_true(op - 1);
		pop(1);
		return 0;
	    }
	    --top;
	    continue;
	}
	/* Look at the next elements of the arrays. */
	i = r_size(&top->proc1) - 1;
	array_get(&top->proc1, i, &top[1].proc1);
	array_get(&top->proc2, i, &top[1].proc2);
	r_dec_size(&top->proc1, 1);
	++top;
	/*
	 * Amazingly enough, the objects' executable attributes are not
	 * required to match.  This means { x load } will match { /x load },
	 * even though this is clearly wrong.
	 */
#if 0
	if (r_has_attr(&top->proc1, a_executable) !=
	    r_has_attr(&top->proc2, a_executable)
	    )
	    break;
#endif
	if (obj_eq(&top->proc1, &top->proc2)) {
	    /* Names don't match strings. */
	    if (r_type(&top->proc1) != r_type(&top->proc2) &&
		(r_type(&top->proc1) == t_name ||
		 r_type(&top->proc2) == t_name)
		)
		break;
	    --top;		/* no recursion */
	    continue;
	}
	if (r_is_array(&top->proc1) && r_is_array(&top->proc2) &&
	    r_size(&top->proc1) == r_size(&top->proc2) &&
	    top < stack + (MAX_DEPTH - 1)
	    ) {
	    /* Descend into the arrays. */
	    continue;
	}
	break;
    }
    /* An exit from the loop indicates that matching failed. */
    make_false(op - 1);
    pop(1);
    return 0;
}

/* <index> <bool> .setsubstitutecolorspace - */
private int
zsetsubstitutecolorspace(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    int index, code;

    check_type(*op, t_boolean);
    check_int_leu(op[-1], 2);
    index = (int)op[-1].value.intval;
    code = gs_setsubstitutecolorspace(igs, index,
				      (op->value.boolval ?
				       gs_currentcolorspace(igs) :
				       NULL));
    if (code >= 0)
	pop(2);
    return code;
}

/* ------ Initialization procedure ------ */

const op_def zmisc3_op_defs[] =
{
    op_def_begin_ll3(),
    {"0cliprestore", zcliprestore},
    {"0clipsave", zclipsave},
    {"2.eqproc", zeqproc},
    {"2.setsubstitutecolorspace", zsetsubstitutecolorspace},
    op_def_end(0)
};
