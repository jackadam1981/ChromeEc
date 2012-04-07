/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LED controls.
 */

#include "board.h"
#include "console.h"
#include "gpio.h"
#include "host_command.h"
#include "i2c.h"
#include "lightbar.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/******************************************************************************/
/* How to talk to the controller */
/******************************************************************************/

/* Since there's absolutely nothing we can do about it if an I2C access
 * isn't working, we're completely ignoring any failures. */

static const uint8_t i2c_addr[] = { 0x54, 0x56 };

static inline void controller_write(int ctrl_num, uint8_t reg, uint8_t val)
{
	i2c_write8(I2C_PORT_LIGHTBAR, i2c_addr[ctrl_num], reg, val);
}

static inline uint8_t controller_read(int ctrl_num, uint8_t reg)
{
	int val;
	i2c_read8(I2C_PORT_LIGHTBAR, i2c_addr[ctrl_num], reg, &val);
	return val;
}

/******************************************************************************/
/* Controller details. We have an ADP8861 and and ADP8863, but we can treat
 * them identically for our purposes */
/******************************************************************************/

/* We need to limit the total current per ISC to no more than 20mA (5mA per
 * color LED, but we have four LEDs in parallel on each ISC). Any more than
 * that runs the risk of damaging the LED component. A value of 0x67 is as high
 * as we want (assuming Square Law), but the blue LED is the least bright, so
 * I've lowered the other colors until they all appear approximately equal
 * brightness when full on. That's still pretty bright and a lot of current
 * drain on the battery, so we'll probably rarely go that high. */
#define MAX_RED   0x5c
#define MAX_GREEN 0x38
#define MAX_BLUE  0x67

/* How we'd like to see the driver chips initialized. The controllers have some
 * auto-cycling capability, but it's not much use for our purposes. For now,
 * we'll just control all color changes actively. */
struct initdata_s {
	uint8_t reg;
	uint8_t val;
};

static const struct initdata_s init_vals[] = {
	{0x01, 0x00},				/* standby mode */
	{0x04, 0x00},				/* no backlight function */
	{0x05, 0x3f},				/* xRGBRGB per chip */
	{0x0f, 0x01},				/* square law looks better */
	{0x10, 0x3f},				/* enable independent LEDs */
	{0x11, 0x00},				/* no auto cycling */
	{0x12, 0x00},				/* no auto cycling */
	{0x13, 0x00},				/* instant fade in/out */
	{0x14, 0x00},				/* not using LED 7 */
	{0x15, 0x00},				/* current for LED 6 (blue) */
	{0x16, 0x00},				/* current for LED 5 (red) */
	{0x17, 0x00},				/* current for LED 4 (green) */
	{0x18, 0x00},				/* current for LED 3 (blue) */
	{0x19, 0x00},				/* current for LED 2 (red) */
	{0x1a, 0x00},				/* current for LED 1 (green) */
};

static void set_from_array(const struct initdata_s *data, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		controller_write(0, data[i].reg, data[i].val);
		controller_write(1, data[i].reg, data[i].val);
	}
}

static void lightbar_reset(void)
{
	uart_printf("%s()\n", __func__);

	/* Keep the controllers out of reset. The reset pullup uses more power
	 * than leaving them in standby. */
	gpio_set_level(GPIO_LIGHTBAR_RESETn, 1);
	set_from_array(init_vals, ARRAY_SIZE(init_vals));
}

/* Controller register lookup tables. */
static const uint8_t led_to_ctrl[] = { 0, 0, 1, 1 };
static const uint8_t led_to_isc[] = { 0x15, 0x18, 0x15, 0x18 };

/* Macro to scale 0-255 into max value */
#define SCALE_ABS(VAL, MAX) ((VAL * MAX)/255 + MAX/256)

/* It will often be simpler to provide an overall brightness control. */
static int brightness = 255;

/* Macro to scale 0-255 by brightness */
#define SCALE(VAL, MAX) SCALE_ABS((VAL * brightness)/255, MAX)

/******************************************************************************/
/* Basic LED control functions. */
/******************************************************************************/

