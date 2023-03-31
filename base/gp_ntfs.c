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


/* file system stuff for MS-Windows WIN32 and MS-Windows NT */
/* hacked from gp_dosfs.c by Russell Lang */

#include "windows_.h"
#include "stdio_.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include "malloc_.h"
#include "memory_.h"
#include "string_.h"
#include "gstypes.h"
#include "gsmemory.h"
#include "gsstruct.h"
#include "gp.h"
#include "gpmisc.h"
#include "gsutil.h"

/* ------ Printer accessing ------ */

/* Put a printer file (which might be stdout) into binary or text mode. */
/* This is not a standard gp procedure, */
/* but all MS-DOS configurations need it. */
static int
setmode_binary(int fno, bool binary)
{
    /* Use non-standard setmode that almost all NT compilers offer. */
#if defined(__STDC__) && !defined(__WATCOMC__)
    return _setmode(fno, binary ? _O_BINARY : _O_TEXT);
#else
    return setmode(fno, binary ? O_BINARY : O_TEXT);
#endif
}
void
gp_set_file_binary(int prnfno, int binary)
{
    DISCARD(setmode_binary(prnfno, binary != 0));
}

/* ------ File accessing -------- */

/* Set a file into binary or text mode. */
int
gp_setmode_binary_impl(FILE * pfile, bool binary)
{
    /* Use non-standard fileno that almost all NT compilers offer. */
#if defined(__STDC__) && !defined(__WATCOMC__)
    int code = setmode_binary(_fileno(pfile), binary);
#else
    int code = setmode_binary(fileno(pfile), binary);
#endif

    return (code == -1 ? -1 : 0);
}

/* ------ File names ------ */

/* Define the character used for separating file names in a list. */
const char gp_file_name_list_separator = ';';

/* Define the string to be concatenated with the file mode */
/* for opening files without end-of-line conversion. */
const char* gp_fmode_binary_suffix = "b";

/* Define the file modes for binary reading or writing. */
const char gp_fmode_rb[] = "rb";
const char gp_fmode_wb[] = "wb";

/* ------ File enumeration ------ */

struct directory_enum_s {
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;
    char *pattern;		/* orig pattern + modified pattern */
    int patlen;			/* orig pattern length */
    int pat_size;		/* allocate space for pattern */
    int head_size;		/* pattern length through last */
    /* :, / or \ */
    int first_time;
    gs_memory_t *memory;
    struct directory_enum_s *previous;
};
gs_private_st_ptrs2(st_directory_enum, struct directory_enum_s, "directory_enum",
                    directory_enum_enum_ptrs, directory_enum_reloc_ptrs, pattern, previous);

typedef struct directory_enum_s directory_enum;

struct file_enum_s {
    char *pattern;
    bool illegal;
    struct directory_enum_s *current;
};
gs_private_st_ptrs2(st_file_enum, struct file_enum_s, "directory_enum",
                    file_enum_enum_ptrs, file_enum_reloc_ptrs, pattern, current);

static int enumerate_directory_init(gs_memory_t *mem, directory_enum *pden, const char *directory, int dir_size, char *filename, const char *pattern, int pat_size)
{
    int length = dir_size + pat_size;

    if (filename)
        length += strlen(filename);

    length = length * 2 + 3;

    /* pattern could be allocated as a string, */
    /* but it's simpler for GC and freeing to allocate it as bytes. */
    pden->pattern = (char *)gs_alloc_bytes(mem, length,
                                     "gp_enumerate_files(pattern)");
    if (pden->pattern == 0)
        return -1;

    memcpy(pden->pattern, directory, dir_size);
    if (dir_size > 1 && directory[dir_size - 1] != '/' && directory[dir_size - 1] != '\\') {
        pden->pattern[dir_size++] = '/';
    }
    if (filename) {
        memcpy(&pden->pattern[dir_size], filename, strlen(filename));
        dir_size += strlen(filename);
        pden->pattern[dir_size++] = '/';
    }

    memcpy(&(pden->pattern[dir_size]), pattern, pat_size);
    pden->pattern[dir_size + pat_size] = 0;

    pden->head_size = dir_size;
    pden->patlen = dir_size + pat_size;
    pden->pat_size = length;
    pden->memory = mem;
    pden->first_time = 1;
    memset(&pden->find_data, 0, sizeof(pden->find_data));
    pden->find_handle = INVALID_HANDLE_VALUE;
    pden->previous = 0L;
    return 0;
}

