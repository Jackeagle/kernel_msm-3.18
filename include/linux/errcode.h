/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_ERRCODE_H
#define _LINUX_ERRCODE_H

#include <linux/stddef.h>

#ifdef CONFIG_SYMBOLIC_ERRCODE
const char *errcode(int err);
#else
static inline const char *errcode(int err)
{
	return NULL;
}
#endif

#endif /* _LINUX_ERRCODE_H */
