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


/* plparams.c -  PJL handling of common device parameters */

#include "std.h"
#include "gsmemory.h"
#include "gsmatrix.h"           /* for gsdevice.h */
#include "gsdevice.h"
#include "gsparam.h"
#include "gp.h"

static int pdfmark_write_list(gs_memory_t *mem, gx_device *device, gs_param_string_array *array_list)
{
    gs_c_param_list list;
    int code;

    gs_c_param_list_write(&list, mem);
    gs_param_list_set_persistent_keys((gs_param_list *) &list, false);
    gs_c_param_list_write_more(&list);
    code = param_write_string_array((gs_param_list *)&list, "pdfmark", array_list);
    if (code < 0)
        return code;

    gs_c_param_list_read(&list);
    code = gs_putdeviceparams(device, (gs_param_list *)&list);

    return code;
}

static int process_pdfmark(gs_memory_t *mem, gx_device *device, char *pdfmark)
{
    char *p, *type, *start, *copy, *stream_data = 0L;
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
     * as strings without the #/#, so we need to strip that here.
     */
    if (parray[tokens].data[0] != '/') {
        gs_free_object(mem, copy, "working buffer for pdfmark processing");
        gs_free_object(mem, parray, "temporary pdfmark array");
        return -1;
    } else {
        parray[tokens].data++;
        parray[tokens].size--;
    }

    /* We need to convert a 'PUT' with a dictonayr into a 'PUTDICT', this is normally done
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

        gp_fseek_64(f, 0, SEEK_END);
        bytes = gp_ftell_64(f);
        parray[tokens - 2].data = (const byte *)gs_alloc_bytes(mem, bytes, "PJL pdfmark, stream");
        if (!parray[tokens - 2].data) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            return -1;
        }
        stream_data = (char *)(parray[tokens - 2].data);

        gp_fseek_64(f, 0, SEEK_SET);
        fread(stream_data, 1, bytes, f);
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
            process_pdfmark(mem, device, pdfmark_start);
            *token_start = end;
            token_start = pdfmark_start = ++p;
        }
    } while (*p != 0x00);
    return 0;
}

int pjl_dist_process_dict(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    return 0;
}

int pjl_dist_process_dict_or_hexstring(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    if (*p[1] == '<') {
        return pjl_dist_process_dict(mem, plist, key, p);
    }
    return 0;
}

int pjl_dist_process_name(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name *key, char **p)
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

int pjl_dist_process_string(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
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
    param_write_string((gs_param_list *)plist, key, &ps);

    return 0;
}

int count_tokens(char *p)
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

int pjl_dist_process_array(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    int tokens = 0, nested = 0, index = 0, code;
    char *start = *p + 1, *p1 = start;
    gs_param_string *parray;
    gs_param_string_array *array_list;

    /* for now we won't handle nested arrays */
    do {
        while (*p1 != ']' && *p1 != 0x00) {
            if (*p1 == '[')
                nested++;
            *p1++;
        }
    }while (nested--);

    if (*p1 == 0x00)
        return -1;

    *p1 = 0x00;
    p1 = start;

    tokens = count_tokens(start);

    array_list = (gs_param_string_array *)gs_alloc_bytes(mem, sizeof(gs_param_string_array), "array in distillerparams");
    if (array_list == NULL)
        return -1;

    parray = (gs_param_string *)gs_alloc_bytes(mem, sizeof(gs_param_string) * tokens, "param string array in distillerparams");
    if (parray == NULL){
        gs_free_object(mem, array_list, "working buffer for distillerparams processing");
        return -1;
    }

    while (*p1 != 0x00){
        switch (*p1) {
            case ' ':
                p1++;
                break;
            case 'f':
                if (strncmp(p1, "false", 5) == 0) {
                    parray[index].data = (const byte *)p1;
                    p1 += 5;
                    *p1++ = 0x00;
                    parray[index].size = 5;
                    parray[index++].persistent = false;
                } else {
                    return -1;
                }
                break;
            case 't':
                if (strncmp(p1, "true", 4) == 0) {
                    parray[index].data = (const byte *)p1;
                    p1 += 4;
                    *p1++ = 0x00;
                    parray[index].size = 4;
                    parray[index++].persistent = false;
                } else {
                    return -1;
                }
                break;
            case '<':
                break;
            case '/':
                parray[index].data = (const byte *)p1;
                while (*p1 != ' ' && *p1 != 0x00)
                    p1++;
                if (*p1 == 0x00)
                    return -1;
                *p1++ = 0x00;
                parray[index].size = strlen(parray[index].data);
                parray[index++].persistent = false;
                break;
            case '(':
                parray[index].data = (const byte *)p1;
                while (*p1 != ')' && *p1 != 0x00)
                    p1++;
                if (*p1 == 0x00)
                    return -1;
                *p1++ = 0x00;
                parray[index].size = strlen(parray[index].data);
                parray[index++].persistent = false;
                break;
            case '[':
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
                break;
            default:
                return -1;
                break;
        }
    }
    array_list->data = parray;
    array_list->persistent = 0;
    array_list->size = tokens;

    code = param_write_string_array((gs_param_list *)plist, key, array_list);
    gs_free_object(mem, parray, "param string array in distillerparams");
    gs_free_object(mem, array_list, "working buffer for distillerparams processing");

    *p = p1++;

    return code;
}

