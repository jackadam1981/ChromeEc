/* Copyright 2022 The ChromiumOS Authors
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
 * @brief Assert if two ports are different
 *
 * @param mux1 First USB mux DTS node
 * @param mux1 Second USB mux DTS node
 */
#define USB_MUX_BUILD_ASSERT_SAME_MUXES(mux1, mux2)                \
	COND_CODE_1(IS_EMPTY(mux1), (),                            \
		    (BUILD_ASSERT(!DT_SAME_NODE(mux1, mux2), #mux2 \
				  " present in chains from different ports")))

/**
 * @brief Compare @p idx mux from @p chain_id with each elemnt of @p mux_list
 *
 * @param chain_id Chain DTS node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param mux_list List of muxes enclosed in parentheses
 */
#define USB_MUX_COMPARE_MUX_WITH_LIST(chain_id, unused2, idx, mux_list) \
	FOR_EACH_FIXED_ARG(USB_MUX_BUILD_ASSERT_SAME_MUXES, (;),        \
			   USB_MUX_GET_CHAIN_N(chain_id, idx),          \
			   __DEBRACKET mux_list)

/**
 * @brief Get @p idx mux from @p chain_id and append comma
 *
 * @param chain_id Chain DTS node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param unused4 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 */
#define USB_MUX_GET_MUX_WITH_COMMA(chain_id, unused2, idx, unused4) \
	USB_MUX_GET_CHAIN_N(chain_id, idx),

/**
 * @brief Filter only DTS nodes that are USB mux chains. On each mux on the
 *        @p chain_id perform @p op
 *
 * @param chain_id Potential chain DTS node ID
 * @param op Operation to perform on each mux. Take chain_id, usb_muxes,
 *           mux_idx, ... as arguments
 * @param ... Arguments to pass to the @p op operation
 */
#define USB_MUX_ONLY_CHAIN_CHILD(chain_id, op, ...)                      \
	COND_CODE_1(DT_NODE_HAS_COMPAT(chain_id, cros_ec_usb_mux_chain), \
		    (DT_FOREACH_PROP_ELEM_VARGS(chain_id, usb_muxes, op, \
						__VA_ARGS__)),           \
		    ())

/**
 * @brief Perform operation @p op on each mux that is present in chains on port
 *        @port_id
 *
 * @param port_id Named usbc port node ID
 * @param op Operation to perform on each mux. Take chain_id, usb_muxes,
 *           mux_idx, ... as arguments
 * @param ... Arguments to pass to the @p op operation
 */
#define USB_MUX_FOREACH_MUX_IN_PORT(port_id, op, ...)                 \
	DT_FOREACH_CHILD_VARGS(port_id, USB_MUX_ONLY_CHAIN_CHILD, op, \
			       __VA_ARGS__)

/**
 * @brief Get list of muxes present in chains on port @p inst
 *
 * @param inst Named usbc port instance number
 * @param unused2 Unused argument to satisfy LISTIFY API
 */
#define USB_MUX_GET_MUX_LIST_FROM_PORT(inst, unused2)  \
	USB_MUX_FOREACH_MUX_IN_PORT(DT_DRV_INST(inst), \
				    USB_MUX_GET_MUX_WITH_COMMA, EMPTY)

/**
 * @brief Get all muxes from all chains on named usbc port instances lower than
 *        @p inst
 *
 * Example:
 *     USB_MUX_GET_MUX_LIST_FROM_PORT(0, EMPTY)
 *     USB_MUX_GET_MUX_LIST_FROM_PORT(1, EMPTY)
 *     ...
 *     USB_MUX_GET_MUX_LIST_FROM_PORT(inst - 1, EMPTY)
 *     EMPTY
 *
 * @param inst Named usbc port instance number
 */
#define USB_MUX_GET_MUXES_FOR_PORT_LESS_THAN(inst) \
	LISTIFY(inst, USB_MUX_GET_MUX_LIST_FROM_PORT, (), EMPTY) EMPTY

/**
 * @brief Check that muxes on port @p inst don't exist on ports with instance
 *        number lower than @p inst
 *
 * @param inst Named usbc port instance number to check
 */
#define USB_MUX_CHECK_MUXES_ON_PORT_INST(inst)                    \
	USB_MUX_FOREACH_MUX_IN_PORT(                              \
		DT_DRV_INST(inst), USB_MUX_COMPARE_MUX_WITH_LIST, \
		(USB_MUX_GET_MUXES_FOR_PORT_LESS_THAN(inst)))

/*
 * Use DT_DRV_COMPAT to make DT_INST_FOREACH_STATUS_OKAY work. This method of
 * of getting all named usbc port instance is used, because
 * USB_MUX_CHECK_MUXES_ON_PORT_INST already use LISTIFY and FOR_EACH_* macros
 */
#ifdef DT_DRV_COMPAT
#undef DT_DRV_COMPAT
#endif /* DT_DRV_COMPAT */

#define DT_DRV_COMPAT named_usbc_port

/*
 * For each named usbc port, check if all muxes on the port are different from
 * muxes on ports with lower instance number.
 */
DT_INST_FOREACH_STATUS_OKAY(USB_MUX_CHECK_MUXES_ON_PORT_INST)

/**
 * Declare all usb_mux_chain structures e.g.
 * MAYBE_CONST struct usb_mux_chain
 * USB_MUX_chain_port_<port_id>_mux_<position_id>;
 */
USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOREACH_NO_ROOT_MUX,
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
USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOR_MAIN_CHAIN, USB_MUX_FOREACH_NO_ROOT_MUX,
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
	USB_MUX_FOR_MAIN_CHAIN, USB_MUX_DEFINE_ROOT_MUX) };
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

#endif /* #if DT_HAS_COMPAT_STATUS_OKAY(cros_ec_usb_mux_chain) */
