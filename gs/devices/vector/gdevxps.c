/* Copyright (C) 2001-2014 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134, San Rafael,
   CA  94903, U.S.A., +1(415)492-9861, for further information.
*/

/* XPS output device */
#include "string_.h"
#include "stdio_.h"
#include "gx.h"
#include "gserrors.h"
#include "gdevvec.h"
#include "gxpath.h"
#include "gzcpath.h"
#include "stream.h"
#include "zlib.h"
#include "stdint_.h"
#include "gdevtifs.h"
#include "gsicc_create.h"
#include "gsicc_cache.h"

#define MAXPRINTERNAME 64
#if defined(__WIN32__) && XPSPRINT==1
int XPSPrint(char *FileName, char *PrinterName, int *reason);
#endif

#define MAXICCNAME 64
#define PROFILEPATH "Documents/1/Resources/Profiles/"

#define MAXTIFFNAME 64
#define IMAGEPATH "Documents/1/Resources/Images/"

/* default resolution. */
#ifndef X_DPI
#  define X_DPI 96
#endif
#ifndef Y_DPI
#  define Y_DPI 96
#endif

/* default constants */
#define XPS_DEFAULT_LINEWIDTH   1.0
#define XPS_DEFAULT_LINECAP     gs_cap_butt
#define XPS_DEFAULT_LINEJOIN    gs_join_miter
#define XPS_DEFAULT_MITERLIMIT  4.0

/* ---------------- Device definition ---------------- */

/* TODO graphics state definitions are not yet used */

/* xps join and cap values. */
static const char *join_str[] = {"Miter", "Bevel", "Round"};
static const char *cap_str[]  = {"Flat",  "Round", "Square", "Triangle"};

typedef enum {XPS_JOIN, XPS_START_CAP, XPS_END_CAP, XPS_THICK, XPS_MITER_LIMIT,
              XPS_STROKE_COLOR, XPS_FILL_COLOR} xps_attr_t;

typedef struct gx_device_xps_vals {
    double f;
    uint64_t i;
} val_t;

/* current and default values of state atrributes */
typedef struct gx_device_xps_attr_state {
    val_t cur;
    val_t def;
    const char *fmt;
    const char **vals;
} attr_t;

/* xps proto state - a copy of this table is made on the heap when the
   device is opened and the current state values are filled in at run
   time. NB not currently used. */
attr_t xps_fixed_state[] = {
     /* attribute           current  default         Format String            values */
    {/* XPS_JOIN */         {0.0, 0}, {0.0, 0},  "StrokeLineJoin=\"%s\"",     join_str},
    {/* XPS_START_CAP */    {0.0, 0}, {0.0, 0},  "StrokeStartLineCap=\"%s\"",  cap_str},
    {/* XPS_END_CAP */      {0.0, 0}, {0.0, 0},  "StrokeEndLineCap=\"%s\"",    cap_str},
    {/* XPS_THICK */        {0.0, 0}, {1.0, 0},  "StrokeThickness=\"%f\"",     NULL},
    {/* XPS_MITER_LIMIT */  {0.0, 0}, {10.0, 0}, "StrokeMiterLimit=\"%f\"",    NULL},
    {/* XPS_STROKE_COLOR */ {0.0, 0}, {0.0, 0},  "Stroke=\"%X\"",              NULL},
    {/* XPS_FILL_COLOR */   {0.0, 0}, {0.0, 0},  "Fill=\"%X\"",                NULL}
};

/* zip file management - a list of temporary files is maintained along
   with the final filename to be used in the archive.  filenames with
   associated contents in temporary files are maintained until the end
   of the job, at which time we enumerate the list and write the zip
   archive. */

typedef struct gx_device_xps_zdata_s {
    FILE *fp;
    ulong count;
} gx_device_xps_zdata_t;

/* Zip info for each file in the zip archive */
typedef struct gx_device_xps_zinfo_s {
    ulong CRC;
    ulong file_size;
    gx_device_xps_zdata_t data;
    long current_pos;
    ushort date;
    ushort time;
} gx_device_xps_zinfo_t;

/* a list of archive file names and their corresponding info
   (gx_device_xps_zinfo_t) */
typedef struct gx_device_xps_f2i_s {
    char *filename;
    gx_device_xps_zinfo_t *info;
    struct gx_device_xps_f2i_s *next;
} gx_device_xps_f2i_t;

/* Used for keeping track of icc profiles that we have written.   This way we 
   avoid writing the same one multiple times.  It would be nice to do this
   based upon the gs_id for images, but that id is really created after we
   have already drawn the path.  Not clear to me how to get a source ID. */
typedef struct xps_icc_data_s {
    int64_t hash;
    int index;
    struct xps_icc_data_s *next;
} xps_icc_data_t;

typedef enum {
    xps_solidbrush,
    xps_imagebrush,
    xps_visualbrush
} xps_brush_t;

typedef struct xps_image_enum_s {
    gdev_vector_image_enum_common;
    gs_matrix mat;
    TIFF *tif; /* in non-GC memory */
    char file_name[MAXTIFFNAME];
    char icc_name[MAXICCNAME];
} xps_image_enum_t;

gs_private_st_suffix_add0(st_xps_image_enum, xps_image_enum_t,
    "xps_image_enum_t", xps_image_enum_enum_ptrs,
    xps_image_enum_reloc_ptrs, st_vector_image_enum);

typedef struct gx_device_xps_s {
    /* superclass state */
    gx_device_vector_common;
    /* zip container bookkeeping */
    gx_device_xps_f2i_t *f2i;
    gx_device_xps_f2i_t *f2i_tail;
    /* local state */
    int page_count;     /* how many output_page calls we've seen */
    int image_count;    /* number of images so far */
    int relationship_count;  /* Pages relationships */
    xps_icc_data_t *icc_data;
    gx_color_index strokecolor, fillcolor;
    xps_brush_t stroketype, filltype;
    xps_image_enum_t *xps_pie;
    double linewidth;
    gs_line_cap linecap;
    gs_line_join linejoin;
    double miterlimit;
    bool can_stroke;
    unsigned char PrinterName[MAXPRINTERNAME];
} gx_device_xps;

gs_public_st_suffix_add1_final(st_device_xps, gx_device_xps,
    "gx_device_xps", device_xps_enum_ptrs, device_xps_reloc_ptrs,
    gx_device_finalize, st_device_vector, xps_pie);

#define xps_device_body(dname, depth)\
  std_device_dci_type_body(gx_device_xps, 0, dname, &st_device_xps, \
                           DEFAULT_WIDTH_10THS * X_DPI / 10, \
                           DEFAULT_HEIGHT_10THS * Y_DPI / 10, \
                           X_DPI, Y_DPI, \
                           (depth > 8 ? 3 : 1), depth, \
                           (depth > 1 ? 255 : 1), (depth > 8 ? 255 : 0), \
                           (depth > 1 ? 256 : 2), (depth > 8 ? 256 : 1))

static dev_proc_open_device(xps_open_device);
static dev_proc_output_page(xps_output_page);
static dev_proc_close_device(xps_close_device);
static dev_proc_get_params(xps_get_params);
static dev_proc_put_params(xps_put_params);
static dev_proc_fill_path(gdev_xps_fill_path);
static dev_proc_stroke_path(gdev_xps_stroke_path);
static dev_proc_finish_copydevice(xps_finish_copydevice);
static dev_proc_begin_image(xps_begin_image);

#define xps_device_procs \
{ \
        xps_open_device, \
        NULL,                   /* get_initial_matrix */\
        NULL,                   /* sync_output */\
        xps_output_page,\
        xps_close_device,\
        gx_default_rgb_map_rgb_color,\
        gx_default_rgb_map_color_rgb,\
        gdev_vector_fill_rectangle,\
        NULL,                   /* tile_rectangle */\
        NULL,                   /* copy_mono */\
        NULL,                   /* copy_color */\
        NULL,                   /* draw_line */\
        NULL,                   /* get_bits */\
        xps_get_params,\
        xps_put_params,\
        NULL,                   /* map_cmyk_color */\
        NULL,                   /* get_xfont_procs */\
        NULL,                   /* get_xfont_device */\
        NULL,                   /* map_rgb_alpha_color */\
        gx_page_device_get_page_device,\
        NULL,                   /* get_alpha_bits */\
        NULL,                   /* copy_alpha */\
        NULL,                   /* get_band */\
        NULL,                   /* copy_rop */\
        gdev_xps_fill_path,\
        gdev_xps_stroke_path,\
        NULL,                   /* fill_mask */\
        NULL,                   /* gdev_vector_fill_trapezoid, */     \
        NULL,                   /* gdev_vector_fill_parallelogram */        \
        NULL,                   /* gdev_vector_fill_triangle */           \
        NULL,                   /* draw_thin_line */\
        xps_begin_image,        /* begin_image */   \
        NULL,                   /* image_data */\
        NULL,                   /* end_image */\
        NULL,                   /* strip_tile_rectangle */\
        NULL,                    /* strip_copy_rop */\
        NULL,                   /* get_clipping_box */\
        NULL,                   /* begin_typed_image */\
        NULL,                   /* get_bits_rectangle */\
        NULL,                   /* map_color_rgb_alpha */\
        NULL,                   /* create_compositor */\
        NULL,                   /* get_hardware_params */\
        NULL,                   /* text_begin */\
        xps_finish_copydevice,\
        NULL,\
}

