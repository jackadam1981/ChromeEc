/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* JTAG interface for Chrome EC */

#ifndef __CROS_EC_JTAG_H
#define __CROS_EC_JTAG_H

#include "common.h"

/**
 * Pre-initialize the JTAG module.
 */
void jtag_pre_init(void);

/**
 * Interrupt handler for JTAG clock.
 *
 * @param signal	Signal which triggered the interrupt.
 */
void jtag_interrupt(enum gpio_signal signal);

#endif  /* __CROS_EC_JTAG_H */
