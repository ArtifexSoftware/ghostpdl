/* Copyright (C) 2020-2025 Artifex Software, Inc.
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
#include "scanchar.h"
#include "sfilter.h"
#include "stream.h"
#include "strimpl.h"

#include "pdf_int.h"
#include "pdf_types.h"
#include "pdf_array.h"
#include "pdf_dict.h"
#include "pdf_font.h"
#include "pdf_font_types.h"
#include "pdf_fontps.h"

static const char *const notdefnamestr = ".notdef";

int
pdfi_pscript_stack_init(pdf_context *pdfi_ctx, pdf_ps_oper_list_t *ops, void *client_data,
                        pdf_ps_ctx_t *s)
{
    int i, size = PDF_PS_STACK_SIZE;
    int initsizebytes = sizeof(pdf_ps_stack_object_t) * PDF_PS_STACK_SIZE;
    s->pdfi_ctx = pdfi_ctx;
    s->ops = ops;
    s->client_data = client_data;

    s->stack = (pdf_ps_stack_object_t *)gs_alloc_bytes(pdfi_ctx->memory, initsizebytes, "pdfi_pscript_stack_init(stack)");
    if (s->stack == NULL)
        return_error(gs_error_VMerror);

    s->cur = s->stack + 1;
    s->toplim = s->cur + size;

    for (i = 0; i < PDF_PS_STACK_GUARDS; i++)
        s->stack[i].type = PDF_PS_OBJ_STACK_BOTTOM;

    for (i = 0; i < PDF_PS_STACK_GUARDS; i++)
        s->stack[size - 1 + i].type = PDF_PS_OBJ_STACK_TOP;

    for (i = 0; i < size - 1; i++) {
        pdf_ps_make_null(&(s->cur[i]));
    }
    return 0;
}

void
pdfi_pscript_stack_finit(pdf_ps_ctx_t *s)
{
    int stackdepth;

    if ((stackdepth = pdf_ps_stack_count(s)) > 0) {
        pdf_ps_stack_pop(s, stackdepth);
    }
    gs_free_object(s->pdfi_ctx->memory, s->stack, "pdfi_pscript_stack_finit(stack)");
}

int
ps_pdf_null_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend)
{
    return 0;
}

int
clear_stack_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    int depth = s->cur - &(s->stack[1]);

    return pdf_ps_stack_pop(s, depth);
}

int
pdf_ps_pop_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    return pdf_ps_stack_pop(s, 1);
}

int
pdf_ps_pop_and_pushmark_func(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend)
{
    int code = pdf_ps_stack_pop(stack, 1);

    if (code >= 0)
        code = pdf_ps_stack_push_mark(stack);
    return code;
}

static inline int
pdf_ps_is_whitespace(int c)
{
    return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA);
}

static inline int
pdf_ps_end_object(int c)
{
    return pdf_ps_is_whitespace(c) || (c == '/') || (c == '[') || (c == ']') || c == '{' || c == '}' || (c == '(') || (c == '<');
}

static inline int
pdf_ps_end_number_object(int c)
{
    return (c != '.' && c != 'e' && c != '-' && (c < '0' || c > '9'));
}

int
pdfi_pscript_interpret(pdf_ps_ctx_t *cs, byte *pdfpsbuf, int64_t buflen)
{
    int code = 0;
    byte *buflim = pdfpsbuf + buflen;
    int arraydepth = 0;
    int stackdepth;

    while (pdfpsbuf < buflim && code >= 0) {
        switch (*pdfpsbuf++) {
            case '%':          /* Comment */
                {
                    while (pdfpsbuf < buflim && *pdfpsbuf != char_EOL && *pdfpsbuf != '\f' &&
                           *pdfpsbuf != char_CR)
                        pdfpsbuf++;

                    if (pdfpsbuf < buflim && *pdfpsbuf == char_EOL)
                        pdfpsbuf++;
                }
                break;
            case '/':          /* name */
                {
                    byte *n = pdfpsbuf;
                    int len;

                    while (pdfpsbuf < buflim && !pdf_ps_end_object((int)*pdfpsbuf))
                        pdfpsbuf++;
                    len = pdfpsbuf - n;
                    code = pdf_ps_stack_push_name(cs, n, len);
                } break;
            case '(':          /* string */
                {
                    byte *s = pdfpsbuf;
                    int len;
                    int depth = 1;

                    while (pdfpsbuf < buflim && depth > 0) {
                        if (*pdfpsbuf == '(') {
                            depth++;
                        }
                        else if (*pdfpsbuf == ')') {
                            depth--;
                        }
                        pdfpsbuf++;
                    }
                    len = (pdfpsbuf - s) - 1;
                    code = pdf_ps_stack_push_string(cs, s, len);
                }
                break;
            case '<':          /* hex string */
                {
                    byte *s = pdfpsbuf;
                    stream_cursor_read pr;
                    stream_cursor_write pw;
                    int odd_digit = -1;

                    if (pdfpsbuf < buflim && *pdfpsbuf == '<') { /* Dict opening "<<" - we don't care */
                        pdfpsbuf++;
                        continue;
                    }
                    while (pdfpsbuf < buflim && *pdfpsbuf != '>')
                        pdfpsbuf++;

                    pr.ptr = s - 1;
                    pr.limit = pdfpsbuf - 1;
                    pw.ptr = s - 1;
                    pw.limit = pdfpsbuf - 1;
                    code = s_hex_process(&pr, &pw, &odd_digit, hex_ignore_garbage);
                    if (code != ERRC && pw.ptr - (s - 1) > 0) {
                        code = pdf_ps_stack_push_string(cs, s, pw.ptr - (s - 1));
                    }
                }
                break;
            case '>': /* For hex strings, this should be handled above */
                {
                    if (pdfpsbuf < buflim && *pdfpsbuf == '>') { /* Dict closing "<<" - we still don't care */
                        pdfpsbuf++;
                    }
                }
               break;
            case '[':;         /* begin array */
            case '{':;         /* begin executable array (mainly, FontBBox) */
                arraydepth++;
                code = pdf_ps_stack_push_arr_mark(cs);
                break;
            case ']':          /* end array */
            case '}':          /* end executable array */
                {
                    pdf_ps_stack_object_t *arr = NULL;
                    int i, size = pdf_ps_stack_count_to_mark(cs, PDF_PS_OBJ_ARR_MARK);

                    if (size > 0 && arraydepth > 0) {
                        arr = (pdf_ps_stack_object_t *) gs_alloc_bytes(cs->pdfi_ctx->memory, size * sizeof(pdf_ps_stack_object_t), "pdfi_pscript_interpret(pdf_ps_stack_object_t");
                        if (arr == NULL) {
                            code = gs_note_error(gs_error_VMerror);
                            /* clean up the stack, including the mark object */
                            (void)pdf_ps_stack_pop(cs, size + 1);
                            size = 0;
                        }
                        else {
                            for (i = 0; i < size; i++) {
                                memcpy(&(arr[(size - 1) - i]), cs->cur, sizeof(*cs->cur));
                                if (pdf_ps_obj_has_type(cs->cur, PDF_PS_OBJ_ARRAY)) {
                                    pdf_ps_make_null(cs->cur);
                                }
                                (void)pdf_ps_stack_pop(cs, 1);
                            }
                            /* And pop the array mark */
                            (void)pdf_ps_stack_pop(cs, 1);
                        }
                    }
                    else {
                        /* And pop the array mark for an emtpy array */
                        (void)pdf_ps_stack_pop(cs, 1);
                    }
                    code = pdf_ps_stack_push_array(cs, arr, size > 0 ? size : 0);
                    arraydepth--;
                    if (arraydepth < 0)
                        arraydepth = 0;
                }
                break;
            case '.':
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
            case '9':{
                    bool is_float = false;
                    int len;
                    byte *n = --pdfpsbuf, *numbuf;

                    while (pdfpsbuf < buflim && !pdf_ps_end_number_object((int)*pdfpsbuf)) {
                        if (*pdfpsbuf == '.' || *pdfpsbuf == 'e')
                            is_float = true;
                        pdfpsbuf++;
                    }
                    len = pdfpsbuf - n;
                    if (len == 1 && *n == '-') {
                        /* Not a number, might be an operator */
                        pdfpsbuf = n + 1;
                        goto retry_as_oper;
                    }
                    numbuf = gs_alloc_bytes(cs->pdfi_ctx->memory, len + 1, "ps pdf number buffer");
                    if (numbuf == NULL) {
                        code = gs_note_error(gs_error_VMerror);
                    }
                    else {
                        memcpy(numbuf, n, len);
                        numbuf[len] = '\0';
                        if (is_float) {
                            float f = (float)atof((const char *)numbuf);

                            code = pdf_ps_stack_push_float(cs, f);
                        }
                        else {
                            int i = atoi((const char *)numbuf);

                            code = pdf_ps_stack_push_int(cs, i);
                        }
                        gs_free_object(cs->pdfi_ctx->memory, numbuf, "ps pdf number buffer");
                    }
                } break;
            case ' ':
            case '\f':
            case '\t':
            case char_CR:
            case char_EOL:
            case char_NULL:
                break;
            default:
              retry_as_oper:{
                    byte *n = --pdfpsbuf;
                    int len, i;
                    int (*opfunc)(gs_memory_t *mem, pdf_ps_ctx_t *stack, byte *buf, byte *bufend) = NULL;
                    pdf_ps_oper_list_t *ops = cs->ops;

                    while (pdfpsbuf < buflim && !pdf_ps_end_object((int)*pdfpsbuf))
                        pdfpsbuf++;

                    if (arraydepth == 0) {
                        len = pdfpsbuf - n;
                        for (i = 0; ops[i].opname != NULL; i++) {
                            if (len == ops[i].opnamelen && !memcmp(n, ops[i].opname, len)) {
                                opfunc = ops[i].oper;
                                break;
                            }
                        }

                        if (opfunc) {
                            code = (*opfunc) (cs->pdfi_ctx->memory, cs, pdfpsbuf, buflim);
                            if (code > 0) {
                                pdfpsbuf += code;
                                code = 0;
                            }
                        }
                    }
                }
                break;
        }
    }
    if ((stackdepth = pdf_ps_stack_count(cs)) > 0) {
        pdf_ps_stack_pop(cs, stackdepth);
    }
    return code;
}

