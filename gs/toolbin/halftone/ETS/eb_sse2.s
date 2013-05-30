;;; Implementation of Even Better core in SSE2 instruction set

	struc	ebctx
.xs		resd	1
.iir_line	resd	1
.r_line		resd	1
.a_line		resd	1
.b_line		resd	1
.dummy		resd	3
.luts		resd	4
.e		resb	16
.e_i_1		resb	16
.r		resb	16
.a		resb	16
.b		resb	16
.ones		resb	16
.twos		resb	16
.aspect2	resb	16
.ehi		resd	4
.elo		resd	4
.ohi		resd	4
.r_mul		resd	4
.kernel		resd	4
.seed1		resd	4
.seed2		resd	4
.tmmat		resd	4
.y		resd	4
	endstruc

	struc	srcbuf
.im		resd	64	; image in floating pt
.rb		resd	64	; 0.5 - rb_lut, in floating pt
.rs		resd	64	; scale for randomness
	endstruc

;;; some of the optimization is tricky. For a = c ? a :	b, we write:
;;;	pandn	a, c	; a = (~a & c)
;;; 	por	b, c	; b = (b | c)
;;;	pandn	a, b	; a = (~(~a & c)) & (b | c)
;;; For the converse, a = c ? b : a, we write:
;;;	pxor	b, a	; b = (a ^ b)
;;;	pand	b, c	; b = (a ^ b) & c
;;;	pxor	a, b	; a = a ^ ((a ^ b) & c)
	
	global	eb_test_sse2
eb_test_sse2:
	push	ebx
	mov	eax, 1
	cpuid
	mov	eax, edx
	shr	eax, 26
	and	eax, byte 1
	jz	eb_test_sse2_1

eb_test_sse2_1:
	pop	ebx
	ret

	global	eb_sse2_set_daz
eb_sse2_set_daz:
	sub	esp, 4
	stmxcsr	[esp]
	mov	eax, [esp]
	or	word [esp], 0x8040
	ldmxcsr	[esp]
	add	esp, 4
	ret

	global	eb_sse2_restore_daz
eb_sse2_restore_daz:
	ldmxcsr	[esp + 4]
	ret

	global	eb_sse2_core
eb_sse2_core:
;;; screen one block of 16 pixels, 4 planes
	struc	args
.header		resb	8
.ctx		resd	1	; context
.destbuf	resd	1	; dest buffers
.srcbuf		resd	1	; source buffer struct
.offset		resd	1	; offset (in pixels)
.tmbase		resd	1	; base addr of threshold mod matrix
	endstruc
	push	ebp
	mov	ebp, esp
	push	ebx
	push	edi
	push	esi

	struc	frame
.obuf		resb	64
	endstruc
	sub	esp, frame_size
;;; align esp to 128-bit boundary
	and	esp, byte -0x10

	mov	eax, [ebp + args.ctx]
	mov	ebx, [ebp + args.srcbuf]
	mov	ecx, 0
	mov	edx, [ebp + args.offset]
	shl	edx, 4
	mov	edi, [eax + ebctx.iir_line]

	movaps	xmm6, [eax + ebctx.e]
	movdqa	xmm4, [eax + ebctx.r]
	movdqa	xmm0, [eax + ebctx.a]
	movdqa	xmm1, [eax + ebctx.b]

