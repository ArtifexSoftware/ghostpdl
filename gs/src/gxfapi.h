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

void gx_set_UFST_Callbacks(P3(LPUB8 (*p_PCLEO_charptr)(P2(LPUB8 pfont_hdr, UW16  sym_code)),
                              LPUB8 (*p_PCLchId2ptr)(P2(IF_STATE *pIFS, UW16 chId)),
                              LPUB8 (*p_PCLglyphID2Ptr)(P2(IF_STATE *pIFS, UW16 glyphID))));

void gx_reset_UFST_Callbacks(P0());