static void lightbar_off(void)
{
	uart_printf("%s()\n", __func__);
	/* Just go into standby mode. No register values should change. */
	controller_write(0, 0x01, 0x00);
	controller_write(1, 0x01, 0x00);
}

static void lightbar_on(void)
{
	uart_printf("%s()\n", __func__);
	/* Come out of standby mode. */
	controller_write(0, 0x01, 0x20);
	controller_write(1, 0x01, 0x20);
}

/* So that we can make brightness changes happen instantly, we need to track
 * the current values. The values in the controllers aren't very helpful. */
static uint8_t current[4][3];

/* LEDs are numbered 0-3, RGB values should be in 0-255. */
static void lightbar_setcolor(int led, int red, int green, int blue)
{
	int ctrl, bank;
	current[led][0] = red;
	current[led][1] = green;
	current[led][2] = blue;
	ctrl = led_to_ctrl[led];
	bank = led_to_isc[led];
	controller_write(ctrl, bank, SCALE(blue, MAX_BLUE));
	controller_write(ctrl, bank+1, SCALE(red, MAX_RED));
	controller_write(ctrl, bank+2, SCALE(green, MAX_GREEN));
}

static inline void lightbar_brightness(int newval)
{
	int i;
	uart_printf("%s(%d)\n", __func__, newval);
	brightness = newval;
	for (i = 0; i < 4; i++)
		lightbar_setcolor(i, current[i][0],
				  current[i][1], current[i][2]);
}


/******************************************************************************/

/* Major colors */
static const struct {
	uint8_t r, g, b;
} testy[] = {
	{0xff, 0x00, 0x00},
	{0x00, 0xff, 0x00},
	{0x00, 0x00, 0xff},
	{0xff, 0xff, 0x00},
	{0x00, 0xff, 0xff},
	{0xff, 0x00, 0xff},
	{0xff, 0xff, 0xff},
};


/******************************************************************************/
/* Now for the pretty patterns */
/******************************************************************************/

#define WAIT_OR_RET(A) do { \
	uint32_t msg = task_wait_event(A); \
	if (!(msg & TASK_EVENT_TIMER)) \
		return TASK_EVENT_CUSTOM(msg); } while (0)

/* off */
static uint32_t sequence_s5(void)
{
	uart_printf("%s()\n", __func__);
	lightbar_off();

	/* The lightbar loses power in S5, so just wait forever. */
	WAIT_OR_RET(-1);
	return 0;
}

/* sleeping */
static uint32_t sequence_s3(void)
{
	int i = 0;
	uart_printf("%s()\n", __func__);
	lightbar_reset();
	lightbar_on();
	lightbar_setcolor(0, 0, 0, 0);
	lightbar_setcolor(1, 0, 0, 0);
	lightbar_setcolor(2, 0, 0, 0);
	lightbar_setcolor(3, 0, 0, 0);
	while (1) {
		WAIT_OR_RET(3000000);
		lightbar_on();
		i = i % 4;
		/* FIXME: indicate battery level? */
		lightbar_setcolor(i, testy[i].r, testy[i].g, testy[i].b);
		WAIT_OR_RET(100000);
		lightbar_setcolor(i, 0, 0, 0);
		i++;
		lightbar_off();
	}

	return 0;
}

/* on */
static uint32_t sequence_s0(void)
{
	int l = 0;
	int n = 0;

	uart_printf("%s()\n", __func__);

	while (1) {
		l = l % 4;
		n = n % 5;
		if (n == 4)
			lightbar_setcolor(l, 0, 0, 0);
		else
			lightbar_setcolor(l, testy[n].r,
					  testy[n].g, testy[n].b);
		l++;
		n++;
		WAIT_OR_RET(50000);
	}

	return 0;
}

