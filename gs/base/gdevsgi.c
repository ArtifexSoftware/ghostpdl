/*
 * This file is distributed with Ghostscript, but its author,
 * Tanmoy Bhattacharya (tanmoy@qcd.lanl.gov) hereby places it in the
 * public domain.
 */

/* $Id$*/
/* SGI raster file driver */
#include "gdevprn.h"
#include "gdevsgi.h"

#define X_DPI 72
#define Y_DPI 72

#define sgi_prn_device(procs, dev_name, num_comp, depth, max_gray, max_color, print_page)\
{prn_device_body(gx_device_printer, procs, dev_name, \
		 DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS, X_DPI, Y_DPI, \
		 0, 0, 0, 0, \
		 num_comp, depth, max_gray, max_color, max_gray+1, max_color+1, \
		 print_page)}

static dev_proc_map_rgb_color(sgi_map_rgb_color);
static dev_proc_map_color_rgb(sgi_map_color_rgb);

static dev_proc_print_page(sgi_print_page);

static gx_device_procs sgi_procs =
  prn_color_procs(gdev_prn_open, gdev_prn_output_page, gdev_prn_close,
		  sgi_map_rgb_color, sgi_map_color_rgb);

const gx_device_printer far_data gs_sgirgb_device =
  sgi_prn_device(sgi_procs, "sgirgb", 3, 24, 255, 255, sgi_print_page);

static gx_color_index
sgi_map_rgb_color(gx_device * dev, const ushort cv[])
{      ushort bitspercolor = dev->color_info.depth / 3;
       ulong max_value = (1 << bitspercolor) - 1;
       ushort red, green, blue;
       red = cv[0]; green = cv[1]; blue = cv[2];

       return ((red*max_value / gx_max_color_value) << (bitspercolor * 2)) +
	      ((green*max_value / gx_max_color_value) << bitspercolor) +
	      (blue*max_value / gx_max_color_value);
}

static int
sgi_map_color_rgb(gx_device *dev, gx_color_index color, ushort prgb[3])
{	ushort bitspercolor = dev->color_info.depth / 3;
	ushort colormask = (1 << bitspercolor) - 1;

	prgb[0] = (ushort)(((color >> (bitspercolor * 2)) & colormask) *
		(ulong)gx_max_color_value / colormask);
	prgb[1] = (ushort)(((color >> bitspercolor) & colormask) *
		(ulong)gx_max_color_value / colormask);
	prgb[2] = (ushort)((color & colormask) *
		(ulong)gx_max_color_value / colormask);
	return 0;
}

typedef struct sgi_cursor_s {
  gx_device_printer *dev;
  int bpp;
  uint line_size;
  byte *data;
  int lnum;
} sgi_cursor;

/* write a short int to disk as big-endean */
static int putshort(unsigned int val, FILE *outf)
{
	unsigned char buf[2];

	buf[0] = (val>>8);
	buf[1] = val;
	return fwrite(buf,2,1,outf);
}

/* write an int to disk as big-endean */
static int putint(unsigned int val, FILE *outf)
{
	unsigned char buf[4];

	buf[0] = (val>>24);
	buf[1] = (val>>16);
	buf[2] = (val>>8);
	buf[3] = val;
	return fwrite(buf,4,1,outf);
}

/* write the header field by field in big-endian */
static void putheader(IMAGE *header, FILE *outf)
{
	int i;
	char filler= '\0';

	putshort(header->imagic, outf);
	fputc(1, outf); /* RLE */
	fputc(1, outf); /* bpp */
	putshort(header->dim, outf);
	putshort(header->xsize, outf);
	putshort(header->ysize, outf);
	putshort(header->zsize, outf);

	putint(header->min_color, outf);
	putint(header->max_color, outf);
	putint(header->wastebytes, outf);

	fwrite(header->name,80,1,outf);

	putint(header->colormap, outf);

   /* put the filler for the rest */
    for (i=0; i<404; i++)
        fputc(filler,outf);
 }

static int
sgi_begin_page(gx_device_printer *bdev, FILE *pstream, sgi_cursor *pcur)
{
     uint line_size;
     byte *data;
     IMAGE *header;

     if (bdev->PageCount >= 1 && !bdev->file_is_new) { /* support single page only */
          emprintf(bdev->memory,
                   "sgi rgb format only supports one page per file.\n"
                   "Please use the '%%d' OutputFile option to create one file for each page.\n");
	  return_error(gs_error_rangecheck);
     }
     line_size = gdev_mem_bytes_per_scan_line((gx_device_printer*)bdev);
     data = (byte*)gs_malloc(bdev->memory, line_size, 1, "sgi_begin_page");
     header= (IMAGE*)gs_malloc(bdev->memory, sizeof(IMAGE),1,"sgi_begin_page");

     if ((data == (byte*)0)||(header == (IMAGE*)0)) {
        gs_free(bdev->memory, data, line_size, 1, "sgi_begin_page");
        gs_free(bdev->memory, header, sizeof(IMAGE),1,"sgi_begin_page");
	return_error(gs_error_VMerror);
     }
     memset(header,0, sizeof(IMAGE));
     header->imagic = IMAGIC;
     header->type = RLE(1);
     header->dim = 3;
     header->xsize=bdev->width;
     header->ysize=bdev->height;
     header->zsize=3;
     header->min_color  = 0;
     header->max_color  = bdev->color_info.max_color;
     header->wastebytes = 0;
     strncpy(header->name,"gs picture",80);
     header->colormap = CM_NORMAL;
     header->dorev=0;
     putheader(header,pstream);
     pcur->dev = bdev;
     pcur->bpp = bdev->color_info.depth;
     pcur->line_size = line_size;
     pcur->data = data;
     return 0;
}

