/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMS TCS3408 light sensor driver
 */

#ifndef __CROS_EC_ALS_TCS3408_H
#define __CROS_EC_ALS_TCS3408_H

#include "driver/als_tcs3408_public.h"

/* ID for TCS34001 and TCS34005 */
#define TCS340015_DEVICE_ID 0x90

/* ID for TCS34003 and TCS34007 */
#define TCS340037_DEVICE_ID 0x93

/* Register Map */
#define TCS_I2C_ENABLE 0x80 /* R/W Enables states */
#define TCS_I2C_ATIME 0x81 /* R/W ALS integration time */
#define TCS_I2C_WTIME 0x83 /* R/W Wait time */
#define TCS_I2C_AILTL 0x84 /* R/W ALS interrupt low threshold low bit */
#define TCS_I2C_AILTH 0x85 /* R/W ALS interrupt low threshold high byte */
#define TCS_I2C_AIHTL 0x86 /* R/W ALS interrupt high threshold low byte */
#define TCS_I2C_AIHTH 0x87 /* R/W ALS interrupt high threshold high byte */
#define TCS_I2C_AUX 0x90 /* R/W Auxiliary ID */
#define TCS_I2C_REVID 0x91 /* R Revision ID */
#define TCS_I2C_ID 0x92 /* R Device ID */
#define TCS_I2C_STATUS 0x93 /* R Device status one */
#define TCS_I2C_ASTATUS 0x94 /* R ALS status*/
#define TCS_I2C_CDATAL 0x95 /* R Clear / IR channel low data register */
#define TCS_I2C_CDATAH 0x96 /* R Clear / IR channel high data register */
#define TCS_I2C_RDATAL 0x97 /* R Red ADC low data register */
#define TCS_I2C_RDATAH 0x98 /* R Red ADC high data register */
#define TCS_I2C_GDATAL 0x99 /* R Green ADC low data register */
#define TCS_I2C_GDATAH 0x9A /* R Green ADC high data register */
#define TCS_I2C_BDATAL 0x9B /* R Blue ADC low data register */
#define TCS_I2C_BDATAH 0x9C /* R Blue ADC high data register */
#define TCS_I2C_WDATAL 0x9D /* R Wideband low Date register */
#define TCS_I2C_WDATAH 0x9E /* R Wideband high Date register */
#define TCS_I2C_FDATAL 0x9F /* R Flicker low Date register */
#define TCS_I2C_FDATAH 0xA0 /* R Flicker High Date register */
#define TCS_I2C_STATUS2 0xA3 /* R/W Device status two */
#define TCS_I2C_STATUS3 0xA4 /* R/W Device status three */
#define TCS_I2C_STATUS5 0xA6 /* R/W Device status five */
#define TCS_I2C_STATUS7 0xA7 /* R/W Device status six */
#define TCS_I2C_CFG0 0xA9 /* R/W Configuration zero */
#define TCS_I2C_CFG1 0xAA /* R/W Configuration one */
#define TCS_I2C_CFG3 0xAC /* R/W Configuration three */
#define TCS_I2C_CFG4 0xAD /* R/W Configuration four */
#define TCS_I2C_CFG6 0xAF /* R/W Configuration six */
#define TCS_I2C_CFG8 0xB1 /* R/W Configuration eight */
#define TCS_I2C_CFG9 0xB2 /* R/W Configuration nine */
#define TCS_I2C_CFG10 0xB3 /* R/W Configuration ten */
#define TCS_I2C_CFG11 0xB4 /* R/W Configuration eleven */
#define TCS_I2C_CFG12 0xB5 /* R/W Configuration twelve */
#define TCS_I2C_PERS 0xBD /* R/W Persistence configuration */
#define TCS_I2C_ASTAPL 0xCA /* R/W ALS integration step size low bit */
#define TCS_I2C_ASTAPH 0xCB /* R/W ALS integration step size high bit */
#define TCS_I2C_AGC 0xCF /* R/W Maximum AGC gains */
#define TCS_I2C_AZCONFIG 0xD6 /* R/W Autozero configuration */
#define TCS_I2C_FDSTATUA 0xDB
/* R/W Flicker detection configuration zero */
#define TCS_I2C_INTENAB 0xF9 /* R/W Enable interrupts */
#define TCS_I2C_CONTROL 0xFA /* R/W Gain control register */

