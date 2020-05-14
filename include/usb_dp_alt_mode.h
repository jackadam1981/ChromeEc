/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DisplayPort alternate mode support */

#ifndef __CROS_EC_USB_DP_ALT_MODE_H
#define __CROS_EC_USB_DP_ALT_MODE_H

#include "stdbool.h"

/*
 * Initialize DP state for the specified port.
 *
 * @param port USB-C port number
 */
void dp_init(int port);

/*
 * Handle recevied DisplayPort VDM ACKs.
 *
 * @param port USB-C port number
 * @param cmd  VDM command from ACK
 */
void dp_vdm_cmd_acked(int port, int cmd);

/*
 * Reset the DisplayPort VDM state for the specified port, as when exiting
 * DisplayPort mode.
 *
 * @param port USB-C port number
 */
void dp_reset_next_command(int port);

/*
 * Construct the next DisplayPort VDM that should be sent.
 *
 * @param port    USB-C port number
 * @param vdm     The VDM payload to be sent; output; must point to at least
 *                VDO_MAX_SIZE elements
 * @param vdo_cnt The number of VDOs in vdm; output
 * @return        True if a VDM was constructed; false if no VDM could be
 *                constructed
 */
bool dp_setup_next_vdm(int port, uint32_t *vdm, uint32_t *vdo_cnt);

#endif  /* __CROS_EC_USB_DP_ALT_MODE_H */
