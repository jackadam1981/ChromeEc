/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "accelgyro.h"
#include "hooks.h"
#include "keyboard_scan.h"

#define SENSOR_MUTEX_NODE		DT_PATH(motionsense_mutex)
#define SENSOR_DATA_NODE		DT_PATH(motionsense_sensor_data)
#define SENSOR_ROT_REF_NODE		DT_PATH(motionsense_rotation_ref)

/* Get&Assign mutex if available */
#define SENSOR_MUTEX_NAME(mutex_id)	DT_CAT(MUTEX_, mutex_id)
#define SENSOR_MUTEX(id)						\
	IF_ENABLED(DT_NODE_HAS_PROP(id, mutex),				\
		(.mutex = &SENSOR_MUTEX_NAME(DT_PHANDLE(id, mutex)),))

/* Get&Assign i2c port information if available */
#define SENSOR_I2C_PORT(id)						\
	IF_ENABLED(DT_NODE_HAS_PROP(id, port),				\
		(.port = I2C_PORT(DT_PHANDLE(id, port)),))

/* Get&Assign i2c address(or SPI) information if available */
#define SENSOR_I2C_SPI_ADDR_FLAGS(id)					\
	IF_ENABLED(DT_NODE_HAS_PROP(id, i2c_spi_addr_flags),		\
		(.i2c_spi_addr_flags = DT_ENUM_TOKEN(id, i2c_spi_addr_flags),))

/* Get&Assign rotation matrix if available */
#define SENSOR_ROT_STD_REF_NAME(id)	DT_CAT(ROT_REF_, id)
#define SENSOR_ROT_STD_REF(id)						\
	IF_ENABLED(DT_NODE_HAS_PROP(id, rot_standard_ref),		\
		(.rot_standard_ref =					\
		 &SENSOR_ROT_STD_REF_NAME(DT_PHANDLE(id, rot_standard_ref)),))

/* Get&Assign driver specific data if available */
#define SENSOR_DATA_NAME(id)		DT_CAT(SENSOR_DAT_, id)
#define SENSOR_DRV_DATA(id)						\
	IF_ENABLED(DT_NODE_HAS_PROP(id, drv_data),			\
		   (.drv_data = &SENSOR_DATA_NAME(DT_PHANDLE(id, drv_data)),))

#define SET_CONFIG_EC(cfg_id, cfg_suffix)				\
	[SENSOR_CONFIG_##cfg_suffix] = {				\
		IF_ENABLED(DT_NODE_HAS_PROP(cfg_id, odr),		\
		   (.odr = DT_PROP(cfg_id, odr),))			\
		IF_ENABLED(DT_NODE_HAS_PROP(cfg_id, ec_rate),		\
		   (.ec_rate = DT_PROP(cfg_id, ec_rate),))		\
	}

/* Get&Assign sensor configs if available */
#define CREATE_SENSOR_CONFIG(cfgs_id)					      \
	.config = {							      \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(cfgs_id, ap)),	      \
			   (SET_CONFIG_EC(DT_CHILD(cfgs_id, ap), AP),))       \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(cfgs_id, ec_s0)),	      \
			   (SET_CONFIG_EC(DT_CHILD(cfgs_id, ec_s0), EC_S0),)) \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(cfgs_id, ec_s3)),	      \
			   (SET_CONFIG_EC(DT_CHILD(cfgs_id, ec_s3), EC_S3),)) \
		IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(cfgs_id, ec_s5)),	      \
			   (SET_CONFIG_EC(DT_CHILD(cfgs_id, ec_s5), EC_S5),)) \
	}

#define SENSOR_CONFIG(id)						\
	IF_ENABLED(DT_NODE_EXISTS(DT_CHILD(id, configs)),		\
		   (CREATE_SENSOR_CONFIG(DT_CHILD(id, configs)),))

#define SENSOR_BASIC_INFO(id)						\
	.name = DT_LABEL(id),						\
	.active_mask = DT_ENUM_TOKEN(id, active_mask),			\
	.location = DT_ENUM_TOKEN(id, location),			\
	.default_range = DT_PROP(id, default_range),			\
	SENSOR_I2C_SPI_ADDR_FLAGS(id)					\
	SENSOR_MUTEX(id)						\
	SENSOR_I2C_PORT(id)						\
	SENSOR_ROT_STD_REF(id)						\
	SENSOR_DRV_DATA(id)						\
	SENSOR_CONFIG(id)

#if DT_NODE_EXISTS(SENSOR_MUTEX_NODE)
#define DECLARE_SENSOR_MUTEX(id)	static mutex_t SENSOR_MUTEX_NAME(id);
#define INIT_SENSOR_MUTEX(id)		k_mutex_init(&SENSOR_MUTEX_NAME(id));

/* Declare mutexes */
DT_FOREACH_CHILD(SENSOR_MUTEX_NODE, DECLARE_SENSOR_MUTEX)

/* Initialize mutexes */
static int init_sensor_mutex(const struct device *dev)
{
	ARG_UNUSED(dev);

	DT_FOREACH_CHILD(SENSOR_MUTEX_NODE, INIT_SENSOR_MUTEX)

	return 0;
}
SYS_INIT(init_sensor_mutex, POST_KERNEL, 50);
#endif /* DT_NODE_EXISTS(SENSOR_MUTEX_NODE) */