// Configration calculations
#define ASTEP_US_PER_100         278
#define ATIME_PER_STEP_X100      278
#define ATIME_MS(ms)             (uint8_t)(((uint32_t)ms*100 + ((uint32_t)ATIME_PER_STEP_X100 >> 1))/(uint32_t)ATIME_PER_STEP_X100 - 1)
#define WTIME_PER_STEP_X100      284
#define WTIME_MS(ms)             (uint8_t)(((uint32_t)ms*100 + ((uint32_t)WTIME_PER_STEP_X100 >> 1))/(uint32_t)WTIME_PER_STEP_X100 - 1)
#define ALS_PERSIST(p)           (uint8_t)((p & 0x0F) << 0)
#define INTEGRATION_CYCLE        2780
#define DIV_POINT                500
#define COEF_SCALE               1000


//CFG1 @0xAA
#define AGAIN_0_5X                      (0x00 << 0)
#define AGAIN_1X                        (0x01 << 0)
#define AGAIN_2X                        (0x02 << 0)
#define AGAIN_4X                        (0x03 << 0)
#define AGAIN_8X                        (0x04 << 0)
#define AGAIN_16X                       (0x05 << 0)
#define AGAIN_32X                       (0x06 << 0)
#define AGAIN_64X                       (0x07 << 0)
#define AGAIN_128X                      (0x08 << 0)
#define AGAIN_256X                      (0x09 << 0)
#define AGAIN_512X                      (0x0A << 0)
#define AGAIN_1024X                     (0x0B << 0)
#define AGAIN_2048X                     (0x0C << 0)

#define ALS_CAL_NONE            0x0
#define ALS_CAL_LUX             0x1
#define ALS_CAL_CCT             0x2
#define ALS_CAL_CHANNEL         0x4
#define ALS_CAL_COEF_SCALE		1000

#define TCS3408_PON_SHIFT 0
#define TCS3408_MASK_PON (1 << TCS3408_PON_SHIFT)

#define TCS3408_AEN_SHIFT 1
#define TCS3408_MASK_AEN (1 << TCS3408_AEN_SHIFT)

#define TCS3408_PEN_SHIFT 2
#define TCS3408_MASK_PEN (1 << TCS3408_PEN_SHIFT)

#define TCS3408_WEN_SHIFT 3
#define TCS3408_MASK_WEN (1 << TCS3408_WEN_SHIFT)

#define TCS3408_FDEN_SHIFT 6
#define TCS3408_MASK_FDEN (1 << TCS3408_FDEN_SHIFT)

#define TCS3408_IBEN_SHIFT 7
#define TCS3408_MASK_IBEN (1 << TCS3408_IBEN_SHIFT)

#define TCS3408_SIEN_SHIFT 0
#define TCS3408_MASK_SIEN (1 << TCS3408_SIEN_SHIFT)

#define TCS3408_CIEN_SHIFT 1
#define TCS3408_MASK_CIEN (1 << TCS3408_CIEN_SHIFT)

#define TCS3408_FIEN_SHIFT 2
#define TCS3408_MASK_FIEN (1 << TCS3408_FIEN_SHIFT)

#define TCS3408_AIEN_SHIFT 3
#define TCS3408_MASK_AIEN (1 << TCS3408_AIEN_SHIFT)

#define TCS3408_PIEN0_SHIFT 4
#define TCS3408_MASK_PIEN0 (1 << TCS3408_PIEN0_SHIFT)

