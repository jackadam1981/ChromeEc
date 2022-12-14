/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_SHIM_INCLUDE_GPIO_TCPC_H_
#define ZEPHYR_SHIM_INCLUDE_GPIO_TCPC_H_

/*
 * Enable the interrupt.
 *
 * All tcpc interrupts are automatically retrieved and enabled from devicetree
 */
int tcpc_enable_interrupt(void);

#endif
