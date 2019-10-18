/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copy from user space to user space
 *
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/linkage.h>

#include <asm/asm-uaccess.h>
#include <asm/assembler.h>
#include <asm/cache.h>

/*
 * Copy from user space to user space (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */

	.macro ldrb1 reg, ptr, offset=0
	USER_F(9998f, ldtrb \reg, [\ptr, \offset])
	.endm

	.macro strb1 reg, ptr, offset=0
	USER_F(9998f, sttrb \reg, [\ptr, \offset])
	.endm

	.macro ldrb1_reg reg, ptr, offset
	add \ptr, \ptr, \offset
	USER_F(9998f, ldtrb \reg, [\ptr])
	sub \ptr, \ptr, \offset
	.endm

	.macro strb1_reg reg, ptr, offset
	add \ptr, \ptr, \offset
	USER_F(9998f, sttrb \reg, [\ptr])
	sub \ptr, \ptr, \offset
	.endm

	.macro ldr1 reg, ptr, offset=0
	USER_F(9998f, ldtr \reg, [\ptr, \offset])
	.endm

	.macro str1 reg, ptr, offset=0
	USER_F(9998f, sttr \reg, [\ptr, \offset])
	.endm

	.macro ldp1 regA, regB, ptr, offset=0
	USER_F(9998f, ldtr \regA, [\ptr, \offset])
	USER_F(9998f, ldtr \regB, [\ptr, \offset + 8])
	.endm

	.macro stp1 regA, regB, ptr, offset=0
	USER_F(9998f, sttr \regA, [\ptr, \offset])
	USER_F(9998f, sttr \regB, [\ptr, \offset + 8])
	.endm

	.macro ldp1_pre regA, regB, ptr, offset
	USER_F(9998f, ldtr \regA, [\ptr, \offset])
	USER_F(9998f, ldtr \regB, [\ptr, \offset + 8])
	add \ptr, \ptr, \offset
	.endm

	.macro stp1_pre regA, regB, ptr, offset
	USER_F(9998f, sttr \regA, [\ptr, \offset])
	USER_F(9998f, sttr \regB, [\ptr, \offset + 8])
	add \ptr, \ptr, \offset
	.endm

	.macro ldrb1_nuao reg, ptr, offset=0
	USER_F(9998f, ldrb \reg, [\ptr, \offset])
	.endm

	.macro strb1_nuao reg, ptr, offset=0
	USER_F(9998f, strb \reg, [\ptr, \offset])
	.endm

	.macro ldrb1_nuao_reg reg, ptr, offset=0
	USER_F(9998f, ldrb \reg, [\ptr, \offset])
	.endm

	.macro strb1_nuao_reg reg, ptr, offset=0
	USER_F(9998f, strb \reg, [\ptr, \offset])
	.endm

	.macro ldr1_nuao reg, ptr, offset=0
	USER_F(9998f, ldr \reg, [\ptr, \offset])
	.endm

	.macro str1_nuao reg, ptr, offset=0
	USER_F(9998f, str \reg, [\ptr, \offset])
	.endm

	.macro ldp1_nuao  regA, regB, ptr, offset=0
	USER_F(9998f, ldp \regA, \regB, [\ptr, \offset])
	.endm

	.macro stp1_nuao regA, regB, ptr, offset=0
	USER_F(9998f, stp \regA, \regB, [\ptr, \offset])
	.endm

	.macro ldp1_pre_nuao regA, regB, ptr, offset
	USER_F(9998f, ldp \regA, \regB, [\ptr, \offset]!)
	.endm

	.macro stp1_pre_nuao regA, regB, ptr, offset
	USER_F(9998f, stp \regA, \regB, [\ptr, \offset]!)
	.endm

	.macro copy_exit
	b	.Luaccess_finish
	.endm

ENTRY(__arch_copy_in_user)
	uaccess_enable_not_uao x3, x4, x5
#include "copy_template_user.S"
.Luaccess_finish:
	uaccess_disable_not_uao x3, x4
	mov	x0, #0
	ret
ENDPROC(__arch_copy_in_user)
EXPORT_SYMBOL(__arch_copy_in_user)
#include "copy_user_fixup.S"
