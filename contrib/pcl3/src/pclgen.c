/******************************************************************************
  File:     $Id: pclgen.c,v 1.21 2001/04/29 10:37:08 Martin Rel $
  Contents: PCL-generating routines
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000, 2001 by Martin Lottermoser		      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  In the implementation of these and other functions I have mainly used the
  following documents:

  - Hewlett-Packard
    "Technical Reference Guide for the HP DeskJet 500 Series Printers"
    First edition, October 1994
    Manual Part Number: C2170-90099
    (Quoted as "TRG500")
  - Hewlett-Packard
    "Hewlett-Packard 300 and 400 Series DeskJet Printers - Software Developer's
      Guide"
    January 1996
    (Quoted as "DJ3/4")
  - Hewlett-Packard
    "HP DeskJet 600/800 Series Printers - Software Developer's PCL Guide"
    Fifth edition, October 1997
    (Quoted as "DJ6/8")
  - Hewlett-Packard
    "DeskJet 1120C Printer - Software Developer's PCL Guide"
    First printing, December 1997. Version 1.0.
    (Quoted as "DJ1120C")
  - Hewlett-Packard
    "Printer Job Language Technical Reference Manual"
    Edition 10, October 1997. HP Part No. 5021-0380.
    (Quoted as "PJLTRM")
  - Lexmark
    "Printer Technical Reference, Version 1.1"
    First edition, February 1999
    (Quoted as "Lexmark-PTR". It deals with PCL 5 and PCL 6.)

  In addition, some other documents are quoted in a form like "BPD02926". These
  were obtained from http://www.hp.com, usually from the directory
  cposupport/printers/support_doc. BPD02926, for example, (a short description
  of PCL commands for series-800 DeskJets) could be found as the file
  bpd02926.html in that directory.

******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Standard headers */
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Special headers */
#include "pclgen.h"

/*****************************************************************************/

/* Prefix for error messages */
#define ERRPREF	"? pclgen: "

/* The usual array-size macro */
#define array_size(a)	(sizeof(a)/sizeof(a[0]))

/* Macro to check whether an octet is an ASCII letter. Note that we can't use
   the isalpha() function because we might be operating in an internationalized
   environment. */
#define is_letter(c)  ((0x41 <= (c) && (c) <= 0x5A) || (0x61 <= (c) && (c) <= 0x7A))

/* Same for digits */
#define is_digit(c)	(0x30 <= (c) && (c) <= 0x39)

/******************************************************************************

  Function: pcl3_levels_to_planes

  This function returns ceiling(ld(levels)), the number of bitplanes needed to
  store the specified number of intensity levels, or 0 if 'levels' is zero.
  'levels' must be <= (ULONG_MAX+1)/2.

******************************************************************************/

unsigned int pcl3_levels_to_planes(unsigned int levels)
{
  unsigned long
    power = 1;
  unsigned int
    planes = 0;
  /* power == 2^planes */

  while (power < levels) {
    power *= 2;	/* multiplication is faster than division */
    planes++;
  }
  /* levels == 0 or 2^(planes-1) < levels <= 2^planes */

  return planes;
}

/******************************************************************************

  Function: send_ERG

  This function sends an "End Raster Graphics" command to 'out', choosing its
  form according to 'level'.

******************************************************************************/

static void send_ERG(gp_file *out, pcl_Level level)
{
  /* PCL: End Raster Graphics/End Raster */
  gp_fputs("\033*r", out);
  if (pcl_use_oldERG(level)) gp_fputc('B', out);
  else if (level == pcl_level_3plus_ERG_both) gp_fputs("bC", out);
  else gp_fputc('C', out);

  return;
}

/******************************************************************************

  Function: pcl3_init_file

  This function must be called to initialize the printer before the first page.
  The 'data' should remain constant within a page. Whenever one changes 'data'
  between pages, this function should be called again.

  The function follows the return code rules described in pclgen.h. It will
  perform a number of validity checks on 'data'. No checks will be performed on
  fields like 'print_quality' which can be passed to the printer without having
  to be interpreted by this component.

  The routine will configure the printer such that the top left corner of the
  logical page is the top left corner of the printable area in raster space.

  The file 'out' should be a binary file for this and all other output
  functions.

******************************************************************************/

