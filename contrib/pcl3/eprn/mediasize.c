/******************************************************************************
  File:     $Id: mediasize.c,v 1.11 2001/04/12 18:35:26 Martin Rel $
  Contents: Operations and data for handling media sizes
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

#include "std.h"

/* Standard headers */
#include <assert_.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Special headers */
#include "mediasize.h"

/*****************************************************************************/

/* Number of elements in an array */
#define array_size(a)	(sizeof(a)/sizeof(a[0]))

/* String length of string literals */
#define STRLEN(s)	(sizeof(s) - 1)

/*****************************************************************************/

/* To ensure consistency I generate the size constant and the name from the
   keyword: */
#define sn(keyword)	ms_##keyword, #keyword

/*  Size list

    This list is ordered such that the size code is the index in the list and
    that entries are sorted by first and second dimension except at the end.
    If you compile without NDEBUG being defined these constraints will be
    checked at runtime before they are needed for the first time.

    The information on the sizes is mostly taken from table B.2 in PPD 4.3.

    The extensions for the A and ISO B series agree with DIN 476 part 1
    (February 1991) which is the German version of EN 20216:1990 which in turn
    is identical with ISO 216:1975. The C series values agree with DIN 476
    part 2 (February 1991). Some values for the A and C series are not listed
    in PPD 4.3 and are taken from DIN 476.
 */
