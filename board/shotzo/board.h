/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shotzo board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/* Select Baseboard features */
#define VARIANT_DEDEDE_EC_IT8320
#include "baseboard.h"

/* Battery */
#define CONFIG_BATTERY_FUEL_GAUGE

/* BC 1.2 */
#define CONFIG_BC12_DETECT_PI3USB9201

/* Charger */
#define CONFIG_CHARGE_RAMP_HW
#define CONFIG_CHARGER_SM5803		/* C0 and C1: Charger */
#define CONFIG_USB_PD_VBUS_DETECT_CHARGER
#define CONFIG_USB_PD_5V_CHARGER_CTRL
#define CONFIG_CHARGER_OTG
#undef  CONFIG_CHARGER_SINGLE_CHIP

/* PWM */
#define CONFIG_PWM

/* TCPC */
#define CONFIG_USB_PD_PORT_MAX_COUNT 2
#define CONFIG_USB_PD_TCPM_ITE_ON_CHIP	/* C0: ITE EC TCPC */
#define CONFIG_USB_PD_TCPM_PS8705	/* C1: PS8705 TCPC*/
#define CONFIG_USB_PD_ITE_ACTIVE_PORT_COUNT 1
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_TCPC_LOW_POWER

/* Thermistors */
#define CONFIG_TEMP_SENSOR
#define CONFIG_THERMISTOR
#define CONFIG_STEINHART_HART_3V3_51K1_47K_4050B

/* USB Mux and Retimer */
#define CONFIG_USB_MUX_IT5205			/* C1: ITE Mux */
#define I2C_PORT_USB_MUX I2C_PORT_USB_C0	/* Required for ITE Mux */

/* USB Type A Features */
#define USB_PORT_COUNT 1
#define CONFIG_USB_PORT_POWER_DUMB

/* Dedicated barreljack charger port */
#undef  CONFIG_DEDICATED_CHARGE_PORT_COUNT
#define CONFIG_DEDICATED_CHARGE_PORT_COUNT 1
#define DEDICATED_CHARGE_PORT 2

/* Unused Features */
#define CONFIG_POWER_BUTTON_IGNORE_LID
#undef CONFIG_BACKLIGHT_LID
#undef CONFIG_HIBERNATE
#undef CONFIG_KEYBOARD_BOOT_KEYS
#undef CONFIG_KEYBOARD_RUNTIME_KEYS
#undef CONFIG_CMD_KEYBOARD
#undef CONFIG_LID_SWITCH
#undef CONFIG_VOLUME_BUTTONS

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "registers.h"

enum chg_id {
	CHARGER_PRIMARY,
	CHARGER_SECONDARY,
	CHARGER_NUM,
};

enum pwm_channel {
	PWM_CH_COUNT,
};

/* ADC channels */
enum adc_channel {
	ADC_VSNS_PP3300_A,     /* ADC0 */
	ADC_TEMP_SENSOR_1,     /* ADC2 */
	ADC_TEMP_SENSOR_2,     /* ADC3 */
	ADC_SUB_ANALOG,        /* ADC13 */
	ADC_TEMP_SENSOR_3,     /* ADC15 */
	ADC_TEMP_SENSOR_4,     /* ADC16 */
	ADC_CH_COUNT
};

enum temp_sensor_id {
	TEMP_SENSOR_1,
	TEMP_SENSOR_2,
	TEMP_SENSOR_3,
	TEMP_SENSOR_4,
	TEMP_SENSOR_COUNT
};

/* List of possible batteries */
enum battery_type {
	BATTERY_DUMMY,
	BATTERY_TYPE_COUNT,
};

enum charge_port {
	CHARGE_PORT_TYPEC0,
	CHARGE_PORT_TYPEC1,
	CHARGE_PORT_BARRELJACK,
};

/* Pin renaming */
#define GPIO_AC_PRESENT		GPIO_BJ_ADP_PRESENT_L

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
