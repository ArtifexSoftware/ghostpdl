/* Copyright (C) 1989, 1990, 1993, 1996, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* stdio redirection */

#ifndef gsio_INCLUDED
#  define gsio_INCLUDED

/* The library and interpreter never use stdin/out/err directly. */
extern FILE *gs_stdio[3];
#define gs_stdin (gs_stdio[0])
#define gs_stdout (gs_stdio[1])
#define gs_stderr (gs_stdio[2])

/* Redefine all the relevant stdio functions to use the above. */
/* Some functions we make illegal, rather than redefining them. */
#undef stdin
#define stdin gs_stdin
#undef stdout
#define stdout gs_stdout
#undef stderr
#define stderr gs_stderr
#undef fgetchar
#define fgetchar() fgetc(stdin)
#undef fputchar
#define fputchar(c) fputc(c, stdout)
#undef getchar
#define getchar() getc(stdin)
#undef gets
#define gets Function._gets_.unavailable
/* We should do something about perror, but since many Unix systems */
/* don't provide the strerror function, we can't.  (No Aladdin-maintained */
/* code uses perror.) */
#undef printf
#define printf Function._printf_.unavailable
#undef putchar
#define putchar(c) fputc(c, stdout)
#undef puts
#define puts(s) (fputs(s, stdout), putchar('\n'))
#undef scanf
#define scanf Function._scanf_.unavailable
#undef vprintf
#define vprintf Function._vprintf_.unavailable
#undef vscanf
#define vscanf Function._vscanf_.unavailable

#endif /* gsio_INCLUDED */
