/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Defines common to gdevpm.c, gspmdrv.c and PM GSview */

#ifndef gdevpm_INCLUDED
#  define gdevpm_INCLUDED

#define SHARED_NAME "\\SHAREMEM\\%s"
#define SYNC_NAME   "\\SEM32\\SYNC_%s"
#define NEXT_NAME   "\\SEM32\\NEXT_%s"
#define MUTEX_NAME  "\\SEM32\\MUTEX_%s"
#define QUEUE_NAME  "\\QUEUES\\%s"

#define GS_UPDATING	1
#define GS_SYNC		2
#define GS_PAGE		3
#define GS_CLOSE	4
#define GS_ERROR	5
#define GS_PALCHANGE	6
#define GS_BEGIN	7
#define GS_END		8

#endif /* gdevpm_INCLUDED */
