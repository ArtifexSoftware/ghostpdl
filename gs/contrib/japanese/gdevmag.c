/* Copyright (C) 1992, 1993 Aladdin Enterprises.  All rights reserved.

This file is part of Ghostscript.

Ghostscript is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
to anyone for the consequences of using it or for whether it serves any
particular purpose or works at all, unless he says so in writing.  Refer
to the Ghostscript General Public License for full details.

Everyone is granted permission to copy, modify and redistribute
Ghostscript, but only under the conditions described in the Ghostscript
General Public License.  A copy of this license is supposed to have been
given to you along with Ghostscript so you can know your rights and
responsibilities.  It should be in a file named COPYING.  Among other
things, the copyright notice and this notice must be preserved on all
copies.  */

/* gdevmag.c */
/* .MAG file format devices for Ghostscript */
#include "gdevprn.h"
#include "gdevpccm.h"
#include <stdlib.h>

/* ------ The device descriptors ------ */

/*
 * Default X and Y resolution.
 */
#define HEIGHT_10THS 117
#define DPI (8000.0/HEIGHT_10THS)
#define WIDTH_10THS (6400.0 / DPI)

static dev_proc_print_page(mag_4bit_print_page);
static dev_proc_open_device(gdev_mag_4bit_open);
static dev_proc_print_page(mag_8bit_print_page);
static dev_proc_open_device(gdev_mag_8bit_open);

/* internal routines. */
static int mag_make_flag(gx_device_printer *,int ,int ,byte **, byte *,byte *);
static int mag_comp_flag(gx_device_printer *,int ,byte * ,byte *,byte *, int, byte *);
static int mag_write_palette(gx_device_printer * ,int );
static int mag_print_page(gx_device_printer * , int , FILE * );

/* 4-bit planar (PC-9801 style) color. */

static gx_device_procs mag16_procs =
  prn_color_procs(gdev_mag_4bit_open, gdev_prn_output_page, gdev_prn_close,
    pc_4bit_map_rgb_color, pc_4bit_map_color_rgb);
gx_device_printer far_data gs_mag16_device =
  prn_device(mag16_procs, "mag16",
        (int)WIDTH_10THS, (int)HEIGHT_10THS,
        DPI, DPI,
        0,0,0,0,			/* margins */
        4, mag_4bit_print_page);

/* 8-bit planar color. */

static gx_device_procs mag256_procs =
  prn_color_procs(gdev_mag_8bit_open, gdev_prn_output_page, gdev_prn_close,
    pc_8bit_map_rgb_color, pc_8bit_map_color_rgb);
gx_device_printer far_data gs_mag256_device =
  prn_device(mag256_procs, "mag256",
        (int)WIDTH_10THS, (int)HEIGHT_10THS,
        DPI, DPI,
        0,0,0,0,			/* margins */
        8, mag_8bit_print_page);

/* ------ Private definitions ------ */

static int
gdev_mag_4bit_open(gx_device *dev)
{
  dev->width = (dev->width + 7) / 8 * 8;
  return gdev_prn_open(dev);
}

static int
gdev_mag_8bit_open(gx_device *dev)
{
  dev->width = (dev->width + 3) / 4 * 4;
  return gdev_prn_open(dev);
}

static int
mag_4bit_print_page(gx_device_printer *pdev,FILE *file)
{  return mag_print_page(pdev,4,file);
}

static int
mag_8bit_print_page(gx_device_printer *pdev,FILE *file)
{  return mag_print_page(pdev,8,file);
}

