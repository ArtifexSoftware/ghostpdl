/******************************************************************************
  File:     $Id: gdeveprn.c,v 1.25 2001/04/30 05:15:51 Martin Rel $
  Contents: Implementation of the abstract ghostscript device 'eprn':
            general functions and page layout
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*                                                                             *
*       Copyright (C) 2000, 2001 by Martin Lottermoser                        *
*       All rights reserved                                                   *
*                                                                             *
*******************************************************************************

  Preprocessor variables:

    EPRN_NO_PAGECOUNTFILE
        Define this if you do not want to use eprn's pagecount-file feature.
        You very likely must define this on Microsoft Windows.

    EPRN_TRACE
        Define this to enable tracing. Only useful for development.

    EPRN_USE_GSTATE (integer)
        Define this to be non-zero if the graphics state should be accessed
        directly instead of via the interpreter context state. Newer ghostscript
        versions require the latter path. The default is zero unless
        GS_REVISION is defined and less than 600.

    GS_REVISION (integer)
        If defined, this must be the ghostscript version number, e.g., 601 for
        ghostscript 6.01.

******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE   500
#endif

/* Preprocessor symbol with version-dependent default */
#ifndef EPRN_USE_GSTATE
#if !defined(GS_REVISION) || GS_REVISION >= 600
#define EPRN_USE_GSTATE 0
#else
#define EPRN_USE_GSTATE 1
#endif
#endif  /* !EPRN_USE_GSTATE */

/*****************************************************************************/

/* Special Aladdin header, must be included before <sys/types.h> on some
   platforms (e.g., FreeBSD). */
#include "std.h"

/* Standard headers */
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef EPRN_TRACE
#include <time.h>
#endif  /* EPRN_TRACE */

/*  Ghostscript headers. With the exception of gdebug.h, these files are only
    needed to compile eprn_forget_defaultmatrix() which needs the prototypes
    for gs_setdefaultmatrix() (in gscoord.h) and gs_main_instance_default()
    (in imain.h). Unfortunately and in disregard of good SE practice,
    ghostscript's header files are not self-contained. Therefore, if this file
    does not compile because of undefined symbols, just add include directives
    until it does.  */
#include "gserrors.h"
#include "iref.h"       /* needed by icstate.h */
#include "gsmemraw.h"   /* needed by icstate.h */
#include "gsmemory.h"   /* needed by icstate.h */
#include "gstypes.h"    /* needed by gsstate.h */
#include "gsstate.h"    /* needed by icstate.h */
#include "icstate.h"    /* for struct gs_context_state_s */
#if !defined(GS_REVISION) || GS_REVISION >= 700
#include "iapi.h"       /* needed by iminst.h */
#endif  /* GS_REVISION */
#include "iminst.h"     /* for struct gs_main_instance_s */
#include "imain.h"      /* for gs_main_instance_default() */
#include "gscoord.h"    /* for gs_setdefaultmatrix() */
#if EPRN_USE_GSTATE
#include "igstate.h"
#endif  /* EPRN_USE_GSTATE */
#ifdef EPRN_TRACE
#include "gdebug.h"
#endif  /* EPRN_TRACE */
#include "gxstdio.h"

/* Special headers for this device */
#ifndef EPRN_NO_PAGECOUNTFILE
#include "pagecount.h"
#endif  /* EPRN_NO_PAGECOUNTFILE */
#include "gdeveprn.h"

/*****************************************************************************/

/* Prefix for error messages */
#define ERRPREF "? eprn: "


/******************************************************************************

  Function: eprn_get_initial_matrix

  This function returns the initial matrix for the device.

  The result is based on the following parameters:
  - eprn: default_orientation, down_shift, right_shift, soft_tumble
  - HWResolution, MediaSize, ShowpageCount

******************************************************************************/

