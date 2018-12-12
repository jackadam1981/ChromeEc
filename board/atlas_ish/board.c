/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Atlas ISH board-specific configuration */

#include "console.h"
#include "i2c.h"
#include "registers.h"
#include "hooks.h"
#include "timer.h"
#include "tsc.h"
#include "touch_wrapper.h"
#include "ish_hid.h"
#include "gpio_list.h"
#include "hwtimer.h"
#include "task.h"
#include "atomic.h"
#include "hooks.h"

/* DEBUG */
#include "util.h"
#include "registers.h"

/* Defines */
#define B50_TSC_DEBUG		0
#if defined(B50_TSC_DEBUG) && B50_TSC_DEBUG
#define CPRINTS(format, args...) cprints(CC_COMMAND, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

#define INTP_ENABLE	1
#define INTP_DISABLE	0

#define INTP_CLEAR	1
#define INTP_NOCLEAR	0

#define NOTIFY_ON	1
#define NOTIFY_OFF	0

#define HOVER_THRESHOLD			10
#define EVENT_FLAG_TOUCHPAD_ACQ		TASK_EVENT_CUSTOM(1 << 7)

/* Global variables and definitions for touch processing */
#define MAX_X_POS               13184
#define MAX_Y_POS               8704
#define MAX_PRESSURE            0xFF
#define AMP_RATIO               30.0
#define CTRD_PALM_ID_START      20000
#define HOVER_TIMEOUT           (500 * MSEC)

/* Offset and scale required to adjust output to the correct range */
#define X_OFFSET        0.25
#define Y_OFFSET        0.35
#define X_SCALE         23.5
#define Y_SCALE         13.8

#define GREY_MIN        234
#define GREY_MAX        255

