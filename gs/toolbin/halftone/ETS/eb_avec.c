#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

#include <stdio.h> /* for FILE, just so we can include evenbetter-rll.h" */
#include "evenbetter-rll.h" /* for ET_SrcPixel */
#include "eb_avec.h"

void
eb_avec_core(eb_ctx_avec *ctx, vector unsigned char *destbuf[4],
	     eb_srcbuf *srcbuf, int offset, const signed char *tmbase)
{
    vector float e;
    vector float e_i_1;
    vector unsigned int r;
    vector unsigned int a;
    vector unsigned int b;
    vector unsigned int *r_line = ctx->r_line;
    vector unsigned int *a_line = ctx->a_line;
    vector unsigned int *b_line = ctx->b_line;
    vector unsigned int r_i;
    vector unsigned int a_i;
    vector unsigned int b_i;
    vector unsigned int u_0, u_1;
    vector unsigned short u16_0;
    vector unsigned char u8_0, u8_1, u8_2, u8_3, u8_4, u8_5, u8_6, u8_7;
    vector bool int b_0, b_1;
    vector float f_0, f_1, f_2, f_3, f_4;
    vector float *iir_line;
    vector float zero = (vector float) vec_splat_u32(0);
    vector float kernel = ctx->kernel;
    vector float r_mul = ctx->r_mul;
    vector float elo = ctx->elo;
    vector float ehi = ctx->ehi;
    vector float ohi = ctx->ohi;
    vector unsigned int aspect2 = ctx->aspect2;
    vector unsigned int ones = vec_splat_u32(1);
    vector unsigned int twos = vec_splat_u32(2);
    vector float mseveneighths = { -0.875, -0.875, -0.875, -0.875 };
    vector float im;
    vector float rb;
    vector float rs;
    vector float e_q;
    vector unsigned char tmpbuf[4];
    int i, off;
    int ctrl_w = 0x10100010;
    vector signed char tmvec;
    vector unsigned char tmperm;

    e = ctx->e;
    e_i_1 = ctx->e_i_1;
    r = ctx->r;
    a = ctx->a;
    b = ctx->b;
    off = offset;
    iir_line = ctx->iir_line + off;

    vec_dst(&a_line[off], ctrl_w, 0);
    vec_dst(&b_line[off], ctrl_w, 1);
    vec_dst(&r_line[off], ctrl_w, 2);
    vec_dst(iir_line, ctrl_w + 0x10000, 3);

    for (i = 0; i < 16; i++) {
	r_i = r_line[off];
	u_0 = vec_adds(r, a);
	b_0 = vec_cmpgt(r_i, u_0);
	r = vec_sel(r_i, u_0, b_0);
	a_i = a_line[off];
	u_1 = vec_adds(a, twos);
	a = vec_sel(a_i, u_1, b_0);
	b_i = b_line[off];
	b = vec_sel(b_i, b, b_0);

	/* compute FS kernel */
	im = srcbuf->im[i];
	rb = srcbuf->rb[i];
	rs = srcbuf->rs[i];
	f_3 = vec_ctf(r, 0);
	f_3 = vec_madd(f_3, r_mul, rb);
	//	f_4 = vec_abs(f_3);
	//	f_3 = vec_madd(f_4, mseveneighths, f_3);
	f_1 = vec_splat(kernel, 3);
	e = vec_madd(f_1, e, zero);
	f_1 = vec_splat(kernel, 0);
	e = vec_madd(f_1, e_i_1, e);
	e_i_1 = iir_line[0];
	f_1 = vec_splat(kernel, 1);
	f_2 = iir_line[1];
	e = vec_madd(f_1, f_2, e);
	f_1 = vec_splat(kernel, 2);
	e = vec_madd(f_1, e_i_1, e);

	/* add threshold modulation */
	tmvec = vec_lde(i, tmbase);
	tmperm = vec_lvsl(i, tmbase);
	tmvec = vec_perm(tmvec, tmvec, tmperm);
	tmvec = vec_splat(tmvec, 0);
	f_0 = vec_ctf((vector signed int)tmvec, 0);
	e_q = vec_madd(f_0, rs, e);
	e_q = vec_add(e_q, f_3);

	e_q = vec_min(e_q, ehi);
	e_q = vec_max(e_q, elo);
	e_q = vec_add(e_q, im);
	e_q = vec_min(e_q, ohi);

	u_0 = vec_ctu(e_q, 0); /* quantization step */
	b_0 = vec_cmpeq(im, zero);
	u_0 = vec_andc(u_0, b_0); /* force 0 if im == 0.0 */
	u16_0 = vec_packs(u_0, u_0);
	u8_0 = vec_packs(u16_0, u16_0);
	vec_ste((vector unsigned int)u8_0, 0, &((unsigned int *)tmpbuf)[i]);
	f_0 = vec_ctf(u_0, 0);
	e = vec_add(e, im);
	e = vec_sub(e, f_0);
	iir_line[0] = e;

	/* update r */
	b_1 = vec_cmpeq(u_0, (vector unsigned int)zero);
	a = vec_sel(ones, a, b_1);
	b = vec_sel(aspect2, b, b_1);
	r = vec_and(r, b_1);
	a_line[off] = a;
	b_line[off] = b;
	r_line[off] = r;

	iir_line++;
	off++;
    }
    ctx->e = e;
    ctx->e_i_1 = e_i_1;
    ctx->r = r;
    ctx->a = a;
    ctx->b = b;

    /* repack output */
    u8_0 = tmpbuf[0]; /* 0a0b0c0d1a1b1c1d2a2b2c2d3a3b3c3d */
    u8_1 = tmpbuf[1];
    u8_2 = tmpbuf[2];
    u8_3 = tmpbuf[3];
    u8_4 = vec_mergeh(u8_0, u8_2); /* 0a8a0b8b0c8c0d8d1a9a1b8b1c8c1d8d */
    u8_5 = vec_mergel(u8_0, u8_2); /* 2aAa...                     3dBd */
    u8_6 = vec_mergeh(u8_1, u8_3); /* 4aCa...                     5dDd */
    u8_7 = vec_mergel(u8_1, u8_3); /* 6aEa...                     7dFd */
    u8_0 = vec_mergeh(u8_4, u8_6); /* 0a4a8aCa0b4b8bCb...         8dCd */
    u8_1 = vec_mergel(u8_4, u8_6); /* 1a5a9aDa0b5b9bDb...         9dDd */
    u8_2 = vec_mergeh(u8_5, u8_7); /* 2a6aAaEa...                 AdEd */
    u8_3 = vec_mergel(u8_5, u8_7); /* 3a7aBaFa..        .         BdFd */
    u8_4 = vec_mergeh(u8_0, u8_2); /* 0a2a4a6a8aAaCaEa...         CbEb */
    u8_5 = vec_mergel(u8_0, u8_2); /* 0c2c...                     CdEd */
    u8_6 = vec_mergeh(u8_1, u8_3); /* 1a3a...                     DbFb */
    u8_7 = vec_mergel(u8_1, u8_3); /* 1c3c...                     DdFd */
    u8_0 = vec_mergeh(u8_4, u8_6); /* 0a1a2a3a...                 EaFa */
    u8_1 = vec_mergel(u8_4, u8_6); /* 0b1b2b3b...                 EbFb */
    u8_2 = vec_mergeh(u8_5, u8_7); /* 0c1c2c3c...                 EcFc */
    u8_3 = vec_mergel(u8_5, u8_7); /* 0d1d2d3d...       .         EcFd */
    destbuf[0][offset >> 4] = u8_0;
    destbuf[1][offset >> 4] = u8_1;
    destbuf[2][offset >> 4] = u8_2;
    destbuf[3][offset >> 4] = u8_3;
    
}

