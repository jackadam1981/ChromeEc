/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * High-res hardware timer
 *
 * SCP hardware 32bit count down timer can be configured to source clock from
 * 32KHz, 26MHz, BCLK or PCLK. This implementation selects BCLK (ULPOSC1/8) as a
 * source, countdown mode and converts to micro second value matching common
 * timer.
 */

#include "common.h"
#include "hwtimer.h"
#include "registers.h"

int __hw_clock_source_init(uint32_t start_t)
{
	SCP_CORE0_GENERAL_CTRL |= CPU_TIMER_INT_EN;
	return 0;
}

uint32_t __hw_clock_source_read(void)
{
	return 0;
}

uint32_t __hw_clock_event_get(void)
{
	return 0;
}

