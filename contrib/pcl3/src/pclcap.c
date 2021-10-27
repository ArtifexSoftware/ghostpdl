/******************************************************************************
  File:     $Id: pclcap.c,v 1.17 2001/03/08 09:17:51 Martin Rel $
  Contents: Description of PCL printer capabilities and supporting functionality
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2000, 2001 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Standard headers */
#include "std.h"
#include <assert.h>
#include <string.h>	/* for memset() */
#include <stdlib.h>

/* Special headers */
#include "pclcap.h"
#include "pclsize.h"

/*****************************************************************************/

/* Number of elements in an array */
#define array_size(a)	(sizeof(a)/sizeof(a[0]))

/* Distance in terms of pixels at 300 ppi */
#define BP_PER_DOT	(BP_PER_IN/300)

/*****************************************************************************/

/*
  Lists of media dimensions supported by each model and the (monochrome raster
  mode) margins in force for them. Read the comments on 'pcl_PageSize' in
  pclgen.h before reading further.

  These data have been obtained from various sources. A number of
  inconsistencies and omissions, partially within a single source, indicate
  that this information is not entirely reliable. If you find more reliable
  information than the one used here or you discover an entry to be definitely
  wrong by doing a test on the printer in question, send me a message.

  The PCL documentation (TRG500, DJ3/4, DJ6/8 and DJ1120C) should be the most
  reliable and I have usually given it priority. Besides this I have used HP
  support documents (e.g., "HP DeskJet 660C Printer - Printable Regions",
  BPD02519), user manuals for individual printers, a support document covering
  a range of printers ("HP DeskJet and DeskWriter Printers - Printable
  Areas/Minimum Margins", BPD05054), and the "Software Developer's Guide for HP
  DeskJet Printer Drivers", May 1996 (called "DG" in what follows).

  The DG seems the least reliable. Apart from several internal inconsistencies
  (conversion between units), some strange data (unexplicably large top margins)
  and frequent deviations from the documentation for the series-500 Deskjets
  (TRG500), I also suspect that some of the sizes listed are not supported by
  the printer at the PCL level but are handled in the driver, i.e., the driver
  uses a PCL Page Size code other than the one belonging to the size requested.
  In addition, HP distinguishes at least between margins when printing from DOS
  and margins when printing in Windows (see, e.g., BPD02519 for the DJ 660C).
  The DG lists the Windows margins which are usually larger than the hardware
  margins. I assume the DOS margins to be the real hardware margins.

  A statement that the DeskJets 500 and 500C but not 510, 520, 540, 550C, 560C
  or any newer models load envelopes long edge first can be found on p. 24 of
  DG. In addition, BPD01246 describes how to load envelopes on the HP DeskJet,
  DeskJet Plus, DJ 500 and DJ 500C printers and it has to be done flap first,
  i.e., long edge first.

  Some care is necessary when inserting margin specifications. In particular
  for envelopes, older (pre-1997) HP documentation frequently gives them in
  landscape orientation instead of in portrait orientation as needed here
  (except when the MS_TRANSVERSE_FLAG is set). A useful rule of thumb: If the
  bottom margin is not the largest of the four margins but the left margin is,
  it's very likely landscape. The value should be around 0.5 in (ca. 12 mm).
*/

