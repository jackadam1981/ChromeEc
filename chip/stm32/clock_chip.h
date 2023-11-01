/* Copyright 2016 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CHIP_STM32_CLOCK_CHIP_H
#define __CROS_EC_CHIP_STM32_CLOCK_CHIP_H

/* Get clock frequency of APB peripherals (PCLK), except timers. */
int clock_get_apb_freq(void);

/* Get timer clock frequency. */
int clock_get_timer_freq(void);

#endif /* __CROS_EC_CHIP_STM32_CLOCK_CHIP_H */
