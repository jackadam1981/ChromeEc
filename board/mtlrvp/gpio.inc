/* -*- mode:c -*-
 *
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

GPIO_INT(WP_L, PIN(D, 5), GPIO_INT_BOTH, fake_interrupt)

GPIO(EC_SPI_OE_MECC_R, PIN(A, 7), GPIO_OUT_HIGH) /* 1.8V */

UNIMPLEMENTED(ENTERING_RW)

/* Alternate pins for UART */
ALTERNATE(PIN_MASK(6, BIT(4) | BIT(5)), GPIO_ALT_FUNC_DEFAULT, MODULE_UART,
	  GPIO_FLAG_NONE)
