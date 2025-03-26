/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_pd.h"

__weak uint8_t board_get_usb_pd_port_count(void)
{
	return 2;
}
__weak enum ec_error_list charger_set_frequency(int freq_khz)
{
	return 0;
}

int battery_is_present(void)
{
	return 1;
}

#ifdef CONFIG_LINK_TEST_SUITE_CHARGE
int extpower_is_present(void)
{
	return 0;
}
#endif