int pjl_dist_process_number(gs_memory_t *mem, gs_c_param_list *plist, gs_param_name key, char **p)
{
    char *start = *p;
    bool integer = true;

    while (**p != ' ' && **p != 0x00) {
        if (**p == '.')
            integer = false;

        (*p)++;
    }

    if (**p != 0x00) {
        **p = 0x00;
        (*p)++;
    }

    if (integer) {
        double f = atof(start);
        param_write_float((gs_param_list *)plist, key, (float *)&f);
    } else {
        long i = atol(start);
        param_write_long((gs_param_list *)plist, key, &i);
    }

    return 0;
}

int pcl_pjl_setdistillerparams(gs_memory_t *mem, gx_device *device, char *distillerparams)
{
    char *p, *type, *start, *copy;
    int tokens = 0, code = 0;
    gs_c_param_list *plist;
    gs_param_name key = NULL;

    plist = gs_c_param_list_alloc(mem, "temp C param list for PJL distillerparams");
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

    while (*p != 0x00){
        switch (*p) {
            case ' ':
                p++;
                break;
            case 'f':
                if (strncmp(p, "false", 5) == 0) {
                    bool t = false;
                    param_write_bool((gs_param_list *)plist, key, &t);
                    p += 5;
                    key = NULL;
                } else {
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                break;
            case 't':
                if (strncmp(p, "true", 4) == 0) {
                    bool t = true;
                    param_write_bool((gs_param_list *)plist, key, &t);
                    p += 4;
                    key = NULL;
                } else {
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                break;
            case '<':
                if (key == NULL) {
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                code = pjl_dist_process_dict_or_hexstring(mem, plist, key, &p);
                key = NULL;
                break;
            case '/':
                code = pjl_dist_process_name(mem, plist, &key, &p);
                break;
            case '(':
                if (key == NULL) {
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                code = pjl_dist_process_string(mem, plist, key, &p);
                key = NULL;
                break;
            case '[':
                if (key == NULL) {
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                code = pjl_dist_process_array(mem, plist, key, &p);
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
                    gs_free_object(mem, copy, "working buffer for distillerparams processing");
                    return -1;
                }
                code = pjl_dist_process_number(mem, plist, key, &p);
                key = NULL;
                break;
            default:
                gs_free_object(mem, copy, "working buffer for distillerparams processing");
                return -1;
                break;
        }
        if (code < 0) {
            gs_free_object(mem, copy, "working buffer for distillerparams processing");
            return -1;
        }
    }

    gs_free_object(mem, copy, "working buffer for distillerparams processing");

    gs_c_param_list_read(plist);
    code = gs_putdeviceparams(device, (gs_param_list *)plist);

    gs_c_param_list_release(plist);
    return 0;
}