static inline bool pdf_ps_name_cmp(pdf_ps_stack_object_t *obj, const char *namestr)
{
    byte *d = NULL;
    int l1, l2;

    if (namestr) {
        l2 = strlen(namestr);
    }

    if (obj->type == PDF_PS_OBJ_NAME) {
        d = obj->val.name;
        l1 = obj->size;
    }
    else if (obj->type == PDF_PS_OBJ_STRING) {
        d = obj->val.name;
        l1 = obj->size;
    }
    if (d != NULL && namestr != NULL && l1 == l2) {
        return memcmp(d, namestr, l1) == 0 ? true : false;
    }
    return false;
}

static int
ps_font_def_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    int code = 0, code2 = 0;
    ps_font_interp_private *priv = (ps_font_interp_private *) s->client_data;

    if ((code = pdf_ps_stack_count(s)) < 2) {
        return pdf_ps_stack_pop(s, code);
    }

    if (pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME)) {
        switch (s->cur[-1].size) {

          case 4:
              if (pdf_ps_name_cmp(&s->cur[-1], "XUID")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY) && uid_is_valid(&priv->gsu.gst1.UID) == false) {
                      int i, size = s->cur[0].size;
                      long *xvals = (long *)gs_alloc_bytes(mem, size *sizeof(long), "ps_font_def_func(xuid vals)");

                      if (xvals != NULL) {
                          for (i = 0; i < size; i++) {
                              if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                                  xvals[i] = s->cur[0].val.arr[i].val.i;
                              }
                              else {
                                  gs_free_object(mem, xvals, "ps_font_def_func(xuid vals)");
                                  xvals = NULL;
                                  break;
                              }
                          }
                      }
                      if (xvals != NULL) {
                          if (priv->gsu.gst1.UID.xvalues != NULL)
                              gs_free_object(mem, priv->gsu.gst1.UID.xvalues, "ps_font_def_func(old xuid vals)");
                          uid_set_XUID(&priv->gsu.gst1.UID, xvals, size);
                      }
                  }
              }
              break;

          case 5:
              if (pdf_ps_name_cmp(&s->cur[-1], "StdHW")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY) && s->cur[0].size > 0) {
                      if (pdf_ps_obj_has_type(&s->cur[0].val.arr[0], PDF_PS_OBJ_INTEGER)) {
                          priv->gsu.gst1.data.StdHW.values[0] = (float)s->cur[0].val.arr[0].val.i;
                          priv->gsu.gst1.data.StdHW.count = 1;
                      }
                      else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[0], PDF_PS_OBJ_FLOAT)) {
                          priv->gsu.gst1.data.StdHW.values[0] = s->cur[0].val.arr[0].val.f;
                          priv->gsu.gst1.data.StdHW.count = 1;
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "StdVW")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY) && s->cur[0].size > 0) {
                      if (pdf_ps_obj_has_type(&s->cur[0].val.arr[0], PDF_PS_OBJ_INTEGER)) {
                          priv->gsu.gst1.data.StdVW.values[0] = (float)s->cur[0].val.arr[0].val.i;
                          priv->gsu.gst1.data.StdVW.count = 1;
                      }
                      else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[0], PDF_PS_OBJ_FLOAT)) {
                          priv->gsu.gst1.data.StdVW.values[0] = s->cur[0].val.arr[0].val.f;
                          priv->gsu.gst1.data.StdVW.count = 1;
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "WMode")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      if (s->cur[0].val.i != 0) {
                          if (s->cur[0].val.i != 1)
                            pdfi_set_warning(s->pdfi_ctx, 0, NULL, W_PDF_BAD_WMODE, "ps_font_def_func", NULL);
                        priv->gsu.gst1.WMode = 1;
                      }
                      else
                        priv->gsu.gst1.WMode = 0;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "lenIV")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      priv->gsu.gst1.data.lenIV = s->cur[0].val.i;
                  }
              }
              break;

          case 6:
              if (pdf_ps_name_cmp(&s->cur[-1], "Notice") && priv->u.t1.notice == NULL) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING)) {
                      pdf_string *subr_str;

                      code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, (unsigned int)s->cur[0].size, (pdf_obj **)&subr_str);
                      if (code < 0) {
                          return code;
                      }
                      pdfi_countup(subr_str);
                      memcpy(subr_str->data, s->cur[0].val.name, s->cur[0].size);

                      pdfi_countdown(priv->u.t1.notice);
                      priv->u.t1.notice = subr_str;
                  }
              }
              break;
          case 8:
              if (pdf_ps_name_cmp(&s->cur[-1], "FontName")) {
                  int fnlen = 0;
                  char *pname = NULL;

                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_NAME)) {
                      fnlen = s->cur[0].size > gs_font_name_max ? gs_font_name_max : s->cur[0].size;
                      pname = (char *)s->cur[0].val.name;
                  }
                  else if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING)) {
                      fnlen = s->cur[0].size > gs_font_name_max ? gs_font_name_max : s->cur[0].size;
                      pname = (char *)s->cur[0].val.string;
                  }
                  if (pname && priv->gsu.gst1.key_name.chars[0] == '\0') {
                      memcpy(priv->gsu.gst1.key_name.chars, pname, fnlen);
                      priv->gsu.gst1.key_name.chars[fnlen] = '\0';
                      priv->gsu.gst1.key_name.size = fnlen;

                      memcpy(priv->gsu.gst1.font_name.chars, pname, fnlen);
                      priv->gsu.gst1.font_name.chars[fnlen] = '\0';
                      priv->gsu.gst1.font_name.size = fnlen;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "FontBBox")) {
                  if (s->cur[0].size > 0 && pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, j;
                      double bbox[4] = { 0, 0, 1000, 1000 };
                      if (pdf_ps_obj_has_type(&s->cur[0].val.arr[0], PDF_PS_OBJ_ARRAY)) { /* This is (probably) a Blend/FontBBox entry */
                          code = pdfi_array_alloc(s->pdfi_ctx, s->cur[0].size, &priv->u.t1.blendfontbbox);
                          if (code >= 0) {
                              pdfi_countup(priv->u.t1.blendfontbbox);
                              for (i = 0; i < s->cur[0].size; i++) {
                                  pdf_ps_stack_object_t *arr = &s->cur[0].val.arr[i];
                                  pdf_array *parr = NULL;
                                  pdf_num *n;
                                  if (pdf_ps_obj_has_type(arr, PDF_PS_OBJ_ARRAY)) {
                                      code = pdfi_array_alloc(s->pdfi_ctx, arr->size, &parr);
                                      if (code < 0)
                                          break;
                                      pdfi_countup(parr);

                                      for (j = 0; j < arr->size; j++) {
                                          if (pdf_ps_obj_has_type(&arr->val.arr[j], PDF_PS_OBJ_INTEGER)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = arr->val.arr[j].val.i;
                                          }
                                          else if (pdf_ps_obj_has_type(&arr->val.arr[j], PDF_PS_OBJ_FLOAT)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_REAL, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.d = arr->val.arr[j].val.f;
                                          }
                                          else {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = 0;
                                          }
                                          if (code < 0)
                                              break;
                                          pdfi_countup(n);
                                          code = pdfi_array_put(s->pdfi_ctx, parr, j, (pdf_obj *)n);
                                          pdfi_countdown(n);
                                          if (code < 0) break;
                                      }
                                  }
                                  if (code >= 0)
                                      code = pdfi_array_put(s->pdfi_ctx, priv->u.t1.blendfontbbox, i, (pdf_obj *)parr);
                                  pdfi_countdown(parr);
                              }
                          }
                      }
                      else if (s->cur[0].size >= 4) {
                          for (i = 0; i < 4; i++) {
                              if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                                  bbox[i] = (double)s->cur[0].val.arr[i].val.i;
                              }
                              else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                                  bbox[i] = (double)s->cur[0].val.arr[i].val.f;
                              }
                          }
                          priv->gsu.gst1.FontBBox.p.x = bbox[0];
                          priv->gsu.gst1.FontBBox.p.y = bbox[1];
                          priv->gsu.gst1.FontBBox.q.x = bbox[2];
                          priv->gsu.gst1.FontBBox.q.y = bbox[3];
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "FontType")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      priv->gsu.gst1.FontType = s->cur[0].val.i;
                      priv->u.t1.pdfi_font_type = s->cur[0].val.i == 1 ? e_pdf_font_type1 : e_pdf_cidfont_type0;
                  }
                  else {
                      priv->gsu.gst1.FontType = 1;
                      priv->u.t1.pdfi_font_type = e_pdf_font_type1;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "Encoding")) {
                  pdf_array *new_enc = NULL;

                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_NAME)) {
                      pdf_name *pname;

                      code = pdfi_name_alloc(s->pdfi_ctx, (byte *) s->cur[0].val.name, s->cur[0].size, (pdf_obj **) &pname);
                      if (code >= 0) {
                          pdfi_countup(pname);

                          code = pdfi_create_Encoding(s->pdfi_ctx, NULL, (pdf_obj *) pname, NULL, (pdf_obj **) &new_enc);
                          if (code >= 0) {
                              pdfi_countdown(priv->u.t1.Encoding);
                              priv->u.t1.Encoding = new_enc;
                          }
                          pdfi_countdown(pname);
                      }
                  }
                  else if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i;

                      code = pdfi_array_alloc(s->pdfi_ctx, s->cur[0].size, &new_enc);
                      if (code >= 0) {
                          pdfi_countup(new_enc);
                          for (i = 0; i < s->cur[0].size; i++) {
                              pdf_name *n = NULL;
                              byte *nm = (byte *) s->cur[0].val.arr[i].val.name;
                              int nlen = s->cur[0].val.arr[i].size;

                              code = pdfi_name_alloc(s->pdfi_ctx, (byte *) nm, nlen, (pdf_obj **) &n);
                              if (code < 0)
                                  break;
                              pdfi_countup(n);
                              code = pdfi_array_put(s->pdfi_ctx, new_enc, (uint64_t) i, (pdf_obj *) n);
                              pdfi_countdown(n);
                              if (code < 0)
                                  break;
                          }
                          if (code < 0) {
                              pdfi_countdown(new_enc);
                          }
                          else {
                              pdfi_countdown(priv->u.t1.Encoding);
                              priv->u.t1.Encoding = new_enc;
                              new_enc = NULL;
                          }
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "UniqueID") && uid_is_valid(&priv->gsu.gst1.UID) == false) {
                  /* Ignore UniqueID if we already have a XUID */
                  if (priv->gsu.gst1.UID.id >= 0) {
                      if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                          uid_set_UniqueID(&priv->gsu.gst1.UID, s->cur[0].val.i);
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "FullName")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING) && priv->u.t1.fullname == NULL) {
                      pdf_string *subr_str;

                      code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, (unsigned int)s->cur[0].size, (pdf_obj **)&subr_str);
                      if (code < 0) {
                          return code;
                      }
                      pdfi_countup(subr_str);
                      memcpy(subr_str->data, s->cur[0].val.name, s->cur[0].size);

                      pdfi_countdown(priv->u.t1.fullname);
                      priv->u.t1.fullname = subr_str;
                  }
              }
              break;

          case 9:
              if (pdf_ps_name_cmp(&s->cur[-1], "PaintType")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      priv->gsu.gst1.PaintType = s->cur[0].val.i;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "StemSnapH")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, size = s->cur[0].size > 12 ? 12 : s->cur[0].size;

                      for (i = 0; i < size; i++) {
                          if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                              priv->gsu.gst1.data.StemSnapH.values[i] = (float)s->cur[0].val.arr[i].val.i;
                          }
                          else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                              priv->gsu.gst1.data.StemSnapH.values[i] = s->cur[0].val.arr[i].val.f;
                          }
                      }
                      priv->gsu.gst1.data.StemSnapH.count = size;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "StemSnapV")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, size = s->cur[0].size > 12 ? 12 : s->cur[0].size;

                      for (i = 0; i < size; i++) {
                          if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                              priv->gsu.gst1.data.StemSnapV.values[i] = (float)s->cur[0].val.arr[i].val.i;
                          }
                          else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                              priv->gsu.gst1.data.StemSnapV.values[i] = s->cur[0].val.arr[i].val.f;
                          }
                      }
                      priv->gsu.gst1.data.StemSnapV.count = size;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "BlueScale")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      priv->gsu.gst1.data.BlueScale = (float)s->cur[0].val.i;
                  }
                  else if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_FLOAT)) {
                      priv->gsu.gst1.data.BlueScale = (float)s->cur[0].val.f;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "Copyright")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING)) {
                      pdf_string *subr_str;

                      code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, (unsigned int)s->cur[0].size, (pdf_obj **)&subr_str);
                      if (code < 0) {
                          return code;
                      }
                      pdfi_countup(subr_str);
                      memcpy(subr_str->data, s->cur[0].val.name, s->cur[0].size);

                      pdfi_countdown(priv->u.t1.copyright);
                      priv->u.t1.copyright = subr_str;
                  }
              }
              break;

          case 10:
              if (pdf_ps_name_cmp(&s->cur[-1], "FontMatrix")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY) && s->cur[0].size >= 6
                    && (priv->gsu.gst1.FontMatrix.xx * priv->gsu.gst1.FontMatrix.yy - priv->gsu.gst1.FontMatrix.yx * priv->gsu.gst1.FontMatrix.xy == 0)) {
                      int i;
                      double fmat[6] = { 0.001, 0, 0, 0.001, 0, 0 };
                      for (i = 0; i < 6; i++) {
                          if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                              fmat[i] = (double)s->cur[0].val.arr[i].val.i;
                          }
                          else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                              fmat[i] = (double)s->cur[0].val.arr[i].val.f;
                          }
                      }
                      priv->gsu.gst1.FontMatrix.xx = fmat[0];
                      priv->gsu.gst1.FontMatrix.xy = fmat[1];
                      priv->gsu.gst1.FontMatrix.yx = fmat[2];
                      priv->gsu.gst1.FontMatrix.yy = fmat[3];
                      priv->gsu.gst1.FontMatrix.tx = fmat[4];
                      priv->gsu.gst1.FontMatrix.ty = fmat[5];
                      priv->gsu.gst1.orig_FontMatrix = priv->gsu.gst1.FontMatrix;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "BlueValues")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, size = s->cur[0].size < 14 ? s->cur[0].size : 14;

                      for (i = 0; i < size; i++) {
                          if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                              priv->gsu.gst1.data.BlueValues.values[i] =
                                  (float)s->cur[0].val.arr[i].val.i;
                          }
                          else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                              priv->gsu.gst1.data.BlueValues.values[i] = s->cur[0].val.arr[i].val.f;
                          }
                          else {
                              if (i == 0)
                                  priv->gsu.gst1.data.BlueValues.values[i] = 0;
                              else
                                  priv->gsu.gst1.data.BlueValues.values[i] = priv->gsu.gst1.data.BlueValues.values[i - 1] + 1;
                          }
                      }
                      priv->gsu.gst1.data.BlueValues.count = size;
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "FamilyName")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_STRING) && priv->u.t1.familyname == NULL) {
                      pdf_string *subr_str;

                      code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, (unsigned int)s->cur[0].size, (pdf_obj **)&subr_str);
                      if (code < 0) {
                          return code;
                      }
                      pdfi_countup(subr_str);
                      memcpy(subr_str->data, s->cur[0].val.name, s->cur[0].size);

                      pdfi_countdown(priv->u.t1.familyname);
                      priv->u.t1.familyname = subr_str;
                  }
              }
              break;

          case 11:
              if (pdf_ps_name_cmp(&s->cur[-1], "StrokeWidth")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_FLOAT)) {
                      priv->gsu.gst1.StrokeWidth = s->cur[0].val.f;
                  }
                  else if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER)) {
                      priv->gsu.gst1.StrokeWidth = (float)s->cur[0].val.i;
                  }
              }
              break;
          case 12:
              if (pdf_ps_name_cmp(&s->cur[-1], "WeightVector")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, size = s->cur[0].size > 16 ? 16 : s->cur[0].size;

                      for (i = 0; i < size; i++) {
                          if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_INTEGER)) {
                              priv->gsu.gst1.data.WeightVector.values[i] = (float)s->cur[0].val.arr[i].val.i;
                          }
                          else if (pdf_ps_obj_has_type(&s->cur[0].val.arr[i], PDF_PS_OBJ_FLOAT)) {
                              priv->gsu.gst1.data.WeightVector.values[i] = s->cur[0].val.arr[i].val.f;
                          }
                          else {
                              priv->gsu.gst1.data.WeightVector.values[i] = 0;
                          }
                      }
                      priv->gsu.gst1.data.WeightVector.count = s->cur[0].size;
                  }
              }
              break;
          case 14:
              if (pdf_ps_name_cmp(&s->cur[-1], "BlendAxisTypes")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i;
                      code = pdfi_array_alloc(s->pdfi_ctx, s->cur[0].size, &priv->u.t1.blendaxistypes);
                      if (code >= 0) {
                          pdfi_countup(priv->u.t1.blendaxistypes);
                          for (i = 0; i < s->cur[0].size; i++) {
                              pdf_ps_stack_object_t *so = &s->cur[0].val.arr[i];
                              pdf_name *n;
                              if (pdf_ps_obj_has_type(so, PDF_PS_OBJ_NAME)) {
                                  code = pdfi_object_alloc(s->pdfi_ctx, PDF_NAME, so->size, (pdf_obj **)&n);
                                  if (code >= 0) {
                                      pdfi_countup(n);
                                      memcpy(n->data, so->val.name, so->size);
                                      n->length = so->size;
                                      code = pdfi_array_put(s->pdfi_ctx, priv->u.t1.blendaxistypes, i, (pdf_obj *)n);
                                      pdfi_countdown(n);
                                  }
                              }
                              if (code < 0)
                                  break;
                          }
                      }
                  }
              }
              else if (pdf_ps_name_cmp(&s->cur[-1], "BlendDesignMap")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      int i, j, k;
                      pdf_ps_stack_object_t *arr1 = &s->cur[0], *arr2, *arr3;
                      pdf_array *parr2, *parr3;
                      code = pdfi_array_alloc(s->pdfi_ctx, arr1->size, &priv->u.t1.blenddesignmap);
                      if (code >= 0) {
                          pdfi_countup(priv->u.t1.blenddesignmap);
                          for (i = 0; i < arr1->size && code >= 0; i++) {
                              if (pdf_ps_obj_has_type(&arr1->val.arr[i], PDF_PS_OBJ_ARRAY)) {
                                  arr2 = &arr1->val.arr[i];
                                  code = pdfi_array_alloc(s->pdfi_ctx, arr2->size, &parr2);
                                  if (code < 0)
                                      break;
                                  for (j = 0; j < arr2->size; j++) {
                                      pdf_num *n;

                                      arr3 = &arr2->val.arr[j];
                                      code = pdfi_array_alloc(s->pdfi_ctx, arr3->size, &parr3);
                                      if (code < 0)
                                          break;

                                      for (k = 0; k < arr3->size; k++) {
                                          if (pdf_ps_obj_has_type(&arr3->val.arr[k], PDF_PS_OBJ_INTEGER)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = arr3->val.arr[k].val.i;
                                          }
                                          else if (pdf_ps_obj_has_type(&arr1->val.arr[i], PDF_PS_OBJ_FLOAT)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_REAL, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.d = arr3->val.arr[k].val.f;
                                          }
                                          else {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = 0;
                                          }
                                          if (code < 0)
                                              break;
                                          pdfi_countup(n);
                                          code = pdfi_array_put(s->pdfi_ctx, parr3, k, (pdf_obj *)n);
                                          pdfi_countdown(n);
                                          if (code < 0)
                                              break;
                                      }
                                      if (code < 0)
                                          break;
                                      pdfi_countup(parr3);
                                      code = pdfi_array_put(s->pdfi_ctx, parr2, j, (pdf_obj *)parr3);
                                      pdfi_countdown(parr3);
                                  }
                                  if (code < 0)
                                      break;
                                  pdfi_countup(parr2);
                                  code = pdfi_array_put(s->pdfi_ctx, priv->u.t1.blenddesignmap, i, (pdf_obj *)parr2);
                                  pdfi_countdown(parr2);
                              }
                          }
                      }
                  }
              }
              break;

          case 20:
              if (pdf_ps_name_cmp(&s->cur[-1], "BlendDesignPositions")) {
                  if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_ARRAY)) {
                      code = pdfi_array_alloc(s->pdfi_ctx, s->cur[0].size, &priv->u.t1.blenddesignpositions);
                      if (code >= 0) {
                          int i, j;
                          pdfi_countup(priv->u.t1.blenddesignpositions);

                          for (i = 0; i < s->cur[0].size && code >= 0; i++) {
                              pdf_ps_stack_object_t *so = &s->cur[0].val.arr[i];

                              if (pdf_ps_obj_has_type(so, PDF_PS_OBJ_ARRAY)) {
                                  pdf_array *sa;
                                  code = pdfi_array_alloc(s->pdfi_ctx, so->size, &sa);
                                  if (code >= 0) {
                                      pdfi_countup(sa);
                                      for (j = 0; j < so->size; j++) {
                                          pdf_num *n;
                                          if (pdf_ps_obj_has_type(&so->val.arr[j], PDF_PS_OBJ_INTEGER)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = so->val.arr[j].val.i;
                                          }
                                          else if (pdf_ps_obj_has_type(&so->val.arr[j], PDF_PS_OBJ_FLOAT)) {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_REAL, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.d = so->val.arr[j].val.f;
                                          }
                                          else {
                                              code = pdfi_object_alloc(s->pdfi_ctx, PDF_INT, 0, (pdf_obj **)&n);
                                              if (code >= 0)
                                                  n->value.i = 0;
                                          }
                                          if (code < 0)
                                              break;
                                          pdfi_countup(n);
                                          code = pdfi_array_put(s->pdfi_ctx, sa, j, (pdf_obj *)n);
                                          pdfi_countdown(n);
                                          if (code < 0) break;
                                      }
                                  }
                                  if (code >= 0) {
                                      code = pdfi_array_put(s->pdfi_ctx, priv->u.t1.blenddesignpositions, i, (pdf_obj *)sa);
                                  }
                                  pdfi_countdown(sa);
                              }
                          }
                      }

                  }
              }
              break;

          default:
              break;
        }
    }

    code2 = pdf_ps_stack_pop(s, 2);
    if (code < 0)
        return code;
    else
        return code2;
}

