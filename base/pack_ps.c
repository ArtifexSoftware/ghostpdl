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

/* Pack a PostScript file into a C file containing an array of strings. */

/* The PostScript data is packed to remove excess whitespace and comments.
 * Optionally, all header comments in the PostScript file can be copied
 * over as comments in the C file.
 *
 *    Usage: pack_ps [-o outfile] [-n name] [-c] ps_file
 *
 *      options (must precede ps_file to be processed):
 *
 *        -o outfile       Output C file name.
 *        -n array_name    Name of the array inside the generated C file.
 *        -c               output PostScript header comments as C comments.
 *
 *        ps_file          Name of the PostScript file to be converted.
 */

#define OUTFILE_NAME_DEFAULT "obj/packed_ps.c"
#define ARRAY_NAME_DEFAULT   "packed_ps"

#include "stdpre.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG_AUX
#define STRIP_PDFR_DEBUG_CALLS   0    /* Disabled for debug builds. */
#else
/* Set to 1 to strip PDFR_DEBUG calls for release builds. */
#define STRIP_PDFR_DEBUG_CALLS   0    /* Disabled for release builds. */
#endif

#if STRIP_PDFR_DEBUG_CALLS
/* Start and end prefixes for PDFR_DEBUG blocks. */
#define PDFR_DEBUG_START_PREFIX "//PDFR_DEBUG"
#define PDFR_DEBUG_END_COMMENT_1 "% //PDFR_DEBUG"
#define PDFR_DEBUG_END_COMMENT_2 "%//PDFR_DEBUG"
#endif

/* Forward references */
static bool readline(FILE * in, char *str, int len);
static int pack_postscript_line(const char *inputline, char *outputline, char *comment, bool nopack);
static void usage(const char *outfilename, const char *arrayname);
int main(int argc, char *argv[]);

/* Read a line from the input. */
static bool
readline(FILE * in, char *str, int len)
{
    /*
     * Unfortunately, we can't use fgets here, because the typical
     * implementation only recognizes the EOL convention of the current
     * platform.
     */
    int i = 0, c = getc(in);

    if (c < 0) {
        return false;
    }
    while (i < len - 1) {
        if (c < 0 || c == 10) {	/* ^J, Unix EOL */
            break;
        }
        if (c == 13) {		/* ^M, Mac EOL */
            c = getc(in);
            if (c != 10 && c >= 0) { /* ^M^J, PC EOL */
                ungetc(c, in);
            }
            break;
        }
        str[i++] = c;
        c = getc(in);
    }
    str[i] = 0;
    return true;
}

/*
 * Strip extraneous whitespace and comments from a line of PostScript code.
 * Return a pointer to any string that remains, or NULL if the line contains
 * no code.
 *
 * Stores the packed string in outputline, and any comment section in comment,
 * as NULL-terminated strings.  If there is no comment, the comment string will
 * be set to an empty string.
 *
 * Returns the length of outputline.  If the line contains no PostScript data
 * other than whitespace or comments, this length will be zero.
 *
 * Note: the caller is responsible for allocating storage for all three strings,
 * and this routine will modify both the outputline and comment parameters.
 * Currently no overflow checking is performed.  In practice, both outputline
 * and comment will be equal to or shorter than the input line, but in the
 * worst case (a line of all double-quotes or all backslashes, neither of
 * which is legal PostScript) the output line could theoretically consume
 * double the space of the input line.  A large fixed size for all three
 * strings, such as 4096, should be sufficient for practical purposes.
 *
 * Note: Although this routine resembles the "doit" method in mkromfs.c, it
 * performs additional escaping of backslash and double-quote characters, since
 * its output is intended to be embedded in a C string.
 */
