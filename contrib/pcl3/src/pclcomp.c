/******************************************************************************
  File:     $Id: pclcomp.c,v 1.11 2000/10/07 17:51:57 Martin Rel $
  Contents: Implementation of PCL compression routines
  Author:   Martin Lottermoser, Greifswaldstrasse 28, 38124 Braunschweig,
            Germany. E-mail: Martin.Lottermoser@t-online.de.

*******************************************************************************
*									      *
*	Copyright (C) 1996, 1997, 1998, 2000 by Martin Lottermoser	      *
*	All rights reserved						      *
*									      *
*******************************************************************************

  If you compile with NDEBUG defined, some runtime checks for programming
  errors (mine and the interface's user's) are omitted.

******************************************************************************/

/*****************************************************************************/

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE	500
#endif

/* Interface definition */
#include "pclgen.h"

/* Standard headers */
#include <assert.h>
#include <string.h>

/*****************************************************************************/

/*  For TIFF compression, we need the two's complement representation of
    numbers in the range [-127, -1], expressed in a 'pcl_Octet'.

    The macro neg() must accept as an argument an 'int' expression with a value
    in that range and convert it such that the result can be assigned to a
    variable of type 'pcl_Octet' with the result that the value of the latter
    becomes the two's complement representation of the number with respect to
    256.

    On most machines one can use a simple assignment. However, the C standard
    specifies the behaviour in these cases (signed to unsigned where the source
    has more bits than the target and the source value is negative) to be
    implementation-defined. Hence the need for a portable solution.
    Define PCLCOMP_NEG if you don't like it.
*/

#ifdef PCLCOMP_NEG
#define neg(number)	(number)
#else
#define neg(number)	(256 + (number))
#endif

/******************************************************************************

  Function: compress_runlength

  This function performs runlength encoding.

  Runlength encoding consists in preceding each octet by a repeat count octet
  the value of which is one less than the repeat count.

  'in' and 'incount' describe the row to be compressed, 'out' an area of at
  least 'maxoutcount' octets to which the result is to be written. The function
  returns the number of octets written or a negative value on error.

  'incount' may be zero, in which case the values of the other arguments are
  irrelevant. Otherwise, 'in' must be non-NULL, and if 'maxoutcount' is positive
  'out' must be non-NULL as well.

******************************************************************************/

static int compress_runlength(const pcl_Octet *in, int incount, pcl_Octet *out,
  int maxoutcount)
{
  int available = maxoutcount;

  /* Here 'incount' is the number of octets to be encoded and 'in' points to
     the first of them. 'available' is the number of octets available in the
     output array and 'out' points to the first of them. */
  while (incount > 0 && available > 1) {
    int count = 0;

    out++;	/* skip repetition count octet, to be filled in later */
    *out = *in;
    do {
      in++; incount--; count++;
    } while (incount > 0 && *in == *out && count < 256);
    *(out - 1) = count - 1;
    out++; available -= 2;
  }

  if (incount > 0) return -1;
  return maxoutcount - available;
}

/******************************************************************************

  Function: compress_tiff

  This function performs TIFF compression (compression method 2).

  'in' and 'incount' describe the row to be compressed, 'out' an area of at
  least 'maxoutcount' octets to which the result is to be written. The function
  returns the number of octets written or a negative value on error.

  'in' must be non-NULL, 'incount' must be positive, 'maxoutcount' must be
  non-negative, and 'out' must be non-NULL is 'maxoutcount' is positive.

  TIFF compression creates an octet stream consisting of three kinds of
  octet sequences:
  - an octet with value in the range [-127, -1] (two's complement)
    followed by a single octet: this means the second octet is to be
    repeated -<first octet>+1 times,
  - an octet with value in the range [0, 127]: this means the next
    <first octet>+1 octets have not been compressed,
  - an octet with the value -128: this is a no-op and must be ignored.
  The first octet determining the kind of sequence is called the "control
  byte".

  This routine generates an output string with a length which is minimal
  for TIFF compression (if it doesn't, it's a bug). Readability of the code
  and minimal execution speed were secondary considerations.

  I have implemented this as a finite state machine. As can be seen from
  the code, I sacrificed the rules of structured programming for this,
  because I found a state transition diagram much more intelligible
  than anything I could code. I then simply transferred it into C.

******************************************************************************/

