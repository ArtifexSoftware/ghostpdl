/* gdevlbp3.c */
/* Canon LASER SHOT LBP-310/320 printer driver for Ghostscript
 * working on standerd/fine image mode.
 *
 * If any comment, write to naoya@mahoroba.ne.jp
 */

#include "gdevprn.h"

#define	mm_to_inch(x)	(x)/25.4

/* The device descriptor */

static dev_proc_print_page(lbp310PrintPage);
static dev_proc_print_page(lbp320PrintPage);

gx_device_printer far_data gs_lbp310_device =
        prn_device(gdev_prn_initialize_device_procs_mono,
        "lbp310",
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        600, 600,
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        1, lbp310PrintPage);

gx_device_printer far_data gs_lbp320_device =
        prn_device(gdev_prn_initialize_device_procs_mono,
        "lbp320",
        DEFAULT_WIDTH_10THS,
        DEFAULT_HEIGHT_10THS,
        600, 600,
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        mm_to_inch(5.0),
        1, lbp320PrintPage);

enum	Paper
        {
                a4, a5, postcard, b5, letter
        };

struct	bounding
        {
                enum Paper	paper;
                int		Top, Bottom, Left, Right;
        };

struct	ppi
        {
                int	w;
                int	h;
                int	id;
        };

static	const struct	ppi PaperInfo[] =
        {
                {2100, 2960, 14}, /* A4 */
                {1485, 2098, 16}, /* A5 */
                {1000, 1480, 18}, /* Postcard */
                {1820, 2570, 26}, /* B5 */
                {2100, 2790, 14}  /* Letter */
        };

static void BoundImage(gx_device_printer *, struct bounding *);
static long CompressImage(gx_device_printer *, struct bounding *, gp_file *, const char *);

static int
lbp310PrintPage(gx_device_printer *pDev, gp_file *fp)
{
        int	i;
        char	Buf[10];
        long	DataSize;
        struct	bounding	Box;

        BoundImage(pDev, &Box);

        DataSize = CompressImage(pDev, &Box, fp, "\x1b[1;%d;%d;11;%d;.r");

        /* ----==== Set size ====---- */
        gs_snprintf(Buf, sizeof(Buf), "0%ld", DataSize);
        i = (DataSize+strlen(Buf)+1)&1;
        /* ----==== escape to LIPS ====---- */
        gp_fprintf(fp, "\x80%s\x80\x80\x80\x80\x0c",Buf+i);
        gp_fprintf(fp, "\x1bP0J\x1b\\");

        return(0);
}

static int
lbp320PrintPage(gx_device_printer *pDev, gp_file *fp)
{
        int	i;
        char	Buf[16];
        long	DataSize;
        struct	bounding	Box;

        BoundImage(pDev, &Box);

        /* ----==== fix bounding box 4-byte align ====---- */
        Box.Left &= ~1;
        Box.Right |= 1;

        /* ----==== JOB start ??? ====---- */
        gp_fprintf(fp, "\x1b%%-12345X@PJL CJLMODE\n@PJL JOB\n");

        DataSize = CompressImage(pDev, &Box, fp, "\x1b[1;%d;%d;11;%d;.&r");

        /* ----==== Set size ====---- */
        gs_snprintf(Buf, sizeof(Buf), "000%ld", DataSize);
        i = (DataSize+strlen(Buf)+1)&3;
        /* ----==== escape to LIPS ====---- */
        gp_fprintf(fp, "\x80%s\x80\x80\x80\x80\x0c",Buf+i);
        gp_fprintf(fp, "\x1bP0J\x1b\\");
        gp_fprintf(fp, "\x1b%%-12345X@PJL CJLMODE\n@PJL EOJ\n\x1b%%-12345X");

        return(0);
}

static void
BoundImage(gx_device_printer *pDev, struct bounding *pBox)
{
        int	x, y, flag;
        int	LineSize = gdev_mem_bytes_per_scan_line((gx_device *)pDev);
        int	Xsize, Ysize, Pt, Pb, Pl, Pr;
        int	Xres = (int)pDev->x_pixels_per_inch,
                Yres = (int)pDev->y_pixels_per_inch,
                height = pDev->height;
        byte	*Buf;
        enum	Paper	paper;

        /* ----==== Check parameters ====---- */
        paper = height*10/Yres < 82 ? postcard :\
                height*10/Yres < 98 ? a5 :\
                height*10/Yres < 109 ? b5 :\
                height*10/Yres < 116 ? letter : a4;
        Xsize = (int)(Xres * mm_to_inch(PaperInfo[paper].w-100) / 160);
        Ysize = (int)(Yres * mm_to_inch(PaperInfo[paper].h-100) / 10);
        /* ----==== Allocate momory ====---- */
        if (LineSize < Xsize*2+1) {
                LineSize = Xsize*2+1;
        }
        Buf = (byte *)gs_malloc(pDev->memory->non_gc_memory, 1, LineSize, "LineBuffer");
        /* ----==== bounding image ====---- */
        Pt = Pb = Pl = Pr = -1;
        for(y=0 ; y<height && y<Ysize ; y++){
                flag = 0;
                gdev_prn_copy_scan_lines(pDev, y, Buf, LineSize);
                for (x=0 ; x<min(LineSize/2, Xsize) ; x++) {
                        if (*(Buf+x*2) || *(Buf+x*2+1)) {
                                if (Pl == -1 || Pl > x) {
                                        Pl = x;
                                }
                                if (Pr < x) {
                                        Pr = x;
                                }
                                flag = 1;
                        }
                }
                if (flag) {
                        if (Pt == -1) {
                                Pt = y;
                        }
                        Pb = y;
                }
        }
        pBox->paper = paper;
        pBox->Top = Pt;
        pBox->Bottom = Pb;
        pBox->Left = Pl;
        pBox->Right = Pr;
        gs_free(pDev->memory->non_gc_memory, Buf, 1, LineSize, "LineBuffer");
}

