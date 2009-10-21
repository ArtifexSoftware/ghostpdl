/*
 * bmpcmp.c: BMP Comparison - utility for use with htmldiff.pl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINX (300)
#define MINY (320)
#define MAXX (600)
#define MAXY (960)


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
                                   int            image_offset)
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
  if ((src_bpp < 8) || (src_bpp == 24))
      src_bpp = 32;

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
  word_width  = width * (src_bpp>>3);
  word_width += 3;
  word_width &= ~3;

  dst = malloc(word_width * height);
  if (dst == NULL)
  {
      fprintf(stderr, "Failed to malloc %d bytes\n", word_width*height);
      return NULL;
  }

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
  *bpp        = src_bpp;

  return dst - word_width*height;
}

static void *bmp_load(char *filename,
                      int  *width,
                      int  *height,
                      int  *span,
                      int  *bpp)
{
  int           type, offset;
  FILE         *file;
  long          filelen;
  unsigned char *data;
  unsigned char *bmp;

  /* Open file, find length, load */
  file = fopen(filename, "rb");
  if (file == NULL)
  {
      fprintf(stderr, "%s failed to open\n", filename);
      return NULL;
  }
  fseek(file, 0, SEEK_END);
  filelen = ftell(file);
  fseek(file, 0, SEEK_SET);
  bmp = malloc(filelen);
  if (bmp == NULL)
  {
      fprintf(stderr, "Failed to allocate %d bytes\n", filelen);
      return NULL;
  }
  fread(bmp, 1, filelen, file);
  fclose(file);

  type = bmp[0] + (bmp[1]<<8);
  if (type == 0) {
    /* Win v1 DDB */
    /* Unsupported for now */
    data = NULL;
  } else if (type == 0x4D42) {
    /* BMP format; Win v2 or above */
    offset = getdword(bmp+10);
    data = bmp_load_sub(bmp+14, width, height, span, bpp, offset-14);
  } else {
    /* Not a BMP file */
    data = NULL;
  }

  free(bmp);
  return data;
}

typedef struct {
    int xmin;
    int ymin;
    int xmax;
    int ymax;
} BBox;

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
                    isrc[-1] = 0xFF0000;
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
                    ssrc[-1] = 0x001F;
                }
            }
            ssrc  += span;
            ssrc2 += span;
        }
    }
}

static void save_bmp(unsigned char *data,
                     BBox          *bbox,
                     int            span,
                     int            bpp,
                     char          *str)
{
    FILE *file;
    char  bmp[14+40];
    int   word_width;
    int   src_bypp;
    int   width, height;
    int   x, y;

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

int main(int argc, char *argv[])
{
    int            w,  h,  s,  bpp;
    int            w2, h2, s2, bpp2;
    int            nx, ny, n;
    int            basenum;
    unsigned char *bmp;
    unsigned char *bmp2;
    BBox           bbox, bbox2, bbox3;
    char           str1[256];
    char           str2[256];
    char           str3[256];

    if (argc < 4)
    {
        fprintf(stderr, "Syntax: bmpcmp <file1> <file2> <outfile_root> [<basenum>]\n");
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

    bmp  = bmp_load(argv[1], &w,  &h,  &s,  &bpp);
    if (bmp == NULL)
    {
        fprintf(stderr, "Failed to load '%s'\n", argv[1]);
        exit(EXIT_FAILURE);
    }
    bmp2 = bmp_load(argv[2], &w2, &h2, &s2, &bpp2);
    if (bmp2 == NULL)
    {
        fprintf(stderr, "Failed to load '%s'\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    if ((w != w2) || (h != h2) || (s != s2) || (bpp != bpp2))
    {
        fprintf(stderr, "Can't compare bmps!\n");
        exit(EXIT_FAILURE);
    }

    find_changed_bbox(bmp, bmp2, w, h, s, bpp, &bbox);

    if ((bbox.xmin > bbox.xmax) || (bbox.ymin > bbox.ymax))
    {
        fprintf(stderr, "No differences found!\n");
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
        bbox2.xmax = w;
    }
    bbox2.ymax += bbox2.ymin;
    if (bbox2.ymax > h)
    {
        bbox2.ymin -= bbox2.ymax-h;
        bbox2.ymax = h;
    }

    /* Now save the changed bmps */
    n = basenum;
    for (w2=0; w2 < nx; w2++)
    {
        for (h2=0; h2 < ny; h2++)
        {
            bbox3.xmin = bbox2.xmin + MAXX*w2;
            bbox3.xmax = bbox3.xmin + MAXX;
            if (bbox3.xmax > bbox2.xmax)
                bbox3.xmax = bbox2.xmax;
            if (bbox3.xmin > bbox3.xmax-MINX)
                bbox3.xmin = bbox3.xmax-MINX;
            bbox3.ymin = bbox2.ymin + MAXY*h2;
            bbox3.ymax = bbox3.ymin + MAXY;
            if (bbox3.ymax > bbox2.ymax)
                bbox3.ymax = bbox2.ymax;
            if (bbox3.ymin > bbox3.ymax-MINY)
                bbox3.ymin = bbox3.ymax-MINY;
            sprintf(str1, "%s.%d.bmp", argv[3], n);
            sprintf(str2, "%s.%d.bmp", argv[3], n+1);
            save_bmp(bmp,  &bbox3, s, bpp, str1);
            save_bmp(bmp2, &bbox3, s, bpp, str2);
            n += 3;
        }
    }
    diff_bmp(bmp, bmp2, w, h, s, bpp);
    n = basenum;
    for (w2=0; w2 < nx; w2++)
    {
        for (h2=0; h2 < ny; h2++)
        {
            bbox3.xmin = bbox2.xmin + MAXX*w2;
            bbox3.xmax = bbox3.xmin + MAXX;
            if (bbox3.xmax > bbox2.xmax)
                bbox3.xmax = bbox2.xmax;
            if (bbox3.xmin > bbox3.xmax-MINX)
                bbox3.xmin = bbox3.xmax-MINX;
            bbox3.ymin = bbox2.ymin + MAXY*h2;
            bbox3.ymax = bbox3.ymin + MAXY;
            if (bbox3.ymax > bbox2.ymax)
                bbox3.ymax = bbox2.ymax;
            if (bbox3.ymin > bbox3.ymax-MINY)
                bbox3.ymin = bbox3.ymax-MINY;
            sprintf(str3, "%s.%d.bmp", argv[3], n+2);
            save_bmp(bmp, &bbox3, s, bpp, str3);
            n += 3;
        }
    }

    return EXIT_SUCCESS;
}