int pcl3_init_file(gs_memory_t *mem, gp_file *out, pcl_FileData *data)
{
  pcl_bool needs_CRD = (data && data->level == pcl_level_3plus_CRD_only);
    /* Do we need Configure Raster Data? */
  int j;
  const pcl_ColorantState *colorant = NULL;
  unsigned int maxhres = 0, maxvres = 0;  /* maximal resolutions in ppi */

  /* Check validity of the arguments */
  {
    pcl_bool invalid;

    invalid = (out == NULL || data == NULL);
    if (invalid)
      errprintf(mem, ERRPREF "Null pointer passed to pcl3_init_file().\n");
    else {
      /* Palette und colorants */
      switch(data->palette) {
        case pcl_no_palette:
        /*FALLTHROUGH*/
        case pcl_black: invalid = data->number_of_colorants != 1; break;
        case pcl_CMY:   invalid = data->number_of_colorants != 3; break;
        case pcl_RGB:   invalid = data->number_of_colorants != 3; break;
        case pcl_CMYK:  invalid = data->number_of_colorants != 4; break;
        default: invalid = data->number_of_colorants <= 0;
      }
      if (invalid)
        errprintf(mem, ERRPREF
          "Palette specification and number of colorants are inconsistent.\n");
      else {
        if (data->colorant == NULL) colorant = data->colorant_array;
        else colorant = data->colorant;

        /* First pass over colorants: find minimal and maximal resolutions,
           check the number of intensity levels */
        data->minvres = colorant[0].vres;
        for (j = 0; j < data->number_of_colorants; j++) {
          if (colorant[j].hres <= 0 || colorant[j].vres <= 0) {
            invalid = TRUE;
            errprintf(mem, ERRPREF
              "The resolution for colorant %d is not positive: %u x %u ppi.\n",
              j, colorant[j].hres, colorant[j].vres);
          }
          else {
            if (colorant[j].vres < data->minvres)
              data->minvres = colorant[j].vres;
            if (colorant[j].hres > maxhres) maxhres = colorant[j].hres;
            if (colorant[j].vres > maxvres) maxvres = colorant[j].vres;
          }
          if (colorant[j].levels < 2 || 0xFFFF < colorant[j].levels) {
            invalid = TRUE;
            errprintf(mem, ERRPREF "The number of intensity levels for "
              "colorant %d is %u instead of at least 2 and at most 65535.\n",
              j, colorant[j].levels);
            /* Actually, DJ6/8 p. 68 requires the levels to be in the range
               2-255, but as there are two octets available in the CRD format
               this seems unnecessarily strict. */
          }
        }

        /* Check for relations between resolutions and the need for CRD */
        if (!invalid)  /* this implies that all resolutions are positive */
          for (j = 0; j < data->number_of_colorants; j++) {
            /* If there is more than one resolution or more than 2 levels we
               need CRD. Note that we compare with 'maxhres' in both lines: */
            if (maxhres != colorant[j].hres ||
                maxhres != colorant[j].vres || colorant[j].levels > 2)
              needs_CRD = TRUE;

            /*  DJ6/8 states that the highest horizontal resolution must be a
                multiple of all lower horizontal resolutions and the same for
                the set of vertical resolutions. The reason is not given but
                these properties imply that it is possible to operate uniformly
                on a grid at the highest resolution.
                In contrast, the way in which raster data are transferred
                (strips with a height equal to the reciprocal of the lowest
                vertical resolution) logically requires that all vertical
                resolutions are multiples of the lowest vertical resolution.
                This condition is not stated in DJ6/8 but it is satisfied by
                all examples given in that document. This includes example 4 on
                page 72 which is meant to illustrate the maximum flexibility of
                format 2 and which describes a situation where the
                corresponding condition does not hold for the horizontal
                resolutions.
            */
            if (colorant[j].vres % data->minvres != 0) {
              invalid = TRUE;
              errprintf(mem, ERRPREF
                "The vertical resolution for colorant %d (%u ppi) is not a "
                "multiple of the lowest vertical resolution (%u ppi).\n",
                j, colorant[j].vres, data->minvres);
            }
            if (maxhres % colorant[j].hres != 0) {
              invalid = TRUE;
              errprintf(mem, ERRPREF
                "The highest horizontal resolution (%u ppi) is not a multiple "
                "of the horizontal resolution for colorant %d (%u ppi).\n",
                maxhres, j, colorant[j].hres);
            }
            if (maxvres % colorant[j].vres != 0) {
              invalid = TRUE;
              errprintf(mem, ERRPREF
                "The highest vertical resolution (%u ppi) is not a multiple "
                "of the vertical resolution for colorant %d (%u ppi).\n",
                maxvres, j, colorant[j].vres);
            }
          }
      }

      if (needs_CRD && data->palette == pcl_RGB) {
        invalid = TRUE;
        if (data->level == pcl_level_3plus_CRD_only)
          errprintf(mem, ERRPREF
            "You can't use an RGB palette at the requested PCL level.\n");
        else
          errprintf(mem, ERRPREF "The specified structure of resolutions and intensity "
            "levels is not possible with an RGB palette.\n");
      }
      if (needs_CRD && !pcl_has_CRD(data->level)) {
        invalid = TRUE;
        errprintf(mem, ERRPREF "The specified structure of resolutions and intensity "
          "levels is not possible at the requested PCL level.\n");
      }
      if (data->palette == pcl_any_palette) {
        needs_CRD = TRUE;
        if (!pcl_has_CRD(data->level)) {
          invalid = TRUE;
          errprintf(mem, ERRPREF "The specified palette is not possible at the "
            "requested PCL level.\n");
        }
      }
      if (needs_CRD && (maxhres > 0xFFFF || maxvres > 0xFFFF)) {
        errprintf(mem, ERRPREF "Resolutions may be at most 65535 ppi when more than one "
          "resolution or more than two intensity levels are requested.\n");
        invalid = TRUE;
      }
      if (data->order_CMYK && data->palette != pcl_CMYK) {
        errprintf(mem, ERRPREF
          "Ordering bit planes as CMYK instead of KCMY is only meaningful\n"
          "  for a CMYK palette.\n");
        invalid = TRUE;
      }

      /* Check PJL job name */
      if (data->PJL_job != NULL) {
        const unsigned char *s = (const unsigned char *)data->PJL_job;

        /* Permissible characters are HT and the octets 32-255 with the
           exception of '"' (PJLTRM, with some corrections). */
        while (*s != '\0' && (*s == '\t' || (32 <= *s && *s != '"'))) s++;
        if (*s != '\0') {
          errprintf(mem,
            ERRPREF "Illegal character in PJL job name (code 0x%02X).\n", *s);
          invalid = TRUE;
        }

        /* There is a maximum of 80 "significant characters" (PJLTRM).
           According to PJLTRM, p. D-8, "String too long" is a parser warning
           and leads to a part of the command being ignored. I play it safe.
           There would also be a warning for an empty string but we treat that
           case differently anyway (see below). */
        if (strlen(data->PJL_job) > 80) {
          errprintf(mem, ERRPREF "PJL job name is too long (more than 80 characters).\n");
          invalid = TRUE;
        }
      }

      /* Check PJL personality name */
      if (data->PJL_language != NULL) {
        const char *s = data->PJL_language;

        /* PJLTRM does not give explicit lexical conventions for personality
           names but from general considerations it should be an "alphanumeric
           variable". The latter must start with a letter and may consist of
           letters and digits. */
        if (is_letter(*s)) do s++; while (is_letter(*s) || is_digit(*s));

        if (*data->PJL_language == '\0') {
          errprintf(mem, ERRPREF "Empty PJL language name.\n");
          invalid = TRUE;
        }
        else if (*s != '\0') {
          errprintf(mem,
            ERRPREF "Illegal character in PJL language name (code 0x%02X).\n",
            *s);
          invalid = TRUE;
        }
      }
    }

    if (invalid) return +1;
  }

  /*  Practically every output file from an HP driver I have seen starts with
      600 and one even with 9600 NUL characters. This seems unnecessary
      and is undocumented, but just in case that there are situations where it
      might make a difference, this module provides the capability. */
  for (j = 0; j < data->NULs_to_send; j++) gp_fputc('\0', out);

  /*  Issue PJL commands if requested. Most newer HP drivers for PCL-3 printers
      follow the NULs with a PJL statement to enter the PCL language
      interpreter. The language name varies (so far I've seen PCL3GUI and
      PCLSLEEK). Some drivers also generate JOB/EOJ statements.
      Interestingly enough, I have seen output from two HP drivers which, at
      the beginning of the job, generated first Printer Reset and then UEL.
      This is wrong (PJLTRM).
  */
  if (data->PJL_job != NULL || data->PJL_language != NULL) {
    gp_fputs("\033%-12345X", out);	/* Universal Exit Language (UEL) */

    /* Start of job */
    if (data->PJL_job != NULL) {
      gp_fputs("@PJL JOB", out);
      if (*data->PJL_job != '\0') gp_fprintf(out, " NAME=\"%s\"", data->PJL_job);
      gp_fputc('\n', out);
    }

    /* Switch personality */
    if (data->PJL_language != NULL)
      gp_fprintf(out, "@PJL ENTER LANGUAGE=%s\n", data->PJL_language);
  }

  /* PCL: Printer Reset */
  gp_fputs("\033E", out);

  /* Additional initialization */
  if (data->init1.length > 0)
    gp_fwrite(data->init1.str, sizeof(pcl_Octet), data->init1.length, out);

  /* Page layout initialization */
  gp_fprintf(out,
    "\033&l%da"	/* PCL: Page Size */
    "0o"	/* PCL: Page Orientation/Orientation: portrait */
    "0L",	/* PCL: Perforation Skip Mode: off. This also effectively sets
                   the PCL top margin to zero. */
    (int) data->size
  );

  /* Media source */
  if (data->media_source != 0)
    gp_fprintf(out, "\033&l%dH", data->media_source);	/* PCL: Media Source */
      /*  Note that a value of zero for the Media Source command means
          "eject the current page". Hence we are losing no functionality by
          reserving 0 to mean "no Media Source request". */
  if (data->media_source != 2 && data->manual_feed) gp_fputs("\033&l2H", out);
   /* I am using two Media Source commands here in case the value 2 means
      "manual feed from the last selected media source" on some printer. */

  /*  Media destination. This is included here merely in the hope that, if a
      PCL-3 printer should support such a feature, the command used is the same
      as in PCL 5/6. At present, I know of no such printer. */
  if (data->media_destination != 0)
    gp_fprintf(out, "\033&l%dG", data->media_destination);
     /* PCL: Paper Destination (BPL02705). A value of zero means "auto select".
      */

  /*  Duplex. Again, I have only PCL-5 documentation for this, but in this case
      I know that the DJ 970C uses the command. */
  if (data->duplex != -1)
    gp_fprintf(out, "\033&l%dS", data->duplex);	/* PCL: Simplex/Duplex Print */

  /* Print quality */
  if (pcl_use_oldquality(data->level)) {
    gp_fprintf(out, "\033*r%dQ", data->raster_graphics_quality);
      /* PCL: Raster Graphics Quality */
    if (data->level > pcl_level_3plus_DJ500)
      gp_fprintf(out, "\033*o%dQ", data->shingling);
        /* PCL: Set Raster Graphics Shingling/Mechanical Print Quality */
    if (data->depletion != 0)
     /*	According to TRG500 p. 6-32, depletion makes no sense for monochrome
        data.  Besides, not all printers react to this command. Hence I'm
        handing the decision to the caller. Note that permitted values are 1-5.
      */
      gp_fprintf(out, "\033*o%dD", data->depletion);
        /* PCL: Set Raster Graphics Depletion */
  }
  else
    gp_fprintf(out,
      "\033&l%dM"	/* PCL: Media Type */
      "\033*o%dM",	/* PCL: Print Quality */
      data->media_type,
      data->print_quality
    );

  /*  PCL: Set Dry Time/Dry Timer. This command is ignored by newer printers
      and is obsolete for every printer supporting the new Media Type and Print
      Quality commands. Because I am uncertain about what "current" means in
      the description of the value 0 ("default dry time for the current print
      quality", TRG500 p. 2-4), I'm putting the command here after the quality
      commands. */
  if (data->dry_time >= 0) gp_fprintf(out, "\033&b%dT", data->dry_time);

  /* End Raster Graphics. This provides a known graphics state, see
     TRG500 p. 6-25, but is probably superfluous here because of the Printer
     Reset command. */
  send_ERG(out, data->level);

  if (data->level != pcl_level_3plus_CRD_only)
    /* PCL: Set Raster Graphics Resolution/Raster Resolution */
    gp_fprintf(out, "\033*t%uR", maxhres < maxvres? maxvres: maxhres);
    /* If different x and y resolutions have been demanded but the printer does
       not support the combination, choosing the larger value here will prevent
       printing beyond the sheet---provided the printer accepts this resolution.
     */

  /* Set PCL unit to reciprocal of largest resolution */
  if (data->level >= pcl_level_3plus_S68)
    gp_fprintf(out, "\033&u%uD", maxhres < maxvres? maxvres: maxhres);
     /* PCL: Unit of Measure. This is not documented but merely mentioned in
        DJ6/8. All HP drivers for newer printers I've looked at (admittedly not
        many) generate this command, including a driver for the DJ 540.
        Actually, as the routines here send a Move CAP Horizontal/Vertical
        (PCL Units) command only for position 0, the units should be irrelevant.
      */

  /* Colour planes */
  if (data->level != pcl_level_3plus_CRD_only &&
      data->palette != pcl_no_palette && data->palette != pcl_any_palette)
    gp_fprintf(out, "\033*r%dU",
        /* PCL: Set Number of Planes per Row/Simple Color */
      data->palette == pcl_RGB? 3:	/* RGB palette */
      -(int)data->number_of_colorants);	/* (K)(CMY) palette */

  /* Configure Raster Data */
  if (needs_CRD) {
    gp_fprintf(out, "\033*g%uW"	/* PCL: Configure Raster Data */
      "\002%c",		/* Format 2: Complex Direct Planar */
      2 + 6*data->number_of_colorants, data->number_of_colorants);

    for (j = 0; j < data->number_of_colorants; j++)
      gp_fprintf(out, "%c%c%c%c%c%c",
        /*  Note that %c expects an 'int' (and converts it to 'unsigned char').
         */
        colorant[j].hres/256,   colorant[j].hres%256,
        colorant[j].vres/256,   colorant[j].vres%256,
        colorant[j].levels/256, colorant[j].levels%256);
  }

  if (gp_ferror(out)) {
    errprintf(mem, ERRPREF "Unidentified system error while writing the output file.\n");
    return -1;
  }

  /* Additional initialization */
  if (data->init2.length > 0)
    gp_fwrite(data->init2.str, sizeof(pcl_Octet), data->init2.length, out);

  /* Determine and set the number of bit planes */
  if (data->palette == pcl_CMY || data->palette == pcl_RGB)
    data->black_planes = 0;
  else data->black_planes =
    pcl3_levels_to_planes(colorant[0].levels)*(colorant[0].vres/data->minvres);
  data->number_of_bitplanes = 0;
  for (j = 0; j < data->number_of_colorants; j++)
    data->number_of_bitplanes +=
      pcl3_levels_to_planes(colorant[j].levels)*
        (colorant[j].vres/data->minvres);

  return 0;
}

