#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_arith.h"

struct _Jbig2ArithState {
  uint32 C;
  int A;
  int I_CX;
  int MPS_CX;

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
	      printf ("read %02x (aa)\n", B);
#ifndef SOFTWARE_CONVENTION
	      as->C += 0xFF00;
#endif
	      as->CT = 8;
	      as->next_word = (0xFF00 | B1) << 16;
	      as->next_word_bytes = 2;
	    }
	  else
	    {
	      printf ("read %02x (a)\n", B);
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
	      printf ("read %02x (ba)\n", B);
#ifndef SOFTWARE_CONVENTION
	      as->C += 0xFF00;
#endif
	      as->CT = 8;
	    }
	  else
	    {
	      as->next_word_bytes--;
	      as->next_word <<= 8;
	      printf ("read %02x (b)\n", B);
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
      printf ("read %02x\n", B);
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
jbig2_arith_trace (Jbig2ArithState *as)
{
  printf ("I = %2d, A = %04x, CT = %2d, C = %08x\n",
	  as->I_CX, as->A, as->CT, as->C);
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

  /* note: these are merely defaults; it may be necessary to provide
     additional arguments to initialize these to non-default values. */
  result->I_CX = 0;
  result->MPS_CX = 0;

  return result;
};

/* could put bit fields in to minimize memory usage */
typedef struct {
  int Qe;
  int NMPS;
  int NLPS;
  bool SWITCH;
} Jbig2ArithQe;

const Jbig2ArithQe jbig2_arith_Qe[] = {
  { 0x5601, 1, 1, 1 },
  { 0x3401, 2, 6, 0 },
  { 0x1801, 3, 9, 0 },
  { 0x0AC1, 4, 12, 0 },
  { 0x0521, 5, 29, 0 },
  { 0x0221, 38, 33, 0 },
  { 0x5601, 7, 6, 1 },
  { 0x5401, 8, 14, 0 },
  { 0x4801, 9, 14, 0 },
  { 0x3801, 10, 14, 0 },
  { 0x3001, 11, 17, 0 },
  { 0x2401, 12, 18, 0 },
  { 0x1C01, 13, 20, 0 },
  { 0x1601, 29, 21, 0 },
  { 0x5601, 15, 14, 1 },
  { 0x5401, 16, 14, 0 },
  { 0x5101, 17, 15, 0 },
  { 0x4801, 18, 16, 0 },
  { 0x3801, 19, 17, 0 },
  { 0x3401, 20, 18, 0 },
  { 0x3001, 21, 19, 0 },
  { 0x2801, 22, 19, 0 },
  { 0x2401, 23, 20, 0 },
  { 0x2201, 24, 21, 0 },
  { 0x1C01, 25, 22, 0 },
  { 0x1801, 26, 23, 0 },
  { 0x1601, 27, 24, 0 },
  { 0x1401, 28, 25, 0 },
  { 0x1201, 29, 26, 0 },
  { 0x1101, 30, 27, 0 },
  { 0x0AC1, 31, 28, 0 },
  { 0x09C1, 32, 29, 0 },
  { 0x08A1, 33, 30, 0 },
  { 0x0521, 34, 31, 0 },
  { 0x0441, 35, 32, 0 },
  { 0x02A1, 36, 33, 0 },
  { 0x0221, 37, 34, 0 },
  { 0x0141, 38, 35, 0 },
  { 0x0111, 39, 36, 0 },
  { 0x0085, 40, 37, 0 },
  { 0x0049, 41, 38, 0 },
  { 0x0025, 42, 39, 0 },
  { 0x0015, 43, 40, 0 },
  { 0x0009, 44, 41, 0 },
  { 0x0005, 45, 42, 0 },
  { 0x0001, 45, 43, 0 },
  { 0x5601, 46, 46, 0 }
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
jbig2_arith_decode (Jbig2ArithState *as)
{
  const Jbig2ArithQe *pqe = &jbig2_arith_Qe[as->I_CX];
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
	      D = 1 - as->MPS_CX;
	      as->MPS_CX ^= pqe->SWITCH;
	      as->I_CX = pqe->NLPS;
	    }
	  else
	    {
	      D = as->MPS_CX;
	      as->I_CX = pqe->NMPS;
	    }
	  jbig2_arith_renormd (as);
	  return D;
	}
      else
	return as->MPS_CX;
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
	  D = as->MPS_CX;
	  as->I_CX = pqe->NMPS;
	}
      else
	{
	  as->A = pqe->Qe;
	  D = 1 - as->MPS_CX;
	  as->MPS_CX ^= pqe->SWITCH;
	  as->I_CX = pqe->NLPS;
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

  ws.get_next_word = test_get_word;
  as = jbig2_arith_new (&ws);
  jbig2_arith_trace (as);

  for (i = 0; i < 256; i++)
    {
      bool D;

      D = jbig2_arith_decode (as);
      printf ("%3d: D = %d, ", i, D);
      jbig2_arith_trace (as);
      
    }
  return 0;
}
#endif
