/*
 * bmpcmp.c: BMP Comparison - utility for use with htmldiff.pl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#define DEBUG_BBOX(A) /* do {(A);} while(0==1) */

/* Values in map field:
 *
 *  0 means Completely unchanged pixel
 *  Bit 0 set means: Pixel is not an exact 1:1 match with original.
 *  Bit 1,2,3 set means: Pixel does not match with another within the window.
 *
 * In detail mode:
 *  0 means Completely unchanged pixel
 *  Bit 0 set means: Pixel is not an exact 1:1 match with original.
 *  Bit 1 set means: Pixel does not match exactly with another within the
 *                   window.
 *  Bit 2 set means: Pixel is not a thresholded match with original.
 *  Bit 3 set means: Pixel is not a thresholded match with another within the
 *                   window.
 *
 * Colors:
 *   0 => Greyscale  Unchanged pixel
 *   1 => Green      Translated within window
 *   3 => Cyan       Thresholded match (no translation)
 *   5 => Yellow     Both Exact translated, and Thresholded match
 *   7 => Orange     Thresholded match
 *   15=> Red        No Match
 */

enum {
    Default_MinX = 300,
    Default_MinY = 320,
    Default_MaxX = 600,
    Default_MaxY = 960
};

typedef struct
{
    int xmin;
    int ymin;
    int xmax;
    int ymax;
} BBox;

typedef struct
{
    /* Read from the command line */
    int        window;
    int        threshold;
    int        exhaustive;
    int        basenum;
    int        maxdiffs;
    char      *filename1;
    char      *filename2;
    char      *outroot;
    /* Fuzzy table */
    int        wTabLen;
    ptrdiff_t *wTab;
    /* Image details */
    int        width;
    int        height;
    int        span;
    int        bpp;
    /* Output BMP sizes */
    BBox       output_size;
} Params;

typedef struct ImageReader
{
    FILE *file;
    void *(*read)(struct ImageReader *,
                  int                *w,
                  int                *h,
                  int                *s,
                  int                *bpp,
                  int                *cmyk);
} ImageReader;

/*
 * Generic diff function type. Diff bmp and bmp2, both of geometry (width,
 * height, span (bytes), bpp). Return the changed bbox area in bbox.
 * Make a map of changes in map
 */
typedef void (DiffFn)(unsigned char *bmp,
                      unsigned char *bmp2,
                      unsigned char *map,
                      BBox          *bbox,
                      Params        *params);


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

  if (src_bpp == 24)
      src_bpp = 32;
  dst_bpp = src_bpp;
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
                      int         *bpp,
                      int         *cmyk)
{
    int            offset;
    long           filelen, filepos;
    unsigned char *data;
    unsigned char *bmp;

    /* No CMYK bmp support */
    *cmyk = 0;

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

static void skip_to_eol(FILE *file)
{
    int c;

    do
    {
        c = fgetc(file);
    }
    while ((c != EOF) && (c != '\n') && (c != '\r'));
}

static int get_uncommented_char(FILE *file)
{
    int c;

    do
    {
        c = fgetc(file);
        if (c != '#')
            return c;
        skip_to_eol(file);
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
            g=255-g;
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
    int r,g,b;
    int w;

    bmp += width*(height-1)<<2;

    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                r = fgetc(file);
                g = fgetc(file);
                b = fgetc(file);
                *bmp++ = b;
                *bmp++ = g;
                *bmp++ = r;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else if (maxval < 255) {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                r = fgetc(file)*255/maxval;
                g = fgetc(file)*255/maxval;
                b = fgetc(file)*255/maxval;
                *bmp++ = b;
                *bmp++ = g;
                *bmp++ = r;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    } else {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                r = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                g = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                b = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = b;
                *bmp++ = g;
                *bmp++ = r;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

static void pam_read(FILE          *file,
                     int            width,
                     int            height,
                     int            maxval,
                     unsigned char *bmp)
{
    int c,m,y,k;
    int w;

    bmp += width*(height-1)<<2;

    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                c = fgetc(file);
                m = fgetc(file);
                y = fgetc(file);
                k = fgetc(file);
                *bmp++ = c;
                *bmp++ = m;
                *bmp++ = y;
                *bmp++ = k;
            }
            bmp -= width<<3;
        }
    } else if (maxval < 255) {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                c = fgetc(file)*255/maxval;
                m = fgetc(file)*255/maxval;
                y = fgetc(file)*255/maxval;
                k = fgetc(file)*255/maxval;
                *bmp++ = c;
                *bmp++ = m;
                *bmp++ = y;
                *bmp++ = k;
            }
            bmp -= width<<3;
        }
    } else {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                c = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                m = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                y = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                k = ((fgetc(file)<<8) + (fgetc(file)))*255/maxval;
                *bmp++ = c;
                *bmp++ = m;
                *bmp++ = y;
                *bmp++ = k;
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
    int r,g,b;
    int w;

    bmp += width*(height-1)<<2;

    if (maxval == 255)
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                r = get_pnm_num(file);
                g = get_pnm_num(file);
                b = get_pnm_num(file);
                *bmp++ = b;
                *bmp++ = g;
                *bmp++ = r;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
    else
    {
        for (; height>0; height--) {
            for (w=width; w>0; w--) {
                r = get_pnm_num(file)*255/maxval;
                g = get_pnm_num(file)*255/maxval;
                b = get_pnm_num(file)*255/maxval;
                *bmp++ = b;
                *bmp++ = g;
                *bmp++ = r;
                *bmp++ = 0;
            }
            bmp -= width<<3;
        }
    }
}

