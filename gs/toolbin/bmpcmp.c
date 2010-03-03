/*
 * bmpcmp.c: BMP Comparison - utility for use with htmldiff.pl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#define MINX (300)
#define MINY (320)
#define MAXX (600)
#define MAXY (960)

typedef struct {
    int xmin;
    int ymin;
    int xmax;
    int ymax;
} BBox;

typedef struct ImageReader
{
  FILE *file;
  void *(*read)(struct ImageReader *, int *w, int *h, int *s, int *bpp);
} ImageReader;

static void *Malloc(size_t size) {
    void *block;
    
    block = malloc(size);
    if (block == NULL) {
        fprintf(stderr, "Failed to malloc %u bytes\n", (unsigned int)size);
        exit(EXIT_FAILURE);
    }
    return block;
}

static void putword(unsigned char *buf, int word) {
  buf[0] = word;
  buf[1] = (word>>8);
}

static int getdword(unsigned char *bmp) {

  return bmp[0]+(bmp[1]<<8)+(bmp[2]<<16)+(bmp[3]<<24);
}

static int getword(unsigned char *bmp) {

  return bmp[0]+(bmp[1]<<8);
}

static void putdword(unsigned char *bmp, int i) {

  bmp[0] = i & 0xFF;
  bmp[1] = (i>>8) & 0xFF;
  bmp[2] = (i>>16) & 0xFF;
  bmp[3] = (i>>24) & 0xFF;
}

static unsigned char *bmp_load_sub(unsigned char *bmp,
                                   int           *width_ret,
                                   int           *height_ret,
                                   int           *span,
                                   int           *bpp,
                                   int            image_offset,
                                   int            filelen)
{
  int size, src_bpp, dst_bpp, comp, xdpi, ydpi, i, x, y, cols;
  int masks, redmask, greenmask, bluemask, col, col2;
  int width, byte_width, word_width, height;
  unsigned char *palette;
  unsigned char *src, *dst;
  int           *dst2;
  short         *dst3;
  int            pal[256];

  size = getdword(bmp);
  if (size == 12) {
    /* Windows v2 - thats fine */
  } else if (size == 40) {
    /* Windows v3 - also fine */
  } else if (size == 108) {
    /* Windows v4 - also fine */
  } else {
    /* Windows 8004 - god knows. Give up */
    return NULL;
  }
  if (size == 12) {
    width  = getword(bmp+4);
    height = getword(bmp+6);
    i      = getword(bmp+8);
    if (i != 1) {
      /* Single plane images only! */
      return NULL;
    }
    src_bpp = getword(bmp+10);
    comp    = 0;
    xdpi    = 90;
    ydpi    = 90;
    cols    = 1<<src_bpp;
    masks   = 0;
    palette = bmp+14;
  } else {
    width  = getdword(bmp+4);
    height = getdword(bmp+8);
    i      = getword(bmp+12);
    if (i != 1) {
      /* Single plane images only! */
      return NULL;
    }
    src_bpp = getword(bmp+14);
    comp    = getdword(bmp+16);
    cols    = getdword(bmp+32);
    if (comp == 3) {
      /* Windows NT */
      redmask = getdword(bmp+40);
      greenmask = getdword(bmp+44);
      bluemask = getdword(bmp+48);
      masks = 1;
      palette = bmp+52;
    } else {
      if (size == 108) {
        /* Windows 95 */
        redmask   = getdword(bmp+40);
        greenmask = getdword(bmp+44);
        bluemask  = getdword(bmp+48);
        /* ColorSpace rubbish ignored for now */
        masks = 1;
        palette = bmp+108;
      } else {
        masks = 0;
        palette = bmp+40;
      }
    }
  }

  dst_bpp = src_bpp;
  if (src_bpp == 24)
      src_bpp = 32;
  if (dst_bpp <= 8)
      dst_bpp = 32;

  /* Read the palette */
  if (src_bpp <= 8) {
    for (i = 0; i < (1<<dst_bpp); i++) {
      col = getdword(palette+i*4);
      col2  = (col & 0x0000FF)<<24;
      col2 |= (col & 0x00FF00)<<8;
      col2 |= (col & 0xFF0000)>>8;
      pal[i] = col2;
    }
  }
  if (image_offset <= 0) {
    src = palette + (1<<src_bpp)*4;
  } else {
    src = bmp + image_offset;
  }

  byte_width  = (width+7)>>3;
  word_width  = width * ((dst_bpp+7)>>3);
  word_width += 3;
  word_width &= ~3;

  dst = Malloc(word_width * height);

  /* Now we do the actual conversion */
  if (comp == 0) {
    switch (src_bpp) {
      case 1:
        for (y = height-1; y >= 0; y--) {
          dst2 = (int *)dst;
          for (x = 0; x < byte_width; x++) {
            i  = *src++;
            *dst2++ = pal[(i   ) & 1];
            *dst2++ = pal[(i>>1) & 1];
            *dst2++ = pal[(i>>2) & 1];
            *dst2++ = pal[(i>>3) & 1];
            *dst2++ = pal[(i>>4) & 1];
            *dst2++ = pal[(i>>5) & 1];
            *dst2++ = pal[(i>>6) & 1];
            *dst2++ = pal[(i>>7) & 1];
          }
          while (x & 3) {
            src++;
            x += 1;
          }
          dst += word_width;
        }
        break;
      case 4:
        for (y = height-1; y >= 0; y--) {
          dst2 = (int *)dst;
          for (x = 0; x < byte_width; x++) {
            i  = *src++;
            *dst2++ = pal[(i   ) & 0xF];
            *dst2++ = pal[(i>>4) & 0xF];
          }
          while (x & 3) {
            src++;
            x += 1;
          }
          dst += word_width;
        }
        break;
      case 8:
        for (y = height-1; y >= 0; y--) {
          dst2 = (int *)dst;
          for (x = 0; x < byte_width; x++) {
            *dst2++ = pal[*src++];
          }
          while (x & 3) {
            src++;
            x += 1;
          }
          dst += word_width;
        }
        break;
      case 16:
        for (y = height-1; y >= 0; y--) {
          dst3 = (short *)dst;
          for (x = 0; x < byte_width; x+=2) {
            i  = (*src++);
            i |= (*src++)<<8;
            *dst3++ = i;
            *dst3++ = i>>8;
          }
          while (x & 3) {
            src++;
            x += 1;
          }
          dst += word_width;
        }
        break;
      case 32:
        if (masks) {
          printf("Send this file to Robin, now! (32bpp with colour masks)\n");
          free(dst);
          return NULL;
        } else {
          for (y = 0; y < height; y++) {
            dst2 = (int *)dst;
            for (x = 0; x < width; x++) {
              i  = (*src++);
              i |= (*src++)<<8;
              i |= (*src++)<<16;
              *dst2++=i;
            }
            x=x*3;
            while (x & 3) {
              src++;
              x += 1;
            }
            dst += word_width;
          }
        }
        break;
    }
  } else {
    printf("Send this file to Robin, now! (compressed)\n");
    free(dst);
    return NULL;
  }

  *span       = word_width;
  *width_ret  = width;
  *height_ret = height;
  *bpp        = dst_bpp;

  return dst - word_width*height;
}

