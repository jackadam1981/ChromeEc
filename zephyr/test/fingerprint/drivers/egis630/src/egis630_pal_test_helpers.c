/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "egis630_pal_test_helpers.h"

uint64_t z_impl_egis630_plat_get_time(void)
{
	return plat_get_time();
}

void z_impl_egis630_plat_wait_time(uint32_t msec)
{
	return plat_wait_time(msec);
}

void z_impl_egis630_plat_sleep_time(uint32_t timeInMs)
{
	return plat_sleep_time(timeInMs);
}

uint32_t z_impl_egis630_plat_get_diff_time(uint64_t begin)
{
	return plat_get_diff_time(begin);
}

void *z_impl_egis630_sys_alloc(size_t count, size_t size)
{
	return sys_alloc(count, size);
}

int z_impl_egis630_periphery_spi_write_read(uint8_t *write, uint32_t size,
					    uint8_t *read, uint32_t rx_len)
{
	return periphery_spi_write_read(write, size, read, rx_len);
}
