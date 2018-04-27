/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the Cortex-M core
 */

#include "common.h"
#include "cpu.h"
#include "hooks.h"

void cpu_init(void)
{
	/* Catch divide by 0 and unaligned access */
	CPU_NVIC_CCR |= CPU_NVIC_CCR_DIV_0_TRAP | CPU_NVIC_CCR_UNALIGN_TRAP;

	/* Enable reporting of memory faults, bus faults and usage faults */
	CPU_NVIC_SHCSR |= CPU_NVIC_SHCSR_MEMFAULTENA |
		CPU_NVIC_SHCSR_BUSFAULTENA | CPU_NVIC_SHCSR_USGFAULTENA;
}

#ifdef CONFIG_ARMV7M_CACHE
static void cpu_invalidate_dcache(void)
{
	int sets_count, ways_count;
	int set, way;

	/* Select Level-1 Data cache (for operations on CCSIDR) */
	CPU_SCB_CCSELR = 0;
	/* Ensure the write is effective */
	asm volatile("dsb");

	/* Number of cache 'sets' - 1 */
	sets_count = (CPU_SCB_CCSIDR >> 13) & 0x7FFF;
	/* Number of cache 'ways' - 1 */
	ways_count = (CPU_SCB_CCSIDR >> 3) & 0x3FF;

	for (set = sets_count; set >= 0 ; set--)
		for (way = ways_count; way >= 0 ; way--)
			/* D-cache invalidate by set/way */
			CPU_SCB_DCISW = (set << 5) | (way << 30);

	asm volatile("dsb; isb");
}

static void cpu_clean_invalidate_dcache(void)
{
	int sets_count, ways_count;
	int set, way;

	/* Select Level-1 Data cache (for operations on CCSIDR) */
	CPU_SCB_CCSELR = 0;
	/* Ensure the write is effective */
	asm volatile("dsb");

	/* Number of cache 'sets' - 1 */
	sets_count = (CPU_SCB_CCSIDR >> 13) & 0x7FFF;
	/* Number of cache 'ways' - 1 */
	ways_count = (CPU_SCB_CCSIDR >> 3) & 0x3FF;

	for (set = sets_count; set >= 0 ; set--)
		for (way = ways_count; way >= 0 ; way--)
			/* D-cache invalidate by set/way */
			CPU_SCB_DCCISW = (set << 5) | (way << 30);

	asm volatile("dsb; isb");
}

static void cpu_invalidate_icache(void)
{
	/*
	 * Invalidates the entire instruction cache to the point of
	 * unification.
	 */
	CPU_SCB_ICIALLU = 0;
	asm volatile("dsb; isb");
}

void cpu_enable_caches(void)
{
	/* Check whether the I-cache is already enabled */
	if (!(CPU_NVIC_CCR & CPU_NVIC_CCR_ICACHE)) {
		/* Invalidate the I-cache first */
		cpu_invalidate_icache();
		/* Turn on the caching */
		CPU_NVIC_CCR |= CPU_NVIC_CCR_ICACHE;
		asm volatile("dsb; isb");
	}
	/* Check whether the D-cache is already enabled */
	if (!(CPU_NVIC_CCR & CPU_NVIC_CCR_DCACHE)) {
		/* Invalidate the D-cache first */
		cpu_invalidate_dcache();
		/* Turn on the caching */
		CPU_NVIC_CCR |= CPU_NVIC_CCR_DCACHE;
		asm volatile("dsb; isb");
	}
}

static void cpu_sysjump_cache(void)
{
	/*
	 * Disable the I-cache and the D-cache
	 * so we will invalidate it after the sysjump if needed
	 * (e.g after a flash update).
	 */
	cpu_clean_invalidate_dcache();
	CPU_NVIC_CCR &= ~(CPU_NVIC_CCR_ICACHE | CPU_NVIC_CCR_DCACHE);
	asm volatile("dsb; isb");
}
DECLARE_HOOK(HOOK_SYSJUMP, cpu_sysjump_cache, HOOK_PRIO_LAST);
#endif /* CONFIG_ARMV7M_CACHE */
