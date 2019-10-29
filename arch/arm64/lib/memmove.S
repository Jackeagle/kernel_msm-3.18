/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013 Linaro Limited. All rights reserved.
 * Copyright (c) 2015 ARM Ltd. All rights reserved.
 *
 * This code is based on glibc Cortex Strings work originally authored by
 * Linaro, found at:
 *
 * https://git.linaro.org/toolchain/cortex-strings.git
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/cache.h>

/*
 * Move a buffer from src to test (alignment handled by the hardware).
 * If dest <= src, call memcpy, otherwise copy in reverse order.
 *
 * Parameters:
 *	x0 - dest
 *	x1 - src
 *	x2 - n
 * Returns:
 *	x0 - dest
 */
/* Parameters and result.  */
#define dstin	x0
#define src	x1
#define count	x2
#define srcend	x3
#define dstend	x4
#define tmp1	x5
#define A_l	x6
#define A_h	x7
#define B_l	x8
#define B_h	x9
#define C_l	x10
#define C_h	x11
#define D_l	x12
#define D_h	x13
#define E_l	count
#define E_h	tmp1

/* All memmoves up to 96 bytes are done by memcpy as it supports overlaps.
   Larger backwards copies are also handled by memcpy. The only remaining
   case is forward large copies.  The destination is aligned, and an
   unrolled loop processes 64 bytes per iteration.
*/

    .weak memmove
ENTRY(__memmove)
ENTRY(memmove)
	sub	tmp1, dstin, src
	cmp	count, 96
	ccmp	tmp1, count, 2, hi
	b.hs	__memcpy

	cbz	tmp1, 3f
	add	dstend, dstin, count
	add	srcend, src, count

	/* Align dstend to 16 byte alignment so that we don't cross cache line
	   boundaries on both loads and stores.	 There are at least 96 bytes
	   to copy, so copy 16 bytes unaligned and then align.	The loop
	   copies 64 bytes per iteration and prefetches one iteration ahead.  */

	and	tmp1, dstend, 15
	ldp	D_l, D_h, [srcend, -16]
	sub	srcend, srcend, tmp1
	sub	count, count, tmp1
	ldp	A_l, A_h, [srcend, -16]
	stp	D_l, D_h, [dstend, -16]
	ldp	B_l, B_h, [srcend, -32]
	ldp	C_l, C_h, [srcend, -48]
	ldp	D_l, D_h, [srcend, -64]!
	sub	dstend, dstend, tmp1
	subs	count, count, 128
	b.ls	2f
	nop
1:
	stp	A_l, A_h, [dstend, -16]
	ldp	A_l, A_h, [srcend, -16]
	stp	B_l, B_h, [dstend, -32]
	ldp	B_l, B_h, [srcend, -32]
	stp	C_l, C_h, [dstend, -48]
	ldp	C_l, C_h, [srcend, -48]
	stp	D_l, D_h, [dstend, -64]!
	ldp	D_l, D_h, [srcend, -64]!
	subs	count, count, 64
	b.hi	1b

	/* Write the last full set of 64 bytes.	 The remainder is at most 64
	   bytes, so it is safe to always copy 64 bytes from the start even if
	   there is just 1 byte left.  */
2:
	ldp	E_l, E_h, [src, 48]
	stp	A_l, A_h, [dstend, -16]
	ldp	A_l, A_h, [src, 32]
	stp	B_l, B_h, [dstend, -32]
	ldp	B_l, B_h, [src, 16]
	stp	C_l, C_h, [dstend, -48]
	ldp	C_l, C_h, [src]
	stp	D_l, D_h, [dstend, -64]
	stp	E_l, E_h, [dstin, 48]
	stp	A_l, A_h, [dstin, 32]
	stp	B_l, B_h, [dstin, 16]
	stp	C_l, C_h, [dstin]
3:	ret

ENDPIPROC(memmove)
EXPORT_SYMBOL(memmove)
ENDPROC(__memmove)
EXPORT_SYMBOL(__memmove)
