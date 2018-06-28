/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_STACKLEAK_H
#define _LINUX_STACKLEAK_H

#include <linux/sched.h>
#include <linux/sched/task_stack.h>

/*
 * Check that the poison value points to the unused hole in the
 * virtual memory map for your platform.
 */
#define STACKLEAK_POISON -0xBEEF

#define STACKLEAK_POISON_CHECK_DEPTH 128

static inline void stackleak_task_init(struct task_struct *task)
{
#ifdef CONFIG_GCC_PLUGIN_STACKLEAK
	task->lowest_stack = (unsigned long)end_of_stack(task) +
						sizeof(unsigned long);
# ifdef CONFIG_STACKLEAK_METRICS
	task->prev_lowest_stack = task->lowest_stack;
# endif
#endif
}

#endif
