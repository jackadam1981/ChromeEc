/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_SIGNAL_INTERRUPT_H__
#define __AP_PWRSEQ_SIGNAL_INTERRUPT_H__


#define PWR_SIG_TAG_ISR	PWR_ISR_

/*
 * Generate enums for the gpio interrupts.
 * These enums are only used internally
 * to assign an index to each signal that is specific
 * to the source.
 */

#define TAG_ISR(tag, name) DT_CAT(tag, name)

#define PWR_ISR_ENUM(id) TAG_ISR(PWR_SIG_TAG_ISR, PWR_SIGNAL_ENUM(id)),

enum pwr_sig_interrupt {
#if HAS_INTERRUPT_SIGNALS
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_interrupt, PWR_ISR_ENUM)
#endif
	PWR_SIG_INTERRUPT_COUNT
};

#undef	PWR_ISR_ENUM
#undef	TAG_ISR

/**
 * @brief Get the value of the power signal from the interrupt GPIO.
 *
 * @param signal The enum of the interrupt to get.
 * @return the current value of the power signal.
 */
int power_signal_interrupt_get(enum pwr_sig_interrupt interrupt);

/**
 * @brief Enable the interrupt
 *
 * @param signal The enum of the interrupt to enable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_interrupt_enable_int(enum pwr_sig_interrupt interrupt);

/**
 * @brief Disable the interrupt
 *
 * @param signal The enum of the interrupt to disable.
 * @return 0 if successful
 * @return -error if failed
 */
int power_signal_interrupt_disable_int(enum pwr_sig_interrupt interrupt);

/**
 * @brief Initialize the Interrupts for the power signals.
 */
void power_signal_interrupt_init(void);

#endif /* __AP_PWRSEQ_SIGNAL_INTERRUPT_H__ */
