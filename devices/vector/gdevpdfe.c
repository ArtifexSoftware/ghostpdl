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


/* Metadata writer. */
#include "gx.h"
#include "gserrors.h"
#include "string_.h"
#include "time_.h"
#include "stream.h"
#include "gp.h"
#include "smd5.h"
#include "gscdefs.h"
#include "gdevpdfx.h"
#include "gdevpdfg.h"
#include "gdevpdfo.h"

/* These two tables map PDFDocEncoding character codes (0x00->0x20 and 0x80->0xAD)
 * to their equivalent UTF-16BE value. That allows us to convert a PDFDocEncoding
 * string to UTF-16BE, and then further translate that into UTF-8.
 * Note 0x7F is individually treated.
 * See pdf_xmp_write_translated().
 */
static char PDFDocEncodingLookupLo [64] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x09, 0x00, 0x0A, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0xD8, 0x02, 0xC7, 0x02, 0xC6, 0x02, 0xD9,
    0x02, 0xDD, 0x02, 0xDB, 0x02, 0xDA, 0x02, 0xDC
};

static char PDFDocEncodingLookupHi [92] = {
    0x20, 0x22, 0x20, 0x20, 0x20, 0x21, 0x20, 0x26,
    0x20, 0x14, 0x20, 0x13, 0x01, 0x92, 0x20, 0x44,
    0x20, 0x39, 0x20, 0x3A, 0x22, 0x12, 0x20, 0x30,
    0x20, 0x1E, 0x20, 0x1C, 0x20, 0x1D, 0x20, 0x18,
    0x20, 0x19, 0x20, 0x1A, 0x21, 0x22, 0xFB, 0x01,
    0xFB, 0x02, 0x01, 0x41, 0x01, 0x52, 0x01, 0x60,
    0x01, 0x78, 0x01, 0x7D, 0x01, 0x31, 0x01, 0x42,
    0x01, 0x53, 0x01, 0x61, 0x01, 0x7E, 0x00, 0x00,
    0x20, 0xAC, 0x00, 0xA1, 0x00, 0xA2, 0x00, 0xA3,
    0x00, 0xA4, 0x00, 0xA5, 0x00, 0xA6, 0x00, 0xA7,
    0x00, 0xA8, 0x00, 0xA9, 0x00, 0xAA, 0x00, 0xAB,
    0x00, 0xAC, 0x00, 0x00
};

static void
copy_bytes(stream *s, const byte **data, int *data_length, int n)
{
    while (n-- && (*data_length)--) {
        stream_putc(s, *((*data)++));
    }
}

/* Write XML data */
static void
pdf_xml_data_write(stream *s, const byte *data, int data_length)
{
    int l = data_length;
    const byte *p = data;

    while (l > 0) {
        switch (*p) {
            case '<' : stream_puts(s, "&lt;"); l--; p++; break;
            case '>' : stream_puts(s, "&gt;"); l--; p++; break;
            case '&' : stream_puts(s, "&amp;"); l--; p++; break;
            case '\'': stream_puts(s, "&apos;"); l--; p++; break;
            case '"' : stream_puts(s, "&quot;"); l--; p++; break;
            default:
                if (*p < 32) {
                    /* Not allowed in XML. */
                    pprintd1(s, "&#%d;", *p);
                    l--; p++;
                } else if (*p >= 0x7F && *p <= 0x9f) {
                    /* Control characters are discouraged in XML. */
                    pprintd1(s, "&#%d;", *p);
                    l--; p++;
                } else if ((*p & 0xE0) == 0xC0) {
                    /* A 2-byte UTF-8 sequence */
                    copy_bytes(s, &p, &l, 2);
                } else if ((*p & 0xF0) == 0xE0) {
                    /* A 3-byte UTF-8 sequence */
                    copy_bytes(s, &p, &l, 3);
                } else if ((*p & 0xF0) == 0xF0) {
                    /* A 4-byte UTF-8 sequence */
                    copy_bytes(s, &p, &l, 4);
                } else {
                    stream_putc(s, *p);
                    l--; p++;
                }
        }
    }
}

/* Write XML string */
static inline void
pdf_xml_string_write(stream *s, const char *data)
{
    pdf_xml_data_write(s, (const byte *)data, strlen(data));
}

/* Begin an opening XML tag */
static inline void
pdf_xml_tag_open_beg(stream *s, const char *data)
{
    stream_putc(s, '<');
    stream_puts(s, data);
}

