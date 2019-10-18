/* SPDX-License-Identifier: GPL-2.0-only */

addr	.req	x15
.section .fixup,"ax"
.align	2
9998:
	// If it falls in the src range then it was a load that failed,
	// otherwise it was a store
	cmp addr, src
	ccmp addr, srcend, #0x0, ge
	csel x0, srcend, dstend, lt
	sub x0, x0, addr
	ret

