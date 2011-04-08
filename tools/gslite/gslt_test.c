/* Copyright (C) 2006 artofcode LLC.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* $Id: gslt_test.c 2991 2007-12-18 23:05:58Z giles $ */
/* Test program for Ghostscript library */

/* Capture stdin/out/err before gsio.h redefines them. */
#include "stdio_.h"
#include "math_.h"
#include "string_.h"
#include "gx.h"
#include "gp.h"
#include "gslt_alloc.h"
#include "gscdefs.h"
#include "gserrors.h"
#include "gslib.h"
#include "gsmatrix.h"
#include "gsstate.h"
#include "gscspace.h"
#include "gscolor2.h"
#include "gscoord.h"
#include "gscie.h"
#include "gscrd.h"
#include "gsiparm3.h"
#include "gsiparm4.h"
#include "gsparam.h"
#include "gspaint.h"
#include "gspath.h"
#include "gspath2.h"
#include "gsrop.h"
#include "gsstruct.h"
#include "gsutil.h"
#include "gxalloc.h"
#include "gxdcolor.h"		/* for gx_device_white/black */
#include "gxdevice.h"
#include "gxht.h"		/* for gs_halftone */
// #include "gdevcmap.h"
#include "gshtx.h"
#include "gxpath.h"

/* Test programs */
static int test1(gs_state *, gs_memory_t *);	/* kaleidoscope */
static int test2(gs_state *, gs_memory_t *);	/* pattern fill */
static int test3(gs_state *, gs_memory_t *);	/* RasterOp */
static int test4(gs_state *, gs_memory_t *);	/* set resolution */
static int test5(gs_state *, gs_memory_t *);	/* images */
static int test6(gs_state *, gs_memory_t *);	/* CIE API, snapping */
#ifdef HALFTONE_FIX
static int test7(gs_state *, gs_memory_t *);	/* non-monot HT */
#endif
static int test8(gs_state *, gs_memory_t *);	/* transp patterns */
static int test9(gs_state *, gs_memory_t *);	/* type 42 font */

static int (*tests[]) (gs_state *, gs_memory_t *) =
{
    test1, test2, test3, test4, test5,
    test6,
#ifdef HALFTONE_FIX
    test7, 
#else
    0,
#endif
    test8, test9,
};

/* Include the extern for the device list. */
extern_gs_lib_device_list();

/* Forward references */
static float odsf(floatp, floatp);


/* return index in gs device list -1 if not found */
static inline int
get_device_index(const gs_memory_t *mem, const char *value)
{
    const gx_device *const *dev_list;
    int num_devs = gs_lib_device_list(&dev_list, NULL);
    int di;

    for ( di = 0; di < num_devs; ++di )
	if ( !strcmp(gs_devicename(dev_list[di]), value) )
	    break;
    if ( di == num_devs ) {
	lprintf1("Unknown device name %s.\n", value);
	return -1;
    }
    return di;
}

