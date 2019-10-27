/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012 Linaro Limited. All rights reserved.
 * Copyright (c) 2015 ARM Ltd. All rights reserved.
 *
 * This code is based on glibc Cortex Strings work originally authored by
 * Linaro, found at:
 *
 * https://git.linaro.org/toolchain/cortex-strings.git
 */


/*
 * Copy a buffer from src to dest (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - dest
 *	x1 - src
 *	x2 - n
 * Returns:
 *	x0 - dest
 */
 #define dstin	x0
 #define src	x1
 #define count	x2
 #define dst	x3
 #define srcend	x4
 #define dstend	x5
 #define A_l	x6
 #define A_lw	w6
 #define A_h	x7
 #define A_hw	w7
 #define B_l	x8
 #define B_lw	w8
 #define B_h	x9
 #define C_l	x10
 #define C_h	x11
 #define D_l	x12
 #define D_h	x13
 #define E_l	src
 #define E_h	count
 #define F_l	srcend
 #define F_h	dst
 #define tmp1	x9

	prfm	PLDL1KEEP, [src]
	add	srcend, src, count
	add	dstend, dstin, count
	cmp	count, 16
	b.ls	L(copy16)
	cmp	count, 96
	b.hi	L(copy_long)

	/* Medium copies: 17..96 bytes.  */
	sub	tmp1, count, 1
	ldp1	A_l, A_h, src
	tbnz	tmp1, 6, L(copy96)
	ldp1	D_l, D_h, srcend, -16
	tbz	tmp1, 5, 1f
	ldp1	B_l, B_h, src, 16
	ldp1	C_l, C_h, srcend, -32
	stp1	B_l, B_h, dstin, 16
	stp1	C_l, C_h, dstend, -32
1:
	stp1	A_l, A_h, dstin
	stp1	D_l, D_h, dstend, -16
	copy_exit

	.p2align 4
	/* Small copies: 0..16 bytes.  */
L(copy16):
	cmp	count, 8
	b.lo	1f
	ldr1	A_l, src
	ldr1	A_h, srcend, -8
	str1	A_l, dstin
	str1	A_h, dstend, -8
	copy_exit
	.p2align 4
1:
	tbz	count, 2, 1f
	ldr1	A_lw, src
	ldr1	A_hw, srcend, -4
	str1	A_lw, dstin
	str1	A_hw, dstend, -4
	copy_exit

	/* Copy 0..3 bytes.  Use a branchless sequence that copies the same
	   byte 3 times if count==1, or the 2nd byte twice if count==2.  */
1:
	cbz	count, 2f
	lsr	tmp1, count, 1
	ldrb1	A_lw, src
	ldrb1	A_hw, srcend, -1
	ldrb1_reg	B_lw, src, tmp1
	strb1	A_lw, dstin
	strb1_reg	B_lw, dstin, tmp1
	strb1	A_hw, dstend, -1
2:	copy_exit

	.p2align 4
	/* Copy 64..96 bytes.  Copy 64 bytes from the start and
	   32 bytes from the end.  */
L(copy96):
	ldp1	B_l, B_h, src, 16
	ldp1	C_l, C_h, src, 32
	ldp1	D_l, D_h, src, 48
	ldp1	E_l, E_h, srcend, -32
	ldp1	F_l, F_h, srcend, -16
	stp1	A_l, A_h, dstin
	stp1	B_l, B_h, dstin, 16
	stp1	C_l, C_h, dstin, 32
	stp1	D_l, D_h, dstin, 48
	stp1	E_l, E_h, dstend, -32
	stp1	F_l, F_h, dstend, -16
	copy_exit

	/* Align DST to 16 byte alignment so that we don't cross cache line
	   boundaries on both loads and stores.	 There are at least 96 bytes
	   to copy, so copy 16 bytes unaligned and then align.	The loop
	   copies 64 bytes per iteration and prefetches one iteration ahead.  */

	.p2align 4
L(copy_long):
	and	tmp1, dstin, 15
	bic	dst, dstin, 15
	ldp1	D_l, D_h, src
	sub	src, src, tmp1
	add	count, count, tmp1	/* Count is now 16 too large.  */
	ldp1	A_l, A_h, src, 16
	stp1	D_l, D_h, dstin
	ldp1	B_l, B_h, src, 32
	ldp1	C_l, C_h, src, 48
	ldp1_pre	D_l, D_h, src, 64
	subs	count, count, 128 + 16	/* Test and readjust count.  */
	b.ls	2f
1:
	stp1	A_l, A_h, dst, 16
	ldp1	A_l, A_h, src, 16
	stp1	B_l, B_h, dst, 32
	ldp1	B_l, B_h, src, 32
	stp1	C_l, C_h, dst, 48
	ldp1	C_l, C_h, src, 48
	stp1_pre	D_l, D_h, dst, 64
	ldp1_pre	D_l, D_h, src, 64
	subs	count, count, 64
	b.hi	1b

	/* Write the last full set of 64 bytes.	 The remainder is at most 64
	   bytes, so it is safe to always copy 64 bytes from the end even if
	   there is just 1 byte left.  */
2:
	ldp1	E_l, E_h, srcend, -64
	stp1	A_l, A_h, dst, 16
	ldp1	A_l, A_h, srcend, -48
	stp1	B_l, B_h, dst, 32
	ldp1	B_l, B_h, srcend, -32
	stp1	C_l, C_h, dst, 48
	ldp1	C_l, C_h, srcend, -16
	stp1	D_l, D_h, dst, 64
	stp1	E_l, E_h, dstend, -64
	stp1	A_l, A_h, dstend, -48
	stp1	B_l, B_h, dstend, -32
	stp1	C_l, C_h, dstend, -16
	copy_exit
