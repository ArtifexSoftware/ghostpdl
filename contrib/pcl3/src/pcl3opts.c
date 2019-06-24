/******************************************************************************
  File:     $Id: pcl3opts.c,v 1.17 2001/05/31 15:19:16 Martin Rel $
  Contents: Program to convert information in PCL-3+ files into options to be
            used for the ghostscript device "pcl3" in order to produce a
            file using a similar configuration
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany; e-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000, 2001 by Martin Lottermoser		      *
*	All rights reserved						      *
*									      *
******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Standard headers */
#include <assert.h>
#include <errno.h>
#include <locale.h>
#include <nl_types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* prevent gp.h redefining fopen */
#define fopen fopen

/* Application headers */
#include "pclsize.h"
#include "pclgen.h"
#include "pclscan.h"

/*****************************************************************************/

#define check(p)							\
  if ((p) == NULL) {							\
    emessage(9, "Memory allocation failure: %s.\n", strerror(errno));	\
    return -1;								\
  }

/*****************************************************************************/

/* Global variables */

static const char *progname;		/* The program's name in messages */
static nl_catd catd = (nl_catd)(-1);	/* NLS message catalogue descriptor */

/*****************************************************************************/

static void message(gp_file *f, int msg_id, const char *fmt, va_list ap)
{
  vfprintf(f, catd == (nl_catd)(-1)? fmt: catgets(catd, 2, msg_id, fmt), ap);

  return;
}

/*****************************************************************************/

static void emessage(int msg_id, const char *fmt, ...)
{
  va_list ap;

  fprintf(stderr, "? %s: ", progname);
  va_start(ap, fmt);
  message(stderr, msg_id, fmt, ap);
  va_end(ap);

  return;
}

/*****************************************************************************/

static void imessage(gp_file *f, int msg_id, const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  message(f, msg_id, fmt, ap);
  va_end(ap);

  return;
}

/*****************************************************************************/

static void check_line_length(gp_file *out, int *line_length, int expected)
{
  if (*line_length + expected <= 78) return;

  fputs(" \\\n", out);
  fputs("    ", out);
  *line_length = 4;

  return;
}

/*****************************************************************************/

/* Type for state and information collected during parsing a file */
typedef struct {
  gp_file *out;

  /* Capability */
  pcl_bool
    seen_ERG_B,		/* End Raster Graphics */
    seen_ERG_C,
    seen_RGR,		/* Raster Graphics Resolution */
    seen_CRD,		/* Configure Raster Data */
    seen_old_quality,
    seen_new_quality,
    seen_unknown_compression;

  /* State */
  int
    first_colorant_planes,
    next_plane,
    number_of_outputs;
  pcl_FileData
    fdata,
    fdata_old;
  pcl_bool
    CRD_active,
    seen_first_colorant,
    seen_raster_data;
} CollectedInfo;

/*****************************************************************************/

