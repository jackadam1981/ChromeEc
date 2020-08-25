/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* nucleo-f411re development board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
<<<<<<< HEAD   (6dbd10 chgramp: Don't ramp DTS suppliers above advertisement)
#include "driver/accelgyro_bmi160.h"
=======
#include "driver/accelgyro_icm_common.h"
#include "driver/accelgyro_icm426xx.h"
>>>>>>> CHANGE (a4b8af board: nucleo-f411re: create icm426xx development platform)
#include "ec_version.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "spi.h"
#include "motion_sense.h"

#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "util.h"

void user_button_evt(enum gpio_signal signal)
{
	ccprintf("Button %d, %d!\n", signal, gpio_get_level(signal));
}

#include "gpio_list.h"

/* Initialize board. */
static void board_init(void)
{
	gpio_enable_interrupt(GPIO_USER_BUTTON_L);
	gpio_enable_interrupt(GPIO_ICM426XX_INT1_L);

	/* No power control yet */
	/* Go to S3 state */
	hook_notify(HOOK_CHIPSET_STARTUP);

	/* Go to S0 state */
	hook_notify(HOOK_CHIPSET_RESUME);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_LAST);

/* ADC channels */
const struct adc_t adc_channels[] = {
	/* Arduino connectors analog pins */
	[ADC1_0] = {"ADC1_0",  3000, 4096, 0, STM32_AIN(0)},
	[ADC1_1] = {"ADC1_1",  3000, 4096, 0, STM32_AIN(1)},
	[ADC1_4] = {"ADC1_4",  3000, 4096, 0, STM32_AIN(4)},
	[ADC1_8] = {"ADC1_8",  3000, 4096, 0, STM32_AIN(8)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 400,
	 GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef CONFIG_SPI_ACCEL_PORT

/* SPI ports */
enum {
	SPI_PORT_ACCEL,
};
const struct spi_device_t spi_devices[] = {
	[SPI_PORT_ACCEL] = { CONFIG_SPI_ACCEL_PORT, 1, GPIO_SPI_ACCEL_CS_L },
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

#endif

/* Base Sensor mutex */
static struct mutex g_base_mutex;

<<<<<<< HEAD   (6dbd10 chgramp: Don't ramp DTS suppliers above advertisement)
static struct bmi160_drv_data_t g_bmi160_data;
=======
static struct icm_drv_data_t g_icm426xx_data;
>>>>>>> CHANGE (a4b8af board: nucleo-f411re: create icm426xx development platform)

struct motion_sensor_t motion_sensors[] = {
	[BASE_ACCEL] = {
	 .name = "Base Accel",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_ICM426XX,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &icm426xx_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_icm426xx_data,
#ifdef CONFIG_SPI_ACCEL_PORT
	 .port = CONFIG_SPI_ACCEL_PORT,
	 .i2c_spi_addr_flags = SLAVE_MK_SPI_ADDR_FLAGS(SPI_PORT_ACCEL),
#else
	 .port = I2C_PORT_ACCEL,
<<<<<<< HEAD   (6dbd10 chgramp: Don't ramp DTS suppliers above advertisement)
	 .addr = BMI160_ADDR0,
	 .rot_standard_ref = NULL,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		 },
		 /* Sensor on for lid angle detection */
		 [SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
			.ec_rate = 100 * MSEC,
		 },
	 },
=======
	 .i2c_spi_addr_flags = SLAVE_MK_I2C_ADDR_FLAGS(ICM426XX_ADDR0_FLAGS),
#endif
	 .default_range = 4,  /* g, to meet CDD 7.3.1/C-1-4 reqs */
	 .min_frequency = ICM426XX_ACCEL_MIN_FREQ,
	 .max_frequency = ICM426XX_ACCEL_MAX_FREQ,
>>>>>>> CHANGE (a4b8af board: nucleo-f411re: create icm426xx development platform)
	},

	[BASE_GYRO] = {
	 .name = "Base Gyro",
	 .active_mask = SENSOR_ACTIVE_S0_S3,
	 .chip = MOTIONSENSE_CHIP_ICM426XX,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &icm426xx_drv,
	 .mutex = &g_base_mutex,
	 .drv_data = &g_icm426xx_data,
#ifdef CONFIG_SPI_ACCEL_PORT
	 .port = CONFIG_SPI_ACCEL_PORT,
	 .i2c_spi_addr_flags = SLAVE_MK_SPI_ADDR_FLAGS(SPI_PORT_ACCEL),
#else
	 .port = I2C_PORT_ACCEL,
<<<<<<< HEAD   (6dbd10 chgramp: Don't ramp DTS suppliers above advertisement)
	 .addr = BMI160_ADDR0,
=======
	 .i2c_spi_addr_flags = SLAVE_MK_I2C_ADDR_FLAGS(ICM426XX_ADDR0_FLAGS),
#endif
>>>>>>> CHANGE (a4b8af board: nucleo-f411re: create icm426xx development platform)
	 .default_range = 1000, /* dps */
<<<<<<< HEAD   (6dbd10 chgramp: Don't ramp DTS suppliers above advertisement)
	 .rot_standard_ref = NULL,
=======
	 .min_frequency = ICM426XX_GYRO_MIN_FREQ,
	 .max_frequency = ICM426XX_GYRO_MAX_FREQ,
>>>>>>> CHANGE (a4b8af board: nucleo-f411re: create icm426xx development platform)
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

#ifdef CONFIG_DMA_HELP
#include "dma.h"
int command_dma_help(int argc, char **argv)
{
	dma_dump(STM32_DMA2_STREAM0);
	dma_test(STM32_DMA2_STREAM0);
	dma_dump(STM32_DMA2_STREAM0);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dmahelp, command_dma_help,
			NULL, "Run DMA test");
#endif
