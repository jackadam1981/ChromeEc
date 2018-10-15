#include "hid_device.h"
#include "util.h"

#include "i2c.h"
#include "i2c_hid_master.h"
#include "gpio.h"

struct hid_descriptor {
	uint16_t wHIDDescLength;
        uint16_t bcdVersion;
        uint16_t wReportDescLength;
        uint16_t wReportDescRegister;
        uint16_t wInputRegister;
        uint16_t wMaxInputLength;
        uint16_t wOutputRegister;
        uint16_t wMaxOutputLength;
        uint16_t wCommandRegister;
        uint16_t wDataRegister;
        uint16_t wVendorID;
        uint16_t wProductID;
        uint16_t wVersionID;
        uint32_t reserved;
} __packed;

int elantp_hid_handle;
int hid_input_desc_size;
int host_ready;


/* Atlas TP configuration */

#include "console.h"
#include "registers.h"
#include "hooks.h"
#include "timer.h"
#include "tsc.h"
#include "hwtimer.h"
#include "task.h"
#include "atomic.h"

/* DEBUG */
#include "util.h"
#include "registers.h"

#ifdef TOUCH_PAD_DEBUG
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

#define INTP_ENABLE	1
#define INTP_DISABLE	0

#define INTP_CLEAR	1
#define INTP_NOCLEAR	0

#define EVENT_FLAG_TOUCHPAD_ACQ		TASK_EVENT_CUSTOM(1 << 7)

static union hid_report tscrpts;

/* Static functions */
static void set_touchpad_intp(int enable, int clear)
{
	if (clear)
		gpio_clear_pending_interrupt(GPIO_ISH_TRACKPAD_INT_L);
	if (enable == INTP_ENABLE) {
		gpio_enable_interrupt(GPIO_ISH_TRACKPAD_INT_L);
	} else {
		gpio_disable_interrupt(GPIO_ISH_TRACKPAD_INT_L);
	}
}

/* Touchpad interrupt handler */
void touchpad_event(enum gpio_signal signal)
{
	task_set_event(TASK_ID_TOUCHPAD_ACQ, EVENT_FLAG_TOUCHPAD_ACQ, 0);
}

void touchpad_acq_task(void)
{
	int ret;
	size_t report_size;

	CPRINTF("Acq start\n");

	while (1) {
		ret = task_wait_event_mask(EVENT_FLAG_TOUCHPAD_ACQ, -1);
		if (!(ret & EVENT_FLAG_TOUCHPAD_ACQ))
			continue;

		if (!host_ready)
			continue;

		tsc_read_report(&tscrpts, INTP_ACT);

		report_size = (size_t)tscrpts.hdr.length_msb << 8 |
			      (size_t)tscrpts.hdr.length_lsb;

		hid_subsys_send_input_report(elantp_hid_handle,
					     (uint8_t *)&tscrpts + 2,
					     report_size - 2);
	}
}

int elantp_initialize(int hid_handle)
{
	elantp_hid_handle = hid_handle;

	tsc_init();

	/* Enable TSC interrupt after TSC initialized */
	set_touchpad_intp(INTP_ENABLE, INTP_CLEAR);

	return 0;
}

int elantp_get_hid_descriptor(int hid_handle, uint8_t *buf, size_t buf_size)
{
	struct hid_descriptor *hid_desc;
	int size = i2c_hid_copy_hid_desc(buf);

	hid_desc = (struct hid_descriptor *)buf;
	hid_input_desc_size = (int) hid_desc->wReportDescLength;

	return size;
}

int elantp_get_report_descriptor(int hid_handle, uint8_t *buf, size_t buf_size)
{
	i2c_hid_copy_rpt_desc(buf);

	host_ready = 1;
	return hid_input_desc_size;
}

int elantp_get_feature_report(int hid_handle, uint8_t report_id, uint8_t *buf,
			     uint32_t buf_size)
{
	CPRINTF("get feature report\n");
	return 0;
}

int elantp_set_feature_report(int hid_handle, uint8_t report_id,
			     const uint8_t *data, size_t data_size)
{
	CPRINTF("set feature report\n");
	return 0;
}

int elantp_get_input_report(int hid_handle, uint8_t report_id,
			   uint8_t *buf, size_t buf_size)
{
	size_t report_size;

	tsc_read_report(&tscrpts, INTP_ACT);

	report_size = (size_t)tscrpts.hdr.length_msb << 8 |
		      (size_t)tscrpts.hdr.length_lsb;

	memcpy(buf, (uint8_t *)&tscrpts + 2, report_size - 2);

	return report_size;
}

static struct hid_callbacks elantp_hid_cbs = {
	.initialize = elantp_initialize,
	.get_hid_descriptor = elantp_get_hid_descriptor,
	.get_report_descriptor = elantp_get_report_descriptor,
	.get_feature_report = elantp_get_feature_report,
	.set_feature_report = elantp_set_feature_report,
	.get_input_report = elantp_get_input_report,
};

static struct hid_device elantp_device = {
	.dev_class = 1,
        .pid = 1,
        .vid = 0,

	.cbs = &elantp_hid_cbs,
};

HID_DEVICE_ENTRY(elantp_device);