/* Write out a page in MAG format. */
/* This routine is used for all formats. */
static int
mag_print_page(gx_device_printer *pdev, int depth, FILE *file)
{
    int code = 0;			/* return code */
    const char *magic = "MAKI02  gs   ";
    char *user = getenv("USER");
    char check[256];
    byte header[32] = "\000\000\000\000"
                      "\000\000\000\000"
                      "\000\000\000\000";
    byte *flag;
    byte *flag_prev;
    byte *flag_a,*flag_b;
    byte *row_buffer;
    byte *row[17];
    byte *pixel;
    int raster = gdev_prn_raster(pdev);
    int height = pdev->height;
    int width = pdev->width;
    int width_pixel = width / ((depth == 4) ? 4 : 2 );
    int flag_size = width_pixel / 2;
    int flag_a_bytes = (flag_size + 7)/8 + 1;
    int flag_b_size = flag_size;
    long flag_b_bytes;
    long pixel_bytes;
    int y;

    row_buffer = (byte *)gs_malloc(pdev->memory->non_gc_memory, raster,17,"mag_row");
    flag = (byte *)gs_malloc(pdev->memory->non_gc_memory, flag_size,2,"mag_flag");
    flag_prev = flag + flag_size;
    flag_a = (byte *)gs_malloc(pdev->memory->non_gc_memory, flag_a_bytes,1,"mag_flag_a");
    flag_b = (byte *)gs_malloc(pdev->memory->non_gc_memory, flag_b_size,1,"mag_flag_b");
    pixel = (byte *)gs_malloc(pdev->memory->non_gc_memory, width,1,"mag_pixel");
    if (row_buffer == 0 || flag == 0 ||
        flag_a == 0 || flag_b == 0 || pixel == 0) {
      code = -1;
      goto mag_done;
    }
    for (y=0;y<17;y++)
      row[y] = row_buffer + raster*y;

    if (user == 0) user = "Unknown";
    strcpy(check,magic);
    sprintf(check+strlen(check),"%-18s",user);
    sprintf(check+31," Ghostscript with %s driver\x1a", pdev->dname);

    /* check sizes of flag and pixel data. */
    pixel_bytes = 0;
    flag_b_bytes = 0;
    {
      byte *f0=flag_prev,*f1=flag;
      memset(f0,0,flag_size);
      for (y=0;y<height;y++) {
        byte *f2;
        pixel_bytes += mag_make_flag(pdev,y,depth,row,f1,pixel);
        flag_b_bytes += mag_comp_flag(pdev,flag_size,f0,f1,flag_a,0,flag_b);
        f2 = f0; f0 = f1; f1 = f2;
      }
    }

    /* make header */
    { long offset;
#define put_word(b,w) ((b)[0] = (w)&0xff,(b)[1] = ((w)>>8)&0xff)
#define put_dword(b,w) (put_word(b,w),put_word((b)+2,(w)>>16))
      header[3] = depth == 4 ? 0 : 0x80;
      put_word(header+8,width-1);
      put_word(header+10,height-1);
      offset = 32 + (1 << depth) * 3;
      put_dword(header+12,offset);
      offset += (flag_size * height + 7) / 8;
      put_dword(header+16,offset);
      put_dword(header+20,flag_b_bytes);
      offset += flag_b_bytes;
      put_dword(header+24,offset);
      put_dword(header+28,pixel_bytes);
#undef put_word
#undef put_dword
    }

    /* write magic number, header, palettes. */
    fputs(check,file);
    fwrite(header,32,1,file);
    mag_write_palette(pdev,depth);

    /* write image */
    { int count;
      int next_bit;
      byte *f0=flag_prev,*f1=flag;

      /* flag A */
      memset(f0,0,flag_size);
      next_bit = 0;
      for (y=0;y<height;y++) {
        byte *f2;
        mag_make_flag(pdev,y,depth,row,f1,pixel);
        mag_comp_flag(pdev,flag_size,f0,f1,flag_a,next_bit,flag_b);
        count = (next_bit + flag_size) / 8;
        next_bit = (next_bit + flag_size) % 8;
        fwrite(flag_a,count,1,file);
        if (next_bit) flag_a[0] = flag_a[count];
        f2 = f0; f0 = f1; f1 = f2;
      }
      if (next_bit) fputc(flag_a[0],file);

      /* flag B */
      memset(f0,0,flag_size);
      for (y=0;y<height;y++) {
        byte *f2;
        mag_make_flag(pdev,y,depth,row,f1,pixel);
        count = mag_comp_flag(pdev,flag_size,f0,f1,flag_a,0,flag_b);
        fwrite(flag_b,count,1,file);
        f2 = f0; f0 = f1; f1 = f2;
      }

      /* pixel */
      for (y=0;y<height;y++) {
        count = mag_make_flag(pdev,y,depth,row,f1,pixel);
        fwrite(pixel,count,1,file);
      }
   }

mag_done:
    if (row_buffer) gs_free(pdev->memory->non_gc_memory, (char *)row_buffer, raster, 17, "mag_row");
    if (flag) gs_free(pdev->memory->non_gc_memory, (char *)flag,flag_size,2,"mag_flag");
    if (flag_a) gs_free(pdev->memory->non_gc_memory, (char *)flag_a,flag_a_bytes,1,"mag_flag_a");
    if (flag_b) gs_free(pdev->memory->non_gc_memory, (char *)flag_b,flag_b_size,1,"mag_flag_b");
    if (pixel) gs_free(pdev->memory->non_gc_memory, (char *)pixel,width,1,"mag_pixel");

    fflush(file);

        return code;
}