static int compress_tiff(const pcl_Octet *in, int incount, pcl_Octet *out,
  int maxoutcount)
{
  pcl_Octet
    last;	/* a remembered octet before the current 'in' value */
  const pcl_Octet
    *end = in + incount - 1;	/* last position in 'in' */
  int
    available = maxoutcount,  /* number of free octets left in 'out' */
    repeated,	/* repeat count during a compressed sequence */
    stored;	/* store count during a non-compressed sequence */

  state1:
    /* No octet is held over to be treated, 'in' points to the next one */
    if (in == end) {
      /* This is the last octet and a single one. */
      if (available < 2) return -1;
      *out = 0; out++;	/* control byte */
      *out = *in;
      available -= 2;
      goto finished;
    }
    last = *in; in++; /* Fetch one octet and remember it. */
    /* to state2 */

  /* state2: */
    /* One octet to be treated is in 'last', 'in' points to the next. */
    if (*in != last) {
      if (available < 3) return -1;
      out++; available--;	/* Skip control byte to be filled in later */
      stored = 0;
      goto state4;
    }
    if (available < 2) return -1;
    repeated = 2;
    /* to state3 */

  state3:
    /* We have read 'repeated' occurrences of 'last', 'in' is positioned on
       the last octet read. It is true that 2 <= repeated < 128 and
       2 <= available. */
    do {
      if (in == end) break;
      in++;
      if (*in != last) break;
      repeated++;
    } while (repeated < 128);

    /* Had to stop accumulating, for whatever reason. Write results. */
    *out = neg(-repeated + 1); out++;	/* control byte */
    *out = last; out++;
    available -= 2;

    /* Decide where to go from here */
    if (*in != last) goto state1;
    if (in == end) goto finished;
    in++;
    goto state1;

  state4:
    /* We have read 'stored'+2 octets, 0 <= stored <= 126. All except the
       last two have already been stored before the current value of 'out',
       leaving space for the control byte at out[-stored-1]. The last two
       octets can be found in 'last' and '*in', and they are not identical.
       We also know that 'available' >= 2.
     */
    do {
      *out = last; stored++; available--; out++;
      if (in == end) {
        *out = *in; stored++; available--;
        out[-stored] = stored - 1; /* control byte */
        goto finished;
      }
      if (available < 2) return -1;
      last = *in;
      in++;
    } while (*in != last && stored <= 126);

    if (*in == last) {
      if (stored < 126) goto state5;
      out[-stored-1] = stored - 1; /* control byte */
      repeated = 2;
      goto state3;
    }

    /* stored == 127, available >= 2 */
    *out = last; stored++; available--; out++;
    out[-stored-1] = stored - 1; /* control byte */
    goto state1;

  state5:
    /* We have read 'stored'+2 octets, 'stored' < 126. 'stored' of them have
       been stored before 'out' with the control byte still to be written to
       out[-stored-1]. The last two octets can be found in 'last' and '*in',
       and they are identical. We also know 2 <= available. */
    if (in == end) {
      *out = last; out++;
      *out = *in;
      stored += 2; available -= 2;
      out[-stored] = stored - 1; /* control byte */
      goto finished;
    }
    in++;
    if (*in == last) {
      out[-stored-1] = stored - 1;	/* control byte */
      repeated = 3;
      goto state3;
    }
    if (available < 3) return -1;
    *out = last; stored++; available--; out++; /* The first repeated octet */
    goto state4;

  finished:
  return maxoutcount - available;
}

#undef neg

