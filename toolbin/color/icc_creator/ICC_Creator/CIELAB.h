/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
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
