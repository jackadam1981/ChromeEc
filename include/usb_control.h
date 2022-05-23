/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB control logic header */

#ifndef __CROS_EC_USB_CONTROL_H
#define __CROS_EC_USB_CONTROL_H

#include "usbc_ocp.h"
#include "usbc_ppc.h"

/**
 * Sets the polarity of the port
 *
 * @param port USB-C port number
 * @param polarity Polarity of CC lines
 */
void pd_set_polarity(int port, enum tcpc_cc_polarity polarity);

/**
 * Turn on/off the SBU FETs.
 *
 * @param port USB-C port number
 * @param enable true:enable, false:disable
 */
void pd_set_sbu(int port, bool enable);

/**
 * Set the Vbus source path current limit
 *
 * @param port USB-C port number
 * @param rp Pull-up values to be aplied as a SRC to advertise current limits
 */
void pd_set_vbus_source_current_limit(int port, enum tcpc_rp_value rp);

/**
 * Set the role the partner device for  PPC and OCP module
 *
 * @param port USB-C port number
 * @param role role of connected device
 * @param ocp_command OCP command
 */
void pd_set_partner_role(int port, enum ppc_device_role role,
			enum ocp_action ocp_command);

/**
 * Turn on/off the VCONN FET
 *
 * @param port USB-C port number
 * @param enable true:enable, false:disable
 */
void pd_set_vconn(int port, bool enable);

/**
 * Initialize the power delivery subsystem chips
 *
 * @param port USB-C port number
 */
void pd_init(int port);

#endif /* __CROS_EC_USB_CONTROL_H */