void eprn_get_initial_matrix(gx_device *device, gs_matrix *mptr)
{
  eprn_Device *dev = (eprn_Device *)device;
  float
    /*  The following two arrays are oriented w.r.t. pixmap device space, i.e.,
        the index 0 refers to the x coordinate (horizontal) and the index 1 to
        the y coordinate (vertical) in pixmap device space. */
    extension[2],       /* media extension in pixels */
    pixels_per_bp[2];   /* resolution */
  int
    j,
    quarters;

#ifdef EPRN_TRACE
  if_debug0(EPRN_TRACE_CHAR, "! eprn_get_initial_matrix()...\n");
#endif

  /* We need 'default_orientation' and also the margins. */
  if (dev->eprn.code == ms_none) {
#ifdef EPRN_TRACE
    if_debug0(EPRN_TRACE_CHAR,
      "! eprn_get_initial_matrix(): code is still ms_none.\n");
#endif
    if (eprn_set_page_layout(dev) != 0)
      eprintf("  Processing can't be stopped at this point although this error "
        "occurred.\n");
      /* The current function has a signature without the ability to signal
         an error condition. */
  }

  quarters = dev->eprn.default_orientation +
    (dev->MediaSize[0] <= dev->MediaSize[1]? 0: 1);
     /* Number of quarter-circle rotations by +90 degrees necessary to obtain
        default user space starting with the y axis upwards in pixmap device
        space.
        It's not documented, but 'MediaSize' is the requested "PageSize" page
        device parameter value and hence is to be interpreted in default (not
        default default!) user space. The condition above therefore tests
        whether landscape orientation has been requested.
      */

  /* Soft tumble option: rotate default user space by 180 degrees on every
     second page */
  if (dev->eprn.soft_tumble && dev->ShowpageCount % 2 != 0) quarters += 2;

  /* Prepare auxiliary data */
  for (j = 0; j < 2; j++) pixels_per_bp[j] = dev->HWResolution[j]/BP_PER_IN;
  /*  'HWResolution[]' contains the standard PostScript page device parameter
      'HWResolution' which is defined in pixels per inch with respect to
       device space. */
  if (quarters % 2 == 0) {
    /* Default user space and pixmap device space agree in what is "horizontal"
       and what is "vertical". */
    extension[0] = dev->MediaSize[0];
    extension[1] = dev->MediaSize[1];
  }
  else {
    extension[0] = dev->MediaSize[1];
    extension[1] = dev->MediaSize[0];
  }
  /* Convert from bp to pixels: */
  for (j = 0; j < 2; j++) extension[j] *= pixels_per_bp[j];
   /* Note that we are using the user-specified extension of the sheet, not the
      "official" one we could obtain in most cases from 'size'. */

  switch (quarters % 4) {
  case 0:
    /*  The y axis of default user space points upwards in pixmap device space.
        The CTM is uniquely characterized by the following mappings from
        default user space to pixmap device space:
          (0, 0)                -> (0, height in pixels)
          (width in bp, 0)      -> (width in pixels, height in pixels)
          (0, height in bp)     -> (0, 0)
        'width' and 'height' refer to the sheet's extension as seen from pixmap
        device space, i.e., width in pixels == extension[0] and
        height in pixels == extension[1].

        From the PLR we find that the CTM is a PostScript matrix
        [a b c d tx ty] used for mapping user space coordinates (x, y) to
        device space coordinates (x', y') as follows:
          x' = a*x + c*y + tx
          y' = b*x + d*y + ty
        Ghostscript's matrix type 'gs_matrix' writes its structure components
        'xx' etc. in storage layout order into a PostScript matrix (see
        write_matrix() in iutil.c), hence we obtain by comparison with
        gsmatrix.h the PostScript matrix [ xx xy yx yy tx ty ].
        The correspondence can also be seen by comparison of the equations
        above with the code in gs_point_transform() in gsmatrix.c.
        It would, however, still be reassuring to have a corresponding
        statement in ghostscript's documentation.
    */
    gx_default_get_initial_matrix(device, mptr);
    /*  Of course, I could also set this directly:
          mptr->xx = pixels_per_bp[0];
          mptr->xy = 0;
          mptr->yx = 0;
          mptr->yy = -pixels_per_bp[1];
          mptr->tx = 0;
          mptr->ty = extension[1];
        Doing it in this way is, however, more stable against dramatic changes
        in ghostscript.
    */
    break;
  case 1:
    /*  The y axis of default user space points to the left in pixmap device
        space. The CTM is uniquely characterized by the following mappings from
        default user space to pixmap device space:
          (0, 0)                -> (width in pixels, height in pixels)
          (height in bp, 0)     -> (width in pixels, 0)
          (0, width in bp)      -> (0, height in pixels)
    */
    mptr->xx = 0;
    mptr->xy = -pixels_per_bp[1];
    mptr->yx = -pixels_per_bp[0];
    mptr->yy = 0;
    mptr->tx = extension[0];
    mptr->ty = extension[1];
    break;
  case 2:
    /*  The y axis of default user space points downwards in pixmap device
        space. The CTM is uniquely characterized by the following mappings from
        default user space to pixmap device space:
          (0, 0)                -> (width in pixels, 0)
          (width in bp, 0)      -> (0, 0)
          (0, height in bp)     -> (width in pixels, height in pixels)
    */
    mptr->xx = -pixels_per_bp[0];
    mptr->xy = 0;
    mptr->yx = 0;
    mptr->yy = pixels_per_bp[1];
    mptr->tx = extension[0];
    mptr->ty = 0;
    break;
  case 3:
    /*  The y axis of default user space points to the right in pixmap device
        space. The CTM is uniquely characterized by the following mappings from
        default user space to pixmap device space:
          (0, 0)                -> (0, 0)
          (height in bp, 0)     -> (0, height in pixels)
          (0, width in bp)      -> (width in pixels, 0)
    */
    mptr->xx = 0;
    mptr->xy = pixels_per_bp[1];
    mptr->yx = pixels_per_bp[0];
    mptr->yy = 0;
    mptr->tx = 0;
    mptr->ty = 0;
    break;
  }

  /*  Finally, shift the device space origin to the top-left corner of the
      printable area. I am deliberately not using the corresponding shift
      feature in gx_device_set_margins() because it achieves its effect by
      using the 'Margins' array which should remain at the user's disposal for
      correcting misadjustments. In addition, gx_device_set_margins() will not
      work correctly for quarters % 4 != 0 anyway.
  */
  {
    gs_matrix translation;

    /*  Translation of pixmap device space origin by top and left margins in
        pixmap device space */
    gs_make_translation(
      -dev->eprn.right_shift*pixels_per_bp[0],
      -dev->eprn.down_shift *pixels_per_bp[1],
      &translation);

    /* Multiply the initial matrix from the right with the translation matrix,
       i.e., in going from user to device space the translation will be applied
       last. */
    gs_matrix_multiply(mptr, &translation, mptr);
  }

#ifdef EPRN_TRACE
  if_debug6(EPRN_TRACE_CHAR, "  Returning [%g %g %g %g %g %g].\n",
    mptr->xx, mptr->xy, mptr->yx, mptr->yy, mptr->tx, mptr->ty);
#endif
  return;
}

