/******************************************************************************
  File:     $Id: gdeveprn.h,v 1.23 2001/04/30 05:15:51 Martin Rel $
  Contents: Header file for the abstract ghostscript device 'eprn'
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany; e-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 2000, 2001 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  The 'eprn' device
  *****************
  In developing my hpdj/pcl3 driver for PCL 3+ I had to implement a lot of
  functionality which was logically independent of PCL. This indicated that the
  functionality offered by ghostscript's 'prn' device was insufficient and
  should be extended, leading to this "extended prn device".

  Functionality
  =============
  The "eprn" device offers the following services to derived devices:
  - A simple but still flexible rendering model. A derived device defines
    printer capability descriptions listing the supported rendering parameters,
    lets the eprn device handle the user's rendering requests, and then just
    fetches the pixels.
  - A PostScript-conforming initialization of default user space with respect
    to landscape orientation and support for the standard page device parameter
    "LeadingEdge".
  - Recognition of media sizes from the document and checking of supported
    sizes based on printer descriptions
  - Flexible handling of size-specific hardware margins
  - Counting of pages printed across gs invocations by means of page count
    files

  Rendering Model
  ===============
  The "eprn" device parameterizes the printer's rendering capabilities as
  follows:
  - process colour model (Gray, RGB, CMY, CMY+K, CMYK)
  - resolution in horizontal and vertical directions
  - number of intensity levels per colorant, chosen independently for black
    and non-black (RGB or CMY) colorants.
  In addition, the rendering process as implemented in this driver is
  parameterized by:
  - method for rendering intensities (printer, halftones or error diffusion)
  These parameters are specified by the user. The device checks whether the
  printer supports the requested combination and, if it does, sets
  ghostscript's internal data structures accordingly.

  The derived device can obtain the resulting pixels by successively calling
  eprn_get_planes() from the page printing procedure.

  Device Coordinates
  ==================
  "Pixmap device space" is the coordinate system used by the "prn" device for
  generating a pixmap and implicit in the API for accessing it (e.g.,
  gdev_prn_copy_scan_lines()). In identifying the directions "left", "bottom",
  "right" and "top" I'm assuming that the x axis is pointing to the right and
  the y axis downwards. This is not concerned with the orientation of device
  space on a sheet printed but is merely a definition of what, e.g., "down"
  means for the eprn device: it is defined as "towards increasing prn y values".
  This makes the code more readable for the case where the real device
  coordinate system (on the sheet) has just this orientation. Too bad for the
  others :-).

  Terms like "width" and "height" are also defined with respect to this
  interpretation, i.e., "width" is an extension in x direction.

  Note that names and interpretations of some of ghostscript's standard
  device structure fields (like 'width', 'height') are also based on this
  convention although this is not explicitly stated. This does however not
  apply to all parameters: for example, HWMargins[] and MediaSize[] are to be
  interpreted in default user space (see gx_default_clip_box()).

  One key assumption of the eprn device is that the top edge of pixmap device
  space is the edge closest to and parallel with the medium's leading edge.
  It is the responsibility of the derived device to ensure this. Usually
  it means that raster lines have to be printed in the order of ascending y.
  (The eprn device will work if the derived device violates this condition, but
  the meaning of user-visible properties of the device would change.)

  The device sets up the device coordinate system such that the device space
  origin is the top left corner of the imageable area where "top" and "left"
  refer to you holding the sheet with the printed side towards you and the
  leading edge at the top. Units and directions are identical with pixmap
  device space.

  Page Descriptions
  =================
  When a PostScript document requests a particular page size by specifying its
  width and height in default user space the eprn device combines this
  information with certain conditions imposed by the derived device and looks
  for a matching page description entry in the printer's capability
  description. The following information from such an entry is needed for
  correctly setting up the relation between default user space and device space:

    - default orientation of the sheet in pixmap device space
      (MS_TRANSVERSE_FLAG; set iff the sheet has width > height in pixmap
      device space)
    - hardware margins

  This part is interpreted by the eprn device.

  In addition, a page description entry may contain a number of flags
  indicating special conditions under which this entry is to be selected or
  carrying information on how to configure the printer. The flags are those
  defined by mediasize.h (except that MS_TRANSVERSE_FLAG and MS_ROTATED_FLAG
  may not be used for this purpose), their interpretation is up to the derived
  device which must request them or to which they will become visible when the
  entry is selected.

  The eprn device takes the following items as the request to be matched with a
  page description entry in the printer's capability description:

    - the media size of the document and, if set, the "LeadingEdge" page device
      parameter
    - a pattern of desired (not necessarily *required*) flags
    - an ordered list of optional flags

  Media size and LeadingEdge are set via PostScript or the command line, the
  flags can be set by the derived device through its device structure instance
  or by calling eprn_set_media_flags(). The optional flags when combined with
  the bitwise OR effectively define a mask of bits which may be ignored when
  checking whether an entry matches the desired flags. The order of optional
  flags is from most to least desirable to ignore.

  The complete request is compared to the list of page descriptions as follows:

   1. If the media size, irrespective of flags, cannot be matched at 5 bp
      tolerance, the request fails with an error message to standard error.
   2. Otherwise a shortened list containing all entries having a matching
      size is compiled, at least conceptually. If the device supports custom
      page sizes, they are listed at the end. This is the "base list".
   3. If LeadingEdge is not null, the MS_TRANSVERSE_FLAG is set or cleared
      accordingly in the pattern of desired flags. This flag is also always
      added to the front of the list of optional flags.
   4. A mask of flags to be ignored is set to zero.
   5. The base list is searched for an entry agreeing with the desired flags
      except possibly for those in the ignore mask.
   6. If no such entry is found, the next flag in the list of optional flags is
      added to the ignore mask and execution continues with step 5. If there is
      no such flag, the request fails and the flag mismatch reporting function
      is called (see below).
   7. If a matching entry is found the list of optional flags is processed
      backwards starting with the flag before the last one added to the ignore
      mask. For each such flag an attempt is made to remove it from the mask.
      If there still is at least one matching entry in the base list the flag
      remains cleared, otherwise it is put back into the mask.
   8. Finally, the first entry matching the request mask and the current
      ignore mask will be selected:
      - its media code will be made available to the derived device in the
        eprn device's 'code' field,
      - the hardware margins will be set from the page description (unless the
        user has explicitly specified a value for the ".HWMargins" page device
        parameter),
      - default user space will be configured based on sheet orientation
        (transverse or not in device space) and page orientation (portrait or
        landscape in default user space).

  Because the error message in step 6 has to report a mismatch on the flags
  and because the interpretation of the flags is fixed by the derived device,
  an error message issued by the eprn device may not be particularly
  illuminating to the user (the user will usually think in terms of the
  interpretation the derived device associates with these flags). Therefore
  issuing this error message can be delegated to a function ("flag mismatch
  reporting function") specified by the derived device in its device structure
  instance.

******************************************************************************/