/* End an XML tag */
static inline void
pdf_xml_tag_end(stream *s)
{
    stream_putc(s, '>');
}

/* End an empty XML tag */
static inline void
pdf_xml_tag_end_empty(stream *s)
{
    stream_puts(s, "/>");
}

/* Write an opening XML tag */
static inline void
pdf_xml_tag_open(stream *s, const char *data)
{
    stream_putc(s, '<');
    stream_puts(s, data);
    stream_putc(s, '>');
}

/* Write a closing XML tag */
static inline void
pdf_xml_tag_close(stream *s, const char *data)
{
    stream_puts(s, "</");
    stream_puts(s, data);
    stream_putc(s, '>');
}

/* Write an attribute name */
static inline void
pdf_xml_attribute_name(stream *s, const char *data)
{
    stream_putc(s, ' ');
    stream_puts(s, data);
    stream_putc(s, '=');
}

/* Write a attribute value */
static inline void
pdf_xml_attribute_value(stream *s, const char *data)
{
    stream_putc(s, '\'');
    pdf_xml_string_write(s, data);
    stream_putc(s, '\'');
}
/* Write a attribute value */
static inline void
pdf_xml_attribute_value_data(stream *s, const byte *data, int data_length)
{
    stream_putc(s, '\'');
    pdf_xml_data_write(s, data, data_length);
    stream_putc(s, '\'');
}

/* Begin an  XML instruction */
static inline void
pdf_xml_ins_beg(stream *s, const char *data)
{
    stream_puts(s, "<?");
    stream_puts(s, data);
}

/* End an XML instruction */
static inline void
pdf_xml_ins_end(stream *s)
{
    stream_puts(s, "?>");
}

/* Write a newline character */
static inline void
pdf_xml_newline(stream *s)
{
    stream_puts(s, "\n");
}

/* Copy to XML output */
static inline void
pdf_xml_copy(stream *s, const char *data)
{
    stream_puts(s, data);
}

/* --------------------------------------------  */

static int
pdf_xmp_time(char *buf, int buf_length)
{
    /* We don't write a day time because we don't have a time zone. */
    struct tm tms;
    time_t t;
    char buf1[4+1+2+1+2+1]; /* yyyy-mm-dd\0 */

#ifdef CLUSTER
    memset(&t, 0, sizeof(t));
    memset(&tms, 0, sizeof(tms));
#else
    time(&t);
    tms = *localtime(&t);
#endif
    gs_snprintf(buf1, sizeof(buf1),
            "%04d-%02d-%02d",
            tms.tm_year + 1900, tms.tm_mon + 1, tms.tm_mday);
    strncpy(buf, buf1, buf_length);
    return strlen(buf);
}

static int
pdf_xmp_convert_time(char *dt, int dtl, char *buf, int bufl)
{   /* The 'dt' buffer is of same size as 'buf'. */
    /* Input  sample : D:199812231952?08'00' */
    /* Output sample : 1997-07-16T19:20:30+01:00 */
    int l = dtl;

    if (l > bufl)
        l = bufl;
    if (dt[0] == 'D' && dt[1] == ':') {
        l -= 2;
        memcpy(buf, dt + 2, l);
    } else
        memcpy(buf, dt, l);
    memcpy(dt, buf, 4); /* year */
    if (l <= 4)
        return 4;

    dt[4] = '-';
    memcpy(dt + 5, buf + 4, 2); /* month */
    if (l <= 6)
        return 7;

    dt[7] = '-';
    memcpy(dt + 8, buf + 6, 2); /* day */
    if (l <= 8)
        return 10;

    dt[10] = 'T';
    memcpy(dt + 11, buf + 8, 2); /* hour */
    dt[13] = ':';
    memcpy(dt + 14, buf + 10, 2); /* minute */
    if (l <= 12) {
        dt[16] = 'Z'; /* Default time zone 0. */
        return 17;
    }

    dt[16] = ':';
    memcpy(dt + 17, buf + 12, 2); /* second */
    if (l <= 14) {
        dt[19] = 'Z'; /* Default time zone 0. */
        return 20;
    }

    dt[19] = buf[14]; /* designator */
    if (dt[19] == 'Z')
        return 20;
    if (l <= 15)
        return 20;
    memcpy(dt + 20, buf + 15, 2); /* Time zone hour difference. */
    if (l <= 17)
        return 22;

    dt[22] = ':';
    /* Skipping '\'' in 'buf'. */
    memcpy(dt + 23, buf + 18, 2); /* Time zone minutes difference. */
    return 25;
}

