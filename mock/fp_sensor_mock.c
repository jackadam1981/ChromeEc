#include "common.h"

#include "fpsensor.h"

#include "console.h"
#define CPRINTF(format, args...) cprintf(CC_FP, format, ## args)
#define CPRINTS(format, args...) cprints(CC_FP, format, ## args)

/* Initialize the connected sensor hardware and put it in a low power mode. */
int fp_sensor_init(void)
{
	CPRINTF("%s\n", __func__);
	return EC_SUCCESS;
}

int fp_sensor_get_info(struct ec_response_fp_info *resp)
{
	CPRINTF("%s\n", __func__);
	resp->version = 0;
	return EC_SUCCESS;
}

void fp_sensor_low_power(void)
{
	CPRINTF("%s\n", __func__);
	/* Nothing */
}

void fp_sensor_configure_detect(void)
{
	CPRINTF("%s\n", __func__);
	/* Nothing */
}

enum finger_state fp_sensor_finger_status(void)
{
	static enum finger_state next = FINGER_NONE;
	CPRINTF("%s\n", __func__);
	next %= (FINGER_PRESENT+1);
	return next++;
}

int fp_sensor_acquire_image(uint8_t *image_data)
{
	CPRINTF("%s\n", __func__);
	return 0;
}

int fp_sensor_acquire_image_with_mode(uint8_t *image_data, int mode)
{
	CPRINTF("%s\n", __func__);
	return 0;
}

int fp_finger_match(void *templ, uint32_t templ_count, uint8_t *image,
			int32_t *match_index, uint32_t *update_bitmap)
{
	CPRINTF("%s\n", __func__);
	return EC_MKBP_FP_ERR_MATCH_YES_UPDATED;
}

int fp_enrollment_begin(void)
{
	CPRINTF("%s\n", __func__);
	return 0;
}

int fp_enrollment_finish(void *templ)
{
	CPRINTF("%s\n", __func__);
	return 0;
}

int fp_finger_enroll(uint8_t *image, int *completion)
{
	CPRINTF("%s\n", __func__);
	return EC_MKBP_FP_ERR_ENROLL_OK;
}