const gx_device_xps gs_xpswrite_device = {
    xps_device_body("xpswrite", 24),
    xps_device_procs
};

/* Vector device procedures */
static int
xps_beginpage(gx_device_vector *vdev);
static int
xps_setlinewidth(gx_device_vector *vdev, double width);
static int
xps_setlinecap(gx_device_vector *vdev, gs_line_cap cap);
static int
xps_setlinejoin(gx_device_vector *vdev, gs_line_join join);
static int
xps_setmiterlimit(gx_device_vector *vdev, double limit);
static int
xps_setdash(gx_device_vector *vdev, const float *pattern,
            uint count, double offset);
static int
xps_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
             gs_logical_operation_t diff);

static int
xps_can_handle_hl_color(gx_device_vector *vdev, const gs_imager_state *pis,
                        const gx_drawing_color * pdc);
static int
xps_setfillcolor(gx_device_vector *vdev, const gs_imager_state *pis,
                 const gx_drawing_color *pdc);
static int
xps_setstrokecolor(gx_device_vector *vdev, const gs_imager_state *pis,
                   const gx_drawing_color *pdc);

static int
xps_dorect(gx_device_vector *vdev, fixed x0, fixed y0,
           fixed x1, fixed y1, gx_path_type_t type);
static int
xps_beginpath(gx_device_vector *vdev, gx_path_type_t type);

static int
xps_moveto(gx_device_vector *vdev, double x0, double y0,
           double x, double y, gx_path_type_t type);
static int
xps_lineto(gx_device_vector *vdev, double x0, double y0,
           double x, double y, gx_path_type_t type);
static int
xps_curveto(gx_device_vector *vdev, double x0, double y0,
            double x1, double y1, double x2, double y2,
            double x3, double y3, gx_path_type_t type);
static int
xps_closepath(gx_device_vector *vdev, double x, double y,
              double x_start, double y_start, gx_path_type_t type);
static int
xps_endpath(gx_device_vector *vdev, gx_path_type_t type);

/* Vector device function table */
static const gx_device_vector_procs xps_vector_procs = {
    xps_beginpage,
    xps_setlinewidth,
    xps_setlinecap,
    xps_setlinejoin,
    xps_setmiterlimit,
    xps_setdash,
    gdev_vector_setflat,
    xps_setlogop,
    xps_can_handle_hl_color,
    xps_setfillcolor,
    xps_setstrokecolor,
    gdev_vector_dopath,
    xps_dorect,
    xps_beginpath,
    xps_moveto,
    xps_lineto,
    xps_curveto,
    xps_closepath,
    xps_endpath
};

/* Various static content we use in all xps jobs.  We don't use all of
   these resource types */
static char *xps_content_types = (char *)"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
    "<Default Extension=\"fdseq\" ContentType=\"application/vnd.ms-package.xps-fixeddocumentsequence+xml\" />"
    "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\" />"
    "<Default Extension=\"fdoc\" ContentType=\"application/vnd.ms-package.xps-fixeddocument+xml\" />"
    "<Default Extension=\"fpage\" ContentType=\"application/vnd.ms-package.xps-fixedpage+xml\" />"
    "<Default Extension=\"ttf\" ContentType=\"application/vnd.ms-opentype\" />"
    "<Default Extension = \"icc\" ContentType = \"application/vnd.ms-color.iccprofile\" />"
    "<Default Extension=\"tif\" ContentType=\"image/tiff\" />"
    "<Default Extension=\"png\" ContentType=\"image/png\" /></Types>";

static char *fixed_document_sequence = (char *)"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<FixedDocumentSequence xmlns=\"http://schemas.microsoft.com/xps/2005/06\">"
    "<DocumentReference Source=\"Documents/1/FixedDocument.fdoc\" />"
    "</FixedDocumentSequence>";

static char *fixed_document_fdoc_header = (char *)"<?xml version=\"1.0\" encoding=\"utf-8\"?>"
    "<FixedDocument xmlns=\"http://schemas.microsoft.com/xps/2005/06\">";

static char *rels_header = (char *)"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";

static char *rels_fdseq = (char *)"<Relationship Type=\"http://schemas.microsoft.com/xps/2005/06/fixedrepresentation\" "
    "Target=\"/FixedDocumentSequence.fdseq\" Id=\"Rdd12fb46c1de43ab\" />\n"
    "</Relationships>\n";

static char *rels_req_type = (char *) "\"http://schemas.microsoft.com/xps/2005/06/required-resource\"";

    /* Procedures to manage the containing zip archive */

/* Append a node of info for this archive file */
static int
zip_new_info_node(gx_device_xps *xps_dev, const char *filename)
{
    gx_device *dev = (gx_device *)xps_dev;
    gs_memory_t *mem = dev->memory;
    int lenstr;

    /* NB should use GC */
    gx_device_xps_zinfo_t *info = 
        (gx_device_xps_zinfo_t *)gs_alloc_bytes(mem->non_gc_memory, sizeof(gx_device_xps_zinfo_t), "zinfo");
    gx_device_xps_f2i_t *f2i = 
        (gx_device_xps_f2i_t *)gs_alloc_bytes(mem->non_gc_memory, sizeof(gx_device_xps_f2i_t), "zinfo node");

    if_debug1m('_', dev->memory, "new node %s\n", filename);
    
    if (info == NULL || f2i == NULL)
        return gs_throw_code(gs_error_Fatal);
    

    f2i->info = info;
    f2i->next = NULL;

    if (xps_dev->f2i == 0) { /* no head */
        xps_dev->f2i = f2i;
        xps_dev->f2i_tail = f2i;
    } else { /* append the node */
        xps_dev->f2i_tail->next = f2i;
        xps_dev->f2i_tail = f2i;
    }

    lenstr = strlen(filename);
    f2i->filename = (char*)gs_alloc_bytes(mem->non_gc_memory, lenstr + 1, "zinfo_filename");
    strcpy(f2i->filename, filename);
        
    info->data.fp = 0;
    info->data.count = 0;
    if (gs_debug_c('_')) {
        gx_device_xps_f2i_t *f2i = xps_dev->f2i;
        int node = 1;
        gx_device_xps_f2i_t *prev_f2i;
        
        while (f2i != NULL) {
            if_debug2m('_', dev->memory, "node:%d %s\n", node++, f2i->filename);
            prev_f2i = f2i;
            f2i=f2i->next;
        }
        if_debug1m('_', dev->memory, "tail okay=%s\n", prev_f2i == xps_dev->f2i_tail ? "yes" : "no");
    }
    return 0;
}

/* add a new file to the zip archive */
static int
zip_add_file(gx_device_xps *xps_dev, const char *filename)
{
    int code = zip_new_info_node(xps_dev, filename);
    if (code < 0)
        return gs_throw_code(gs_error_Fatal);
    return 0;
}

/* look up the information associated with the zip archive [filename] */
static gx_device_xps_zinfo_t *
zip_look_up_file_info(gx_device_xps *xps_dev, const char *filename)
{
    gx_device_xps_f2i_t *cur = xps_dev->f2i;
    while (cur) {
        if (!strcmp(cur->filename, filename))
            break;
        cur = cur->next;
    }
    return (cur ? cur->info : NULL);
}

/* Add data to an archived zip file, create the file if it doesn't exist */
static int
zip_append_data(gs_memory_t *mem, gx_device_xps_zinfo_t *info, byte *data, uint len)
{
    uint count;

    /* if there is no data then this is the first call for this
       archive file, open a temporary file to store the data. */
    if (info->data.count == 0) {
        char *filename =
          (char *)gs_alloc_bytes(mem, gp_file_name_sizeof, "zip_append_data(filename)");
        FILE *fp;
        
        if (!filename) {
            return(gs_throw_code(gs_error_VMerror));
        }
        
        fp = gp_open_scratch_file_rm(mem, "xpsdata-",
                                        filename, "wb+");

        if (fp == NULL) {
            gs_free_object(mem, filename, "zip_append_data(filename)");
            return gs_throw_code(gs_error_Fatal);
        }
        gs_free_object(mem, filename, "zip_append_data(filename)");
        info->data.fp = fp;
    }

    /* shouldn't happen unless the first call opens the temporary file
       but writes no data. */
    if (info->data.fp == NULL)
        return gs_throw_code(gs_error_Fatal);

    count = fwrite(data, 1, len, info->data.fp);
    if (count != len) {
        fclose(info->data.fp);
        return -1;
    }

    /* probably unnecessary but makes debugging easier */
    fflush(info->data.fp);

    info->data.count += len;

    return 0;
}

