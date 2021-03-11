/* Copyright (C) 2001-2021 Artifex Software, Inc.
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

    /* release the memory allocated for the list parameter */
    gs_c_param_list_release(&list);

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
        gp_file *f;
        char *filename;
        int bytes;

        if (parray[tokens - 2].data[0] != '(') {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            return -1;
        }
        filename = (char *)&(parray[tokens - 2].data[1]);
        filename[strlen(filename) - 1] = 0x00;

        f = gp_fopen(mem, (const char *)filename, "rb");
        if (!f) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            return -1;
        }

        if (gp_fseek(f, 0, SEEK_END) != 0) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            gp_fclose(f);
            return -1;
        }
        bytes = gp_ftell(f);
        if (bytes < 0) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            gp_fclose(f);
            return -1;
        }

        parray[tokens - 2].data = (const byte *)gs_alloc_bytes(mem, bytes, "PJL pdfmark, stream");
        if (!parray[tokens - 2].data) {
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            gp_fclose(f);
            return -1;
        }
        stream_data = (char *)(parray[tokens - 2].data);

        if (gp_fseek(f, 0, SEEK_SET) != 0) {
            gs_free_object(mem, stream_data, "PJL pdfmark, stream");
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            gp_fclose(f);
            return gs_note_error(gs_error_ioerror);
        }
        code = gp_fread(stream_data, 1, bytes, f);
        if (code != bytes) {
            gs_free_object(mem, stream_data, "PJL pdfmark, stream");
            gs_free_object(mem, copy, "working buffer for pdfmark processing");
            gs_free_object(mem, parray, "temporary pdfmark array");
            gp_fclose(f);
            return gs_note_error(gs_error_ioerror);
        }
        gp_fclose(f);
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
    code = gs_param_list_add_tokens((gs_param_list *)plist, p);
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
