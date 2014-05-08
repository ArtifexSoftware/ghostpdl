/*
 * Testbed implementation of Even Better Screening. 
 * Updated to function properly by Robin Watts and Michael Vrhel
 *
 * Code in this module is covered by US Patents 5055942 and
 * 5917614 and corresponding international patents. Contact
 * Artifex for more information, or see:
 *
 *   http://www.artifex.com/page/imaging-science.html
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "ets.h"

/* source for threshold matrix - need to improve build process */
#include "tm.h"

#define ETS_VERSION 150

#define ETS_SHIFT 16
#define IMO_SHIFT 14

#define FANCY_COUPLING

typedef struct _ETS_PlaneCtx ETS_PlaneCtx;
typedef unsigned int uint32;

struct _ETS_Ctx {
    int width;
    int n_planes;
    int levels; /* Number of levels on output, <= 256 */
    ETS_PlaneCtx **plane_ctx;
    int aspect_x;
    int aspect_y;
    int *strengths;
    int elo;
    int ehi;
    int *c_line;

    int ets_style;
    int r_style;

    uint32 seed1;
    uint32 seed2;

    FILE *dump_file;
    ETS_DumpLevel dump_level;

    /* Threshold modulation array */
    int y;
    int tmwid;
    int tmheight;
    const signed char *tmmat;
};

struct _ETS_PlaneCtx {
    int  width;     /* Width of plane in pixels */
    int *dst_line;  /* Output pointer */
    int *err_line;  /* Total error carried out of pixel in the line above */
    int *r_line;    /* expected distance table (see paper for details) */
    int *a_line;    /* expected distance intermediate table (see paper) */
    int *b_line;    /* expected distance intermediate table (see paper) */
    int *lut;       /* Table to map from input source value to internal
                     * intensity level. Internal intensity level is 0 to
                     * 1<<ETS_SHIFT. */
    int *dist_lut;  /* A table of "expected distance between set pixels"
                     * values, stored in fixed point format with (ETS_SHIFT-c1)
                     * fractional bits. Values outside of the 'level 0-1' band
                     * will be set to 0 to avoid ETS weighting being used. */
    char *rs_lut;   /* Random noise table; values between 0 and 24. x meaning
                     * use 32-x bits of random noise, */
    int   c1;       /* Shift adjustment for the dist_lut. */
    int   tm_offset;/* Plane offset within tm data */
};

void *
ets_malloc_aligned(int size, int align)
{
    void *result;
    void *alloced = malloc(size + align);
    int pad;

    if (alloced == 0)
        return 0;
    pad = ((12 - (int)alloced) & 15) + 4;
    result = (void *)(pad + (char *)alloced);
    ((int *)result)[-1] = pad;
    return result;
}

void
ets_free_aligned(void *p)
{
    int pad = ((int *)p)[-1];
    free((char*)p - pad);
}

static double
compute_distscale(const ETS_Params *params)
{
    double distscale = params->distscale;

    if (distscale == 0.0)
    {
        distscale = -1;
        switch(params->aspect_x)
        {
        case 1:
            switch(params->aspect_y)
            {
            case 1:
                distscale = 0.95;
                break;
            case 2:
                distscale = 1.8;
                break;
            case 3:
                distscale = 2.4; /* FIXME */
                break;
            case 4:
                distscale = 3.6;
                break;
            }
            break;
        case 2:
            switch(params->aspect_y)
            {
            case 1:
                distscale = 1.8;
                break;
            case 2:
                break;
            case 3:
                distscale = 1.35; /* FIXME */
                break;
            case 4:
                break;
            }
            break;
        case 3:
            switch(params->aspect_y)
            {
            case 1:
                distscale = 2.4; /* FIXME */
                break;
            case 2:
                distscale = 1.35; /* FIXME */
                break;
            case 3:
                break;
            case 4:
                distscale = 0.675; /* FIXME */
                break;
            }
            break;
        case 4:
            switch(params->aspect_y)
            {
            case 1:
                distscale = 3.6;
                break;
            case 2:
                break;
            case 3:
                distscale = 0.675; /* FIXME */
                break;
            case 4:
                break;
            }
            break;
        }
        if (distscale == -1)
        {
            fprintf(stderr, "aspect ratio of %d:%d not supported\n",
                    params->aspect_x, params->aspect_y);
            exit(1);
        }
    }
    return distscale;
}

