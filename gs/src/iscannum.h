/* Copyright (C) 1994, 1995, 1999 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Interface to Ghostscript number scanner */

#ifndef iscannum_INCLUDED
#  define iscannum_INCLUDED

/*
 * Scan a number.  If the number consumes the entire string, return 0;
 * if not, set *psp to the first character beyond the number and return 1.
 * Note that scan_number does not mark the result ref as "new".
 */
int scan_number(P5(const byte * sp, const byte * end, int sign, ref * pref,
		   const byte ** psp));

#endif /* iscannum_INCLUDED */
