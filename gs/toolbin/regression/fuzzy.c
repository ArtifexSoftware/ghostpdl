/**
 * Fuzzy comparison utility. Copyright 2001-2002 artofcode LLC.
 **/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

typedef unsigned char uchar;
typedef int bool;
#define FALSE 0
#define TRUE 1

typedef struct _Image Image;

struct _Image {
  int (*close) (Image *self);
  int (*get_scan_line) (Image *self, uchar *buf);
  int width;
  int height;
  int n_chan;
  int bpp; /* bits per pixel */
  int depth;
};

typedef struct _FuzzyParams FuzzyParams;

struct _FuzzyParams {
  int tolerance;    /* in pixel counts */
  int window_size;
};

typedef struct _FuzzyReport FuzzyReport;

struct _FuzzyReport {
  int n_diff;
  int n_outof_tolerance;
  int n_outof_window;
};

int
image_get_rgb_scan_line (Image *image, uchar *buf)
{
  uchar *image_buf;
  int width = image->width;
  int depth = image->depth;
  int code;
  int x;

  if (image->n_chan == 3 && image->bpp == 8 && depth == 256)
    return image->get_scan_line (image, buf);

  image_buf = malloc (image->n_chan * ((width * image->bpp + 7) >> 3));
  code = image->get_scan_line (image, image_buf);
  if (code < 0)
    {
      /* skip */
    }
  else if (image->n_chan == 1 && image->bpp == 8)
    {
      for (x = 0; x < width; x++)
	{
	  uchar g = image_buf[x];
	  if (depth != 256)
	    {
	      g = (((int)g) * 255) / depth;
	    }
	  buf[x * 3] = g;
	  buf[x * 3 + 1] = g;
	  buf[x * 3 + 2] = g;
	}
    }
  else if (image->n_chan == 1 && image->bpp == 1)
    {
      for (x = 0; x < width; x++)
	{
	  uchar g = -!(image_buf[x >> 3] && (128 >> (x & 7)));
	  buf[x * 3] = g;
	  buf[x * 3 + 1] = g;
	  buf[x * 3 + 2] = g;
	}
    }
  else if (image->n_chan == 3 && image->bpp == 8)
    {
      for (x = 0; x < width * 3; x++)
	{
	  uchar g = image_buf[x];

	  g = (((int)g) * 255) / depth;
	  buf[x] = g;
	}
    }
  else
    code = -1;
  free (image_buf);
  return code;
}

int
image_close (Image *image)
{
  return image->close (image);
}

typedef struct _ImagePnm ImagePnm;

struct _ImagePnm {
  Image super;
  FILE *f;
};

static int
image_pnm_close (Image *self)
{
  ImagePnm *pnm = (ImagePnm *)self;
  int code;

  code = fclose (pnm->f);
  free (self);
  return code;
}

static int
image_pnm_get_scan_line (Image *self, uchar *buf)
{
  ImagePnm *pnm = (ImagePnm *)self;
  int n_bytes = self->n_chan * ((self->width * self->bpp + 7) >> 3);
  int code;

  code = fread (buf, 1, n_bytes, pnm->f);
  return (code < n_bytes) ? -1 : 0;
}

