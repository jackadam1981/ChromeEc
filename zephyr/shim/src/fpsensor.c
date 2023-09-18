/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/cros_fingerprint.h>
#include <fingerprint/fingerprint_alg.h>
#include <fpsensor.h>
#include <fpsensor_detect.h>
#include <fpsensor_state.h>
#include <task.h>

#if !DT_HAS_CHOSEN(cros_fp_fingerprint_sensor)
#error "cros-fp,fingerprint-sensor device must be chosen"
#else
#define fp_sensor_dev DEVICE_DT_GET(DT_CHOSEN(cros_fp_fingerprint_sensor))
#endif

enum fp_sensor_type fpsensor_detect_get_type(void)
{
	enum fp_sensor_type ret;

	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(div_highside), 1);
	k_usleep(1);
	switch (gpio_pin_get_dt(
		GPIO_DT_FROM_NODELABEL(fp_sensor_sel))) {
	case 0:
		ret = FP_SENSOR_TYPE_ELAN;
		break;
	case 1:
		ret = FP_SENSOR_TYPE_FPC;
		break;
	default:
		ret = FP_SENSOR_TYPE_UNKNOWN;
		break;
	}
	/* We leave GPIO_DIVIDER_HIGHSIDE enabled, since the dragonclaw
	 * development board use it to enable the AND gate (U10) to CS.
	 * Production boards could disable this to save power since it's
	 * only needed for initial detection on those boards.
	 */
	return ret;
}

static void fp_sensor_irq(const struct device *dev, uint32_t event)
{
	task_set_event(TASK_ID_FPSENSOR, TASK_EVENT_SENSOR_IRQ);
}

int fp_sensor_init(void)
{
	int rc;

	rc = fingerprint_init(fp_sensor_dev);
	if (rc) {
		return rc;
	}

	rc = fingerprint_algorithm_init();
	if (rc) {
		return rc;
	}

	rc = fingerprint_config(fp_sensor_dev, fp_sensor_irq);
	if (rc) {
		return rc;
	}

	return 0;
}

int fp_sensor_deinit(void)
{
	int rc;

	rc = fingerprint_algorithm_exit();
	if (rc) {
		return rc;
	}

	rc = fingerprint_deinit(fp_sensor_dev);
	if (rc) {
		return rc;
	}

	return 0;
}

int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	struct fingerprint_info info;
	int rc;

	rc = fingerprint_get_info(fp_sensor_dev, &info);
	if (rc) {
		return rc;
	}

	resp->vendor_id = info.vendor_id;
	resp->product_id = info.product_id;
	resp->model_id = info.model_id;
	resp->version = info.version;
	resp->frame_size = info.frame_size;
	resp->pixel_format = info.pixel_format;
	resp->width = info.width;
	resp->height = info.height;
	resp->bpp = info.bpp;
	resp->errors = info.errors;

	return 0;
}

int fp_enrollment_begin(void)
{
	return fingerprint_enroll_start();
}

int fp_finger_enroll(uint8_t *image, int *completion)
{
	return fingerprint_enroll_step(image, completion);
}

int fp_enrollment_finish(void *templ)
{
	return fingerprint_enroll_finish(templ);
}

int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
		    int32_t *match_index, uint32_t *update_bitmap)
{
	return fingerprint_match(templ, templ_count, image, match_index,
				 update_bitmap);
}

void fp_sensor_low_power(void)
{
	fingerprint_enter_low_power(fp_sensor_dev);
}

int fp_maintenance(void)
{
	uint8_t buffer[CONFIG_FP_SENSOR_IMAGE_SIZE];

	return fingerprint_maintenance(fp_sensor_dev, buffer,
		CONFIG_FP_SENSOR_IMAGE_SIZE);
}
