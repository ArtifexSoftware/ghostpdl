/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */

/*$RCSfile$ $Revision$ */
/* Interface to Adobe Type 1 encryption/decryption. */

#ifndef gscrypt1_INCLUDED
#  define gscrypt1_INCLUDED

/* Normal public interface */
typedef ushort crypt_state;
int gs_type1_encrypt(P4(byte * dest, const byte * src, uint len,
			crypt_state * pstate));
int gs_type1_decrypt(P4(byte * dest, const byte * src, uint len,
			crypt_state * pstate));

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
