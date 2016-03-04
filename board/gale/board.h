/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* gale board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART1 (PA9/PA10) */
#define CONFIG_UART_CONSOLE 1
#undef  CONFIG_UART_TX_DMA
#undef  CONFIG_UART_RX_DMA

/* Optional features */
#define CONFIG_ADC
#define CONFIG_BOARD_PRE_INIT
#undef  CONFIG_CMD_I2C_SCAN
#undef  CONFIG_CMD_I2C_XFER
#undef  CONFIG_CMD_IDLE_STATS
#undef  CONFIG_CMD_SHMEM
#undef  CONFIG_CMD_TIMERINFO
#define CONFIG_CMD_TYPEC
#undef  CONFIG_COMMON_GPIO_SHORTNAMES
#define CONFIG_CONSOLE_CMDHELP
#undef  CONFIG_DEBUG_ASSERT
#define CONFIG_FORCE_CONSOLE_RESUME
#undef  CONFIG_HIBERNATE
#undef  CONFIG_HOSTCMD_EVENTS
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_I2C_SLAVE
#undef  CONFIG_LID_SWITCH
#undef  CONFIG_LOW_POWER_IDLE
#undef  CONFIG_PWM
#define CONFIG_STM_HWTIMER32
#undef  CONFIG_TASK_PROFILING
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_CUSTOM_VDM
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_USB_PD_VBUS_DETECT_NONE
#define CONFIG_USB_PD_PORT_COUNT 1
#define CONFIG_USB_PD_TCPC
#define CONFIG_USB_PD_TCPM_STUB
#define CONFIG_USBC_SS_MUX
#define CONFIG_VBOOT_HASH
#undef  CONFIG_WATCHDOG
#undef  CONFIG_WATCHDOG_HELP
/* Use PSTATE embedded in the RO image, not in its own erase block */
#undef  CONFIG_FLASH_PSTATE_BANK
#undef  CONFIG_FW_PSTATE_SIZE
#define CONFIG_FW_PSTATE_SIZE 0

/* USART and USB stream drivers */
#define CONFIG_STREAM_USART
#define CONFIG_STREAM_USART2
#define CONFIG_STREAM_USB
/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x500f

/* Prevent the USB driver from initializing at boot */
#define CONFIG_USB_INHIBIT_INIT

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE   0
#define USB_IFACE_AP_STREAM 1
#define USB_IFACE_UNUSED    2
#define USB_IFACE_SPI       3
#define USB_IFACE_COUNT     4

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL   0
#define USB_EP_CONSOLE   1
#define USB_EP_AP_STREAM 2
#define USB_EP_SPI       3
#define USB_EP_COUNT     4

/* Enable console over USB */
#define CONFIG_USB_CONSOLE

#define CONFIG_SPI_MASTER
/* Enable control of SPI over USB */
#define CONFIG_SPI_FLASH_PORT    0  /* First SPI master port */
#define CONFIG_USB_SPI
#define CONFIG_CMD_SPI_XFER
/* Enable Case Closed Debugging */
#define CONFIG_CASE_CLOSED_DEBUG

/* I2C ports configuration */
#define I2C_PORT_SLAVE  0
#define I2C_PORT_EC I2C_PORT_SLAVE

/* slave address for host commands */
#ifdef HAS_TASK_HOSTCMD
#define CONFIG_HOSTCMD_I2C_SLAVE_ADDR CONFIG_USB_PD_I2C_SLAVE_ADDR
#endif

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2
#define TIM_ADC     15

#include "gpio_signal.h"

/* ADC signal */
enum adc_channel {
	ADC_CC1 = 0,
	ADC_CC2,
	ADC_VBUS,
	ADC_CUR_SENSE,
	ADC_CH_COUNT
};

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,
	USB_STR_AP_STREAM_NAME,

	USB_STR_COUNT
};

/* 1.5A Rp */
#define PD_SRC_VNC            PD_SRC_1_5_VNC_MV
#define PD_SRC_RD_THRESHOLD   PD_SRC_1_5_RD_THRESH_MV

/* start as a sink in case we have no other power supply/battery */
#define PD_DEFAULT_STATE PD_STATE_SNK_DISCONNECTED

/* TODO: determine the following board specific type-C power constants */
/* we are never a source : don't care about power supply */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  0 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 0 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 12000
#define PD_MAX_POWER_MW       15000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     5000

/* Send host event to AP */
void pd_send_host_event(int mask);
/* Set power supply ready */
void board_set_power_supply_ready(int ready);

#endif /* !__ASSEMBLER__ */

#endif /* __BOARD_H */
