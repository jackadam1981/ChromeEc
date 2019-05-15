/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_CONFIG_CHIP_IT83202_H
#define __CROS_EC_CONFIG_CHIP_IT83202_H

#define CHIP_CORE_RISCV

/* CPU core BFD configuration */
#include "core/riscv-rv32i/config_core.h"

 /* RISCV core */
#define CHIP_CORE_RISCV
#define CHIP_ILM_DLM_ORDER

/****************************************************************************/
/* Memory mapping */

#define CHIP_DLM_BASE               0x80080000
#define CHIP_ILM_BASE               0x80000000
 /* 0x80081000~0x80081FFF */
#define CHIP_H2RAM_BASE             (CHIP_DLM_BASE + 0x1000)
 /* 0x80082000~0x80082FFF */
#define CHIP_RAMCODE_BASE           (CHIP_DLM_BASE + 0x2000)

#define CONFIG_RAM_BASE             (0x80080000 + 0x3000)
#define CONFIG_RAM_SIZE             0x0003D000

/* System stack size */
#define CONFIG_STACK_SIZE           1024

/* non-standard task stack sizes */
#define SMALLER_TASK_STACK_SIZE     512
#define IDLE_TASK_STACK_SIZE        640
#define LARGER_TASK_STACK_SIZE      896
#define VENTI_TASK_STACK_SIZE       1024

/* Default task stack size */
#define TASK_STACK_SIZE             640

#define CONFIG_PROGRAM_MEMORY_BASE  (CHIP_ILM_BASE)

#if defined(CHIP_VARIANT_IT83202AX)
/* TODO: enable properly chip config option. */
#define CONFIG_FLASH_SIZE           0x00040000
/* chip id is 3 bytes */
#define IT83XX_CHIP_ID_3BYTES
/*
 * More GPIOs can be set as 1.8v input.
 * Please refer to gpio_1p8v_sel[] for 1.8v GPIOs.
 */
#define IT83XX_GPIO_1P8V_PIN_EXTENDED
/* All GPIOs support interrupt on rising, falling, and either edge. */
#define IT83XX_GPIO_INT_FLEXIBLE
/* Enable interrupts of group 21 and 22. */
#define IT83XX_INTC_GROUP_21_22_SUPPORT
/* Enable detect type-c plug in interrupt. */
#define IT83XX_INTC_PLUG_IN_SUPPORT
#else
#error "Unsupported chip variant!"
#endif

#endif  /* __CROS_EC_CONFIG_CHIP_IT83202_H */