/* Crap function, but does the job - assumes that if the first char matches
 * for the rest not to match is an error */
static int skip_string(FILE *file, const char *string)
{
    int c;

    /* Skip over any whitespace */
    do {
        c = get_uncommented_char(file);
    } while (isspace(c));

    /* Read the string */
    if (c != *string++) {
        ungetc(c, file);
        return 0;
    }

    /* Skip the string */
    while (*string) {
        c = fgetc(file);
        if (c != *string++) {
            ungetc(c, file);
            return 0;
        }
    }
    return 1;
}

static void pam_header_read(FILE *file,
                            int  *width,
                            int  *height,
                            int  *maxval)
{
    while (1) {
        if        (skip_string(file, "WIDTH")) {
            *width = get_pnm_num(file);
        } else if (skip_string(file, "HEIGHT")) {
            *height = get_pnm_num(file);
        } else if (skip_string(file, "DEPTH")) {
            if (get_pnm_num(file) != 4) {
                fprintf(stderr, "Only CMYK PAMs!\n");
                exit(1);
            }
        } else if (skip_string(file, "MAXVAL")) {
            *maxval = get_pnm_num(file);
        } else if (skip_string(file, "TUPLTYPE")) {
            if (!skip_string(file, "CMYK")) {
                fprintf(stderr, "Only CMYK PAMs!\n");
                exit(1);
            }
        } else if (skip_string(file, "ENDHDR")) {
          skip_to_eol(file);
          return;
        } else {
            /* Unknown header string. Just skip to the end of the line */
            skip_to_eol(file);
        }
    }
}

static void *pnm_read(ImageReader *im,
                      int         *width,
                      int         *height,
                      int         *span,
                      int         *bpp,
                      int         *cmyk)
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
    *cmyk = 0;
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
        case 7:
            read = pam_read;
            *cmyk = 1;
            break;
        default:
            /* Eh? */
            fprintf(stderr, "Unknown PxM format!\n");
            return NULL;
    }
    if (read == pam_read) {
        pam_header_read(im->file, width, height, &maxval);
    } else {
        *width  = get_pnm_num(im->file);
        *height = get_pnm_num(im->file);
        if (read != pbm_read)
            maxval = get_pnm_num(im->file);
        else
            maxval = 1;
    }

    *span   = *width * 4;
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

static void simple_diff(unsigned char *bmp,
                        unsigned char *bmp2,
                        unsigned char *map,
                        BBox          *bbox2,
                        Params        *params)
{
    int    x, y;
    int   *isrc, *isrc2;
    BBox   bbox;
    int    w    = params->width;
    int    h    = params->height;
    int    span = params->span;

    bbox.xmin = w;
    bbox.ymin = h;
    bbox.xmax = -1;
    bbox.ymax = -1;

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
                if (x < bbox.xmin)
                    bbox.xmin = x;
                if (x > bbox.xmax)
                    bbox.xmax = x;
                if (y < bbox.ymin)
                    bbox.ymin = y;
                if (y > bbox.ymax)
                    bbox.ymax = y;
                *map++ = 1;
            }
            else
            {
                *map++ = 0;
            }
        }
        isrc  += span;
        isrc2 += span;
    }
    *bbox2 = bbox;
}

static void simple_diff2(unsigned char *bmp,
                         unsigned char *bmp2,
                         unsigned char *map,
                         BBox          *bbox2,
                         Params        *params)
{
    int    x, y;
    int   *isrc, *isrc2;
    short *ssrc, *ssrc2;
    BBox   bbox;
    int    w;
    int    h;
    int    span = params->span;

    w = bbox2->xmax - bbox2->xmin;
    h = bbox2->ymax - bbox2->ymin;
    bbox.xmin = bbox2->xmax;
    bbox.ymin = bbox2->ymax;
    bbox.xmax = -1;
    bbox.ymax = -1;

    if (params->bpp == 32)
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
    *bbox2 = bbox;
}

typedef struct FuzzyParams FuzzyParams;