/******************************************************************************

  Function: print_flags

  Print a textual description of 'flags' to the error stream.

******************************************************************************/

static void print_flags(ms_MediaCode flags, const ms_Flag *user_flags)
{
  /* Non-standard flags first */
  if (user_flags != NULL) {
    while (user_flags->code != ms_none) {
      if (user_flags->code & flags) {
        eprintf1("%s", user_flags->name);
        flags &= ~user_flags->code;
      }
      user_flags++;
    }
  }

  /* Standard substrings */
  if (flags & MS_SMALL_FLAG) eprintf(MS_SMALL_STRING);
  if (flags & MS_BIG_FLAG  ) eprintf(MS_BIG_STRING);
  if (flags & MS_EXTRA_FLAG) eprintf(MS_EXTRA_STRING);
  flags &= ~(MS_SMALL_FLAG | MS_BIG_FLAG | MS_EXTRA_FLAG);

  /* Completeness check */
  if (flags & ~MS_TRANSVERSE_FLAG)
    eprintf1("0x%04X", (unsigned int)(flags & ~MS_TRANSVERSE_FLAG));

  /* Standard qualifier */
  if (flags & MS_TRANSVERSE_FLAG) eprintf("." MS_TRANSVERSE_STRING);

  return;
}

/******************************************************************************

  Function: eprn_flag_mismatch

  This routine is called if the media size can be supported for some
  combination of flags but not for that combination which has been requested.
  The parameter 'no_match' indicates whether these flags are supported at all
  for any of the supported sizes or not.

  If the derived device has set a flag mismatch error reporting function, the
  call will be passed to that function. Otherwise a general error message is
  written through the graphics library's eprintf().

******************************************************************************/

static void eprn_flag_mismatch(const struct s_eprn_Device *eprn,
  bool no_match)
{
  if (eprn->fmr != NULL) (*eprn->fmr)(eprn, no_match);
  else {
    const char *epref = eprn->CUPS_messages? CUPS_ERRPREF: "";

    eprintf2("%s" ERRPREF "The %s does not support ",
      epref, eprn->cap->name);
    if (eprn->desired_flags == 0) eprintf("an empty set of media flags");
    else {
      eprintf("the \"");
      print_flags(eprn->desired_flags, eprn->flag_desc);
      eprintf("\" flag(s)");
    }
    eprintf1("\n%s  (ignoring presence or absence of \"", epref);
    {
      ms_MediaCode optional = MS_TRANSVERSE_FLAG;
      if (eprn->optional_flags != NULL) {
        const ms_MediaCode *of = eprn->optional_flags;
        while (*of != ms_none) optional |= *of++;
      }
      print_flags(optional, eprn->flag_desc);
    }
    eprintf("\") for ");
    if (no_match) eprintf("any"); else eprintf("this");
    eprintf(" page size.\n");
  }

  return;
}

/******************************************************************************

  Function: better_flag_match

  This function returns true iff the flags in 'new_code' match the requested
  flags (strictly) better than those in 'old_code'.

******************************************************************************/

static bool better_flag_match(ms_MediaCode desired,
  const ms_MediaCode *optional, ms_MediaCode old_code, ms_MediaCode new_code)
{
  ms_MediaCode
    old_diff,   /* difference between old flags and desired flags */
    new_diff;   /* difference between new flags and desired flags */

  /* Ignore the size information */
  old_code = ms_flags(old_code);
  new_code = ms_flags(new_code);

  /* Determine differences to desired flags */
  old_diff = old_code ^ desired;
  new_diff = new_code ^ desired;

  /* Check for exact matches */
  if (old_diff == 0) return false;
  if (new_diff == 0) return true;

  /* Is the difference at most MS_TRANSVERSE_FLAG? */
  old_diff = old_diff & ~MS_TRANSVERSE_FLAG;
  new_diff = new_diff & ~MS_TRANSVERSE_FLAG;
  if (old_diff == 0) return false;
  if (new_diff == 0) return true;

  /* Loop over the remaining optional flags */
  if (optional != NULL) {
    const ms_MediaCode *opt = optional;

    while (*opt != ms_none) {
      old_diff = old_diff & ~*opt;
      new_diff = new_diff & ~*opt;
      if (old_diff == 0) {
        if (new_diff != 0) return false;
        /* At this point both are matches at the same level of optional flags.
           Now look for the last preceding flag in which they differ. */
        {
          ms_MediaCode diff = ms_flags(old_code ^ new_code);
          while (optional < opt && (diff & *opt) == 0) opt--;
          if ((diff & *opt) == 0) {
            if ((diff & MS_TRANSVERSE_FLAG) == 0) return false;
            /* old and new differ in MS_TRANSVERSE_FLAG */
            return (new_code & MS_TRANSVERSE_FLAG) ==
              (desired & MS_TRANSVERSE_FLAG);
          }
          return (new_code & *opt) == (desired & *opt);
        }
      }
      if (new_diff == 0) return true;
      opt++;
    }
  }

  return false; /* Both codes are mismatches at this point */
}

/******************************************************************************

  Function: flag_match

  This function returns true iff 'code' is an acceptable match for the flag
  request.

******************************************************************************/

static bool flag_match(ms_MediaCode desired,
  const ms_MediaCode *optional, ms_MediaCode code)
{
  code = (ms_flags(code) ^ desired) & ~MS_TRANSVERSE_FLAG;
  if (code == 0) return true;

  if (optional == NULL) return false;

  while (*optional != ms_none && code != 0) {
    code = code & ~*optional;
    optional++;
  }

  return code == 0;
}

