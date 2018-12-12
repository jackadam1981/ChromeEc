/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test motion sense hid code.
 */
#include <math.h>
#include <stdio.h>

#include "accelgyro.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "motion_lid.h"
#include "motion_sense.h"
#include "motion_sense_hid.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"


#define TEST_LID_EC_RATE (10 * MSEC)

/*****************************************************************************/
/* Mock functions */
static int accel_init(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int accel_read(const struct motion_sensor_t *s, vector_3_t v)
{
	rotate(s->xyz, *s->rot_standard_ref, v);
	return EC_SUCCESS;
}

static int accel_set_range(const struct motion_sensor_t *s,
				const int range,
				const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_range(const struct motion_sensor_t *s)
{
	return 0;
}

static int accel_set_resolution(const struct motion_sensor_t *s,
				const int res,
				const int rnd)
{
	return EC_SUCCESS;
}

static int accel_get_resolution(const struct motion_sensor_t *s)
{
	return 0;
}

int test_data_rate[2] = { 0 };

static int accel_set_data_rate(const struct motion_sensor_t *s,
				const int rate,
				const int rnd)
{
	test_data_rate[s - motion_sensors] = rate | (rnd ? ROUND_UP_FLAG : 0);
	return EC_SUCCESS;
}

static int accel_get_data_rate(const struct motion_sensor_t *s)
{
	return test_data_rate[s - motion_sensors];
}

const struct accelgyro_drv test_motion_sense = {
	.init = accel_init,
	.read = accel_read,
	.set_range = accel_set_range,
	.get_range = accel_get_range,
	.set_resolution = accel_set_resolution,
	.get_resolution = accel_get_resolution,
	.set_data_rate = accel_set_data_rate,
	.get_data_rate = accel_get_data_rate,
};

struct motion_sensor_t motion_sensors[] = {
	{.name = "base",
	 .active_mask = SENSOR_ACTIVE_S0_S3_S5,
	 .chip = MOTIONSENSE_CHIP_LSM6DS0,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &test_motion_sense,
	 .rot_standard_ref = NULL,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE
		 },
		 /* Used for double tap */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE * 100,
		 },
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},
	{.name = "lid",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_KXCJ9,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &test_motion_sense,
	 .rot_standard_ref = NULL,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 119000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE,
		 },
		 /* Used for double tap */
		 [SENSOR_CONFIG_EC_S3] = {
			 .odr = 200000 | ROUND_UP_FLAG,
			 .ec_rate = TEST_LID_EC_RATE * 100,
		 },
		 [SENSOR_CONFIG_EC_S5] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
	 },
	},
};

const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);

