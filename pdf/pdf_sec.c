/* Copyright (C) 2001-2018 Artifex Software, Inc.
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
#include "smd5.h"
#include "sarc4.h"

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
    if (R > 3 && ctx->EncryptMetadata) {
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

static int check_user_password_preR5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    pdf_string *Key = NULL, *XORKey = NULL;
    int code = 0, i, j, KeyLenBytes = KeyLen / 8;
    pdf_stream *stream, *arc4_stream;
    stream_arcfour_state state;
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

static int check_owner_password_preR5(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    char Key[32];
    int code = 0, i, j, KeyLenBytes = KeyLen / 8;
    pdf_string *EKey = NULL;
    gs_md5_state_t md5;
    pdf_stream *stream, *arc4_stream;
    stream_arcfour_state state;
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
    pdfi_countdown(Key);
    return code;
}

static int check_user_password_R4(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    return_error(gs_error_undefined);
}

static int check_owner_password_R4(pdf_context *ctx, char *Password, int Len, int KeyLen, int R)
{
    return_error(gs_error_undefined);
}

/* Compute a decryption key for an 'object'. The decryption key for a string or stream is
 * calculated by algorithm 3.1.
 */
int pdfi_compute_objkey(pdf_context *ctx, pdf_obj *obj, pdf_string **Key)
{
    char *Buffer;
    int idx, ELength, code = 0;
    gs_md5_state_t md5;
    int64_t object_num;
    uint32_t generation_num;

    if (obj->object_num == 0) {
        /* The object is a direct object, use the object number of the container instead */
        /* Not yet implemented, will need plumbing changes in pdfi_dereference */
    } else {
        object_num = obj->object_num;
        generation_num = obj->generation_num;
    }

    /* Step 1, obtain the object and generation numbers (see arguments). If the string is
     * a direct object, use the identifier of the indirect object containing it.
     */
    Buffer = (char *)gs_alloc_bytes(ctx->memory, ctx->EKey->length + 5, "");
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

    /* If using the AES algorithm, extend the encryption key an additional 4 bytes
     * by adding the value "sAlT" which corresponds to the hexadecimal 0x73416c54
     * (This addition is done for backward compatibility and is not intended to
     * provide addtional security).
     */
    /* Not yet implemented */

    /* Step 3
     * Initialise the MD5 function and pass the result of step 2 as input to this function
     */
    gs_md5_init(&md5);
    gs_md5_append(&md5, (gs_md5_byte_t *)Buffer, ctx->EKey->length + 5);
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

    gs_free_object(ctx->memory, Buffer, "");

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
    pdf_dict *d = NULL;
    pdf_obj *o = NULL;
    pdf_string *s;
    char *U;
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

    if (s->length != 32) {
        pdfi_countdown(s);
        code = gs_error_rangecheck;
        goto done;
    }
    memcpy(ctx->O, s->data, 32);
    pdfi_countdown(s);
    s = NULL;

    code = pdfi_dict_get_type(ctx, d, "U", PDF_STRING, (pdf_obj **)&s);
    if (code < 0)
        goto done;

    if (s->length != 32) {
        pdfi_countdown(s);
        code = gs_error_rangecheck;
        goto done;
    }
    memcpy(ctx->U, s->data, 32);

    code = pdfi_dict_knownget_type(ctx, d, "EncryptMetadata", PDF_BOOL, &o);
    if (code < 0)
        goto done;
    if (code > 0)
        ctx->EncryptMetadata = ((pdf_bool *)o)->value;
    else
        ctx->EncryptMetadata = false;

    switch(ctx->R) {
        case 2:
            /* Revision 2 is always 40-bit RC4 */
            if (KeyLen == 0)
                KeyLen = 40;
            ctx->StrF = ctx->StmF = V1;
            /* First see if the file is encrypted with an Owner password and no user password
             * in which case an empty user password will work to decrypt it.
             */
            code = check_user_password_preR5(ctx, "", 0, KeyLen, 2);
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
            code = check_user_password_preR5(ctx, "", 0, KeyLen, 3);
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
            code = check_user_password_preR5(ctx, "", 0, KeyLen, 4);
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
            emprintf(ctx->memory, "\n   **** Error: Revision 5 standard security handler is an Adobe.\n");
            emprintf(ctx->memory, "\n               proprietary extension and not supported.\n");
            code = gs_error_rangecheck;
            break;
        case 6:
            /* Revision 6 is always 256-bit AES */
            if (KeyLen == 0)
                KeyLen = 256;
            ctx->StrF = ctx->StmF = AESV3;
            code = gs_error_unknownerror;
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
    pdfi_countdown(d);
    pdfi_countdown(o);
    return code;
}