int
pdf_get_docinfo_item(gx_device_pdf *pdev, const char *key, char *buf, int buf_length)
{
    const cos_value_t *v = cos_dict_find(pdev->Info, (const byte *)key, strlen(key));
    int l;
    const byte *s;

    if (v != NULL && (v->value_type == COS_VALUE_SCALAR ||
                        v->value_type == COS_VALUE_CONST)) {
        if (v->contents.chars.size >= 2 && v->contents.chars.data[0] == '(') {
            s = v->contents.chars.data + 1;
            l = v->contents.chars.size - 2;
        } else {
            s = v->contents.chars.data;
            l = v->contents.chars.size;
        }
    } else
        return 0;
    if (l < 0)
        l = 0;
    if (l > buf_length)
        l = buf_length;
    memcpy(buf, s, l);
    return l;
}

static inline byte
decode_escape(const byte *data, int data_length, size_t *index)
{
    byte c;

    (*index)++; /* skip '\' */
    if (*index >= data_length)
        return 0; /* Must_not_happen, because the string is PS encoded. */
    c = data[*index];
    switch (c) {
        case '(': return '(';
        case ')': return ')';
        case '\\': return '\\';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'b': return '\b';
        case 'f': return '\f';
        default:
            break;
    }
    if (c >= '0' && c <= '7') {
        int oct_loop;
        /* octal */
        byte v = c - '0';

        /* Octal values should always be three digits, one is consumed above! */
        for (oct_loop = 0;oct_loop < 2; oct_loop++) {
            (*index)++;
            if (*index >= data_length)
                /* Ran out of data, return what we found */
                return v;
            c = data[*index];
            if (c < '0' || c > '7') {
                /* Ran out of numeric data, return what we found */
                /* Need to 'unget' the non-numeric character */
                (*index)--;
                break;
            }
            v = v * 8 + (c - '0');
        }
        return v;
    }
    return c; /* A wrong escapement sequence. */
}

/*
 * Once the bits are split out into bytes of UTF-8, this is a mask OR-ed
 * into the first byte, depending on how many bytes follow.  There are
 * as many entries in this table as there are UTF-8 sequence types.
 * (I.e., one byte sequence, two byte... etc.). Remember that sequencs
 * for *legal* UTF-8 will be 4 or fewer bytes total.
 */
static const char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

static int gs_ConvertUTF16(unsigned char *UTF16, size_t UTF16Len, unsigned char **UTF8Start, int UTF8Len)
{
    size_t i, bytes = 0;
    uint32_t U32 = 0;
    unsigned short U16;
    unsigned char *UTF8 = *UTF8Start;
    unsigned char *UTF8End = UTF8 + UTF8Len;

    if (UTF16Len % sizeof(short) != 0)
        return gs_note_error(gs_error_rangecheck);

    for (i=0;i<UTF16Len / sizeof(short);i++)
    {
        U16 = (*UTF16++) << 8;
        U16 += *UTF16++;

        if (U16 >= 0xD800 && U16 <= 0xDBFF) {
            /* Ensure at least two bytes of input left */
            if (i == (UTF16Len / sizeof(short)) - 1)
                return gs_note_error(gs_error_rangecheck);

            U32 += (U16 & 0x3FF) << 10;
            U16 = (*(UTF16++) << 8);
            U16 += *(UTF16++);
            i++;

            /* Ensure a high order surrogate is followed by a low order surrogate */
            if (U16 < 0xDC00 || U16 > 0xDFFF)
                return gs_note_error(gs_error_rangecheck);

            U32 += (U16 & 0x3FF) | 0x10000;
            bytes = 4;
        } else {
            if (U16 >= 0xDC00 && U16 <= 0xDFFF) {
                /* We got a low order surrogate without a preceding high-order */
                return gs_note_error(gs_error_rangecheck);
            }

            if(U16 < 0x80) {
                bytes = 1;
            } else {
                if (U16 < 0x800) {
                    bytes = 2;
                } else {
                    bytes = 3;
                }
            }
        }

        if (UTF8 + bytes > UTF8End)
            return gs_note_error(gs_error_VMerror);

        /* Write from end to beginning, low bytes first */
        UTF8 += bytes;

        switch(bytes) {
            case 4:
                *--UTF8 = (unsigned char)((U32 | 0x80) & 0xBF);
                U16 = U32 >> 6;
            case 3:
                *--UTF8 = (unsigned char)((U16 | 0x80) & 0xBF);
                U16 >>= 6;
            case 2:
                *--UTF8 = (unsigned char)((U16 | 0x80) & 0xBF);
                U16 >>= 6;
            case 1:
                *--UTF8 = (unsigned char)(U16 | firstByteMark[bytes]);
                break;
            default:
                return gs_note_error(gs_error_rangecheck);
        }

        /* Move to start of next set */
        UTF8 += bytes;
    }
    *UTF8Start = UTF8;
    return 0;
}

