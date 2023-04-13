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


/* gsparaml.c -  Handling of reading lists of params from strings */

#include <stdlib.h>
#include "gsparam.h"
#include "gserrors.h"
#include "string_.h"

static int
add_tokens(gs_param_list *plist, gs_param_name key, char **pp, uint *dict_count);

static int
walk_number(char **p, bool *is_integer)
{
    char *p1 = *p;
    bool integer = true;

    if (*p1 == '+')
        p1++;
    while (*p1 == ' ')
        p1++;
    while (*p1 == '-')
        p1++;
    while (*p1 == ' ')
        p1++;
    if (*p1 == 0 || ((*p1 < '0' || *p1 > '9') && (*p1 != '.')))
        return -1;
    while ((*p1 >= '0' && *p1 <= '9') || *p1 == '.') {
        if (*p1 == '.') {
            if (!integer) /* Can't cope with multiple .'s */
                return -1;
            integer = false;
        }

        p1++;
    }
    /* Allow for exponent form. */
    if (*p1 == 'e' || *p1 == 'E') {
        p1++;
        if (*p1 == '-')
            p1++;
        if (*p1 < '0' || *p1 > '9')
            return -1;
        while (*p1 >= '0' && *p1 <= '9')
            p1++;
    }

    *is_integer = integer;
    *p = p1;

    return 0;
}

/* Delimiter chars, as taken from pdf spec. Any of these characters
 * ends a token. */
static int
ends_token(const char *p)
{
    return (*p == 0 ||
            *p == 9 ||
            *p == 10 ||
            *p == 12 ||
            *p == 13 ||
            *p == 32 ||
            *p == '/' ||
            *p == '%' ||
            *p == '<' || *p == '>' ||
            *p == '[' || *p == ']' ||
            *p == '{' || *p == '}' ||
            *p == '(' || *p == ')');
}

/* Dictionaries are surprisingly easy, we just make a param_dict
 * and then call the existing routine to parse the string and
 * add tokens to the parameter list contained in the dictionary.
 */
static int
process_dict(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    gs_param_dict dict;
    int code, code2;

    /* We are implicitly relying on that fact that we're working to
     * C param lists, not ref param lists here, as C param lists don't
     * need the size up front, but ref param lists do. This makes the
     * recursion MUCH simpler. */
    code = param_begin_write_dict((gs_param_list *)plist, key, &dict, false);
    if (code < 0)
        return code;

    gs_param_list_set_persistent_keys(dict.list, false);

    dict.size = 0;
    code = add_tokens(dict.list, NULL, p, &dict.size);
    if (code >= 0) {
        if ((*p)[0] != '>' || (*p)[1] != '>')
            code = gs_error_syntaxerror;
        else
           (*p) += 2;
    }
    code2 = param_end_write_dict((gs_param_list *)plist, key, &dict);
    return code < 0 ? code : code2;
}

static int
process_dict_or_hexstring(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *p1 = *p, *src, *dest, data;
    int i;
    gs_param_string ps;

    if (p1[1] == '<') {
        *p += 2;
        return process_dict(mem, plist, key, p);
    }

    dest = p1;
    src = p1+1;
    while (*src && *src != '>') {
        data = 0;
        for (i=0;i<2;i++) {
            if (*src >= '0' && *src <= '9') {
                data = (data << 4);
                data += (*src - '0');
            } else if (*src >= 'A' && *src <= 'F') {
                data = (data << 4);
                data += (*src - 'A' + 10);
            } else if (*src >= 'a' && *src <= 'f') {
                data = (data << 4);
                data += (*src - 'a' + 10);
            } else {
                return -1;
            }
            src++;
        }
        *dest++ = data;
    }

    if (*src == 0)
        return -1;

    *p = src + 1;

    ps.data = (const byte *)p1;
    ps.size = dest - p1;
    ps.persistent = false;
    return param_write_string((gs_param_list *)plist, key, &ps);
}

