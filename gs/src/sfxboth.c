/* Copyright (C) 1997 Aladdin Enterprises.  All rights reserved.
 * This software is licensed to a single customer by Artifex Software Inc.
 * under the terms of a specific OEM agreement.
 */

/*$RCSfile$ $Revision$ */
/* File stream implementation using both stdio and direct OS calls */

#include "sfxstdio.c"

#define KEEP_FILENO_API		/* see sfxfd.c */
#include "sfxfd.c"
