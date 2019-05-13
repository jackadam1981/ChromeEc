/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#ifndef __CROS_EC_WATCHDOG_H
#define __CROS_EC_WATCHDOG_H

#include <stdint.h>

#include "config.h"

/**
 * Initialize the watchdog.
 *
 * This will cause the CPU to reboot if it has been more than 2 watchdog
 * periods since watchdog_reload() has been called.
 */
int watchdog_init(void);

/**
 * Watchdog reset counter.
 *
 * Chips which support an always-on section of memory may wish to save
 * and restore a reset counter when the watchdog expires to prevent
 * the watchdog from resetting too many times. Chips that support this
 * will need to define the corresponding save/restore functions.
 */


#ifdef CONFIG_WATCHDOG
/* defined under common/watchdog.c */
void watchdog_set_reset_counter(uint32_t value);
uint32_t watchdog_get_reset_counter(void);

/* to be defined by chip */
void watchdog_save_reset_counter(uint32_t value);
uint32_t watchdog_restore_reset_counter(void);
#else
static __unused void watchdog_set_reset_counter(uint32_t value)
{
}

static __unused uint32_t watchdog_get_reset_counter(void)
{
	return 0;
}

static __unused void watchdog_save_reset_counter(uint32_t value)
{
}

static __unused uint32_t watchdog_restore_reset_counter(void)
{
	return 0;
}
#endif

/**
 * Display a trace with information about an expired watchdog timer
 *
 * This shows the location in the code where the expiration happened.
 * Usually this helps locate a loop which is blocking execution of the
 * watchdog task.
 *
 * @param excep_lr	Value of lr to indicate caller return
 * @param excep_sp	Value of sp to indicate caller task id
 */
void watchdog_trace(uint32_t excep_lr, uint32_t excep_sp);

/**
 * Watchdog has not been tickled recently warning. This function should be
 * called when the watchdog is close to firing.
 */
void watchdog_warning_irq(void);

/* Reload the watchdog counter */
#ifdef CONFIG_WATCHDOG
void watchdog_reload(void);
#else
static inline void watchdog_reload(void) { }
#endif

#endif /* __CROS_EC_WATCHDOG_H */
