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

/* gxdevcli.h */
/* 5.1x patches for masked image code */

#include "gxdevice.h"

/* ---------------- Miscellaneous ---------------- */

#define gs__st_ptrs6(scope_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  private ENUM_PTRS_BEGIN(penum) return 0;\
    ENUM_PTR(0,stype,e1); ENUM_PTR(1,stype,e2); ENUM_PTR(2,stype,e3);\
    ENUM_PTR(3,stype,e4); ENUM_PTR(4,stype,e5); ENUM_PTR(5,stype,e6);\
  ENUM_PTRS_END\
  private RELOC_PTRS_BEGIN(preloc) ;\
    RELOC_PTR(stype,e1); RELOC_PTR(stype,e2); RELOC_PTR(stype,e3);\
    RELOC_PTR(stype,e4); RELOC_PTR(stype,e5); RELOC_PTR(stype,e6);\
  RELOC_PTRS_END\
  gs__st_composite_only(scope_st, stname, stype, sname, penum, preloc)
#define gs_public_st_ptrs6(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  gs__st_ptrs6(public_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)
#define gs_private_st_ptrs6(stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)\
  gs__st_ptrs6(private_st, stname, stype, sname, penum, preloc, e1, e2, e3, e4, e5, e6)

void gx_device_init(P4(gx_device *dev, const gx_device *proto,
		       gs_memory_t *mem, bool internal));

/* ---------------- Pixel processing macros ---------------- */

/*
 * These macros support code that processes data pixel-by-pixel (or, to be
 * more accurate, packed arrays of values -- they may be complete pixels
 * or individual components of pixels).
 *
 * Supported #s of bits per value (bpv) are 1, 2, 4, 8, 12[, 16[, 24, 32]].
 * The suffix 12, 16, or 32 on a macro name indicates the maximum value
 * of bpv that the macro is prepared to handle.
 *
 * The setup macros number bits within a byte in big-endian order, i.e.,
 * 0x80 is bit 0, 0x01 is bit 7.  However, sbit/dbit may use a different
 * representation for better performance.  ****** NYI ******
 */

#define sample_end_\
  default: return_error(gs_error_rangecheck);\
  } } while (0)

/* Declare variables for loading. */
#define sample_load_declare(sptr, sbit)\
  const byte *sptr;\
  int sbit
#define sample_load_declare_setup(sptr, sbit, ptr, bitno, sbpv)\
  const byte *sptr = (ptr);\
  int sample_load_setup(sbit, bitno, sbpv)

/* Set up to load starting at a given bit number. */
#define sample_load_setup(sbit, bitno, sbpv)\
  sbit = (bitno)

