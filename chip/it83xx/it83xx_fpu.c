/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This macro will mask all interrupts and switching
 * CPU's ALU (Arithmetic Logic Unit) to floating point operation mode.
 * (IEEE standard 754 floating point)
 */
#define ENABLE_IT83XX_FPU() do { \
		asm volatile("sethi $r4,0x80\n\t"); \
		asm volatile("ori $r4,$r4,0x189\n\t"); \
		asm volatile("mtsr $r4,$dlmb\n\t"); \
		asm volatile("dsb\n\t"); \
	} while (0)

/* Restore interrupts and ALU. */
#define DISABLE_IT83XX_FPU() do { \
		asm volatile("sethi $r4,0x80\n\t"); \
		asm volatile("ori $r4,$r4,0x9\n\t"); \
		asm volatile("mtsr $r4,$dlmb\n\t"); \
		asm volatile("dsb\n\t"); \
	} while (0)

float __addsf3(float a, float b)
{
	float ret;

	ENABLE_IT83XX_FPU();
	asm volatile (
		/* Floating-point addition single-precision */
		"add45 %1,%2\n\t"
		: "=r"(ret)
		: "r"(a), "r"(b)
		: "$r4"
	);
	DISABLE_IT83XX_FPU();

	return ret;
}

float __subsf3(float a, float b)
{
	float ret;

	ENABLE_IT83XX_FPU();
	asm volatile (
		/* Floating-point subtraction single-precision */
		"sub45 %1,%2\n\t"
		: "=r"(ret)
		: "r"(a), "r"(b)
		: "$r4"
	);
	DISABLE_IT83XX_FPU();

	return ret;
}

float __mulsf3(float a, float b)
{
	float ret;

	ENABLE_IT83XX_FPU();
	asm volatile (
		/* Floating-point multiplication single-precision */
		"mul33 %1,%2\n\t"
		: "=r"(ret)
		: "r"(a), "r"(b)
		: "$r4"
	);
	DISABLE_IT83XX_FPU();

	return ret;
}

float __divsf3(float a, float b)
{
	float ret;

	ENABLE_IT83XX_FPU();
	asm volatile (
		/* Floating-point division single-precision */
		"divsr %1,%1,%1,%2\n\t"
		: "=r"(ret)
		: "r"(a), "r"(b)
		: "$r4"
	);
	DISABLE_IT83XX_FPU();

	return ret;
}