#ifndef _gdeveprn_h	/* Inclusion protection */
#define _gdeveprn_h

/*****************************************************************************/

/* Special Aladdin header, must be included before <sys/types.h> on some
   platforms (e.g., FreeBSD). This necessity seems to be triggered for example
   by <stdio.h>. */
#include "std.h"

/* Standard headers */
#include <stdio.h>	/* for 'FILE' and 'size_t' */

/* Ghostscript headers */
#include "gdevprn.h"
/* This header is assumed to include a definition for 'bool' (stdpre.h). */

/* Other header files */
#include "mediasize.h"

/*****************************************************************************/

/*  Types to describe supported page sizes and associated setup information.
    All sizes are in bp and all orientations refer to pixmap device space.
 */

/* Discrete page sizes */
typedef struct {
  ms_MediaCode code;
   /* The media flags in 'code' identify the conditions under which the size
      is supported ("required flags") and carry information needed in order
      to adapt ghostscript or the printer to using this entry ("optional
      flags"). The distinction between the two groups of flags is fixed by the
      derived device when calling eprn_set_media_flags() except that special
      rules apply to MS_TRANSVERSE_FLAG which is handled by eprn.
      Setting MS_TRANSVERSE_FLAG indicates that media of this size are normally
      fed such that width > height in pixmap device space, otherwise
      width <= height is assumed.
      Setting MS_ROTATED_FLAG is not permitted.
      The interpretation of all other flags is up to the derived device.
    */
  float left, bottom, right, top;
   /* Hardware margins (non-negative) in bp */
} eprn_PageDescription;

/* Custom page sizes */
typedef struct {
  /*  The first two lines have the same meaning as for 'eprn_PageDescription'.
      However, apart from flags, the 'code' field must be 'ms_CustomPageSize'.
  */
  ms_MediaCode code;
  float left, bottom, right, top;
  float width_min, width_max, height_min, height_max;
   /* Permissible range in bp for width and height when a sheet's dimension is
      given such that width <= height. */
} eprn_CustomPageDescription;