/******************************************************************************

  Function: pcl3_begin_page

  This function sets CAP on the top margin of the logical page. It may only
  be called after pcl3_init_file() and not while a page is still open.

******************************************************************************/

int pcl3_begin_page(gp_file *out, pcl_FileData *global)
{
  gp_fputs("\033*p0Y", out);
    /* PCL: Vertical Cursor Positioning by Dots/Move CAP Vertical (PCL Units) */

  return 0;
}

/******************************************************************************

  Function: pcl3_begin_raster

  This function starts raster mode at the left margin of the logical page
  and at the current height.

  The function may only be called within a page and not in raster mode.

  The raster data structure '*data' will be checked for validity and parts of
  the data may be modified. In particular, the caller should already have
  allocated appropriate storage in the 'next' and, for differential compression
  methods, 'previous' arrays. Until raster mode is terminated, the caller may
  only modify the storage currently pointed to by the pointers in 'next' and
  their corresponding length fields.

******************************************************************************/

int pcl3_begin_raster(gp_file *out, pcl_RasterData *data)
{
  const pcl_FileData *global = NULL;
  int j;

  /* Check 'data' for validity */
  {
    pcl_bool invalid;

    invalid = (data == NULL || data->global == NULL || data->next == NULL ||
      data->workspace[0] == NULL || data->workspace_allocated <= 0);
    if (!invalid) {
      global = data->global;

      for (j = 0;
        j < global->number_of_bitplanes &&
          (data->next[j].length == 0 || data->next[j].str != NULL);
        j++);
      invalid = j < global->number_of_bitplanes;
      if (!invalid && pcl_cm_is_differential(global->compression)) {
        invalid = (data->previous == NULL ||
                   (global->compression == pcl_cm_delta &&
                    data->workspace[1] == NULL));
        if (!invalid) {
          for (j = 0;
            j < global->number_of_bitplanes &&
              (data->previous[j].length == 0 || data->previous[j].str != NULL);
            j++);
          invalid = j < global->number_of_bitplanes;
        }
      }
    }

    if (invalid) {
      errprintf(out->memory, ERRPREF "Invalid data structure passed to pcl3_begin_raster().\n");
      return +1;
    }
  }

  /* Allocate the seed plane array */
  data->seed_plane = (pcl_OctetString **)
    malloc(global->number_of_bitplanes*sizeof(pcl_OctetString *));
  if (data->seed_plane == NULL) {
    errprintf(out->memory, ERRPREF "Memory allocation failure in pcl3_begin_raster().\n");
    return -1;
  }
  memset(data->seed_plane, 0,
    global->number_of_bitplanes*sizeof(pcl_OctetString *));

  /* Use the seed plane array for differential compression methods */
  if (pcl_cm_is_differential(global->compression)) {
    /*  HP's documentation is a bit obscure concerning what the seed plane for
        a particular bit plane is. Most of the documentation talks only about
        seed rows (and "rows" consist of planes), but then one suddenly comes
        across a statement like "a separate seed row is maintained for each
        graphic plane" (DJ6/8 p. 57). I've also never found a statement what
        the seed row/plane/whatever for a bitplane in a strip group with
        multiple lines or multiple planes per colorant is, except that DJ6/8
        (p. 60) states explicitly that one cannot use Seed Row Source in that
        situation to select it.

        I therefore have to make a few assumptions. The following seem sensible:
        - The PCL interpreter maintains independent "seed lines" for each
          colorant. Each line consists of independent seed planes, one for each
          bit plane in a pixel line for that colorant (i.e., there are
          pcl3_levels_to_planes(levels) seed planes for a colorant accepting
          'levels' intensity levels).
        - If the current compression method (this is a global property of the
          interpreter) is a differential one, a bit plane is interpreted as a
          delta plane with respect to the corresponding bit plane in the
          current colorant's preceding line.
    */

    int strip;
    const pcl_ColorantState *colorant = NULL;
    int plane = 0;

    if (global->colorant == NULL) colorant = global->colorant_array;
    else colorant = global->colorant;

    for (strip = 0; strip < global->number_of_colorants; strip++) {
      int lines = colorant[strip].vres/global->minvres;
      int planes = pcl3_levels_to_planes(colorant[strip].levels);
      int l, p;

      /* The first line of the colorant strip refers to the last line in the
         preceding strip. I'm assuming Seed Row Source == 0 here. */
      for (p = 0; p < planes; p++, plane++)
        data->seed_plane[plane] = data->previous + plane + (lines - 1)*planes;

      /* Subsequent lines refer to the preceding line in the current strip
         group */
      for (l = 1; l < lines; l++)
        for (p = 0; p < planes; p++, plane++)
          data->seed_plane[plane] = data->next + plane - planes;
    }
  }

  /* Start raster mode */
  if (data->width > 0) {
    gp_fprintf(out, "\033*r%uS", data->width);
    /* PCL: Set Raster Graphics Width/Source Raster Width. The value is the
       number of pixels at the lowest horizontal resolution (see DJ6/8 p. 66).
       This is reset by End Raster Graphics. */
  }
  gp_fputs("\033*p0X"	/* PCL: Horizontal Cursor Positioning by Dots */
        "\033*r1A", out);
          /* PCL: Start Raster Graphics: at current position */

  /* After Start Raster Graphics the seed row consists of all zeroes
     (DJ6/8 p. 50). */
  if (pcl_cm_is_differential(global->compression))
    for (j = 0; j < global->number_of_bitplanes; j++)
      data->previous[j].length = 0;

  gp_fputs("\033*b", out);
    /* We use combined escape sequences, all with this prefix. */

  /*  The old End Raster Graphics command (with 'B') does not reset the
      compression method to 'pcl_cm_none'. We could keep track of the current
      compression method, but this would mean that we then have to track also
      the Printer Reset command (DJ6/8 p. 55). It's easier to set the method
      explicitly here. The worst which could happen is that in the case of
      old printers we generate two consecutive commands the first of which is
      superfluous. */
  if (pcl_use_oldERG(global->level)) {
    /* Raster Graphics Compression Method */
    gp_fprintf(out, "%dm", (int)global->compression);
    data->current_compression = global->compression;
  }
  else data->current_compression = pcl_cm_none;

  return 0;
}

