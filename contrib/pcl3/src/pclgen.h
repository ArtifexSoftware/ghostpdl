/******************************************************************************
  File:     $Id: pclgen.h,v 1.25 2001/08/18 17:41:29 Martin Rel $
  Contents: Header for PCL-generating routines
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000, 2001 by Martin Lottermoser		      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  The functions declared in this header file generate code for
  Hewlett-Packard's Printer Command Language level 3+ ("PCL 3+", also called
  "PCL 3 Plus").

  The routines support only raster graphics data.

******************************************************************************/

#ifndef _pclgen_h	/* Inclusion protection */
#define _pclgen_h

/*****************************************************************************/

/* Standard headers */
#include "gp.h"

/*****************************************************************************/

/* PCL dialects */
typedef enum {
  pcl_level_3plus_DJ500,
   /* End Raster Graphics is still ESC * r B, no Shingling */
  pcl_level_3plus_ERG_both,
   /* ESC*rbC, otherwise identical with 'pcl_level_3plus_S5' */
  pcl_level_3plus_S5,
   /* DeskJet 500 series except 540: old quality commands, no
      Configure Raster Data, ESC*rC */
  pcl_level_3plus_S68,
   /* DeskJet 540, DeskJet 600 and 800 series */
  pcl_level_3plus_CRD_only
   /* printers always and only using Configure Raster Data instead of
      Set Number of Planes per Row and Set Raster Graphics Resolution */
} pcl_Level;

/* Test macros */
#define pcl_use_oldERG(level)		(level <= pcl_level_3plus_DJ500)
#define pcl_use_oldquality(level)	(level <= pcl_level_3plus_S5)
#define pcl_has_CRD(level)		(level >= pcl_level_3plus_S68)

/*****************************************************************************/

