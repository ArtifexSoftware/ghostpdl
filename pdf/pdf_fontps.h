/* Copyright (C) 2020-2024 Artifex Software, Inc.
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

/* common code for Postscript-type font handling */

#ifndef PDF_FONTPS_H
#define PDF_FONTPS_H

#include "gxfont1.h"        /* for gs_font_type1_s */

typedef enum
{
  PDF_PS_OBJ_NULL,
  PDF_PS_OBJ_INTEGER,
  PDF_PS_OBJ_FLOAT,
  PDF_PS_OBJ_STRING,
  PDF_PS_OBJ_NAME,
  PDF_PS_OBJ_ARRAY,
  PDF_PS_OBJ_MARK,
  PDF_PS_OBJ_TRUE,
  PDF_PS_OBJ_FALSE,
  PDF_PS_OBJ_ARR_MARK,
  PDF_PS_OBJ_DICT_MARK,
  PDF_PS_OBJ_STACK_TOP,
  PDF_PS_OBJ_STACK_BOTTOM
} pdf_ps_obj_type;

typedef struct pdf_ps_stack_object_s pdf_ps_stack_object_t;

struct pdf_ps_stack_object_s
{
  pdf_ps_obj_type type;
  uint32_t size;
  union v {
    int i;
    float f;
    byte *string;
    byte *name;
    pdf_ps_stack_object_t *arr;
  } val;
};

/* The maximum size of a set of a single "block" is 100 "items"
   the worst of which is cid ranges which take three objects
   (lo character code, hi character, low cid) for each item.
   Thus we need an available stack depth of up to 300 objects.
   Allowing for possible bad behaviour, 350 seems like a safe
   size. There are stack limit guard entries at the top and bottom
 */
typedef struct pdf_ps_ctx_s pdf_ps_ctx_t;

#define PDF_PS_OPER_NAME_AND_LEN(s) (const byte *)s, sizeof(s) - 1

typedef struct {
  const byte *opname;
  const int opnamelen;
  int (*oper)(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend);
} pdf_ps_oper_list_t;

#define PDF_PS_STACK_SIZE 360
#define PDF_PS_STACK_GUARDS 1
#define PDF_PS_STACK_GROW_SIZE PDF_PS_STACK_SIZE + 2 * PDF_PS_STACK_GUARDS
#define PDF_PS_STACK_MAX PDF_PS_STACK_SIZE * 16 /* Arbitrary value.... */
struct pdf_ps_ctx_s
{
  pdf_context *pdfi_ctx;
  pdf_ps_stack_object_t *cur; /* current top of the stack */
  pdf_ps_stack_object_t *toplim; /* the upper limit of the stack */
  pdf_ps_stack_object_t *stack;
  pdf_ps_oper_list_t *ops;
  void *client_data;
};


typedef struct {
    union {
        pdf_font_type1 t1;
        pdf_cidfont_type0 cidt0;
    } u;
    union {
        gs_font_type1 gst1;
    } gsu;
} ps_font_interp_private;


int pdfi_read_ps_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int fbuflen, ps_font_interp_private *ps_font_priv);

int pdfi_pscript_stack_init(pdf_context *pdfi_ctx, pdf_ps_oper_list_t *ops, void *client_data, pdf_ps_ctx_t *s);
void pdfi_pscript_stack_finit(pdf_ps_ctx_t *s);
int pdfi_pscript_interpret(pdf_ps_ctx_t *cs, byte *pspdfbuf, int64_t buflen);

/* Begin default operator functions */
int ps_pdf_null_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend);
int clear_stack_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend);
int pdf_ps_pop_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend);
int pdf_ps_pop_and_pushmark_func(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend);
/* End default operator functions */

