/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_DMA_CHIP_H
#define __CROS_EC_DMA_CHIP_H

uint32_t dma_ap_to_scp(uint32_t ap_addr);
uint32_t dma_scp_to_ap(uint32_t scp_addr);
uint32_t dma_ap_to_scp_cache(uint32_t ap_addr);
uint32_t dma_scp_cache_to_ap(uint32_t scp_addr);

#endif /* #ifndef __CROS_EC_DMA_CHIP_H */