static int
sgi_next_row(sgi_cursor *pcur)
{    if (pcur->lnum < 0)
       return 1;
     gdev_prn_copy_scan_lines((gx_device_printer*)pcur->dev,
			      pcur->lnum--, pcur->data, pcur->line_size);
     return 0;
}

#define bdev ((gx_device_printer *)pdev)

static int
sgi_print_page(gx_device_printer *pdev, FILE *pstream)
{      sgi_cursor cur;
       int code = sgi_begin_page(bdev, pstream, &cur);
       uint bpe, mask;
       int separation;
       int *rowsizes;
       byte *edata ;
       long lastval;
       int rownumber;

       if (pdev->PageCount >= 1 && !pdev->file_is_new)
	  return_error(gs_error_rangecheck);  /* support single page only, can't happen */

#define aref2(a,b) (a)*bdev->height+(b)
	   rowsizes=(int*)gs_malloc(pdev->memory, sizeof(int),3*bdev->height,"sgi_print_page");
       edata =  (byte*)gs_malloc(pdev->memory, cur.line_size, 1, "sgi_begin_page");

       if((code<0)||(rowsizes==(int*)NULL)||(edata==(byte*)NULL)) {
           code = gs_note_error(gs_error_VMerror);
           goto free_mem;
       }

       lastval = 512+4*6*bdev->height; /* skip offset table */
       fseek(pstream,lastval,0);
       for (separation=0; separation < 3; separation++)
	 {
	   cur.lnum = cur.dev->height-1;
	   rownumber = 0;
	   bpe = cur.bpp/3;
	   mask = (1<<bpe) - 1;
	   while ( !(code=sgi_next_row(&cur)))
	     { byte *bp;
	       uint x;
	       int shift;
	       byte *curcol=cur.data;
	       byte *startcol=edata;
	       int count;
	       byte todo, cc;
	       byte *iptr, *sptr, *optr, *ibufend;
	       for (bp = cur.data, x=0, shift = 8 - cur.bpp;
		    x < bdev->width;
		    )
		 { uint pixel = 0;
		   uint r, g, b;
		   switch (cur.bpp >> 3)
		     {
		     case 3: pixel = (uint)*bp << 16; bp++;
		     case 2: pixel += (uint)*bp << 8; bp++;
		     case 1: pixel += *bp; bp++; break;
		     case 0: pixel = *bp >> shift;
		       if ((shift-=cur.bpp) < 0)
			 bp++, shift += 8; break;
		     }
		   ++x;
		   b = pixel & mask; pixel >>= bpe;
		   g = pixel & mask; pixel >>= bpe;
		   r = pixel & mask;
		   switch(separation)
		     {
		     case 0: *curcol++=r; break;
		     case 1: *curcol++=g; break;
		     case 2: *curcol++=b; break;
		     }
		 }
	       iptr=cur.data;
	       optr=startcol;
	       ibufend=curcol-1;
	       while(iptr<ibufend) {
		 sptr = iptr;						
		 iptr += 2;
		 while((iptr<ibufend)&&((iptr[-2]!=iptr[-1])||(iptr[-1]!=iptr[0])))
		   iptr++;
		 iptr -= 2;
		 count = iptr-sptr;
		 while(count) {
		   todo = count>126 ? 126:count;
		   count -= todo;
		   *optr++ = 0x80|todo;
		   while(todo--)
		     *optr++ = *sptr++;
		 }
		 sptr = iptr;
		 cc = *iptr++;
		 while( (iptr<ibufend) && (*iptr == cc) )
		   iptr++;
		 count = iptr-sptr;
		 while(count) {
		   todo = count>126 ? 126:count;
		   count -= todo;
		   *optr++ = todo;
		   *optr++ = cc;
		 }
	       }
               *optr++ = 0;
	       rowsizes[aref2(separation,rownumber++)] = optr-startcol;
	       if (fwrite(startcol,1,optr-startcol,pstream) != optr-startcol) {
                   code = gs_note_error(gs_error_ioerror);
                   goto free_mem;
               }
	     }
	 }
       fseek(pstream,512L,0);
       for(separation=0; separation<3; separation++)
	 for(rownumber=0; rownumber<bdev->height; rownumber++)
	   {putint(lastval,pstream);
	    lastval+=rowsizes[aref2(separation,rownumber)];}
       for(separation=0; separation<3; separation++)
	 for(rownumber=0; rownumber<bdev->height; rownumber++)
	   {lastval=rowsizes[aref2(separation,rownumber)];
	    putint(lastval,pstream);}
     free_mem:
       gs_free(pdev->memory, (char*)cur.data, cur.line_size, 1,
		 "sgi_print_page(done)");
       gs_free(pdev->memory, (char*)edata, cur.line_size, 1, "sgi_print_page(done)");
       gs_free(pdev->memory, (char*)rowsizes,4,3*bdev->height,"sgi_print_page(done)");
       return (code < 0 ? code : 0);
}
