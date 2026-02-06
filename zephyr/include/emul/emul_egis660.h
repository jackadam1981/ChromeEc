/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef ZEPHYR_INCLUDE_EMUL_EMUL_EGIS660_H_
#define ZEPHYR_INCLUDE_EMUL_EMUL_EGIS660_H_

/* EGIS660 example hardware id */
#define EGIS660_HWID 0x2D11

/**
 * Set hardware id returned by emulator
 *
 * @param target The target emulator to modify
 * @param hardware_id new hardware id
 */
void egis660_set_hwid(const struct emul *target, uint16_t hardware_id);

/**
 * Get low power mode status
 *
 * @param target The target emulator to get status
 */
uint8_t egis660_get_low_power_mode(const struct emul *target);

#endif /* ZEPHYR_INCLUDE_EMUL_EMUL_EGIS660_H_ */
