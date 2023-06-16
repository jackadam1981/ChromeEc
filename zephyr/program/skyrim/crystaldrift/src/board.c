/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "chipset.h"
#include "console.h"
#include "cros_board_info.h"
#include "hooks.h"
#include "i2c.h"

#include <zephyr/drivers/gpio.h>

static void usb_porta_startup(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_usbhub_en), 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, usb_porta_startup, HOOK_PRIO_DEFAULT);

static void usb_porta_shutdown(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_usbhub_en), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, usb_porta_shutdown, HOOK_PRIO_DEFAULT);

#define MP2845A_FLAGS 0x20
/*Command code*/
#define PAGE 0x00
#define OPERATION 0x01
#define MFR_SLOPE_SET_1PHS 0x50
#define MFR_SLOPE_SET_2PHS 0x51
#define MFR_SLOPE_SET_3PHS 0x52
#define MFR_SLOPE_SET_4PHS 0x53
#define MFR_SLOPE_SET_5PHS 0x54
#define MFR_SLOPE_SET_DCM 0x55
#define MFR_SLOPE_SET_EXT 0x56
#define IOUT_CAL_OFFSET_GAIN 0x57
#define MFR_CB_SATU_PI 0x58
#define MFR_VCAL_PI 0x59
#define OCP_CAL_GAIN_OFFSET 0x5A
#define MFR_CUR_GAIN_OFFSET 0x5B
#define MFR_CS_OFFSET32 0x5C
#define MFR_CS_OFFSET54 0x5D
#define MFR_TEMP_GAIN_OFFSET 0x5E
#define MFR_RAIL_CTRL1 0x5F
#define MFR_RAIL_CTRL2 0x60
#define MFR_TRANS_VR_BOOT 0x61
#define VOUT_COMMAND 0x62
#define MFR_PHS_CFG_ADDR 0x63
#define MFR_SW_PRD_SET 0x64
#define MFR_FREQ_DET 0x65
#define MFR_PWR_DLY 0x66
#define MFR_PWM_MINTIME_SET 0x67
#define MFR_MINOFF_TIME 0x68
#define MFR_VOUT_CMPS_MAX 0x69
#define MFR_VOUT_TRIM 0x6A
#define MFR_PSI_TRIM 0x6B
#define MFR_PROTECT_CFG 0x6C
#define MFR_APSI_1PHL 0x6D
#define MFR_DYNAMIC_CTRL 0x6E
#define MFR_UVP_OVP_DELAY 0x6F
#define VIN_ON_OFF_SET 0x70
#define VIN_OV_UV_SET 0x71
#define MFR_PLATFORM_SET 0x72
#define MFR_DROOP_SET 0x73
#define VOUT_CAP_MAX_MIN 0x74
#define OVUV_OCWARN_THRESHOLD 0x75
#define TOTAL_OCP_SET 0x76
#define MFR_SVI3_VERSION_ID 0x77
#define MFR_MFG_ID_SCALE_VIP0 0x78
#define SVI3_CONFIG_BOOT_SRP0 0x79
#define MFR_ROC_IMON_SET 0x7A
#define MFR_DEBUG1 0x7B
#define MFR_DEBUG2 0x7C
#define PRODUCT_DATA_CODE 0x7F
#define MFR_I2C_PASSWORD 0x82
#define CODE_REV 0x83

/*Command page1 code*/
#define MFR_MFG_ID_SCALE_VI 0x77
#define SVI3_CONFIG_BOOT_SR 0x78

/*Command page2 code*/
#define MFR_IOUT_OFFSET1_PS2 0x53
#define MFR_IOUT_OFFSET2_PS2 0x54

