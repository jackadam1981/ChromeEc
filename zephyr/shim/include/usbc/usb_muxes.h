/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_CHROME_USBC_USB_MUXES_H
#define ZEPHYR_CHROME_USBC_USB_MUXES_H

#include <devicetree.h>
#include <sys/util_macro.h>
#include "usb_mux.h"
#include "usbc/tcpci_usb_mux.h"
#include "usbc/it5205_usb_mux.h"
#include "usbc/tusb1064_usb_mux.h"
#include "usbc/virtual_usb_mux.h"

/**
 * @brief List of USB mux drivers compatibles and their configurations. Each
 *        elment of list has to have (compatible, config) format.
 */
#define USB_MUX_DRIVERS						\
	(TCPCI_TCPM_USB_MUX_COMPAT, USB_MUX_CONFIG_TCPCI_TCPM),	\
	(IT5205_USB_MUX_COMPAT, USB_MUX_CONFIG_IT5205),		\
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
 * @param id Device node ID
 */
#define USB_MUX_PORT(id)		DT_REG_ADDR(DT_PARENT(id))

/**
 * @brief Name of USB mux structure. Note, that root of chain is not referred by
 *        this name, but usb_muxes[USB_MUX_PORT(id)].
 *
 * @param id Device node ID
 */
#define USB_MUX_STRUCT_NAME(id)		DT_CAT(USB_MUX_NODE_, id)

/**
 * @brief Declaration of USB mux structure
 *
 * @param id Device node ID
 */
#define USB_MUX_STRUCT_DECLARE(id)	struct usb_mux USB_MUX_STRUCT_NAME(id)

/**
 * @brief Get pointer to next USB mux in chain or NULL if it is last mux
 *
 * @param id Device node ID
 */
#define USB_MUX_NEXT(id)						\
	COND_CODE_1(DT_NODE_HAS_PROP(id, next_mux),			\
		    (&USB_MUX_STRUCT_NAME(DT_PHANDLE(id, next_mux))),	\
		    (NULL))

/**
 * @brief Generate pointer to function from @p cb_name property or NULL
 *        if property doesn't exist
 *
 * @param id Device node ID
 * @param cb_name Name of property with callback function
 */
#define USB_MUX_CALLBACK_OR_NULL(id, cb_name)				\
	COND_CODE_1(DT_NODE_HAS_PROP(id, cb_name),			\
				    (&DT_STRING_TOKEN(id, cb_name)),	\
				    (NULL))

/**
 * @brief Set struct usb_mux fields common for all USB muxes
 *
 * @param id Device node ID
 */
#define USB_MUX_COMMON_FIELDS(id)					\
	.usb_port = USB_MUX_PORT(id),					\
	.next_mux = USB_MUX_NEXT(id),					\
	.board_init = USB_MUX_CALLBACK_OR_NULL(id, board_init),		\
	.board_set = USB_MUX_CALLBACK_OR_NULL(id, board_set)

/**
 * @brief If device node @p id pass check for presence first-mux property,
 *        than @p op macro function is used with @p id and @p fn arguments
 *
 * @param id Device node ID
 * @param fn Function argument passed to @p op
 * @param need_first If 1, look if @p id node has first-mux property.
 *                   If 0, look if @p id node has not first-mux property
 * @param op Function to execute if condition is meet
 */
#define USB_MUX_CONFIG(id, fn, need_first, op)				\
	COND_CODE_##need_first(DT_PROP(id, first_mux),			\
			       (op(id, fn)), ())

/**
 * @brief Declare USB mux structure. Second argument is ignored.
 *
 * @param id Device node ID
 * @param _ Ignore
 */
#define USB_MUX_DECLARE(id, _)		USB_MUX_STRUCT_DECLARE(id);

/**
 * @brief Define USB mux structure using driver USB_MUX_CONFIG_* macro
 *
 * @param id Device node ID
 * @param fn Driver configuration function
 */
#define USB_MUX_DEFINE(id, fn)		USB_MUX_STRUCT_DECLARE(id) = fn(id);

/**
 * @brief Define entry of usb_muxes array using driver USB_MUX_CONFIG_* macro
 *
 * @param id Device node ID
 * @param fn Driver configuration function
 */
#define USB_MUX_ARRAY(id, fn)		[USB_MUX_PORT(id)] = fn(id),

/**
 * @brief Call @p op with @p driver configuration for device nodes that has
 *        compatible of @p driver. Nodes are limited by @p need_first to
 *        execute @p op only for first mux in chain or all other muxes.
 *
 * @param driver USB mux driver description in format (compatible, config)
 * @param need_first If 1, use only first muxes in chain,
 *                   If 0, use all muxes expect first ones in chain
 * @param op Opperation to perform on selected usb muxes. Needs to accept node
 *           ID and driver config as arguments.
 */
#define USB_MUX_DT_FOREACH(driver, need_first, op)			\
	DT_FOREACH_STATUS_OKAY_VARGS(USB_MUX_DRIVER_GET_COMPAT(driver),	\
				     USB_MUX_CONFIG,			\
				     USB_MUX_DRIVER_GET_CONFIG(driver),	\
				     need_first, op)

/**
 * @brief Call @p op with @p driver configuration for device nodes that has
 *        compatible of @p driver and are first mux in chain (has first-mux
 *        property)
 *
 * @param driver USB mux driver description in format (compatible, config)
 * @param op Opperation to perform on usb muxes first in chain. Needs to accept
 *           node ID and driver config as arguments.
 */
#define USB_MUX_FIRST(driver, op)	USB_MUX_DT_FOREACH(driver, 1, op)

/**
 * @brief Call @p op with @p driver configuration for device nodes that has
 *        compatible of @p driver and are not first mux in chain (has not
 *        first-mux property)
 *
 * @param driver USB mux driver description in format (compatible, config)
 * @param op Opperation to perform on usb muxes that are not first in chain.
 *           Needs to accept node ID and driver config as arguments.
 */
#define USB_MUX_OTHER(driver, op)	USB_MUX_DT_FOREACH(driver, 0, op)

/** Forward declare all usb_mux structures */
FOR_EACH_FIXED_ARG(USB_MUX_OTHER, (), USB_MUX_DECLARE, USB_MUX_DRIVERS)

#endif /* ZEPHYR_CHROME_USBC_USB_MUXES_H */