/* Seek within an archived zip file. Needed to use with tiff */
static uint64_t
zip_seek(gs_memory_t *mem, gx_device_xps_zinfo_t *info, uint64_t off, int whence)
{
    if (info->data.fp == NULL && off == 0)
        return 0;

    if (info->data.fp == NULL)
        return (uint64_t)-1;

    if (gp_fseek_64(info->data.fp, (gs_offset_t)off, whence) < 0) {
        return (uint64_t)-1;
    }
    return (gp_ftell_64(info->data.fp));
}

/* write to one of the archives (filename) in the zip archive */
static int
write_to_zip_file(gx_device_xps *xps_dev, const char *filename,
                  byte *data, uint len)
{
    gx_device *dev = (gx_device *)xps_dev;
    gs_memory_t *mem = dev->memory;

    gx_device_xps_zinfo_t *info = zip_look_up_file_info(xps_dev, filename);
    int code = 0;

    /* No information on this archive file, create a new zip entry
       info node */
    if (info == NULL) {
        code = zip_add_file(xps_dev, filename);
        if (code < 0)
            return gs_rethrow_code(code);
        info = zip_look_up_file_info(xps_dev, filename);
    }

    if (info == NULL)
        return gs_throw_code(gs_error_Fatal);

    code = zip_append_data(mem, info, data, len);
    if (code < 0)
        return gs_rethrow_code(code);

    return code;
}

/* Seek in an archive.  Needed for tiff writing support */
static uint64_t
seek_to_zip_file(gx_device_xps *xps_dev, const char *filename, uint64_t off,
        int whence)
{
    gx_device *dev = (gx_device *)xps_dev;
    gs_memory_t *mem = dev->memory;

    gx_device_xps_zinfo_t *info = zip_look_up_file_info(xps_dev, filename);
    int code = 0;

    /* No information on this archive file, create a new zip entry
    info node */
    if (info == NULL) {
        code = zip_add_file(xps_dev, filename);
        if (code < 0)
            return gs_rethrow_code(code);
        info = zip_look_up_file_info(xps_dev, filename);
    }

    if (info == NULL)
        return gs_throw_code(gs_error_Fatal);

    return zip_seek(mem, info, off, whence);
}

static void
put_bytes(stream *zs, byte *buf, uint len)
{
    uint used;
    sputs(zs, buf, len, &used);
    /* NB return value */
}

static void
put_u32(stream *zs, unsigned long l)
{
    sputc(zs, (byte)(l));
    sputc(zs, (byte)(l >> 8));
    sputc(zs, (byte)(l >> 16));
    sputc(zs, (byte)(l >> 24));
}

static void
put_u16(stream *zs, ushort s)
{
    sputc(zs, (byte)(s));
    sputc(zs, (byte)(s >> 8));
}


/* TODO - the following 2 definitions need to done correctly */
static ushort
make_dos_date(uint year, uint month, uint day)
{
    uint delta_1980 = year - 1980;
    return (delta_1980 << 9 | month << 5 | day);
}

static ushort
make_dos_time(uint hour, uint min, uint sec)
{
    /* note the seconds are multiplied by 2 */
    return (hour << 11 | min << 5 | sec >> 1);
}

/* convenience routine. */
static int
write_str_to_zip_file(gx_device_xps *xps_dev, const char *filename,
                      const char *str)
{
    return write_to_zip_file(xps_dev, filename, (byte *)str, strlen(str));
}

/* zip up a single file moving its data from the temporary file to the
   zip container. */
static int
zip_close_archive_file(gx_device_xps *xps_dev, const char *filename)
{

    gx_device_xps_zinfo_t *info = zip_look_up_file_info(xps_dev, filename);
    gx_device_xps_zdata_t data = info->data;
    byte buf[4];
    unsigned long crc = 0;
    int count = 0;
    int len;
    /* we can't use the vector stream accessor because it calls beginpage */
    stream *f = ((gx_device_vector*)xps_dev)->strm;

    if (info == NULL)
        return -1;

    if (data.count >= 0) {
        FILE *fp = data.fp;
        uint nread;
        crc = crc32(0L, Z_NULL, 0);
        rewind(fp);
        while (!feof(fp)) {
            nread = fread(buf, 1, sizeof(buf), fp);
            crc = crc32(crc, buf, nread);
            count = count + nread;
        }
        /* If this is a TIFF file, then update the data count information.
           During the writing of the TIFF directory there is seeking and
           relocations that occur, which make data.count incorrect. 
           We could just do this for all the files and avoid the test. */
        len = strlen(filename);
        if (len > 3 && (strncmp("tif", &(filename[len - 3]), 3) == 0)) {
            info->data.count = count;
            data = info->data;
        }
    }

    info->current_pos = stell(f);
    info->CRC = crc;

    /* NB should use ghostscript "gp" time and date functions for
       this, for now we hardwire the dates. */
    info->date = make_dos_date(2012, 2, 16);
    info->time = make_dos_time(9, 15, 0);

    put_u32(f, 0x04034b50); /* magic */
    put_u16(f, 20);         /* version */
    put_u16(f, 0);          /* flags */
    put_u16(f, 0);          /* method */
    put_u16(f, info->time);
    put_u16(f, info->date);
    put_u32(f, crc);
    put_u32(f, data.count); /* compressed */
    put_u32(f, data.count); /* uncompressed */
    put_u16(f, strlen(filename));
    put_u16(f, 0);         /* extra field length */
    put_bytes(f, (byte *)filename, strlen(filename));
    {
        FILE *fp = data.fp;
        rewind(fp);
        while (!feof(fp)) {
                ulong nread = fread(buf, 1, sizeof(buf), fp);
                put_bytes(f, buf, nread);
        }
        fclose(fp);
    }
    put_bytes(f, 0, 0); /* extra field */
    return 0;
}

/* walk the file info node list writing all the files to the
   archive */
static int
zip_close_all_archive_files(gx_device_xps *xps_dev)
{
    gx_device_xps_f2i_t *f2i = xps_dev->f2i;
    while (f2i) {
        int code = zip_close_archive_file(xps_dev, f2i->filename);
        if (code < 0) {
            return code;
        }
        f2i = f2i->next;
    }
    return 0;
}

/* write all files to the zip container and write the zip central
   directory */
static int
zip_close_archive(gx_device_xps *xps_dev)
{
    gx_device_xps_f2i_t *f2i = xps_dev->f2i;

    int entry_count = 0;
    long pos_before_cd;
    long pos_after_cd;
    /* we can't use the vector stream accessor because it calls beginpage */
    stream *f = ((gx_device_vector*)xps_dev)->strm;
    int code = zip_close_all_archive_files(xps_dev);

    pos_before_cd = stell(f);

    if (code < 0)
        return code;

    while (f2i) {
        gx_device_xps_zinfo_t *info = f2i->info;
        put_u32(f, 0x02014b50); /* magic */
        put_u16(f, 20); /* version made by */
        put_u16(f, 20); /* version required */
        put_u16(f, 0); /* bit flag */
        put_u16(f, 0); /* compression method */
        put_u16(f, info->time);
        put_u16(f, info->date);
        put_u32(f, info->CRC);
        put_u32(f, info->data.count);  /* compressed */
        put_u32(f, info->data.count); /* uncompressed */
        put_u16(f, strlen(f2i->filename));

        /* probably will never use the next 6 so we just set them to
           zero here */
        put_u16(f, 0); /* extra field length */
        put_u16(f, 0); /* file comment length */
        put_u16(f, 0); /* disk number */
        put_u16(f, 0); /* internal file attributes */
        put_u32(f, 0); /* external file attributes */

        put_u32(f, info->current_pos);
        put_bytes(f, (byte *)f2i->filename, strlen(f2i->filename));

        put_bytes(f, 0, 0); /* extra field */
        put_bytes(f, 0, 0); /* extra comment */

        entry_count++;
        f2i = f2i->next;
    }

    pos_after_cd = stell(f);

    put_u32(f, 0x06054b50);
    put_u16(f, 0); /* number of disks */
    put_u16(f, 0); /* disk where central directory starts */
    put_u16(f, entry_count); /* # of records in cd */
    put_u16(f, entry_count); /* total # of records in cd */
    put_u32(f, pos_after_cd - pos_before_cd); /* size of central cd */
    put_u32(f, pos_before_cd); /* offset of central directory */
    put_u16(f, 0); /* comment length */
    put_bytes(f, 0, 0); /* comment */

    return 0;
}