/******************************************************************************

  Function: write_delta_replacement

  This function writes a replacement string for delta compression (method 3),
  i.e. the sequence of command byte, optional extension offset bytes, and the
  replacement bytes.

  'out' points to a sequence of at least 'available' octets to which the string
  is to be written. 'reloffset' is the "left offset" value for the replacement.
  'in' points to a sequence of 'replace_count' octets to be replaced.
  'replace_count' must lie between 1 and 8, inclusive.

  This function returns a negative value on error or the number of octets
  written otherwise.

******************************************************************************/

static int write_delta_replacement(pcl_Octet *out, int available, int reloffset,
  const pcl_Octet *in, int replace_count)
{
  int used;
  assert(1 <= replace_count && replace_count <= 8);

  /* Prepare the command byte and, possibly, the extension offset bytes */
  used = 1;
  if (available < used) return -1;
  *out = (replace_count - 1) << 5;
  if (reloffset < 31) {
    *out++ += reloffset;
  }
  else {
    /* Large offset */
    *out++ += 31;
    reloffset -= 31;
    used += reloffset/255 + 1;
    if (available < used) return -1;
    while (reloffset >= 255) {
      *out++ = 255;
      reloffset -= 255;
    }
    *out++ = reloffset;
  }

  /* Transfer the replacement bytes */
  used += replace_count;
  if (available < used) return -1;
  while (replace_count > 0) {
    *out++ = *in++;
    replace_count--;
  }

  return used;
}

/******************************************************************************

  Function: compress_delta

  This function performs delta row compression (method 3).

  The pairs (in, incount) and (prev, prevcount) describe the row to be
  compressed and the row sent last (seed row), of course in uncompressed
  form. (out, maxcount) refers to a storage area of 'maxoutcount' length to
  which the compressed result for 'in' is to be written.
  All three octet strings must be valid and any may be zero.

  It is assumed that any difference in length between 'in' and 'prev' is
  (logically) due to trailing zero octets having been suppressed in the shorter
  of the two.

  The function returns the number of octets written to 'out' (a zero value is
  possible and refers to a row identical with the one sent last), or a negative
  value on error.

******************************************************************************/

/*  First a macro needed several times for comparing old and new row.
    Because we really need string substitution for the 'invalue', 'prevvalue'
    and 'repstart' parameters this cannot be realized by a function.
    This loop depends on the following variables external to it:
    pos, absoffset, out, opos, maxoutcount.
*/
#define delta_loop(bound, invalue, prevvalue, repstart)			\
  while (pos < bound) {							\
    if (invalue != prevvalue) {						\
      int reloffset = pos - absoffset;	/* "left offset" */		\
      absoffset = pos;	/* first different octet */			\
                                                                        \
      /* Collect different octets, at most 8 */				\
      do pos++;								\
      while (pos < bound && pos < absoffset + 8 && invalue != prevvalue); \
      /* All the octets with positions in [absoffset, pos) have to */	\
      /* be replaced, and there are at most 8 of them. */		\
                                                                        \
      /* Write the replacement string */				\
      {									\
        int written;							\
        written = write_delta_replacement(out + opos, maxoutcount - opos, \
          reloffset, repstart, pos - absoffset);			\
        if (written < 0) return -1;					\
        opos += written;						\
      }									\
      absoffset = pos;							\
    }									\
    else pos++;								\
  }