/*
  Here follows an example of using "eprn_PageDescription". It describes a
  printer supporting A4 and US Letter, the first with 5 mm margins, the latter
  with 0.2 inches:

    const eprn_PageDescription page_description[] = {
        {ms_A4,       5*BP_PER_MM,   5*BP_PER_MM,   5*BP_PER_MM,   5*BP_PER_MM},
        {ms_Letter, 0.2*BP_PER_IN, 0.2*BP_PER_IN, 0.2*BP_PER_IN, 0.2*BP_PER_IN},
        {ms_none, 0, 0, 0, 0}
      };

  The last entry is a sentinel value which will be required later.

******************************************************************************/

/*  Types to describe supported combinations of resolutions, process colour
    models and intensity levels per colorant

    These types often contain pointers to lists terminated with a sentinel
    value. As these are usually static data, this makes it easier to share data
    between printer descriptions.
 */

/* Process colour models */
typedef enum {
  eprn_DeviceGray,
  eprn_DeviceRGB,
  eprn_DeviceCMY,
  eprn_DeviceCMY_plus_K,
   /* 'eprn_DeviceCMY_plus_K' is a process colour model using the CMYK
      colorants but never mixing black with one of the others. The underlying
      native colour space is 'DeviceRGB'. */
  eprn_DeviceCMYK
} eprn_ColourModel;

/* Horizontal and vertical resolutions w.r.t. pixmap device space */
typedef struct {
  float h, v;
} eprn_Resolution;

/* Range for the number of intensity levels */
typedef struct {
  unsigned short from, to;	/* both inclusive */
} eprn_IntensityLevels;

/* Combined resolutions and intensities. Any combination of the listed
   resolutions and the listed intensity levels is permitted. */
typedef struct {
  const eprn_Resolution *resolutions;
   /* either NULL (all resolutions are accepted) or a list terminated with
      a {0.0, 0.0} entry */
  const eprn_IntensityLevels *levels;
   /* non-NULL; this list is terminated with {0, 0}. The variable is a list in
      order to permit easier specification of discrete levels. */
} eprn_ResLev;

/* A complete instance of a rendering capability entry */
typedef struct {
  eprn_ColourModel colour_model;
   /* A value of 'eprn_DeviceCMYK' implies the capability for
      'eprn_DeviceCMY_plus_K' because the difference is realized in this driver
      and is not part of the printer's capabilities.
      In contrast, 'eprn_DeviceCMY_plus_K' or 'eprn_DeviceCMYK' do *not* imply
      the capability for CMY or Gray! This is because we assume that
      'colour_model' corresponds to particular hardware settings for the
      printer.
    */
  const eprn_ResLev *info[2];
   /* Supported resolutions and intensities indexed by colorant. The first
      entry refers to black except for eprn_DeviceRGB and eprn_DeviceCMY when
      it describes the three non-black colorants. The second entry is ignored
      unless both black and non-black colorants are used in which case it can
      either refer to the latter or be NULL to indicate that the black entry
      applies also to the non-black colorants.
      Any non-null info[] points to a list terminated with a {NULL, NULL} entry.
      Only values for those colorants described by different (non-NULL) 'info'
      entries in an instance of this type can be chosen independently. Hence at
      most black can have values different from those of the other colorants.
    */
} eprn_ColourInfo;

/*
  As an example application of these types, consider a bilevel device
  supporting 600 ppi monochrome printing:

    const eprn_Resolution sixhundred[] = { {600, 600}, {0, 0}};
    const eprn_IntensityLevels bilevel[] = { {2, 2}, {0, 0}};
    const eprn_ResLev rl_600_2[] = { {sixhundred, bilevel}, {NULL, NULL}};
    const eprn_ColourInfo mono_600_2[] = {
        {eprn_DeviceGray, {rl_600_2, NULL}},
        {eprn_DeviceGray, {NULL, NULL}}
      };

******************************************************************************/