int
main(int argc, const char *argv[])
{
    char achar;
    gs_memory_t *mem;
    /* memory braindamage */
#define gs_stdin mem->gs_lib_ctx->fstdin
    gs_state *pgs;
    const gx_device *const *list;
    gx_device *dev;
    int code = 0;

    /* a simple allocator to avoid the complications of the full
       featured memory allocator */
    mem = gslt_alloc_init();
    gp_init();
    gs_lib_init1(mem);
    if (argc < 3 || (achar = argv[2][0]) < '1' ||
	achar > '0' + countof(tests)
	) {
	lprintf1("Usage: gslt [device] 1..%c\n", '0' + (char)countof(tests));
	exit(1);
    }
    memset(gs_debug, 0, 128);
    gs_debug['@'] = 1;
    gs_debug['?'] = 1;
/*gs_debug['B'] = 1; *//****** PATCH ******/
/*gs_debug['L'] = 1; *//****** PATCH ******/
    gs_log_errors = 0;
    /*
     * gs_iodev_init must be called after the rest of the inits, for
     * obscure reasons that really should be documented!
     */
    gs_iodev_init(mem);
    {
        int devindex = get_device_index(mem, argv[1]);
        if (devindex < 0) {
            lprintf1("device %s not found\n", argv[1]);
	    exit(1);
        }
        gs_lib_device_list(&list, NULL);
        gs_copydevice(&dev, list[devindex], mem);
    }
    // stefan foo: pulled for linking to gs 8.13
    //check_device_separable(dev);
    gx_device_fill_in_procs(dev);
    /* Print out the device name just to test the gsparam.c API. */
    {
	gs_c_param_list list;
	gs_param_string nstr;

	gs_c_param_list_write(&list, mem);
	code = gs_getdeviceparams(dev, (gs_param_list *) & list);
	if (code < 0) {
	    lprintf1("getdeviceparams failed! code = %d\n", code);
	    exit(1);
	}
	gs_c_param_list_read(&list);
	code = param_read_string((gs_param_list *) & list, "Name", &nstr);
	if (code < 0) {
	    lprintf1("reading Name failed! code = %d\n", code);
	    exit(1);
	}
	dputs("Device name = ");
	debug_print_string(nstr.data, nstr.size);
	dputs("\n");
	gs_c_param_list_release(&list);
    }
    /*
     * If this is a device that takes an OutputFile, set the OutputFile
     * to "-" in the copy.
     */
    {
	gs_c_param_list list;
	gs_param_string nstr;

	gs_c_param_list_write(&list, mem);
	param_string_from_string(nstr, "-");
	code = param_write_string((gs_param_list *)&list, "OutputFile", &nstr);
	if (code < 0) {
	    lprintf1("writing OutputFile failed! code = %d\n", code);
	    exit(1);
	}
	gs_c_param_list_read(&list);
	code = gs_putdeviceparams(dev, (gs_param_list *)&list);
	gs_c_param_list_release(&list);
	if (code < 0 && code != gs_error_undefined) {
	    lprintf1("putdeviceparams failed! code = %d\n", code);
	    exit(1);
	}
    }
    pgs = gs_state_alloc(mem);
    gs_setdevice_no_erase(pgs, dev);	/* can't erase yet */
    {
	gs_point dpi;
	gs_screen_halftone ht;

	gs_dtransform(pgs, 72.0, 72.0, &dpi);
	ht.frequency = min(fabs(dpi.x), fabs(dpi.y)) / 16.001;
	ht.angle = 0;
	ht.spot_function = odsf;
	gs_setscreen(pgs, &ht);
    }
    /* gsave and grestore (among other places) assume that */
    /* there are at least 2 gstates on the graphics stack. */
    /* Ensure that now. */
    gs_gsave(pgs);
    gs_erasepage(pgs);

    if (tests[achar - '1']) {
	dprintf1("gslt test case = %d\n", achar - '1');
        code = (*tests[achar - '1']) (pgs, mem);
        gs_output_page(pgs, 1, 1);
        if (code)
            dprintf1("**** Test returned code = %d.\n", code);
        dputs("Done.  Press <enter> to exit.\n");
        fgetc(gs_stdin);
    } else {
        dputs("test not defined\n");
    }
    gs_lib_finit(0, 0, mem);
    return code;
     
#undef mem
}
/* Ordered dither spot function */
static float
odsf(floatp x, floatp y)
{
    static const byte dither[256] =
    {
	0x0E, 0x8E, 0x2E, 0xAE, 0x06, 0x86, 0x26, 0xA6, 0x0C, 0x8C, 0x2C, 0xAC, 0x04, 0x84, 0x24, 0xA4,
	0xCE, 0x4E, 0xEE, 0x6E, 0xC6, 0x46, 0xE6, 0x66, 0xCC, 0x4C, 0xEC, 0x6C, 0xC4, 0x44, 0xE4, 0x64,
	0x3E, 0xBE, 0x1E, 0x9E, 0x36, 0xB6, 0x16, 0x96, 0x3C, 0xBC, 0x1C, 0x9C, 0x34, 0xB4, 0x14, 0x94,
	0xFE, 0x7E, 0xDE, 0x5E, 0xF6, 0x76, 0xD6, 0x56, 0xFC, 0x7C, 0xDC, 0x5C, 0xF4, 0x74, 0xD4, 0x54,
	0x01, 0x81, 0x21, 0xA1, 0x09, 0x89, 0x29, 0xA9, 0x03, 0x83, 0x23, 0xA3, 0x0B, 0x8B, 0x2B, 0xAB,
	0xC1, 0x41, 0xE1, 0x61, 0xC9, 0x49, 0xE9, 0x69, 0xC3, 0x43, 0xE3, 0x63, 0xCB, 0x4B, 0xEB, 0x6B,
	0x31, 0xB1, 0x11, 0x91, 0x39, 0xB9, 0x19, 0x99, 0x33, 0xB3, 0x13, 0x93, 0x3B, 0xBB, 0x1B, 0x9B,
	0xF1, 0x71, 0xD1, 0x51, 0xF9, 0x79, 0xD9, 0x59, 0xF3, 0x73, 0xD3, 0x53, 0xFB, 0x7B, 0xDB, 0x5B,
	0x0D, 0x8D, 0x2D, 0xAD, 0x05, 0x85, 0x25, 0xA5, 0x0F, 0x8F, 0x2F, 0xAF, 0x07, 0x87, 0x27, 0xA7,
	0xCD, 0x4D, 0xED, 0x6D, 0xC5, 0x45, 0xE5, 0x65, 0xCF, 0x4F, 0xEF, 0x6F, 0xC7, 0x47, 0xE7, 0x67,
	0x3D, 0xBD, 0x1D, 0x9D, 0x35, 0xB5, 0x15, 0x95, 0x3F, 0xBF, 0x1F, 0x9F, 0x37, 0xB7, 0x17, 0x97,
	0xFD, 0x7D, 0xDD, 0x5D, 0xF5, 0x75, 0xD5, 0x55, 0xFF, 0x7F, 0xDF, 0x5F, 0xF7, 0x77, 0xD7, 0x57,
	0x02, 0x82, 0x22, 0xA2, 0x0A, 0x8A, 0x2A, 0xAA, 0x00, 0x80, 0x20, 0xA0, 0x08, 0x88, 0x28, 0xA8,
	0xC2, 0x42, 0xE2, 0x62, 0xCA, 0x4A, 0xEA, 0x6A, 0xC0, 0x40, 0xE0, 0x60, 0xC8, 0x48, 0xE8, 0x68,
	0x32, 0xB2, 0x12, 0x92, 0x3A, 0xBA, 0x1A, 0x9A, 0x30, 0xB0, 0x10, 0x90, 0x38, 0xB8, 0x18, 0x98,
	0xF2, 0x72, 0xD2, 0x52, 0xFA, 0x7A, 0xDA, 0x5A, 0xF0, 0x70, 0xD0, 0x50, 0xF8, 0x78, 0xD8, 0x58
    };
    int i = (int)((x + 1) * 7.9999);
    int j = (int)((y + 1) * 7.9999);

    return dither[16 * i + j] / 256.0;
}

/* Fill a rectangle. */
static int
fill_rect1(gs_state * pgs, floatp x, floatp y, floatp w, floatp h)
{
    gs_rect r;

    r.q.x = (r.p.x = x) + w;
    r.q.y = (r.p.y = y) + h;
    return gs_rectfill(pgs, &r, 1);
}