void
eb_avec_rev_rs(eb_ctx_avec *ctx, int offset)
{
    int i;
    vector unsigned int *r_line = ctx->r_line;
    vector unsigned int *a_line = ctx->a_line;
    vector unsigned int *b_line = ctx->b_line;
    vector unsigned int r;
    vector unsigned int a;
    vector unsigned int b;
    vector unsigned int r_i;
    vector unsigned int a_i;
    vector unsigned int b_i;
    vector unsigned int u_0, u_1, u_2, u_3, u_4, u_5;
    vector unsigned int aspect2 = ctx->aspect2;
    vector unsigned int twice_aspect2 = vec_add(aspect2, aspect2);
    vector unsigned int twos = vec_splat_u32(2);
    vector bool int b_0;
    unsigned int ctrl_w = 0x1010fff0;

    r = ctx->r;
    a = ctx->a;
    b = ctx->b;

    vec_dst(&a_line[offset], ctrl_w, 0);
    vec_dst(&b_line[offset], ctrl_w, 1);
    vec_dst(&r_line[offset], ctrl_w, 2);

    for (i = 0; i < 16; i++) {
	r_i = r_line[offset];
	a_i = a_line[offset];
	b_i = b_line[offset];
	u_0 = vec_adds(r, a);
	u_1 = vec_adds(u_0, b);
	u_2 = vec_adds(r_i, b_i);
	b_0 = vec_cmpgt(u_2, u_1);
	u_3 = vec_adds(a, twos);
	r = vec_sel(r_i, u_0, b_0);
	a = vec_sel(a_i, u_3, b_0);
	b = vec_sel(b_i, b, b_0);
	a_line[offset] = a;
	u_4 = vec_adds(b, twice_aspect2);
	b_line[offset] = u_4;
	u_5 = vec_adds(r, b);
	r_line[offset] = u_5;
	offset--;
    }
    ctx->r = r;
    ctx->a = a;
    ctx->b = b;
}

