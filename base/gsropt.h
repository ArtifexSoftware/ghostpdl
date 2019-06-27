/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* RasterOp / transparency type definitions */

#ifndef gsropt_INCLUDED
#  define gsropt_INCLUDED

#include "stdpre.h"
#ifdef HAVE_SSE2
#include <emmintrin.h>
#endif

/*
 * This file defines the types for some library extensions that are
 * motivated by PCL5 and also made available for PostScript:
 * RasterOp, source and pattern white-pixel transparency, and
 * per-pixel "render algorithm" information.
 */

/* Raster ops are explained in Chapter 5 of "The PCL 5 Color Technical
 * Reference Manual", but essentially, they provide a way of mixing
 * various bitmaps together. The most general form is where a texture
 * bitmap, a source bitmap, and a destination bitmap are added together to
 * give a new destination bitmap value.
 *
 * Each bitmap value is either 0 or 1, so essentially each raster op is
 * a function of the form f(D,S,T). (Confusingly, the HP documentation uses
 * the name 'pattern' in the english descriptive text in an inconsistent way
 * to sometimes mean 'texture' and sometimes mean something that is used to
 * help generate the texture. We will use texture exclusively here.)
 *
 * There are therefore 8 possible values on input to such functions, and 2
 * possible output values for each input. Hence 2^8 = 256 possible raster
 * op functions.
 *
 * For any given function, f(D,S,T), we can form an 8 bit number as follows:
 *  Bit 0 = f(0,0,0)
 *  Bit 1 = f(1,0,0)
 *  Bit 2 = f(0,1,0)
 *  Bit 3 = f(1,1,0)
 *  Bit 4 = f(0,0,1)
 *  Bit 5 = f(1,0,1)
 *  Bit 6 = f(0,1,1)
 *  Bit 7 = f(1,1,1)
 * Running this in reverse for each number from 0 to 255 we have an
 * enumeratation of the possible 3 input raster op functions.
 *
 * We could therefore define F(op,D,S,T) to be a 'master' function that
 * encapsulates all these raster op functions as follows:
 *  F(op,D,S,T) = !!(op & ((D?0xAA:0x55) & (S?0xCC:0x33) & (T?0xF0:0xF)))
 * We don't actually use that technique here (as it only works for 1bpp
 * inputs, and we actually use the rasterops for doing multiple bits at
 * once - it is mentioned for interests sake only.
 */

/*
 * By the magic of Boolean algebra, we can operate on the rop codes using
 * Boolean operators and get the right result.  E.g., the value of
 * (rop3_S & rop3_D) is the rop3 code for S & D.  We just have to remember
 * to mask results with rop2_1 or rop3_1 if necessary.
 */

/* 2-input RasterOp */
typedef enum {
    rop2_0 = 0,
    rop2_S = 0xc,               /* source */
#define rop2_S_shift 2
    rop2_D = 0xa,               /* destination */
#define rop2_D_shift 1
    rop2_1 = 0xf,
#define rop2_operand(shift, d, s)\
  ((shift) == 2 ? (s) : (d))
    rop2_default = rop2_S
} gs_rop2_t;

/*
 * For the 3-input case, we follow H-P's inconsistent terminology:
 * the transparency mode is called pattern transparency, but the third
 * RasterOp operand is called texture, not pattern.
 */

/* We have to define rop3_{T,S,D} as enum values to shut compilers
 * up that do switch checking. We also have to #define them to allow
 * the preprocessor templating to work. */

/* 3-input RasterOp */
typedef enum {
    rop3_0 = 0,
    rop3_T = 0xf0,              /* texture */
#define rop3_T 0xf0
#define rop3_T_shift 4
    rop3_S = 0xcc,              /* source */
#define rop3_S 0xcc
#define rop3_S_shift 2
    rop3_D = 0xaa,              /* destination */
#define rop3_D 0xaa
#define rop3_D_shift 1
    rop3_1 = 0xff,
    rop3_default = rop3_T | rop3_S
} gs_rop3_t;

/* All the transformations on rop3s are designed so that */
/* they can also be used on lops.  The only place this costs anything */
/* is in rop3_invert. */

/*
 * Invert an operand.
 */
#define rop3_invert_(op, mask, shift)\
  ( (((op) & mask) >> shift) | (((op) & (rop3_1 - mask)) << shift) |\
    ((op) & ~rop3_1) )
#define rop3_invert_D(op) rop3_invert_(op, rop3_D, rop3_D_shift)
#define rop3_invert_S(op) rop3_invert_(op, rop3_S, rop3_S_shift)
#define rop3_invert_T(op) rop3_invert_(op, rop3_T, rop3_T_shift)
/*
 * Pin an operand to 0.
 */