/* I2C port map */
const struct i2c_port_t i2c_ports[]  = {
	{"trackpad", I2C_PORT_TP, 1000,
	GPIO_I2C_PORT_TP_SCL, GPIO_I2C_PORT_TP_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* Raw framebuffer (double buffered) */
static struct touch_frame frames[2];
static union hid_report tscrpts[2];

/* Current active framebuffer index */
static volatile int frame_active_index;

/* New frame pending flag */
static volatile uint32_t new_frame_pending;

/* Processed contact records */
static struct Contact contacts[MAX_FINGERS * 2];
static int contact_num;
static int hover;

/* Current slot allocation */
static int slots[MAX_FINGERS];

/* Draw heatmap in console when enabled */
static int enable_heatmap_visualization;

#if defined(B50_TSC_DEBUG) && B50_TSC_DEBUG
static void dump_heatmap(struct touch_frame *buf)
{
	uint32_t i;
	/* Address and Hdr */
	ccprintf("Adr=0x%08x Lng=%d Rid=%02x Fidx=%d\n"
		, buf
		, (buf->hdr.length_msb << 8) + buf->hdr.length_lsb
		, buf->hdr.rid
		, buf->hdr.index);
	/* Mutual Cap. 2D Heatmap */
	ccprintf("MutualCap:");
	for (i = 0; i < FRAME_SIZE; i++) {
		if (!(i % SENSE_SIZE))
			ccprintf("\n");
		ccprintf(" %03d", buf->frame[i]);
	}
	ccprintf("\n");
	/* Self Cap. Col */
	ccprintf("SelfCap_Col:\n");
	for (i = 0; i < SENSE_SIZE; i++)
		ccprintf(" %03d", buf->sense[i]);
	ccprintf("\n");
	/* Self_Cap. Row */
	ccprintf("SelfCap_Row:\n");
	for (i = 0; i < FORCE_SIZE; i++)
		ccprintf(" %03d", buf->force[i]);
	ccprintf("\n");
}

static int board_print_heatmap_buf(int argc, char *argv[])
{
	ccprintf("np:%d faidx:%d\n", new_frame_pending, frame_active_index);
	dump_heatmap(&(frames[0]));
	dump_heatmap(&(frames[1]));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(printht, board_print_heatmap_buf, "N/A",
			"print heatmap buffers");
#endif

/* Static functions */
static void set_touchpad_intp(int enable, int clear)
{
	if (clear)
		gpio_clear_pending_interrupt(GPIO_ISH_TRACKPAD_INT_L);
	if (enable == INTP_ENABLE)
		gpio_enable_interrupt(GPIO_ISH_TRACKPAD_INT_L);
	else
		gpio_disable_interrupt(GPIO_ISH_TRACKPAD_INT_L);
}

static void board_init_tsc_deferred(void)
{
	/* Initialize TSC */
	tsc_init();

	/* Enable TSC interrupt after TSC initialized */
	set_touchpad_intp(INTP_ENABLE, INTP_CLEAR);

	CPRINTF("Board tsc init complete\n");
}
DECLARE_DEFERRED(board_init_tsc_deferred);

static void board_init(void)
{
	/* Variables Init */
	frame_active_index = 0;
	report_active_index = 0;
	/* Deferred call tsc init */
	hook_call_deferred(&board_init_tsc_deferred_data, (2 * MSEC));

	CPRINTF("Board init complete\n");
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);

static void visualize_heatmap(struct touch_frame *frame)
{
	int i, j;
	int index;
	int value;
	int color_value;

	ccputs("\n");
	for (j = kSensorCols - 1; j >= 0; j--) {
		for (i = 0; i < kSensorRows; i++) {
			index = (i * kSensorCols + j);
			value = frame->frame[index];
			color_value = GREY_MIN +
					(int)(value / (256.0
						/ (GREY_MAX - GREY_MIN)));
			ccprintf("\x1B[48;5;%dm  \x1B[0m", color_value);
		}
		ccputs("\n");
		cflush();
	}
}

static void compile_report(uint8_t button)
{
	/* Save report into back buffer */
	struct touch_report *touch = &touch_reports[report_active_index ^ 1];
	struct touch_report *touch_old = &touch_reports[report_active_index];
	struct mouse_report *mouse = &mouse_reports[report_active_index ^ 1];
	int i = 0, j = 0;

	touch->button = button;
	touch->count = MAX_FINGERS;

	/* Windows expects scan time to be in units of 100us.  As Windows
	 * measures the delta of scan times between the first and the current
	 * report, we simply report the __hw_clock_source_read() value (which
	 * is in resolution of 1us) divided by 100 as the scan time.
	 */
	touch->scan_time = __hw_clock_source_read() / 100;

	if (!contact_num) {
		touch->finger[0].confidence = 0;
		touch->finger[0].tip = 0;
		touch->finger[0].inrange = hover;
		touch->finger[0].id = 0;
		touch->finger[0].x = MAX_X_POS / 2;
		touch->finger[0].y = MAX_Y_POS / 2;
		touch->finger[0].width = 0;
		touch->finger[0].height = 0;
		touch->finger[0].pressure = 0;
		touch->finger[0].orientation = 0;

		slots[0] = hover ? touch_get_next_tracking_id() : -1;
		i++;
	}

	for (; i < MAX_FINGERS; i++) {
		if (slots[i] == -1) {
			touch->finger[i].id = i;
			touch->finger[i].confidence = 0;
			touch->finger[i].tip = 0;
			touch->finger[i].inrange = 0;
			continue;
		}

		for (j = 0; j < contact_num; j++) {
			struct Contact *ct = &contacts[j];
			int16_t x, y, w, h, theta;
			uint8_t amp;

			if (slots[i] != ct->tracking_id)
				continue;

			x = (ct->x0 + X_OFFSET) * MAX_X_POS / X_SCALE;
			y = (ct->y0 + Y_OFFSET) * MAX_Y_POS / Y_SCALE;
			w = ct->sigma_x * MAX_X_POS / kSensorRows;
			h = ct->sigma_y * MAX_Y_POS / kSensorCols;

			/* Clamp x y positions and amplitude. */
			x = MIN(MAX(0, x), MAX_X_POS - 1);
			y = MIN(MAX(0, y), MAX_Y_POS - 1);
#if defined(AXIS_Y_INVERT)
			/* Invert y */
			y = MAX_Y_POS - 1 - y;
#endif
			if (ct->tracking_id < CTRD_PALM_ID_START) {
				amp = MIN(MAX(0, (int)(ct->amp / AMP_RATIO)),
						MAX_PRESSURE);
			} else {
				amp = MAX_PRESSURE;
			}

			/* Calculate orientation.
			 * The theta value coming out of centroiding ranges
			 * from (-180, 180). But since the one can't
			 * really distinguish if a finger is pointing up or
			 * down, the only valid ranges are (-90, 90).
			 *
			 * The azimuth usage require us to output a value
			 * between (0, MAX) signaling the counter-clockwise
			 * rotation of the finger. So in our case we should
			 * output value with in the range of [270, 360] and
			 * [0, 90].
			 */
			theta = ct->theta / M_PI * 180;
			if (w > h)
				theta += 90;
			if (theta > 90)
				theta += 180;
			if (theta < -90)
				theta += 180;
			if (theta < 0)
				theta += 360;

			/* Make it counter-clockwise */
			theta = 360 - theta;

			touch->finger[i].id = i;
			/* Windows considers any contact with width or height
			 * greater than 25mm to unintended, and expects the
			 * confidence value to be cleared for such a contact.
			 * For now, we simply treat tip and confidence the
			 * same.
			 *
			 * TODO(b/70681946): Set confidence based on
			 * width and height.
			 */
			touch->finger[i].confidence = 1;
			touch->finger[i].tip = 1;
			touch->finger[i].inrange = 1;
			touch->finger[i].x = x;
			touch->finger[i].y = y;
			touch->finger[i].width = w;
			touch->finger[i].height = h;
			touch->finger[i].pressure = amp;
			touch->finger[i].orientation = theta;
			break;
		}
	}

	mouse->button1 = touch->button;
	if (touch->finger[0].tip == 1 && touch_old->finger[0].tip == 1) {
		/* The relative X/Y movements in the mouse report are computed
		 * based on the deltas of absolute X/Y positions between the
		 * previous and current touch report. The computed deltas need
		 * to be scaled for a smooth mouse movement.
		 *
		 * TODO(b/70681946): The 1/10 scaling is just based on
		 * experiments on eve. Tune the scaling factor.
		 */
		mouse->x = (touch->finger[0].x - touch_old->finger[0].x) / 10;
		mouse->y = (touch->finger[0].y - touch_old->finger[0].y) / 10;
	} else {
		mouse->x = 0;
		mouse->y = 0;
	}

	/* Send report */
	ish_hid_send_report(report_active_index ^ 1);

	/* Swap buffer */
	report_active_index ^= 1;
}

static int detect_hover(struct touch_frame *frame)
{
	int i;
	uint8_t value;

	for (i = 0; i < kSensorRows; i++) {
		value = frame->force[i];
		if (value > HOVER_THRESHOLD)
			return 1;
	}
	for (i = 0; i < kSensorCols; i++) {
		value = frame->sense[i];
		if (value > HOVER_THRESHOLD)
			return 1;
	}
	return 0;
}

static void stop_hover(void)
{
	/* Set force and sense to full value to simulate no hover */
	memset(&frames[frame_active_index].force, 0, FORCE_SIZE);
	memset(&frames[frame_active_index].sense, 0, SENSE_SIZE);
	task_wake(TASK_ID_CENTROIDING);
}
DECLARE_DEFERRED(stop_hover);

static int command_heatmap_visualization(int argc, char *argv[])
{
	int val;

	if (argc != 2)
		return EC_ERROR_PARAM_COUNT;

	if (parse_bool(argv[1], &val))
		enable_heatmap_visualization = val;
	else
		return EC_ERROR_PARAM1;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(heatmap, command_heatmap_visualization,
			"<BOOLEAN>",
			"Turn on/off touch sensor heatmap visualization");

/* Host commands */

/* Functions */
/*
 * Dummy functions to remove 'undefined' symbol link error for acpi.o
 * due to CONFIG_HOSTCMD_LPC flag
 */
int lpc_query_host_event_state(void)
{
	return 0;
}

void lpc_set_acpi_status_mask(uint8_t mask)
{
}

void deferred_tsc_init(void)
{
	tsc_init();
}
DECLARE_DEFERRED(deferred_tsc_init);


/* Touchpad interrupt handler */
void touchpad_event(enum gpio_signal signal)
{
	task_set_event(TASK_ID_TOUCHPAD_ACQ, EVENT_FLAG_TOUCHPAD_ACQ, 0);
}

void touchpad_acq_task(void)
{
	int ret;

	CPRINTF("Acq start\n");
	while (1) {
		ret = task_wait_event_mask(EVENT_FLAG_TOUCHPAD_ACQ, -1);
		if (!(ret & EVENT_FLAG_TOUCHPAD_ACQ))
			continue;

		if (new_frame_pending) {
			tsc_send_ack();
			task_wake(TASK_ID_CENTROIDING);
		} else {
			tsc_read_report(&(tscrpts[frame_active_index ^ 1])
					, INTP_ACT);
			tsc_read_frame(&frames[frame_active_index ^ 1]);
			atomic_or(&new_frame_pending, 1);
			task_wake(TASK_ID_CENTROIDING);
		}
	}
}

void centroiding_task(void)
{
	CPRINTF("Calc wake up\n");
	/* Centroiding Init */
	touch_init();

	while (1) {
		int prev_hover = hover;

		touch_process_frame(frames[frame_active_index].frame,
					contacts, &contact_num, slots);
		hover = (contact_num ?
			0 : detect_hover(&frames[frame_active_index]));

		if (contact_num || hover != prev_hover)
			compile_report(tscrpts[frame_active_index]
						.ptp_mode_report.button);

		if (enable_heatmap_visualization)
			visualize_heatmap(&frames[frame_active_index]);

		if (new_frame_pending) {
			frame_active_index ^= 1;
			atomic_clear(&new_frame_pending, 1);
		} else {
			/* Since hover finger leaving does not trigger an
			 * interrupt, we schedule a stop_hover deferred task
			 * to simulate hover finger leaving.
			 */
			if (hover)
				hook_call_deferred(&stop_hover_data,
						HOVER_TIMEOUT);
			task_wait_event(-1);

			/* Clear previous defer call since we got new events */
			hook_call_deferred(&stop_hover_data, -1);
		}

	}
}
