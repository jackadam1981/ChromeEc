/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __ZEPHYR_SHIM_TCPCI_USB_MUX_H
#define __ZEPHYR_SHIM_TCPCI_USB_MUX_H

#include "tcpm/ps8xxx_public.h"
#include "tcpm/tcpci.h"

#define TCPCI_TCPM_USB_MUX_COMPAT	cros_ec_tcpci_usb_mux

#define USB_MUX_CONFIG_TCPCI_TCPM(mux_id, port_id, idx)			\
	{								\
		USB_MUX_COMMON_FIELDS(mux_id, port_id, idx),		\
		.driver = &tcpci_tcpm_usb_mux_driver,			\
		.hpd_update = USB_MUX_CALLBACK_OR_NULL(mux_id,		\
						       hpd_update),	\
	}

#endif /* __ZEPHYR_SHIM_TCPCI_USB_MUX_H */