/******************************************************************************

  Function: pcl3_skip_groups

  Routine to skip a number of strip groups. 'count' is the number of strip
  groups (vertical distance in units of the reciprocal of the lowest vertical
  raster resolution).

  This function may only be called in raster mode.

******************************************************************************/

int pcl3_skip_groups(gp_file *out, pcl_RasterData *data, unsigned int count)
{
  int j;

  /*  I don't know what happens with the seed row when 'count' is zero, but as
      the command is superfluous in that case anyway I'm ignoring it. */
  if (count == 0) return 0;

  gp_fprintf(out, "%uy", count);
   /* PCL: Relative Vertical Pixel Movement/Y Offset.
      The statement in DJ6/8, p. 52, that "this command zero-fills the offset
      area" is incorrect. This can be seen when printing with an RGB palette on
      a DJ 850C where it results in a white area, not a black one. Hence it
      really skips these groups. */

  /* This command has zeroed the seed row (DJ6/8 p. 52). */
  if (pcl_cm_is_differential(data->global->compression))
    for (j = 0; j < data->global->number_of_bitplanes; j++)
      data->previous[j].length = 0;

  return 0;
}

/******************************************************************************

  Function: send_plane

  This function sends a bit plane to the printer. It returns zero on success
  and a non-zero value otherwise. In the latter case, an error message will
  have been issued on stderr.

  'final' indicates whether this is the last plane of the row or not.
  'method_demanded' contains the PCL compression method desired, '*method_used'
  the one actually used in the last transmission. This last variable will be
  reset to the method used in this invocation.
  'in' points to the octet string to be sent as plane data, 'prev' points to
  the string previously sent (or whatever the reference plane for a differential
  compression method is) and may be NULL if 'method_demanded' refers
  to a purely "horizontal" method.
  'out' is the file to which the plane should be written.
  'out_bf1' and possibly 'out_bf2' are pointers to storage areas of at least
  length 'out_bf_size' which can be used as scratch areas by this function.
  'out_bf1' must be non-NULL.
  'out_bf2' need only be non-NULL if 'method_demanded' is 'pcl_cm_delta'.
  'out_bf_size' must be positive but need not be larger than 'in->length'+2.

******************************************************************************/

