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
