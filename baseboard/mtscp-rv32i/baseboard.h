/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MT SCP RV32i board configuration */

#ifndef __CROS_EC_BASEBOARD_H
#define __CROS_EC_BASEBOARD_H

/* #define CC_DEFAULT (CC_ALL & ~(CC_MASK(CC_HOSTCMD) | CC_MASK(CC_IPI))) */
#define CC_DEFAULT (CC_ALL)

#define CONFIG_FLASH_SIZE_BYTES CONFIG_RAM_BASE
#define CONFIG_LTO
#define CONFIG_POWER_TRACK_HOST_SLEEP_STATE

#if defined(CHIP_VARIANT_MT8195_CORE1)
#undef CONFIG_LTO
#define CONFIG_UART_CONSOLE 0
#else
#define CONFIG_UART_CONSOLE 1
#endif

/* IPI configs */
#define CONFIG_IPC_SHARED_OBJ_BUF_SIZE 288
#define CONFIG_IPC_SHARED_OBJ_ADDR                                             \
	(SCP_FW_END -                                                         \
	 (CONFIG_IPC_SHARED_OBJ_BUF_SIZE + 2 * 4 /* int32_t */) * 2)
#define CONFIG_IPI
#define CONFIG_RPMSG_NAME_SERVICE

#define SCP_IPI_INIT 0
#define SCP_IPI_VDEC_H264 1
#define SCP_IPI_VDEC_VP8 2
#define SCP_IPI_VDEC_VP9 3
#define SCP_IPI_VENC_H264 4
#define SCP_IPI_VENC_VP8 5
#define SCP_IPI_MDP_INIT 6
#define SCP_IPI_MDP_DEINIT 7
#define SCP_IPI_MDP_FRAME 8
#define SCP_IPI_DIP 9
#define SCP_IPI_ISP_CMD 10
#define SCP_IPI_ISP_FRAME 11
#define SCP_IPI_FD_CMD 12
#define SCP_IPI_HOST_COMMAND 13
#define SCP_IPI_VDEC_LAT 14
#define SCP_IPI_VDEC_CORE 15
#define SCP_IPI_COUNT 16

#define IPI_COUNT SCP_IPI_COUNT

#define SCP_IPI_NS_SERVICE 0xFF

/*
 * (1) DRAM cacheable region
 * (2) DRAM non-cacheable region
 * (3) Panic data region
 * (4) Kernel DMA allocable region
 * (5) DRAM end address
 *
 *                              base (size)
 * ---+-------------------- (1) CONFIG_DRAM_BASE (CONFIG_DRAM_SIZE)
 * C  | DRAM .text, .rodata
 *    | DRAM .data LMA
 *    | DRAM .bss, .data
 * ---+-------------------- (2) DRAM_NC_BASE (DRAM_NC_SIZE)
 * NC | .dramnc
 *    +-------------------- (3) CONFIG_PANIC_DRAM_BASE (CONFIG_PANIC_DRAM_SIZE)
 *    | Panic Data
 *    +-------------------- (4) KERNEL_BASE (KERNEL_SIZE)
 *    | Kernel DMA allocable
 *    | for MDP, etc.
 * ---+-------------------- (5) CONFIG_DRAM_SIZE + DRAM_TOTAL_SIZE (NA)
 *
 *     base       size
 * MT8192
 * (1) 0x10000000 0x00500000
 * (2) 0x10500000 0
 * (3) 0x10500000 0
 * (4) 0x10500000 0x00F00000
 * (5) 0x11400000
 * MT8195 core0
 * (1) 0x10000000 0x004FF000
 * (2) 0x104FF000 0
 * (3) 0x104FF000 0x00001000
 * (4) 0x10500000 0x00500000
 * (5) 0x10A00000
 * MT8195 core1
 * (1) 0x10A00000 0x003FF000
 * (2) 0x10DFF000 0x0
 * (3) 0x10DFF000 0x00001000
 * (4) 0x10E00000 0x01A00000
 * (5) 0x12800000
 */

/* size of (1) */
#define CONFIG_DRAM_SIZE (DRAM_TOTAL_SIZE - CONFIG_PANIC_DRAM_SIZE - DRAM_NC_SIZE - KERNEL_SIZE)
/* base of (2) */
#define DRAM_NC_BASE (CONFIG_DRAM_BASE + CONFIG_DRAM_SIZE)
/* base of (3) */
#define CONFIG_PANIC_DRAM_BASE (DRAM_NC_BASE + DRAM_NC_SIZE)
/* base of (4) */
#define KERNEL_BASE (CONFIG_PANIC_DRAM_BASE + CONFIG_PANIC_DRAM_SIZE)

#if defined(CHIP_VARIANT_MT8192)
/* base of (1) */
#define CONFIG_DRAM_BASE 0x10000000
/* Shared memory address in AP physical address space. */
#define CONFIG_DRAM_BASE_LOAD 0x50000000
/* size of (2) */
#define DRAM_NC_SIZE 0
/* size of (3) */
#define CONFIG_PANIC_DRAM_SIZE 0
/* size of (4) */
#define KERNEL_SIZE 0xF00000
/* DRAM total size for (5) */
#define DRAM_TOTAL_SIZE 0x01400000 /* 20 MB */
#endif /* CHIP_VARIANT_MT8192 */

#if defined(CHIP_VARIANT_MT8195)
#define CONFIG_DRAM_BASE 0x10000000
#define CONFIG_DRAM_BASE_LOAD 0x50000000
#define DRAM_NC_SIZE 0
#define CONFIG_PANIC_DRAM_SIZE 0x00001000 /* 4K */
#define KERNEL_SIZE 0x500000
#define DRAM_TOTAL_SIZE 0x00A00000 /* 10 MB */
#endif /* CHIP_VARIANT_MT8195 */

#if defined(CHIP_VARIANT_MT8195_CORE1)
#define CONFIG_DRAM_BASE 0x10A00000
#define CONFIG_DRAM_BASE_LOAD 0x50A00000
#define DRAM_NC_SIZE 0x0
#define CONFIG_PANIC_DRAM_SIZE 0x00001000 /* 4K */
#define KERNEL_SIZE 0x01A00000
#define DRAM_TOTAL_SIZE 0x01E00000 /* 30 MB */
#endif /* CHIP_VARIANT_MT8195_CORE1 */

#define SCP_CORE1_RAM_SIZE 0x10000 /* 64K */
#define SCP_CORE1_RAM_PADDING 0xc00 /* for 4K-alignment */

/* MPU settings */
#define NR_MPU_ENTRIES 16

#ifndef __ASSEMBLER__
#include "gpio_signal.h"
#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BASEBOARD_H */