static inline void pdf_ps_make_null(pdf_ps_stack_object_t *obj)
{
  obj->type = PDF_PS_OBJ_NULL;
  obj->size = 0;
  memset(&obj->val, 0x00, sizeof(obj->val));
}
static inline void pdf_ps_make_array_mark(pdf_ps_stack_object_t *obj)
{
  obj->type = PDF_PS_OBJ_ARR_MARK;
  obj->size = 0;
}
static inline void pdf_ps_make_dict_mark(pdf_ps_stack_object_t *obj)
{
  obj->type = PDF_PS_OBJ_DICT_MARK;
  obj->size = 0;
}
static inline void pdf_ps_make_mark(pdf_ps_stack_object_t *obj)
{
  obj->type = PDF_PS_OBJ_MARK;
  obj->size = 0;
}
static inline void pdf_ps_make_int(pdf_ps_stack_object_t *obj, int val)
{
  obj->type = PDF_PS_OBJ_INTEGER;
  obj->size = 0;
  obj->val.i = val;
}
static inline void pdf_ps_make_float(pdf_ps_stack_object_t *obj, float fval)
{
  obj->type = PDF_PS_OBJ_FLOAT;
  obj->size = 0;
  obj->val.f = fval;
}
static inline void pdf_ps_make_string(pdf_ps_stack_object_t *obj, byte *str, uint32_t len)
{
  obj->type = PDF_PS_OBJ_STRING;
  obj->size = len;
  obj->val.string = str;
}
static inline void pdf_ps_make_name(pdf_ps_stack_object_t *obj, byte *nm, uint32_t len)
{
  obj->type = PDF_PS_OBJ_NAME;
  obj->size = len;
  obj->val.name = nm;
}
static inline void pdf_ps_make_array(pdf_ps_stack_object_t *obj, pdf_ps_stack_object_t *obj2, uint32_t len)
{
  obj->type = PDF_PS_OBJ_ARRAY;
  obj->size = len;
  obj->val.arr = obj2;
}
static inline void pdf_ps_make_boolean(pdf_ps_stack_object_t *obj, bool b)
{
  obj->type = b ? PDF_PS_OBJ_TRUE : PDF_PS_OBJ_FALSE;
  obj->size = 0;
}

static inline int pdf_ps_obj_has_type(pdf_ps_stack_object_t *o, pdf_ps_obj_type t)
{
    return o->type == t;
}

static inline uint32_t pdf_ps_obj_size(pdf_ps_stack_object_t *o)
{
    uint32_t s = 0;
    if (o->type == PDF_PS_OBJ_ARRAY || o->type == PDF_PS_OBJ_NAME || o->type == PDF_PS_OBJ_STRING) {
        s = o->size;
    }
    return s;
}

/* The stack can grow, but doesn't shrink, just gets destroyed
   when we're done interpreting
 */
static inline int pdf_ps_stack_push(pdf_ps_ctx_t *s)
{
    /* Extending the stack pretty inefficient, but it shouldn't happen often
       for valid files
     */
    if (s->cur + 1 >= s->toplim - 1) {
        int i, currsize = s->toplim - s->stack;
        int newsize = currsize + PDF_PS_STACK_GROW_SIZE;
        int newsizebytes = newsize * sizeof(pdf_ps_stack_object_t);
        pdf_ps_stack_object_t *nstack;

        if (newsize < PDF_PS_STACK_MAX) {
            nstack = (pdf_ps_stack_object_t *)gs_alloc_bytes(s->pdfi_ctx->memory, newsizebytes, "pdf_ps_stack_push(nstack)");
            if (nstack != NULL) {
                memcpy(nstack, s->stack, (currsize - 1) * sizeof(pdf_ps_stack_object_t));

                for (i = 0; i < PDF_PS_STACK_GUARDS; i++)
                    nstack[newsize - PDF_PS_STACK_GUARDS + i].type = PDF_PS_OBJ_STACK_TOP;

                for (i = currsize - 1; i < newsize - PDF_PS_STACK_GUARDS; i++) {
                    pdf_ps_make_null(&(nstack[i]));
                }

                gs_free_object(s->pdfi_ctx->memory, s->stack, "pdf_ps_stack_push(s->stack)");
                s->stack = nstack;
                s->cur = s->stack + currsize - 2;
                s->toplim = s->stack + newsize;
            }
            else {
                return_error(gs_error_VMerror);
            }
        }
        else {
            return_error(gs_error_stackoverflow);
        }
    }
    s->cur++;
    if (pdf_ps_obj_has_type(s->cur, PDF_PS_OBJ_STACK_TOP))
        return_error(gs_error_pdf_stackoverflow);
    if (pdf_ps_obj_has_type(s->cur, PDF_PS_OBJ_STACK_BOTTOM))
        return_error(gs_error_stackunderflow);
    return 0;
}

