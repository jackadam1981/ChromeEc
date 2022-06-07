/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_USBC_USB_MUXES_H
#define ZEPHYR_CHROME_USBC_USB_MUXES_H

#include <zephyr/devicetree.h>
#include <zephyr/sys/util_macro.h>
#include "usb_mux.h"
#include "usbc/anx7483_usb_mux.h"
#include "usbc/bb_retimer_usb_mux.h"
#include "usbc/it5205_usb_mux.h"
#include "usbc/tcpci_usb_mux.h"
#include "usbc/tusb1064_usb_mux.h"
#include "usbc/virtual_usb_mux.h"

/**
 * @brief List of USB mux drivers compatibles and their configurations. Each
 *        element of list has to have (compatible, config) format.
 */
#define USB_MUX_DRIVERS						\
	(ANX7483_USB_MUX_COMPAT, USB_MUX_CONFIG_ANX7483),	\
	(BB_RETIMER_USB_MUX_COMPAT, USB_MUX_CONFIG_BB_RETIMER),	\
	(IT5205_USB_MUX_COMPAT, USB_MUX_CONFIG_IT5205),		\
	(PS8XXX_USB_MUX_COMPAT, USB_MUX_CONFIG_TCPCI_TCPM),	\
	(TCPCI_TCPM_USB_MUX_COMPAT, USB_MUX_CONFIG_TCPCI_TCPM),	\
	(TUSB1064_USB_MUX_COMPAT, USB_MUX_CONFIG_TUSB1064),	\
	(VIRTUAL_USB_MUX_COMPAT, USB_MUX_CONFIG_VIRTUAL)

/**
 * @brief Get compatible from @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 */
#define USB_MUX_DRIVER_GET_COMPAT(driver)	GET_ARG_N(1, __DEBRACKET driver)

/**
 * @brief Get configuration from @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 */
#define USB_MUX_DRIVER_GET_CONFIG(driver)	GET_ARG_N(2, __DEBRACKET driver)

/**
 * @brief USB mux port number based on parent node in DTS
 *
 * @param port_id USBC node ID
 */
#define USB_MUX_PORT(port_id)		DT_REG_ADDR(port_id)

/**
 * @brief Name of USB mux structure if node is not EMPTY. Note, that root of
 *        chain is not referred by this name, but usb_muxes[USB_MUX_PORT(id)].
 *
 * @param mux_id USB mux node ID
 */
#define USB_MUX_STRUCT_NAME(mux_id)					\
	COND_CODE_0(IS_EMPTY(mux_id), (DT_CAT(USB_MUX_NODE_, mux_id)), (EMPTY))

/**
 * @brief Add pointer sufix to USB mux structure name
 *
 * @param struct_name USB mux structure name
 */
#define USB_MUX_STRUCT_NAME_PTR(struct_name)	DT_CAT(struct_name, _ptr)

/**
 * @brief Name of pointer to USB mux structure. Note, that this pointer is not
 *        valid for root of chain, since it has no structure defined.
 *
 * @param mux_id USB mux node ID
 */
#define USB_MUX_STRUCT_PTR(mux_id)					\
	USB_MUX_STRUCT_NAME_PTR(USB_MUX_STRUCT_NAME(mux_id))

/**
 * @brief USB muxes in chain should be constant only if configuration
 *        cannot change in runtime
 */
#define MAYBE_CONST COND_CODE_1(CONFIG_PLATFORM_EC_USB_MUX_RUNTIME_CONFIG, \
				(), (const))

/**
 * @brief Declaration of USB mux structure
 *
 * @param mux_id USB mux node ID
 */
#define USB_MUX_STRUCT_DECLARE(mux_id)					\
	MAYBE_CONST struct usb_mux USB_MUX_STRUCT_NAME(mux_id)

/**
 * @brief Declaration of pointer to USB mux structure
 *
 * @param mux_id USB mux node ID
 */
#define USB_MUX_STRUCT_PTR_DECLARE(mux_id)				\
	MAYBE_CONST struct usb_mux *USB_MUX_STRUCT_PTR(mux_id)