static int
pack_postscript_line(const char *inputline, char *outputline, char *comment, bool nopack)
{
    const char *str = inputline;
    const char *from;
    char *to;
    int in_string = 0;

    /* Clear the output strings. */
    outputline[0] = '\0';
    comment[0] = '\0';

    while (!nopack && (*str == ' ' || *str == '\t')) {       /* strip leading whitespace */
        ++str;
    }
    if (*str == 0) {       /* all whitespace */
        return 0;
    }
    if (!strncmp(str, "%END", 4)) {   /* keep these for .skipeof */
        strcpy(outputline, str);
        strcpy(comment, str);
        return strlen(outputline);
    }

    /*
     * Copy the string over, removing:
     *  - All comments not within string literals;
     *  - Whitespace adjacent to '[' ']' '{' '}';
     *  - Whitespace before '/' '(' '<';
     *  - Whitespace after ')' '>'.
     */
    for (from = str, to = outputline; (*to = *from) != 0; ++from, ++to) {
        switch (*from) {
            case '%':
                if (!in_string && !nopack) {
                    /* Store the rest of the line in the comment. */
                    while (*from && (/* (*from == '%') || */ (*from == ' ') || (*from == '\t'))) {
                        from++;
                    }
                    strcpy(comment, from);
                    break;
                }
                continue;
            case ' ':
            case '\t':
                if (!nopack && to > outputline && !in_string && strchr(" \t>[]{})", to[-1])) {
                    --to;
                }
                continue;
            case '(':
            case '<':
            case '/':
            case '[':
            case ']':
            case '{':
            case '}':
                if (!nopack && to > outputline && !in_string && strchr(" \t", to[-1])) {
                    *--to = *from;
                }
                if (*from == '(') {
                    ++in_string;
                }
                continue;
            case ')':
                --in_string;
                continue;
            case '\"':
                /* Because we're writing to a C string, every double-quote we output needs to be escaped with a preceding backslash. */
                *to = '\\';
                *++to = *from;
                continue;
            case '\\':
                /* Because we're writing to a C string, every backslash we output needs to be escaped with a preceding backslash. */
                *++to = '\\';

                if (from[1] == '\\') {
                    *++to = '\\';  /* A double-backslash turns into four backslashes inside a C quote. */
                    *++to = *++from;
                }
                else if (from[1] == '(' || from[1] == ')') {
                    *++to = *++from;  /* Output as "\\(" or "\\)" */
                }
                continue;
            default:
                continue;
        }
        break;
    }

    /* Strip trailing whitespace from outputline. */
    while (to > outputline && (to[-1] == ' ' || to[-1] == '\t')) {
        --to;
    }
    *to = 0;

    /* Strip trailing whitespace from the comment. */
    if (strlen(comment) > 0) {
        int comment_len = strlen(comment);
        char *ptr = comment + comment_len - 1;

        while ((*ptr == ' ') || (*ptr == '\t')) {
            *(ptr--) = '\0';
        }

        /* If all that's left is the percent sign, skip it unless it's the only thing on the line. */
        if ((strlen (comment) == 1) && (strlen(outputline) != 0)) {
            comment [0] = '\0';
        }
    }

    /* Return line length, so 0 = no data found. */
    return strlen(outputline);
}

static void
usage(const char *outfilename, const char *arrayname)
{
    printf("\n");
    printf("    Usage: pack_ps [-o outfile] [-n name] [-c] ps_file\n");
    printf("\n");
    printf("        options (must precede ps_file to be processed):\n");
    printf("            -o outfile       default: %s - output file name\n", outfilename);
    printf("            -n array_name    default: %s - name of the array inside the generated C file.\n", arrayname);
    printf("            -c               output PostScript header comments as C comments.\n");
}

