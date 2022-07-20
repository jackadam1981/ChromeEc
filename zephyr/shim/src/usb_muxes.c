/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/sys/util_macro.h>
#include "usb_mux.h"
#include "usbc/usb_muxes.h"

/**
 * This prevents creating struct usb_mux usb_muxes[] for platforms that didn't
 * migrate USB mux configuration to DTS yet.
 */
#if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_usb_mux_chain)

/**
 * Define usb_mux_chain structures e.g.
 *
 * MAYBE_CONST struct usb_mux_chain
 * USB_MUX_chain_port_<port_id>_mux_<position_id> = {
 *         .mux = &USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_virtual_mux_0,
 *         .next = &USB_MUX_chain_port_0_mux_1,
 * }
 */
USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOREACH_NO_ROOT_MUX,
			    USB_MUX_CHAIN_STRUCT_DEFINE_OP)

/**
 * Forward declarations for board_init and board_set callbacks. e.g.
 * int c0_mux0_board_init(const struct usb_mux *);
 * int c1_mux0_board_set(const struct usb_mux *, mux_state_t);
 */
USB_MUX_FOREACH_MUX(USB_MUX_CB_BOARD_INIT_DECLARE_IF_EXISTS)
USB_MUX_FOREACH_MUX(USB_MUX_CB_BOARD_SET_DECLARE_IF_EXISTS)

/**
 * Define root of each USB muxes chain e.g.
 * [0] = {
 *         .mux = &USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_virtual_mux_0,
 *         .next = &USB_MUX_chain_port_0_mux_1,
 * },
 * [1] = { ... },
 */
MAYBE_CONST struct usb_mux_chain usb_muxes[] = { USB_MUX_FOREACH_CHAIN(
	USB_MUX_DEFINE_ROOT_MUX) };
BUILD_ASSERT(ARRAY_SIZE(usb_muxes) == CONFIG_USB_PD_PORT_MAX_COUNT);

/**
 * Define all USB muxes e.g.
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_mux_0 = {
 *         .board_init = NULL,
 *         .board_set = NULL,
 *         .flags = 0,
 *         .driver = &virtual_usb_mux_driver,
 *         .hpd_update = &virtual_hpd_update,
 * };
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_<node_id> = { ... };
 */
USB_MUX_FOREACH_MUX(USB_MUX_DEFINE)

/* Create bb_controls only if BB or HB retimer driver is enabled */
#if defined(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB) || \
	defined(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_HB)
/**
 * @brief bb_controls array should be constant only if configuration cannot
 *        change in runtime
 */
#define BB_CONTROLS_CONST                                                    \
	COND_CODE_1(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG, \
		    (), (const))

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
#endif /* CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB/HB */

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_usb_mux_chain) */