static void *bmp_read(ImageReader *im,
                      int         *width,
                      int         *height,
                      int         *span,
                      int         *bpp)
{
    int            offset;
    long           filelen, filepos;
    unsigned char *data;
    unsigned char *bmp;

    filepos = ftell(im->file);
    fseek(im->file, 0, SEEK_END);
    filelen = ftell(im->file);
    fseek(im->file, 0, SEEK_SET);
    
    /* If we were at the end to start, then we'd read our one and only
     * image. */
    if (filepos == filelen)
        return NULL;

    /* Load the whole lot into memory */
    bmp = Malloc(filelen);
    fread(bmp, 1, filelen, im->file);

    offset = getdword(bmp+10);
    data = bmp_load_sub(bmp+14, width, height, span, bpp, offset-14, filelen);
    free(bmp);
    return data;
}

static int get_uncommented_char(FILE *file)
{
    int c;
    
    do
    {
        c = fgetc(file);
        if (c != '#')
            return c;
        do {
            c = fgetc(file);
        } while ((c != EOF) && (c != '\n') && (c != '\r'));
    }
    while (c != EOF);

    return EOF;
}

static int get_pnm_num(FILE *file)
{
    int c;
    int val = 0;
    
    /* Skip over any whitespace */
    do {
        c = get_uncommented_char(file);
    } while (isspace(c));
    
    /* Read the number */
    while (isdigit(c))
    {
        val = val*10 + c - '0';
        c = get_uncommented_char(file);
    }
    
    /* assume the last c is whitespace */
    return val;
}