/* Printer capability description */
typedef struct {
  const char *name;
   /* This string is used in error messages generated by the device. It should
      describe the printer, not a gs device. In selecting an appropriate value
      imagine a statement like "... is not supported by the <name>" or "The
      <name> does not support ...".
    */

  /* Media sizes and hardware margins */
  const eprn_PageDescription *sizes;
  /*  List of supported discrete media sizes and associated hardware margins,
      terminated by an entry with 'code' == ms_none. This must always be
      non-NULL. */
  const eprn_CustomPageDescription *custom;
  /*  This may be NULL, in which case the printer does not support custom
      page sizes. Otherwise it is a list of entries, terminated with an entry
      having 0.0 for 'width_max'. */
  float bottom_increment;
  /*  Change of bottom hardware margin when not printing in DeviceGray,
      specified in bp. This correction is ignored if the bottom margin is
      exactly zero.
      Some HP DeskJets have a larger hardware margin at the bottom for the
      colour cartridge but this is not the case for banner printing. */

  /* Colour capabilities */
  const eprn_ColourInfo *colour_info;
  /*  List of supported colour models, resolutions and intensity levels.
      This must always be non-NULL and terminated with an entry having NULL as
      'info[0]'. There must be at least one 'eprn_ColourInfo' entry permitting
      identical resolution for all colorants.
   */
} eprn_PrinterDescription;

/*
  To continue our series of examples, here is a complete printer description:

    const eprn_PrinterDescription description = {
        "Wrzlbrnf printer", page_description, NULL, 0, mono_600_2
      };

******************************************************************************/

/* Intensity rendering methods */
typedef enum {
  eprn_IR_printer,
  eprn_IR_halftones,
  eprn_IR_FloydSteinberg
} eprn_IR;

/*****************************************************************************/

/* Unsigned 8-bit values (should really be 'uint8_t') */
typedef unsigned char eprn_Octet;

/* Octet strings */
typedef struct {
  eprn_Octet *str;
  int length;	/* zero or the length in 'eprn_Octet' instances of the area
                   filled starting at '*str' */
} eprn_OctetString;

/* Type for flag mismatch reporting function */
struct s_eprn_Device;
 /* The preceding statement is needed in order to establish a forward
    declaration for "struct s_eprn_Device" at file scope. */
typedef void (*eprn_FlagMismatchReporter)(const struct s_eprn_Device *dev,
                bool no_match);
/*  A function of this kind will be called if the requested media flags cannot
    be satisfied by the printer although the size itself is supported for some
    (unspecified) set of flags. The parameter 'no_match' indicates whether the
    printer supports the set of required flags for some size or not at all
    (this is intended to be useful in the case of flags denoting printer
    capabilities). The requested set of media selection flags is available in
    'dev->eprn.desired_flags' and 'dev->eprn.optional_flags' (MS_ROTATED_FLAG
    will never be set).
    The function must write an error message.
*/

/*****************************************************************************/

/*  The eprn device variables. These are deliberately defined as a structure
    and not as usual in ghostscript as a macro body to be inserted into a
    structure because I want to have namespace isolation for these fields.
    A derived device may read any of these parameters but should never change
    them directly. The only exception is 'soft_tumble'.
 */
