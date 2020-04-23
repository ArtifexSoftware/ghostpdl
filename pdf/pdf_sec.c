/* Copyright (C) 2001-2020 Artifex Software, Inc.
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

/* PDF decryption routines */

#include "pdf_stack.h"
#include "pdf_file.h"
#include "pdf_dict.h"
#include "pdf_array.h"
#include "pdf_sec.h"
#include "pdf_misc.h"
#include "strmio.h"
#include "smd5.h"
#include "sarc4.h"
#include "aes.h"
#include "sha2.h"
#include "pdf_utf8.h"

/* The padding string as defined in step 1 of Algorithm 3.2 */
static char PadString[32] = {
    0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41, 0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
    0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80, 0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

/* If EncryptMetadata is true we need to add 4 bytes of 0xFF to the MD5 hash
 * when computing an encryption key (Algorithm 3.2, step 6 when R is 4 or more)
 */
static char R4String[4] = {
    0xFF, 0xFF, 0xFF, 0xFF
};

/* If using the AES filter, we need to add this to the encryption
 * key when creating the key for decrypting objects (streams or strings)
 */
static char sAlTString[4] = {
    0x73, 0x41, 0x6c, 0x54
};

static int pdf_compute_encryption_key_preR5(pdf_context *ctx, char *Password, int PasswordLen, int KeyLen, pdf_string **EKey, int R)
{
    char Key[32];
    int code = 0, KeyLenBytes = KeyLen / 8, i;
    char P[4];
    gs_md5_state_t md5;
    pdf_array *a = NULL;
    pdf_string *s = NULL;

    *EKey = NULL;
    /* Algorithm 3.2 */
    /* Step 1. Pad or truncate the password string to exactly 32 bytes
     * using the defined padding string.
     */
    if (PasswordLen > 32)
        PasswordLen = 32;

    memcpy(Key, Password, PasswordLen);

    if (PasswordLen < 32)
        memcpy(&Key[PasswordLen], PadString, 32 - PasswordLen);

    /* 2. Initialise the MD5 hash function and pass the result of step 1 to this function */
    gs_md5_init(&md5);
    gs_md5_append(&md5, (gs_md5_byte_t *)&Key, 32);

    /* 3. Pass the value of the encryption dictionary's O entry to the MD5 hash */
    gs_md5_append(&md5, (gs_md5_byte_t *)ctx->O, 32);

    /* 4. Treat the value of P as an unsigned 4 byte integer and pass those bytes to the MD5 */
    P[3] = ((uint32_t)ctx->P) >> 24;
    P[2] = ((uint32_t)ctx->P & 0x00ff0000) >> 16;
    P[1] = ((uint32_t)ctx->P & 0x0000ff00) >> 8;
    P[0] = ((uint32_t)ctx->P & 0xff);
    gs_md5_append(&md5, (gs_md5_byte_t *)P, 4);

    /* 5. Pass the first element of the file's file identifier array */
    code = pdfi_dict_get_type(ctx, ctx->Trailer, "ID", PDF_ARRAY, (pdf_obj **)&a);
    if (code < 0) {
        if (code == gs_error_undefined) {
            emprintf(ctx->memory, "   **** Error: ID key in the trailer is required for encrypted files.\n");
            emprintf(ctx->memory, "               File may not be possible to decrypt.\n");
        }
        return code;
    }
    code = pdfi_array_get_type(ctx, a, (uint64_t)0, PDF_STRING, (pdf_obj **)&s);
    if (code < 0)
        goto done;
    gs_md5_append(&md5, s->data, s->length);

    /* Step 6
     * (revision 4 or greater) If document Metadata is not being encrypted
     * pass 4 bytes with the value 0xFFFFFFFF to the MD5 hash function.
     */
    if (R > 3 && !ctx->EncryptMetadata) {
        gs_md5_append(&md5, (const gs_md5_byte_t *)R4String, 4);
    }

    /* 7. Finish the hash */
    gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);

    code = pdfi_alloc_object(ctx, PDF_STRING, KeyLenBytes, (pdf_obj **)EKey);
    if (code < 0)
        goto done;
    pdfi_countup((pdf_obj *)*EKey);

    /* Step 8
     * (revision 3 or greater) Do the following 50 times. Take the output from the
     * previous MD5 hash and pass hte first n bytes of the output as input to a new
     * MD5 hash, where n is the number of bytes of the encryption key as defined by
     * the value of the encryption dictionary's Length entry. (NB Length is in bits)
     */
    if (R > 2) {
        for (i=0;i < 50; i++) {
            memcpy((*EKey)->data, Key, KeyLenBytes);
            gs_md5_init(&md5);
            gs_md5_append(&md5, (gs_md5_byte_t *)(*EKey)->data, KeyLenBytes);
            gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);
        }
    }

    /* Step 9
     * Set the encryption key to the first n bytes of the output from the final MD5 hash
     * where n is always 5 for revision 2 but, for revision 3 or greater, depends on the
     * value of the encryption dictionary's Length entry.
     */
    memcpy((*EKey)->data, Key, KeyLenBytes);

done:
    pdfi_countdown(s);
    pdfi_countdown(a);
    return code;
}

