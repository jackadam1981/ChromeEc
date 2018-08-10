#include "common.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/accel_bma2x2.h"
#include "driver/accel_kionix.h"
#include "hooks.h"
#include "motion_sense.h"

/* Lid Sensor mutex */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/* Lid accel private data */
static struct bmi160_drv_data_t g_bmi160_data;
static struct kionix_accel_data g_kx022_data;

/* BMA255 private data */
static struct accelgyro_saved_data_t g_bma255_data;

/* Matrix to rotate accelrator into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1), 0},
	{ FLOAT_TO_FP(1), 0, 0},
	{ 0, 0, FLOAT_TO_FP(1)}
};

const matrix_3x3_t lid_standard_ref = {
	{ FLOAT_TO_FP(1), 0, 0},
	{ 0, FLOAT_TO_FP(-1), 0},
	{ 0, 0, FLOAT_TO_FP(-1)}
};

const matrix_3x3_t rotation_x180_z90 = {
	{ 0, FLOAT_TO_FP(-1), 0 },
	{ FLOAT_TO_FP(-1), 0, 0 },
	{ 0, 0, FLOAT_TO_FP(-1) }
};

const struct motion_sensor_t lid_accel_1 = {
	.name = "Lid Accel",
	.active_mask = SENSOR_ACTIVE_S0_S3,
	.chip = MOTIONSENSE_CHIP_KX022,
	.type = MOTIONSENSE_TYPE_ACCEL,
	.location = MOTIONSENSE_LOC_LID,
	.drv = &kionix_accel_drv,
	.mutex = &g_lid_mutex,
	.drv_data = &g_kx022_data,
	.port = I2C_PORT_ACCEL,
	.addr = KX022_ADDR1,
	.rot_standard_ref = &rotation_x180_z90,
	.min_frequency = KX022_ACCEL_MIN_FREQ,
	.max_frequency = KX022_ACCEL_MAX_FREQ,
	.default_range = 2, /* g, to support tablet mode */
	.config = {
		/* EC use accel for angle detection */
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 10000 | ROUND_UP_FLAG,
		},
		/* Sensor on in S3 */
		[SENSOR_CONFIG_EC_S3] = {
			.odr = 10000 | ROUND_UP_FLAG,
		},
	},
};

struct motion_sensor_t motion_sensors[] = {
	[LID_ACCEL] = {
		.name = "Lid Accel",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_BMA255,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bma2x2_accel_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bma255_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMA2x2_I2C_ADDR1,
		.rot_standard_ref = &lid_standard_ref,
		.min_frequency = BMA255_ACCEL_MIN_FREQ,
		.max_frequency = BMA255_ACCEL_MAX_FREQ,
		.default_range = 2, /* g, to support tablet mode */
		.config = {
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 0,
			},
			/* Sensor on in S3 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 0,
			},
		},
	},
	[BASE_ACCEL] = {
		.name = "Base Accel",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_BMI160,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &bmi160_drv,
		.mutex = &g_base_mutex,
		.drv_data = &g_bmi160_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMI160_ADDR0,
		.rot_standard_ref = &base_standard_ref,
		.min_frequency = BMI160_ACCEL_MIN_FREQ,
		.max_frequency = BMI160_ACCEL_MAX_FREQ,
		.default_range = 2, /* g, to support tablet mode  */
		.config = {
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* Sensor on in S3 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 0,
			},
		},
	},
	[BASE_GYRO] = {
		.name = "Base Gyro",
		.active_mask = SENSOR_ACTIVE_S0_S3,
		.chip = MOTIONSENSE_CHIP_BMI160,
		.type = MOTIONSENSE_TYPE_GYRO,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &bmi160_drv,
		.mutex = &g_base_mutex,
		.drv_data = &g_bmi160_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMI160_ADDR0,
		.default_range = 1000, /* dps */
		.rot_standard_ref = &base_standard_ref,
		.min_frequency = BMI160_GYRO_MIN_FREQ,
		.max_frequency = BMI160_GYRO_MAX_FREQ,
	},
};
unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

static void setup_motion_sensors(void)
{
	switch (oem) {
	case PROJECT_AKALI:
		if (sku & SKU_ID_MASK_CONVERTIBLE) {
			/* Rotate axis for Akali 360 */
			motion_sensors[LID_ACCEL] = lid_accel_1;
			motion_sensors[BASE_ACCEL].rot_standard_ref = NULL;
			motion_sensors[BASE_GYRO].rot_standard_ref = NULL;
		} else {
			/* Clamshell Akali has no accel/gyro */
			motion_sensor_count = ARRAY_SIZE(motion_sensors) - 2;
		}
		break;
	default:
		break;
	}

	/* Enable Accel/Gyro interrupt for convertibles. */
	if (sku & SKU_ID_MASK_CONVERTIBLE)
		gpio_enable_interrupt(GPIO_ACCELGYRO3_INT_L);
}
DECLARE_HOOK(HOOK_INIT, setup_motion_sensors, HOOK_PRIO_DEFAULT);
