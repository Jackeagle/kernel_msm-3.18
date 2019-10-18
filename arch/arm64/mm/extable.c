// SPDX-License-Identifier: GPL-2.0
/*
 * Based on arch/arm/mm/extable.c
 */

#include <linux/extable.h>
#include <linux/uaccess.h>

int fixup_exception(struct pt_regs *regs, unsigned long addr)
{
	const struct exception_table_entry *fixup;

	fixup = search_exception_tables(instruction_pointer(regs));
	if (fixup) {
		unsigned long offset = fixup->fixup;
		if (offset & FIXUP_WITH_ADDR) {
			regs->regs[15] = addr;
			offset &= ~FIXUP_WITH_ADDR;
		}
		regs->pc = (unsigned long)&fixup->fixup + offset;
	}
	return fixup != NULL;
}