static int
ps_font_true_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    (void)mem;
    return pdf_ps_stack_push_boolean(s, true);
}

static int
ps_font_false_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    (void)mem;
    return pdf_ps_stack_push_boolean(s, false);
}

static int
ps_font_dict_begin_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    (void)mem;
    return pdf_ps_stack_push_dict_mark(s);
}

static int
ps_font_dict_end_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    int dsize = pdf_ps_stack_count_to_mark(s, PDF_PS_OBJ_DICT_MARK);

    (void)mem;
    if (dsize >= 0)
        return pdf_ps_stack_pop(s, dsize + 1);  /* Add one for the mark object */
    else
        return 0;
}

static stream *
push_eexec_filter(gs_memory_t *mem, byte *buf, byte *bufend)
{
    stream *fs, *ffs = NULL;
    stream *sstrm;
    stream_exD_state *st;
    byte *strbuf;

    sstrm = file_alloc_stream(mem, "push_eexec_filter(buf stream)");
    if (sstrm == NULL)
        return NULL;

    /* Because of streams <shrug!> we advance the buffer one byte */
    buf++;
    sread_string(sstrm, buf, bufend - buf);
    sstrm->close_at_eod = false;

    fs = s_alloc(mem, "push_eexec_filter(fs)");
    strbuf = gs_alloc_bytes(mem, 4096, "push_eexec_filter(buf)");
    st = gs_alloc_struct(mem, stream_exD_state, s_exD_template.stype, "push_eexec_filter(st)");
    if (fs == NULL || st == NULL || strbuf == NULL) {
        sclose(sstrm);
        gs_free_object(mem, sstrm, "push_eexec_filter(buf stream)");
        gs_free_object(mem, fs, "push_eexec_filter(fs)");
        gs_free_object(mem, st, "push_eexec_filter(st)");
        goto done;
    }
    memset(st, 0x00, sizeof(stream_exD_state));

    s_std_init(fs, strbuf, 69, &s_filter_read_procs, s_mode_read);
    st->memory = mem;
    st->templat = &s_exD_template;
    fs->state = (stream_state *) st;
    fs->procs.process = s_exD_template.process;
    fs->strm = sstrm;
    (*s_exD_template.set_defaults) ((stream_state *) st);
    st->cstate = 55665;
    st->binary = -1;
    st->lenIV = 4;
    st->keep_spaces = true;
    (*s_exD_template.init) ((stream_state *) st);
    fs->close_at_eod = false;
    ffs = fs;
  done:
    return ffs;
}

