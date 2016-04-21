/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Select DMA channel for a peripheral
 *
 * @param channel: Channel # base 0 (Note some STM32s use base 1)
 * @param peripheral: Refer to the TRM for 'peripheral request signals'
 */
void dma_select_channel(enum dma_channel channel, unsigned char peripheral);