static int
ets_log2(int x)
{
    int y = 0;
    int z;

    for (z = x; z > 1; z = z >> 1)
        y++;
    return y;
}

static int
ets_log2up(int x)
{
    return ets_log2(x-1)+1;
}

static int
compute_randshift(int nl, int rs_base, int levels)
{
    int rs = rs_base;

    if ((nl > (90 << (ETS_SHIFT - 10)) &&
         nl < (129 << (ETS_SHIFT - 10))) ||
        (nl > (162 << (ETS_SHIFT - 10)) &&
         nl < (180 << (ETS_SHIFT - 10))))
        rs--;
    else if (nl > (321 << (ETS_SHIFT - 10)) &&
             nl < (361 << (ETS_SHIFT - 10)))
    {
        rs--;
        if (nl > (331 << (ETS_SHIFT - 10)) &&
            nl < (351 << (ETS_SHIFT - 10)))
            rs--;
    }
    else if ((nl == (levels - 1) << ETS_SHIFT) &&
             nl > (((levels - 1) << ETS_SHIFT) -
                   (1 << (ETS_SHIFT - 2))))
    {
        /* don't add randomness in extreme shadows */
    }
    else if ((nl > (3 << (ETS_SHIFT - 2))))
    {
        nl -= (nl + (1 << (ETS_SHIFT - 2))) & -(1 << (ETS_SHIFT - 1));
        if (nl < 0) nl = -nl;
        if (nl < (1 << (ETS_SHIFT - 4))) rs--;
        if (nl < (1 << (ETS_SHIFT - 5))) rs--;
        if (nl < (1 << (ETS_SHIFT - 6))) rs--;
    }
    else
    {
        if (nl < (3 << (ETS_SHIFT - 3))) nl += 1 << (ETS_SHIFT - 2);
        nl = nl - (1 << (ETS_SHIFT - 1));
        if (nl < 0) nl = -nl;
        if (nl < (1 << (ETS_SHIFT - 4))) rs--;
        if (nl < (1 << (ETS_SHIFT - 5))) rs--;
        if (nl < (1 << (ETS_SHIFT - 6))) rs--;
    }
    return rs;
}

/* Maximum number of planes, but actually we want to dynamically
   allocate all scratch buffers that depend on this. */
#define M 16

/**
 * ets_line: Screen a line using Even TonenFS screeing.
 * @ctx: An #EBPlaneCtx context.
 * @dest: Array of destination buffers, 8 bpp pixels each.
 * @src: Array of source buffer, ET_SrcPixel pixels each.
 *
 * Screens a single line using Even ToneFS screening.
 **/