/*  Page Size codes (permitted arguments for the PCL command "Page Size")

    There exists an area of confusion around this command in connection with
    "orientation" in various senses. Unfortunately, the documentation made
    externally available by Hewlett-Packard is not only incomplete but also
    inconsistent in this respect. (Well, not only in this respect, but don't
    get me started on this topic or I'll run out of screen space.) In
    particular, HP does not distinguish between "input orientation"
    (orientation of a sheet with respect to the feeding direction: short edge
    first or long edge first) and "page orientation" (orientation of a sheet
    with respect to the "up" direction of the page contents: portrait or
    landscape). This is further complicated by the fact that PCL permits
    several relative orientations between text and graphics data.

    The following information is therefore partially in disagreement with
    HP documentation but does agree with the actual behaviour of the DJ 850C.

    (1) The Page Orientation command has no influence on the orientation of
        raster graphics data.
    (2) Therefore it is possible to define, independent of the Page Orientation
        setting, a coordinate system on the medium by reference to the printing
        of raster lines ("rows"): the order of pixels within a row defines what
        is left and right, and the order in which successive rows are printed
        distinguishes between up and down. I call this the "raster space"
        coordinate system. (Its units are irrelevant, but we need the
        directions.)
    (3) Because of the way DeskJet printers are constructed (limited memory!),
        raster space is fixed with respect to the feeding direction: the "up"
        edge of a sheet in raster space is the one entering the printer first
        (the leading edge).
    (4) Hence the media orientation in raster space depends only on the
        input orientation: short edge first leads to portrait and long edge
        first to landscape orientation in raster space.
    (5) Among other effects, the Page Size command sets in particular the
        printable region for the media size requested.
    (6) For raster graphics data, clipping occurs at the edge of the printable
        region. This happens at the right edge in raster space.
    (7) It is not possible in PCL to specify the input orientation one has
        chosen. In particular, the sign of a Page Size code has no influence on
        the printable region as seen in raster space.
    (8) Some Page Size codes do modify the orientation for text printing on
        some printers, but this can be overridden with a subsequent Page
        Orientation command.
    (9) The Page Size command usually sets up the printable region under the
        assumption of portrait orientation in raster space (i.e., the input
        orientation is short edge first). Exceptions occur with envelope sizes
        (Env10 and EnvDL) on older DeskJet printers (500 and 500C, possibly
        also 400) where envelopes have to be fed long edge first.

    The key conclusion to be drawn from this is that the layer represented by
    the functions in this interface does not have any information on the media
    orientation in raster space nor can this information be formulated in PCL
    and passed through this layer. It is information shared between the printer
    and the client code calling this interface, and the caller must therefore
    obtain it from an external source.

    In introducing the concept of a "raster space" I have deliberately avoided
    the term "physical page" used in the PCL documentation. The latter is not
    made sufficiently precise, and for printing raster data as done here the
    first concept is the one required.

    Two Page Size codes are marked "PS3". These are from Adobe's "LanguageLevel
    3 Specification and Adobe PostScript 3 Version 3010 Product Supplement"
    (dated 1997-10-10), page 352, where eight PCL-5 page size codes are listed.
    Six of these agree with PCL-3 page size codes, and the remaining two are
    those inserted here and marked "PS3". I have not heard of any PCL-3 printer
    supporting these two sizes as discrete sizes.

    The identifiers are usually named after the corresponding 'mediaOption'
    keyword from Adobe's PPD 4.3 specification.
*/
typedef enum {
  pcl_ps_default = 0,	/* default size for the printer (e.g., BPD03004 or
    BPD02926; but note that in some versions of these documents the text
    representation is wrong, you should look at the hex codes) */
  pcl_ps_Executive = 1,	/* US Executive (7.25 x 10.5 in).
    According to Adobe's PPD 4.3 specification, media sizes called by this name
    vary by up to 1/2 in. */
  pcl_ps_Letter = 2,	/* US Letter (8.5 x 11 in) */
  pcl_ps_Legal = 3,	/* US Legal (8.5 x 14 in) */
  pcl_ps_Tabloid = 6,	/* US Tabloid (11 x 17 in) or Ledger (17 x 11 in)
                           (DJ1120C). The designation as Tabloid versus Ledger
                           is not always consistent. I'm following PPD 4.3. */
  pcl_ps_Statement = 15, /* US Statement (5.5 x 8.5 in) (DJ1120C) */
  pcl_ps_HPSuperB = 16,	/* Super B (13 x 19 in (330 x 483 mm) according to
                           DJ1120C, while 305 x 487 mm according to PPD 4.3).
                           Not supported in PCL 3 according to BPD07645
                           (HP 2500C). */
  pcl_ps_A6 = 24,	/* ISO/JIS A6 (105 x 148 mm) */
  pcl_ps_A5 = 25,	/* ISO/JIS A5 (148 x 210 mm) */
  pcl_ps_A4 = 26,	/* ISO/JIS A4 (210 x 297 mm) */
  pcl_ps_A3 = 27,	/* ISO/JIS A3 (297 x 420 mm) (DJ1120C, BPL02327) */
  pcl_ps_JISB5 = 45,	/* JIS B5 (182 x 257 mm) */
  pcl_ps_JISB4 = 46,	/* JIS B4 (257 x 364 mm) (DJ1120C, BPL02327) */
  pcl_ps_Postcard = 71,	/* Japanese Hagaki postcard (100 x 148 mm) */
  pcl_ps_DoublePostcard = 72,	/* Japanese Oufuko-Hagaki postcard
                           (148 x 200 mm) (DJ6/8) */
  pcl_ps_A6Card = 73,	/* "ISO and JIS A6 card" (DJ6/8, DJ1120C). This is the
                           value given for most DeskJets supporting A6. I do
                           not know what the difference between this value and
                           pcl_ps_A6 (24) is. */
  pcl_ps_Index4x6in = 74,  /* US Index card (4 x 6 in) */
  pcl_ps_Index5x8in = 75,  /* US Index card (5 x 8 in) */
  pcl_ps_Index3x5in = 78,  /* US Index card (3 x 5 in) */
  pcl_ps_EnvMonarch = 80,  /* US Monarch (3.875 x 7.5 in) (PS3, BPL02327) */
  pcl_ps_Env10 = 81,	/* US No. 10 envelope (4.125 x 9.5 in) */
  pcl_ps_Env10_Negative = -81,  /* also US No. 10 envelope */
  /* According to Lexmark-PTR, 89 is US No. 9 envelope (3.875 x 8.875 in). */
  pcl_ps_EnvDL = 90,	/* ISO DL (110 x 220 mm) */
  pcl_ps_EnvDL_Negative = -90,  /* also ISO DL. This value is hinted at in
    TRG500 p. 3-2 in the "Range" line at the bottom of the table and it is
    explicitly listed in DJ3/4 on p. 36. On a DJ 850C, I found no difference in
    text orientation with respect to the value 90. */
  pcl_ps_EnvC5 = 91,	/* ISO C5 (162 x 229 mm) in PCL 5 (PS3, BPL02327) */
  pcl_ps_EnvC6 = 92,	/* ISO C6 (114 x 162 mm) */
  pcl_ps_ISOB5 = 100,	/* ISO B5 (176 x 250 mm, BPL02327; the dimensions in
    millimetres in BPL02327 are wrong, those in inches are right) */
  pcl_ps_CustomPageSize = 101,  /* Custom page size */
  pcl_ps_EnvUS_A2 = 109,  /* US A2 envelope (4.375 x 5.75 in) */
  pcl_ps_EnvChou3 = 110, /* Japanese long Envelope #3 (120 mm x 235 mm)
                            (DJ1120C) */
  pcl_ps_EnvChou4 = 111, /* Japanese long envelope #4 (90 mm x 205 mm)
                            (DJ1120C) */
  pcl_ps_EnvKaku2 = 113	/* Kaku envelope (240 x 332 mm) (DJ1120C) */
} pcl_PageSize;

