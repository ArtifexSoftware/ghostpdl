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

#include "strmio.h"
#include "stream.h"
#include "scanchar.h"

#include "pdf_int.h"
#include "pdf_cmap.h"

#include "pdf_stack.h"
#include "pdf_dict.h"
#include "pdf_file.h"

typedef enum
{
  CMAP_OBJ_NULL,
  CMAP_OBJ_INTEGER,
  CMAP_OBJ_FLOAT,
  CMAP_OBJ_STRING,
  CMAP_OBJ_NAME,
  CMAP_OBJ_ARRAY,
  CMAP_OBJ_MARK,
  CMAP_OBJ_ARR_MARK,
  CMAP_OBJ_DICT_MARK,
  CMAP_OBJ_STACK_TOP,
  CMAP_OBJ_STACK_BOTTOM
} cmap_obj_type;

typedef struct cmap_stack_object_s cmap_stack_object_t;

struct cmap_stack_object_s
{
  cmap_obj_type type;
  int size;
  union v {
    int i;
    float f;
    byte *string;
    byte *name;
    cmap_stack_object_t *arr;
  } val;
};

/* The maximum size of a set of a single "block" is 100 "items"
   the worst of which is cid ranges which take three objects
   (lo character code, hi character, low cid) for each item.
   Thus we need an available stack depth of up to 300 objects.
   Allowing for possible bad behaviour, 350 seems like a safe
   size. There are stack limit guard entries at the top and bottom
 */
#define CMAP_STACK_SIZE 350
#define CMAP_STACK_GUARDS 1

typedef struct
{
  gs_memory_t *memory;
  cmap_stack_object_t *cur; /* current top of the stack */
  cmap_stack_object_t *toplim; /* the upper limit of the stack */
  cmap_stack_object_t stack[CMAP_STACK_SIZE + 2 * CMAP_STACK_GUARDS];
} cmap_stack_t;

static inline void cmap_make_null(cmap_stack_object_t *obj)
{
  obj->type = CMAP_OBJ_NULL;
  obj->size = 0;
  memset(&obj->val, 0x00, sizeof(obj->val));
}
static inline void cmap_make_array_mark(cmap_stack_object_t *obj)
{
  obj->type = CMAP_OBJ_ARR_MARK;
  obj->size = 0;
}
static inline void cmap_make_dict_mark(cmap_stack_object_t *obj)
{
  obj->type = CMAP_OBJ_DICT_MARK;
  obj->size = 0;
}
static inline void cmap_make_mark(cmap_stack_object_t *obj)
{
  obj->type = CMAP_OBJ_MARK;
  obj->size = 0;
}
static inline void cmap_make_int(cmap_stack_object_t *obj, int val)
{
  obj->type = CMAP_OBJ_INTEGER;
  obj->size = 0;
  obj->val.i = val;
}
static inline void cmap_make_float(cmap_stack_object_t *obj, float fval)
{
  obj->type = CMAP_OBJ_FLOAT;
  obj->size = 0;
  obj->val.f = fval;
}
static inline void cmap_make_string(cmap_stack_object_t *obj, byte *str, int len)
{
  obj->type = CMAP_OBJ_STRING;
  obj->size = len;
  obj->val.string = str;
}
static inline void cmap_make_name(cmap_stack_object_t *obj, byte *nm, int len)
{
  obj->type = CMAP_OBJ_NAME;
  obj->size = len;
  obj->val.name = nm;
}
static inline void cmap_make_array(cmap_stack_object_t *obj, cmap_stack_object_t *obj2, int len)
{
  obj->type = CMAP_OBJ_ARRAY;
  obj->size = len;
  obj->val.arr = obj2;
}

static inline bool cmap_obj_has_type(cmap_stack_object_t *o, cmap_obj_type t)
{
    return o->type == t;
}

static void cmap_stack_init(gs_memory_t *mem, cmap_stack_t *s)
{
  int i, size = (sizeof(s->stack) / sizeof(s->stack[0])) - 2;
  s->memory = mem;
  s->cur = &(s->stack[0]);
  s->toplim = s->cur + size;

  for (i = 0; i < CMAP_STACK_GUARDS; i++)
      s->stack[i].type = CMAP_OBJ_STACK_BOTTOM;

  for (i = 0; i < CMAP_STACK_GUARDS; i++)
      s->stack[size + 1 + i].type = CMAP_OBJ_STACK_TOP;

  for (i = 0; i < size; i++) {
    cmap_make_null(&(s->cur[i]));
  }
}

static inline int cmap_stack_push(cmap_stack_t *s)
{
    s->cur++;
    if (cmap_obj_has_type(s->cur, CMAP_OBJ_STACK_TOP))
        return_error(gs_error_stackoverflow);
    if (cmap_obj_has_type(s->cur, CMAP_OBJ_STACK_BOTTOM))
        return_error(gs_error_stackunderflow);
    return 0;
}

static inline int cmap_stack_pop(cmap_stack_t *s, unsigned int n)
{
    while(n--) {
        /* We only have one dimensional arrays to worry about */
        if (cmap_obj_has_type(s->cur, CMAP_OBJ_ARRAY)) {
            gs_free_object(s->memory, s->cur->val.arr,  "cmap_stack_pop(s->cur->val.arr)");
        }
        cmap_make_null(s->cur);
        s->cur--;
        if (cmap_obj_has_type(s->cur, CMAP_OBJ_STACK_TOP))
            return_error(gs_error_stackoverflow);
        if (cmap_obj_has_type(s->cur, CMAP_OBJ_STACK_BOTTOM))
            return_error(gs_error_stackunderflow);
    }
    return 0;
}