static void
pop_eexec_filter(gs_memory_t *mem, stream *s)
{
    stream *src = s->strm;
    byte *b = s->cbuf;

    sclose(s);
    gs_free_object(mem, s, "pop_eexec_filter(s)");
    gs_free_object(mem, b, "pop_eexec_filter(b)");
    if (src)
        sclose(src);
    gs_free_object(mem, src, "pop_eexec_filter(strm)");
}

/* We decode the eexec data in place */
static int
ps_font_eexec_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    stream *strm;
    int c;

    if (bufend <= buf) {
        return_error(gs_error_invalidfont);
    }

    strm = push_eexec_filter(mem, buf, bufend);
    while (1) {
        c = sgetc(strm);
        if (c < 0)
            break;
        *buf = (byte) c;
        buf++;
    }
    pop_eexec_filter(mem, strm);

    return 0;
}

/* Normally, for us, "array" is a NULL op.
 *The exception is when the name /Subrs is two objects
 *down from the top of the stack, then we can use this call
 *to record how many subrs we expect, and allocate space for them
 */
static int
ps_font_array_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    ps_font_interp_private *priv = (ps_font_interp_private *) s->client_data;
    int code = 0;

    if (pdf_ps_stack_count(s) < 2) {
        return pdf_ps_stack_pop(s, 1);
    }
    if (pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME) &&
        pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER) &&
        !memcmp(s->cur[-1].val.name, PDF_PS_OPER_NAME_AND_LEN("Subrs"))) {

        if (s->cur[0].val.i > 0) {
            pdfi_countdown(priv->u.t1.Subrs);

            code = pdfi_object_alloc(s->pdfi_ctx, PDF_ARRAY, (unsigned int)s->cur[0].val.i, (pdf_obj **)&priv->u.t1.Subrs);
            if (code < 0) {
                return code;
            }
            pdfi_countup(priv->u.t1.Subrs);
        }
        code = pdf_ps_stack_pop(s, 1);
    }
    else if (pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME) &&
             pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER) &&
             !memcmp(s->cur[-1].val.name, PDF_PS_OPER_NAME_AND_LEN("Encoding"))) {
        /* We're defining a custom encoding array */
        pdf_ps_stack_object_t *arr = NULL;
        int size = s->cur[0].val.i;

        if (size > 0) {
            arr = (pdf_ps_stack_object_t *) gs_alloc_bytes(mem, size *sizeof(pdf_ps_stack_object_t), "ps_font_array_func(encoding array)");
            if (arr != NULL) {
                code = pdf_ps_stack_pop(s, 1);
                if (code < 0) {
                    gs_free_object(mem, arr, "ps_font_array_func(encoding array)");
                }
                else {
                    int i;

                    for (i = 0; i < size; i++) {
                        pdf_ps_make_name(&arr[i], (byte *) notdefnamestr, strlen(notdefnamestr));
                    }
                    code = pdf_ps_stack_push_array(s, arr, size);
                }
            }
            else {
                code = gs_note_error(gs_error_VMerror);
            }
        }
    }
    return code;
}

