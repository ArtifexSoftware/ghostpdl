/* Copyright (C) 1991, 2000 Aladdin Enterprises.  All rights reserved.
  
  This software is provided AS-IS with no warranty, either express or
  implied.
  
  This software is distributed under license and may not be copied,
  modified or distributed except as expressly authorized under the terms
  of the license contained in the file LICENSE in this distribution.
  
  For more information about licensing, please refer to
  http://www.ghostscript.com/licensing/. For information on
  commercial licensing, go to http://www.artifex.com/licensing/ or
  contact Artifex Software, Inc., 101 Lucas Valley Road #110,
  San Rafael, CA  94903, U.S.A., +1(415)492-9861.
*/

/* $Id: gxfapiu.c,v 1.6 2002/06/16 05:48:56 lpd Exp $ */
/* Font API support : UFST callback dispatch. */

/* GS includes : */
#include "gx.h"
/* UFST includes : */
#undef true
#undef false
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"
/* GS includes : */
#include "gxfapiu.h"

#define MAX_STATIC_FCO_COUNT 2

/* -------------------- UFST callback dispatcher ------------- */

/*  This code provides dispatching UFST callbacks to GS or PCL. */

struct IF_STATE;

private LPUB8 stub_PCLEO_charptr(FSP LPUB8 pfont_hdr, UW16  sym_code)
{   return NULL;
}

private LPUB8 stub_PCLchId2ptr(FSP UW16 chId)
{   return NULL;
}

private LPUB8 stub_PCLglyphID2Ptr(FSP UW16 glyphID)
{   return NULL;
}

/*
    The following 4 variables are defined statically 
    because the language switching project doesn't define 
    a general dynamic context for all interpreters.
 */

static LPUB8 (*m_PCLEO_charptr)(FSP LPUB8 pfont_hdr, UW16  sym_code) = stub_PCLEO_charptr;
static LPUB8 (*m_PCLchId2ptr)(FSP UW16 chId) = stub_PCLchId2ptr;
static LPUB8 (*m_PCLglyphID2Ptr)(FSP UW16 glyphID) = stub_PCLglyphID2Ptr;
#if !UFST_REENTRANT
static bool global_UFST_lock = false;
static fco_list_elem static_fco_list[MAX_STATIC_FCO_COUNT];
static char static_fco_paths[MAX_STATIC_FCO_COUNT][gp_file_name_sizeof];
static int static_fco_count = 0;
#endif


LPUB8 PCLEO_charptr(FSP LPUB8 pfont_hdr, UW16  sym_code)
{   return m_PCLEO_charptr(FSA pfont_hdr, sym_code);
}

LPUB8 PCLchId2ptr(FSP UW16 chId)
{   return m_PCLchId2ptr(FSA chId);
}

LPUB8 PCLglyphID2Ptr(FSP UW16 glyphID)
{   return m_PCLglyphID2Ptr(FSA glyphID);
}

/* Set UFST callbacks. */
/* Warning : the language switch progect doesn't guarantee
   that this function is called when switching
   to another interpreter. Therefore each interpreter must take
   care for its own callback methods before they 
   may be called by UFST.
 */
 /* Warning : this function may cause a reentrancy problem
    due to a modification of static variables.
    Nevertheless this problem isn't important in a
    sngle interpreter build because the values
    really change on the first demand only.
    See also a comment in gs_fapiufst_finit.
  */
void gx_set_UFST_Callbacks(LPUB8 (*p_PCLEO_charptr)(FSP LPUB8 pfont_hdr, UW16  sym_code),
                           LPUB8 (*p_PCLchId2ptr)(FSP UW16 chId),
                           LPUB8 (*p_PCLglyphID2Ptr)(FSP UW16 glyphID))
{   m_PCLEO_charptr = (p_PCLEO_charptr != NULL ? p_PCLEO_charptr : stub_PCLEO_charptr);
    m_PCLchId2ptr = (p_PCLchId2ptr != NULL ? p_PCLchId2ptr : stub_PCLchId2ptr);
    m_PCLglyphID2Ptr = (p_PCLglyphID2Ptr != NULL ? p_PCLglyphID2Ptr : stub_PCLglyphID2Ptr);
}

#if !UFST_REENTRANT
/* The following 2 functions provide a locking of a 
   global static UFST instance, which must be a singleton 
   when UFST works for embedded multilanguage system. 
   When setting a lock, the language swithing code
   must initialize and uninitialise UFST by immediate calls.
 */
void gs_set_UFST_lock(bool lock)
{
    global_UFST_lock = lock;
}
bool gs_get_UFST_lock(void)
{
    return global_UFST_lock;
}
#endif /*!UFST_REENTRANT*/

/* Access to the static FCO list for the language switching project. */

fco_list_elem *gx_UFST_find_static_fco(const char *font_file_path)
{   
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
	if (!strcmp(static_fco_list[i].file_path, font_file_path))
	    return &static_fco_list[i];
#endif
    return NULL;
}

fco_list_elem *gx_UFST_find_static_fco_handle(SW16 fcHandle)
{   
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
	if (static_fco_list[i].fcHandle == fcHandle)
	    return &static_fco_list[i];
#endif
    return NULL;
}

UW16 gx_UFST_open_static_fco(const char *font_file_path, SW16 *result_fcHandle)
{   
#if !UFST_REENTRANT
    SW16 fcHandle;
    UW16 code;
    fco_list_elem *e;

    if (static_fco_count >= MAX_STATIC_FCO_COUNT)
	return ERR_fco_NoMem;
    code = CGIFfco_Open(FSA (UB8 *)font_file_path, &fcHandle);
    if (code != 0)
	return code;
    e = &static_fco_list[static_fco_count];
    strncpy(static_fco_paths[static_fco_count], font_file_path, 
	    sizeof(static_fco_paths[static_fco_count]));
    e->file_path = static_fco_paths[static_fco_count];
    e->fcHandle = fcHandle;
    e->open_count = -1; /* Unused for static FCOs. */
    static_fco_count++;
    *result_fcHandle = fcHandle;
    return 0;
#else
    **result_fcHandle = -1;
    return ERR_fco_NoMem;
#endif
}

UW16 gx_UFST_close_static_fco(SW16 fcHandle)
{   
#if !UFST_REENTRANT
    int i;

    for (i = 0; i < static_fco_count; i++)
	if (static_fco_list[i].fcHandle == fcHandle)
	    break;
    if (i >= static_fco_count)
	return ERR_fco_NoMem;
    CGIFfco_Close(FSA fcHandle);
    for (i++; i < static_fco_count; i++) {
	static_fco_list[i - 1] = static_fco_list[i];
	strcpy(static_fco_paths[i - 1], static_fco_paths[i]);
    }
    static_fco_count--;
#endif
    return 0;
}