struct FuzzyParams {
    int            window;
    int            threshold;
    int            wTabLen;
    ptrdiff_t     *wTab;
    int            width;
    int            height;
    int            span;
    int          (*slowFn)(FuzzyParams   *self,
                           unsigned char *src,
                           unsigned char *src2,
                           unsigned char *map,
                           int            x,
                           int            y);
    int          (*fastFn)(FuzzyParams   *self,
                           unsigned char *src,
                           unsigned char *src2,
                           unsigned char *map);
};

static int fuzzy_slow(FuzzyParams   *fuzzy_params,
                      unsigned char *isrc,
                      unsigned char *isrc2,
                      unsigned char *map,
                      int            x,
                      int            y)
{
    int xmin, ymin, xmax, ymax;
    int span, t;

    /* left of window = max(0, x - window) - x */
    xmin = - fuzzy_params->window;
    if (xmin < -x)
        xmin = -x;
    /* right of window = min(width, x + window) - x */
    xmax = fuzzy_params->window;
    if (xmax > fuzzy_params->width-x)
        xmax = fuzzy_params->width-x;
    /* top of window = max(0, y - window) - y */
    ymin = - fuzzy_params->window;
    if (ymin < -y)
        ymin = -y;
    /* bottom of window = min(height, y + window) - y */
    ymax = fuzzy_params->window;
    if (ymax > fuzzy_params->height-y)
        ymax = fuzzy_params->height-y;
    span = fuzzy_params->span;
    t    = fuzzy_params->threshold;

    for (y = ymin; y < ymax; y++)
    {
        for (x = xmin; x < xmax; x++)
        {
            int o = x+y*span;
            int v;

            v = isrc[0]-isrc2[o];
            if (v < 0)
                v = -v;
            if (v > t)
                continue;
            v = isrc[1]-isrc2[o+1];
            if (v < 0)
                v = -v;
            if (v > t)
                continue;
            v = isrc[2]-isrc2[o+2];
            if (v < 0)
                v = -v;
            if (v > t)
                continue;
            v = isrc[3]-isrc2[o+3];
            if (v < 0)
                v = -v;
            if (v <= t)
                return 0;
        }
    }
    *map |= 15;
    return 1;
}

static int fuzzy_slow_exhaustive(FuzzyParams   *fuzzy_params,
                                 unsigned char *isrc,
                                 unsigned char *isrc2,
                                 unsigned char *map,
                                 int            x,
                                 int            y)
{
    int          xmin, ymin, xmax, ymax;
    int          span, t;
    unsigned int flags = 15;
    int          ret   = 1;

    /* left of window = max(0, x - window) - x */
    xmin = - fuzzy_params->window;
    if (xmin < -x)
        xmin = -x;
    /* right of window = min(width, x + window) - x */
    xmax = fuzzy_params->window;
    if (xmax > fuzzy_params->width-x)
        xmax = fuzzy_params->width-x;
    /* top of window = max(0, y - window) - y */
    ymin = - fuzzy_params->window;
    if (ymin < -y)
        ymin = -y;
    /* bottom of window = min(height, y + window) - y */
    ymax = fuzzy_params->window;
    if (ymax > fuzzy_params->height-y)
        ymax = fuzzy_params->height-y;
    span = fuzzy_params->span;
    t    = fuzzy_params->threshold;

    for (y = ymin; y < ymax; y++)
    {
        for (x = xmin; x < xmax; x++)
        {
            int o = x+y*span;
            int v;
            int exact = 1;

            v = isrc[0]-isrc2[o];
            if (v < 0)
                v = -v;
            if (v != 0)
                exact = 0;
            if (v > t)
                continue;
            v = isrc[1]-isrc2[o+1];
            if (v < 0)
                v = -v;
            if (v != 0)
                exact = 0;
            if (v > t)
                continue;
            v = isrc[2]-isrc2[o+2];
            if (v < 0)
                v = -v;
            if (v != 0)
                exact = 0;
            if (v > t)
                continue;
            v = isrc[3]-isrc2[o+3];
            if (v < 0)
                v = -v;
            if (v != 0)
                exact = 0;
            if (v <= t) {
                /* We match within the tolerance */
                flags &= ~(1<<3);
                if ((x | y) == 0)
                    flags &= ~(1<<2);
                if (exact) {
                    *map |= 1;
                    return 0;
                }
                ret = 0;
            }
        }
    }
    *map |= flags;
    return ret;
}

static int fuzzy_fast(FuzzyParams   *fuzzy_params,
                      unsigned char *isrc,
                      unsigned char *isrc2,
                      unsigned char *map)
{
    int        i;
    ptrdiff_t *wTab = fuzzy_params->wTab;
    int        t    = fuzzy_params->threshold;

    for (i = fuzzy_params->wTabLen; i > 0; i--)
    {
        int o = *wTab++;
        int v;

        v = isrc[0]-isrc2[o];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        v = isrc[1]-isrc2[o+1];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        v = isrc[2]-isrc2[o+2];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        v = isrc[3]-isrc2[o+3];
        if (v < 0)
            v = -v;
        if (v <= t)
            return 0;
    }
    *map |= 15;
    return 1;
}