int
pdf_xmp_write_translated(gx_device_pdf *pdev, stream *s, const byte *data, int data_length,
                         void(*write)(stream *s, const byte *data, int data_length))
{
    int code = 0;
    size_t i, j=0;
    unsigned char *buf0 = NULL, *buf1 = NULL;

    if (data_length == 0)
        return 0;

    buf0 = (unsigned char *)gs_alloc_bytes(pdev->memory, data_length * sizeof(unsigned char),
                    "pdf_xmp_write_translated");
    if (buf0 == NULL)
        return_error(gs_error_VMerror);
    for (i = 0; i < (size_t)data_length; i++) {
        byte c = data[i];

        if (c == '\\')
            c = decode_escape(data, data_length, &i);
        buf0[j] = c;
        j++;
    }
    if (buf0[0] != 0xfe || buf0[1] != 0xff) {
        /* We must assume that the information is PDFDocEncoding. In this case
         * we need to convert it into UTF-8. If we just convert it to UTF-16
         * then we can safely fall through to the code below.
         */
        /* NB the code below skips the BOM in positions 0 and 1, so we need
         * two extra bytes, to be ignored.
         */
        buf1 = (unsigned char *)gs_alloc_bytes(pdev->memory, (j * sizeof(short)) + 2,
                    "pdf_xmp_write_translated");
        if (buf1 == NULL) {
            gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
            return_error(gs_error_VMerror);
        }
        memset(buf1, 0x00, (j * sizeof(short)) + 2);
        for (i = 0; i < j; i++) {
            if ((buf0[i] >= 0x20 && buf0[i] < 0x7F) || buf0[i] >= 0xAE)
                buf1[(i * 2) + 3] = buf0[i];
            else {
                if (buf0[i] == 0x7F) {
                    emprintf1(pdev->memory, "PDFDocEncoding %x is undefined\n", buf0[i]);
                    code = gs_note_error(gs_error_rangecheck);
                    goto error;
                }
                else {
                    if (buf0[i] < 0x20) {
                        buf1[(i * 2) + 2] = PDFDocEncodingLookupLo[(buf0[i]) * 2];
                        buf1[(i * 2) + 3] = PDFDocEncodingLookupLo[((buf0[i]) * 2) + 1];
                        if (PDFDocEncodingLookupLo[((buf0[i]) * 2) + 1] == 0x00) {
                            emprintf1(pdev->memory, "PDFDocEncoding %x is undefined\n", buf0[i]);
                            code = gs_note_error(gs_error_rangecheck);
                            goto error;
                        }
                    } else {
                        buf1[(i * 2) + 2] = PDFDocEncodingLookupHi[(buf0[i] - 0x80) * 2];
                        buf1[(i * 2) + 3] = PDFDocEncodingLookupHi[((buf0[i] - 0x80) * 2) + 1];
                        if (PDFDocEncodingLookupHi[((buf0[i] - 0x80) * 2) + 1] == 0x00) {
                            emprintf1(pdev->memory, "PDFDocEncoding %x is undefined\n", buf0[i]);
                            code = gs_note_error(gs_error_rangecheck);
                            goto error;
                        }
                    }
                }
            }
        }
        gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
        buf0 = buf1;
        data_length = j = (j * 2) + 2;
    }
    {
        /* Its a Unicode (UTF-16BE) string, convert to UTF-8 */
        short *buf0b;
        char *buf1, *buf1b;
        int code;

        /* A single UTF-16 (2 bytes) can end up as 4 bytes in UTF-8 */
        buf1 = (char *)gs_alloc_bytes(pdev->memory, data_length * 2 * sizeof(unsigned char),
                    "pdf_xmp_write_translated");
        if (buf1 == NULL) {
            gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
            return_error(gs_error_VMerror);
        }
        buf1b = buf1;
        /* Skip the Byte Order Mark (0xfe 0xff) */
        buf0b = (short *)(buf0 + 2);
        code = gs_ConvertUTF16((unsigned char *)buf0b, j - 2, (unsigned char **)&buf1b, data_length * 2 * sizeof(unsigned char));
        if (code < 0) {
            gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
            gs_free_object(pdev->memory, buf1, "pdf_xmp_write_translated");
            return code;
        }

        /* s and write can be NULL in order to use this function to test whether the specified data can be converted to UTF8 */
        if (s && write)
            write(s, (const byte*)buf1, buf1b - buf1);

        gs_free_object(pdev->memory, buf1, "pdf_xmp_write_translated");
    }
    gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
    return 0;

error:
    gs_free_object(pdev->memory, buf0, "pdf_xmp_write_translated");
    gs_free_object(pdev->memory, buf1, "pdf_xmp_write_translated");
    return code;
}