/* Get rotation matrix if available */
#define MAT_ITEM(i, id)	FLOAT_TO_FP((int32_t)(DT_PROP_BY_IDX(id, mat33, i)))
#define DECLARE_SENSOR_ROT_REF(id)					\
	IF_ENABLED(DT_NODE_HAS_PROP(id, mat33),				\
	(static const mat33_fp_t SENSOR_ROT_STD_REF_NAME(id) = {	\
		{							\
			FOR_EACH_FIXED_ARG(MAT_ITEM, (,), id, 0, 1, 2)	\
		},							\
		{							\
			FOR_EACH_FIXED_ARG(MAT_ITEM, (,), id, 3, 4, 5)	\
		},							\
		{							\
			FOR_EACH_FIXED_ARG(MAT_ITEM, (,), id, 6, 7, 8)	\
		},							\
	};))

/* Declare rotation matrix */
#if DT_NODE_EXISTS(SENSOR_ROT_REF_NODE)
DT_FOREACH_CHILD(SENSOR_ROT_REF_NODE, DECLARE_SENSOR_ROT_REF)
#endif

/*
 * Here, we declare all driver specific data which are defined in
 * drvdata-<chip>.inc ,in turn, included in sensor_drvdata_list.inc.
 */
#if DT_NODE_EXISTS(SENSOR_NODE)
#include "motionsense_driver/sensor_drvdata_list.inc"
#endif

#define DO_MK_SENSOR_ENTRY(						\
		id, s_chip, s_type, s_drv, s_min_freq, s_max_freq)	\
	[SENSOR_ID(id)] = {						\
		SENSOR_BASIC_INFO(id)					\
		.chip = s_chip,						\
		.type = s_type,						\
		.drv = &s_drv,						\
		.min_frequency = s_min_freq,				\
		.max_frequency = s_max_freq				\
	},

#define MK_SENSOR_ENTRY(inst, s_compat, s_chip, s_type, s_drv,		\
		s_min_freq, s_max_freq)					\
	DO_MK_SENSOR_ENTRY(DT_INST(inst, s_compat),			\
		s_chip, s_type, s_drv, s_min_freq, s_max_freq)

/*
 * Motion sensor entry creation.
 * A motion sensor driver should create <chip>-drvinfo.inc and should have
 * CREATE_MOTION_SENSOR() in the .inc.

 * CREATE_MOTION_SENSOR() is used by every motion sensor driver
 * to create the corresponding motion_sensor_t entry in motion_sensors array.
 */
#define CREATE_MOTION_SENSOR(s_compat, s_chip, s_type, s_drv,		\
		s_min_freq, s_max_freq)					\
	UTIL_LISTIFY(DT_NUM_INST_STATUS_OKAY(s_compat), MK_SENSOR_ENTRY,\
		s_compat, s_chip, s_type, s_drv, s_min_freq, s_max_freq)

/*
 * Here, we declare all motion sensors with help from sensor specific
 * <chip>-drvinfo.inc.
 */
struct motion_sensor_t motion_sensors[] = {
#if DT_NODE_EXISTS(SENSOR_NODE)
#include "motionsense_driver/sensor_drv_list.inc"
#endif
};

#ifdef CONFIG_DYNAMIC_MOTION_SENSOR_COUNT
unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);
#else
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);
#endif

/* Create a list of ALS sensors needed by motion sense */
#if DT_NODE_HAS_PROP(SENSOR_INFO_NODE, als_sensors)
#define ALS_SENSOR_ENTRY_WITH_COMMA(i, id)		\
	&motion_sensors[SENSOR_ID(DT_PHANDLE_BY_IDX(id, als_sensors, i))],
/* ALS instances when LPC mapping is needed. Each entry directs to a sensor. */
const struct motion_sensor_t *motion_als_sensors[] = {
	UTIL_LISTIFY(DT_PROP_LEN(SENSOR_INFO_NODE, als_sensors),
		     ALS_SENSOR_ENTRY_WITH_COMMA, SENSOR_INFO_NODE)
};
BUILD_ASSERT(ARRAY_SIZE(motion_als_sensors) == ALS_COUNT);
#endif

/* Enable interrupts for motion sensors */
#if DT_NODE_HAS_PROP(SENSOR_INFO_NODE, sensor_irqs)
#define SENSOR_GPIO_ENABLE_INTERRUPT(i, id)		\
	gpio_enable_interrupt(				\
		GPIO_SIGNAL(DT_PHANDLE_BY_IDX(id, sensor_irqs, i)));
static void sensor_enable_irqs(void)
{
	UTIL_LISTIFY(DT_PROP_LEN(SENSOR_INFO_NODE, sensor_irqs),
		     SENSOR_GPIO_ENABLE_INTERRUPT, SENSOR_INFO_NODE)
}
DECLARE_HOOK(HOOK_INIT, sensor_enable_irqs, HOOK_PRIO_DEFAULT);
#endif
