/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_ANX7452_USB_MUX_H
#define __ZEPHYR_SHIM_ANX7452_USB_MUX_H

#include "driver/retimer/anx7452_public.h"

#define ANX7452_USB_MUX_COMPAT analogix_anx7452

#define USB_MUX_CONFIG_ANX7452(mux_id)                         \
	{                                                      \
		USB_MUX_COMMON_FIELDS(mux_id),                 \
			.driver = &anx7452_usb_retimer_driver, \
			.i2c_port = I2C_PORT_BY_DEV(mux_id),   \
			.i2c_addr_flags = DT_REG_ADDR(mux_id), \
	}

#endif /* __ZEPHYR_SHIM_ANX7452_USB_MUX_H */
