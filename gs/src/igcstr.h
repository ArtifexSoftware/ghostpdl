/* Copyright (C) 1995 Aladdin Enterprises.  All rights reserved.
  
  This software is licensed to a single customer by Artifex Software Inc.
  under the terms of a specific OEM agreement.
*/

/*$RCSfile$ $Revision$ */
/* Internal interface to string garbage collector */

#ifndef igcstr_INCLUDED
#  define igcstr_INCLUDED

/* Exported by ilocate.c for igcstr.c */
chunk_t *gc_locate(P2(const void *, gc_state_t *));

/* Exported by igcstr.c for igc.c */
void gc_strings_set_marks(P2(chunk_t *, bool));
bool gc_string_mark(P4(const byte *, uint, bool, gc_state_t *));
void gc_strings_clear_reloc(P1(chunk_t *));
void gc_strings_set_reloc(P1(chunk_t *));
void gc_strings_compact(P1(chunk_t *));
string_proc_reloc(igc_reloc_string);
const_string_proc_reloc(igc_reloc_const_string);

#endif /* igcstr_INCLUDED */