typedef struct s_eprn_Device {
  /* Capabilities */
  const eprn_PrinterDescription *cap;
   /* non-NULL. The data pointed to may not change unless this is immediately
      followed by a call to eprn_init_device(). */
  char *media_file;
   /* gs_malloc()-allocated media configuration file name.
      Non-NULL if 'media_overrides' is non-NULL. */
  eprn_PageDescription *media_overrides;
   /* A list of supported media dimensions and corresponding hardware margins.
      This is usually NULL, meaning that 'cap->sizes' and 'cap->custom' should
      be used.
      If non-NULL, 'media_overrides' must be terminated with an entry having
      'code' == ms_none. If this field is non-NULL, only the sizes listed here
      will be supported. The device will support custom page sizes only if
      'cap->custom' is not NULL and 'media_overrides' contains an entry for
      'ms_CustomPageSize'.
      The pointer refers to gs_malloc()-allocated storage.
    */

  /* Page setup */
  const ms_Flag *flag_desc;
   /* Description of the additional flags the derived device understands. This
      can be NULL or a list terminated with an entry having 'code' equal to
      'ms_none'. */
  ms_MediaCode
    desired_flags;
  const ms_MediaCode
    *optional_flags;		/* may be NULL; terminated with 'ms_none'. */
  eprn_FlagMismatchReporter fmr;	/* may be NULL */
  ms_MediaCode code;
   /* Page size and other media attributes determined based on printer
      capabilities. A value of 'ms_none' indicates that the page description
      entry to be used has not (yet) been successfully identified and that
      therefore some of the other fields in this structure are not valid. */

  /* Coordinates */
  int default_orientation;
   /* Direction of the positive y axis of default default user space (the
      requested PageSize had width <= height) as seen from pixmap device space:
          0	up
          1	left
          2	down
          3	right
      Hence the value is the number of +90 degrees rotations necessary to
      obtain the "up" direction of default default user space (positive y axis)
      from the "up" direction of pixmap device space (negative y axis).
      The values 0 and 2 imply that the medium has width <= height in pixmap
      device space, 1 and 3 imply height <= width. */
  bool leading_edge_set;
   /* True iff someone set the 'LeadingEdge' page device parameter. In this
      case its value is stored in 'default_orientation'. The values permitted
      for 'default_orientation' and their interpretation are chosen such that
      this gives the desired result.
      If this value is false, the device will select a value for
      'default_orientation' based on the MS_TRANSVERSE_FLAG in 'code'. */
  float
    right_shift,
    down_shift;
     /* Seen from pixmap device space, the top left corner of the sheet is at
        (-right_shift, -down_shift). Both values are in bp and represent
        hardware margins, i.e., the origin of pixmap device space is a corner
        of the sheet's imageable area.
        These parameters are logically superfluous and could be derived from
        'HWMargins[]' and possibly other data (like 'default_orientation'),
        provided one knows what the reference system for 'HWMargins[]' is.
        This is not documented but it seems to be default user space. In order
        to have values with a reliable interpretation and because the data are
        originally given in pixmap device space anyway I have introduced these
        variables. */
  bool keep_margins;
     /* True iff margin information should be taken from 'HWMargins[]' instead
        of from the printer description. */
  bool soft_tumble;
     /* If this field is set, every second page will have its default user
        coordinate system rotated by 180 degrees. */

  dev_proc_fillpage((*orig_fillpage));

  /* Colour */
  eprn_ColourModel
    colour_model;	/* Colour model selected */
  unsigned int
    black_levels,	/* Number of intensity levels for black colorant */
    non_black_levels;	/* Number of intensity levels for non-black colorants */
  eprn_IR
    intensity_rendering;  /* method to use */

  /* Page counting and other spooler support */
  char
    *pagecount_file;	/* gs_malloc()-allocated name of page count file.
                           May be NULL. */
  bool
    CUPS_accounting,	/* Send CUPS page accounting messages. */
    CUPS_messages;	/* Add CUPS message prefixes to error messages and
                           warnings. */

  /* Support for the standard page device parameter "MediaPosition" */
  bool media_position_set;
  int media_position;

  /*  Internal parameters. These have reliable values only after a successful
      call to eprn_open(). */
  unsigned int
    bits_per_colorant;	/* Number of bits per colorant used in 'gx_color_index'
                           values. Constant while the device is open. */
  eprn_OctetString
    scan_line,		/* 'str' is gs_malloc()-allocated */
    next_scan_line;	/* 'str' is gs_malloc()-allocated. Non-null only for
                           Floyd-Steinberg error diffusion. */
  unsigned int
    octets_per_line,	/* Constant while the device is open. */
    output_planes;	/* Constant while the device is open. */
  int next_y;		/* During a call to eprn_output_page(), the device y
    coordinate for the next scan line to fetch from the "prn" device. For
    eprn_IR_FloydSteinberg, the next scan line to return is actually already
    present in 'next_scan_line' with its device coordinate being "next_y - 1",
    unless 'next_y' is zero in which case we have finished. */
  gs_gstate * pgs;
} eprn_Eprn;

/* Macro for device structure type definitions. Note that in contrast to
   gx_prn_device_common this macro does include the inherited attributes. */
#define gx_eprn_device_common						\
  gx_device_common;		/* Attributes common to all devices */	\
  gx_prn_device_common;		/* Attributes for all printers */	\
  eprn_Eprn eprn

/* Base type needed for casts */
typedef struct {
  gx_eprn_device_common;
} eprn_Device;

#define private_st_device_EPRN(eprn_enum, eprn_reloc) \
   gs_private_st_composite(st_eprn_Device, eprn_Device, \
    "eprn_Device", eprn_enum, eprn_reloc);

