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

char PadString[32] = {
    0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41, 0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
    0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80, 0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};

/* Algorithm 3.2 */
static int pdf_compute_encryption_key_preR5(pdf_context *ctx, char *Password, int Len, pdf_string **EKey, int R)
{
    char Key[32];
    int code = 0, ELength = 5;
    char P[4];
    gs_md5_state_t md5;
    pdf_array *a = NULL;
    pdf_string *s = NULL;

    *EKey = NULL;
    /* 1. Pad or truncate the password string to exactly 32 bytes */
    if (Len > 32)
        Len = 32;

    memcpy(Key, Password, Len);

    if (Len < 32)
        memcpy(&Key[Len], PadString, 32 - Len);

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
    if (code < 0)
        return code;
    code = pdfi_array_get_type(ctx, a, (uint64_t)0, PDF_STRING, (pdf_obj **)&s);
    if (code < 0)
        goto done;
    gs_md5_append(&md5, s->data, s->length);

    /* Step 6 is only for revision 4 or greater */

    /* 7. Finish the hash */
    gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);

    /* Step 8 is only for revision 3 or greater */

    code = pdfi_alloc_object(ctx, PDF_STRING, ELength, (pdf_obj **)EKey);
    if (code >= 0) {
        memcpy((*EKey)->data, Key, ELength);
        pdfi_countup((pdf_obj *)*EKey);
    }

done:
    pdfi_countdown(s);
    pdfi_countdown(a);
    return code;
}

int check_user_password_R2(pdf_context *ctx, char *Password, int Len)
{
    pdf_string *Key;
    int code = 0;
    pdf_stream *stream, *arc4_stream;
    stream_arcfour_state state;
    char Buffer[32];

    code = pdf_compute_encryption_key_preR5(ctx, Password, Len, &Key, 2);
    if (code < 0)
        return code;

    code = pdfi_open_memory_stream_from_memory(ctx, 32, (byte *)PadString, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_Arc4_filter(ctx, Key, stream, &arc4_stream);

    sfread(Buffer, 1, 32, arc4_stream->s);

    pdfi_close_file(ctx, arc4_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    if (memcmp(Buffer, ctx->U, 32) != 0) {
        code = gs_error_unknownerror;
        goto error;
    } else {
        /* Password authenticated, we can now use teh calculated encryption key to decrypt the file */
        ctx->EKey = Key;
    }

    return 0;

error:
    gs_free_object(ctx->memory, Key, "memory for encryption key");
    return code;
}

int check_owner_password_R2(pdf_context *ctx, char *Password)
{
    char Key[32];
    int Len = strlen(Password), code = 0, ELength = 5;
    pdf_string *EKey = NULL;
    gs_md5_state_t md5;
    pdf_stream *stream, *arc4_stream;
    stream_arcfour_state state;
    char Buffer[33];

    /* 1. Pad or truncate the password string to exactly 32 bytes */
    if (Len > 32)
        Len = 32;

    memcpy(Key, Password, Len);

    if (Len < 32)
        memcpy(&Key[Len], PadString, 32 - Len);

    /* 2. Initialise the MD5 hash function and pass the result of step 1 to this function */
    gs_md5_init(&md5);
    gs_md5_append(&md5, (gs_md5_byte_t *)&Key, 32);

    /* 3. Only for R3 or greater */

    /* 4. Finish the hash */
    gs_md5_finish(&md5, (gs_md5_byte_t *)&Key);

    code = pdfi_alloc_object(ctx, PDF_STRING, ELength, (pdf_obj **)&EKey);
    if (code >= 0)
        memcpy(EKey->data, Key, ELength);

    code = pdfi_open_memory_stream_from_memory(ctx, 32, (byte *)ctx->O, &stream);
    if (code < 0)
        goto error;

    code = pdfi_apply_Arc4_filter(ctx, EKey, stream, &arc4_stream);
    pdfi_countdown(EKey);

    sfread(Buffer, 1, 32, arc4_stream->s);

    pdfi_close_file(ctx, arc4_stream);
    pdfi_close_memory_stream(ctx, NULL, stream);

    Buffer[32] = 0x00;
    code = check_user_password_R2(ctx, Buffer, 32);

error:
    pdfi_countdown(Key);
    return code;
}

int pdfi_compute_objkey(pdf_context *ctx, int64_t object_num, uint32_t generation_num, pdf_string **StreamKey)
{
    char *Buffer;
    int idx, ELength, code = 0;
    gs_md5_state_t md5;

    Buffer = (char *)gs_alloc_bytes(ctx->memory, ctx->EKey->length + 5, "");
    if (Buffer == NULL)
        return gs_note_error(gs_error_VMerror);

    memcpy(Buffer, ctx->EKey->data, ctx->EKey->length);
    idx = ctx->EKey->length;
    Buffer[idx] = object_num & 0xff;
    Buffer[++idx] = (object_num & 0xff00) >> 8;
    Buffer[++idx] = (object_num & 0xff0000) >> 16;
    Buffer[++idx] = generation_num & 0xff;
    Buffer[++idx] = (generation_num & 0xff00) >> 8;

    gs_md5_init(&md5);
    gs_md5_append(&md5, (gs_md5_byte_t *)Buffer, ctx->EKey->length + 5);
    gs_md5_finish(&md5, (gs_md5_byte_t *)Buffer);

    ELength = ctx->EKey->length + 5;
    if (ELength > 16)
        ELength = 16;

    code = pdfi_alloc_object(ctx, PDF_STRING, (uint64_t)ELength, (pdf_obj **)StreamKey);
    if (code >= 0)
        memcpy((*StreamKey)->data, Buffer, ELength);

    gs_free_object(ctx->memory, Buffer, "");

    return code;
}

int pdfi_read_Encryption(pdf_context *ctx)
{
    int code = 0, V = 0;
    pdf_dict *d = NULL;
    pdf_obj *o = NULL;
    pdf_string *s;
    char *U;
    int64_t i64;

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

    if (i64 < 1 || i64 > 4) {
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

    switch(ctx->R) {
        case 2:
            /* First see if the file is encrypted with an Owner password and no user password
             * in which case an empty user password will work to decrypt it.
             */
            code = check_user_password_R2(ctx, "", 0);
            if (code < 0) {
                if(ctx->Password) {
                    /* Empty user password didn't work, try the password we've been given as a user password */
                    code = check_user_password_R2(ctx, ctx->Password, strlen(ctx->Password));
                    if (code < 0) {
                        code = check_owner_password_R2(ctx, ctx->Password);
                    }
                }
            }
            break;
        case 3:
            code = gs_error_unknownerror;
            break;
        case 4:
            code = gs_error_unknownerror;
            break;
        case 5:
            code = gs_error_unknownerror;
            break;
        case 6:
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
