/* Copyright (C) 2001-2019 Artifex Software, Inc.
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


/* plparams.c -  PJL handling of pdfwrite device parameters */

#include "std.h"
#include "gsmemory.h"
#include "gsmatrix.h"           /* for gsdevice.h */
#include "gsdevice.h"
#include "gsparam.h"
#include "gp.h"
#include "gserrors.h"
#include "string_.h"
#include "plparams.h"
#include <stdlib.h>

static int pjl_dist_process_dict(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p);
static int count_tokens(char *p);
static int pjl_dist_process_dict_or_hexstring(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p);
static int pjl_dist_process_string(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p);
static int pjl_dist_add_tokens_to_list(gs_param_list *, char **);
static int pjl_dist_process_number(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p);
static int pjl_dist_process_array(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p);
static int pjl_dist_process_name(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name *key, char **p);

/*--------------------------------------------- pdfmark -------------------------------------------*/

/* Add a newly created param_string array to a param list, and then use that
 * param list as an argument to put the device parameters.
 */
static int pdfmark_write_list(gs_memory_t *mem, gx_device *device, gs_param_string_array *array_list)
{
    gs_c_param_list list;
    int code;

    /* Set the list to writeable, and initialise it */
    gs_c_param_list_write(&list, mem);
    /* We don't want keys to be persistent, as we are going to throw
     * away our array, force them to be copied
     */
    gs_param_list_set_persistent_keys((gs_param_list *) &list, false);

    /* Make really sure the list is writable, but don't initialise it */
    gs_c_param_list_write_more(&list);

    /* Add the param string array to the list */
    code = param_write_string_array((gs_param_list *)&list, "pdfmark", array_list);
    if (code < 0)
        return code;

    /* Set the param list back to readable, so putceviceparams can readit (mad...) */
    gs_c_param_list_read(&list);

    /* and set the actual device parameters */
    code = gs_putdeviceparams(device, (gs_param_list *)&list);

    return code;
}

/* pdfmark operations always take an array parameter. Because (see below) we
 * know that array valuess must be homogenous types we can treat them all as
 * parameter strings.
 */