/* Normally, for us, "dict" is a NULL op.
 *The exception is when the name /CharStrings is two objects
 *down from the top of the stack, then we can use this call
 *to record how many charstrings we expect, and allocate space for them
 */
static int
ps_font_dict_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    ps_font_interp_private *priv = (ps_font_interp_private *) s->client_data;

    if (pdf_ps_stack_count(s) < 2) {
        return pdf_ps_stack_pop(s, 1);
    }
    if (pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME) &&
        pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER) &&
        !memcmp(s->cur[-1].val.name, PDF_PS_OPER_NAME_AND_LEN("CharStrings"))) {
        int code;
        pdf_dict *d = NULL;

        if (priv->u.t1.CharStrings == NULL) {
            code = pdfi_dict_alloc(s->pdfi_ctx, s->cur[0].val.i, &d);
            if (code < 0) {
                priv->u.t1.CharStrings = NULL;
                (void)pdf_ps_stack_pop(s, 1);
                return code;
            }

            priv->u.t1.CharStrings = d;
            pdfi_countup(priv->u.t1.CharStrings);
        }
    }
    return pdf_ps_stack_pop(s, 1);
}

static int
pdf_ps_pop2_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    return pdf_ps_stack_pop(s, 2);
}

static int
pdf_ps_pop4_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    return pdf_ps_stack_pop(s, 4);
}