#define TCS3408_PIEN1_SHIFT 5
#define TCS3408_MASK_PIEN1 (1 << TCS3408_PIEN1_SHIFT)

#define TCS3408_PSIEN_SHIFT 6
#define TCS3408_MASK_PSIEN (1 << TCS3408_PSIEN_SHIFT)

#define TCS3408_ASIEN_SHIFT 7
#define TCS3408_MASK_ASIEN (1 << TCS3408_ASIEN_SHIFT)

#define TCS3408_SIEN_FD_SHIFT 6
#define TCS3408_MASK_SIEN_FD_SHIFT (1 << TCS3408_SIEN_FD_SHIFT)

#define TCS3408_CLEAR_SAI_ACTIVE_SHIFT 0
#define TCS3408_MASK_CLEAR_SAI_ACTIVE (1 << TCS3408_CLEAR_SAI_ACTIVE_SHIFT)

#define TCS3408_FIFO_CLR_SHIFT 1
#define TCS3408_MASK_FIFO_CLR (1 << TCS3408_FIFO_CLR_SHIFT)

#define TCS3408_ALS_MANUAL_AZ_SHIFT 2
#define TCS3408_MASK_ALS_MANUAL_AZ (1 << TCS3408_ALS_MANUAL_AZ_SHIFT)

#define TCS3408_AGAIN_SHIFT 0
#define TCS3408_MASK_AGAIN (0x0C << TCS3408_AGAIN_SHIFT)

#define TCS3408_RAM_BANK_SHIFT 0
#define TCS3408_MASK_RAM_BANK (0x3 << TCS3408_RAM_BANK_SHIFT)

#define TCS3408_ALS_TRIGGER_LONG_SHIFT 2
#define TCS3408_MASK_ALS_TRIGGER_LONG (1 << TCS3408_ALS_TRIGGER_LONG_SHIFT)

#define TCS3408_PROX_TRIGGER_LONG_SHIFT 3
#define TCS3408_MASK_PROX_TRIGGER_LONG (1 << TCS3408_PROX_TRIGGER_LONG)

#define TCS3408_LOWPOWER_IDLE_SHIFT 5
#define TCS3408_MASK_LOWPOWER_IDLE (1 << TCS3408_LOWPOWER_IDLE_SHIFT)

#define TCS3408_SWAP_PROX_ALS5_SHIFT 0
#define TCS3408_MASK_SWAP_PROX_ALS5 (1 << TCS3408_SWAP_PROX_ALS5_SHIFT)

#define TCS3408_ALS_AGC_ENABLE_SHIFT 2
#define TCS3408_MASK_ALS_AGC_ENABLE (1 << TCS3408_ALS_AGC_ENABLE_SHIFT)

#define TCS3408_FD_AGC_DISABLE_SHIFT 3
#define TCS3408_MASK_FD_AGC_DISABLE (1 << TCS3408_FD_AGC_DISABLE_SHIFT)

#define TCS3408_CONCURRENT_PROX_AND_ALS_SHIFT 4
#define TCS3408_MASK_CONCURRENT_PROX_AND_ALS (1 << TCS3408_CONCURRENT_PROX_AND_ALS_SHIFT)

#define TCS3408_FIFO_THR_SHIFT 6
#define TCS3408_MASK_FIFO_THR (0xC0 << TCS3408_FIFO_THR_SHIFT)

#define TCS3408_APERS_SHIFT 0
#define TCS3408_MASK_APERS (0xF << TCS3408_APERS_SHIFT)

#define TCS3408_PPERS_SHIFT 4
#define TCS3408_MASK_PPERS (0x0F << TCS3408_PPERS_SHIFT)

