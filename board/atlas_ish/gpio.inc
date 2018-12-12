/* -*- mode:c -*-
 *
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Those are example, board specific */
/* Trackpad interrupt pin, need replace isr with related routine */
GPIO_INT(ISH_TRACKPAD_INT_L, PIN(1), GPIO_INT_F_FALLING, touchpad_event)

/* Use UART RX pin as wake up source, replace isr with related routine  */
/*GPIO(ISH_GP2,                PIN(2), GPIO_INT_F_FALLING, uart_rx_wake_isr)*/

/* Those are used by common code, don't change*/
UNIMPLEMENTED(ENTERING_RW)
UNIMPLEMENTED(ISH_I2C0_SDA)
UNIMPLEMENTED(ISH_I2C0_SCL)
