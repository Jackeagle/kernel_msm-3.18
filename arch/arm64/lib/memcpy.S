/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2013 ARM Ltd.
 * Copyright (C) 2013 Linaro.
 *
 * This code is based on glibc cortex strings work originally authored by Linaro
 * be found @
 *
 * http://bazaar.launchpad.net/~linaro-toolchain-dev/cortex-strings/trunk/
 * files/head:/src/aarch64/
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/cache.h>

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

 #define L(l) .L ## l

	.macro ldrb1 reg, ptr, offset=0
	ldrb \reg, [\ptr, \offset]
	.endm

	.macro strb1 reg, ptr, offset=0
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldr1 reg, ptr, offset=0
	ldr \reg, [\ptr, \offset]
	.endm

	.macro str1 reg, ptr, offset=0
	str \reg, [\ptr, \offset]
	.endm

	.macro ldp1 regA, regB, ptr, offset=0
	ldp \regA, \regB, [\ptr, \offset]
	.endm

	.macro stp1 regA, regB, ptr, offset=0
	stp \regA, \regB, [\ptr, \offset]
	.endm

	.macro ldrb1_reg reg, ptr, offset
	ldrb1 \reg, \ptr, \offset
	.endm

	.macro strb1_reg reg, ptr, offset
	strb1 \reg, \ptr, \offset
	.endm

	.macro ldp1_pre regA, regB, ptr, offset
	ldp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro stp1_pre regA, regB, ptr, offset
	stp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro copy_exit
	ret
	.endm

	.weak memcpy
ENTRY(__memcpy)
ENTRY(memcpy)
#include "copy_template.S"
ENDPIPROC(memcpy)
EXPORT_SYMBOL(memcpy)
ENDPROC(__memcpy)
EXPORT_SYMBOL(__memcpy)
