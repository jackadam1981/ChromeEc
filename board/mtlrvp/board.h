/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#undef CONFIG_ADC
#undef CONFIG_HIBERNATE
#undef CONFIG_SPI_FLASH

/* GPIO64/65 are used as UART pins. */
#define NPCX_UART_MODULE2 1

/* Power sequencing */
#define GPIO_EC_SPI_OE_N GPIO_EC_SPI_OE_MECC_R
#define GPIO_PG_EC_ALL_SYS_PWRGD GPIO_ALL_SYS_PWRGD
#define GPIO_PG_EC_RSMRST_ODL GPIO_RSMRST_PWRGD
#define GPIO_PCH_SLP_S0_L GPIO_PCH_SLP_S0_N_EC
#define GPIO_SYS_RESET_L GPIO_SYS_RST_ODL_EC
#define GPIO_PCH_RSMRST_L GPIO_PM_RSMRST_R_N
#define GPIO_PCH_PWRBTN_L GPIO_PM_PWRBTN_N_EC
#define GPIO_EN_PP3300_A GPIO_EC_DS3_R
#define GPIO_PCH_PWROK GPIO_PCH_PWROK_EC_R
#define GPIO_CPU_PROCHOT GPIO_PROCHOT_EC
#define GPIO_POWER_BUTTON_L GPIO_SMC_ONOFF_N
#define GPIO_LID_OPEN GPIO_SMC_LID

/* eSPI/Host communication */
#define GPIO_PCH_WAKE_L GPIO_PCH_WAKE_N

/* Chipset */
#define CONFIG_CHIPSET_METEORLAKE

/* SoC / PCH */
#define CONFIG_CHIPSET_RESET_HOOK
#define CONFIG_HOST_INTERFACE_ESPI
#define CONFIG_HOST_INTERFACE_ESPI_VW_SLP_S3
#define CONFIG_HOST_INTERFACE_ESPI_VW_SLP_S4
#define CONFIG_HOST_INTERFACE_ESPI_VW_SLP_S5
#define CONFIG_POWER_COMMON
#define CONFIG_POWER_S0IX
#define CONFIG_POWER_S4_RESIDENCY
#define CONFIG_POWER_TRACK_HOST_SLEEP_STATE

/* Board ID */
#define CONFIG_BOARD_VERSION_GPIO

/* Power Button */
#define CONFIG_POWER_BUTTON
#define CONFIG_POWER_BUTTON_X86
#define CONFIG_LID_SWITCH

#ifndef __ASSEMBLER__
#include "gpio_signal.h"

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
