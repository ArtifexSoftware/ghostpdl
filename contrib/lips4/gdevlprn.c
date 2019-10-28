/* Copyright (C) 1999 Norihito Ohmori.

   Ghostscript laser printer driver supporting Library.

   This software is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
   to anyone for the consequences of using it or for whether it serves any
   particular purpose or works at all, unless he says so in writing.  Refer
   to the GNU General Public License for full details.

   Everyone is granted permission to copy, modify and redistribute
   this software, but only under the conditions described in the GNU
   General Public License.  A copy of this license is supposed to have been
   given to you along with this software so you can know your rights and
   responsibilities.  It should be in a file named COPYING.  Among other
   things, the copyright notice and this notice must be preserved on all
   copies.
 */

/*$Id: gdevlprn.c,v 1.3 2002/07/20 21:03:21 tillkamppeter Exp $ */
/* Library for Laser Printer Driver */

/*
   Main part of this library was taked from Hiroshi Narimatsu's
   Ghostscript driver, epag-3.08.
   <ftp://ftp.humblesoft.com/pub/epag-3.08.tar.gz>
 */

/*
   ************************
   * This Library Options *
   ************************

   -dTumble         : for Duplex
   true: short edge binding, false: long edge binding
   the default is false.
   -dBlockLine=number      : number of line to make output area.
   -dBlockWidth=number   : rectangle width for output area.
   -dBlockHeight=number  : rectangle height for output area.
   -dShowBubble          : Draw rectangles, output area.
 */

#include "gdevlprn.h"

/* ------ Get/put parameters ------ */

/* Get parameters.  Laser Printer devices add several more parameters */
/* to the default Printer Device set. */
int
lprn_get_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) dev;
    int code = gdev_prn_get_params(dev, plist);
    int ncode;

    if (code < 0)
        return code;

    if ((ncode = param_write_bool(plist, "ManualFeed", &lprn->ManualFeed)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, "NegativePrint", &lprn->NegativePrint)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, "Tumble", &lprn->Tumble)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, "RITOff", &lprn->RITOff)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, "BlockLine", &lprn->BlockLine)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, "BlockWidth", &lprn->nBw)) < 0)
        code = ncode;

    if ((ncode = param_write_int(plist, "BlockHeight", &lprn->nBh)) < 0)
        code = ncode;

    if ((ncode = param_write_bool(plist, "ShowBubble", &lprn->ShowBubble)) < 0)
        code = ncode;

    return code;
}