/* On entry, p points to the '/'. Because we need to null terminate
 * to cope with reading the key of key/value pairs, we move all the
 * chars back by 1, overwriting the '/' to give us room. This avoids
 * us relying on trailing whitespace. */
static int
process_name(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name *key, char **p)
{
    char *out = *p;
    char *in = *p + 1;
    char *start = out;
    gs_param_string ps;

    while (!ends_token(in)) {
        if (*in == '#') {
            int v;
            if (in[1] >= '0' && in[1] <= '9')
                v = (in[1] - '0')<<4;
            else if (in[1] >= 'a' && in[1] <= 'f')
                v = (in[1] - 'a' + 10)<<4;
            else if (in[1] >= 'A' && in[1] <= 'F')
                v = (in[1] - 'a' + 10)<<4;
            else
                return -1;
            if (in[2] >= '0' && in[2] <= '9')
                v += (in[2] - '0');
            else if (in[2] >= 'a' && in[2] <= 'f')
                v += (in[2] - 'a' + 10);
            else if (in[2] >= 'A' && in[2] <= 'F')
                v += (in[2] - 'a' + 10);
            else
                return -1;
            if (v == 0)
                return -1;
            *out++ = v;
            in += 3;
            continue;
        }
        *out++ = *in++;
    }

    /* Null terminate (in case it's the '*key = NULL' case below) */
    *out = 0;
    *p = in;

    if (*key == NULL)
        *key = (gs_param_name)start;
    else {
        ps.data = (const byte *)start;
        ps.size = out - start;
        ps.persistent = false;
        param_write_name((gs_param_list *)plist, *key, &ps);
        *key = NULL;
    }
    return 0;
}

static int
process_string(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *p1 = *p;
    char *start = p1 + 1;
    gs_param_string ps;

    while (*p1 && *p1 != ')')
        p1++;

    if (*p1 == 0)
        return -1;

    *p = p1 + 1; /* Resume after the ')' */

    ps.data = (const byte *)start;
    ps.size = p1-start;
    ps.persistent = false;
    return param_write_string((gs_param_list *)plist, key, &ps);
}

/* Arrays are *way* more complicated than dicts :-(
 * We have 4 different kinds of arrays; name, string, int and float.
 * It seems that parameter arrays can only contain homogenous data, it
 * all has to be of the same type. This complicates matters because we
 * can't know in advance what the type is!
 *
 * So we only handle 3 types of array; int, float and string. Anything
 * which isn't one of those either gets converted to a string or (arrays
 * and dictionaries) throws an error.
 *
 * For numbers, we look at the first element, if it's an integer we make
 * an int array, otherwise we make a float array. If we start an int array
 * and later encounter a float, we make a new float array, copy the existing
 * integers into it (converting to floats) and throw away the old int array.
 *
 * Otherwise if we encounter an object whose type doesn't match the array we
 * created we throw an error.
 */
