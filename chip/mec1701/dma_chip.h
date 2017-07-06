/* Copyright 2017 The Chromium OS Authors. All rights reserved
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MEC17xx DMA controller chip level API
 */
/** @file dma_chip.h
 *MEC17xx Direct Memory Access block
 */
/** @defgroup MEC17xx dma
 */

#ifndef _DMA_CHIP_H
#define _DMA_CHIP_H

#include <stdint.h>
#include <stddef.h>


#ifdef __cplusplus
extern "C" {
#endif

/* Place any C interfaces here */

void dma_xfr_start_rx(const struct dma_option *option,
		uint32_t dma_xfr_ulen,
		unsigned count, void *memory);

void dma_xfr_prepare_tx(const struct dma_option *option, unsigned count,
		const void *memory, uint32_t dma_xfr_units);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _DMA_CHIP_H */
/**   @}
 */