static inline int cmap_stack_push_arr_mark(cmap_stack_t *s)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_array_mark(s->cur);
    return 0;
}

static inline int cmap_stack_push_dict_mark(cmap_stack_t *s)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_dict_mark(s->cur);
    return 0;
}

static inline int cmap_stack_push_mark(cmap_stack_t *s)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_mark(s->cur);
    return 0;
}

static inline int cmap_stack_push_int(cmap_stack_t *s, int i)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_int(s->cur, i);
    return 0;
}

static inline int cmap_stack_push_float(cmap_stack_t *s, float f)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_float(s->cur, f);
    return 0;
}

static inline int cmap_stack_push_string(cmap_stack_t *s, byte *str, int len)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_string(s->cur, str, len);
    return 0;
}

static inline int cmap_stack_push_name(cmap_stack_t *s, byte *nm, int len)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_name(s->cur, nm, len);
    return 0;
}

static inline int cmap_stack_push_array(cmap_stack_t *s, cmap_stack_object_t *a, int len)
{
    int code = cmap_stack_push(s);
    if (code < 0) return code;

    cmap_make_array(s->cur, a, len);
    return 0;
}

static inline cmap_obj_type cmap_stack_obj_type(cmap_stack_t *s)
{
    return s->cur->type;
}

static inline int cmap_stack_count_to_mark(cmap_stack_t *s, cmap_obj_type mtype)
{
    int i = 0, depth = s->cur - &(s->stack[1]);
    for (i = 0; i < depth; i++) {
        if (s->cur->type == CMAP_OBJ_STACK_BOTTOM)
            return_error(gs_error_stackunderflow);
        if (s->cur[-i].type == mtype)
            break;
    }
    return i;
}

static inline int cmap_stack_count(cmap_stack_t *s)
{
    return s->cur - &(s->stack[1]);
}

static int null_oper_func(gs_memory_t *mem, cmap_stack_t *stack, pdf_cmap *pdficmap)
{
    return 0;
}

static int clear_stack_oper_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    int depth = s->cur - &(s->stack[1]);

    return cmap_stack_pop(s, depth);
}

static int pop_oper_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    return cmap_stack_pop(s, 1);
}

static int cmap_usecmap_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    pdf_name *n = NULL;
    pdf_cmap *upcmap = NULL;

    if (cmap_stack_count(s) < 1)
        return_error(gs_error_stackunderflow);

    /* If we've already got some definitions, ignore the usecmap op */
    if (pdficmap->code_space.num_ranges == 0) {
        byte *nstr = NULL;
        int code, len = s->cur[0].size;

        if (cmap_obj_has_type(&(s->cur[0]), CMAP_OBJ_NAME)) {
            nstr = s->cur[0].val.name;
        }
        else if (cmap_obj_has_type(&(s->cur[0]), CMAP_OBJ_STRING)) {
            nstr = s->cur[0].val.string;
        }
        code = pdfi_make_name(pdficmap->ctx, nstr, len, (pdf_obj **)&n);
        if (code >= 0) {
            code = pdfi_read_cmap(pdficmap->ctx, (pdf_obj *)n, &upcmap);
            if (code >= 0) {
                gx_code_space_range_t * ranges =
                         (gx_code_space_range_t *)gs_alloc_byte_array(mem, upcmap->code_space.num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_usecmap_func(ranges)");
                if (ranges != NULL) {
                    int i;
                    memcpy(&pdficmap->code_space, &upcmap->code_space, sizeof(pdficmap->code_space));
                    for (i = 0; i < upcmap->code_space.num_ranges; i++) {
                        memcpy(&(ranges[i]), &(upcmap->code_space.ranges[i]), sizeof(ranges[i]));
                    }
                    pdficmap->code_space.ranges = ranges;
                    memcpy(&pdficmap->cmap_range, &upcmap->cmap_range, sizeof(pdficmap->cmap_range));
                    memcpy(&pdficmap->notdef_cmap_range, &upcmap->notdef_cmap_range, sizeof(pdficmap->notdef_cmap_range));
                    /* Once we've assumed control of these, NULL out entries for the sub-cmap. */
                    upcmap->cmap_range.ranges = NULL;
                    upcmap->notdef_cmap_range.ranges = NULL;
                    /* But we keep the subcmap itself because we rely on its storage */
                    pdficmap->next = upcmap;
                    pdfi_countup(upcmap);
                }
            }
        }

    }
    pdfi_countdown(upcmap);
    pdfi_countdown(n);
    return cmap_stack_pop(s, 1);
}

#if 0 /* no longer used */
static int cmap_pushmark_func(gs_memory_t *mem, cmap_stack_t *stack, pdf_cmap *pdficmap)
{
    return cmap_stack_push_mark(stack);
}
#endif

static int cmap_pop_and_pushmark_func(gs_memory_t *mem, cmap_stack_t *stack, pdf_cmap *pdficmap)
{
    int code = cmap_stack_pop(stack, 1);
    if (code >= 0)
        code = cmap_stack_push_mark(stack);
    return code;
}