static int fuzzy_fast_exhaustive(FuzzyParams   *fuzzy_params,
                                 unsigned char *isrc,
                                 unsigned char *isrc2,
                                 unsigned char *map)
{
    int            i;
    ptrdiff_t     *wTab  = fuzzy_params->wTab;
    int            t     = fuzzy_params->threshold;
    unsigned char  flags = 15;
    int            ret   = 1;

    for (i = fuzzy_params->wTabLen; i > 0; i--)
    {
        int o = *wTab++;
        int v;
        int exact = 1;

        v = isrc[0]-isrc2[o];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        if (v != 0)
            exact = 0;
        v = isrc[1]-isrc2[o+1];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        if (v != 0)
            exact = 0;
        v = isrc[2]-isrc2[o+2];
        if (v < 0)
            v = -v;
        if (v > t)
            continue;
        if (v != 0)
            exact = 0;
        v = isrc[3]-isrc2[o+3];
        if (v < 0)
            v = -v;
        if (v <= t) {
            /* We match within the tolerance */
            flags &= ~(1<<3);
            if (o == 0)
                flags &= ~(1<<2);
            if (exact) {
                *map |= 1;
                return 0;
            }
            ret = 0;
        }
    }
    *map |= flags;
    return ret;
}


static void fuzzy_diff(unsigned char *bmp,
                       unsigned char *bmp2,
                       unsigned char *map,
                       BBox          *bbox2,
                       Params        *params)
{
    int          x, y;
    int         *isrc, *isrc2;
    BBox         bbox;
    int          w, h, span;
    int          border;
    int          lb, rb, tb, bb;
    FuzzyParams  fuzzy_params;

    w = params->width;
    h = params->height;
    bbox.xmin = w;
    bbox.ymin = h;
    bbox.xmax = -1;
    bbox.ymax = -1;
    fuzzy_params.window    = params->window>>1;
    fuzzy_params.threshold = params->threshold;
    fuzzy_params.wTab      = params->wTab;
    fuzzy_params.wTabLen   = params->wTabLen;
    fuzzy_params.threshold = params->threshold;
    fuzzy_params.width     = params->width;
    fuzzy_params.height    = params->height;
    fuzzy_params.span      = params->span;
    if (params->exhaustive)
    {
        fuzzy_params.slowFn    = fuzzy_slow_exhaustive;
        fuzzy_params.fastFn    = fuzzy_fast_exhaustive;
    } else {
        fuzzy_params.slowFn    = fuzzy_slow;
        fuzzy_params.fastFn    = fuzzy_fast;
    }
    span                   = params->span;

    /* Figure out borders */
    border = params->window>>1;
    lb     = border;
    if (lb > params->width)
        lb = params->width;
    tb     = border;
    if (tb > params->height)
        tb = params->height;
    rb     = border;
    if (rb > params->width-lb)
        rb = params->width-lb;
    bb     = border;
    if (bb > params->height-tb)
        bb = params->height;

    isrc  = (int *)bmp;
    isrc2 = (int *)bmp2;
    span >>= 2;
    span -= w;
    for (y = 0; y < tb; y++)
    {
        for (x = 0; x < w; x++)
        {
            if (*isrc++ != *isrc2++)
            {
                *map++ |= 1;
                if (fuzzy_params.slowFn(&fuzzy_params,
                                        (unsigned char *)(isrc-1),
                                        (unsigned char *)(isrc2-1),
                                        map-1,
                                        x, y))
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
            else
            {
                *map++ = 0;
            }
        }
        isrc  += span;
        isrc2 += span;
    }
    for (; y < h-bb; y++)
    {
        for (x = 0; x < lb; x++)
        {
            if (*isrc++ != *isrc2++)
            {
                *map++ |= 1;
                if (fuzzy_params.slowFn(&fuzzy_params,
                                        (unsigned char *)(isrc-1),
                                        (unsigned char *)(isrc2-1),
                                        map-1,
                                        x, y))
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
            else
            {
                *map++ = 0;
            }
        }
        for (; x < w-rb; x++)
        {
            if (*isrc++ != *isrc2++)
            {
                *map++ |= 1;
                if (fuzzy_params.fastFn(&fuzzy_params,
                                        (unsigned char *)(isrc-1),
                                        (unsigned char *)(isrc2-1),
                                        map-1))
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
            else
            {
                *map++ = 0;
            }
        }
        for (; x < w; x++)
        {
            if (*isrc++ != *isrc2++)
            {
                *map++ |= 1;
                if (fuzzy_params.slowFn(&fuzzy_params,
                                        (unsigned char *)(isrc-1),
                                        (unsigned char *)(isrc2-1),
                                        map-1,
                                        x, y))
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
            else
            {
                *map++ = 0;
            }
        }
        isrc  += span;
        isrc2 += span;
    }
    for (; y < bb; y++)
    {
        for (x = 0; x < w; x++)
        {
            if (*isrc++ != *isrc2++)
            {
                *map++ |= 1;
                if (fuzzy_params.slowFn(&fuzzy_params,
                                        (unsigned char *)(isrc-1),
                                        (unsigned char *)(isrc2-1),
                                        map-1,
                                        x, y))
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
        }
        isrc  += span;
        isrc2 += span;
    }
    *bbox2 = bbox;
}

static int BBox_valid(BBox *bbox)
{
    return ((bbox->xmin < bbox->xmax) && (bbox->ymin < bbox->ymax));
}

static void uncmyk_bmp(unsigned char *bmp,
                       BBox          *bbox,
                       int            span)
{
    int w, h;
    int x, y;

    bmp  += span    *(bbox->ymin)+(bbox->xmin<<2);
    w     = bbox->xmax - bbox->xmin;
    h     = bbox->ymax - bbox->ymin;
    span -= 4*w;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int c, m, y, k, r, g, b;

            c = *bmp++;
            m = *bmp++;
            y = *bmp++;
            k = *bmp++;

            r = (255-c-k);
            if (r < 0)
                r = 0;
            g = (255-m-k);
            if (g < 0)
                g = 0;
            b = (255-y-k);
            if (b < 0)
                b = 0;
            bmp[-1] = 0;
            bmp[-2] = r;
            bmp[-3] = g;
            bmp[-4] = b;
        }
        bmp += span;
    }
}

static void diff_bmp(unsigned char *bmp,
                     unsigned char *map,
                     BBox          *bbox,
                     int            span,
                     int            map_span)
{
    int  w, h;
    int  x, y;
    int *isrc;

    isrc      = (int *)bmp;
    span    >>= 2;
    isrc     += span    *(bbox->ymin)+bbox->xmin;
    map      += map_span*(bbox->ymin)+bbox->xmin;
    w         = bbox->xmax - bbox->xmin;
    h         = bbox->ymax - bbox->ymin;
    span     -= w;
    map_span -= w;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int m = *map++;
            int i = *isrc;

            switch (m) {
                case 0:
                {
                    /* Matching pixel - greyscale it */
                    int a;

                    a  =  i      & 0xFF;
                    a += (i>> 8) & 0xFF;
                    a += (i>>16) & 0xFF;
                    a += 0xFF*3*2;
                    a /= 6*2;

                    *isrc++ = a | (a<<8) | (a<<16);
                    break;
                }
                case 1:
                    *isrc++ = 0x00FF00; /* Green */
                    break;
                case 3:
                    *isrc++ = 0x00FFFF; /* Cyan */
                    break;
                case 5:
                    *isrc++ = 0xFFFF00; /* Yellow */
                    break;
                case 7:
                    *isrc++ = 0xFF8000; /* Orange */
                    break;
                case 15:
                    *isrc++ = 0xFF0000; /* Red */
                    break;
                default:
                    fprintf(stderr,
                            "Internal error: unexpected map type %d\n", m);
                    isrc++;
                    break;
            }
        }
        isrc += span;
        map  += map_span;
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
    putword (bmp+26, 1);                        /* Bit planes */
    putword (bmp+28, (bpp == 16 ? 16 : 24));
    putdword(bmp+30, 0);                        /* Compression type */
    putdword(bmp+34, 0);                        /* Compressed size */
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
      rows[height - y - 1] = &data[(y + bbox->ymin)*span + bbox->xmin * src_bypp - 1];
    png_set_rows(png, info, rows);

    /* write out the image */
    png_write_png(png, info,
        PNG_TRANSFORM_STRIP_FILLER_BEFORE|
        PNG_TRANSFORM_BGR, NULL);

    free(rows);

png_cleanup:
    png_destroy_write_struct(&png, &info);
    fclose(file);
    return;
}
#endif /* HAVE_LIBPNG */

static void syntax(void)
{
    fprintf(stderr, "Syntax: bmpcmp [options] <file1> <file2> <outfile_root> [<basenum>] [<maxdiffs>]\n");
    fprintf(stderr, "  -w <window> or -w<window>         window size (default=1)\n");
    fprintf(stderr, "                                    (good values = 1, 3, 5, 7, etc)\n");
    fprintf(stderr, "  -t <threshold> or -t<threshold>   threshold   (default=0)\n");
    fprintf(stderr, "  -e                                exhaustive search\n");
    fprintf(stderr, "  -o <minx> <maxx> <miny> <maxy>    Output bitmap size hints (0 for default)\n");
    fprintf(stderr, "  -h or --help or -?                Output this message and exit\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  <file1> and <file2> can be "
#ifdef HAVE_LIBPNG
                    "png, "
#endif /* HAVE_LIBPNG */
                    "bmp, ppm, pgm, pbm or pam files.\n");
    fprintf(stderr, "  This will produce a series of <outfile_root>.<number>.bmp files\n");
    fprintf(stderr, "  and a series of <outfile_root>.<number>.meta files.\n");
    fprintf(stderr, "  The maxdiffs value determines the maximum number of bitmaps\n");
    fprintf(stderr, "  produced - 0 (or unsupplied) is taken to mean unlimited.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  To ignore 1 pixel moves:\n");
    fprintf(stderr, "    bmpcmp in.pam out.pam out\\diff -w 3\n");
    fprintf(stderr, "  To ignore small color changes:\n");
    fprintf(stderr, "    bmpcmp in.pam out.pam out\\diff -t 7\n");
    fprintf(stderr, "  To see the types of pixel changes in a picture:\n");
    fprintf(stderr, "    bmpcmp in.pam out.pam out\\diff -w 3 -t 7 -e\n");
    exit(EXIT_FAILURE);
}

static void parseArgs(int argc, char *argv[], Params *params)
{
    int arg;
    int i;

    /* Set defaults */
    memset(params, 0, sizeof(*params));
    params->window = 1;

    arg = 0;
    i = 1;
    while (i < argc)
    {
        if (argv[i][0] == '-')
        {
            switch (argv[i][1]) {
                case 'w':
                    if (argv[i][2]) {
                        params->window = atoi(&argv[i][2]);
                    } else if (++i < argc) {
                        params->window = atoi(argv[i]);
                    }
                    break;
                case 't':
                    if (argv[i][2]) {
                        params->threshold = atoi(&argv[i][2]);
                    } else if (++i < argc) {
                        params->threshold = atoi(argv[i]);
                    }
                    break;
                case 'o':
                    if (argc <= i+3)
                        syntax();
                    if (argv[i][2]) {
                        params->output_size.xmin = atoi(&argv[i][2]);
                        params->output_size.xmax = atoi(argv[i+1]);
                        params->output_size.ymin = atoi(argv[i+2]);
                        params->output_size.ymax = atoi(argv[i+3]);
                        i += 3;
                    } else if (argc <= i+4) {
                        syntax();
                    } else {
                        params->output_size.xmin = atoi(argv[++i]);
                        params->output_size.xmax = atoi(argv[++i]);
                        params->output_size.ymin = atoi(argv[++i]);
                        params->output_size.ymax = atoi(argv[++i]);
                    }
                    break;
                case 'e':
                    params->exhaustive = 1;
                    break;
                case 'h':
                case '?':
                case '-': /* Hack :) */
                    syntax();
                    break;
                default:
                    syntax();
            }
        } else {
            switch (arg) {
                case 0:
                    params->filename1 = argv[i];
                    break;
                case 1:
                    params->filename2 = argv[i];
                    break;
                case 2:
                    params->outroot = argv[i];
                    break;
                case 3:
                    params->basenum = atoi(argv[i]);
                    break;
                case 4:
                    params->maxdiffs = atoi(argv[i]);
                    break;
                default:
                    syntax();
            }
            arg++;
        }
        i++;
    }

    if (arg < 3)
    {
        syntax();
    }
    
    /* Sanity check */
    if (params->output_size.xmin == 0)
        params->output_size.xmin = Default_MinX;
    if (params->output_size.xmax == 0)
        params->output_size.xmax = Default_MaxX;
    if (params->output_size.ymin == 0)
        params->output_size.ymin = Default_MinY;
    if (params->output_size.ymax == 0)
        params->output_size.ymax = Default_MaxY;
    if (params->output_size.xmax < params->output_size.xmin)
        params->output_size.xmax = params->output_size.xmin;
    if (params->output_size.ymax < params->output_size.ymin)
        params->output_size.ymax = params->output_size.ymin;
}

static void makeWindowTable(Params *params, int span, int bpp)
{
    int        x, y, i, w;
    ptrdiff_t *wnd;

    params->window |= 1;
    w = (params->window+1)>>1;
    if (params->wTab == NULL) {
        params->wTabLen = params->window*params->window;
        params->wTab    = Malloc(params->wTabLen*sizeof(ptrdiff_t));
    }
    wnd = params->wTab;
    *wnd++ = 0;

#define OFFSET(x,y) (x*(bpp>>3)+y*span)

    for(i=1;i<params->window;i++) {
        x = i;
        y = 0;
        while (x != 0)
        {
            if ((x < w) && (y < w)) {
                *wnd++ = OFFSET( x, y);
                *wnd++ = OFFSET(-x,-y);
                *wnd++ = OFFSET(-y, x);
                *wnd++ = OFFSET( y,-x);
            }
            x--;
            y++;
        }
    }

#undef OFFSET

}

static void rediff(unsigned char *map,
                   BBox          *global,
                   Params        *params)
{
    BBox  local;
    int   x, y;
    int   w, h;
    int   span = params->width;

    w = global->xmax - global->xmin;
    h = global->ymax - global->ymin;
    local.xmin = global->xmax;
    local.ymin = global->ymax;
    local.xmax = -1;
    local.ymax = -1;

    map += span*(global->ymin)+global->xmin;
    span -= w;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            if (*map++ != 0)
            {
                if (x < local.xmin)
                    local.xmin = x;
                if (x > local.xmax)
                    local.xmax = x;
                if (y < local.ymin)
                    local.ymin = y;
                if (y > local.ymax)
                    local.ymax = y;
            }
        }
        map += span;
    }

    local.xmin += global->xmin;
    local.ymin += global->ymin;
    local.xmax += global->xmin;
    local.ymax += global->ymin;
    local.xmax++;
    local.ymax++;
    if ((local.xmax-local.xmin > 0) &&
        (local.xmax-local.xmin < params->output_size.xmin))
    {
        int d = params->output_size.xmin;

        if (d > w)
            d = w;
        d -= (local.xmax-local.xmin);
        local.xmin -= d>>1;
        local.xmax += d-(d>>1);
        if (local.xmin < global->xmin)
        {
            local.xmax += global->xmin-local.xmin;
            local.xmin  = global->xmin;
        }
        if (local.xmax > global->xmax)
        {
            local.xmin -= local.xmax-global->xmax;
            local.xmax  = global->xmax;
        }
    }
    if ((local.ymax-local.ymin > 0) &&
        (local.ymax-local.ymin < params->output_size.ymin))
    {
        int d = params->output_size.ymin;

        if (d > h)
            d = h;
        d -= (local.ymax-local.ymin);
        local.ymin -= d>>1;
        local.ymax += d-(d>>1);
        if (local.ymin < global->ymin)
        {
            local.ymax += global->ymin-local.ymin;
            local.ymin  = global->ymin;
        }
        if (local.ymax > global->ymax)
        {
            local.ymin -= local.ymax-global->ymax;
            local.ymax  = global->ymax;
        }
    }
}