#ifdef HAVE_LIBIDN
static int apply_sasl(pdf_context *ctx, char *Password, int Len, char **NewPassword, int NewLen)
{
    byte *buffer;
    uint buffer_size;
    uint output_size;
    Stringprep_rc err;

    buffer_size = Len * 11 + 1;
    buffer = (byte *)gs_alloc_bytes(ctx->memory, buffer_size, "saslprep result");
    if (buffer == NULL)
        return_error(gs_error_VMerror);

    err = stringprep((char *)buffer, buffer_size, 0, stringprep_saslprep);
    if (err != STRINGPREP_OK) {
        gs_free_object(ctx->memory, buffer, "saslprep result");

        /* Since we're just verifying the password to an existing
         * document here, we don't care about "invalid input" errors
         * like STRINGPREP_CONTAINS_PROHIBITED.  In these cases, we
         * ignore the error and return the original string unchanged --
         * chances are it's not the right password anyway, and if it
         * is we shouldn't gratuitously fail to decrypt the document.
         *
         * On the other hand, errors like STRINGPREP_NFKC_FAILED are
         * real errors, and should be returned to the user.
         *
         * Fortunately, the stringprep error codes are sorted to make
         * this easy: the errors we want to ignore are the ones with
         * codes less than 100. */
        if ((int)err < 100)
            return 0;

        return_error(gs_error_ioerror);
    }

    NewLen = strlen((char *)buffer);
    *NewPassword = buffer;

    return 0;

}
#endif

static int check_user_password_R5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    char *UTF8_Password, *Test = NULL, Buffer[32], UEPadded[48];
    int NewLen;
    int code = 0;
    pdf_stream *stream = NULL, *filter_stream = NULL;
    pdf_string *Key = NULL;

    /* Algorithm 3.11 from the Adobe extensions to the ISO 32000 specification (Extension Level 3) */
    /* Step 1 convert the password to UTF-8 */

    /* From the original code in Resource/Init/pdf_sec.ps:
     * Step 1.
     * If the .saslprep operator isn't available (because ghostscript
     * wasn't built with libidn support), just skip this step.  ASCII
     * passwords will still work fine, and even most non-ASCII passwords
     * will be okay; any non-ASCII passwords that fail will produce a
     * warning from pdf_process_Encrypt.
     */
#ifdef HAVE_LIBIDN
    code = apply_sasl(ctx, Password, Len, &UTF8_Password, &NewLen);
    if (code < 0)
        return code;
#else
    UTF8_Password = Password;
    NewLen = Len;