static int
pdf_xmp_write_docinfo_item(gx_device_pdf *pdev, stream *s, const char *key, const char *default_value,
                           void(*write)(stream *s, const byte *data, int data_length))
{
    const cos_value_t *v = cos_dict_find(pdev->Info, (const byte *)key, strlen(key));

    if (v != NULL && (v->value_type == COS_VALUE_SCALAR ||
                        v->value_type == COS_VALUE_CONST)) {
        if (v->contents.chars.size >= 2 && v->contents.chars.data[0] == '(')
            return pdf_xmp_write_translated(pdev, s, v->contents.chars.data + 1,
                        v->contents.chars.size - 2, write);
        else
            return pdf_xmp_write_translated(pdev, s, v->contents.chars.data,
                        v->contents.chars.size, write);
    } else {
        stream_puts(s, default_value);
        return 0;
    }
}

static uint64_t
pdf_uuid_time(gx_device_pdf *pdev)
{
    long *dt = pdev->uuid_time; /* In seconds since Jan. 1, 1980 and fraction in nanoseconds. */
    uint64_t t;

    /* UUIDs use time in 100ns ticks since Oct 15, 1582. */
    t = (uint64_t)10000000 * dt[0] + dt[0] / 100; /* since Jan. 1, 1980 */
    t += (uint64_t) (1000*1000*10)         /* seconds */
       * (uint64_t) (60 * 60 * 24)         /* days */
       * (uint64_t) (17+30+31+365*397+99); /* # of days */
    return t;
}

static void writehex(char **p, ulong v, int l)
{
    int i = l * 2;
    static const char digit[] = "0123456789abcdef";

    for (; i--;)
        *((*p)++) = digit[v >> (i * 4) & 15];
}

static void
pdf_make_uuid(const byte node[6], uint64_t uuid_time, ulong time_seq, char *buf, int buf_length)
{
    char b[45], *p = b;
    ulong  uuid_time_lo = (ulong)(uuid_time & 0xFFFFFFFF);       /* MSVC 7.1.3088           */
    ushort uuid_time_md = (ushort)((uuid_time >> 32) & 0xFFFF);  /* cannot compile this     */
    ushort uuid_time_hi = (ushort)((uuid_time >> 48) & 0x0FFF);  /* as function arguments.  */

    writehex(&p, uuid_time_lo, 4); /* time_low */
    *(p++) = '-';
    writehex(&p, uuid_time_md, 2); /* time_mid */
    *(p++) = '-';
    writehex(&p, uuid_time_hi | (ushort)(1 << 12), 2); /* time_hi_and_version */
    *(p++) = '-';
    writehex(&p, (time_seq & 0x3F00) >> 8, 1); /* clock_seq_hi_and_reserved */
    writehex(&p, time_seq & 0xFF, 1); /* clock_seq & 0xFF */
    *(p++) = '-';
    writehex(&p, node[0], 1);
    writehex(&p, node[1], 1);
    writehex(&p, node[2], 1);
    writehex(&p, node[3], 1);
    writehex(&p, node[4], 1);
    writehex(&p, node[5], 1);
    *p = 0;
    strncpy(buf, b, strlen(b) + 1);
}

static int
pdf_make_instance_uuid(gx_device_pdf *pdev, const byte digest[6], char *buf, int buf_length)
{
    char URI_prefix[5] = "uuid:";

    memcpy(buf, URI_prefix, 5);
    if (pdev->InstanceUUID.size) {
        int l = min(buf_length - 6, pdev->InstanceUUID.size);

        memcpy(buf+5, pdev->InstanceUUID.data, l);
        buf[l+5] = 0;
    } else
        pdf_make_uuid(digest, pdf_uuid_time(pdev), pdev->DocumentTimeSeq, buf + 5, buf_length - 5);
    return 0;
}