/* Initialize an enumeration.  Note that * and ? in a directory */
/* don't work with the OS call currently used. The '\' escape	*/
/* character is removed for the 'Find...File' function.		*/
file_enum *
gp_enumerate_files_init_impl(gs_memory_t * mem, const char *pat, uint patlen)
{
    directory_enum *pden;
    file_enum *pfen;
    int pat_size = 2 * patlen + 1;
    char *pattern;
    int hsize = 0;
    int i, j;

    pden = gs_alloc_struct(mem, directory_enum, &st_directory_enum, "gp_enumerate_files");
    if (pden == 0)
        return 0;
    pfen = gs_alloc_struct(mem, file_enum, &st_file_enum, "gp_enumerate_files");
    if (pfen == 0) {
        gs_free_object(mem, pden, "free directory enumerator on error");
        return 0;
    }
    pfen->current = pden;
    pfen->illegal = false;

    /* pattern could be allocated as a string, */
    /* but it's simpler for GC and freeing to allocate it as bytes. */
    pattern = (char *)gs_alloc_bytes(mem, pat_size,
                                     "gp_enumerate_files(pattern)");
    if (pattern == 0) {
        gs_free_object(mem, pden, "free directory enumerator on error");
        gs_free_object(mem, pfen, "free file enumerator on error");
        return 0;
    }

    /* translate the template into a pattern. Note that */
    /* a final '\' or '/' in the string is discarded.                */
    for (i = 0, j=0; i < patlen; i++) {
        if (j > 0 && (pattern[j-1] == '/' || pattern[j-1] == '\\')) {
            while (pat[i] == '/' || pat[i] == '\\') {
                i++;
                if (i == patlen)
                    break;         /* '\' at end ignored */
            }
        }
        pattern[j++]=pat[i];
    }

    pfen->pattern = pattern;
    pat = pfen->pattern;
    patlen = j;

    /* Scan for last path separator to determine 'head_size' (directory part) */
    for (i = 0; i < patlen; i++) {
        if(pat[i] == '/' || pat[i] == '\\' || pat[i] == ':')
        hsize = i + 1;
    }

    /* Scan for illegal characters in the directory path */
    for (i=0; i < hsize; i++) {
        if (pat[i] == '*' || pat[i] == '?')
            /* We can't abort cleanly from here so we store the flag for later */
            /* See gp_enumerate_files_next() below. */
            pfen->illegal = true;
    }

    if (enumerate_directory_init(mem, pden, pfen->pattern, hsize, NULL, &pat[hsize], patlen - hsize) < 0)
    {
        gs_free_object(mem, pattern, "free file enumerator pattern buffer on error");
        gs_free_object(mem, pden, "free directory enumerator on error");
        gs_free_object(mem, pfen, "free file enumerator on error");
        return 0;
    }

    return pfen;
}

/* Enumerate the next file. */
uint
gp_enumerate_files_next_impl(gs_memory_t * mem, file_enum * pfen, char *ptr, uint maxlen)
{
    directory_enum *new_denum = NULL, *pden = pfen->current;
    int code = 0;
    uint len;
    char outfname[(sizeof(pden->find_data.cFileName)*3+1)/2];

    if (pfen->illegal) {
        gp_enumerate_files_close(mem, pfen);
        return ~(uint) 0;
    }

    for(;;) {
        if (pden->first_time) {
            wchar_t *pat;
            pat = malloc(gp_utf8_to_uint16(NULL, pden->pattern)*sizeof(wchar_t));
            if (pat == NULL) {
                code = -1;
                break;
            }
            gp_utf8_to_uint16(pat, pden->pattern);
#ifdef METRO
            pden->find_handle = FindFirstFileExW(pat, FindExInfoStandard, &(pden->find_data), FindExSearchNameMatch, NULL, 0);
#else
            pden->find_handle = FindFirstFileW(pat, &(pden->find_data));
#endif
            free(pat);
            if (pden->find_handle == INVALID_HANDLE_VALUE) {
                if (pden->previous) {
                    FindClose(pden->find_handle);
                    gs_free_object(pden->memory, pden->pattern,
                       "gp_enumerate_files_next(pattern)");
                    new_denum = pden->previous;
                    gs_free_object(pden->memory, pden, "gp_enumerate_files_next");
                    pden = new_denum;
                    pfen->current = pden;
                    continue;
                } else {
                    code = -1;
                    break;
                }
            }
            pden->first_time = 0;
        } else {
            if (!FindNextFileW(pden->find_handle, &(pden->find_data))) {
                if (pden->previous) {
                    FindClose(pden->find_handle);
                    gs_free_object(pden->memory, pden->pattern,
                       "gp_enumerate_files_next(pattern)");
                    new_denum = pden->previous;
                    gs_free_object(pden->memory, pden, "gp_enumerate_files_close");
                    pden = new_denum;
                    pfen->current = pden;
                    continue;
                } else {
                    code = -1;
                    break;
                }
            }
        }
        if ( wcscmp(L".",  pden->find_data.cFileName)
            && wcscmp(L"..", pden->find_data.cFileName)) {
                if (pden->find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    new_denum = gs_alloc_struct(pden->memory, directory_enum, &st_directory_enum, "gp_enumerate_files");
                    if (new_denum != 0) {
                        char *fname;
                        fname = gs_alloc_bytes(pden->memory, gp_uint16_to_utf8(NULL, pden->find_data.cFileName)*sizeof(wchar_t), "temporary wchar buffer");
                        if (fname == NULL) {
                            gs_free_object(pden->memory, new_denum, "free directory enumerator on error");
                        } else {
                            gp_uint16_to_utf8(fname, pden->find_data.cFileName);
                            if (enumerate_directory_init(pden->memory, new_denum, pden->pattern, pden->head_size,
                                fname, "*", 1) < 0)
                            {
                                gs_free_object(pden->memory, new_denum, "free directory enumerator on error");
                            }
                            gs_free_object(pden->memory, fname, "free temporary wchar buffer");
                            new_denum->previous = pden;
                            pden = new_denum;
                            pfen->current = pden;
                        }
                    }
                }
                else
                    break;
        }
    }

    if (code != 0) {		/* All done, clean up. */
        gp_enumerate_files_close(mem, pfen);
        return ~(uint) 0;
    }
    gp_uint16_to_utf8(outfname, pden->find_data.cFileName);
    len = strlen(outfname);

    if (pden->head_size + len < maxlen) {
        memcpy(ptr, pden->pattern, pden->head_size);
        strcpy(ptr + pden->head_size, outfname);
        return pden->head_size + len;
    } else {
        return (pden->head_size + len);
    }
}

