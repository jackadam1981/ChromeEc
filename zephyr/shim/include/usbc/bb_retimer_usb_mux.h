/* Copyright 2022 The ChromiumOS Authors
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

/**
 * @brief Get reset gpio for @p mux_id retimer
 *
 * @param mux_id BB retimer DTS node
 */
#define BB_RETIMER_RESET_GPIO(mux_id) GPIO_SIGNAL(DT_PHANDLE(mux_id, reset_pin))

/**
 * @brief Get LS_EN gpio for @p mux_id retimer
 *
 * @param mux_id BB retimer DTS node
 */
#define BB_RETIMER_LS_EN_GPIO(mux_id)                             \
	COND_CODE_1(DT_NODE_HAS_PROP(mux_id, ls_en_pin),          \
		    (GPIO_SIGNAL(DT_PHANDLE(mux_id, ls_en_pin))), \
		    (GPIO_UNIMPLEMENTED))

/**
 * @brief Unique identifier for BB retimer @p mux_gpio signal associated with
 *        @p chain_id
 *
 * @param chain_id Chain DTS node ID
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio) \
	UTIL_CAT(mux_gpio, chain_id)

/**
 * @brief Initialize struct bb_usb_control for @p chain_id using @p op_val
 *        to get GPIO signal value
 *
 * @param chain_id Chain DTS node ID
 * @param op_val Macro to get GPIO signal value
 */
#define BB_RETIMER_CONTROLS_CONFIG(chain_id, op_val)                         \
	{                                                                    \
		.retimer_rst_gpio = op_val(chain_id, BB_RETIMER_RESET_GPIO), \
		.usb_ls_en_gpio = op_val(chain_id, BB_RETIMER_LS_EN_GPIO),   \
	}

/**
 * @brief Set entry in bb_controls array
 *
 * @param chain_id Chain DTS node ID
 */
#define USB_MUX_BB_RETIMER_CONTROL_ARRAY(chain_id)          \
	[USBC_PORT(chain_id)] = BB_RETIMER_CONTROLS_CONFIG( \
		chain_id, BB_RETIMER_CHAIN_GPIO_NAME),

/**
 * @brief Use @p mux_gpio to get BB retimer GPIO signal for @p mux_id and
 *        insert @p prefix before the signal
 *
 * @param chain_id Chain DTS node ID
 * @param mux_id MUX DTS node ID
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 * @param prefix Prefix to insert before the GPIO signal
 */
#define BB_RETIMER_GET_GPIO_WITH_PREFIX(chain_id, mux_id, mux_gpio, prefix) \
	prefix mux_gpio(mux_id)

/**
 * @brief Call @p op if @p mux_id has @p compat
 *
 * @param chain_id Chain DTS node ID
 * @param mux_id MUX DTS node ID
 * @param op Operation to perform on USB muxes
 * @param compat Compatible property
 */
#define USB_MUX_ONLY_COMPAT(chain_id, mux_id, op, compat, ...) \
	COND_CODE_1(DT_NODE_HAS_COMPAT(mux_id, compat),        \
		    (op(chain_id, mux_id, __VA_ARGS__)), ())

/**
 * @brief Call USB_MUX_ONLY_COMPAT to perform @p op only if @p idx mux on
 *        @p chain_id is BB retimer
 *
 * @param chain_id Chain DTS node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 * @param ... Arguments to pass to the @p op operation
 */
#define USB_MUX_ONLY_BB(chain_id, unused2, idx, op, ...)                      \
	USB_MUX_ONLY_COMPAT(chain_id, USB_MUX_GET_CHAIN_N(chain_id, idx), op, \
			    BB_RETIMER_USB_MUX_COMPAT, __VA_ARGS__)

/**
 * @brief Call @p op for every BB retimer mux in @p chain_id
 *
 * @param chain_id Chain DTS node ID
 * @param op Operation to perform on BB retimer
 * @param ... Additional arguments for @p op
 */
#define USB_MUX_FOREACH_BB_MUX(chain_id, op, ...)                            \
	DT_FOREACH_PROP_ELEM_VARGS(chain_id, usb_muxes, USB_MUX_ONLY_BB, op, \
				   __VA_ARGS__)

