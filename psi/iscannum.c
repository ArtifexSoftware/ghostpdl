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


/* Number scanner for Ghostscript interpreter */
#include "math_.h"
#include "ghost.h"
#include "ierrors.h"
#include "scommon.h"
#include "iscan.h"
#include "iscannum.h"		/* defines interface */
#include "scanchar.h"
#include "store.h"

/*
 * Warning: this file has a "spaghetti" control structure.  But since this
 * code accounts for over 10% of the execution time of some PostScript
 * files, this is one of the few places we feel this is justified.
 */

/*
 * Scan a number.  If the number consumes the entire string, return 0;
 * if not, set *psp to the first character beyond the number and return 1.
 */
int
scan_number(const byte * str, const byte * end, int sign,
            ref * pref, const byte ** psp, int scanner_options)
{
    const byte *sp = str;
#define GET_NEXT(cvar, sp, end_action)\
  if (sp >= end) { end_action; } else cvar = *sp++

    /*
     * Powers of 10 up to 6 can be represented accurately as
     * a single-precision float.
     */
#define NUM_POWERS_10 6
    static const float powers_10[NUM_POWERS_10 + 1] = {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6
    };
    static const double neg_powers_10[NUM_POWERS_10 + 1] = {
        1e0, 1e-1, 1e-2, 1e-3, 1e-4, 1e-5, 1e-6
    };

    ps_int ival;
    double dval;
    int exp10;
    int code = 0;
    int c, d;
    ps_uint max_scan; /* max signed or unsigned int */
    ps_int max_ps_int_scan, min_ps_int_scan;
    const byte *const decoder = scan_char_decoder;
#define IS_DIGIT(d, c)\
  ((d = decoder[c]) < 10)
#define WOULD_OVERFLOW(val, d, maxv)\
  (val >= maxv / 10 && (val > maxv / 10 || d > (int64_t)(maxv % 10)))

    GET_NEXT(c, sp, return_error(gs_error_syntaxerror));
    if (!IS_DIGIT(d, c)) {
        if (c != '.')
            return_error(gs_error_syntaxerror);
        /* Might be a number starting with '.'. */
        GET_NEXT(c, sp, return_error(gs_error_syntaxerror));
        if (!IS_DIGIT(d, c))
            return_error(gs_error_syntaxerror);
        ival = 0;
        goto i2r;
    }
    /* Accumulate an integer in ival. */
    /* Do up to 4 digits without a loop, */
    /* since we know this can't overflow and since */
    /* most numbers have 4 (integer) digits or fewer. */
    ival = d;
    if (end - sp >= 3) {	/* just check once */
        if (!IS_DIGIT(d, (c = *sp))) {
            sp++;
            goto ind;
        }
        ival = ival * 10 + d;
        if (!IS_DIGIT(d, (c = sp[1]))) {
            sp += 2;
            goto ind;
        }
        ival = ival * 10 + d;
        sp += 3;
        if (!IS_DIGIT(d, (c = sp[-1])))
            goto ind;
        ival = ival * 10 + d;
    }

    max_ps_int_scan = scanner_options & SCAN_CPSI_MODE ? MAX_PS_INT32 : MAX_PS_INT;
    min_ps_int_scan = scanner_options & SCAN_CPSI_MODE ? MIN_PS_INT32 : MIN_PS_INT;

    max_scan = scanner_options & SCAN_PDF_UNSIGNED && sign >= 0 ? ~((ps_int)0) : max_ps_int_scan;

    for (;; ival = ival * 10 + d) {
        GET_NEXT(c, sp, goto iret);
        if (!IS_DIGIT(d, c))
            break;
        if (WOULD_OVERFLOW(((ps_uint)ival), d, max_scan)) {
            if (ival == max_ps_int_scan / 10 && d == (max_ps_int_scan % 10) + 1 && sign < 0) {
                GET_NEXT(c, sp, c = EOFC);
                dval = -(double)min_ps_int_scan;
                if (c == 'e' || c == 'E') {
                    exp10 = 0;
                    goto fs;
                } else if (c == '.') {
                    GET_NEXT(c, sp, c = EOFC);
                    exp10 = 0;
                    goto fd;
                } else if (!IS_DIGIT(d, c)) {
                    ival = min_ps_int_scan;
                    break;
                }
            } else
                dval = (double)ival;
            goto l2d;
        }
    }
  ind:				/* We saw a non-digit while accumulating an integer in ival. */
    switch (c) {
        case '.':
            GET_NEXT(c, sp, c = EOFC);
            goto i2r;
        default:
            *psp = sp;
            code = 1;
            break;
        case EOFC:
            break;
        case 'e':
        case 'E':
            if (sign < 0)
                ival = -ival;
            dval = (double)ival;
            exp10 = 0;
            goto fe;
        case '#':
            {
                const int radix = ival;
                ps_int uval = 0, imax;

                if (sign || radix < min_radix || radix > max_radix)
                    return_error(gs_error_syntaxerror);
                /* Avoid multiplies for power-of-2 radix. */
                if (!(radix & (radix - 1))) {
                    int shift;

                    switch (radix) {
                        case 2:
                            shift = 1, imax = MAX_PS_UINT >> 1;
                            break;
                        case 4:
                            shift = 2, imax = MAX_PS_UINT >> 2;
                            break;
                        case 8:
                            shift = 3, imax = MAX_PS_UINT >> 3;
                            break;
                        case 16:
                            shift = 4, imax = MAX_PS_UINT >> 4;
                            break;
                        case 32:
                            shift = 5, imax = MAX_PS_UINT >> 5;
                            break;
                        default:	/* can't happen */
                            return_error(gs_error_rangecheck);
                    }
                    for (;; uval = (uval << shift) + d) {
                        GET_NEXT(c, sp, break);
                        d = decoder[c];
                        if (d >= radix) {
                            *psp = sp;
                            code = 1;
                            break;
                        }
                        if (uval > imax)
                            return_error(gs_error_limitcheck);
                    }
                } else {
                    ps_int irem = MAX_PS_UINT % radix;

                    imax = MAX_PS_UINT / radix;
                    for (;; uval = uval * radix + d) {
                        GET_NEXT(c, sp, break);
                        d = decoder[c];
                        if (d >= radix) {
                            *psp = sp;
                            code = 1;
                            break;
                        }
                        if (uval >= imax &&
                            (uval > imax || d > irem)
                            )
                            return_error(gs_error_limitcheck);
                    }
                }
                if (scanner_options & SCAN_CPSI_MODE) {
                    ps_uint32 int1 = 0;
                    int1 |= (uval & 0xffffffff);
                    make_int(pref, (ps_int)((ps_int32)int1));
                }
                else
                    make_int(pref, uval);

                return code;
            }
    }
iret:
    if (scanner_options & SCAN_CPSI_MODE) {
        make_int(pref, (sign < 0 ? (ps_int32)-ival : (ps_int32)ival));
    }
    else {
        make_int(pref, (sign < 0 ? (ps_int)-ival : (ps_int)ival));
    }
    return code;

    /* Accumulate a double in dval. */
l2d:
    exp10 = 0;
    for (;;) {
        dval = dval * 10 + d;
        GET_NEXT(c, sp, c = EOFC);
        if (!IS_DIGIT(d, c))
            break;
    }
    switch (c) {
        case '.':
            GET_NEXT(c, sp, c = EOFC);
            exp10 = 0;
            goto fd;
        default:
            *psp = sp;
            code = 1;
            /* falls through */
        case EOFC:
            if (sign < 0)
                dval = -dval;
            goto rret;
        case 'e':
        case 'E':
            exp10 = 0;
            goto fs;
        case '#':
            return_error(gs_error_syntaxerror);
    }

    /* We saw a '.' while accumulating an integer in ival. */
i2r:
    exp10 = 0;
    while (IS_DIGIT(d, c) || c == '-') {
        /*
         * PostScript gives an error on numbers with a '-' following a '.'
         * Adobe Acrobat Reader (PDF) apparently doesn't treat this as an
         * error. Experiments show that the numbers following the '-' are
         * ignored, so we swallow the fractional part. SCAN_PDF_INV_NUM
         *  enables this compatibility kloodge.
         */
        if (c == '-') {
            if ((SCAN_PDF_INV_NUM & scanner_options) == 0)
                break;
            do {
                GET_NEXT(c, sp, c = EOFC);
            } while (IS_DIGIT(d, c));
            break;
        }
        if (WOULD_OVERFLOW(ival, d, max_int)) {
            dval = (double)ival;
            goto fd;
        }
        ival = ival * 10 + d;
        exp10--;
        GET_NEXT(c, sp, c = EOFC);
    }
    if (sign < 0)
        ival = -ival;
    /* Take a shortcut for the common case */
    if (!(c == 'e' || c == 'E' || exp10 < -NUM_POWERS_10)) {	/* Check for trailing garbage */
        if (c != EOFC)
            *psp = sp, code = 1;
        make_real(pref, ival * neg_powers_10[-exp10]);
        return code;
    }
    dval = (double)ival;
    goto fe;

    /* Now we are accumulating a double in dval. */
fd:
    while (IS_DIGIT(d, c)) {
        dval = dval * 10 + d;
        exp10--;
        GET_NEXT(c, sp, c = EOFC);
    }
fs:
    if (sign < 0)
        dval = -dval;
fe:
    /* Now dval contains the value, negated if necessary. */
    switch (c) {
        case 'e':
        case 'E':
            {			/* Check for a following exponent. */
                int esign = 0;
                int iexp;

                GET_NEXT(c, sp, return_error(gs_error_syntaxerror));
                switch (c) {
                    case '-':
                        esign = 1;
                        /* fall through */
                    case '+':
                        GET_NEXT(c, sp, return_error(gs_error_syntaxerror));
                }
                /* Scan the exponent.  We limit it arbitrarily to 999. */
                if (!IS_DIGIT(d, c))
                    return_error(gs_error_syntaxerror);
                iexp = d;
                for (;; iexp = iexp * 10 + d) {
                    GET_NEXT(c, sp, break);
                    if (!IS_DIGIT(d, c)) {
                        *psp = sp;
                        code = 1;
                        break;
                    }
                    if (iexp > 99)
                        return_error(gs_error_limitcheck);
                }
                if (esign)
                    exp10 -= iexp;
                else
                    exp10 += iexp;
                break;
            }
        default:
            *psp = sp;
            code = 1;
        case EOFC:
            ;
    }
    /* Compute dval * 10^exp10. */
    if (exp10 > 0) {
        while (exp10 > NUM_POWERS_10)
            dval *= powers_10[NUM_POWERS_10],
                exp10 -= NUM_POWERS_10;
        dval *= powers_10[exp10];
    } else if (exp10 < 0) {
        while (exp10 < -NUM_POWERS_10)
            dval /= powers_10[NUM_POWERS_10],
                exp10 += NUM_POWERS_10;
        dval /= powers_10[-exp10];
    }
    /*
     * Check for an out-of-range result.  Currently we don't check for
     * absurdly large numbers of digits in the accumulation loops,
     * but we should.
     */
    if (dval >= 0) {
        if (dval > MAX_FLOAT)
            return_error(gs_error_limitcheck);
    } else {
        if (dval < -MAX_FLOAT)
            return_error(gs_error_limitcheck);
    }
rret:
    make_real(pref, dval);
    return code;
}