static int cmap_endcodespacerange_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    int i, numranges, to_pop = cmap_stack_count_to_mark(s, CMAP_OBJ_MARK);
    gx_code_space_t *code_space = &pdficmap->code_space;
    int nr = code_space->num_ranges;
    gx_code_space_range_t *gcsr = code_space->ranges;

    /* increment to_pop to cover the mark object */
    numranges = to_pop++;
    while (numranges % 2) numranges--;

    if (numranges > 0 && cmap_obj_has_type(&(s->cur[0]), CMAP_OBJ_STRING) &&
        cmap_obj_has_type(&(s->cur[-1]), CMAP_OBJ_STRING)) {

        code_space->num_ranges += numranges >> 1;

        code_space->ranges = (gx_code_space_range_t *)gs_alloc_byte_array(mem, code_space->num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_endcodespacerange_func(ranges)");
        if (nr > 0) {
            memcpy(code_space->ranges, gcsr, nr);
            gs_free_object(mem, gcsr, "cmap_endcodespacerange_func(gcsr");
        }

        for (i = nr; i < code_space->num_ranges; i++) {
            memcpy(code_space->ranges[i].first, s->cur[-((i * 2) + 1)].val.string, s->cur[-((i * 2) + 1)].size);
            memcpy(code_space->ranges[i].last, s->cur[-(i * 2)].val.string, s->cur[-(i * 2)].size);
            code_space->ranges[i].size = s->cur[-(i * 2)].size;
        }
    }
    return cmap_stack_pop(s, to_pop);
}

static int cmap_insert_map(pdfi_cmap_range_t *cmap_range, pdfi_cmap_range_map_t *pdfir)
{
    if (cmap_range->ranges == NULL) {
        cmap_range->ranges = cmap_range->ranges_tail = pdfir;
    }
    else {
        cmap_range->ranges_tail->next = pdfir;
        cmap_range->ranges_tail = pdfir;
    }
    cmap_range->numrangemaps++;
    return 0;
}

static int general_endcidrange_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap, pdfi_cmap_range_t *cmap_range)
{
    int ncodemaps, to_pop = cmap_stack_count_to_mark(s, CMAP_OBJ_MARK);
    int i, j;
    pdfi_cmap_range_map_t *pdfir;
    cmap_stack_object_t *stobj;

    /* increment to_pop to cover the mark object */
    ncodemaps = to_pop++;
    /* mapping should have 3 objects on the stack:
     * startcode, endcode and basecid
     */
    while (ncodemaps % 3) ncodemaps--;

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 3) {
        int preflen, valuelen;

        if (cmap_obj_has_type(&(stobj[i + 2]), CMAP_OBJ_INTEGER)
        &&  cmap_obj_has_type(&(stobj[i + 1]), CMAP_OBJ_STRING)
        &&  cmap_obj_has_type(&(stobj[i]), CMAP_OBJ_STRING)){
            uint cidbase = stobj[i + 2].val.i;

            /* First, find the length of the prefix */
            for (preflen = 0; preflen < stobj[i].size; preflen++) {
                if(stobj[i].val.string[preflen] != stobj[i + 1].val.string[preflen]) {
                    break;
                }
            }

            if (preflen == stobj[i].size) {
                preflen = 1;
            }

            /* Find how many bytes we need for the cidbase value */
            /* We always store at least two bytes for the cidbase value */
            for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                DO_NOTHING;

            valuelen = ((valuelen + 7) & ~7) >> 3;

            /* The prefix is already directly in the gx_cmap_lookup_range_t
             * We need to store the lower and upper character codes, after lopping the prefix
             * off them. The upper and lower codes must be the same number of bytes.
             */
            j = sizeof(pdfi_cmap_range_map_t) + 2 * (stobj[i].size - preflen) + valuelen;

            pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
            if (pdfir != NULL) {
                gx_cmap_lookup_range_t *gxr = &pdfir->range;
                pdfir->next = NULL;
                gxr->num_entries = 1;
                gxr->keys.data = (byte *)&(pdfir[1]);
                gxr->values.data = gxr->keys.data + 2 * (stobj[i].size - preflen);

                gxr->cmap = NULL;
                gxr->font_index = 0;
                gxr->key_is_range = true;
                gxr->value_type = cmap_range == &(pdficmap->cmap_range) ? CODE_VALUE_CID : CODE_VALUE_NOTDEF;
                gxr->key_prefix_size = preflen;
                gxr->key_size = stobj[i].size - gxr->key_prefix_size;
                memcpy(gxr->key_prefix, stobj[i].val.string, gxr->key_prefix_size);

                memcpy(gxr->keys.data, stobj[i].val.string + gxr->key_prefix_size, stobj[i].size - gxr->key_prefix_size);
                memcpy(gxr->keys.data + (stobj[i].size - gxr->key_prefix_size), stobj[i + 1].val.string + gxr->key_prefix_size, stobj[i + 1].size - gxr->key_prefix_size);

                gxr->keys.size = (stobj[i].size - gxr->key_prefix_size) + (stobj[i + 1].size - gxr->key_prefix_size);
                for (j = 0; j < valuelen; j++) {
                    gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                }
                gxr->value_size = valuelen; /* I'm not sure.... */
                gxr->values.size = valuelen;
                if (cmap_insert_map(cmap_range, pdfir) < 0) break;
            }
            else {
                break;
            }
        }
    }
    return cmap_stack_pop(s, to_pop);
}

static int cmap_endcidrange_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    return general_endcidrange_func(mem, s, pdficmap, &pdficmap->cmap_range);
}

static int cmap_endnotdefrange_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    return general_endcidrange_func(mem, s, pdficmap, &pdficmap->notdef_cmap_range);
}