static struct hid_descriptor hid_desc = {
	.wHIDDescLength = 0x001e, // 30 bytes max
	.bcdVersion = 0x0100,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = I2C_HID_HEADER_SIZE +
	sizeof(struct hid_accel_input_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER
};

static struct hid_accel_input_report input = {
	.report_id = 0, // maps to sensor id
	.sensor_state = 0,
	.sensor_event = 0,
	.x = 0,
	.y = 0,
	.z = 0
};

static int test_read_hid_descr(void);

/* Set spoof sensor values and return sensor pointer. Used for testing*/
static void set_spoof_sensor(struct motion_sensor_t *sensor)
{
	sensor->xyz[X] = 4;
	sensor->xyz[Y] = 5;
	sensor->xyz[Z] = 6;

	spoof_sensor = sensor;
}

/* Emulate host by sending set sampling frequency report */
static int test_set_accel_sampling_freq(void)
{
/* |buffer| is expected to contain the values written to the command register
 * followed by the values written to the data register, upon receiving a
 * SET_REPORT command, in the following byte sequence format:
 *
 *   00 30 - command register address (0x3000)
 *   xx    - report type and ID
 *   03    - SET_REPORT
 *   00 30 - data register address (0x3000)
 *   xx xx - length
 *   xx    - report ID
 *   xx... - report data from byte 9
 *
 * Note that command register and data register have the same address. Also,
 * any report ID >= 15 requires an extra byte after the SET_REPORT byte, which
 * is not supported here as we don't have any report ID >= 15.
 *
 * In summary, we expect |buffer| contains at least 10 bytes where the report
 * data starts at buffer[9]. If |buffer| contains the incorrect number bytes,
 * we ignore the report.
 * [host buffer received with set_report cmd and odr to set]
 */
	uint32_t new_odr;
	struct motion_sensor_t *sensor;

	sensor = &motion_sensors[0];
	set_spoof_sensor(sensor);
	spoof_host_buffer[0] = 0x00; // cmd reg lsb
	spoof_host_buffer[1] = 0x30; // cmd reg msb
	spoof_host_buffer[2] = 0x31; // report type (feature rpt) & report ID
	spoof_host_buffer[3] = 0x03; // set_report cmd opcode
	spoof_host_buffer[4] = 0x00; // data reg same as cmd reg. lsb
	spoof_host_buffer[5] = 0x30; // data reg msb
	spoof_host_buffer[6] = 0x00; // report len lsb
	spoof_host_buffer[7] = 0x04; // report len msb. len=4 as for odr int32
	spoof_host_buffer[8] = REPORT_ID_BASE_ACCEL_SAMPLING_RATE; // report ID
	spoof_host_buffer[9] = 0x05; // odr data starts here
	spoof_host_buffer[10] = 0x0;
	spoof_host_buffer[11] = 0x0;
	spoof_host_buffer[12] = 0x0;

	i2c_hid_process(sizeof(spoof_host_buffer),
	spoof_host_buffer, send_response);
	new_odr = get_new_odr(get_spoof_sensor());
	printf("New ODR is: %d\n", new_odr);
	TEST_ASSERT(new_odr == (spoof_host_buffer[9] & 0x0F));
	return EC_SUCCESS;

}

/* Emulate host by requesting for hid descriptor */
static int test_read_hid_descr(void)
{
	uint16_t hid_desc_arr[sizeof(hid_desc)];
	int i, j;

	j = 0;

	hid_desc_arr[0] = hid_desc.wHIDDescLength;
	hid_desc_arr[1] = hid_desc.bcdVersion;
	hid_desc_arr[2] = hid_desc.wReportDescLength;
	hid_desc_arr[3] = hid_desc.wReportDescRegister;
	hid_desc_arr[4] = hid_desc.wInputRegister;
	hid_desc_arr[5] = hid_desc.wMaxInputLength;
	hid_desc_arr[6] = hid_desc.wOutputRegister;
	hid_desc_arr[7] = hid_desc.wMaxOutputLength;
	hid_desc_arr[8] = hid_desc.wCommandRegister;
	hid_desc_arr[9] = hid_desc.wDataRegister;

	spoof_host_buffer[0] = 0x00; // hid_descr cmd reg lsb
	spoof_host_buffer[1] = 0x10; // hid_descr cmd reg msb
	printf("HID Descr len: %d\n", sizeof(hid_desc));

	i2c_hid_process(sizeof(spoof_host_buffer),
	spoof_host_buffer, send_response);
// Check that response in i2c buffer matches hid_descr exactly
	TEST_ASSERT((spoof_host_buffer[0] & 0xFF) == hid_desc_arr[j]);
	j++;

	for (i = 3; i < 20; i += 2) {
		TEST_ASSERT((spoof_host_buffer[i] <<
		8 | spoof_host_buffer[i-1]) == hid_desc_arr[j]);
		j++;
	}

	return EC_SUCCESS;

}

/* Emulate host by requesting for report descriptor */
static int test_read_report_descr(void)
{
	int same;

	spoof_host_buffer[0] = 0x00; // report_descr cmd reg lsb
	spoof_host_buffer[1] = 0x50; // report_descr cmd reg msb
	i2c_hid_process(sizeof(spoof_host_buffer),
	spoof_host_buffer, send_response);
	same = compareReportDescResponse(spoof_host_buffer, report_desc);
	TEST_ASSERT(same == 1);
	return EC_SUCCESS;

}

// check that response in i2c buffer matches report_descr exactly
int compareReportDescResponse(uint8_t *buffer, const uint8_t *report_desc)
{
	int i, j;

	for (i = 0; i < sizeof(report_desc); i++) {
		for (j = 0; j < sizeof(buffer); j++) {
			if (report_desc[i] != buffer[j])
				return 0;
			else
				return 1;
		}
	}
	return -1;
}

/* Emulate host by requesting for accel data as input report
 * INPUT REPORT STRUCTURE: REQUEST BUFFER
 * host_buffer[0] = cmd reg lsb
 * host_buffer[1] = cmd reg msb
 * host_buffer[2] = rpt type & rpt id
 * host_buffer[3] = cmd opcode (get/ set report)
 * host_buffer[4] = data reg lsb
 * host_buffer[5] = data reg lsb

 * INPUT REPORT STRUCTURE: RESPONSE BUFFER
 * host_buffer[0] = rpt len lsb   makes up I2C_HID_HEADER_SIZE
 * host_buffer[1] = rpt len msb   makes up I2C_HID_HEADER_SIZE
 * host_buffer[2] = rpt id   struct hid_accel_input_report starts here
 * host_buffer[3] = x data lsb
 * host_buffer[4] = x data msb
 * host_buffer[5] = y data lsb
 * host_buffer[6] = y data msb
 * host_buffer[7] = z data lsb
 * host_buffer[8] = z data msb
 * host_buffer[9] = sensor_state data
 * host_buffer[10] = sensor_event data
 */
static int test_input_report_content(void)
{
// Host buffer received with get_report cmd
	int i;
	struct motion_sensor_t *sensor;
// REQUEST SPOOF BUFFER
	spoof_host_buffer[0] = 0x00; // cmd reg lsb
	spoof_host_buffer[1] = 0x30; // cmd reg msb
	spoof_host_buffer[2] = 0x11; // report type (input report) & report ID
	spoof_host_buffer[3] = 0x02; // get_report cmd opcode
	spoof_host_buffer[4] = 0x00; // data reg same as cmd reg. lsb
	spoof_host_buffer[5] = 0x30; // data reg msb
	spoof_host_buffer[6] = 0x00; // report len lsb
	spoof_host_buffer[7] = sizeof(input); // report len msb
	spoof_host_buffer[8] = REPORT_ID_BASE_ACCEL; // report ID

// Create spoof sensor and set x,y,z values to return in input report
	sensor = &motion_sensors[0];
	set_spoof_sensor(sensor);
	i2c_hid_process(sizeof(spoof_host_buffer),
	spoof_host_buffer, send_response);
	printf("Buffer contents after getting hid input report:\n");
	for (i = 0; i < (I2C_HID_HEADER_SIZE +
	sizeof(struct hid_accel_input_report)); i++) {
		printf("spoof_host_buffer[%d]: %x\n", i, spoof_host_buffer[i]);
	}
	TEST_ASSERT(spoof_host_buffer[0] == 12); // report total len
	TEST_ASSERT(spoof_host_buffer[2] == 1); // REPORT_ID_BASE_ACCEL
	TEST_ASSERT(spoof_host_buffer[3] == 0); // sensor_state val = 0
	TEST_ASSERT(spoof_host_buffer[4] == 0); // sensor_event val = 0
	TEST_ASSERT(spoof_host_buffer[6] == 4); // x val
	TEST_ASSERT(spoof_host_buffer[8] == 5); // y val
	TEST_ASSERT(spoof_host_buffer[10] == 6); // z val

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	RUN_TEST(test_read_hid_descr);
	RUN_TEST(test_read_report_descr);
	RUN_TEST(test_set_accel_sampling_freq);
	RUN_TEST(test_input_report_content);

	test_print_result();
}

