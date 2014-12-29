/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cm3.h"

/*
 * Configure and start the systick timer
 * @details Enables the timer, interrupt generation, and clock source as the core clock
 * @param reload_val Reload value (24-bits)
 */
void cm3_systick_config(uint32_t reload_val, uint32_t int_en)
{
	GREG32(M3, SYST_RVR) = reload_val;
	GREG32(M3, SYST_CSR) = GC_M3_SYST_CSR_ENABLE_MASK |
		GC_M3_SYST_CSR_CLKSOURCE_MASK;

	if (int_en)
		GREG32(M3, SYST_CSR) |= GC_M3_SYST_CSR_TICKINT_MASK;
}

/*
 * Return the current systick timer value
 * @returns Current systick value
 * @note This value is a 24-bit downcounter
 */
uint32_t cm3_systick_read(void)
{
	return GREG32(M3, SYST_CVR);
}