/*****************************************************************************/

/* Raster graphics compression methods */
typedef enum {
  pcl_cm_none = 0,	/* Unencoded, non-compressed method */
  pcl_cm_rl = 1,	/* Run-Length Encoding */
  pcl_cm_tiff = 2,	/* Tagged Image File Format (TIFF) revision 4.0
                           "packbits" encoding */
  pcl_cm_delta = 3,	/* Delta Row Compression */
  pcl_cm_adaptive = 5,	/* Adaptive Compression */
  pcl_cm_crdr = 9	/* Compressed Replacement Delta Row Encoding */
} pcl_Compression;

/* Macro to test for differential compression methods */
#define pcl_cm_is_differential(cm)		\
  ((cm) == pcl_cm_delta || (cm) == pcl_cm_adaptive || (cm) == pcl_cm_crdr)

/*****************************************************************************/

/* Type and constants for boolean variables */
typedef int pcl_bool;
#ifndef FALSE
#define FALSE	0
#else
#if FALSE != 0
#error "FALSE is defined as non-zero."
#endif	/* FALSE != 0 */
#endif	/* FALSE */
#ifndef TRUE
#define TRUE	1
#else
#if TRUE == 0
#error "TRUE is defined as zero."
#endif	/* TRUE == 0 */
#endif	/* TRUE */

/* Palette (process colour model) */
typedef enum {
  pcl_no_palette,	/* Don't send Number of Planes per Row */
  pcl_black,
  pcl_CMY,
  pcl_CMYK,
  pcl_RGB,		/* Using the RGB palette is discouraged by HP. */
  pcl_any_palette	/* Don't use this value unless you know what you are
                           doing. */
} pcl_Palette;

/* Information per colorant for raster images */
typedef struct {
  unsigned int hres, vres;	/* Resolution in ppi. The orientation refers to
    raster space. Both values must be non-zero. */
  unsigned int levels;	/* Number of intensity levels. 2 or larger. */
} pcl_ColorantState;

