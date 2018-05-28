/* Copyright (C) 2001-2018 Artifex Software, Inc.
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


/* Command line argument list management */
#include "ctype_.h"
#include "stdio_.h"
#include "string_.h"
#include "gsexit.h"
#include "gsmemory.h"
#include "gsargs.h"
#include "gserrors.h"

int codepoint_to_utf8(char *cstr, int rune)
{
    int idx = 0;

    if (rune < 0x80) {
        cstr[idx++] = rune;
    } else {
        if (rune < 0x800) {
            cstr[idx++] = 0xc0 | (rune>>6);
        } else {
            if (rune < 0x10000) {
                cstr[idx++] = 0xe0 | (rune>>12);
            } else {
                if (rune < 0x200000) {
                    cstr[idx++] = 0xf0 | (rune>>18);
                } else {
                    /* Shouldn't ever be required, but included for completeness */
                    if (rune < 0x4000000) {
                        cstr[idx++] = 0xf8 | (rune>>24);
                    } else {
                        cstr[idx++] = 0xfc | (rune>>30);
                        cstr[idx++] = 0x80 | ((rune>>24) & 0x3f);
                    }
                    cstr[idx++] = 0x80 | ((rune>>18) & 0x3f);
                }
                cstr[idx++] = 0x80 | ((rune>>12) & 0x3f);
            }
            cstr[idx++] = 0x80 | ((rune>>6) & 0x3f);
        }
        cstr[idx++] = 0x80 | (rune & 0x3f);
    }

    return idx;
}

static int get_codepoint_utf8(FILE *file, const char **astr)
{
    int c;
    int rune;
    int len;

    /* This code spots the BOM for utf8 and ignores it. Strictly speaking
     * this may be wrong, as we are only supposed to ignore it at the beginning
     * of the string, but if anyone is stupid enough to use ZWNBSP (zero width
     * non breaking space) in the middle of their strings, then they deserve
     * what they get. */

    do {
        c = (file ? fgetc(file) : (**astr ? (int)(unsigned char)*(*astr)++ : EOF));
        if (c == EOF)
            return EOF;
        if (c < 0x80)
            return c;
lead: /* We've just read a byte >= 0x80, presumably a leading byte */
        if (c < 0xc0)
            continue; /* Illegal - skip it */
        else if (c < 0xe0)
            len = 1, rune = c & 0x1f;
        else if (c < 0xf0)
            len = 2, rune = c & 0xf;
        else if (c < 0xf8)
            len = 3, rune = c & 7;
        else if (c < 0xfc)
            len = 4, rune = c & 3;
        else if (c < 0xfe)
            len = 5, rune = c & 1;
        else
            continue; /* Illegal - skip it */
        do {
            c = (file ? fgetc(file) : (**astr ? (int)(unsigned char)*(*astr)++ : EOF));
            if (c == EOF)
                return EOF;
            rune = (rune<<6) | (c & 0x3f);
        } while (((c & 0xC0) == 0x80) && --len);
        if (len) {
            /* The rune we are collecting is improperly formed. */
            if (c < 0x80) {
                /* Just return the simple char we've ended on. */
                return c;
            }
            /* Start collecting again */
            goto lead;
        }
        if (rune == 0xFEFF)
            continue; /* BOM. Skip it */
        break;
    } while (1);

    return rune;
}

/* Initialize an arg list. */
void
arg_init(arg_list * pal, const char **argv, int argc,
         FILE         *(*arg_fopen)(const char *fname, void *fopen_data),
         void         *fopen_data,
         int           (*get_codepoint)(FILE *file, const char **astr),
         gs_memory_t  *memory)
{
    pal->expand_ats = true;
    pal->arg_fopen = arg_fopen;
    pal->fopen_data = fopen_data;
    pal->get_codepoint = (get_codepoint ? get_codepoint : get_codepoint_utf8);
    pal->memory = memory;
    pal->argp = argv + 1;
    pal->argn = argc - 1;
    pal->depth = 0;
}

