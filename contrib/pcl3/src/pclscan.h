/******************************************************************************
  File:     $Id: pclscan.h,v 1.6 2000-10-22 11:05:34+02 Martin Rel $
  Contents: Header for PCL scanner
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany, e-mail: Martin.Lottermoser@t-online.de

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

#ifndef _pclscan_h	/* Inclusion protection */
#define _pclscan_h

/*****************************************************************************/

/* Standard headers */
#include <stdio.h>

/*****************************************************************************/

/* Macro to convert a termination character into a parameter character.
   It should only be applied to arguments in the range 64-94.
 */
#define pcl_to_parameter_character(c)	((c) + ('a' - 'A'))

/*****************************************************************************/

#ifndef _pcl_Octet_defined
#define _pcl_Octet_defined
typedef unsigned char pcl_Octet;
#endif

typedef struct {
  unsigned short kind;
   /* The value can be in the range 1-6:
      1:  control code. chars[0] is this code and 'i' gives the number of its
          occurrences (positive).
      2:  two-character escape sequence. chars[0] is the second character, the
          first was ESC.
      3:  parameterized escape sequence. chars[0] is the parameterized
          character, chars[1] is the group character, and chars[2] is the
          termination character.
      4:  same as 3, except that the sequence is not terminated. In this case
          chars[2] is the octet which would be used as the termination
          character, not the octet actually read from the input stream
          (parameter character). pcl_to_parameter_character() can be used to
          obtain the latter.
      5:  same as 3, except that this is a part of a combined escape sequence
          started earlier.
      6:  same as 4, except that this is a part of a combined escape sequence
          started earlier.
    */
  pcl_Octet chars[3];
  pcl_Octet prefix;
   /* This can be one of:
      '\0': no value given
      ' ':  value given without a sign
      '+':  value given with prefix "+"
      '-':  value given with prefix "-"
    */
  int i;	/* integer part of the value (including the sign) */
  unsigned int scale;
    /*  This can be zero or any power of ten. A value of zero means that the
        value was given without a decimal point, a value of one that there were
        no digits after the point. */
  int fraction;
   /* fractional part of the value (including the sign). The entire value is
      i + ((float)fraction)/scale, provided 'scale' is non-zero. */
} pcl_Command;

/*
  A negative return code indicates an error which will terminate scanning.
  A return code of zero indicates success.
  A positive return code indicates that the interpreter or handler does not know
  the command or the data and that the scanner should handle the case. */
typedef int (*pcl_CommandInterpreter)(FILE *in, const pcl_Command *cmd,
  void *idata);
typedef int (*pcl_UnknownDataHandler)(FILE *in, void *hdata);
/*  A handler for unknown data should never read over an ESC character which
    might start a printer command. */

extern int pcl_is_control_code(int c);
extern int pcl_scan(FILE *in, pcl_CommandInterpreter interpreter, void *idata,
  pcl_UnknownDataHandler handler, void *hdata);

/*****************************************************************************/

#endif	/* Inclusion protection */