static int
process_array(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    int code = 0;
    gs_param_type array_type = gs_param_type_null;
    int index = 0, array_max = 0;
    char *start = *p + 1, *p1 = start;
    gs_param_string *parray = 0L;
    char *array_data = 0x00;
    gs_param_string_array string_array;
    gs_param_int_array int_array;
    gs_param_float_array float_array;

    p1 = start;

    while (*p1 != ']' && code == 0) {
        switch (*p1) {
            case ' ':
                p1++;
                break;

            /* We used to parse 'false' and 'true' here, but they ended
             * up as string params, rather that bools, thus making
             * [ false ] and [ (false) ] parse to the be the same thing.
             * That feels wrong, so we've removed the code until param
             * lists actually support arrays of bools. */

            case '<':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (index == array_max) {
                    int new_max = array_max * 2;
                    if (new_max == 0)
                        new_max = 32;
                    if (array_data == NULL) {
                        array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                    } else {
                        char *new_array = (char *)gs_resize_object(mem, array_data, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                        if (new_array == NULL) {
                            code = gs_error_VMerror;
                            break;
                        }
                        array_data = new_array;
                    }
                    array_max = new_max;
                    array_type = gs_param_type_string_array;
                }
                if (*(p1+1) == '<') {
                    code = gs_error_typecheck;
                    break;
                    /* dictionary inside an array, not supported */
                } else {
                    char *src, *dest;
                    char data = 0;
                    int i;

                    parray = (gs_param_string *)array_data;
                    src = dest = ++p1;
                    parray[index].data = (const byte *)p1;
                    while (*src && *src != '>') {
                        data = 0;
                        for (i=0;i<2;i++) {
                            if (*src >= '0' && *src <= '9') {
                                data = (data << 4);
                                data += (*src - '0');
                            } else if (*src >= 'A' && *src <= 'F') {
                                data = (data << 4);
                                data += (*src - 'A' + 10);
                            } else if (*src >= 'a' && *src <= 'f') {
                                data = (data << 4);
                                data += (*src - 'a' + 10);
                            } else {
                                goto return_minus_one;
                            }
                            src++;
                        }
                        *dest++ = data;
                    }
                    parray[index].size = dest - p1;
                    parray[index++].persistent = false;
                    p1 = src;
                }
                break;

            case '/':
                if (array_type != gs_param_type_null && array_type != gs_param_type_name_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (index == array_max) {
                    int new_max = array_max * 2;
                    if (new_max == 0)
                        new_max = 32;
                    if (array_data == NULL) {
                        array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                    } else {
                        char *new_array = (char *)gs_resize_object(mem, array_data, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                        if (new_array == NULL) {
                            code = gs_error_VMerror;
                            break;
                        }
                        array_data = new_array;
                    }
                    array_max = new_max;
                    array_type = gs_param_type_name_array;
                }
                parray = (gs_param_string *)array_data;
                parray[index].data = (const byte *)++p1;
                while (!ends_token(p1))
                    p1++;
                parray[index].size = p1 - (char *)(parray[index].data);
                if (parray[index].size == 0)
                    goto return_minus_one;
                parray[index++].persistent = false;
                break;

            case '(':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (index == array_max) {
                    int new_max = array_max * 2;
                    if (new_max == 0)
                        new_max = 32;
                    if (array_data == NULL) {
                        array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                    } else {
                        char *new_array = (char *)gs_resize_object(mem, array_data, sizeof(gs_param_string) * new_max, "param string array in param parsing");
                        if (new_array == NULL) {
                            code = gs_error_VMerror;
                            break;
                        }
                        array_data = new_array;
                    }
                    array_max = new_max;
                    array_type = gs_param_type_string_array;
                }
                parray = (gs_param_string *)array_data;
                parray[index].data = (const byte *)p1;
                while (*p1 && *p1 != ')')
                    p1++;
                if (*p1 == 0)
                    goto return_minus_one;
                parray[index].size = p1 - (char *)(parray[index].data);
                parray[index++].persistent = false;
                break;
            case '[':
                /* Nested arrays, not supported */
                code = gs_error_typecheck;
                break;
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
            case '.':
            case '+':
            case '-':
                if (array_type == gs_param_type_string_array || array_type == gs_param_type_name_array ) {
                    code = gs_error_typecheck;
                    break;
                } else {
                    bool integer;
                    const char *start = p1;
                    char c;
                    float *floats;
                    int *ints, i;

                    code = walk_number(&p1, &integer);
                    if (code < 0)
                        break;

                    if (array_type == gs_param_type_int_array && !integer) {
                        ints = (int *)array_data;
                        floats = (float *)gs_alloc_bytes(mem, sizeof(float) * array_max, "param string array in param parsing");
                        if (floats == NULL){
                            code = gs_error_VMerror;
                            break;
                        }
                        array_type = gs_param_type_float_array;
                        for (i=0;i<index;i++){
                            floats[i] = (float)(ints[i]);
                        }
                        gs_free_object(mem, ints, "param string array in param parsing");
                        array_data = (char *)floats;
                    }
                    if (index == array_max) {
                        union { float f; int i; } size_me;
                        int new_max = array_max * 2;
                        if (new_max == 0) {
                            new_max = 32;
                            array_type = integer ? gs_param_type_int_array : gs_param_type_float_array;
                        }
                        if (array_data == NULL) {
                            array_data = (char *)gs_alloc_bytes(mem, sizeof(size_me) * new_max, "param string array in param parsing");
                        } else {
                            char *new_array = (char *)gs_resize_object(mem, array_data, sizeof(size_me) * new_max, "param string array in param parsing");
                            if (new_array == NULL) {
                                code = gs_error_VMerror;
                                break;
                            }
                            array_data = new_array;
                        }
                        array_max = new_max;
                    }
                    c = *p1;
                    *p1 = 0;
                    if (array_type == gs_param_type_int_array) {
                        ints = (int *)array_data;
                        ints[index++] = (int)atol(start);
                    } else {
                        floats = (float *)array_data;
                        floats[index++] = (float)atof(start);
                    }
                    *p1 = c;
                }
                break;
            default:
                code = gs_error_typecheck;
                break;
        }
    }
    if (0) {
return_minus_one:
        code = -1;
    }

    /* Now we have to deal with adding the array to the parm list, there are
     * (of course!) different calls for each array type....
     */
    if (code >= 0)
    {
        *p = p1 + 1;
        switch(array_type) {
            case gs_param_type_string_array:
                string_array.data = (const gs_param_string *)array_data;
                string_array.persistent = 0;
                string_array.size = index;
                code = param_write_string_array((gs_param_list *)plist, key, &string_array);
                break;
            case gs_param_type_name_array:
                string_array.data = (const gs_param_string *)array_data;
                string_array.persistent = 0;
                string_array.size = index;
                code = param_write_name_array((gs_param_list *)plist, key, &string_array);
                break;
            case gs_param_type_int_array:
                int_array.data = (const int *)array_data;
                int_array.persistent = 0;
                int_array.size = index;
                code = param_write_int_array((gs_param_list *)plist, key, &int_array);
                break;
            case gs_param_type_float_array:
                float_array.data = (const float *)array_data;
                float_array.persistent = 0;
                float_array.size = index;
                code = param_write_float_array((gs_param_list *)plist, key, &float_array);
                break;

            default:
                break;
        }
    }

    /* And now we can throw away the array data, we copied it to the param list. */
    gs_free_object(mem, array_data, "param string array in param parsing");

    return code;
}

/* We rely on the fact that we can overwrite, then restore *end here. */
static int
process_number(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    bool integer;
    const char *start = *p;
    char c;
    int code = walk_number(p, &integer);

    if (code < 0)
        return code;

    /* Hacky. Null terminate so that atof/atol don't overrun. This is
     * safe because at worst p points to the null terminator byte, and
     * we can safely overwrite end for a moment. Ick. */
    c = **p;
    **p = 0;
    if (!integer) {
        float f = (float)atof(start);
        code = param_write_float((gs_param_list *)plist, key, (float *)&f);
    } else {
        /* FIXME: Should probably really be int64_t here rather than int? */
        long i = atol(start);
        code = param_write_long((gs_param_list *)plist, key, &i);
    }
    **p = c;

    return code;
}

static int
add_tokens(gs_param_list *plist, gs_param_name key, char **pp, uint *dict_count)
{
    char *p = *pp;
    int code = 0;
    /* If single == true, then we are looking for a single value,
     * otherwise it's a list of key/value pairs */
    int single = (key != NULL);
    /* If single_done, then we've read our single value. Any non
     * whitespace we read is an error. */
    int single_done = 0;
    bool f = false, t = true;

    while (*p) {
        switch (*p) {
            case ' ':
                p++;
                break;
            case 'f':
                if (single_done || key == NULL)
                    return -1;
                if (strncmp(p, "false", 5) != 0)
                    return -1;
                if (!ends_token(p+5))
                    return -1;
                code = param_write_bool((gs_param_list *)plist, key, &f);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                p += 5;
                single_done = single;
                key = NULL;
                break;
            case 't':
                if (single_done || key == NULL)
                    return -1;
                if (strncmp(p, "true", 4) != 0)
                    return -1;
                if (!ends_token(p+4))
                    return -1;
                code = param_write_bool((gs_param_list *)plist, key, &t);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                p += 4;
                single_done = single;
                key = NULL;
                break;
            case '<':
                if (single_done || key == NULL)
                    return -1;
                code = process_dict_or_hexstring(plist->memory, (gs_c_param_list *)plist, key, &p);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                single_done = single;
                key = NULL;
                break;
            case '/':
            {
                int have_key = (key != NULL);
                if (single_done)
                    return -1;
                code = process_name(plist->memory, (gs_c_param_list *)plist, &key, &p);
                if (code >= 0 && have_key && dict_count != NULL)
                    (*dict_count)++;
                if (have_key) {
                    single_done = single;
                    key = NULL;
                }
                break;
            }
            case '(':
                if (single_done || key == NULL)
                    return -1;
                code = process_string(plist->memory, (gs_c_param_list *)plist, key, &p);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                single_done = single;
                key = NULL;
                break;
            case '[':
                if (single_done || key == NULL)
                    return -1;
                code = process_array(plist->memory, (gs_c_param_list *)plist, key, &p);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                single_done = single;
                key = NULL;
                break;
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
            case '.':
                if (single_done || key == NULL)
                    return -1;
                code = process_number(plist->memory, (gs_c_param_list *)plist, key, &p);
                if (code >= 0 && dict_count != NULL)
                    (*dict_count)++;
                single_done = single;
                key = NULL;
                break;
            case '>':
                if (dict_count != NULL && p[1] == '>') {
                    if (key != NULL)
                        return -1;
                    *pp = p;
                    return 0;
                }
                return -1;
            default:
                return -1;
                break;
        }
        if (code < 0)
            return code;
    }

    *pp = p;
    return 0;
}

/* Given a string to parse (a list of key/value pairs), parse it and add
 * what we find to the supplied param list.
 */
int gs_param_list_add_tokens(gs_param_list *plist, char *p)
{
    char *r = p;
    return add_tokens(plist, NULL, &r, NULL);
}

/* Given a key, and a string representing a single (maybe complex) value
 * to parse, parse it and add what we find to the supplied param list.
 */
int gs_param_list_add_parsed_value(gs_param_list *plist, gs_param_name key, const char *p)
{
    size_t len;
    char *q, *r;
    int code;

    /* Treat NULL as the empty string. */
    if (p == NULL)
        return 0;

    len = strlen(p) + 1;
    q = (char *)gs_alloc_bytes(plist->memory, len, "gs_param_list_add_parsed_value");
    if (q == NULL)
        return_error(gs_error_VMerror);
    memcpy(q, p, len);

    r = q;
    code = add_tokens(plist, key, &r, NULL);

    gs_free_object(plist->memory, q, "gs_param_list_add_parsed_value");

    return code;
}

typedef struct {
    char *value;
    int *len;
    char last;
} outstate;

static void
out_string(outstate *out, const char *str)
{
    int slen = str ? (int)strlen(str) : 0;

    if (slen == 0)
        return;

    if (out->last != 0 && out->last != ')' && out->last != '>' &&
        out->last != '[' && out->last != ']' && out->last != '}' &&
        *str != '(' && *str != ')' && *str != '<' && *str != '>' &&
        *str != '[' && *str != ']' && *str != '{' && *str != '}' &&
        *str != '/') {
        /* We need to insert some whitespace */
        *out->len += 1;
        if (out->value != NULL) {
            *out->value++ = ' ';
            *out->value = 0;
        }
    }

    *out->len += slen;
    out->last = str[slen-1];
    if (out->value != NULL) {
        memcpy(out->value, str, slen);
        out->value += slen;
        *out->value = 0;
    }
}

static void
string_to_string(const char *data, int len, outstate *out)
{
    int i;
    char text[4];
    const char *d = data;

    /* Check to see if we have any awkward chars */
    for (i = len; i != 0; i--) {
        if (*d < 32 || *d >= 127 || *d == ')')
            break;
        d++;
    }

    /* No awkward chars, do it the easy way. */
    if (i == 0) {
        d = data;
        out_string(out, "(");
        out->last = 0;
        text[1] = 0;
        for (i = len; i != 0; i--) {
            text[0] = *d++;
            out->last = 0;
            out_string(out, text);
        }
        out->last = 0;
        out_string(out, ")");
        return;
    }

    /* Output as hexstring */
    out_string(out, "<");
    text[2] = 0;
    for (i = 0; i < len; i++) {
        text[0] = "0123456789ABCDEF"[(*data >> 4) & 15];
        text[1] = "0123456789ABCDEF"[(*data++) & 15];
        out->last = 0;
        out_string(out, text);
    }
    out_string(out, ">");
}

static void
name_to_string(const char *data, int len, outstate *out)
{
    int i;
    char text[4];

    out_string(out, "/");
    text[3] = 0;
    for (i = 0; i < len; i++) {
        char c = *data++;
        if (c > 32 && c < 127 && c != '/' && c != '#' &&
            c != '<' && c != '>' &&
            c != '[' && c != ']' &&
            c != '(' && c != ')' &&
            c != '{' && c != '}') {
            text[0] = c;
            text[1] = 0;
        } else {
            text[0] = '#';
            text[1] = "0123456789ABCDEF"[(c >> 4) & 15];
            text[2] = "0123456789ABCDEF"[c & 15];
        }
        out->last = 0;
        out_string(out, text);
    }
}

static void
int_array_to_string(gs_param_int_array ia, outstate *out)
{
    int i;
    char text[32];

    out_string(out, "[");
    for (i = 0; i < ia.size; i++) {
        gs_snprintf(text, sizeof(text), "%d", ia.data[i]);
        out_string(out, text);
    }
    out_string(out, "]");
}

static void
print_float(char text[32], float f)
{
    /* We attempt to tidy up %f's somewhat unpredictable output
     * here, so rather than printing 0.10000000 we print 0.1 */
    char *p = text;
    int frac = 0;
    gs_snprintf(text, 32, "%f", f);
    /* Find the terminator, or 'e' to spot exponent mode. */
    while (*p && *p != 'e' && *p != 'E') {
        if (*p == '.')
            frac = 1;
        p++;
    }
    /* If we've hit the terminator, and passed a '.' at some point
     * we know we potentially have a tail to tidy up. */
    if (*p == 0 && frac) {
        p--;
        /* Clear a trail of 0's. */
        while (*p == '0')
            *p-- = 0;
        /* If we cleared the entire fractional part, remove the . */
        if (*p == '.') {
            /* Allow for -.0000 => -0 rather than - */
            if (p == text || p[-1] < '0' || p[-1] > '9')
                *p = '0', p[1] = 0;
            else
                p[0] = 0;
        }
    }
}

static void
float_array_to_string(gs_param_float_array fa, outstate *out)
{
    int i;
    char text[32];

    out_string(out, "[");
    for (i = 0; i < fa.size; i++) {
        print_float(text, fa.data[i]);
        out_string(out, text);
    }
    out_string(out, "]");
}

static void
string_array_to_string(gs_param_string_array sa, outstate *out)
{
    int i;

    out_string(out, "[");
    for (i = 0; i < sa.size; i++) {
        string_to_string((const char *)sa.data[i].data, sa.data[i].size, out);
    }
    out_string(out, "]");
}

static void
name_array_to_string(gs_param_string_array na, outstate *out)
{
    int i;

    out_string(out, "[");
    for (i = 0; i < na.size; i++) {
        name_to_string((const char *)na.data[i].data, na.data[i].size, out);
    }
    out_string(out, "]");
}

static int to_string(gs_param_list *plist, gs_param_name key, outstate *out);

static int
out_dict(gs_param_collection *dict, outstate *out)
{
    gs_param_list *plist = dict->list;
    gs_param_enumerator_t enumerator;
    gs_param_key_t key;
    int code;

    out_string(out, "<<");

    param_init_enumerator(&enumerator);
    while ((code = param_get_next_key(plist, &enumerator, &key)) == 0) {
        char string_key[256];	/* big enough for any reasonable key */

        if (key.size > sizeof(string_key) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        memcpy(string_key, key.data, key.size);
        string_key[key.size] = 0;
        name_to_string((char *)key.data, key.size, out);
        code = to_string(plist, string_key, out);
        if (code < 0)
            break;
    }

    out_string(out, ">>");
    if (code == 1)
        code = 0;

    return code;
}

static int
to_string(gs_param_list *plist, gs_param_name key, outstate *out)
{
    int code = 0;
    gs_param_typed_value pvalue;

    pvalue.type = gs_param_type_any;
    code = param_read_typed(plist, key, &pvalue);
    if (code < 0)
        return code;
    if (code > 0)
        return_error(gs_error_undefined);
    switch (pvalue.type) {
    case gs_param_type_null:
        out_string(out, "null");
        break;
    case gs_param_type_bool:
        if (pvalue.value.b)
            out_string(out, "true");
        else
            out_string(out, "false");
        break;
    case gs_param_type_int:
    {
        char text[32];
        gs_snprintf(text, sizeof(text), "%d", pvalue.value.i);
        out_string(out, text);
        break;
    }
    case gs_param_type_i64:
    {
        char text[32];
        gs_snprintf(text, sizeof(text), "%"PRId64, pvalue.value.i64);
        out_string(out, text);
        break;
    }
    case gs_param_type_long:
    {
        char text[32];
        gs_snprintf(text, sizeof(text), "%ld", pvalue.value.l);
        out_string(out, text);
        break;
    }
    case gs_param_type_size_t:
    {
        char text[32];
        gs_snprintf(text, sizeof(text), "%"PRIdSIZE, pvalue.value.z);
        out_string(out, text);
        break;
    }
    case gs_param_type_float:
    {
        char text[32];
        print_float(text, pvalue.value.f);
        out_string(out, text);
        break;
    }
    case gs_param_type_dict:
        code = out_dict(&pvalue.value.d, out);
        break;
    case gs_param_type_dict_int_keys:
        return -1;
    case gs_param_type_array:
        return -1;
    case gs_param_type_string:
        string_to_string((char *)pvalue.value.s.data, pvalue.value.s.size, out);
        break;
    case gs_param_type_name:
        name_to_string((char *)pvalue.value.n.data, pvalue.value.n.size, out);
        break;
    case gs_param_type_int_array:
        int_array_to_string(pvalue.value.ia, out);
        break;
    case gs_param_type_float_array:
        float_array_to_string(pvalue.value.fa, out);
        break;
    case gs_param_type_string_array:
        string_array_to_string(pvalue.value.sa, out);
        break;
    case gs_param_type_name_array:
        name_array_to_string(pvalue.value.na, out);
        break;
    default:
        return -1;
    }

    return code;
}

int gs_param_list_to_string(gs_param_list *plist, gs_param_name key, char *value, int *len)
{
    outstate out;

    out.value = value;
    out.len = len;
    out.last = 0;
    *len = 1; /* Always space for the terminator. */
    if (value)
        *value = 0;
    return to_string(plist, key, &out);
}

int gs_param_list_dump(gs_param_list *plist)
{
    gs_param_enumerator_t enumerator;
    gs_param_key_t key;
    int code;
    char buffer[4096];
    int len;

    param_init_enumerator(&enumerator);
    while ((code = param_get_next_key(plist, &enumerator, &key)) == 0) {
        char string_key[256];	/* big enough for any reasonable key */

        if (key.size > sizeof(string_key) - 1) {
            code = gs_note_error(gs_error_rangecheck);
            break;
        }
        memcpy(string_key, key.data, key.size);
        string_key[key.size] = 0;
        dlprintf1("%s ", string_key);
        code = gs_param_list_to_string(plist, string_key, buffer, &len);
        if (code < 0)
            break;
        dlprintf1("%s ", buffer);
    }
    dlprintf("\n");
    return code;
}
