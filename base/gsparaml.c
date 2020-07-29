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


/* gsparaml.c -  Handling of reading lists of params from strings */

#include <stdlib.h>
#include "gsparam.h"
#include "gserrors.h"
#include "string_.h"

/* We need to know how many items are in an array or dictionary */
static int count_tokens(char *p)
{
    int tokens = 0;

    while (*p != 0x00){
        switch (*p) {
            case ' ':
                p++;
                break;
            case 'f':
                if (strncmp(p, "false", 5) != 0)
                    return -1;
                p += 5;
                tokens++;
                break;
            case 't':
                if (strncmp(p, "true", 4) != 0)
                    return -1;
                p += 4;
                tokens++;
                break;
            case '<':
                if (p[1] == '>') {
                    int nesting = 0;

                    p+=2;
                    while (*p != 0x00) {
                        if (*p == '>' && p[1] == '>') {
                            p++;
                            if (nesting == 0)
                                break;
                        } else
                            if (*p == '<' && p[1] == '<') {
                                p += 2;
                                nesting++;
                            } else
                                p++;
                    }
                    if (*p == 0x00)
                        return -1;
                    tokens++;
                    p++;
                } else {
                    while (*p != 0x00 && *p != '>')
                        p++;
                    if (*p == 0x00)
                        return -1;
                    tokens++;
                    p++;
                }
                break;
            case '/':
                while (*p != 0x00 && *p != ' ')
                    p++;
                tokens++;
                break;
            case '(':
                while (*p != 0x00 && *p != ')')
                    p++;
                if (*p == 0x00)
                    return -1;
                p++;
                tokens++;
                break;
            case '[':
                while (*p != 0x00 && *p != ']')
                    p++;
                if (*p == 0x00)
                    return -1;
                p++;
                tokens++;
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
                while (*p != 0x00 && *p != ' ') {
                    if ((*p < 0x30 || *p > 0x39) && *p != '.')
                        return -1;
                    p++;
                }
                tokens++;
                break;
            default:
                return -1;
                break;
        }
    }
    return tokens;
}

/* Dictionaries are surprisingly easy, we just make a param_dict
 * and then call the existing routine to parse the string and
 * add tokens to the parameter list contained in the dictionary.
 */
static int
process_dict(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    gs_param_dict dict;
    int code, tokens, nested = 0;
    char *p1 = *p, *start = *p;

    /* for now we won't handle nested dicts */
    do {
        if (*p1 != '>' && *p1 != 0x00) {
            if (*p1 == '<' && *(p1 + 1) == '<') {
                nested++;
                p1 += 2;
            } else
                p1++;
        } else {
            if (*(p1 + 1) != '>' && *(p1 + 1) != 0x00) {
                p1++;
            } else {
                if (!nested)
                    break;
                nested--;
                p1 += 2;
            }
        }
    } while (1);

    if (*p1 == 0x00)
        return -1;

    /* NULL out the dictionary end marks */
    *p1++ = 0x00;
    if (*p1 == 0x00)
        return -1;
    *p1 = 0x00;

    /* Move the string pointer past the end of the dictionary so that
     * parsing will continue correctly
     */
    *p = p1 + 1;

    p1 = start;

    tokens = count_tokens(p1);
    dict.size = tokens;

    code = param_begin_write_dict((gs_param_list *)plist, key, &dict, false);
    if (code < 0)
        return code;

    gs_param_list_set_persistent_keys(dict.list, false);

    code = gs_param_list_add_tokens(dict.list, p1);
    if (code < 0)
        return code;

    code = param_end_write_dict((gs_param_list *)plist, key, &dict);
    return code;
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

    src = dest = p1;
    while (*src != 0x00 && *src != '>') {
        data = 0;
        for (i=0;i<2;i++) {
            if(*src >= '0' && *src <= '9') {
                data = (data << 4);
                data += (*src - '0');
            } else {
                if (*src >= 'A' && *src <= 'F') {
                    data = (data << 4);
                    data += (*src - 'A' + 10);
                } else {
                    if (*src >= 'a' && *src <= 'f') {
                        data = (data << 4);
                        data += (*src - 'a' + 10);
                    } else {
                        return -1;
                    }
                }
            }
            src++;
        }
        *dest++ = data;
    }
    *dest = 0x00;

    *p = dest + 1;

    ps.data = (const byte *)p1;
    ps.size = strlen(p1);
    ps.persistent = false;
    return param_write_string((gs_param_list *)plist, key, &ps);
}

static int
process_name(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name *key, char **p)
{
    char *start = *p + 1;
    gs_param_string ps;

    while (**p != ' ' && **p != 0x00)
        (*p)++;

    if (**p != 0x00) {
        **p = 0x00;
        (*p)++;
    }

    if (*key == NULL)
        *key = (gs_param_name)start;
    else {
        ps.data = (const byte *)start;
        ps.size = strlen(start);
        ps.persistent = false;
        param_write_name((gs_param_list *)plist, *key, &ps);
        *key = NULL;
    }
    return 0;
}

