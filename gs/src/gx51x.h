/* Copyright (C) 1998 Aladdin Enterprises.  All rights reserved.
  
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

/* gx51x.h */
/* 5.1x patches for 5c project */
/* This file is #included in std.h. */

/* Make dlp* synonymous with dp*. */
#define dlputc(chr) dputc(chr)
#define dlputs(str) dputs(str)
#define dlprintf1(str,arg1)\
  dprintf1(str, arg1)
#define dlprintf2(str,arg1,arg2)\
  dprintf2(str, arg1, arg2)
#define dlprintf3(str,arg1,arg2,arg3)\
  dprintf3(str, arg1, arg2, arg3)
#define dlprintf4(str,arg1,arg2,arg3,arg4)\
  dprintf4(str, arg1, arg2, arg3, arg4)
#define dlprintf5(str,arg1,arg2,arg3,arg4,arg5)\
  dprintf5(str, arg1, arg2, arg3, arg4, arg5)
#define dlprintf6(str,arg1,arg2,arg3,arg4,arg5,arg6)\
  dprintf6(str, arg1, arg2, arg3, arg4, arg5, arg6)
#define dlprintf7(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7)\
  dprintf7(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7)
#define dlprintf8(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8)\
  dprintf8(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)
#define dlprintf9(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9)\
  dprintf9(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)
#define dlprintf10(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10)\
  dprintf10(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#define dlprintf11(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11)\
  dprintf11(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)
#define dlprintf12(str,arg1,arg2,arg3,arg4,arg5,arg6,arg7,arg8,arg9,arg10,arg11,arg12)\
  dprintf12(str, arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12)

/* Define ENUM_CALL and RELOC_CALL. */
#define ENUM_CALL(procvar, ptr, size, index)\
  (*procvar)(ptr, size, index, pep)  /* (5.1x ENUM_CALL doesn't pass gcst) */
#define ENUM_RETURN_CALL(procvar, ptr, size, index)\
  return ENUM_CALL(procvar, ptr, size, index)
#define RELOC_CALL(procvar, ptr, size)\
  (*procvar)(ptr, size, gcst)

/* Define the new suffix_add0_local structure schema. */
#define gs__st_suffix_add0_local(scope_st, stname, stype, sname, penum, preloc, supstname)\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_suffix_add0_local(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0_local(public_st, stname, stype, sname, penum, preloc, supstname)
#define gs_private_st_suffix_add0_local(stname, stype, sname, penum, preloc, supstname)\
  gs__st_suffix_add0_local(private_st, stname, stype, sname, penum, preloc, supstname)

/* Patch "shared" elements of gs_imager_state. */
#ifndef gs_color_space_DEFINED
#  define gs_color_space_DEFINED
typedef struct gs_color_space_s gs_color_space;
#endif
extern gs_color_space *cs_DeviceGray;
extern gs_color_space *cs_DeviceRGB;
extern gs_color_space *cs_DeviceCMYK;
#define gs_imager_state_shared(pis, elt) (elt)
#define gs_color_space_DeviceGray() (cs_DeviceGray)
#define gs_color_space_DeviceRGB() (cs_DeviceRGB)
#define gs_color_space_DeviceCMYK() (cs_DeviceCMYK)

/* Odds and ends */
#define BEGIN	do {
#define END	} while (0)
#define gx_cpath_list(pcpath) (&(pcpath)->list)
