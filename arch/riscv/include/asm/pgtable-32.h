/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#if !defined(CONFIG_RISCV_VM_SV32)
#error Only Sv32 supported
#endif

#include <asm-generic/pgtable-nopmd.h>
#include <linux/const.h>

/* Size of region mapped by a page global directory */
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#endif /* _ASM_RISCV_PGTABLE_32_H */