/* Brush management */
static void
xps_setstrokebrush(gx_device_xps *xps, xps_brush_t type)
{
    if_debug1m('_', xps->memory, "xps_setstrokebrush:%d\n", (int)type);

    xps->stroketype = type;
}

static void
xps_setfillbrush(gx_device_xps *xps, xps_brush_t type)
{
    if_debug1m('_', xps->memory, "xps_setfillbrush:%d\n", (int)type);

    xps->filltype = type;
}

/* Device procedures */
static int
xps_open_device(gx_device *dev)
{
    gx_device_vector *const vdev = (gx_device_vector*)dev;
    gx_device_xps *const xps = (gx_device_xps*)dev;
    int code = 0;

    vdev->v_memory = dev->memory;
    vdev->vec_procs = &xps_vector_procs;
    gdev_vector_init(vdev);
    code = gdev_vector_open_file_options(vdev, 512,
        VECTOR_OPEN_FILE_SEQUENTIAL);
    if (code < 0)
      return gs_rethrow_code(code);

    /* xps-specific initialization goes here */
    xps->page_count = 0;
    xps->relationship_count = 0;
    xps->strokecolor = gx_no_color_index;
    xps->fillcolor = gx_no_color_index;
    xps_setstrokebrush(xps, xps_solidbrush);
    xps_setfillbrush(xps, xps_solidbrush);
    /* these should be the graphics library defaults instead? */
    xps->linewidth = XPS_DEFAULT_LINEWIDTH;
    xps->linecap = XPS_DEFAULT_LINECAP;
    xps->linejoin = XPS_DEFAULT_LINEJOIN;
    xps->miterlimit = XPS_DEFAULT_MITERLIMIT;
    xps->can_stroke = true;
    /* zip info */
    xps->f2i = NULL;
    xps->f2i_tail = NULL;

    /* Image related stuff */
    xps->image_count = 0;
    xps->xps_pie = NULL;

    /* ICC related stuff */
    xps->icc_data = NULL;

    code = write_str_to_zip_file(xps, (char *)"FixedDocumentSequence.fdseq",
                                 fixed_document_sequence);
    if (code < 0)
        return gs_rethrow_code(code);
    
    code = write_str_to_zip_file(xps, (char *)"[Content_Types].xml",
                                 xps_content_types);
    if (code < 0)
        return gs_rethrow_code(code);

    code = write_str_to_zip_file(xps,
                                 (char *)"Documents/1/FixedDocument.fdoc",
                                 fixed_document_fdoc_header);
    if (code < 0)
        return gs_rethrow_code(code);

    /* Right out the magical stuff related to the fix document sequence relationship */
    code = write_str_to_zip_file(xps, (char *)"_rels/.rels", rels_header);
    if (code < 0)
        return gs_rethrow_code(code);

    code = write_str_to_zip_file(xps, (char *)"_rels/.rels", rels_fdseq);
    if (code < 0)
        return gs_rethrow_code(code);

    return code;
}

/* write xps commands to Pages/%d.fpage */
static int
write_str_to_current_page(gx_device_xps *xps, const char *str)
{
    const char *page_template = "Documents/1/Pages/%d.fpage";
    char buf[128]; /* easily enough to accommodate the string and a page number */

    /* we're one ahead of the page count */
    int code = gs_sprintf(buf, page_template, xps->page_count+1);
    if (code < 0)
        return gs_rethrow_code(code);
    
    return write_str_to_zip_file(xps, buf, str);
}

/* add relationship to Pages/_rels/%d.fpage.rels */
static int
add_new_relationship(gx_device_xps *xps, const char *str)
{
    const char *rels_template = "Documents/1/Pages/_rels/%d.fpage.rels";
    char buf[128]; /* easily enough to accommodate the string and a page number */
    char line[300];
    const char *fmt;

    int code = gs_sprintf(buf, rels_template, xps->page_count + 1);
    if (code < 0)
        return gs_rethrow_code(code);

    /* Check if this is our first one */
    if (xps->relationship_count == 0) 
        write_str_to_zip_file(xps, buf, rels_header);

    /* Now add the reference */
    fmt = "<Relationship Target = \"/%s\" Id = \"R%d\" Type = %s/>\n";
    gs_sprintf(line, fmt, str, xps->relationship_count, rels_req_type);

    xps->relationship_count++;

    return write_str_to_zip_file(xps, buf, line);
}

static int
close_page_relationship(gx_device_xps *xps)
{
    const char *rels_template = "Documents/1/Pages/_rels/%d.fpage.rels";
    char buf[128]; /* easily enough to accommodate the string and a page number */

    int code = gs_sprintf(buf, rels_template, xps->page_count + 1);
    if (code < 0)
        return gs_rethrow_code(code);

    write_str_to_zip_file(xps, buf, "</Relationships>");
    return 0;
}

        /* Page management */

/* Complete a page */
static int
xps_output_page(gx_device *dev, int num_copies, int flush)
{
    gx_device_xps *const xps = (gx_device_xps*)dev;
    gx_device_vector *vdev = (gx_device_vector *)dev;
    int code;

    write_str_to_current_page(xps, "</Canvas></FixedPage>");
    if (xps->relationship_count > 0)
    {
        /* Close the relationship xml */
        code = close_page_relationship(xps);
        if (code < 0)
            return gs_rethrow_code(code);
        xps->relationship_count = 0;  /* Reset for next page */
    }
    xps->page_count++;

    if (ferror(xps->file))
      return gs_throw_code(gs_error_ioerror);

    if ((code=gx_finish_output_page(dev, num_copies, flush)) < 0)
        return code;

    /* Check if we need to change the output file for separate
       pages. NB not sure if this will work correctly. */
    if (gx_outputfile_is_separate_pages(((gx_device_vector *)dev)->fname, dev->memory)) {
        if ((code = xps_close_device(dev)) < 0)
            return code;
        code = xps_open_device(dev);
    }

    if_debug1m('_', dev->memory, "xps_output_page - page=%d\n", xps->page_count);
    vdev->in_page = false;

    return code;
}

static void
xps_release_icc_info(gx_device *dev)
{
    gx_device_xps *xps = (gx_device_xps*)dev;
    xps_icc_data_t *curr;
    xps_icc_data_t *icc_data = xps->icc_data;

    while (icc_data != NULL) {
        curr = icc_data;
        icc_data = icc_data->next;
        gs_free(dev->memory->non_gc_memory, curr, sizeof(xps_icc_data_t), 1,
            "xps_release_icc_info");
    }
    return;
}

/* Close the device */
static int
xps_close_device(gx_device *dev)
{
    const gx_device_xps *xps = (gx_device_xps*)dev;
    int code;

    /* closing for the FixedDocument */
    code = write_str_to_zip_file(xps, "Documents/1/FixedDocument.fdoc", "</FixedDocument>");
    if (code < 0)
        return gs_rethrow_code(code);

    if (ferror(xps->file))
      return gs_throw_code(gs_error_ioerror);

    code = zip_close_archive(xps);
    if (code < 0)
        return gs_rethrow_code(code);

    /* Release the icc info */
    xps_release_icc_info(dev);

#if defined(__WIN32__) && XPSPRINT==1
    code = gdev_vector_close_file((gx_device_vector*)dev);
    if (code < 0)
        return gs_rethrow_code(code);

    if (strlen((const char *)xps->PrinterName)) {
        int reason;
        code = XPSPrint(xps->fname, (char *)xps->PrinterName, &reason);
        if (code < 0) {
            switch(code) {
                case -1:
                    break;
                case -2:
                    eprintf1("ERROR: Could not create competion event: %08X\n", reason);
                    break;
                case -3:
                    eprintf1("ERROR: Could not create MultiByteString from PrinerName: %s\n", xps->PrinterName);
                    break;
                case -4:
                    eprintf1("ERROR: Could not start XPS print job: %08X\n", reason);
                    break;
                case -5:
                    eprintf1("ERROR: Could not create XPS OM Object Factory: %08X\n", reason);
                    break;
                case -6:
                    eprintf1("ERROR: Could not create MultiByteString from OutputFile: %s\n", xps->fname);
                    break;
                case -7:
                    eprintf1("ERROR: Could not create Package from File %08X\n", reason);
                    break;
                case -8:
                    eprintf1("ERROR: Could not write Package to stream %08X\n", reason);
                    break;
                case -9:
                    eprintf1("ERROR: Could not close job stream: %08X\n", reason);
                    break;
                case -10:
                    eprintf1("ERROR: Wait for completion event failed: %08X\n", reason);
                    break;
                case -11:
                    eprintf1("ERROR: Could not get job status: %08X\n", reason);
                    break;
                case -12:
                    eprintf("ERROR: job was cancelled\n");
                    break;
                case -13:
                    eprintf1("ERROR: Print job failed: %08X\n", reason);
                    break;
                case -14:
                    eprintf("ERROR: unexpected failure\n");
                    break;
                case -15:
                case -16:
                    eprintf("ERROR: XpsPrint.dll does not exist or is missing a required method\n");
                    break;
            }
            return(gs_throw_code(gs_error_invalidaccess));
        }
        return(0);
    }
#else
    return gdev_vector_close_file((gx_device_vector*)dev);
#endif
}

