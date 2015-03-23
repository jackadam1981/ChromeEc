/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* oak USB-C port VBUS interrupt */

#ifndef __USBC_PORT_H
#define __USBC_PORT_H

void vbus_wake_interrupt(enum gpio_signal signal);

#endif /* __USBC_PORT_H */
