/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Reston board configuration */

#ifndef __BOARD_H
#define __BOARD_H

/* Optional features */
#define CONFIG_ADC
#define CONFIG_DAC
#define CONFIG_I2C
#define CONFIG_USB
#define CONFIG_USB_HID
#define CONFIG_WATCHDOG_HELP
#undef CONFIG_LID_SWITCH

/* USB configuration */
#define CONFIG_USB_PID 0x5006
/* By default, enable all console messages excepted USB */
#define CC_DEFAULT     (CC_ALL & ~CC_MASK(CC_USB))

/* we don't have a write protect switch. */
#define CONFIG_SYSTEM_UNLOCKED

#ifndef __ASSEMBLER__

/* I2C ports configuration */
#define I2C_PORT_MASTER 0
#define I2C_PORTS_USED 1

/* Timer selection */
#define TIM_CLOCK_MSB 3
#define TIM_CLOCK_LSB 9
#define TIM_WATCHDOG  4

/* GPIO signal list */
enum gpio_signal {
	GPIO_AMP_WARN,
	GPIO_AMP_EN,

	GPIO_FSR_COLA,
	GPIO_FSR_COLB,
	GPIO_FSR_COLC,
	GPIO_FSR_COLD,
	GPIO_FSR_COLE,
	GPIO_FSR_COLF,
	GPIO_FSR_COLG,
	GPIO_FSR_COLH,
	GPIO_FSR_COLI,
	GPIO_FSR_COLJ,
	GPIO_FSR_ROW1,
	GPIO_FSR_ROW2,
	GPIO_FSR_ROW3,
	GPIO_FSR_ROW4,
	GPIO_FSR_ROW5,
	GPIO_FSR_ROW6,
	GPIO_FSR_ROW7,
	GPIO_FSR_ROW8,
	GPIO_FSR_ROW9,
	GPIO_FSR_ROW10,

	GPIO_HAP_ROW1,
	GPIO_HAP_ROW2,
	GPIO_HAP_ROW3,
	GPIO_HAP_ROW4,
	GPIO_HAP_ROW5,
	GPIO_HAP_ROW6,
	GPIO_HAP_ROW7,
	GPIO_HAP_ROW8,
	GPIO_HAP_ROW9,
	GPIO_HAP_ROW10,
	GPIO_HAP_COLA,
	GPIO_HAP_COLB,
	GPIO_HAP_COLC,
	GPIO_HAP_COLD,
	GPIO_HAP_COLE,
	GPIO_HAP_COLF,
	GPIO_HAP_COLG,
	GPIO_HAP_COLH,
	GPIO_HAP_COLI,
	/* Test points */
	GPIO_TP_PB8,
	GPIO_TP_PB9,
	GPIO_TP_PD5,
	GPIO_TP_PD6,
	GPIO_TP_PD7,
	GPIO_TP_PE1,
	GPIO_TP_PE2,
	GPIO_TP_PE3,
	GPIO_TP_PE4,
	GPIO_MCU_WK2,

	GPIO_I2C1_SCL,
	GPIO_I2C1_SDA,
	GPIO_DAC_OUT,

	/* Unimplemented signals we emulate */
	GPIO_I2C2_SCL,
	GPIO_I2C2_SDA,
	GPIO_ENTERING_RW,
	GPIO_WP_L,
	/* Number of GPIOs; not an actual GPIO */
	GPIO_COUNT
};

/* ADC signal */
enum adc_channel {
	ADC_CH_FSR_COLA = 0,
	ADC_CH_FSR_COLB,
	ADC_CH_FSR_COLC,
	ADC_CH_FSR_COLD,
	ADC_CH_FSR_COLE,
	ADC_CH_FSR_COLF,
	ADC_CH_FSR_COLG,
	ADC_CH_FSR_COLH,
	ADC_CH_FSR_COLI,
	ADC_CH_FSR_COLJ,
	/* Number of ADC channels */
	ADC_CH_COUNT
};

void trigger_feedback(int row, int col);

/* USB string indexes */
enum usb_strings {
	USB_STR_DESC = 0,
	USB_STR_VENDOR,
	USB_STR_PRODUCT,
	USB_STR_VERSION,

	USB_STR_COUNT
};
#endif /* !__ASSEMBLER__ */

/* USB interface indexes (use define rather than enum to expand them) */
#define USB_IFACE_HID    0
#define USB_IFACE_COUNT  1

/* USB endpoint indexes (use define rather than enum to expand them) */
#define USB_EP_CONTROL   0
#define USB_EP_HID       1
#define USB_EP_COUNT     2

#endif /* __BOARD_H */
