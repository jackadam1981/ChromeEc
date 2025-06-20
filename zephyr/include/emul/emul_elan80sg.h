/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_ELAN80SG_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_ELAN80SG_H_

/**
 * Get low power mode status
 *
 * @param target The target emulator to get status
 */
uint8_t elan80sg_get_low_power_mode(const struct emul *target);

/**
 * Stop handling IRQ gpio
 *
 * @param target The target emulator
 */
void elan80sg_stop_irq(const struct emul *target);

/**
 * Stop SPI transactions
 *
 * @param target The target emulator
 */
void elan80sg_stop_spi(const struct emul *target);

/**
 * Start SPI transactions
 *
 * @param target The target emulator
 */
void elan80sg_start_spi(const struct emul *target);

/**
 * Set the Hardware ID (HWID) registers on the Elan80sg emulator.
 *
 * The full 16-bit HWID is constructed from the concatenated high and low bytes.
 * This function simulates the device having a specific hardware revision.
 *
 * @param target The target emulator
 * @param hwid_lo The low byte of the 16-bit HWID
 * @param hwid_hi The high byte of the 16-bit HWID
 */
void elan80sg_set_hwid(const struct emul *target, uint8_t hwid_lo,
		       uint8_t hwid_hi);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_ELAN80SG_H_ */
