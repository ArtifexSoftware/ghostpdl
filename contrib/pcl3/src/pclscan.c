/******************************************************************************
  File:     $Id: pclscan.c,v 1.8 2000-10-22 11:05:34+02 Martin Rel $
  Contents: PCL scanner
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany, e-mail: Martin.Lottermoser@t-online.de

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED	1
#endif

/* Standard headers */
#include <errno.h>
#include <nl_types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Special headers */
#include "pclscan.h"

/*****************************************************************************/

static nl_catd catd = (nl_catd)-1;	/* NLS catalogue descriptor */

static void message(int id, const char *s)
{
  if (catd != (nl_catd)-1) s = catgets(catd, 1, id, s);
   /* The check might not be necessary (X/Open requires catgets() to return 's'
      if the call is unsuccessful for *any* reason and the possible errors
      include EBADF for an invalid 'catd'), but one never knows. */

  fputs("? pclscan: ", stderr);
  fprintf(stderr, s);
  fputc('\n', stderr);

  return;
}

/*****************************************************************************/

static const pcl_Octet ESC = '\x1B';

/******************************************************************************

  Function: pcl_is_control_code

  This function returns a non-zero value iff 'c', taken to be an unsigned char
  converted to 'int', is a PCL control code.

******************************************************************************/

int pcl_is_control_code(int c)
{
  return
    c <= ' ' && (
      c == '\0' ||
      /* TRG500 does not mention NUL as a control code. */
      c == '\b' || c == '\t' || c == '\n' || c == '\f' || c == '\r' ||
      c == '\x0E' /* Shift Out (SO) */ ||
      c == '\x0F' /* Shift In (SI) */ ||
      c == '\x11' /* Device Control 1 (DC1) */ ||
      c == '\x13' /* Device Control 3 (DC3) */ ||
      c == ESC ||
      c == ' ');
}

/*****************************************************************************/

static int cmp_strings(const void *a, const void *b)
{
  return strncmp((const char *)a, (const char *)b, 3);
}

static int default_interpreter(gp_file *in, const pcl_Command *cmd)
{
  /* Skip over arguments for those commands which are known to have them */
  if (cmd->kind >= 3) {
    if (strncmp((const char *)cmd->chars, "%-X", 3) == 0 && cmd->i == 12345) {
      /* Universal Exit Language (UEL)/Start of PJL */
      int c;

      do {
        c = fgetc(in);
        if (c != '@') break;
        do c = fgetc(in); while (c != EOF && c != '\n');
      } while (c != EOF);
      if (c != EOF) ungetc(c, in);
    }
    else if (cmd->i > 0) {
      static const char with_args[][4] = {
        /* Must be sorted with respect to cmp_strings() */
        "&bW",
        "&iW",
        "&pX",	/* Transparent Print Mode */
        "(sW",	/* Download Character */
        ")sW",	/* Create Font */
        "*bV",	/* Transfer Raster Graphics Data by Plane */
        "*bW",	/* Transfer Raster Graphics Data by Row */
        "*dW",	/* Palette Configuration */
        "*gW",	/* Configure Raster Data */
        "*oW"};

      if (bsearch(cmd->chars, with_args,
          sizeof(with_args)/sizeof(with_args[0]), sizeof(with_args[0]),
          &cmp_strings) != NULL) {
        int j;

        j = cmd->i;
        while (j > 0 && fgetc(in) != EOF) j--;
        if (j > 0) {
          message(1, "Premature EOF on input.");
          return -1;
        }
      }
    }
  }
  else if (cmd->kind == 2 && cmd->chars[0] == 'Y') {
    /* Display Functions Mode ON */
    int c;

    /* Read until EOF or Display Functions Mode OFF */
    do {
      do c = fgetc(in); while (c != EOF && c != ESC);
      if (c != EOF) c = fgetc(in);
    } while (c != EOF && c != 'Z');

    if (c != 'Z') {
      message(1, "Premature EOF on input.");
      return -1;
    }
  }

  return 0;
}

/*****************************************************************************/

static int default_handler(gp_file *in)
{
  int c;

  do {
    c = fgetc(in);
  } while (c != EOF && !pcl_is_control_code(c));
  if (c != EOF) ungetc(c, in);

  return 0;
}