static const eprn_PageDescription
  /* Order of margins: left, bottom, right, top. */
  hpdj3xx_sizes[] = {
    /*  These are the page descriptions for the DeskJets Portable, 310, 320 and
        340, taken from DJ3/4, pp. 1 and 20. When printing in colour, the
        bottom margin is larger by 50 d.
        Note that BPD05054 gives different margins.
    */
    {ms_Letter,
      75*BP_PER_DOT, 120*BP_PER_DOT, 75*BP_PER_DOT, 30*BP_PER_DOT},
    {ms_Legal,
      75*BP_PER_DOT, 120*BP_PER_DOT, 75*BP_PER_DOT, 30*BP_PER_DOT},
    {ms_A4,
      37*BP_PER_DOT, 120*BP_PER_DOT, 43*BP_PER_DOT, 30*BP_PER_DOT},
    {ms_Executive,
      75*BP_PER_DOT, 120*BP_PER_DOT, 75*BP_PER_DOT, 30*BP_PER_DOT},
    {ms_none}
  },
  hpdj400_sizes[] = {
    /*  These are the page descriptions for the DeskJet 400 taken from DJ3/4,
        pp. 1 and 21, and are stated to be valid valid for black and colour.
        Page 21 contains the following statement: "The mechanism will
        physically shift the page image downwards by a nominal 0.08 inch
        (2.0 mm)." I conclude this to mean that the real top margin is
        0.08 inch instead of zero as specified and that the real bottom margin
        is smaller than specified by 0.08 inch.
        Note that BPD05054 gives different margins.
    */
    {ms_Letter,
      0.25f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 0.25f*BP_PER_IN, 0.08f*BP_PER_IN},
    {ms_Legal,
      0.25f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 0.25f*BP_PER_IN, 0.08f*BP_PER_IN},
    {ms_A4,
      0.125f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 3.6f*BP_PER_MM, 0.08f*BP_PER_IN},
    {ms_Executive,
      0.25f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 0.25f*BP_PER_IN, 0.08f*BP_PER_IN},
    {ms_JISB5,
      3.175f*BP_PER_MM, (0.5f-0.08f)*BP_PER_IN, 3.25f*BP_PER_MM, 0.08f*BP_PER_IN},
    /*  DJ3/4 p. 21: "Envelopes are printed in the landscape mode". As the
        margins are given such that the largest value is designated as "bottom"
        and the bottom is designated on page 19 as a long edge, I conclude that
        the DJ 400 feeds envelopes long edge first. The list on p. 1 gives the
        dimensions for Env10 in portrait and EnvDL in landscape orientation.
     */
    {ms_Env10 | MS_TRANSVERSE_FLAG,
      0.125f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 0.08f*BP_PER_IN, 0.08f*BP_PER_IN},
    {ms_EnvDL | MS_TRANSVERSE_FLAG,
      0.125f*BP_PER_IN, (0.5f-0.08f)*BP_PER_IN, 0.11f*BP_PER_IN, 0.08f*BP_PER_IN},
    {ms_none}
  },
  hpdj500_sizes[] = {
    /*  These are taken from the TRG500 p. 1-18 except for No. 10 envelopes
        which are supported according to p. 3-2 and where I took the margins
        from BPD05054. The resulting collection agrees with BPD05054 which
        declares it to be valid for the HP DeskJet and the HP DeskJet Plus as
        well.
        Order of margins: left, bottom, right, top. */
    {ms_Letter,
      0.25f*BP_PER_IN, 0.57f*BP_PER_IN, 0.25f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_Legal,
      0.25f*BP_PER_IN, 0.57f*BP_PER_IN, 0.25f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_A4,
      3.1f*BP_PER_MM, 0.57f*BP_PER_IN, 3.6f*BP_PER_MM, 0.1f*BP_PER_IN},
    {ms_Env10 | MS_TRANSVERSE_FLAG, /* Margins from BPD05054 */
      0.75f*BP_PER_IN, 0.57f*BP_PER_IN, 0.75f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_none}
  },
  hpdj500c_sizes[] = {
    /*  The data are from the TRG500 pp. 1-18 and 1-19 except for No. 10
        envelopes which are supported according to p. 3-2 and which I took from
        BPD05054. The values listed in both documents agree.
        These are the values for the black cartridge; the CMY cartridge needs
        0.17 inches more at the bottom.
        Order of margins: left, bottom, right, top. */
    {ms_Letter,
      0.25f*BP_PER_IN, 0.4f*BP_PER_IN, 0.25f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_Legal,
      0.25f*BP_PER_IN, 0.4f*BP_PER_IN, 0.25f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_A4,
      3.1f*BP_PER_MM, 0.4f*BP_PER_IN, 3.6f*BP_PER_MM, 0.1f*BP_PER_IN},
    {ms_Env10 | MS_TRANSVERSE_FLAG,
       /* Margins from BPD05054, but I've chosen 0.4 in for the bottom margin
          instead of 0.57 in as listed there because it looks to me like the
          colour bottom. Compare with the DJ 500. */
      0.75f*BP_PER_IN, 0.4f*BP_PER_IN, 0.75f*BP_PER_IN, 0.1f*BP_PER_IN},
    {ms_none}
  },
  common_sizes[] = { /* DJs 510, 520, 550C und 560C for printing in black */
    /*  The data are from the TRG500 p. 1-19 except for envelopes which are
        supported according to p. 3-2 and where I took the margins from
        BPD05054. The values listed in both documents agree.
        For colour, the bottom margin must be increased by 0.13 inches.
        Order of margins: left, bottom, right, top. */
    {ms_Letter,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Legal,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Executive,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4,
      3.1f*BP_PER_MM, 0.46f*BP_PER_IN, 3.6f*BP_PER_MM, 0.04f*BP_PER_IN},
    /* Envelopes are supported according the TRG500, the margins are from
       BPD05054. */
    {ms_Env10,	/* given in landscape by HP */
      0.123f*BP_PER_IN, 0.71f*BP_PER_IN, 0.125f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvDL,	/* given in landscape by HP */
      3.1f*BP_PER_MM, 18.0f*BP_PER_MM, 3.6f*BP_PER_MM, 1.0f*BP_PER_MM},
    {ms_none}
  },
  hpdj540_sizes[] = {
    /*  The data are from the TRG500 pp. 1-20 and 1-21 except that I have used
        a top margin of 0.04 inch from the DG instead of zero from the TRG500.
        Taking into account that the bottom margin is larger by 0.13 inches in
        colour, the resulting data agree with those in the DG, pp. 65-66.
        Order of margins: left, bottom, right, top. */
    {ms_Executive,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Letter,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Legal,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4,	3.2f*BP_PER_MM, 11.7f*BP_PER_MM, 3.2f*BP_PER_MM, 0.04f*BP_PER_IN},
    {ms_A5,	3.2f*BP_PER_MM, 18.0f*BP_PER_MM, 3.2f*BP_PER_MM, 0.04f*BP_PER_IN},
    {ms_JISB5,	4.2f*BP_PER_MM, 11.7f*BP_PER_MM, 4.2f*BP_PER_MM, 0.04f*BP_PER_IN},
    {ms_Index4x6in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index5x8in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A6 | PCL_CARD_FLAG,
      3.2f*BP_PER_MM, 18.0f*BP_PER_MM, 3.2f*BP_PER_MM, 0.04f*BP_PER_IN},
    {ms_Env10,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_EnvDL,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_EnvC6,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_Postcard, 4.2f*BP_PER_MM, 18.0f*BP_PER_MM, 4.2f*BP_PER_MM, 0.04f*BP_PER_IN},
    {ms_none}
  },
  hpdj660c_sizes[] = {
    /*  These are taken from "HP DeskJet 660C Printer - Printable Regions",
        BPD02519, 1996 (obtained in March 1997), except that I've again
        increased the top margin from zero to 0.04 inches following BPD05054
        and DG pp. 69-70. These are the values for printing in black from DOS.
        Colour printing increases the bottom margin by 0.13 inch. Adding this
        value reproduces the data from BPD05054 and the DG. This is, however, a
        contradiction with DG because BPD02519 gives different values for
        printing from Windows.
    */
    {ms_Letter,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Legal,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Executive,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4,	0.13f*BP_PER_IN, 0.46f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A5,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_JISB5,	0.17f*BP_PER_IN, 0.46f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
     /* The bottom margin for JISB5 is given with large differences. BPD02519
        specifies it as 0.46 in for black from DOS, 0.59 in DOS/colour (agreeing
        with +0.13 in for 'bottom_increment') and the same for Windows black or
        colour. BPD05054 gives 0.84 in without distinguishing between black and
        colour, and the DG gives 0.59 in. */
    {ms_Env10,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_EnvDL,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_EnvC6,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
      /* given in landscape by HP */
    {ms_Index4x6in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index5x8in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A6 | PCL_CARD_FLAG,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Postcard,
      0.17f*BP_PER_IN, 0.71f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvUS_A2,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
       /* Supported (BPD02925, BPD05054, DG), margins from BPD05054 agreeing
          with DG p. 70 */
    {ms_none}
  },
  hpdj680c_sizes[] = {
   /* For a change, these data are taken from the DG pp. 73-74. It is assumed
      that they are there given for colour printing and that the additional
      bottom margin included is 0.13 inches. Except for banner printing which
      is not supported by the DJ 660C, this gives identical margins as those
      for the DJ 660C.
      These data also almost agree with the German printer manual,
        Hewlett Packard
        "Weitere Informationen ueber den HP DeskJet 690C Series-Drucker"
        1. Auflage, September 1996
        Bestellnummer C4562-60105, Artikelnummer C4562-90160
      for the DJ 690C. The difference is at most 0.31 mm except for the bottom
      margin in the case of JIS B5 which is given as 21.2 mm instead of 14.9 mm
      (0.83 in vs. 0.59 in).
      The DJ 690C does support banner printing (DJ6/8 p. 9).
    */
    {ms_Letter,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4,	0.13f*BP_PER_IN, 0.46f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Legal,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Env10,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Executive,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvDL,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A5,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_JISB5,	0.17f*BP_PER_IN, 0.46f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvC6,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index4x6in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index5x8in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A6 | PCL_CARD_FLAG,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Postcard,
      0.17f*BP_PER_IN, 0.71f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvUS_A2,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Letter | MS_BIG_FLAG,	/* banner */
      0.25f*BP_PER_IN, 0.0f, 0.25f*BP_PER_IN, 0.0f},
    {ms_A4 | MS_BIG_FLAG,	/* banner */
      0.13f*BP_PER_IN, 0.0f, 0.13f*BP_PER_IN, 0.0f},
      /* BPD05054 claims left and right margins of 0.25 in. */
    {ms_none}
  },
  hpdj6xx_and_8xx_sizes[] = {
   /* This entry is a combination from the sizes supported according to
      DJ6/8 pp. 23-24 by all series 600 and 800 DeskJets, and the corresponding
      margins from 'hpdj660c_sizes[]' above. This is only used for
      series 600 DeskJets for which there is no other information and not for
      series 800 DeskJets.
    */
    {ms_Executive,
      0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Letter,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Legal,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A5,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4,	0.13f*BP_PER_IN, 0.46f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_JISB5,	0.17f*BP_PER_IN, 0.46f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Postcard,
      0.17f*BP_PER_IN, 0.71f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_DoublePostcard,	/* not present in 'hpdj660c_sizes[]'; guessed */
      0.17f*BP_PER_IN, 0.71f*BP_PER_IN, 0.17f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A6 | PCL_CARD_FLAG,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index4x6in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Index5x8in,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_Env10,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvDL,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvC6,	0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_EnvUS_A2,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_none}
  },
  hpdj850c_sizes[] = {
   /* Apart from JIS B5, these specifications have been taken from the German
      HP user manual for the DeskJet 850C, "HP DeskJet 850C Handbuch",
      C2145-90248, 3/95.  They are valid for black and colour printing.
      The manual agrees with "HP DeskJet 850C and 855C Printer -- Printable
      Regions" (BPD02523), HP FIRST #: 2789, 1995-09-05, and the newer
      "HP DeskJet 855C, 850C, 870C and 890C Series Printers -- Printable Areas"
        (BPD02523), effective date 1996-06-06.
      There are some minor deviations in the DG, pp. 81-82, which I have
      partially adopted.
      First some shorthand notation for margin types. The order is:
        left, bottom, right, top.
    */
#define type1	6.4f*BP_PER_MM, 11.7f*BP_PER_MM, 6.4f*BP_PER_MM, 1.0f*BP_PER_MM
#define type2	3.4f*BP_PER_MM, 11.7f*BP_PER_MM, 3.4f*BP_PER_MM, 1.0f*BP_PER_MM
#define type3	3.4f*BP_PER_MM, 11.7f*BP_PER_MM, 3.4f*BP_PER_MM, 11.7f*BP_PER_MM
#define type4	3.2f*BP_PER_MM, 11.7f*BP_PER_MM, 3.2f*BP_PER_MM, 1.0f*BP_PER_MM
#define type5	3.2f*BP_PER_MM, 22.0f*BP_PER_MM, 3.2f*BP_PER_MM, 1.0f*BP_PER_MM
    {ms_Letter,		type1},
    {ms_Legal,		type1},
    {ms_Executive,	type1},
    {ms_A4,		type2},
    {ms_A5,		type2},	/* also BPD05054. DG: type4 */
    {ms_JISB5,		type4},	/* from DG */
     /* The German handbook as well as BPD02523 and BPD05054 state that this
        should be type3. This is however demonstrably wrong for the DJ 850C.
        Besides I can't see why the top margin should be so much larger for
        JIS B5 than for the other sizes. 'type4' agrees fairly well with what
        I've measured on a DJ 850C. In particular the right and bottom clipping
        margins are definitely close to 3 and 11.5 mm, respectively.
      */
    {ms_Index4x6in,	type4},
    {ms_Index5x8in,	type4},
    {ms_A6 | PCL_CARD_FLAG, type4},
    {ms_Postcard,	type4},
    {ms_Env10,		type5},
    {ms_EnvDL,		type5},
    {ms_EnvC6,		type5},
    /*	BPD02926 claims that the series-800 DeskJets support also US A2 envelope
        size (ms_EnvUS_A2). I've experimented with a DeskJet 850C and I don't
        believe it: when sent this page size code, the printer establishes
        clipping regions which agree with those for US Letter size. This
        indicates that the PCL interpreter has not recognized the page size code
        and has therefore switched to the default size. */
    {ms_none}
#undef type1
/* type2 will be needed in a moment */
#undef type3
#undef type4
#undef type5
  },
  hpdj1120c_sizes[] = {
   /* These values are from DJ1120C, pp. 11-12, and from BPD05567. I don't
      consider them particularly trustworthy. */
    {ms_Executive,	0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN,
                        0.12f*BP_PER_IN},
    {ms_Letter,		0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN,
                        0.12f*BP_PER_IN},
    {ms_Legal,		0.25f*BP_PER_IN, 0.46f*BP_PER_IN, 0.25f*BP_PER_IN,
                        0.12f*BP_PER_IN},
    {ms_Tabloid,	0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},
    {ms_Statement,	0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},	/* Not in BPD05567. */
    {ms_HPSuperB,	0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},
    {ms_A6,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},	/* Not in BPD05567. */
    {ms_A5,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_A4,		0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.13 in, 0.46 in, 0.13 in, 0.12 in. */
    {ms_A3,		0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},	/* Only in BPD05567. */
    {ms_JISB5,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_JISB4,		0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},	/* Not in BPD05567. */
    {ms_Postcard,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_A6 | PCL_CARD_FLAG, 0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_Index4x6in,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_Index5x8in,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_Env10,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvDL,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvC6,		0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvUS_A2,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvChou3,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvChou4,	0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN,
                        0.12f*BP_PER_IN},
      /* BPD05567: 0.12 in, 0.46 in, 0.12 in, 0.12 in. */
    {ms_EnvKaku2,	0.20f*BP_PER_IN, 0.46f*BP_PER_IN, 0.20f*BP_PER_IN,
                        0.12f*BP_PER_IN},	/* Not in BPD05567. */
    /* Banners are listed only in BPD05567: */
    {ms_Letter | MS_BIG_FLAG,
      0.25f*BP_PER_IN, 0, 0.25f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_A4 | MS_BIG_FLAG,
      0.13f*BP_PER_IN, 0, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN},
    {ms_none}
  };

/*---------------------------------------------------------------------------*/

/*  Custom page size descriptions.
    First the limits: width min and max, height min and max.
    Then the margins in the usual order: left, bottom, right, top.

    Again, I have assumed something I could find no explicit documentation for,
    namely that these limits refer to raster space (which is identical to
    pixmap device space for the 'pcl3' device). It is, however, clear from the
    upper limits for the DJ 850C that "height" does not refer to the direction
    orthogonal to the feeding direction.
 */
static const eprn_CustomPageDescription
  hpdj540_custom_sizes[] = {
   /* These values are taken from the document "HP DeskJet 540 Printer -
      Print Regions", BPD02194, 1996, except for the right margin which is
      stated to be application-dependent (the TRG500 recommends that one should
      enforce at least 1/8 in) and the top margin which I've extended from
      zero to 0.04 in.
      These are the correct values for printing in black. Note that the
      DeskJet 540 in colour mode needs a bottom margin which is larger by
      0.13 in.
    */
    { /* Page width 7.12 to 8.5 in */
      ms_CustomPageSize,
      0.13f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN, 0.04f*BP_PER_IN,
      7.12f*BP_PER_IN, 8.5f*BP_PER_IN, 5.83f*BP_PER_IN, 14*BP_PER_IN
    },
    { /* Page width 5 to 7.12 in */
      ms_CustomPageSize,
      0.13f*BP_PER_IN, 0.71f*BP_PER_IN, 0.125f*BP_PER_IN, 0.04f*BP_PER_IN,
      5*BP_PER_IN, 7.12f*BP_PER_IN, 5.83f*BP_PER_IN, 14*BP_PER_IN
    },
    {ms_none,  0, 0, 0, 0,  0, 0, 0, 0}
  },
  hpdj_6xx_and_8xx_custom_sizes[] = {
    /* Ranges from DJ6/8 p. 23, margins from A4 for the DJ 660C (BPD02519) */
    {
      ms_CustomPageSize,
      0.13f*BP_PER_IN, 0.46f*BP_PER_IN, 0.13f*BP_PER_IN, 0.04f*BP_PER_IN,
      5*BP_PER_IN, 8.5f*BP_PER_IN,  148*BP_PER_MM, 14*BP_PER_IN
    },
    {ms_none,  0, 0, 0, 0,  0, 0, 0, 0}
  },
  hpdj850c_custom_sizes[] = {
    { /*
        The following values have been taken from the German DeskJet 850C
        manual, "HP DeskJet 850C Handbuch", C2145-90248, 3/95.
        They agree with the values given in the newer version of BPD02523 for
        the DeskJets 850C, 855C, 870C and 890C except that the left and right
        margins are stated to be 3.44 mm instead of 3.4 mm. This is almost
        certainly a misprint because the accompanying value in inches is
        identical (0.13 in, i.e. 3.30 mm), and specifying the margins to a
        hundredth of a millimetre is ridiculous.
      */
      ms_CustomPageSize,
      type2,
      100*BP_PER_MM, 216*BP_PER_MM, 148*BP_PER_MM, 356*BP_PER_MM
    },
    {ms_none,  0, 0, 0, 0,  0, 0, 0, 0}
  },
  hpdj1120c_custom_sizes[] = {
    {  /* Taken from DJ1120C pp. 10 and 12. The minimum sizes are guessed
          (smallest supported discrete dimensions) */
      ms_CustomPageSize,
      0.125f*BP_PER_IN, 0.46f*BP_PER_IN, 0.125f*BP_PER_IN, 0.12f*BP_PER_IN,
      90*BP_PER_MM, 13*BP_PER_IN, 146*BP_PER_MM, 19*BP_PER_IN,
    },
    {ms_none,  0, 0, 0, 0,  0, 0, 0, 0}
  },
  any_custom_sizes[] = {
   /* Any value is permitted in this case. This is approximated by permitting
      10^-37-10^37 bp for both, width and height (10^37 bp = 3.5*10^33 m).
      Note that 10^37 is the smallest upper bound and 10^-37 the largest lower
      bound for 'float' values permitted by the C standard. */
    {
      ms_CustomPageSize,
      type2,	/* use DJ 850C margins */
      1.0E-37f, 1.0E37f,  1.0E-37f, 1.0E37f
    },
    {ms_none,  0, 0, 0, 0,  0, 0, 0, 0}
  };

#undef type2

/*****************************************************************************/

/* Resolution lists */
static const eprn_Resolution
  threehundred[] = {
    { 300, 300 }, { 0, 0 }
  },
  six_by_three[] = {
    { 600, 300 }, { 0, 0 }
  },
  sixhundred[] = {
    { 600, 600 }, { 0, 0 }
  },
  basic_resolutions[] = {
    { 75, 75 }, { 100, 100 }, { 150, 150 }, { 300, 300 }, { 0, 0 }
     /* This is the list of resolutions supported by the series 300, 400 and
        500 DeskJets (except the DJ 540) in black and colour and by the series
        600 and 800 DeskJets (except the DJs 85xC) in colour. */
  },
  basic_without_100[] = {
    { 75, 75 }, { 150, 150 }, { 300, 300 }, { 0, 0 }
  },
  rel_hpdj1120c_black[] = {
    { 150, 150 }, { 300, 300 }, { 600, 600 }, { 0, 0 }
  },
  rel_150_and_300[] = {
    { 150, 150 }, { 300, 300 }, { 0, 0 }
  };

/* Lists of permitted numbers of intensity levels */
static const eprn_IntensityLevels
  two_levels[] = { {2, 2}, {0, 0} },
  four_levels[] = { {4, 4}, {0, 0} },
  two_to_four_levels[] = { {2, 4}, {0, 0} },
  three_to_four_levels[] = { {3, 4}, {0, 0} },
  any_levels[] = { {2, 65535}, {0, 0} };
  /* More than 65535 levels cannot be represented in a Configure Raster Data
     command with format 2. */

/* Combinations of resolutions and intensity levels */
static const eprn_ResLev
  rl_basic[] = {
    {basic_resolutions, two_levels}, {NULL, NULL}
  },
  rl_basic_without_100[] = {
    {basic_without_100, two_levels}, {NULL, NULL}
  },
  rl_hpdj6xx_black[] = {
    {basic_resolutions, two_levels}, {six_by_three, two_levels},
    {sixhundred, two_levels}, {NULL, NULL}
  },
  rl_hpdj6xx_colour[] = {
    {basic_resolutions, two_levels}, {six_by_three, two_levels}, {NULL, NULL}
  },
  rl_three[] = {
    {threehundred, two_levels}, {NULL, NULL}
  },
  rl_six[] = {
    {sixhundred, two_levels}, {NULL, NULL}
  },
  rl_hpdj85x_black[] = {
    {basic_without_100, two_levels}, {sixhundred, two_levels}, {NULL, NULL}
  },
  rl_three_with_levels_2_4[] = {
    {threehundred, two_to_four_levels}, {NULL, NULL}
  },
  rl_three_with_levels_3_4[] = {
    {threehundred, three_to_four_levels}, {NULL, NULL}
  },
  rl_three_with_levels_4[] = {
    {threehundred, four_levels}, {NULL, NULL}
  },
  rl_hpdj1120c_black[] = {
    {rel_hpdj1120c_black, two_levels},
    {threehundred, four_levels},
    {NULL, NULL}
  },
  rl_150_and_300[] = {
    {rel_150_and_300, two_levels}, {NULL, NULL}
  },
  rl_any_two_levels[] = {
    /* Any resolution, but only two intensity levels */
    {NULL, two_levels}, {NULL, NULL}
  },
  rl_any[] = {
    {NULL, any_levels}, {NULL, NULL}
  };

/*****************************************************************************/

/* Colour capability lists */
static const eprn_ColourInfo
  ci_old_mono[] = {	/* DeskJet 5xx monochrome printers */
    { eprn_DeviceGray, {rl_basic, NULL} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj500c[] = {	/* HP DeskJet 500C: black or CMY cartridge */
    { eprn_DeviceGray, {rl_basic, NULL} },
    { eprn_DeviceCMY,  {rl_basic, NULL} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj540[] = {	/* HP DeskJet 540: black or CMY cartridge */
    { eprn_DeviceGray, {rl_basic_without_100, NULL} },
    { eprn_DeviceCMY,  {rl_basic_without_100, NULL} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj5xx_cmyk[] = {
    /* HP DeskJet 550C or 560C: black and CMY cartridges, but the inks should
       not mix. */
    { eprn_DeviceGray, {rl_basic, NULL} },
    { eprn_DeviceCMY,  {rl_basic, NULL} },
    { eprn_DeviceCMY_plus_K, {rl_basic, NULL} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj600[] = {
    /* HP DeskJet 600: black or CMY cartridge */
    { eprn_DeviceGray, {rl_hpdj6xx_black, NULL} },
    { eprn_DeviceCMY,  {rl_hpdj6xx_colour, NULL} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj6xx[] = {
    /* Series-600 CMYK printers */
    { eprn_DeviceGray, {rl_hpdj6xx_black, NULL} },
    { eprn_DeviceCMY,  {rl_hpdj6xx_colour, NULL} },
    { eprn_DeviceCMYK, {rl_hpdj6xx_colour, NULL} },
    { eprn_DeviceCMYK, {rl_six, rl_three} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj85x[] = {
    { eprn_DeviceGray, {rl_hpdj85x_black, NULL} },
    { eprn_DeviceCMY,  {rl_basic_without_100, NULL} },
    { eprn_DeviceCMYK, {rl_basic_without_100, NULL} },
    /* The following I have determined experimentally on a DJ 850C: */
    { eprn_DeviceGray, {rl_three_with_levels_4, NULL} },
    { eprn_DeviceCMYK, {rl_six, rl_three_with_levels_2_4} },
    { eprn_DeviceCMYK, {rl_three_with_levels_4, rl_three_with_levels_3_4} },
    { eprn_DeviceGray, {NULL, NULL} }
  },
  ci_hpdj1120c[] = {
    /* DJ1120C p. 48 */
    { eprn_DeviceGray,	{rl_hpdj1120c_black, NULL} },
    { eprn_DeviceCMYK,	{rl_six, rl_three_with_levels_4} },
    { eprn_DeviceCMYK,	{rl_six, rl_three} },
    { eprn_DeviceCMYK,	{rl_three_with_levels_4, rl_three_with_levels_3_4} },
    { eprn_DeviceCMYK,	{rl_150_and_300, NULL} },
    /* The following are combined from DJ1120C pp. 36 and 52. */
    { eprn_DeviceGray,	{rl_hpdj85x_black, NULL} },
    { eprn_DeviceCMY,	{rl_basic_without_100, NULL} },
    { eprn_DeviceCMYK,	{rl_basic_without_100, NULL} },
    { eprn_DeviceGray,	{NULL, NULL} }
  },
  ci_any[] = {
    { eprn_DeviceGray, {rl_any, NULL} },
    { eprn_DeviceRGB, {rl_any_two_levels, NULL} },
    { eprn_DeviceCMY, {rl_any, NULL} },
    { eprn_DeviceCMYK, {rl_any, rl_any} },
    { eprn_DeviceGray, {NULL, NULL} }
  };

/*****************************************************************************/

/* Descriptions of known PCL printers */

const pcl_PrinterDescription pcl3_printers[] = {
  /*  For the HP DeskJet and the HP DeskJet Plus I am guessing that they can
      be treated like the DJ 500. This seems reasonable because several HP
      documents group the HP DeskJet, the HP DeskJet Plus and the 500 and
      500C DeskJets together. */
  { HPDeskJet, pcl_level_3plus_DJ500,
    { "HP DeskJet", hpdj500_sizes, NULL, 0.0, ci_old_mono } },
  { HPDeskJetPlus, pcl_level_3plus_DJ500,
    { "HP DeskJet Plus", hpdj500_sizes, NULL, 0.0, ci_old_mono } },
  { HPDJPortable, pcl_level_3plus_S5,
    { "HP DeskJet Portable", hpdj3xx_sizes, NULL, 0.0, ci_old_mono } },
     /* DJ3/4 p. 2: This printer behaves as the 550C without the colour
        cartridge. */
  { HPDJ310, pcl_level_3plus_S5,
    { "HP DeskJet 310", hpdj3xx_sizes, NULL, 50*BP_PER_DOT, ci_hpdj500c } },
     /* DJ3/4 p. 3: The 3xx DeskJets with the black cartridge installed behave
        identically to the DJ Portable, with the colour cartridge they can be
        treated as the DJ 500C. */
  { HPDJ320, pcl_level_3plus_S5,
    { "HP DeskJet 320", hpdj3xx_sizes, NULL, 50*BP_PER_DOT, ci_hpdj500c } },
  { HPDJ340, pcl_level_3plus_S5,
    { "HP DeskJet 340", hpdj3xx_sizes, NULL, 50*BP_PER_DOT, ci_hpdj500c } },
  { HPDJ400, pcl_level_3plus_S5,
    { "HP DeskJet 400", hpdj400_sizes, NULL, 0.0, ci_hpdj500c } },
     /* DJ3/4 p. 3 and p. 6. */
  { HPDJ500,	pcl_level_3plus_DJ500,
    {"HP DeskJet 500", hpdj500_sizes, NULL, 0.0, ci_old_mono } },
  { HPDJ500C,	pcl_level_3plus_S5,
    { "HP DeskJet 500C", hpdj500c_sizes, NULL, 0.17f*BP_PER_IN, ci_hpdj500c } },
  { HPDJ510,	pcl_level_3plus_S5,
    { "HP DeskJet 510", common_sizes, NULL, 0.0, ci_old_mono } },
  { HPDJ520,	pcl_level_3plus_S5,
    { "HP DeskJet 520", common_sizes, NULL, 0.0, ci_old_mono } },
  { HPDJ540,	pcl_level_3plus_S68,
    { "HP DeskJet 540", hpdj540_sizes, hpdj540_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj540 } },
  { HPDJ550C,	pcl_level_3plus_S5,
    { "HP DeskJet 550C", common_sizes, NULL, 0.13f*BP_PER_IN, ci_hpdj5xx_cmyk }},
  { HPDJ560C,	pcl_level_3plus_S5,
    { "HP DeskJet 560C", common_sizes, NULL, 0.13f*BP_PER_IN, ci_hpdj5xx_cmyk }},
  { pcl3_generic_old, pcl_level_3plus_ERG_both,
    { "unspecified PCL-3+ printer (old)", common_sizes, NULL, 0.0, ci_any } },
  { HPDJ600,	pcl_level_3plus_S68,
    { "HP DeskJet 600", hpdj6xx_and_8xx_sizes, hpdj_6xx_and_8xx_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj600 } },
  { HPDJ660C,	pcl_level_3plus_S68,
    { "HP DeskJet 660C", hpdj660c_sizes, hpdj_6xx_and_8xx_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj6xx } },
  { HPDJ670C,	pcl_level_3plus_S68,
    { "HP DeskJet 670C", hpdj660c_sizes, hpdj_6xx_and_8xx_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj6xx } },
    /* This printer can be treated as the DJ 660C (DJ6/8 p. 2). */
  { HPDJ680C,	pcl_level_3plus_S68,
    { "HP DeskJet 680C", hpdj680c_sizes, hpdj_6xx_and_8xx_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj6xx } },
  { HPDJ690C,	pcl_level_3plus_S68,
    { "HP DeskJet 690C", hpdj680c_sizes, hpdj_6xx_and_8xx_custom_sizes,
      0.13f*BP_PER_IN, ci_hpdj6xx } },
  { HPDJ850C,	pcl_level_3plus_S68,
    { "HP DeskJet 850C", hpdj850c_sizes, hpdj850c_custom_sizes,
      0.0, ci_hpdj85x } },
  { HPDJ855C,	pcl_level_3plus_S68,
    { "HP DeskJet 855C", hpdj850c_sizes, hpdj850c_custom_sizes,
      0.0, ci_hpdj85x } },
  /* I'm treating the 870C and the 890C just as the 85xCs. */
  { HPDJ870C,	pcl_level_3plus_S68,
    { "HP DeskJet 870C", hpdj850c_sizes, hpdj850c_custom_sizes,
      0.0, ci_hpdj85x } },
  { HPDJ890C,	pcl_level_3plus_S68,
    { "HP DeskJet 890C", hpdj850c_sizes, hpdj850c_custom_sizes,
      0.0, ci_hpdj85x } },
  { HPDJ1120C,	pcl_level_3plus_S68,
    { "HP DeskJet 1120C", hpdj1120c_sizes, hpdj1120c_custom_sizes,
      0.0, ci_hpdj1120c } },
  { pcl3_generic_new, pcl_level_3plus_S68,
    {"unspecified PCL-3+ printer",
      hpdj850c_sizes, any_custom_sizes, 0.0, ci_any }}
};

/*****************************************************************************/
#undef CHECK_CONSTRAINTS

#ifdef CHECK_CONSTRAINTS
static int checked = 0;

static void check(void)
{
  int j;

  /* Check that 'pcl3_printers[]' is indexed by printer code */
  for (j = 0; j < array_size(pcl3_printers); j++)
    assert(pcl3_printers[j].id == j);

  checked = 1;

  return;
}
#endif

/******************************************************************************

  Function: pcl3_fill_defaults

  This function overwrites all fields in '*data' irrespective of their old
  contents and replaces them with default values acceptable for the printer.
  In particular, everything which is printer-specific and cannot be chosen by
  the user will be set in accordance with 'printer'.

******************************************************************************/

void pcl3_fill_defaults(pcl_Printer printer, pcl_FileData *data)
{
#ifdef CHECK_CONSTRAINTS
  if (!checked) check();
#endif

  /* Set everything to zero */
  memset(data, 0, sizeof(pcl_FileData));

  /* Data which must not be zero */
  /* At present, every printer listed here supports 300 ppi monochrome data
     with two intensity levels. Hence we need not look at the printer
     description. */
  data->number_of_colorants = 1;
  data->colorant_array[0].hres = data->colorant_array[0].vres = 300;
  data->colorant_array[0].levels = 2;

  /* Data which are fixed by the printer model */
  data->level = pcl3_printers[printer].level;

  /* Data for which zero is acceptable but not sensible */
  data->duplex = -1;	/* no simplex/duplex requests */
  data->dry_time = -1;	/* request no dry time */
  switch (printer) {
    case HPDeskJet:
      /*FALLTHROUGH*/
    case HPDeskJetPlus:
      /*FALLTHROUGH*/
    case HPDJ500:
       /* The DJ 500 does not support method 9 but it does support method 3
          (TRG500). I am guessing that the same is true for the HP DeskJet and
          the HP DeskJet Plus. */
      data->compression = pcl_cm_delta;
      break;
    case pcl3_generic_old:
      /*FALLTHROUGH*/
    case pcl3_generic_new:
       /* All HP drivers I have seen use method 2 when writing a PCL file to
          disk. Presumably, this is the most portable compression method. */
      data->compression = pcl_cm_tiff;
      break;
    default:
      /* The best compression method, but not always supported. */
      data->compression = pcl_cm_crdr;
  }

  /* Derived data */
  pcl3_set_oldquality(data);

  return;
}
