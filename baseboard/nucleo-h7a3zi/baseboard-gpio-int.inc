/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * These are to support the Nucleo-H7A3ZI-Q.
 */

/* Interrupts */
GPIO_INT(SPI1_NSS, PIN(A, 4),  GPIO_INPUT,    spi_event)
GPIO_INT(BTN1,     PIN(C, 13), GPIO_INT_BOTH, button_event)