static void print_result(CollectedInfo *ip)		/* NLS: 10, 70 */
{
  pcl_bool
    different_resolutions = FALSE,
    different_non_black_levels = FALSE;
  pcl_Compression
    default_compression;
  int j;
  int ll;	/* line length */
  unsigned int min_hres, min_vres, black_levels, non_black_levels, start;

  if (ip->number_of_outputs > 0)
    imessage(ip->out, 10,
      "\nThe previous raster data section is followed by one with a different\n"
      "configuration.\n");
  ip->number_of_outputs++;

  /* Find the smallest resolutions and look for different resolutions */
  min_hres = ip->fdata.colorant_array[0].hres;
  min_vres = ip->fdata.colorant_array[0].vres;
  for (j = 1; j < ip->fdata.number_of_colorants; j++) {
    int
      h = ip->fdata.colorant_array[j].hres,
      v = ip->fdata.colorant_array[j].vres;
    if (h != min_hres || v != min_vres) different_resolutions = TRUE;
    if (h < min_hres) min_hres = h;
    if (v < min_vres) min_vres = v;
  }

  black_levels = ip->fdata.colorant_array[0].levels;

  /* Check for deviations in the non-black levels */
  start = (ip->fdata.palette == pcl_CMY || ip->fdata.palette == pcl_RGB? 0: 1);
  non_black_levels = ip->fdata.colorant_array[start].levels;
  for (j = 1; j < 3; j++)
    if (ip->fdata.colorant_array[start + j].levels != non_black_levels)
      different_non_black_levels = TRUE;

  if (different_resolutions || different_non_black_levels) {
    imessage(ip->out, 11,
      "I've found a raster data section which uses a combination of "
        "resolutions\n"
      "and intensity levels which pcl3 cannot reproduce:\n");
    for (j = 0; j < ip->fdata.number_of_colorants; j++)
      imessage(ip->out, 12, "    colorant %d: %4d x %4d ppi %2d levels\n",
        j + 1,
        ip->fdata.colorant_array[j].hres, ip->fdata.colorant_array[j].vres,
        ip->fdata.colorant_array[j].levels);
    if (ip->fdata.palette == pcl_CMYK &&
        non_black_levels > 2 && black_levels < 4)
      black_levels = 4;
  }

  imessage(ip->out, 13,
    "My best guess for the command line needed to generate a similar file "
    "is:\n");

  /* Device selection */
  ll = -1;
  ll += fprintf(ip->out, "\n  gs -sDEVICE=pcl3 -sSubdevice=");
  default_compression = pcl_cm_tiff;
  if (ip->seen_CRD || ip->seen_new_quality)
    ll += fprintf(ip->out, "unspec");
  else if (ip->seen_ERG_B && !ip->seen_ERG_C && ip->fdata.shingling == -1 &&
      (ip->fdata.palette == pcl_no_palette || ip->fdata.palette == pcl_black)) {
      /* This command line can still fail because of, e.g., resolution. */
    ll += fprintf(ip->out, "hpdj500");
    default_compression = pcl_cm_delta;
  }
  else
    ll += fprintf(ip->out, "unspecold");

  /* Special global language features */
  if (ip->fdata.NULs_to_send > 0) {
    check_line_length(ip->out, &ll, sizeof("-dSendNULs=9600"));
       /* Note the missing SP which is instead accounted for by NUL. */
    ll += fprintf(ip->out, " -dSendNULs=%d", ip->fdata.NULs_to_send);
  }
  if (ip->fdata.PJL_job != NULL) {
    check_line_length(ip->out, &ll,
      sizeof("-sPJLJob=") + strlen(ip->fdata.PJL_job));
    ll += fprintf(ip->out, " -sPJLJob=%s", ip->fdata.PJL_job);
  }
  if (ip->fdata.PJL_language != NULL) {
    check_line_length(ip->out, &ll,
      sizeof("-sPJLLanguage=") + strlen(ip->fdata.PJL_language));
    ll += fprintf(ip->out, " -sPJLLanguage=%s", ip->fdata.PJL_language);
  }
  if (ip->seen_CRD && !ip->seen_RGR) {
    check_line_length(ip->out, &ll, sizeof("-dOnlyCRD"));
    ll += fprintf(ip->out, " -dOnlyCRD");
  }

  /* Media handling except media source */
  if (ip->fdata.manual_feed) {
    check_line_length(ip->out, &ll, sizeof("-dManualFeed"));
    ll += fprintf(ip->out, " -dManualFeed");
  }
  if (ip->fdata.duplex > 0) {
    const char s[] = " -dDuplex";

    if (ip->fdata.duplex == 1) {
      const char s[] = " -sDuplexCapability=sameLeadingEdge";
      check_line_length(ip->out, &ll, sizeof(s) - 1);
      ll += fprintf(ip->out, s);
    }
    else if (ip->fdata.duplex == 2) {
      const char s[] = " -sDuplexCapability=oppositeLeadingEdge";
      check_line_length(ip->out, &ll, sizeof(s) - 1);
      ll += fprintf(ip->out, s);
    }
    check_line_length(ip->out, &ll, sizeof(s) - 1);
    ll += fprintf(ip->out, s);
  }
  if (ip->fdata.dry_time >= 0) {
    check_line_length(ip->out, &ll, sizeof("-dDryTime=") + 4);
    ll += fprintf(ip->out, " -dDryTime=%d", ip->fdata.dry_time);
  }

  /* Resolution */
  if (min_hres != 0) {
    check_line_length(ip->out, &ll, 12);
    ll += fprintf(ip->out, " -r%u", min_hres);
    if (min_vres != min_hres) ll += fprintf(ip->out, "x%u", min_vres);
  }

  /* Colour model */
  if (ip->fdata.palette != pcl_no_palette) {
    check_line_length(ip->out, &ll, sizeof("-sColourModel=CMY+K"));
    ll += fprintf(ip->out, " -sColourModel=");
    switch (ip->fdata.palette) {
      case pcl_black:
        ll += fprintf(ip->out, "Gray"); break;
      case pcl_RGB:
        ll += fprintf(ip->out, "RGB"); break;
      case pcl_CMY:
        ll += fprintf(ip->out, "CMY"); break;
      case pcl_CMYK:
        if (ip->seen_new_quality || ip->seen_CRD)	/* Guess */
          ll += fprintf(ip->out, "CMYK");
        else
          ll += fprintf(ip->out, "CMY+K");
        break;
      default:
        assert(0);	/* bug guard */
    }
  }

  /* Intensity levels */
  if (ip->fdata.palette == pcl_CMY) {
    if (non_black_levels > 2) {
      check_line_length(ip->out, &ll, sizeof("-dCMYLevels=") + 2);
      ll += fprintf(ip->out, " -dCMYLevels=%d", non_black_levels);
    }
  }
  else {
    if (black_levels > 2) {
      check_line_length(ip->out, &ll, sizeof("-dBlackLevels=") + 2);
      ll += fprintf(ip->out, " -dBlackLevels=%d", black_levels);
    }
    if (ip->fdata.palette == pcl_CMYK && non_black_levels > 2) {
      check_line_length(ip->out, &ll, sizeof("-dCMYLevels=") + 2);
      ll += fprintf(ip->out, " -dCMYLevels=%d", non_black_levels);
    }
  }

  /* Print quality, possibly also medium */
  if (ip->seen_new_quality) {
    check_line_length(ip->out, &ll, sizeof("-sPrintQuality=presentation"));
    ll += fprintf(ip->out, " -sPrintQuality=");
    switch (ip->fdata.print_quality) {
      case -1:
        ll += fprintf(ip->out, "draft"); break;
      case 0:
        ll += fprintf(ip->out, "normal"); break;
      case 1:
        ll += fprintf(ip->out, "presentation"); break;
      default:
        ll += fprintf(ip->out, "%d", ip->fdata.print_quality);
    }

    check_line_length(ip->out, &ll,
      sizeof("-sMedium='quick dry transparency'"));
    ll += fprintf(ip->out, " -sMedium=");
    switch (ip->fdata.media_type) {
      case 0:
        ll += fprintf(ip->out, "plain"); break;
      case 1:
        ll += fprintf(ip->out, "bond"); break;
      case 2:
        ll += fprintf(ip->out, "Premium"); break;
      case 3:
        ll += fprintf(ip->out, "glossy"); break;
      case 4:
        ll += fprintf(ip->out, "transparency"); break;
      case 5:
        ll += fprintf(ip->out, "'quick dry glossy'"); break;
      case 6:
        ll += fprintf(ip->out, "'quick dry transparency'"); break;
      default:
        ll += fprintf(ip->out, "%d", ip->fdata.media_type);
    }
  }
  else if (ip->seen_old_quality) {
    if (ip->fdata.depletion != 0) {
      check_line_length(ip->out, &ll, sizeof("-dDepletion=") + 2);
      ll += fprintf(ip->out, " -dDepletion=%d", ip->fdata.depletion);
    }
    if (ip->fdata.shingling != -1) {
      check_line_length(ip->out, &ll, sizeof("-dShingling=") + 2);
      ll += fprintf(ip->out, " -dShingling=%d", ip->fdata.shingling);
    }
    if (ip->fdata.raster_graphics_quality != -1) {
      check_line_length(ip->out, &ll,
        sizeof("-dRasterGraphicsQuality=") + 2);
      ll += fprintf(ip->out, " -dRasterGraphicsQuality=%d",
        ip->fdata.raster_graphics_quality);
    }
  }

  /* Compression. We display this option only if the value is different from
     what pcl3 would otherwise use. */
  if (ip->fdata.compression != default_compression) {
    check_line_length(ip->out, &ll, sizeof("-dCompressionMethod=") + 1);
    ll += fprintf(ip->out, " -dCompressionMethod=%d", ip->fdata.compression);
  }

  fputs("\n\n", ip->out);
  if (ip->fdata.palette == pcl_CMYK && !ip->seen_first_colorant) {
    imessage(ip->out, 70,
      "Although the colour model in this file is declared as CMYK, "
        "the file does not\n"
      "use the black colorant. If you find that the printer does not accept "
        "a file\n"
      "generated with the command line above, "
        "try the CMY colour model instead.\n\n");
  }

  if (ip->fdata.size != 0) {
    ms_MediaCode media_code = pcl3_media_code(ip->fdata.size);
    const ms_SizeDescription *size = ms_find_size_from_code(media_code);
    if (size == NULL)
      imessage(ip->out, 14,
        "This file uses a PCL page size code (%d) which is unknown to pcl3.\n",
        (int)ip->fdata.size);
    else {
      imessage(ip->out, 15, "The page size is set to %s", size->name);
       /* I'm using the size specification as an indication of the PCL Page Size
          code used, hence I'm adding the card flag if present. Note that
          adding "Big" is not useful. */
      if (media_code & PCL_CARD_FLAG) fputs(PCL_CARD_STRING, ip->out);
      if (size->dimen[0] > 0) {
        const char *unit = catgets(catd, 2, 16, "mm");

        fputs(" (", ip->out);
        if (strcmp(unit, "bp") == 0)
          fprintf(ip->out, "%.0f x %.0f bp", size->dimen[0],
            size->dimen[1]);
        else if (strcmp(unit, "in") == 0)
          fprintf(ip->out, "%.1f x %.1f in", size->dimen[0]/BP_PER_IN,
            size->dimen[1]/BP_PER_IN);
        else
          fprintf(ip->out, "%.0f x %.0f mm", size->dimen[0]/BP_PER_MM,
            size->dimen[1]/BP_PER_MM);
        fputc(')', ip->out);
      }
      fputs(".\n", ip->out);
    }
  }
  if (ip->fdata.media_source != 0 && ip->fdata.media_source != 1)
    imessage(ip->out, 19, "The media source (input tray) selected is %d.\n",
      ip->fdata.media_source);
  if (ip->fdata.media_destination != 0)
    imessage(ip->out, 17,
      "The media destination (output tray) selected is %d.\n",
      ip->fdata.media_destination);
  if (ip->fdata.media_source != 0 && ip->fdata.media_source != 1 ||
      ip->fdata.media_destination != 0)
    imessage(ip->out, 18,
      "To be able to select a media position (input or output) you will have "
        "to\n"
      "configure the `InputAttributes' or `OutputAttributes' dictionaries\n"
      "appropriately. See the manual page gs-pcl3(1).\n");

  return;
}