/******************************************************************************

  Function: eprn_set_page_layout

  This function determines media size, sheet orientation in pixmap device space,
  the orientation of default user space, and the imageable area. It should be
  called whenever the page device parameters "PageSize" and "LeadingEdge",
  the media flags, or the page descriptions have been changed.

  The function returns zero on success and a non-zero value otherwise.
  In the latter case, an error message has been issued. This can only
  occur if the media size is not supported with the flags requested.

  On success, the following variables in the device structure are consistent:
  width, height, MediaSize[], HWMargins[], eprn.code, eprn.default_orientation,
  eprn.right_shift, eprn.down_shift.

******************************************************************************/

int eprn_set_page_layout(eprn_Device *dev)
{
  bool
    no_match = true,
     /* Are the requested flags supported for some size? */
    landscape = dev->MediaSize[0] > dev->MediaSize[1];
     /* It's not documented, but 'MediaSize' is the requested "PageSize" page
        device parameter value and hence is to be interpreted in default (not
        default default!) user space. */
  const char *epref = dev->eprn.CUPS_messages? CUPS_ERRPREF: "";
  const eprn_CustomPageDescription
    *best_cmatch = NULL;        /* best custom page size match */
  eprn_Eprn
    *eprn = &dev->eprn;
  const eprn_PageDescription
    *best_cdmatch = NULL,     /* best custom page size match in discrete list*/
    *best_dmatch = NULL,        /* best discrete match */
    *pd;                        /* loop variable */
  float
    /* Page width and height in bp with w <= h (in a moment): */
    w = dev->MediaSize[0],
    h = dev->MediaSize[1],
    /* pixmap device space margins in bp (canonical order): */
    margins[4];
  int
    quarters;
  ms_MediaCode
    desired = eprn->desired_flags;

#ifdef EPRN_TRACE
  if_debug3(EPRN_TRACE_CHAR,
    "! eprn_set_page_layout(): PageSize = [%.0f %.0f], "
      "desired_flags = 0x%04X.\n",
    dev->MediaSize[0], dev->MediaSize[1], (unsigned int)desired);
#endif

  /* Ensure w <= h */
  if (w > h) {
    float temp;
    temp = w; w = h; h = temp;
    /* This has effectively split 'MediaSize[]' into 'w', 'h' and 'landscape'.
     */
  }

  /* Initialization of primary return value */
  eprn->code = ms_none;

  /* Put the LeadingEdge value into the desired flag pattern if it's set */
  if (eprn->leading_edge_set) {
    if (eprn->default_orientation % 2 == 0)     /* true on short edge first */
      desired &= ~MS_TRANSVERSE_FLAG;
    else
      desired |= MS_TRANSVERSE_FLAG;
  }

  /* Find best match in discrete sizes */
  if (eprn->media_overrides == NULL) pd = eprn->cap->sizes;
  else pd = eprn->media_overrides;
  while (pd->code != ms_none) {
    const ms_SizeDescription *ms = ms_find_size_from_code(pd->code);
    if (ms->dimen[0] > 0.0 /* ignore variable sizes */ &&
        fabs(w - ms->dimen[0])  <= 5.0 &&
        fabs(h - ms->dimen[1]) <= 5.0) {
       /* The size does match at 5 bp tolerance. This value has been chosen
          arbitrarily to be equal to PostScript's PageSize matching tolerance
          during media selection. The tolerance should really be that at which
          the printer in question distinguishes between sizes or smaller than
          that in order to at least prevent printing on unsupported sizes.
        */
      if (best_dmatch == NULL ||
          better_flag_match(desired, eprn->optional_flags, best_dmatch->code,
            pd->code))
        best_dmatch = pd;
      if (flag_match(desired, eprn->optional_flags, pd->code))
        no_match = false;
    }
    pd++;
  }

  /* Next find the best match among the custom size descriptions */
  if (eprn->cap->custom != NULL) {
    const eprn_CustomPageDescription *cp = eprn->cap->custom;

    /* First check whether the size is in the supported range */
    while (cp->width_max > 0.0) {
      if (cp->width_min  <= w && w <= cp->width_max &&
          cp->height_min <= h && h <= cp->height_max) {
        /* The size does match. */
        if (best_cmatch == NULL ||
            better_flag_match(desired, eprn->optional_flags, best_cmatch->code,
              cp->code))
          best_cmatch = cp;
        if (eprn->media_overrides == NULL &&
            flag_match(desired, eprn->optional_flags, cp->code))
          no_match = false;
      }
      cp++;
    }

    /* If we have read a media configuration file, the flags to be matched
       must be sought in 'media_overrides'. */
    if (best_cmatch != NULL && eprn->media_overrides != NULL) {
      for (pd = eprn->media_overrides; pd->code != ms_none; pd++) {
        if (ms_without_flags(pd->code) == ms_CustomPageSize) {
          if (best_cdmatch == NULL ||
              better_flag_match(desired, eprn->optional_flags,
                best_cdmatch->code, pd->code))
            best_cdmatch = pd;
          if (flag_match(desired, eprn->optional_flags, pd->code))
            no_match = false;
        }
      }
    }
  }

  /*  Now the 'best_*match' variables indicate for each of the categories of
      page descriptions to which extent the size is supported at all (non-NULL
      value) and what the best flag match in the category is. Here we now check
      for NULL values, i.e., size matches. */
  if (best_dmatch == NULL) {
    /* No discrete match */
    if (best_cmatch == NULL) {
      /* No match at all. */
      eprintf3("%s" ERRPREF
        "This document requests a page size of %.0f x %.0f bp.\n",
           epref, dev->MediaSize[0], dev->MediaSize[1]);
      if (eprn->cap->custom == NULL) {
        /* The printer does not support custom page sizes */
        if (eprn->media_overrides != NULL)
          eprintf1(
            "%s  The media configuration file does not contain an entry for "
              " this size.\n", epref);
        else
          eprintf2("%s  This size is not supported by the %s.\n",
            epref, eprn->cap->name);
      }
      else
        eprintf3(
          "%s  This size is not supported as a discrete size and it exceeds "
            "the\n"
          "%s  custom page size limits for the %s.\n",
          epref, epref, eprn->cap->name);
      return -1;
    }
    if (eprn->media_overrides != NULL && best_cdmatch == NULL) {
      eprintf6("%s" ERRPREF
        "This document requests a page size of %.0f x %.0f bp\n"
        "%s  but there is no entry for this size in the "
          "media configuration file\n"
        "%s  %s.\n",
        epref, dev->MediaSize[0], dev->MediaSize[1], epref, epref,
        eprn->media_file);
      return -1;
    }
  }
  /* Now we have: best_dmatch != NULL || best_cmatch != NULL &&
     (eprn->media_overrides == NULL || best_cdmatch != NULL). */

  /* Find a flag match among the size matches found so far */
  {
    ms_MediaCode custom_code = ms_none;
      /* best custom page size match (either from cmatch or dcmatch) */
    if (best_cmatch != NULL &&
        (eprn->media_overrides == NULL || best_cdmatch != NULL))
      custom_code = (eprn->media_overrides == NULL?
        best_cmatch->code: best_cdmatch->code);

    if (best_dmatch == NULL ||
        (best_cmatch != NULL &&
         better_flag_match(desired, eprn->optional_flags, best_dmatch->code,
                           custom_code))) {
      if (flag_match(desired, eprn->optional_flags, custom_code)) {
        if (eprn->media_overrides == NULL) {
          eprn->code = best_cmatch->code;
          margins[0] = best_cmatch->left;
          margins[1] = best_cmatch->bottom;
          margins[2] = best_cmatch->right;
          margins[3] = best_cmatch->top;
        }
        else {
          eprn->code = best_cdmatch->code;
          margins[0] = best_cdmatch->left;
          margins[1] = best_cdmatch->bottom;
          margins[2] = best_cdmatch->right;
          margins[3] = best_cdmatch->top;
        }
      }
    }
    else {
      if (flag_match(desired, eprn->optional_flags, best_dmatch->code)) {
        eprn->code = best_dmatch->code;
        margins[0] = best_dmatch->left;
        margins[1] = best_dmatch->bottom;
        margins[2] = best_dmatch->right;
        margins[3] = best_dmatch->top;
      }
    }
  }
  /* If we've found a match, 'code' is no longer 'ms_none'. */
  if (eprn->code == ms_none) {
    eprn_flag_mismatch(eprn, no_match);
    return -1;
  }

  /* Adapt the orientation of default default user space if not prescribed */
  if (!eprn->leading_edge_set) {
    if (eprn->code & MS_TRANSVERSE_FLAG) eprn->default_orientation = 3;
     /* This leads to 0 if landscape orientation is requested. */
    else eprn->default_orientation = 0;
  }

  /*
    Now 'eprn->default_orientation % 2' describes the sheet's orientation in
    pixmap device space. If this does not agree with the width and height
    values in the device instance, we'll have to adapt them.
    This is only necessary if there is a significant difference between width
    and height.
   */
  if (fabs(w - h) > 1 /* arbitrary */ &&
    (eprn->default_orientation % 2 == 0) !=
        (dev->width/dev->HWResolution[0] <= dev->height/dev->HWResolution[1])) {
    bool reallocate = false;

#ifdef EPRN_TRACE
    if_debug0(EPRN_TRACE_CHAR,
      "! eprn_set_page_layout(): width-height change is necessary.\n");
#endif

    /* Free old storage if the device is open */
    if (dev->is_open) {
#ifdef EPRN_TRACE
      if_debug0(EPRN_TRACE_CHAR, "! eprn_set_page_layout(): Device is open.\n");
#endif
      reallocate = true;
       /* One could try and call the allocation/reallocation routines of the
          prn device directly, but they are not available in older ghostscript
          versions and this method is safer anyway because it relies on a
          documented API. */
      gdev_prn_close((gx_device *)dev);         /* ignore the result */
    }

    /*  Now set width and height via gx_device_set_media_size(). This function
        sets 'MediaSize[]', 'width', and 'height' based on the assumption that
        default user space has a y axis which is vertical in pixmap device
        space. This may be wrong and we have to fix it. Because fixing
        'MediaSize[]' is simpler, gx_device_set_media_size() is called such
        that it gives the correct values for 'width' and 'height'. */
    if (eprn->default_orientation % 2 == 0) {
      /* portrait orientation of the sheet in pixmap device space */
      gx_device_set_media_size((gx_device *)dev, w, h);
      if (landscape) {
        dev->MediaSize[0] = h;
        dev->MediaSize[1] = w;
      }
    }
    else {
      /* landscape orientation in pixmap device space (transverse) */
      gx_device_set_media_size((gx_device *)dev, h, w);
      if (!landscape) {
        dev->MediaSize[0] = w;
        dev->MediaSize[1] = h;
      }
    }

    /* If the device is/was open, reallocate storage */
    if (reallocate) {
      int rc;

      rc = gdev_prn_open((gx_device *)dev);
      if (rc < 0) {
        eprintf2("%s" ERRPREF
          "Failure of gdev_prn_open(), code is %d.\n",
          epref, rc);
        return rc;
      }
    }
  }

  /* Increase the bottom margin for coloured modes except if it is exactly
     zero */
  if (eprn->colour_model != eprn_DeviceGray && margins[1] != 0.0)
    margins[1] += eprn->cap->bottom_increment;

  /* Number of +90-degree rotations needed for default user space: */
  quarters = eprn->default_orientation;
  if (landscape) quarters = (quarters + 1)%4;

  /* Store the top and left margins in the device structure for use by
     eprn_get_initial_matrix() and set the margins of the printable area if
     we may.
     gx_device_set_margins() (see gsdevice.c) copies the margins[] array to
     HWMargins[] which is presumably to be interpreted in default user space
     (see gs_initclip() in gspath.c), and if its second argument is true it
     also modifies the offset variable Margins[]. The first property means
     that gx_device_set_margins() can only be used if default user space and
     pixmap device space have the same "up" direction, and the second
     appropriates a parameter which is intended for the user.
  */
  if (eprn->keep_margins) {
    eprn->down_shift  = dev->HWMargins[3 - quarters];
    eprn->right_shift = dev->HWMargins[(4 - quarters)%4];
  }
  else {
    int j;

    eprn->down_shift  = margins[3];
    eprn->right_shift = margins[0];

    if (quarters != 0) {
       /* The "canonical margin order" for ghostscript is left, bottom, right,
          top. Hence for, e.g., a +90-degree rotation ('quarters' is 1) of
          default user space with respect to pixmap device space the left
          margin (index 0) in default user space is actually the bottom margin
          (index 1) in pixmap device space, the bottom margin is the right one,
          etc.
        */
      for (j = 0; j < 4; j++) dev->HWMargins[j] = margins[(j+quarters)%4];
      /* 'HWMargins[]' is in bp (see gxdevcli.h) */
    }
    else {
      /* Convert to inches */
      for (j = 0; j < 4; j++) margins[j] /= BP_PER_IN;

      gx_device_set_margins((gx_device *)dev, margins, false);
       /* Of course, I could set HWMargins[] directly also in this case. This
          way is however less prone to break on possible future incompatible
          changes to ghostscript and it covers the most frequent case (portrait
          and short edge first). */
    }
  }

  return 0;
}

