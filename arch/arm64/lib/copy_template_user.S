/* SPDX-License-Identifier: GPL-2.0-only */

#define L(l) .L ## l

	alternative_if_not ARM64_HAS_UAO
	b	L(copy_non_uao)
	alternative_else_nop_endif
#include "copy_template.S"

#define ldp1 ldp1_nuao
#define ldp1_pre ldp1_pre_nuao
#define stp1 stp1_nuao
#define stp1_pre stp1_pre_nuao
#define ldr1 ldr1_nuao
#define str1 str1_nuao
#define ldrb1 ldrb1_nuao
#define strb1 strb1_nuao
#define ldrb1_reg ldrb1_nuao_reg
#define strb1_reg strb1_nuao_reg

L(copy_non_uao):
#undef L
#define L(l) .Lnuao ## l
#include "copy_template.S"
