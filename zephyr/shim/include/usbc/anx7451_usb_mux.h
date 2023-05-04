/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_ANX7451_USB_MUX_H
#define __ZEPHYR_SHIM_ANX7451_USB_MUX_H

/*#include "driver/retimer/anx7451_public.h"*/
#include "driver/usb_mux/anx7451.h"

/* put inanx7451_public.h */

struct anx7451_retimer_control {
	enum gpio_signal usb_en_gpio;
};

#define ANX7451_USB_MUX_COMPAT analogix_anx7451

#define ANX7451_USB_EN_GPIO(mux_id) GPIO_SIGNAL(DT_PHANDLE(mux_id, usb_en_pin))

/*#define ANX7451_DP_EN_GPIO(mux_id)				  \
	COND_CODE_1(DT_NODE_HAS_PROP(mux_id, dp_en_pin),          \
		    (GPIO_SIGNAL(DT_PHANDLE(mux_id, dp_en_pin))), \
		    (GPIO_UNIMPLEMENTED))*/

#define ANX7451_CONTROLS_CONFIG(mux_id)			\
	{                                                       \
		.usb_en_gpio = 0,		\
		/*.usb_en_gpio = ANX7451_USB_EN_GPIO(mux_id),*/		\
		/*.dp_enable_gpio = ANX7451_DP_EN_GPIO(mux_id),*/	\
	}

#define USB_MUX_ANX7451_CONTROL_ARRAY(mux_id)			\
	[USB_MUX_PORT(mux_id)] = ANX7451_CONTROLS_CONFIG(mux_id),

#define USB_MUX_ANX7451_CONTROLS_ARRAY				\
	DT_FOREACH_STATUS_OKAY(ANX7451_USB_MUX_COMPAT,		\
			       USB_MUX_ANX7451_CONTROL_ARRAY)

#define USB_MUX_CONFIG_ANX7451(mux_id)                         \
	{                                                      \
		USB_MUX_COMMON_FIELDS(mux_id),                 \
		.driver = &anx7451_usb_mux_driver,     \
		.i2c_port = I2C_PORT_BY_DEV(mux_id),   \
		.i2c_addr_flags = DT_REG_ADDR(mux_id), \
	}

#endif /* __ZEPHYR_SHIM_ANX7451_USB_MUX_H */
