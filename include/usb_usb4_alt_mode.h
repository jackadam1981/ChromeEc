/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB4 mode support
 * Refer USB Type-C Cable and Connector Specification Release 2.0 Section 5 and
 * USB Power Delivery Specification Revision 3.0, Version 2.0 Section 6.4.8
 */

#ifndef __CROS_EC_USB_USB4_ALT_MODE_H
#define __CROS_EC_USB_USB4_ALT_MODE_H

#include <stdint.h>

#include "tcpm.h"

/*
 * Initialize USB4 state for the specified port.
 *
 * @param port USB-C port number
 */
void usb4_init(int port);

/*
 * Resets usb4 state and mux state on receiving a nak/time out
 *
 * @param port USB-C port number
 */
void usb4_entry_failed(int port);

/*
 * Returns True if port, port partner and cable supports USB4 mode
 *
 * @param port    USB-C port number
 * i@return        True if USB4 mode is supported,
 *                False otherwise
 */
bool pd_is_usb4_mode_capable(int port);

/*
 * Handles ACKed USB4 response
 *
 * @param port    USB-C port number
 * @param type    Transmit type (SOP, SOP') for request
 */
void enter_usb_acked(int port, enum tcpm_transmit_type type);

/*
 * Handles Naked USB4 response
 *
 * @param port    USB-C port number
 * @param type    Transmit type (SOP, SOP') for request
 */
void enter_usb_naked(int port, enum tcpm_transmit_type type);

/*
 * Construct the next USB4 EUD that should be sent.
 *
 * @param port    USB-C port number
 */
uint32_t enter_usb_setup_next_msg(int port);

#endif