#define TCS3408_FIFO_WRITE_ASTATUS_SHIFT 0
#define TCS3408_MASK_FIFO_WRITE_ASTATUS (1 << TCS3408_FIFO_WRITE_ASTATUS_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA0_SHIFT 1
#define TCS3408_MASK_FIFO_WRITE_ADATA0 (1 << TCS3408_FIFO_WRITE_ADATA0_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA1_SHIFT 2
#define TCS3408_MASK_FIFO_WRITE_ADATA1 (1 << TCS3408_FIFO_WRITE_ADATA1_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA2_SHIFT 3
#define TCS3408_MASK_FIFO_WRITE_ADATA2 (1 << TCS3408_FIFO_WRITE_ADATA2_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA3_SHIFT 4
#define TCS3408_MASK_FIFO_WRITE_ADATA3 (1 << TCS3408_FIFO_WRITE_ADATA3_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA4_SHIFT 5
#define TCS3408_MASK_FIFO_WRITE_ADATA4 (1 << TCS3408_FIFO_WRITE_ADATA4_SHIFT)

#define TCS3408_FIFO_WRITE_ADATA5_SHIFT 6
#define TCS3408_MASK_FIFO_WRITE_ADATA5 (1 << TCS3408_FIFO_WRITE_ADATA5_SHIFT)

#define TCS3408_FIFO_WRITE_PDATA_SHIFT 7
#define TCS3408_MASK_FIFO_WRITE_PDATA (1 << TCS3408_FIFO_WRITE_PDATA_SHIFT)

#define TCS3408_SINT_SHIFT 0
#define TCS3408_MASK_SINT (1 << TCS3408_SINT_SHIFT)

#define TCS3408_CINT_SHIFT 1
#define TCS3408_MASK_CINT (1 << TCS3408_CINT_SHIFT)

#define TCS3408_FINT_SHIFT 2
#define TCS3408_MASK_FINT (1 << TCS3408_FINT_SHIFT)

#define TCS3408_AINT_SHIFT 3
#define TCS3408_MASK_AINT (1 << TCS3408_AINT_SHIFT)

#define TCS3408_PINT0_SHIFT 4
#define TCS3408_MASK_PINT0 (1 << TCS3408_PINT0_SHIFT)

#define TCS3408_PINT1_SHIFT 5
#define TCS3408_MASK_PINT1 (1 << TCS3408_PINT1_SHIFT)

#define TCS3408_PSAT_SHIFT 6
#define TCS3408_MASK_PSAT (1 << TCS3408_PSAT_SHIFT)

#define TCS3408_ASAT_SHIFT 7
#define TCS3408_MASK_ASAT (1 << TCS3408_ASAT_SHIFT)

#define TCS3408_FIFO_OV_SHIFT 7
#define TCS3408_MASK_FIFO_OV (1 << TCS3408_FIFO_OV_SHIFT)


#define TCS_I2C_ENABLE_POWER_ON BIT(0)
#define TCS_I2C_ENABLE_ALS_ENABLE BIT(1)
#define TCS_I2C_ENABLE_WAIT_ENABLE BIT(3)
#define TCS_I2C_ENABLE_INT_ENABLE BIT(3)
#define TCS_I2C_FLICKER_DETCTION_ENABLE BIT(6)
#define TCS_I2C_ENABLE_SLEEP_AFTER_INT BIT(4)
#define TCS_I2C_ENABLE_MASK                                    \
	(TCS_I2C_ENABLE_POWER_ON | TCS_I2C_ENABLE_ALS_ENABLE | \
	 TCS_I2C_ENABLE_WAIT_ENABLE | TCS_I2C_FLICKER_DETCTION_ENABLE)

enum tcs3408_mode {
	TCS3408_MODE_SUSPEND = 0,
	TCS3408_MODE_IDLE = (TCS_I2C_ENABLE_POWER_ON),
	TCS3408_MODE_COLLECTING =
		(TCS_I2C_ENABLE_POWER_ON | TCS_I2C_ENABLE_ALS_ENABLE),
};