/* Put properties. */
int
lprn_put_params(gx_device * dev, gs_param_list * plist)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) dev;
    int ecode = 0;
    int code;
    gs_param_name param_name;
    bool ManualFeed = lprn->ManualFeed;
    bool NegativePrint = lprn->NegativePrint;
    bool Tumble = lprn->Tumble;
    bool RITOff = lprn->RITOff;
    int BlockLine = lprn->BlockLine;
    int BlockWidth = lprn->nBw;
    int BlockHeight = lprn->nBh;
    bool ShowBubble = lprn->ShowBubble;

    if ((code = param_read_bool(plist,
                                (param_name = "ManualFeed"),
                                &ManualFeed)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }

    if ((code = param_read_bool(plist,
                                (param_name = "NegativePrint"),
                                &NegativePrint)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }
    if ((code = param_read_bool(plist,
                                (param_name = "Tumble"),
                                &Tumble)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }
    if ((code = param_read_bool(plist,
                                (param_name = "RITOff"),
                                &RITOff)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }
    switch (code = param_read_int(plist,
                                  (param_name = "BlockWidth"),
                                  &BlockWidth)) {
        case 0:
            if (BlockWidth < 0)
                ecode = gs_error_rangecheck;
            else
                break;
            goto bwidthe;
        default:
            ecode = code;
          bwidthe:param_signal_error(plist, param_name, ecode = code);
        case 1:
            break;
    }

    switch (code = param_read_int(plist,
                                  (param_name = "BlockLine"),
                                  &BlockLine)) {
        case 0:
            if (BlockLine < 0)
                ecode = gs_error_rangecheck;
            else
                break;
            goto crowe;
        default:
            ecode = code;
          crowe:param_signal_error(plist, param_name, ecode = code);
        case 1:
            break;
    }

    switch (code = param_read_int(plist,
                                  (param_name = "BlockHeight"),
                                  &BlockHeight)) {
        case 0:
            if (BlockHeight < 0)
                ecode = gs_error_rangecheck;
            else
                break;
            goto bheighte;
        default:
            ecode = code;
          bheighte:param_signal_error(plist, param_name, ecode = code);
        case 1:
            break;
    }

    if ((code = param_read_bool(plist,
                                (param_name = "ShowBubble"),
                                &ShowBubble)) < 0) {
        param_signal_error(plist, param_name, ecode = code);
    }
    if (ecode < 0)
        return ecode;
    code = gdev_prn_put_params(dev, plist);
    if (code < 0)
        return code;

    lprn->ManualFeed = ManualFeed;
    lprn->NegativePrint = NegativePrint;
    lprn->Tumble = Tumble;
    lprn->RITOff = RITOff;
    lprn->BlockLine = BlockLine;
    lprn->nBw = BlockWidth;
    lprn->nBh = BlockHeight;
    lprn->ShowBubble = ShowBubble;

    return 0;
}

static void lprn_bubble_flush_all(gx_device_printer * pdev, gp_file * fp);
static void lprn_process_line(gx_device_printer * pdev, gp_file * fp, int r, int h);
static void lprn_bubble_flush(gx_device_printer * pdev, gp_file * fp, Bubble * bbl);
static int lprn_is_black(gx_device_printer * pdev, int r, int h, int bx);
static void lprn_bubble_gen(gx_device_printer * pdev, int x0, int x1, int y0, int y1);
static void lprn_rect_add(gx_device_printer * pdev, gp_file * fp, int r, int h, int start, int end);

int
lprn_print_image(gx_device_printer * pdev, gp_file * fp)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int y;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    Bubble *bbtbl;
    Bubble *bbl;
    int i;
    int ri, rmin, read_y;
    int code = 0;
    Bubble *bubbleBuffer;
    int maxBx, maxBy, maxY;
    int start_y_block = 0;	/* start of data in buffer */
    int num_y_blocks = 0;	/* number of data line [r:r+h-1] */

    maxBx = (bpl + lprn->nBw - 1) / lprn->nBw;
    maxBy = (pdev->height + lprn->nBh - 1) / lprn->nBh;
    maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    if (!(lprn->ImageBuf = gs_malloc(pdev->memory->non_gc_memory, bpl, maxY, "lprn_print_image(ImageBuf)")))
        return_error(gs_error_VMerror);
    if (!(lprn->TmpBuf = gs_malloc(pdev->memory->non_gc_memory, bpl, maxY, "lprn_print_iamge(TmpBuf)")))
        return_error(gs_error_VMerror);
    if (!(lprn->bubbleTbl = gs_malloc(pdev->memory->non_gc_memory, sizeof(Bubble *), maxBx, "lprn_print_image(bubbleTbl)")))
        return_error(gs_error_VMerror);
    if (!(bubbleBuffer = gs_malloc(pdev->memory->non_gc_memory, sizeof(Bubble), maxBx, "lprn_print_image(bubbleBuffer)")))
        return_error(gs_error_VMerror);

    for (i = 0; i < maxBx; i++)
        lprn->bubbleTbl[i] = NULL;
    bbtbl = bubbleBuffer;

    for (i = 0; i < maxBx - 1; i++)
        bbtbl[i].next = &bbtbl[i + 1];
    bbtbl[i].next = NULL;
    lprn->freeBubbleList = &bbtbl[0];

    for (y = 0; y < maxBy; y++) {
        if (num_y_blocks + lprn->nBh > maxY) {	/* bubble flush for next data */
            rmin = start_y_block + lprn->nBh;	/* process the data under rmin */
            for (i = 0; i < maxBx; i++) {
                bbl = lprn->bubbleTbl[i];
                if (bbl != NULL && bbl->brect.p.y < rmin)
                    lprn_bubble_flush(pdev, fp, bbl);
            }
            num_y_blocks -= lprn->nBh;	/* data if keeps in [r:r+h] */
            start_y_block += lprn->nBh;

        }
        ri = start_y_block + num_y_blocks;	/* read position */
        read_y = ri % maxY;	/* end of read position */
        code = gdev_prn_copy_scan_lines(pdev, ri, lprn->ImageBuf + bpl * read_y, bpl * lprn->nBh);
        if (code < 0)
            return code;

        num_y_blocks += lprn->nBh;

        lprn_process_line(pdev, fp, start_y_block, num_y_blocks);
    }
    lprn_bubble_flush_all(pdev, fp);	/* flush the rest of bubble */

    gs_free(pdev->memory->non_gc_memory, lprn->ImageBuf, bpl, maxY, "lprn_print_image(ImageBuf)");
    gs_free(pdev->memory->non_gc_memory, lprn->TmpBuf, bpl, maxY, "lprn_print_iamge(TmpBuf)");
    gs_free(pdev->memory->non_gc_memory, lprn->bubbleTbl, sizeof(Bubble *), maxBx, "lprn_print_image(bubbleTbl)");
    gs_free(pdev->memory->non_gc_memory, bubbleBuffer, sizeof(Bubble), maxBx, "lprn_print_image(bubbleBuffer)");

    return code;
}

/*
 * epag_bubble_flush_all: Output the rect of bubble.
 */
static void
lprn_bubble_flush_all(gx_device_printer * pdev, gp_file * fp)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int i = 0;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxBx = (bpl + lprn->nBw - 1) / lprn->nBw;

    for (i = 0; i < maxBx; i++) {
        if (lprn->bubbleTbl[i] != NULL) {
            lprn_bubble_flush(pdev, fp, lprn->bubbleTbl[i]);
        } else
            break;
    }
}

/*
 *    Process bh lines raster data.
 */
static void
lprn_process_line(gx_device_printer * pdev, gp_file * fp, int r, int h)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    int bx, bInBlack, bBlack, start;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int maxBx = (bpl + lprn->nBw - 1) / lprn->nBw;

    bInBlack = 0;
    for (bx = 0; bx < maxBx; bx++) {
        bBlack = lprn_is_black(pdev, r, h, bx);
        if (!bInBlack) {
            if (bBlack) {
                start = bx;
                bInBlack = 1;
            }
        } else {
            if (!bBlack) {
                bInBlack = 0;
                lprn_rect_add(pdev, fp, r, h, start, bx);
            }
        }
    }
    if (bInBlack)
        lprn_rect_add(pdev, fp, r, h, start, bx - 1);
}

