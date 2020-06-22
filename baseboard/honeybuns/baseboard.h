/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns baseboard configuration */

#ifndef __CROS_EC_BASEBOARD_H
#define __CROS_EC_BASEBOARD_H

/* EC Defines */
#define CONFIG_CRC8
/* #define CONFIG_ADC */

/* TODO Define FLASH_PSTATE_LOCKED prior to building MP FW. */
#undef CONFIG_FLASH_PSTATE_LOCKED

/*
 * TODO(b/148493929): Revisit these timer values once support for STM32G4 has
 * been added. These macros are required to build the EC image.
 */
/* 48 MHz SYSCLK clock frequency */
#define CPU_CLOCK 48000000
#define CONFIG_STM_HWTIMER32
#define TIM_CLOCK32 2
#define TIM_CLOCK_MSB  3
#define TIM_CLOCK_LSB 15
#define TIM_WATCHDOG 7

/* Honeybuns platform does not have a lid switch */
#undef CONFIG_LID_SWITCH

#undef CONFIG_UART_CONSOLE
#define CONFIG_UART_TX_DMA
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 2048


#define I2C_ADDR_EEPROM_FLAGS   0x50
#define CONFIG_CROS_BOARD_INFO
#define CONFIG_BOARD_VERSION_CBI
#define CONFIG_CMD_CBI


/* USB defines */
/* USB Configuration */
#define CONFIG_USB
#define CONFIG_STREAM_USB
#define CONFIG_USB_UPDATE
#define CONFIG_USB_SERIALNO
#define DEFAULT_SERIALNO "Uninitialized"
#define CONFIG_MAC_ADDR
#define DEFAULT_MAC_ADDR "Uninitialized"

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL		0
#define USB_EP_UPDATE		1
#define USB_EP_CONSOLE		2
#define USB_EP_COUNT		3

#define USB_IFACE_UPDATE	0
#define USB_IFACE_CONSOLE	1
#define USB_IFACE_COUNT		2

#ifndef __ASSEMBLER__
/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_SERIALNO,
	USB_STR_VERSION,
	USB_STR_I2C_NAME,
	USB_STR_UPDATE_NAME,
#ifdef CONFIG_USB_ISOCHRONOUS
	USB_STR_HEATMAP_NAME,
#endif
	USB_STR_COUNT
};
#endif

/* USB Type C and USB PD defines */
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_TCPMV2
#define CONFIG_USB_DRP_ACC_TRYSRC
/* No AP on any honeybuns variants */
#undef CONFIG_USB_PD_HOST_CMD

#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_CUSTOM_PDO
#define CONFIG_USB_PD_DUAL_ROLE
/* #define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0 */
#define CONFIG_USB_PD_TCPM_MUX
#define CONFIG_USB_PD_TCPM_PS8805
#define CONFIG_USB_PD_TCPM_STM32GX
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_DECODE_SOP
#define CONFIG_USB_PID 0x5048

#define CONFIG_USB_PD_VBUS_DETECT_PPC
#define CONFIG_USB_PD_DISCHARGE_PPC
#define CONFIG_USBC_PPC_SN5S330
#define CONFIG_USBC_PPC_VCONN
#define CONFIG_USBC_PPC_DEDICATED_INT
#define CONFIG_CMD_PPC_DUMP
#define CONFIG_CMD_TCPC_DUMP

#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_UFP_USB3

#define CONFIG_USB_BCD_DEV 0x0001 /* v 0.01 */
#define CONFIG_USB_PD_IDENTITY_HW_VERS 1
#define CONFIG_USB_PD_IDENTITY_SW_VERS 1

/* Define typical operating power and max power. */
#define PD_MAX_VOLTAGE_MV     20000
#define PD_MAX_CURRENT_MA     3000
#define PD_MAX_POWER_MW       45000
#define PD_OPERATING_POWER_MW 15000

/* TODO(b:147314141): Verify these timings */
#define PD_POWER_SUPPLY_TURN_ON_DELAY	30000	/* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY	250000	/* us */
#define PD_VCONN_SWAP_DELAY		5000	/* us */


/* BC 1.2 */

/* I2C Bus Configuration */
#define CONFIG_I2C
#define CONFIG_I2C_MASTER

#ifndef __ASSEMBLER__

struct power_seq {
	int signal;
	int pol;
	int delay;
};

enum adc_channel {
	ADC_NOT_NEEDED,	/* ADC0 */
	ADC_CH_COUNT
};

extern const struct power_seq board_power_seq[];
extern void baseboard_manage_hpd_event(int signal);

#endif /* !__ASSEMBLER__ */


#endif /* __CROS_EC_BASEBOARD_H */