static int
pdf_make_document_uuid(gx_device_pdf *pdev, const byte digest[6], char *buf, int buf_length)
{
    char URI_prefix[5] = "uuid:";

    memcpy(buf, URI_prefix, 5);
    if (pdev->DocumentUUID.size) {
        int l = min(buf_length - 6, pdev->DocumentUUID.size);

        memcpy(buf+5, pdev->DocumentUUID.data, l);
        buf[l+5] = 0;
    } else
        pdf_make_uuid(digest, pdf_uuid_time(pdev), pdev->DocumentTimeSeq, buf+5, buf_length - 5);
    return 0;
}

static const char dd[]={'\'', '\357', '\273', '\277', '\'', 0};

/* --------------------------------------------  */

/* Write Document metadata */
static int
pdf_write_document_metadata(gx_device_pdf *pdev, const byte digest[6])
{
    char instance_uuid[45], document_uuid[45], cre_date_time[40], mod_date_time[40], date_time_buf[40];
    int cre_date_time_len, mod_date_time_len;
    int code;
    stream *s = pdev->strm;

    code = pdf_make_instance_uuid(pdev, digest, instance_uuid, sizeof(instance_uuid));
    if (code < 0)
        return code;
    code = pdf_make_document_uuid(pdev, digest, document_uuid, sizeof(document_uuid));
    if (code < 0)
        return code;

    /* PDF/A XMP reference recommends setting UUID to empty. If not empty must be a URI */
    if (pdev->PDFA != 0)
        instance_uuid[0] = 0x00;

    cre_date_time_len = pdf_get_docinfo_item(pdev, "/CreationDate", cre_date_time, sizeof(cre_date_time));
    if (!cre_date_time_len)
        cre_date_time_len = pdf_xmp_time(cre_date_time, sizeof(cre_date_time));
    else
        cre_date_time_len = pdf_xmp_convert_time(cre_date_time, cre_date_time_len, date_time_buf, sizeof(date_time_buf));
    mod_date_time_len = pdf_get_docinfo_item(pdev, "/ModDate", mod_date_time, sizeof(mod_date_time));
    if (!mod_date_time_len)
        mod_date_time_len = pdf_xmp_time(mod_date_time, sizeof(mod_date_time));
    else
        mod_date_time_len = pdf_xmp_convert_time(mod_date_time, mod_date_time_len, date_time_buf, sizeof(date_time_buf));

    pdf_xml_ins_beg(s, "xpacket");
    pdf_xml_attribute_name(s, "begin");
    pdf_xml_copy(s, dd);
    pdf_xml_attribute_name(s, "id");
    pdf_xml_attribute_value(s, "W5M0MpCehiHzreSzNTczkc9d");
    pdf_xml_ins_end(s);
    pdf_xml_newline(s);

    pdf_xml_copy(s, "<?adobe-xap-filters esc=\"CRLF\"?>\n");
    pdf_xml_copy(s, "<x:xmpmeta xmlns:x='adobe:ns:meta/'"
                              " x:xmptk='XMP toolkit 2.9.1-13, framework 1.6'>\n");
    {
        pdf_xml_copy(s, "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#' "
                                 "xmlns:iX='http://ns.adobe.com/iX/1.0/'>\n");
        {

            pdf_xml_tag_open_beg(s, "rdf:Description");
            pdf_xml_copy(s, " rdf:about=\"\"");
            pdf_xml_attribute_name(s, "xmlns:pdf");
            pdf_xml_attribute_value(s, "http://ns.adobe.com/pdf/1.3/");

            if (cos_dict_find(pdev->Info, (const byte *)"/Keywords", 9)) {
                pdf_xml_tag_end(s);
                pdf_xml_tag_open_beg(s, "pdf:Producer");
                pdf_xml_tag_end(s);
                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Producer", "UnknownProducer",
                        pdf_xml_data_write);
                if (code < 0)
                    return code;
                pdf_xml_tag_close(s, "pdf:Producer");
                pdf_xml_newline(s);

                pdf_xml_tag_open_beg(s, "pdf:Keywords");
                pdf_xml_tag_end(s);
                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Keywords", "Unknown",
                        pdf_xml_data_write);
                if (code < 0)
                    return code;
                pdf_xml_tag_close(s, "pdf:Keywords");
                pdf_xml_newline(s);

                pdf_xml_tag_close(s, "rdf:Description");
                pdf_xml_newline(s);
            } else {
                pdf_xml_attribute_name(s, "pdf:Producer");
                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Producer", "UnknownProducer",
                        pdf_xml_attribute_value_data);
                if (code < 0)
                    return code;
                pdf_xml_tag_end_empty(s);
                pdf_xml_newline(s);
            }

            pdf_xml_tag_open_beg(s, "rdf:Description");
            pdf_xml_copy(s, " rdf:about=\"\"");
            pdf_xml_attribute_name(s, "xmlns:xmp");
            pdf_xml_attribute_value(s, "http://ns.adobe.com/xap/1.0/");
            pdf_xml_tag_end(s);
            if (!pdev->OmitInfoDate) {
                {
                    pdf_xml_tag_open_beg(s, "xmp:ModifyDate");
                    pdf_xml_tag_end(s);
                    mod_date_time[mod_date_time_len] = 0x00;
                    pdf_xml_copy(s, mod_date_time);
                    pdf_xml_tag_close(s, "xmp:ModifyDate");
                    pdf_xml_newline(s);
                }
                {
                    pdf_xml_tag_open_beg(s, "xmp:CreateDate");
                    pdf_xml_tag_end(s);
                    cre_date_time[cre_date_time_len] = 0x00;
                    pdf_xml_copy(s, cre_date_time);
                    pdf_xml_tag_close(s, "xmp:CreateDate");
                    pdf_xml_newline(s);
                }
            }
            {
                pdf_xml_tag_open_beg(s, "xmp:CreatorTool");
                pdf_xml_tag_end(s);
                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Creator", "UnknownApplication",
                        pdf_xml_data_write);
                if (code < 0)
                    return code;
                pdf_xml_tag_close(s, "xmp:CreatorTool");
            }
            pdf_xml_tag_close(s, "rdf:Description");
            pdf_xml_newline(s);

            pdf_xml_tag_open_beg(s, "rdf:Description");
            pdf_xml_copy(s, " rdf:about=\"\"");
            pdf_xml_attribute_name(s, "xmlns:xapMM");
            pdf_xml_attribute_value(s, "http://ns.adobe.com/xap/1.0/mm/");
            pdf_xml_attribute_name(s, "xapMM:DocumentID");
            pdf_xml_attribute_value(s, document_uuid);
            pdf_xml_tag_end_empty(s);
            pdf_xml_newline(s);

            pdf_xml_tag_open_beg(s, "rdf:Description");
            pdf_xml_copy(s, " rdf:about=\"\"");
            pdf_xml_attribute_name(s, "xmlns:dc");
            pdf_xml_attribute_value(s, "http://purl.org/dc/elements/1.1/");
            pdf_xml_attribute_name(s, "dc:format");
            pdf_xml_attribute_value(s,"application/pdf");
            pdf_xml_tag_end(s);
            {
                pdf_xml_tag_open(s, "dc:title");
                {
                    pdf_xml_tag_open(s, "rdf:Alt");
                    {
                        pdf_xml_tag_open_beg(s, "rdf:li");
                        pdf_xml_attribute_name(s, "xml:lang");
                        pdf_xml_attribute_value(s, "x-default");
                        pdf_xml_tag_end(s);
                        {
                           code = pdf_xmp_write_docinfo_item(pdev, s,  "/Title", "Untitled",
                                    pdf_xml_data_write);
                            if (code < 0)
                                return code;
                        }
                        pdf_xml_tag_close(s, "rdf:li");
                    }
                    pdf_xml_tag_close(s, "rdf:Alt");
                }
                pdf_xml_tag_close(s, "dc:title");

                if (cos_dict_find(pdev->Info, (const byte *)"/Author", 7)) {
                    pdf_xml_tag_open(s, "dc:creator");
                    {   /* According to the PDF/A specification
                           "it shall be represented by an ordered Text array of
                           length one whose single entry shall consist
                           of one or more names". */
                        pdf_xml_tag_open(s, "rdf:Seq");
                        {
                            pdf_xml_tag_open(s, "rdf:li");
                            {
                                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Author", "Unknown",
                                            pdf_xml_data_write);
                                if (code < 0)
                                    return code;
                            }
                            pdf_xml_tag_close(s, "rdf:li");
                        }
                        pdf_xml_tag_close(s, "rdf:Seq");
                    }
                    pdf_xml_tag_close(s, "dc:creator");
                }
                if (cos_dict_find(pdev->Info, (const byte *)"/Subject", 8)) {
                    pdf_xml_tag_open(s, "dc:description");
                    {
                        pdf_xml_tag_open(s, "rdf:Alt");
                        {
                            pdf_xml_tag_open_beg(s, "rdf:li");
                            pdf_xml_attribute_name(s, "xml:lang");
                            pdf_xml_attribute_value(s, "x-default");
                            pdf_xml_tag_end(s);
                            {
                                code = pdf_xmp_write_docinfo_item(pdev, s,  "/Subject", "No Subject",
                                            pdf_xml_data_write);
                                if (code < 0)
                                    return code;
                            }
                            pdf_xml_tag_close(s, "rdf:li");
                        }
                        pdf_xml_tag_close(s, "rdf:Alt");
                    }
                    pdf_xml_tag_close(s, "dc:description");
                }
            }
            pdf_xml_tag_close(s, "rdf:Description");
            pdf_xml_newline(s);
            if (pdev->PDFA != 0) {
                pdf_xml_tag_open_beg(s, "rdf:Description");
                pdf_xml_copy(s, " rdf:about=\"\"");
                pdf_xml_attribute_name(s, "xmlns:pdfaid");
                pdf_xml_attribute_value(s, "http://www.aiim.org/pdfa/ns/id/");
                pdf_xml_attribute_name(s, "pdfaid:part");
                switch(pdev->PDFA) {
                    case 1:
                        pdf_xml_attribute_value(s,"1");
                        break;
                    case 2:
                        pdf_xml_attribute_value(s,"2");
                        break;
                    case 3:
                        pdf_xml_attribute_value(s,"3");
                        break;
                }
                pdf_xml_attribute_name(s, "pdfaid:conformance");
                pdf_xml_attribute_value(s,"B");
                pdf_xml_tag_end_empty(s);
           }
        }
        if (pdev->ExtensionMetadata) {
            pdf_xml_copy(s, pdev->ExtensionMetadata);
        }
        pdf_xml_copy(s, "</rdf:RDF>\n");
    }
    pdf_xml_copy(s, "</x:xmpmeta>\n");

    pdf_xml_copy(s, "                                                                        \n");
    pdf_xml_copy(s, "                                                                        \n");
    pdf_xml_copy(s, "<?xpacket end='w'?>");
    return 0;
}