/* make flag and pixel data */
static int
mag_make_flag(gx_device_printer *pdev,int line_no, int depth,
  byte **row, byte *flag,byte *pixel)
{
  int x;
  int raster = gdev_prn_raster(pdev);
  byte *ppixel = pixel;

  { byte *prow = row[16];
    for (x=16;x>0;x--)
      row[x] = row[x-1];
    row[0] = prow;
  }
  gdev_prn_copy_scan_lines(pdev,line_no,row[0],raster);

  if (depth == 4) {
    for (x=0;x<raster;x++) {
      if (x&1) *(row[0]+x/2) |= *(row[0]+x);
      else     *(row[0]+x/2) = *(row[0]+x) << 4;
    }
    raster = (raster + 1) / 2;
  }

#define check_pixel(a,b) \
        (x >= (a) && line_no >= (b) && \
     *(row[0]+x)   == *(row[b] + (x - (a))) && \
         *(row[0]+x+1) == *(row[b] + (x+1 - (a))))
#define set_flag(v) \
    ((x & 2) ? (flag[x>>2] |= (v)) : (flag[x>>2] = (v)<<4))

  for (x=0; x<raster ;x+=2) {
    if (check_pixel(2,0))       set_flag(1);  /*  1 */
    else if (check_pixel(0,1))  set_flag(4);  /*  4 */
    else if (check_pixel(2,1))  set_flag(5);  /*  5 */
    else if (check_pixel(0,2))  set_flag(6);  /*  6 */
    else if (check_pixel(2,2))  set_flag(7);  /*  7 */
    else if (check_pixel(0,4))  set_flag(9);  /*  9 */
    else if (check_pixel(2,4))  set_flag(10); /* 10 */
    else if (check_pixel(4,0))  set_flag(2);  /*  2 */
    else if (check_pixel(4,2))  set_flag(8);  /*  8 */
    else if (check_pixel(4,4))  set_flag(11); /* 11 */
    else if (check_pixel(0,8))  set_flag(12); /* 12 */
    else if (check_pixel(2,8))  set_flag(13); /* 13 */
    else if (check_pixel(4,8))  set_flag(14); /* 14 */
    else if (check_pixel(8,0))  set_flag(3);  /*  3 */
    else if (check_pixel(0,16)) set_flag(15); /* 15 */
    else {                                     /*  0 */
      set_flag(0);
      *ppixel++ = *(row[0] + x);
      *ppixel++ = *(row[0] + x+1);
    }
  }
#undef check_pixel
#undef set_flag

  return ppixel - pixel;
}

/* compress flags (make flag A and B) */
static int
mag_comp_flag(gx_device_printer *pdev, int size,byte *f0,byte *f1,
  byte *flag_a,int next_bit,byte *flag_b)
{
  byte mask = 0x80>>next_bit;
  byte *pflag_b = flag_b;

  for ( ;size>0 ; size--) {
    byte b = *f0 ^ *f1;
    if (mask == 0x80) {
      *flag_a = 0;
    }
    if (b) {
      *flag_a |= mask;
      *pflag_b++ = b;
    }
    mask >>= 1;
    f0++;
    f1++;
    if (mask == 0) {
      mask = 0x80;
      flag_a++;
    }
  }

  return pflag_b - flag_b;
}

/* write palette */
static int
mag_write_palette(gx_device_printer *pdev,int depth)
{
  uint i;
  gx_color_value rgb[3];
  int max_index = 1 << depth;

  for ( i = 0; i < max_index; i++ ) {
    byte grb[3];
    (pdev->orig_procs.map_color_rgb)((gx_device *)pdev, (gx_color_index)i, rgb);
    grb[0] = rgb[1] >> (gx_color_value_bits - 8);
    grb[1] = rgb[0] >> (gx_color_value_bits - 8);
    grb[2] = rgb[2] >> (gx_color_value_bits - 8);
    fwrite(grb, 3,1,pdev->file);
  }
  return 0;
}