int main(int argc, char *argv[])
{
    int            w,  h,  s,  bpp,  cmyk;
    int            w2, h2, s2, bpp2, cmyk2;
    int            nx, ny, n;
    int            xstep, ystep;
    int            imagecount;
    unsigned char *bmp;
    unsigned char *bmp2;
    unsigned char *map;
    BBox           bbox, bbox2;
    BBox          *boxlist;
    char           str1[256];
    char           str2[256];
    char           str3[256];
    char           str4[256];
    ImageReader    image1, image2;
    DiffFn        *diffFn;
    Params         params;

    parseArgs(argc, argv, &params);
    if (params.window <= 1 && params.threshold == 0) {
        diffFn = simple_diff;
    } else {
        diffFn = fuzzy_diff;
    }

    image_open(&image1, params.filename1);
    image_open(&image2, params.filename2);

    imagecount = 1;
    while (((bmp2 = NULL,
             bmp  = image1.read(&image1,&w, &h, &s, &bpp, &cmyk )) != NULL) &&
           ((bmp2 = image2.read(&image2,&w2,&h2,&s2,&bpp2,&cmyk2)) != NULL))
    {
        /* Check images are compatible */
        if ((w != w2) || (h != h2) || (s != s2) || (bpp != bpp2) ||
            (cmyk != cmyk2))
        {
            fprintf(stderr,
                    "Can't compare images "
                    "(w=%d,%d) (h=%d,%d) (s=%d,%d) (bpp=%d,%d) (cmyk=%d,%d)!\n",
                    w, w2, h, h2, s, s2, bpp, bpp2, cmyk, cmyk2);
            exit(EXIT_FAILURE);
        }

        if (params.window != 0)
        {
            makeWindowTable(&params, s, bpp);
        }
        map = Malloc(s*w*sizeof(unsigned char));
        params.width  = w;
        params.height = h;
        params.span   = s;
        params.bpp    = bpp;
        (*diffFn)(bmp, bmp2, map, &bbox, &params);
        if ((bbox.xmin <= bbox.xmax) && (bbox.ymin <= bbox.ymax))
        {
            /* Make the bbox sensibly exclusive */
            bbox.xmax++;
            bbox.ymax++;

            DEBUG_BBOX(fprintf(stderr, "Raw bbox=%d %d %d %d\n",
                               bbox.xmin, bbox.ymin, bbox.xmax, bbox.ymax));
            /* Make bbox2.xmin/ymin be the centre of the changed area */
            bbox2.xmin = (bbox.xmin + bbox.xmax + 1)/2;
            bbox2.ymin = (bbox.ymin + bbox.ymax + 1)/2;

            /* Calculate subdivisions of image to fit as best as possible
             * into our max/min sizes. */
            nx = 1;
            ny = 1;
            xstep = bbox.xmax - bbox.xmin;
            if (xstep > params.output_size.xmax)
            {
                nx = 1+(xstep/params.output_size.xmax);
                xstep = params.output_size.xmax;
            }
            if (xstep < params.output_size.xmin)
                xstep = params.output_size.xmin;
            if (xstep*nx > w)
                xstep = (w+nx-1)/nx;
            bbox2.xmax = xstep*nx;
            ystep = bbox.ymax - bbox.ymin;
            bbox2.ymax = ystep;
            if (ystep > params.output_size.ymax)
            {
                ny = 1+(ystep/params.output_size.ymax);
                ystep = params.output_size.ymax;
            }
            if (ystep < params.output_size.ymin)
                ystep = params.output_size.ymin;
            if (ystep*ny > h)
                ystep = (h+ny-1)/ny;
            bbox2.ymax = ystep*ny;

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

            DEBUG_BBOX(fprintf(stderr, "Expanded bbox=%d %d %d %d\n",
                               bbox2.xmin, bbox2.ymin, bbox2.xmax, bbox2.ymax));

            /* bbox */
            boxlist = Malloc(sizeof(*boxlist) * nx * ny);

            /* Now save the changed bmps */
            n = params.basenum;
            boxlist--;
            for (w2=0; w2 < nx; w2++)
            {
                for (h2=0; h2 < ny; h2++)
                {
                    boxlist++;
                    boxlist->xmin = bbox2.xmin + xstep*w2;
                    boxlist->xmax = boxlist->xmin + xstep;
                    if (boxlist->xmax > bbox2.xmax)
                        boxlist->xmax = bbox2.xmax;
                    boxlist->ymin = bbox2.ymin + ystep*h2;
                    boxlist->ymax = boxlist->ymin + ystep;
                    if (boxlist->ymax > bbox2.ymax)
                        boxlist->ymax = bbox2.ymax;
                    DEBUG_BBOX(fprintf(stderr, "Retesting bbox=%d %d %d %d\n",
                                       boxlist->xmin, boxlist->ymin,
                                       boxlist->xmax, boxlist->ymax));

                    rediff(map, boxlist, &params);
                    if (!BBox_valid(boxlist))
                        continue;
                    DEBUG_BBOX(fprintf(stderr, "Reduced bbox=%d %d %d %d\n",
                                       boxlist->xmin, boxlist->ymin,
                                       boxlist->xmax, boxlist->ymax));
                    if (cmyk)
                    {
                        uncmyk_bmp(bmp,  boxlist, s);
                        uncmyk_bmp(bmp2, boxlist, s);
                    }
#ifdef HAVE_LIBPNG
                    sprintf(str1, "%s.%05d.png", params.outroot, n);
                    sprintf(str2, "%s.%05d.png", params.outroot, n+1);
                    sprintf(str3, "%s.%05d.png", params.outroot, n+2);
                    save_png(bmp,  boxlist, s, bpp, str1);
                    save_png(bmp2, boxlist, s, bpp, str2);
#else
                    sprintf(str1, "%s.%05d.bmp", params.outroot, n);
                    sprintf(str2, "%s.%05d.bmp", params.outroot, n+1);
                    sprintf(str3, "%s.%05d.bmp", params.outroot, n+2);
                    save_bmp(bmp,  boxlist, s, bpp, str1);
                    save_bmp(bmp2, boxlist, s, bpp, str2);
#endif
                    diff_bmp(bmp, map, boxlist, s, w);
#ifdef HAVE_LIBPNG
                    save_png(bmp, boxlist, s, bpp, str3);
#else
                    save_bmp(bmp, boxlist, s, bpp, str3);
#endif
                    sprintf(str4, "%s.%05d.meta", params.outroot, n);
                    save_meta(boxlist, str4, w, h, imagecount);
                    n += 3;
                    /* If there is a maximum set */
                    if (params.maxdiffs > 0)
                    {
                        /* Check to see we haven't exceeded it */
                        params.maxdiffs--;
                        if (params.maxdiffs == 0)
                        {
                            goto done;
                        }
                    }
                }
            }
            params.basenum = n;

            boxlist -= nx*ny;
            boxlist++;
            free(boxlist);
        }
        free(bmp);
        free(bmp2);
        free(map);
    }

done:
    /* If one loaded, and the other didn't - that's an error */
    if ((bmp2 != NULL) && (bmp == NULL))
    {
        fprintf(stderr, "Failed to load image %d from '%s'\n",
                imagecount, params.filename1);
        exit(EXIT_FAILURE);
    }
    if ((bmp != NULL) && (bmp2 == NULL))
    {
        fprintf(stderr, "Failed to load image %d from '%s'\n",
                imagecount, params.filename2);
        exit(EXIT_FAILURE);
    }

    image_close(&image1);
    image_close(&image2);


    return EXIT_SUCCESS;
}
