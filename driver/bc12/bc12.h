/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB BC 1.2 Charger Detector driver definitions */

#include "gpio.h"

#define BC12_FLAGS_ENABLE_ACTIVE_LOW		(1 << 0)
#define BC12_FLAGS_CHG_DET_ACTIVE_LOW		(1 << 1)
#define BC12_FLAGS_PP5000_EN_CTRL		(1 << 2)

struct bc12_config_t {
	/*
	 * Enable signal to BC 1.2. Can be active high or low depending on
	 * BC12_FLAGS_ENABLE_ACTIVE_LOW flag bit.
	 */
	enum gpio_signal chip_enable_pin;
	/*
	 * Charger detect signal from BC 1.2 chip. Can be active high or low
	 * depending on BC12_FLAGS_CHG_DET_ACTIVE_LOW flag bit.
	 */
	enum gpio_signal chg_det_pin;
	/* Configuration flags with prefix BC12_FLAGS. */
	int flags;
};

/*
 * Array that contains boards-specific configuration for BC 1.2 charging chips.
 */
extern const struct bc12_config_t bc12_config[CONFIG_USB_PD_PORT_COUNT];