/* Respond to a device parameter query from the client */
static int
xps_get_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    if_debug0m('_', dev->memory, "xps_get_params\n");

    /* call our superclass to add its standard set */
    code = gdev_vector_get_params(dev, plist);
    if (code < 0)
      return gs_rethrow_code(code);

#if defined(__WIN32__) && XPSPRINT==1
    {
        gs_param_string ofns;
        gx_device_xps *const xps = (gx_device_xps*)dev;

        /* xps specific parameters are added to plist here */
        ofns.data = (const byte *)&xps->PrinterName;
        ofns.size = strlen(xps->fname);
        ofns.persistent = false;
        if ((code = param_write_string(plist, "PrinterName", &ofns)) < 0)
            return code;
    }
#endif
    return code;
}

/* Read the device parameters passed to us by the client */
static int
xps_put_params(gx_device *dev, gs_param_list *plist)
{
    int code = 0;

    if_debug0m('_', dev->memory, "xps_put_params\n");

    /* xps specific parameters are parsed here */
#if defined(__WIN32__) && XPSPRINT==1
    {	/* NB: The following is not strictly correct since changes are */
        /*     made even if 'gdev_vector_put_params' returns an error. */
        gs_param_name param_name;
        gs_param_string pps;
        gx_device_xps *const xps = (gx_device_xps*)dev;
        switch (code = param_read_string(plist, (param_name = "PrinterName"), &pps)) {
        case 0:
            if (pps.size > 64) {
                eprintf1("\nERROR: PrinterName too long (max %d)\n", MAXPRINTERNAME);
            } else {
                memcpy(xps->PrinterName, pps.data, pps.size);
                xps->PrinterName[pps.size] = 0;
            }
            break;
        default:
            param_signal_error(plist, param_name, code);
        case 1:
            /*            memset(&xps->PrinterName, 0x00, MAXPRINTERNAME);*/
            break;
        }
    }
#endif
    /* call our superclass to get its parameters, like OutputFile */
    code = gdev_vector_put_params(dev, plist);	/* errors are handled by caller  or setpagedevice */

    return code;
}

static int xps_finish_copydevice(gx_device *dev, const gx_device *from_dev)
{
    const gx_device_xps *xps = (gx_device_xps*)dev;

    memset(xps->PrinterName, 0x00, MAXPRINTERNAME);
    return 0;
}

static int
set_state_color(gx_device_vector *vdev, const gx_drawing_color *pdc, gx_color_index *color)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    
    /* hack so beginpage is called */
    (void)gdev_vector_stream((gx_device_vector*)xps);
 
    /* Usually this is not an actual error but a signal to the
       graphics library to simplify the color */
    if (!gx_dc_is_pure(pdc)) {
        return_error(gs_error_rangecheck);
    }

    *color = gx_dc_pure_color(pdc);
    return 0;
}

static int
xps_setfillcolor(gx_device_vector *vdev, const gs_imager_state *pis, const gx_drawing_color *pdc)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;

    if_debug1m('_', xps->memory, "xps_setfillcolor:%06X\n", (uint32_t)gx_dc_pure_color(pdc));
    
    return set_state_color(vdev, pdc, &xps->fillcolor);
}

static int
xps_setstrokecolor(gx_device_vector *vdev, const gs_imager_state *pis, const gx_drawing_color *pdc)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    
    if_debug1m('_', xps->memory, "xps_setstrokecolor:%06X\n", (uint32_t)gx_dc_pure_color(pdc));
    
    return set_state_color(vdev, pdc, &xps->strokecolor);
}

static int
xps_beginpage(gx_device_vector *vdev)
{

    gx_device_xps *xps = (gx_device_xps *)vdev;
    char buf[128];
    int code = 0;

    if_debug0m('_', xps->memory, "xps_beginpage\n");

    {
        const char *template = "<PageContent Source=\"Pages/%d.fpage\" />";
        /* Note page count is 1 less than the current page */
        code = gs_sprintf(buf, template, xps->page_count + 1);
        if (code < 0)
            return gs_rethrow_code(code);

        /* Put a reference to this new page in the FixedDocument */
        code = write_str_to_zip_file(xps, "Documents/1/FixedDocument.fdoc", buf);
        if (code < 0)
            return gs_rethrow_code(code);

    }

    {
        const char *page_size_template = "<FixedPage Width=\"%d\" Height=\"%d\" "
            "xmlns=\"http://schemas.microsoft.com/xps/2005/06\" xml:lang=\"en-US\">\n";
        code = gs_sprintf(buf, page_size_template,
                       (int)(xps->MediaSize[0] * 4.0/3.0),  /* pts -> 1/96 inch */
                       (int)(xps->MediaSize[1] * 4.0/3.0));
        if (code < 0)
            return gs_rethrow_code(code);
        code = write_str_to_current_page(xps, buf);
        if (code < 0)
            return gs_rethrow_code(code);
    }
    {
        const char *canvas_template = "<Canvas RenderTransform=\"%g,%g,%g,%g,%g,%g\">\n";
        code = gs_sprintf(buf, canvas_template,
                       96.0/xps->HWResolution[0], 0.0, 0.0,
                       96.0/xps->HWResolution[1], 0.0, 0.0);
        if (code < 0)
            return gs_rethrow_code(code);

        code = write_str_to_current_page(xps, buf);
        if (code < 0)
            return gs_rethrow_code(code);
    }

    if_debug4m('_', xps->memory,
               "page info: resx=%g resy=%g width=%d height=%d\n",
               xps->HWResolution[0], xps->HWResolution[1],
               (int)xps->MediaSize[0], (int)xps->MediaSize[1]);

    return code;
}

static int
xps_setlinewidth(gx_device_vector *vdev, double width)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;

    if_debug1m('_', xps->memory, "xps_setlinewidth(%lf)\n", width);

    xps->linewidth = width;

    return 0;
}
static int
xps_setlinecap(gx_device_vector *vdev, gs_line_cap cap)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    const char *linecap_names[] = {"butt", "round", "square",
        "triangle", "unknown"};

    if (cap < 0 || cap > gs_cap_unknown)
        return gs_throw_code(gs_error_rangecheck);
    if_debug1m('_', xps->memory, "xps_setlinecap(%s)\n", linecap_names[cap]);

    xps->linecap = cap;

    return 0;
}
static int
xps_setlinejoin(gx_device_vector *vdev, gs_line_join join)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    const char *linejoin_names[] = {"miter", "round", "bevel",
        "none", "triangle", "unknown"};

    if (join < 0 || join > gs_join_unknown)
        return gs_throw_code(gs_error_rangecheck);
    if_debug1m('_', xps->memory, "xps_setlinejoin(%s)\n", linejoin_names[join]);

    xps->linejoin = join;

    return 0;
}
static int
xps_setmiterlimit(gx_device_vector *vdev, double limit)
{
    if_debug1m('_', vdev->memory, "xps_setmiterlimit(%lf)\n", limit);
    return 0;
}
static int
xps_setdash(gx_device_vector *vdev, const float *pattern,
            uint count, double offset)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    if_debug2m('_', vdev->memory, "xps_setdash count:%d offset:%g\n", count, offset);
    xps->can_stroke = (count == 0);
    return 0;
}
static int
xps_setlogop(gx_device_vector *vdev, gs_logical_operation_t lop,
             gs_logical_operation_t diff)
{
    if_debug2m('_', vdev->memory, "xps_setlogop(%u,%u) set logical operation\n",
        lop, diff);
    /* XPS can fake some simpler modes, but we ignore this for now. */
    return 0;
}

        /* Other state */