/*****************************************************************************/

static int action_PageSize(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->fdata.size = cmd->i;

  return 0;
}

/*****************************************************************************/

static int action_dry_time(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->fdata.dry_time = cmd->i;

  return 0;
}

/*****************************************************************************/

static int action_destination(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->fdata.media_destination = cmd->i;

  return 0;
}

/*****************************************************************************/

static int action_source(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  if (cmd->i != 0 && cmd->i != -2) {
    if (cmd->i == 2) ip->fdata.manual_feed = TRUE;
    else ip->fdata.media_source = cmd->i;
  }

  return 0;
}

/*****************************************************************************/

static int action_media_type(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->seen_new_quality = TRUE;
  ip->fdata.media_type = cmd->i;

  return 0;
}

/*****************************************************************************/

static int action_duplex(gp_file *in, const pcl_Command *cmd, void *i)
{								/* NLS: 60 */
  CollectedInfo *ip = i;

  ip->fdata.duplex = cmd->i;
  if (ip->fdata.duplex < 0 || 2 < ip->fdata.duplex)
    imessage(ip->out, 60, "I've found an unknown value for the command to "
      "select\nsimplex/duplex printing: %d.\n", ip->fdata.duplex);

  return 0;
}

/*****************************************************************************/

/* PJL white space. Note: no expressions with side effects permitted. */
#define PJL_ws(c)	((c) == ' ' || (c) == '\t')

