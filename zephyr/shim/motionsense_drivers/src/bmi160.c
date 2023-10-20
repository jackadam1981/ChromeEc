/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef __REQUIRE_ZEPHYR_GPIOS__
#undef __REQUIRE_ZEPHYR_GPIOS__
#endif
#include "accelgyro_bmi160_public.h"
#include "accelgyro_bmi_common_public.h"
#include "errno_map.h"

#include <string.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(sensor_shim, CONFIG_SENSOR_LOG_LEVEL);

void bmi160_interrupt(enum gpio_signal signal)
{
	ARG_UNUSED(signal);
}

static int init(struct motion_sensor_t *s)
{
	return sensor_init_done(s);
}

#define ACCEL_TO_MILLI(value) (((int64_t)(value) * 1e8) / 980665)
#define GYRO_TO_MILLI(value) (((int64_t)(value) * 1e9) / 17453)

static int q31_to_milli(q31_t value, int8_t shift, bool is_accel)
{
	int64_t intermediate = is_accel ? ACCEL_TO_MILLI(value) :
					  GYRO_TO_MILLI(value);

	if (shift > 0) {
		intermediate = intermediate << shift;
	} else if (shift < 0) {
		intermediate = intermediate >> -shift;
	}

	printk("intermediate = %" PRIi64 "\n", intermediate);

	return intermediate >> 31;
}

static int bmi160_set_range(struct motion_sensor_t *s, int range, int rnd)
{
	struct bmi_drv_data_t *data = s->drv_data;
	enum sensor_channel channel;
	struct sensor_value value;
	struct sensor_value actual_value;
	int64_t actual_range;
	int rc;

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		channel = SENSOR_CHAN_ACCEL_XYZ;

		/* Convert range from g to um/s^2 */
		range *= SENSOR_G;
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		channel = SENSOR_CHAN_GYRO_XYZ;

		/* Convert range from deg/s to urad/s*/
		range = range * SENSOR_PI / 180;
	} else {
		return EC_ERROR_INVAL;
	}

	const int64_t requested_range = range;
	int tries_remaining = 5;

	while (tries_remaining-- > 0) {
		value.val1 = range / 1000000;
		value.val2 = range % 1000000;

		rc = sensor_attr_set(data->dev, channel, SENSOR_ATTR_FULL_SCALE,
				     &value);

		if (rc != 0) {
			if (rc == -EINVAL && rnd == 0) {
				if (channel == SENSOR_CHAN_ACCEL_XYZ &&
				    range - SENSOR_G > 0) {
					range -= SENSOR_G;
					continue;
				}
				if (channel == SENSOR_CHAN_GYRO_XYZ &&
				    range - SENSOR_PI > 0) {
					range -= SENSOR_PI;
					continue;
				}
			}
			return errno_to_ec(rc);
		}

		rc = sensor_attr_get(data->dev, channel, SENSOR_ATTR_FULL_SCALE,
				     &actual_value);

		if (rc != 0) {
			return errno_to_ec(rc);
		}

		actual_range = sensor_value_to_micro(&actual_value);

		if (rnd == 0) {
			/* Round down, actual_value must be <= */
			if (actual_range <= requested_range) {
				return EC_SUCCESS;
			}
			/* Driver failed to set the range to the expected one,
			 * try a lower range by 10%
			 */
			range = range * 9 / 10;
		} else {
			/* Round up, actual_value must be >= */
			if (requested_range <= actual_range) {
				return EC_SUCCESS;
			}
			/* Driver failed to st the range to the expected one,
			 * try a bigger range by 10%
			 */
			range = range * 11 / 10;
		}
	}

	return EC_ERROR_INVAL;
}

