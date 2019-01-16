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


/* Definitions for DCT filters */
/* Requires stream.h, strimpl.h, jpeg/jpeglib.h */

#ifndef sdct_INCLUDED
#  define sdct_INCLUDED

#include "setjmp_.h"		/* for jmp_buf */
#include "gscms.h"
#include "strimpl.h"
#include "jpeglib_.h"


/* ------ DCT filters ------ */

/*
 * Define the stream state.
 * The jpeg_xxx_data structs are allocated in immovable memory
 * to simplify use of the IJG library.
 */
/* The pointer "blocks" has been replaced with "dummy" as it is no longer needed,
 * but "dummy" is there in its place to maintain consistency in allocations
 * between the decode and encode filters: both are allocated with alloc_struct
 * (which is used to allocate structures with pointers known to the garbage collector),
 * removing "dummy" would mean alloc_struct for decoding filters, and alloc_bytes
 * for encoding filters (with no pointers known to the garbage collector).
 *
 * dummy can be enumerated/reloc'ed by the garbage collection but as
 * dummy == NULL *always*
 * the garbage collector will just skip over it.
 */
#define jpeg_stream_data_common\
                /* We put a copy of the stream template here, because */\
                /* the minimum buffer sizes depend on the image parameters. */\
        stream_template templat;\
        struct jpeg_error_mgr err;\
        gsfix_jmp_buf exit_jmpbuf;\
        gs_memory_t *memory;	/* heap */\
        gs_memory_t *cmem;	/* chunk allocator for library allocations */\
        byte *dummy;   /* see comment above */\
        int Height; /* For JPEG files using a DNL (Define Number of Lines) marker */\
                /* The following are documented in Adobe TN 5116. */\
        int Picky;		/* 0 or 1 */\
        int Relax		/* 0 or 1 */

typedef struct jpeg_stream_data_s {
    jpeg_stream_data_common;
} jpeg_stream_data;

/* Define initialization for the non-library part of the stream state. */
#define jpeg_stream_data_common_init(pdata)\
BEGIN\
  (pdata)->Height = 0;\
  (pdata)->Picky = 0;\
  (pdata)->Relax = 0;\
  (pdata)->dummy = 0;\
END

typedef struct jpeg_compress_data_s {
    jpeg_stream_data_common;
    /* cinfo must immediately follow the common fields */
    struct jpeg_compress_struct cinfo;
    struct jpeg_destination_mgr destination;
    byte finish_compress_buf[100];
    int fcb_size, fcb_pos;
} jpeg_compress_data;

extern_st(st_jpeg_compress_data);
#define public_st_jpeg_compress_data()	/* in sdcte.c */\
  gs_public_st_ptrs1(st_jpeg_compress_data, jpeg_compress_data,\
    "JPEG compress data", jpeg_compress_data_enum_ptrs, jpeg_compress_data_reloc_ptrs, dummy)

#define DCTD_PassThrough(proc)\
  int proc(void *d, byte *Buffer, int Size)

typedef struct jpeg_decompress_data_s {
    jpeg_stream_data_common;
    /* dinfo must immediately follow the common fields, */
    /* so that it has same offset as cinfo. */
    struct jpeg_decompress_struct dinfo;
    struct jpeg_source_mgr source;
    long skip;			/* # of bytes remaining to skip in input */
    bool input_eod;		/* true when no more input data available */
    bool faked_eoi;		/* true when fill_input_buffer inserted EOI */
    byte *scanline_buffer;	/* buffer for oversize scanline, or NULL */
    uint bytes_in_scanline;	/* # of bytes remaining to output from same */
    int PassThrough;                    /* 0 or 1 */
    bool StartedPassThrough;            /* Don't signal multiple starts for the same decode */
    DCTD_PassThrough((*PassThroughfn)); /* We don't want the stream code or
                                         * JPEG code to have to handle devices
                                         * so we use a function at the interpreter level
                                         */
    void *device;                       /* The device we need to send PassThrough data to */
} jpeg_decompress_data;

#define private_st_jpeg_decompress_data()	/* in zfdctd.c */\
  gs_private_st_ptrs2(st_jpeg_decompress_data, jpeg_decompress_data,\
    "JPEG decompress data", jpeg_decompress_data_enum_ptrs,\
    jpeg_decompress_data_reloc_ptrs, dummy, scanline_buffer)

/* The stream state itself.  This is kept in garbage-collectable memory. */
typedef struct stream_DCT_state_s {
    stream_state_common;
    /* The following are set before initialization. */
    /* Note that most JPEG parameters go straight into */
    /* the IJG data structures, not into this struct. */
    gs_const_string Markers;	/* NULL if no Markers parameter */
    float QFactor;
    int ColorTransform;		/* -1 if not specified */
    bool NoMarker;		/* DCTEncode only */
    gs_memory_t *jpeg_memory;	/* heap for library allocations */
    /* This is a pointer to immovable storage. */
    union _jd {
        jpeg_stream_data *common;
        jpeg_compress_data *compress;
        jpeg_decompress_data *decompress;
    } data;
    /* ICC Profile information */
    cmm_profile_t *icc_profile;  /* This pointer is NOT in GC memory */
    byte icc_marker;
    ulong icc_position;
    /* DCTEncode sets this before initialization;
     * DCTDecode cannot set it until the JPEG headers are read.
     */
    uint scan_line_size;
    /* The following are updated dynamically. */
    int phase;
} stream_DCT_state;

/* The state descriptor is public only to allow us to split up */
/* the encoding and decoding filters. */
extern_st(st_DCT_state);
#define public_st_DCT_state()	/* in sdctc.c */\
  gs_public_st_const_strings1_ptrs1_final(st_DCT_state, stream_DCT_state,\
    "DCTEncode/Decode state", dct_enum_ptrs, dct_reloc_ptrs, stream_dct_finalize, Markers, data.common)
/*
 * NOTE: the client *must* invoke the set_defaults procedure in the
 * template before calling the init procedure.
 */
extern const stream_template s_DCTD_template;
extern const stream_template s_DCTE_template;

/* Define an internal procedure for setting stream defaults. */
/* Clients do not call this. */
void s_DCT_set_defaults(stream_state * st);

void
stream_dct_end_passthrough(jpeg_decompress_data *jddp);

#endif /* sdct_INCLUDED */