void
ets_line(ETS_Ctx *etc, uchar **dest, const ETS_SrcPixel *const *src)
{
    int a[M], b[M];
    int e_1_0[M], e_m1_1[M], e_0_1[M], e_1_1[M];
    int i;
    int im;
    int *pa, *pb, *piir, *pr;
    int r[M], rg;
    int xd;
    uint32 seed1 = etc->seed1;
    uint32 seed2 = etc->seed2;
    uint32 sum;
    int plane_idx;
    int n_planes = etc->n_planes;
    int levels = etc->levels;
#ifdef OLD_QUANT
    int dith_mul = levels << 8;
#else
    int dith_mul = (levels - 1) << 8;
#endif
    int imo_mul = (1 << (ETS_SHIFT + IMO_SHIFT)) / (levels - 1);
    int aspect_x2 = etc->aspect_x * etc->aspect_x;
    int aspect_y2 = etc->aspect_y * etc->aspect_y;
    int *strengths = etc->strengths;
    int elo = etc->elo;
    int ehi = etc->ehi;
    int coupling;
    int *c_line = etc->c_line;
    int rand_shift;
    const signed char *tmline = etc->tmmat + (etc->y % etc->tmheight) * etc->tmwid;
    int tmmask = etc->tmwid - 1;

    xd = etc->width;

    /* Setup initial conditions for walking across the scanline. Because we
     * are dealing with multiple planes, we have arrays of each variable,
     * indexed by p = plane number.
     *   a[p]     = 2x+1 (where x is the horizontal distance to the nearest set pixel)
     *   b[p]     = 2y+1 (where y is the vertical distance to the nearest set pixel)
     *   r[p]     = distance^2 to the nearest set pixel in this plane.
     *   e_0_1[p] = error from pixel above
     *   e_1_0[p] = error from pixel to the left
     *   e_m1_1[p]= error from pixel above right
     *   e_1_1[p] = error from pixel above left
     * Because the source line is run length compressed, we need to run
     * length decode it. We do this using the following:
     *   count[p]   = number of pixels left in this run
     *   src_idx[p] = which run length entry we are currently on.
     */

    /* A potted recap of the distance calculations in the paper for easy
     * reference.
     *   distance to last dot   = SQR( (aspect_y * x)^2 + (aspect_x * y)^2 )
     *   r       =  distance^2  =      (aspect_y * x)^2 + (aspect_x * y)^2
     *           = aspect_y^2 * x^2 + aspect_x^2 * y^2
     *   r_below - r = (aspect_x^2 * (y+1)^2) - (aspect_x^2 * y^2)
     *               = aspect_x^2 * ( (y+1)^2 - y^2 )
     *               = aspect_x^2 * ( 2y + 1 )
     *   r_under - r = (aspect_y^2 * (x+1)^2) - (aspect_y^2 * x^2)
     *               = aspect_y^2 * ( (x+1)^2 - x^2 )
     *               = aspect_y^2 * ( 2x + 1 )
     * So, we keep:
     *   a       = aspect_y^2 * (2x+1)
     *   b       = aspect_x^2 * (2y+1)
     * And we can then update r by adding either a or b at each stage.
     */
    for (plane_idx = 0; plane_idx < n_planes; plane_idx++)
    {
        ETS_PlaneCtx *ctx = etc->plane_ctx[plane_idx];

        a[plane_idx] = aspect_y2; /* aspect_y^2 * (2x + 1) where x = 0 */
        b[plane_idx] = aspect_x2; /* aspect_x^2 * (2y + 1) where y = 0 */
        r[plane_idx] = 0;
        e_0_1[plane_idx] = 0;
        e_1_0[plane_idx] = 0;
        e_m1_1[plane_idx] = ctx->err_line[0];
    }

    coupling = 0;

    for (i = 0; i < xd; i++)
    {
#ifdef FANCY_COUPLING
        coupling += c_line[i];
#else
        coupling = 0;
#endif
        /* Lookup image data and compute R for all planes. */
        for (plane_idx = 0; plane_idx < n_planes; plane_idx++)
        {
            ETS_PlaneCtx *ctx = etc->plane_ctx[plane_idx];
            ETS_SrcPixel src_pixel = src[plane_idx][i];
            int new_r;
            int c1 = ctx->c1;
            int rlimit = 1 << (30 - ETS_SHIFT + c1);
            uchar *dst_ptr = dest[plane_idx];
            int new_e_1_0;
            int achieved_error;
            int err;
            int imo;
            int expected_r;

            pr = ctx->r_line;
            pa = ctx->a_line;
            pb = ctx->b_line;
            piir = ctx->err_line;

            im         = ctx->lut[src_pixel];      /* image pixel (ink level) */
            expected_r = ctx->dist_lut[src_pixel]; /* expected distance */
            rand_shift = ctx->rs_lut[src_pixel];   /* random noise shift */

            /* Forward pass distance computation; equation 2 from paper */
            if (r[plane_idx] + a[plane_idx] < pr[i])
            {
                r[plane_idx] += a[plane_idx];
                a[plane_idx] += 2*aspect_y2;
            }
            else
            {
                a[plane_idx] = pa[i];
                b[plane_idx] = pb[i];
                r[plane_idx] = pr[i];
            }

            /* Shuffle all the errors and read the next one. */
            e_1_1[plane_idx] = e_0_1[plane_idx];
            e_0_1[plane_idx] = e_m1_1[plane_idx];
            e_m1_1[plane_idx] = i == xd - 1 ? 0 : piir[i + 1];
            /* Reuse of variables here; new_e_1_0 is the total error passed
             * into this pixel, with the traditional fs weights. */
            new_e_1_0 = ((e_1_0[plane_idx] * 7 + e_m1_1[plane_idx] * 3 +
                          e_0_1[plane_idx] * 5 + e_1_1[plane_idx] * 1) >> 4);

            /* White pixels stay white */
            if (im == 0)
            {
                dst_ptr[i] = 0;
                /* If we are forcing white pixels to stay white, we should
                 * not propagate errors through them. Or at the very least
                 * we should attenuate such errors. */
                new_e_1_0 = 0;
            }
            else
            {
                /* The guts of ets (Equation 5) */
                int ets_bias;

                if (expected_r == 0)
                {
                    ets_bias = 0;
                }
                else
                {
                    /* Read the current distance, and clamp to avoid overflow
                     * in subsequent calculations. */
                    new_r = r[plane_idx];
                    if (new_r > rlimit)
                        new_r = rlimit;
                    /* Should we store back with the limit? */

                    /* Change the units on the distance to match our lut
                     * and subtract our actual distance (rg) from the expected
                     * distance (expected_r). */
                    rg = new_r << (ETS_SHIFT - c1);
                    ets_bias = rg - expected_r;

                    /* So ets_bias is the difference that we want to base our
                     * threshold modulation on (section 2.1 of the paper).
                     * Exactly how do we do that? We present various options
                     * here.
                     *   0   no modulation
                     *   1   what the code did when it came to me. No reference
                     *       to this in the paper.
                     *   2   use it unchanged.
                     *   3   like 1, but same shift either side of 0.
                     *   4+  scale the modulation down.
                     */
                    switch (etc->ets_style)
                    {
                    case 0:
                        ets_bias = 0;
                        break;
                    case 1:
                        if (ets_bias > 0) ets_bias >>= 3;
                        break;
                    case 2:
                        break;
                    case 3:
                        ets_bias >>= 3;
                        break;
                    default:
                        ets_bias /= etc->ets_style-3;
                    }
                }

                /* Non white pixels get biased, and have the error
                 * applied. The error starts from the total error passed
                 * in. */
                err = new_e_1_0;

                /* Plus any ETS bias (calculated above) */
                err += ets_bias;

                /* Plus any random noise. Again various options here:
                 *    0   No random noise
                 *    1   The code as it came to me, using lookup table
                 *    2   commented out when it came to me; using pseudo
                 *        random numbers generated from seed.
                 */
                switch(etc->r_style)
                {
                case 0:
                    break;
                case 2:
                    /* Add the two seeds together */
                    sum = seed1 + seed2;

                    /* If the add generated a carry, increment
                     * the result of the addition.
                     */
                    if (sum < seed1 || sum < seed2) sum++;

                    /* Seed2 becomes old seed1, seed1 becomes result */
                    seed2 = seed1;
                    seed1 = sum;

                    err -= (sum >> rand_shift) - (0x80000000 >> rand_shift);
                    break;
                case 1:
                    err += tmline[(i+ctx->tm_offset) & tmmask] << (24 - rand_shift);
                    break;
                }

                /* Clamp the error; this is explained in the paper in
                 * section 6 just after equation 7. */
                /* FIXME: Understand this better */
                if (err < elo)
                    err = elo;
                else if (err > ehi)
                    err = ehi;

                /* Add the coupling to our combined 'error + bias' value */
                /* FIXME: Are we sure this shouldn't be clamped? */
                err += coupling;

                /* Calculate imo = the quantised image value (Equation 7) */
#ifdef OLD_QUANT
                imo = ((err + im) * dith_mul) >> (ETS_SHIFT + 8);
#else
                imo = ((err + im) * dith_mul + (1 << (ETS_SHIFT + 7))) >> (ETS_SHIFT + 8);
#endif
                /* Clamp to allow for over/underflow due to large errors */
                if (imo < 0) imo = 0;
                else if (imo > levels - 1) imo = levels - 1;

                /* Store final output pixel */
                dst_ptr[i] = imo;

                /* Calculate the error between the desired and the obtained
                 * pixel values. */
                achieved_error = im - ((imo * imo_mul) >> IMO_SHIFT);

                /* And the error passed in is updated with the error for
                 * this pixel. */
                new_e_1_0 += achieved_error;

                /* Do the magic coupling here; strengths is 0 when
                 * multiplane optimisation is turned off, hence coupling
                 * remains 0 always. Equation 6. */
                coupling += (achieved_error * strengths[plane_idx]) >> 8;

                /* If we output a set pixel, then reset our distances. */
                if (imo != 0)
                {
                    a[plane_idx] = aspect_y2;
                    b[plane_idx] = aspect_x2;
                    r[plane_idx] = 0;
                }
            }

            /* Store the values back for the next pass (Equation 3) */
            pa[i] = a[plane_idx];
            pb[i] = b[plane_idx];
            pr[i] = r[plane_idx];
            piir[i] = new_e_1_0;
            e_1_0[plane_idx] = new_e_1_0;
        }
#ifdef FANCY_COUPLING
        coupling = coupling >> 1;
        c_line[i] = coupling;
#endif
    }

    /* Note: this isn't white optimized, but the payoff is probably not
       that important. */
#ifdef FANCY_COUPLING
    coupling = 0;
    for (i = xd - 1; i >= 0; i--)
    {
        coupling = (coupling + c_line[i]) >> 1;
        c_line[i] = (coupling - (coupling >> 4));
    }
#endif

    /* Update distances. Reverse scanline pass. */
    for (plane_idx = 0; plane_idx < n_planes; plane_idx++)
    {
        ETS_PlaneCtx *ctx = etc->plane_ctx[plane_idx];
        int av, bv, rv;
        int c1 = ctx->c1;
        int rlimit = 1 << (30 - ETS_SHIFT + c1);

        pr = ctx->r_line;
        pa = ctx->a_line;
        pb = ctx->b_line;

        av = aspect_y2;
        bv = aspect_x2;
        rv = 0;
        for (i = xd - 1; i >= 0; i--)
        {
            /* Equation 4 from the paper */
            if (rv + bv + av < pr[i] + pb[i])
            {
                rv += av;
                av += (aspect_y2<<1);
            }
            else
            {
                rv = pr[i];
                av = pa[i];
                bv = pb[i];
            }
            if (rv > rlimit) rv = rlimit;
            pa[i] = av;
            pb[i] = bv + (aspect_x2 << 1);
            pr[i] = rv + bv;
        }
    }

     etc->seed1 = seed1;
     etc->seed2 = seed2;
     etc->y++;
}

