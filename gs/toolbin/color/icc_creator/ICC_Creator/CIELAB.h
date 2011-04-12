/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

#pragma once

#define MAX_NAME_SIZE 260

typedef struct cielab_s {

    float lstar;
    float astar;
    float bstar;

} cielab_t;

typedef struct colornames_s {

    char name[MAX_NAME_SIZE];
    int length;

} colornames_t;

