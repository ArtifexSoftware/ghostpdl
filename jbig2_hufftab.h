/*
    jbig2dec
    
    Copyright (c) 2001 artofcode LLC.
    
    This software is distributed under license and may not
    be copied, modified or distributed except as expressly
    authorized under the terms of the license contained in
    the file LICENSE in this distribution.
                                                                                
    For information on commercial licensing, go to
    http://www.artifex.com/licensing/ or contact
    Artifex Software, Inc.,  101 Lucas Valley Road #110,
    San Rafael, CA  94903, U.S.A., +1(415)492-9861.
    
    $Id$
*/

/* predefined Huffman table definitions 
    -- See Annex B of the JBIG2 draft spec */

#ifndef JBIG2_HUFFTAB_H
#define JBIG2_HUFFTAB_H

/* types are in jbig2_huffman.h, you must include that first */

struct _Jbig2HuffmanEntry {
  union {
    int32_t RANGELOW;
    Jbig2HuffmanTable *ext_table;
  } u;
  byte PREFLEN;
  byte RANGELEN;
  byte flags;
};

struct _Jbig2HuffmanTable {
  int log_table_size;
  Jbig2HuffmanEntry *entries;
};

typedef struct _Jbig2HuffmanLine Jbig2HuffmanLine;

struct _Jbig2HuffmanLine {
  int PREFLEN;
  int RANGELEN;
  int RANGELOW;
};

struct _Jbig2HuffmanParams {
  bool HTOOB;
  int n_lines;
  const Jbig2HuffmanLine *lines;
};

/* Table B.1 */
const Jbig2HuffmanLine
jbig_huffman_lines_A[] = {
  { 1, 4, 0 },
  { 2, 8, 16 },
  { 3, 16, 272 },
  { 0, 32, -1 },   /* low */
  { 3, 32, 65808 } /* high */
};

const Jbig2HuffmanParams
jbig_huffman_params_A = { FALSE, 5, jbig_huffman_lines_A };

/* Table B.2 */
const Jbig2HuffmanLine
jbig_huffman_lines_B[] = {
  { 1, 0, 0 },
  { 2, 0, 1 },
  { 3, 0, 2 },
  { 4, 3, 3 },
  { 5, 6, 11 },
  { 0, 32, -1 }, /* low */
  { 6, 32, 75 }, /* high */
  { 6, 0, 0 }
};

const Jbig2HuffmanParams
jbig_huffman_params_B = { TRUE, 8, jbig_huffman_lines_B };

/* Table B.3 */
const Jbig2HuffmanLine
jbig_huffman_lines_C[] = {
  { 8, 8, -256 },
  { 1, 0, 0 },
  { 2, 0, 1 },
  { 3, 0, 2 },
  { 4, 3, 3 },
  { 5, 6, 11 },
  { 8, 32, -257 }, /* low */
  { 7, 32, 75 },   /* high */
  { 6, 0, 0 } /* OOB */
};

const Jbig2HuffmanParams
jbig_huffman_params_C = { TRUE, 9, jbig_huffman_lines_C };

/* Table B.4 */
const Jbig2HuffmanLine
jbig_huffman_lines_D[] = {
  { 1, 0, 1 },
  { 2, 0, 2 },
  { 3, 0, 3 },
  { 4, 3, 4 },
  { 5, 6, 12 },
  { 0, 32, -1 }, /* low */
  { 5, 32, 76 }, /* high */
};

const Jbig2HuffmanParams
jbig_huffman_params_D = { FALSE, 7, jbig_huffman_lines_D };

/* Table B.5 */
const Jbig2HuffmanLine
jbig_huffman_lines_E[] = {
	{7, 8, -255},
	{1, 0, 1},
	{2, 0, 2},
	{3, 0, 3},
	{4, 3, 4},
	{5, 6, 12},
	{7, 32, -256},
	{6, 32, 76}
};

const Jbig2HuffmanParams
jbig_huffman_params_E = { FALSE, 8, jbig_huffman_lines_E };

/* Table B.14 */
const Jbig2HuffmanLine
jbig_huffman_lines_N[] = {
  { 3, 0, -2 },
  { 3, 0, -1 },
  { 1, 0, 0 },
  { 3, 3, 1 },
  { 3, 6, 2 },
  { 0, 32, -1 }, /* low */
  { 0, 32, 3 }, /* high */
};

const Jbig2HuffmanParams
jbig_huffman_params_N = { FALSE, 7, jbig_huffman_lines_N };


#endif /* JBIG2_HUFFTAB_H */