static int send_plane(pcl_bool final,
  pcl_Compression method_demanded, pcl_Compression *method_used,
  const pcl_OctetString *in, const pcl_OctetString *prev, gp_file *out,
  pcl_Octet *out_bf1, pcl_Octet *out_bf2, size_t out_bf_size)
{
  int
    rc = 0;		/* Return code from commands */
  pcl_Compression
    choice;		/* Method chosen */
  pcl_OctetString
    out1,
    out2,
    send;		/* Octets to be sent to the printer */

  /* Initialize 'out1' (no dynamic initializers for structs in ISO/ANSI C) */
  out1.str = out_bf1;
  out1.length = in->length + (*method_used == pcl_cm_none? 0: 2);
    /* 2 is the cost of switching to 'pcl_cm_none'. */
  if (out1.length > out_bf_size) out1.length = out_bf_size;

  /* Set 'send' to a compressed row to be sent and 'choice' to the compression
     method employed. */
  if (method_demanded == pcl_cm_delta) {
    /*  Method 3 (delta row compression) has a widely varying effectiveness,
        depending on the structure of the input. Hence it is best combined
        with a non-delta method like method 2, as is done here on a per-plane
        basis, or method 1, as inherent in method 9.
        The procedure here is simple: try both methods, and then take the one
        giving the shortest output.
    */
    int c1, c2;	/* cost in octets */

    /* Try delta row compression */
    rc = pcl_compress(pcl_cm_delta, in, prev, &out1);
    if (rc == 0) c1 = out1.length; else c1 = -1;
    if (*method_used != pcl_cm_delta && c1 >= 0) c1 += 2;
      /* cost of switching methods */

    /* Try TIFF compression */
    if (0 == c1) c2 = -1;
    else {
      int bound = in->length + (*method_used == pcl_cm_none? 0: 2);
      if (c1 >= 0 && c1 < bound) {
        /* We're interested in TIFF compression only if it results in an octet
           string shorter than the one produced by delta row compression. */
        bound = c1;
        if (*method_used != pcl_cm_tiff && bound >= 2) bound -= 2;
      }
      out2.str = out_bf2; out2.length = bound;
      rc = pcl_compress(pcl_cm_tiff, in, NULL, &out2);
      if (rc == 0) c2 = out2.length; else c2 = -1;
      if (*method_used != pcl_cm_tiff && c2 >= 0) c2 += 2;
    }

    /* Select the better of the two, or no compression */
    if (c1 < 0) {
      if (c2 < 0) choice = pcl_cm_none;
      else choice = pcl_cm_tiff;
    }
    else {
      if (c2 < 0 || c1 <= c2) choice = pcl_cm_delta;
      else choice = pcl_cm_tiff;
    }
    switch (choice) {
    case pcl_cm_tiff:
      send = out2; break;
    case pcl_cm_delta:
      send = out1; break;
    default:
      send = *in;
    }
  }
  else {
    if (method_demanded != pcl_cm_none &&
        pcl_compress(method_demanded, in, prev, &out1) == 0) {
      /* Send compressed data */
      send = out1;
      choice = method_demanded;
    }
    else {
      /* Send uncompressed data */
      send = *in;
      choice = pcl_cm_none;
    }
  }

  /* Switch compression methods, if needed */
  if (*method_used != choice) {
    /* Raster Graphics Compression Method */
    if (gp_fprintf(out, "%dm", (int)choice) < 0) {
      errprintf(out->memory, ERRPREF "Error from fprintf(): %s.\n", strerror(errno));
      return -1;
    }
    *method_used = choice;
  }

  /* Transfer plane to the printer */
  if (send.length == 0) {
    errno = 0;
    if (final)
      gp_fputc('w', out);
      /* PCL: Transfer Raster Graphics Data by Row/Transfer Raster by Row/Block
       */
    else
      gp_fputc('v', out);
      /* PCL: Transfer Raster Graphics Data by Plane/Transfer Raster by Plane */
    if (errno != 0) {
      errprintf(out->memory, ERRPREF "Error from fputc(): %s.\n", strerror(errno));
      return -1;
    }
  }
  else {
    /* PCL: Transfer Raster Graphics Data by Row/Block/Plane */
    if (gp_fprintf(out, "%d%c", send.length, final? 'w': 'v') < 0) {
      errprintf(out->memory, ERRPREF "Error from fprintf(): %s.\n", strerror(errno));
      return -1;
    }
    if (gp_fwrite(send.str, sizeof(pcl_Octet), send.length, out) != send.length) {
      errprintf(out->memory, ERRPREF "Error in fwrite(): %s.\n", strerror(errno));
      return -1;
    }
  }

  return 0;
}