static void pbm_read(FILE          *file,
                     int            width,
                     int            height,
                     int            maxval,
                     unsigned char *bmp)
{
    int w;
    int byte, mask, g;
    
    bmp += width*(height-1)<<2;
    
    for (; height>0; height--) {
        mask = 0;
        for (w=width; w>0; w--) {
            if (mask == 0)
            {
                byte = fgetc(file);
                mask = 0x80;
            }
            g = byte & mask;
            if (g != 0)
                g = 255;
            mask >>= 1;
            *bmp++ = g;
            *bmp++ = g;
            *bmp++ = g;
            *bmp++ = 0;
        }
        bmp -= width<<3;
    }
}

static void pgm_read(FILE          *file,
                     int            width,
                     int            height,
                     int            maxval,
                     unsigned char *bmp)
{
    int w;
    
    bmp += width*(height-1)<<2;
    
    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                int g = fgetc(file);
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else if (maxval < 255) {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                int g = fgetc(file)*255/maxval;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                int g = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

static void ppm_read(FILE          *file,
                     int            width,
                     int            height,
                     int            maxval,
                     unsigned char *bmp)
{
    int w;
    
    bmp += width*(height-1)<<2;
    
    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                *bmp++ = fgetc(file);
                *bmp++ = fgetc(file);
                *bmp++ = fgetc(file);
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else if (maxval < 255) {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                *bmp++ = fgetc(file)*255/maxval;
                *bmp++ = fgetc(file)*255/maxval;
                *bmp++ = fgetc(file)*255/maxval;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                *bmp++ = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

static void pbm_read_plain(FILE          *file,
                           int            width,
                           int            height,
                           int            maxval,
                           unsigned char *bmp)
{
    int w;
    int g;
    
    bmp += width*(height-1)<<2;
    
    for (; height>0; height--) {
        for (w=width; w>0; w--) {
            g = get_pnm_num(file);
            if (g != 0)
                g = 255;
            *bmp++ = g;
            *bmp++ = g;
            *bmp++ = g;
            *bmp++ = 0;
        }
        bmp -= width<<3;
    }
}

static void pgm_read_plain(FILE          *file,
                           int            width,
                           int            height,
                           int            maxval,
                           unsigned char *bmp)
{
    int w;
    
    bmp += width*(height-1)<<2;
    
    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                int g = get_pnm_num(file);
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                int g = get_pnm_num(file)*255/maxval;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = g;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

static void ppm_read_plain(FILE          *file,
                           int            width,
                           int            height,
                           int            maxval,
                           unsigned char *bmp)
{
    int w;
    
    bmp += width*(height-1)<<2;
    
    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                *bmp++ = get_pnm_num(file);
                *bmp++ = get_pnm_num(file);
                *bmp++ = get_pnm_num(file);
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
    else
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                *bmp++ = get_pnm_num(file)*255/maxval;
                *bmp++ = get_pnm_num(file)*255/maxval;
                *bmp++ = get_pnm_num(file)*255/maxval;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

static void *pnm_read(ImageReader *im,
                      int         *width,
                      int         *height,
                      int         *span,
                      int         *bpp)
{
    unsigned char *bmp;
    int            c, maxval;
    void          (*read)(FILE *, int, int, int, unsigned char *);

    c = fgetc(im->file);
    /* Skip over any white space before the P */
    while ((c != 'P') && (c != EOF)) {
        c = fgetc(im->file);
    }
    if (c == EOF)
        return NULL;
    switch (get_pnm_num(im->file))
    {
        case 1:
            read = pbm_read_plain;
            break;
        case 2:
            read = pgm_read_plain;
            break;
        case 3:
            read = ppm_read_plain;
            break;
        case 4:
            read = pbm_read;
            break;
        case 5:
            read = pgm_read;
            break;
        case 6:
            read = ppm_read;
            break;
        default:
            /* Eh? */
            fprintf(stderr, "Unknown PxM format!\n");
            return NULL;
    }
    *width  = get_pnm_num(im->file);
    *span   = *width * 4;
    *height = get_pnm_num(im->file);
    if (read != pbm_read)
        maxval = get_pnm_num(im->file);
    else
        maxval = 1;
    *bpp = 32; /* We always convert to 32bpp */
    
    bmp = Malloc(*width * *height * 4);

    read(im->file, *width, *height, maxval, bmp);
    return bmp;
}

static void image_open(ImageReader *im,
                       char        *filename)
{
    int type;
    
    im->file = fopen(filename, "rb");
    if (im->file == NULL) {
        fprintf(stderr, "%s failed to open\n", filename);
        exit(EXIT_FAILURE);
    }
    
    /* Identify the filetype */
    type  = fgetc(im->file);
    
    if (type == 0x50) {
        /* Starts with a P! Must be a P?M file. */
        im->read = pnm_read;
        ungetc(0x50, im->file);
    } else {
        type |= (fgetc(im->file)<<8);
        if (type == 0x4d42) { /* BM */
            /* BMP format; Win v2 or above */
            im->read = bmp_read;
        } else {
            fprintf(stderr, "%s: Unrecognised image type\n", filename);
            exit(EXIT_FAILURE);
        }
    }
}

static void image_close(ImageReader *im)
{
    fclose(im->file);
    im->file = NULL;
}

static void find_changed_bbox(unsigned char *bmp,
                              unsigned char *bmp2,
                              int            w,
                              int            h,
                              int            span,
                              int            bpp,
                              BBox          *bbox)
{
    int    x, y;
    int   *isrc, *isrc2;
    short *ssrc, *ssrc2;

    bbox->xmin = w;
    bbox->ymin = h;
    bbox->xmax = -1;
    bbox->ymax = -1;

    if (bpp == 32)
    {
        isrc  = (int *)bmp;
        isrc2 = (int *)bmp2;
        span >>= 2;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                if (*isrc++ != *isrc2++)
                {
                    if (x < bbox->xmin)
                        bbox->xmin = x;
                    if (x > bbox->xmax)
                        bbox->xmax = x;
                    if (y < bbox->ymin)
                        bbox->ymin = y;
                    if (y > bbox->ymax)
                        bbox->ymax = y;
                }
            }
            isrc  += span;
            isrc2 += span;
        }
    }
    else
    {
        ssrc  = (short *)bmp;
        ssrc2 = (short *)bmp2;
        span >>= 1;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                if (*ssrc++ != *ssrc2++)
                {
                    if (x < bbox->xmin)
                        bbox->xmin = x;
                    if (x > bbox->xmax)
                        bbox->xmax = x;
                    if (y < bbox->ymin)
                        bbox->ymin = y;
                    if (y > bbox->ymax)
                        bbox->ymax = y;
                }
            }
            ssrc  += span;
            ssrc2 += span;
        }
    }
}

static void rediff(unsigned char *bmp,
                   unsigned char *bmp2,
                   int            span,
                   int            bpp,
                   BBox          *bbox2)
{
    int    x, y;
    int   *isrc, *isrc2;
    short *ssrc, *ssrc2;
    BBox   bbox;
    int    w;
    int    h;

    w = bbox2->xmax - bbox2->xmin;
    h = bbox2->ymax - bbox2->ymin;
    bbox.xmin = w;
    bbox.ymin = h;
    bbox.xmax = -1;
    bbox.ymax = -1;

    if (bpp == 32)
    {
        isrc  = (int *)bmp;
        isrc2 = (int *)bmp2;
        span >>= 2;
        isrc  += span*(bbox2->ymin)+bbox2->xmin;
        isrc2 += span*(bbox2->ymin)+bbox2->xmin;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                if (*isrc++ != *isrc2++)
                {
                    if (x < bbox.xmin)
                        bbox.xmin = x;
                    if (x > bbox.xmax)
                        bbox.xmax = x;
                    if (y < bbox.ymin)
                        bbox.ymin = y;
                    if (y > bbox.ymax)
                        bbox.ymax = y;
                }
            }
            isrc  += span;
            isrc2 += span;
        }
    }
    else
    {
        ssrc  = (short *)bmp;
        ssrc2 = (short *)bmp2;
        ssrc  += span*(bbox2->ymin)+bbox2->xmin;
        ssrc2 += span*(bbox2->ymin)+bbox2->xmin;
        span >>= 1;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                if (*ssrc++ != *ssrc2++)
                {
                    if (x < bbox.xmin)
                        bbox.xmin = x;
                    if (x > bbox.xmax)
                        bbox.xmax = x;
                    if (y < bbox.ymin)
                        bbox.ymin = y;
                    if (y > bbox.ymax)
                        bbox.ymax = y;
                }
            }
            ssrc  += span;
            ssrc2 += span;
        }
    }
    if ((bbox.xmin > bbox.xmax) || (bbox.ymin > bbox.ymax))
    {
        /* No changes. Return with invalid bbox */
        *bbox2 = bbox;
        return;
    }
    bbox.xmin += bbox2->xmin;
    bbox.ymin += bbox2->ymin;
    bbox.xmax += bbox2->xmin;
    bbox.ymax += bbox2->ymin;
    bbox.xmax++;
    bbox.ymax++;
    if ((bbox.xmax-bbox.xmin > 0) &&
        (bbox.xmax-bbox.xmin < MINX))
    {
        int d = MINX;
        
        if (d > w)
            d = w;
        d -= (bbox.xmax-bbox.xmin);
        bbox.xmin -= d>>1;
        bbox.xmax += d-(d>>1);
        if (bbox.xmin < bbox2->xmin)
        {
            bbox.xmax += bbox2->xmin-bbox.xmin;
            bbox.xmin  = bbox2->xmin;
        }
        if (bbox.xmax > bbox2->xmax)
        {
            bbox.xmin -= bbox.xmax-bbox2->xmax;
            bbox.xmax  = bbox2->xmax;
        }
    }
    if ((bbox.ymax-bbox.ymin > 0) && (bbox.ymax-bbox.ymin < MINY))
    {
        int d = MINY;
        
        if (d > h)
            d = h;
        d -= (bbox.ymax-bbox.ymin);
        bbox.ymin -= d>>1;
        bbox.ymax += d-(d>>1);
        if (bbox.ymin < bbox2->ymin)
        {
            bbox.ymax += bbox2->ymin-bbox.ymin;
            bbox.ymin  = bbox2->ymin;
        }
        if (bbox.ymax > bbox2->ymax)
        {
            bbox.ymin -= bbox.ymax-bbox2->ymax;
            bbox.ymax  = bbox2->ymax;
        }
    }
    *bbox2 = bbox;
}