static const ms_SizeDescription list[] = {
  {ms_none, "", 	{0.0,		0.0}},	/* never returned */
  {sn(A10),		{26*BP_PER_MM,	37*BP_PER_MM}},
  {sn(EnvC10),		{28*BP_PER_MM,	40*BP_PER_MM}},	/* DIN 476 T2: 1991 */
  {sn(ISOB10),		{31*BP_PER_MM,	44*BP_PER_MM}},
  {sn(JISB10),		{32*BP_PER_MM,	45*BP_PER_MM}},
  {sn(A9),		{37*BP_PER_MM,	52*BP_PER_MM}},
  {sn(EnvC9),		{40*BP_PER_MM,	57*BP_PER_MM}},	/* DIN 476 T2: 1991 */
  {sn(ISOB9),		{44*BP_PER_MM,	62*BP_PER_MM}},
  {sn(JISB9),		{45*BP_PER_MM,	64*BP_PER_MM}},
  {sn(A8),		{52*BP_PER_MM,	74*BP_PER_MM}},
  {sn(EnvC8),		{57*BP_PER_MM,	81*BP_PER_MM}},	/* DIN 476 T2: 1991 */
  {sn(ISOB8),		{62*BP_PER_MM,	88*BP_PER_MM}},
  {sn(JISB8),		{64*BP_PER_MM,	91*BP_PER_MM}},
  {sn(A7),		{74*BP_PER_MM,	105*BP_PER_MM}},
  {sn(Index3x5in),	{3*BP_PER_IN,	5*BP_PER_IN}},	     /* 76.2 x 127 mm */
  {sn(EnvC7),		{81*BP_PER_MM,	114*BP_PER_MM}},
  {sn(ISOB7),		{88*BP_PER_MM,	125*BP_PER_MM}},
  {sn(EnvChou4),	{90*BP_PER_MM,	205*BP_PER_MM}},
  {sn(JISB7),		{91*BP_PER_MM,	128*BP_PER_MM}},
  {sn(EnvMonarch),	{3.875*BP_PER_IN, 7.5*BP_PER_IN}}, /* 98.4 x 190.5 mm */
  {sn(Env9),		{3.875*BP_PER_IN, 8.875*BP_PER_IN}}, /* 98.4x225.4 mm */
  {sn(Postcard),	{100*BP_PER_MM,	148*BP_PER_MM}},
  {sn(Index4x6in),	{4.0*BP_PER_IN,	6.0*BP_PER_IN}},  /* 101.6 x 152.4 mm */
  {sn(Env10),		{4.125*BP_PER_IN, 9.5*BP_PER_IN}}, /* 104.8 x 241.3 mm*/
  {sn(A6),		{105*BP_PER_MM,	148*BP_PER_MM}},
  {sn(EnvDL),		{110*BP_PER_MM,	220*BP_PER_MM}},
  {sn(EnvUS_A2),	{4.375*BP_PER_IN, 5.75*BP_PER_IN}}, /* 111.1x146.1 mm */
  {sn(EnvC6),		{114*BP_PER_MM,	162*BP_PER_MM}},
  {sn(EnvChou3),	{120*BP_PER_MM,	235*BP_PER_MM}},
  {sn(ISOB6),		{125*BP_PER_MM,	176*BP_PER_MM}},
  {sn(Index5x8in),	{5.0*BP_PER_IN,	8.0*BP_PER_IN}},    /* 127 x 203.2 mm */
  {sn(JISB6),		{128*BP_PER_MM,	182*BP_PER_MM}},
  {sn(Statement),	{5.5*BP_PER_IN,	8.5*BP_PER_IN}},  /* 139.7 x 215.9 mm */
  {sn(DoublePostcard),	{148*BP_PER_MM,	200*BP_PER_MM}},
  {sn(A5),		{148*BP_PER_MM,	210*BP_PER_MM}},
  {sn(EnvC5),		{162*BP_PER_MM,	229*BP_PER_MM}},
  {sn(ISOB5),		{176*BP_PER_MM,	250*BP_PER_MM}},
  {sn(JISB5),		{182*BP_PER_MM,	257*BP_PER_MM}},
  {sn(Executive),	{7.25*BP_PER_IN, 10.5*BP_PER_IN}}, /* 184.2 x 266.7 mm*/
    /* Media called by this name may vary up to 0.5" in dimension (PPD 4.3). */
  {sn(A4),		{210*BP_PER_MM,	297*BP_PER_MM}},
  {sn(Folio),		{210*BP_PER_MM, 330*BP_PER_MM}},
  {sn(Quarto),		{8.5f*BP_PER_IN,	10.83f*BP_PER_IN}},  /* 215.9 x 275.1 mm
        PPD 4.3 uses bp values for the definition, but this does not agree
        with the mm values it specifies. The inch specifications fit. */
  {sn(Letter),		{8.5f*BP_PER_IN,	11.0f*BP_PER_IN}}, /* 215.9 x 279.4 mm */
  {sn(Legal),		{8.5f*BP_PER_IN,	14.0f*BP_PER_IN}}, /* 215.9 x 355.6 mm */
  {sn(EnvKaku3),	{216*BP_PER_MM,	277*BP_PER_MM}},
  {sn(SuperA),		{227*BP_PER_MM,	356*BP_PER_MM}},
  {sn(ARCHA),		{9*BP_PER_IN,	12*BP_PER_IN}},	  /* 228.6 x 304.8 mm */
  {sn(EnvC4),		{229*BP_PER_MM,	324*BP_PER_MM}},
  {sn(EnvKaku2),	{240*BP_PER_MM,	332*BP_PER_MM}},
  {sn(ISOB4),		{250*BP_PER_MM,	353*BP_PER_MM}},
  {sn(JISB4),		{257*BP_PER_MM,	364*BP_PER_MM}},
  {sn(Tabloid),		{11*BP_PER_IN,	17*BP_PER_IN}},   /* 279.4 x 431.8 mm */
  {sn(A3),		{297*BP_PER_MM,	420*BP_PER_MM}},
  {sn(ARCHB),		{12*BP_PER_IN,	18*BP_PER_IN}},	  /* 304.8 x 457.2 mm */
  {sn(SuperB),		{305*BP_PER_MM,	487*BP_PER_MM}},
  {sn(EnvC3),		{324*BP_PER_MM,	458*BP_PER_MM}},
  {sn(HPSuperB),	{13*BP_PER_IN,	19*BP_PER_IN}},	  /* 330.2 x 482.6 mm */
  {sn(ISOB3),		{353*BP_PER_MM,	500*BP_PER_MM}},
  {sn(JISB3),		{364*BP_PER_MM,	515*BP_PER_MM}},
  {sn(A2),		{420*BP_PER_MM,	594*BP_PER_MM}},
  {sn(ARCHC),		{18*BP_PER_IN,	24*BP_PER_IN}},	  /* 457.2 x 609.6 mm */
  {sn(EnvC2),		{458*BP_PER_MM,	648*BP_PER_MM}},
  {sn(ISOB2),		{500*BP_PER_MM,	707*BP_PER_MM}},
  {sn(JISB2),		{515*BP_PER_MM,	728*BP_PER_MM}},
  {sn(A1),		{594*BP_PER_MM,	841*BP_PER_MM}},
  {sn(ARCHD),		{24*BP_PER_IN,	36*BP_PER_IN}},	  /* 609.6 x 914.4 mm */
  {sn(EnvC1),		{648*BP_PER_MM,	917*BP_PER_MM}},
  {sn(ISOB1),		{707*BP_PER_MM,	1000*BP_PER_MM}},
  {sn(JISB1),		{728*BP_PER_MM,	1030*BP_PER_MM}},
  {sn(A0),		{841*BP_PER_MM,	1189*BP_PER_MM}},
  {sn(ARCHE),		{36*BP_PER_IN,	48*BP_PER_IN}},	 /* 914.4 x 1219.2 mm */
  {sn(EnvC0),		{917*BP_PER_MM,	1297*BP_PER_MM}},
  {sn(ISOB0),		{1000*BP_PER_MM, 1414*BP_PER_MM}},
  {sn(JISB0),		{1030*BP_PER_MM, 1456*BP_PER_MM}},
  {sn(2A0),		{1189*BP_PER_MM, 1682*BP_PER_MM}}, /* DIN 476 T1:1991 */
  {sn(4A0),		{1682*BP_PER_MM, 2378*BP_PER_MM}}, /* DIN 476 T1:1991 */
  /* End of discrete sizes */
  {sn(CustomPageSize),	{0.0, 0.0}},
  {sn(MaxPage),		{0.0, 0.0}}
};