#define rop3_know_0_(op, mask, shift)\
  ( (((op) & (rop3_1 - mask)) << shift) | ((op) & ~mask) )
#define rop3_know_D_0(op) rop3_know_0_(op, rop3_D, rop3_D_shift)
#define rop3_know_S_0(op) rop3_know_0_(op, rop3_S, rop3_S_shift)
#define rop3_know_T_0(op) rop3_know_0_(op, rop3_T, rop3_T_shift)
/*
 * Pin an operand to 1.
 */
#define rop3_know_1_(op, mask, shift)\
  ( (((op) & mask) >> shift) | ((op) & ~(rop3_1 - mask)) )
#define rop3_know_D_1(op) rop3_know_1_(op, rop3_D, rop3_D_shift)
#define rop3_know_S_1(op) rop3_know_1_(op, rop3_S, rop3_S_shift)
#define rop3_know_T_1(op) rop3_know_1_(op, rop3_T, rop3_T_shift)
/*
 * Swap S and T.
 */
#define rop3_swap_S_T(op)\
  ( (((op) & rop3_S & ~rop3_T) << (rop3_T_shift - rop3_S_shift)) |\
    (((op) & ~rop3_S & rop3_T) >> (rop3_T_shift - rop3_S_shift)) |\
    ((op) & ~(rop3_S ^ rop3_T)) )

#define lop_swap_S_T(op)\
  ( ((rop3_swap_S_T(op)) & ~(lop_S_transparent | lop_T_transparent)) |\
    (((op) & lop_S_transparent) ? lop_T_transparent : 0) |\
    (((op) & lop_T_transparent) ? lop_S_transparent : 0) )

/*
 * Account for transparency.
 */
#define rop3_use_D_when_0_(op, mask)\
  (((op) & ~(rop3_1 - mask)) | (rop3_D & ~mask))
#define rop3_use_D_when_1_(op, mask)\
  (((op) & ~mask) | (rop3_D & mask))
#define rop3_use_D_when_S_0(op) rop3_use_D_when_0_(op, rop3_S)
#define rop3_use_D_when_S_1(op) rop3_use_D_when_1_(op, rop3_S)
#define rop3_use_D_when_T_0(op) rop3_use_D_when_0_(op, rop3_T)
#define rop3_use_D_when_T_1(op) rop3_use_D_when_1_(op, rop3_T)
/*
 * Invert the result.
 */
#define rop3_not(op) ((op) ^ rop3_1)
/*
 * Test whether an operand is used.
 */
#define rop3_uses_(op, mask, shift)\
  ( ((((op) << shift) ^ (op)) & mask) != 0 )
#define rop3_uses_D(op) rop3_uses_(op, rop3_D, rop3_D_shift)
#define rop3_uses_S(op) rop3_uses_(op, rop3_S, rop3_S_shift)
#define rop3_uses_T(op) rop3_uses_(op, rop3_T, rop3_T_shift)
/*
 * Test whether an operation is idempotent, i.e., whether
 *      f(D, S, T) = f(f(D, S, T), S, T)   (for all D,S,T)
 * f has a range of 0 or 1, so this is equivalent to the condition that:
 *    There exists no s,t, s.t. (f(0,s,t) == 1 && f(1,s,t) == 0)
 * or equivalently:
 *    For all s, t, ~(f(0,s,t) == 1 &&   f(1,s,t) == 0 )
 * or For all s, t, ~(f(0,s,t) == 1 && ~(f(1,s,t) == 1))
 * or For all s, t, ~((f(0,s,t) & ~f(1,s,t)) == 1)
 * or For all s, t,  ((f(0,s,t) & ~f(1,s,t)) == 0)
 *
 * We can code this as a logical operation by observing that:
 *     ((~op)                 & rop3_D) gives us a bitfield of ~f(1,s,t)
 *     (((op)<<rop_3_D_shift) & rop3_D) gives us a bitfield of  f(0,s,t)
 * So
 *     ((~op) & rop3_D) & ((op<<rop3_D_shift) & rop3_D) == 0 iff idempotent
 * === ((~op) & (op<<rop3_D_shift) & rop3_D) == 0 iff idempotent
 */
#define rop3_is_idempotent(op)\
  !( (~op) & ((op) << rop3_D_shift) & rop3_D )

/* Transparency */
#define source_transparent_default false
#define pattern_transparent_default false

/*
 * We define a logical operation as a RasterOp, transparency flags,
 * and render algorithm all packed into a single integer.
 * In principle, we should use a structure, but most C implementations
 * implement structure values very inefficiently.
 *
 * In addition, we define a "pdf14" flag which indicates that PDF
 * transparency is in effect. This doesn't change rendering in any way,
 * but does force the lop to be considered non-idempotent. The lop_pdf14
 * bit is used for fill/stroke ops but even if this is clear, we still
 * may be painting in a transparency buffer in an idempotent mode.
 */