/******************************************************************************

  Function to check "@PJL" prefix

  Return codes:
   0: starts correctly, prefix has been read,
   1: starts wrong, read characters have been pushed back,
  -1: starts wrong, read characters could not be pushed back and an error
      message has been issued.
*/

static int check_prefix(gp_file *in)
{
  static const pcl_Octet prefix[] = "@PJL";
    /* Note that the "@PJL" prefix is required to be uppercase in PJL. */
  int c, j;

  /* Check for prefix followed by white space or EOL */
  j = 0;	/* number of matching characters read */
  while (j < 4 && (c = fgetc(in)) == prefix[j]) j++;
  if (j == 4) {
    c = fgetc(in);
    if (PJL_ws(c) || c == '\r' || c == '\n') j++;
    /* I'm assuming that we're dealing with legal PJL, i.e., the CR is followed
       by LF. It is not the responsibility of this program to discover PJL
       syntax errors. */
  }
  if (j < 5) {
    /* Keep in mind that one byte of push-back is guaranteed to work. */
    if (c != EOF) ungetc(c, in);
    while (j > 0) {
      j--;
      if (ungetc(prefix[j], in) == EOF) {
        emessage(80,
          "Error trying to push back characters while parsing PJL:\n  %s.\n",
          strerror(errno));
        /* This should only happen for unprofessional PJL, i.e., without a
           terminating ENTER LANGUAGE. */
        return -1;
      }
    }

    return 1;
  }

  /* Ensure we've not read past the end of the line */
  if (c == '\n') ungetc(c, in);

  return 0;
}

/*****************************************************************************/

#define PJL_letter(c) ('a' <= (c) && (c) <= 'z' || 'A' <= (c) && (c) <= 'Z')

/*****************************************************************************/

#define TILE_SIZE	200

