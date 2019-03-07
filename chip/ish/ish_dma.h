/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISH_DMA_H
#define __CROS_EC_ISH_DMA_H

/* DMA return codes */
#define DMA_RC_OK 0 /* Success */
#define DMA_RC_TO 1 /* Time out */
#define DMA_RC_HW 2 /* HW error (OCP) */

/* DMA channels */
#define PAGING_CHAN 0
#define KERNEL_CHAN 1

#define DST_IS_DRAM (1 << 0)
#define SRC_IS_DRAM (1 << 1)
#define NON_SNOOP (1 << 2)

/* ISH5 and on */
#define RS0 0x0
#define RS3 0x3
#define RS_SRC_OFFSET 3
#define RS_DST_OFFSET 5

#define PAGE_SIZE 4096

#define read32(addr) (REG32(addr))
#define write32(addr, value) ((REG32(addr)) = value)

typedef enum {
	SRAM_TO_SRAM = 0,
	SRAM_TO_UMA = DST_IS_DRAM | (RS3 << RS_DST_OFFSET),
	UMA_TO_SRAM = SRC_IS_DRAM | (RS3 << RS_SRC_OFFSET),
	HOST_DRAM_TO_SRAM = SRC_IS_DRAM | (RS0 << RS_SRC_OFFSET),
	SRAM_TO_HOST_DRAM = DST_IS_DRAM | (RS0 << RS_DST_OFFSET)
} dma_mode_t;

void ish_dma_disable(void);
void ish_dma_init(void);
int ish_dma_copy(uint32_t chan, uint32_t dst, uint32_t src, uint32_t length,
		 dma_mode_t mode);
void ish_dma_set_msb(uint32_t chan, uint32_t dst_msb, uint32_t src_msb);
int ish_dma_page(uint32_t dst, uint32_t src,
		 int page_in); /* API for page manager/d0i3 task */
int ish_wait_for_dma_done(uint32_t ch);
void ish_dma_ocp_timeout_disable(void);
#endif
