/* Copyright (C) 1995, 1996, 1998 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */


/* Utility for merging all the Ghostscript initialization files */
/* (gs_*.ps) into a single file, optionally converting them to C data. */
#include "stdpre.h"
#include <stdio.h>
#include <string.h>

/* Usage:
 *    geninit <init-file.ps> <gconfig.h> <merged-init-file.ps>
 *      geninit <init-file.ps> <gconfig.h> -c <merged-init-file.c>
 */

/* Forward references */
private void merge_to_c(P4(const char *inname, FILE * in, FILE * config,
			   FILE * out));
private void merge_to_ps(P4(const char *inname, FILE * in, FILE * config,
			    FILE * out));

#define LINE_SIZE 128

int
main(int argc, char *argv[])
{
    const char *fin;
    FILE *in;
    const char *fconfig;
    FILE *config;
    const char *fout;
    FILE *out;
    bool to_c = false;

    if (argc == 4)
	fin = argv[1], fconfig = argv[2], fout = argv[3];
    else if (argc == 5 && !strcmp(argv[3], "-c"))
	fin = argv[1], fconfig = argv[2], fout = argv[4], to_c = true;
    else {
	fprintf(stderr, "\
Usage: geninit gs_init.ps gconfig.h gs_xinit.ps\n\
or     geninit gs_init.ps gconfig.h -c gs_init.c\n");
	exit(1);
    }
    in = fopen(fin, "r");
    if (in == 0) {
	fprintf(stderr, "Cannot open %s for reading.\n", fin);
	exit(1);
    }
    config = fopen(fconfig, "r");
    if (config == 0) {
	fprintf(stderr, "Cannot open %s for reading.\n", fconfig);
	fclose(in);
	exit(1);
    }
    out = fopen(fout, "w");
    if (out == 0) {
	fprintf(stderr, "Cannot open %s for writing.\n", fout);
	fclose(config);
	fclose(in);
	exit(1);
    }
    if (to_c)
	merge_to_c(fin, in, config, out);
    else
	merge_to_ps(fin, in, config, out);
    fclose(out);
    return 0;
}

/* Read a line from the input. */
private bool
rl(FILE * in, char *str, int len)
{
    if (fgets(str, len, in) == NULL)
	return false;
    str[strlen(str) - 1] = 0;	/* remove newline */
    return true;
}

/* Write a line on the output. */
private void
wlc(FILE * out, const char *str)
{
    int n = 0;
    const char *p = str;

    for (; *p; ++p) {
	char c = *p;
	const char *format = "%d,";

	if (c >= 32 && c < 127)
	    format = (c == '\'' || c == '\\' ? "'\\%c'," : "'%c',");
	fprintf(out, format, c);
	if (++n == 15) {
	    fputs("\n", out);
	    n = 0;
	}
    }
    fputs("10,\n", out);
}
private void
wl(FILE * out, const char *str, bool to_c)
{
    if (to_c)
	wlc(out, str);
    else
	fprintf(out, "%s\n", str);
}

/*
 * Strip whitespace and comments from a line of PostScript code as possible.
 * Return a pointer to any string that remains, or NULL if none.
 * Note that this may store into the string.
 */
private char *
doit(char *line)
{
    char *str = line;
    char *from;
    char *to;
    int in_string = 0;

    while (*str == ' ' || *str == '\t')		/* strip leading whitespace */
	++str;
    if (*str == 0)		/* all whitespace */
	return NULL;
    if (!strncmp(str, "%END", 4))	/* keep these for .skipeof */
	return str;
    if (str[0] == '%')		/* comment line */
	return NULL;
    /*
     * Copy the string over itself removing:
     *  - All comments not within string literals;
     *  - Whitespace adjacent to []{};
     *  - Whitespace before /(;
     *  - Whitespace after ).
     */
    for (to = from = str; (*to = *from) != 0; ++from, ++to) {
	switch (*from) {
	    case '%':
		if (!in_string)
		    break;
		continue;
	    case ' ':
	    case '\t':
		if (to > str && !in_string && strchr(" \t[]{})", to[-1]))
		    --to;
		continue;
	    case '(':
		if (to > str && !in_string && strchr(" \t", to[-1]))
		    *--to = *from;
		++in_string;
		continue;
	    case ')':
		--in_string;
		continue;
	    case '/':
	    case '[':
	    case ']':
	    case '{':
	    case '}':
		if (to > str && !in_string && strchr(" \t", to[-1]))
		    *--to = *from;
		continue;
	    case '\\':
		if (from[1] == '\\' || from[1] == '(' || from[1] == ')')
		    *++to = *++from;
		continue;
	    default:
		continue;
	}
	break;
    }
    /* Strip trailing whitespace. */
    while (to > str && (to[-1] == ' ' || to[-1] == '\t'))
	--to;
    *to = 0;
    return str;
}

