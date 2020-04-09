/* Copyright (C) 2001-2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* pxparse.c */
/* PCL XL parser */

#include "memory_.h"
#include "gdebug.h"
#include "gserrors.h"
#include "gsio.h"
#include "gstypes.h"
#include "plparse.h"
#include "pxattr.h"
#include "pxenum.h"
#include "pxerrors.h"
#include "pxoper.h"
#include "pxtag.h"
#include "pxvalue.h"
#include "pxparse.h"            /* requires pxattr.h, pxvalue.h */
#include "pxptable.h"           /* requires pxenum.h, pxoper.h, pxvalue.h */
#include "pxstate.h"
#include "pxpthr.h"
#include "gsstruct.h"
#include "pxgstate.h"           /* Prototype for px_high_level_pattern */

/*
 * We define the syntax of each possible tag by 4 parameters:
 *	- The minimum number of input bytes required to parse it;
 *	- A mask and match value for the state(s) in which it is legal;
 *	- A transition mask to be xor'ed with the state.
 * See pxparse.h for the state mask values.
 */
typedef struct px_tag_syntax_s
{
    byte min_input;
    byte state_value;
    byte state_mask;
    byte state_transition;
} px_tag_syntax_t;

/* Define the common syntaxes. */
#define N (ptsData|ptsReading)  /* normal state */
#define I {1,0,0,0}             /* illegal or unusual */
#define P {1,ptsInPage,ptsInPage|N,0}   /* ordinary operator */
#define C {1,ptsInSession,ptsInSession|N,0}     /* control operator */
#define R(p,r,t)	/* reading operator */\
 {1,ptsInSession|p|r,ptsInSession|p|N,t}
#define D(n) {n,0,ptsData,ptsData}      /* data type */
#define DI D(1)                 /* invalid data type */
#define A(n) {n,ptsData,ptsData,ptsData}        /* attribute */

/* Define the syntax of all tags. */
static const px_tag_syntax_t tag_syntax[256] = {
/*0x*/
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,
/*1x*/
    I, I, I, I, I, I, I, I, I, I, I, {2, 0, 0, 0}, I, I, I, I,
/*2x*/
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,
/*3x*/
    I, I, I, I, I, I, I, I, I, I, I, I, I, I, I, I,
/*4x*/
    {2, 0, ptsInSession | N, 0},
    {1, 0, ptsInSession | N, ptsInSession},
    {1, ptsInSession, ptsInSession | ptsInPage | N, ptsInSession},
    {1, ptsInSession, ptsInSession | ptsInPage | N, ptsInPage},
    {1, ptsInPage, ptsInPage | N, ptsInPage},
    P, {1, 1, 1, 0}, C,
    C, C, P, P, P, P, P, R(0, 0, ptsReadingFont),
/*5x*/
    R(0, ptsReadingFont, 0),
    R(0, ptsReadingFont, ptsReadingFont),
    R(0, 0, ptsReadingChar),
    R(0, ptsReadingChar, 0),
    R(0, ptsReadingChar, ptsReadingChar),
    C, P, P,
    P, P, P,
    {1, ptsInSession, ptsInSession | ptsExecStream | N, ptsReadingStream},
    {1, ptsInSession | ptsReadingStream, ptsInSession | ptsExecStream | N, 0},
    {1, ptsInSession | ptsReadingStream, ptsInSession | ptsExecStream | N,
     ptsReadingStream},
    {1, ptsInSession, ptsInSession | ptsReadingStream | N, 0},
    P,
/*6x*/
    P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
/*7x*/
    P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
/*8x*/
    P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
/*9x*/
    P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
/*ax*/
    P, P, P, P, P, P, P, P, P, P, P, P, P, P, P, P,
/*bx*/
    R(ptsInPage, 0, ptsReadingImage),
    R(ptsInPage, ptsReadingImage, 0),
    R(ptsInPage, ptsReadingImage, ptsReadingImage),
    R(ptsInPage, 0, ptsReadingRastPattern),
    R(ptsInPage, ptsReadingRastPattern, 0),
    R(ptsInPage, ptsReadingRastPattern, ptsReadingRastPattern),
    R(ptsInPage, 0, ptsReadingScan),
    P,
    R(ptsInPage, ptsReadingScan, ptsReadingScan),
    R(ptsInPage, ptsReadingScan, 0),
    P, P, P, P, P, P,
/*cx*/
    D(2), D(3), D(5), D(3), D(5), D(5), DI, DI,
    D(3), D(3), D(3), D(3), D(3), D(3), DI, DI,
/*dx*/
    D(3), D(5), D(9), D(5), D(9), D(9), DI, DI,
    DI, DI, DI, DI, DI, DI, DI, DI,
/*ex*/
    D(5), D(9), D(17), D(9), D(17), D(17), DI, DI,
    DI, DI, DI, DI, DI, DI, DI, DI,
/*fx*/
    I, I, I, I, I, I, I, I,
    A(2), A(3), {5, 0, 0, 0}, {2, 0, 0, 0}, I, I, I, I
};
#undef I
#undef P
#undef C
#undef R
#undef D
#undef DI
#undef A