void mp2845_init(void)
{
	/*set page0 param*/
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS, PAGE,
		       0x00))
		goto mp2845_error;
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
		       OPERATION, 0x80))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_1PHS, 0x046C))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_2PHS, 0x0942))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_3PHS, 0x1028))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_4PHS, 0x151E))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_5PHS, 0x1A10))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_DCM, 0x0496))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_EXT, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			IOUT_CAL_OFFSET_GAIN, 0xE8CD))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CB_SATU_PI, 0xB50A))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VCAL_PI, 0xA020))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OCP_CAL_GAIN_OFFSET, 0x8000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CUR_GAIN_OFFSET, 0x7BA4))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CS_OFFSET32, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CS_OFFSET54, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TEMP_GAIN_OFFSET, 0x3214))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL1, 0x6CE0))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL2, 0x2CA1))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TRANS_VR_BOOT, 0x0303))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_COMMAND, 0x44C8))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PHS_CFG_ADDR, 0x48A0))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SW_PRD_SET, 0x1CA7))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_FREQ_DET, 0xAAA9))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PWR_DLY, 0x0E83))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PWM_MINTIME_SET, 0x40EF))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MINOFF_TIME, 0x2410))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_CMPS_MAX, 0x80FF))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_TRIM, 0x0035))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PSI_TRIM, 0x8888))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PROTECT_CFG, 0x070F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_APSI_1PHL, 0x00A3))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DYNAMIC_CTRL, 0x012F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_UVP_OVP_DELAY, 0x0641))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VIN_ON_OFF_SET, 0x2622))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VIN_OV_UV_SET, 0xB028))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PLATFORM_SET, 0x1534))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DROOP_SET, 0x5848))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_CAP_MAX_MIN, 0x4E32))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OVUV_OCWARN_THRESHOLD, 0x7866))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			TOTAL_OCP_SET, 0x8242))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SVI3_VERSION_ID, 0x0145))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MFG_ID_SCALE_VIP0, 0x0292))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			SVI3_CONFIG_BOOT_SRP0, 0x0874))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_ROC_IMON_SET, 0x00FE))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DEBUG1, 0x3A44))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DEBUG2, 0x070F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			PRODUCT_DATA_CODE, 0x0002))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_I2C_PASSWORD, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			CODE_REV, 0x4B01))
		goto mp2845_error;

	/*set page1 param*/
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS, PAGE,
		       0x01))
		goto mp2845_error;
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
		       OPERATION, 0x80))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_1PHS, 0x046C))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_2PHS, 0x0842))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_DCM, 0x0496))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_EXT, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			IOUT_CAL_OFFSET_GAIN, 0x00CD))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CB_SATU_PI, 0x000A))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VCAL_PI, 0xA020))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OCP_CAL_GAIN_OFFSET, 0x8000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CUR_GAIN_OFFSET, 0x7BA4))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TEMP_GAIN_OFFSET, 0x3214))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL1, 0x6CE0))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL2, 0x08A1))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TRANS_VR_BOOT, 0x0306))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_COMMAND, 0x44C8))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SW_PRD_SET, 0x00A7))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_FREQ_DET, 0xAAA9))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PWR_DLY, 0x0083))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MINOFF_TIME, 0x0410))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_CMPS_MAX, 0x00FF))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_TRIM, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PSI_TRIM, 0x0888))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PROTECT_CFG, 0x000F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_APSI_1PHL, 0x00A3))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DYNAMIC_CTRL, 0x012F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_UVP_OVP_DELAY, 0x0641))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PLATFORM_SET, 0x1A34))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DROOP_SET, 0x5800))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_CAP_MAX_MIN, 0x4B32))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OVUV_OCWARN_THRESHOLD, 0x7166))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			TOTAL_OCP_SET, 0x8042))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MFG_ID_SCALE_VI, 0x9002))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			SVI3_CONFIG_BOOT_SR, 0x0001))
		goto mp2845_error;

	/*set page2 param*/
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS, PAGE,
		       0x02))
		goto mp2845_error;
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
		       OPERATION, 0x80))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_1PHS, 0x046C))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_IOUT_OFFSET1_PS2, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_IOUT_OFFSET2_PS2, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_DCM, 0x0496))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_EXT, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			IOUT_CAL_OFFSET_GAIN, 0x00CD))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VCAL_PI, 0xA020))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OCP_CAL_GAIN_OFFSET, 0x8000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CUR_GAIN_OFFSET, 0x7BA4))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TEMP_GAIN_OFFSET, 0x3214))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL1, 0x9B30))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL2, 0x0511))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TRANS_VR_BOOT, 0x0304))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_COMMAND, 0x44C8))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SW_PRD_SET, 0x00A7))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_FREQ_DET, 0xAAA9))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PWR_DLY, 0x0083))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MINOFF_TIME, 0x0410))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_CMPS_MAX, 0x00FF))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_TRIM, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PSI_TRIM, 0x0088))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PROTECT_CFG, 0x000F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_APSI_1PHL, 0x00A3))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DYNAMIC_CTRL, 0x012F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_UVP_OVP_DELAY, 0x0641))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PLATFORM_SET, 0x1A34))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DROOP_SET, 0x5866))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_CAP_MAX_MIN, 0x4B32))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OVUV_OCWARN_THRESHOLD, 0x5866))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			TOTAL_OCP_SET, 0x5C42))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MFG_ID_SCALE_VI, 0x8802))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			SVI3_CONFIG_BOOT_SR, 0x0002))
		goto mp2845_error;

	/*set page3 param*/
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS, PAGE,
		       0x03))
		goto mp2845_error;
	if (i2c_write8(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
		       OPERATION, 0x80))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_1PHS, 0x046C))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_2PHS, 0x0842))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_DCM, 0x044B))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SLOPE_SET_EXT, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			IOUT_CAL_OFFSET_GAIN, 0xE2CD))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CB_SATU_PI, 0x000A))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VCAL_PI, 0xA020))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OCP_CAL_GAIN_OFFSET, 0x8000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_CUR_GAIN_OFFSET, 0x7BA4))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TEMP_GAIN_OFFSET, 0x3214))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL1, 0x6CE0))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_RAIL_CTRL2, 0x0061))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_TRANS_VR_BOOT, 0x0304))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_COMMAND, 0x44C8))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_SW_PRD_SET, 0x00A7))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_FREQ_DET, 0xAAA9))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PWR_DLY, 0x0083))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MINOFF_TIME, 0x2410))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_CMPS_MAX, 0x00FF))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_VOUT_TRIM, 0x0000))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PSI_TRIM, 0x0088))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PROTECT_CFG, 0x000F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_APSI_1PHL, 0x00A3))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DYNAMIC_CTRL, 0x012F))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_UVP_OVP_DELAY, 0x0641))
		goto mp2845_error;

	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_PLATFORM_SET, 0x1534))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_DROOP_SET, 0x596C))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			VOUT_CAP_MAX_MIN, 0x4B32))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			OVUV_OCWARN_THRESHOLD, 0x3E66))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			TOTAL_OCP_SET, 0x4442))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			MFR_MFG_ID_SCALE_VI, 0x8802))
		goto mp2845_error;
	if (i2c_write16(chg_chips[CHARGER_SOLO].i2c_port, MP2845A_FLAGS,
			SVI3_CONFIG_BOOT_SR, 0x0874))
		goto mp2845_error;
	return;
mp2845_error:
	ccprints("%s failed", __func__);
};
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, mp2845_init, HOOK_PRIO_DEFAULT + 1);
