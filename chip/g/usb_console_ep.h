/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_USB_CONSOLE_H
#define __CROS_EC_USB_CONSOLE_H

#include "queue.h"

extern struct queue const tx_q;
extern struct queue const rx_q;

/* True if the Tx/IN FIFO can take some bytes from us. */
int usb_console_tx_fifo_is_ready(void);

/* True if usb_console endpoint is reset. False otherwise. */
void usb_console_handle_output(void);

/* True if usb_console endpoint is reset. False otherwise. */
int usb_console_is_reset(void);
#endif
