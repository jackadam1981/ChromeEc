/* -*- mode:c -*-
 *
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Note: order must match main board */
GPIO(PVT_CS0,             PORT(14), 6, GPIO_ODR_HIGH, NULL)

ALTERNATE(PORT(16), 0x24,     1, MODULE_UART,          0) /* UART0 */
ALTERNATE(PORT(12), 0x14,     1, MODULE_SPI,           0)
ALTERNATE(PORT(6),  0x10,     1, MODULE_SPI,           0)