/* Stubs for GC */
const gs_ptr_procs_t ptr_struct_procs =
{NULL, NULL, NULL};
const gs_ptr_procs_t ptr_string_procs =
{NULL, NULL, NULL};
const gs_ptr_procs_t ptr_const_string_procs =
{NULL, NULL, NULL};
void *				/* obj_header_t * */
gs_reloc_struct_ptr(const void * /* obj_header_t * */ obj, gc_state_t * gcst)
{
    return (void *)obj;
}
void
gs_reloc_string(gs_string * sptr, gc_state_t * gcst)
{
}
void
gs_reloc_const_string(gs_const_string * sptr, gc_state_t * gcst)
{
}

/* Other stubs */
void
gs_to_exit(const gs_memory_t *mem, int exit_status)
{
    gs_lib_finit(exit_status, 0, mem);
}

void
gs_abort(const gs_memory_t *mem)
{
    gs_to_exit(mem, 1); /* cleanup */
    gp_do_exit(1); /* system independent exit() */	
}


/* Return the number with the magnitude of x and the sign of y. */
/* This is a BSD addition to libm; not all compilers have it. */
static double
gs_copysign(floatp x, floatp y)
{
   return ( y >= 0  ? fabs(x) : -fabs(x) );
}


/* ---------------- Test program 1 ---------------- */
/* Draw a colored kaleidoscope. */

/* Random number generator */
static long rand_state = 1;
static long
rand(void)
{
#define A 16807
#define M 0x7fffffff
#define Q 127773		/* M / A */
#define R 2836			/* M % A */
    rand_state = A * (rand_state % Q) - R * (rand_state / Q);
    /* Note that rand_state cannot be 0 here. */
    if (rand_state <= 0)
	rand_state += M;
#undef A
#undef M
#undef Q
#undef R
    return rand_state;
}
static int
test1(gs_state * pgs, gs_memory_t * mem)
{
    int n;

    gs_scale(pgs, 72.0, 72.0);
    gs_translate(pgs, 4.25, 5.5);
    gs_scale(pgs, 4.0, 4.0);
    gs_newpath(pgs);
    for (n = 200; --n >= 0;) {
	int j;

#define rf() (rand() / (1.0 * 0x10000 * 0x8000))
	double r = rf(), g = rf(), b = rf();
	double x0 = rf(), y0 = rf(), x1 = rf(), y1 = rf(), x2 = rf(), y2 = rf();

	gs_setrgbcolor(pgs, r, g, b);
	for (j = 0; j < 6; j++) {
	    gs_gsave(pgs);
	    gs_rotate(pgs, 60.0 * j);
	    gs_moveto(pgs, x0, y0);
	    gs_lineto(pgs, x1, y1);
	    gs_lineto(pgs, x2, y2);
	    gs_fill(pgs);
	    gs_grestore(pgs);
	}
    }
#undef mem
    return 0;
}

/* ---------------- Test program 2 ---------------- */
/* Fill an area with a pattern. */

static int
test2(gs_state * pgs, gs_memory_t * mem)
{
    gs_client_color cc;
    gx_tile_bitmap tile;
    /*const */ byte tpdata[] =
    {
    /* Define a pattern that looks like this:
       ..xxxx
       .....x
       .....x
       ..xxxx
       .x....
       x.....
     */
	0x3c, 0, 0, 0, 0x04, 0, 0, 0, 0x04, 0, 0, 0, 0x3c, 0, 0, 0,
	0x40, 0, 0, 0, 0x80, 0, 0, 0
    };

    gs_newpath(pgs);
    gs_moveto(pgs, 100.0, 300.0);
    gs_lineto(pgs, 500.0, 500.0);
    gs_lineto(pgs, 200.0, 100.0);
    gs_lineto(pgs, 300.0, 500.0);
    gs_lineto(pgs, 500.0, 200.0);
    gs_closepath(pgs);
    gs_setrgbcolor(pgs, 0.0, 0.0, 0.0);
    gs_gsave(pgs);
    gs_fill(pgs);
    gs_grestore(pgs);
    tile.data = tpdata;
    tile.raster = 4;
    tile.size.x = tile.rep_width = 6;
    tile.size.y = tile.rep_height = 6;
    tile.id = gx_no_bitmap_id;
    gs_makebitmappattern(&cc, &tile, true, pgs, NULL);
    /* Note: color space is DeviceRGB */
    cc.paint.values[0] = 0.0;
    cc.paint.values[1] = 1.0;
    cc.paint.values[2] = 1.0;
    gs_setpattern(pgs, &cc);
    gs_eofill(pgs);
    gs_makebitmappattern(&cc, &tile, false, pgs, NULL);
    gs_setcolor(pgs, &cc);
    gs_moveto(pgs, 50.0, 50.0);
    gs_lineto(pgs, 300.0, 50.0);
    gs_lineto(pgs, 50.0, 300.0);
    gs_closepath(pgs);
    gs_setrgbcolor(pgs, 1.0, 0.0, 0.0);
    gs_gsave(pgs);
    gs_fill(pgs);
    gs_grestore(pgs);
    gs_setpattern(pgs, &cc);
    gs_eofill(pgs);
    return 0;
}

/* ---------------- Test program 3 ---------------- */
/* Exercise RasterOp a little. */
/* Currently, this only works with monobit devices. */