eb_sse2_core1:
;;; pixel loop
	;; invariant:	 xmm4 = r, xmm0 = a, xmm1 = b
	mov	esi, [eax + ebctx.r_line]
	paddd	xmm4, xmm0
	movdqa	xmm3, [esi + edx] ; r[i]
	movdqa	xmm2, xmm3
	movaps	xmm7, [eax + ebctx.e_i_1]
	pcmpgtd	xmm3, xmm4	; xmm3 is ones if r + a < rline[x]
	mov	esi, [eax + ebctx.a_line]
	pxor	xmm4, xmm2
	movdqa	xmm5, [esi + edx] ; a[i]
	pand	xmm4, xmm3
	paddd	xmm0, [eax + ebctx.twos]
	pxor	xmm2, xmm4	; r = ... ? r + a : r[i]
	movdqa	[eax + ebctx.r], xmm2
	pandn	xmm0, xmm3
	mov	esi, [eax + ebctx.b_line]
	por	xmm5, xmm3

	pandn	xmm0, xmm5	; a = ... ? a + 2 : a[i]
	movdqa	xmm5, [esi + edx] ; b[i]
	pandn	xmm1, xmm3
	movdqa	[eax + ebctx.a], xmm0
	por	xmm5, xmm3
	movaps	xmm3, [eax + ebctx.kernel]
	pandn	xmm1, xmm5	; b = ... ? b : b[i]
	movdqa	[eax + ebctx.b], xmm1

	;; xmm2 = r (updated)


 	mov	esi, [ebp + args.tmbase]
	xor	eax, eax
	mov	al, [esi + ecx]
	shl	eax, 24		; could avoid this by changing the rand lut
	cvtsi2ss xmm4, eax
	pshufd	xmm4, xmm4, 0
		;; note: if we swapped eax and esi, we could probably avoid this
	mov	eax, [ebp + args.ctx]

	;;  compute FS kernel
	pshufd	xmm1, xmm3, 0x00 ; 1/16
	mulps	xmm1, xmm7
	movaps	xmm7, [edi + edx]	;  e_i
	pshufd	xmm5, xmm3, 0xff ; 7/16
	mulps	xmm6, xmm5
	cvtdq2ps xmm2, xmm2
	mulps	xmm2, [eax + ebctx.r_mul]
	addps	xmm6, xmm1
	mulps	xmm4, [ebx + srcbuf.rs]
	pshufd	xmm0, xmm3, 0xaa ; 5/16
	mulps	xmm0, xmm7
	addps	xmm2, [ebx + srcbuf.rb]
	pshufd	xmm3, xmm3, 0x55 ; 3/16
	movaps	[eax + ebctx.e_i_1], xmm7
	addps	xmm0, xmm6
	movdqa	xmm5, [ebx + srcbuf.im]
	mulps	xmm3, [edi + edx + 0x10]	;  e[i+1]
	addps	xmm2, xmm4

	addps	xmm0, xmm3

	movaps	xmm6, xmm0
	addps	xmm0, xmm2
	minps	xmm0, [eax + ebctx.ehi]
	pxor	xmm4, xmm4
	maxps	xmm0, [eax + ebctx.elo]
	addps	xmm0, xmm5
	subps	xmm2, xmm2
	minps	xmm0, [eax + ebctx.ohi]
	addps	xmm6, xmm5
	maxps	xmm0, xmm2
	cvttps2dq	xmm1, xmm0 ; this is where the actual quantization is
	cmpneqps xmm2, xmm5
	pand	xmm1, xmm2	; force 0 if im == 0.0
	cvtdq2ps xmm3, xmm1
	subps	xmm6, xmm3
	movdqa	xmm0, [eax + ebctx.a]
	pcmpeqd	xmm4, xmm1	; 1's when pixel is 0
	;movdqa	xmm1, [eax + ebctx.r] ; for visualizing r buf when testing
	movaps	xmm7, [edi + edx]
	packssdw xmm1, xmm1
	movaps	[edi + edx], xmm6
	packuswb xmm1, xmm1
	movd	[esp + ecx * 4], xmm1

	;; update r
	mov	esi, [eax + ebctx.a_line]
	movdqa	xmm2, [eax + ebctx.ones]
	pandn	xmm0, xmm4
	por	xmm2, xmm4
	movdqa	xmm1, [eax + ebctx.b]
	pandn	xmm0, xmm2
	movdqa	[esi + edx], xmm0
	mov	esi, [eax + ebctx.b_line]
	pandn	xmm1, xmm4
	movdqa	xmm2, [eax + ebctx.aspect2]
	por	xmm2, xmm4
	pand	xmm4, [eax + ebctx.r]
	pandn	xmm1, xmm2
	movdqa	[esi + edx], xmm1
	add	ebx, byte 0x10
	mov	esi, [eax + ebctx.r_line]
	movdqa	[esi + edx], xmm4
	add	edx, byte 0x10
	add	ecx, byte 1

	cmp	ecx, byte 0x10
	jne	eb_sse2_core1

	movdqa	[eax + ebctx.r], xmm4
	movdqa	[eax + ebctx.a], xmm0
	movdqa	[eax + ebctx.b], xmm1
	movaps	[eax + ebctx.e], xmm6

