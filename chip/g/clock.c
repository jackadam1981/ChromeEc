/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "registers.h"
#include "pmu.h"

void clock_init(void)
{
	pmu_clock_switch_rc_trim(1);
}

void clock_enable_module(enum module_id module, int enable)
{
	switch (module) {
	case MODULE_UART:
		pmu_clock_en(PERIPH_UART0);
		break;
	case MODULE_I2C:
		pmu_clock_en(PERIPH_I2C0);
		pmu_clock_en(PERIPH_I2C1);
		break;
	case MODULE_SPI_MASTER:
		pmu_clock_en(PERIPH_SPI);
		break;
	case MODULE_SPI:
		pmu_clock_en(PERIPH_SPS);
		break;
	case MODULE_USB:
		pmu_clock_en(PERIPH_USB);
		break;
	case MODULE_PMU:
		pmu_clock_en(PERIPH_PMU);
		break;
	default:
		break;
	}
	return;
}