static int
pdf_ps_standardencoding_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    return pdf_ps_stack_push_name(s, (byte *) "StandardEncoding", 16);
}

/* { string currentfile exch readstring pop } */
static int
pdf_ps_RD_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    ps_font_interp_private *priv = (ps_font_interp_private *) s->client_data;
    int code;
    int size = 0;

    if (pdf_ps_stack_count(s) >= 1) {
        if (priv->u.t1.Subrs != NULL && priv->u.t1.CharStrings == NULL) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER) &&
                pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_INTEGER)) {
                int inx = s->cur[-1].val.i;

                size = s->cur[0].val.i;
                if (size < 0)
                    return_error(gs_error_invalidfont);

                buf++;
                if (buf + size < bufend) {
                    pdf_string *subr_str;

                    code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, (unsigned int)size, (pdf_obj **)&subr_str);
                    if (code < 0) {
                        return code;
                    }
                    memcpy(subr_str->data, buf, size);
                    pdfi_countup(subr_str);
                    code = pdfi_array_put(s->pdfi_ctx, priv->u.t1.Subrs, inx, (pdf_obj *)subr_str);
                    pdfi_countdown(subr_str);
                    if (code < 0)
                        return code;
                }
            }
        }
        else if (priv->u.t1.CharStrings != NULL) {
            if (pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_INTEGER) &&
                pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_NAME)) {
                pdf_string *str = NULL;
                pdf_obj *key = NULL;

                size = s->cur[0].val.i;
                if (size < 0)
                    return_error(gs_error_invalidfont);

                buf++;
                code = pdfi_name_alloc(s->pdfi_ctx, (byte *) s->cur[-1].val.name, s->cur[-1].size, &key);
                if (code < 0) {
                    (void)pdf_ps_stack_pop(s, 2);
                    return code;
                }
                pdfi_countup(key);

                if (buf + size < bufend) {
                    code = pdfi_object_alloc(s->pdfi_ctx, PDF_STRING, size, (pdf_obj **) &str);
                    if (code < 0) {
                        pdfi_countdown(key);
                        (void)pdf_ps_stack_pop(s, 2);
                        return code;
                    }
                    pdfi_countup(str);
                    memcpy(str->data, buf, size);

                    code = pdfi_dict_put_obj(s->pdfi_ctx, priv->u.t1.CharStrings, key, (pdf_obj *) str, false);
                    if (code < 0) {
                       pdfi_countdown(str);
                       pdfi_countdown(key);
                       (void)pdf_ps_stack_pop(s, 2);
                       return code;
                    }
                }
                pdfi_countdown(str);
                pdfi_countdown(key);
            }
        }
        code = pdf_ps_stack_pop(s, 2);
        return code < 0 ? code : size + 1;
    }
    return 0;
}