/**
 * @brief Get BB retimer @p mux_gpio signal value for @p chain_id
 *
 * Constructed value has format:
 * 0 | mux_gpio(bb_retimer_1_mux_id) | mux_gpio(bb_retimer_2_mux_id)
 * ... | mux_gpio(bb_retimer_n_mux_id)
 *
 * @param chain_id Chain DTS node ID
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHAIN_GPIO_VAL(chain_id, mux_gpio)                        \
	(0 USB_MUX_FOREACH_BB_MUX(chain_id, BB_RETIMER_GET_GPIO_WITH_PREFIX, \
				  mux_gpio, |))

/**
 * @brief Define BB retimer @p mux_gpio signal for @p chain_id
 *
 * @param chain_id Chain DTS node ID
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_DEFINE_GPIO(chain_id, mux_gpio)       \
	BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio) = \
		BB_RETIMER_CHAIN_GPIO_VAL(chain_id, mux_gpio),

/**
 * @brief Check if all BB retimers on the @p chain_id has the same configuration
 *        of @p mux_gpio
 *
 * Constructed static assert has format:
 * BUILD_ASSERT(1 && (int)BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio) ==
 *                   (int)mux_gpio(bb_retimer_1_mux_id)
 *                && (int)BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio) ==
 *                   (int)mux_gpio(bb_retimer_2_mux_id)
 *                   ...
 *                && (int)BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio) ==
 *                   (int)mux_gpio(bb_retimer_n_mux_id),
 *              "BB muxes in chain chain_id has different mux_gpio")
 *
 * @param chain_id Chain DTS node ID
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHECK_GPIO(chain_id, mux_gpio)                              \
	BUILD_ASSERT(                                                          \
		1 USB_MUX_FOREACH_BB_MUX(                                      \
			chain_id, BB_RETIMER_GET_GPIO_WITH_PREFIX, mux_gpio,   \
			&&(int)BB_RETIMER_CHAIN_GPIO_NAME(chain_id,            \
							  mux_gpio) == (int)), \
		"BB muxes in chain " #chain_id " has different " #mux_gpio);

/**
 * @brief Define BB retimer enum gpio signal for each chain
 *
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_DEFINE_GPIO_FOREACH_CHAIN(mux_gpio) \
	enum { USB_MUX_FOREACH_CHAIN_VARGS(BB_RETIMER_DEFINE_GPIO, mux_gpio) };

/**
 * @brief Check if for each chain that BB retimers are on the chain have
 *        the same configuration of @p mux_gpio. This limitation is present,
 *        because bb_control[] array is defined per chain not per mux.
 *
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHECK_GPIO_FOREACH_CHAIN(mux_gpio) \
	USB_MUX_FOREACH_CHAIN_VARGS(BB_RETIMER_CHECK_GPIO, mux_gpio)

/**
 * @brief Check if USBC port child @p chain_id is chain and compere GPIO signal
 *        @p mux_gpio between @p chain_id and @p main_chain_id
 *
 * @param main_chain_id Main chain DTS node ID. This chain was used to generate
 *                      bb_controls[]
 * @param chain_id Chain DTS node ID to compare against main chain
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHECK_GPIO_WITH_CHAIN(chain_id, main_chain_id, mux_gpio)   \
	COND_CODE_1(                                                          \
		DT_NODE_HAS_COMPAT(chain_id, cros_ec_usb_mux_chain),          \
		(BUILD_ASSERT(                                                \
			 (BB_RETIMER_CHAIN_GPIO_NAME(main_chain_id,           \
						     mux_gpio) ==             \
			  BB_RETIMER_CHAIN_GPIO_NAME(chain_id, mux_gpio)) ||  \
				 (BB_RETIMER_CHAIN_GPIO_NAME(chain_id,        \
							     mux_gpio) == 0), \
			 #main_chain_id                                       \
			 " and " #chain_id " have different " #mux_gpio       \
			 " configuration. BB retimers in alternative chain "  \
			 "need to have the same GPIO configuration as BB "    \
			 "retimers on main chain when BB runtime config is "  \
			 "disabled");),                                       \
		())

/**
 * @brief Call BB_RETIMER_CHECK_GPIO_WITH_CHAIN for each alternative chain that
 *        is on the same USBC port as @p main_chain_id
 *
 * @param main_chain_id Main chain DTS node ID. This chain was used to generate
 *                      bb_controls[]
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHECK_GPIO_WITH_OTHER_CHAINS(main_chain_id, mux_gpio)     \
	DT_FOREACH_CHILD_STATUS_OKAY_VARGS(DT_PARENT(main_chain_id),         \
					   BB_RETIMER_CHECK_GPIO_WITH_CHAIN, \
					   main_chain_id, mux_gpio)

/**
 * @brief If bb_controls[] is not runtime configurable, check if GPIO signal
 *        @p mux_gpio matches for each alternative mux chain associated with
 *        the same USBC port
 *
 * @param mux_gpio Macro to get gpio signal from BB retimer DTS node
 */
#define BB_RETIMER_CHECK_ALTERNATIVE_CHAIN_GPIO(mux_gpio)                    \
	COND_CODE_1(                                                         \
		CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG, (), \
		(USB_MUX_FOREACH_CHAIN_VARGS(                                \
			USB_MUX_FOR_MAIN_CHAIN,                              \
			BB_RETIMER_CHECK_GPIO_WITH_OTHER_CHAINS, mux_gpio)))

/**
 * @brief Call USB_MUX_BB_RETIMER_CONTROL_ARRAY for every main chain in DTS
 */
#define USB_MUX_BB_RETIMERS_CONTROLS_ARRAY                  \
	USB_MUX_FOREACH_CHAIN_VARGS(USB_MUX_FOR_MAIN_CHAIN, \
				    USB_MUX_BB_RETIMER_CONTROL_ARRAY)

/**
 * When using alternative chain, update bb_controls[] if BB retimers are
 * present and bb_controls[] is not constant.
 */
#if defined(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB_RUNTIME_CONFIG) && \
	(defined(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB) ||           \
	 defined(CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_HB))
/**
 * @brief Configure alternative bb_controls[] value for @p chain_id
 *
 * @param chain_id Chain DTS node ID
 */
#define BB_RETIMER_ALTERNATIVE_CONFIG(chain_id)                    \
	bb_controls[USBC_PORT(chain_id)] =                         \
		(struct bb_usb_control)BB_RETIMER_CONTROLS_CONFIG( \
			chain_id, BB_RETIMER_CHAIN_GPIO_VAL);
#else
#define BB_RETIMER_ALTERNATIVE_CONFIG(chain_id) /* nothing to do */
#endif /* CONFIG_PLATFORM_EC_USBC_RETIMER_INTEL_BB/HB */

#endif /* __ZEPHYR_SHIM_BB_RETIMER_USB_MUX_H */