static int
xps_can_handle_hl_color(gx_device_vector *vdev, const gs_imager_state *pis,
                          const gx_drawing_color *pdc)
{
    if_debug0m('_', vdev->memory, "xps_can_handle_hl_color\n");
    return 0;
}

/* Paths */
static bool
image_brush_fill(gx_path_type_t path_type, xps_brush_t brush_type)
{
    return brush_type == xps_imagebrush;
}

static bool
drawing_path(gx_path_type_t path_type, xps_brush_t brush_type)
{
    return ((path_type & gx_path_type_stroke) || (path_type & gx_path_type_fill) ||
        image_brush_fill(path_type, brush_type));
}

static void
xps_finish_image_path(gx_device_vector *vdev)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[300];
    const char *fmt;
    gs_matrix matrix;

    /* Path is started.  Do the image brush image brush and close the path */
    write_str_to_current_page(xps, "\t<Path.Fill>\n");
    write_str_to_current_page(xps, "\t\t<ImageBrush ");
    fmt = "ImageSource = \"{ColorConvertedBitmap /%s /%s}\" Viewbox=\"%d, %d, %d, %d\" ViewboxUnits = \"Absolute\" Viewport = \"%d, %d, %d, %d\" ViewportUnits = \"Absolute\" TileMode = \"None\" >\n";
    gs_sprintf(line, fmt, xps->xps_pie->file_name, xps->xps_pie->icc_name,
        0, 0, xps->xps_pie->width, xps->xps_pie->height, 0, 0,
        xps->xps_pie->width, xps->xps_pie->height);
    write_str_to_current_page(xps, line);

    /* Now the render transform.  This is applied to the image brush. Path
    is already transformed */
    write_str_to_current_page(xps, "\t\t\t<ImageBrush.Transform>\n");
    fmt = "\t\t\t\t<MatrixTransform Matrix = \"%g,%g,%g,%g,%g,%g\" />\n";
    matrix = xps->xps_pie->mat;
    gs_sprintf(line, fmt,
        matrix.xx, matrix.xy, matrix.yx, matrix.xx, matrix.tx, matrix.ty);
    write_str_to_current_page(xps, line);
    write_str_to_current_page(xps, "\t\t\t</ImageBrush.Transform>\n");
    write_str_to_current_page(xps, "\t\t</ImageBrush>\n");
    write_str_to_current_page(xps, "\t</Path.Fill>\n");
    /* End this path */
    write_str_to_current_page(xps, "</Path>\n");
}

static int
xps_dorect(gx_device_vector *vdev, fixed x0, fixed y0,
           fixed x1, fixed y1, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[300];
    const char *fmt;
    uint32_t c;

    (void)gdev_vector_stream((gx_device_vector*)xps);

    if_debug9m('_', xps->memory,
               "rect type=%d coords=%g,%g %g,%g %g,%g %g,%g\n", type,
                fixed2float(x0), fixed2float(y0),
                fixed2float(x0), fixed2float(y1),
                fixed2float(x1), fixed2float(y1),
                fixed2float(x1), fixed2float(y0));
               
    
    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_dorect: type not supported %x\n", type);
        return 0;
    }

    if ((type & gx_path_type_stroke) && !xps->can_stroke) {
        return_error(gs_error_rangecheck);
    }

    if (image_brush_fill(type, xps->filltype)) {
        /* Do the path data  */
        fmt = "<Path Data=\"M %g, %g L %g, %g %g, %g %g, %g Z\" >\n";
        gs_sprintf(line, fmt,
            fixed2float(x0), fixed2float(y0),
            fixed2float(x0), fixed2float(y1),
            fixed2float(x1), fixed2float(y1),
            fixed2float(x1), fixed2float(y0));
        write_str_to_current_page(xps, line);
        /* And now the rest of the details */
        xps_finish_image_path(vdev);
    } else if (type & gx_path_type_fill) {
        /* Solid fill */
        write_str_to_current_page(xps, "<Path ");
        /* NB - F0 should be changed for a different winding type */
        fmt = "Fill=\"#%06X\" Data=\"M %g,%g V %g H %g V %g Z\" ";
        c = xps->fillcolor & 0xffffffL;
        gs_sprintf(line, fmt, c,
                   fixed2float(x0), fixed2float(y0),
                   fixed2float(y1), fixed2float(x1),
                   fixed2float(y0));
        write_str_to_current_page(xps, line);
        write_str_to_current_page(xps, "/>\n");
    } else {
        /* Solid stroke */
        write_str_to_current_page(xps, "<Path ");
        fmt = "Stroke=\"#%06X\" Data=\"M %g,%g V %g H %g V %g Z\" ";
        c = xps->strokecolor & 0xffffffL;
        gs_sprintf(line, fmt, c,
                   fixed2float(x0), fixed2float(y0),
                   fixed2float(y1), fixed2float(x1),
                   fixed2float(y0));
        write_str_to_current_page(xps, line);

        if (type & gx_path_type_stroke) {
            /* NB format width. */
            fmt = "StrokeThickness=\"%g\" ";
            gs_sprintf(line, fmt, xps->linewidth);
            write_str_to_current_page(xps, line);
        }
        write_str_to_current_page(xps, "/>\n");
    }
    /* end and close NB \n not necessary. */
    return 0;
}

static int
gdev_xps_fill_path(gx_device * dev, const gs_imager_state * pis, gx_path * ppath,
                   const gx_fill_params * params,
                   const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    if (gx_path_is_void(ppath)) {
        return 0;
    }
    return gdev_vector_fill_path(dev, pis, ppath, params, pdcolor, pcpath);
}

static int
gdev_xps_stroke_path(gx_device * dev, const gs_imager_state * pis, gx_path * ppath,
                     const gx_stroke_params * params,
                     const gx_drawing_color * pdcolor, const gx_clip_path * pcpath)
{
    if (gx_path_is_void(ppath)) {
        return 0;
    }
    return gdev_vector_stroke_path(dev, pis, ppath, params, pdcolor, pcpath);
}
           
static int
xps_beginpath(gx_device_vector *vdev, gx_path_type_t type)
{
    char line[300];
    gx_device_xps *xps = (gx_device_xps *)vdev;
    uint32_t c;
    const char *fmt;
    
    (void)gdev_vector_stream((gx_device_vector*)xps);

    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_beginpath: type not supported %x\n", type);
        return 0;
    }

    if (!xps->can_stroke) {
        return_error(gs_error_rangecheck);
    }

    c = type & gx_path_type_fill ? xps->fillcolor : xps->strokecolor;
    c &= 0xffffffL;

    if (!image_brush_fill(type, xps->filltype)) {
        write_str_to_current_page(xps, "<Path ");
        if (type & gx_path_type_fill)
            fmt = "Fill=\"#%06X\" Data=\"";
        else
            fmt = "Stroke=\"#%06X\" Data=\"";
        gs_sprintf(line, fmt, c);
        write_str_to_current_page(xps, line);
    }
    else {
        write_str_to_current_page(xps, "<Path Data=\"");
    }
    
    if_debug1m('_', xps->memory, "xps_beginpath %s\n", line);

    return 0;
}

static int
xps_moveto(gx_device_vector *vdev, double x0, double y0,
           double x, double y, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[300];

    if_debug2m('_', xps->memory, "xps_moveto %g %g\n", x, y);

    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_moveto: type not supported %x\n", type);
        return 0;
    }

    gs_sprintf(line, " M %g,%g", x, y);
    write_str_to_current_page(xps, line);
    if_debug1m('_', xps->memory, "xps_moveto %s", line);
    return 0;
}

static int
xps_lineto(gx_device_vector *vdev, double x0, double y0,
           double x, double y, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[200];

    if_debug2m('_', xps->memory, "xps_lineto %g %g\n", x, y);
    
    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_lineto: type not supported %x\n", type);
        return 0;
    }
    gs_sprintf(line, " L %g,%g", x, y);
    write_str_to_current_page(xps, line);
    if_debug1m('_', xps->memory, "xps_lineto %s\n", line);
    return 0;
}

static int
xps_curveto(gx_device_vector *vdev, double x0, double y0,
            double x1, double y1, double x2, double y2,
            double x3, double y3, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[200];
    
    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_curveto: type not supported %x\n", type);
        return 0;
    }
    
    gs_sprintf(line, " C %g,%g %g,%g %g,%g", x1, y1,
            x2,y2,x3,y3);
    write_str_to_current_page(xps,line);
    if_debug1m('_', xps->memory, "xps_curveto %s\n", line);
            
    return 0;
}

static int
xps_closepath(gx_device_vector *vdev, double x, double y,
              double x_start, double y_start, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;

    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_closepath: type not supported %x\n", type);
        return 0;
    }

    write_str_to_current_page(xps, " Z");
    if_debug0m('_', xps->memory, "xps_closepath");

    return 0;
}

