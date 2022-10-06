/* Copyright (C) 2022 Artifex Software, Inc.
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

/* Top-level API implementation for text file handling */

/* Language wrapper implementation (see pltop.h) */


/* Enable the following for a dump of the codepoints to stdout. */
/* #define DEBUG_CODEPOINTS */

/* Enable the following for a hacky dump of the output PCL to file. */
/* #define DEBUG_DUMP_PCL */

#ifdef DEBUG_DUMP_PCL
#include <stdio.h>
static FILE *debug_pcl_out = NULL;
static void wipe(void)
{
        fclose(debug_pcl_out);
        debug_pcl_out = NULL;
}
static void
debug_as_pcl(const char *p, int n)
{
        if (debug_pcl_out == NULL)
        {
            debug_pcl_out = fopen("debug_pcl_out", "wb");
            atexit(wipe);
        }
        fwrite(p, n, 1, debug_pcl_out);
}
#endif

#include "pltop.h"
#include "plmain.h"

#include "plparse.h" /* for e_ExitLanguage */
#include "plmain.h"
#include "gxdevice.h" /* so we can include gxht.h below */
#include "gserrors.h"
#include "gp.h"
#include "assert_.h"

/*
 * The TXT interpeter is identical to pl_interp_t.
 * The TXT interpreter instance is derived from pl_interp_implementation_t.
 */

typedef enum
{
    TXT_STATE_INIT = 0,
    TXT_STATE_UTF8,
    TXT_STATE_UTF8_MAYBE,
    TXT_STATE_UTF16_LE,
    TXT_STATE_UTF16_BE,
    TXT_STATE_ASCII
} txt_state_t;

typedef struct txt_interp_instance_s txt_interp_instance_t;

struct txt_interp_instance_s
{
    gs_memory_t *memory;                /* memory allocator to use */

    pl_interp_implementation_t *sub;
    gx_device *device;

    int buffered;
    byte buffer[4];

    int state;
    int detected;
    int just_had_lf;
    int just_had_cr;
    int col;
};

enum
{
    TXT_UNDETECTED = -1,
    TXT_UNKNOWN,
    TXT_UTF8,
    TXT_UTF8_MAYBE,
    TXT_UTF16_LE,
    TXT_UTF16_BE,
    TXT_ASCII,
};