/* Push a string onto an arg list. */
int
arg_push_memory_string(arg_list * pal, char *str, bool parsed, gs_memory_t * mem)
{
    return arg_push_decoded_memory_string(pal, str, parsed, parsed, mem);
}

int
arg_push_decoded_memory_string(arg_list * pal, char *str, bool parsed, bool decoded, gs_memory_t * mem)
{
    arg_source *pas;

    if (pal->depth == arg_depth_max) {
        lprintf("Too much nesting of @-files.\n");
        return 1;
    }
    pas = &pal->sources[pal->depth];
    pas->is_file = false;
    pas->u.s.parsed = parsed;
    pas->u.s.decoded = decoded;
    pas->u.s.chars = str;
    pas->u.s.memory = mem;
    pas->u.s.str = str;
    pal->depth++;
    return 0;
}

/* Clean up an arg list. */
void
arg_finit(arg_list * pal)
{
    while (pal->depth) {
        arg_source *pas = &pal->sources[--(pal->depth)];

        if (pas->is_file)
            fclose(pas->u.file);
        else if (pas->u.s.memory)
            gs_free_object(pas->u.s.memory, pas->u.s.chars, "arg_finit");
    }
}

static int get_codepoint(FILE *file, const char **astr, arg_list *pal, arg_source *pas)
{
    return (pas->u.s.decoded ? get_codepoint_utf8(file, astr) : pal->get_codepoint(file, astr));
}

/* Get the next arg from a list. */
/* Note that these are not copied to the heap. */
/* returns:
 * >0 - valid argument
 *  0 - arguments exhausted
 * <0 - error condition
 * *argstr is *always* set: to the arg string if it is valid,
 * or to NULL otherwise
 */