static int
process_string(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *start = *p + 1;
    gs_param_string ps;

    while (**p != ')' && **p != 0x00)
        (*p)++;

    if (**p != ')')
        return -1;

    **p= 0x00;
    (*p)++;

    ps.data = (const byte *)start;
    ps.size = strlen(start);
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
    int tokens = 0, nested = 0, index = 0, code = 0;
    gs_param_type array_type = gs_param_type_null;
    char *start = *p + 1, *p1 = start;
    gs_param_string *parray = 0L;
    char *array_data = 0x00;
    gs_param_string_array string_array;
    gs_param_int_array int_array;
    gs_param_float_array float_array;

    /* for now we won't handle nested arrays */
    do {
        while (*p1 != ']' && *p1 != 0x00) {
            if (*p1 == '[')
                nested++;
            p1++;
        }
    } while (nested--);

    if (*p1 == 0x00)
        return -1;

    *p1 = 0x00;
    p1 = start;

    tokens = count_tokens(start);

    while (*p1 != 0x00 && code == 0){
        switch (*p1) {
            case ' ':
                p1++;
                break;

            case 'f':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (array_type == gs_param_type_null) {
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in param parsing");
                    if (array_data == NULL){
                        code = gs_error_VMerror;
                        break;
                    }
                    array_type = gs_param_type_string_array;
                }
                if (strncmp(p1, "false", 5) == 0) {
                    parray = (gs_param_string *)array_data;
                    parray[index].data = (const byte *)p1;
                    p1 += 5;
                    *p1++ = 0x00;
                    parray[index].size = 5;
                    parray[index++].persistent = false;
                } else {
                    code = gs_error_typecheck;
                    break;
                }
                break;

            case 't':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (array_type == gs_param_type_null) {
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in param parsing");
                    if (array_data == NULL){
                        code = gs_error_VMerror;
                        break;
                    }
                    array_type = gs_param_type_string_array;
                }
                if (strncmp(p1, "true", 4) == 0) {
                    parray = (gs_param_string *)array_data;
                    parray[index].data = (const byte *)p1;
                    p1 += 4;
                    *p1++ = 0x00;
                    parray[index].size = 4;
                    parray[index++].persistent = false;
                } else {
                    code = gs_error_typecheck;
                    break;
                }
                break;

            case '<':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (array_type == gs_param_type_null) {
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in param parsing");
                    if (array_data == NULL){
                        code = gs_error_VMerror;
                        break;
                    }
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
                    while (*src != 0x00 && *src != '>') {
                        data = 0;
                        for (i=0;i<2;i++) {
                            if(*src >= '0' && *src <= '9') {
                                data = (data << 4);
                                data += (*src - '0');
                            } else {
                                if (*src >= 'A' && *src <= 'F') {
                                    data = (data << 4);
                                    data += (*src - 'A' + 10);
                                } else {
                                    if (*src >= 'a' && *src <= 'f') {
                                        data = (data << 4);
                                        data += (*src - 'a' + 10);
                                    } else {
                                        return -1;
                                    }
                                }
                            }
                            src++;
                        }
                        *dest++ = data;
                    }
                    *dest = 0x00;
                    parray[index].size = strlen((char *)(parray[index].data));
                    parray[index++].persistent = false;
                }
                break;

            case '/':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (array_type == gs_param_type_null) {
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in param parsing");
                    if (array_data == NULL){
                        code = gs_error_VMerror;
                        break;
                    }
                    array_type = gs_param_type_string_array;
                }
                parray = (gs_param_string *)array_data;
                parray[index].data = (const byte *)p1;
                while (*p1 != ' ' && *p1 != 0x00)
                    p1++;
                if (*p1 == 0x00)
                    return -1;
                *p1++ = 0x00;
                parray[index].size = strlen((char *)(parray[index].data));
                parray[index++].persistent = false;
                break;

            case '(':
                if (array_type != gs_param_type_null && array_type != gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                }
                if (array_type == gs_param_type_null) {
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in param parsing");
                    if (array_data == NULL){
                        code = gs_error_VMerror;
                        break;
                    }
                    array_type = gs_param_type_string_array;
                }
                parray = (gs_param_string *)array_data;
                parray[index].data = (const byte *)p1;
                while (*p1 != ')' && *p1 != 0x00)
                    p1++;
                if (*p1 == 0x00)
                    return -1;
                *p1++ = 0x00;
                parray[index].size = strlen((char *)(parray[index].data));
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
                if (array_type == gs_param_type_string_array) {
                    code = gs_error_typecheck;
                    break;
                } else {
                    int integer = 1;
                    char *c = p1;
                    float *floats;
                    int *ints, i;

                    integer = 1;
                    while (*p1 != 0x00 && ((*p1 >= '0' && *p1 <= '9') || *p1 == '.')) {
                        if (*p1 == '.')
                            integer = 0;
                        p1++;
                    }
                    if (*p1 == 0x00)
                        return -1;
                    *p1++ = 0x00;

                    if (array_type == gs_param_type_null) {
                        if (integer) {
                            ints = (int *)gs_alloc_bytes(mem, sizeof(int) * tokens, "param string array in param parsing");
                            if (ints == NULL){
                                code = gs_error_VMerror;
                                break;
                            }
                            array_type = gs_param_type_int_array;
                            array_data = (char *)ints;
                        } else {
                            floats = (float *)gs_alloc_bytes(mem, sizeof(float) * tokens, "param string array in param parsing");
                            if (floats == NULL){
                                code = gs_error_VMerror;
                                break;
                            }
                            array_type = gs_param_type_float_array;
                            array_data = (char *)floats;
                        }
                    }
                    if (array_type == gs_param_type_int_array && !integer) {
                        ints = (int *)array_data;
                        floats = (float *)gs_alloc_bytes(mem, sizeof(float) * tokens, "param string array in param parsing");
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
                    if (array_type == gs_param_type_int_array) {
                        ints = (int *)array_data;
                        ints[index++] = (int)atoi(c);
                    } else {
                        floats = (float *)array_data;
                        floats[index++] = (float)atof(c);
                    }
                }
                break;
            default:
                code = gs_error_typecheck;
                break;
        }
    }

    /* Now we have to deal with adding the array to the parm list, there are
     * (of course!) different calls for each array type....
     */
    switch(array_type) {
        case gs_param_type_string_array:
            string_array.data = (const gs_param_string *)array_data;
            string_array.persistent = 0;
            string_array.size = tokens;
            code = param_write_string_array((gs_param_list *)plist, key, &string_array);
            break;
        case gs_param_type_int_array:
            int_array.data = (const int *)array_data;
            int_array.persistent = 0;
            int_array.size = tokens;
            code = param_write_int_array((gs_param_list *)plist, key, &int_array);
            break;
        case gs_param_type_float_array:
            float_array.data = (const float *)array_data;
            float_array.persistent = 0;
            float_array.size = tokens;
            code = param_write_float_array((gs_param_list *)plist, key, &float_array);
            break;

        default:
            break;
    }

    /* And now we can throw away the array data, we copied it to the param list. */
    gs_free_object(mem, array_data, "param string array in param parsing");

    /* Update the string pointer to point past the 0x00 byte marking the end of the array */
    *p = p1 + 1;

    return code;
}

static int
process_number(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *start = *p;
    bool integer = true;
    int code = 0;

    while (**p != ' ' && **p != 0x00) {
        if (**p == '.')
            integer = false;

        (*p)++;
    }

    if (**p != 0x00) {
        **p = 0x00;
        (*p)++;
    }

    if (!integer) {
        float f = (float)atof(start);
        code = param_write_float((gs_param_list *)plist, key, (float *)&f);
    } else {
        long i = atol(start);
        code = param_write_long((gs_param_list *)plist, key, &i);
    }

    return code;
}

static int
add_tokens(gs_param_list *plist, gs_param_name key, char *p)
{
    int code = 0;
    /* If single == true, then we are looking for a single value,
     * otherwise it's a list of key/value pairs */
    int single = (key != NULL);
    /* If single_done, then we've read our single value. Any non
     * whitespace we read is an error. */
    int single_done = 0;
    bool f = false, t = true;

    while (*p != 0x00) {
        switch (*p) {
            case ' ':
                p++;
                break;
            case 'f':
                if (single_done || key == NULL)
                    return -1;
                if (strncmp(p, "false", 5) != 0)
                    return -1;
                code = param_write_bool((gs_param_list *)plist, key, &f);
                p += 5;
                single_done = single;
                key = NULL;
                break;
            case 't':
                if (single_done || key == NULL)
                    return -1;
                if (strncmp(p, "true", 4) != 0)
                    return -1;
                code = param_write_bool((gs_param_list *)plist, key, &t);
                p += 4;
                single_done = single;
                key = NULL;
                break;
            case '<':
                if (single_done || key == NULL)
                    return -1;
                code = process_dict_or_hexstring(plist->memory, (gs_c_param_list *)plist, key, &p);
                single_done = single;
                key = NULL;
                break;
            case '/':
            {
                int have_key = (key != NULL);
                if (single_done)
                    return -1;
                code = process_name(plist->memory, (gs_c_param_list *)plist, &key, &p);
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
                single_done = single;
                key = NULL;
                break;
            case '[':
                if (single_done || key == NULL)
                    return -1;
                code = process_array(plist->memory, (gs_c_param_list *)plist, key, &p);
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
                single_done = single;
                key = NULL;
                break;
            default:
                return -1;
                break;
        }
        if (code < 0)
            return code;
    }

    return 0;
}

/* Given a string to parse (a list of key/value pairs), parse it and add
 * what we find to the supplied param list.
 */
int gs_param_list_add_tokens(gs_param_list *plist, char *p)
{
    return add_tokens(plist, NULL, p);
}

/* Given a key, and a string representing a single (maybe complex) value
 * to parse, parse it and add what we find to the supplied param list.
 */
int gs_param_list_add_parsed_value(gs_param_list *plist, gs_param_name key, char *p)
{
    return add_tokens(plist, key, p);
}