/*****************************************************************************/

/* 'pcl_Octet' is used to store 8-bit bytes as unsigned numbers. */
#ifndef _pcl_Octet_defined
#define _pcl_Octet_defined
typedef unsigned char pcl_Octet;
#endif

/* Octet strings */
typedef struct {
  pcl_Octet *str;	/* Data area for storing the octet string. */
  int length;		/* Length of 'str' in octets (non-negative). */
} pcl_OctetString;
/*  A "valid" pcl_OctetString is one where either 'length' is zero, or
    'length' is positive and 'str' is non-NULL and points to a storage
    area of at least 'length' octets.
    A "zero" pcl_OctetString is one where 'length' is zero.
*/

/*****************************************************************************/

/* File-global data */
typedef struct {
  pcl_Level level;

  /* Printer initialization */
  int NULs_to_send;
  char
    *PJL_job,		/* PJL job name. Ignored if NULL. */
    *PJL_language;	/* PJL personality. Ignored if NULL. */

  /* Additional initialization commands (ignored if zero) */
  pcl_OctetString
    init1,		/* send immediately after resetting the printer */
    init2;		/* send after all other initialization commands */

  /* Media */
  pcl_PageSize size;
  int
    media_type,
    media_source,	/* 0: don't request a particular media source */
    media_destination,	/* 0: don't request a particular media destination.
                           Not based on HP documentation. */
    duplex;		/* -1: don't request anything in this respect,
      0: simplex, 1: duplex long-edge binding, 2: duplex short-edge binding
      (BPL02705). I assume the correct interpretation of the duplex values to
      be:
        1: duplex, print back side with the same top edge in raster space
        2: duplex, print back side with top and bottom edges exchanged in
           raster space
      */
  pcl_bool manual_feed;

  /* Print quality selection. Which of these variables are used depends on the
     PCL level chosen. */
  int
    print_quality,
    depletion,		/* 0: no depletion command */
    shingling,
    raster_graphics_quality;

  /* Colour information */
  pcl_Palette palette;
  unsigned int number_of_colorants;
  pcl_ColorantState *colorant;
   /* This variable must either be NULL or point to an array of at least
      'number_of_colorants' elements. The order of colorants is (K)(CMY) except
      for 'pcl_any_palette'. If the pointer is NULL, 'colorant_array' is used
      instead. */
  pcl_ColorantState colorant_array[4];
    /* Used if 'colorant' is NULL and with the same meaning. This is only
       possible if 'number_of_colorants' is at most 4. */
  pcl_bool order_CMYK;
   /* For pcl_CMYK, should the order of bit planes when sent to the printer be
      reversed to CMYK instead of KCMY? This is not standard PCL but is needed
      by at least one Olivetti printer (Olivetti JP792). */

  int dry_time;	/* negative for "don't send this command" */
  pcl_Compression compression;

  /* Derived information managed by the routines declared here. These fields
     have reliable values only after a call to plc3_init_file(). */
  unsigned short number_of_bitplanes;
  unsigned short black_planes;
  unsigned int minvres;	/* minimal vertical resolution */
} pcl_FileData;

/*****************************************************************************/

/* Types for raster data */

