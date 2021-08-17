/* Copyright (C) 2001-2021 Artifex Software, Inc.
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


/* Coordinate system operators for Ghostscript library */
#include "math_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsccode.h"		/* for gxfont.h */
#include "gxfarith.h"
#include "gxfixed.h"
#include "gxmatrix.h"
#include "gxfont.h"		/* for char_tm */
#include "gxpath.h"		/* for gx_path_translate */
#include "gzstate.h"
#include "gxcoord.h"		/* requires gsmatrix, gsstate */
#include "gxdevice.h"

/* Choose whether to enable the rounding code in update_ctm. */
#define ROUND_CTM_FIXED 0

/* Forward declarations */
#ifdef DEBUG
#define trace_ctm(pgs) trace_matrix_fixed((pgs)->memory, &(pgs)->ctm)
static void trace_matrix_fixed(const gs_memory_t *mem, const gs_matrix_fixed *);
static void trace_matrix(const gs_memory_t *mem, const gs_matrix *);

#endif

/* Macro for ensuring ctm_inverse is valid */
#ifdef DEBUG
#  define print_inverse(pgs)\
     if ( gs_debug_c('x') )\
       dmlprintf(pgs->memory, "[x]Inverting:\n"), trace_ctm(pgs), trace_matrix(pgs->memory, &pgs->ctm_inverse)
#else
#  define print_inverse(pgs) DO_NOTHING
#endif
#define ensure_inverse_valid(pgs)\
        if ( !pgs->ctm_inverse_valid )\
           {	int code = ctm_set_inverse(pgs);\
                if ( code < 0 ) return code;\
           }

static int
ctm_set_inverse(gs_gstate * pgs)
{
    int code = gs_matrix_invert(&ctm_only(pgs), &pgs->ctm_inverse);

    print_inverse(pgs);
    if (code < 0)
        return code;
    pgs->ctm_inverse_valid = true;
    return 0;
}

/* Machinery for updating fixed version of ctm. */
/*
 * We (conditionally) adjust the floating point translation
 * so that it exactly matches the (rounded) fixed translation.
 * This avoids certain unpleasant rounding anomalies, such as
 * 0 0 moveto currentpoint not returning 0 0, and () stringwidth
 * not returning 0 0.
 */
#if ROUND_CTM_FIXED
#  define update_t_fixed(mat, t, t_fixed, v)\
    (set_float2fixed_vars((mat).t_fixed, v),\
     set_fixed2float_var((mat).t, (mat).t_fixed))
#else /* !ROUND_CTM_FIXED */
#  define update_t_fixed(mat, t, t_fixed, v)\
    ((mat).t = (v),\
     set_float2fixed_vars((mat).t_fixed, (mat).t))
#endif /* (!)ROUND_CTM_FIXED */
#define f_fits_in_fixed(f) f_fits_in_bits(f, fixed_int_bits)
#define update_matrix_fixed(mat, xt, yt)\
  ((mat).txy_fixed_valid = (f_fits_in_fixed(xt) && f_fits_in_fixed(yt) ?\
                            (update_t_fixed(mat, tx, tx_fixed, xt),\
                             update_t_fixed(mat, ty, ty_fixed, yt), true) :\
                            ((mat).tx = (xt), (mat).ty = (yt), false)))
#define update_ctm(pgs, xt, yt)\
  (pgs->ctm_inverse_valid = false,\
   pgs->char_tm_valid = false,\
   update_matrix_fixed(pgs->ctm, xt, yt))

/* ------ Coordinate system definition ------ */

int
gs_initmatrix(gs_gstate * pgs)
{
    gs_matrix imat;

    gs_defaultmatrix(pgs, &imat);
    update_ctm(pgs, imat.tx, imat.ty);
    set_ctm_only(pgs, imat);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf(pgs->memory, "[x]initmatrix:\n"), trace_ctm(pgs);
#endif
    return 0;
}