static int process_pdfmark(gs_memory_t *mem, gx_device *device, char *pdfmark)
{
    char *p, *start, *copy, *stream_data = 0L;
    int tokens = 0, code = 0;
    gs_param_string *parray;
    gs_param_string_array array_list;
    bool putdict = false;

    /* Our parsing will alter the string contents, so copy it and perform the parsing on the copy */
    copy = (char *)gs_alloc_bytes(mem, strlen(pdfmark) + 1, "working buffer for pdfmark processing");
    if (copy == 0)
        return -1;
    strcpy(copy, pdfmark);

    start = copy + 1;
    if (*pdfmark != '[') {
        gs_free_object(mem, copy, "working buffer for pdfmark processing");
        return -1;
    }

    p = start;
    while (*p != 0x00){
        if(*p == '(') {
            while (*p != ')' && *p != 0x00) {
                if (*p == '\\')
                    p++;
                p++;
            }
            if (*p != ')') {
                gs_free_object(mem, copy, "working buffer for pdfmark processing");
                return -1;
            }
        } else {
            if (*p == ' ') {
                tokens++;
            }
        }
        p++;
    }

    if (*(p-1) != ' ')
        tokens++;

    /* We need an extra one for a dummy CTM */
    tokens++;

    parray = (gs_param_string *)gs_alloc_bytes(mem, tokens * sizeof(gs_param_string), "temporary pdfmark array");
    if (!parray) {
        gs_free_object(mem, copy, "working buffer for pdfmark processing");
        return -1;
    }

    tokens = 0;
    while (*start == ' ')
        start++;
    p = start;

    while (*p != 0x00){
        if(*p == '(') {
            while (*p != ')' && *p != 0x00) {
                if (*p == '\\')
                    p++;
                p++;
            }
            if (*p != ')') {
                gs_free_object(mem, copy, "working buffer for pdfmark processing");
                return -1;
            }
        } else {
            if (*p == ' ') {
                if (strncmp(start, "<<", 2) == 0) {
                    putdict = true;
                } else {
                    if (strncmp(start, ">>", 2) != 0) {
                        *p = 0x00;
                        parray[tokens].data = (const byte *)start;
                        parray[tokens].size = strlen(start);
                        parray[tokens++].persistent = false;
                    }
                }
                start = ++p;
            } else
                p++;
        }
    }
    if (*(p-1) != ' ') {
        parray[tokens].data = (const byte *)start;
        parray[tokens].size = strlen(start);
        parray[tokens++].persistent = false;
    }

    /* Move last entry up one and add a dummy CTM where it used to be */
    parray[tokens].data = parray[tokens - 1].data;
    parray[tokens].size = parray[tokens - 1].size;
    parray[tokens].persistent = parray[tokens - 1].persistent;
    parray[tokens - 1].data = (const byte *)"[0 0 0 0 0 0]";
    parray[tokens - 1].size = 13;
    parray[tokens - 1].persistent = false;

    /* Features are name objects (ie they start with a '/') but putdeviceparams wants them
     * as strings without the '/', so we need to strip that here.
     */
    if (parray[tokens].data[0] != '/') {
        gs_free_object(mem, copy, "working buffer for pdfmark processing");
        gs_free_object(mem, parray, "temporary pdfmark array");
        return -1;
    } else {
        parray[tokens].data++;
        parray[tokens].size--;
    }

    /* We need to convert a 'PUT' with a dictonary into a 'PUTDICT', this is normally done
     * in PostScript
     */
    if (putdict && strncmp((const char *)(parray[tokens].data), "PUT", 3) == 0) {
        parray[tokens].data = (const byte *)".PUTDICT";
        parray[tokens].size = 8;
        parray[tokens].persistent = false;
    }
    /* We also need some means to handle file data. Normally ths is done by creating a
     * PostScript file object and doing a 'PUT', but we can't do that, so we define a
     * special variety of 'PUT' called 'PUTFILE' and we handle that here.
     */
    if (strncmp((const char *)(parray[tokens].data), "PUTFILE", 7) == 0) {
        FILE *f;
        char *filename;
        int bytes;

        if (parray[tokens - 2].data[0] != '(') {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            return -1;
        }
        filename = (char *)&(parray[tokens - 2].data[1]);
        filename[strlen(filename) - 1] = 0x00;

        f = gp_fopen((const char *)filename, "rb");
        if (!f) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            return -1;
        }

        if (gp_fseek_64(f, 0, SEEK_END) != 0) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            fclose(f);
            return -1;
        }
        bytes = gp_ftell_64(f);
        if (bytes < 0) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            fclose(f);
            return -1;
        }
            
        parray[tokens - 2].data = (const byte *)gs_alloc_bytes(mem, bytes, "PJL pdfmark, stream");
        if (!parray[tokens - 2].data) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            fclose(f);
            return -1;
        }
        stream_data = (char *)(parray[tokens - 2].data);

        if (gp_fseek_64(f, 0, SEEK_SET) != 0) {
            gs_free_object(mem, stream_data, "PJL pdfmark, stream");
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            fclose(f);
            return gs_note_error(gs_error_ioerror);
        }
        code = fread(stream_data, 1, bytes, f);
        if (code != bytes) {
            gs_free_object(mem, stream_data, "PJL pdfmark, stream");
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            fclose(f);
            return gs_note_error(gs_error_ioerror);
        }
        fclose(f);
        parray[tokens - 2].size = bytes;

        parray[tokens].data = (const byte *)".PUTSTREAM";
        parray[tokens].size = 10;
        parray[tokens].persistent = false;
    }

    array_list.data = parray;
    array_list.persistent = 0;
    array_list.size = ++tokens;

    code = pdfmark_write_list(mem, device, &array_list);

    if (stream_data)
        gs_free_object(mem, stream_data, "PJL pdfmark, stream");
    gs_free_object(mem, copy, "working buffer for pdfmark processing");
    gs_free_object(mem, parray, "temporary pdfmark array");

    return code;
}

int pcl_pjl_pdfmark(gs_memory_t *mem, gx_device *device, char *pdfmark)
{
    char *pdfmark_start, *token_start, end, *p;
    int code;

    p = token_start = pdfmark_start = pdfmark + 1;

    do {
        while (*p != ' ' && *p != '"' && *p != 0x00)
            p++;
        if((p - token_start) != 7 || strncmp(token_start, "pdfmark", 7) != 0){
            if (*p != 0x00)
                token_start = ++p;
            else
                break;
        } else {
            token_start--;
            end = *token_start;
            *token_start = 0x00;
            code = process_pdfmark(mem, device, pdfmark_start);
            if (code < 0)
                return code;

            *token_start = end;
            token_start = pdfmark_start = ++p;
        }
    } while (*p != 0x00);
    return 0;
}