static int BBox_valid(BBox *bbox)
{
    return ((bbox->xmin < bbox->xmax) && (bbox->ymin < bbox->ymax));
}

static void diff_bmp(unsigned char *bmp,
                     unsigned char *bmp2,
                     int            w,
                     int            h,
                     int            span,
                     int            bpp)
{
    int    x, y;
    int   *isrc, *isrc2;
    int    i, i2;
    short *ssrc, *ssrc2;
    short  s, s2;

    if (bpp == 32)
    {
        isrc  = (int *)bmp;
        isrc2 = (int *)bmp2;
        span >>= 2;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                i  = *isrc++;
                i2 = *isrc2++;
                if (i == i2)
                {
                    int a;

                    a  =  i      & 0xFF;
                    a += (i>> 8) & 0xFF;
                    a += (i>>16) & 0xFF;
                    a += 0xFF*3*2;
                    a /= 6*2;

                    isrc[-1] = a | (a<<8) | (a<<16);
                }
                else
                {
                    int r,g,b,r2,g2,b2;

                    r  = i  & 0x0000FF;
                    g  = i  & 0x00FF00;
                    b  = i  & 0xFF0000;
                    r2 = i2 & 0x0000FF;
                    g2 = i2 & 0x00FF00;
                    b2 = i2 & 0xFF0000;
                    if ((abs(r-r2) <= 3) && (abs(g-g2) <= 0x300) && (abs(b-b2)<= 0x30000))
                        isrc[-1] = 0x00FF00;
                    else
                        isrc[-1] = 0x00FFFF;
                }
            }
            isrc  += span;
            isrc2 += span;
        }
    }
    else
    {
        ssrc  = (short *)bmp;
        ssrc2 = (short *)bmp2;
        span >>= 1;
        span -= w;
        for (y = 0; y < h; y++)
        {
            for (x = 0; x < w; x++)
            {
                s  = *ssrc++;
                s2 = *ssrc2++;
                if (s == s2)
                {
                    int a;

                    a  =  s      & 0x1F;
                    a += (s>> 6) & 0x1F;
                    a += (s>>11) & 0x1F;
                    a += 3*0x1F*2;
                    a /= 6*2;

                    ssrc[-1] = a | (a<<6) | ((a & 0x10)<<5) | (a<<11);
                }
                else
                {
                    int r,g,b,r2,g2,b2;

                    r  =  s       & 0x1f;
                    g  = (s >> 5) & 0x3f;
                    b  = (s >>11) & 0x1f;
                    r2 =  s2      & 0x1f;
                    g2 = (s2>> 5) & 0x3f;
                    b2 = (s2>>11) & 0x1f;
                    if ((abs(r-r2) <= 1) && (abs(g-g2) <= 3) && (abs(b-b2)<= 1))
                        ssrc[-1] = 0x07E0;
                    else
                        ssrc[-1] = 0x001F;
                }
            }
            ssrc  += span;
            ssrc2 += span;
        }
    }
}