static int
xps_endpath(gx_device_vector *vdev, gx_path_type_t type)
{
    gx_device_xps *xps = (gx_device_xps *)vdev;
    char line[200];
    const char *fmt;

    /* skip non-drawing paths for now */
    if (!drawing_path(type, xps->filltype)) {
        if_debug1m('_', xps->memory, "xps_endpath: type not supported %x\n", type);
        return 0;
    }

    if (image_brush_fill(type, xps->filltype)) {
        write_str_to_current_page(xps, "\" >\n");
        /* And now the rest of the details */
        xps_finish_image_path(vdev);
    } else if (type & gx_path_type_stroke) {
        /* NB format width. */
        fmt = "\" StrokeThickness=\"%g\" />\n";
        gs_sprintf(line, fmt, xps->linewidth);
        write_str_to_current_page(xps, line);
    } else { /* fill */
        /* close the path data attribute */
        write_str_to_current_page(xps, "\" />\n");
    }

    return 0;
}

/* Image handling */

static image_enum_proc_plane_data(xps_image_plane_data);
static image_enum_proc_end_image(xps_image_end_image);
static const gx_image_enum_procs_t xps_image_enum_procs = {
    xps_image_plane_data, xps_image_end_image
};

/* High level image support */

/* Prototypes */
static TIFF* tiff_from_name(gx_device_xps *dev, const char *name, int big_endian,
    bool usebigtiff);
static void tiff_set_values(xps_image_enum_t *pie, TIFF *tif, 
                            cmm_profile_t *profile);
static void xps_tiff_set_handlers(void);

/* Check if we have the ICC profile in the package */
static xps_icc_data_t*
xps_find_icc(const gx_device_xps *xdev, cmm_profile_t *icc_profile)
{
    xps_icc_data_t *icc_data = xdev->icc_data;

    while (icc_data != NULL) {
        if (icc_data->hash == gsicc_get_hash(icc_profile)) {
            return icc_data;
        }
        icc_data = icc_data->next;
    }
    return NULL;
}

static int
xps_create_icc_name(const gx_device_xps *xps_dev, cmm_profile_t *profile, char *name)
{
    xps_icc_data_t *icc_data;

    icc_data = xps_find_icc(xps_dev, profile);
    if (icc_data == NULL)
        return gs_throw_code(gs_error_rangecheck);  /* Should be there */

    snprintf(name, MAXICCNAME, "%sProfile_%d.icc", PROFILEPATH, icc_data->index);
    return 0;
}

static void
xps_create_image_name(gx_device *dev, char *name)
{
    gx_device_xps *const xdev = (gx_device_xps *)dev;

    snprintf(name, MAXTIFFNAME, "%s%d.tif", IMAGEPATH, xdev->image_count);
    xdev->image_count++;
}

static int
xps_write_profile(const gs_imager_state *pis, cmm_profile_t *profile, const gx_device_xps *xps_dev)
{
    char file_name[MAXICCNAME];
    byte *profile_buffer;
    int size;

    int code;

    code = xps_create_icc_name(xps_dev, profile, &(file_name[0]));
    if (code < 0)
        return gs_rethrow_code(code);

    /* Need V2 ICC Profile */
    profile_buffer = gsicc_create_getv2buffer(pis, profile, &size);
    code = write_to_zip_file(xps_dev, file_name, profile_buffer, size);
    if (code < 0)
        return gs_rethrow_code(code);
    return 0;
}

static int
xps_add_icc_relationship(xps_image_enum_t *pie)
{
    gx_device_xps *xps = (gx_device_xps*) (pie->dev);
    int code;

    code = add_new_relationship(xps, pie->icc_name);
    if (code < 0)
        return gs_rethrow_code(code);

    return 0;
}

static int 
xps_add_image_relationship(xps_image_enum_t *pie)
{
    gx_device_xps *xps = (gx_device_xps*) (pie->dev);
    int code;

    code = add_new_relationship(xps, pie->file_name);
    if (code < 0)
        return gs_rethrow_code(code);

    return 0;
}

static int
xps_begin_image(gx_device *dev, const gs_imager_state *pis, 
                const gs_image_t *pim, gs_image_format_t format, 
                const gs_int_rect *prect, const gx_drawing_color *pdcolor,
                const gx_clip_path *pcpath, gs_memory_t *mem,
                gx_image_enum_common_t **pinfo)
{
    const gx_device_vector *vdev = (gx_device_vector *)dev;
    gx_device_xps *const xdev = (gx_device_xps *)dev;
    const gs_color_space *pcs = pim->ColorSpace;
    xps_image_enum_t *pie;
    xps_icc_data_t *icc_data;
    int base_index;
    gs_matrix mat;
    int code;
    gx_clip_path cpath;
    gs_fixed_rect bbox;

    /* No image mask yet */
    if (((const gs_image1_t *)pim)->ImageMask)
        goto use_default;

    /* No wacky color spaces yet, including indexed */
    base_index = gs_color_space_get_index(pcs);
    if (base_index > gs_color_space_index_DeviceCMYK &&
        base_index != gs_color_space_index_ICC)
        goto use_default;

    /* Only 8 bit / component */
    if (pim->BitsPerComponent != 8)
        goto use_default;

    gs_matrix_invert(&pim->ImageMatrix, &mat);
    gs_matrix_multiply(&mat, &ctm_only(pis), &mat);

    pie = gs_alloc_struct(mem, xps_image_enum_t, &st_xps_image_enum,
                          "xps_begin_image");
    if (pie == 0) {
        code = gs_note_error(gs_error_VMerror);
        goto fail;
    }

    /* Set the brush types to image */
    xps_setstrokebrush(xdev, xps_imagebrush);
    xps_setfillbrush(xdev, xps_imagebrush);
    pie->mat = mat;
    xdev->xps_pie = pie;
    /* We need this set a bit early for the ICC relationship writing */
    pie->dev = (gx_device*) xdev;

    /* Now we actually write out the image and icc profile data to the zip
      package. Test if profile is already here.  If not add it. */
    if (xps_find_icc(xdev, pcs->cmm_icc_profile_data) == NULL)
    {
        icc_data = (xps_icc_data_t*)gs_alloc_bytes(dev->memory->non_gc_memory,
            sizeof(xps_icc_data_t), "xps_begin_image");
        if (icc_data == NULL)
            gs_throw(gs_error_VMerror, "Allocation of icc_data failed");

        icc_data->hash = gsicc_get_hash(pcs->cmm_icc_profile_data);
        if (xdev->icc_data == NULL) {
            icc_data->index = 0;
            xdev->icc_data = icc_data;
            xdev->icc_data->next = NULL;
        } else {
            icc_data->next = xdev->icc_data;
            icc_data->index = icc_data->next->index + 1;
            xdev->icc_data = icc_data;
        }
        /* Add profile to the package */
        code = xps_write_profile(pis, pcs->cmm_icc_profile_data, xdev);
        if (code < 0)
            return gs_rethrow_code(code);

        /* Get name for mark up and for relationship. Have to wait and do 
           this after it is added to the package */
        code = xps_create_icc_name(xdev, pcs->cmm_icc_profile_data, &(pie->icc_name[0]));
        if (code < 0)
            return gs_rethrow_code(code);

        /* Add ICC relationship */
        xps_add_icc_relationship(pie);
    } else {
        /* Get name for mark up.  We already have it in the relationship and list */
        code = xps_create_icc_name(xdev, pcs->cmm_icc_profile_data, &(pie->icc_name[0]));
        if (code < 0)
            return gs_rethrow_code(code);
    }
    /* Get image name for mark up */
    xps_create_image_name(dev, &(pie->file_name[0]));

    /* Set width and height here */
    pie->width = pim->Width;
    pie->height = pim->Height;

    if (pcpath == NULL) {
        (*dev_proc(dev, get_clipping_box)) (dev, &bbox);
        gx_cpath_init_local(&cpath, dev->memory);
        code = gx_cpath_from_rectangle(&cpath, &bbox);
        pcpath = &cpath;
    } else {
        /* Force vector device to do new path as the clip path is the image
           path.  I had a case where the clip path ids were the same but the 
           CTM was changing which resulted in subsequent images coming up 
           missing on the page. i.e. only the first one was shown. */
        ((gx_device_vector*) vdev)->clip_path_id = vdev->no_clip_path_id;
    }

    code = gdev_vector_begin_image(vdev, pis, pim, format, prect,
        pdcolor, pcpath, mem, &xps_image_enum_procs,
        (gdev_vector_image_enum_t *)pie);
    if (code < 0)
        return code;

    /* Null out the device pie.  It was just needed for the above vector command */
    xdev->xps_pie = NULL;

    pie->tif = tiff_from_name(xdev, pie->file_name, false, false);
    xps_tiff_set_handlers();
    tiff_set_values(pie, pie->tif, pcs->cmm_icc_profile_data);
    code = TIFFCheckpointDirectory(pie->tif);

    *pinfo = (gx_image_enum_common_t *)pie;
    return 0;
fail:
    gs_free_object(mem, pie, "xps_begin_image");
use_default:
    return gx_default_begin_image(dev, pis, pim, format, prect,
        pdcolor, pcpath, mem, pinfo);
}

