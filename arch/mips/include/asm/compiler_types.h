/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __ASM_COMPILER_TYPES_H__
#define __ASM_COMPILER_TYPES_H__

/*
 * With GCC 4.5 onwards we can use __builtin_unreachable to indicate to the
 * compiler that a particular code path will never be hit. This allows it to be
 * optimised out of the generated binary.
 *
 * Unfortunately at least GCC 4.6.3 through 7.3.0 inclusive suffer from a bug
 * that can lead to instructions from beyond an unreachable statement being
 * incorrectly reordered into earlier delay slots if the unreachable statement
 * is the only content of a case in a switch statement. This can lead to
 * seemingly random behaviour, such as invalid memory accesses from incorrectly
 * reordered loads or stores. See this potential GCC fix for details:
 *
 *   https://gcc.gnu.org/ml/gcc-patches/2015-09/msg00360.html
 *
 * It is unclear whether GCC 8 onwards suffer from the same issue - nothing
 * relevant is mentioned in GCC 8 release notes and nothing obviously relevant
 * stands out in GCC commit logs, but these newer GCC versions generate very
 * different code for the testcase which doesn't exhibit the bug.
 *
 * GCC also handles stack allocation suboptimally when calling noreturn
 * functions or calling __builtin_unreachable():
 *
 *   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82365
 *
 * We work around both of these issues by placing a volatile asm statement,
 * which GCC is prevented from reordering past, prior to __builtin_unreachable
 * calls.
 *
 * The .insn statement is required to ensure that any branches to the
 * statement, which sadly must be kept due to the asm statement, are known to
 * be branches to code and satisfy linker requirements for microMIPS kernels.
 */
#define barrier_before_unreachable() asm volatile(".insn")

#endif /* __ASM_COMPILER_TYPES_H__ */