static int
test3(gs_state * pgs, gs_memory_t * mem)
{
    gx_device *dev = gs_currentdevice(pgs);
    gx_color_index black = gx_device_black(dev);
    gx_color_index white = gx_device_white(dev);
    gx_color_index black2[2];
    gx_color_index black_white[2];
    gx_color_index white_black[2];
    long pattern[max(align_bitmap_mod / sizeof(long), 1) * 4];

#define pbytes ((byte *)pattern)
    gx_tile_bitmap tile;

    black2[0] = black2[1] = black;
    black_white[0] = white_black[1] = black;
    black_white[1] = white_black[0] = white;
    pbytes[0] = 0xf0;
    pbytes[align_bitmap_mod] = 0x90;
    pbytes[align_bitmap_mod * 2] = 0x90;
    pbytes[align_bitmap_mod * 3] = 0xf0;
    tile.data = pbytes;
    tile.raster = align_bitmap_mod;
    tile.size.x = tile.size.y = 4;
    tile.id = gs_next_ids(mem, 1);
    tile.rep_width = tile.rep_height = 4;
    (*dev_proc(dev, copy_rop))
	(dev, NULL, 0, 0, gx_no_bitmap_id, black2,
	 &tile, white_black, 100, 100, 150, 150, 0, 0, rop3_T);
    (*dev_proc(dev, copy_rop))
	(dev, NULL, 0, 0, gx_no_bitmap_id, black2,
	 NULL, NULL, 120, 120, 110, 110, 0, 0, ~rop3_S & rop3_1);
    (*dev_proc(dev, copy_rop))
	(dev, NULL, 0, 0, gx_no_bitmap_id, black2,
	 &tile, white_black, 110, 110, 130, 130, 0, 0, rop3_T ^ rop3_D);
#undef pbytes
    return 0;
}

/* ---------------- Test program 4 ---------------- */
/* Set the resolution dynamically. */

static int
test4(gs_state * pgs, gs_memory_t * mem)
{
    gs_c_param_list list;
    float resv[2];
    gs_param_float_array ares;
    int code;
    gx_device *dev = gs_currentdevice(pgs);

    gs_c_param_list_write(&list, mem);
    resv[0] = resv[1] = 100;
    ares.data = resv;
    ares.size = 2;
    ares.persistent = true;
    code = param_write_float_array((gs_param_list *) & list,
				   "HWResolution", &ares);
    if (code < 0) {
	lprintf1("Writing HWResolution failed: %d\n", code);
	exit(1);
    }
    gs_c_param_list_read(&list);
    code = gs_putdeviceparams(dev, (gs_param_list *) & list);
    gs_c_param_list_release(&list);
    if (code < 0) {
	lprintf1("Setting HWResolution failed: %d\n", code);
	exit(1);
    }
    gs_initmatrix(pgs);
    gs_initclip(pgs);
    if (code == 1) {
	code = (*dev_proc(dev, open_device)) (dev);
	if (code < 0) {
	    lprintf1("Reopening device failed: %d\n", code);
	    exit(1);
	}
    }
    gs_moveto(pgs, 0.0, 72.0);
    gs_rlineto(pgs, 72.0, 0.0);
    gs_rlineto(pgs, 0.0, 72.0);
    gs_closepath(pgs);
    gs_stroke(pgs);
    return 0;
}

/* ---------------- Test program 5 ---------------- */
/* Test masked (and non-masked) images. */