/* Process the next piece of an image. */
static int
xps_image_plane_data(gx_image_enum_common_t *info, 
                     const gx_image_plane_t *planes, int height, 
                     int *rows_used)
{
    xps_image_enum_t *pie = (xps_image_enum_t *)info;
    int data_bit = planes[0].data_x * info->plane_depths[0];
    int width_bits = pie->width * info->plane_depths[0];
    int i;
    int code;
    byte *data;

    /****** SHOULD HANDLE NON-BYTE-ALIGNED DATA ******/
    if (width_bits != pie->bits_per_row || (data_bit & 7) != 0)
        return_error(gs_error_rangecheck);
    if (height > pie->height - pie->y)
        height = pie->height - pie->y;
    for (i = 0; i < height; pie->y++, ++i) {
        data = planes[0].data + planes[0].raster * i + (data_bit >> 3);
        code = TIFFWriteScanline(pie->tif, data, pie->y, 0);
        if (code < 0)
            return code;
    }
    *rows_used = height;
    return pie->y >= pie->height;
}

/* Clean up by releasing the buffers. */
static int
xps_image_end_image(gx_image_enum_common_t * info, bool draw_last)
{
    xps_image_enum_t *pie = (xps_image_enum_t *)info;
    int code = 0;

    code = TIFFWriteDirectory(pie->tif);
    TIFFCleanup(pie->tif);

    /* N.B. Write the final strip, if any. */

    /* Reset the brush type to solid */
    xps_setstrokebrush((gx_device_xps *) (pie->dev), xps_solidbrush);
    xps_setfillbrush((gx_device_xps *) (pie->dev), xps_solidbrush);

    /* Add the image relationship */
    code = xps_add_image_relationship(pie);
    if (code < 0)
        return gs_rethrow_code(code);

    return code;
}

/* Tiff related code so that we can output all the image data in Tiff format.
   Tiff has the advantage of supporting Gray, RGB, CMYK as well as multiple
   bit depts and having lossless compression.  Much of this was borrowed from
   gstiffio.c but refactored to enable us to write out to the zip archive. */

#define TIFF_PRINT_BUF_LENGTH 1024
static const char tifs_msg_truncated[] = "\n*** Previous line has been truncated.\n";

static void 
tiff_set_values(xps_image_enum_t *pie, TIFF *tif, cmm_profile_t *profile)
{
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, pie->height);
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, pie->width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, pie->height);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, 96.0);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, 96.0);

    switch (profile->data_cs)
    {
        case  gsGRAY:
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, pie->bits_per_pixel);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
            break;
        case  gsRGB:
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, pie->bits_per_pixel / 3);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
            break;
        case gsCMYK:
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, pie->bits_per_pixel / 4);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_SEPARATED);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 4);
            break;
        default:
            TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, pie->bits_per_pixel);
            TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
            TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    }
}

/* place to hold the data for our libtiff i/o hooks */
typedef struct tifs_io_xps_t
{
    const char *filename;
    gx_device_xps *pdev;
} tifs_io_xps;

/* libtiff i/o hooks */
static int
xps_tifsDummyMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
    (void)fd;
    (void)pbase;
    (void)psize;
    return (0);
}

static void
xps_tifsDummyUnmapProc(thandle_t fd, void* base, toff_t size)
{
    (void)fd;
    (void)base;
    (void)size;
}

static size_t
xps_tifsReadProc(thandle_t fd, void* buf, size_t size)
{
    size_t size_io = (size_t)size;

    if ((size_t)size_io != size) {
        return (size_t)-1;
    }
    return 0;
}

static size_t
xps_tifsWriteProc(thandle_t fd, void* buf, size_t size)
{
    tifs_io_xps *tiffio = (tifs_io_xps *)fd;
    size_t size_io = (size_t)size;
    int code;

    if ((size_t)size_io != size) {
        return (size_t)-1;
    }

    code = write_to_zip_file(tiffio->pdev, tiffio->filename, (byte *)buf, size);
    if (code < 0)
        return gs_rethrow_code(code);

    return size;
}

static uint64_t
xps_tifsSeekProc(thandle_t fd, uint64_t off, int whence)
{
    tifs_io_xps *tiffio = (tifs_io_xps *)fd;
    gs_offset_t off_io = (gs_offset_t)off;

    if ((uint64_t)off_io != off) {
        return (uint64_t)-1; 
    }

    return seek_to_zip_file(tiffio->pdev, tiffio->filename, off, whence);
}

static int
xps_tifsCloseProc(thandle_t fd)
{
    tifs_io_xps *tiffio = (tifs_io_xps *)fd;
    gx_device_xps *pdev = tiffio->pdev;

    gs_free(pdev->memory->non_gc_memory, tiffio, sizeof(tifs_io_xps), 1, 
        "xps_tifsCloseProc");
    return 0;
}

/* Could not see where this was getting used so basically a dummy proc
   for now. */
static uint64_t
xps_tifsSizeProc(thandle_t fd)
{
    uint64_t length = 0;

    return length;
}

static TIFF *
tiff_from_name(gx_device_xps *dev, const char *name, int big_endian, bool usebigtiff)
{
    char mode[5] = "w";
    int modelen = 1;
    TIFF *t;
    tifs_io_xps *tiffio;

    if (big_endian)
        mode[modelen++] = 'b';
    else
        mode[modelen++] = 'l';

    if (usebigtiff)
        /* this should never happen for libtiff < 4.0 - see tiff_put_some_params() */
        mode[modelen++] = '8';

    mode[modelen] = (char)0;

    tiffio = (tifs_io_xps *)gs_malloc(dev->memory->non_gc_memory, 
            sizeof(tifs_io_xps), 1, "tiff_from_filep");
    if (!tiffio) {
        return NULL;
    }
    tiffio->filename = name;
    tiffio->pdev = dev;

    t = TIFFClientOpen(name, mode,
        (thandle_t)tiffio, (TIFFReadWriteProc)xps_tifsReadProc,
        (TIFFReadWriteProc)xps_tifsWriteProc, (TIFFSeekProc)xps_tifsSeekProc,
        xps_tifsCloseProc, (TIFFSizeProc)xps_tifsSizeProc, xps_tifsDummyMapProc,
        xps_tifsDummyUnmapProc);

    return t;
}

static void
xps_tifsWarningHandlerEx(thandle_t client_data, const char *module, 
                         const char *fmt, va_list ap)
{
    tifs_io_xps *tiffio = (tifs_io_xps *)client_data;
    gx_device_xps *pdev = tiffio->pdev;
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s", buf);
        dmlprintf1(pdev->memory, "%s\n", tifs_msg_truncated);
    }
    else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }
}

static void
xps_tifsErrorHandlerEx(thandle_t client_data, const char *module, 
                       const char *fmt, va_list ap)
{
    tifs_io_xps *tiffio = (tifs_io_xps *)client_data;
    gx_device_xps *pdev = tiffio->pdev;
    const char *max_size_error = "Maximum TIFF file size exceeded";
    int count;
    char buf[TIFF_PRINT_BUF_LENGTH];

    count = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (count >= sizeof(buf) || count < 0)  { /* C99 || MSVC */
        dmlprintf1(pdev->memory, "%s\n", buf);
        dmlprintf1(pdev->memory, "%s", tifs_msg_truncated);
    }
    else {
        dmlprintf1(pdev->memory, "%s\n", buf);
    }

#if (TIFFLIB_VERSION >= 20111221)
    if (!strncmp(fmt, max_size_error, strlen(max_size_error))) {
        dmlprintf(pdev->memory, "Use -dUseBigTIFF(=true) for BigTIFF output\n");
    }
#endif
}

static void
xps_tiff_set_handlers(void)
{
    (void)TIFFSetErrorHandler(NULL);
    (void)TIFFSetWarningHandler(NULL);
    (void)TIFFSetErrorHandlerExt(xps_tifsErrorHandlerEx);
    (void)TIFFSetWarningHandlerExt(xps_tifsWarningHandlerEx);
}
