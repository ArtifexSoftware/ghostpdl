/* Copyright (C) 2001-2023 Artifex Software, Inc.
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

#include "memory_.h"
#include "ghost.h"
#include "oper.h"
#include "dstack.h"	/* for systemdict */
#include "estack.h"
#include "ialloc.h"
#include "iutil.h"
#include "idict.h"
#include "iname.h"
#include "string_.h"	/* memcmp() */
#include "store.h"
#include "aes.h"
#include "sha2.h"

/* Implementation of the PDF security handler revision6 (PDF 1.7 ExtensionLevel 8 algorithm)
 *
 * Adobe/ISO has not yet released the details, so the algorithm reference is:
 * http://esec-lab.sogeti.com/post/The-undocumented-password-validation-algorithm-of-Adobe-Reader-X
 *
 * The code below is the same as (and copied from) the MuPDF implementation.
 */

static void
pdf_compute_hardened_hash_r6(unsigned char *password, int pwlen, unsigned char salt[16], unsigned char *ownerkey, unsigned char hash[32])
{
	unsigned char data[(128 + 64 + 48) * 64];
	unsigned char block[64];
	int block_size = 32;
	int data_len = 0;
	int i, j, sum;

    SHA256_CTX sha256;
    SHA384_CTX sha384;
    SHA512_CTX sha512;
    aes_context aes;

    pSHA256_Init(&sha256);
    pSHA256_Update(&sha256, password, pwlen);
    pSHA256_Update(&sha256, salt, 8);
    if (ownerkey)
        pSHA256_Update(&sha256, ownerkey, 48);
    pSHA256_Final((uint8_t *)block, &sha256);

	for (i = 0; i < 64 || i < data[data_len * 64 - 1] + 32; i++)
	{
		/* Step 2: repeat password and data block 64 times */
		memcpy(data, password, pwlen);
		memcpy(data + pwlen, block, block_size);
		if (ownerkey)
			memcpy(data + pwlen + block_size, ownerkey, 48);
		data_len = pwlen + block_size + (ownerkey ? 48 : 0);
		for (j = 1; j < 64; j++)
			memcpy(data + j * data_len, data, data_len);

		/* Step 3: encrypt data using data block as key and iv */
		aes_setkey_enc(&aes, block, 128);
		aes_crypt_cbc(&aes, AES_ENCRYPT, data_len * 64, block + 16, data, data);

		/* Step 4: determine SHA-2 hash size for this round */
		for (j = 0, sum = 0; j < 16; j++)
			sum += data[j];

		/* Step 5: calculate data block for next round */
		block_size = 32 + (sum % 3) * 16;
		switch (block_size)
		{
        case 32:
            pSHA256_Init(&sha256);
            pSHA256_Update(&sha256, data, data_len * 64);
            pSHA256_Final((uint8_t *)block, &sha256);
            break;
        case 48:
            pSHA384_Init(&sha384);
            pSHA384_Update(&sha384, data, data_len * 64);
            pSHA384_Final((uint8_t *)block, &sha384);
            break;
        case 64:
            pSHA512_Init(&sha512);
            pSHA512_Update(&sha512, data, data_len * 64);
            pSHA512_Final((uint8_t *)block, &sha512);
            break;
		}
	}

	memset(data, 0, sizeof(data));
	memcpy(hash, block, 32);
}

static void
pdf_compute_encryption_key_r6(unsigned char *password, int pwlen, unsigned char *O, unsigned char *OE, unsigned char *U, unsigned char *UE, int ownerkey, unsigned char *validationkey, unsigned char *output)
{
	unsigned char hash[32];
	unsigned char iv[16];
    aes_context aes;

	if (pwlen > 127)
		pwlen = 127;

	pdf_compute_hardened_hash_r6(password, pwlen,
		(ownerkey ? O : U) + 32,
		ownerkey ? U : NULL, validationkey);
	pdf_compute_hardened_hash_r6(password, pwlen,
        (ownerkey ? O : U) + 40,
        (ownerkey ? U : NULL), hash);

	memset(iv, 0, sizeof(iv));
    aes_setkey_dec(&aes, hash, 256);
	aes_crypt_cbc(&aes, AES_DECRYPT, 32, iv,
		ownerkey ? OE : UE, output);
}

/* (password) <encryption dict> check_r6_password (key) true|false */
static int
zcheck_r6_password(i_ctx_t * i_ctx_p)
{
    os_ptr  op = osp;
    ref *CryptDict, *Oref, *OEref, *Uref, *UEref, *Pref;
    int code, PWlen;
	unsigned char validation[32];
	unsigned char output[32];
    ref stref;
    byte *body;

    check_op(2);

    CryptDict = op--;
    Pref = op;
    if (!r_has_type(CryptDict, t_dictionary))
        return_error(gs_error_typecheck);
    if (!r_has_type(Pref, t_string))
        return_error(gs_error_typecheck);

    code = dict_find_string(CryptDict, "O", &Oref);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_undefined);
    if (!r_has_type(Oref, t_string))
      return_error(gs_error_typecheck);
    if (r_size(Oref) < 48)
        return_error(gs_error_invalidaccess);

    code = dict_find_string(CryptDict, "OE", &OEref);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_undefined);
    if (!r_has_type(OEref, t_string))
      return_error(gs_error_typecheck);
    if (r_size(OEref) < 32)
        return_error(gs_error_invalidaccess);

    code = dict_find_string(CryptDict, "U", &Uref);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_undefined);
    if (!r_has_type(Uref, t_string))
      return_error(gs_error_typecheck);
    if (r_size(Uref) < 48)
        return_error(gs_error_invalidaccess);

    code = dict_find_string(CryptDict, "UE", &UEref);
    if (code < 0)
        return code;
    if (code == 0)
        return_error(gs_error_undefined);
    if (!r_has_type(UEref, t_string))
      return_error(gs_error_typecheck);
    if (r_size(UEref) < 32)
        return_error(gs_error_invalidaccess);

    ref_stack_pop(&o_stack, 2);
    op = osp;

    PWlen = r_size(Pref);

    /* First, try the password as the user password */
    pdf_compute_encryption_key_r6((unsigned char *)Pref->value.const_bytes, PWlen, (unsigned char *)Oref->value.const_bytes,
        (unsigned char *)OEref->value.const_bytes, (unsigned char *)Uref->value.const_bytes, (unsigned char *)UEref->value.const_bytes, 0, validation, output);

    if (memcmp(validation, Uref->value.const_bytes, 32) != 0){
        /* It wasn't the user password, maybe its the owner password */
        pdf_compute_encryption_key_r6((unsigned char *)Pref->value.const_bytes, PWlen, (unsigned char *)Oref->value.const_bytes,
            (unsigned char *)OEref->value.const_bytes, (unsigned char *)Uref->value.const_bytes, (unsigned char *)UEref->value.const_bytes, 1, validation, output);

        if (memcmp(validation, Oref->value.const_bytes, 32) != 0){
            /* Doesn't seem to be a valid password.... */
            push(1);
            make_bool(op, 0);
            return 0;
        }
    }

    body = ialloc_string(32, "r6 encryption key");
    if (body == 0)
        return_error(gs_error_VMerror);
    push(1);
    memcpy(body, output, 32);
    make_string(&stref, a_all | icurrent_space, 32, body);
    ref_assign(op, &stref);
    push(1);
    make_bool(op, 1);

    return 0;
}

const op_def zpdf_r6_op_defs[] =
{
    { "2check_r6_password", zcheck_r6_password },
    op_def_end(0)
};
