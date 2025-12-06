/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for IT8XXX2 emulator
 */

#ifndef __EMUL_IT8XXX2_H
#define __EMUL_IT8XXX2_H

#include "driver/tcpm/it83xx_pd.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>

/* Helper functions for the host build environment to map addresses */
#define IT8XXX2_PORT_FROM_ADDR(addr) /* ... implemented in emul_it8xxx2.c */
#define IT8XXX2_OFFSET_FROM_ADDR(addr) /* ... implemented in emul_it8xxx2.c */

/* Public API for controlling the emulator state */
void emul_it8xxx2_init(void);
void emul_it8xxx2_check_write(int port, int offset, uint8_t value);
void emul_it8xxx2_set_cc_state(int port, enum tcpc_cc_voltage_state cc1_state,
			       enum tcpc_cc_voltage_state cc2_state);
void emul_it8xxx2_trigger_irq(int port); /* Function to signal the EC task */
#endif /* __EMUL_IT8XXX2_H */
