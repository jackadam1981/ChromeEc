/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Very simple 8-bit CRC function.
 */
#ifndef __EC_CRC8_H__
#define __EC_CRC8_H__

uint8_t crc8(const void *data, int len);

#endif /* __EC_CRC8_H__ */
