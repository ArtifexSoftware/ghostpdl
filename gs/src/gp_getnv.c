/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Standard implementation of gp_getenv */
#include "stdio_.h"
#include "string_.h"
#include "gsmemory.h"
#include "gstypes.h"
#include "gp.h"

/* Import the C getenv function. */
extern char *getenv(P1(const char *));

/* Get the value of an environment variable.  See gp.h for details. */
int
gp_getenv(const char *key, char *ptr, int *plen)
{
    const char *str = getenv(key);

    if (str) {
	int len = strlen(str);

	if (len < *plen) {
	    /* string fits */
	    strcpy(ptr, str);
	    *plen = len + 1;
	    return 0;
	}
	/* string doesn't fit */
	*plen = len + 1;
	return -1;
    }
    /* missing key */
    if (*plen > 0)
	*ptr = 0;
    *plen = 1;
    return 1;
}
