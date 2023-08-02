/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* String operators */
#include "memory_.h"
#include "ghost.h"
#include "gsutil.h"
#include "ialloc.h"
#include "iname.h"
#include "ivmspace.h"
#include "oper.h"
#include "store.h"

/* The generic operators (copy, get, put, getinterval, putinterval, */
/* length, and forall) are implemented in zgeneric.c. */

/* <int> .bytestring <bytestring> */
static int
zbytestring(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    byte *sbody;
    uint size;

    check_op(1);
    check_int_leu(*op, max_int);
    size = (uint)op->value.intval;
    sbody = ialloc_bytes(size, ".bytestring");
    if (sbody == 0)
        return_error(gs_error_VMerror);
    make_astruct(op, a_all | icurrent_space, sbody);
    memset(sbody, 0, size);
    return 0;
}

/* <int> string <string> */
int
zstring(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    byte *sbody;
    uint size;

    check_op(1);
    check_type(*op, t_integer);
    if (op->value.intval < 0 )
        return_error(gs_error_rangecheck);
    if (op->value.intval > max_string_size )
        return_error(gs_error_limitcheck); /* to match Distiller */
    size = op->value.intval;
    sbody = ialloc_string(size, "string");
    if (sbody == 0)
        return_error(gs_error_VMerror);
    make_string(op, a_all | icurrent_space, size, sbody);
    memset(sbody, 0, size);
    return 0;
}

/* <name> .namestring <string> */
static int
znamestring(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    check_op(1);
    check_type(*op, t_name);
    name_string_ref(imemory, op, op);
    return 0;
}

/* <string> <pattern> anchorsearch <post> <match> -true- */
/* <string> <pattern> anchorsearch <string> -false- */
static int
zanchorsearch(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    uint size = r_size(op);

    check_op(2);
    check_read_type(*op, t_string);
    check_read_type(*op1, t_string);
    if (size <= r_size(op1) && !memcmp(op1->value.bytes, op->value.bytes, size)) {
        os_ptr op0 = op;

        push(1);
        *op0 = *op1;
        r_set_size(op0, size);
        op1->value.bytes += size;
        r_dec_size(op1, size);
        make_true(op);
    } else
        make_false(op);
    return 0;
}

/* <string> <pattern> (r)search <post> <match> <pre> -true- */
/* <string> <pattern> (r)search <string> -false- */
static int
search_impl(i_ctx_t *i_ctx_p, bool forward)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    uint size = r_size(op);
    uint count;
    byte *pat;
    byte *ptr;
    byte ch;
    int incr = forward ? 1 : -1;

    check_op(2);
    check_read_type(*op1, t_string);
    check_read_type(*op, t_string);
    if (size > r_size(op1)) {	/* can't match */
        make_false(op);
        return 0;
    }
    count = r_size(op1) - size;
    ptr = op1->value.bytes;
    if (size == 0)
        goto found;
    if (!forward)
        ptr += count;
    pat = op->value.bytes;
    ch = pat[0];
    do {
        if (*ptr == ch && (size == 1 || !memcmp(ptr, pat, size)))
            goto found;
        ptr += incr;
    }
    while (count--);
    /* No match */
    make_false(op);
    return 0;
found:
    op->tas.type_attrs = op1->tas.type_attrs;
    op->value.bytes = ptr;				/* match */
    op->tas.rsize = size;				/* match */
    push(2);
    op[-1] = *op1;					/* pre */
    op[-3].value.bytes = ptr + size;			/* post */
    if (forward) {
        op[-1].tas.rsize = ptr - op[-1].value.bytes;	/* pre */
        op[-3].tas.rsize = count;			/* post */
    } else {
        op[-1].tas.rsize = count;			/* pre */
        op[-3].tas.rsize -= count + size;		/* post */
    }
    make_true(op);
    return 0;
}

/* Search from the start of the string */
static int
zsearch(i_ctx_t *i_ctx_p)
{
    return search_impl(i_ctx_p, true);
}

/* Search from the end of the string */
static int
zrsearch(i_ctx_t *i_ctx_p)
{
    return search_impl(i_ctx_p, false);
}

/* <string> <charstring> .stringbreak <int|null> */
static int
zstringbreak(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    uint i, j;

    check_op(2);
    check_read_type(op[-1], t_string);
    check_read_type(*op, t_string);
    /* We can't use strpbrk here, because C doesn't allow nulls in strings. */
    for (i = 0; i < r_size(op - 1); ++i)
        for (j = 0; j < r_size(op); ++j)
            if (op[-1].value.const_bytes[i] == op->value.const_bytes[j]) {
                make_int(op - 1, i);
                goto done;
            }
    make_null(op - 1);
 done:
    pop(1);
    return 0;
}

/* <obj> <pattern> .stringmatch <bool> */
static int
zstringmatch(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    bool result;

    check_op(2);
    check_read_type(*op, t_string);
    switch (r_type(op1)) {
        case t_string:
            check_read(*op1);
            goto cmp;
        case t_name:
            name_string_ref(imemory, op1, op1);	/* can't fail */
cmp:
            result = string_match(op1->value.const_bytes, r_size(op1),
                                  op->value.const_bytes, r_size(op),
                                  NULL);
            break;
        default:
            result = (r_size(op) == 1 && *op->value.bytes == '*');
    }
    make_bool(op1, result);
    pop(1);
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def zstring_op_defs[] =
{
    {"1.bytestring", zbytestring},
    {"2anchorsearch", zanchorsearch},
    {"1.namestring", znamestring},
    {"2search", zsearch},
    {"2rsearch", zrsearch},
    {"1string", zstring},
    {"2.stringbreak", zstringbreak},
    {"2.stringmatch", zstringmatch},
    op_def_end(0)
};