static int compress_delta(const pcl_Octet *in, int incount,
  const pcl_Octet *prev, int prevcount, pcl_Octet *out, int maxoutcount)
{
  int
    absoffset,	/* absolute offset (starting with zero) */
    mincount,	/* min(incount, prevcount) */
    opos,	/* next position in the output */
    pos;	/* next position in the input rows */

  /* Treat the special case of a zero output buffer (actually, the bad case is
     merely the one where 'out' is NULL) */
  if (maxoutcount == 0) {
    if (incount == prevcount &&
      (incount == 0 || memcmp(in, prev, incount) == 0)) return 0;
      /* Can there be machines where memcmp() compares bits beyond those
         used for the 'pcl_Octet's? Unlikely. */
    return -1;
  }

  /* Initialization */
  mincount = (incount < prevcount? incount: prevcount);
  pos = 0; opos = 0;
  absoffset = 0;	/* first untreated octet, i.e. position after the last
                           unaltered octet. */

  /* Loop over parts common to this and the last row */
  delta_loop(mincount, in[pos], prev[pos], in + absoffset);
  /*  Note: This artificial separation at the 'mincount' position (logically,
      both rows have equal length) is simpler to program but can result in
      the compressed row being 1 octet longer than necessary. */

  /* Treat length differences between 'in' and 'prev'. */
  if (mincount < incount) {
    /* We have to send all octets in the 'in' row which are non-zero. */
    delta_loop(incount, in[pos], 0, in + absoffset);
  }
  else {
    /* We have to replace all non-zero octets in the previous row. */
    pcl_Octet zero_block[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    delta_loop(prevcount, 0, prev[pos], zero_block);
  }
  assert(opos <= maxoutcount);

  return opos;
}

#undef delta_loop

/******************************************************************************

  Function: write_crdr_header

  This function writes the header for compressed replacement delta row encoding
  (method 9). It returns the number of octets written or a negative value on
  error.

******************************************************************************/

static int write_crdr_header(pcl_bool compressed, pcl_Octet *out,
  int maxoutcount, int reloffset, int repcount)
{
  int
    maxvalue,
    shift,
    used = 1;	/* command byte */

  /* The command byte */
  if (maxoutcount < 1) return -1;
  if (compressed) *out = 0x80;	/* control bit: compressed */
  else *out = 0;		/* control bit: uncompressed */
  maxvalue = (compressed? 3: 15);
  shift = (compressed? 5: 3);
  if (reloffset < maxvalue) {
    *out += reloffset << shift;
    reloffset = -1;
  }
  else {
    *out += maxvalue << shift;
    reloffset -= maxvalue;
  }
  /* The value to be encoded for 'repcount' is different from 'repcount': */
  if (compressed) repcount -= 2;
  else repcount--;
  assert(repcount >= 0);
  maxvalue = (compressed? 31: 7);
  if (repcount < maxvalue) {
    *out += repcount;
    repcount = -1;
  }
  else {
    *out += maxvalue;
    repcount -= maxvalue;
  }
  out++;

  /* Optional offset bytes */
  while (reloffset >= 0) {
    if (used >= maxoutcount) return -1;
    *out++ = (reloffset >= 255? 255: reloffset);
    reloffset -= 255;
    used++;
  }

  /* Optional replacement count bytes */
  while (repcount >= 0) {
    if (used >= maxoutcount) return -1;
    *out++ = (repcount >= 255? 255: repcount);
    repcount -= 255;
    used++;
  }

  return used;
}

/******************************************************************************

  Function: write_crdr_uncompressed

  This function returns the number of octets written or a negative value on
  error.

  'in' may be NULL, indicating a sequence of 'repcount' null octets.
  This case is practically irrelevant except for 'repcount' == 1.

******************************************************************************/

static int write_crdr_uncompressed(pcl_Octet *out, int maxoutcount,
  int reloffset, const pcl_Octet *in, int repcount)
{
  int used = write_crdr_header(FALSE, out, maxoutcount, reloffset, repcount);
  if (used < 0 || used + repcount > maxoutcount) return -1;

  out += used;
  if (in == NULL) memset(out, 0, repcount*sizeof(pcl_Octet));
  else memcpy(out, in, repcount*sizeof(pcl_Octet));

  return used + repcount;
}

/******************************************************************************

  Function: write_crdr_compressed

  This function returns the number of octets written or a negative value on
  error.

******************************************************************************/

static int write_crdr_compressed(pcl_Octet *out, int maxoutcount, int reloffset,
  pcl_Octet in, int repeat_count)
{
  int used = write_crdr_header(TRUE, out, maxoutcount, reloffset, repeat_count);
  if (used < 0 || used >= maxoutcount) return -1;

  out += used;
  *out = in;

  return used + 1;
}

/******************************************************************************

  Function: write_crdr_replacement

  This function returns the number of octets written to 'out' or a negative
  value on error.

  'in' may be NULL, indicating a sequence of 'repcount' null octets.
  'repcount' must be positive.

******************************************************************************/

static int write_crdr_replacement(pcl_Octet *out, int maxoutcount,
  int reloffset, const pcl_Octet *in, int repcount)
{
  const pcl_Octet *final;
  int written = 0;

  /* Treat the case of a null sequence */
  if (in == NULL) {
    if (repcount == 1)
      return write_crdr_uncompressed(out, maxoutcount, reloffset, in, repcount);
    return write_crdr_compressed(out, maxoutcount, reloffset, 0, repcount);
  }

  /* Loop over 'in', dividing it into sections at the boundaries of
     sequences of repeated octets. */
  final = in + repcount - 1;
  while (repcount > 0) {
    /* Advance 'bdup' over non-repeated octet */
    const pcl_Octet *bdup;
    bdup = in;
    while (bdup < final && *bdup != *(bdup + 1)) bdup++;

    /* If there is something either before a repeated section or before the
       end, encode it uncompressed. */
    if (in < bdup || bdup == final) {
      int incount = (bdup == final? repcount: bdup - in);
      int rc;
      rc = write_crdr_uncompressed(out + written, maxoutcount - written,
        reloffset, in, incount);
      if (rc < 0) return rc;
      written += rc;
      reloffset = 0;
      repcount -= incount;
      if (repcount > 0) in += incount;
    }

    /* If we have encountered a repeated section, encode it compressed.
       Note that the compressed version for a repetition is never longer than
       the uncompressed one, not even for a repeat count of 2, although in this
       case it might have equal length depending on the offset. */
    if (bdup < final) {
      const pcl_Octet *edup = bdup + 1;
      int incount, rc;
      while (edup < final && *(edup + 1) == *bdup) edup++;
      incount = edup - bdup + 1;
      rc = write_crdr_compressed(out + written, maxoutcount - written,
        reloffset, *bdup, incount);
      if (rc < 0) return rc;
      written += rc;
      reloffset = 0;
      repcount -= incount;
      if (repcount > 0) in = edup + 1;
    }
  }

  return written;
}

/******************************************************************************

  Function: compress_crdr

  This function performs compressed replacement delta row encoding (compression
  method 9).

  The pairs (in, incount) and (prev, prevcount) describe the row to be
  compressed and the row sent last (seed row), of course in uncompressed
  form. (out, maxcount) refers to a storage area of 'maxoutcount' length to
  which the compressed result for 'in' is to be written.
  All three octet strings must be valid and any may be zero.

  It is assumed that any difference in length between 'in' and 'prev' is
  (logically) due to trailing zero octets having been suppressed in the shorter
  of the two.

  The function returns the number of octets written to 'out' (a zero value is
  possible and refers to a row identical with the one sent last), or a negative
  value on error.

  This function and those it calls are very similar to the functions for delta
  row compression.

******************************************************************************/

/*  Again, as for delta row compression, I'm using a macro for comparison. */
#define crdr_loop(bound, invalue, prevvalue, repstart)			\
  while (pos < bound) {							\
    if (invalue == prevvalue) pos++;					\
    else {								\
      int reloffset = pos - absoffset, written;				\
      absoffset = pos;							\
      do pos++; while (pos < bound && invalue != prevvalue);		\
                                                                        \
      written = write_crdr_replacement(out + opos, maxoutcount - opos,	\
        reloffset, repstart, pos - absoffset);				\
      if (written < 0) return written;					\
      absoffset = pos;							\
      opos += written;							\
    }									\
  }

static int compress_crdr(const pcl_Octet *in, int incount,
  const pcl_Octet *prev, int prevcount, pcl_Octet *out, int maxoutcount)
{
  int
    absoffset = 0,
    mincount = (incount < prevcount? incount: prevcount),
    opos = 0,
    pos = 0;

  /* Treat the special case of a zero output buffer (again, the bad case is
     merely the one where 'out' is NULL) */
  if (maxoutcount == 0) {
    if (incount == prevcount &&
      (incount == 0 || memcmp(in, prev, incount) == 0)) return 0;
    return -1;
  }

  crdr_loop(mincount, in[pos], prev[pos], in + absoffset);
  if (mincount < incount) {
    crdr_loop(incount, in[pos], 0, in + absoffset);
  }
  else {
    crdr_loop(prevcount, 0, prev[pos], NULL);
  }

  return opos;
}

#undef crdr_loop

/******************************************************************************

  Function: pcl_compress

  This function compresses an octet string using the compression algorithm
  specified by 'method'.

  The arguments 'in' and 'out' must be non-NULL. They point to the data to be
  compressed ('in->length' octets starting at 'in->str') and the area to which
  the compressed result should be written (at most 'out->length' octets
  starting at 'out->str'). If '*in' and '*out' are both non-zero (see
  definitions above), their storage areas ('in->str' to
  'in->str + in->length - 1' and 'out->str' to 'out->str + out->length - 1')
  should not overlap.

  If 'method' refers to a method entailing "vertical" compression, i.e.
  compression with respect to an octet string previously sent, 'prev' must
  point to this previous string. This is the case for methods 3 and 9.
  The variable is ignored otherwise and may be NULL. If it is needed, it should
  not overlap with the storage area of '*out'.

  The function returns a non-zero value in case of insufficient space in '*out'.
  In that situation, part of 'out->str' may have been overwritten already.
  Otherwise, a value of zero is returned and 'out->length' is set to the number
  of octets the compressed result occupies in 'out->str'.

******************************************************************************/

/* Test macro for an argument of type "pcl_OctetString *" */
#define is_valid(s)	\
  (s != NULL && ((s)->length == 0 || ((s)->length > 0 && (s)->str != NULL)))

int pcl_compress(pcl_Compression method, const pcl_OctetString *in,
  const pcl_OctetString *prev, pcl_OctetString *out)
{
  int result = -1;

  /* Prevent silly mistakes with the arguments */
  assert((is_valid(in) && is_valid(out) &&
          method != pcl_cm_delta && method != pcl_cm_crdr) || is_valid(prev));

  /* Treat zero-length case for the "purely horizontal" methods */
  if (in->length == 0 && method != pcl_cm_delta && method != pcl_cm_crdr) {
    out->length = 0;
    return 0;
  }

  switch (method) {
  case pcl_cm_none:	/* oh, well... */
    if (out->length <= in->length) {
      memcpy(out->str, in->str, in->length*sizeof(pcl_Octet));
      result = in->length;
    }
    break;
  case pcl_cm_rl:
    result = compress_runlength(in->str, in->length, out->str, out->length);
    break;
  case pcl_cm_tiff:
    result = compress_tiff(in->str, in->length, out->str, out->length);
    break;
  case pcl_cm_delta:
    result = compress_delta(in->str, in->length, prev->str, prev->length,
      out->str, out->length);
    break;
  case pcl_cm_crdr:
    result = compress_crdr(in->str, in->length, prev->str, prev->length,
      out->str, out->length);
    break;
  default:
    assert(0);	/* Illegal value for compression method */
    /* If NDEBUG is defined, we fall through with the default value for
       'result' which is -1, i.e., failure. */
  }

  /* Assign the length of the output octet string */
  if (result >= 0) {
    out->length = result;
    result = 0;
  }

  return result;
}