static int bmi160_set_data_rate(const struct motion_sensor_t *s, int rate,
				int rnd)
{
	struct bmi_drv_data_t *data = s->drv_data;
	enum sensor_channel channel;
	struct sensor_value value;
	struct sensor_value actual_value;
	const int requested_rate = rate;
	int actual_rate;
	int rc;

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		channel = SENSOR_CHAN_ACCEL_XYZ;
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		channel = SENSOR_CHAN_GYRO_XYZ;
	} else {
		return EC_ERROR_INVAL;
	}

	int tries_remaining = 5;

	while (tries_remaining-- > 0) {
		value.val1 = rate / 1000;
		value.val2 = (rate % 1000) * 1000;

		rc = sensor_attr_set(data->dev, channel,
				     SENSOR_ATTR_SAMPLING_FREQUENCY, &value);

		if (rc != 0) {
			if (rc == -EINVAL && rnd == 0 && (rate % 1000) != 0) {
				rate = (rate / 1000) * 1000;
				continue;
			}
			return errno_to_ec(rc);
		}

		rc = sensor_attr_get(data->dev, channel,
				     SENSOR_ATTR_SAMPLING_FREQUENCY,
				     &actual_value);

		if (rc != 0) {
			return errno_to_ec(rc);
		}

		actual_rate =
			(actual_value.val1 * 1000) + (actual_value.val2 / 1000);

		if (rnd == 0) {
			/* Round down, actual_value must be <= */
			if (actual_rate <= requested_rate) {
				return EC_SUCCESS;
			}
			/* Driver failed to set the ODR to the expected one, try
			 * a lower ODR by 10%
			 */
			rate = rate * 9 / 10;
		} else {
			/* Round up, actual_value must be >= */
			if (requested_rate <= actual_rate) {
				return EC_SUCCESS;
			}
			/* Driver failed to st the ODR to the expected one, try
			 * a bigger ODR by 10%
			 */
			rate = rate * 11 / 10;
		}
	}

	return EC_ERROR_INVAL;
}

static int bmi160_get_data_rate(const struct motion_sensor_t *s)
{
	struct bmi_drv_data_t *data = s->drv_data;
	enum sensor_channel channel;
	struct sensor_value value;

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		channel = SENSOR_CHAN_ACCEL_XYZ;
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		channel = SENSOR_CHAN_GYRO_XYZ;
	} else {
		return EC_ERROR_INVAL;
	}

	int rc = sensor_attr_get(data->dev, channel,
				 SENSOR_ATTR_SAMPLING_FREQUENCY, &value);

	if (rc != 0) {
		LOG_ERR("Failed to get data rate");
		return 0;
	}

	return (value.val1 * 1000) + (value.val2 / 1000);
}

static int bmi160_get_offset(const struct motion_sensor_t *s, int16_t *offset,
			     int16_t *temp)
{
	struct bmi_drv_data_t *data = s->drv_data;
	enum sensor_channel channel;
	struct sensor_value values[3];
	intv3_t v;
	int rc;

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		channel = SENSOR_CHAN_ACCEL_XYZ;
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		channel = SENSOR_CHAN_GYRO_XYZ;
	} else {
		return EC_ERROR_INVAL;
	}

	rc = sensor_attr_get(data->dev, channel, SENSOR_ATTR_OFFSET, values);
	if (rc != 0) {
		return errno_to_ec(rc);
	}

	/* Convert mps2 -> g and rad/s to deg/s */
	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		for (int i = 0; i < 3; ++i) {
			v[i] = CLAMP(sensor_ms2_to_ug(&values[i]) / 1000,
				     INT16_MIN, INT16_MAX);
		}
	} else {
		for (int i = 0; i < 3; ++i) {
			v[i] = CLAMP(sensor_rad_to_10udegrees(&values[i]) / 100,
				     INT16_MIN, INT16_MAX);
		}
	}

	rotate(v, *s->rot_standard_ref, v);
	offset[0] = v[0];
	offset[1] = v[1];
	offset[2] = v[2];

	/* See if the die temperature is available */
	rc = sensor_attr_get(data->dev, SENSOR_CHAN_DIE_TEMP,
			     SENSOR_ATTR_OFFSET, values);
	if (rc == 0) {
		*temp = CLAMP(values[0].val1, INT16_MIN, INT16_MAX);
	} else {
		*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	}
	return 0;
}

static int bmi160_set_offset(const struct motion_sensor_t *s,
			     const int16_t *offset, int16_t temp)
{
	struct bmi_drv_data_t *data = s->drv_data;
	enum sensor_channel channel;
	struct sensor_value values[3];
	intv3_t v = { offset[0], offset[1], offset[2] };
	int rc;

	rotate_inv(v, *s->rot_standard_ref, v);

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		channel = SENSOR_CHAN_ACCEL_XYZ;
		/* offset is in mg, we need m/s^2 for Zephyr */
		for (int i = 0; i < 3; ++i) {
			sensor_ug_to_ms2((int32_t)v[i] * INT32_C(1000),
					 &values[i]);
		}
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		channel = SENSOR_CHAN_GYRO_XYZ;
		/* offset is in mdeg/s, we need deg/s for Zephyr */
		for (int i = 0; i < 3; ++i) {
			sensor_10udegrees_to_rad((int32_t)v[i] * INT32_C(100),
						 &values[i]);
		}
	} else {
		return EC_ERROR_INVAL;
	}

	rc = sensor_attr_set(data->dev, channel, SENSOR_ATTR_OFFSET, values);
	if (rc != 0) {
		return errno_to_ec(rc);
	}

	values[0].val1 = temp;
	values[0].val2 = 0;
	sensor_attr_set(data->dev, SENSOR_CHAN_DIE_TEMP, SENSOR_ATTR_OFFSET,
			values);

	return 0;
}