#define lop_rop(lop) ((gs_rop3_t)((lop) & 0xff))        /* must be low-order bits */
#define lop_S_transparent 0x100
#define lop_T_transparent 0x200
#define lop_pdf14 0x400

static inline int
lop_sanitize(int lop)
{
    int olop = lop;

    /* Nobble transparency using ROP conversion */
    /* The T transparency flag only has an effect if
     * we aren't on the leading diagonal. */
    if (olop & lop_T_transparent &&
        ((lop & 0xF0)>>4) != (lop & 0x0F))
        lop = (lop & 0xCF) | 0x20;
    if (olop & lop_S_transparent)
        lop = (lop & 0x33) | 0x88;
    /* Preserve the PDF14 bit */
    lop |= (olop & lop_pdf14);

    return lop;
}

typedef uint gs_logical_operation_t;

#define lop_default\
  (rop3_default |\
   (source_transparent_default ? lop_S_transparent : 0) |\
   (pattern_transparent_default ? lop_T_transparent : 0))

     /* Test whether a logical operation uses S or T. */
#define lop_uses_S(lop)\
  (rop3_uses_S(lop) || ((lop) & (lop_S_transparent | lop_T_transparent)))
#define lop_uses_T(lop)\
  (rop3_uses_T(lop) || ((lop) & lop_T_transparent))
/* Test whether a logical operation just sets D = x if y = 0. */
#define lop_no_T_is_S(lop)\
  (((lop) & (lop_S_transparent | (rop3_1 - rop3_T))) == (rop3_S & ~rop3_T))
#define lop_no_S_is_T(lop)\
  (((lop) & (lop_T_transparent | (rop3_1 - rop3_S))) == (rop3_T & ~rop3_S))
/* Test whether a logical operation is idempotent. */
#define lop_is_idempotent(lop) (rop3_is_idempotent(lop) && !(lop & lop_pdf14))

/*
 * Define the logical operation versions of some RasterOp transformations.
 * Note that these also affect the transparency flags.
 */
#define lop_know_S_0(lop)\
  (rop3_know_S_0(lop) & ~lop_S_transparent)
#define lop_know_T_0(lop)\
  (rop3_know_T_0(lop) & ~lop_T_transparent)
#define lop_know_S_1(lop)\
  (lop & lop_S_transparent ? rop3_D : rop3_know_S_1(lop))
#define lop_know_T_1(lop)\
  (lop & lop_T_transparent ? rop3_D : rop3_know_T_1(lop))

/* Define the interface to the table of 256 RasterOp procedures. */
typedef unsigned long rop_operand;
typedef rop_operand (*rop_proc)(rop_operand D, rop_operand S, rop_operand T);

/* Define the table of operand usage by the 256 RasterOp operations. */
typedef enum {
    rop_usage_none = 0,
    rop_usage_D = 1,
    rop_usage_S = 2,
    rop_usage_DS = 3,
    rop_usage_T = 4,
    rop_usage_DT = 5,
    rop_usage_ST = 6,
    rop_usage_DST = 7
} rop_usage_t;

/* Define the table of RasterOp implementation procedures. */
extern const rop_proc rop_proc_table[256];

/* Define the table of RasterOp operand usage. */
extern const byte /*rop_usage_t*/ rop_usage_table[256];

/* Rather than just doing one raster operation at a time (which costs us
 * a significant amount of time due to function call overheads), we attempt
 * to ameliorate this by doing runs of raster operations at a time.
 */

/* The raster op run operator type */
typedef struct rop_run_op_s rop_run_op;

/* The contents of these structs should really be private, but we define them
 * in the open here to allow us to easily allocate the space on the stack
 * and thus avoid costly malloc/free overheads. */
typedef union rop_source_s {
    struct {
        const byte *ptr;
        int         pos;
    } b;
    rop_operand c;
} rop_source;

/*
scolors and tcolors in the following structure should really be
gx_color_index *, but to do that would require gxcindex.h being
included everywhere this header is, and it's not. Including it
at the top of this header runs other things into problems, so
living with void *'s until we can get the header inclusion
sorted.
*/
struct rop_run_op_s {
    void (*run)(rop_run_op *, byte *dest, int len);
    void (*runswap)(rop_run_op *, byte *dest, int len);
    rop_source s;
    rop_source t;
    int rop;
    byte depth;
    byte flags;
    byte mul;
    byte dpos;
    const void *scolors;
    const void *tcolors;
    void (*release)(rop_run_op *);
    void *opaque;
};