static int action_UEL(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;
  int c;
  pcl_Octet *buffer, *s;
  size_t allocated, used;

  if (cmd->i != 12345) return 1;

  /* As we have entered PJL, the previous personality is irrelevant. */
  if (ip->fdata.PJL_language != NULL) {
    free(ip->fdata.PJL_language); ip->fdata.PJL_language = NULL;
  }

  buffer = (pcl_Octet *)malloc(TILE_SIZE);
  check(buffer);
  allocated = TILE_SIZE;

  /* Read one PJL command at a time (necessary in order to catch ENTER) */
  do {
    c = check_prefix(in);
    if (c != 0) c = EOF;
    else {
      /* Read up to EOL or EOF */
      used = 0;
      while ((c = fgetc(in)) != EOF && c != '\n') {
        if (used >= allocated - 1) {
          allocated += TILE_SIZE;
          buffer = (pcl_Octet *)realloc(buffer, allocated);
          check(buffer);
        }
        buffer[used] = c;
        used++;
      }
      /* Remove an optional trailing CR */
      if (used > 0 && buffer[used-1] == 'r') used--;
      buffer[used] = '\0';

      /* Skip optional whitespace */
      s = buffer;
      while (PJL_ws(*s)) s++;

      if (PJL_letter(*s)) {
        pcl_Octet *t;
        int l;

        /* Isolate command and convert to upper case. Note that we're running
           in an internationalized environment, hence we can't use toupper(). */
        t = s;
        do {
          if ('a' <= *t && *t <= 'z') *t -= 'a' - 'A';
          t++;
        } while (*t != '\0' && !PJL_ws(*t));

        /* Identify command */
        l = t - s;
#if 0
        fprintf(stderr, "Command (length %d): `", l);
        fwrite(s, 1, l, stderr);
        fprintf(stderr, "'.\n");
#endif
        if (l == sizeof("ENTER") - 1 && strncmp(s, "ENTER", l) == 0 &&
            PJL_ws(*t)) {
          /* Check for "LANGUAGE" */
          s = t + 1;
          while (PJL_ws(*s)) s++;
          t = s;
          while (*t != '\0' && !PJL_ws(*t) && *t != '=') {
            if ('a' <= *t && *t <= 'z') *t -= 'a' - 'A';
            t++;
          }
          l = t - s;
          if (l == sizeof("LANGUAGE") - 1 && strncmp(s, "LANGUAGE", t - s) == 0
              && (PJL_ws(*t) || *t == '=')) {
            s = strchr(t, '=');	/* again assuming legal PJL */
            if (s != NULL) {
              s++;
              while (PJL_ws(*s)) s++;
              if (PJL_letter(*s)){
                t = s + 1;
                while (*t != '\0' && !PJL_ws(*t)) t++;
                ip->fdata.PJL_language = (char *)malloc(t - s + 1);
                check(ip->fdata.PJL_language);
                strncpy(ip->fdata.PJL_language, s, t - s);
                ip->fdata.PJL_language[t - s] = '\0';
              }
            }
            c = EOF;	/* exit PJL and this loop */
          }
        }
        else if (l == 3 && strncmp(s, "JOB", 3) == 0) {
          /* Don't bother to identify the job name; let the user create an
             unnamed job. */
          static char jobname[] = "";
          ip->fdata.PJL_job = jobname;
        }
      }
    }
  }
  while (c != EOF);

  free(buffer);

  return 0;
}

/*****************************************************************************/

                                                                /* NLS: 20 */
static int action_compression(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  if (cmd->i != pcl_cm_none && cmd->i != pcl_cm_rl && cmd->i != pcl_cm_tiff &&
      cmd->i != pcl_cm_delta && cmd->i != pcl_cm_crdr) {
    if (!ip->seen_unknown_compression)
      imessage(ip->out, 20,
        "This file uses at least one compression method for raster data (%d)\n"
        "which pcl3 does not know.\n", cmd->i);
    ip->seen_unknown_compression = TRUE;
  }
  else {
    if (cmd->i > ip->fdata.compression) ip->fdata.compression = cmd->i;
  }

  return 0;
}

/*****************************************************************************/

/* The following function is only called for "*bV" and "*bW". */

static int action_raster_data(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;
  int j;

  ip->seen_raster_data = TRUE;
  if (!ip->seen_first_colorant && cmd->i > 0 &&
      ip->next_plane < ip->first_colorant_planes)
    ip->seen_first_colorant = TRUE;

  for (j = cmd->i; j > 0; j--) fgetc(in);
  if (cmd->chars[2] == 'W') ip->next_plane = 0; else ip->next_plane++;

  return 0;
}

/*****************************************************************************/

#define two_octets(index)	(buffer[index]*256 + buffer[(index) + 1])

/*****************************************************************************/