static int
test5(gs_state * pgs, gs_memory_t * mem)
{
    gx_device *dev = gs_currentdevice(pgs);
    gx_image_enum_common_t *info;
    gx_image_plane_t planes[5];
    gx_drawing_color dcolor;
    int code;
    static const byte data3[] =
    {
	0x00, 0x44, 0x88, 0xcc,
	0x44, 0x88, 0xcc, 0x00,
	0x88, 0xcc, 0x00, 0x44,
	0xcc, 0x00, 0x44, 0x88
    };
    gs_color_space gray_cs;

    gs_cspace_init_DeviceGray(mem, &gray_cs);

    /*
     * Neither ImageType 3 nor 4 needs a current color,
     * but some intermediate code assumes it's valid.
     */
    set_nonclient_dev_color(&dcolor, 0);

    /* Scale everything up, and fill the background. */
    {
	gs_matrix mat;

	gs_currentmatrix(pgs, &mat);
	mat.xx = gs_copysign(98.6, mat.xx);
	mat.yy = gs_copysign(98.6, mat.yy);
	mat.tx = floor(mat.tx) + 0.499;
	mat.ty = floor(mat.ty) + 0.499;
	gs_setmatrix(pgs, &mat);
    }
    gs_setrgbcolor(pgs, 1.0, 0.9, 0.9);
    fill_rect1(pgs, 0.25, 0.25, 4.0, 6.0);
    gs_setrgbcolor(pgs, 0.5, 1.0, 0.5);

#if 0
    /* Make things a little more interesting.... */
    gs_translate(pgs, 1.0, 1.0);
    gs_rotate(pgs, 10.0);
    gs_scale(pgs, 1.3, 0.9);
#endif

#define do_image(image, idata)\
  BEGIN\
  code = gx_device_begin_typed_image(dev, (gs_imager_state *)pgs, NULL,\
     (gs_image_common_t *)&image, NULL, &dcolor, NULL, mem, &info);\
  /****** TEST code >= 0 ******/\
  planes[0].data = idata;\
  planes[0].data_x = 0;\
  planes[0].raster = (image.Height * image.BitsPerComponent + 7) >> 3;\
  code = gx_image_plane_data(info, planes, image.Height);\
  /****** TEST code == 1 ******/\
  code = gx_image_end(info, true);\
  /****** TEST code >= 0 ******/\
  END

#define W 4
#define H 4

    /* Test an unmasked image. */
    gs_gsave(pgs);
    {
	gs_image1_t image1;
	void *info1;
        gs_color_space cs;

        gs_cspace_init_DeviceGray(mem, &cs);
	gs_image_t_init(&image1, &cs);
	/* image */
	image1.ImageMatrix.xx = W;
	image1.ImageMatrix.yy = -H;
	image1.ImageMatrix.ty = H;
	/* data_image */
	image1.Width = W;
	image1.Height = H;
	image1.BitsPerComponent = 8;

	gs_translate(pgs, 0.5, 4.0);
        code = (*dev_proc(dev, begin_image))
          (dev, (const gs_imager_state *)pgs, &image1,
           gs_image_format_chunky, (const gs_int_rect *)0,
           &dcolor, NULL, mem, &info1);
/****** TEST code >= 0 ******/
	planes[0].data = data3;
	planes[0].data_x = 0;
	planes[0].raster =
	    (image1.Height * image1.BitsPerComponent + 7) >> 3;
	/* Use the old image_data API. */
	code = (*dev_proc(dev, image_data))
          (dev, info1, &planes[0].data, 0,
           planes[0].raster, image1.Height);
/****** TEST code == 1 ******/
	code = (*dev_proc(dev, end_image))(dev, info1, true);
/****** TEST code >= 0 ******/
    }
    gs_grestore(pgs);

    /* Test an explicitly masked image. */
    gs_gsave(pgs);
    {
	gs_image3_t image3;
	static const byte data3mask[] =
	{
	    0x60,
	    0x90,
	    0x90,
	    0x60
	};
	static const byte data3x2mask[] =
	{
	    0x66,
	    0x99,
	    0x99,
	    0x66,
	    0x66,
	    0x99,
	    0x99,
	    0x66
	};

	gs_image3_t_init(&image3, &gray_cs, interleave_scan_lines);
	/* image */
	image3.ImageMatrix.xx = W;
	image3.ImageMatrix.yy = -H;
	image3.ImageMatrix.ty = H;
	/* data_image */
	image3.Width = W;
	image3.Height = H;
	image3.BitsPerComponent = 8;
	/* MaskDict */
	image3.MaskDict.ImageMatrix = image3.ImageMatrix;
	image3.MaskDict.Width = image3.Width;
	image3.MaskDict.Height = image3.Height;

	/* Display with 1-for-1 mask and image. */
	gs_translate(pgs, 0.5, 2.0);
	code = gx_device_begin_typed_image(dev, (gs_imager_state *) pgs,
				       NULL, (gs_image_common_t *) & image3,
					   NULL, &dcolor, NULL, mem, &info);
/****** TEST code >= 0 ******/
	planes[0].data = data3mask;
	planes[0].data_x = 0;
	planes[0].raster = (image3.MaskDict.Height + 7) >> 3;
	planes[1].data = data3;
	planes[1].data_x = 0;
	planes[1].raster =
	    (image3.Height * image3.BitsPerComponent + 7) >> 3;
	code = gx_image_plane_data(info, planes, image3.Height);
/****** TEST code == 1 ******/
	code = gx_image_end(info, true);
/****** TEST code >= 0 ******/

	/* Display with 2-for-1 mask and image. */
	image3.MaskDict.ImageMatrix.xx *= 2;
	image3.MaskDict.ImageMatrix.yy *= 2;
	image3.MaskDict.ImageMatrix.ty *= 2;
	image3.MaskDict.Width *= 2;
	image3.MaskDict.Height *= 2;
	gs_translate(pgs, 1.5, 0.0);
	code = gx_device_begin_typed_image(dev, (gs_imager_state *) pgs,
				       NULL, (gs_image_common_t *) & image3,
					   NULL, &dcolor, NULL, mem, &info);
/****** TEST code >= 0 ******/
	planes[0].data = data3x2mask;
	planes[0].raster = (image3.MaskDict.Width + 7) >> 3;
	{
	    int i;

	    for (i = 0; i < H; ++i) {
		planes[1].data = 0;
		code = gx_image_plane_data(info, planes, 1);
		planes[0].data += planes[0].raster;
/****** TEST code == 0 ******/
		planes[1].data = data3 + i * planes[1].raster;
		code = gx_image_plane_data(info, planes, 1);
		planes[0].data += planes[0].raster;
/****** TEST code >= 0 ******/
	    }
	}
/****** TEST code == 1 ******/
	code = gx_image_end(info, true);
/****** TEST code >= 0 ******/
    }
    gs_grestore(pgs);

    /* Test a chroma-keyed masked image. */
    gs_gsave(pgs);
    {
	gs_image4_t image4;
	const byte *data4 = data3;

	gs_image4_t_init(&image4, &gray_cs);
	/* image */
	image4.ImageMatrix.xx = W;
	image4.ImageMatrix.yy = -H;
	image4.ImageMatrix.ty = H;
	/* data_image */
	image4.Width = W;
	image4.Height = H;
	image4.BitsPerComponent = 8;

	/* Display with a single mask color. */
	gs_translate(pgs, 0.5, 0.5);
	image4.MaskColor_is_range = false;
	image4.MaskColor[0] = 0xcc;
	do_image(image4, data4);

	/* Display a second time with a color range. */
	gs_translate(pgs, 1.5, 0.0);
	image4.MaskColor_is_range = true;
	image4.MaskColor[0] = 0x40;
	image4.MaskColor[1] = 0x90;
	do_image(image4, data4);
    }
    gs_grestore(pgs);

#undef W
#undef H
#undef do_image
    return 0;
}

/* ---------------- Test program 6 ---------------- */
/* Test the C API for CIE CRDs, and color snapping. */

static void
spectrum(gs_state * pgs, int n)
{
    float den = n;
    float den1 = n - 1;
    float den2 = (n * 2 - 1) * n;
    int a, b, c;

    for (a = 0; a < n; ++a)
	for (b = 0; b < n; ++b)
	    for (c = 0; c < n; ++c) {
		double size = (n * 2 - c * 2 - 1) / den2;
		gs_client_color cc;

		cc.paint.values[0] = a / den1;
		cc.paint.values[1] = b / den1;
		cc.paint.values[2] = c / den1;
		gs_setcolor(pgs, &cc);
		fill_rect1(pgs,
			   a / den + c / den2, b / den + c / den2,
			   size, size);
	    }
}
static float
render_abc(floatp v, const gs_cie_render * ignore_crd)
{
    return v / 2;
}