static int general_endcidchar_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap, pdfi_cmap_range_t *cmap_range)
{
    int ncodemaps, to_pop = cmap_stack_count_to_mark(s, CMAP_OBJ_MARK);
    int i, j;
    pdfi_cmap_range_map_t *pdfir;
    cmap_stack_object_t *stobj;

    /* increment to_pop to cover the mark object */
    ncodemaps = to_pop++;
    /* mapping should have 2 objects on the stack:
     * startcode, endcode and basecid
     */
    while (ncodemaps % 2) ncodemaps--;

    stobj = &s->cur[-ncodemaps] + 1;

    for (i = 0; i < ncodemaps; i += 2) {
        int preflen = 1, valuelen;

        if (cmap_obj_has_type(&(stobj[i + 1]), CMAP_OBJ_INTEGER)
        &&  cmap_obj_has_type(&(stobj[i]), CMAP_OBJ_STRING)) {
            uint cidbase = stobj[i + 1].val.i;

            /* Find how many bytes we need for the cidbase value */
            /* We always store at least two bytes for the cidbase value */
            for (valuelen = 16; valuelen < 32 && (cidbase >> valuelen) > 0; valuelen += 1)
                DO_NOTHING;

            preflen = stobj[i].size > 4 ? 4 : stobj[i].size;

            valuelen = ((valuelen + 7) & ~7) >> 3;

            /* The prefix is already directly in the gx_cmap_lookup_range_t
             * We need to store the lower and upper character codes, after lopping the prefix
             * off them. The upper and lower codes must be the same number of bytes.
             */
            j = sizeof(pdfi_cmap_range_map_t) + (stobj[i].size - preflen) + valuelen;

            pdfir = (pdfi_cmap_range_map_t *)gs_alloc_bytes(mem, j, "cmap_endcidrange_func(pdfi_cmap_range_map_t)");
            if (pdfir != NULL) {
                gx_cmap_lookup_range_t *gxr = &pdfir->range;
                pdfir->next = NULL;
                gxr->num_entries = 1;
                gxr->keys.data = (byte *)&(pdfir[1]);
                gxr->values.data = gxr->keys.data + (stobj[i].size - preflen);

                gxr->cmap = NULL;
                gxr->font_index = 0;
                gxr->key_is_range = false;
                gxr->value_type = cmap_range == &(pdficmap->cmap_range) ? CODE_VALUE_CID : CODE_VALUE_NOTDEF;
                gxr->key_prefix_size = preflen;
                gxr->key_size = stobj[i].size - gxr->key_prefix_size;
                memcpy(gxr->key_prefix, stobj[i].val.string, gxr->key_prefix_size);

                memcpy(gxr->keys.data, stobj[i].val.string + gxr->key_prefix_size, stobj[i].size - gxr->key_prefix_size);

                gxr->keys.size = stobj[i].size - gxr->key_prefix_size;
                for (j = 0; j < valuelen; j++) {
                    gxr->values.data[j] = (cidbase >> ((valuelen - 1 - j) * 8)) & 255;
                }
                gxr->value_size = valuelen; /* I'm not sure.... */
                gxr->values.size = valuelen;
                if (cmap_insert_map(cmap_range, pdfir) < 0) break;
            }
            else {
                break;
            }
        }
    }
    return cmap_stack_pop(s, to_pop);
}

static int cmap_endcidchar_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    return general_endcidchar_func(mem, s, pdficmap, &pdficmap->cmap_range);
}

static int cmap_endnotdefchar_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    return general_endcidchar_func(mem, s, pdficmap, &pdficmap->notdef_cmap_range);
}


#define CMAP_NAME_AND_LEN(s) (const byte *)s, sizeof(s) - 1

static int cmap_def_func(gs_memory_t *mem, cmap_stack_t *s, pdf_cmap *pdficmap)
{
    int code = 0, code2 = 0;

    if (cmap_stack_count(s) < 2) {
        return cmap_stack_pop(s, 1);
    }

    if (cmap_obj_has_type(&s->cur[-1], CMAP_OBJ_NAME)) {
        if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Registry"))) {
            pdficmap->csi_reg.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(Registry)");
            if (pdficmap->csi_reg.data != NULL) {
                pdficmap->csi_reg.size = s->cur[0].size;
                if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_STRING)) {
                    memcpy(pdficmap->csi_reg.data, s->cur[0].val.string, s->cur[0].size);
                }
                else {
                    memcpy(pdficmap->csi_reg.data, s->cur[0].val.name, s->cur[0].size);
                }
                pdficmap->csi_reg.data[pdficmap->csi_reg.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Ordering"))) {
            pdficmap->csi_ord.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(Ordering)");
            if (pdficmap->csi_ord.data != NULL) {
                pdficmap->csi_ord.size = s->cur[0].size;
                if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_STRING))
                    memcpy(pdficmap->csi_ord.data, s->cur[0].val.string, s->cur[0].size);
                else
                    memcpy(pdficmap->csi_ord.data, s->cur[0].val.name, s->cur[0].size);
                pdficmap->csi_ord.data[pdficmap->csi_ord.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("Supplement"))) {
            if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_INTEGER)) {
                pdficmap->csi_supplement = s->cur[0].val.i;
            }
            else {
                pdficmap->csi_supplement = 0;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapName"))) {
            pdficmap->name.data = gs_alloc_bytes(mem, s->cur[0].size + 1, "cmap_def_func(CMapName)");
            if (pdficmap->name.data != NULL) {
                pdficmap->name.size = s->cur[0].size;
                if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_STRING))
                    memcpy(pdficmap->name.data, s->cur[0].val.string, s->cur[0].size);
                else
                    memcpy(pdficmap->name.data, s->cur[0].val.name, s->cur[0].size);
                pdficmap->name.data[pdficmap->name.size] = '\0';
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapVersion"))) {
            if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_INTEGER)) {
                pdficmap->vers = (float)s->cur[0].val.i;
            }
            else if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_FLOAT)){
                pdficmap->vers = s->cur[0].val.f;
            }
            else {
                pdficmap->vers = (float)0;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("CMapType"))) {
            if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_INTEGER)) {
                pdficmap->cmaptype = s->cur[0].val.i;
            }
            else {
                pdficmap->type = 1;
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("XUID"))) {
            if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_ARRAY)) {
                int len = s->cur->size;
                pdficmap->uid.xvalues = (long *)gs_alloc_bytes(mem, len * sizeof(*pdficmap->uid.xvalues), "cmap_def_func(XUID)");
                if (pdficmap->uid.xvalues != NULL) {
                     int i;
                     cmap_stack_object_t *a = s->cur->val.arr;
                     pdficmap->uid.id = -len;
                     for (i = 0; i < len; i++) {
                         if (cmap_obj_has_type(&a[i], CMAP_OBJ_INTEGER)) {
                             pdficmap->uid.xvalues[i] = (long)a[i].val.i;
                         }
                         else {
                             pdficmap->uid.xvalues[i] = 0;
                         }
                     }
                }
                else {
                    code = gs_note_error(gs_error_VMerror);
                }
            }
        }
        else if (!memcmp(s->cur[-1].val.name, CMAP_NAME_AND_LEN("WMode"))) {
            if (cmap_obj_has_type(&s->cur[0], CMAP_OBJ_INTEGER)) {
                pdficmap->wmode = s->cur[0].val.i;
            }
            else {
                pdficmap->wmode = 0;
            }
        }
    }

    code2 = cmap_stack_pop(s, 2);
    if (code < 0)
        return code;
    else
        return code2;
}