#define TCS_I2C_CONTROL_MASK 0x0C
#define TCS_I2C_STATUS_RGBC_VALID BIT(3) /* in A3 bit 3*/
#define TCS_I2C_STATUS_ALS_IRQ BIT(4)
#define TCS_I2C_STATUS_ALS_DIGITAL_SATURATED BIT(4)
#define TCS_I2C_STATUS_ALS_ANALOG_SATURATED BIT(3)

#define TCS_I2C_AUX_ASL_INT_ENABLE BIT(5)

/* Light data resides at 0x94 thru 0x98 */
#define TCS_DATA_START_LOCATION TCS_I2C_CDATAL
#define TCS_CLEAR_DATA_SIZE 2
#define TCS_RGBC_DATA_SIZE 8

#define TCS3408_DRV_DATA(_s) ((struct als_drv_data_t *)(_s)->drv_data)
#define TCS3408_RGB_DRV_DATA(_s) \
	((struct tcs3408_rgb_drv_data_t *)(_s)->drv_data)

/*
 * Factor to multiply light value by to determine if an increase in gain
 * would cause the next value to saturate.
 *
 * On the TCS3408, gain increases 4x each time again register setting is
 * incremented.  However, I see cases where values that are 24% of saturation
 * go into saturation after increasing gain, causing a back-and-forth cycle to
 * occur :
 *
 * [134.654994 tcs3408_adjust_sensor_for_saturation value=65535 100% Gain=2 ]
 * [135.655064 tcs3408_adjust_sensor_for_saturation value=15750 24% Gain=1 ]
 * [136.655107 tcs3408_adjust_sensor_for_saturation value=65535 100% Gain=2 ]
 *
 * To avoid this, we require value to be <= 20% of saturation level
 * (TCS_GAIN_SAT_LEVEL) before allowing gain to be increased.
 */
#define TCS_GAIN_ADJUST_FACTOR 5
#define TCS_GAIN_SAT_LEVEL (TCS_SATURATION_LEVEL / TCS_GAIN_ADJUST_FACTOR)
#define TCS_UPSHIFT_FACTOR_N 25 /* upshift factor = 2.5 */
#define TCS_UPSHIFT_FACTOR_D 10
#define TCS_GAIN_UPSHIFT_LEVEL \
	(TCS_SATURATION_LEVEL * TCS_UPSHIFT_FACTOR_D / TCS_UPSHIFT_FACTOR_N)

/*
 * Percentage of saturation level that the auto-adjusting anti-saturation
 * method will drive towards.
 */
#define TSC_SATURATION_LOW_BAND_PERCENT 90
#define TSC_SATURATION_LOW_BAND_LEVEL \
	(TCS_SATURATION_LEVEL * TSC_SATURATION_LOW_BAND_PERCENT / 100)

enum crbg_index {
	CLEAR_CRGB_IDX = 0,
	RED_CRGB_IDX,
	GREEN_CRGB_IDX,
	BLUE_CRGB_IDX,
	WIDE_BAND_IDX,
	FLICKER_IDX,
	CRGB_COUNT,
};

#if defined(CONFIG_ZEPHYR)
#if DT_NODE_EXISTS(DT_ALIAS(tcs3408_int))
/*
 * Get the mostion sensor ID of the TCS3408 sensor that
 * generates the interrupt.
 * The interrupt is converted to the event and transferred to motion
 * sense task that actually handles the interrupt.
 *
 * Here, we use alias to get the motion sensor ID
 *
 * e.g) als_clear below is the label of a child node in /motionsense-sensors
 * aliases {
 *     tcs3408-int = &als_clear;
 * };
 */
#define CONFIG_ALS_TCS3408_INT_EVENT \
	TASK_EVENT_MOTION_SENSOR_INTERRUPT(SENSOR_ID(DT_ALIAS(tcs3408_int)))
#endif
#endif /* CONFIG_ZEPHYR */

#endif /* __CROS_EC_ALS_TCS3408_H */