/******************************************************************************

  Function: pcl3_transfer_group

  Routine to transmit a strip group given as a sequence of bitplanes. A strip
  group consists of one strip of raster rows for every colorant. Every strip
  has the same height (reciprocal of the lowest vertical resolution) and all
  the strips will be overlaid. For more details, see the description of
  'pcl_RasterData' in the header file.

  The bit planes will be sent as specified without first trying to reduce their
  length by removing trailing null octets.

  This routine may only be called in raster mode.

  On success and if a differential compression method has been requested, this
  function will exchange the values of the 'next' and 'previous' arrays (not
  the storage areas pointed to by their 'str' fields). Hence the calling code
  need merely fill in the new sequence of bit planes into the areas currently
  pointed to from the 'next' array, set the length fields, call this function,
  and repeat the procedure for subsequent strip groups. On allocating the
  'next' and 'previous' arrays it is therefore advisable to allocate storage of
  identical length for corresponding elements in 'next' and 'previous'.

******************************************************************************/

int pcl3_transfer_group(gp_file *out, pcl_RasterData *data)
{
  const pcl_FileData *global = data->global;
  int
    final,
    j;

  /* Send the bit planes in their proper order */
  if (global->palette == pcl_CMYK && global->order_CMYK) {
    /* First CMY */
    for (j = global->black_planes; j < global->number_of_bitplanes; j++) {
      if (send_plane(FALSE,
          global->compression, &data->current_compression, data->next + j,
          data->seed_plane[j],
          out,
          data->workspace[0], data->workspace[1],
          data->workspace_allocated) != 0)
        return -1;
    }
    /* Now black */
    final = global->black_planes - 1;
    for (j = 0; j < global->black_planes; j++) {
      if (send_plane(j == final,
          global->compression, &data->current_compression, data->next + j,
          data->seed_plane[j],
          out,
          data->workspace[0], data->workspace[1],
          data->workspace_allocated) != 0)
        return -1;
    }
  }
  else {
    /* Output order is K, CMY, KCMY or RGB */
    final = global->number_of_bitplanes - 1;
    for (j = 0; j < global->number_of_bitplanes; j++) {
      if (send_plane(j == final,
          global->compression, &data->current_compression, data->next + j,
          data->seed_plane[j],
          out,
          data->workspace[0], data->workspace[1],
          data->workspace_allocated) != 0)
        return -1;
    }
  }

  /* Switch old and new planes in case of differential compression methods */
  if (pcl_cm_is_differential(data->global->compression))
    for (j = 0; j < global->number_of_bitplanes; j++) {
      pcl_OctetString tmp;
      tmp = data->previous[j];
      data->previous[j] = data->next[j];
      data->next[j] = tmp;
    }

  return 0;
}

