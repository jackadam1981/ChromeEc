/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* STM32F072-discovery board based USB PD evaluation configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000

/* the UART console is on USART2 (PA14/PA15) */
#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_CONSOLE 2

/*
 * Flash layout: we redefine the sections offsets and sizes as we will use
 * RO/RW regions of different sizes.
 *
 * RO will contain all the useful code (PD task, TCPC drivers), while RW will be
 * a minimal image with only console enabled.
 */
#undef _IMAGE_SIZE
#undef CONFIG_ROLLBACK_OFF
#undef CONFIG_ROLLBACK_SIZE
#undef CONFIG_FLASH_PSTATE
#undef CONFIG_FW_PSTATE_SIZE
#undef CONFIG_FW_PSTATE_OFF
#undef CONFIG_SHAREDLIB_SIZE
#undef CONFIG_RO_MEM_OFF
#undef CONFIG_RO_STORAGE_OFF
#undef CONFIG_RO_SIZE
#undef CONFIG_RW_MEM_OFF
#undef CONFIG_RW_STORAGE_OFF
#undef CONFIG_RW_SIZE
#undef CONFIG_EC_PROTECTED_STORAGE_OFF
#undef CONFIG_EC_PROTECTED_STORAGE_SIZE
#undef CONFIG_EC_WRITABLE_STORAGE_OFF
#undef CONFIG_EC_WRITABLE_STORAGE_SIZE
#undef CONFIG_WP_STORAGE_OFF
#undef CONFIG_WP_STORAGE_SIZE

#define CONFIG_FLASH_PSTATE
/* Do not use a dedicated PSTATE bank */
#undef CONFIG_FLASH_PSTATE_BANK

#define CONFIG_SHAREDLIB_SIZE	0
#define CONFIG_RO_MEM_OFF	0
#define CONFIG_RO_STORAGE_OFF	0
#define CONFIG_RO_SIZE		(100*1024)

#define CONFIG_RW_MEM_OFF	(CONFIG_RO_SIZE + CONFIG_RO_MEM_OFF)
#define CONFIG_RW_STORAGE_OFF	0
#define CONFIG_RW_SIZE		(CONFIG_FLASH_SIZE_BYTES - \
				 (CONFIG_RW_MEM_OFF - CONFIG_RO_MEM_OFF))

#define CONFIG_EC_PROTECTED_STORAGE_OFF		CONFIG_RO_MEM_OFF
#define CONFIG_EC_PROTECTED_STORAGE_SIZE	CONFIG_RO_SIZE
#define CONFIG_EC_WRITABLE_STORAGE_OFF		CONFIG_RW_MEM_OFF
#define CONFIG_EC_WRITABLE_STORAGE_SIZE		CONFIG_RW_SIZE

#define CONFIG_WP_STORAGE_OFF		CONFIG_EC_PROTECTED_STORAGE_OFF
#define CONFIG_WP_STORAGE_SIZE		CONFIG_EC_PROTECTED_STORAGE_SIZE

/* Common RO/RW defines */
#undef CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH
#undef CONFIG_CMD_PD
/* All TCPC and PD features only exist for RO */
#ifdef SECTION_IS_RO

/* Optional features */
#define CONFIG_HW_CRC
#define CONFIG_I2C
#define CONFIG_I2C_DEBUG
#define CONFIG_I2C_CONTROLLER
#define CONFIG_STM_HWTIMER32
#undef CONFIG_CMD_PD
#undef CONFIG_CMD_I2C_SCAN
#undef CONFIG_CMD_GPIO_EXTENDED
#undef CONFIG_CMD_CRASH
#undef CONFIG_CMD_SYSINFO
/* USB Power Delivery configuration */
/* #define CONFIG_USB_PD_TCPC_LOW_POWER */
#define CONFIG_USB_POWER_DELIVERY
/* #define CONFIG_USB_PD_TCPMV1 */

#define CONFIG_USB_PD_TCPMV2
#define CONFIG_USB_PD_REV30
#undef CONFIG_USB_CHARGER
#define CONFIG_USB_PD_DECODE_SOP
#define CONFIG_USB_DRP_ACC_TRYSRC
#define CONFIG_MATH_UTIL

#define CONFIG_USB_TYPEC_SM
#define CONFIG_USB_PRL_SM
#define CONFIG_USB_PE_SM

#define CONFIG_USB_PD_TBT_COMPAT_MODE
#define CONFIG_USB_PD_USB4
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_DFP_ONLY

/* #define CONFIG_USB_PD_DEBUG_LEVEL 3 */
/* #define DEBUG_PRINT_FLAG_AND_EVENT_NAMES */
#define CONFIG_USB_PD_EXTENDED_MESSAGES
#define CONFIG_CMD_PD
/* #define USB_PD_DEBUG_LABELS */
/* #define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE */

#define CONFIG_USB_PD_TRY_SRC

#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_PDO
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_PORT_MAX_COUNT 1
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_VBUS_DETECT_TCPC

#define CONFIG_USB_PD_MUX_CTRL_BY_ANX7406
#define CONFIG_USBC_RETIMER_ANX7443
#define CONFIG_USB_PD_TCPM_ANX7406_AUX
#define CONFIG_USB_PD_TCPM_ANX7406
/* #define CONFIG_USB_PD_TCPM_ANX7447 */
#define CONFIG_USB_PD_TCPM_MUX

#undef CONFIG_USB_PD_INITIAL_DRP_STATE
#define CONFIG_USB_PD_INITIAL_DRP_STATE PD_DRP_TOGGLE_ON

#undef CONFIG_USB_PD_PULLUP
#define CONFIG_USB_PD_PULLUP TYPEC_RP_USB

/* fake board specific type-C power constants */
#define PD_POWER_SUPPLY_TURN_ON_DELAY  30000  /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY 650000 /* us */

/* Define typical operating power and max power */
#define PD_OPERATING_POWER_MW 15000
#define PD_MAX_POWER_MW       60000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_VOLTAGE_MV     20000

/* I2C master port connected to the TCPC */
#define I2C_PORT_TCPC 0
#define I2C_PORT_PD_MCU 0

/* Timer selection */

#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
/* delay to turn on/off vconn */

/* USB Configuration */
#define CONFIG_USB
#define CONFIG_USB_PID 0x500f
#define CONFIG_USB_CONSOLE

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_CONSOLE 0
#define USB_IFACE_COUNT   1

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL 0
#define USB_EP_CONSOLE 1
#define USB_EP_COUNT   2

#endif /* SECTION_IS_RO */

/*
 * Allow dangerous commands all the time, since we don't have a write protect
 * switch.
 */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* Timer selection */
#define TIM_CLOCK32 2

#include "gpio_signal.h"

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,
	USB_STR_CONSOLE_NAME,

	USB_STR_COUNT
};

void board_reset_pd_mcu(void);

#endif /* !__ASSEMBLER__ */
#endif /* __BOARD_H */