typedef struct {
  /* Data to be provided by the caller */
  unsigned int width;	/* maximal length of raster lines in pixels at the
    lowest horizontal resolution. This may be zero to indicate "as large as
    possible", but this could waste printer memory. Always set this when
    using an RGB palette unless you have reliable data on the clipping
    boundary. The value should probably always be a multiple of 8. */
  pcl_FileData *global;		/* must point to the data used in the last call
                                   to pcl3_init_file() */
  pcl_OctetString
    *previous, *next;
    /*  These variables point to the sequences of bit planes for the old and
        the new strip group. The total number of bit planes per group can be
        obtained in the 'number_of_bitplanes' parameter in '*global'.

        The order of bit planes within a strip group is as follows:
        - Each strip group consists of a number of "colorant strips", one for
          each colorant. All strips in such a group cover the same region on
          the page. Except for 'pcl_RGB', colorant strips are ordered in the
          sequence (K)(CMY), independent of 'order_CMYK'. For an RGB palette,
          the order is (of course) RGB.
        - Each colorant strip contains a number of pixel lines for this
          colorant. If there is more than one line, the lines are ordered from
          top to bottom as seen from raster space. The number of lines is
          the ratio of that colorant's vertical resolution to the smallest
          vertical resolution.
        - Within each pixel line for a colorant, bit planes are ordered from
          least to most significant. The number of bit planes within a line is
          pcl3_levels_to_planes(levels) for this colorant.
          When bit planes are again combined into lines for a particular
          colorant, the resulting value per pixel denotes the intensity for
          that colorant. A value of zero denotes absence of that colorant.
        'previous' will be ignored (and may then be NULL) unless a differential
        compression method is requested (pcl_cm_is_differential()).

        When using an RGB palette you should always send bit planes extending
        over the whole 'width' because shorter bit planes are implicitly
        extended with null octets and a pixel value of zero denotes black in an
        RGB palette which is not usually desired.
    */
  pcl_Octet *workspace[2];
    /* Storage for the use of these routines. workspace[0] must be non-NULL,
       workspace[1] will be ignored except for Delta Row Compression. */
  size_t workspace_allocated;
    /* Length allocated for each non-NULL 'workspace[]'. This should be at
       least 2 larger than the longest possible bit plane, otherwise raster
       data might have to be sent in uncompressed form even if that would take
       more space. */

  /* Internal data for these routines */
  pcl_Compression current_compression;	/* last compression method used */
  pcl_OctetString **seed_plane;	/* seed plane indexed by plane index */
} pcl_RasterData;

/******************************************************************************

  The routines assume a simple state machine:

  - You are either on a page or between pages.
  - If you are on a page, you are either in raster mode or not.

  You should call pcl3_init_file() before the first page and between pages
  whenever you change the file-global data (like the resolution). Otherwise,
  use the ..._begin() and ..._end() functions to navigate between the states.

  Look into the implementation file for detailed interface descriptions for
  these routines.

  The error conventions are the same for all file, page and raster functions:
  - A return code of zero indicates success.
  - A positive return code indicates an argument error. This should be
    avoidable by careful programming.
  - A negative return code indicates an environment error (e.g., disk overflow).
  If a function returns with a non-zero code, an error message will have been
  issued on standard error.

******************************************************************************/

/* Auxiliary functions */
extern unsigned int pcl3_levels_to_planes(unsigned int levels);
extern int pcl3_set_printquality(pcl_FileData *data, int quality);
extern int pcl3_set_mediatype(pcl_FileData *data, int mediatype);
extern int pcl3_set_oldquality(pcl_FileData *data);
extern int pcl_compress(pcl_Compression method, const pcl_OctetString *in,
  const pcl_OctetString *prev, pcl_OctetString *out);

/* File and page functions */
extern int pcl3_init_file(gs_memory_t *mem, gp_file *out, pcl_FileData *global);
extern int pcl3_begin_page(gp_file *out, pcl_FileData *global);
extern int pcl3_end_page(gp_file *out, pcl_FileData *global);
extern int pcl3_end_file(gp_file *out, pcl_FileData *global);

/* Raster functions */
extern int pcl3_begin_raster(gp_file *out, pcl_RasterData *data);
extern int pcl3_skip_groups(gp_file *out, pcl_RasterData *data,
  unsigned int count);
extern int pcl3_transfer_group(gp_file *out, pcl_RasterData *data);
extern int pcl3_end_raster(gp_file *out, pcl_RasterData *data);

/*****************************************************************************/

#endif	/* Inclusion protection */