int
gs_defaultmatrix(const gs_gstate * pgs, gs_matrix * pmat)
{
    gx_device *dev;

    if (pgs->ctm_default_set) {	/* set after Install */
        *pmat = pgs->ctm_default;
        return 1;
    }
    dev = gs_currentdevice_inline(pgs);
    gs_deviceinitialmatrix(dev, pmat);
    /* Add in the translation for the Margins. */
    pmat->tx += dev->Margins[0];
    pmat->ty += dev->Margins[1];
    return 0;
}

int
gs_setdefaultmatrix(gs_gstate * pgs, const gs_matrix * pmat)
{
    if (pmat == NULL) {
        pgs->ctm_default_set = false;
        pgs->ctm_initial_set = false;
    } else {
        gx_device *dev;

        pgs->ctm_default = *pmat;
        pgs->ctm_default_set = true;

        /* We also store the current 'initial' matrix, so we can spot
         * changes in this in future. */
        dev = gs_currentdevice_inline(pgs);
        gs_deviceinitialmatrix(dev, &pgs->ctm_initial);
        pgs->ctm_initial_set = 1;
    }
    return 0;
}

int
gs_updatematrices(gs_gstate *pgs)
{
    gx_device *dev;
    gs_matrix newdefault, init, t, inv, newctm;
    int code;
#ifdef DEBUG
    gs_matrix *mat;
#endif

    /* Read the current device initial matrix. */
    dev = gs_currentdevice_inline(pgs);
    gs_deviceinitialmatrix(dev, &init);

#ifdef DEBUG
    if (gs_debug_c('x'))
        dlprintf("[x]updatematrices\n");
#endif

    if (pgs->ctm_default_set == 0 ||
        pgs->ctm_initial_set == 0) {
        /* If neither default or initial are set, then store them for the
         * first time. */
        pgs->ctm_initial = init;
        pgs->ctm_initial_set = 1;
        pgs->ctm_default = init;
        pgs->ctm_default_set = 1;
#ifdef DEBUG
        if (gs_debug_c('x')) {
            mat = &pgs->ctm_initial;
            dlprintf6("storing initial/default = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
        }
#endif
        return 0;
    }

#ifdef DEBUG
    if (gs_debug_c('x')) {
        mat = &init;
        dlprintf6("initial        = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
        mat = &pgs->ctm_default;
        dlprintf6("default        = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
        mat = (gs_matrix *)&pgs->ctm;
        dlprintf6("ctm            = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
        mat = &pgs->ctm_initial;
        dlprintf6("stored initial = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
    }
#endif
    /* If no change, then nothing else to do here. */
    if (init.xx == pgs->ctm_initial.xx &&
        init.xy == pgs->ctm_initial.xy &&
        init.yx == pgs->ctm_initial.yx &&
        init.yy == pgs->ctm_initial.yy &&
        init.tx == pgs->ctm_initial.tx &&
        init.ty == pgs->ctm_initial.ty)
        return 0;

    /* So, the initial matrix has changed from what it was
     * the last time the default matrix was set. The default
     * matrix is some modification of the initial matrix
     * (typically a scale, or a translation, or a flip or
     * some combination thereof). Now the initial matrix
     * has changed (possibly because of Nup, or because of
     * a device doing Duplex etc), the default matrix is
     * almost certainly wrong. We therefore adjust it here.*/

    /* So originally: old_default = modification.old_init
     * and we want:   new_default = modification.new_init
     *
     * So: modification = old_default.INV(old_init)
     *     new_default  = old_default.INV(old_init).new_init
     */
    code = gs_matrix_invert(&pgs->ctm_initial, &inv);
    if (code < 0)
        return code;
    code = gs_matrix_multiply(&pgs->ctm_default, &inv, &t);
    if (code < 0)
        return code;
    code = gs_matrix_multiply(&t, &init, &newdefault);
    if (code < 0)
        return code;

    /* Now, the current ctm is similarly derived from the
     * old default. We want to update it to be derived (in the
     * same way) from the new default.
     *
     * So:  old_ctm = modification.old_default
     *      old_ctm.INV(old_default) = modification
     * And: new_ctm = modification.new_default
     *              = old_ctm.INV(old_default).new_default
     */
    code = gs_matrix_invert(&pgs->ctm_default, &inv);
    if (code < 0)
        return code;
    code = gs_matrix_multiply((gs_matrix *)&pgs->ctm, &inv, &t);
    if (code < 0)
        return code;
    code = gs_matrix_multiply(&t, &newdefault, &newctm);
    if (code < 0)
        return code;

    pgs->ctm_initial = init;
    pgs->ctm_default = newdefault;
    gs_setmatrix(pgs, &newctm);

#ifdef DEBUG
    if (gs_debug_c('x')) {
        mat = &pgs->ctm_default;
        dlprintf6("new default    = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
        mat = (gs_matrix *)&pgs->ctm;
        dlprintf6("new ctm        = %g %g %g %g %g %g\n", mat->xx, mat->xy, mat->yx, mat->yy, mat->tx, mat->ty);
    }
#endif

    /* This is a bit nasty. This resets the clipping box to the page.
     * We need to do this, because otherwise the clipping box is
     * not updated with the ctm, and (typically) the entire contents
     * of the page end up clipped away. This will break usages where
     * we run 1 file (or set of postscript commands) to set the clipping
     * box, and then another file to actually draw stuff to be clipped.
     * Given this will only go wrong in the case where the device is
     * Nupping or Duplexing, we'll live with this for now. */
    return gs_initclip(pgs);
}

int
gs_currentmatrix(const gs_gstate * pgs, gs_matrix * pmat)
{
    *pmat = ctm_only(pgs);
    return 0;
}

/* Set the current transformation matrix for rendering text. */
/* Note that this may be based on a font other than the current font. */
int
gs_setcharmatrix(gs_gstate * pgs, const gs_matrix * pmat)
{
    gs_matrix cmat;
    int code = gs_matrix_multiply(pmat, &ctm_only(pgs), &cmat);

    if (code < 0)
        return code;
    update_matrix_fixed(pgs->char_tm, cmat.tx, cmat.ty);
    char_tm_only(pgs) = cmat;
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf(pgs->memory, "[x]setting char_tm:"), trace_matrix_fixed(pgs->memory, &pgs->char_tm);
#endif
    pgs->char_tm_valid = true;
    return 0;
}

/* Read (after possibly computing) the current transformation matrix */
/* for rendering text.  If force=true, update char_tm if it is invalid; */
/* if force=false, don't update char_tm, and return an error code. */
int
gs_currentcharmatrix(gs_gstate * pgs, gs_matrix * ptm, bool force)
{
    if (!pgs->char_tm_valid) {
        int code;

        if (!force)
            return_error(gs_error_undefinedresult);
        code = gs_setcharmatrix(pgs, &pgs->font->FontMatrix);
        if (code < 0)
            return code;
    }
    if (ptm != NULL)
        *ptm = char_tm_only(pgs);
    return 0;
}

int
gs_setmatrix(gs_gstate * pgs, const gs_matrix * pmat)
{
    update_ctm(pgs, pmat->tx, pmat->ty);
    set_ctm_only(pgs, *pmat);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf(pgs->memory, "[x]setmatrix:\n"), trace_ctm(pgs);
#endif
    return 0;
}

int
gs_gstate_setmatrix(gs_gstate * pgs, const gs_matrix * pmat)
{
    update_matrix_fixed(pgs->ctm, pmat->tx, pmat->ty);
    set_ctm_only(pgs, *pmat);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf(pgs->memory, "[x]imager_setmatrix:\n"), trace_ctm(pgs);
#endif
    return 0;
}

int
gs_settocharmatrix(gs_gstate * pgs)
{
    if (pgs->char_tm_valid) {
        pgs->ctm = pgs->char_tm;
        pgs->ctm_inverse_valid = false;
        return 0;
    } else
        return_error(gs_error_undefinedresult);
}

int
gs_translate(gs_gstate * pgs, double dx, double dy)
{
    gs_point pt;
    int code;

    if ((code = gs_distance_transform(dx, dy, &ctm_only(pgs), &pt)) < 0)
        return code;
    pt.x = (float)pt.x + pgs->ctm.tx;
    pt.y = (float)pt.y + pgs->ctm.ty;
    update_ctm(pgs, pt.x, pt.y);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf4(pgs->memory, "[x]translate: %f %f -> %f %f\n",
                  dx, dy, pt.x, pt.y),
            trace_ctm(pgs);
#endif
    return 0;
}

int
gs_translate_untransformed(gs_gstate * pgs, double dx, double dy)
{
    gs_point pt;

    pt.x = (float)dx + pgs->ctm.tx;
    pt.y = (float)dy + pgs->ctm.ty;
    update_ctm(pgs, pt.x, pt.y);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf4(pgs->memory, "[x]translate_untransformed: %f %f -> %f %f\n",
                  dx, dy, pt.x, pt.y),
            trace_ctm(pgs);
#endif
    return 0;
}

int
gs_scale(gs_gstate * pgs, double sx, double sy)
{
    pgs->ctm.xx *= sx;
    pgs->ctm.xy *= sx;
    pgs->ctm.yx *= sy;
    pgs->ctm.yy *= sy;
    pgs->ctm_inverse_valid = false, pgs->char_tm_valid = false;
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf2(pgs->memory, "[x]scale: %f %f\n", sx, sy), trace_ctm(pgs);
#endif
    return 0;
}

int
gs_rotate(gs_gstate * pgs, double ang)
{
    int code = gs_matrix_rotate(&ctm_only(pgs), ang,
                                &ctm_only_writable(pgs));

    pgs->ctm_inverse_valid = false, pgs->char_tm_valid = false;
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf1(pgs->memory, "[x]rotate: %f\n", ang), trace_ctm(pgs);
#endif
    return code;
}

int
gs_concat(gs_gstate * pgs, const gs_matrix * pmat)
{
    gs_matrix cmat;
    int code = gs_matrix_multiply(pmat, &ctm_only(pgs), &cmat);

    if (code < 0)
        return code;
    update_ctm(pgs, cmat.tx, cmat.ty);
    set_ctm_only(pgs, cmat);
#ifdef DEBUG
    if (gs_debug_c('x'))
        dmlprintf(pgs->memory, "[x]concat:\n"), trace_matrix(pgs->memory, pmat), trace_ctm(pgs);
#endif
    return code;
}

/* ------ Coordinate transformation ------ */

#define is_skewed(pmat) (!(is_xxyy(pmat) || is_xyyx(pmat)))

int
gs_transform(gs_gstate * pgs, double x, double y, gs_point * pt)
{
    return gs_point_transform(x, y, &ctm_only(pgs), pt);
}

int
gs_dtransform(gs_gstate * pgs, double dx, double dy, gs_point * pt)
{
    return gs_distance_transform(dx, dy, &ctm_only(pgs), pt);
}

int
gs_itransform(gs_gstate * pgs, double x, double y, gs_point * pt)
{				/* If the matrix isn't skewed, we get more accurate results */
    /* by using transform_inverse than by using the inverse matrix. */
    if (!is_skewed(&pgs->ctm)) {
        return gs_point_transform_inverse(x, y, &ctm_only(pgs), pt);
    } else {
        ensure_inverse_valid(pgs);
        return gs_point_transform(x, y, &pgs->ctm_inverse, pt);
    }
}

int
gs_idtransform(gs_gstate * pgs, double dx, double dy, gs_point * pt)
{				/* If the matrix isn't skewed, we get more accurate results */
    /* by using transform_inverse than by using the inverse matrix. */
    if (!is_skewed(&pgs->ctm)) {
        return gs_distance_transform_inverse(dx, dy,
                                             &ctm_only(pgs), pt);
    } else {
        ensure_inverse_valid(pgs);
        return gs_distance_transform(dx, dy, &pgs->ctm_inverse, pt);
    }
}

int
gs_gstate_idtransform(const gs_gstate * pgs, double dx, double dy,
                      gs_point * pt)
{
    return gs_distance_transform_inverse(dx, dy, &ctm_only(pgs), pt);
}

/* ------ For internal use only ------ */

/* Set the translation to a fixed value, and translate any existing path. */
/* Used by gschar.c to prepare for a BuildChar or BuildGlyph procedure. */
int
gx_translate_to_fixed(register gs_gstate * pgs, fixed px, fixed py)
{
    double fpx = fixed2float(px);
    double fdx = fpx - pgs->ctm.tx;
    double fpy = fixed2float(py);
    double fdy = fpy - pgs->ctm.ty;
    fixed dx, dy;
    int code;

    if (pgs->ctm.txy_fixed_valid) {
        dx = float2fixed(fdx);
        dy = float2fixed(fdy);
        code = gx_path_translate(pgs->path, dx, dy);
        if (code < 0)
            return code;
        if (pgs->char_tm_valid && pgs->char_tm.txy_fixed_valid)
            pgs->char_tm.tx_fixed += dx,
                pgs->char_tm.ty_fixed += dy;
    } else {
        if (!gx_path_is_null(pgs->path))
            return_error(gs_error_limitcheck);
    }
    pgs->ctm.tx = fpx;
    pgs->ctm.tx_fixed = px;
    pgs->ctm.ty = fpy;
    pgs->ctm.ty_fixed = py;
    pgs->ctm.txy_fixed_valid = true;
    pgs->ctm_inverse_valid = false;
    if (pgs->char_tm_valid) {	/* Update char_tm now, leaving it valid. */
        pgs->char_tm.tx += fdx;
        pgs->char_tm.ty += fdy;
    }
#ifdef DEBUG
    if (gs_debug_c('x')) {
        dmlprintf2(pgs->memory, "[x]translate_to_fixed %g, %g:\n",
                   fixed2float(px), fixed2float(py));
        trace_ctm(pgs);
        dmlprintf(pgs->memory, "[x]   char_tm:\n");
        trace_matrix_fixed(pgs->memory, &pgs->char_tm);
    }
#endif
    gx_setcurrentpoint(pgs, fixed2float(pgs->ctm.tx_fixed), fixed2float(pgs->ctm.ty_fixed));
    pgs->current_point_valid = true;
    return 0;
}

/* Scale the CTM and character matrix for oversampling. */
int
gx_scale_char_matrix(register gs_gstate * pgs, int sx, int sy)
{
#define scale_cxy(s, vx, vy)\
  if ( s != 1 )\
   {	pgs->ctm.vx *= s;\
        pgs->ctm.vy *= s;\
        pgs->ctm_inverse_valid = false;\
        if ( pgs->char_tm_valid )\
        {	pgs->char_tm.vx *= s;\
                pgs->char_tm.vy *= s;\
        }\
   }
    scale_cxy(sx, xx, yx);
    scale_cxy(sy, xy, yy);
#undef scale_cxy
    if_debug2m('x', pgs->memory, "[x]char scale: %d %d\n", sx, sy);
    return 0;
}

/* Compute the coefficients for fast fixed-point distance transformations */
/* from a transformation matrix. */
/* We should cache the coefficients with the ctm.... */
int
gx_matrix_to_fixed_coeff(const gs_matrix * pmat, register fixed_coeff * pfc,
                         int max_bits)
{
    gs_matrix ctm;
    int scale = -10000;
    int expt, shift;

    ctm = *pmat;
    pfc->skewed = 0;
    if (!is_fzero(ctm.xx)) {
        discard(frexp(ctm.xx, &scale));
    }
    if (!is_fzero(ctm.xy)) {
        discard(frexp(ctm.xy, &expt));
        if (expt > scale)
            scale = expt;
        pfc->skewed = 1;
    }
    if (!is_fzero(ctm.yx)) {
        discard(frexp(ctm.yx, &expt));
        if (expt > scale)
            scale = expt;
        pfc->skewed = 1;
    }
    if (!is_fzero(ctm.yy)) {
        discard(frexp(ctm.yy, &expt));
        if (expt > scale)
            scale = expt;
    }
    /*
     * There are two multiplications in fixed_coeff_mult: one involves a
     * factor that may have max_bits significant bits, the other may have
     * fixed_fraction_bits (_fixed_shift) bits.  Ensure that neither one
     * will overflow.
     */
    if (max_bits < fixed_fraction_bits)
        max_bits = fixed_fraction_bits;
    scale = sizeof(long) * 8 - 1 - max_bits - scale;

    shift = scale - _fixed_shift;
    if (shift > 0) {
        pfc->shift = shift;
        pfc->round = (fixed) 1 << (shift - 1);
    } else {
        pfc->shift = 0;
        pfc->round = 0;
        scale -= shift;
    }
#define SET_C(c)\
  if ( is_fzero(ctm.c) ) pfc->c = 0;\
  else pfc->c = (long)ldexp(ctm.c, scale)
    SET_C(xx);
    SET_C(xy);
    SET_C(yx);
    SET_C(yy);
#undef SET_C
#ifndef GS_THREADSAFE
#ifdef DEBUG
    if (gs_debug_c('x')) {
        dlprintf6("[x]ctm: [%6g %6g %6g %6g %6g %6g]\n",
                  ctm.xx, ctm.xy, ctm.yx, ctm.yy, ctm.tx, ctm.ty);
        dlprintf6("   scale=%d fc: [0x%lx 0x%lx 0x%lx 0x%lx] shift=%d\n",
                  scale, pfc->xx, pfc->xy, pfc->yx, pfc->yy,
                  pfc->shift);
    }
#endif
#endif
    pfc->max_bits = max_bits;
    return 0;
}

/*
 * Handle the case of a large value or a value with a fraction part.
 * See gxmatrix.h for more details.
 */
fixed
fixed_coeff_mult(fixed value, long coeff, const fixed_coeff *pfc, int maxb)
{
    int shift = pfc->shift;

    /*
     * Test if the value is too large for simple long math.
     */
    if ((value + (fixed_1 << (maxb - 1))) & (-fixed_1 << maxb)) {
        /* The second argument of fixed_mult_quo must be non-negative. */
        return
            (coeff < 0 ?
             -fixed_mult_quo(value, -coeff, fixed_1 << shift) :
             fixed_mult_quo(value, coeff, fixed_1 << shift));
    } else {
        /*
         * The construction above guarantees that the multiplications
         * won't overflow the capacity of an int.
         */
        return (fixed)
            arith_rshift(fixed2int_var(value) * coeff
                         + fixed2int(fixed_fraction(value) * coeff)
                         + pfc->round, shift);
    }
}

/* ------ Debugging printout ------ */

#ifdef DEBUG

/* Print a matrix */
static void
trace_matrix_fixed(const gs_memory_t *mem, const gs_matrix_fixed * pmat)
{
    trace_matrix(mem, (const gs_matrix *)pmat);
    if (pmat->txy_fixed_valid) {
        dmprintf2(mem, "\t\tt_fixed: [%6g %6g]\n",
                  fixed2float(pmat->tx_fixed),
                  fixed2float(pmat->ty_fixed));
    } else {
        dmputs(mem, "\t\tt_fixed not valid\n");
    }
}
static void
trace_matrix(const gs_memory_t *mem, register const gs_matrix * pmat)
{
    dmlprintf6(mem, "\t[%6g %6g %6g %6g %6g %6g]\n",
               pmat->xx, pmat->xy, pmat->yx, pmat->yy, pmat->tx, pmat->ty);
}

#endif
