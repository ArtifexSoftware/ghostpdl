#include <stdio.h>
#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_arith.h"

#ifdef DEBUG
#include <stdio.h>
#endif

struct _Jbig2ArithState {
  uint32 C;
  int A;

  int CT;

  uint32 next_word;
  int next_word_bytes;

  Jbig2WordStream *ws;
  int offset;
};

#define SOFTWARE_CONVENTION

static void
jbig2_arith_bytein (Jbig2ArithState *as)
{
  byte B;

  if (as->next_word_bytes == 0)
    {
      Jbig2WordStream *ws = as->ws;
      as->next_word = ws->get_next_word (ws, as->offset);
      as->offset += 4;
      as->next_word_bytes = 4;
    }
  /* Figure F.3 */
  B = (as->next_word >> 24) & 0xFF;
  if (B == 0xFF)
    {
      byte B1;
      if (as->next_word_bytes == 1)
	{
	  Jbig2WordStream *ws = as->ws;
	  as->next_word = ws->get_next_word (ws, as->offset);
	  as->offset += 4;
	  B1 = (as->next_word >> 24) & 0xFF;
	  if (B1 > 0x8F)
	    {
#ifdef DEBUG
	      printf ("read %02x (aa)\n", B);
#endif
#ifndef SOFTWARE_CONVENTION
	      as->C += 0xFF00;
#endif
	      as->CT = 8;
	      as->next_word = (0xFF00 | B1) << 16;
	      as->next_word_bytes = 2;
	    }
	  else
	    {
#ifdef DEBUG
	      printf ("read %02x (a)\n", B);
#endif
	      /* Note: the spec calls for adding (or subtracting) B <<
		 9 here. However, to be consistent with the sample run
		 in Annex H.2, we use the B << 8 instead. */
#ifdef SOFTWARE_CONVENTION
	      as->C += 0xFE00 - (B << 8);
#else
	      as->C += 0xFF00;
#endif
	      as->CT = 7;
	      as->next_word_bytes = 4;
	    }
	}
      else
	{
	  B1 = (as->next_word >> 16) & 0xFF;
	  if (B1 > 0x8F)
	    {
#ifdef DEBUG
	      printf ("read %02x (ba)\n", B);
#endif
#ifndef SOFTWARE_CONVENTION
	      as->C += 0xFF00;
#endif
	      as->CT = 8;
	    }
	  else
	    {
	      as->next_word_bytes--;
	      as->next_word <<= 8;
#ifdef DEBUG
	      printf ("read %02x (b)\n", B);
#endif
	      /* Note: the spec calls for adding (or subtracting) B <<
		 9 here. However, to be consistent with the sample run
		 in Annex H.2, we use the B << 8 instead. */
#ifdef SOFTWARE_CONVENTION
	      as->C += 0xFE00 - (B << 8);
#else
	      as->C += 0xFF00;
#endif
	      as->CT = 7;
	    }
	}
    }
  else
    {
#ifdef DEBUG
      printf ("read %02x\n", B);
#endif
#ifdef SOFTWARE_CONVENTION
      as->C += 0xFF00 - (B << 8);
#else
      as->C += (B << 8);
#endif
      as->CT = 8;
      as->next_word <<= 8;
      as->next_word_bytes--;
    }
}

#ifdef DEBUG
#include <stdio.h>

static void
jbig2_arith_trace (Jbig2ArithState *as, Jbig2ArithCx cx)
{
  printf ("I = %2d, MPS = %d, A = %04x, CT = %2d, C = %08x\n",
	  cx & 0x7f, cx >> 7, as->A, as->CT, as->C);
}
#endif

Jbig2ArithState *
jbig2_arith_new (Jbig2WordStream *ws)
{
  Jbig2ArithState *result = (Jbig2ArithState *)malloc (sizeof(Jbig2ArithState));

  result->ws = ws;

  result->next_word = ws->get_next_word (ws, 0);
  result->next_word_bytes = 4;
  result->offset = 4;

  /* Figure F.1 */
#ifdef SOFTWARE_CONVENTION
  result->C = (~(result->next_word >> 8)) & 0xFF0000;
#else
  result->C = (result->next_word >> 8) & 0xFF0000;
#endif
  result->next_word <<= 8;
  result->next_word_bytes--;

  jbig2_arith_bytein (result);
  result->C <<= 7;
  result->CT -= 7;
  result->A = 0x8000;

  return result;
};

/* could put bit fields in to minimize memory usage */
typedef struct {
  unsigned short Qe;
  byte mps_xor; /* mps_xor = index ^ NMPS */
  byte lps_xor; /* lps_xor = index ^ NLPS ^ (SWITCH << 7) */
} Jbig2ArithQe;