/**
 * @brief Get pointer by referencing @p name or NULL if @p name is EMPTY
 *
 * @param name Identifier to reference
 */
#define USB_MUX_POINTER_OR_NULL(name)					\
	COND_CODE_0(IS_EMPTY(name), (&name), (NULL))

/**
 * @brief Get node id of @p idx USB mux in chain
 *
 * @param idx Position of USB mux in chain
 * @param port_id USBC node ID
 */
#define USB_MUX_GET_CHAIN_N(idx, port_id)				\
	DT_PHANDLE_BY_IDX(port_id, usb_muxes, idx)

/**
 * @brief Get node id of next USB mux in chain or EMPTY if it is last mux
 *
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 */
#define USB_MUX_NEXT(port_id, idx)					\
	GET_ARG_N(2, GET_ARGS_LESS_N(idx,				\
			LISTIFY(DT_PROP_LEN(port_id, usb_muxes),	\
				USB_MUX_GET_CHAIN_N, (,), port_id)),	\
		  EMPTY)

/**
 * @brief Get pointer to next USB mux in chain or NULL if it is last mux
 *
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 */
#define USB_MUX_NEXT_POINTER(port_id, idx)				\
	USB_MUX_POINTER_OR_NULL(USB_MUX_STRUCT_NAME(USB_MUX_NEXT(port_id, idx)))

/**
 * @brief Generate pointer to function from @p cb_name property or NULL
 *        if property doesn't exist
 *
 * @param mux_id USB mux node ID
 * @param cb_name Name of property with callback function
 */
#define USB_MUX_CALLBACK_OR_NULL(mux_id, cb_name)			\
	USB_MUX_POINTER_OR_NULL(DT_STRING_TOKEN_OR(mux_id, cb_name, EMPTY))

/**
 * @brief Set struct usb_mux fields common for all USB muxes and alter flags
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param flags_mask Mask for bits that should be igonred in flags property
 * @param flags_val Value that should be used instead for masked bits
 */
#define USB_MUX_COMMON_FIELDS_WITH_FLAGS(mux_id, port_id, idx,		\
					 flags_mask, flags_val)		\
	.usb_port = USB_MUX_PORT(port_id),				\
	.next_mux = USB_MUX_NEXT_POINTER(port_id, idx),			\
	.board_init = USB_MUX_CALLBACK_OR_NULL(mux_id, board_init),	\
	.board_set = USB_MUX_CALLBACK_OR_NULL(mux_id, board_set),	\
	.flags = (DT_PROP(mux_id, flags) & ~(flags_mask)) | (flags_val)

/**
 * @brief Set struct usb_mux fields common for all USB muxes
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 */
#define USB_MUX_COMMON_FIELDS(mux_id, port_id, idx)			\
	USB_MUX_COMMON_FIELDS_WITH_FLAGS(mux_id, port_id, idx, 0, 0)

/**
 * @brief Expands to 1 if @p mux_id has @p compat compatible. It is required
 *        to makes sure that @p compat is expanded before DT_NODE_HAS_COMPAT
 *
 * @param mux_id USB mux node ID
 * @param compat USB mux driver compatible
 */
#define USB_MUX_IS_COMPATIBLE(mux_id, compat)	\
	DT_NODE_HAS_COMPAT(mux_id, compat)

/**
 * @brief Expands to @p driver config if @p mux_id is compatible with @p driver
 *
 * @param driver USB mux driver description in format (compatible, config)
 * @param mux_id USB mux node ID
 */
#define USB_MUX_DRIVER_CONFIG_IF_COMPAT(driver, mux_id)			\
	COND_CODE_1(USB_MUX_IS_COMPATIBLE(				\
			mux_id, USB_MUX_DRIVER_GET_COMPAT(driver)),	\
		   (USB_MUX_DRIVER_GET_CONFIG(driver)), ())

/**
 * @brief Find driver from USB_MUX_DRIVERS that is compatible with @p mux_id
 *
 * @param mux_id USB mux node ID
 */