/* Load a value from memory, without incrementing. */
#define sample_load12_(value, sptr, sbit, sbpv)\
  do { switch ( (sbpv) >> 2 ) {\
  case 0: value = (*(sptr) >> (8 - (sbit) - (sbpv))) & ((sbpv) - 1); break;\
  case 1: value = (*(sptr) >> (4 - (sbit))) & 0xf; break;\
  case 2: value = *(sptr); break;\
  case 3:\
    value = ((sbit) ? ((*(sptr) & 0xf) << 8) | (sptr)[1] :\
	      (*(sptr) << 4) | ((sptr)[1] >> 4));\
    break;
#define sample_load12(value, sptr, sbit, sbpv)\
  sample_load12_(value, sptr, sbit, sbpv)\
  sample_end_
#define sample_load_next12(value, sptr, sbit, sbpv)\
  sample_load12(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)
#define sample_load16_(value, sptr, sbit, sbpv)\
  sample_load12_(value, sptr, sbit, sbpv)\
  case 4: value = (*(sptr) << 8) | (sptr)[1]; break;
#define sample_load16(value, sptr, sbit, sbpv)\
  sample_load16_(value, sptr, sbit, sbpv)\
  sample_end_
#define sample_load_next16(value, sptr, sbit, sbpv)\
  sample_load16(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)
#define sample_load32(value, sptr, sbit, sbpv)\
  sample_load16_(value, sptr, sbit, sbpv)\
  case 6: value = (*(sptr) << 16) | ((sptr)[1] << 8) | (sptr)[2]; break;\
  case 8:\
    value = (*(sptr) << 24) | ((sptr)[1] << 16) | ((sptr)[2] << 8) | sptr[3];\
    break;\
  sample_end_
#define sample_load_next32(value, sptr, sbit, sbpv)\
  sample_load32(value, sptr, sbit, sbpv);\
  sample_next(sptr, sbit, sbpv)

/* Declare variables for storing. */
#define sample_store_declare(dptr, dbit, dbbyte)\
  byte *dptr;\
  int dbit;\
  byte dbbyte /* maybe should be uint? */
#define sample_store_declare_setup(dptr, dbit, dbbyte, ptr, bitno, dbpv)\
  byte *dptr = (ptr);\
  int sample_store_setup(dbit, bitno, dbpv);\
  byte /* maybe should be uint? */\
    sample_store_preload(dbbyte, dptr, dbit, dbpv)

/* Set up to store starting at a given bit number. */
#define sample_store_setup(dbit, bitno, dbpv)\
  dbit = (bitno)

/* Prepare for storing by preloading any partial byte. */
#define sample_store_preload(dbbyte, dptr, dbit, dbpv)\
  dbbyte = ((dbit) ? (byte)(*(dptr) & (0xff00 >> (dbit))) : 0)

/* Store a value and increment the pointer. */
#define sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  do { switch ( (dbpv) >> 2 ) {\
  case 0:\
    if ( (dbit += (dbpv)) == 8 )\
       *(dptr)++ = dbbyte | (value), dbbyte = 0, dbit = 0;\
    else dbbyte |= (value) << (8 - dbit);\
    break;\
  case 1:\
    if ( dbit ^= 4 ) dbbyte = (byte)((value) << 4);\
    else *(dptr)++ = dbbyte | (value);\
    break;\
  /* case 2 is deliberately omitted */\
  case 3:\
    if ( dbit ^= 4 ) *(dptr)++ = (value) >> 4, dbbyte = (byte)((value) << 4);\
    else\
      *(dptr) = dbbyte | ((value) >> 8), (dptr)[1] = (byte)(value), dptr += 2;\
    break;
#define sample_store_next12(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_
#define sample_store_next16(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 4: *(dptr)++ = (byte)((value) >> 8);\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_
#define sample_store_next32(value, dptr, dbit, dbpv, dbbyte)\
  sample_store_next12_(value, dptr, dbit, dbpv, dbbyte)\
  case 8: *(dptr)++ = (byte)((value) >> 24);\
  case 6: *(dptr)++ = (byte)((value) >> 16);\
  case 4: *(dptr)++ = (byte)((value) >> 8);\
  case 2: *(dptr)++ = (byte)(value); break;\
  sample_end_

/* Skip over storing one sample.  This may or may not store into the */
/* skipped region. */
#define sample_store_skip_next(dptr, dbit, dbpv, dbbyte)\
  if ( (dbpv) < 8 ) {\
    sample_store_flush(dptr, dbit, dbpv, dbbyte);\
    sample_next(dptr, dbit, dbpv);\
  } else dptr += ((dbpv) >> 3)

/* Finish storing by flushing any partial byte. */
#define sample_store_flush(dptr, dbit, dbpv, dbbyte)\
  if ( (dbit) != 0 )\
    *(dptr) = dbbyte | (*(dptr) & (0xff >> (dbit)));

/* Increment a pointer to the next sample. */
#define sample_next(ptr, bit, bpv)\
  do { bit += (bpv); ptr += bit >> 3; bit &= 7; } while (0)

/* ---------------- Image-related definitions ---------------- */

#ifndef gx_image_enum_common_t_DEFINED
#  define gx_image_enum_common_t_DEFINED
typedef struct gx_image_enum_common_s gx_image_enum_common_t;
#endif


#undef dev_t_proc_begin_typed_image
#define dev_t_proc_begin_typed_image(proc, dev_t)\
  int proc(P9(dev_t *dev,\
    const gs_imager_state *pis, const gs_matrix *pmat,\
    const gs_image_common_t *pim, const gs_int_rect *prect,\
    const gx_drawing_color *pdcolor, const gx_clip_path *pcpath,\
    gs_memory_t *memory, gx_image_enum_common_t **pinfo))
#undef dev_proc_begin_typed_image
#define dev_proc_begin_typed_image(proc)\
  dev_t_proc_begin_typed_image(proc, gx_device)

     /* Patch the definition of image_enum_proc_end_image in gxiparam.h */
#undef dev_proc_end_image
#define dev_proc_end_image(proc)\
  int proc(P3(gx_device *dev, gx_image_enum_common_t *info, bool draw_last))

typedef struct gx_image_plane_s {
  const byte *data;
  int data_x;
  uint raster;
} gx_image_plane_t;
#define image_enum_proc_plane_data(proc)\
  int proc(P4(gx_device *dev,\
    gx_image_enum_common_t *info, const gx_image_plane_t *planes,\
    int height))
dev_proc_begin_image(gx_device_begin_image);
dev_proc_begin_typed_image(gx_device_begin_typed_image);
int gx_device_image_data(P6(gx_device *dev,
  gx_image_enum_common_t *info, const byte **planes, int data_x,
  uint raster, int height));
image_enum_proc_plane_data(gx_device_image_plane_data);
dev_proc_end_image(gx_device_end_image);