static void save_meta(BBox *bbox, char *str, int w, int h, int page)
{
    FILE *file;

    file = fopen(str, "wb");
    if (file == NULL)
        return;

    fprintf(file, "PW=%d\nPH=%d\nX=%d\nY=%d\nW=%d\nH=%d\nPAGE=%d\n",
            w, h, bbox->xmin, h-bbox->ymax,
            bbox->xmax-bbox->xmin, bbox->ymax-bbox->ymin, page);
    fclose(file);
}

static void save_bmp(unsigned char *data,
                     BBox          *bbox,
                     int            span,
                     int            bpp,
                     char          *str)
{
    FILE          *file;
    unsigned char  bmp[14+40];
    int            word_width;
    int            src_bypp;
    int            width, height;
    int            x, y;

    file = fopen(str, "wb");
    if (file == NULL)
        return;

    width  = bbox->xmax - bbox->xmin;
    height = bbox->ymax - bbox->ymin;

    src_bypp = (bpp == 16 ? 2 : 4);
    if (bpp == 16)
        word_width = width*2;
    else
        word_width = width*3;
    word_width += 3;
    word_width &= ~3;

    bmp[0] = 'B';
    bmp[1] = 'M';
    putdword(bmp+2, 14+40+word_width*height);
    putdword(bmp+6, 0);
    putdword(bmp+10, 14+40);
    /* Now the bitmap header */
    putdword(bmp+14, 40);
    putdword(bmp+18, width);
    putdword(bmp+22, height);
    putword (bmp+26, 1);			/* Bit planes */
    putword (bmp+28, (bpp == 16 ? 16 : 24));
    putdword(bmp+30, 0);			/* Compression type */
    putdword(bmp+34, 0);			/* Compressed size */
    putdword(bmp+38, 354);
    putdword(bmp+42, 354);
    putdword(bmp+46, 0);
    putdword(bmp+50, 0);

    fwrite(bmp, 1, 14+40, file);

    data += bbox->xmin * src_bypp;
    data += bbox->ymin * span;

    if (bpp == 16)
        fwrite(data, 1, span*height, file);
    else
    {
        int zero = 0;

        word_width -= width*3;
        for (y=0; y<height; y++)
        {
            for (x=0; x<width; x++)
            {
                fwrite(data, 1, 3, file);
                data += 4;
            }
            if (word_width)
                fwrite(&zero, 1, word_width, file);
            data += span-(4*width);
        }
    }
    fclose(file);
}

