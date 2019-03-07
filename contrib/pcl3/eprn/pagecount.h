/******************************************************************************
  File:     $Id: pagecount.h,v 1.3 2000/11/19 07:05:17 Martin Rel $
  Contents: Header for pagecount file functions
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany; e-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

#ifndef _pagecount_h	/* Inclusion protection */
#define _pagecount_h

#include "std.h"

#ifdef _MSC_VER
#define EPRN_NO_PAGECOUNTFILE
#else

/*****************************************************************************/

/*  The following two functions are used to read and write a "page count file".
    Such a file should contain a single line with a non-negative decimal
    integer. pcf_getcount() reads the file and returns the value,
    pcf_inccount() increases the number in the file by the specified amount.
    A non-existent file is treated as a file containing the number zero.

    The routines are safe against concurrent access.
*/
extern int pcf_getcount(const gs_memory_t *mem, const char *filename, unsigned long *count);
extern int pcf_inccount(const gs_memory_t *mem, const char *filename, unsigned long by);

/*****************************************************************************/

#endif

#endif	/* Inclusion protection */
