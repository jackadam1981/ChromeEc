/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32-specific DAC module for Chrome EC */

#ifndef __DAC_CHIP_H
#define __DAC_CHIP_H

int dac_play_samples(const uint8_t *data, int count, int sample_rate);

#endif /* __DAC_CHIP_H */
