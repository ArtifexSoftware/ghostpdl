/* Copyright (C) 1994 Aladdin Enterprises.  All rights reserved.

   This software is licensed to a single customer by Artifex Software Inc.
   under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* Declarations for exits */

#ifndef gsexit_INCLUDED
#  define gsexit_INCLUDED

void gs_exit_with_code(P2(int exit_status, int code));
void gs_exit(P1(int exit_status));

#define gs_abort() gs_exit(1)

/* The only reason we export gs_exit_status is so that window systems */
/* with alert boxes can know whether to pause before exiting if */
/* the program terminates with an error.  There must be a better way .... */
extern int gs_exit_status;

#endif /* gsexit_INCLUDED */