/**
 * ets_plane_free: Free an #EBPlaneCtx context.
 * @ctx: The #EBPlaneCtx context to free.
 *
 * Frees @ctx.
 **/
static void
ets_plane_free(ETS_PlaneCtx *ctx)
{
    free(ctx->err_line);
    free(ctx->r_line);
    free(ctx->a_line);
    free(ctx->b_line);
    free(ctx->lut);
    free(ctx->dist_lut);
    free(ctx->rs_lut);
    free(ctx);
}

/**
 * ets_new: Create new Even ToneFS screening context.
 * @source_width: Width of source buffer.
 * @dest_width: Width of destination buffer, in pixels.
 * @lut: Lookup table for gray values.
 *
 * Creates a new context for Even ToneFS screening.
 *
 * If @dest_width is larger than @source_width, then input lines will
 * be expanded using nearest-neighbor sampling.
 *
 * @lut should be an array of 256 values, one for each possible input
 * gray value. @lut is a lookup table for gray values. Output is from
 * 0 for white (no ink) to ....
 *
 *
 * Return value: The new #EBPlaneCtx context.
 **/
static ETS_PlaneCtx *
ets_plane_new(const ETS_Params *params, ETS_Ctx *etc, int plane_idx)
{
    int width = params->width;
    int *lut = params->luts[plane_idx];
    ETS_PlaneCtx *result;
    int i;
    int *new_lut;
    int *dist_lut;
    char *rs_lut;
    double distscale = compute_distscale(params);
    int c1;
    int rlimit;
    int log2_levels, log2_aspect;
    int rs_base;

    result = (ETS_PlaneCtx *)malloc(sizeof(ETS_PlaneCtx));

    result->width = width;

    log2_levels = ets_log2(params->levels);
    log2_aspect = ets_log2(params->aspect_x) + ets_log2(params->aspect_y); /* FIXME */
    c1 = 6 + log2_aspect + log2_levels;
    if (params->c1_scale)
        c1 -= params->c1_scale[plane_idx];
    result->c1 = c1;
    rlimit = 1 << (30 - ETS_SHIFT + c1);
    result->tm_offset = TM_WIDTH/ets_log2up(params->n_planes);

    /* Set up a lut to map input values from the source domain to the
     * amount of ink. Callers can provide a lut of their own, which can be
     * used for gamma correction etc. In the absence of this, a linear
     * distribution is assumed. The user supplied lut should map from
     * 'amount of light' to 'gamma adjusted amount of light', as the code
     * subtracts the final value from (1<<ETS_SHIFT) (typically 65536) to
     * get 'amount of ink'. */
    new_lut = (int *)malloc((ETS_SRC_MAX + 1) * sizeof(int));
    for (i = 0; i < ETS_SRC_MAX + 1; i++)
    {
        int nli;

        if (lut == NULL)
        {
#if ETS_SRC_MAX == 255
            nli = (i * 65793 + (i >> 7)) >> (24 - ETS_SHIFT);
#else
            nli = (i * ((double) (1 << ETS_SHIFT)) / ETS_SRC_MAX) + 0.5;
#endif
        }
        else
            nli = lut[i] >> (24 - ETS_SHIFT);
        if (params->polarity == ETS_BLACK_IS_ZERO)
            new_lut[i] = (1 << ETS_SHIFT) - nli;
        else
            new_lut[i] = nli;
    }

    /* Here we calculate 2 more lookup tables. These could be separated out
     * into 2 different loops, but are done in 1 to avoid a small amount of
     * recalculation.
     *   dist_lut[i] = expected distance between dots for a greyscale of level i
     *   rs_lut[i]   = whacky random noise scale factor.
     */
    dist_lut = (int *)malloc((ETS_SRC_MAX + 1) * sizeof(int));
    rs_lut   = (char *)malloc((ETS_SRC_MAX + 1) * sizeof(int));

    rs_base = 35 - ETS_SHIFT + log2_levels - params->rand_scale;

    /* The paper says that the expected 'value' for a grayshade g is:
     *     d_avg = 0.95 / 0.95/(g^2)
     * This seems wrong to me. Let's consider some common cases; for a given
     * greyscale, lay out the 'ideal' dithering, then consider removing each
     * set pixel in turn and measuring the distance between that pixel and
     * the closest set pixel.
     *
     * g = 1/2  #.#.#.#.   visibly, expected distance = SQR(2)
     *          .#.#.#.#
     *          #.#.#.#.
     *          .#.#.#.#
     *
     * g = 1/4  #.#.#.#.  expected distance = 2
     *          ........
     *          #.#.#.#.
     *          ........
     *
     * g = 1/16 #...#...  expected distance = 4
     *          ........
     *          ........
     *          ........
     *          #...#...
     *          ........
     *          ........
     *          ........
     *
     * This rough approach leads us to suspect that we should be finding
     * values roughly proportional to 1/SQR(g). Given the algorithm works in
     * terms of square distance, this means 1/g. This is at odds with the
     * value given in the paper. Being charitable and assuming that the paper
     * means 'squared distance' when it says 'value', we are still a square
     * off.
     *
     * Nonetheless, the code as supplied uses 0.95/g for the squared distance
     * (i.e. it appears to agree with our logic here).
     */
    for (i = 0; i <= ETS_SRC_MAX; i++)
    {
        double dist;
        int nl = new_lut[i] * (params->levels - 1);
        int rs;

        /* This is (or is supposed to be) equation 5 from the paper. If nl
         * is g, why aren't we dividing by nl*nl ? */
        if (nl == 0)
        {
            /* The expected distance for an ink level of 0 is infinite. Just
             * put 0! */
            dist = 0;
        }
        else if (nl >= ((1<<ETS_SHIFT)/(params->levels-1)))
        {
            /* New from RJW: Our distance measurements are only meaningful
             * within the bottom 'level band' of the output. Do not apply
             * ETS to higher ink levels. */
            dist = 0;
        }
        else
        {
            dist = (distscale * (1 << (2 * ETS_SHIFT - c1))) / nl;
            if (dist > rlimit << (ETS_SHIFT - c1))
                dist = rlimit << (ETS_SHIFT - c1);
        }

        if (params->rand_scale_luts == NULL)
        {
            rs = compute_randshift(nl, rs_base, params->levels);
            rs_lut[i] = rs;
        }
        else
        {
            int val = params->rand_scale_luts[plane_idx][i];

            rs_lut[i] = rs_base + 16 - ets_log2(val + (val >> 1));
        }
        dist_lut[i] = (int)dist;
    }

    result->lut = new_lut;
    result->dist_lut = dist_lut;
    result->rs_lut = rs_lut;

    result->err_line = (int *)calloc(width, sizeof(int));
    result->r_line = (int *)calloc(width, sizeof(int));
    result->a_line = (int *)calloc(width, sizeof(int));
    result->b_line = (int *)calloc(width, sizeof(int));
    for (i = 0; i < width; i++)
    {
        result->a_line[i] = 1;
        result->b_line[i] = 1;
        /* Initialize error with a non zero random value to ensure dots don't 
           land on dots when we have same planes with same gray level and
           the plane interaction option is turned off.  Ideally the level
           of this error should be based upon the values of the first line
           to ensure that things get primed properly */
        result->err_line[i] = -((rand () & 0x7fff) << 6) >> (24 - ETS_SHIFT);
    }

    return result;
}

