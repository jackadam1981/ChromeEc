/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_POWER_BUTTON_H
#define __EC_BOARD_CR50_POWER_BUTTON_H


/**
 * Enable power button release interrupt.
 *
 * @param none
 * @return none
 */
void power_button_release_interrupt_enable(void);

/**
 * disable power button release interrupt.
 *
 * @param none
 * @return none
 */
void power_button_release_interrupt_disable(void);

#endif  /* ! __EC_BOARD_CR50_POWER_BUTTON_H */
