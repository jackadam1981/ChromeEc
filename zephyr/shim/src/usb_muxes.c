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
 * @brief Check if port of mux at index @p idx in chain @p chain_id is the same
 *        as port of the chain
 *
 * @param idx Index of mux
 * @param chain_id Chain DTS node ID
 */
#define USB_MUX_CMP_PORTS(idx, chain_id) \
	USBC_PORT(chain_id) == USBC_PORT(USB_MUX_GET_CHAIN_N(idx, chain_id))

/**
 * @brief Check if all muxes in chain @p chain_id have the same USB-C port
 *
 * @param chain_id Chain DTS node ID
 */
#define USB_MUX_CHECK_ALL_PORTS_ARE_SAME(chain_id)               \
	BUILD_ASSERT(LISTIFY(DT_PROP_LEN(chain_id, usb_muxes),   \
			     USB_MUX_CMP_PORTS, (&&), chain_id), \
		     "USB muxes in " #chain_id                   \
		     " chain has mixed USB-C ports");

/**
 * Check at build time that within one chain there are only muxes for the same
 * port.
 */
USB_MUX_FOREACH_CHAIN(USB_MUX_CHECK_ALL_PORTS_ARE_SAME)

/**
 * Declare all usb_mux_chain structures e.g.
 * MAYBE_CONST struct usb_mux_chain
 * USB_MUX_chain_port_<port_id>_mux_<position_id>;
 */
USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOREACH_NO_FIRST_MUX,
			    USB_MUX_CHAIN_STRUCT_DECLARE_OP)

/**
 * Define usb_mux_chain structures for main chain e.g.
 *
 * MAYBE_CONST struct usb_mux_chain
 * USB_MUX_chain_port_<port_id>_mux_<position_id> = {
 *         .mux = &USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_virtual_mux_0,
 *         .next = &USB_MUX_chain_port_0_mux_1,
 * }
 */
USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOR_MAIN_CHAIN,
			    USB_MUX_FOREACH_NO_FIRST_MUX,
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
MAYBE_CONST struct usb_mux_chain usb_muxes[] = { USB_MUX_FOREACH_CHAIN_VARGS(
	USB_MUX_FOR_MAIN_CHAIN, USB_MUX_DEFINE_FIRST_MUX) };
BUILD_ASSERT(ARRAY_SIZE(usb_muxes) == CONFIG_USB_PD_PORT_MAX_COUNT);

/**
 * Define all USB muxes e.g.
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_DT_N_S_usbc_S_port0_0_S_mux_0 = {
 *         .usb_port = 0,
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

BB_RETIMER_CHECK_SAME_CONTROLS(BB_RETIMER_INSTANCES_LIST)

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