ETS_Ctx *
ets_new(const ETS_Params *params)
{
    ETS_Ctx *result = (ETS_Ctx *)malloc(sizeof(ETS_Ctx));
    int n_planes = params->n_planes;
    int i;
    int using_vectors = 0;

    if (params->dump_file)
    {
        int header[5];

        header[0] = 0x70644245;
        header[1] = 'M' * 0x1010000 + 'I' * 0x101;
        header[2] = ETS_VERSION;
        header[3] = ETS_SRC_MAX;
        header[4] = sizeof(ETS_SrcPixel);
        fwrite(header, sizeof(int), sizeof(header) / sizeof(header[0]),
               params->dump_file);
        if (params->dump_level >= ETS_DUMP_PARAMS)
        {
            fwrite(params, 1, sizeof(ETS_Params), params->dump_file);
        }
        if (params->dump_level >= ETS_DUMP_LUTS)
        {
            for (i = 0; i < params->n_planes; i++)
                fwrite(params->luts[i], sizeof(int), ETS_SRC_MAX + 1,
                       params->dump_file);
        }
    }

    result->width = params->width;
    result->n_planes = n_planes;
    result->levels = params->levels;

    result->aspect_x = params->aspect_x;
    result->aspect_y = params->aspect_y;

    result->ehi = (int)(0.6 * (1 << ETS_SHIFT) / (params->levels - 1));
    result->elo = -result->ehi;

    result->strengths = (int *)malloc(sizeof(int) * n_planes);
    memcpy(result->strengths, params->strengths,
            sizeof(int) * n_planes);

    result->ets_style = params->ets_style;
    result->r_style = params->r_style;

    result->c_line = (int *)calloc(params->width, sizeof(int));

    result->seed1 = 0x5324879f;
    result->seed2 = 0xb78d0945;

    result->dump_file = params->dump_file;
    result->dump_level = params->dump_level;

    result->plane_ctx = (ETS_PlaneCtx **)malloc(sizeof(ETS_PlaneCtx *) * n_planes);
    for (i = 0; i < n_planes; i++)
        result->plane_ctx[i] = ets_plane_new(params, result, i);
    result->y = 0;
    result->tmmat = tmmat;
    result->tmwid = TM_WIDTH;
    result->tmheight = TM_HEIGHT;
    return result;
}

/**
 * even_better_free: Free an #EvenBetterCtx context.
 * @ctx: The #EvenBetterCtx context to free.
 *
 * Frees @ctx.
 **/
void
ets_free(ETS_Ctx *ctx)
{
    int i;
    int n_planes = ctx->n_planes;

    if (ctx->dump_file)
        fclose(ctx->dump_file);

    for (i = 0; i < n_planes; i++)
        ets_plane_free(ctx->plane_ctx[i]);
    free(ctx->plane_ctx);
    free(ctx->strengths);
    free(ctx->c_line);

    free(ctx);
}