#ifdef HAVE_LIBPNG
static void save_png(unsigned char *data,
                     BBox          *bbox,
                     int            span,
                     int            bpp,
                     char          *str)
{
    FILE *file;
    png_structp png;
    png_infop   info;
    png_bytep   *rows;
    int   word_width;
    int   src_bypp;
    int   bpc;
    int   width, height;
    int   y;
    
    file = fopen(str, "wb");
    if (file == NULL)
        return;

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        fclose(file);
        return;
    }
    info = png_create_info_struct(png);
    if (info == NULL)
        /* info being set to NULL above makes this safe */
        goto png_cleanup;
    
    /* libpng using longjmp() for error 'callback' */
    if (setjmp(png_jmpbuf(png)))
        goto png_cleanup;
    
    /* hook the png writer up to our FILE pointer */
    png_init_io(png, file);
    
    /* fill out the image header */
    width  = bbox->xmax - bbox->xmin;
    height = bbox->ymax - bbox->ymin;
    bpc = 8; /* FIXME */
    png_set_IHDR(png, info, width, height, bpc, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    /* fill out pointers to each row */
    /* we use bmp coordinates where the zero-th row is at the bottom */
    src_bypp = (bpp == 16 ? 2 : 4);
    if (bpp == 16)
        word_width = width*2;
    else
        word_width = width*3;
    word_width += 3;
    word_width &= ~3;
    rows = malloc(sizeof(*rows)*height);
    if (rows == NULL)
        goto png_cleanup;
    for (y = 0; y < height; y++)
      rows[height - y - 1] = &data[(y + bbox->ymin)*span + bbox->xmin];
    png_set_rows(png, info, rows);
    
    /* write out the image */
    png_write_png(png, info, PNG_TRANSFORM_STRIP_FILLER_AFTER, NULL);

png_cleanup:
    png_destroy_write_struct(&png, &info);
    fclose(file);
    return;
}
#endif /* HAVE_LIBPNG */