/*  Macro for initializing device structure instances (device prototypes)

    This macro corresponds to the prn_device*() macros which are used when
    basing a device directly on the prn device.

    The parameters are:
      dtype:	identifier of the device structure type
      procs:	pointer to static device procedure table
                (const gx_device_procs *, not NULL)
      dname:	name of the device (const char *, not NULL)
      xdpi:	horizontal resolution in ppi (float, positive)
      ydpi:	vertical resolution in ppi (float, positive)
      print_page:  page printing procedure to be called by
                gdev_prn_output_page(), not NULL
      cap:	pointer to printer capability description
                (const eprn_PrinterDescription *, not NULL, the description may
                not change until immediately before the next call to
                eprn_init_device())
      flag_desc  description of the non-standard flags the derived device
                accepts (const ms_Flag *, may be NULL, otherwise terminated
                with an entry having 'ms_none' for 'code')
      desired	pattern of desired media flags (ms_MediaCode)
      optional	NULL or a list of optional media flags (const ms_MediaCode *)
                terminated with an 'ms_none' value
      fmr	NULL or pointer to a flag mismatch reporting function
                (eprn_FlagMismatchReporter)

    All storage referred to by pointers remains under the control of the
    derived device and should not be modified during the lifetime of the
    device unless explicitly permitted.

    This macro assumes that it is dealing with a printer supporting at least
    DeviceGray with 2 intensity levels and the page size described by
    (DEFAULT_WIDTH_10THS, DEFAULT_HEIGHT_10THS), usually US Letter or ISO A4.
    This is also the initial state. If a derived device wishes to use another
    initial state, it should define its own macro.
 */
#define eprn_device_initdata(dtype, procs, dname, xdpi, ydpi, print_page, cap, flag_desc, desired, optional, fmr) \
  prn_device_std_body(			\
    dtype,				\
    procs,				\
    dname,				\
    DEFAULT_WIDTH_10THS,		\
    DEFAULT_HEIGHT_10THS,		\
    xdpi, ydpi,				\
    0.0, 0.0, 0.0, 0.0, /* margins */	\
    1,		/* color_info.depth */	\
    print_page),			\
  {					\
    cap,	/* cap */		\
    NULL,	/* media_file */	\
    NULL,	/* media_overrides */	\
    flag_desc,	/* flag_desc */	\
    desired,	/* desired_flags */	\
    optional,	/* optional_flags */	\
    fmr,	/* fmr */		\
    ms_none,	/* code */		\
    0,		/* default_orientation */\
    false,	/* leading_edge_set */	\
    0.0,	/* right_shift */	\
    0.0,	/* down_shift */	\
    false,	/* keep_margins */	\
    false,	/* soft_tumble */	\
    NULL,       /* orig_fillpage */     \
    eprn_DeviceGray, /* colour_model */	\
    2,		/* black_levels */	\
    0,		/* non_black_levels */	\
    eprn_IR_halftones, /* intensity_rendering */ \
    NULL,	/* pagecount_file */	\
    false,	/* CUPS_accounting */	\
    false,	/* CUPS_messages */	\
    false,	/* media_position_set */\
    0,		/* media_position */	\
    0,		/* bits_per_colorant */	\
    {NULL, 0},	/* scan_line */		\
    {NULL, 0},	/* next_scan_line */	\
    0,		/* octets_per_line */	\
    0,		/* output_planes */	\
    0,		/* next_y */		\
    NULL        /* pgs    */            \
  }

/*  For the calling conventions of the following functions consult the comments
    preceding each function's implementation. */

/* Initialize the eprn device for another printer model */
extern int eprn_init_device(eprn_Device *dev,
  const eprn_PrinterDescription *desc);

/* Modify the information on supported media sizes and associated hardware
   margins by reading new values from a file */
extern int eprn_set_media_data(eprn_Device *dev, const char *media_file,
  size_t length);

/* Set media flags */
extern void eprn_set_media_flags(eprn_Device *dev, ms_MediaCode desired,
  const ms_MediaCode *optional);

/*****************************************************************************/

/* Device procedures */
extern dev_proc_open_device(eprn_open_device);
extern dev_proc_get_initial_matrix(eprn_get_initial_matrix);
extern dev_proc_output_page(eprn_output_page);
extern dev_proc_close_device(eprn_close_device);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_RGB);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_RGB_flex);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_RGB_max);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_CMY_or_K);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_CMY_or_K_flex);
extern dev_proc_map_rgb_color(eprn_map_rgb_color_for_CMY_or_K_max);
extern dev_proc_map_color_rgb(eprn_map_color_rgb);
extern dev_proc_get_params(eprn_get_params);
extern dev_proc_put_params(eprn_put_params);
extern dev_proc_map_cmyk_color(eprn_map_cmyk_color);
extern dev_proc_map_cmyk_color(eprn_map_cmyk_color_flex);
extern dev_proc_map_cmyk_color(eprn_map_cmyk_color_max);
extern dev_proc_map_cmyk_color(eprn_map_cmyk_color_glob);
extern dev_proc_fillpage(eprn_fillpage);