const Jbig2ArithQe jbig2_arith_Qe[] = {
  { 0x5601,  1 ^  0,  1 ^  0 ^ 0x80 },
  { 0x3401,  2 ^  1,  6 ^  1 },
  { 0x1801,  3 ^  2,  9 ^  2 },
  { 0x0AC1,  4 ^  3, 12 ^  3 },
  { 0x0521,  5 ^  4, 29 ^  4 },
  { 0x0221, 38 ^  5, 33 ^  5 },
  { 0x5601,  7 ^  6,  6 ^  6 ^ 0x80 },
  { 0x5401,  8 ^  7, 14 ^  7 },
  { 0x4801,  9 ^  8, 14 ^  8 },
  { 0x3801, 10 ^  9, 14 ^  9 },
  { 0x3001, 11 ^ 10, 17 ^ 10 },
  { 0x2401, 12 ^ 11, 18 ^ 11 },
  { 0x1C01, 13 ^ 12, 20 ^ 12 },
  { 0x1601, 29 ^ 13, 21 ^ 13 },
  { 0x5601, 15 ^ 14, 14 ^ 14 ^ 0x80 },
  { 0x5401, 16 ^ 15, 14 ^ 15 },
  { 0x5101, 17 ^ 16, 15 ^ 16 },
  { 0x4801, 18 ^ 17, 16 ^ 17 },
  { 0x3801, 19 ^ 18, 17 ^ 18 },
  { 0x3401, 20 ^ 19, 18 ^ 19 },
  { 0x3001, 21 ^ 20, 19 ^ 20 },
  { 0x2801, 22 ^ 21, 19 ^ 21 },
  { 0x2401, 23 ^ 22, 20 ^ 22 },
  { 0x2201, 24 ^ 23, 21 ^ 23 },
  { 0x1C01, 25 ^ 24, 22 ^ 24 },
  { 0x1801, 26 ^ 25, 23 ^ 25 },
  { 0x1601, 27 ^ 26, 24 ^ 26 },
  { 0x1401, 28 ^ 27, 25 ^ 27 },
  { 0x1201, 29 ^ 28, 26 ^ 28 },
  { 0x1101, 30 ^ 29, 27 ^ 29 },
  { 0x0AC1, 31 ^ 30, 28 ^ 30 },
  { 0x09C1, 32 ^ 31, 29 ^ 31 },
  { 0x08A1, 33 ^ 32, 30 ^ 32 },
  { 0x0521, 34 ^ 33, 31 ^ 33 },
  { 0x0441, 35 ^ 34, 32 ^ 34 },
  { 0x02A1, 36 ^ 35, 33 ^ 35 },
  { 0x0221, 37 ^ 36, 34 ^ 36 },
  { 0x0141, 38 ^ 37, 35 ^ 37 },
  { 0x0111, 39 ^ 38, 36 ^ 38 },
  { 0x0085, 40 ^ 39, 37 ^ 39 },
  { 0x0049, 41 ^ 40, 38 ^ 40 },
  { 0x0025, 42 ^ 41, 39 ^ 41 },
  { 0x0015, 43 ^ 42, 40 ^ 42 },
  { 0x0009, 44 ^ 43, 41 ^ 43 },
  { 0x0005, 45 ^ 44, 42 ^ 44 },
  { 0x0001, 45 ^ 45, 43 ^ 45 },
  { 0x5601, 46 ^ 46, 46 ^ 46 }
};

static void
jbig2_arith_renormd (Jbig2ArithState *as)
{
  /* Figure E.18 */
  do
    {
      if (as->CT == 0)
	jbig2_arith_bytein (as);
      as->A <<= 1;
      as->C <<= 1;
      as->CT--;
    }
  while ((as->A & 0x8000) == 0);
}

bool
jbig2_arith_decode (Jbig2ArithState *as, Jbig2ArithCx *pcx)
{
  Jbig2ArithCx cx = *pcx;
  const Jbig2ArithQe *pqe = &jbig2_arith_Qe[cx & 0x7f];
  bool D;

  /* Figure F.2 */
  as->A -= pqe->Qe;
  if (
#ifdef SOFTWARE_CONVENTION
      (as->C) >> 16 < as->A
#else
      !((as->C >> 16) < pqe->Qe)
#endif
      )
    {
#ifndef SOFTWARE_CONVENTION
      as->C -= pqe->Qe << 16;
#endif
      if ((as->A & 0x8000) == 0)
	{
	  /* MPS_EXCHANGE, Figure E.16 */
	  if (as->A < pqe->Qe)
	    {
	      D = 1 - (cx >> 7);
	      *pcx ^= pqe->lps_xor;
	    }
	  else
	    {
	      D = cx >> 7;
	      *pcx ^= pqe->mps_xor;
	    }
	  jbig2_arith_renormd (as);
	  return D;
	}
      else
	return cx >> 7;
    }
  else
    {
#ifdef SOFTWARE_CONVENTION
      as->C -= (as->A) << 16;
#endif
      /* LPS_EXCHANGE, Figure E.17 */
      if (as->A < pqe->Qe)
	{
	  as->A = pqe->Qe;
	  D = cx >> 7;
	  *pcx ^= pqe->mps_xor;
	}
      else
	{
	  as->A = pqe->Qe;
	  D = 1 - (cx >> 7);
	  *pcx ^= pqe->lps_xor;
	}
      jbig2_arith_renormd (as);
      return D;
    }
}

#ifdef TEST

static int32
test_get_word (Jbig2WordStream *self, int offset)
{
  byte stream[] = {
    0x84, 0xC7, 0x3B, 0xFC, 0xE1, 0xA1, 0x43, 0x04, 0x02, 0x20, 0x00, 0x00,
    0x41, 0x0D, 0xBB, 0x86, 0xF4, 0x31, 0x7F, 0xFF, 0x88, 0xFF, 0x37, 0x47,
    0x1A, 0xDB, 0x6A, 0xDF, 0xFF, 0xAC,
    0x00, 0x00
  };
  if (offset >= sizeof(stream))
    return 0;
  else
    return (stream[offset] << 24) | (stream[offset + 1] << 16) |
      (stream[offset + 2] << 8) | stream[offset + 3];
}

int
main (int argc, char **argv)
{
  Jbig2WordStream ws;
  Jbig2ArithState *as;
  int i;
  Jbig2ArithCx cx = 0;

  ws.get_next_word = test_get_word;
  as = jbig2_arith_new (&ws);
  jbig2_arith_trace (as, cx);

  for (i = 0; i < 256; i++)
    {
      bool D;

      D = jbig2_arith_decode (as, &cx);
      printf ("%3d: D = %d, ", i, D);
      jbig2_arith_trace (as, cx);
      
    }
  return 0;
}
#endif