/* Allocate a parser state. */
px_parser_state_t *
px_process_alloc(gs_memory_t * memory)
{
    px_parser_state_t *st = (px_parser_state_t *) gs_alloc_bytes(memory,
                                                                 sizeof
                                                                 (px_parser_state_t),
                                                                 "px_process_alloc");

    if (st == 0)
        return 0;
    st->memory = memory;
    px_process_init(st, true);
    return st;
}

/* Release a parser state. */
void
px_process_release(px_parser_state_t * st)
{
    gs_free_object(st->memory, st, "px_process_alloc");
}

/* Initialize the parser state. */
void
px_process_init(px_parser_state_t * st, bool big_endian)
{
    st->big_endian = big_endian;
    st->operator_count = 0;
    st->parent_operator_count = 0;
    st->last_operator = pxtNull;
    st->saved_count = 0;
    st->data_left = 0;
    st->macro_state = ptsInitial;
    st->stack_count = 0;
    st->data_proc = 0;
    {
        int i;

        for (i = 0; i < max_px_args; ++i)
            st->args.pv[i] = 0;
    }
    st->args.parser = 0;        /* for garbage collector */
    memset(st->attribute_indices, 0, px_attribute_next);
}

/* Get numeric values from the input. */
#define get_uint16(st, p) uint16at(p, st->big_endian)
#define get_sint16(st, p) sint16at(p, st->big_endian)
#define get_uint32(st, p) uint32at(p, st->big_endian)
#define get_sint32(st, p) sint32at(p, st->big_endian)
#define get_real32(st, p) real32at(p, st->big_endian)

/* Move an array to the heap for persistence if needed. */
/* See pxoper.h for details. */
static int
px_save_array(px_value_t * pv, px_state_t * pxs, client_name_t cname,
              uint nbytes)
{
    if (pv->type & pxd_on_heap) {       /* Turn off the "on heap" bit, to prevent freeing. */
        pv->type &= ~pxd_on_heap;
    } else {                    /* Allocate a heap copy.  Only the first nbytes bytes */
        /* of the data are valid. */
        uint num_elements = pv->value.array.size;
        uint elt_size = value_size(pv);
        byte *copy = gs_alloc_byte_array(pxs->memory, num_elements,
                                         elt_size, cname);

        if (copy == 0)
            return_error(errorInsufficientMemory);
        memcpy(copy, pv->value.array.data, nbytes);
        pv->value.array.data = copy;
    }
    return 0;
}

/* Clear the stack and the attribute indices, */
/* and free heap-allocated arrays. */
#define clear_stack()\
        for ( ; sp > st->stack; --sp )\
          { if ( sp->type & pxd_on_heap )\
              gs_free_object(memory, (void *)sp->value.array.data,\
                             "px stack pop");\
            st->attribute_indices[sp->attribute] = 0;\
          }

/* Define data tracing if debugging. */
#ifdef DEBUG
#  define trace_data(mem, format, cast, ptr, count)\
                  do\
                    { uint i_;\
                      for ( i_ = 0; i_ < count; ++i_ )\
                        dmprintf1(mem, format, (cast)((ptr)[i_]));\
                      dmputc(mem, '\n');\
                    }\
                  while (0)
