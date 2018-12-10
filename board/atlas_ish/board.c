/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atlas ISH board-specific configuration */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#include "gpio_list.h" /* has to be included last */
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

uint32_t c_sqrt, c_fabs, c_log, c_exp, c_pow, c_ceil, c_atan2, c_atan, c_sin;
uint32_t c_cos, c_acos, c_isnan, c_isinf;

/* I2C port map */
const struct i2c_port_t i2c_ports[]  = {
	{"trackpad", I2C_PORT_TP, 1000,
	GPIO_I2C_PORT_TP_SCL, GPIO_I2C_PORT_TP_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* dummy functions to remove 'undefined' symbol link error for acpi.o
 * due to CONFIG_LPC flag
 */
#ifdef CONFIG_HOSTCMD_LPC
int lpc_query_host_event_state(void)
{
	return 0;
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
}
#endif


#include "touch_wrapper.h"
#include "test_image.h"

/* Processed contact records */
struct Contact contacts[CTRD_NUM_MAX_FINGERS * 2];
int contact_num;
int slots[CTRD_NUM_MAX_FINGERS];
uint8_t touch_inited;

static inline timestamp_t rdtsc(void)
{
	timestamp_t ts;

	__asm__ volatile ("rdtsc" : "=a" (ts.le.lo), "=d" (ts.le.hi));

	return ts;
}

void centroiding_test(int n_frame)
{
	int i, j;
	timestamp_t ts;
	uint32_t duration;
	timestamp_t tsc_start, tsc_end;
	struct Contact *c;
	double fps;

	CPRINTF("centroiding test starting\n");

	if (!touch_inited) {
		touch_inited = 1;
		touch_init();
	}

	ts = get_time();
	tsc_start = rdtsc();

	for (i = 0; i < n_frame; i++)
		touch_process_frame(test_image[i % 5],
				    contacts, &contact_num, slots);

	tsc_end = rdtsc();
	duration = time_since32(ts);
	fps = 1000000.0 * n_frame / (double)duration;
	CPRINTF("test run %d frames in %u(us) --> %u FPS\n",
		n_frame, duration, (uint32_t)fps);
	CPRINTF("tsc cycles: %u\n", tsc_end.le.lo - tsc_start.le.lo);

	/* Dump test frame contacts */
	#define V(x) ((int)(c->x * 1000))
	for (i = 0; i < 5; i++) {
		touch_process_frame(test_image[i],
				    contacts, &contact_num, slots);
		CPRINTF("frame %d - contact_num %d\n", i, contact_num);
		for (j = 0; j < CTRD_NUM_MAX_FINGERS * 2; j++) {
			if (!contacts[j].valid)
				continue;
			c = &contacts[j];
			CPRINTF("  c[%d] - a:%d x:%d y:%d sx:%d sy:%d "
				"theta:%d id:%d thumb:%d palm:%d\n",
				j, V(amp), V(x0), V(y0), V(sigma_x),
				V(sigma_y), V(theta), c->tracking_id,
				V(thumb_likelihood), (int)c->is_palm);
		}
	}
}

int command_perf_centroiding(int argc, char *argv[])
{
	int n_frame = 100;
	char *e;

	if (argc >= 2) {
		n_frame = strtoi(argv[1], &e, 10);
		if (*e)
			n_frame = 100;
		n_frame = MAX(n_frame, 100);
		n_frame = MIN(n_frame, 2000);
	}

	centroiding_test(n_frame);

	return 0;
}
DECLARE_CONSOLE_COMMAND(perf, command_perf_centroiding,
			"[100~2000 frames]",
			"Test run number of frames in centroiding loop");
