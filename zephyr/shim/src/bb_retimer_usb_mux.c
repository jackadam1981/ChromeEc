/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/sys/util_macro.h>
#include "usb_mux.h"
#include "usbc/usb_muxes.h"

/**
 * This prevents creating struct usb_mux bb_controls[] for platforms that didn't
 * migrate USB mux configuration to DTS yet.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_usb_mux_chain)

/**
 * @brief bb_controls array should be constant only if configuration cannot
 *        change in runtime
 */
#define BB_CONTROLS_CONST                                                    \
	COND_CODE_1(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG, \
		    (), (const))

/** Define BB retimer GPIO signals for each chain */
BB_RETIMER_DEFINE_GPIO_FOREACH_CHAIN(BB_RETIMER_RESET_GPIO)
BB_RETIMER_DEFINE_GPIO_FOREACH_CHAIN(BB_RETIMER_LS_EN_GPIO)

/**
 * Check if for all chains, all BB retimers present on a chain has the same GPIO
 * signal property. This is required, because bb_controls[] is defined per
 * USBC port/chain not per BB retimer instance.
 */
BB_RETIMER_CHECK_GPIO_FOREACH_CHAIN(BB_RETIMER_RESET_GPIO)
BB_RETIMER_CHECK_GPIO_FOREACH_CHAIN(BB_RETIMER_LS_EN_GPIO)

/**
 * If CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG is disabled, check
 * that all alternative chains associated with the same USBC port, have the same
 * GPIO configuration
 */
BB_RETIMER_CHECK_ALTERNATIVE_CHAIN_GPIO(BB_RETIMER_RESET_GPIO)
BB_RETIMER_CHECK_ALTERNATIVE_CHAIN_GPIO(BB_RETIMER_RESET_GPIO)

/**
 * Define bb_controls for BB retimers in USB muxes chain e.g.
 * [0] = {
 *         .retimer_rst_gpio = IOEX_USB_C0_BB_RETIMER_RST,
 *         .usb_ls_en_gpio = IOEX_USB_C0_BB_RETIMER_LS_EN,
 * },
 * [1] = { ... },
 */
BB_CONTROLS_CONST struct bb_usb_control bb_controls[] = {
	USB_MUX_BB_RETIMERS_CONTROLS_ARRAY
};

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_usb_mux_chain) */