static void print_CRD(gp_file *out, const pcl_Octet *buffer)
{
  int j;

  for (j = 0; j < buffer[1]; j++)
    imessage(out, 12, "    colorant %d: %4d x %4d ppi %2d levels\n",
      j + 1, two_octets(2 + 6*j), two_octets(4 + 6*j),
      two_octets(6 + 6*j));

  return;
}

/*****************************************************************************/

static int action_CRD(gp_file *in, const pcl_Command *cmd, void *i) /* NLS: 30 */
{
  CollectedInfo *ip = i;
  pcl_Octet *buffer;
  int errors = 0;

  ip->seen_CRD = TRUE;

  if (cmd->i <= 0) {
    ip->CRD_active = FALSE;
    ip->fdata.palette = pcl_no_palette;
    ip->fdata.number_of_colorants = 1;
    ip->fdata.colorant_array[0].hres = ip->fdata.colorant_array[0].vres = 75;
    ip->fdata.colorant_array[0].levels = 2;
    ip->first_colorant_planes = 1;
    return 0;
  }
  ip->CRD_active = TRUE;

  buffer = (pcl_Octet *)malloc(cmd->i);
  check(buffer);
  fread(buffer, sizeof(pcl_Octet), cmd->i, in);

  if (buffer[0] == 2) {
    /* Just occasionally, a little paranoia is good for the soul, let alone for
       for the reliability of the program! */
    if (2 + buffer[1]*6 != cmd->i) {
      emessage(30, "Illegal field length in Configure Raster Data command.\n");
      return -1;
    }

    if (buffer[1] == 1 || buffer[1] == 3 || buffer[1] == 4) {
      int j;

      ip->fdata.number_of_colorants = buffer[1];
      switch (ip->fdata.number_of_colorants) {
        case 1:
          ip->fdata.palette = pcl_black; break;
        case 3:
          ip->fdata.palette = pcl_CMY; break;
        case 4:
          ip->fdata.palette = pcl_CMYK; break;
      }

      for (j = 0; j < ip->fdata.number_of_colorants; j++) {
        ip->fdata.colorant_array[j].hres = two_octets(2 + 6*j);
        ip->fdata.colorant_array[j].vres = two_octets(4 + 6*j);
        ip->fdata.colorant_array[j].levels = two_octets(6 + 6*j);
      }

      {
        int power = 1;
        ip->first_colorant_planes = 0;

        while (power < ip->fdata.colorant_array[0].levels) {
          power *= 2;
          ip->first_colorant_planes++;
        }
      }
    }
    else {
      emessage(31,
        "This file contains a Configure Raster Data command with an\n"
        "  illegal number of colorants (%d) and the following data:\n",
        buffer[1]);
      print_CRD(stderr, buffer);
      errors++;
    }
  }
  else {
    emessage(32,
      "This file contains a Configure Raster Data command with an\n"
      "  unknown format (%d).\n",
      (int)buffer[0]);
    if (cmd->i > 2 && cmd->i == 2 + buffer[1]*6) {
      imessage(stderr, 33, "  It *might* mean the following:\n");
      print_CRD(stderr, buffer);
      imessage(stderr, 34, "  This format is not supported by pcl3.\n");
    }
    errors++;
  }
  free(buffer);

  return errors > 0? -1: 0;
}

/*****************************************************************************/

static int action_old_quality(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->seen_old_quality = TRUE;
  if (cmd->chars[1] == 'o') {
    if (cmd->chars[2] == 'D') ip->fdata.depletion = cmd->i;
    else if (cmd->chars[2] == 'Q') ip->fdata.shingling = cmd->i;
  }
  else if (cmd->chars[1] == 'r' && cmd->chars[2] == 'Q')
    ip->fdata.raster_graphics_quality = cmd->i;

  return 0;
}

/*****************************************************************************/

static int action_quality(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->seen_new_quality = TRUE;
  ip->fdata.print_quality = cmd->i;

  return 0;
}

/*****************************************************************************/

                                                                /* NLS: 40 */
static int action_colour(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  if (!ip->CRD_active) {
    switch (cmd->i) {
      case -4:
        ip->fdata.palette = pcl_CMYK; break;
      case -3:
        ip->fdata.palette = pcl_CMY; break;
      case -1:
        /*FALLTHROUGH*/
      case 0:
        /*FALLTHROUGH*/
      case 1:
        ip->fdata.palette = pcl_black; break;
      case 3:
        ip->fdata.palette = pcl_RGB; break;
      default:
        emessage(41, "This file uses a palette (%d) not supported by pcl3.\n",
          cmd->i);
        return -1;
    }
    ip->fdata.number_of_colorants = abs(cmd->i);

    {
      int j;

      for (j = 0; j < ip->fdata.number_of_colorants; j++) {
        ip->fdata.colorant_array[j].hres = ip->fdata.colorant_array[j].vres =
          ip->fdata.colorant_array[0].hres;
        ip->fdata.colorant_array[j].levels = 2;
      }
    }

    ip->first_colorant_planes = 1;
  }

  return 0;
}

