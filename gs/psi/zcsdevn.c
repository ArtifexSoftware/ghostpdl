/* Copyright (C) 2001-2006 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id$ */
/* DeviceN color space support */
#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "gxcspace.h"		/* must precede gscolor2.h */
#include "gscolor2.h"
#include "gscdevn.h"
#include "gxcdevn.h"
#include "estack.h"
#include "ialloc.h"
#include "icremap.h"
#include "ifunc.h"
#include "igstate.h"
#include "iname.h"
#include "zht2.h"

/* Imported from gscdevn.c */
extern const gs_color_space_type gs_color_space_type_DeviceN;

/* ------ Initialization procedure ------ */

const op_def zcsdevn_op_defs[] =
{
    op_def_begin_ll3(),
#if 0
    {"1.setdevicenspace", zsetdevicenspace},
    {"1.attachdevicenattributespace", zattachdevicenattributespace},
#endif
    op_def_end(0)
};
