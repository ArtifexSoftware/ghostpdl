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
/* Font API support : UFST common initialization */

/* GS includes : */
#include "gx.h"

/* UFST includes : */
#undef true
#undef false
#undef frac_bits
#include "cgconfig.h"
#include "ufstport.h"
#include "shareinc.h"
/* GS includes : */
#include "gxfapiu.h"

#define MAX_STATIC_FCO_COUNT 4

/* -------------------- UFST callback dispatcher ------------- */

/*  This code provides dispatching UFST callbacks to the PDL */

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

/* This global is defined here because the UFST needs it to link against. */
/* set to 1 to display UFST debugging statements */
GLOBAL const SW16 trace_sw = 0;


/*
    The following 4 variables are defined statically 

They could be stored in the gs_lib_ctx but that would require casting the types
to avoid including ufst's typedefs. 
*/

static LPUB8 (*m_PCLEO_charptr)(FSP LPUB8 pfont_hdr, UW16  sym_code) = stub_PCLEO_charptr;
static LPUB8 (*m_PCLchId2ptr)(FSP UW16 chId) = stub_PCLchId2ptr;
static LPUB8 (*m_PCLglyphID2Ptr)(FSP UW16 glyphID) = stub_PCLglyphID2Ptr;
static fco_list_elem static_fco_list[MAX_STATIC_FCO_COUNT] = {0, 0, 0, 0};
static char static_fco_paths[MAX_STATIC_FCO_COUNT][gp_file_name_sizeof];
static int static_fco_count = 0;

LPUB8 PCLEO_charptr(FSP LPUB8 pfont_hdr, UW16  sym_code)
{   return m_PCLEO_charptr(FSA pfont_hdr, sym_code);
}

LPUB8 PCLchId2ptr(FSP UW16 chId)
{   return m_PCLchId2ptr(FSA chId);
}

LPUB8 PCLglyphID2Ptr(FSP UW16 glyphID)
{   return m_PCLglyphID2Ptr(FSA glyphID);
}

/* Set UFST callbacks. Each PDL will want it's own character build function and must set the callbacks
 * upon language entry/initialization.
 */

void gx_set_UFST_Callbacks(LPUB8 (*p_PCLEO_charptr)(FSP LPUB8 pfont_hdr, UW16  sym_code),
                           LPUB8 (*p_PCLchId2ptr)(FSP UW16 chId),
                           LPUB8 (*p_PCLglyphID2Ptr)(FSP UW16 glyphID))
{   m_PCLEO_charptr = (p_PCLEO_charptr != NULL ? p_PCLEO_charptr : stub_PCLEO_charptr);
    m_PCLchId2ptr = (p_PCLchId2ptr != NULL ? p_PCLchId2ptr : stub_PCLchId2ptr);
    m_PCLglyphID2Ptr = (p_PCLglyphID2Ptr != NULL ? p_PCLglyphID2Ptr : stub_PCLglyphID2Ptr);
}

#define MAX_OPEN_LIBRARIES  5   /* NB */
#define BITMAP_WIDTH        1   /* must be 1, 2, 4 or 8 */


/* returns negative on error, 
 * 1 on I just initialized for the first time and you might want to as well.
 * 0 I've already initialized but its ok to call me.
 * NB: since this is using a static library UFST the initialization is static as well 
 * ie no multi-threading through here, once per process please.  
 */
int
gx_UFST_init(UB8 ufst_root_dir[])
{
    IFCONFIG            config_block;
    int status;
    static bool ufst_initialized = false;

    if (!ufst_initialized) {
	strcpy(config_block.ufstPath, ufst_root_dir);
	strcpy(config_block.typePath, ufst_root_dir);
	config_block.num_files = MAX_OPEN_LIBRARIES;  /* max open library files */
	config_block.bit_map_width = BITMAP_WIDTH;    /* bitmap width 1, 2 or 4 */

	/* These parameters were set in open_UFST() (fapiufst.c) but were left
	   uninitialized in pl_load_built_in_fonts() (plulfont.c). */
	config_block.typePath[0] = 0;

	if ((status = CGIFinit(FSA0)) != 0) {
	    dprintf1("CGIFinit() error: %d\n", status);
	    return status;
	}
	if ((status = CGIFconfig(FSA &config_block)) != 0) {
	    dprintf1("CGIFconfig() error: %d\n", status);
	    return status;
	}
	if ((status = CGIFenter(FSA0)) != 0) {
	    dprintf1("CGIFenter() error: %u\n",status);
	    return status;
	}
	ufst_initialized = TRUE;
	return 1; /* first time, caller may have more initialization to do */
    }
    return 0;  /* been here before, caller has no more initialization do do */
}

int
gx_UFST_fini(void)
{
    CGIFexit(FSA0);
    return 0;
}

/* Access to the static FCO list for the language switching project. */

fco_list_elem *gx_UFST_find_static_fco(const char *font_file_path)
{   
    int i;

    for (i = 0; i < static_fco_count; i++)
	if (!strcmp(static_fco_list[i].file_path, font_file_path))
	    return &static_fco_list[i];

    return NULL;
}

fco_list_elem *gx_UFST_find_static_fco_handle(SW16 fcHandle)
{   
    int i;

    for (i = 0; i < static_fco_count; i++)
	if (static_fco_list[i].fcHandle == fcHandle)
	    return &static_fco_list[i];

    return NULL;
}

SW16 gx_UFST_find_fco_handle_by_name(const char *font_file_path)
{   
    fco_list_elem *fco = gx_UFST_find_static_fco(font_file_path);

    if (fco)
	return fco->fcHandle;
    return 0; // or is it -1 
}

UW16 gx_UFST_open_static_fco(const char *font_file_path, SW16 *result_fcHandle)
{   
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
}

UW16 gx_UFST_close_static_fco(SW16 fcHandle)
{   
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

    return 0;
}

void gx_UFST_close_static_fcos()
{
    for(; static_fco_count; )
	gx_UFST_close_static_fco(static_fco_list[0].fcHandle);
}