#undef sn

/* Constant which is at least 1 longer than the longest known name */
#define LONGER_THAN_NAMES	15

/*****************************************************************************/

#undef CHECK_CONSTRAINTS

#ifdef CHECK_CONSTRAINTS
static char checked = 0;

/* Function to check constraints on table entries */
static void check(void)
{
  int j;

  /* ms_none */
  assert(list[0].size == 0);

  for (j = 1; j < array_size(list); j++) {
    assert(list[j].size == j);
    assert(list[j].dimen[0] <= list[j].dimen[1]);
    assert(strlen(list[j].name) < LONGER_THAN_NAMES);
    assert(list[j].dimen[0] == 0.0 || list[j-1].dimen[0] < list[j].dimen[0] ||
           (list[j-1].dimen[0] == list[j].dimen[0] &&
            list[j-1].dimen[1] <= list[j].dimen[1]));
  }

  /* Check that the highest accepted value does not collide with the flags */
  assert((ms_MaxPage & MS_FLAG_MASK) == 0);

  checked = 1;

  return;
}

#endif	/* !NDEBUG */

/*****************************************************************************/

const ms_SizeDescription *ms_find_size_from_code(ms_MediaCode code)
{
#ifdef CHECK_CONSTRAINTS
  if (!checked) check();
#endif
  code = ms_without_flags(code);
  if (code < 1 || array_size(list) <= code) return NULL;

  return list + code;
}

/*****************************************************************************/

/* Function to compare two pointers to 'ms_SizeDescription *' by the 'name'
   fields pointed to. */

static int cmp_by_name(const void *a, const void *b)
{
  return strcmp((*(const ms_SizeDescription *const *)a)->name,
    (*(const ms_SizeDescription *const *)b)->name);
}

/******************************************************************************

  Function: find_flag

  This function searches for a number of flags at the end of 'name' which is
  taken to be a string of length '*length'. If a matching entry is found,
  '*length' is decreased by the length of the string matched and the flag value
  is returned. On failure, the return value is zero and '*length' is not
  modified.

******************************************************************************/

static ms_MediaCode find_flag(const char *name, size_t *length,
  const ms_Flag *flag_list)
{
  int j = 0;
  size_t L;

  while (flag_list[j].code != 0 &&
    ((L = strlen(flag_list[j].name)) >= *length ||
      strncmp(name + *length - L, flag_list[j].name, L) != 0)) j++;
  if (flag_list[j].code == 0) return 0;

  *length -= L;

  return flag_list[j].code;
}

/*****************************************************************************/

/* Table of standard substrings, sorted in the order preferred for name
   generation */
static const ms_Flag substrings[] = {
  {MS_BIG_FLAG,		MS_BIG_STRING},
  {MS_SMALL_FLAG,	MS_SMALL_STRING},
  {MS_ROTATED_FLAG,	MS_ROTATED_STRING},
  {MS_EXTRA_FLAG,	MS_EXTRA_STRING},
  {0, NULL}
};

/* If you get an error when compiling the following, then MAX_MEDIASIZES
 * (defined in pcltables.h) must be increased. */
typedef struct
{
        char compile_time_assert[array_size(list) <= MAX_MEDIASIZES ? 1 : -1];
} compile_time_assert_for_list_length;

/*****************************************************************************/