Image *
open_pnm_image (const char *fn)
{
  FILE *f = fopen (fn, "rb");
  int width, height;
  int n_chan, bpp, depth;
  char linebuf[256];
  ImagePnm *image;

  if (f == NULL)
    return NULL;

  image = (ImagePnm *)malloc (sizeof(ImagePnm));
  image->f = f;
  if (fgets (linebuf, sizeof(linebuf), f) == NULL ||
      linebuf[0] != 'P' || linebuf[1] < '4' || linebuf[1] > '6')
    {
      fclose (f);
      return NULL;
    }
  switch (linebuf[1])
    {
    case '4':
      n_chan = 1;
      bpp = 1;
      depth = 1;
      break;
    case '5':
      n_chan = 1;
      bpp = 8;
      depth = -1;
      break;
    case '6':
      n_chan = 3;
      bpp = 8;
      depth = -1;
      break;
    default:
      fclose (f);
      return NULL;
    }
  do
    {
      if (fgets (linebuf, sizeof(linebuf), f) == NULL)
	{
	  fclose (f);
	  return NULL;
	}
    }
  while (linebuf[0] == '#');
  if (sscanf (linebuf, "%d %d", &width, &height) != 2)
    {
      fclose (f);
      return NULL;
    }
  if (bpp == 8)
    {
      do
	{
	  if (fgets (linebuf, sizeof(linebuf), f) == NULL)
	    {
	      fclose (f);
	      return NULL;
	    }
	}
      while (linebuf[0] == '#');
      if (sscanf (linebuf, "%d", &depth) != 1)
	{
	  fclose (f);
	  return NULL;
	}
    }
  image->super.close = image_pnm_close;
  image->super.get_scan_line = image_pnm_get_scan_line;
  image->super.width = width;
  image->super.height = height;
  image->super.n_chan = n_chan;
  image->super.bpp = bpp;
  image->super.depth = depth;
  return &image->super;
}

Image *
open_image_file (const char *fn)
{
  /* This is the place to add a dispatcher for other image types. */
  return open_pnm_image (fn);
}

static uchar **
alloc_window (int row_bytes, int window_size)
{
  uchar **buf = (uchar **)malloc (window_size * sizeof(uchar *));
  int i;

  for (i = 0; i < window_size; i++)
    buf[i] = malloc (row_bytes);
  return buf;
}

static void
free_window (uchar **buf, int window_size)
{
  int i;

  for (i = 0; i < window_size; i++)
    free (buf[i]);
  free (buf);
}

static void
roll_window (uchar **buf, int window_size)
{
  int i;
  uchar *tmp1, *tmp2;

  tmp1 = buf[window_size - 1];
  for (i = 0; i < window_size; i++)
    {
      tmp2 = buf[i];
      buf[i] = tmp1;
      tmp1 = tmp2;
    }
}

static bool
check_window (uchar **buf1, uchar **buf2,
	      const FuzzyParams *fparams,
	      int x, int y, int width, int height)
{
  int tolerance = fparams->tolerance;
  int window_size = fparams->window_size;
  int i, j;
  const int half_win = window_size >> 1;
  const uchar *rowmid1 = buf1[half_win];
  const uchar *rowmid2 = buf2[half_win];
  uchar r1 = rowmid1[x * 3], g1 = rowmid1[x * 3 + 1], b1 = rowmid1[x * 3 + 2];
  uchar r2 = rowmid2[x * 3], g2 = rowmid2[x * 3 + 1], b2 = rowmid2[x * 3 + 2];
  bool match1 = FALSE, match2 = FALSE;

  for (j = -half_win; j <= half_win; j++)
    {
      const uchar *row1 = buf1[j + half_win];
      const uchar *row2 = buf2[j + half_win];
      for (i = -half_win; i <= half_win; i++)
	if ((i != 0 || j != 0) &&
	    x + i >= 0 && x + i < width &&
	    y + j >= 0 && y + j < height)
	  {
	    if (abs (r1 - row2[(x + i) * 3]) <= tolerance &&
		abs (g1 - row2[(x + i) * 3 + 1]) <= tolerance &&
		abs (b1 - row2[(x + i) * 3 + 2]) <= tolerance)
	      match1 = TRUE;
	    if (abs (r2 - row1[(x + i) * 3]) <= tolerance &&
		abs (g2 - row1[(x + i) * 3 + 1]) <= tolerance &&
		abs (b2 - row1[(x + i) * 3 + 2]) <= tolerance)
	      match2 = TRUE;
	  }
    }
  return !(match1 && match2);
}

