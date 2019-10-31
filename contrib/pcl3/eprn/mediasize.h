/******************************************************************************
  File:     $Id: mediasize.h,v 1.11 2001/04/12 18:35:26 Martin Rel $
  Contents: Header file for working with various media sizes
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

#ifndef _mediasize_h	/* Inclusion protection */
#define _mediasize_h

/*****************************************************************************/

/* Macros for conversion between units.
   All these macros resolve to floating point numbers. */

/* Millimetres per inch */
#define MM_PER_IN	25.4f

/* Big points (units in PostScript's default user space) per inch */
#define BP_PER_IN	72.0f

/* Big points per millimetre */
#define BP_PER_MM	(BP_PER_IN/MM_PER_IN)

/******************************************************************************

  Sizes
  =====

  The constants of type 'ms_Size' each represent a rectangular area ("sheet")
  completely characterized by the lengths of its two edges. The orientation
  of the sheet is unspecified but note that the 'ms_SizeDescription' entries
  are guaranteed to return information such that dimen[0] <= dimen[1].

  The names of the size constants are mostly identical to the values for the
  "mediaOption" keywords in PPD files, prefixed with "ms_".  The keywords are
  documented in Appendix B, "Registered mediaOption Keywords", of:

      Adobe Systems Incorporated
      "PostScript Printer Description File Format Specification"
      Version 4.3
      9 February 1996
      Document ID: PN LPS5003

  It can be obtained from http://www.adobe.com.

  The following list does not contain all sizes from PPD 4.3, but all
  internationally standardized sizes are present.
*/

typedef enum {
  ms_none,		/* no size; useful as a sentinel or default value */
  ms_A10,
  ms_EnvC10,		/* Not a mediaOption keyword. */
  ms_ISOB10,
  ms_JISB10,		/* PPD 4.3 calls this "B10" */
  ms_A9,
  ms_EnvC9,		/* Not a mediaOption keyword. */
  ms_ISOB9,
  ms_JISB9,		/* PPD 4.3 calls this "B9" */
  ms_A8,
  ms_EnvC8,		/* Not a mediaOption keyword. */
  ms_ISOB8,
  ms_JISB8,		/* PPD 4.3 calls this "B8" */
  ms_A7,
  ms_Index3x5in,	/* US index card 3 x 5 in. Not a mediaOption keyword. */
  ms_EnvC7,
  ms_ISOB7,
  ms_EnvChou4,		/* Japanese */
  ms_JISB7,		/* PPD 4.3 calls this "B7" */
  ms_EnvMonarch,	/* US */
  ms_Env9,		/* US */
  ms_Postcard,		/* Japanese Hagaki postcard */
  ms_Index4x6in,	/* US index card 4 x 6 in. Not a mediaOption keyword. */
  ms_Env10,		/* US */
  ms_A6,
  ms_EnvDL,
  ms_EnvUS_A2,		/* US A2 envelope. Not a mediaOption keyword. */
  ms_EnvC6,
  ms_EnvChou3,		/* Japanese */
  ms_ISOB6,
  ms_Index5x8in,	/* US index card 5 x 8 in. Not a mediaOption keyword. */
  ms_JISB6,		/* PPD 4.3 calls this "B6" */
  ms_Statement,		/* US */
  ms_DoublePostcard,	/* double Japanese postcard */
  ms_A5,
  ms_EnvC5,
  ms_ISOB5,
  ms_JISB5,		/* PPD 4.3 calls this "B5" */
  ms_Executive,		/* US */
  ms_A4,
  ms_Folio,		/* US */
  ms_Quarto,		/* US */
  ms_Letter,		/* US */
  ms_Legal,		/* US */
  ms_EnvKaku3,		/* Japanese? */
  ms_SuperA,
  ms_ARCHA,		/* US */
  ms_EnvC4,
  ms_EnvKaku2,		/* Japanese? */
  ms_ISOB4,
  ms_JISB4,		/* PPD 4.3 calls this "B4" */
  ms_Tabloid,		/* US */
  ms_A3,
  ms_ARCHB,		/* US */
  ms_SuperB,
  ms_EnvC3,
  ms_HPSuperB,		/* what Hewlett-Packard calls "SuperB" (13x19 in).
                           Not a mediaOption keyword. */
  ms_ISOB3,
  ms_JISB3,		/* PPD 4.3 calls this "B3" */
  ms_A2,
  ms_ARCHC,		/* US */
  ms_EnvC2,
  ms_ISOB2,
  ms_JISB2,		/* PPD 4.3 calls this "B2" */
  ms_A1,
  ms_ARCHD,		/* US */
  ms_EnvC1,
  ms_ISOB1,
  ms_JISB1,		/* PPD 4.3 calls this "B1" */
  ms_A0,
  ms_ARCHE,		/* US */
  ms_EnvC0,
  ms_ISOB0,
  ms_JISB0,		/* PPD 4.3 calls this "B0" */
  ms_2A0,		/* Not a mediaOption keyword. */
  ms_4A0,		/* Not a mediaOption keyword. */
  /* End of discrete sizes */
  ms_CustomPageSize,	/* no particular size. Not a mediaOption keyword. */
  ms_MaxPage		/* largest available size on a particular device */
} ms_Size;

typedef struct {
  ms_Size size;
  const char *name;
   /* Names are identical to the name for the size constant after stripping the
      "ms_" prefix, hence the map from size names to size codes (except
      'ms_none') is bijective. There is no description for 'ms_none'. */
  float dimen[2];
   /* Given in bp and with 'dimen[0]' <= 'dimen[1]'. Both values are zero for
      unspecified sizes like 'ms_CustomPageSize' and both are positive
      otherwise.
    */
} ms_SizeDescription;

