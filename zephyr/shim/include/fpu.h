/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_FPU_H
#define __CROS_EC_FPU_H

/*
 * These functions are available in newlib but we are are using Zephyr's
 * minimal library at present.
 *
 * This file is not called math.h to avoid a conflict with the toolchain's
 * built-in version.
 *
 * This code is taken from core/cortex-m/include/fpu.h
 */

#ifdef CONFIG_PLATFORM_EC_FPU

/* Implementation for Cortex-M */
#ifdef CONFIG_CPU_CORTEX_M
static inline float sqrtf(float v)
{
	float root;

	/* Use the CPU instruction */
	__asm__ volatile("fsqrts %0, %1" : "=w"(root) : "w"(v));

	return root;
}

static inline float fabsf(float v)
{
	float root;

	/* Use the CPU instruction */
	__asm__ volatile("fabss %0, %1" : "=w"(root) : "w"(v));

	return root;
}
#elif CONFIG_RISCV
static inline float sqrtf(float v)
{
	float root;

#if CONFIG_SOC_IT8XXX2 && !CONFIG_FPU
	/*
	 * IT8xxx2 can't enable CONFIG_FPU but does support the F extension.
	 * These functions are implemented in terms of the relevant opcodes
	 * because the CPU is known to support the instructions but the
	 * assembler does not recognize their mnemonics when CONFIG_FPU is off.
	 * See riscv-ite/float-emul.S for more detail and related functions.
	 */
	register float vt __asm__("t0") = v;

	__asm__(
		/*
		 * When F is disabled we use the soft-float calling convention
		 * so need to move the input and output values between integer
		 * and floating-point registers.
		 */
		".word 0xf0028053\n" /* fmv.w.x ft0, t0 */
		".word 0x58007053\n" /* fsqrt.s ft0, ft0 */
		".word 0xe00002d3\n" /* fmv.x.w t0, ft0 */
		: "+r"(vt));
	root = vt;
#else
	__asm__("fsqrt.s %0, %1" : "=f"(root) : "f"(v));
#endif
	return root;
}

static inline float fabsf(float v)
{
	float abs;

	__asm__("fabs.s %0, %1" : "=f"(abs) : "f"(v));
	return abs;
}
#else
#error "Unsupported core: please add an implementation"
#endif

#endif /* CONFIG_PLATFORM_EC_FPU */

#endif /* __CROS_EC_MATH_H */
