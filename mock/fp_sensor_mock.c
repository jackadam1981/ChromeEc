
#include <stdlib.h>

#include "common.h"
#include "fp_sensor_mock.h"
#include "fpsensor.h"

#include "console.h"
#define CPRINTF(format, args...) cprintf(CC_FP, format, ## args)
#define CPRINTS(format, args...) cprints(CC_FP, format, ## args)

struct mock_ctrl_fp_sensor mock_ctrl_fp_sensor = MOCK_CTRL_DEFAULT_FP_SENSOR;

/* Initialize the connected sensor hardware and put it in a low power mode. */
test_mockable int fp_sensor_init(void)
{
	return mock_ctrl_fp_sensor.fp_sensor_init_return;
}

test_mockable int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	resp->version = 0;
	return mock_ctrl_fp_sensor.fp_sensor_get_info_return;
}

test_mockable void fp_sensor_low_power(void)
{
}

test_mockable void fp_sensor_configure_detect(void)
{
}

test_mockable enum finger_state fp_sensor_finger_status(void)
{
	return mock_ctrl_fp_sensor.fp_sensor_finger_status_return;
}

test_mockable int fp_sensor_acquire_image(uint8_t *image_data)
{
	return mock_ctrl_fp_sensor.fp_sensor_acquire_image_return;
}

test_mockable int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode)
{
	return mock_ctrl_fp_sensor.fp_sensor_acquire_image_with_mode_return;
}

test_mockable int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
			int32_t *match_index, uint32_t *update_bitmap)
{
	return mock_ctrl_fp_sensor.fp_finger_match_return;
}

test_mockable int fp_enrollment_begin(void)
{
	return mock_ctrl_fp_sensor.fp_enrollment_begin_return;
}

test_mockable int fp_enrollment_finish(void *templ)
{
	return mock_ctrl_fp_sensor.fp_enrollment_finish_return;
}

test_mockable int fp_finger_enroll(uint8_t *image, int *completion)
{
	return mock_ctrl_fp_sensor.fp_finger_enroll_return;
}