/* Clean up the file enumeration. */
void
gp_enumerate_files_close_impl(gs_memory_t * mem, file_enum * pfen)
{
    directory_enum *ptenum, *pden = pfen->current;
    gs_memory_t *mem2 = pden->memory;
    (void)mem;

    while (pden) {
        if (pden->find_handle != INVALID_HANDLE_VALUE)
            FindClose(pden->find_handle);
        gs_free_object(mem2, pden->pattern,
                   "gp_enumerate_files_close(pattern)");
        ptenum = pden->previous;
        gs_free_object(mem2, pden, "gp_enumerate_files_close");
        pden = ptenum;
    };
    gs_free_object(mem2, pfen->pattern,
         "gp_enumerate_files_close(pattern)");
    gs_free_object(mem2, pfen, "gp_enumerate_files_close");
}

/* -------------- Helpers for gp_file_name_combine_generic ------------- */

uint gp_file_name_root(const char *fname, uint len)
{   int i = 0;

    if (len == 0)
        return 0;
    if (len > 1 && fname[0] == '\\' && fname[1] == '\\') {
        /* A network path: "\\server\share\" */
        int k = 0;

        for (i = 2; i < len; i++)
            if (fname[i] == '\\' || fname[i] == '/')
                if (k++) {
                    i++;
                    break;
                }
    } else if (fname[0] == '/' || fname[0] == '\\') {
        /* Absolute with no drive. */
        i = 1;
    } else if (len > 1 && fname[1] == ':') {
        /* Absolute with a drive. */
        i = (len > 2 && (fname[2] == '/' || fname[2] == '\\') ? 3 : 2);
    }
    return i;
}

uint gs_file_name_check_separator(const char *fname, int len, const char *item)
{   if (len > 0) {
        if (fname[0] == '/' || fname[0] == '\\')
            return 1;
    } else if (len < 0) {
        if (fname[-1] == '/' || fname[-1] == '\\')
            return 1;
    }
    return 0;
}

bool gp_file_name_is_parent(const char *fname, uint len)
{   return len == 2 && fname[0] == '.' && fname[1] == '.';
}

bool gp_file_name_is_current(const char *fname, uint len)
{   return len == 1 && fname[0] == '.';
}

const char *gp_file_name_separator(void)
{   return "/";
}

const char *gp_file_name_directory_separator(void)
{   return "/";
}

const char *gp_file_name_parent(void)
{   return "..";
}

const char *gp_file_name_current(void)
{   return ".";
}

bool gp_file_name_is_parent_allowed(void)
{   return true;
}

bool gp_file_name_is_empty_item_meanful(void)
{   return false;
}

gp_file_name_combine_result
gp_file_name_combine(const char *prefix, uint plen, const char *fname, uint flen,
                    bool no_sibling, char *buffer, uint *blen)
{
    return gp_file_name_combine_generic(prefix, plen,
            fname, flen, no_sibling, buffer, blen);
}

bool
gp_file_name_good_char(unsigned char c)
{
	return c >= ' ' && ! strchr("\"*:<>?\\/|", c);
}