/******************************************************************************

  Media codes
  ===========

  PPD 4.3 describes several standard qualifiers ("." followed by a string) and
  substrings for mediaOption keywords. The type 'ms_MediaCode' is
  intended for representing a combination of 'ms_Size' with a set of flags
  holding additional information of this kind.

  The additional information, as standardized by Adobe and as far as it is
  supported here, consists of information concerning external conditions
  ("Extra" and "Transverse") and information relating to the way the medium is
  used ("Big"/"Small" and "Rotated"). I've also added two flags which the using
  application can endow with any meaning it needs.

  Use bitwise OR with the flag values to construct an 'ms_MediaCode' instance
  from an 'ms_Size' instance.
  Use ms_flags() and ms_without_flags() to split a value of type 'ms_MediaCode'
  into its components.
*/

typedef unsigned int ms_MediaCode;

/* Extraction of components */
#define MS_FLAG_MASK		0xFF00U
#define ms_flags(s)		((s) & MS_FLAG_MASK)
#define ms_without_flags(s)	((ms_Size)((s) & ~MS_FLAG_MASK))

/*  The "Small" substring or qualifier describes a sheet of the same size as
    without this modification but with a smaller imageable area (larger
    hardware margins). */
#define MS_SMALL_STRING		"Small"
#define MS_SMALL_FLAG		0x0400U

/*  The "Big" substring or qualifier describes a sheet of the same size as
    without this modification but with a larger imageable area (smaller
    hardware margins).
    This is not an Adobe-defined string, but note that there are sizes
    "PRC32K" and "PRC32KBig" with identical dimensions in PPD 4.3. */
#define MS_BIG_STRING		"Big"
#define MS_BIG_FLAG		0x0800U

/*  The "Rotated" substring is used to distinguish between a sheet's two
    different orientations with respect to the page contents. */
#define MS_ROTATED_STRING	"Rotated"
#define MS_ROTATED_FLAG		0x1000U

/*  The "Extra" substring or qualifier identifies a sheet which is actually a
    bit larger than specified by the size code (in PPD 4.3 usually 1 inch in
    each dimension). */
#define MS_EXTRA_STRING		"Extra"
#define MS_EXTRA_FLAG		0x2000U

/*  The "Transverse" qualifier is used to distinguish between a sheet's two
    different orientations with respect to an external reference system, for
    example the feeding direction. */
#define MS_TRANSVERSE_STRING	"Transverse"
#define MS_TRANSVERSE_FLAG	0x4000U

/*  Flags with user-definable interpretation */
#define MS_USER_FLAG_1		0x0200U
#define MS_USER_FLAG_2		0x0100U

/* Type for lists of flags and their names */
typedef struct {
  ms_MediaCode code;
  const char *name;
} ms_Flag;

/*****************************************************************************/

/*  Find the size description for a media code. The flags in 'code' are
    ignored. If the code is unknown, a NULL pointer will be returned.
 */
extern const ms_SizeDescription *ms_find_size_from_code(ms_MediaCode code);

/*===========================================================================*/

/*  Generate a media code for a name

    Acceptable names are all size names and all size names extended with any
    combination of the known substrings and qualifiers. The rules for forming a
    name recognized by this function are:
    - The order is: base name, substrings, qualifiers.
    - No string may appear twice.
    - At most one of "Big" and "Small" may be present.
    Note that "Transverse" is only permitted as a qualifier and "Rotated" may
    only appear as a substring. PPD 4.3 seems to prefer substrings to
    qualifiers where possible.

    The pointer 'user_flag_list', if non-NULL, must refer to a sequence of
    entries associating user flags with names. The list is terminated by the
    first entry having a zero 'code'. The names in this list will be recognized
    as substrings in addition to the standard names. The list will be searched
    linearly for a matching entry.

    The function returns 'ms_none' if it cannot convert the name.

    The function has the following restrictions:
    - The flags returned do not distinguish between appearance as a substring
      and appearance as a qualifier or consider the order of appearance.
    - There is no support for serialization qualifiers.
*/
extern ms_MediaCode ms_find_code_from_name(const char *name,
  const ms_Flag *user_flag_list);

/*===========================================================================*/

/*  Construct a name for a media code

    The pointer 'buffer' must be non-NULL and point to a storage area of at
    least 'length' octets. The function will convert 'code' into a string
    representation and write it to 'buffer', terminating it with NUL. The list
    'user_flag_list', if non-NULL, is taken into account for the conversion of
    user flags in 'code'. It is searched linearly until the first entry with a
    zero 'code'. If an appropriate entry is found it is added as a substring
    before all standard substrings.

    The function returns zero on success and a non-zero value otherwise.
    In the latter case, 'errno' will have been set to one of the following
    values:
    - EDOM: 'code' contains an unkown size or an unknown flag. The last case
      includes the situation where a user flag is found in 'code' and
      'user_flag_list' is NULL or does not contain a matching entry.
    - EINVAL: 'buffer' is NULL or 'length' is zero.
    - ERANGE: 'length' is insufficient to store the resulting string.
 */
extern int ms_find_name_from_code(char *buffer, size_t length,
  ms_MediaCode code, const ms_Flag *user_flag_list);

/*****************************************************************************/

#endif	/* Inclusion protection */
