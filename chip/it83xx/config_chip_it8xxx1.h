/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_IT8XXX1_H
#define __CROS_EC_CONFIG_CHIP_IT8XXX1_H

/* CPU core BFD configuration */
#include "core/nds32/config_core.h"

/* N8 core */
#define CHIP_CORE_NDS32
#define CHIP_ILM_DLM_ORDER
/* The base address of EC interrupt controller registers. */
#define CHIP_EC_INTC_BASE           0x00F01100

/****************************************************************************/
/* Memory mapping */

#define CONFIG_RAM_BASE             0x00080000
#define CONFIG_RAM_SIZE             0x00010000
/* We reserve 12KB space for ramcode, h2ram, and immu sections. */
#define CHIP_RAM_SPACE_RESERVED     0x3000

/* CONFIG_RAM_BASE+0x1000 ~ CONFIG_RAM_BASE+0x1fff */
#define CHIP_H2RAM_BASE             (CONFIG_RAM_BASE + 0x1000)
/* CONFIG_RAM_BASE+0x2000 ~ CONFIG_RAM_BASE+0x2fff */
#define CHIP_RAMCODE_BASE           (CONFIG_RAM_BASE + 0x2000)
#define CHIP_EXTRA_STACK_SPACE      0

#define CONFIG_PROGRAM_MEMORY_BASE  0x00000000

#if defined(CHIP_VARIANT_IT83201AX)
/* This is the physical size of the flash on the chip. We'll reserve one bank
 * in order to emulate per-bank write-protection UNTIL REBOOT. The hardware
 * doesn't support a write-protect pin, and if we make the write-protection
 * permanent, it can't be undone easily enough to support RMA.
 */
#define CONFIG_FLASH_SIZE           0x00040000
/* chip id is 3 bytes */
#define IT83XX_CHIP_ID_3BYTES
/*
 * Disable eSPI pad, then PLL change
 * (include EC clock frequency) is succeed even CS# is low.
 */
#define IT83XX_ESPI_INHIBIT_CS_BY_PAD_DISABLED
/* The slave frequency is adjustable (bit[2-0] at register IT83XX_ESPI_GCAC1) */
#define IT83XX_ESPI_SLAVE_MAX_FREQ_CONFIGURABLE
/* Watchdog reset supports hardware reset. */
#define IT83XX_ETWD_HW_RESET_SUPPORT
/*
 * More GPIOs can be set as 1.8v input.
 * Please refer to gpio_1p8v_sel[] for 1.8v GPIOs.
 */
#define IT83XX_GPIO_1P8V_PIN_EXTENDED
/* All GPIOs support interrupt on rising, falling, and either edge. */
#define IT83XX_GPIO_INT_FLEXIBLE
/* Enable detect type-c plug in interrupt. */
#define IT83XX_INTC_PLUG_IN_SUPPORT
#else
#error "Unsupported chip variant of it8xxx1 series!"
#endif

#endif  /* __CROS_EC_CONFIG_CHIP_IT8XXX1_H */
