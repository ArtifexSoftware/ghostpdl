/* Copyright (C) 2001-2006 artofcode LLC.
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
/* Font API support */

/* GS includes : */
#include "gx.h"
/* UFST includes : */
#undef true	/* GS/UFST definition conflict. */
#undef false	/* GS/UFST definition conflict. */
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"
/* GS includes : */
#include "gxfapi.h"

/* -------------------- UFST callback dispatcher ------------- */

/*  This code provides dispatching UFST callbacks to GS or PCL. */

struct IF_STATE;

private LPUB8 stub_PCLchId2ptr(IF_STATE *pIFS, UW16 chId)
{   return NULL;
}

/*
    The following variable is defined statically until the
    reentrancy of graphics library is fixed.
 */

private LPUB8 (*m_PCLchId2ptr)(IF_STATE *pIFS, UW16 chId) = stub_PCLchId2ptr;


LPUB8 PCLEO_charptr(IF_STATE *pIFS, LPUB8 pfont_hdr, UW16  sym_code)
{   return NULL; /* Never used. */
}

LPUB8 PCLchId2ptr(IF_STATE *pIFS, UW16 chId)
{   return m_PCLchId2ptr(FSA chId);
}

LPUB8 PCLglyphID2Ptr(IF_STATE *pIFS, UW16 glyphID)
{   /* Same as PCLchId2ptr. */
    return m_PCLchId2ptr(FSA glyphID);
}

void gx_set_UFST_Callbacks(LPUB8 (*p_PCLchId2ptr)(IF_STATE *pIFS, UW16 chId))
{   m_PCLchId2ptr = p_PCLchId2ptr;
}

void gx_reset_UFST_Callbacks()
{   m_PCLchId2ptr = stub_PCLchId2ptr;
}
