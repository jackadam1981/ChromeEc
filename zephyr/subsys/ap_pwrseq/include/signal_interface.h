/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_INTERFACE_H__
#define __AP_PWRSEQ_SIGNAL_INTERFACE_H__

#include <power_signals.h>

/**
 * @brief Get the value of the power signal.
 *
 * @param signal The power_signal value to get.
 * @return the current value of the power signal.
 */
int power_signal_get(enum power_signal signal);

/**
 * @brief Set the output of this power signal.
 *
 * Only some signals allow the output to be set.
 *
 * @param signal The power_signal to set.
 * @param value The output value to set it to.
 * @return 0 is successful
 * @return negative If output cannot be set.
 */
int power_signal_set(enum power_signal signal, int value);

/**
 * @brief Enable interrupts for this power signal
 *
 * For signals that allow an interrupt to be used,
 * enable the interrupt.
 *
 * @param signal The power_signal to enable interrupts for.
 * @return 0 is successful
 * @return negative If interrupt cannot be enabled.
 */
int power_signal_enable_interrupt(enum power_signal signal);

/**
 * @brief Disable interrupts for this power signal
 *
 * For signals that allow an interrupt to be used,
 * disable the interrupt.
 *
 * @param signal The power_signal to disable interrupts for.
 * @return 0 is successful
 * @return negative If interrupt are not available on this signal.
 */
int power_signal_disable_interrupt(enum power_signal signal);

/**
 * @brief Get the debug name associated with this signal.
 *
 * @param signal The power_signal value to get.
 * @return string The name of the signal.
 */
const char *power_signal_name(enum power_signal signal);

/**
 * @brief Initialize the power signal interface.
 *
 * Called when the power sequence code is ready to start
 * processing inputs and outputs.
 */
void power_signal_init(void);

/**
 * @brief Power signal interrupt handler
 *
 * Called when an input signal causes an interrupt.
 */
void power_signal_interrupt(void);

#endif /* __AP_PWRSEQ_SIGNAL_INTERFACE_H__ */