static void
trace_array_data(const gs_memory_t * mem, const char *label,
                 const px_value_t * pav)
{
    px_data_type_t type = pav->type;
    const byte *ptr = pav->value.array.data;
    uint count = pav->value.array.size;
    bool big_endian = (type & pxd_big_endian) != 0;
    bool text = (type & pxd_ubyte) != 0;
    uint i;

    dmputs(mem, label);
    dmputs(mem, (type & pxd_ubyte ? " <" : " {"));
    for (i = 0; i < count; ++i) {
        if (!(i & 15) && i) {
            const char *p;

            dmputs(mem, "\n  ");
            for (p = label; *p; ++p)
                dmputc(mem, ' ');
        }
        if (type & pxd_ubyte) {
            dmprintf1(mem, "%02x ", ptr[i]);
            if (ptr[i] < 32 || ptr[i] > 126)
                text = false;
        } else if (type & pxd_uint16)
            dmprintf1(mem, "%u ", uint16at(ptr + i * 2, big_endian));
        else if (type & pxd_sint16)
            dmprintf1(mem, "%d ", sint16at(ptr + i * 2, big_endian));
        else if (type & pxd_uint32)
            dmprintf1(mem, "%lu ", (ulong) uint32at(ptr + i * 4, big_endian));
        else if (type & pxd_sint32)
            dmprintf1(mem, "%ld ", (long)sint32at(ptr + i * 4, big_endian));
        else if (type & pxd_real32)
            dmprintf1(mem, "%g ", real32at(ptr + i * 4, big_endian));
        else
            dmputs(mem, "? ");
    }
    dmputs(mem, (type & pxd_ubyte ? ">\n" : "}\n"));
    if (text) {
        dmputs(mem, "%chars: \"");
        debug_print_string(mem, ptr, count);
        dmputs(mem, "\"\n");
    }
}
#  define trace_array(mem,pav)\
     if ( gs_debug_c('I') )\
       trace_array_data(mem,"array:", pav)
#else
#  define trace_data(mem, format, cast, ptr, count) DO_NOTHING
#  define trace_array(mem,pav) DO_NOTHING
#endif

