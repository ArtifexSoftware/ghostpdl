/* Copyright (C) 2022-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

#ifndef PARAM1
#define PARAMCAT(A,B) A ## B
#define PARAM1(A) PARAMCAT(TOKEN_, A),
#define PARAM2(A,B) PARAMCAT(TOKEN_, B),
#endif

// Errors
PARAM2("", NOT_A_KEYWORD)
// Insert new error tokens here.
PARAM2("", TOO_LONG)
PARAM2("", INVALID_KEY)
// Real tokens start here, at TOKEN_INVALID_KEY+1
PARAM2("\"", QUOTE)
PARAM2("'", APOSTROPHE)
PARAM1(B)
PARAM2("B*", Bstar)
PARAM1(BDC)
PARAM1(BI)
PARAM1(BMC)
PARAM1(BT)
PARAM1(BX)
PARAM1(CS)
PARAM1(DP)
PARAM1(Do)
PARAM1(EI)
PARAM1(EMC)
PARAM1(ET)
PARAM1(EX)
PARAM1(F)
PARAM1(G)
PARAM1(ID)
PARAM1(J)
PARAM1(K)
PARAM1(M)
PARAM1(MP)
PARAM1(Q)
PARAM1(R)
PARAM1(RG)
PARAM1(S)
PARAM1(SC)
PARAM1(SCN)
PARAM2("T*", Tstar)
PARAM1(TD)
PARAM1(TJ)
PARAM1(TL)
PARAM1(Tc)
PARAM1(Td)
PARAM1(Tf)
PARAM1(Tj)
PARAM1(Tm)
PARAM1(Tr)
PARAM1(Ts)
PARAM1(Tw)
PARAM1(Tz)
PARAM1(W)
PARAM2("W*", Wstar)
PARAM1(b)
PARAM2("b*", bstar)
PARAM1(c)
PARAM1(cm)
PARAM1(cs)
PARAM1(d)
PARAM1(d0)
PARAM1(d1)
PARAM2("endobj", ENDOBJ)
PARAM2("endstream", ENDSTREAM)
PARAM1(f)
PARAM2("f*", fstar)
PARAM2("false", PDF_FALSE)
PARAM1(g)
PARAM1(gs)
PARAM1(h)
PARAM1(i)
PARAM1(j)
PARAM1(k)
PARAM1(l)
PARAM1(m)
PARAM1(n)
PARAM1(null)
PARAM2("obj", OBJ)
PARAM1(q)
PARAM1(r)
PARAM1(re)
PARAM1(rg)
PARAM1(ri)
PARAM1(s)
PARAM1(sc)
PARAM1(scn)
PARAM1(sh)
PARAM2("startxref", STARTXREF)
PARAM2("stream", STREAM)
PARAM2("trailer", TRAILER)
PARAM2("true", PDF_TRUE)
PARAM1(v)
PARAM1(w)
PARAM2("xref", XREF)
PARAM1(y)

#undef PARAM1
#undef PARAM2
#undef PARAMCAT