/*****************************************************************************/

static int action_resolution(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  ip->seen_RGR = TRUE;

  if (!ip->CRD_active) {
    int j;

    for (j = 0; j < ip->fdata.number_of_colorants; j++) {
      ip->fdata.colorant_array[j].hres =
        ip->fdata.colorant_array[j].vres = cmd->i;
      ip->fdata.colorant_array[j].levels = 2;
    }
  }

  return 0;
}

/*****************************************************************************/

static int action_end_raster(gp_file *in, const pcl_Command *cmd, void *i)
{
  CollectedInfo *ip = i;

  if (cmd->chars[2] == 'B') ip->seen_ERG_B = TRUE;
  else if (cmd->chars[2] == 'C') ip->seen_ERG_C = TRUE;

  if (ip->seen_raster_data &&
      memcmp(&ip->fdata, &ip->fdata_old, sizeof(pcl_FileData)) != 0) {
    print_result(ip);
    memcpy(&ip->fdata_old, &ip->fdata, sizeof(pcl_FileData));
  }
  ip->seen_raster_data = FALSE;

  return 0;
}

/*****************************************************************************/

typedef struct {
  char cmd[4];
  pcl_CommandInterpreter action;
} ActionEntry;

static ActionEntry action_table[] = {
  {"%-X", &action_UEL},			/* Universal Exit Language */
  {"&bT", &action_dry_time},		/* Set Dry Time */
  {"&lA", &action_PageSize},		/* Page Size */
  {"&lG", &action_destination},		/* Set Output Bin */
  {"&lH", &action_source},		/* Media Source */
  {"&lM", &action_media_type},		/* Media Type */
  {"&lS", &action_duplex},		/* Simplex/Duplex Print */
  {"*bM", &action_compression},		/* Raster Graphics Compression Method */
  {"*bV", &action_raster_data},	    /* Transfer Raster Graphics Data by Plane */
  {"*bW", &action_raster_data},	      /* Transfer Raster Graphics Data by Row */
  {"*gW", &action_CRD},			/* Configure Raster Data */
  {"*oD", &action_old_quality},		/* Set Raster Graphics Depletion */
  {"*oM", &action_quality},		/* Print Quality */
  {"*oQ", &action_old_quality},		/* Set Raster Graphics Shingling */
  {"*rB", &action_end_raster},		/* End Raster Graphics (old) */
  {"*rC", &action_end_raster},		/* End Raster Graphics (new) */
  {"*rQ", &action_old_quality},		/* Raster Graphics Quality */
  {"*rU", &action_colour},		/* Set Number of Planes Per Row */
  {"*tR", &action_resolution}		/* Set Raster Graphics Resolution */
};

/*****************************************************************************/

static int cmp_entries(const void *a, const void *b)
{
  return
    strncmp(((const ActionEntry *)a)->cmd, ((const ActionEntry *)b)->cmd, 3);
}

/*****************************************************************************/

static int analyzer(gp_file *in, const pcl_Command *cmd, void *i)
{
  ActionEntry key;
  const ActionEntry *entry;
  CollectedInfo *ip = i;

  /* Control codes */
  if (cmd->kind == 1) {
    if (cmd->chars[0] == '\0' &&
        /* Guard against NULs somewhere else except initially */
        !ip->seen_RGR && !ip->seen_CRD)
      ip->fdata.NULs_to_send = cmd->i;
    return 0;
  }

  /* Two-character escape sequences */
  if (cmd->kind == 2) {
    switch (cmd->chars[0]) {
    case 'E':	/* Printer Reset */
      {
        /* Preserve some global data (mostly PJL state) across PCL Reset
           invocations */
        unsigned int NULs_to_send = ip->fdata.NULs_to_send;
        char *PJL_language = ip->fdata.PJL_language;
        char *PJL_job = ip->fdata.PJL_job;

        ip->CRD_active = FALSE;
        ip->seen_raster_data = FALSE;
        memset(&ip->fdata, 0, sizeof(pcl_FileData));
        ip->fdata.colorant_array[0].hres =
          ip->fdata.colorant_array[0].vres = 75;
        ip->fdata.colorant_array[0].levels = 2;
        ip->fdata.number_of_colorants = 1;
        ip->fdata.duplex = -1;
        ip->fdata.dry_time = -1;
        ip->fdata.raster_graphics_quality = -1;
        ip->fdata.shingling = -1;

        ip->fdata.NULs_to_send = NULs_to_send;
        ip->fdata.PJL_language = PJL_language;
        ip->fdata.PJL_job = PJL_job;
      }
      break;
    default:
      return 1;
    }
    return 0;
  }

  /* Parameterized escape sequences */
  strncpy(key.cmd, cmd->chars, 3);
  entry = (const ActionEntry *)bsearch(&key, action_table,
    sizeof(action_table)/sizeof(ActionEntry), sizeof(ActionEntry), cmp_entries);
  if (entry != NULL) return (*entry->action)(in, cmd, ip);

  return 1;
}