/* Merge a file from input to output. */
private void
flush_buf(FILE * out, char *buf, bool to_c)
{
    if (buf[0] != 0) {
	wl(out, buf, to_c);
	buf[0] = 0;
    }
}
private void
mergefile(const char *inname, FILE * in, FILE * config, FILE * out, bool to_c)
{
    char line[LINE_SIZE + 1];
    char buf[LINE_SIZE + 1];
    char *str;

    buf[0] = 0;
    while (rl(in, line, LINE_SIZE)) {
	char psname[LINE_SIZE + 1];
	int nlines;

	if (!strncmp(line, "%% Replace ", 11) &&
	    sscanf(line + 11, "%d %s", &nlines, psname) == 2
	    ) {
	    flush_buf(out, buf, to_c);
	    while (nlines-- > 0)
		rl(in, line, LINE_SIZE);
	    if (psname[0] == '(') {
		FILE *ps;

		psname[strlen(psname) - 1] = 0;
		ps = fopen(psname + 1, "r");
		if (ps == 0) {
		    fprintf(stderr, "Cannot open %s for reading.\n", psname + 1);
		    exit(1);
		}
		mergefile(psname + 1, ps, config, out, to_c);
	    } else if (!strcmp(psname, "INITFILES")) {
		/*
		 * We don't want to bind config.h into geninit, so
		 * we parse it ourselves at execution time instead.
		 */
		rewind(config);
		while (rl(config, psname, LINE_SIZE))
		    if (!strncmp(psname, "psfile_(\"", 9)) {
			FILE *ps;
			char *quote = strchr(psname + 9, '"');

			*quote = 0;
			ps = fopen(psname + 9, "r");
			if (ps == 0) {
			    fprintf(stderr, "Cannot open %s for reading.\n", psname + 9);
			    exit(1);
			}
			mergefile(psname + 9, ps, config, out, to_c);
		    }
	    } else {
		fprintf(stderr, "Unknown %%%% Replace %d %s\n",
			nlines, psname);
		exit(1);
	    }
	} else if (!strcmp(line, "currentfile closefile")) {
	    /* The rest of the file is debugging code, stop here. */
	    break;
	} else {
	    str = doit(line);
	    if (str == 0)
		continue;
	    if (buf[0] != '%' &&	/* special retained comment */
		strlen(buf) + strlen(str) < LINE_SIZE &&
		(strchr("(/[]{}", str[0]) ||
		 (buf[0] != 0 && strchr(")[]{}", buf[strlen(buf) - 1])))
		)
		strcat(buf, str);
	    else {
		flush_buf(out, buf, to_c);
		strcpy(buf, str);
	    }
	}
    }
    flush_buf(out, buf, to_c);
    fprintf(stderr, "%s: %ld bytes, output pos = %ld\n",
	    inname, ftell(in), ftell(out));
    fclose(in);
}

/* Merge and produce a C file. */
private void
merge_to_c(const char *inname, FILE * in, FILE * config, FILE * out)
{
    char line[LINE_SIZE + 1];

    fputs("/*\n", out);
    while ((rl(in, line, LINE_SIZE), line[0]))
	fprintf(out, "%s\n", line);
    fputs("*/\n", out);
    fputs("\n", out);
    fputs("/* Pre-compiled interpreter initialization string. */\n", out);
    fputs("\n", out);
    fputs("const unsigned char gs_init_string[] = {\n", out);
    mergefile(inname, in, config, out, true);
    fputs("10};\n", out);
    fputs("const unsigned int gs_init_string_sizeof = sizeof(gs_init_string);\n", out);
}

/* Merge and produce a PostScript file. */
private void
merge_to_ps(const char *inname, FILE * in, FILE * config, FILE * out)
{
    char line[LINE_SIZE + 1];

    while ((rl(in, line, LINE_SIZE), line[0]))
	fprintf(out, "%s\n", line);
    mergefile(inname, in, config, out, false);
}
