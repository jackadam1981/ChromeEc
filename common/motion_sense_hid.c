/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Motion sense hid module */

#include "accelgyro.h"
#include "atomic.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gesture.h"
#include "hooks.h"
#include "host_command.h"
#include "hwtimer.h"
#include "lid_angle.h"
#include "lightbar.h"
#include "math_util.h"
#include "mkbp_event.h"
#include "motion_sense.h"
#include "motion_sense_hid.h"
#include "motion_lid.h"
#include "power.h"
#include "queue.h"
#include "tablet_mode.h"
#include "timer.h"
#include "task.h"
#include "util.h"

#define INVALID_ID		-1

/* Reports (double buffered) */
static struct hid_accel_input_report input_reports[2];

/* Current active report buffer index */
int report_active_index;

/* Sensor odr */
uint32_t base_accel_odr;

/* TODO(): To remove, test code can define sensor array and host buffer. */
#ifdef CONFIG_ACCEL_SPOOF_MODE
struct motion_sensor_t *spoof_sensor;
uint8_t spoof_host_buffer[512];
#endif

/* Map feature report ID to report/ sensor ID*/
static int hid_get_sensorid_from_featureid(int feature_id)
{
	if (feature_id <= 0)
		return INVALID_ID;

	switch (feature_id) {
	case(6 || 7 || 8 || 9 || 10 || 11 || 12):
	return REPORT_ID_BASE_ACCEL;
	}
	return -1;
}

/* Console output macros */
#define CPUTS(outstr) cputs(CC_MOTION_SENSE, outstr)
#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ## args)

/*
 * Adjustment in us to ec rate when calculating interrupt interval:
 * To be sure the EC will send an interrupt even if it finishes processing
 * events slightly earlier than the previous period.
 */
#define MOTION_SENSOR_INT_ADJUSTMENT_US 10

/*
 * Current power level (S0, S3, S5, ...)
 */
//test_export_static enum chipset_state_mask sensor_active_hid;