/******************************************************************************

  Function: eprn_init_device

  This function sets 'cap' to 'desc' and all device parameters which are
  modified through the put_params routines to default values. The resolution is
  left at its old value (and don't ask me why or I'll start to whimper). If the
  device is open when this function is called the device will be closed
  afterwards.

  'desc' may not be NULL.

******************************************************************************/

int eprn_init_device(eprn_Device *dev, const eprn_PrinterDescription *desc)
{
  eprn_Eprn *eprn = &dev->eprn;
  int j;
  float hres, vres;
  int code;

  if (dev->is_open) gs_closedevice((gx_device *)dev);

  assert(desc != NULL);
  eprn->cap = desc;
  eprn_set_media_data(dev, NULL, 0);

  /* The media flags are retained because they have not been prescribed by the
     user directly in contact with eprn but are completely under the control
     of the derived device. */

  eprn->code = ms_none;
  eprn->leading_edge_set = false;
  eprn->right_shift = 0;
  eprn->down_shift = 0;
  eprn->keep_margins = false;
  eprn->soft_tumble = false;
  for (j = 0; j < 4; j++) dev->HWMargins[j] = 0;

  /* Set to default colour state, ignoring request failures */
  eprn->colour_model = eprn_DeviceGray;
  eprn->black_levels = 2;
  eprn->non_black_levels = 0;
  eprn->intensity_rendering = eprn_IR_halftones;
  hres = dev->HWResolution[0];
  vres = dev->HWResolution[1];
  code = eprn_check_colour_info(desc->colour_info, &eprn->colour_model,
      &hres, &vres, &eprn->black_levels, &eprn->non_black_levels);
  if (code) {
    return code;
  }
  if (eprn->pagecount_file != NULL) {
    gs_free(dev->memory->non_gc_memory, eprn->pagecount_file, strlen(eprn->pagecount_file) + 1,
      sizeof(char), "eprn_init_device");
    eprn->pagecount_file = NULL;
  }

  eprn->media_position_set = false;

  return 0;
}