#define CMAP_OPER_NAME_LEN(s) (const byte *)s, sizeof(s) - 1

typedef struct {
  const byte *opname;
  const int opnamelen;
  int (*oper)(gs_memory_t *mem, cmap_stack_t *stack, pdf_cmap *pdficmap);
} cmap_oper_list_t;

static const cmap_oper_list_t cmap_oper_list[] =
{
  {CMAP_OPER_NAME_LEN("usecmap"), cmap_usecmap_func},
  {CMAP_OPER_NAME_LEN("usefont"), null_oper_func},
  {CMAP_OPER_NAME_LEN("beginusematrix"), null_oper_func},
  {CMAP_OPER_NAME_LEN("endusematrix"), null_oper_func},
  {CMAP_OPER_NAME_LEN("begincodespacerange"), cmap_pop_and_pushmark_func},
  {CMAP_OPER_NAME_LEN("endcodespacerange"), cmap_endcodespacerange_func},
  {CMAP_OPER_NAME_LEN("begincmap"), null_oper_func},
  {CMAP_OPER_NAME_LEN("beginbfchar"), null_oper_func},
  {CMAP_OPER_NAME_LEN("endbfchar"), null_oper_func},
  {CMAP_OPER_NAME_LEN("beginbfrange"), null_oper_func},
  {CMAP_OPER_NAME_LEN("endbfrange"), null_oper_func},
  {CMAP_OPER_NAME_LEN("begincidchar"), cmap_pop_and_pushmark_func},
  {CMAP_OPER_NAME_LEN("endcidchar"), cmap_endcidchar_func},
  {CMAP_OPER_NAME_LEN("begincidrange"), cmap_pop_and_pushmark_func},
  {CMAP_OPER_NAME_LEN("endcidrange"), cmap_endcidrange_func},
  {CMAP_OPER_NAME_LEN("beginnotdefchar"), cmap_pop_and_pushmark_func},
  {CMAP_OPER_NAME_LEN("endnotdefchar"), cmap_endnotdefchar_func},
  {CMAP_OPER_NAME_LEN("beginnotdefrange"), cmap_pop_and_pushmark_func},
  {CMAP_OPER_NAME_LEN("endnotdefrange"), cmap_endnotdefrange_func},
  {CMAP_OPER_NAME_LEN("findresource"), clear_stack_oper_func},
  {CMAP_OPER_NAME_LEN("dict"), pop_oper_func},
  {CMAP_OPER_NAME_LEN("begin"), null_oper_func},
  {CMAP_OPER_NAME_LEN("end"), null_oper_func},
  {CMAP_OPER_NAME_LEN("pop"), null_oper_func},
  {CMAP_OPER_NAME_LEN("def"), cmap_def_func},
  {CMAP_OPER_NAME_LEN("dup"), null_oper_func},
  {CMAP_OPER_NAME_LEN("defineresource"), clear_stack_oper_func},
  {CMAP_OPER_NAME_LEN("beginrearrangedfont"), clear_stack_oper_func}, /* we should never see this */
  {NULL, 0, NULL}
};

static int
pdf_cmap_open_file(pdf_context *ctx, gs_string *cmap_name, byte **buf, int64_t *buflen)
{
    int code = 0;
    /* FIXME: romfs hardcoded coded for now */
    stream *s;
    char fname[gp_file_name_sizeof];
    const char *path_pfx = "%rom%Resource/CMap/";
    fname[0] = '\0';

    strncat(fname, path_pfx, strlen(path_pfx));
    strncat(fname, (char *)cmap_name->data, cmap_name->size);
    s = sfopen(fname, "r", ctx->memory);
    if (s == NULL) {
        code = gs_note_error(gs_error_undefinedfilename);
    }
    else {
        sfseek(s, 0, SEEK_END);
        *buflen = sftell(s);
        sfseek(s, 0, SEEK_SET);
        *buf = gs_alloc_bytes(ctx->memory, *buflen, "pdf_cmap_open_file(buf)");
        if (*buf != NULL) {
            sfread(*buf, 1, *buflen, s);
        }
        else {
            code = gs_note_error(gs_error_VMerror);
        }
        sfclose(s);
    }
    return code;
}

