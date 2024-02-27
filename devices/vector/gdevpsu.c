/* Copyright (C) 2001-2024 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* PostScript-writing utilities */
#include "math_.h"
#include "time_.h"
#include "stat_.h"
#include "unistd_.h"
#include "gx.h"
#include "gscdefs.h"
#include "gxdevice.h"
#include "gdevpsu.h"
#include "spprint.h"
#include "stream.h"
#include "gserrors.h"

/* ---------------- Low level ---------------- */

/* Write a 0-terminated array of strings as lines. */
int
psw_print_lines(gp_file *f, const char *const lines[])
{
    int i;
    for (i = 0; lines[i] != 0; ++i) {
        if (fprintf(f, "%s\n", lines[i]) < 0)
            return_error(gs_error_ioerror);
    }
    return 0;
}

/* Write the ProcSet name. */
static void
psw_put_procset_name(stream *s, const gx_device *dev,
                     const gx_device_pswrite_common_t *pdpc)
{
    pprints1(s, "GS_%s", dev->dname);
    pprintd3(s, "_%d_%d_%d",
            (int)pdpc->LanguageLevel,
            (int)(pdpc->LanguageLevel * 10 + 0.5) % 10,
            pdpc->ProcSet_version);
}
static void
psw_print_procset_name(gp_file *f, const gx_device *dev,
                       const gx_device_pswrite_common_t *pdpc)
{
    byte buf[100];		/* arbitrary */
    stream s;

    s_init(&s, dev->memory);
    swrite_file(&s, f, buf, sizeof(buf));
    psw_put_procset_name(&s, dev, pdpc);
    sflush(&s);
}

/* Write a bounding box. */
static void
psw_print_bbox(gp_file *f, const gs_rect *pbbox)
{
    fprintf(f, "%%%%BoundingBox: %d %d %d %d\n",
            (int)floor(pbbox->p.x), (int)floor(pbbox->p.y),
            (int)ceil(pbbox->q.x), (int)ceil(pbbox->q.y));
    fprintf(f, "%%%%HiResBoundingBox: %f %f %f %f\n",
            pbbox->p.x, pbbox->p.y, pbbox->q.x, pbbox->q.y);
}

/* ---------------- File level ---------------- */

static const char *const psw_ps_header[] = {
    "%!PS-Adobe-3.0",
    "%%Pages: (atend)",
    0
};

static const char *const psw_eps_header[] = {
    "%!PS-Adobe-3.0 EPSF-3.0",
    0
};

static const char *const psw_begin_prolog[] = {
    "%%EndComments",
    "%%BeginProlog",
    "% This copyright applies to everything between here and the %%EndProlog:",
                                /* copyright */
                                /* begin ProcSet */
    0
};

/*
 * To achieve page independence, every page must in the general case
 * set page parameters. To preserve duplexing the page cannot set page
 * parameters. The following code checks the current page size and sets
 * it only if it is necessary.
 */
static const char *const psw_ps_procset[] = {
        /* <w> <h> <sizename> setpagesize - */
   "/PageSize 2 array def"
   "/setpagesize"              /* x y /a4 -> -          */
     "{ PageSize aload pop "   /* x y /a4 x0 y0         */
       "3 index eq exch",      /* x y /a4 bool x0       */
       "4 index eq and"        /* x y /a4 bool          */
         "{ pop pop pop"
         "}"
         "{ PageSize dup  1",  /* x y /a4 [  ] [  ] 1   */
           "5 -1 roll put 0 "  /* x /a4 [ y] 0          */
           "4 -1 roll put "    /* /a4                   */
           "dup null eq {false} {dup where} ifelse"
             "{ exch get exec" /* -                     */
             "}",
             "{ pop"           /* -                     */
               "/setpagedevice where",
                 "{ pop 1 dict dup /PageSize PageSize put setpagedevice"
                 "}",
                 "{ /setpage where"
                     "{ pop PageSize aload pop pageparams 3 {exch pop} repeat",
                       "setpage"
                     "}"
                   "if"
                 "}"
               "ifelse"
             "}"
           "ifelse"
         "}"
       "ifelse"
     "} bind def",
    0
};