/* Flags for passing into rop_get_run_op */
enum {
    rop_s_constant = 1,
    rop_t_constant = 2,
    rop_s_1bit     = 4,
    rop_t_1bit     = 8
};

/* To use a rop_run_op, allocate it on the stack, then (if T or S are constant)
 * call one of:
 */
void rop_set_s_constant(rop_run_op *op, int s);
void rop_set_t_constant(rop_run_op *op, int t);

/* Then call rop_get_run_op with a pointer to it to fill it in with details
 * of an implementer. If you're lucky (doing a popular rop) you'll get an
 * optimised implementation. If you're not, you'll get a general purpose
 * slow rop. You will always get an implementation of some kind though.
 *
 * You should logical or together the flags - this tells the routine whether
 * s and t are constant, or will be varying across the run.
 *
 * If this function returns non zero, the ROP has been optimised out.
 */
int rop_get_run_op(rop_run_op *op, int rop, int depth, int flags);

/* Next, (for non-constant S or T) you should set the values of S and T.
 * (Constant values were handled earlier, remember?) Each of these can
 * either be a constant value, or a pointer to a run of bytes. It is the
 * callers responsibility to set these in the correct way (corresponding
 * to the flags passed into the call to rop_get_run_op.
 *
 * For cases where depth < 8, and a bitmap is used, we have to specify the
 * start bit position within the byte. (All data in rop bitmaps is considered
 * to be bigendian). */
void rop_set_s_constant(rop_run_op *op, int s);
void rop_set_s_bitmap(rop_run_op *op, const byte *s);
void rop_set_s_bitmap_subbyte(rop_run_op *op, const byte *s, int startbitpos);
void rop_set_s_colors(rop_run_op *op, const byte *scolors);
void rop_set_t_bitmap(rop_run_op *op, const byte *t);
void rop_set_t_bitmap_subbyte(rop_run_op *op, const byte *s, int startbitpos);
void rop_set_t_colors(rop_run_op *op, const byte *scolors);

/* At last we call the rop_run function itself. (This can happen multiple
 * times, perhaps once per scanline, with any required calls to the
 * rop_set_{s,t} functions.) The length field is given in terms of 'number
 * of depth bits'.
 *
 * This version is for the depth >= 8 case. */
void rop_run(rop_run_op *op, byte *d, int len);

/* This version tells us how many bits to skip over in the first byte, and
 * is therefore only useful in the depth < 8 case. */
void rop_run_subbyte(rop_run_op *op, byte *d, int startbitpos, int len);

/* And finally we release our rop (in case any allocation was done). */
void rop_release_run_op(rop_run_op *op);

/* We offer some of these functions implemented as macros for speed */
#define rop_set_s_constant(OP,S)          ((OP)->s.c = (S))
#define rop_set_s_bitmap(OP,S)            ((OP)->s.b.ptr = (S))
#define rop_set_s_bitmap_subbyte(OP,S,B) (((OP)->s.b.ptr = (S)),((OP)->s.b.pos = (B)))
#define rop_set_s_colors(OP,S)            ((OP)->scolors = (const byte *)(S))
#define rop_set_t_constant(OP,T)          ((OP)->t.c = (T))
#define rop_set_t_bitmap(OP,T)            ((OP)->t.b.ptr = (T))
#define rop_set_t_bitmap_subbyte(OP,T,B) (((OP)->t.b.ptr = (T)),((OP)->t.b.pos = (B)))
#define rop_set_t_colors(OP,T)            ((OP)->tcolors = (const byte *)(T))
#define rop_run(OP,D,LEN)                (((OP)->run)(OP,D,LEN))
#define rop_run_subbyte(OP,D,S,L)        (((OP)->dpos=(byte)S),((OP)->run)(OP,D,L))
#define rop_release_run_op(OP)           do { rop_run_op *OP2 = (OP); \
                                              if (OP2->release) OP2->release(OP2); \
                                         } while (0==1)

#ifdef _MSC_VER /* Are we using MSVC? */
#  if defined(_M_IX86) || defined(_M_AMD64) /* Are we on an x86? */
#    if MSC_VER >= 1400  /* old versions have _byteswap_ulong() in stdlib.h */
#      include "intrin.h"
#    endif
#  define ENDIAN_SWAP_INT _byteswap_ulong
#endif

#elif defined(HAVE_BSWAP32)

#define ENDIAN_SWAP_INT __builtin_bswap32

#elif defined(HAVE_BYTESWAP_H)

#include <byteswap.h>
#define ENDIAN_SWAP_INT bswap_32

#endif

#endif /* gsropt_INCLUDED */