static inline int
cmap_is_whitespace(int c)
{
    return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA);
}


static inline int
cmap_end_object(int c)
{
    return cmap_is_whitespace(c) || (c == '/') || (c == '[') || (c == ']') || (c == '(') || (c == '<');
}

static inline int
cmap_end_number_object(int c)
{
    return (c != '.' && (c < '0' || c > '9'));
}

static inline bool ishex(char c)
{
    if (c < 0x30)
        return false;

    if (c > 0x39) {
        if (c > 'F') {
            if (c < 'a')
                return false;
            if (c > 'f')
                return false;
            return true;
        } else {
            if (c < 'A')
                return false;
            return true;
        }
    } else
        return true;
}

static inline int decodehex(char c)
{
    int retval = 0;
    if (ishex(c)) {
        if (c > 0x39) {
            if (c > 'F') {
                retval = c - 0x57;
            } else {
                retval = c - 0x37;
            }
        } else {
            retval = c - 0x30;
        }
    }
    return retval;
}


static int
pdfi_interpret_cmap(gs_memory_t *mem, byte *cmapbuf, int64_t buflen, pdf_cmap *pdficmap)
{
    int code = 0;
    byte *buflim = cmapbuf + buflen;
    cmap_stack_t cmap_stack, *cs = &cmap_stack;

    cmap_stack_init(mem, cs);

    while (cmapbuf < buflim && code >= 0) {
        switch (*cmapbuf++) {
            case '%': /* Comment */
              {
                while (cmapbuf++ < buflim && *cmapbuf != char_EOL && *cmapbuf != '\f'
                       && *cmapbuf != char_CR)
                  ;
                  if (*cmapbuf == char_EOL) cmapbuf++;
              }
              break;
            case '/': /* name */
              {
                  byte *n = cmapbuf;
                  int len;
                  while (cmapbuf++ < buflim && !cmap_end_object((int)*cmapbuf))
                    ;
                  len = cmapbuf - n;
                  code = cmap_stack_push_name(cs, n, len);
              }
              break;
            case '(': /* string */
              {
                  byte *s = cmapbuf;
                  int len;
                  while (cmapbuf++ < buflim && *cmapbuf != ')')
                    ;
                  len = cmapbuf - s;
                  cmapbuf++; /* move past the trailing ')' */
                  code = cmap_stack_push_string(cs, s, len);
              }
              break;
            case '<': /* hex string */
              {
                  byte *s = cmapbuf;
                  byte *s2 = s;
                  int len, i;
                  byte hbuf[2];
                  while (cmapbuf++ < buflim && *cmapbuf != '>')
                    ;
                  len = cmapbuf - s;
                  while (len % 2) len--;
                  for (i = 0; i < len; i += 2) {
                      hbuf[0] = s[i];
                      hbuf[1] = s[i + 1];
                      *s2++ = (decodehex(hbuf[0]) << 4) | decodehex(hbuf[1]);
                  }
                  cmapbuf++; /* move past the trailing '>' */
                  code = cmap_stack_push_string(cs, s, len >> 1);
              }
              break;
             case '[': ;/* begin array */
              code = cmap_stack_push_arr_mark(cs);
              break;
             case ']': /* end array */
               {
                   cmap_stack_object_t *arr = NULL;
                   int i, size = cmap_stack_count_to_mark(cs, CMAP_OBJ_ARR_MARK);
                   if (size > 0) {
                       arr = (cmap_stack_object_t *)gs_alloc_bytes(mem, size * sizeof(cmap_stack_object_t), "pdfi_interpret_cmap(cmap_stack_object_t");
                       if (arr == NULL) {
                           code = gs_note_error(gs_error_VMerror);
                           /* clean up the stack, including the mark object */
                           (void)cmap_stack_pop(cs, size + 1);
                           size = 0;
                       }
                       else {
                           for (i = 0; i < size; i++) {
                               memcpy(&(arr[(size - 1) - i]), cs->cur, sizeof(*cs->cur));
                               (void)cmap_stack_pop(cs, 1);
                           }
                           /* And pop the array mark */
                           (void)cmap_stack_pop(cs, 1);
                       }
                   }
                   code = cmap_stack_push_array(cs, arr, size > 0 ? size : 0);
               }
               break;
             case '-':
             case '+':
             case '0':
             case '1':
             case '2':
             case '3':
             case '4':
             case '5':
             case '6':
             case '7':
             case '8':
             case '9':
               {
                 bool is_float = false;
                 int len;
                 byte *n = --cmapbuf, *numbuf;

                 while (cmapbuf++ < buflim && !cmap_end_number_object((int)*cmapbuf)) {
                   if (*cmapbuf == '.') is_float = true;
                 }
                 len = cmapbuf - n;
                 numbuf = gs_alloc_bytes(mem, len + 1, "cmap number buffer");
                 if (numbuf == NULL) {
                   code = gs_note_error(gs_error_VMerror);
                 }
                 else {
                   memcpy(numbuf, n, len);
                   numbuf[len] = '\0';
                   if (is_float) {
                     float f = (float)atof((const char *)numbuf);
                     code = cmap_stack_push_float(cs, f);
                   }
                   else {
                     int i = atoi((const char *)numbuf);
                     code = cmap_stack_push_int(cs, i);
                   }
                   gs_free_object(mem, numbuf, "cmap number buffer");
                 }
               }
               break;
             case ' ':
             case '\f':
             case '\t':
             case char_CR:
             case char_EOL:
             case char_NULL:
               break;
             default:
               {
                 byte *n = --cmapbuf;
                 int len, i;
                 int (*opfunc)(gs_memory_t *mem, cmap_stack_t *stack, pdf_cmap *pdficmap) = NULL;

                 while (cmapbuf++ < buflim && !cmap_end_object((int)*cmapbuf))
                   ;
                 len = cmapbuf - n;
                 for (i = 0; cmap_oper_list[i].opname != NULL; i++) {
                     if (len == cmap_oper_list[i].opnamelen
                         && !memcmp(n, cmap_oper_list[i].opname, len)) {
                         opfunc = cmap_oper_list[i].oper;
                         break;
                     }
                 }
                 if (opfunc)
                     code = (*opfunc)(mem, cs, pdficmap);
               }
               break;
        }
    }

    return code;
}