/******************************************************************************

  Function: eprn_set_media_flags

******************************************************************************/

void eprn_set_media_flags(eprn_Device *dev, ms_MediaCode desired,
  const ms_MediaCode *optional)
{
  dev->eprn.code = ms_none;

  dev->eprn.desired_flags = desired;
  dev->eprn.optional_flags = optional;

  return;
}

/******************************************************************************

  Function: eprn_open_device

  This function "opens" the device. According to Drivers.htm, the 'open_device'
  functions are called before any output is sent to the device, and they must
  ensure that the device instance is valid, possibly by doing suitable
  initialization.

  This particular implementation also checks whether the requested page size
  is supported by the printer. This discovery must, unfortunately, be
  delayed until the moment this function is called. Note that this also implies
  that various eprn parameters depending on the page size (e.g., 'eprn.code')
  can be relied upon to have valid values only after the device has been
  successfully opened. The same applies to rendering parameters.

  This function also opens the parts defined by base classes.

  The function returns zero on success and a ghostscript error value otherwise.

******************************************************************************/

int eprn_open_device(gx_device *device)
{
  eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;
  const char *epref = eprn->CUPS_messages? CUPS_ERRPREF: "";
  int rc;

#ifdef EPRN_TRACE
  if_debug0(EPRN_TRACE_CHAR, "! eprn_open_device()...\n");
#endif

  /* Checks on page size and determination of derived values */
  if (eprn_set_page_layout((eprn_Device *)device) != 0)
    return_error(gs_error_rangecheck);

  /* Check the rendering parameters */
  if (eprn_check_colour_info(eprn->cap->colour_info, &eprn->colour_model,
      &device->HWResolution[0], &device->HWResolution[1],
      &eprn->black_levels, &eprn->non_black_levels) != 0) {
    gs_param_string str;

    eprintf1("%s" ERRPREF "The requested combination of colour model (",
      epref);
    str.size = 0;
    if (eprn_get_string(eprn->colour_model, eprn_colour_model_list, &str) != 0)
      assert(0); /* Bug. No harm on NDEBUG because I've just set the size. */
    errwrite(device->memory, (const char *)str.data, str.size * sizeof(str.data[0]));
    eprintf7("),\n"
      "%s  resolution (%gx%g ppi) and intensity levels (%d, %d) is\n"
      "%s  not supported by the %s.\n",
      epref, device->HWResolution[0], device->HWResolution[1],
      eprn->black_levels, eprn->non_black_levels, epref, eprn->cap->name);
    return_error(gs_error_rangecheck);
  }

  /* Initialization for colour rendering */
  if (device->color_info.num_components == 4) {
    /* Native colour space is 'DeviceCMYK' */
    set_dev_proc(device, map_rgb_color, NULL);

    if (eprn->intensity_rendering == eprn_IR_FloydSteinberg)
      set_dev_proc(device, map_cmyk_color, &eprn_map_cmyk_color_max);
    else if (device->color_info.max_gray > 1 || device->color_info.max_color > 1)
      set_dev_proc(device, map_cmyk_color, &eprn_map_cmyk_color_flex);
    else
      set_dev_proc(device, map_cmyk_color, &eprn_map_cmyk_color);

    if (eprn->intensity_rendering == eprn_IR_FloydSteinberg)
      set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K_max);
    else if (device->color_info.max_gray > 1 || device->color_info.max_color > 1)
      set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K_flex);
    else
      set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K);

  }
  else {
    set_dev_proc(device, map_cmyk_color, NULL);

    if (eprn->colour_model == eprn_DeviceRGB) {
      if (eprn->intensity_rendering == eprn_IR_FloydSteinberg)
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_RGB_max);
      else if (device->color_info.max_color > 1)
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_RGB_flex);
      else
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_RGB);
    } else {
      if (eprn->intensity_rendering == eprn_IR_FloydSteinberg)
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K_max);
      else if (device->color_info.max_gray > 1 || device->color_info.max_color > 1)
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K_flex);
      else
        set_dev_proc(device, map_rgb_color, &eprn_map_rgb_color_for_CMY_or_K);
    }
  }
  eprn->output_planes = eprn_bits_for_levels(eprn->black_levels) +
    3 * eprn_bits_for_levels(eprn->non_black_levels);