/* HID functions */
static struct hid_descriptor hid_desc = {
	.wHIDDescLength = 30,
	.bcdVersion = 0x0100,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = I2C_HID_HEADER_SIZE +
	sizeof(struct hid_accel_input_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER,
	.wVendorID = 0x18d1,  /* Google Vendor ID from USB */
	.wProductID = 0x5037,  /* Register at "Google USB ID allocation" */
	.wVersionID = 0x6776
};

BUILD_ASSERT(sizeof(hid_desc) == 30);

static struct hid_accel_input_report input = {
	.report_id = 0, // maps to sensor id
	.sensor_state = 0,
	.sensor_event = 0,
	.x = 0,
	.y = 0,
	.z = 0
};

/* INPUT REPORT STRUCTURE: REQUEST BUFFER
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
 * host_buffer[3] = sensor_state data
 * host_buffer[4] = sensor_event data
 * host_buffer[5] = x data lsb
 * host_buffer[6] = x data msb
 * host_buffer[7] = y data lsb
 * host_buffer[8] = y data msb
 * host_buffer[9] = z data lsb
 * host_buffer[10] = z data msb
 */
size_t hid_fill_buffer(uint8_t *buffer, uint8_t report_id, const void *data,
			  size_t data_len)
{
	size_t response_len = I2C_HID_HEADER_SIZE + data_len;

	buffer[0] = response_len & 0xFF; // report len lsb. I2C_HID_HEADER
	buffer[1] = (response_len >> 8) & 0xFF; // rpt len msb. I2C_HID_HEADER
	memcpy(buffer + I2C_HID_HEADER_SIZE, data, data_len);
#ifdef CONFIG_ACCEL_SPOOF_MODE
	memcpy(spoof_host_buffer + I2C_HID_HEADER_SIZE, data, data_len);
#endif
	return response_len;
}

/* Fill input report struct with data so that hid_fill_buffer can use it*/
int hid_compile_input(int report_id)
{
	struct motion_sensor_t *sensor;
	int i;

	sensor = hid_host_sensor_id_to_real_sensor(report_id);
	ccprintf("In hid compile input after spoofing x, y, z %d, %d, %d\n",
	sensor->xyz[X], sensor->xyz[Y], sensor->xyz[Z]);
	if (sensor == NULL)
		return EC_RES_INVALID_PARAM;
	input.report_id = report_id;
	input.sensor_state = 0;
	input.sensor_event = 0;
	input.x = sensor->xyz[X];
	input.y = sensor->xyz[Y];
	input.z = sensor->xyz[Z];
	ccprintf("In hid compile input: Get sensor x, y, z %d, %d, %d\n",
	input.x, input.y, input.z);
	for (i = 0; i < 2 ; i++)
		input_reports[i] = input; // input reports are x2 buffered
	return 0;

}

#ifdef CONFIG_ACCEL_SPOOF_MODE
struct motion_sensor_t *get_spoof_sensor(void)
{
	return spoof_sensor;
}

/* Dummy fn for tests. Defined at chip level*/
void send_response(int len)
{
	ccprintf("Dummy Send Response method for tests:\n");
}

int get_new_odr(struct motion_sensor_t *sensor)
{
	// Check if SET_REPORT sets the new sensor odr
	return sensor->config[SENSOR_CONFIG_AP].odr;
}
#endif

/* Extracts report data from |buffer| into |data|.
 *
 * |buffer| is expected to contain the values written to the command register
 * followed by the values written to the data register, upon receiving a
 * SET_REPORT command, in the following byte sequence format:
 *
 *   00 30 - command register address (0x3000)
 *   xx    - report type and ID
 *   03    - SET_REPORT
 *   00 30 - data register address (0x3000)
 *   xx xx - length
 *   xx    - report ID
 *   xx... - report data
 *
 * Note that command register and data register have the same address. Also,
 * any report ID >= 15 requires an extra byte after the SET_REPORT byte, which
 * is not supported here as we don't have any report ID >= 15.
 *
 * In summary, we expect |buffer| contains at least 10 bytes where the report
 * data starts at buffer[9]. If |buffer| contains the incorrect number bytes,
 * we ignore the report.
 */
int extract_report(uint64_t len, uint8_t *buffer, void *data,
			   uint64_t data_len)
{
	if (len == 9 + data_len)
		memcpy(data, buffer + 9, data_len);
#ifdef CONFIG_ACCEL_SPOOF_MODE
		memcpy(data, spoof_host_buffer + 9, data_len);
#endif
		ccprintf("Data set in extract report%d\n", *((int *)data));
		return *((int *)data);
}

/* Function to map hid report IDs to motion sensor. */
struct motion_sensor_t
	*hid_host_sensor_id_to_real_sensor(int report_id)
{
	struct motion_sensor_t *sensor;
	int report_id_mapped;

	if (report_id > motion_sensor_count || report_id == 0)
		return NULL;
	/* As we can't use HID report ID of 0, we map to motion_sensors[] by
	 * subtracting 1 from the report ID
	 */
	report_id_mapped = report_id - 1;
	ccprintf("Report ID afer mapping %d\n", report_id_mapped);
#ifdef CONFIG_ACCEL_SPOOF_MODE
	sensor = get_spoof_sensor();
#else
	sensor = &motion_sensors[report_id_mapped];
	/* if sensor is powered and initialized, return match */
#ifdef SENSOR_ACTIVE
	if (SENSOR_ACTIVE(sensor) && (sensor->state == SENSOR_INITIALIZED))
		return sensor;
#endif /* defined(SENSOR_ACTIVE) */
#endif
	/* If no match then the EC currently doesn't support ID received. */
	return sensor;
}


void i2c_hid_process(int data_len, uint8_t *buffer,
		     void (*send_response)(int len))
{
	uint16_t reg;
	size_t response_len;

	if (data_len < 2)
		reg = INPUT_REPORT_REGISTER;
	else
		reg = *((uint16_t*)buffer);

	ccprintf("Processing  %x - %d\n", reg, data_len);
	switch (reg) {
	case EC_ACPI_HID_DESCRIPTOR_ADDR:
		/* Return HID descr to incompatible types when assigning to typehost */
		memcpy(buffer, &hid_desc, sizeof(hid_desc));
#ifdef CONFIG_ACCEL_SPOOF_MODE
		memcpy(spoof_host_buffer, &hid_desc, sizeof(hid_desc));
#endif
		send_response(sizeof(hid_desc));
		break;

	case REPORT_DESC_REGISTER:
		/* Return Report descr to host */
		ccprintf("Retrieve REPORT_DESC_REGISTER %x\n", reg);
		memcpy(buffer, &report_desc, sizeof(report_desc));
#ifdef CONFIG_ACCEL_SPOOF_MODE
		memcpy(spoof_host_buffer, &report_desc, sizeof(report_desc));
#endif
		send_response(sizeof(report_desc));
		break;

	case INPUT_REPORT_REGISTER:
		/* Return input report to host */
		// Need to add code to check if reset is pending. Not sure of GPIO used
		hid_compile_input(REPORT_ID_BASE_ACCEL);
		response_len = hid_fill_buffer(buffer, REPORT_ID_BASE_ACCEL,
				&input_reports[0],
				sizeof(struct hid_accel_input_report));
		ccprintf("&input_reports[0] x:%d\n", input_reports[0].x);
		ccprintf("&input_reports[0] y:%d\n", input_reports[0].y);
		ccprintf("&input_reports[0] z:%d\n", input_reports[0].z);

		send_response(response_len);
		break;
		//gpio_set_level(GPIO_INT_L, 1);

		/* Process cmd from host */
	case COMMAND_REGISTER:
		ccprintf("Inside i2c hid process fn: data set is: %.*h\n",
			 data_len, buffer);
		hid_command_process(data_len, buffer, send_response);
		break;
	default:
		// Ignore invalid register.
		return;
	}
}

int hid_command_process(int len, uint8_t *buffer,
				   void (*send_response)(int len))
{
	uint8_t command = buffer[3] & 0x0F;
	uint8_t report_type_id = buffer[2];
	uint8_t rpt_id = report_type_id & 0x0F;
	size_t response_len;
	uint32_t data_set;
	struct motion_sensor_t *sensor;
	int ret = EC_RES_INVALID_PARAM;

	switch (command) {
	case I2C_HID_CMD_SET_POWER:
		ccprintf("I2C-HID: SET_POWER %s\n",
			 buffer[2] & 0x3 ? "Sleep": "ON");
		/* Set Power is a NOOP for now. */
		send_response(0);
		break;
	case I2C_HID_CMD_RESET:
		ccprintf("I2C-HID: RESET\n");
		/* Reset is a NOOP for now. */
		send_response(0);
		break;
	/* For both input and feature reports */
	case I2C_HID_CMD_GET_REPORT:
		ccprintf("I2C-HID: command get_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL:
			hid_compile_input(REPORT_ID_BASE_ACCEL);

			response_len = hid_fill_buffer(
					buffer, REPORT_ID_BASE_ACCEL,
					&input_reports[0],
					sizeof(struct hid_accel_input_report));
			ccprintf("&input_reports[0] x, y, z: %d %d %d\n",
				 input_reports[0].x,
				 input_reports[0].y,
				 input_reports[0].z);
			break;
		default:
			response_len = 2;
			buffer[0] = response_len;
			buffer[1] = 0;
			break;
		}

		send_response(response_len);
		break;
	case I2C_HID_CMD_SET_REPORT:
		ccprintf("I2C-HID: command set_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL_SAMPLING_RATE: {
			uint8_t sensor_rpt =
				hid_get_sensorid_from_featureid(rpt_id);

			data_set = extract_report(len, buffer, &base_accel_odr,
				       sizeof(base_accel_odr));
			sensor =
				hid_host_sensor_id_to_real_sensor(sensor_rpt);
			ccprintf("New ODR set %d\n", data_set);
			ccprintf("ODR place holder var: %d\n", base_accel_odr);
			ccprintf("Sensor report id: %d\n", sensor_rpt);

			if (sensor == NULL) {
				ret = EC_RES_INVALID_PARAM;
				break;
			}
			/* Set new data rate */

			/* To be sure timestamps are calculated
			 * properly, send an event to have a
			 * timestamp inserted in the FIFO.
			 */
#ifdef CONFIG_ACCEL_FIFO
			motion_sense_insert_timestamp();
#endif
			sensor->config[SENSOR_CONFIG_AP].odr = base_accel_odr;

			ret = motion_sense_set_data_rate(sensor);
			if (ret != EC_SUCCESS)
				break;

			/* The new ODR may suspend sensor, leaving
			 * samples in the FIFO. Flush it explicitly.
			 */
			task_set_event(TASK_ID_MOTIONSENSE,
				TASK_EVENT_MOTION_ODR_CHANGE, 0);

			/*
			 * If the sensor was suspended before, or now
			 * suspended, we have to recalculate the EC
			 * sampling rate
			 */
			motion_sense_set_motion_intervals();
			break;
		}
		default:
			ret = EC_RES_INVALID_PARAM;
		}
		send_response(ret * -1);
		break;
	default:
		ccprintf("I2C-HID: unknown command %d\n", command);
		send_response(EC_RES_INVALID_PARAM * -1);
	}
	return 0;
}

