/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Header file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#ifndef __CROS_EC_INTEL_PD_TASK_H
#define __CROS_EC_INTEL_PD_TASK_H

/**
 * PD interrupt to wake the task
 *
 * All the PD interrupts are muxed to single GPIO to keep common code for
 * single port / dual port PD solutions offered by different vendors.
 *
 * @param signal Signal that generates the interrupt
 */
void intel_pd_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_INTEL_PD_TASK_H */