/* Prepare a source buffer from 8-bit input. Return 0 if source is
   all-zero. */
int
eb_avec_prep_srcbuf(eb_ctx_avec *ctx, int n_planes,
		    eb_srcbuf *srcbuf, const ET_SrcPixel *const *src,
		    int off)
{
    vector unsigned char c0, c1, c2, c3, c4, c5, c6, c7;
    vector unsigned char zero = vec_splat_u8(0);
    vector unsigned short s0, s1;
    vector unsigned int u0;
    vector float f0, f1;
    vector bool int b0;
    vector float imscale1 = ctx->imscale1;
    vector float imscale2 = ctx->imscale2;
    vector float rbmul = ctx->rbmul;
    vector float rsbase = ctx->rsbase;
    vector float onehalf = { 0.5, 0.5, 0.5, 0.5 };

    c0 = ((vector unsigned char *)src[0])[off >> 4];
    c1 = n_planes > 1 ? ((vector unsigned char *)src[1])[off >> 4] : zero;
    c2 = n_planes > 2 ? ((vector unsigned char *)src[2])[off >> 4] : zero;
    c3 = n_planes > 3 ? ((vector unsigned char *)src[3])[off >> 4] : zero;

    /* check for zeros */
    c4 = vec_or(c0, c1);
    c5 = vec_or(c2, c3);
    c4 = vec_or(c4, c5);
    if (vec_all_eq(c4, zero))
	return 0;

    c4 = vec_mergeh(c0, c2); /* C0 Y0 .. C7 Y7 */
    c5 = vec_mergeh(c1, c3); /* M0 K0 .. M7 K7 */
    c6 = vec_mergel(c0, c2); /* C8 Y8 ... */
    c7 = vec_mergel(c1, c3);

    c0 = vec_mergeh(c4, c5); /* C0 M0 Y0 K0 ... C3 M3 Y3 K3 */

    if (*((float *)&ctx->imscale2) == 0.0) {
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);
    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[0] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[0] = (vector float)f0;
    srcbuf->rs[0] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[1] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[1] = (vector float)f0;
    srcbuf->rs[1] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[2] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[2] = (vector float)f0;
    srcbuf->rs[2] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[3] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[3] = (vector float)f0;
    srcbuf->rs[3] = (vector float)rsbase;

    c0 = vec_mergel(c4, c5); /* C4 M4 Y4 K4 ... C7 M7 Y7 K7 */
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[4] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[4] = (vector float)f0;
    srcbuf->rs[4] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[5] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[5] = (vector float)f0;
    srcbuf->rs[5] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[6] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[6] = (vector float)f0;
    srcbuf->rs[6] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[7] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[7] = (vector float)f0;
    srcbuf->rs[7] = (vector float)rsbase;

    c0 = vec_mergeh(c6, c7); /* C8 M8 Y8 K8 ... Cb Mb Yb Kb */
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[8] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[8] = (vector float)f0;
    srcbuf->rs[8] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[9] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[9] = (vector float)f0;
    srcbuf->rs[9] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[10] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[10] = (vector float)f0;
    srcbuf->rs[10] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[11] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[11] = (vector float)f0;
    srcbuf->rs[11] = (vector float)rsbase;

    c0 = vec_mergel(c6, c7); /* Cc Mc Yc Kc ... Cf Mf Yf Kf */
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[12] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[12] = (vector float)f0;
    srcbuf->rs[12] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[13] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[13] = (vector float)f0;
    srcbuf->rs[13] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[14] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[14] = (vector float)f0;
    srcbuf->rs[14] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f0 = vec_madd(f0, imscale1, (vector float)zero);
    srcbuf->im[15] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[15] = (vector float)f0;
    srcbuf->rs[15] = (vector float)rsbase;
    } else {
      vector float foff = ctx->foff;
      c0 = vec_nor(c0, c0);
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[0] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[0] = (vector float)f0;
    srcbuf->rs[0] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[1] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[1] = (vector float)f0;
    srcbuf->rs[1] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[2] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[2] = (vector float)f0;
    srcbuf->rs[2] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[3] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[3] = (vector float)f0;
    srcbuf->rs[3] = (vector float)rsbase;

    c0 = vec_mergel(c4, c5); /* C4 M4 Y4 K4 ... C7 M7 Y7 K7 */
      c0 = vec_nor(c0, c0);
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[4] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[4] = (vector float)f0;
    srcbuf->rs[4] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[5] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[5] = (vector float)f0;
    srcbuf->rs[5] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[6] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[6] = (vector float)f0;
    srcbuf->rs[6] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[7] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[7] = (vector float)f0;
    srcbuf->rs[7] = (vector float)rsbase;

    c0 = vec_mergeh(c6, c7); /* C8 M8 Y8 K8 ... Cb Mb Yb Kb */
      c0 = vec_nor(c0, c0);
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[8] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[8] = (vector float)f0;
    srcbuf->rs[8] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[9] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[9] = (vector float)f0;
    srcbuf->rs[9] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[10] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[10] = (vector float)f0;
    srcbuf->rs[10] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[11] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[11] = (vector float)f0;
    srcbuf->rs[11] = (vector float)rsbase;

    c0 = vec_mergel(c6, c7); /* Cc Mc Yc Kc ... Cf Mf Yf Kf */
      c0 = vec_nor(c0, c0);
    s0 = (vector unsigned short)vec_mergeh(zero, c0);
    s1 = (vector unsigned short)vec_mergel(zero, c0);

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[12] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[12] = (vector float)f0;
    srcbuf->rs[12] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s0);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[13] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[13] = (vector float)f0;
    srcbuf->rs[13] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergeh((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[14] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[14] = (vector float)f0;
    srcbuf->rs[14] = (vector float)rsbase;

    u0 = (vector unsigned int)vec_mergel((vector unsigned short)zero, s1);
    f0 = vec_ctf(u0, 8);
    /* scale, do gamma here */
    f1 = vec_rsqrte(f0);
    f1 = vec_madd(f1, f0, (vector float)zero);
    f1 = vec_nmsub(f1, imscale2, foff);
    f0 = vec_nmsub(f0, imscale1, f1);
    b0 = vec_cmpge(f0, (vector float)zero);
    f0 = vec_sel(foff, f0, b0);
    srcbuf->im[15] = f0;
    f0 = vec_re(f0);
    f0 = vec_madd(f0, rbmul, onehalf);
    srcbuf->rb[15] = (vector float)f0;
    srcbuf->rs[15] = (vector float)rsbase;
    }

    return 1;
}
