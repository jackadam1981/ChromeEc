/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Those are example, board specific */
/* Trackpad interrupt pin, need replace tp_isr with related routine */
/*GPIO_INT(ISH_TRACKPAD_INT_L,    PIN(1), GPIO_INT_F_LOW,    tp_isr)*/

/* SPI not used */
/*GPIO(ISH_SPI_CS_L,              PIN(0),  GPIO_OUT_HIGH)*/

/* Enable if UART using interrupt, replace uart_isr with related routine  */
/*GPIO(ISH_UART0_RX,              PIN(2),  GPIO_INT_F_LOW, uart_isr)*/

/* Those are used by common code, don't change*/
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(I2C0_SCL)
UNIMPLEMENTED(I2C0_SDA)
