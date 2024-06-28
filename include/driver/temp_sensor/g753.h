/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* G753 temperature sensor module for Chrome EC */

#ifndef __CROS_EC_G753_H
#define __CROS_EC_G753_H

/*
G754
PRODUCT TWO-WIRE-ADDRESS TEMPERATURE-ZONE
G754A      1110000           Zone1
G754B      1110001           Zone2
G754C      1110010           Zone3
G754D      1110011           Zone4
G754E      1110100           Zone5
G754F      1110101           Zone6
G754G      1110110           Zone7
G754H      1110111           Zone8
 */

#ifdef CONFIG_TEMP_SENSOR_G754B
#define G75x_I2C_ADDR_FLAGS 0x71
#else
#define G75x_I2C_ADDR_FLAGS 0x48
#endif

#define G753_IDX_INTERNAL 0

/* G753 register */
#define G753_TEMP_LOCAL 0x00
#define G753_STATUS 0x02
#define G753_CONFIGURATION_R 0x03
#define G753_CONVERSION_RATE_R 0x04
#define G753_LOCAL_TEMP_HIGH_LIMIT_R 0x05
#define G753_CONFIGURATION_W 0x09
#define G753_CONVERSION_RATE_W 0x0A
#define G753_LOCAL_TEMP_HIGH_LIMIT_W 0x0B
#define G753_ONESHOT 0x0F
#define G753_Customer_Data_Log_Register_1 0x2D
#define G753_Customer_Data_Log_Register_2 0x2E
#define G753_Customer_Data_Log_Register_3 0x2F
#define G753_ALERT_MODE 0xBF
#define G753_CHIP_ID 0xFD
#define G753_VENDOR_ID 0xFE
#define G753_DEVICE_ID 0xFF

/* Config register bits */
#define G753_CONFIGURATION_STANDBY BIT(6)
#define G753_CONFIGURATION_ALERT_MASK BIT(7)

/* Status register bits */
#define G753_STATUS_LOCAL_TEMP_HIGH_ALARM BIT(6)
#define G753_STATUS_BUSY BIT(7)

struct g753_sensor_t {
	int i2c_port;
	int i2c_addr_flags;
};

extern const struct g753_sensor_t g753_sensors[];

/**
 * Get the last polled value of a sensor.
 *
 * @param idx		Index to read. Idx indicates whether to read die
 *			temperature or external temperature.
 * @param temp_ptr	Destination for temperature in K.
 *
 * @return EC_SUCCESS if successful, non-zero if error.
 */
int g753_get_val(int idx, int *temp_ptr);

#ifdef CONFIG_ZEPHYR
void g753_update_temperature(int idx);
int g753_get_val_k(int idx, int *temp_mk_ptr);
#endif /* CONFIG_ZEPHYR */

#endif /* __CROS_EC_G753_H */