/*   Search the bx line to make output */
static int
lprn_is_black(gx_device_printer * pdev, int r, int h, int bx)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    int bh = lprn->nBh;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int x, y, y0;
    byte *p;
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    y0 = (r + h - bh) % maxY;
    for (y = 0; y < bh; y++) {
        p = &lprn->ImageBuf[(y0 + y) * bpl + bx * lprn->nBw];
        for (x = 0; x < lprn->nBw; x++) {
            /* bpl isn't necessarily a multiple of lprn->nBw, so
            we need to explicitly stop after the last byte in this
            line to avoid accessing either the next line's data or
            going off the end of our buffer completely. This avoids
            https://bugs.ghostscript.com/show_bug.cgi?id=701785. */
            if (bx * lprn->nBw + x >= bpl)  break;
            if (p[x] != 0)
                return 1;
        }
    }
    return 0;
}

static void
lprn_rect_add(gx_device_printer * pdev, gp_file * fp, int r, int h, int start, int end)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    int x0 = start * lprn->nBw;
    int x1 = end * lprn->nBw - 1;
    int y0 = r + h - lprn->nBh;
    int y1 = y0 + lprn->nBh - 1;
    int i;
    Bubble *bbl;

    if ((bbl = lprn->bubbleTbl[start]) != NULL &&
        bbl->brect.q.y == y0 - 1 &&
        bbl->brect.p.x == x0 &&
        bbl->brect.q.x == x1) {
        bbl->brect.q.y = y1;
    } else {
        for (i = start; i <= end; i++)
            if (lprn->bubbleTbl[i] != NULL)
                lprn_bubble_flush(pdev, fp, lprn->bubbleTbl[i]);
        lprn_bubble_gen(pdev, x0, x1, y0, y1);
    }
}

static void
lprn_bubble_gen(gx_device_printer * pdev, int x0, int x1, int y0, int y1)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;

    Bubble *bbl;
    int bx0, bx1, bx;

    assert(lprn->freeBubbleList != NULL);

    bbl = lprn->freeBubbleList;
    lprn->freeBubbleList = bbl->next;

    bbl->brect.p.x = x0;
    bbl->brect.q.x = x1;
    bbl->brect.p.y = y0;
    bbl->brect.q.y = y1;

    bx0 = x0 / lprn->nBw;
    bx1 = (x1 + lprn->nBw - 1) / lprn->nBw;

    for (bx = bx0; bx <= bx1; bx++) {
        assert(lprn->bubbleTbl[bx] == NULL);
        lprn->bubbleTbl[bx] = bbl;
    }
}

/* make output */
void
lprn_bubble_flush(gx_device_printer * pdev, gp_file * fp, Bubble * bbl)
{
    gx_device_lprn *const lprn = (gx_device_lprn *) pdev;
    int i, j, bx;
    byte *p;
    int bx0 = bbl->brect.p.x / lprn->nBw;
    int bx1 = (bbl->brect.q.x + lprn->nBw - 1) / lprn->nBw;
    int bpl = gdev_mem_bytes_per_scan_line(pdev);
    int x = bbl->brect.p.x * 8;
    int y = bbl->brect.p.y;
    int width = (bbl->brect.q.x - bbl->brect.p.x + 1) * 8;
    int height = bbl->brect.q.y - bbl->brect.p.y + 1;
    int maxY = lprn->BlockLine / lprn->nBh * lprn->nBh;

    for (i = 0; i < height; i++) {
        p = lprn->ImageBuf + ((i + y) % maxY) * bpl;
        for (j = 0; j < width / 8; j++) {
            if (lprn->NegativePrint)
                *(lprn->TmpBuf + i * width / 8 + j) = ~*(p + j + bbl->brect.p.x);
            else
                *(lprn->TmpBuf + i * width / 8 + j) = *(p + j + bbl->brect.p.x);
        }
    }

    (*lprn->image_out) (pdev, fp, x, y, width, height);

    /* Initialize bubbleTbl */
    for (bx = bx0; bx <= bx1; bx++) {
        assert(lprn->bubbleTbl[bx] == bbl);
        lprn->bubbleTbl[bx] = NULL;
    }

    bbl->next = lprn->freeBubbleList;
    lprn->freeBubbleList = bbl;
}