/*--------------------------------------------- distillerparams -------------------------------------------*/

/* Dictionaries are surprisingly easy, we just make a param_dict
 * and then call the existing routine to parse the string and
 * add tokens to the parameter list contained in the dictionary.
 */
static int pjl_dist_process_dict(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
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
                if (nested)
                    nested--;
                else
                    break;
                p1 += 2;
            }
        }
    }while (1);

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

    code = pjl_dist_add_tokens_to_list(dict.list, &p1);
    if (code < 0)
        return code;

    code = param_end_write_dict((gs_param_list *)plist, key, &dict);
    return code;
}

static int pjl_dist_process_dict_or_hexstring(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *p1 = *p, *src, *dest, data;
    int i;
    gs_param_string ps;

    if (p1[1] == '<') {
        *p += 2;
        return pjl_dist_process_dict(mem, plist, key, p);
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

static int pjl_dist_process_name(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name *key, char **p)
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

static int pjl_dist_process_string(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
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
                if (strncmp(p, "false", 5) == 0) {
                    p += 5;
                    tokens++;
                } else {
                    return -1;
                }
                break;
            case 't':
                if (strncmp(p, "true", 4) == 0) {
                    p += 4;
                    tokens++;
                } else {
                    return -1;
                }
                break;
            case '<':
                if (p[1] == '>') {
                    int nesting = 0;

                    p+=2;
                    while (*p != 0x00) {
                        if (*p == '>' && p[1] == '>') {
                            if (nesting == 0) {
                                p++;
                                break;
                            }
                            p++;
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
                    while (*p != 0x00 && *p != '>') {
                        p++;
                    }
                    if (*p == 0x00)
                        return -1;
                    tokens++;
                    p++;
                }
                break;
            case '/':
                while (*p != 0x00 && *p != ' ') {
                    p++;
                }
                tokens++;
                break;
            case '(':
                while (*p != 0x00 && *p != ')') {
                    p++;
                }
                if (*p == 0x00)
                    return -1;
                p++;
                tokens++;
                break;
            case '[':
                while (*p != 0x00 && *p != ']') {
                    p++;
                }
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

/* Arrays are *way* more complicated than dicts :-(
 * We have 4 different kinds of arrays; name, string, int and float
 * It seems that parameter arrays can only contain homogenous data, it
 * all has to be of the same type. This complicates matters because we
 * can't know in advance what the type is!
 *
 * So we only handle 3 types of array; int, float and string. Anything
 * which isn't one of those either gets converted to a string or (arrays and
 * dictionaries) throws an error.
 *
 * For numbers, we look at the first element, if its an integer we make
 * an int array otherwise we make a float array. If we start an int array
 *  and later encounter a float, we make a new float array, copy the existing
 * integers into it (converting to floats) and throw away the old int array.
 *
 * Otherwise if we encounter an object whose type doesnt' match the array we
 * created we throw an error.
 */
static int pjl_dist_process_array(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
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
    }while (nested--);

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
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
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
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
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
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
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
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
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
                    array_data = (char *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
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
                            ints = (int *)gs_alloc_bytes(mem, sizeof(int) * tokens, "param string array in distillerparams");
                            if (ints == NULL){
                                code = gs_error_VMerror;
                                break;
                            }
                            array_type = gs_param_type_int_array;
                            array_data = (char *)ints;
                        } else {
                            floats = (float *)gs_alloc_bytes(mem, sizeof(float) * tokens, "param string array in distillerparams");
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
                        floats = (float *)gs_alloc_bytes(mem, sizeof(float) * tokens, "param string array in distillerparams");
                        if (floats == NULL){
                            code = gs_error_VMerror;
                            break;
                        }
                        array_type = gs_param_type_float_array;
                        for (i=0;i<index;i++){
                            floats[i] = (float)(ints[i]);
                        }
                        gs_free_object(mem, ints, "param string array in distillerparams");
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
    gs_free_object(mem, array_data, "param string array in distillerparams");

    /* Update the string pointer to point past the 0x00 byte marking the end of the array */
    *p = p1 + 1;

    return code;
}

static int pjl_dist_process_number(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
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

/* Given a string to parse, parse it and add what we find to the supplied
 * param list.
 */
static int pjl_dist_add_tokens_to_list(gs_param_list *plist, char **p)
{
    char *p1 = *p;
    int code = 0;
    gs_param_name key = NULL;

    while (*p1 != 0x00){
        switch (*p1) {
            case ' ':
                p1++;
                break;
            case 'f':
                if (strncmp(p1, "false", 5) == 0) {
                    bool t = false;
                    code = param_write_bool((gs_param_list *)plist, key, &t);
                    p1 += 5;
                    key = NULL;
                } else {
                    return -1;
                }
                break;
            case 't':
                if (strncmp(p1, "true", 4) == 0) {
                    bool t = true;
                    code = param_write_bool((gs_param_list *)plist, key, &t);
                    p1 += 4;
                    key = NULL;
                } else {
                    return -1;
                }
                break;
            case '<':
                if (key == NULL) {
                    return -1;
                }
                code = pjl_dist_process_dict_or_hexstring(plist->memory, (gs_c_param_list *)plist, key, &p1);
                key = NULL;
                break;
            case '/':
                code = pjl_dist_process_name(plist->memory, (gs_c_param_list *)plist, &key, &p1);
                break;
            case '(':
                if (key == NULL) {
                    return -1;
                }
                code = pjl_dist_process_string(plist->memory, (gs_c_param_list *)plist, key, &p1);
                key = NULL;
                break;
            case '[':
                if (key == NULL) {
                    return -1;
                }
                code = pjl_dist_process_array(plist->memory, (gs_c_param_list *)plist, key, &p1);
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
                if (key == NULL) {
                    return -1;
                }
                code = pjl_dist_process_number(plist->memory, (gs_c_param_list *)plist, key, &p1);
                key = NULL;
                break;
            default:
                return -1;
                break;
        }
        if (code < 0) {
            return code;
        }
    }

    return 0;
}

int pcl_pjl_setdistillerparams(gs_memory_t *mem, gx_device *device, char *distillerparams)
{
    char *p, *start, *copy;
    int code = 0;
    gs_c_param_list *plist;

    plist = gs_c_param_list_alloc(mem, "temp C param list for PJL distillerparams");
    if (plist == NULL)
        return -1;
    gs_c_param_list_write(plist, mem);
    gs_param_list_set_persistent_keys((gs_param_list *) plist, false);
    gs_c_param_list_write_more(plist);

    /* Our parsing will alter the string contents, so copy it and perform the parsing on the copy */
    copy = (char *)gs_alloc_bytes(mem, strlen(distillerparams) + 1, "working buffer for distillerparams processing");
    if (copy == 0)
        return -1;
    strcpy(copy, distillerparams);

    if (*copy == '"') {
        start = copy + 1;
        copy[strlen(copy) - 1] = 0x00;
    }
    else
        start = copy;

    if (*start != '<' || start[1] != '<') {
        gs_free_object(mem, copy, "working buffer for distillerparams processing");
        return -1;
    }
    start += 2;

    if (copy[strlen(copy) - 1] != '>' || copy[strlen(copy) - 2] != '>') {
        gs_free_object(mem, copy, "working buffer for distillerparams processing");
        return -1;
    }
    copy[strlen(copy) - 2] = 0x00;

    while (*start == ' ')
        start++;
    p = start;

    /* Parse the string, adding what we find to the plist */
    code = pjl_dist_add_tokens_to_list((gs_param_list *)plist, &p);
    if (code < 0) {
        gs_c_param_list_release(plist);
        return code;
    }

    /* Discard our working copy of the string */
    gs_free_object(mem, copy, "working buffer for distillerparams processing");

    /* Set the list to 'read' (bonkers...) */
    gs_c_param_list_read(plist);

    /* Actually set the newly created device parameters */
    code = gs_putdeviceparams(device, (gs_param_list *)plist);

    /* And throw away the plist, we're done with it */
    gs_c_param_list_release(plist);
    return code;
}
