/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/linkage.h>

#include <asm/asm-uaccess.h>
#include <asm/assembler.h>
#include <asm/cache.h>

/*
 * Copy to user space from a kernel buffer (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */

	.macro ldrb1 reg, ptr, offset=0
	ldrb \reg, [\ptr, \offset]
	.endm

	.macro strb1 reg, ptr, offset=0
	8888: sttrb \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro ldrb1_reg reg, ptr, offset
	ldrb \reg, [\ptr, \offset]
	.endm

	.macro strb1_reg reg, ptr, offset
	add \ptr, \ptr, \offset
	8888: sttrb \reg, [\ptr]
	sub \ptr, \ptr, \offset
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro ldr1 reg, ptr, offset=0
	ldr \reg, [\ptr, \offset]
	.endm

	.macro str1 reg, ptr, offset=0
	8888: sttr \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro ldp1 regA, regB, ptr, offset=0
	ldp \regA, \regB, [\ptr, \offset]
	.endm

	.macro stp1 regA, regB, ptr, offset=0
	8888: sttr \regA, [\ptr, \offset]
	8889: sttr \regB, [\ptr, \offset + 8]
	_asm_extable_faultaddr	8888b,9998f;
	_asm_extable_faultaddr	8889b,9998f;
	.endm

	.macro ldp1_pre regA, regB, ptr, offset
	ldp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro stp1_pre regA, regB, ptr, offset
	8888: sttr \regA, [\ptr, \offset]
	8889: sttr \regB, [\ptr, \offset + 8]
	add \ptr, \ptr, \offset
	_asm_extable_faultaddr	8888b,9998f;
	_asm_extable_faultaddr	8889b,9998f;
	.endm

	.macro ldrb1_nuao reg, ptr, offset=0
	ldrb \reg, [\ptr, \offset]
	.endm

	.macro strb1_nuao reg, ptr, offset=0
	8888: strb \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro ldrb1_nuao_reg reg, ptr, offset=0
	ldrb \reg, [\ptr, \offset]
	.endm

	.macro strb1_nuao_reg reg, ptr, offset=0
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldr1_nuao reg, ptr, offset=0
	ldr \reg, [\ptr, \offset]
	.endm

	.macro str1_nuao reg, ptr, offset=0
	8888: str \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro ldp1_nuao  regA, regB, ptr, offset=0
	ldp \regA, \regB, [\ptr, \offset]
	.endm

	.macro ldp1_pre_nuao regA, regB, ptr, offset
	ldp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro stp1_nuao regA, regB, ptr, offset=0
	8888: stp \regA, \regB, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro stp1_pre_nuao regA, regB, ptr, offset
	8888: stp \regA, \regB, [\ptr, \offset]!
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro copy_exit
	b	.Luaccess_finish
	.endm

ENTRY(__arch_copy_to_user)
	uaccess_enable_not_uao x3, x4, x5
#include "copy_template_user.S"
.Luaccess_finish:
	uaccess_disable_not_uao x3, x4
	mov	x0, #0
	ret
ENDPROC(__arch_copy_to_user)
EXPORT_SYMBOL(__arch_copy_to_user)
#include "copy_user_fixup.S"