;;; repack output
	mov	eax, [ebp + args.destbuf]
	mov	ecx, [ebp + args.offset]
	mov	ebx, [eax]
	mov	edx, [eax + 4]
	mov	esi, [eax + 8]
	mov	edi, [eax + 12]
	movdqa	xmm0, [esp]		; 3d3c3b3a2d2c2b2a1d1c1b1a0d0c0b0a
	;; 0 1 2 3
	movdqa	xmm4, xmm0
	movdqa	xmm2, [esp + 0x20]
	punpckhbw xmm4, xmm2	; Bd3dBc3cBb3cBa3aAd2dAc2cAb2bAa2a
	punpcklbw xmm0, xmm2	; 9d1d9c1c9b1b9a1a8d0d8c0c8b0b8a0a
	movdqa	xmm1, [esp + 0x10]	; 7d7c7b7a6d6c6b6a5d5c5b5a4d4c4b4a
	movdqa	xmm2, xmm1
	movdqa	xmm3, [esp + 0x30]
	punpcklbw xmm2, xmm3	; Dd5dDc5cDb5cDa5aCd4dCc4cCb4bCa4a
	punpckhbw xmm1, xmm3	; Fd7dFc7cFb7bFa7aEd6dEc6cEb6bEa6a
	;; 0 4 2 1
	movdqa	xmm3, xmm0
	punpckhbw xmm3, xmm2
	punpcklbw xmm0, xmm2
	movdqa	xmm2, xmm4
	punpcklbw xmm4, xmm1
	punpckhbw xmm2, xmm1
	;; 0 3 4 2
	movdqa	xmm1, xmm0
	punpckhbw xmm1, xmm4
	punpcklbw xmm0, xmm4
	movdqa	xmm4, xmm3
	punpcklbw xmm4, xmm2
	punpckhbw xmm3, xmm2
	;; 0 1 4 3
	movdqa	xmm2, xmm0
	punpckhbw xmm2, xmm4
	movdqa	[edx + ecx], xmm2
	punpcklbw xmm0, xmm4
	movdqa	[ebx + ecx], xmm0 ; FaEaDaCaBaAa9a8a7a6a5a4a3a2a1a0a
	movdqa	xmm4, xmm1
	punpcklbw xmm4, xmm3
	movdqa	[esi + ecx], xmm4
	punpckhbw xmm1, xmm3
	movdqa	[edi + ecx], xmm1
	;; 0 2 4 1

	mov	esi, [ebp - 12]
	mov	edi, [ebp - 8]
	mov	ebx, [ebp - 4]
	leave
	ret

	global	eb_sse2_rev_rs
eb_sse2_rev_rs:
	struc	rargs
.header		resb	8
.ctx		resd	1	; context
.offset		resd	1	; offset (in pixels)
	endstruc
	push	ebp
	mov	ebp, esp
	push	edi
	push	esi

	mov	eax, [ebp + rargs.ctx]
	mov	ecx, [ebp + rargs.offset]
	shl	ecx, 4
	mov	esi, [eax + ebctx.r_line]
	add	esi, ecx
	mov	edi, [eax + ebctx.a_line]
	add	edi, ecx
	mov	edx, [eax + ebctx.b_line]
	add	edx, ecx
	movdqa	xmm0, [eax + ebctx.r]
	movdqa	xmm1, [eax + ebctx.a]
	movdqa	xmm7, [eax + ebctx.aspect2]
	paddd	xmm7, xmm7
	movdqa	xmm2, [eax + ebctx.b]
	mov	ecx, 0x10
eb_rev_rs1:
	movdqa	xmm3, [esi]	; pr[i]
	paddd	xmm0, xmm1
	movdqa	xmm4, [edx]	; pb[i]
	movdqa	xmm6, xmm4
	movdqa	xmm5, xmm0
	paddd	xmm6, xmm3
	paddd	xmm5, xmm2
	pcmpgtd	xmm5, xmm6	; 1's if rv + bv + av > pr[i] + pb[i]
	paddd	xmm1, [eax + ebctx.twos]
	pxor	xmm3, xmm0
	pxor	xmm4, xmm2
	pand	xmm3, xmm5
	pand	xmm4, xmm5
	pxor	xmm0, xmm3
	pxor	xmm2, xmm4
	movdqa	xmm4, [edi]	; pa[i]
	movdqa	xmm3, xmm0
	pxor	xmm4, xmm1
	movdqa	xmm6, xmm2
	paddd	xmm3, xmm2
	paddd	xmm6, xmm7
	movdqa	[esi], xmm3	; pr[i] = rv + bv
	pand	xmm4, xmm5
	movdqa	[edx], xmm6	; pb[i] = bv + 2 * aspect_2
	pxor	xmm1, xmm4
	movdqa	[edi], xmm1	; pa[i] = av
	sub	edi, byte 0x10
	sub	edx, byte 0x10
	sub	esi, byte 0x10
	sub	ecx, byte 1
	jne	eb_rev_rs1
	movdqa	[eax + ebctx.r], xmm0
	movdqa	[eax + ebctx.a], xmm1
	movdqa	[eax + ebctx.b], xmm2

	pop	esi
	pop	edi
	leave
	ret

