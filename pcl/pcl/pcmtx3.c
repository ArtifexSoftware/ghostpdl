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


/* pcmtx3.c - 3 x 3 matrix utilities for PCL device independent color spaces */
#include "string_.h"
#include "math_.h"
#include "gx.h"
#include "gstypes.h"
#include "pcommand.h"
#include "pcmtx3.h"

/*
 * Calculate the co-factor for the (i, j) component of a 3 x 3 matrix,
 * including the sign. Note that i and j range from 0 to 2.
 *
 * The cofactor of the (i, j) entry in an n-dimensional matrix is the
 * determinant of the (n - 1) dimensional matrix formed by dropping the
 * ith row and jth column of the given matrix, multiplied by -1 if
 * (i + j) is odd.
 */
static double
calc_cofactor(int i, int j, const pcl_mtx3_t * pmtx)
{
    int i1 = (i == 0 ? 1 : 0);
    int i2 = (i == 2 ? 1 : 2);
    int j1 = (j == 0 ? 1 : 0);
    int j2 = (j == 2 ? 1 : 2);
    double cf = pmtx->a[3 * i1 + j1] * pmtx->a[3 * i2 + j2]
        - pmtx->a[3 * i1 + j2] * pmtx->a[3 * i2 + j1];

    return (((i + j) & 0x1) != 0) ? -cf : cf;
}

/*
 * Form the "cofactor matrix" corresponding to the given matrix.
 *
 * For this routine, pinmtx and poutmtx must be different.
 */
static void
make_cofactor_mtx(const pcl_mtx3_t * pinmtx, pcl_mtx3_t * poutmtx)
{
    int i;

    for (i = 0; i < 3; i++) {
        int j;

        for (j = 0; j < 3; j++)
            poutmtx->a[3 * i + j] = calc_cofactor(i, j, pinmtx);
    }
}

/*
 * Add and subtract 3 dimensional vectors. These are not currently needed,
 * but are included for completeness. No inner-product is provided because
 * color spaces do not, in general, have geometric or even metric properties
 * (the CID L*a*b* space is an exception, and then only approximately).
 *
 * Any of the three pointer operands may be the same.
 */
void
pcl_vec3_add(const pcl_vec3_t * pivec1,
             const pcl_vec3_t * pivec2, pcl_vec3_t * poutvec)
{
    poutvec->vc.v1 = pivec1->vc.v1 + pivec2->vc.v1;
    poutvec->vc.v2 = pivec1->vc.v2 + pivec2->vc.v2;
    poutvec->vc.v3 = pivec1->vc.v3 + pivec2->vc.v3;
}

void
pcl_vec3_sub(const pcl_vec3_t * pivec1,
             const pcl_vec3_t * pivec2, pcl_vec3_t * poutvec)
{
    poutvec->vc.v1 = pivec1->vc.v1 - pivec2->vc.v1;
    poutvec->vc.v2 = pivec1->vc.v2 - pivec2->vc.v2;
    poutvec->vc.v3 = pivec1->vc.v3 - pivec2->vc.v3;
}

/*
 * Apply a matrix to a color component vector. Note that vectors are inter-
 * preted as row vectors, and matrices are specified in a row-first order.
 *
 * The code will properly handle the case pinvec == poutvec.
 */
void
pcl_vec3_xform(const pcl_vec3_t * pinvec,
               pcl_vec3_t * poutvec, const pcl_mtx3_t * pmtx)
{
    pcl_vec3_t tmp_vec;
    int i;

    for (i = 0; i < 3; i++)
        tmp_vec.va[i] = pinvec->vc.v1 * pmtx->a[i]
            + pinvec->vc.v2 * pmtx->a[i + 3]
            + pinvec->vc.v3 * pmtx->a[i + 6];
    poutvec->vc = tmp_vec.vc;
}

/*
 * Invert a 3 x 3 matrix.
 *
 * This will properly handle the case of pinmtx == poutmtx.
 *
 * Returns 0 on success, e_Range if the matrix provided is singular.
 */
