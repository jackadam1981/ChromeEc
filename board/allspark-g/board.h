/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* cube board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/*
 * The flash size is only 32kB.
 * No space for 2 partitions,
 * put only RW at the beginning of the flash
 */
#undef  CONFIG_FW_INCLUDE_RO
#undef  CONFIG_RW_MEM_OFF
#define CONFIG_RW_MEM_OFF 0
#undef  CONFIG_RO_SIZE
#define CONFIG_RO_SIZE 0
/* Fake full size if we had a RO partition */
#undef  CONFIG_RW_SIZE
#define CONFIG_RW_SIZE CONFIG_FLASH_SIZE

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART1 (PA9/PA10) */
#undef  CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_ADC
#define CONFIG_BOARD_PRE_INIT
#undef  CONFIG_CMD_I2C_SCAN
#undef  CONFIG_CMD_I2C_XFER
#undef  CONFIG_CMD_IDLE_STATS
#undef  CONFIG_CMD_SHMEM
#undef  CONFIG_COMMON_GPIO_SHORTNAMES
#define CONFIG_CONSOLE_CMDHELP
#undef  CONFIG_CONSOLE_HISTORY
#define CONFIG_CONSOLE_HISTORY 2
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_FORCE_CONSOLE_RESUME
#undef  CONFIG_HIBERNATE
#undef  CONFIG_HOSTCMD_EVENTS
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#undef  CONFIG_LID_SWITCH
#undef  CONFIG_LOW_POWER_IDLE
#define CONFIG_STM_HWTIMER32
#define CONFIG_TASK_PROFILING
#undef  CONFIG_UART_TX_BUF_SIZE
#undef  CONFIG_UART_TX_DMA
#undef  CONFIG_UART_RX_DMA
#define CONFIG_UART_TX_BUF_SIZE 128
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_INTERNAL_COMP
#undef CONFIG_USB_PD_NO_VBUS_DETECT
#define CONFIG_USB_PD_PORT_COUNT 1
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_TCPM_STUB
#define CONFIG_USBC_VCONN
#define CONFIG_VBOOT_HASH
#undef  CONFIG_WATCHDOG
#undef  CONFIG_WATCHDOG_HELP
/* Use PSTATE embedded in the RO image, not in its own erase block */
#undef  CONFIG_FLASH_PSTATE_BANK
#undef  CONFIG_FW_PSTATE_SIZE
#define CONFIG_FW_PSTATE_SIZE 0

/* I2C ports configuration */
#define I2C_PORT_MASTER  0
#define I2C_PORT_EC I2C_PORT_MASTER

/* USB configuration */
#define CONFIG_USB_PID 0x5012
#define CONFIG_USB_BCD_DEV 0x0001 /* v 0.01 */

/* Realtek switch configuration */
#define CONFIG_RTK_SWITCH

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     3

#include "gpio_signal.h"

/* ADC signal */
enum adc_channel {
	ADC_C0_CC1_PD = 0,
	ADC_VBUS,
	ADC_CH_COUNT
};

/* 1.5A Rp */
#define PD_SRC_VNC            PD_SRC_1_5_VNC_MV
#define PD_SRC_RD_THRESHOLD   PD_SRC_1_5_RD_THRESH_MV

/* we are a power supply, boot as a power source waiting for a sink */
#define PD_DEFAULT_STATE PD_STATE_SRC_DISCONNECTED

/* delay necessary for the voltage transition on the power supply */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  50000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 50000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 12000
#define PD_MAX_POWER_MW       15000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     5000

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
