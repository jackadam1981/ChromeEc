/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __CROS_EC_DACS_H
#define __CROS_EC_DACS_H

enum dac_t {
	CC0_DAC = 1,
	CC1_DAC,
};

void init_dacs(void);
void enable_dac(enum dac_t dac, uint8_t en);
int write_dac(enum dac_t dac, uint16_t value);

#endif /* __CROS_EC_DACS_H */