/* Process a buffer of PCL XL commands. */
int
px_process(px_parser_state_t * st, px_state_t * pxs, stream_cursor_read * pr)
{
    const byte *orig_p = pr->ptr;
    const byte *next_p = orig_p;        /* start of data not copied to saved */
    const byte *p;
    const byte *rlimit;
    px_value_t *sp = &st->stack[st->stack_count];

#define stack_limit &st->stack[max_stack - 1]
    gs_memory_t *memory = st->memory;
    int code = 0;
    uint left;
    uint min_left;
    px_tag_t tag;
    const px_tag_syntax_t *syntax = 0;

    st->args.parser = st;
    st->parent_operator_count = 0;      /* in case of error */
    /* Check for leftover data from the previous call. */
  parse:if (st->saved_count) { /* Fill up the saved buffer so we can make progress. */
        int move = min(sizeof(st->saved) - st->saved_count,
                       pr->limit - next_p);

        memcpy(&st->saved[st->saved_count], next_p + 1, move);
        next_p += move;
        p = st->saved - 1;
        rlimit = p + st->saved_count + move;
    } else {                    /* No leftover data, just read from the input. */
        p = next_p;
        rlimit = pr->limit;
    }
  top:if (st->data_left) {     /* We're in the middle of reading an array or data block. */
        if (st->data_proc) {    /* This is a data block. */
            uint avail = min(rlimit - p, st->data_left);
            uint used;

            st->args.source.available = avail;
            st->args.source.data = p + 1;
            code = (*st->data_proc) (&st->args, pxs);
            /* If we get a 'remap_color' error, it means we are dealing with a
             * pattern, and the device supports high level patterns. So we must
             * use our high level pattern implementation.
             */
            if (code == gs_error_Remap_Color) {
                code = px_high_level_pattern(pxs->pgs);
                code = (*st->data_proc) (&st->args, pxs);
            }
            used = st->args.source.data - (p + 1);
#ifdef DEBUG
            if (gs_debug_c('I')) {
                px_value_t data_array;

                data_array.type = pxd_ubyte;
                data_array.value.array.data = p + 1;
                data_array.value.array.size = used;
                trace_array_data(pxs->memory, "data:", &data_array);
            }
#endif
            p = st->args.source.data - 1;
            st->data_left -= used;
            if (code < 0) {
                st->args.source.position = 0;
                goto x;
            } else if ((code == pxNeedData)
                       || (code == pxPassThrough && st->data_left != 0)) {
                code = 0;       /* exit for more data */
                goto x;
            } else {
                st->args.source.position = 0;
                st->data_proc = 0;
                if (st->data_left != 0) {
                    code = gs_note_error(errorExtraData);
                    goto x;
                }
                clear_stack();
            }
        } else {                /* This is an array. */
            uint size = sp->value.array.size;
            uint scale = value_size(sp);
            uint nbytes = size * scale;
            byte *dest =
                (byte *) sp->value.array.data + nbytes - st->data_left;

            left = rlimit - p;
            if (left < st->data_left) { /* We still don't have enough data to fill the array. */
                memcpy(dest, p + 1, left);
                st->data_left -= left;
                p = rlimit;
                code = 0;
                goto x;
            }
            /* Complete the array and continue parsing. */
            memcpy(dest, p + 1, st->data_left);
            trace_array(memory, sp);
            p += st->data_left;
        }
        st->data_left = 0;
    } else if (st->data_proc) { /* An operator is awaiting data. */
        /* Skip white space until we find some. */
        code = 0;               /* in case we exit */
        /* special case - VendorUnique has a length attribute which
           we've already parsed and error checked */
        if (st->data_proc == pxVendorUnique) {
            st->data_left =
                st->stack[st->attribute_indices[pxaVUDataLength]].value.i;
            goto top;
        } else {
            while ((left = rlimit - p) != 0) {
                switch ((tag = p[1])) {
                case pxtNull:
                case pxtHT:
                case pxtLF:
                case pxtVT:
                case pxtFF:
                case pxtCR:
                    ++p;
                    continue;
                case pxt_dataLength:
                    if (left < 5)
                        goto x; /* can't look ahead */
                    st->data_left = get_uint32(st, p + 2);
                    if_debug2m('i', memory, "tag=  0x%2x  data, length %u\n",
                               p[1], st->data_left);
                    p += 5;
                    goto top;
                case pxt_dataLengthByte:
                    if (left < 2)
                        goto x; /* can't look ahead */
                    st->data_left = p[2];
                    if_debug2m('i', memory, "tag=  0x%2x  data, length %u\n",
                               p[1], st->data_left);
                    p += 2;
                    goto top;
                default:
                    {
                        code = gs_note_error(errorMissingData);
                        goto x;
                    }
                }
            }
        }
    }
    st->args.source.position = 0;
    st->args.source.available = 0;
    while ((left = rlimit - p) != 0 &&
           left >= (min_left = (syntax = &tag_syntax[tag = p[1]])->min_input)
        ) {
        int count;

#ifdef DEBUG
        if (gs_debug_c('i')) {
            dmprintf1(memory, "tag=  0x%02x  ", tag);
            if (tag == pxt_attr_ubyte || tag == pxt_attr_uint16) {
                px_attribute_t attr =
                    (tag == pxt_attr_ubyte ? p[2] : get_uint16(st, p + 2));
                const char *aname = px_attribute_names[attr];

                if (aname)
                    dmprintf1(memory, "   @%s\n", aname);
                else
                    dmprintf1(memory, "   attribute %u ???\n", attr);
            } else {
                const char *format;
                const char *tname;
                bool operator = false;

                if (tag < 0x40)
                    format = "%s\n", tname = px_tag_0_names[tag];
                else if (tag < 0xc0)
                    format = "%s", tname = px_operator_names[tag - 0x40],
                        operator = true;
                else {
                    tname = px_tag_c0_names[tag - 0xc0];
                    if (tag < 0xf0)
                        format = "      %s";    /* data values follow */
                    else
                        format = "%s\n";
                }
                if (tname) {
                    dmprintf1(memory, format, tname);
                    if (operator)
                        dmprintf1(memory, " (%ld)\n", st->operator_count + 1);
                } else
                    dmputs(memory, "???\n");
            }
        }
#endif
        if ((st->macro_state & syntax->state_mask) != syntax->state_value) {
            /*
             * We should probably distinguish here between
             * out-of-context operators and illegal tags, but it's too
             * much trouble.
             */
            code = gs_note_error(errorIllegalOperatorSequence);
            if (tag >= 0x40 && tag < 0xc0)
                st->last_operator = tag;
            goto x;
        }
        st->macro_state ^= syntax->state_transition;
        switch (tag >> 3) {
            case 0:
                switch (tag) {
                    case pxtNull:
                        ++p;
                        continue;
                    default:
                        break;
                }
                break;
            case 1:
                switch (tag) {
                    case pxtHT:
                    case pxtLF:
                    case pxtVT:
                    case pxtFF:
                    case pxtCR:
                        ++p;
                        continue;
                    default:
                        break;
                }
                break;
            case 3:
                if (tag == pxt1b) {     /* ESC *//* Check for UEL */
                    if (memcmp(p + 1, "\033%-12345X", min(left, 9)))
                        break;  /* not UEL, error */
                    if (left < 9)
                        goto x; /* need more data */
                    p += 9;
                    code = e_ExitLanguage;
                    goto x;
                }
                break;
            case 4:
                switch (tag) {
                    case pxtSpace:
                        /* break; will error, compatible with lj */
                        /* ++p;continue; silently ignores the space */
                        ++p;
                        continue;
                    default:
                        break;
                }
                break;
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 20:
            case 21:
            case 22:
            case 23:
                /* Operators */
                /* Make sure that we have all the required attributes, */
                /* and no attributes that are neither required nor */
                /* optional.  (It's up to the operator to make any */
                /* more precise checks than this. */
                st->operator_count++;
                /* if this is a passthrough operator we have to tell
                   the passthrough module if this operator was
                   preceded by another passthrough operator or a
                   different xl operator */
                if (tag == pxtPassThrough) {
                    pxpcl_passthroughcontiguous(pxs, st->last_operator == tag);
                } else if (st->last_operator == pxtPassThrough) {
                    pxpcl_endpassthroughcontiguous(pxs);
                }

                st->last_operator = tag;
                {
                    const px_operator_definition_t *pod =
                        &px_operator_definitions[tag - 0x40];
                    int left = sp - st->stack;
                    const byte /*px_attribute_t */  * pal = pod->attrs;
                    px_value_t **ppv = st->args.pv;
                    bool required = true;
                    code = 0;
                    /*
                     * Scan the attributes.  Illegal attributes take priority
                     * over missing attributes, which in turn take priority
                     * over illegal data types.
                     */
                    for (;; ++pal, ++ppv) {
                        px_attribute_t attr = *pal;
                        uint index;

                        if (!attr) {    /*
                                         * We've reached the end of either the required or
                                         * the optional attribute list.
                                         */
                            if (!required)
                                break;
                            required = false;
                            --ppv;      /* cancel incrementing */
                            continue;
                        }
                        if ((index = st->attribute_indices[attr]) == 0) {
                            if (required)
                                code = gs_note_error(errorMissingAttribute);
                            else
                                *ppv = 0;
                        } else {        /* Check the attribute data type and value. */
                            px_value_t *pv = *ppv = &st->stack[index];
                            const px_attr_value_type_t *pavt =
                                &px_attr_value_types[attr];
                            int acode;

                            if ((~pavt->mask & pv->type &
                                 (pxd_structure | pxd_representation)) ||
                                (pavt->mask == (pxd_scalar | pxd_ubyte) &&
                                 (pv->value.i < 0
                                  || pv->value.i > pavt->limit))
                                ) {
                                if (code >= 0)
                                    code =
                                        gs_note_error
                                        (errorIllegalAttributeDataType);
                            }
                            if (pavt->proc != 0
                                && (acode = (*pavt->proc) (pv)) < 0) {
                                if (code >= 0)
                                    code = acode;
                            }
                            --left;
                        }
                    }

                    /* Make sure there are no attributes left over. */
                    if (left)
                        code = gs_note_error(errorIllegalAttribute);
                    if (code >= 0) {
                        st->args.source.phase = 0;
                        code = (*pod->proc) (&st->args, pxs);
                        /* If we get a 'remap_color' error, it means we are dealing with a
                         * pattern, and the device supports high level patterns. So we must
                         * use our high level pattern implementation.
                         */
                        if (code == gs_error_Remap_Color) {
                            code = px_high_level_pattern(pxs->pgs);
                            if (code < 0)
                                goto x;
                            code = (*pod->proc) (&st->args, pxs);
                        }
                    }
                    if (code < 0)
                        goto x;
                    /* Check whether the operator wanted source data. */
                    if (code == pxNeedData) {
                        if (!pxs->data_source_open) {
                            code = gs_note_error(errorDataSourceNotOpen);
                            goto x;
                        }
                        st->data_proc = pod->proc;
                        ++p;
                        goto top;
                    }
                }
                clear_stack();
                ++p;
                continue;
            case 24:
                sp[1].type = pxd_scalar;
                count = 1;
                goto data;
            case 26:
                sp[1].type = pxd_xy;
                count = 2;
                goto data;
            case 28:
                sp[1].type = pxd_box;
                count = 4;
                goto data;
                /* Scalar, point, and box data */
              data:{
                    int i;

                    if (sp == stack_limit) {
                        code = gs_note_error(errorInternalOverflow);
                        goto x;
                    }
                    ++sp;
                    sp->attribute = 0;
                    p += 2;
#ifdef DEBUG
#  define trace_scalar(mem, format, cast, alt)\
                  if ( gs_debug_c('i') )\
                    trace_data(mem, format, cast, sp->value.alt, count)
#else
#  define trace_scalar(mem, format, cast, alt) DO_NOTHING
#endif
                    switch (tag & 7) {
                        case pxt_ubyte & 7:
                            sp->type |= pxd_ubyte;
                            for (i = 0; i < count; ++p, ++i)
                                sp->value.ia[i] = *p;
                          dux:trace_scalar(pxs->memory, " %lu", ulong,
                                         ia);
                            --p;
                            continue;
                        case pxt_uint16 & 7:
                            sp->type |= pxd_uint16;
                            for (i = 0; i < count; p += 2, ++i)
                                sp->value.ia[i] = get_uint16(st, p);
                            goto dux;
                        case pxt_uint32 & 7:
                            sp->type |= pxd_uint32;
                            for (i = 0; i < count; p += 4, ++i)
                                sp->value.ia[i] = get_uint32(st, p);
                            goto dux;
                        case pxt_sint16 & 7:
                            sp->type |= pxd_sint16;
                            for (i = 0; i < count; p += 2, ++i)
                                sp->value.ia[i] = get_sint16(st, p);
                          dsx:trace_scalar(pxs->memory, " %ld", long,
                                         ia);
                            --p;
                            continue;
                        case pxt_sint32 & 7:
                            sp->type |= pxd_sint32;
                            for (i = 0; i < count; p += 4, ++i)
                                sp->value.ia[i] = get_sint32(st, p);
                            goto dsx;
                        case pxt_real32 & 7:
                            sp->type |= pxd_real32;
                            for (i = 0; i < count; p += 4, ++i)
                                sp->value.ra[i] = get_real32(st, p);
                            trace_scalar(pxs->memory, " %g", double, ra);

                            --p;
                            continue;
                        default:
                            break;
                    }
                }
                break;
            case 25:
                /* Array data */
                {
                    const byte *dp;
                    uint nbytes;

                    if (sp == stack_limit) {
                        code = gs_note_error(errorInternalOverflow);
                        goto x;
                    }
                    switch (p[2]) {
                        case pxt_ubyte:
                            sp[1].value.array.size = p[3];
                            dp = p + 4;
                            break;
                        case pxt_uint16:
                            if (left < 4) {
                                if_debug0m('i', memory, "...\n");
                                /* Undo the state transition. */
                                st->macro_state ^= syntax->state_transition;
                                goto x;
                            }
                            sp[1].value.array.size = get_uint16(st, p + 3);
                            dp = p + 5;
                            break;
                        default:
                            st->last_operator = tag;    /* for error message */
                            code = gs_note_error(errorIllegalTag);
                            goto x;
                    }
                    nbytes = sp[1].value.array.size;
                    if_debug1m('i', memory, "[%u]\n", sp[1].value.array.size);
                    switch (tag) {
                        case pxt_ubyte_array:
                            sp[1].type = pxd_array | pxd_ubyte;
                          array:++sp;
                            if (st->big_endian)
                                sp->type |= pxd_big_endian;
                            sp->value.array.data = dp;
                            sp->attribute = 0;
                            /* Check whether we have enough data for the entire */
                            /* array. */
                            if (rlimit + 1 - dp < nbytes) {     /* Exit now, continue reading when we return. */
                                uint avail = rlimit + 1 - dp;

                                code = px_save_array(sp, pxs, "partial array",
                                                     avail);
                                if (code < 0)
                                    goto x;
                                sp->type |= pxd_on_heap;
                                st->data_left = nbytes - avail;
                                st->data_proc = 0;
                                p = rlimit;
                                goto x;
                            }
                            p = dp + nbytes - 1;
                            trace_array(memory, sp);
                            continue;
                        case pxt_uint16_array:
                            sp[1].type = pxd_array | pxd_uint16;
                          a16:nbytes <<= 1;
                            goto array;
                        case pxt_uint32_array:
                            sp[1].type = pxd_array | pxd_uint32;
                          a32:nbytes <<= 2;
                            goto array;
                        case pxt_sint16_array:
                            sp[1].type = pxd_array | pxd_sint16;
                            goto a16;
                        case pxt_sint32_array:
                            sp[1].type = pxd_array | pxd_sint32;
                            goto a32;
                        case pxt_real32_array:
                            sp[1].type = pxd_array | pxd_real32;
                            goto a32;
                        default:
                            break;
                    }
                    break;
                }
                break;
            case 31:
                {
                    px_attribute_t attr;
                    const byte *pnext;

                    switch (tag) {
                        case pxt_attr_ubyte:
                            attr = p[2];
                            pnext = p + 2;
                            goto a;
                        case pxt_attr_uint16:
                            attr = get_uint16(st, p + 2);
                            pnext = p + 3;
                          a:if (attr >=
                                px_attribute_next)
                                break;
                            /*
                             * We could check the attribute value type here, but
                             * in order to match the behavior of the H-P printers,
                             * we don't do it until we see the operator.
                             *
                             * It is legal to specify the same attribute more than
                             * once; the last value has priority.  If this happens,
                             * since the order of attributes doesn't matter, we can
                             * just replace the former value on the stack.
                             */
                            sp->attribute = attr;
                            if (st->attribute_indices[attr] != 0) {
                                px_value_t *old_sp =
                                    &st->stack[st->attribute_indices[attr]];
                                /* If the old value is on the heap, free it. */
                                if (old_sp->type & pxd_on_heap)
                                    gs_free_object(memory,
                                                   (void *)old_sp->value.
                                                   array.data,
                                                   "old value for duplicate attribute");
                                *old_sp = *sp--;
                            } else
                                st->attribute_indices[attr] = sp - st->stack;
                            p = pnext;
                            continue;
                        case pxt_dataLength:
                            /*
                             * Unexpected data length operators are normally not
                             * allowed, but there might be a zero-length data
                             * block immediately following a zero-size image,
                             * which doesn't ask for any data.
                             */
                            if (uint32at(p + 2, true /*arbitrary */ ) == 0) {
                                p += 5;
                                continue;
                            }
                            break;
                        case pxt_dataLengthByte:
                            /* See the comment under pxt_dataLength above. */
                            if (p[2] == 0) {
                                p += 2;
                                continue;
                            }
                            break;
                        default:
                            break;
                    }
                }
                break;
            default:
                break;
        }
        /* Unknown tag value.  Report an error. */
        st->last_operator = tag;        /* for error message */
        code = gs_note_error(errorIllegalTag);
        break;
    }
  x:                           /* Save any leftover input. */
    left = rlimit - p;
    if (rlimit != pr->limit) {  /* We were reading saved input. */
        if (left <= next_p - orig_p) {  /* We finished reading the previously saved input. */
            /* Continue reading current input, unless we got an error. */
            p = next_p -= left;
            rlimit = pr->limit;
            st->saved_count = 0;
            if (code >= 0)
                goto parse;
        } else {                /* There's still some previously saved input left over. */
            memmove(st->saved, p + 1, st->saved_count = left);
            p = next_p;
            rlimit = pr->limit;
            left = rlimit - p;
        }
    }
    /* Except in case of error, save any remaining input. */
    if (code >= 0) {
        if (left + st->saved_count > sizeof(st->saved)) {       /* Fatal error -- shouldn't happen! */
            code = gs_note_error(errorInternalOverflow);
            st->saved_count = 0;
        } else {
            memcpy(&st->saved[st->saved_count], p + 1, left);
            st->saved_count += left;
            p = rlimit;
        }
    }
    pr->ptr = p;
    st->stack_count = sp - st->stack;
    /* Move to the heap any arrays whose data was being referenced */
    /* directly in the input buffer. */
    for (; sp > st->stack; --sp)
        if ((sp->type & (pxd_array | pxd_on_heap)) == pxd_array) {
            int code = px_save_array(sp, pxs, "px stack array to heap",
                                     sp->value.array.size * value_size(sp));

            if (code < 0)
                break;
            sp->type |= pxd_on_heap;
        }
    if (code < 0 && syntax != 0) {      /* Undo the state transition. */
        st->macro_state ^= syntax->state_transition;
    }
    return code;
}

uint
px_parser_data_left(px_parser_state_t * pxp)
{
    return pxp->data_left;
}