#if !defined(GS_REVISION) || GS_REVISION >= 600
  /*  According to my understanding, the following call should be superfluous
      (because the colour mapping functions may not be called while the device
      is closed) and I am also not aware of any situation where it does make a
      difference. It shouldn't do any harm, though, and I feel safer with it :-)
  */
  gx_device_decache_colors(device);
#endif

#ifndef EPRN_NO_PAGECOUNTFILE
  /* Read the page count value */
  if (eprn->pagecount_file != NULL) {
    unsigned long count;
    if (pcf_getcount(device->memory, eprn->pagecount_file, &count) == 0)
      device->PageCount = count;
       /* unsigned to signed. The C standard permits
          an implementation to generate an overflow indication if the value is
          too large. I consider this to mean that the type of 'PageCount' is
          inappropriate :-). Note that eprn does not use 'PageCount' for
          updating the file. */
    else {
      /* pcf_getcount() has issued an error message. */
      eprintf(
        "  No further attempts will be made to access the page count file.\n");
      gs_free(device->memory->non_gc_memory, eprn->pagecount_file, strlen(eprn->pagecount_file) + 1,
        sizeof(char), "eprn_open_device");
      eprn->pagecount_file = NULL;
    }
  }
#endif  /* !EPRN_NO_PAGECOUNTFILE */

  /* Open the "prn" device part */
  if ((rc = gdev_prn_open(device)) != 0) return rc;

  /* if device has been subclassed (FirstPage/LastPage device) then make sure we use
   * the subclassed device.
   */
  while (device->child)
      device = device->child;
  eprn = &((eprn_Device *)device)->eprn;

  /* Just in case a previous open call failed in a derived device (note that
     'octets_per_line' is still the same as then): */
  if (eprn->scan_line.str != NULL)
    gs_free(device->memory->non_gc_memory, eprn->scan_line.str, eprn->octets_per_line, sizeof(eprn_Octet),
      "eprn_open_device");
  if (eprn->next_scan_line.str != NULL) {
    gs_free(device->memory->non_gc_memory, eprn->next_scan_line.str, eprn->octets_per_line, sizeof(eprn_Octet),
      "eprn_open_device");
    eprn->next_scan_line.str = NULL;
  }

  /* Calls which might depend on prn having been initialized */
  eprn->octets_per_line = gdev_prn_raster((gx_device_printer *)device);
  eprn->scan_line.str = (eprn_Octet *) gs_malloc(device->memory->non_gc_memory, eprn->octets_per_line,
    sizeof(eprn_Octet), "eprn_open_device");
  if (eprn->intensity_rendering == eprn_IR_FloydSteinberg) {
    eprn->next_scan_line.str = (eprn_Octet *) gs_malloc(device->memory->non_gc_memory, eprn->octets_per_line,
      sizeof(eprn_Octet), "eprn_open_device");
    if (eprn->next_scan_line.str == NULL && eprn->scan_line.str != NULL) {
      gs_free(device->memory->non_gc_memory, eprn->scan_line.str, eprn->octets_per_line, sizeof(eprn_Octet),
        "eprn_open_device");
      eprn->scan_line.str = NULL;
    }
  }
  if (eprn->scan_line.str == NULL) {
    eprintf1("%s" ERRPREF
      "Memory allocation failure from gs_malloc() in eprn_open_device().\n",
      epref);
    return_error(gs_error_VMerror);
  }

  return rc;
}