ms_MediaCode ms_find_code_from_name(mediasize_table *tables,
                                    const char *name,
                                    const ms_Flag *user_flag_list)
{
  const char *end;
  char stripped_name[LONGER_THAN_NAMES];
  ms_SizeDescription
    keydata,
    *key = &keydata;
  const ms_SizeDescription **found;
  ms_MediaCode flags = 0;
  size_t l;

  /* On the first use of this function, compile a table of pointers into the
     list which is sorted by the names of the sizes. */
  if (tables->mediasize_list_inited == 0) {
    int entries = 1; /* ignore 'ms_none' */
    while (entries < array_size(list)) {
      tables->mediasize_list[entries] = list + entries;
      entries++;
    }
    qsort(tables->mediasize_list, array_size(list) - 1, sizeof(ms_SizeDescription *), &cmp_by_name);
    tables->mediasize_list_inited = 1;
  }

  /* Prevent idiots (like myself) from crashing the routine */
  if (name == NULL) return ms_none;

  /* Identify trailing qualifiers */
  end = strchr(name, '.');	/* before first qualifier */
  if (end == NULL) end = strchr(name, '\0');
  else {
    const char *s = end, *t;
    do {
      ms_MediaCode flag;

      s++;
      if ((t = strchr(s, '.')) == NULL) t = strchr(s, '\0');
      l = t - s;
#define set_if(keyword)					\
        if (l == STRLEN(MS_##keyword##_STRING) &&	\
          strncmp(s, MS_##keyword##_STRING, l) == 0) flag = MS_##keyword##_FLAG
      set_if(TRANSVERSE);
      else set_if(BIG);
      else set_if(SMALL);
      else set_if(EXTRA);
      else return ms_none;
#undef set_if
      if ((flag & flags) != 0) return ms_none;	/* no duplicates */
      flags |= flag;
      s = t;
    } while (*t != '\0');
  }

  /* Now search for recognizable substrings */
  l = end - name;	/* length of uninterpreted part of the name */
  while (1) {
    ms_MediaCode flag;

    if ((flag = find_flag(name, &l, substrings)) == 0 &&
        (user_flag_list == NULL ||
           (flag = find_flag(name, &l, user_flag_list)) == 0))
      break;	/* loop exit */

    if ((flag & flags) != 0) return ms_none;	/* no duplicates */
    flags |= flag;
  }
  end = name + l;

  if ((flags & MS_BIG_FLAG) != 0 && (flags & MS_SMALL_FLAG) != 0)
    return ms_none;

  /* Prepare key for looking up the size part of the name */
  l = end - name;
  if (l >= LONGER_THAN_NAMES) return ms_none;
  strncpy(stripped_name, name, l);
  stripped_name[l] = '\0';
  keydata.name = stripped_name;

  /* Search */
  found = (const ms_SizeDescription **)bsearch(&key, tables->mediasize_list, array_size(list) - 1,
    sizeof(ms_SizeDescription *), &cmp_by_name);

  return found == NULL? ms_none: ((*found)->size | flags);
}

/******************************************************************************

  Function: add_substrings

  This function appends names for the flags in '*code' to 'buffer', identifying
  flags by means of 'flag_list' which must be terminated with an entry having
  'code' == 0.

  'buffer' must point to a NUL-terminated string, '*length' must be the number
  of characters which this routine may write after the NUL.

  The function will return zero on success and a non-zero value of error.
  All flags matched will be removed from '*code' and '*length' will be updated
  to reflect the number of characters still available.

******************************************************************************/

static int add_substrings(char *buffer, size_t *length, ms_MediaCode *code,
  const ms_Flag *flag_list)
{
  int j;
  size_t l;

  j = 0;
  buffer = strchr(buffer, '\0');
  while (flag_list[j].code != 0) {
    if (flag_list[j].code & *code) {
      l = strlen(flag_list[j].name);
      if (*length < l) {
        errno = ERANGE;
        return -1;
      }
      *code &= ~flag_list[j].code;
      strcpy(buffer, flag_list[j].name);
      buffer += l;
      *length -= l;
    }
    j++;
  }

  return 0;
}

/*****************************************************************************/

extern int ms_find_name_from_code(char *buffer, size_t length,
  ms_MediaCode code, const ms_Flag *user_flag_list)
{
  const ms_SizeDescription *desc = ms_find_size_from_code(code);
  size_t l;

  if (buffer == NULL || length == 0) {
    errno = EINVAL;
    return -1;
  }

  /* Size name */
  if (desc == NULL) {
    errno = EDOM;
    return -1;
  }
  l = strlen(desc->name);
  if (length <= l) {
    errno = ERANGE;
    return -1;
  }
  strcpy(buffer, desc->name);
  length -= l + 1;
  code = ms_flags(code);

  /* Substrings */
  if ((user_flag_list != NULL &&
       add_substrings(buffer, &length, &code, user_flag_list) != 0) ||
      add_substrings(buffer, &length, &code, substrings) != 0) return -1;

  /* Transverse qualifier */
  if (code & MS_TRANSVERSE_FLAG) {
    if (length < 1 + STRLEN(MS_TRANSVERSE_STRING)) {
      errno = ERANGE;
      return -1;
    }
    strcat(buffer, "." MS_TRANSVERSE_STRING);
    code &= ~MS_TRANSVERSE_FLAG;
  }

  /* Check for unrecognized flags */
  if (code != 0) {
    errno = EDOM;
    return -1;
  }

  return 0;
}
