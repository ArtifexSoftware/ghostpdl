/* Copyright (C) 1991, 2000 Aladdin Enterprises.  All rights reserved.
  
  This file is part of AFPL Ghostscript.
  
  AFPL Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author or
  distributor accepts any responsibility for the consequences of using it, or
  for whether it serves any particular purpose or works at all, unless he or
  she says so in writing.  Refer to the Aladdin Free Public License (the
  "License") for full details.
  
  Every copy of AFPL Ghostscript must include a copy of the License, normally
  in a plain ASCII text file named PUBLIC.  The License grants you the right
  to copy, modify and redistribute AFPL Ghostscript, but only under certain
  conditions described in the License.  Among other things, the License
  requires that the copyright notice and this notice be preserved on all
  copies.
*/

/*$Id$ */
/* Font API support */

/* GS includes : */
#include "gx.h"
/* UFST includes : */
#include "CGCONFIG.H"
#include "PORT.H"    
#include "SHAREINC.H"
/* GS includes : */
#include "gxfapi.h"

/* -------------------- UFST callback dispatcher ------------- */

/*  This code provides dispatching UFST callbacks to GS or PCL. */

struct IF_STATE;

private LPUB8 stub_PCLEO_charptr(LPUB8 pfont_hdr, UW16  sym_code)
{   return NULL;
}

private LPUB8 stub_PCLchId2ptr(IF_STATE *pIFS, UW16 chId)
{   return NULL;
}

private LPUB8 stub_PCLglyphID2Ptr(IF_STATE *pIFS, UW16 glyphID)
{   return NULL;
}

/*
    The following 3 variables are defined statically until the
    reentrancy of graphics library is fixed.
    Also we are waiting for Agfa to fix the reentrancy of UFST callback PCLEO_charptr.
 */

private LPUB8 (*m_PCLEO_charptr)(LPUB8 pfont_hdr, UW16  sym_code) = stub_PCLEO_charptr;
private LPUB8 (*m_PCLchId2ptr)(IF_STATE *pIFS, UW16 chId) = stub_PCLchId2ptr;
private LPUB8 (*m_PCLglyphID2Ptr)(IF_STATE *pIFS, UW16 glyphID) = stub_PCLglyphID2Ptr;


LPUB8 PCLEO_charptr(LPUB8 pfont_hdr, UW16  sym_code)
{   return m_PCLEO_charptr(pfont_hdr, sym_code);
}

LPUB8 PCLchId2ptr(IF_STATE *pIFS, UW16 chId)
{   return m_PCLchId2ptr(FSA chId);
}

LPUB8 PCLglyphID2Ptr(IF_STATE *pIFS, UW16 glyphID)
{   return m_PCLglyphID2Ptr(FSA glyphID);
}

void gx_set_UFST_Callbacks(LPUB8 (*p_PCLEO_charptr)(P2(LPUB8 pfont_hdr, UW16  sym_code)),
                           LPUB8 (*p_PCLchId2ptr)(P2(IF_STATE *pIFS, UW16 chId)),
                           LPUB8 (*p_PCLglyphID2Ptr)(P2(IF_STATE *pIFS, UW16 glyphID)))
{   m_PCLEO_charptr = p_PCLEO_charptr;
    m_PCLchId2ptr = p_PCLchId2ptr;
    m_PCLglyphID2Ptr = p_PCLglyphID2Ptr;
}

void gx_reset_UFST_Callbacks()
{   m_PCLEO_charptr = stub_PCLEO_charptr;
    m_PCLchId2ptr = stub_PCLchId2ptr;
    m_PCLglyphID2Ptr = stub_PCLglyphID2Ptr;
}
