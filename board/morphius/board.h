/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Morphius board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

#define VARIANT_ZORK_TREMBYLE

#include <stdbool.h>
#include "baseboard.h"

/*
 * Allow dangerous commands.
 * TODO: Remove this config before production.
 */
#define CONFIG_SYSTEM_UNLOCKED
#define CONFIG_I2C_DEBUG

#define CONFIG_MKBP_USE_GPIO

#undef CONFIG_LED_ONOFF_STATES
/* Battery */
#define CONFIG_BATTERY_LEVEL_NEAR_FULL 91

#define CONFIG_8042_AUX
#define CONFIG_PS2
#define CONFIG_CMD_PS2
/* Motion sensing drivers */
#define CONFIG_ACCELGYRO_BMI160
#define CONFIG_ACCELGYRO_BMI160_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(BASE_ACCEL)
#define CONFIG_ACCEL_INTERRUPTS
#define CONFIG_ACCEL_KX022
#define CONFIG_CMD_ACCELS
#define CONFIG_CMD_ACCEL_INFO
#define CONFIG_TABLET_MODE
#define CONFIG_LID_ANGLE
#define CONFIG_LID_ANGLE_UPDATE
#define CONFIG_LID_ANGLE_SENSOR_BASE BASE_ACCEL
#define CONFIG_LID_ANGLE_SENSOR_LID LID_ACCEL

/* GPIO mapping from board specific name to EC common name. */
#define CONFIG_BATTERY_PRESENT_GPIO	GPIO_EC_BATT_PRES_ODL
#define GPIO_AC_PRESENT			GPIO_ACOK_OD
#define GPIO_CPU_PROCHOT		GPIO_PROCHOT_ODL
#define GPIO_EC_INT_L			GPIO_EC_AP_INT_ODL
#define GPIO_ENABLE_BACKLIGHT_L		GPIO_EC_EDP_BL_DISABLE
#define GPIO_ENTERING_RW		GPIO_EC_ENTERING_RW
#define GPIO_KBD_KSO2			GPIO_EC_KSO_02_INV
#define GPIO_PCH_RSMRST_L		GPIO_EC_FCH_RSMRST_L
#define GPIO_PCH_SLP_S3_L		GPIO_SLP_S3_L
#define GPIO_PCH_SLP_S5_L		GPIO_SLP_S5_L
#define GPIO_PCH_WAKE_L			GPIO_EC_FCH_WAKE_L
#define GPIO_POWER_BUTTON_L		GPIO_EC_PWR_BTN_ODL
#define GPIO_S0_PGOOD			GPIO_S0_PWROK_OD
#define GPIO_S5_PGOOD			GPIO_EC_PWROK_OD
#define GPIO_SYS_RESET_L		GPIO_EC_SYS_RST_L
#define GPIO_VOLUME_DOWN_L		GPIO_VOLDN_BTN_ODL
#define GPIO_VOLUME_UP_L		GPIO_VOLUP_BTN_ODL
#define GPIO_WP_L			GPIO_EC_WP_L

#ifndef __ASSEMBLER__

void ps2_pwr_en_interrupt(enum gpio_signal signal);

/* These GPIOs moved. Temporarily detect and support the V0 HW. */
extern enum gpio_signal GPIO_PCH_PWRBTN_L;
extern enum gpio_signal GPIO_PCH_SYS_PWROK;

enum battery_type {
	BATTERY_SMP,
	BATTERY_SUNWODA,
	BATTERY_LGC,
	BATTERY_TYPE_COUNT,
};

enum mft_channel {
	MFT_CH_0 = 0,
	/* Number of MFT channels */
	MFT_CH_COUNT,
};

enum pwm_channel {
	PWM_CH_KBLIGHT = 0,
	PWM_CH_FAN,
	PWM_CH_POWER_LED,
	PWM_CH_COUNT
};


/*****************************************************************************
 * CBI EC FW Configuration
 */
#include "cbi_ec_fw_config.h"

/**
 * MORPHIUS_T_USB0_AC
 *	USB-A0  Speed: 5 Gbps
 *		Retimer: none
 *	USB-C0  Speed: 5 Gbps
 *		Retimer: PI3DPX1207
 *		TCPC: NCT3807
 *		PPC: AOZ1380
 *		IOEX: TCPC
 */
enum morphius_ec_cfg_usb_mb_type {
	MORPHIUS_T_USB0_AC = 0,
};

/**
 * MORPHIUS_T_USB1_C_HDMI
 *	USB-A1  none
 *	USB-C1  Speed: 5 Gbps
 *		Retimer: PS8818
 *		TCPC: NCT3807
 *		PPC: NX20P3483
 *		IOEX: TCPC
 *	HDMI    Exists: yes
 *		Retimer: PI3HDX1204
 *		MST Hub: none
 *
 * MORPHIUS_T_USB1_C_HDMI_MSTHUB
 *	USB-A1  none
 *	USB-C1  Speed: 5 Gbps
 *		Retimer: PS8802
 *		TCPC: NCT3807
 *		PPC: NX20P3483
 *		IOEX: TCPC
 *	HDMI    Exists: yes
 *		Retimer: none
 *		MST Hub: RTD2141B
 */
enum morphius_ec_cfg_usb_db_type {
	MORPHIUS_T_USB1_C_HDMI = 0,
	MORPHIUS_T_USB1_C_HDMI_MSTHUB = 1,
};

static inline enum morphius_ec_cfg_usb_db_type
ec_config_has_usb_db(void)
{
	return get_cbi_ec_cfg_usb_db();
}


#define HAS_USBC1_RETIMER_PS8802 \
			(BIT(MORPHIUS_T_USB1_C_HDMI_MSTHUB))

static inline bool ec_config_has_usbc1_retimer_ps8802(void)
{
	return !!(BIT(ec_config_has_usb_db()) &
		  HAS_USBC1_RETIMER_PS8802);
}


#define HAS_USBC1_RETIMER_PS8818 \
			(BIT(MORPHIUS_T_USB1_C_HDMI))

static inline bool ec_config_has_usbc1_retimer_ps8818(void)
{
	return !!(BIT(ec_config_has_usb_db()) &
		  HAS_USBC1_RETIMER_PS8818);
}

#endif /* !__ASSEMBLER__ */


#endif /* __CROS_EC_BOARD_H */