#define USB_MUX_FIND_DRIVER_CONFIG(mux_id)				\
	FOR_EACH_FIXED_ARG(USB_MUX_DRIVER_CONFIG_IF_COMPAT, (), mux_id,	\
			   USB_MUX_DRIVERS)

/**
 * @brief Get driver configuration macro for @p mux_id and call @p op
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_CALL_OP(mux_id, port_id, idx, op)			\
	op(mux_id, port_id, idx, USB_MUX_FIND_DRIVER_CONFIG(mux_id))

/**
 * @brief Get USB mux node ID and call USB_MUX_CALL_OP
 *
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_DO(port_id, idx, op)					\
	USB_MUX_CALL_OP(USB_MUX_GET_CHAIN_N(idx, port_id), port_id, idx, op)

/**
 * @brief Get USB mux node ID and USBC node ID from @p alt_mux_id alternative
 *        USB mux node and call USB_MUX_CALL_OP
 *
 * @param alt_mux_id Alternative USB mux node ID
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_DO_ALT_MUX(alt_mux_id, op)				\
	USB_MUX_CALL_OP(DT_PHANDLE(alt_mux_id, usb_mux),		\
			DT_PHANDLE(alt_mux_id, port), 0, op)

/**
 * @brief Declare USB mux structure
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param conf Driver configuration function
 */
#define USB_MUX_DECLARE(mux_id, port_id, idx, conf)			\
	extern USB_MUX_STRUCT_DECLARE(mux_id);

/**
 * @brief Declare pointer to USB mux structure
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param conf Driver configuration function
 */
#define USB_MUX_DECLARE_PTR(mux_id, port_id, idx, conf)			\
	extern USB_MUX_STRUCT_PTR_DECLARE(mux_id);

/**
 * @brief Define and initialise pointer to USB mux structure to @p struct_name
 *
 * @param mux_id USB mux node ID
 * @param struct_name Name of structure that pointer should point to
 */
#define USB_MUX_DEFINE_PTR(mux_id, struct_name)				\
	USB_MUX_STRUCT_PTR_DECLARE(mux_id) = &struct_name

/**
 * @brief Define pointer to USB mux structure for first USB mux in chain
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param conf Driver configuration function
 */
#define USB_MUX_DEFINE_FIRST_PTR(mux_id, port_id, idx, conf)		\
	USB_MUX_DEFINE_PTR(mux_id, usb_muxes[USB_MUX_PORT(port_id)]);

/**
 * @brief Define USB mux structure using driver USB_MUX_CONFIG_* macro.
 *        Define pointer to USB mux structure and initialise it.
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param conf Driver configuration function
 */
#define USB_MUX_DEFINE(mux_id, port_id, idx, conf)			\
	USB_MUX_STRUCT_DECLARE(mux_id) = conf(mux_id, port_id, idx);	\
	USB_MUX_DEFINE_PTR(mux_id, USB_MUX_STRUCT_NAME(mux_id));

/**
 * @brief Define entry of usb_muxes array using driver USB_MUX_CONFIG_* macro
 *
 * @param mux_id USB mux node ID
 * @param port_id USBC node ID
 * @param idx Position of USB mux in chain
 * @param conf Driver configuration function
 */
#define USB_MUX_ARRAY(mux_id, port_id, idx, conf)			\
	[USB_MUX_PORT(port_id)] = conf(mux_id, port_id, idx),

/**
 * @brief Call @p op with first mux in chain
 *
 * @param port_id USBC node ID
 * @param op Operation to perform on USB mux first in chain. Needs to accept
 *           USB mux node ID, USBC port node ID, position in chain, and driver
 *           config as arguments.
 */
#define USB_MUX_FIRST(port_id, op)					\
	USB_MUX_DO(port_id, 0, op)

