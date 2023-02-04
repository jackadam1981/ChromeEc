/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Very simple 16-bit CRC function.
 */
#ifndef __CROS_EC_CRC16_H
#define __CROS_EC_CRC16_H

#include <stdint.h>

/**
 * Return CRC-16 of the data, using X^16 + X^15 + X^2 + 1 polynomial.
 * @param data uint8_t *, input, a pointer to input data
 * @param len int, input, size of input data in bytes
 * @return the crc-16 of the input data.
 */
uint16_t cros_crc16(const uint8_t *data, int len);

/**
 * Return CRC-16 of the data, based upon pre-calculated partial CRC of previous
 * data.
 * @param data uint8_t *, input, a pointer to input data
 * @param len int, input, size of input data in bytes
 * @param previous_crc uint16_t, input, pre-calculated CRC of previous data.
 *        Seed with zero for a new calculation.
 * @return the crc-16 of the input data.
 */
uint16_t cros_crc16_arg(const uint8_t *data, int len, uint16_t previous_crc);

#endif /* __CROS_EC_CRC16_H */