int
pdf_document_metadata(gx_device_pdf *pdev)
{
    if (pdev->CompatibilityLevel < 1.4)
        return 0;
    if (cos_dict_find_c_key(pdev->Catalog, "/Metadata"))
        return 0;

    if (pdev->ParseDSCCommentsForDocInfo || pdev->PreserveEPSInfo || pdev->PDFA) {
        pdf_resource_t *pres;
        char buf[20];
        byte digest[6] = {0,0,0,0,0,0};
        int code;
        int options = DATA_STREAM_NOT_BINARY;

        sflush(pdev->strm);
        s_MD5C_get_digest(pdev->strm, digest, sizeof(digest));
        if (pdev->EncryptMetadata)
            options |= DATA_STREAM_ENCRYPT;
        code = pdf_open_aside(pdev, resourceMetadata, gs_no_id, &pres, true, options);
        if (code < 0)
            return code;
        code = cos_dict_put_c_key_string((cos_dict_t *)pres->object, "/Type", (const byte *)"/Metadata", 9);
        if (code < 0) {
            pdf_close_aside(pdev);
            return code;
        }
        code = cos_dict_put_c_key_string((cos_dict_t *)pres->object, "/Subtype", (const byte *)"/XML", 4);
        if (code < 0) {
            pdf_close_aside(pdev);
            return code;
        }
        code = pdf_write_document_metadata(pdev, digest);
        if (code < 0) {
            pdf_close_aside(pdev);
            return code;
        }
        code = pdf_close_aside(pdev);
        if (code < 0)
            return code;
        code = COS_WRITE_OBJECT(pres->object, pdev, resourceNone);
        if (code < 0)
            return code;
        gs_snprintf(buf, sizeof(buf), "%ld 0 R", pres->object->id);
        pdf_record_usage(pdev, pres->object->id, resource_usage_part9_structure);

        code = cos_dict_put_c_key_object(pdev->Catalog, "/Metadata", pres->object);
        if (code < 0)
            return code;

    }
    return 0;
}

/* --------------------------------------------  */
