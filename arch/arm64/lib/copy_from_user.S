/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 ARM Ltd.
 */

#include <linux/linkage.h>

#include <asm/asm-uaccess.h>
#include <asm/assembler.h>
#include <asm/cache.h>

/*
 * Copy from user space to a kernel buffer (alignment handled by the hardware)
 *
 * Parameters:
 *	x0 - to
 *	x1 - from
 *	x2 - n
 * Returns:
 *	x0 - bytes not copied
 */

	.macro ldrb1 reg, ptr, offset=0
	8888: ldtrb \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro strb1 reg, ptr, offset=0
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldrb1_reg reg, ptr, offset
	add \ptr, \ptr, \offset
	8888: ldtrb \reg, [\ptr]
	sub \ptr, \ptr, \offset
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro strb1_reg reg, ptr, offset
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldr1 reg, ptr, offset=0
	8888: ldtr \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro str1 reg, ptr, offset=0
	str \reg, [\ptr, \offset]
	.endm

	.macro ldp1 regA, regB, ptr, offset=0
	8888: ldtr \regA, [\ptr, \offset]
	8889: ldtr \regB, [\ptr, \offset + 8]
	_asm_extable_faultaddr	8888b,9998f;
	_asm_extable_faultaddr	8889b,9998f;
	.endm

	.macro stp1 regA, regB, ptr, offset=0
	stp \regA, \regB, [\ptr, \offset]
	.endm

	.macro ldp1_pre regA, regB, ptr, offset
	8888: ldtr \regA, [\ptr, \offset]
	8889: ldtr \regB, [\ptr, \offset + 8]
	add \ptr, \ptr, \offset
	_asm_extable_faultaddr	8888b,9998f;
	_asm_extable_faultaddr	8889b,9998f;
	.endm

	.macro stp1_pre regA, regB, ptr, offset
	stp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro ldrb1_nuao reg, ptr, offset=0
	8888: ldrb \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro strb1_nuao reg, ptr, offset=0
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldrb1_nuao_reg reg, ptr, offset=0
	8888: ldrb \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro strb1_nuao_reg reg, ptr, offset=0
	strb \reg, [\ptr, \offset]
	.endm

	.macro ldr1_nuao reg, ptr, offset=0
	8888: ldr \reg, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro str1_nuao reg, ptr, offset=0
	str \reg, [\ptr, \offset]
	.endm

	.macro ldp1_nuao  regA, regB, ptr, offset=0
	8888: ldp \regA, \regB, [\ptr, \offset]
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro stp1_nuao regA, regB, ptr, offset=0
	stp \regA, \regB, [\ptr, \offset]
	.endm

	.macro ldp1_pre_nuao regA, regB, ptr, offset
	8888: ldp \regA, \regB, [\ptr, \offset]!
	_asm_extable_faultaddr	8888b,9998f;
	.endm

	.macro stp1_pre_nuao regA, regB, ptr, offset
	stp \regA, \regB, [\ptr, \offset]!
	.endm

	.macro copy_exit
	b	.Luaccess_finish
	.endm

ENTRY(__arch_copy_from_user)
	uaccess_enable_not_uao x3, x4, x5
#include "copy_template_user.S"
.Luaccess_finish:
	uaccess_disable_not_uao x3, x4
	mov	x0, #0
	ret
ENDPROC(__arch_copy_from_user)
EXPORT_SYMBOL(__arch_copy_from_user)
#include "copy_user_fixup.S"
