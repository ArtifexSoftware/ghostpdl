/* Copyright (C) 1993, 1994, 1998 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Interface to procedures in iutil2.c */

#ifndef iutil2_INCLUDED
#  define iutil2_INCLUDED

/* ------ Password utilities ------ */

/* Define the password structure. */
/* NOTE: MAX_PASSWORD must match the initial password lengths in gs_lev2.ps. */
#define MAX_PASSWORD 64		/* must be at least 11 */
typedef struct password_s {
    uint size;
    byte data[MAX_PASSWORD];
} password;

# define NULL_PASSWORD {0, {0}}

/* Transmit a password to or from a parameter list. */
int param_read_password(P3(gs_param_list *, const char *, password *));
int param_write_password(P3(gs_param_list *, const char *, const password *));

/* Check a password from a parameter list. */
/* Return 0 if OK, 1 if not OK, or an error code. */
int param_check_password(P2(gs_param_list *, const password *));

/* Read a password from, or write a password into, a dictionary */
/* (presumably systemdict). */
int dict_read_password(P3(password *, const ref *, const char *));
int dict_write_password(P3(const password *, ref *, const char *));

#endif /* iutil2_INCLUDED */