static const char *const psw_end_prolog[] = {
    "end def",
    "%%EndResource",		/* ProcSet */
    "/pagesave null def",	/* establish binding */
    "%%EndProlog",
    0
};

/* Return true when the file is seekable.
 * On Windows NT ftell() returns some non-EOF value when used on pipes.
 */
static bool
is_seekable(gp_file *f)
{
    struct stat buf;

    if(fstat(fileno(f), &buf))
      return 0;
    return S_ISREG(buf.st_mode);
}

/*
 * Write the file header, up through the BeginProlog.  This must write to a
 * file, not a stream, because it may be called during finalization.
 */
int
psw_begin_file_header(gp_file *f, const gx_device *dev, const gs_rect *pbbox,
                      gx_device_pswrite_common_t *pdpc, bool ascii)
{
    psw_print_lines(f, (pdpc->ProduceEPS ? psw_eps_header : psw_ps_header));
    if (pbbox) {
        psw_print_bbox(f, pbbox);
        pdpc->bbox_position = 0;
    } else if (!is_seekable(f)) {	/* File is not seekable. */
        pdpc->bbox_position = -1;
        fputs("%%BoundingBox: (atend)\n", f);
        fputs("%%HiResBoundingBox: (atend)\n", f);
    } else {		/* File is seekable, leave room to rewrite bbox. */
        pdpc->bbox_position = gp_ftell(f);
        fputs("%...............................................................\n", f);
        fputs("%...............................................................\n", f);
    }
#ifdef CLUSTER
    fprintf(f, "%%%%Creator: GPL Ghostscript (%s)\n", dev->dname);
#else
    fprintf(f, "%%%%Creator: %s %ld (%s)\n", gs_product, (long)gs_revision,
            dev->dname);
#endif
    {
        time_t t;
        struct tm tms;

#ifdef CLUSTER
        memset(&t, 0, sizeof(t));
        memset(&tms, 0, sizeof(tms));
#else
        time(&t);
        tms = *localtime(&t);
#endif
        fprintf(f, "%%%%CreationDate: %d/%02d/%02d %02d:%02d:%02d\n",
                tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday,
                tms.tm_hour, tms.tm_min, tms.tm_sec);
    }
    if (ascii)
        fputs("%%DocumentData: Clean7Bit\n", f);
    if (pdpc->LanguageLevel >= 2.0)
        fprintf(f, "%%%%LanguageLevel: %d\n", (int)pdpc->LanguageLevel);
    else if (pdpc->LanguageLevel == 1.5)
        fputs("%%Extensions: CMYK\n", f);
    psw_print_lines(f, psw_begin_prolog);
#ifndef CLUSTER
    fprintf(f, "%% %s\n", gs_copyright);
#endif
    fputs("%%BeginResource: procset ", f);
    fflush(f);
    psw_print_procset_name(f, dev, pdpc);
    fprintf(f, " %5.3lf %d\n/", pdpc->ProcSet_version / 1000.0, 0);
    fflush(f);
    psw_print_procset_name(f, dev, pdpc);
    fputs(" 80 dict dup begin\n", f);
    psw_print_lines(f, psw_ps_procset);
    fflush(f);
    if (ferror(f))
        return_error(gs_error_ioerror);
    return 0;
}

/*
 * End the file header.
 */
int
psw_end_file_header(gp_file *f)
{
    return psw_print_lines(f, psw_end_prolog);
}

/*
 * End the file.
 */