static void
fuzzy_diff_images (Image *image1, Image *image2, const FuzzyParams *fparams,
		   FuzzyReport *freport)
{
  int width = image1->width, height = image1->height;
  int tolerance = fparams->tolerance;
  int window_size = fparams->window_size;
  int row_bytes = width * 3;
  uchar **buf1 = alloc_window (row_bytes, window_size);
  uchar **buf2 = alloc_window (row_bytes, window_size);
  int y;

  freport->n_diff = 0;
  freport->n_outof_tolerance = 0;
  freport->n_outof_window = 0;

  for (y = 0; y < height; y++)
    {
      int x;
      uchar *row1 = buf1[0];
      uchar *row2 = buf2[0];
      uchar *rowmid1 = buf1[window_size >> 1];
      uchar *rowmid2 = buf2[window_size >> 1];

      image_get_rgb_scan_line (image1, row1);
      image_get_rgb_scan_line (image2, row2);
      for (x = 0; x < width; x++)
	{
	  if (rowmid1[x * 3] != rowmid2[x * 3] ||
	      rowmid1[x * 3 + 1] != rowmid2[x * 3 + 1] ||
	      rowmid1[x * 3 + 2] != rowmid2[x * 3 + 2])
	    {
	      freport->n_diff++;
	      if (abs (rowmid1[x * 3] - rowmid2[x * 3]) > tolerance ||
		  abs (rowmid1[x * 3 + 1] - rowmid2[x * 3 + 1]) > tolerance ||
		  abs (rowmid1[x * 3 + 2] - rowmid2[x * 3 + 2]) > tolerance)
		{
		  freport->n_outof_tolerance++;
		  if (check_window (buf1, buf2, fparams, x, y, width, height))
		    freport->n_outof_window++;
		}
	    }
	}
      roll_window (buf1, window_size);
      roll_window (buf2, window_size);
    }

  free_window (buf1, window_size);
  free_window (buf2, window_size);
}

static const char *
get_arg (int argc, char **argv, int *pi, const char *arg)
{
  if (arg[0] != 0)
    return arg;
  else
    {
      (*pi)++;
      if (*pi == argc)
	return NULL;
      else
	return argv[*pi];
    }
}

int
usage (void)
{
  fprintf (stderr, "Usage: fuzzy [-w window_size] [-t tolerance] a.ppm b.ppm\n");
  return 1;
}

int
main (int argc, char **argv)
{
  Image *image1, *image2;
  FuzzyParams fparams;
  FuzzyReport freport;
  const char *fn[2];
  int fn_idx = 0;
  int i;

  fparams.tolerance = 2;
  fparams.window_size = 3;

  for (i = 1; i < argc; i++)
    {
      const char *arg = argv[i];

      if (arg[0] == '-')
	{
	  switch (arg[1])
	    {
	    case 'w':
	      fparams.window_size = atoi (get_arg (argc, argv, &i, arg + 2));
	      if ((fparams.window_size & 1) == 0)
		{
		  fprintf (stderr, "window size must be odd\n");
		  return 1;
		}
	      break;
	    case 't':
	      fparams.tolerance = atoi (get_arg (argc, argv, &i, arg + 2));
	      break;
	    default:
	      return usage ();
	    }
	}
      else if (fn_idx < sizeof(fn)/sizeof(fn[0]))
	fn[fn_idx++] = argv[i];
      else
	return usage ();
    }

  if (fn_idx < 2)
    return usage ();

  image1 = open_image_file (fn[0]);
  if (image1 == NULL)
    {
      fprintf (stderr, "Error opening %s\n", fn[0]);
      return 1;
    }

  image2 = open_image_file (fn[1]);
  if (image2 == NULL)
    {
      image_close (image1);
      fprintf (stderr, "Error opening %s\n", fn[1]);
      return 1;
    }

  fuzzy_diff_images (image1, image2, &fparams, &freport);

  if (freport.n_outof_window > 0)
    {
      printf ("%s: %d different, %d out of tolerance, %d out of window\n",
	      fn[0], freport.n_diff, freport.n_outof_tolerance,
	      freport.n_outof_window);
      return 1;
    }
  else
    return 0;
}
