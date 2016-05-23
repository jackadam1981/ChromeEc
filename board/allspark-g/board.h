/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* allspark-g board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/*
 * The console task is too big to include in both RO and RW images. Therefore,
 * if the console task is defined, then only build an RW image. This can be
 * useful for debugging to have a full console. Otherwise, without this task,
 * a full RO and RW is built with a limited one-way output console.
 */
#ifdef HAS_TASK_CONSOLE
/*
 * The flash size is only 64kB.
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
#endif /* HAS_TASK_CONSOLE */

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART1 (PA9/PA10) */
#undef  CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 1

/* Optional features */
#define CONFIG_ADC
#undef  CONFIG_ADC_WATCHDOG
#define CONFIG_BOARD_PRE_INIT
#define CONFIG_COMMON_GPIO_SHORTNAMES
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_FORCE_CONSOLE_RESUME
#undef  CONFIG_HIBERNATE
#undef  CONFIG_HOSTCMD_EVENTS
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_I2C_MASTER
#undef  CONFIG_LID_SWITCH
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_LTO
#define CONFIG_STM_HWTIMER32
#undef  CONFIG_TASK_PROFILING
#undef  CONFIG_UART_TX_BUF_SIZE
#undef  CONFIG_UART_TX_DMA
#undef  CONFIG_UART_RX_DMA
#define CONFIG_UART_TX_BUF_SIZE 128
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_HW_DEV_ID_BOARD_MAJOR USB_PD_HW_DEV_ID_ALLSPARK_G
#define CONFIG_USB_PD_HW_DEV_ID_BOARD_MINOR 1
#undef  CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_FLASH
#define CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_USB_PD_IDENTITY_HW_VERS 1
#define CONFIG_USB_PD_IDENTITY_SW_VERS 1
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_LOG_SIZE 256
#undef  CONFIG_USB_PD_TRY_SRC
#undef  CONFIG_USB_PD_NO_VBUS_DETECT
#define CONFIG_USB_PD_PORT_COUNT 1
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_TCPM_STUB
#define CONFIG_USBC_VCONN
#define CONFIG_VBOOT_HASH
#undef  CONFIG_SHA256
#undef  CONFIG_WATCHDOG
#undef  CONFIG_WATCHDOG_HELP
/* Use PSTATE embedded in the RO image, not in its own erase block */
#undef  CONFIG_FLASH_PSTATE_BANK
#undef  CONFIG_FW_PSTATE_SIZE
#define CONFIG_FW_PSTATE_SIZE 0

#ifdef HAS_TASK_CONSOLE
#undef  CONFIG_CONSOLE_HISTORY
#define CONFIG_CONSOLE_HISTORY 2
#else
#undef  CONFIG_CONSOLE_CMDHELP
#define CONFIG_DEBUG_PRINTF
#define UARTN CONFIG_UART_CONSOLE
#define UARTN_BASE STM32_USART_BASE(CONFIG_UART_CONSOLE)
#endif /* HAS_TASK_CONSOLE */

/* I2C ports configuration */
#define I2C_PORT_MASTER  0
#define I2C_PORT_EC I2C_PORT_MASTER

/* USB configuration */
#define CONFIG_USB_PID 0x501D
#define CONFIG_USB_BCD_DEV 0x0001 /* v 0.01 */

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     3

#include "gpio_signal.h"

/* ADC signal */
enum adc_channel {
  ADC_TEMP_DETECT = 0,
	ADC_C0_CC1_PD,
	ADC_VBUS,
	ADC_CH_COUNT
};

/* 3.0A Rp */
#define PD_SRC_VNC            PD_SRC_3_0_VNC_MV
#define PD_SRC_RD_THRESHOLD   PD_SRC_3_0_RD_THRESH_MV

/* we are a power supply, boot as a power source waiting for a sink */
#define PD_DEFAULT_STATE PD_STATE_SRC_DISCONNECTED

/* delay necessary for the voltage transition on the power supply */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  50000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 50000 /* us */

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