static int
pdf_ps_put_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    int code;

    if ((code = pdf_ps_stack_count(s)) < 4) {
        return 0;
    }

    if (pdf_ps_obj_has_type(&s->cur[-3], PDF_PS_OBJ_NAME) &&
        !memcmp(s->cur[-3].val.name, PDF_PS_OPER_NAME_AND_LEN("Encoding")) &&
        pdf_ps_obj_has_type(&s->cur[-2], PDF_PS_OBJ_ARRAY) &&
        pdf_ps_obj_has_type(&s->cur[-1], PDF_PS_OBJ_INTEGER) &&
        pdf_ps_obj_has_type(&s->cur[0], PDF_PS_OBJ_NAME)) {
        if (s->cur[-1].val.i >= 0 && s->cur[-1].val.i < s->cur[-2].size) {
            pdf_ps_make_name(&s->cur[-2].val.arr[s->cur[-1].val.i], s->cur[0].val.name, s->cur[0].size);
        }
    }

    code = pdf_ps_stack_pop(s, 2);
    return code;
}

static int
pdf_ps_closefile_oper_func(gs_memory_t *mem, pdf_ps_ctx_t *s, byte *buf, byte *bufend)
{
    return bufend - buf;
}

static pdf_ps_oper_list_t ps_font_oper_list[] = {
    {PDF_PS_OPER_NAME_AND_LEN("RD"), pdf_ps_RD_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("-|"), pdf_ps_RD_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("|"), pdf_ps_put_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("findresource"), clear_stack_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("dict"), ps_font_dict_func},
    {PDF_PS_OPER_NAME_AND_LEN("begin"), ps_font_dict_begin_func},
    {PDF_PS_OPER_NAME_AND_LEN("end"), ps_font_dict_end_func},
    {PDF_PS_OPER_NAME_AND_LEN("pop"), ps_pdf_null_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("def"), ps_font_def_func},
    {PDF_PS_OPER_NAME_AND_LEN("dup"), ps_pdf_null_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("defineresource"), clear_stack_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("definefont"), clear_stack_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("readonly"), ps_pdf_null_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("true"), ps_font_true_func},
    {PDF_PS_OPER_NAME_AND_LEN("false"), ps_font_false_func},
    {PDF_PS_OPER_NAME_AND_LEN("eexec"), ps_font_eexec_func},
    {PDF_PS_OPER_NAME_AND_LEN("array"), ps_font_array_func},
    {PDF_PS_OPER_NAME_AND_LEN("known"), pdf_ps_pop_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("if"), pdf_ps_pop_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("ifelse"), pdf_ps_pop2_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("for"), pdf_ps_pop4_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("put"), pdf_ps_put_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("StandardEncoding"), pdf_ps_standardencoding_oper_func},
    {PDF_PS_OPER_NAME_AND_LEN("closefile"), pdf_ps_closefile_oper_func},
    {NULL, 0, NULL}
};

int
pdfi_read_ps_font(pdf_context *ctx, pdf_dict *font_dict, byte *fbuf, int fbuflen, ps_font_interp_private *ps_font_priv)
{
    int code = 0;
    pdf_ps_ctx_t ps_font_ctx;

    code = pdfi_pscript_stack_init(ctx, ps_font_oper_list, ps_font_priv, &ps_font_ctx);
    if (code < 0)
        goto error_out;

    code = pdfi_pscript_interpret(&ps_font_ctx, fbuf, fbuflen);
    pdfi_pscript_stack_finit(&ps_font_ctx);
    /* We have several files that have a load of garbage data in the stream after the font is defined,
       and that can end up in a stackoverflow error, even though we have a complete font. Override it
       and let the Type 1 specific code decide for itself if it can use the font.
     */
    if (code == gs_error_pdf_stackoverflow)
        code = 0;

    return code;
  error_out:
    code = gs_error_invalidfont;
    return code;
}
