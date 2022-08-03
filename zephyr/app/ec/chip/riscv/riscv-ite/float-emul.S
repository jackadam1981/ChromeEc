/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IT8xxx2 implements the RISC-V F extension, but because compiler support
 * for M needs to be disabled, it is also difficult to enable F since
 * typical library configurations assume that F implies M and cause errors
 * if F is enabled but not M: RV32IAFC is not a generally supported
 * configuration, whereas RV32IMAFC is. See discussion at
 * https://github.com/zephyrproject-rtos/zephyr/pull/45881 for more
 * detail on why M is disabled and how it relates to F.
 *
 * This file implements the libgcc soft-float arithmetic functions in terms
 * of the F extension instructions, because they are known to be supported
 * by the CPU. Because the assembler doesn't recognize F instructions when
 * the extension is disabled, we need to write them as their opcodes
 * directly.
 */

.macro prolog name
.section .text
.align 2
.globl \name
.type \name, @function
\name:
.endm

.macro epilog name
    ret
.size \name, .-\name
.endm

/* op a0 -> a0 */
.macro ffunc1 name opcode
    prolog \name
    .word 0xf0050553    /* fmv.w.x fa0, a0 */
    .word \opcode
    .word 0xe0050553    /* fmv.x.w a0, fa0 */
    epilog \name
.endm

/* op (a0 a1) -> a0 */
.macro ffunc2 name opcode
    prolog \name
    .word 0xf0050553    /* fmv.w.x fa0, a0 */
    .word 0xf00585d3    /* fmv.w.x fa1, a1 */
    .word \opcode
    .word 0xe0050553    /* fmv.x.w a0, fa0 */
    epilog \name
.endm

/* fadd.s fa0, fa0, fa1 */
ffunc2 __addsf3 0x00b57553

/* fsub.s fa0, fa0, fa1 */
ffunc2 __subsf3 0x08b57553

/* fmul.s fa0, fa0, fa1 */
ffunc2 __mulsf3 0x10b57553

/* fdiv.s fa0, fa0, fa1 */
ffunc2 __divsf3 0x18b57553

/* float -> int */
    prolog __fixsfsi
    .word 0xf0050553    /* fmv.w.x fa0, a0 */
    .word 0xc0051553    /* fcvt.w.s a0, fa0, rtz */
    epilog __fixsfsi

/* int -> float */
    prolog __floatsisf
    .word 0xd0057553    /* fcvt.s.w fa0, a0 */
    .word 0xe0050553    /* fmv.x.w a0, fa0 */
    epilog __floatsisf