int
pcl_mtx3_invert(const pcl_mtx3_t * pinmtx, pcl_mtx3_t * poutmtx)
{
    pcl_mtx3_t cf_mtx;
    double det;
    int i;

    make_cofactor_mtx(pinmtx, &cf_mtx);
    det = pinmtx->c.a11 * cf_mtx.c.a11
        + pinmtx->c.a12 * cf_mtx.c.a12 + pinmtx->c.a13 * cf_mtx.c.a13;
    if (det == 0.0)
        return e_Range;
    for (i = 0; i < 3; i++) {
        int j;

        for (j = 0; j < 3; j++)
            poutmtx->a[3 * i + j] = cf_mtx.a[3 * j + i] / det;
    }

    return 0;
}

/*
 * Add, subtract, and multiply two 3 x 3 matrices. These are not currently
 * needed, but are included for completenese.
 *
 * In all cases, any of the three pointers provided may be identical.
 */
void
pcl_mtx3_add(const pcl_mtx3_t * pinmtx1,
             const pcl_mtx3_t * pinmtx2, pcl_mtx3_t * poutmtx)
{
    int i;

    for (i = 0; i < 9; i++)
        poutmtx->a[i] = pinmtx1->a[i] + pinmtx2->a[i];
}

void
pcl_mtx3_sub(const pcl_mtx3_t * pinmtx1,
             const pcl_mtx3_t * pinmtx2, pcl_mtx3_t * poutmtx)
{
    int i;

    for (i = 0; i < 9; i++)
        poutmtx->a[i] = pinmtx1->a[i] - pinmtx2->a[i];
}

void
pcl_mtx3_mul(const pcl_mtx3_t * pinmtx1,
             const pcl_mtx3_t * pinmtx2, pcl_mtx3_t * poutmtx)
{
    pcl_mtx3_t tmp_mtx;
    int i;

    for (i = 0; i < 3; i++) {
        int j;

        for (j = 0; j < 3; j++) {
            int k;
            double val = 0.0;

            for (k = 0; k < 3; k++)
                val += pinmtx1->a[3 * i + k] * pinmtx2->a[3 * k + j];
            tmp_mtx.a[3 * i + j] = val;
        }
    }
    *poutmtx = tmp_mtx;
}

/*
 * Convert a pcl_mtx3_t structure to and from a gs_matrix3 struct. Identity
 * transformations are rare in PCL, so no attempt is made to identify them.
 *
 * Note that the conversion transposes the matrix in interpretation, though
 * physically entries remain in the same order. This is due to the use of
 * a "column vector" interpretation of color components in the graphic library.
 */
void
pcl_mtx3_convert_to_gs(const pcl_mtx3_t * pinmtx, gs_matrix3 * pgsmtx)
{
    pgsmtx->cu.u = pinmtx->c.a11;
    pgsmtx->cu.v = pinmtx->c.a12;
    pgsmtx->cu.w = pinmtx->c.a13;
    pgsmtx->cv.u = pinmtx->c.a21;
    pgsmtx->cv.v = pinmtx->c.a22;
    pgsmtx->cv.w = pinmtx->c.a23;
    pgsmtx->cw.u = pinmtx->c.a31;
    pgsmtx->cw.v = pinmtx->c.a32;
    pgsmtx->cw.w = pinmtx->c.a33;
    pgsmtx->is_identity = false;
}

void
pcl_mtx3_convert_from_gs(pcl_mtx3_t * poutmtx, const gs_matrix3 * pgsmtx)
{
    if (pgsmtx->is_identity) {
        memset(poutmtx, 0, sizeof(pcl_mtx3_t));
        poutmtx->c.a11 = 1.0;
        poutmtx->c.a22 = 1.0;
        poutmtx->c.a22 = 1.0;
    } else {
        poutmtx->c.a11 = pgsmtx->cu.u;
        poutmtx->c.a12 = pgsmtx->cu.v;
        poutmtx->c.a13 = pgsmtx->cu.w;
        poutmtx->c.a21 = pgsmtx->cv.u;
        poutmtx->c.a22 = pgsmtx->cv.v;
        poutmtx->c.a23 = pgsmtx->cw.v;
        poutmtx->c.a31 = pgsmtx->cw.u;
        poutmtx->c.a32 = pgsmtx->cw.v;
        poutmtx->c.a33 = pgsmtx->cw.w;
    }
}