/******************************************************************************

  Function: eprn_close_device

******************************************************************************/

int eprn_close_device(gx_device *device)
{
  eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;

#ifdef EPRN_TRACE
  if_debug0(EPRN_TRACE_CHAR, "! eprn_close_device()...\n");
#endif

  if (eprn->scan_line.str != NULL) {
    gs_free(device->memory->non_gc_memory, eprn->scan_line.str, eprn->octets_per_line, sizeof(eprn_Octet),
      "eprn_close_device");
    eprn->scan_line.str = NULL;
  }
  if (eprn->next_scan_line.str != NULL) {
    gs_free(device->memory->non_gc_memory, eprn->next_scan_line.str, eprn->octets_per_line, sizeof(eprn_Octet),
      "eprn_close_device");
    eprn->next_scan_line.str = NULL;
  }

  return gdev_prn_close(device);
}

/******************************************************************************

  Function: eprn_forget_defaultmatrix

  This function tells the ghostscript kernel to forget the default matrix,
  i.e., to consult the get_initial_matrix device procedure the next time the
  default CTM is needed.

******************************************************************************/

static void eprn_forget_defaultmatrix(gx_device *device)
{
  eprn_Eprn *eprn = &((eprn_Device *)device)->eprn;

  gs_setdefaultmatrix((gs_gstate *)eprn->pgs, NULL);

  return;
}

/******************************************************************************

  Function: eprn_output_page

  This function is a wrapper for gdev_prn_output_page() in order to catch the
  number of pages printed and to initialize the eprn_get_planes() API.

******************************************************************************/

int eprn_output_page(gx_device *dev, int num_copies, int flush)
{
  eprn_Eprn *eprn = &((eprn_Device *)dev)->eprn;
  int rc;

#ifdef EPRN_TRACE
  clock_t start_time = clock();
  if_debug0(EPRN_TRACE_CHAR, "! eprn_output_page()...\n");
#endif

  /* Initialize eprn_get_planes() data */
  eprn->next_y = 0;
  if (eprn->intensity_rendering == eprn_IR_FloydSteinberg) {
    /* Fetch the first line and store it in 'next_scan_line'. */
    if (eprn_fetch_scan_line((eprn_Device *)dev, &eprn->next_scan_line) == 0)
      eprn->next_y++;
  }

  /* Ship out */
  rc = gdev_prn_output_page(dev, num_copies, flush);

  /*  CUPS page accounting message. The CUPS documentation is not perfectly
      clear on whether one should generate this message before printing a page
      or after printing has been successful. The rasterto* filters generate it
      before sending the page, but as the scheduler uses these messages for
      accounting, this seems unfair.
  */
  if (rc == 0 && eprn->CUPS_accounting)
    eprintf2("PAGE: %ld %d\n", dev->ShowpageCount, num_copies);
    /* The arguments are the number of the page, starting at 1, and the number
       of copies of that page. */

#ifndef EPRN_NO_PAGECOUNTFILE
  /* On success, record the number of pages printed */
  if (rc == 0 && eprn->pagecount_file != NULL) {
    assert(num_copies > 0);     /* because of signed/unsigned */
    if (pcf_inccount(dev->memory, eprn->pagecount_file, num_copies) != 0) {
      /* pcf_inccount() has issued an error message. */
      eprintf(
        "  No further attempts will be made to access the page count file.\n");
      gs_free(dev->memory->non_gc_memory, eprn->pagecount_file, strlen(eprn->pagecount_file) + 1,
        sizeof(char), "eprn_output_page");
      eprn->pagecount_file = NULL;
    }
  }
#endif  /* !EPRN_NO_PAGECOUNTFILE */

  /* If soft tumble has been demanded, ensure the get_initial_matrix procedure
     is consulted for the next page */
  if (eprn->soft_tumble) eprn_forget_defaultmatrix(dev);

#ifdef EPRN_TRACE
  if_debug1(EPRN_TRACE_CHAR, "! eprn_output_page() terminates after %f s.\n",
    ((float)(clock() - start_time))/CLOCKS_PER_SEC);
#endif

  return rc;
}
