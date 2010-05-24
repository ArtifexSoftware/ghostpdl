/* Copyright (C) 2001-2007 Artifex Software, Inc.
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
/* Sample implementation for client custom processing of color spaces. */

/*
 * This module has been created to demonstrate how to add support for the use
 * of custom color handling to the Ghostscript graphics library via a custom color
 * callback mechanism.
 *
 * See the comments at the start of src/gsncdummy.c for more information.
 */

#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gscdefs.h"
#include "gsnamecl.h"
#include "malloc_.h"
#include "ghost.h"
#include "oper.h"
#include "gsncdummy.h"

/*
 * This procedure is here to simplify debugging.  Normally one would expect the
 * custom color callback structure to be set up by a calling application.
 * Since I do not have a calling application, I need a simple way to setup the
 * callback parameter.  The callback parameter is passed as a string value.
 * This routine puts the address of our demo callback structure into the
 * provided string.
 *
 * This routine allows the demo version of the PANTONE logic to be enabled
 * by adding the following to the command line:
 *  -c "<< /CustomColorCallback 32 string .pantonecallback >> setsystemparams" -f
 */

/* <string> .pantonecallback <string> */
static int
zpantonecallback(i_ctx_t *i_ctx_p)
{
    return 0;
}

/* ------ Initialization procedure ------ */

const op_def pantone_op_defs[] =
{
    {"1.pantonecallback", zpantonecallback},
    op_def_end(0)
};