static inline void pdf_ps_free_array_contents(pdf_ps_ctx_t *s, pdf_ps_stack_object_t *o)
{
    int i;
    for (i = 0; i < o->size; i++) {
        if (pdf_ps_obj_has_type(&o->val.arr[i], PDF_PS_OBJ_ARRAY)) {
            pdf_ps_stack_object_t *po = o->val.arr[i].val.arr;
            pdf_ps_free_array_contents(s, &o->val.arr[i]);
            gs_free_object(s->pdfi_ctx->memory, po,  "pdf_ps_free_array_contents");
        }
        pdf_ps_make_null(&o->val.arr[i]);
    }
}

static inline int pdf_ps_stack_pop(pdf_ps_ctx_t *s, unsigned int n)
{
    int n2 = n > s->cur - &(s->stack[0]) ? s->cur - &(s->stack[0]) : n;
    while(n2--) {
        /* We only have one dimensional arrays to worry about */
        if (pdf_ps_obj_has_type(s->cur, PDF_PS_OBJ_ARRAY)) {
            pdf_ps_free_array_contents(s, s->cur);
            gs_free_object(s->pdfi_ctx->memory, s->cur->val.arr,  "pdf_ps_stack_pop(s->cur->val.arr)");
        }
        pdf_ps_make_null(s->cur);
        s->cur--;
        if (pdf_ps_obj_has_type(s->cur, PDF_PS_OBJ_STACK_TOP))
            return_error(gs_error_pdf_stackoverflow);
        if (pdf_ps_obj_has_type(s->cur, PDF_PS_OBJ_STACK_BOTTOM))
            return_error(gs_error_stackunderflow);
    }
    return 0;
}

static inline int pdf_ps_stack_push_arr_mark(pdf_ps_ctx_t *s)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_array_mark(s->cur);
    return 0;
}

static inline int pdf_ps_stack_push_dict_mark(pdf_ps_ctx_t *s)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_dict_mark(s->cur);
    return 0;
}

static inline int pdf_ps_stack_push_mark(pdf_ps_ctx_t *s)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_mark(s->cur);
    return 0;
}

static inline int pdf_ps_stack_push_int(pdf_ps_ctx_t *s, int i)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_int(s->cur, i);
    return 0;
}

static inline int pdf_ps_stack_push_float(pdf_ps_ctx_t *s, float f)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_float(s->cur, f);
    return 0;
}

/* String, name and array have an arbitrary limit of 64k on their size. */
static inline int pdf_ps_stack_push_string(pdf_ps_ctx_t *s, byte *str, uint32_t len)
{
    int code;

    if (len > 65535)
        return gs_error_limitcheck;

    code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_string(s->cur, str, len);
    return 0;
}

static inline int pdf_ps_stack_push_name(pdf_ps_ctx_t *s, byte *nm, uint32_t len)
{
    int code;

    if (len > 65535)
        return gs_error_limitcheck;

    code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_name(s->cur, nm, len);
    return 0;
}

static inline int pdf_ps_stack_push_array(pdf_ps_ctx_t *s, pdf_ps_stack_object_t *a, uint32_t len)
{
    int code;

    if (len > 65535)
        return gs_error_limitcheck;

    code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_array(s->cur, a, len);
    return 0;
}

static inline int pdf_ps_stack_push_boolean(pdf_ps_ctx_t *s, bool b)
{
    int code = pdf_ps_stack_push(s);
    if (code < 0) return code;

    pdf_ps_make_boolean(s->cur, b);
    return 0;
}

static inline pdf_ps_obj_type pdf_ps_stack_obj_type(pdf_ps_ctx_t *s)
{
    return s->cur->type;
}

static inline int pdf_ps_stack_count_to_mark(pdf_ps_ctx_t *s, pdf_ps_obj_type mtype)
{
    int i = gs_error_stackunderflow, depth = s->cur - &(s->stack[0]) + 1;
    for (i = 0; i < depth; i++) {
        if (s->cur[-i].type == PDF_PS_OBJ_STACK_BOTTOM) {
            i = gs_note_error(gs_error_unmatchedmark);
            break;
        }
        if (s->cur[-i].type == mtype)
            break;
    }
    return i;
}

static inline int pdf_ps_stack_count(pdf_ps_ctx_t *s)
{
    return s->cur - &(s->stack[1]);
}

#endif