/******************************************************************************

  Function: pcl_scan

  This function scans a PCL file, separating it into printer commands and
  unknown data, and calling the functions '(*interpreter)()' and '(*handler)()'
  with these parts, respectively.

  'in' must have been opened as a binary file and for reading.
  'interpreter' or 'handler' may be null in which case default routines are
  used.
  If a non-NULL function pointer is provided for 'interpreter' or 'handler',
  the function will be passed the value of 'idata' or 'hdata', respectively.

  This function returns zero on success and a negative value otherwise.
  If one of the interpreter or data handler functions returns a negative value,
  processing will stop and the value will be returned as the return value of
  pcl_scan().

******************************************************************************/

int pcl_scan(gp_file *in, pcl_CommandInterpreter interpreter, void *idata,
  pcl_UnknownDataHandler handler, void *hdata)
{
  int
    c,
    rc = 0;

  /* Open catalogue descriptor */
  catd = catopen("pcltools", 0);
  if (catd == (nl_catd)(-1) && errno != ENOENT)
     /* A system returning ENOENT if no catalogue is available is for example
        Sun Solaris 2.5. */
    fprintf(stderr,
      "?-W pclscan: Error trying to open message catalogue: %s.\n",
      strerror(errno));

  /* Loop over input */
  while (rc >= 0 && (c = fgetc(in)) != EOF) {
    if (c == ESC) {
      pcl_Command command;

      if ((c = fgetc(in)) == EOF) {
        rc = -1;
        message(1, "Premature EOF on input.");
        break;
      }
      command.chars[0] = c;
      if (48 <= c && c <= 126) {
        /* Two-character escape sequence */
        command.kind = 2;
        if (interpreter == NULL ||
            (rc = (*interpreter)(in, &command, idata)) > 0)
          rc = default_interpreter(in, &command);
      }
      else {
        int continued;

        /* Parameterized escape sequence or garbage. The character we've just
           read is the "parameterized character" and it should be in the range
           33-47. */

        /* Now the group character (should be in the range 96-126, but HP uses
           sometimes at least '-', which has the value 45) */
        if ((c = fgetc(in)) == EOF) {
          rc = -1;
          message(1, "Premature EOF on input.");
          break;
        }
        command.chars[1] = c;

        continued = 0;
        do {
          /* Now for the value */
          if ((c = fgetc(in)) == EOF) {
            rc = -1;
            message(1, "Premature EOF on input.");
            break;
          }
          if (c == '+' || c == '-' || '0' <= c && c <= '9') {
            if (c == '+' || c == '-') command.prefix = c;
            else command.prefix = ' ';
            ungetc(c, in);
            if (fscanf(in, "%d", &command.i) != 1) {
              rc = -1;
              message(2, "Syntax error in value field.");
              break;
            }

            /* Decimal point? */
            if ((c = fgetc(in)) == EOF) {
              rc = -1;
              message(1, "Premature EOF on input.");
              break;
            }
            command.scale = 0;
            command.fraction = 0;
            if (c == '.') {
              command.scale = 1;
              while ((c = fgetc(in)) != EOF && '0' <= c && c <= '9') {
                command.fraction = command.fraction*10 + (c - '0');
                command.scale *= 10;
              }
              if (c == EOF) {
                rc = -1;
                message(1, "Premature EOF on input.");
                break;
              }
              if (command.prefix == '-') command.fraction = -command.fraction;
            }
          }
          else {
            command.prefix = '\0'; /* no value given */
            command.i = 0;
          }

          /* Final character */
          if (96 <= c && c <= 126) {
            /* Parameter character */
            command.chars[2] = c - ('a' - 'A');
            command.kind = (continued? 6: 4);
            continued = 1;
          }
          else {
            /* Termination character (should be in the range 64-94) */
            command.chars[2] = c;
            command.kind = (continued? 5: 3);
            continued = 0;
          }

          if (interpreter == NULL ||
              (rc = (*interpreter)(in, &command, idata)) > 0)
            rc = default_interpreter(in, &command);
        } while (rc == 0 && continued);
      }
    }
    else if (pcl_is_control_code(c)) {
      pcl_Command command;

      command.chars[0] = c;
      command.kind = 1;
      command.i = 1;	/* number of occurrences */

      while ((c = fgetc(in)) != EOF && c == command.chars[0]) command.i++;
      if (c != EOF) ungetc(c, in);

      if (interpreter == NULL ||
          (rc = (*interpreter)(in, &command, idata)) > 0)
        rc = default_interpreter(in, &command);
    }
    else {
      ungetc(c, in);
      if (handler == NULL || (rc = (*handler)(in, hdata)) > 0)
        rc = default_handler(in);
    }
  }

  /* Close catalogue descriptor */
  if (catd != (nl_catd)-1) catclose(catd);

  return rc;
}
