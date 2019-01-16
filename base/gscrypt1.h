/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Interface to Adobe Type 1 encryption/decryption. */

#ifndef gscrypt1_INCLUDED
#  define gscrypt1_INCLUDED

#include "stdpre.h"

/* Normal public interface */
typedef ushort crypt_state;
int gs_type1_encrypt(byte * dest, const byte * src, uint len,
                     crypt_state * pstate);
int gs_type1_decrypt(byte * dest, const byte * src, uint len,
                     crypt_state * pstate);

/* Define the encryption parameters and procedures. */
#define crypt_c1 ((ushort)52845)
#define crypt_c2 ((ushort)22719)
/* c1 * c1' == 1 mod 2^16. */
#define crypt_c1_inverse ((ushort)27493)
#define encrypt_next(ch, state, chvar)\
  (chvar = ((ch) ^ (state >> 8)),\
   state = (chvar + state) * crypt_c1 + crypt_c2)
#define decrypt_this(ch, state)\
  ((ch) ^ (state >> 8))
#define decrypt_next(ch, state, chvar)\
  (chvar = decrypt_this(ch, state),\
   decrypt_skip_next(ch, state))
#define decrypt_skip_next(ch, state)\
  (state = ((ch) + state) * crypt_c1 + crypt_c2)
#define decrypt_skip_previous(ch, state)\
  (state = (state - crypt_c2) * crypt_c1_inverse - (ch))

#endif /* gscrypt1_INCLUDED */