/**
 * @brief Call USB_MUX_DO if @p idx is not 0 (is not first mux in chain)
 *
 * @param port_id USBC node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_DO_SKIP_FIRST(port_id, unused2, idx, op)		\
	COND_CODE_1(UTIL_BOOL(idx), (USB_MUX_DO(port_id, idx, op)), ())

/**
 * @brief Call @p op with every mux in chain expect the first one
 *
 * @param port_id USBC node ID
 * @param op Operation to perform on USB muxes. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_NO_FIRST(port_id, op)					\
	DT_FOREACH_PROP_ELEM_VARGS(port_id, usb_muxes,			\
				   USB_MUX_DO_SKIP_FIRST, op)

/**
 * @brief Call USB_MUX_DO for @p idx USB mux
 *
 * @param port_id USBC node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on USB muxes
 */
#define USB_MUX_DO_ALL(port_id, unused2, idx, op)			\
	USB_MUX_DO(port_id, idx, op)

/**
 * @brief Call @p op with all muxes in chain
 *
 * @param port_id USBC node ID
 * @param op Operation to perform on USB muxes in chain. Needs to accept
 *           USB mux node ID, USBC port node ID, position in chain, and driver
 *           config as arguments.
 */
#define USB_MUX_ALL(port_id, op)					\
	DT_FOREACH_PROP_ELEM_VARGS(port_id, usb_muxes, USB_MUX_DO_ALL, op)

/**
 * @brief Call @p op if @p idx mux in chain has BB retimer compatible
 *
 * @param port_id USBC node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param op Operation to perform on BB retimer
 */
#define USB_MUX_ONLY_BB_RETIMER(port_id, unused2, idx, op)		\
	COND_CODE_1(USB_MUX_IS_COMPATIBLE(				\
			USB_MUX_GET_CHAIN_N(idx, port_id),		\
			BB_RETIMER_USB_MUX_COMPAT),			\
			(op(USB_MUX_GET_CHAIN_N(idx, port_id), port_id, \
			    idx, BB_RETIMER_CONTROLS_CONFIG)), ())

/**
 * @brief Call @p op with every BB retimer in chain
 *
 * @param port_id USBC node ID
 * @param op Operation to perform on BB retimers. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_BB_RETIMERS(port_id, op)				\
	DT_FOREACH_PROP_ELEM_VARGS(port_id, usb_muxes,			\
				   USB_MUX_ONLY_BB_RETIMER, op)

/**
 * @brief If @p port_id has usb_muxes property, call @p op with every mux in
 *        chain that passes @p filter
 *
 * @param port_id USBC node ID
 * @param filter Macro that should filter USB muxes and call @p op on them.
 *               It has @p port_id and @p op as arguments. It is called
 *               only for @p port_id that has usb_muxes property.
 * @param op Operation to perform on USB muxes. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_USBC_PORT_HAS_MUXES(port_id, filter, op)		\
	COND_CODE_1(DT_NODE_HAS_PROP(port_id, usb_muxes),		\
		    (filter(port_id, op)), ())

/**
 * @brief For every USBC port that has muxes, call @p op with every mux in chain
 *        that passes @p filter
 *
 * @param filter Macro that should filter USB muxes and call @p op on them.
 *               It has USBC port node ID and @p op as arguments. It is called
 *               only for USBC ports that have usb_muxes property.
 * @param op Operation to perform on USB muxes. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_FOREACH_USBC_PORT(filter, op)				\
	DT_FOREACH_STATUS_OKAY_VARGS(named_usbc_port,			\
				     USB_MUX_USBC_PORT_HAS_MUXES,	\
				     filter, op)

/**
 * @brief For every alternative USB mux call @p op
 *
 * @param op Operation to perform on USB mux. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_FOREACH_ALT_USB_MUX(op)					\
	DT_FOREACH_STATUS_OKAY_VARGS(cros_ec_alt_usb_mux,		\
				     USB_MUX_DO_ALT_MUX, op)

/**
 * @brief For every alternative USBC muxes chain, call @p op with every mux
 *        in chain that passes @p filter
 *
 * @param filter Macro that should filter USB muxes and call @p op on them.
 *               It has USBC port node ID and @p op as arguments.
 * @param op Operation to perform on USB muxes. Needs to accept USB mux node
 *           ID, USBC port node ID, position in chain, and driver config as
 *           arguments.
 */