#endif
    if (NewLen > 127)
        NewLen = 127;

    Test = (char *)gs_alloc_bytes(ctx->memory, NewLen + 8, "R5 password test");
    if (Test == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    /* Try to validate the password as the user password */
    /* concatenate the password */
    memcpy(Test, UTF8_Password, NewLen);
    /* With the 'User Validation Salt' (stored as part of the /O string */
    memcpy(&Test[NewLen], &ctx->U[32], 8);

    /* Now calculate the SHA256 hash */
    code = pdfi_open_memory_stream_from_memory(ctx, NewLen + 8, (byte *)Test, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_SHA256_filter(ctx, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    if (memcmp(Buffer, ctx->U, 32) != 0) {
        code = gs_note_error(gs_error_unknownerror);
        goto error;
    }

    /* Password matched */
    /* Finally calculate the decryption key */
    gs_free_object(ctx->memory, Test, "R5 password test");

    /* Password + last 8 bytes of /U */
    Test = (char *)gs_alloc_bytes(ctx->memory, NewLen + 8, "R5 password test");
    if (Test == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    memcpy(Test, UTF8_Password, NewLen);
    /* The 'User Key Salt' (stored as part of the /O string */
    memcpy(&Test[NewLen], &ctx->U[40], 8);

    /* Now calculate the SHA256 hash */
    code = pdfi_open_memory_stream_from_memory(ctx, NewLen + 8, (byte *)Test, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_SHA256_filter(ctx, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    memset(UEPadded, 0x00, 16);
    memcpy(&UEPadded[16], ctx->UE, 32);

    code = pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&Key);
    if (code < 0)
        goto error;
    /* pdfi_alloc_object() creates objects with a refrence count of 0 */
    pdfi_countup(Key);
    memcpy(Key->data, Buffer, 32);

    /* Now apply AESDecode to the padded UE string, using the SHA from above as the key */
    code = pdfi_open_memory_stream_from_memory(ctx, 48, (byte *)UEPadded, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_AES_filter(ctx, Key, false, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);
    pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&ctx->EKey);
    if (ctx->EKey == NULL)
        goto error;
    memcpy(ctx->EKey->data, Buffer, 32);
    pdfi_countup(ctx->EKey);

error:
    pdfi_countdown(Key);
    gs_free_object(ctx->memory, Test, "R5 password test");
#ifdef HAVE_LIBIDN
    gs_free_object(ctx->memory, UTF8_Password, "free sasl result");
#endif
    return code;
}

/* Implementation of the PDF security handler revision6 (PDF 1.7 ExtensionLevel 8 algorithm)
 *
 * Adobe/ISO has not yet released the details, so the algorithm reference is:
 * http://esec-lab.sogeti.com/post/The-undocumented-password-validation-algorithm-of-Adobe-Reader-X
 *
 * The code below is the same as (and copied from) the MuPDF implementation. And copied from the
 * Ghostscript implementation in zpdf_r6.c. The ISO specification is now released and the algorithms
 * used here are documented in the PDF 2.0 specification ISO 32000-2:2017
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

static int check_user_password_R6(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
	unsigned char validation[32];
	unsigned char output[32];

    pdf_compute_encryption_key_r6((unsigned char *)Password, Len, (unsigned char *)ctx->O, (unsigned char *)ctx->OE,
                                  (unsigned char *)ctx->U, (unsigned char *)ctx->UE, 0, validation, output);

    if (memcmp(validation, ctx->U, 32) != 0)
        return_error(gs_error_unknownerror);

    pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&ctx->EKey);
    if (ctx->EKey == NULL)
        return_error(gs_error_VMerror);;
    memcpy(ctx->EKey->data, output, 32);
    pdfi_countup(ctx->EKey);

    return 0;
}

static int check_user_password_preR5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    pdf_string *Key = NULL, *XORKey = NULL;
    int code = 0, i, j, KeyLenBytes = KeyLen / 8;
    pdf_stream *stream, *arc4_stream;
    char Buffer[32];
    char Hash[16];
    gs_md5_state_t md5;
    pdf_string *s = NULL;
    pdf_array *a = NULL;

    /* Algorithm 3.6, step 1
     * perform all but the last step of Algorithm 3,4 (Revision 2)
     * or Algorithm 3.5 (revision 3 or greater).
     */

    /* Algorithm 3.4 step 1
     * Create an encryption key based on the user password string as described in Algorithm 3.2
     */
    code = pdf_compute_encryption_key_preR5(ctx, Password, Len, KeyLen, &Key, R);
    if (code < 0)
        return code;

    switch (R) {
        case 2:
            /* Algorithm 3.4, step 2
             * Encrypt the 32 byte padding string from step 1 of Algorithm 3.2, with an RC4
             * encryption function, using the key from the preceding step.
             */

            code = pdfi_open_memory_stream_from_memory(ctx, 32, (byte *)PadString, &stream);
            if (code < 0)
                goto error;

            code = pdfi_apply_Arc4_filter(ctx, Key, stream, &arc4_stream);
            if (code < 0) {
                pdfi_close_memory_stream(ctx, NULL, stream);
                goto error;
            }

            sfread(Buffer, 1, 32, arc4_stream->s);
            pdfi_close_file(ctx, arc4_stream);
            pdfi_close_memory_stream(ctx, NULL, stream);

            /* Algorithm 3.6 step 2
             * If the result of the step above is equal to the value of the encryption dictionary
             * U entry the password supplied is the correct user password.
             */
            if (memcmp(Buffer, ctx->U, 32) != 0) {
                code = gs_error_unknownerror;
                goto error;
            } else {
                /* Password authenticated, we can now use teh calculated encryption key to decrypt the file */
                ctx->EKey = Key;
            }
            break;
        case 3:
        case 4:
            /* Algorithm 3.5 step 2
             * Pass the 32 byte padding string from step 1 of Algorithm 3.2 to an MD5 hash */
            gs_md5_init(&md5);
            gs_md5_append(&md5, (gs_md5_byte_t *)PadString, 32);
            code = pdfi_dict_get_type(ctx, ctx->Trailer, "ID", PDF_ARRAY, (pdf_obj **)&a);
            if (code < 0) {
                if (code == gs_error_undefined) {
                    emprintf(ctx->memory, "   **** Error: ID key in the trailer is required for encrypted files.\n");
                    emprintf(ctx->memory, "               File may not be possible to decrypt.\n");
                }
                return code;
            }

            /* Step 3
             * Pass the first element of the file's file identifier array ti the hash function
             * and finish the hash
             */
            code = pdfi_array_get_type(ctx, a, (uint64_t)0, PDF_STRING, (pdf_obj **)&s);
            if (code < 0)
                goto error;
            gs_md5_append(&md5, s->data, s->length);
            gs_md5_finish(&md5, (gs_md5_byte_t *)&Hash);

            /* Step 4
             * Encrypt the 16-byte result of the hash using an RC4 encryption function with
             * the encryption key from step 1 (of Algorithm 3.5).
             */
            code = pdfi_open_memory_stream_from_memory(ctx, 16, (byte *)Hash, &stream);
            if (code < 0)
                goto error;

            code = pdfi_apply_Arc4_filter(ctx, Key, stream, &arc4_stream);
            if (code < 0) {
                pdfi_close_memory_stream(ctx, NULL, stream);
                goto error;
            }

            sfread(Buffer, 1, 16, arc4_stream->s);
            pdfi_close_file(ctx, arc4_stream);
            pdfi_close_memory_stream(ctx, NULL, stream);

            code = pdfi_alloc_object(ctx, PDF_STRING, KeyLenBytes, (pdf_obj **)&XORKey);
            if (code < 0)
                goto error;
            /* pdfi_alloc_object() creates objects with a reference count of 0 */
            pdfi_countup(XORKey);

            /* Step 5
             * Do the following 19 times. Take the output from the previous invocation of the RC4
             * function and pas it as input to a new invocation of the function; use an encryption key
             * generated by taking each byte of the original encyption key (obtained in step 1 of
             * algorithm 3.5) and performing an XOR operation between that byte and the single byte
             * value of the iteration counter (from 1 to 19).
             */
            for (i=1;i < 20;i++) {
                memcpy(Hash, Buffer, 16);
                code = pdfi_open_memory_stream_from_memory(ctx, 16, (byte *)Hash, &stream);
                if (code < 0)
                    goto error;

                for (j=0;j < KeyLenBytes;j++) {
                    XORKey->data[j] = Key->data[j] ^ i;
                }

                code = pdfi_apply_Arc4_filter(ctx, XORKey, stream, &arc4_stream);
                if (code < 0) {
                    pdfi_close_memory_stream(ctx, NULL, stream);
                    goto error;
                }
                sfread(Buffer, 1, 16, arc4_stream->s);
                pdfi_close_file(ctx, arc4_stream);
                pdfi_close_memory_stream(ctx, NULL, stream);
            }

            /* Algorithm 3.6 step 2
             * If the result of the step above is equal to the value of the encryption dictionary U entry
             * (comparing on the first 16 bytes in the case of revision 3 of greater)
             * the password supplied is the correct user password.
             */
            if (memcmp(Buffer, ctx->U, 16) != 0) {
                code = gs_error_unknownerror;
                goto error;
            } else {
                /* Password authenticated, we can now use the calculated encryption key to decrypt the file */
                ctx->EKey = Key;
            }
            break;
        default:
            code = gs_note_error(gs_error_rangecheck);
            goto error;
            break;
    }

    pdfi_countdown(XORKey);
    pdfi_countdown(s);
    pdfi_countdown(a);
    return 0;

error:
    pdfi_countdown(XORKey);
    pdfi_countdown(Key);
    pdfi_countdown(s);
    pdfi_countdown(a);
    return code;
}

static int check_owner_password_R5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    char *UTF8_Password, *Test = NULL, Buffer[32], OEPadded[48];
    int NewLen;
    int code = 0;
    pdf_stream *stream = NULL, *filter_stream = NULL;
    pdf_string *Key = NULL;

    /* From the original code in Resource/Init/pdf_sec.ps:
     * Step 1.
     * If the .saslprep operator isn't available (because ghostscript
     * wasn't built with libidn support), just skip this step.  ASCII
     * passwords will still work fine, and even most non-ASCII passwords
     * will be okay; any non-ASCII passwords that fail will produce a
     * warning from pdf_process_Encrypt.
     */
#ifdef HAVE_LIBIDN
    code = apply_sasl(ctx, Password, Len, &UTF8_Password, &NewLen);
    if (code < 0)
        return code;
#else
    UTF8_Password = Password;
    NewLen = Len;
#endif
    if (NewLen > 127)
        NewLen = 127;

    Test = (char *)gs_alloc_bytes(ctx->memory, NewLen + 8 + 48, "r5 password test");
    if (Test == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    /* concatenate the password */
    memcpy(Test, UTF8_Password, NewLen);
    /* With the 'Owner Validation Salt' (stored as part of the /O string */
    memcpy(&Test[NewLen], &ctx->O[32], 8);
    /* and also concatenated with the /U string, which is defined to be 48 bytes for revision 5 */
    memcpy(&Test[NewLen + 8], &ctx->U, 48);

    /* Now calculate the SHA256 hash */
    code = pdfi_open_memory_stream_from_memory(ctx, NewLen + 8 + 48, (byte *)Test, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_SHA256_filter(ctx, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    if (memcmp(Buffer, ctx->O, 32) != 0) {
        code = gs_note_error(gs_error_unknownerror);
        goto error;
    }

    /* Password matched */
    /* Finally calculate the decryption key */
    gs_free_object(ctx->memory, Test, "R5 password test");

    /* Password + last 8 bytes of /O */
    Test = (char *)gs_alloc_bytes(ctx->memory, NewLen + 8 + 48, "R5 password test");
    if (Test == NULL) {
        code = gs_note_error(gs_error_VMerror);
        goto error;
    }

    memcpy(Test, UTF8_Password, NewLen);
    /* The 'User Key Salt' (stored as part of the /O string */
    memcpy(&Test[NewLen], &ctx->O[40], 8);
    memcpy(&Test[NewLen + 8], ctx->U, 48);

    /* Now calculate the SHA256 hash */
    code = pdfi_open_memory_stream_from_memory(ctx, NewLen + 8 + 48, (byte *)Test, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_SHA256_filter(ctx, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    memset(OEPadded, 0x00, 16);
    memcpy(&OEPadded[16], ctx->OE, 32);

    code = pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&Key);
    if (code < 0)
        goto error;
    /* pdfi_alloc_object() creates objects with a refrence count of 0 */
    pdfi_countup(Key);
    memcpy(Key->data, Buffer, 32);

    /* Now apply AESDecode to the padded UE string, using the SHA from above as the key */
    code = pdfi_open_memory_stream_from_memory(ctx, 48, (byte *)OEPadded, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_AES_filter(ctx, Key, false, stream, &filter_stream);
    if (code < 0) {
        pdfi_close_memory_stream(ctx, NULL, stream);
        goto error;
    }

    sfread(Buffer, 1, 32, filter_stream->s);
    pdfi_close_file(ctx, filter_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);
    pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&ctx->EKey);
    if (ctx->EKey == NULL)
        goto error;
    memcpy(ctx->EKey->data, Buffer, 32);
    pdfi_countup(ctx->EKey);

error:
    pdfi_countdown(Key);
    gs_free_object(ctx->memory, Test, "R5 password test");
#ifdef HAVE_LIBIDN
    gs_free_object(ctx->memory, UTF8_Password, "free sasl result");
#endif
    return code;
}

static int check_owner_password_R6(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
	unsigned char validation[32];
	unsigned char output[32];

    pdf_compute_encryption_key_r6((unsigned char *)Password, Len, (unsigned char *)ctx->O, (unsigned char *)ctx->OE,
                                  (unsigned char *)ctx->U, (unsigned char *)ctx->UE, 1, validation, output);

    if (memcmp(validation, ctx->O, 32) != 0)
        return_error(gs_error_unknownerror);

    pdfi_alloc_object(ctx, PDF_STRING, 32, (pdf_obj **)&ctx->EKey);
    if (ctx->EKey == NULL)
        return_error(gs_error_VMerror);;
    memcpy(ctx->EKey->data, output, 32);
    pdfi_countup(ctx->EKey);

    return 0;
}

static int check_owner_password_preR5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    char Key[32];
    int code = 0, i, j, KeyLenBytes = KeyLen / 8;
    pdf_string *EKey = NULL;
    gs_md5_state_t md5;
    pdf_stream *stream, *arc4_stream;
    char Buffer[32], Arc4Source[32];

    /* Algorithm 3.7 */
    /* Step 1, Compute an encryption key from steps 1-4 of Algorithm 3.3 */

    /* Algorithm 3.3, step 1. Pad or truncate the password string to exactly 32 bytes */
    if (Len > 32)
        Len = 32;

    memcpy(Key, Password, Len);

    if (Len < 32)
        memcpy(&Key[Len], PadString, 32 - Len);

    /* Algorithm 3.3, step 2. Initialise the MD5 hash function and pass the result of step 1 to this function */
    gs_md5_init(&md5);
    gs_md5_append(&md5, (gs_md5_byte_t *)&Key, 32);
    gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);

    /* Algorithm 3.3, step 3. Only for R3 or greater */
    if (R > 2) {
        code = pdfi_alloc_object(ctx, PDF_STRING, KeyLenBytes, (pdf_obj **)&EKey);
        if (code < 0)
            goto error;
        /* pdfi_alloc_object() creates objects with a refrence count of 0 */
        pdfi_countup(EKey);

        for (i = 0; i < 50; i++) {
            gs_md5_init(&md5);
            gs_md5_append(&md5, (gs_md5_byte_t *)&Key, KeyLenBytes);
            gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);
        }
        /* Algorithm 3.3, step 4. Use KeyLen bytes of the final hash as an RC$ key */
        /* Algorithm 3.7, step 2 (R >= 3) */
        memcpy(Buffer, ctx->O, 32);

        /* Algorithm 3.7 states (at the end):
         * "performing an XOR (exclusive or) operation between each byte of the key and the single-byte value of the iteration counter (from 19 to 0)."
         * which implies that the loop should run 20 times couting down from 19 to 0. For decryption at least this is completely
         * incorrect. Doing that results in completely garbage output.
         * By using 0 as the first index we get the correct Key when XOR'ing that with the
         * key computed above, and continuing until the loop counter reaches 19 gives us the correct
         * result.
         */
        for (i=0; i<20; i++) {
            memcpy(Arc4Source, Buffer, 32);
            code = pdfi_open_memory_stream_from_memory(ctx, 32, (byte *)Arc4Source, &stream);
            if (code < 0)
                goto error;
            for(j=0;j< KeyLenBytes;j++){
                EKey->data[j] = Key[j] ^ i;
            }
            code = pdfi_apply_Arc4_filter(ctx, EKey, stream, &arc4_stream);
            sfread(Buffer, 1, 32, arc4_stream->s);
            pdfi_close_file(ctx, arc4_stream);
            pdfi_close_memory_stream(ctx, NULL, stream);
        }

    } else {
        /* Algorithm 3.3, step 4. For revision 2 always use 5 bytes of the final hash as an RC4 key */
        code = pdfi_alloc_object(ctx, PDF_STRING, 5, (pdf_obj **)&EKey);
        if (code < 0)
            goto error;
        memcpy(EKey->data, Key, 5);

        /* Algorithm 3.7, step 2 (R == 2) Use RC4 with the computed key to decrypt the O entry of the crypt dict */
        code = pdfi_open_memory_stream_from_memory(ctx, 32, (byte *)ctx->O, &stream);
        if (code < 0)
            goto error;

        code = pdfi_apply_Arc4_filter(ctx, EKey, stream, &arc4_stream);
        pdfi_countdown(EKey);

        sfread(Buffer, 1, 32, arc4_stream->s);

        pdfi_close_file(ctx, arc4_stream);
        pdfi_close_memory_stream(ctx, NULL, stream);
    }

    /* Algorithm 3.7, step 3, the result of step 2 purports to be the user password, check it */
    code = check_user_password_preR5(ctx, Buffer, 32, KeyLen, R);

error:
    pdfi_countdown(EKey);
    return code;
}

/* Compute a decryption key for an 'object'. The decryption key for a string or stream is
 * calculated by algorithm 3.1.
 */
int pdfi_compute_objkey(pdf_context *ctx, pdf_obj *obj, pdf_string **Key)
{
    char *Buffer;
    int idx, ELength, code = 0, md5_length = 0;
    gs_md5_state_t md5;
    int64_t object_num;
    uint32_t generation_num;

    if (ctx->R < 5) {
        if (obj->object_num == 0) {
            /* The object is a direct object, use the object number of the container instead */
            object_num = obj->indirect_num;
            generation_num = obj->indirect_gen;
        } else {
            object_num = obj->object_num;
            generation_num = obj->generation_num;
        }

        /* Step 1, obtain the object and generation numbers (see arguments). If the string is
         * a direct object, use the identifier of the indirect object containing it.
         * Buffer length is a maximum of the Encryption key + 3 bytes from the object number
         * + 2 bytes from the generation number and (for AES filters) 4 bytes of sALT.
         */
        Buffer = (char *)gs_alloc_bytes(ctx->memory, ctx->EKey->length + 9, "");
        if (Buffer == NULL)
            return gs_note_error(gs_error_VMerror);

        /* Step 2, Treating the object number and generation number as binary integers, extend
         * the original n-byte encryption key (calculated in pdfi_read_Encryption) to n+5 bytes
         * by appending the low order 3 bytes of the object number and the low order 2 bytes of
         * the generation number in that order, low-order byte first. (n is 5 unless the value
         * of V in the encryption dictionary is greater than 1 in which case n is the value of
         * Length divided by 8). Because we store the encryption key is as a PDF string object,
         * we can just use the length of the string data, we calculated the length as part of
         * creating the key.
         */
        memcpy(Buffer, ctx->EKey->data, ctx->EKey->length);
        idx = ctx->EKey->length;
        Buffer[idx] = object_num & 0xff;
        Buffer[++idx] = (object_num & 0xff00) >> 8;
        Buffer[++idx] = (object_num & 0xff0000) >> 16;
        Buffer[++idx] = generation_num & 0xff;
        Buffer[++idx] = (generation_num & 0xff00) >> 8;

        md5_length = ctx->EKey->length + 5;

        /* If using the AES algorithm, extend the encryption key an additional 4 bytes
         * by adding the value "sAlT" which corresponds to the hexadecimal 0x73416c54
         * (This addition is done for backward compatibility and is not intended to
         * provide addtional security).
         */
        if (ctx->StmF == AESV2 || ctx->StmF == AESV3){
            memcpy(&Buffer[++idx], sAlTString, 4);
            md5_length += 4;
        }

        /* Step 3
         * Initialise the MD5 function and pass the result of step 2 as input to this function
         */
        gs_md5_init(&md5);
        gs_md5_append(&md5, (gs_md5_byte_t *)Buffer, md5_length);
        gs_md5_finish(&md5, (gs_md5_byte_t *)Buffer);

        /* Step 4
         * Use the first n+5 bytes, up to a maximum of 16, of the output from the MD5
         * hash as the key for the RC4 or AES symmetric key algorithms, along with the
         * string or stream data to be encrypted.
         */
        ELength = ctx->EKey->length + 5;
        if (ELength > 16)
            ELength = 16;

        code = pdfi_alloc_object(ctx, PDF_STRING, (uint64_t)ELength, (pdf_obj **)Key);
        if (code >= 0)
            memcpy((*Key)->data, Buffer, ELength);
        /* pdfi_alloc_object() creates objects with a refrence count of 0 */
        pdfi_countup(*Key);

        gs_free_object(ctx->memory, Buffer, "");
    } else {
        /* Revision 5 & 6 don't use the object number and generation, just return the pre-calculated key */
        *Key = ctx->EKey;
        pdfi_countup(*Key);
    }
    return code;
}

int pdfi_decrypt_string(pdf_context *ctx, pdf_string *string)
{
    int code = 0;
    pdf_stream *stream = NULL, *crypt_stream = NULL;
    pdf_string *EKey = NULL;
    char *Buffer = NULL;

    if (!is_compressed_object(ctx, string->indirect_num, string->indirect_gen)) {
        Buffer = (char *)gs_alloc_bytes(ctx->memory, string->length, "pdfi_decrypt_string");
        if (Buffer == NULL)
            return_error(gs_error_VMerror);

        code = pdfi_compute_objkey(ctx, (pdf_obj *)string, &EKey);
        if (code < 0)
            goto error;

        code = pdfi_open_memory_stream_from_memory(ctx, string->length, (byte *)string->data, &stream);
        if (code < 0)
            goto error;

        switch(ctx->StmF) {
            /* There are only two possible filters, RC4 or AES, we take care
             * of the number of bits in the key by using ctx->Length.
             */
            case V1:
            case V2:
                code = pdfi_apply_Arc4_filter(ctx, EKey, stream, &crypt_stream);
                break;
            case AESV2:
            case AESV3:
                code = pdfi_apply_AES_filter(ctx, EKey, 1, stream, &crypt_stream);
                break;
            default:
                code = gs_error_rangecheck;
        }
        if (code < 0) {
            pdfi_close_memory_stream(ctx, NULL, stream);
            goto error;
        }

        sfread(Buffer, 1, string->length, crypt_stream->s);

        pdfi_close_file(ctx, crypt_stream);
        pdfi_close_memory_stream(ctx, NULL, stream);
        pdfi_countdown(EKey);

        memcpy(string->data, Buffer, string->length);
        gs_free_object(ctx->memory, Buffer, "pdfi_decrypt_string");
    }
    return code;

error:
    gs_free_object(ctx->memory, Buffer, "pdfi_decrypt_string");
    pdfi_countdown(EKey);
    return code;
}

/* Read the Encrypt dictionary entries and store the relevant ones
 * in the PDF context for easy access. Check whether the file is
 * readable without a password and if not, check to see if we've been
 * supplied a password. If we have try the password as the user password
 * and if that fails as the owner password. Store the calculated decryption key
 * for later use decrypting objects.
 */
int pdfi_read_Encryption(pdf_context *ctx)
{
    int code = 0, V = 0, KeyLen;
    pdf_dict *CF_dict = NULL, *StdCF_dict = NULL;
    pdf_dict *d = NULL;
    pdf_obj *o = NULL;
    pdf_string *s = NULL;
    int64_t i64;
    double f;

    if (ctx->pdfdebug)
        dmprintf(ctx->memory, "%% Checking for Encrypt dictionary\n");

    code = pdfi_dict_get(ctx, ctx->Trailer, "Encrypt", (pdf_obj **)&d);

    if (code == gs_error_undefined)
        return 0;
    else
        if (code < 0)
            return code;

    code = pdfi_dict_get(ctx, d, "Filter", &o);
    if (code < 0)
        return code;

    if (o->type != PDF_NAME) {
        code = gs_error_typecheck;
        goto done;
    }

    code = pdfi_dict_knownget_number(ctx, d, "Length", &f);
    if (code < 0)
        goto done;

    if (code > 0)
        KeyLen = (int)f;
    else
        KeyLen = 0;

    if (!pdfi_name_is((pdf_name *)o, "Standard")) {
        char *Str = NULL;

        Str = (char *)gs_alloc_bytes(ctx->memory, ((pdf_name *)o)->length + 1, "temp string for warning");
        if (Str == NULL) {
            code = gs_error_VMerror;
            goto done;
        }
        memset(Str, 0x00, ((pdf_name *)o)->length + 1);
        memcpy(Str, ((pdf_name *)o)->data, ((pdf_name *)o)->length);
        emprintf1(ctx->memory, "   **** Warning: This file uses an unknown security handler %s\n", Str);
        gs_free_object(ctx->memory, Str, "temp string for warning");
        code = gs_error_typecheck;
        goto done;
    }
    pdfi_countdown(o);
    o = NULL;

    code = pdfi_dict_get_int(ctx, d, "V", &i64);
    if (code < 0)
        goto done;

    if (i64 < 1 || i64 > 5) {
        code = gs_error_rangecheck;
        goto done;
    }

    switch(V) {
        case 1:
            break;
        case 2:
        case 3:
            break;
        case 4:
            break;
        case 5:
            break;
    }
    ctx->V = (int)i64;

    code = pdfi_dict_get_int(ctx, d, "R", &i64);
    if (code < 0)
        goto done;
    ctx->R = (int)i64;

    code = pdfi_dict_get_int(ctx, d, "P", &i64);
    if (code < 0)
        goto done;
    ctx->P = (int)i64;

    code = pdfi_dict_get_type(ctx, d, "O", PDF_STRING, (pdf_obj **)&s);
    if (code < 0)
        goto done;

    if (ctx->R < 5) {
        if (s->length < 32) {
            pdfi_countdown(s);
            code = gs_error_rangecheck;
            goto done;
        }
        memcpy(ctx->O, s->data, 32);
    } else {
        if (s->length < 48) {
            pdfi_countdown(s);
            code = gs_error_rangecheck;
            goto done;
        }
        memcpy(ctx->O, s->data, 48);
    }
    pdfi_countdown(s);
    s = NULL;

    code = pdfi_dict_get_type(ctx, d, "U", PDF_STRING, (pdf_obj **)&s);
    if (code < 0)
        goto done;

    if (ctx->R < 5) {
        if (s->length < 32) {
            pdfi_countdown(s);
            code = gs_error_rangecheck;
            goto done;
        }
        memcpy(ctx->U, s->data, 32);
    } else {
        if (s->length < 48) {
            pdfi_countdown(s);
            code = gs_error_rangecheck;
            goto done;
        }
        memcpy(ctx->U, s->data, 48);
    }
    pdfi_countdown(s);
    s = NULL;

    code = pdfi_dict_knownget_type(ctx, d, "EncryptMetadata", PDF_BOOL, &o);
    if (code < 0)
        goto done;
    if (code > 0) {
        ctx->EncryptMetadata = ((pdf_bool *)o)->value;
        pdfi_countdown(o);
    }
    else
        ctx->EncryptMetadata = true;

    if (ctx->R > 3) {
        /* Check the Encrypt dictionary has default values for Stmf and StrF
         * and that they have the names /StdCF. We don't support anything else.
         */
        code = pdfi_dict_get_type(ctx, d, "StmF", PDF_NAME, &o);
        if (code < 0)
            goto done;
        if (!pdfi_name_is((pdf_name *)o, "StdCF")) {
            code = gs_note_error(gs_error_undefined);
            goto done;
        }
        pdfi_countdown(o);

        code = pdfi_dict_knownget_type(ctx, d, "StrF", PDF_NAME, &o);
        if (code < 0)
            goto done;
        if (!pdfi_name_is((pdf_name *)o, "StdCF")) {
            code = gs_note_error(gs_error_undefined);
            goto done;
        }
        pdfi_countdown(o);

        /* Validated StmF and StrF, now check the Encrypt dictionary for the definition of
         * the Crypt Filter dictionary and ensure it has a /StdCF dictionary.
         */
        code = pdfi_dict_get_type(ctx, d, "CF", PDF_DICT, (pdf_obj **)&CF_dict);
        if (code < 0)
            goto done;

        code = pdfi_dict_get_type(ctx, CF_dict, "StdCF", PDF_DICT, (pdf_obj **)&StdCF_dict);
        if (code < 0)
            goto done;

        code = pdfi_dict_get_type(ctx, StdCF_dict, "CFM", PDF_NAME, &o);
        if (code < 0)
            goto done;
        if (pdfi_name_is((pdf_name *)o, "V2")) {
            ctx->StmF = ctx->StrF = V2;
        } else {
            if (pdfi_name_is((pdf_name *)o, "AESV2")) {
                ctx->StmF = ctx->StrF = AESV2;
            } else {
                if (pdfi_name_is((pdf_name *)o, "AESV3")) {
                    ctx->StmF = ctx->StrF = AESV3;
                } else {
                    emprintf(ctx->memory, "\n   **** Error: Unknown default encryption method in crypt filter.\n");
                    code = gs_error_rangecheck;
                    goto done;
                }
            }
        }
        pdfi_countdown(o);
        o = NULL;

        if (ctx->R > 4) {
            code = pdfi_dict_get_type(ctx, d, "OE", PDF_STRING, (pdf_obj **)&s);
            if (code < 0)
                goto done;

            if (s->length != 32) {
                pdfi_countdown(s);
                code = gs_error_rangecheck;
                goto done;
            }
            memcpy(ctx->OE, s->data, 32);
            pdfi_countdown(s);
            s = NULL;

            code = pdfi_dict_get_type(ctx, d, "UE", PDF_STRING, (pdf_obj **)&s);
            if (code < 0)
                goto done;

            if (s->length != 32) {
                pdfi_countdown(s);
                code = gs_error_rangecheck;
                goto done;
            }
            memcpy(ctx->UE, s->data, 32);
            pdfi_countdown(s);
            s = NULL;
        }
    }

    switch(ctx->R) {
        case 2:
            /* Revision 2 is always 40-bit RC4 */
            if (KeyLen == 0)
                KeyLen = 40;
            ctx->StrF = ctx->StmF = V1;
            /* First see if the file is encrypted with an Owner password and no user password
             * in which case an empty user password will work to decrypt it.
             */
            code = check_user_password_preR5(ctx, (char *)"", 0, KeyLen, 2);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 2);
                    if (code < 0) {
                        code = check_owner_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 2);
                    }
                }
            }
            break;
        case 3:
            /* Revision 3 is always 128-bit RC4 */
            if (KeyLen == 0)
                KeyLen = 128;
            ctx->StrF = ctx->StmF = V2;
            code = check_user_password_preR5(ctx, (char *)"", 0, KeyLen, 3);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                    if (code < 0) {
                        code = check_owner_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                    }
                }
            }
            break;
        case 4:
            /* Revision 4 is either AES or RC4, but its always 128-bits */
            if (KeyLen == 0)
                KeyLen = 128;
            code = check_user_password_preR5(ctx, (char *)"", 0, KeyLen, 4);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 4);
                    if (code < 0) {
                        code = check_owner_password_preR5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 4);
                    }
                }
            }
            break;
        case 5:
            if (KeyLen == 0)
                KeyLen = 256;
            ctx->StrF = ctx->StmF = AESV3;
            code = check_user_password_R5(ctx, (char *)"", 0, KeyLen, 3);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_R5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                    if (code < 0) {
                        code = check_owner_password_R5(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                        /* If the supplied Password fails as the user *and* owner password, mayeb its in
                         * the locale, not UTF-8, try converting to UTF-8
                         */
                        if (code < 0) {
                            pdf_string *P = NULL, *P_UTF8 = NULL;

                            code = pdfi_alloc_object(ctx, PDF_STRING, strlen(ctx->Password), (pdf_obj **)&P);
                            if (code < 0)
                                goto done;
                            memcpy(P->data, ctx->Password, P->length);
                            code = locale_to_utf8(ctx, P, &P_UTF8);
                            if (code < 0) {
                                pdfi_countdown(P);
                                goto done;
                            }
                            code = check_user_password_R5(ctx, (char *)P_UTF8->data, P_UTF8->length, KeyLen, 3);
                            if (code < 0)
                                code = check_owner_password_R5(ctx, (char *)P_UTF8->data, P_UTF8->length, KeyLen, 3);
                            pdfi_countdown(P);
                            pdfi_countdown(P_UTF8);
                        }
                    }
                }
            }
            break;
        case 6:
            /* Revision 6 is always 256-bit AES */
            if (KeyLen == 0)
                KeyLen = 256;
            ctx->StrF = ctx->StmF = AESV3;
            code = check_user_password_R6(ctx, (char *)"", 0, KeyLen, 3);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_R6(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                    if (code < 0) {
                        code = check_owner_password_R6(ctx, ctx->Password, strlen(ctx->Password), KeyLen, 3);
                        /* If the supplied Password fails as the user *and* owner password, mayeb its in
                         * the locale, not UTF-8, try converting to UTF-8
                         */
                        if (code < 0) {
                            pdf_string *P = NULL, *P_UTF8 = NULL;

                            code = pdfi_alloc_object(ctx, PDF_STRING, strlen(ctx->Password), (pdf_obj **)&P);
                            if (code < 0)
                                goto done;
                            memcpy(P->data, ctx->Password, P->length);
                            code = locale_to_utf8(ctx, P, &P_UTF8);
                            if (code < 0) {
                                pdfi_countdown(P);
                                goto done;
                            }
                            code = check_user_password_R6(ctx, (char *)P_UTF8->data, P_UTF8->length, KeyLen, 3);
                            if (code < 0)
                                code = check_owner_password_R6(ctx, (char *)P_UTF8->data, P_UTF8->length, KeyLen, 3);
                            pdfi_countdown(P);
                            pdfi_countdown(P_UTF8);
                        }
                    }
                }
            }
            break;
        default:
            emprintf1(ctx->memory, "\n   **** Warning: This file uses an unknown standard security handler revision: %d\n", ctx->R);
            code = gs_error_rangecheck;
            goto done;
    }
    if (code < 0) {
        if(ctx->Password) {
            emprintf(ctx->memory, "\n   **** Error: Password did not work.\n");
            emprintf(ctx->memory, "               Cannot decrypt PDF file.\n");
        } else
            emprintf(ctx->memory, "\n   **** This file requires a password for access.\n");
    } else
        ctx->is_encrypted = true;

done:
    pdfi_countdown(CF_dict);
    pdfi_countdown(StdCF_dict);
    pdfi_countdown(d);
    pdfi_countdown(o);
    pdfi_countdown(s);
    return code;
}
