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


/* Command line argument list management */
#include "ctype_.h"
#include "stdio_.h"
#include "string_.h"
#include "gsexit.h"
#include "gsmemory.h"
#include "gsargs.h"
#include "gserrors.h"
#include "gp.h"

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

static int get_codepoint_utf8(stream *s, const char **astr)
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
        c = (s ? spgetc(s) : (**astr ? (int)(unsigned char)*(*astr)++ : EOF));
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
            c = (s ? spgetc(s) : (**astr ? (int)(unsigned char)*(*astr)++ : EOF));
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
int
arg_init(arg_list     * pal,
         const char  **argv,
         int           argc,
         stream      *(*arg_fopen)(const char *fname, void *fopen_data),
         void         *fopen_data,
         int           (*get_codepoint)(stream *s, const char **astr),
         gs_memory_t  *memory)
{
    int code;
    const char *arg;

    pal->expand_ats = true;
    pal->arg_fopen = arg_fopen;
    pal->fopen_data = fopen_data;
    pal->get_codepoint = (get_codepoint ? get_codepoint : get_codepoint_utf8);
    pal->memory = memory;
    pal->argp = argv;
    pal->argn = argc;
    pal->depth = 0;
    pal->sources[0].is_file = 0;
    pal->sources[0].u.s.memory = NULL;
    pal->sources[0].u.s.decoded = 0;
    pal->sources[0].u.s.parsed = 0;

    /* Stash the 0th one */
    code = arg_next(pal, &arg, memory);
    if (code < 0)
        return code;
    return gs_lib_ctx_stash_exe(memory->gs_lib_ctx, arg);
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

    if (pal->depth+1 == arg_depth_max) {
        lprintf("Too much nesting of @-files.\n");
        return 1;
    }
    pas = &pal->sources[++pal->depth];
    pas->is_file = false;
    pas->u.s.parsed = parsed;
    pas->u.s.decoded = decoded;
    pas->u.s.chars = str;
    pas->u.s.memory = mem;
    pas->u.s.str = str;
    return 0;
}

/* Clean up an arg list. */
void
arg_finit(arg_list * pal)
{
    /* No cleanup is required for level 0 */
    while (pal->depth) {
        arg_source *pas = &pal->sources[pal->depth--];

        if (pas->is_file)
            sclose(pas->u.strm);
        else if (pas->u.s.memory)
            gs_free_object(pas->u.s.memory, pas->u.s.chars, "arg_finit");
    }
}