/******************************************************************************

  Function: work

  On errors, the return value is positive, on success it is zero.

******************************************************************************/

static int work(gp_file *in, gp_file *out)				/* NLS: 50 */
{
  CollectedInfo info;
  int rc;

  memset(&info, 0, sizeof(CollectedInfo));
  info.out = out;
  info.fdata.duplex = -1;
  info.fdata.dry_time = -1;
  info.fdata.raster_graphics_quality = -1;
  info.fdata.shingling = -1;

  rc = pcl_scan(in, &analyzer, &info, NULL, NULL);
  if (rc != 0) return 1;

  if (info.number_of_outputs == 0) {
    emessage(50,
      "This does not seem to be a PCL file containing raster data.\n");
    return 1;
  }

  return 0;
}

/******************************************************************************

  Function: main

  Usage: pcl3opts <options> <input files>
  <option> ::= '-o' <output file>
  <input files> ::= <empty> | <input file> <input files>

******************************************************************************/

int main(int argc, char **argv)
{
  char
    *outfile_name = NULL;
  gp_file
    *outfile,
    *infile;
  int
    c,
    errors = 0,
    number_of_infiles;

#ifndef NDEBUG
  {
    int j;

    for (j = 1; j < sizeof(action_table)/sizeof(ActionEntry); j++)
      assert(cmp_entries(action_table + (j - 1), action_table + j) < 0);
  }
#endif /* !NDEBUG */

  /* Set the locale depending on the environment */
  setlocale(LC_ALL, "");
  catd = catopen("pcl3opts", 0);

  puts("*** pcl3opts\n");	/* Note that puts() appends another '\n'. */

  /* Set the program's name. We assume: argc <= 0 || parts(argv) >= 1. */
  if (argc <= 0 || argv[0] == NULL || argv[0][0] == '\0')
    progname = "pcl3opts";
  else {
    if ((progname = strrchr(argv[0], '/')) == NULL || progname[1] == '\0')
      progname = argv[0];
    else
      progname++;
  }

  /* Parse the options */
  while ((c = getopt(argc, argv, "o:")) != EOF) {
    switch(c) {
    case 'o':
      outfile_name = optarg;
      break;
    default:
      errors++;
      break;
    }
  }

  /* Leave on errors in the command line */
  if (errors > 0) {
    emessage(1, "Usage: %s [<options>] [<input files>].\n"
      "  <option> ::= -o <output file>\n",
      progname);
    exit(EXIT_FAILURE);
  }

  /* Open the output file */
  if (outfile_name == NULL) outfile = stdout;
  else {
    outfile = fopen(outfile_name, "w");
    if (outfile == NULL) {
      emessage(2, "The file `%s' could not be opened for writing:\n  %s.\n",
        outfile_name, strerror(errno));
      exit(EXIT_FAILURE);
    }
  }

  /* Loop over the input files */
  number_of_infiles = argc - optind;
  if (number_of_infiles <= 0) errors += work(stdin, outfile);
  else do {
    char *infile_name;

    /* Open */
    infile_name = argv[optind];
    infile = fopen(infile_name, "rb");
    if (infile == NULL) {
      emessage(3, "Could not open the input file `%s':\n  %s.\n",
        infile_name, strerror(errno));
      exit(EXIT_FAILURE);
    }

    if (number_of_infiles > 1) fprintf(outfile, "*** %s:\n", infile_name);
    errors += work(infile, outfile);

    /* Close */
    if (fclose(infile) != 0) {
      emessage(4, "Error closing `%s':\n  %s.\n",
        infile_name, strerror(errno));
      errors++;
    }
    optind++;
    if (errors == 0 && optind < argc) fputs("\n", outfile);
  } while (errors == 0 && optind < argc);

  /* Close the output file */
  if (fclose(outfile) != 0) {
    emessage(4, "Error closing `%s':\n  %s.\n",
      outfile_name == NULL? "[standard output]": outfile_name, strerror(errno));
    errors++;
  }

  catclose(catd);

  exit(errors == 0? EXIT_SUCCESS: EXIT_FAILURE);
}