#define USB_MUX_FOREACH_ALT_USB_MUX_CHAIN(filter, op)			\
	DT_FOREACH_STATUS_OKAY_VARGS(cros_ec_alt_usb_mux_chain,		\
				     filter, op)

/**
 * Forward declare all usb_mux structures e.g.
 * MAYBE_CONST struct usb_mux USB_MUX_NODE_<node_id>;
 */
USB_MUX_FOREACH_USBC_PORT(USB_MUX_NO_FIRST, USB_MUX_DECLARE)

/**
 * Forward delcare all alternative usb_mux structures
 */
USB_MUX_FOREACH_ALT_USB_MUX(USB_MUX_DECLARE)

/**
 * Forward declare all pointers to usb_mux structures e.g.
 * extern MAYBE_CONST struct usb_mux *USB_MUX_NODE_<node_id>_ptr;
 */
USB_MUX_FOREACH_USBC_PORT(USB_MUX_ALL, USB_MUX_DECLARE_PTR)
USB_MUX_FOREACH_ALT_USB_MUX_CHAIN(USB_MUX_ALL, USB_MUX_DECLARE_PTR)

/**
 * @brief Set cur_mux to point at port ID emelent of usb_muxes array. Copy
 *        the first USB mux structure from alternative mux chain if it is
 *        different from the first USB mux from orginal chain.
 *        This macro should be used only internally.
 *
 * @param alt_id Alternative USB mux chain node ID
 * @param port_id USBC node ID
 */
#define USB_MUX_ALTERNATE_CHANGE_ROOT(alt_id, port_id)			\
	cur_mux = &usb_muxes[USB_MUX_PORT(port_id)];			\
	/* Replacing is required only if first mux is different */	\
	if (!DT_SAME_NODE(USB_MUX_GET_CHAIN_N(0, alt_id),		\
			  USB_MUX_GET_CHAIN_N(0, port_id))) {		\
		memcpy(cur_mux,						\
		       USB_MUX_STRUCT_PTR(				\
				USB_MUX_GET_CHAIN_N(0, alt_id)),	\
		       sizeof(struct usb_mux));				\
	}


/**
 * @brief Set next_mux in @p idx alternative USB mux structure. In case of
 *        the first mux in chain call USB_MUX_ALTERNATE_CHANGE_FIRST.
 *        This macro should be used only internally.
 *
 * @param alt_id Alternative USB mux chain node ID
 * @param unused2 This argument is expected by DT_FOREACH_PROP_ELEM_VARGS
 * @param idx Position of USB mux in chain
 * @param port_id USBC node ID
 */
#define USB_MUX_ALTERNATE_NEXT(alt_id, unused2, idx, port_id)		\
	COND_CODE_0(UTIL_BOOL(idx),					\
		    (USB_MUX_ALTERNATE_CHANGE_ROOT(alt_id, port_id)),	\
		    (cur_mux = &USB_MUX_STRUCT_NAME(			\
				USB_MUX_GET_CHAIN_N(idx, alt_id));))	\
	cur_mux->next_mux = USB_MUX_NEXT_POINTER(alt_id, idx);

/**
 * @brief Enable alternative USB mux chain. Introduce temporary cur_mux pointer
 *
 * @param alt_id Alternative USB mux chain node ID
 */
#define USB_MUX_ENABLE_ALTERNATE_NODE(alt_id)				\
	do {								\
		struct usb_mux *cur_mux;				\
		BUILD_ASSERT(DT_NODE_EXISTS(alt_id),			\
			     "USB mux alternate node does not exist");	\
		DT_FOREACH_PROP_ELEM_VARGS(				\
			alt_id, usb_muxes, USB_MUX_ALTERNATE_NEXT,	\
			DT_PHANDLE(alt_id, alternate_for))		\
	} while (0)

/**
 * @brief Enable alternative USB mux chain
 *
 * @param nodelabel Label of alternative USB mux chain
 */
#define USB_MUX_ENABLE_ALTERNATE(nodelabel)				\
	USB_MUX_ENABLE_ALTERNATE_NODE(DT_NODELABEL(nodelabel))

#endif /* ZEPHYR_CHROME_USBC_USB_MUXES_H */