/******************************************************************************

  Function: pcl3_end_raster

  This function may only be called in raster mode which it terminates.

******************************************************************************/

int pcl3_end_raster(gp_file *out, pcl_RasterData *data)
{
  gp_fputs("0Y", out);
    /*  PCL: Relative Vertical Pixel Movement/Y Offset. This is a simple way
        to terminate the combined escape sequence started at the beginning of
        raster mode. */

  /* End Raster Graphics */
  send_ERG(out, data->global->level);
  if (!pcl_use_oldERG(data->global->level))
    data->current_compression = pcl_cm_none;
      /* TRG500 p. 6-40, Lexmark-PTR pp. 2-40/41 */

  free(data->seed_plane); data->seed_plane = NULL;

  return 0;
}

/******************************************************************************

  Function: pcl3_end_page

  This function must be called to finish a page. It may only be called if the
  page has been opened and not in raster mode.

******************************************************************************/

int pcl3_end_page(gp_file *out, pcl_FileData *data)
{
  /* Eject the page */
  gp_fputc('\f', out);

  if (gp_ferror(out)) {
    errprintf(out->memory, ERRPREF "Unidentified system error while writing the output file.\n");
    return -1;
  }

  return 0;
}

/******************************************************************************

  Function: pcl3_end_file

  This function should be called at the end of each print job in order to leave
  the printer in a known state. However, this is only a matter of courtesy for
  print jobs which do not follow HP's recommendations and do not start with
  Printer Reset as the initial command.

******************************************************************************/