static int
test6(gs_state * pgs, gs_memory_t * mem)
{
    gs_color_space *pcs;
    gs_cie_abc *pabc;
    gs_cie_render *pcrd;
    static const gs_vector3 white_point =
    {1, 1, 1};
    static const gs_cie_render_proc3 encode_abc =
    {
	{render_abc, render_abc, render_abc}
    };
#ifdef COLOR_MAP_DEVICE
    gx_device_cmap *cmdev;
#endif /* COLOR_MAP_DEVICE */
    int code;
    gs_color_space rgb_cs;

    gs_cspace_init_DeviceRGB(mem, &rgb_cs);

    gs_scale(pgs, 150.0, 150.0);
    gs_translate(pgs, 0.5, 0.5);
    gs_setcolorspace(pgs, &rgb_cs);
    spectrum(pgs, 5);
    gs_translate(pgs, 1.2, 0.0);
    /* We must set the CRD before the color space. */
    code = gs_cie_render1_build(&pcrd, mem, "test6");
    if (code < 0)
	return code;
    gs_cie_render1_initialize(mem, pcrd, NULL, &white_point, NULL,
			      NULL, NULL, NULL,
			      NULL, NULL, NULL,
			      NULL, &encode_abc, NULL,
			      NULL);
    gs_setcolorrendering(pgs, pcrd);
    gs_cspace_build_CIEABC(&pcs, NULL, mem);
    /* There should be an API for initializing CIE color spaces too.... */
    pabc = pcs->params.abc;
    pabc->common.points.WhitePoint = white_point;
    gs_cie_abc_complete(pabc);
    /* End of initializing the color space. */
    gs_setcolorspace(pgs, pcs);
    spectrum(pgs, 5);
#ifdef COLOR_MAP_DEVICE
    /* Now test color snapping. */
    cmdev =
	gs_alloc_struct_immovable(mem, gx_device_cmap, &st_device_cmap,
				  "cmap device");
    gdev_cmap_init(cmdev, gs_currentdevice(pgs),
		   device_cmap_snap_to_primaries);
    gs_setdevice_no_init(pgs, (gx_device *) cmdev);
#endif /* COLOR_MAP_DEVICE */
    gs_setrgbcolor(pgs, 0.0, 0.0, 0.0);		/* back to DeviceRGB space */
    gs_translate(pgs, -1.2, 1.2);
    spectrum(pgs, 5);
    gs_translate(pgs, 1.2, 0.0);
#ifdef COLOR_MAP_DEVICE
    set_cmap_method(cmdev, device_cmap_monochrome, pgs, mem);
#endif /* COLOR_MAP_DEVICE */
    spectrum(pgs, 5);
    gs_translate(pgs, -1.2, 1.2);
#ifdef COLOR_MAP_DEVICE
    set_cmap_method(cmdev, device_cmap_color_to_black_over_white, pgs, mem);
#endif /* COLOR_MAP_DEVICE */
    spectrum(pgs, 5);
    return 0;
}

/* ---------------- Test program 7 ---------------- */
/* Test the C API for non-monotonic halftones. */

#ifdef HALFTONE_FIX
static int
test7(gs_state * pgs, gs_memory_t * mem)
{
    /* Define a non-monotonic 4 x 4 halftone with 4 gray levels. */
    static const byte masks[1 * 4 * 4] =
    {
    /* 0% */
	0x00, 0x00, 0x00, 0x00,
    /* 25% */
	0x80, 0x40, 0x20, 0x10,
    /* 50% */
	0xa0, 0xa0, 0x50, 0x50,
    /* 75% */
	0xd0, 0xe0, 0x70, 0xb0
    };
    gs_ht *pht;
    int code;
    int i;

    /* Fabricate a Type 5 halftone. */
    code = gs_ht_build(&pht, 1, mem);
    dprintf1("ht build code = %d\n", code);
    code = gs_ht_set_mask_comp(pht, 0,
			       4, 4, 4, masks, NULL, NULL);
    dprintf1("set mask code = %d\n", code);
    code = gs_sethalftone(pgs, pht);
    dprintf1("sethalftone code = %d\n", code);
    for (i = 0; i <= 4; ++i) {
	gs_setgray(pgs, i / 4.0);
	fill_rect1(pgs, 100 + i * 100, 100, 50, 50);
    }
    return 0;
}
#endif

/* ---------------- Test program 8 ---------------- */
/* Test partially transparent patterns */

static int
test8(gs_state * pgs, gs_memory_t * mem)
{
    /*
     * Define a 16 x 16 pattern using a 4-entry palette
     * (white, red, green, black).
     */
    static const byte pdata[] =
    {
	0x7f, 0xff, 0x00, 0x03,
	0x7f, 0xff, 0x00, 0x0c,
	0x50, 0x00, 0x00, 0x30,
	0x50, 0x00, 0x00, 0xc0,
	0x50, 0x00, 0x03, 0x00,
	0x50, 0x00, 0x0c, 0x00,
	0x50, 0x00, 0x30, 0x00,
	0x50, 0x00, 0xc0, 0x00,
	0xf0, 0x00, 0xc0, 0x00,
	0xf0, 0x00, 0x30, 0x00,
	0xf0, 0x00, 0x0c, 0x00,
	0xf0, 0x00, 0x03, 0x00,
	0xf0, 0x00, 0x00, 0xc0,
	0xf0, 0x00, 0x00, 0x30,
	0xea, 0x55, 0xaa, 0x5c,
	0xea, 0x55, 0xaa, 0x57,
    };
    gs_depth_bitmap ptile;
    gs_const_string table;
    gs_color_space *pcs;
    gs_client_color ccolor;
    gs_color_space rgb_cs;

    gs_cspace_init_DeviceRGB(mem, &rgb_cs);

    table.data =
	(const byte *)"\377\377\377\377\000\000\000\377\000\000\000\000";
    table.size = 12;
    gs_cspace_build_Indexed(&pcs, &rgb_cs, 4, &table, mem);
    ptile.data = pdata;
    ptile.raster = 4;
    ptile.size.x = ptile.size.y = 16;
    ptile.id = gs_no_bitmap_id;
    ptile.pix_depth = 2;
    ptile.num_comps = 1;
    gs_makepixmappattern(&ccolor, &ptile, false /*mask */ , NULL /*pmat */ ,
			 gs_no_id, pcs, 0 /*white_index */ , pgs, mem);
    {
	gs_rect r;

	r.p.x = 100;
	r.p.y = 100;
	r.q.x = 200;
	r.q.y = 200;
	gs_setrgbcolor(pgs, 1.0, 1.0, 0.0);
	gs_rectfill(pgs, &r, 1);
	gs_setpattern(pgs, &ccolor);
	// gs_settexturetransparent(pgs, true);
	gs_rectfill(pgs, &r, 1);
	r.p.x += 150;
	r.q.x += 150;
	gs_setrgbcolor(pgs, 1.0, 1.0, 0.0);
	gs_rectfill(pgs, &r, 1);
	gs_setpattern(pgs, &ccolor);
	// gs_settexturetransparent(pgs, false);
	gs_rectfill(pgs, &r, 1);
    }
    return 0;
}

