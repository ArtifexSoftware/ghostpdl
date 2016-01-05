/******************************************************************************
  File:     $Id: pclsize.c,v 1.10 2001/08/18 17:41:49 Martin Rel $
  Contents: Maps between PCL Page Size codes und size information
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1999, 2000 by Martin Lottermoser			      *
*	All rights reserved						      *
*									      *
******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Standard headers */
#include <stdlib.h>
#include <string.h>

/* Specific headers */
#include "pclsize.h"

/*****************************************************************************/

/* Number of elements in an array */
#define array_size(a)	(sizeof(a)/sizeof(a[0]))

/*****************************************************************************/

/*  Map from media code to PCL Page Size code

    This list is used to find a PCL Page Size code for a particular media code.
    It will be sorted by media code before the first use.

    This structure is based on the assumption that one needs only a single
    Page Size code for each supported media code. See the discussion in
    pclgen.h.
*/

typedef struct {
  ms_MediaCode mc;
  pcl_PageSize ps;
} CodeEntry;

#define me(keyword)	{ms_##keyword, pcl_ps_##keyword}

static CodeEntry code_map[] = {
  me(Executive),
  me(Letter),
  me(Legal),
  me(Tabloid),
  me(Statement),
  me(HPSuperB),
  me(A6),
  me(A5),
  me(A4),
  me(A3),
  me(JISB5),
  me(JISB4),
  me(Postcard),
  me(DoublePostcard),
  {ms_A6 | PCL_CARD_FLAG, pcl_ps_A6Card},
  me(Index4x6in),
  me(Index5x8in),
  me(Index3x5in),
  me(EnvMonarch),
  me(Env10),
   /* I've used 'pcl_ps_Env10' instead of 'pcl_ps_Env10_Negative' because of the
      DJ500 and DJ500C models which do not accept 'pcl_ps_Env10_Negative'. */
  me(EnvDL),
  me(EnvC5),
  me(EnvC6),
  me(ISOB5),
  me(CustomPageSize),
  me(EnvUS_A2),
  me(EnvChou3),
  me(EnvChou4),
  me(EnvKaku2)
};

#undef me

/******************************************************************************

  Function: cmp_by_size

  This function compares two 'CodeEntry' instances by their media codes.

******************************************************************************/

static int cmp_by_size(const void *a, const void *b)
{
  return ((const CodeEntry *)a)->mc - ((const CodeEntry *)b)->mc;
}

/******************************************************************************

  Function: pcl3_page_size

  If it knows a PCL Page Size code for the specified 'code', the function will
  return it. Otherwise the return value will be 'pcl_ps_default'.

  Flags in 'code' will be ignored except for PCL_CARD_FLAG.

******************************************************************************/

pcl_PageSize pcl3_page_size(ms_MediaCode code)
{
  static pcl_bool initialized = FALSE;
  CodeEntry key;
  const CodeEntry *result;

  /* Sort the table if necessary */
  if (!initialized) {
    qsort(code_map, array_size(code_map), sizeof(CodeEntry), cmp_by_size);
    initialized = TRUE;
  }

  key.mc = ms_without_flags(code) |( code & PCL_CARD_FLAG);
  result = (const CodeEntry *)bsearch(&key, code_map, array_size(code_map),
    sizeof(CodeEntry), cmp_by_size);

  return result == NULL? pcl_ps_default: result->ps;
}

/******************************************************************************

  Function: cmp_by_code

  This function compares two 'CodeEntry' instances by their PCL Page Size codes.

******************************************************************************/

static int cmp_by_code(const void *a, const void *b)
{
  return ((const CodeEntry *)a)->ps - ((const CodeEntry *)b)->ps;
}

/******************************************************************************

  Function: pcl3_media_code

  This function will return a media code for the given PCL page size code.
  If the code is unknown, 'ms_none' will be returned.

******************************************************************************/

ms_MediaCode pcl3_media_code(pcl_PageSize code)
{
  static CodeEntry inverse_map[array_size(code_map)];
  static pcl_bool initialized = FALSE;
  CodeEntry key;
  const CodeEntry *result;

  /* Construct the table if necessary */
  if (!initialized) {
    memcpy(&inverse_map, &code_map, sizeof(inverse_map));
    qsort(inverse_map, array_size(inverse_map), sizeof(CodeEntry), cmp_by_code);
    initialized = TRUE;
  }

  key.ps = code;
  result = (const CodeEntry *)bsearch(&key, inverse_map,
    array_size(inverse_map), sizeof(CodeEntry), cmp_by_code);
  if (result == NULL) {
    key.ps = -code;
     /* Actually, this is a generalization on my part: I am assuming that any
        two valid PCL Page Size codes with the same absolute value refer to the
        same media extension irrespective of sheet orientation in raster space.
        I have found negative Page Size codes in HP documentation only for
        Env10 and EnvDL. */
    result = (const CodeEntry *)bsearch(&key, inverse_map,
      array_size(inverse_map), sizeof(CodeEntry), cmp_by_code);
  }

  return result == NULL? ms_none: result->mc;
}

/******************************************************************************

  Function: pcl3_size_description

  This function will try to find a media size description for the specified
  'code'. If the code is unknown, the function will return a NULL pointer.

******************************************************************************/

const ms_SizeDescription *pcl3_size_description(pcl_PageSize code)
{
  return ms_find_size_from_code(pcl3_media_code(code));
}