static int
pdfi_make_gs_cmap(gs_memory_t *mem, pdf_cmap *pdficmap)
{
    int code = 0, i;
    gs_cmap_adobe1_t *pgscmap = 0;
    gx_cmap_lookup_range_t *lookups, *ndlookups = NULL;
    pdfi_cmap_range_map_t *l;
    /* FIXME: We have to use gs_cmap_adobe1_alloc() to get the cmap procs
       but even if it gets passed num_ranges = 0 it still allocates
       a zero length array. Change gs_cmap_adobe1_alloc to work better
     */
    if ((code = gs_cmap_adobe1_alloc(&pgscmap, pdficmap->wmode,
                                     pdficmap->name.data,
                                     pdficmap->name.size,
                                     1,
                                     0, 0, 0, 0, 0, mem)) >= 0) {
        gs_free_object(mem, pgscmap->code_space.ranges, "empty ranges");

        lookups = gs_alloc_struct_array(mem, pdficmap->cmap_range.numrangemaps,
                               gx_cmap_lookup_range_t,
                               &st_cmap_lookup_range_element,
                               "pdfi_make_gs_cmap(lookup ranges)");
        if (lookups == NULL) {
            gs_free_object(mem, pgscmap, "pdfi_make_gs_cmap(pgscmap)");
            code = gs_note_error(gs_error_VMerror);
            goto done;
        }
        if (pdficmap->notdef_cmap_range.numrangemaps > 0){
            ndlookups = gs_alloc_struct_array(mem, pdficmap->notdef_cmap_range.numrangemaps,
                               gx_cmap_lookup_range_t,
                               &st_cmap_lookup_range_element,
                               "pdfi_make_gs_cmap(notdef lookup ranges)");
            if (ndlookups == NULL) {
                gs_free_object(mem, lookups, "pdfi_make_gs_cmap(lookups)");
                gs_free_object(mem, pgscmap, "pdfi_make_gs_cmap(pgscmap)");
                code = gs_note_error(gs_error_VMerror);
                goto done;
            }
        }
        pgscmap->def.lookup = lookups;
        pgscmap->def.num_lookup = pdficmap->cmap_range.numrangemaps;
        pgscmap->notdef.lookup = ndlookups;
        pgscmap->notdef.num_lookup = pdficmap->notdef_cmap_range.numrangemaps;

        pgscmap->CIDSystemInfo[0].Registry.data = pdficmap->csi_reg.data;
        pgscmap->CIDSystemInfo[0].Registry.size = pdficmap->csi_reg.size;
        pgscmap->CIDSystemInfo[0].Ordering.data = pdficmap->csi_ord.data;
        pgscmap->CIDSystemInfo[0].Ordering.size = pdficmap->csi_ord.size;
        pgscmap->CIDSystemInfo[0].Supplement = pdficmap->csi_supplement;
        memcpy(&pgscmap->code_space, &pdficmap->code_space, sizeof(pgscmap->code_space));
        memcpy(&pgscmap->uid, &pdficmap->uid, sizeof(pdficmap->uid));
        l = pdficmap->cmap_range.ranges;
        for (i = 0; i < pdficmap->cmap_range.numrangemaps && l != NULL; i++) {
            memcpy(&lookups[i], &l->range, sizeof(gx_cmap_lookup_range_t));
            l = l->next;
        }

        l = pdficmap->notdef_cmap_range.ranges;
        for (i = 0; i < pdficmap->notdef_cmap_range.numrangemaps && l != NULL; i++) {
            memcpy(&ndlookups[i], &l->range, sizeof(gx_cmap_lookup_range_t));
            l = l->next;
        }

        pdficmap->gscmap = pgscmap;
    }

done:
    return code;
}