static int get_codepoint(arg_list *pal, arg_source *pas)
{
    int (*fn)(stream *s, const char **str);

    fn = (!pas->is_file && pas->u.s.decoded ? get_codepoint_utf8 : pal->get_codepoint);
    return fn(pas->is_file ? pas->u.strm : NULL, &pas->u.s.str);
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
    char *cstr;
    int c;
    int i;
    bool in_quote, eol;
    int prev_c_was_equals = 0;

    *argstr = NULL;

    /* Loop over arguments, finding one to return. */
    do {
        pas = &pal->sources[pal->depth];
        if (!pas->is_file && pas->u.s.parsed) {
            /* This string is a "pushed-back" argument (retrieved
             * by a preceding arg_next(), but not processed). No
             * decoding is required. */
            /* assert(pas->u.s.decoded); */
            if (strlen(pas->u.s.str) >= arg_str_max) {
                errprintf(errmem, "Command too long: %s\n", pas->u.s.str);
                return_error(gs_error_Fatal);
            }
            strcpy(pal->cstr, pas->u.s.str);
            *argstr = pal->cstr;
            if (pas->u.s.memory)
                gs_free_object(pas->u.s.memory, pas->u.s.chars, "arg_next");
            pal->depth--;
        } else {
            /* We need to decode the next argument */
            if (pal->depth == 0) {
                if (pal->argn <= 0)
                    return 0; /* all done */
                /* Move onto the next argument from the string. */
                pal->argn--;
                pas->u.s.str = *(pal->argp++);
            }
            /* Skip a prefix of whitespace. */
            do {
                c = get_codepoint(pal, pas);
            } while (c > 0 && c < 256 && isspace(c));
            if (c == EOF) {
                /* EOF before any argument characters. */
                if (pas->is_file) {
                    sclose(pas->u.strm);
                    gs_free_object(pas->u.strm->memory, pas->u.strm, "arg stream");
                    pas->u.strm = NULL;
                }
                else if (pas->u.s.memory)
                    gs_free_object(pas->u.s.memory, pas->u.s.chars,
                                   "arg_next");
                /* If depth is 0, then we are reading from the simple
                 * argument list and we just hit an "empty" argument
                 * (such as -o ""). Return this. */
                if (pal->depth == 0)
                {
                    *argstr = pal->cstr;
                    pal->cstr[0] = 0;
                    break;
                }
                /* If depth > 0, then we're reading from a response
                 * file, and we've hit the end of the response file.
                 * Pop up one level and continue. */
                pal->depth--;
                continue; /* Next argument */
            }
    #define is_eol(c) (c == '\r' || c == '\n')
            /* Convert from astr into pal->cstr, and return it as *argstr. */
            *argstr = cstr = pal->cstr;
            in_quote = false;
            /* We keep track of whether we have just read an "eol" or not,
             * in order to skip # characters at the start of a line
             * (possibly preceeded by whitespace). We do NOT want this to
             * apply to the start of arguments in the arg list, so only
             * set eol to be true, if we are in a file. */
            eol = pal->depth > 0;
            for (i = 0;;) {
                if (c == EOF) {
                    if (in_quote) {
                        cstr[i] = 0;
                        errprintf(errmem,
                                  "Unterminated quote in @-file: %s\n", cstr);
                        return_error(gs_error_Fatal);
                    }
                    break; /* End of arg */
                }
                /* c != 0 */
                /* If we aren't parsing from the arglist (i.e. depth > 0)
                 * then we break on whitespace (unless we're in quotes). */
                if (pal->depth > 0 && !in_quote && c > 0 && c < 256 && isspace(c))
                    break; /* End of arg */
                /* c isn't leading or terminating whitespace. */
                if (c == '#' && eol) {
                    /* Skip a comment. */
                    do {
                        c = get_codepoint(pal, pas);
                    } while (c != 0 && !is_eol(c) && c != EOF);
                    if (c == '\r')
                        c = get_codepoint(pal, pas);
                    if (c == '\n')
                        c = get_codepoint(pal, pas);
                    prev_c_was_equals = 0;
                    continue; /* Next char */
                }
                if (c == '\\' && pal->depth > 0) {
                    /* Check for \ followed by newline. */
                    c = get_codepoint(pal, pas);
                    if (is_eol(c)) {
                        if (c == '\r')
                            c = get_codepoint(pal, pas);
                        if (c == '\n')
                            c = get_codepoint(pal, pas);
                        eol = true;
                        prev_c_was_equals = 0;
                        continue; /* Next char */
                    }
                    {
                        char what;

                        if (c == '"') {
                            /* currently \" is treated as literal ". No other literals yet.
                             * We may expand this in future. */
                            what = c;
                            c = get_codepoint(pal, pas);
                        } else {
                            /* \ anywhere else is treated as a printing character. */
                            /* This is different from the Unix shells. */
                            what = '\\';
                        }

                        if (i >= arg_str_max - 1) {
                            cstr[i] = 0;
                            errprintf(errmem, "Command too long: %s\n", cstr);
                            return_error(gs_error_Fatal);
                        }
                        cstr[i++] = what;
                        eol = false;
                        prev_c_was_equals = 0;
                        continue; /* Next char */
                    }
                }
                /* c will become part of the argument */
                if (i >= arg_str_max - 1) {
                    cstr[i] = 0;
                    errprintf(errmem, "Command too long: %s\n", cstr);
                    return_error(gs_error_Fatal);
                }
                /* Now, some (slightly hairy) code to allow quotes to protect whitespace.
                 * We only allow for double-quote quoting within @files, as a) command-
                 * line args passed via argv are zero terminated so we should have no
                 * confusion with whitespace, and b) callers using the command line will
                 * have to have carefully quoted double-quotes to make them survive the
                 * shell anyway! */
                if (c == '"' && pal->depth > 0) {
                    if ((i == 0 || prev_c_was_equals) && !in_quote)
                        in_quote = true;
                    else if (in_quote) {
                        /* Need to check the next char to see if we're closing at the end */
                        c = get_codepoint(pal, pas);
                        if (c > 0 && c < 256 && isspace(c)) {
                            /* Reading from an @file, we've hit a space char. That's good, this
                             * was a close quote. */
                            cstr[i] = 0;
                            break;
                        }
                        /* Not a close quote, just a literal quote. */
                        i += codepoint_to_utf8(&cstr[i], '"');
                        eol = false;
                        prev_c_was_equals = 0;
                        continue; /* Jump to the start of the loop without reading another char. */
                    } else
                        i += codepoint_to_utf8(&cstr[i], c);
                }
                else
                    i += codepoint_to_utf8(&cstr[i], c);
                eol = is_eol(c);
                prev_c_was_equals = (c == '=') || (c == '#');
                c = get_codepoint(pal, pas);
            }
            cstr[i] = 0;
        }

        /* At this point *argstr is full of utf8 encoded argument. */
        /* If it's an @filename argument, then deal with it, and never return
         * it to the caller. */
        if (pal->expand_ats && **argstr == '@') {
            char *fname;
            stream *s;
            if (pal->depth+1 == arg_depth_max) {
                errprintf(errmem, "Too much nesting of @-files.\n");
                return_error(gs_error_Fatal);
            }
            fname = (char *)*argstr + 1; /* skip @ */

            if (gs_add_control_path(pal->memory, gs_permit_file_reading, fname) < 0)
                return_error(gs_error_Fatal);

            s = (*pal->arg_fopen) (fname, pal->fopen_data);
            DISCARD(gs_remove_control_path(pal->memory, gs_permit_file_reading, fname));
            if (s == NULL) {
                errprintf(errmem, "Unable to open command line file %s\n", *argstr);
                return_error(gs_error_Fatal);
            }
            pas = &pal->sources[++pal->depth];
            pas->is_file = true;
            pas->u.strm = s;
            *argstr = NULL; /* Empty the argument string so we don't return it. */
            continue; /* Loop back to parse the first arg from the file. */
        }
    } while (*argstr == NULL || **argstr == 0); /* Until we get a non-empty arg */

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