static int
identify_from_buffer(const unsigned char *s, int len)
{
    int count_controls = 0;
    int count_hi = 0;
    int count_tabs = 0;
    int plausibly_utf8 = 1;
    int i;

    /* UTF-8 with a BOM */
    if (len >= 3 && s[0] == 0xef && s[1] == 0xbb && s[2] == 0xbf)
        return TXT_UTF8;
    /* UTF-16 (little endian) */
    if (len >= 2 && s[0] == 0xff && s[1] == 0xfe)
        return TXT_UTF16_LE;
    /* UTF-16 (big endian) */
    if (len >= 2 && s[0] == 0xfe && s[1] == 0xff)
        return TXT_UTF16_BE;

    /* Gather some stats. */
    for (i = 0; i < len; i++)
    {
        if (s[i] == 9)
        {
            count_tabs++;
        }
        else if (s[i] == 12)
        {
            /* Form feed. We'll let that slide. */
        }
        else if (s[i] == 10)
        {
           if (i+1 < len && s[i+1] == 13)
                i++;
        }
        else if (s[i] == 13)
        {
           if (i+1 < len && s[i+1] == 10)
                i++;
        }
        else if (s[i] < 32 || s[i] == 0x7f)
        {
            count_controls++;
        }
        else if (s[i] < 0x7f)
        {
            /* Seems like a reasonable ASCII value. */
        }
        else
        {
            count_hi++;
            if ((s[i] & 0xF8) == 0xF0)
            {
                /* 3 following bytes */
                if (i+1 < len && (s[i+1] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else if (i+2 < len && (s[i+2] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else if (i+3 < len && (s[i+3] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else
                    i+=3;
            }
            else if ((s[i] & 0xF0) == 0xE0)
            {
                /* 2 following bytes */
                if (i+1 < len && (s[i+1] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else if (i+2 < len && (s[i+2] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else
                    i+=2;
            }
            else if ((s[i] & 0xE0) == 0xC0)
            {
                /* 1 following bytes */
                if (i+1 < len && (s[i+1] & 0xC0) != 0x80)
                    plausibly_utf8 = 0;
                else
                    i++;
            }
            else
                plausibly_utf8 = 0;
        }
    }

    /* Any (non tab/cr/lf/ff) control characters probably means this isn't text. */
    if (count_controls > 0)
        return TXT_UNKNOWN;
    /* If we've managed to decode all that as utf8 without problem, it's probably text. */
    if (plausibly_utf8)
        return TXT_UTF8_MAYBE;
    /* If we're hitting too many top bit set chars, give up. */
    if (count_hi > len/10)
        return TXT_UNKNOWN;

    return TXT_ASCII;
}

static int
txt_detect_language(const char *t, int len)
{
    const unsigned char *s = (const unsigned char *)t;

    switch (identify_from_buffer(s, len))
    {
    case TXT_UTF8:
    case TXT_UTF16_LE:
    case TXT_UTF16_BE:
        /* PCL spots files with lots of ESCs in them at confidence
         * level 80. We'll use 70, cos we don't want to override that. */
        return 70;
    case TXT_UTF8_MAYBE:
    case TXT_ASCII:
        return 60;
    default:
    case TXT_UNKNOWN:
        break;
    }

    return 0;
}

static const pl_interp_characteristics_t *
txt_impl_characteristics(const pl_interp_implementation_t *pimpl)
{
    static pl_interp_characteristics_t txt_characteristics =
    {
        "TXT",
        txt_detect_language,
    };
    return &txt_characteristics;
}

/* Do per-instance interpreter allocation/init. No device is set yet */
static int
txt_impl_allocate_interp_instance(pl_interp_implementation_t *impl,
                                  gs_memory_t *pmem)
{
    txt_interp_instance_t *instance;

    instance = (txt_interp_instance_t *) gs_alloc_bytes(pmem,
            sizeof(txt_interp_instance_t), "txt_impl_allocate_interp_instance");

    if (!instance)
        return_error(gs_error_VMerror);

    instance->memory = pmem;
    instance->sub = NULL;

    impl->interp_client_data = instance;

    return 0;
}

/* Prepare interp instance for the next "job" */
static int
txt_impl_init_job(pl_interp_implementation_t *impl,
                  gx_device                  *pdevice)
{
    txt_interp_instance_t *instance = impl->interp_client_data;

    instance->device = pdevice;
    instance->state = TXT_STATE_INIT;
    instance->buffered = 0;
    instance->detected = TXT_UNDETECTED;
    instance->just_had_lf = 0;
    instance->just_had_cr = 0;
    instance->col = 0;

    instance->sub = pl_main_get_pcl_instance(instance->memory);

    return pl_init_job(instance->sub, instance->device);
}

#define ESC 27

static int
send_bytes(txt_interp_instance_t *instance, const byte *p, int n)
{
    stream_cursor_read cursor;

#ifdef DEBUG_DUMP_PCL
    debug_as_pcl(p, n);
#endif

    cursor.ptr = p-1;
    cursor.limit = cursor.ptr + n;

    return instance->sub->proc_process(instance->sub, &cursor);
}

static void
drop_buffered(txt_interp_instance_t *instance, int n)
{
    assert(instance->buffered >= n);
    instance->buffered -= n;
    if (instance->buffered > 0)
        memmove(instance->buffer, &instance->buffer[n], instance->buffered);
}

static int
send_pcl_init(txt_interp_instance_t *instance)
{
    static byte init[] = {
            ESC, 'E',                     // Reset
            ESC, '&', 'l', '0', 'O',      // Orientation
            ESC, '&', 'k', '1', '0', 'H', // Horizontal spacing 10/120 of an inch.
            ESC, '&', 'l', '8', 'C',      // Vertical line spacing 8/48 of an inch.
            ESC, '&', 't', '8', '3', 'P', // &t = double byte parsing, 83 = utf-8, P = ?
            ESC, '(', '1', '8', 'N',      // Primary symbol set = 18N = Unicode
            ESC, '(', 's', '0', 'P',      // Fixed pitch
            ESC, '(', 's', '1', '2', 'H', // Secondary fixed pitch 12cpi
            ESC, '(', 's', '8', 'V',      // Point size 8
            ESC, '(', 's', '3', 'T',      // Typeface number 3
            ESC, '&', 's', '0', 'C'       // Wrappity wrap wrap
    };

    return send_bytes(instance, init, sizeof(init));
}

static int
send_urc(txt_interp_instance_t *instance, int n)
{
    static byte unicode_replacement_char_as_utf8[] = { 0xe3, 0xbf, 0xbd };

    if (instance->state == TXT_STATE_UTF8_MAYBE)
    {
        /* We were guessing that this was UTF8. Now we know it's not. Drop back to ascii. */
        instance->state = TXT_STATE_ASCII;
        return 0;
    }

    drop_buffered(instance, n);

    return send_bytes(instance, unicode_replacement_char_as_utf8, sizeof(unicode_replacement_char_as_utf8));
}

static int
send_utf8(txt_interp_instance_t *instance, int val)
{
    byte buf[4];
    int n;

    /* Finally, send the val! */
    if (val < 0x80)
    {
        buf[0] = val;
        n = 1;
    }
    else if (val < 0x800)
    {
        buf[0] = 0xC0 + (val>>6);
        buf[1] = 0x80 + (val & 0x3F);
        n = 2;
    }
    else if (val < 0x10000)
    {
        buf[0] = 0xE0 + (val>>12);
        buf[1] = 0x80 + ((val>>6) & 0x3F);
        buf[2] = 0x80 + (val & 0x3F);
        n = 3;
    }
    else
    {
        buf[0] = 0xF0 + (val>>18);
        buf[1] = 0x80 + ((val>>12) & 0x3F);
        buf[2] = 0x80 + ((val>>6) & 0x3F);
        buf[3] = 0x80 + (val & 0x3F);
        n = 4;
    }
    return send_bytes(instance, buf, n);
}

/* All our actual codepoints should flow through here. So this is where
 * we do the housekeeping. */
static int
send_codepoint(txt_interp_instance_t *instance, int val)
{
    int code;

#ifdef DEBUG_CODEPOINTS
    dprintf3("Sending codepoint %d (%x) %c\n", val, val, val >= 32 && val <= 255 && val != 127 ? val : '.');
#endif

    /* Tidy up whatever mess of CR/LF we are passed. */
    if (val == '\r')
    {
        /* If we've got a CR and we've just had a LF, swallow this. */
        if (instance->just_had_lf)
        {
            instance->just_had_lf = 0;
            return 0;
        }
        instance->just_had_cr = 1;
        val = '\n';
    }
    else if (val == '\n')
    {
        /* If we've got a LF and we've just had a CR, swallow this. */
        if (instance->just_had_cr)
        {
            instance->just_had_cr = 0;
            return 0;
        }
        instance->just_had_lf = 1;
    }
    else
    {
        instance->just_had_cr = 0;
        instance->just_had_lf = 0;
    }

    /* Keep track of what column we're at to so we can do tab handling. */
    if (val == '\n')
    {
        instance->col = 0;
        code = send_utf8(instance, '\n');
        if (code < 0)
            return code;
        return send_utf8(instance, '\r');
    }
    if (val == '\t')
    {
        int spaces = 8 - (instance->col & 7);
        while (spaces--)
        {
            int code = send_utf8(instance, ' ');
            if (code < 0)
                return code;
            instance->col++;
        }
        return 0;
    }
    instance->col++;

#if 0
    /* No need for this as PCL line wrapping works for us. If PCL ever
     * decides to wrap at a number of columns that aren't a multiple of
     * 8 then we'll need to do it manually again!. */
    if (instance->col == 80)
    {
        instance->col = 0;
        code = send_utf8(instance, '\n');
        if (code < 0)
            return code;
        return send_utf8(instance, '\r');
    }
#endif

    return send_utf8(instance, val);
}

static int
process_block(txt_interp_instance_t *instance, const byte *ptr, int n)
{
    int code;
    byte *s = &instance->buffer[0];
    int old_state = instance->state;
    int val;

    if (instance->detected == TXT_UNDETECTED)
    {
        instance->detected = identify_from_buffer(ptr, n);
        /* If we're thinking we're ASCII, go straight there. Otherwise, we'll let the
         * BOM detection below run its course. */
        if (instance->detected == TXT_ASCII)
            instance->state = TXT_STATE_ASCII;
    }

    while (n)
    {
        if (instance->state == old_state)
        {
            assert(instance->buffered < 4);
            s[instance->buffered++] = *ptr++;
            n--;
        }
        old_state = instance->state;

        switch (instance->state)
        {
        case TXT_STATE_INIT:

            if (instance->buffered == 3 && s[0] == 0xef && s[1] == 0xbb && s[2] == 0xbf)
            {
                instance->state = TXT_STATE_UTF8;
            }
            else if (instance->buffered == 2 && s[0] == 0xff && s[1] == 0xfe)
            {
                instance->state = TXT_STATE_UTF16_LE;
            }
            else if (instance->buffered == 2 && s[0] == 0xfe && s[1] == 0xff)
            {
                instance->state = TXT_STATE_UTF16_BE;
            }
            else if (instance->buffered >= 3)
            {
                /* We haven't found a BOM, try for utf8. */
                instance->state = TXT_STATE_UTF8_MAYBE;
            }

            /* If we've recognised the BOM, then send the init string. */
            if (instance->state != TXT_STATE_INIT)
            {
                code = send_pcl_init(instance);
                if (code < 0)
                    return code;
            }
            break;
        case TXT_STATE_UTF8:
        case TXT_STATE_UTF8_MAYBE:
            if ((s[0] & 0xF8) == 0xF0)
            {
                /* 3 following bytes */
                if (instance->buffered >= 2 && (s[1] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 1);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered >= 3 && (s[2] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 2);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered == 4 && (s[3] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 3);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered == 4)
                {
                    /* Valid encoding of 4 bytes */
                    val = ((s[0] & 0x7)<<18) | ((s[1] & 0x3f)<<12) | ((s[2] & 0x3f)<<6) |  (s[3] & 0x3f);
                    drop_buffered(instance, 4);
                    code = send_codepoint(instance, val);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered != 1 && instance->buffered != 2 && instance->buffered != 3)
                {
                    /* Should never happen. */
                    return_error(gs_error_Fatal);
                }
            }
            else if ((s[0] & 0xF0) == 0xE0)
            {
                /* 2 following bytes */
                if (instance->buffered >= 2 && (s[1] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 1);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered >= 3 && (s[2] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 2);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered == 3)
                {
                    /* Valid encoding of 3 bytes */
                    val = ((s[0] & 0xF)<<12) | ((s[1] & 0x3f)<<6) | (s[2] & 0x3f);
                    drop_buffered(instance, 3);
                    code = send_codepoint(instance, val);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered != 1 && instance->buffered != 2)
                {
                    /* Should never happen. */
                    return_error(gs_error_Fatal);
                }
            }
            else if ((s[0] & 0xE0) == 0xC0)
            {
                /* 1 following bytes */
                if (instance->buffered >= 2 && (s[1] & 0xC0) != 0x80)
                {
                    code = send_urc(instance, 1);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered == 2)
                {
                    /* Valid encoding of 2 bytes */
                    val = ((s[0] & 0x1F)<<6) | (s[1] & 0x3f);
                    drop_buffered(instance, 2);
                    code = send_codepoint(instance, val);
                    if (code < 0)
                        return code;
                }
                else if (instance->buffered != 1)
                {
                    /* Should never happen. */
                    return_error(gs_error_Fatal);
                }
            }
            else if ((s[0] & 0xC0) == 0x80)
            {
                /* A continuation byte at the start. Should never see this. */
                code = send_urc(instance, 1);
                if (code < 0)
                    return code;
            }
            else if (s[0] < 0x80)
            {
                /* Simple byte. */
                val = s[0];
                drop_buffered(instance, 1);
                code = send_codepoint(instance, val);
                if (code < 0)
                    return code;
            }
            else
            {
                /* Bytes we should never see in a UTF-8 file! (0xf8-0xff) */
                code = send_urc(instance, 1);
                if (code < 0)
                    return code;
            }
            break;
        case TXT_STATE_UTF16_LE:
            if (instance->buffered < 2)
                break;
            if (s[1] >= 0xD8 && s[1] < 0xDC)
            {
                /* High surrogate */
                if (instance->buffered < 4)
                    break;
                if (s[3] < 0xDC || s[3] > 0xDF)
                {
                    /* Not followed by a low surrogate! Ignore the high surrogate. */
                    code = send_urc(instance, 2);
                    if (code < 0)
                        return code;
                }
                val = (((s[0] | (s[1]<<8)) - 0xdc00)<<10) + (s[2] | (s[3]<<8)) - 0xdc00 + 0x10000;
                drop_buffered(instance, 4);
            }
            else
            {
                val = s[0] | (s[1]<<8);
                drop_buffered(instance, 2);
            }
            code = send_codepoint(instance, val);
            if (code < 0)
                return code;
            break;
        case TXT_STATE_UTF16_BE:
            if (instance->buffered < 2)
                break;
            if (s[0] >= 0xD8 && s[0] < 0xDC)
            {
                /* High surrogate */
                if (instance->buffered < 4)
                    break;
                if (s[2] < 0xDC || s[2] > 0xDF)
                {
                    /* Not followed by a low surrogate! Ignore the high surrogate. */
                    code = send_urc(instance, 2);
                    if (code < 0)
                        return code;
                }
                val = (((s[1] | (s[0]<<8)) - 0xdc00)<<10) + (s[3] | (s[2]<<8)) - 0xdc00 + 0x10000;
                drop_buffered(instance, 4);
            }
            else
            {
                val = s[1] | (s[0]<<8);
                drop_buffered(instance, 2);
            }
            code = send_codepoint(instance, val);
            if (code < 0)
                return code;
            break;
        case TXT_STATE_ASCII:
            do
            {
                code = send_codepoint(instance, s[0]);
                if (code < 0)
                    return code;
                drop_buffered(instance, 1);
            }
            while (instance->buffered > 0);
            break;
        default:
            return_error(gs_error_Fatal);
        }
    }
    return 0;
}

/* Parse an entire random access file */
#if 0
static int
txt_impl_process_file(pl_interp_implementation_t *impl, const char *filename)
{
    txt_interp_instance_t *instance = impl->interp_client_data;
    int code, code1;
    gp_file *file;

    file = gp_fopen(instance->memory, filename, "rb");
    if (file == 0)
        return_error(gs_error_ioerror);

    instance->sub = pl_main_get_pcl_instance(instance->memory);

    code = pl_init_job(instance->sub, instance->device);
    if (code >= 0)
    {
        code = pl_process_file(instance->sub, filename);
    }

    code1 = pl_dnit_job(instance->sub);
    if (code >= 0)
        code = code1;

    gp_fclose(file);

    return code;
}
#endif

/* Do any setup for parser per-cursor */
static int                      /* ret 0 or +ve if ok, else -ve error code */
txt_impl_process_begin(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Parse a cursor-full of data */
static int
txt_impl_process(pl_interp_implementation_t *impl, stream_cursor_read *cursor)
{
    txt_interp_instance_t *instance = impl->interp_client_data;
    int avail;
    int code;

    avail = cursor->limit - cursor->ptr;
    code = process_block(instance, cursor->ptr + 1, avail);
    cursor->ptr = cursor->limit;

    return code;
}

static int                      /* ret 0 or +ve if ok, else -ve error code */
txt_impl_process_end(pl_interp_implementation_t * impl)
{
    return 0;
}

/* Skip to end of job.
 * Return 1 if done, 0 ok but EOJ not found, else negative error code.
 */
static int
txt_impl_flush_to_eoj(pl_interp_implementation_t *impl, stream_cursor_read *pcursor)
{
    /* assume SO files cannot be pjl embedded */
    pcursor->ptr = pcursor->limit;
    return 0;
}

/* Parser action for end-of-file */
static int
txt_impl_process_eof(pl_interp_implementation_t *impl)
{
    txt_interp_instance_t *instance = impl->interp_client_data;

    if (instance->sub)
        return pl_process_eof(instance->sub);

    return 0;
}

/* Report any errors after running a job */
static int
txt_impl_report_errors(pl_interp_implementation_t *impl,
                       int code,           /* prev termination status */
                       long file_position, /* file position of error, -1 if unknown */
                       bool force_to_cout  /* force errors to cout */
                       )
{
    txt_interp_instance_t *instance = impl->interp_client_data;
    int ret = 0;

    if (instance->sub)
        ret = pl_report_errors(instance->sub, code, file_position, force_to_cout);

    return ret;
}

/* Wrap up interp instance after a "job" */
static int
txt_impl_dnit_job(pl_interp_implementation_t *impl)
{
    txt_interp_instance_t *instance = impl->interp_client_data;
    int code = 0;

    if (instance->sub)
        code = pl_dnit_job(instance->sub);
    instance->sub = NULL;
    instance->device = NULL;

    return code;
}

/* Deallocate a interpreter instance */
static int
txt_impl_deallocate_interp_instance(pl_interp_implementation_t *impl)
{
    txt_interp_instance_t *instance = impl->interp_client_data;

    gs_free_object(instance->memory, instance, "so_impl_deallocate_interp_instance");

    return 0;
}

/* Parser implementation descriptor */
pl_interp_implementation_t txt_implementation =
{
    txt_impl_characteristics,
    txt_impl_allocate_interp_instance,
    NULL,                       /* get_device_memory */
    NULL,                       /* set_param */
    NULL,                       /* add_path */
    NULL,                       /* post_args_init */
    txt_impl_init_job,
    NULL,                       /* run_prefix_commands */
    NULL,                       /* txt_impl_process_file, */
    txt_impl_process_begin,
    txt_impl_process,
    txt_impl_process_end,
    txt_impl_flush_to_eoj,
    txt_impl_process_eof,
    txt_impl_report_errors,
    txt_impl_dnit_job,
    txt_impl_deallocate_interp_instance,
    NULL,
};