int
main(int argc, char *argv[])
{
    const char *outfilename = OUTFILE_NAME_DEFAULT;
    const char *arrayname = ARRAY_NAME_DEFAULT;
    const char *infilename = NULL;
    bool output_comments = false;
    bool no_pack = false;
    FILE *infile;
    FILE *outfile;

    int atarg = 1;
    int total_input_length = 0;
    int total_output_length = 0;
    int total_code_lines = 0;
    int total_comment_header_lines = 0;
#if STRIP_PDFR_DEBUG_CALLS
    int pdfr_debug_start_count = 0;
    int pdfr_debug_end_count = 0;
    bool skip_this_line = false;
#endif
    time_t buildtime = 0;
    char *env_source_date_epoch;

#define INPUT_LINE_LENGTH_MAX 4096
    char inputline[INPUT_LINE_LENGTH_MAX];

    /* At least an input file name must be provided. */
    if (argc < 2) {
        usage(outfilename, arrayname);
        exit(1);
    }

    /* Process arguments denoted with dashes. */
    while (atarg < argc) {
        if (argv[atarg][0] != '-') {
            /* End of optional arguments */
            break;
        }
        switch (argv[atarg][1]) {
        case 'o' :  /* output file name */
            /* Skip to next argument */
            if (++atarg >= argc) {
                usage(outfilename, arrayname);
                exit(0);
            }
            outfilename = argv[atarg++];
            break;

        case 'n' : /* C array name for this block of code */
            /* Skip to next argument */
            if (++atarg >= argc) {
                usage(outfilename, arrayname);
                exit(0);
            }
            arrayname = argv[atarg++];
            break;

        case 'c' : /* Enable comments in output C file */
            /* Skip to next argument */
            output_comments = true;
            atarg++;
            break;
        case 'd' : /* Don't string whitespace or comments */
            /* Skip to next argument */
            no_pack = true;
            atarg++;
            break;
        }
    }

    /* The final argument is the file name to be processed. */
    if (atarg >= argc) {
        usage(outfilename, arrayname);
        exit(-1);
    }
    infilename = argv[atarg];

    printf("%s:\n", argv[0]);
    printf("  Input file:  %s\n", infilename);
    printf("  Output file: %s\n", outfilename);
    printf("  Array name:  %s\n", arrayname);

    infile = fopen(infilename, "r");
    if (infile == NULL) {
        printf("Unable to open input file \"%s\"\n", infilename);
        exit(-1);
    }
    outfile = fopen(outfilename, "w");
    if (outfile == NULL) {
        fclose(infile);
        printf("Unable to open output file \"%s\"\n", outfilename);
        exit(-1);
    }

    /* Output a header comment showing the source file and build time. */
    if ((env_source_date_epoch = getenv("SOURCE_DATE_EPOCH"))) {
        buildtime = strtoul(env_source_date_epoch, NULL, 10);
    }
    if (!buildtime) {
        buildtime = time(NULL);
    }
    fprintf(outfile,"/* Auto-generated from PostScript file \"%s\" at time %ld */\n", infilename, (long)buildtime);

    while (readline(infile, inputline, INPUT_LINE_LENGTH_MAX)) {

        char packedline[INPUT_LINE_LENGTH_MAX];
        char comment[INPUT_LINE_LENGTH_MAX];
        int unpackedlen = strlen(inputline);
        int packedlen = pack_postscript_line(inputline, packedline, comment, no_pack);
        int commentlen = strlen(comment);

#if STRIP_PDFR_DEBUG_CALLS
        skip_this_line = false;
        if (!strncmp(packedline, PDFR_DEBUG_START_PREFIX, strlen(PDFR_DEBUG_START_PREFIX))) {
            /* Start of PDFR_DEBUG command found. */
            pdfr_debug_start_count++;
            if (pdfr_debug_start_count != pdfr_debug_end_count+1) {
                printf ("ERROR: missing PDFR_DEBUG terminating comment for call %d.\n", pdfr_debug_start_count);
                fclose(infile);
                fclose(outfile);
                exit(-1);
            }
        }

        /* Skip the line if we're in a PDFR_DEBUG block.  By checking here before we look for the
         * trailing comment, we can suppress single-line as well as multi-line PDFR_DEBUG blocks.
         */
        if (pdfr_debug_start_count != pdfr_debug_end_count) {
            skip_this_line = true;
        }

        if ((!strncmp(comment, PDFR_DEBUG_END_COMMENT_1, strlen(PDFR_DEBUG_END_COMMENT_1))) ||
            (!strncmp(comment, PDFR_DEBUG_END_COMMENT_2, strlen(PDFR_DEBUG_END_COMMENT_2)))) {

            /* End of PDFR_DEBUG command found. */
            pdfr_debug_end_count++;
            if (pdfr_debug_start_count != pdfr_debug_end_count) {
                printf ("ERROR: extra PDFR_DEBUG terminating comment for call %d.\n", pdfr_debug_start_count+1);
                fclose(infile);
                fclose(outfile);
                exit(-1);
            }
        }

        if (skip_this_line) {
            continue;
        }
#endif
        if (packedlen > 0) {
            total_code_lines++;
        }

        total_input_length += unpackedlen;
        total_output_length += packedlen;

        /* Output any comments at the head of the file if requested. */
        if (output_comments) {
            if ((total_code_lines == 0) && (commentlen > 0)) {
                total_comment_header_lines++;

                if (total_comment_header_lines == 1) {
                    fprintf(outfile, "/* %s\n", comment);
                }
                else {
                    fprintf(outfile, " * %s\n", comment);
                }
            }
            else if ((total_code_lines == 1) && (total_comment_header_lines > 0) && (packedlen > 0)) {
                fprintf(outfile, " */\n");
            }
        }

        if (packedlen > 0) {
            if (total_code_lines == 1) {
                fprintf(outfile,"const char *%s [] = {\n", arrayname);
            }
            /* Output the line with no comment. */
            fprintf(outfile, "\"%s\\n\",\n", packedline);
        }
    }
    if (total_code_lines > 0) {
        fprintf(outfile, "0x00\n");
        fprintf(outfile, "};\n");
    }

#if STRIP_PDFR_DEBUG_CALLS
    /* Make sure no PDFR_DEBUG calls were left unmatched. */
    if (pdfr_debug_start_count != pdfr_debug_end_count) {
        printf ("ERROR: missing final PDFR_DEBUG terminating comment.\n");
        fclose(infile);
        fclose(outfile);
        exit(-1);
    }
#endif

    /* Display processing statistics. */
    printf("  Processed %d lines of PostScript data.\n", total_code_lines); 
    printf("  %d bytes of PostScript data packed down to %d bytes.\n", total_input_length, total_output_length);
#if STRIP_PDFR_DEBUG_CALLS
    printf("  %d PDFR_DEBUG calls removed from the code.\n", pdfr_debug_start_count);
#endif

    /* Close files and exit with success. */
    fclose(infile);
    fclose(outfile);

    return 0;
}