int pcl3_end_file(gp_file *out, pcl_FileData *data)
{
  /* For banner printing, HP recommends to eject the page via Media Source.
     The printer then enters a paper-unloading state. */
  if (data->media_source == -1)
    gp_fputs("\033&l0H", out);	/* PCL: Media Source: Eject Page */

  /* PCL: Printer Reset */
  gp_fputs("\033E", out);

  /* Terminate PJL */
  if (data->PJL_job != NULL || data->PJL_language != NULL) {
    /* PJL: UEL */
    gp_fputs("\033%-12345X", out);

    if (data->PJL_job != NULL) {
      /* PJL: End of Job. Some HP PCL-3 drivers using JOB omit this command.
         According to PJLTRM, it is required in that case. */
      gp_fputs("@PJL EOJ\n", out);

      /* PJL: UEL. All output I've seen from HP's PCL-3 drivers using EOJ
         omits this final UEL. According to PJLTRM, it is required. In my
         opinion it doesn't make any difference because in both cases the
         printer expects PJL in preference to other data next and the rules
         for deciding whether it's PJL or not are also the same. Note that the
         command does not influence the printer's state. */
      gp_fputs("\033%-12345X", out);
    }
  }

  if (gp_ferror(out)) {
    errprintf(out->memory, ERRPREF "Unidentified system error while writing the output file.\n");
    return -1;
  }

  return 0;
}

/******************************************************************************

  Function: pcl3_set_oldquality

  This function determines and sets the old quality parameters based on the
  current 'palette', 'print_quality' and 'media_type' values. This is mostly
  done as recommended by HP on page 6-34 of the TRG500 but that information is
  not complete.

  In particular, nothing is said about RGB palettes which I'm treating here
  like CMY palettes.

  If the input values cannot be mapped, the function sets the old parameters as
  if the offending value were 0 (normal quality or plain paper respectively)
  and returns a non-zero exit code.

******************************************************************************/

int pcl3_set_oldquality(pcl_FileData *data)
{
  /* A media type of 3 means glossy paper, 4 is transparency film. */

  switch (data->print_quality) {
  case -1 /* draft */:
    data->depletion = 3;		/* 50 % */
    data->raster_graphics_quality = 1;	/* draft */
    if (data->media_type == 4) data->shingling = 1;	/* 2 passes */
    else data->shingling = 0;		/* no shingling */
    break;
  case 1 /* presentation */:
    if (3 <= data->media_type && data->media_type <= 4)
      data->depletion = 1;		/* 0 % */
    else if (data->palette == pcl_CMY || data->palette == pcl_RGB)
      data->depletion = 2;		/* 25 % */
    else
      data->depletion = 3;		/* 50 % */
       /* Actually, TRG500 recommends 5 (50 % with gamma correction), but we
          assume that gamma correction will be handled externally. */
    data->raster_graphics_quality = 2;	/* high */
    data->shingling = 2;		/* 4 passes */
    break;
  default: /* normal or an illegal value */
    data->depletion = 2;		/* 25 % */
    data->raster_graphics_quality = 0;	/* use current control panel setting */
    if (data->media_type == 3 ||
        (data->media_type == 4 && data->palette != pcl_CMY &&
         data->palette != pcl_RGB))
      data->shingling = 2;		/* 4 passes (25 % each pass) */
    else data->shingling = 1;		/* 2 passes (50 % each pass) */
  }

  /* No depletion for monochrome data */
  if (data->palette <= pcl_black) data->depletion = 0;

  return -1 <= data->print_quality && data->print_quality <= 1 &&
    0 <= data->media_type && data->media_type <= 4? 0: 1;
}

/*****************************************************************************/

int pcl3_set_printquality(pcl_FileData *data, int quality)
{
  data->print_quality = quality;
  if (pcl_use_oldquality(data->level)) return pcl3_set_oldquality(data);

  return 0;
}

/*****************************************************************************/

int pcl3_set_mediatype(pcl_FileData *data, int mediatype)
{
  data->media_type = mediatype;
  if (pcl_use_oldquality(data->level)) return pcl3_set_oldquality(data);

  return 0;
}