/* type42 test.  Lots of setup stuff here since the graphics library
   leaves most font parsing to the client. */
#include "gxfont.h"
#include "gxchar.h"
#include "gsgdata.h"
#include "gxfont42.h"
#include "gxfcache.h"

/* big endian accessors */
#define get_uint16(bptr)\
    (((bptr)[0] << 8) | (bptr)[1])
#define get_int16(bptr)\
    (((int)get_uint16(bptr) ^ 0x8000) - 0x8000)

static int
test9_get_int16(const byte *bptr)
{	
    return get_int16(bptr);
}

static uint
test9_get_uint16(const byte *bptr)
{	
    return get_uint16(bptr);
}

static long
test9_get_int32(const byte *bptr)
{	
    return ((long)get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

static ulong
test9_get_uint32(const byte *bptr)
{	
    return ((ulong)get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}

/* ACCESS is braindamage from gstype42.c - */
/* Set up a pointer to a substring of the font data. */
/* Free variables: pfont, string_proc. */
#define ACCESS(mem, base, length, vptr)\
  BEGIN\
    code = (*string_proc)(pfont, (ulong)(base), length, &vptr);\
    if ( code < 0 ) return code;\
    if ( code > 0 ) return_error(gs_error_invalidfont);\
  END

/* find a tt table by name derived from plchar.c */
ulong
test9_tt_find_table(gs_font_type42 *pfont, const char *tname, uint *plen)
{
    const byte *OffsetTable;
    int code;
    uint numTables;
    const byte *TableDirectory;
    uint i;
    ulong table_dir_offset = 0;
    int (*string_proc)(gs_font_type42 *, ulong, uint, const byte **) =
	pfont->data.string_proc;

    /* nb check return values */
    ACCESS(pfont->memory, 0, 12, OffsetTable);
    ACCESS(pfont->memory, table_dir_offset, 12, OffsetTable);
    numTables = test9_get_uint16(OffsetTable + 4);
    ACCESS(pfont->memory, table_dir_offset + 12, numTables * 16, TableDirectory);
    for ( i = 0; i < numTables; ++i ) {
        const byte *tab = TableDirectory + i * 16;
        if ( !memcmp(tab, tname, 4) ) {
            if ( plen )
                *plen = test9_get_uint32(tab + 12);
            return test9_get_uint32(tab + 8);
        }
    }
    return 0;
}

/* encode, derived from plchar.c */
static gs_glyph
test9_tt_encode_char(gs_font *p42, gs_char chr, gs_glyph not_used)
{
    /* NB encode me */
    return chr - 29;
}

static gs_char
test9_tt_decode_glyph(gs_font *p42, gs_glyph glyph, int ch)
{
    return GS_NO_CHAR;
}

static int
test9_tt_glyph_name(gs_font *pf, gs_glyph glyph, gs_const_string *pstr)
{
    return 0;
}

static int
test9_tt_string_proc(gs_font *p42, ulong offset, uint length, const byte **pdata)
{
    
    /* NB bounds check offset + length - use gs_object_size for memory
       buffers - if file read should fail */
    *pdata = p42->client_data + offset;
    return 0;
}

/* derived from plchar.c */
static int
test9_tt_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont, gs_char chr, gs_glyph glyph)
{
    int code;
    float sbw[4], w2[6];

    code = gs_type42_get_metrics((gs_font_type42 *)pfont, glyph, sbw);
    if (code < 0)
        return code;
    w2[0] = sbw[2], w2[1] = sbw[3];
    /* don't ask me about the following, I just work here. */
    { 
#define pbfont ((gs_font_base *)pfont)

        const gs_rect *pbbox =  &pbfont->FontBBox;
#undef pbfont

        w2[2] = pbbox->p.x, w2[3] = pbbox->p.y;
        w2[4] = pbbox->q.x, w2[5] = pbbox->q.y;
        if ( pfont->PaintType ) {   
            double expand = max(1.415, gs_currentmiterlimit(pgs)) *
                gs_currentlinewidth(pgs) / 2;

            w2[2] -= expand, w2[3] -= expand;
            w2[4] += expand, w2[5] += expand;
        }
    }

    if ( (code = gs_moveto(pgs, 0.0, 0.0)) < 0 )
        return code;

    if ( (code = gs_setcachedevice(penum, pgs, w2)) < 0 )
        return code;
    
    code = gs_type42_append(glyph,
                            (gs_imager_state *)pgs,
                            gx_current_path(pgs),
                            &penum->log2_scale,
                            gs_show_in_charpath(penum) != cpm_show,
                            pfont->PaintType,
                            (gs_font_type42 *)pfont);
    if ( code >= 0 )
        code = (pfont->PaintType ? gs_stroke(pgs) : gs_fill(pgs));
    return code;
}

byte *
test9_load_font_data(const char *filename, gs_memory_t * mem)
{
    FILE *in = fopen(filename, gp_fmode_rb);
    byte *data;
    ulong size;
    if (in == NULL)
        return NULL;
    size = (fseek(in, 0L, SEEK_END), ftell(in));
    rewind(in);
    data = gs_alloc_bytes(mem, size, "test9_load_font data");
    if ( data == 0 ) { 
        fclose(in);
        return NULL;
    }

    /* NB check size */
    fread(data, 1, size, in);
    fclose(in);
    return data;
}
    
/* windows tt file name */

#define TTF_FILENAME "/windows/fonts/A028-Ext.ttf"

/* TEST9 NB:

   1) assumes character successfully renders to cache.  There should
   be a charpath fallback if looking up the character in the cache
   returns NULL.

   2) Cache memory management needs investigation

   3) This implementation is one character at a time.

   4) needs encoding - fill in test9_tt_encode_char().

*/

static int
test9(gs_state * pgs, gs_memory_t * mem)
{
        
    gs_font_type42 *p42 = gs_alloc_struct(mem, gs_font_type42,
                                          &st_gs_font_type42,
                                          "new p42");
    gs_font_dir *pfont_dir = gs_font_dir_alloc(mem);

    byte *pfont_data = test9_load_font_data(TTF_FILENAME, mem);
    
    if (!pfont_data || !pfont_dir || !p42)
        return -1;

    /* no shortage of things to initialize */
    p42->next = p42->prev = 0;
    p42->memory = mem;
    p42->dir = pfont_dir;
    p42->is_resource = false;
    gs_notify_init(&p42->notify_list, gs_memory_stable(mem));
    p42->base = p42;
    p42->client_data = pfont_data;
    p42->WMode = 0;
    p42->PaintType = 0;
    p42->StrokeWidth = 0;
    p42->procs.init_fstack = gs_default_init_fstack;
    p42->procs.next_char_glyph = gs_default_next_char_glyph;
    p42->font_name.size = 0;
    p42->key_name.size = 0;
    p42->procs.glyph_name = test9_tt_glyph_name;
    p42->procs.decode_glyph = test9_tt_decode_glyph;
    p42->procs.define_font = gs_no_define_font;
    p42->procs.make_font = gs_no_make_font;
    p42->procs.font_info = gs_default_font_info;
    p42->procs.glyph_info = gs_default_glyph_info;
    p42->procs.glyph_outline = gs_no_glyph_outline;
    p42->procs.encode_char = test9_tt_encode_char;
    p42->procs.build_char = test9_tt_build_char;
    p42->id = gs_next_ids(mem, 1);
    gs_make_identity(&p42->FontMatrix);
    p42->FontType = ft_TrueType;
    p42->BitmapWidths = true;
    p42->ExactSize = fbit_use_outlines;
    p42->InBetweenSize = fbit_use_outlines;
    p42->TransformedChar = fbit_use_outlines;
    p42->FontBBox.p.x = p42->FontBBox.p.y =
        p42->FontBBox.q.x = p42->FontBBox.q.y = 0;
    uid_set_UniqueID(&p42->UID, p42->id);
    p42->encoding_index = ENCODING_INDEX_UNKNOWN;
    p42->nearest_encoding_index = ENCODING_INDEX_UNKNOWN;
    /* Initialize Type 42 specific data. */
    p42->data.string_proc = test9_tt_string_proc;
    gs_type42_font_init(p42);
    
    gs_definefont(pfont_dir, (gs_font *)p42);
    gs_setfont(pgs, (gs_font *)p42);
    {
        gs_text_params_t text_params;
        gs_text_enum_t *penum;
        byte *mystr = "The quick brown fox";
        floatp FontRenderingEmSize = 20; /* XPS terminology */
        gs_matrix fmat; /* font matrix */
        int code;
        {
            int i;
            if (gs_moveto(pgs, 72.0, 72.0) != 0)
                return -1;

            for (i = 0; i < strlen(mystr); i++) {
                dprintf1("%d\n", i);
                text_params.operation = (TEXT_FROM_STRING | TEXT_DO_DRAW | TEXT_RETURN_WIDTH);
                text_params.data.bytes = mystr + i;
                text_params.size = 1;


                gs_make_identity(&fmat);
                if ((gs_matrix_scale(&fmat, FontRenderingEmSize, FontRenderingEmSize, &fmat) != 0) ||
                    (gs_setcharmatrix(pgs, &fmat) != 0) ||
                    (gs_text_begin(pgs, &text_params, mem, &penum) != 0) ||
                    (gs_text_process(penum) != 0)) {
                    dprintf("text_failed\n");
                    return -1;
                }
                {
                    gs_show_enum *psenum = (gs_show_enum *)penum;
                    cached_fm_pair *ppair;
                    gs_fixed_point sub_pix_or = {0, 0};
                    cached_char *cc;
                    gs_matrix cmat;
                    gs_currentcharmatrix(pgs, &cmat, true);
                    code = gx_lookup_fm_pair(penum->current_font,
                                             &cmat,
                                             &penum->log2_scale,
                                             /* NB design grid */
                                             false,
                                             &ppair);

                    cc = gx_lookup_cached_char(penum->current_font, 
                                               ppair, 
                                               penum->returned.current_glyph,
                                               /* nb next three assume
                                                  no veritcal writing
                                                  '0', bitdepth '1'
                                                  and no sub pixel
                                                  stuff */
                                               0, 
                                               1, 
                                               &sub_pix_or);

                    debug_dump_bitmap(cc_bits(cc), 
                                      cc_raster(cc),
                                      cc->height, "");
                    /* update point (device space) */
                    gx_path_add_relative_point(gx_current_path(pgs),
                                               cc->wxy.x,
                                               cc->wxy.y);
                    gs_text_release(penum, "test_9");

                }
            }
        }
    }
    return 0;
}