int
psw_end_file(gp_file *f, const gx_device *dev,
             const gx_device_pswrite_common_t *pdpc, const gs_rect *pbbox,
             int /* should be long */ page_count)
{
    if (f == NULL)
        return 0;	/* clients should be more careful */
    fprintf(f, "%%%%Trailer\n%%%%Pages: %ld\n", (long)page_count);
    if (ferror(f))
        return_error(gs_error_ioerror);
    if (dev->PageCount > 0 && pdpc->bbox_position != 0) {
        if (pdpc->bbox_position >= 0) {
            int64_t save_pos = gp_ftell(f);

            gp_fseek(f, pdpc->bbox_position, SEEK_SET);
            /* Theoretically the bbox device should fill in the bounding box
             * but this does nothing because we don't write on the page.
             * So if bbox = 0 0 0 0, replace with the device page size.
             */
            if(pbbox->p.x == 0 && pbbox->p.y == 0
                && pbbox->q.x == 0 && pbbox->q.y == 0) {
                gs_rect bbox;
                int width = (int)(dev->width * 72.0 / dev->HWResolution[0] + 0.5);
                int height = (int)(dev->height * 72.0 / dev->HWResolution[1] + 0.5);

                bbox.p.x = 0;
                bbox.p.y = 0;
                bbox.q.x = width;
                bbox.q.y = height;
                psw_print_bbox(f, &bbox);
            } else
                psw_print_bbox(f, pbbox);
            fputc('%', f);
            if (ferror(f))
                return_error(gs_error_ioerror);
            gp_fseek(f, save_pos, SEEK_SET);
        } else
            psw_print_bbox(f, pbbox);
    }
    if (!pdpc->ProduceEPS)
        fputs("%%EOF\n", f);
    if (ferror(f))
        return_error(gs_error_ioerror);
    return 0;
}

/* ---------------- Page level ---------------- */

/*
 * Write the page header.
 */
int
psw_write_page_header(stream *s, const gx_device *dev,
                      const gx_device_pswrite_common_t *pdpc,
                      bool do_scale, int64_t page_ord, int dictsize)
{
    long page = dev->PageCount + 1;
    int width = (int)(dev->width * 72.0 / dev->HWResolution[0] + 0.5);
    int height = (int)(dev->height * 72.0 / dev->HWResolution[1] + 0.5);

    pprintld2(s, "%%%%Page: %ld %ld\n", page, page_ord);
    if (!pdpc->ProduceEPS)
        pprintld2(s, "%%%%PageBoundingBox: 0 0 %ld %ld\n", width, height);

    stream_puts(s, "%%BeginPageSetup\n");
    /*
     * Adobe's documentation says that page setup must be placed outside the
     * save/restore that encapsulates the page contents, and that the
     * showpage must be placed after the restore.  This means that to
     * achieve page independence, *every* page's setup code must include a
     * setpagedevice that sets *every* page device parameter that is changed
     * on *any* page.  Currently, the only such parameter relevant to this
     * driver is page size, but there might be more in the future.
     */
    psw_put_procset_name(s, dev, pdpc);
    stream_puts(s, " begin\n");
    if (!pdpc->ProduceEPS) {
        typedef struct ps_ {
            const char *size_name;
            int width, height;
        } page_size;
        static const page_size sizes[] = {
            {"/11x17", 792, 1224},
            {"/a3", 842, 1191},
            {"/a4", 595, 842},
            {"/b5", 501, 709},
            {"/ledger", 1224, 792},
            {"/legal", 612, 1008},
            {"/letter", 612, 792},
            {"null", 0, 0}
        };
        const page_size *p = sizes;

        while (p->size_name[0] == '/') {
            if((p->width - 5) <= width && (p->width + 5) >= width) {
                if((p->height - 5) <= height && (p->height + 5) >= height) {
                    break;
                } else
                    ++p;
            }
            else
                ++p;
        }
        pprintd2(s, "%d %d ", width, height);
        pprints1(s, "%s setpagesize\n", p->size_name);
    }
    pprintd1(s, "/pagesave save store %d dict begin\n", dictsize);
    if (do_scale)
        pprintg2(s, "%g %g scale\n",
                 72.0 / dev->HWResolution[0], 72.0 / dev->HWResolution[1]);
    stream_puts(s, "%%EndPageSetup\ngsave mark\n");
    if (s->end_status == ERRC)
        return_error(gs_error_ioerror);
    return 0;
}

/*
 * Write the page trailer.  We do this directly to the file, rather than to
 * the stream, because we may have to do it during finalization.
 */
int
psw_write_page_trailer(gp_file *f, int num_copies, int flush)
{
    fprintf(f, "cleartomark end end pagesave restore\n");
    if (num_copies != 1)
        fprintf(f, "userdict /#copies %d put\n", num_copies);
    fprintf(f, " %s\n%%%%PageTrailer\n", (flush ? "showpage" : "copypage"));
    fflush(f);
    if (ferror(f))
        return_error(gs_error_ioerror);
    return 0;
}
