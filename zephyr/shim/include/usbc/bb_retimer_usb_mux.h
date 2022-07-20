/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_BB_RETIMER_USB_MUX_H
#define __ZEPHYR_SHIM_BB_RETIMER_USB_MUX_H

#include "driver/retimer/bb_retimer_public.h"

#define BB_RETIMER_USB_MUX_COMPAT intel_jhl8040r

#define USB_MUX_CONFIG_BB_RETIMER(mux_id)                      \
	{                                                      \
		USB_MUX_COMMON_FIELDS(mux_id),                 \
			.driver = &bb_usb_retimer,             \
			.hpd_update = bb_retimer_hpd_update,   \
			.i2c_port = I2C_PORT_BY_DEV(mux_id),   \
			.i2c_addr_flags = DT_REG_ADDR(mux_id), \
	}

#define BB_RETIMER_CONTROLS_CONFIG(mux_id)                            \
	{                                                             \
		.retimer_rst_gpio =                                   \
			GPIO_SIGNAL(DT_PHANDLE(mux_id, reset_pin)),   \
		.usb_ls_en_gpio = COND_CODE_1(                        \
			DT_NODE_HAS_PROP(mux_id, ls_en_pin),          \
			(GPIO_SIGNAL(DT_PHANDLE(mux_id, ls_en_pin))), \
			(GPIO_UNIMPLEMENTED)),                        \
	}

/**
 * @brief Set entry in bb_controls array
 *
 * @param chain_id Chain DTS node ID
 * @param mux_id BB retimer node ID
 */
#define USB_MUX_BB_RETIMER_CONTROL_ARRAY(chain_id, mux_id) \
	[USBC_PORT(chain_id)] = BB_RETIMER_CONTROLS_CONFIG(mux_id),

/**
 * @brief Call @p op if @p mux_id has @p compat
 *
 * @param chain_id Chain DTS node ID
 * @param mux_id MUX DTS node ID
 * @param op Operation to perform on USB muxes
 * @param compat Compatible property
 */
#define USB_MUX_ONLY_COMPAT(chain_id, mux_id, op, compat) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(mux_id, compat),   \
		    (op(chain_id, mux_id)), ())

/**
 * @brief Call USB_MUX_ONLY_COMPAT to perform @p op only if @p idx mux on
 *        @p chain_id is BB retimer
 *
 * @param chain_id Chain DTS node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_ONLY_BB(chain_id, unused2, idx, op)                           \
	USB_MUX_ONLY_COMPAT(chain_id, USB_MUX_GET_CHAIN_N(chain_id, idx), op, \
			    BB_RETIMER_USB_MUX_COMPAT)

/**
 * @brief Call @p op for every BB retimer mux in @p chain_id
 *
 * @param chain_id Chain DTS node ID
 * @param op Operation to perform on BB retimer
 */
#define USB_MUX_FOREACH_BB_MUX(chain_id, op) \
	DT_FOREACH_PROP_ELEM_VARGS(chain_id, usb_muxes, USB_MUX_ONLY_BB, op)

/**
 * @brief Call USB_MUX_BB_RETIMER_CONTROL_ARRAY for every chain in DTS
 *        containing BB retimer
 */
#define USB_MUX_BB_RETIMERS_CONTROLS_ARRAY                  \
	USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOREACH_BB_MUX, \
				    USB_MUX_BB_RETIMER_CONTROL_ARRAY)

#endif /* __ZEPHYR_SHIM_BB_RETIMER_USB_MUX_H */