static long
CompressImage(gx_device_printer *pDev, struct bounding *pBox, gp_file *fp, const char *format)
{
        int	x, y, i, count = 255;
        int	Xres = (int)pDev->x_pixels_per_inch;
        int	LineSize = gdev_mem_bytes_per_scan_line((gx_device *)pDev);
        byte	*Buf, oBuf[128], c_prev = 0, c_cur, c_tmp;
        long	DataSize = 0;

        /* ----==== Printer initialize ====---- */
        /* ----==== start TEXT mode ====---- */
        gp_fprintf(fp, "\x1b%%@");
        /* ----==== job start ====---- */
        gp_fprintf(fp, "\x1bP35;%d;1J;GhostScript\x1b\\", Xres);
        /* ----==== soft reset ====---- */
        gp_fprintf(fp, "\x1b<");
        /* ----==== select size as dot ====---- */
        gp_fprintf(fp, "\x1b[7 I");
        /* ----==== ??? ====---- */
        gp_fprintf(fp, "\x1b[;1;'v");
        /* ----==== set paper size ====---- */
        gp_fprintf(fp, "\x1b[%d;;p", PaperInfo[pBox->paper].id);
        /* ----==== select sheet feeder ====---- */
        gp_fprintf(fp, "\x1b[1q");
        /* ----==== disable automatic FF ====---- */
        gp_fprintf(fp, "\x1b[?2h");
        /* ----==== set number of copies ====---- */
        gp_fprintf(fp, "\x1b[%dv", 1);
        /* ----==== move CAP location ====---- */
        gp_fprintf(fp, "\x1b[%d;%df", pBox->Top, pBox->Left*16);
        /* ----==== draw raster image ====---- */
        gp_fprintf(fp, format, pBox->Right-pBox->Left+1,
                   Xres, pBox->Bottom-pBox->Top+1);

        /* ----==== Allocate momory ====---- */
        Buf = (byte *)gs_malloc(pDev->memory->non_gc_memory, 1, LineSize, "LineBuffer");
        /* ----==== transfer raster image ====---- */
        for (y=pBox->Top ; y<=pBox->Bottom ; y++) {
                gdev_prn_copy_scan_lines(pDev, y, Buf, LineSize);
                for (x=pBox->Left*2 ; x<=pBox->Right*2+1 ; x++) {
                /* ----==== check pointer & Reverse bit order ====---- */
                        c_cur = 0;
                        if (x<LineSize) {
                                c_tmp = *(Buf+x);
                                for (i=0 ; i<8 ; i++) {
                                        c_cur = (c_cur << 1) | (c_tmp & 1);
                                        c_tmp = c_tmp >> 1;
                                }
                        }
                        /* ----==== Compress data ====---- */
                        if (count < 0) {
                                if (c_prev == c_cur && count > -127) {
                                        count--;
                                        continue;
                                } else {
                                        gp_fprintf(fp, "%c%c", count, c_prev);
                                        DataSize += 2;
                                }
                        } else if (count == 0) {
                                if (c_prev == c_cur) {
                                        count--;
                                } else {
                                        count++;
                                        c_prev = *(oBuf+count) = c_cur;
                                }
                        continue;
                        } else if (count < 127) {
                                if (c_prev == c_cur) {
                                        gp_fprintf(fp, "%c", count-1);
                                        gp_fwrite(oBuf, 1, count, fp);
                                        DataSize += (count+1);
                                        count = -1;
                                } else {
                                        count++;
                                        c_prev = *(oBuf+count) = c_cur;
                                }
                        continue;
                        } else if (count == 127) {
                                gp_fprintf(fp, "%c", count);
                                gp_fwrite(oBuf, 1, count+1, fp);
                                DataSize += (count+2);
                        }
                        c_prev = *oBuf = c_cur;
                        count = 0;
                }
        }

        /* ----==== flush data ====---- */
        if (count < 0) {
                gp_fprintf(fp, "%c%c", count, c_prev);
                DataSize += 2;
        } else {
                gp_fprintf(fp, "%c", count);
                gp_fwrite(oBuf, 1, count+1, fp);
                DataSize += (count+2);
        }

        gs_free(pDev->memory->non_gc_memory, Buf, 1, LineSize, "LineBuffer");
        return(DataSize);
}