int
pdfi_read_cmap(pdf_context *ctx, pdf_obj *cmap, pdf_cmap **pcmap)
{
    int code = 0;
    pdf_cmap pdficm[3] = {0};
    pdf_cmap *pdfi_cmap = &(pdficm[1]);
    stream *cmap_str = NULL;
    byte *buf = NULL;
    int64_t buflen;

    if (cmap->type == PDF_NAME) {
        gs_string cmname;
        pdf_name *cmapn = (pdf_name *)cmap;
        cmname.data = cmapn->data;
        cmname.size = cmapn->length;
        code = pdf_cmap_open_file(ctx, &cmname, &buf, &buflen);
        if (code < 0)
            goto error_out;
    }
    else {
        if (cmap->type == PDF_DICT) {
            pdf_obj *ucmap;
            pdf_cmap *upcmap = NULL;

            code = pdfi_dict_knownget(ctx, (pdf_dict *)cmap, "UseCMap", &ucmap);
            if (code > 0) {
                code = pdfi_read_cmap(ctx, ucmap, &upcmap);
                if (code >= 0) {
                    gx_code_space_range_t * ranges =
                         (gx_code_space_range_t *)gs_alloc_byte_array(ctx->memory, upcmap->code_space.num_ranges,
                          sizeof(gx_code_space_range_t), "cmap_usecmap_func(ranges)");
                    if (ranges != NULL) {
                        int i;
                        memcpy(&pdfi_cmap->code_space, &upcmap->code_space, sizeof(pdfi_cmap->code_space));
                        for (i = 0; i < upcmap->code_space.num_ranges; i++) {
                            memcpy(&(ranges[i]), &(upcmap->code_space.ranges[i]), sizeof(ranges[i]));
                        }
                        pdfi_cmap->code_space.ranges = ranges;
                        memcpy(&pdfi_cmap->cmap_range, &upcmap->cmap_range, sizeof(pdfi_cmap->cmap_range));
                        memcpy(&pdfi_cmap->notdef_cmap_range, &upcmap->notdef_cmap_range, sizeof(pdfi_cmap->notdef_cmap_range));
                        /* Once we've assumed control of these, NULL out entries for the sub-cmap. */
                        upcmap->cmap_range.ranges = NULL;
                        upcmap->notdef_cmap_range.ranges = NULL;
                        /* But we keep the subcmap itself because we rely on its storage */
                        pdfi_cmap->next = upcmap;
                    }
                }
                else {
                    pdfi_countdown(upcmap);
                }
            }

            code = pdfi_stream_to_buffer(ctx, (pdf_dict *)cmap, &buf, &buflen);
            if (code < 0) {
                goto error_out;
            }
        }
        else {
          code = gs_note_error(gs_error_typecheck);
          goto error_out;
        }
    }
    pdfi_cmap->ctx = ctx;
    pdfi_cmap->buf = buf;
    pdfi_cmap->buflen = buflen;

    /* In case of technically invalid CMap files which do not contain a CMapType, See Bug #690737.
     * This makes sure we clean up the CMap contents in pdfi_free_cmap() below.
     */
    pdfi_cmap->cmaptype = 1;

    code = pdfi_interpret_cmap(ctx->memory, buf, buflen, pdfi_cmap);
    if (code < 0) goto error_out;

    code = pdfi_make_gs_cmap(ctx->memory, pdfi_cmap);

    if (code >= 0) {
        *pcmap = (pdf_cmap *)gs_alloc_bytes(ctx->memory, sizeof(pdf_cmap), "pdfi_read_cmap(*pcmap)");
        if (*pcmap != NULL) {
            pdfi_cmap->type = PDF_CMAP;
            pdfi_cmap->ctx = ctx;
            pdfi_cmap->refcnt = 1;
            pdfi_cmap->object_num = cmap->object_num;
            pdfi_cmap->generation_num = cmap->generation_num;
            pdfi_cmap->indirect_num = cmap->indirect_num;
            pdfi_cmap->indirect_gen = cmap->indirect_gen;
            memcpy(*pcmap, pdfi_cmap, sizeof(pdf_cmap));
            pdfi_cmap = *pcmap;
            /* object_num can be zero if the dictionary was defined inline */
            if (pdfi_cmap->object_num != 0) {
                code = replace_cache_entry(ctx, (pdf_obj *)pdfi_cmap);
            }
        }
    }

error_out:
    if (cmap_str)
        sfclose(cmap_str);
    return code;
}

int pdfi_free_cmap(pdf_obj *cmapo)
{
    pdf_cmap *cmap = (pdf_cmap *)cmapo;
    if (cmap->cmaptype == 1) {
        pdfi_cmap_range_map_t *pdfir;
        gs_cmap_adobe1_t *pgscmap = cmap->gscmap;
        gs_free_object(OBJ_MEMORY(cmap), pgscmap->def.lookup, "pdfi_free_cmap(def.lookup)");
        gs_free_object(OBJ_MEMORY(cmap), pgscmap->notdef.lookup, "pdfi_free_cmap(notdef.lookup)");
        (void)gs_cmap_free((gs_cmap_t *)pgscmap, OBJ_MEMORY(cmap));
        gs_free_object(OBJ_MEMORY(cmap), cmap->code_space.ranges, "pdfi_free_cmap(code_space.ranges");
        pdfir = cmap->cmap_range.ranges;
        while (pdfir != NULL) {
            pdfi_cmap_range_map_t *pdfir2 = pdfir->next;
            gs_free_object(OBJ_MEMORY(cmap), pdfir, "pdfi_free_cmap(cmap_range.ranges");
            pdfir = pdfir2;
        }
        pdfir = cmap->notdef_cmap_range.ranges;
        while (pdfir != NULL) {
            pdfi_cmap_range_map_t *pdfir2 = pdfir->next;
            gs_free_object(OBJ_MEMORY(cmap), pdfir, "pdfi_free_cmap(cmap_range.ranges");
            pdfir = pdfir2;
        }
        gs_free_object(OBJ_MEMORY(cmap), cmap->csi_reg.data, "pdfi_free_cmap(csi_reg.data");
        gs_free_object(OBJ_MEMORY(cmap), cmap->csi_ord.data, "pdfi_free_cmap(csi_ord.data");
        gs_free_object(OBJ_MEMORY(cmap), cmap->name.data, "pdfi_free_cmap(name.data");
        gs_free_object(OBJ_MEMORY(cmap), cmap->uid.xvalues, "pdfi_free_cmap(xuid.xvalues");
        pdfi_countdown(cmap->next);
        gs_free_object(OBJ_MEMORY(cmap), cmap->buf, "pdfi_free_cmap(cmap->buf");
        gs_free_object(OBJ_MEMORY(cmap), cmap, "pdfi_free_cmap(cmap");
    }

    return 0;
}