int main(int argc, char *argv[])
{
    int            w,  h,  s,  bpp;
    int            w2, h2, s2, bpp2;
    int            nx, ny, n;
    int            basenum;
    int            imagecount;
    int            maxdiffs;
    unsigned char *bmp;
    unsigned char *bmp2;
    BBox           bbox, bbox2;
    BBox          *boxlist;
    char           str1[256];
    char           str2[256];
    char           str3[256];
    char           str4[256];
    ImageReader    image1, image2;

    if (argc < 4)
    {
        fprintf(stderr, "Syntax: bmpcmp <file1> <file2> <outfile_root> [<basenum>] [<maxdiffs>]\n");
        fprintf(stderr, "  <file1> and <file2> can be bmp, ppm, pgm or pbm files.\n");
        fprintf(stderr, "  This will produce a series of <outfile_root>.<number>.bmp files\n");
        fprintf(stderr, "  and a series of <outfile_root>.<number>.meta files.\n");
        fprintf(stderr, "  The maxdiffs value determines the maximum number of bitmaps\n");
        fprintf(stderr, "  produced - 0 (or unsupplied) is taken to mean unlimited.\n");
        exit(EXIT_FAILURE);
    }

    if (argc > 4)
    {
        basenum = atoi(argv[4]);
    }
    else
    {
        basenum = 0;
    }

    if (argc > 5)
    {
        maxdiffs = atoi(argv[5]);
    }
    else
    {
        maxdiffs = 0;
    }
    
    image_open(&image1, argv[1]);
    image_open(&image2, argv[2]);
    
    imagecount = 1;
    while (((bmp2 = NULL,
             bmp  = image1.read(&image1, &w,  &h,  &s,  &bpp )) != NULL) &&
           ((bmp2 = image2.read(&image2, &w2, &h2, &s2, &bpp2)) != NULL))
    {
        /* Check images are compatible */
        if ((w != w2) || (h != h2) || (s != s2) || (bpp != bpp2))
        {
            fprintf(stderr,
                    "Can't compare images "
                    "(w=%d,%d) (h=%d,%d) (s=%d,%d) (bpp=%d,%d)!\n",
                    w, w2, h, h2, s, s2, bpp, bpp2);
            exit(EXIT_FAILURE);
        }

        find_changed_bbox(bmp, bmp2, w, h, s, bpp, &bbox);
        if ((bbox.xmin > bbox.xmax) && (bbox.ymin > bbox.ymax))
        {
            /* The script will scream for us */
            /* fprintf(stderr, "No differences found!\n"); */
            /* Unchanged */
            exit(EXIT_SUCCESS);
        }
        /* Make the bbox sensibly exclusive */
        bbox.xmax++;
        bbox.ymax++;

        /* Make bbox2.xmin/ymin be the centre of the changed area */
        bbox2.xmin = (bbox.xmin + bbox.xmax + 1)/2;
        bbox2.ymin = (bbox.ymin + bbox.ymax + 1)/2;

        /* Make bbox2.xmax/ymax be the width of the changed area */
        nx = 1;
        ny = 1;
        bbox2.xmax = bbox.xmax - bbox.xmin;
        if (bbox2.xmax < MINX)
            bbox2.xmax = MINX;
        if (bbox2.xmax > MAXX)
        {
            nx = 1+(bbox2.xmax/MAXX);
            bbox2.xmax = MAXX*nx;
        }
        bbox2.ymax = bbox.ymax - bbox.ymin;
        if (bbox2.ymax < MINY)
            bbox2.ymax = MINY;
        if (bbox2.ymax > MAXY)
        {
            ny = 1+(bbox2.ymax/MAXY);
            bbox2.ymax = MAXY*ny;
        }

        /* Now make the real bbox */
        bbox2.xmin -= bbox2.xmax>>1;
        if (bbox2.xmin < 0)
            bbox2.xmin = 0;
        bbox2.ymin -= bbox2.ymax>>1;
        if (bbox2.ymin < 0)
            bbox2.ymin = 0;
        bbox2.xmax += bbox2.xmin;
        if (bbox2.xmax > w)
        {
            bbox2.xmin -= bbox2.xmax-w;
            if (bbox2.xmin < 0)
                bbox2.xmin = 0;
            bbox2.xmax = w;
        }
        bbox2.ymax += bbox2.ymin;
        if (bbox2.ymax > h)
        {
            bbox2.ymin -= bbox2.ymax-h;
            if (bbox2.ymin < 0)
                bbox2.ymin = 0;
            bbox2.ymax = h;
        }

        /* bbox */
        boxlist = Malloc(sizeof(*boxlist) * nx * ny);

        /* Now save the changed bmps */
        n = basenum;
        boxlist--;
        for (w2=0; w2 < nx; w2++)
        {
            for (h2=0; h2 < ny; h2++)
            {
                boxlist++;
                boxlist->xmin = bbox2.xmin + MAXX*w2;
                boxlist->xmax = boxlist->xmin + MAXX;
                if (boxlist->xmax > bbox2.xmax)
                    boxlist->xmax = bbox2.xmax;
                if (boxlist->xmin > boxlist->xmax-MINX)
                {
                    boxlist->xmin = boxlist->xmax-MINX;
                    if (boxlist->xmin < 0)
                        boxlist->xmin = 0;
                }
                boxlist->ymin = bbox2.ymin + MAXY*h2;
                boxlist->ymax = boxlist->ymin + MAXY;
                if (boxlist->ymax > bbox2.ymax)
                    boxlist->ymax = bbox2.ymax;
                if (boxlist->ymin > boxlist->ymax-MINY)
                {
                    boxlist->ymin = boxlist->ymax-MINY;
                    if (boxlist->ymin < 0)
                        boxlist->ymin = 0;
                }
                rediff(bmp, bmp2, s, bpp, boxlist);
                if (!BBox_valid(boxlist))
                    continue;
#ifdef HAVE_LIBPNG
                sprintf(str1, "%s.%05d.png", argv[3], n);
                sprintf(str2, "%s.%05d.png", argv[3], n+1);
                save_png(bmp,  boxlist, s, bpp, str1);
                save_png(bmp2, boxlist, s, bpp, str2);
#else
                sprintf(str1, "%s.%05d.bmp", argv[3], n);
                sprintf(str2, "%s.%05d.bmp", argv[3], n+1);
                save_bmp(bmp,  boxlist, s, bpp, str1);
                save_bmp(bmp2, boxlist, s, bpp, str2);
#endif
                sprintf(str4, "%s.%05d.meta", argv[3], n);
                save_meta(boxlist, str4, w, h, imagecount);
                n += 3;
            }
        }
        boxlist -= nx*ny;
        diff_bmp(bmp, bmp2, w, h, s, bpp);
        n = basenum;
        for (w2=0; w2 < nx; w2++)
        {
            for (h2=0; h2 < ny; h2++)
            {
                boxlist++;
                if (!BBox_valid(boxlist))
                    continue;
                sprintf(str3, "%s.%05d.bmp", argv[3], n+2);
                save_bmp(bmp, boxlist, s, bpp, str3);
                n += 3;
            }
        }
        basenum = n;

        boxlist -= nx*ny;
	boxlist++;
        free(boxlist);
        free(bmp);
        free(bmp2);
        
        /* If there is a maximum set */
        if (maxdiffs > 0)
        {
            /* Check to see we haven't exceeded it */
            maxdiffs--;
            if (maxdiffs == 0)
            {
                break;
            }
        }
    }

    /* If one loaded, and the other didn't - that's an error */
    if ((bmp2 != NULL) && (bmp == NULL))
    {
        fprintf(stderr, "Failed to load image %d from '%s'\n",
                imagecount, argv[1]);
        exit(EXIT_FAILURE);
    }
    if ((bmp != NULL) && (bmp2 == NULL))
    {
        fprintf(stderr, "Failed to load image %d from '%s'\n",
                imagecount, argv[2]);
        exit(EXIT_FAILURE);
    }

    image_close(&image1);
    image_close(&image2);
    

    return EXIT_SUCCESS;
}