static int bmi160_get_resolution(const struct motion_sensor_t *s)
{
	ARG_UNUSED(s);
	return 16;
}

static int bmi160_set_scale(const struct motion_sensor_t *s,
			    const uint16_t *scale, int16_t temp)
{
	struct bmi_drv_data_t *data = s->drv_data;
	struct accelgyro_saved_data_t *saved_data = &data->saved_data[s->type];

	ARG_UNUSED(temp);
	saved_data->scale[0] = scale[0];
	saved_data->scale[1] = scale[1];
	saved_data->scale[2] = scale[2];
	return EC_SUCCESS;
}

static int bmi160_get_scale(const struct motion_sensor_t *s, uint16_t *scale,
			    int16_t *temp)
{
	const struct bmi_drv_data_t *data = s->drv_data;
	const struct accelgyro_saved_data_t *saved_data =
		&data->saved_data[s->type];

	scale[0] = saved_data->scale[0];
	scale[1] = saved_data->scale[1];
	scale[2] = saved_data->scale[2];
	*temp = EC_MOTION_SENSE_INVALID_CALIB_TEMP;
	return EC_SUCCESS;
}

RTIO_DEFINE_WITH_MEMPOOL(single_read_rtio_context, 8, 8, 8, 16, 4);

static int bmi160_read(const struct motion_sensor_t *s, intv3_t out)
{
	const struct sensor_decoder_api *decoder;
	struct sensor_read_config *read_config = s->iodev->data;
	bool is_accel;

	if (s->type == MOTIONSENSE_TYPE_ACCEL) {
		read_config->channels[0] = SENSOR_CHAN_ACCEL_XYZ;
		is_accel = true;
	} else if (s->type == MOTIONSENSE_TYPE_GYRO) {
		read_config->channels[0] = SENSOR_CHAN_GYRO_XYZ;
		is_accel = false;
	} else {
		return errno_to_ec(-ENOTSUP);
	}

	int rc = sensor_read(s->iodev, &single_read_rtio_context, NULL);

	if (rc != 0) {
		return errno_to_ec(rc);
	}

	/* Block waiting on response */
	struct rtio_cqe *cqe =
		rtio_cqe_consume_block(&single_read_rtio_context);
	uint8_t *buf = NULL;
	uint32_t buf_len = 0;
	uint16_t frame_count;
	uint32_t fit = 0;
	struct sensor_three_axis_data data;

	rc = cqe->result;
	rtio_cqe_get_mempool_buffer(&single_read_rtio_context, cqe, &buf,
				    &buf_len);

	/* Release the CQE */
	rtio_cqe_release(&single_read_rtio_context, cqe);

	/* Process data */
	//	if (s->decoder == NULL) {
	rc = sensor_get_decoder(s->dev, &decoder);
	if (rc != 0) {
		goto end;
	}
	//	}

	__ASSERT_NO_MSG(decoder->get_frame_count(buf, read_config->channels[0],
						 0, &frame_count) == 0);
	__ASSERT_NO_MSG(frame_count == 1);

	__ASSERT_NO_MSG(decoder->decode(buf, read_config->channels[0], 0, &fit,
					1, &data) == 1);

	printk("[x] %d << %d\n", data.readings[0].x, data.shift);
	out[0] = q31_to_milli(data.readings[0].x, data.shift, is_accel);
	printk("[y] %d << %d\n", data.readings[0].y, data.shift);
	out[1] = q31_to_milli(data.readings[0].y, data.shift, is_accel);
	printk("[z] %d << %d\n", data.readings[0].z, data.shift);
	out[2] = q31_to_milli(data.readings[0].z, data.shift, is_accel);

end:
	/* Release the memory */
	rtio_release_buffer(&single_read_rtio_context, buf, buf_len);

	if (rc == 0) {
		rotate(out, *s->rot_standard_ref, out);
	}

	return rc;
}

const struct accelgyro_drv bmi160_drv = {
	.init = init,
	.set_range = bmi160_set_range,
	.set_data_rate = bmi160_set_data_rate,
	.get_data_rate = bmi160_get_data_rate,
	.get_offset = bmi160_get_offset,
	.set_offset = bmi160_set_offset,
	.get_resolution = bmi160_get_resolution,
	.set_scale = bmi160_set_scale,
	.get_scale = bmi160_get_scale,
	.read = bmi160_read,
};
