/* Copyright (C) 2000 Aladdin Enterprises.  All rights reserved.

   This file is part of Aladdin Ghostscript.

   Aladdin Ghostscript is distributed with NO WARRANTY OF ANY KIND.  No author
   or distributor accepts any responsibility for the consequences of using it,
   or for whether it serves any particular purpose or works at all, unless he
   or she says so in writing.  Refer to the Aladdin Ghostscript Free Public
   License (the "License") for full details.

   Every copy of Aladdin Ghostscript must include a copy of the License,
   normally in a plain ASCII text file named PUBLIC.  The License grants you
   the right to copy, modify and redistribute Aladdin Ghostscript, but only
   under certain conditions described in the License.  Among other things, the
   License requires that the copyright notice and this notice be preserved on
   all copies.
 */

/*$Id$ */
/* Common data definitions for CMaps and CID-keyed fonts */

#ifndef gxcid_INCLUDED
#  define gxcid_INCLUDED

#include "gsstype.h"

/* Define the structure for CIDSystemInfo. */
typedef struct gs_cid_system_info_s {
    gs_const_string Registry;
    gs_const_string Ordering;
    int Supplement;
} gs_cid_system_info_t;
extern_st(st_cid_system_info);
extern_st(st_cid_system_info_element);
#define public_st_cid_system_info() /* in gsfcid.c */\
  gs_public_st_const_strings2(st_cid_system_info, gs_cid_system_info_t,\
    "gs_cid_system_info_t", cid_si_enum_ptrs, cid_si_reloc_ptrs,\
    Registry, Ordering)
#define st_cid_system_info_num_ptrs 2
#define public_st_cid_system_info_element() /* in gsfcid.c */\
  gs_public_st_element(st_cid_system_info_element, gs_cid_system_info_t,\
    "gs_cid_system_info_t[]", cid_si_elt_enum_ptrs, cid_si_elt_reloc_ptrs,\
    st_cid_system_info)

#endif /* gxcid_INCLUDED */