/*  Macro for initializing device procedure tables

    This macro corresponds to the macro prn_params_procs() which is used when
    basing a device directly on the prn device.

    If your device does not need all of the procedures in the argument list,
    use the following defaults:
      p_open:		eprn_open_device
      p_close:		eprn_close_device
      p_get_params:	eprn_get_params
      p_put_params:	eprn_put_params
    On the other hand, if your driver needs its own procedure in any of these
    cases its code must also call the appropriate default routine.
*/
#define eprn_procs_initdata(p_open, p_close, p_get_params, p_put_params) \
  p_open,			/* open_device */		\
  eprn_get_initial_matrix,	/* get_initial_matrix */	\
  NULL,				/* sync_output */		\
  eprn_output_page,		/* output_page */		\
  p_close,			/* close_device */		\
  eprn_map_rgb_color_for_CMY_or_K,  /* map_rgb_color */		\
  eprn_map_color_rgb,		/* map_color_rgb */		\
  NULL,				/* fill_rectangle */		\
  NULL,				/* tile_rectangle */		\
  NULL,				/* copy_mono */			\
  NULL,				/* copy_color */		\
  NULL,				/* draw_line */			\
  NULL,				/* get_bits */			\
  p_get_params,			/* get_params */		\
  p_put_params,			/* put_params */		\
  eprn_map_cmyk_color_glob,	/* map_cmyk_color */		\
  NULL,				/* get_xfont_procs */		\
  NULL,				/* get_xfont_device */		\
  NULL,				/* map_rgb_alpha_color */	\
  gx_page_device_get_page_device, /* get_page_device */         \
  NULL,                         /* get_alpha_bits */            \
  NULL,                         /* copy_alpha */                \
  NULL,                         /* get_band */                  \
  NULL,                         /* copy_rop */                  \
  NULL,                         /* fill_path */                 \
  NULL,                         /* stroke_path */               \
  NULL,                         /* fill_mask */                 \
  NULL,                         /* fill_trapezoid */            \
  NULL,                         /* fill_parallelogram */        \
  NULL,                         /* fill_triangle */             \
  NULL,                         /* draw_thin_line */            \
  NULL,                         /* begin_image */               \
  NULL,                         /* image_data */                \
  NULL,                         /* end_image */                 \
  NULL,                         /* strip_tile_rectangle */      \
  NULL,                         /* strip_copy_rop */            \
  NULL,                         /* get_clipping_box */          \
  NULL,                         /* begin_typed_image */         \
  NULL,                         /* get_bits_rectangle */        \
  NULL,                         /* map_color_rgb_alpha */       \
  NULL,                         /* create_compositor */         \
  NULL,                         /* get_hardware_params */       \
  NULL,                         /* text_begin */                \
  NULL,                         /* finish_copydevice */         \
  NULL,                         /* begin_transparency_group */  \
  NULL,                         /* end_transparency_group */    \
  NULL,                         /* begin_transparency_mask */   \
  NULL,                         /* end_transparency_mask */     \
  NULL,                         /* discard_transparency_layer */\
  NULL,                         /* get_color_mapping_procs */   \
  NULL,                         /* get_color_comp_index */      \
  NULL,                         /* encode_color */              \
  NULL,                         /* decode_color */              \
  NULL,                         /* pattern_manage */            \
  NULL,                         /* fill_rectangle_hl_color */   \
  NULL,                         /* include_color_space */       \
  NULL,                         /* fill_linear_color_scanline */\
  NULL,                         /* fill_linear_color_trapezoid */ \
  NULL,                         /* fill_linear_color_triangle */\
  NULL,                         /* update_spot_equivalent_colors */\
  NULL,                         /* ret_devn_params */              \
  eprn_fillpage                 /* fillpage */
  /* The remaining fields should be NULL. */

/*****************************************************************************/

/*  Access to pixels

    Of course, you could also use the prn interface to obtain pixel values.
    This, however, requires information on how eprn interprets 'gx_color_index'
    values (see eprnrend.c for a description) and bypasses all processing steps
    (error diffusion and the clipping of null bytes) which happen in
    eprn_get_planes().
*/

/*  Number of bits and hence bit planes used to represent 'levels' intensity
    levels in eprn_get_planes(). The return values are independent of the
    device's state. */
extern unsigned int eprn_bits_for_levels(unsigned int levels);