/* sleep to on */
static uint32_t sequence_s3s0(void)
{
	uart_printf("%s()\n", __func__);
	lightbar_reset();
	lightbar_on();
	lightbar_setcolor(0, 0, 0, 255);
	WAIT_OR_RET(200000);
	lightbar_setcolor(1, 255, 0, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(2, 255, 255, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(3, 0, 255, 0);
	WAIT_OR_RET(200000);
	return 0;
}

/* on to sleep */
static uint32_t sequence_s0s3(void)
{
	uart_printf("%s()\n", __func__);
	lightbar_on();
	lightbar_setcolor(0, 0, 0, 255);
	lightbar_setcolor(1, 255, 0, 0);
	lightbar_setcolor(2, 255, 255, 0);
	lightbar_setcolor(3, 0, 255, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(0, 0, 0, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(1, 0, 0, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(2, 0, 0, 0);
	WAIT_OR_RET(200000);
	lightbar_setcolor(3, 0, 0, 0);
	return 0;
}

/* These two probably aren't much use, since the lightbar loses power when the
 * CPU is in S5. */

/* Off to sleep */
static uint32_t sequence_s5s3(void)
{
	uart_printf("%s()\n", __func__);
	return 0;
}

/* Sleep to off. */
static uint32_t sequence_s3s5(void)
{
	uart_printf("%s()\n", __func__);
	return 0;
}

/* FIXME: This can be removed. */
static uint32_t sequence_test(void)
{
	int i, j, k, r, g, b;
	int kmax = 254;
	int kstep = 8;

	uart_printf("%s()\n", __func__);

	lightbar_reset();
	lightbar_on();
	for (i = 0; i < ARRAY_SIZE(testy); i++) {
		for (k = 0; k <= kmax; k += kstep) {
			for (j = 0; j < 4; j++) {
				r = testy[i].r ? k : 0;
				g = testy[i].g ? k : 0;
				b = testy[i].b ? k : 0;
				lightbar_setcolor(j, r, g, b);
			}
			WAIT_OR_RET(10000);
		}
		for (k = kmax; k >= 0; k -= kstep) {
			for (j = 0; j < 4; j++) {
				r = testy[i].r ? k : 0;
				g = testy[i].g ? k : 0;
				b = testy[i].b ? k : 0;
				lightbar_setcolor(j, r, g, b);
			}
			WAIT_OR_RET(10000);
		}
	}

	return 0;
}

/* This uses the auto-cycling features of the controllers to make a semi-random
 * pattern of slowly fading colors. This is interesting only because it doesn't
 * require any effort from the EC. */
static uint32_t sequence_pulse(void)
{
	uint32_t msg;
	int r, g, b;

	uart_printf("%s()\n", __func__);
	lightbar_reset();

	r = SCALE(255, MAX_RED);
	g = SCALE(255, MAX_BLUE);
	b = SCALE(255, MAX_GREEN);

	lightbar_on();
	controller_write(0, 0x11, 0xce);
	controller_write(0, 0x12, 0x67);
	controller_write(0, 0x13, 0xef);

	controller_write(0, 0x15, b);
	controller_write(0, 0x16, r);
	controller_write(0, 0x17, g);
	controller_write(0, 0x18, b);
	controller_write(0, 0x19, r);
	controller_write(0, 0x1a, g);

	controller_write(1, 0x11, 0xce);
	controller_write(1, 0x12, 0x67);
	controller_write(1, 0x13, 0xcd);

	controller_write(1, 0x15, b);
	controller_write(1, 0x16, r);
	controller_write(1, 0x17, g);
	controller_write(1, 0x18, b);
	controller_write(1, 0x19, r);
	controller_write(1, 0x1a, g);

	/* Not using WAIT_OR_RET() here, because we want to clean up when we're
	 * done. The only way out is to get a message. */
	msg = task_wait_event(-1);
	lightbar_reset();
	lightbar_on();
	return TASK_EVENT_CUSTOM(msg);
}

/****************************************************************************/
/* Lightbar task. It just cycles between various pretty patterns. */
/****************************************************************************/

static uint32_t (*sequence[])(void) = {
	0,
	sequence_s5,
	sequence_s3,
	sequence_s0,
	sequence_s5s3,
	sequence_s3s0,
	sequence_s0s3,
	sequence_s3s5,
	sequence_test,
	sequence_pulse,
};

void lightbar_task(void)
{
	uint32_t msg;
	enum lightbar_sequence state;

	lightbar_reset();

	/* FIXME: What to do first? */
	state = LIGHTBAR_S5;

	while (1) {
		msg = sequence[state]();
		uart_printf("%s(%d)\n", __func__, msg);
		msg = TASK_EVENT_CUSTOM(msg);
		if (msg && msg < LIGHTBAR_NUM_SEQUENCES)
			state = TASK_EVENT_CUSTOM(msg);
		else
			switch (state) {
			case LIGHTBAR_S5S3:
				state = LIGHTBAR_S3;
				break;
			case LIGHTBAR_S3S0:
				state = LIGHTBAR_S0;
				break;
			case LIGHTBAR_S0S3:
				state = LIGHTBAR_S3;
				break;
			case LIGHTBAR_S3S5:
				state = LIGHTBAR_S5;
				break;
			case LIGHTBAR_TEST:
				state = LIGHTBAR_S0;
			default:
				break;
			}
	}
}


/* Request a preset sequence from the lightbar task. */
void lightbar_sequence(enum lightbar_sequence num)
{
	uart_printf("%s(%d)\n", __func__, num);
	if (num < LIGHTBAR_NUM_SEQUENCES)
		task_set_event(TASK_ID_LIGHTBAR,
			       TASK_EVENT_WAKE | TASK_EVENT_CUSTOM(num), 0);
}


/****************************************************************************/
/* Host commands via LPC bus */
/****************************************************************************/

/* FIXME(wfrichar): provide the same functions as the EC console */

static enum lpc_status lpc_cmd_reset(uint8_t *data)
{
	lightbar_reset();
	return EC_LPC_RESULT_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_LPC_COMMAND_LIGHTBAR_RESET, lpc_cmd_reset);

static enum lpc_status lpc_cmd_test(uint8_t *data)
{
	lightbar_sequence(LIGHTBAR_TEST);
	return EC_LPC_RESULT_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_LPC_COMMAND_LIGHTBAR_TEST, lpc_cmd_test);


/****************************************************************************/
/* EC console commands */
/****************************************************************************/

static int help(char *prog)
{
	uart_printf("Usage:  %s\n", prog);
	uart_printf("        %s reset\n", prog);
	uart_printf("        %s off\n", prog);
	uart_printf("        %s on\n", prog);
	uart_printf("        %s msg NUM\n", prog);
	uart_printf("        %s brightness NUM\n", prog);
	uart_printf("        %s CTRL REG VAL\n", prog);
	uart_printf("        %s LED RED GREEN BLUE\n", prog);
	return EC_ERROR_UNKNOWN;
}

static void dump_em(void)
{
	int reg, d1, d2, i;
	int reglist[] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
			  0x08, 0x09, 0x0a,                         0x0f,
			  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
			  0x18, 0x19, 0x1a };
	for (i = 0; i < ARRAY_SIZE(reglist); i++) {
		reg = reglist[i];
		d1 = controller_read(0, reg);
		d2 = controller_read(1, reg);
		uart_printf(" %02x     %02x     %02x\n", reg, d1, d2);
	}
}

static int command_lightbar(int argc, char **argv)
{
	if (1 == argc) {		/* no args = dump 'em all */
		dump_em();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "reset")) {
		lightbar_reset();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "off")) {
		lightbar_off();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "on")) {
		lightbar_on();
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "brightness")) {
		char *e;
		int num = strtoi(argv[2], &e, 16);
		lightbar_brightness(num);
		return EC_SUCCESS;
	}

	if (!strcasecmp(argv[1], "msg")) {
		char *e;
		int num = strtoi(argv[2], &e, 16);
		lightbar_sequence(num);
		return EC_SUCCESS;
	}

	if (4 == argc) {
		char *e;
		int ctrl = strtoi(argv[1], &e, 16);
		int reg = strtoi(argv[2], &e, 16);
		int val = strtoi(argv[3], &e, 16);
		controller_write(ctrl, reg, val);
		return EC_SUCCESS;
	}

	if (5 == argc) {
		char *e;
		int led = strtoi(argv[1], &e, 16);
		int red = strtoi(argv[2], &e, 16);
		int green = strtoi(argv[3], &e, 16);
		int blue = strtoi(argv[4], &e, 16);
		lightbar_setcolor(led, red, green, blue);
		return EC_SUCCESS;
	}

	return help(argv[0]);
}
DECLARE_CONSOLE_COMMAND(lightbar, command_lightbar);
