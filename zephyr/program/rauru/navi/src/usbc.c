/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "driver/tcpm/it83xx_pd.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

#ifdef CONFIG_USB_PD_TCPM_ITE_ON_CHIP
const struct cc_para_t *board_get_cc_tuning_parameter(enum usbpd_port port)
{
	const static struct cc_para_t
		cc_parameter[CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT] = {
			{
				.rc_filter =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
				.swing_time =
					IT83XX_TX_PRE_DRIVING_TIME_6_UNIT, /* trim value */
			},
			{
				.rising_time =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
				.falling_time =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
				.swing_time =
					IT83XX_TX_PRE_DRIVING_TIME_DEFAULT,
			},
		};
	return &cc_parameter[port];
}
#endif