/*  Total number of bit planes written by eprn_get_planes().
    The value returned is reliable only after the eprn part of the device has
    been successfully opened. The value remains constant while the device is
    open and equals the sum of eprn_bits_for_levels() for all colorants. */
extern unsigned int eprn_number_of_bitplanes(eprn_Device *dev);

/*  Maximal lengths, in terms of the number of 'eprn_Octet' instances, for each
    bit plane returned by eprn_get_planes() for this device.
    These values are reliable after the eprn part of the device has been
    successfully opened and remain constant while the device is open.
    The array 'lengths' must have at least the length returned by
    eprn_number_of_bitplanes(). */
extern void eprn_number_of_octets(eprn_Device *dev, unsigned int lengths[]);

/*  The following function may be called only from the page printing procedure
    (see the macro eprn_device_initdata() above). For every invocation of that
    procedure, the first call of this function returns the scan line with
    device coordinate y = 0, subsequent invocations return the lines with the
    y coordinate incremented by 1 for every call.

    This function returns each scan line split into bit planes and stored into
    the areas pointed to by the 'bitplanes' array. The order of colorants is
    (K)(CMY) or RGB, where the parentheses indicate optional groups, depending
    on the colour model. Within each colorant, bit planes are stored from least
    to most significant. The number of bit planes per colorant is given by
    eprn_bits_for_levels(levels) where 'levels' is the number of intensity
    levels for this colorant.

    If the bit planes for a particular colorant result in the value zero for a
    certain pixel, this indicates a complete absence of that colorant for this
    pixel. The highest possible value is one less than the number of intensity
    levels for that colorant.

    The 'str' entries in the 'bitplanes' array must be non-NULL and point to
    storage areas of a length sufficient to hold the number of octets as
    returned by eprn_number_of_octets(). The 'length' entries will be set such
    that there will be no trailing zero octets in the octet strings returned.
    Bits are stored in an octet from most to least signicant bit (left to
    right). If a bit plane does not fill an integral number of octets the last
    octet will be filled on the right as if there were additional white pixels.

    This function returns zero if there was a scan line to be fetched and a
    non-zero value otherwise.
 */
extern int eprn_get_planes(eprn_Device *dev, eprn_OctetString bitplanes[]);

/*****************************************************************************/

/* Auxiliary types and functions for parameter processing */

/* Type to describe an association between a string and an integer */
typedef struct {
  const char *name;
  int value;
} eprn_StringAndInt;

/*  Routines on 'eprn_StringAndInt' arrays
    These are used to map names to integers and vice versa. It is possible to
    have several names for the same value. The first name given in the array
    for a particular 'value' is the canonical name for this value, i.e. the one
    returned by the eprn_get_string() routine.
    The arrays must be terminated by entries with a NULL pointer for 'name'.
*/
struct gs_param_string_s;	/* Forward declaration */
extern int eprn_get_string(int in_value, const eprn_StringAndInt *table,
  struct gs_param_string_s *out_value);
extern int eprn_get_int(const struct gs_param_string_s *in_value,
  const eprn_StringAndInt *table, int *out_value);

/*****************************************************************************/

/* CUPS message prefixes */
#define CUPS_ERRPREF	"ERROR: "
#define CUPS_WARNPREF	"WARNING: "

/*****************************************************************************/

/* Selection character for tracing */
#ifndef EPRN_TRACE_CHAR
#define EPRN_TRACE_CHAR	'_'
#endif	/* !EPRN_TRACE_CHAR */

/* Debugging function */
struct gs_param_list_s;
extern void eprn_dump_parameter_list(struct gs_param_list_s *plist);

/* Internal data structures */
extern const eprn_StringAndInt eprn_colour_model_list[];

/* Internal functions */
extern int eprn_set_page_layout(eprn_Device *dev);
extern int eprn_check_colour_info(const eprn_ColourInfo *list,
  eprn_ColourModel *model, float *hres, float *vres,
  unsigned int *black_levels, unsigned int *non_black_levels);
extern int eprn_fetch_scan_line(eprn_Device *dev, eprn_OctetString *line);
extern void eprn_finalize(bool is_RGB, unsigned int non_black_levels,
  int planes, eprn_OctetString *plane, eprn_Octet **ptr, int pixels);
extern void eprn_split_FS(eprn_OctetString *line, eprn_OctetString *next_line,
  int max_octets, eprn_ColourModel colour_model, unsigned int black_levels,
  unsigned int non_black_levels, eprn_OctetString bitplanes[]);

/*****************************************************************************/

#endif	/* Inclusion protection */
