/* Copyright (C) 2001-2012 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/


/* Extended (non-standard) filter creation */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gsstruct.h"
#include "ialloc.h"
#include "idict.h"
#include "idparam.h"
#include "store.h"
#include "strimpl.h"
#include "sfilter.h"
#include "sbtx.h"
#include "shcgen.h"
#include "smtf.h"
#include "ifilter.h"


/* <array> <max_length> .computecodes <array> */
/* The first max_length+1 elements of the array will be filled in with */
/* the code counts; the remaining elements will be replaced with */
/* the code values.  This is the form needed for the Tables element of */
/* the dictionary parameter for the BoundedHuffman filters. */
static int
zcomputecodes(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    os_ptr op1 = op - 1;
    uint asize;
    hc_definition def;
    ushort *data;
    long *freqs;
    int code = 0;

    check_type(*op, t_integer);
    check_write_type(*op1, t_array);
    asize = r_size(op1);
    if (op->value.intval < 1 || op->value.intval > max_hc_length)
        return_error(gs_error_rangecheck);
    def.num_counts = op->value.intval;
    if (asize < def.num_counts + 2)
        return_error(gs_error_rangecheck);
    def.num_values = asize - (def.num_counts + 1);
    data = (ushort *) gs_alloc_byte_array(imemory, asize, sizeof(ushort),
                                          "zcomputecodes");
    freqs = (long *)gs_alloc_byte_array(imemory, def.num_values,
                                        sizeof(long),
                                        "zcomputecodes(freqs)");

    if (data == 0 || freqs == 0)
        code = gs_note_error(gs_error_VMerror);
    else {
        uint i;

        def.counts = data;
        def.values = data + (def.num_counts + 1);
        for (i = 0; i < def.num_values; i++) {
            const ref *pf = op1->value.const_refs + i + def.num_counts + 1;

            if (!r_has_type(pf, t_integer)) {
                code = gs_note_error(gs_error_typecheck);
                break;
            }
            freqs[i] = pf->value.intval;
        }
        if (!code) {
            code = hc_compute(&def, freqs, imemory);
            if (code >= 0) {
                /* Copy back results. */
                for (i = 0; i < asize; i++)
                    make_int(op1->value.refs + i, data[i]);
            }
        }
    }
    gs_free_object(imemory, freqs, "zcomputecodes(freqs)");
    gs_free_object(imemory, data, "zcomputecodes");
    if (code < 0)
        return code;
    pop(1);
    return code;
}


/* ------ Byte translation filters ------ */

/* Common setup */
static int
bt_setup(os_ptr op, stream_BT_state * pbts)
{
    check_read_type(*op, t_string);
    if (r_size(op) != 256)
        return_error(gs_error_rangecheck);
    memcpy(pbts->table, op->value.const_bytes, 256);
    return 0;
}

/* <target> <table> ByteTranslateEncode/filter <file> */
/* <target> <table> <dict> ByteTranslateEncode/filter <file> */
static int
zBTE(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream_BT_state bts;
    int code = bt_setup(op, &bts);

    if (code < 0)
        return code;
    return filter_write(op, 0, &s_BTE_template, (stream_state *)&bts, 0);
}

/* <target> <table> ByteTranslateDecode/filter <file> */
/* <target> <table> <dict> ByteTranslateDecode/filter <file> */
static int
zBTD(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;
    stream_BT_state bts;
    int code = bt_setup(op, &bts);

    if (code < 0)
        return code;
    return filter_read(i_ctx_p, 0, &s_BTD_template, (stream_state *)&bts, 0);
}

/* ------ Move-to-front filters ------ */

/* <target> MoveToFrontEncode/filter <file> */
/* <target> <dict> MoveToFrontEncode/filter <file> */
static int
zMTFE(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    return filter_write_simple(op, &s_MTFE_template);
}

/* <source> MoveToFrontDecode/filter <file> */
/* <source> <dict> MoveToFrontDecode/filter <file> */
static int
zMTFD(i_ctx_t *i_ctx_p)
{
    os_ptr op = osp;

    return filter_read_simple(op, &s_MTFD_template);
}

/* ================ Initialization procedure ================ */

const op_def zfilterx_op_defs[] =
{
    {"2.computecodes", zcomputecodes},	/* not a filter */
    op_def_begin_filter(),
                /* Non-standard filters */
    {"2ByteTranslateEncode", zBTE},
    {"2ByteTranslateDecode", zBTD},
    {"1MoveToFrontEncode", zMTFE},
    {"1MoveToFrontDecode", zMTFD},
    op_def_end(0)
};
