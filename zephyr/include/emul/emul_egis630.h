/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_EGIS630_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_EGIS630_H_

/* EGIS630 example hardware id */
#define EGIS630_HWID 0x276

/**
 * Set hardware id returned by emulator
 *
 * @param target The target emulator to modify
 * @param hardware_id new hardware id
 */
void egis630_set_hwid(const struct emul *target, uint16_t hardware_id);

/**
 * Get low power mode status
 *
 * @param target The target emulator to get status
 */
uint8_t egis630_get_low_power_mode(const struct emul *target);

/**
 * Stop handling IRQ gpio
 *
 * @param target The target emulator
 */
void egis630_stop_irq(const struct emul *target);

/**
 * Stop SPI transactions
 *
 * @param target The target emulator
 */
void egis630_stop_spi(const struct emul *target);

/**
 * Start SPI transactions
 *
 * @param target The target emulator
 */
void egis630_start_spi(const struct emul *target);

/**
 * Flip the IRQ value
 *
 * @param target The target emulator
 */
void egis630_emul_gpio_flip_irq(const struct emul *target);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_EGIS630_H_ */
