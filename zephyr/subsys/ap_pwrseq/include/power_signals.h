/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __AP_PWRSEQ_POWER_SIGNALS_H__
#define __AP_PWRSEQ_POWER_SIGNALS_H__

#include <devicetree.h>

#if DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq)

/*
 * Create guards so that code used for a source is only
 * included if that signal source is configured.
 */
#define	HAS_GPIO_SIGNALS  DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_gpio)
#define	HAS_VW_SIGNALS	  DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_vw)
#define	HAS_EXT_SIGNALS   DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_external)
#define	HAS_NAMED_GPIO_SIGNALS	\
	DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_named_gpio)
#define	HAS_INTERRUPT_SIGNALS	\
	DT_HAS_COMPAT_STATUS_OKAY(intel_ap_pwrseq_interrupt)

/**
 * @brief Definitions for AP power sequence signals.
 *
 * Defines the enums for the AP power sequence signals.
 */

/**
 * @brief Generate the enum for this power signal.
 */
#define PWR_SIGNAL_ENUM(id) \
	 DT_STRING_UPPER_TOKEN(id, enum_name)

#define PWR_SIGNAL_ENUM_COMMA(id) \
	 PWR_SIGNAL_ENUM(id),
/**
 * @brief Enum of all power signals
 *
 * Defines the enums of all the power signals configured
 * in the system. Uses the 'enum-name' property to name
 * the signal.
 */
enum power_signal {
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_gpio, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_vw, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_external, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_interrupt, PWR_SIGNAL_ENUM_COMMA)
DT_FOREACH_STATUS_OKAY(intel_ap_pwrseq_named_gpio, PWR_SIGNAL_ENUM_COMMA)
	POWER_SIGNAL_COUNT,
};

#undef PWR_SIGNAL_ENUM_COMMA

#if HAS_EXT_SIGNALS
/**
 * Definitions required for external (board-specific)
 * power signals.
 *
 * int board_power_signal_get(enum power_signal signal)
 * {
 *     int value;
 *
 *     switch(signal) {
 *     default:
 *         LOG(LOG_ERR, "Unknown power signal!");
 *         return -1;
 *
 *     case PWR_VCCST_PWRGD:
 *         value = ...
 *         return value;
 *     }
 * }
 *
 */

/*
 * Board specific functions (if required)
 */
int board_power_signal_get(enum power_signal signal);
int board_power_signal_set(enum power_signal signal, int value);

#endif /* HAS_EXT_SIGNALS */

/**
 * @brief Get the value of this power signal.
 *
 * Retrieve the value of this power signal.
 *
 * @param signal The power_signal to get.
 * @return 0 or 1 Value of signal
 * @return negative If error occurred
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

typedef uint32_t power_signal_mask_t;

/**
 * @brief Update the stored mask of power signals.
 */
void power_update_signals(void);

/**
 * @brief Get the current power signals as a mask.
 *
 * @return Power signals as a mask.
 */
power_signal_mask_t power_get_signals(void);

/**
 * @brief Set the signal debug mask
 *
 * Sets a debug mask of signals.
 * A log is generated whenever any of these signals change.
 *
 * @param debug Mask of signals to be flagged for logging
 */
void power_set_debug(power_signal_mask_t debug);

/**
 * @brief Get the signal debug mask
 *
 * Gets the current debug mask of signals.
 *
 * @return Current mask of signals to be flagged for logging
 */
power_signal_mask_t power_get_debug(void);

/**
 * @brief Check if the desired signals are asserted.
 *
 * @param want Mask of signals to be checked
 * @return True if all the wanted signals are asserted.
 */
static inline bool power_has_signals(power_signal_mask_t want)
{
	return (power_get_signals() & want) == want;
}

/**
 * @brief Wait until the selected power signals match
 *
 * Given a signal mask and wanted value, wait until the
 * selected power signals match the wanted value.
 *
 * @param want The value of the signals to wait for.
 * @param mask The mask of the selected signals
 * @param timeout The amount of time to wait in ms.
 * @return 0 if the signals matched.
 * @return negative If the signals did not match before the timeout.
 */
int power_wait_mask_signals_timeout(power_signal_mask_t want,
				    power_signal_mask_t mask,
				    int timeout);

/**
 * @brief Wait until the selected power signals match
 *
 * Given a set of signals, wait until all of
 * the signals are asserted.
 *
 * @param want The value of the signals to wait for.
 * @param timeout The amount of time to wait in ms.
 * @return 0 if the signals matched.
 */
static inline int power_wait_signals_timeout(power_signal_mask_t want,
					     int timeout)
{
	return power_wait_mask_signals_timeout(want, want, timeout);
}


/**
 * @brief Create a mask from a power signal.
 */
#define POWER_SIGNAL_MASK(signal) (1 << (signal))

#endif /* DT_HAS_COMPAT_STATUS_OKAY */

#endif /* __AP_PWRSEQ_POWER_SIGNALS_H__ */
