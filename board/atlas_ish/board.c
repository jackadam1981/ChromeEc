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

/* Touchpad interrupt handler */
void tp_handler(enum gpio_signal s);
#include "gpio_list.h" /* has to be included last */

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

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

/*
 * GPIO interrupt and context switching stress test.
 *
 * This test tries to address a racing condition that stops IOAPIC driver
 * from calling GPIO interrupt handler
 */

#define TP_ADDR 0x2A
#define DESC_SIZE_MAX 0x38

static int tp_int_ack(void)
{
	uint8_t ack_output[DESC_SIZE_MAX];

	return i2c_xfer(I2C_PORT_TP, TP_ADDR,
			NULL, 0, ack_output, DESC_SIZE_MAX);
}

static int tp_reset(void)
{
	uint8_t cmd_reset[2] = {0x00, 0x01};

	return i2c_xfer(I2C_PORT_TP, TP_ADDR,
			cmd_reset, sizeof(cmd_reset), NULL, 0);
}

static int tp_set_ptp(void)
{
	uint8_t cmd_set_ptp[] = {
		0x05, 0x00,  /* HID 0x0005: command reg address */
		0x33, 0x03,  /* HID 0x0333: set report input mode */
		0x06, 0x00,  /* HID 0x0006: data reg address */
		0x05, 0x00,  /* payload size: 3 bytes */
		0x03,        /* set rig input mode */
		0x03, 0x00}; /* 0x0003: PTP mode */

	return i2c_xfer(I2C_PORT_TP, TP_ADDR,
			cmd_set_ptp, sizeof(cmd_set_ptp), NULL, 0);
}

static int tp_set_mouse(void)
{
	uint8_t cmd_set_ptp[] = {
		0x05, 0x00,  /* HID 0x0005: command reg address */
		0x33, 0x03,  /* HID 0x0333: set report input mode */
		0x06, 0x00,  /* HID 0x0006: data reg address */
		0x05, 0x00,  /* payload size: 3 bytes */
		0x03,        /* set rig input mode */
		0x00, 0x00}; /* 0x0000: mouse mode */

	return i2c_xfer(I2C_PORT_TP, TP_ADDR,
			cmd_set_ptp, sizeof(cmd_set_ptp), NULL, 0);
}

static int tp_wakeup(void)
{
	uint8_t cmd_wakeup[] = {
		0x05, 0x00,
		0x00, 0x08};

	return i2c_xfer(I2C_PORT_TP, TP_ADDR,
			cmd_wakeup, sizeof(cmd_wakeup), NULL, 0);
}

int command_tp_mode(int argc, char *argv[])
{
	if (argc > 1) {
		if (!strcasecmp("mouse", argv[1])) {
			CPRINTF("Set TP mouse mode: %d\n",
				tp_set_mouse());
			return 0;
		}
		if (!strcasecmp("ptp", argv[1])) {
			CPRINTF("Set TP ptp mode: %d\n",
				tp_set_mouse());
			return 0;
		}
	}

	return EC_ERROR_PARAM1;
}

static int tp_init(void)
{
	int ret;

	ret = tp_reset();
	if (ret)
		return ret;
	/* Reset takes 10ms */
	msleep(10);
	ret = tp_wakeup();
	if (ret)
		return ret;
	ret = tp_set_ptp();
	if (ret)
		return ret;
	return tp_int_ack();
}

uint32_t cnt_tp_int, cnt_tp_ack;
timestamp_t ts_tp_start, ts_tp_wait;

void dump_tp_int(void);
DECLARE_DEFERRED(dump_tp_int);

void dump_tp_int(void)
{
	static uint32_t cnt_int_last;
	static uint32_t cnt_ack_last;
	static timestamp_t ts_last;
	uint32_t cnt_int_now = cnt_tp_int;
	uint32_t cnt_ack_now = cnt_tp_ack;
	timestamp_t ts = get_time();

	CPRINTF("TP int:%u ack:%u duration:%u us, time: %11.6ld s\n",
		cnt_int_now - cnt_int_last,
		cnt_ack_now - cnt_ack_last,
		(uint32_t)(ts.val - ts_last.val),
		ts.val);
	if (cnt_tp_int != cnt_int_last) {
		cnt_int_last = cnt_int_now;
		cnt_ack_last = cnt_ack_now;
		ts_last = ts;
		hook_call_deferred(&dump_tp_int_data, SECOND);
	}
}

void tp_handler(enum gpio_signal s)
{
	cnt_tp_int++;
	task_wake(TASK_ID_TP);
}

void tp_task(void)
{
	if (tp_init())
		CPRINTF("TP init failed...\n");
	CPRINTF("2nd TP init returns: %d\n", tp_init());

	gpio_enable_interrupt(GPIO_TOUCHPAD_INT);

	cnt_tp_int = 0;
	cnt_tp_ack = 0;

	while (1) {
		ts_tp_wait = get_time();
		task_wait_event(-1);
		if (time_since32(ts_tp_wait) > (500*MSEC)) {
			cnt_tp_int = 0;
			cnt_tp_ack = 0;
			ts_tp_start = get_time();
			hook_call_deferred(&dump_tp_int_data, SECOND);
			continue;
		}
		tp_int_ack();
		cnt_tp_ack++;
	}
}