int
arg_next(arg_list * pal, const char **argstr, const gs_memory_t *errmem)
{
    arg_source *pas;
    FILE *f;
    const char *astr = 0; /* initialized only to pacify gcc */
    char *cstr;
    int c;
    int i;
    bool in_quote, eol, end_on_whitespace;

    *argstr = NULL;

top:pas = &pal->sources[pal->depth - 1];
    end_on_whitespace = 1;
    if (pal->depth == 0) {
        if (pal->argn <= 0) { /* all done */
            return 0;
        }
        pal->argn--;
        astr = *(pal->argp++);
        f = NULL;
        end_on_whitespace = 0;
        goto decode;
    }
    if (pas->is_file)
        f = pas->u.file;
    else if (pas->u.s.parsed)
        /* this string is a "pushed-back" argument                  */
        /* (retrieved by a preceding arg_next(), but not processed) */
        /* assert(pas->u.s.decoded); */
        if (strlen(pas->u.s.str) >= arg_str_max) {
            errprintf(errmem, "Command too long: %s\n", pas->u.s.str);
            return_error(gs_error_Fatal);
        } else {
            strcpy(pal->cstr, pas->u.s.str);
            *argstr = pal->cstr;
            if (pas->u.s.memory)
                gs_free_object(pas->u.s.memory, pas->u.s.chars, "arg_next");
            pal->depth--;
            pas--;
            goto at;
        }
    else
        astr = pas->u.s.str, f = NULL;
  decode:
    *argstr = cstr = pal->cstr;
#define is_eol(c) (c == '\r' || c == '\n')
    i = 0;
    in_quote = false;
    eol = true;
    c = get_codepoint(f, &astr, pal, pas);
    for (i = 0;;) {
        if (c == EOF) {
            if (in_quote) {
                cstr[i] = 0;
                errprintf(errmem,
                          "Unterminated quote in @-file: %s\n", cstr);
                return_error(gs_error_Fatal);
            }
            if (i == 0) {
                /* EOF before any argument characters. */
                if (f != NULL)
                    fclose(f);
                else if (pas->u.s.memory)
                    gs_free_object(pas->u.s.memory, pas->u.s.chars,
                                   "arg_next");
                pal->depth--;
                goto top;
            }
            break;
        }
        /* c != 0 */
        if (c > 0 && c < 256 && isspace(c)) {
            if (i == 0) {
                c = get_codepoint(f, &astr, pal, pas);
                continue;
            }
            if (!in_quote && end_on_whitespace)
                break;
        }
        /* c isn't leading or terminating whitespace. */
        if (c == '#' && eol) {
            /* Skip a comment. */
            do {
                c = get_codepoint(f, &astr, pal, pas);
            } while (!(c == 0 || is_eol(c)));
            if (c == '\r')
                c = get_codepoint(f, &astr, pal, pas);
            if (c == '\n')
                c = get_codepoint(f, &astr, pal, pas);
            continue;
        }
        if (c == '\\') {
            /* Check for \ followed by newline. */
            c = get_codepoint(f, &astr, pal, pas);
            if (is_eol(c)) {
                if (c == '\r')
                    c = get_codepoint(f, &astr, pal, pas);
                if (c == '\n')
                    c = get_codepoint(f, &astr, pal, pas);
                eol = true;
                continue;
            }
            /* \ anywhere else is treated as a printing character. */
            /* This is different from the Unix shells. */
            if (i >= arg_str_max - 1) {
                cstr[i] = 0;
                errprintf(errmem, "Command too long: %s\n", cstr);
                return_error(gs_error_Fatal);
            }
            cstr[i++] = '\\';
            eol = false;
            continue;
        }
        /* c will become part of the argument */
        if (i >= arg_str_max - 1) {
            cstr[i] = 0;
            errprintf(errmem, "Command too long: %s\n", cstr);
            return_error(gs_error_Fatal);
        }
        /* Allow quotes to protect whitespace. */
        /* (special cases have already been handled and don't reach this point) */
        if (c == '"')
            in_quote = !in_quote;
        else
#ifdef GS_NO_UTF8
            cstr[i++] = c;
#else
            i += codepoint_to_utf8(&cstr[i], c);
#endif
        eol = is_eol(c);
        c = get_codepoint(f, &astr, pal, pas);
    }
    cstr[i] = 0;
    if (f == NULL)
        pas->u.s.str = astr;
  at:if (pal->expand_ats && *argstr[0] == '@') {
        char *fname;
        if (pal->depth == arg_depth_max) {
            errprintf(errmem, "Too much nesting of @-files.\n");
            return_error(gs_error_Fatal);
        }
        fname = (char *)*argstr + 1; /* skip @ */
        f = (*pal->arg_fopen) (fname, pal->fopen_data);
        if (f == NULL) {
            errprintf(errmem, "Unable to open command line file %s\n", *argstr);
            return_error(gs_error_Fatal);
        }
        pal->depth++;
        pas++;
        pas->is_file = true;
        pas->u.file = f;
        goto top;
    }

    return 1;
}

/* Copy an argument string to the heap. */
char *
arg_copy(const char *str, gs_memory_t * mem)
{
    char *sstr = (char *)gs_alloc_bytes(mem, strlen(str) + 1, "arg_copy");

    if (sstr == 0) {
        lprintf("Out of memory!\n");
        return NULL;
    }
    strcpy(sstr, str);
    return sstr;
}

/* Free a previously arg_copy'd string */
void
arg_free(char *str, gs_memory_t * mem)
{
    gs_free_object(mem, str, "arg_copy");
}

int arg_strcmp(arg_list *pal, const char *arg, const char *match)
{
    int rune, c;

    if (!arg || !match)
        return 1;
    do {
        rune = pal->get_codepoint(NULL, &arg);
        if (rune == -1)
            rune = 0;
        c = *match++;
        if (rune != c)
            return rune - c;
    } while (rune && c);
    return 0;
}
