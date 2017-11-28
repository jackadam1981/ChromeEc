/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/* Common APIs for USB Type-C Power Path Controllers (PPC) */

/**
 * Is the port sourcing Vbus
 *
 * @param port: The type c port.
 * @return 1 if sourcing Vbus, 0 if not.
 */
int ppc_is_sourcing_vbus(int port);

/**
 * Board specific callback when overcurrent status changes.
 *
 * @param port: The Type-C port where the overcurrent event happened.
 * @param is_oc: 1: The port is currently overcurrented, 0: The port is not
 *               overcurrented.
 */
void board_overcurrent_event(int port, int is_oc);
