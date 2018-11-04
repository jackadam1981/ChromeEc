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
#define UNIT_TEST_MODE
#undef BOARD_MODE

/* Reports (double buffered) */
struct hid_accel_input_report input_reports[2];
struct hid_accel_feature_report feature_reports[2];

/* Current active report buffer index */
int report_active_index;

/* Sensor odr */
uint32_t base_accel_odr;

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

#ifdef CONFIG_ORIENTATION_SENSOR
/*
 * Orientation mode vectors, must match sequential ordering of
 * known orientations from enum motionsensor_orientation
 */
const vector_3_t orientation_modes[] = {
	[MOTIONSENSE_ORIENTATION_LANDSCAPE] = { 0, -1, 0 },
	[MOTIONSENSE_ORIENTATION_PORTRAIT] = { 1, 0, 0 },
	[MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_PORTRAIT] = { -1, 0, 0 },
	[MOTIONSENSE_ORIENTATION_UPSIDE_DOWN_LANDSCAPE] = { 0, 1, 0 },
};
#endif

/*
 * Sampling interval for measuring acceleration and calculating lid angle.
 */
test_export_static unsigned int motion_interval_hid;

/* Delay between FIFO interruption. */
#ifdef CONFIG_CMD_ACCELS
static unsigned int motion_int_interval;
#endif

#ifdef CONFIG_CMD_ACCEL_INFO
static int accel_disp;
#endif

#define SENSOR_ACTIVE(_sensor) (sensor_active_hid & (_sensor)->active_mask)

#if defined(CONFIG_HOSTCMD_X86) || defined(TEST_MOTION_LID)
#define UPDATE_HOST_MEM_MAP
#endif

/*
 * Adjustment in us to ec rate when calculating interrupt interval:
 * To be sure the EC will send an interrupt even if it finishes processing
 * events slightly earlier than the previous period.
 */
#define MOTION_SENSOR_INT_ADJUSTMENT_US 10

/*
 * Mutex to protect sensor values between host command task and
 * motion sense task:
 * When we process CMD_DUMP, we want to be sure the motion sense
 * task is not updating the sensor values at the same time.
 */
 #ifdef BOARD_MODE
static struct mutex g_sensor_mutex;
#endif
/*
 * Current power level (S0, S3, S5, ...)
 */
test_export_static enum chipset_state_mask sensor_active_hid;

#ifdef CONFIG_ACCEL_SPOOF_MODE
static void print_spoof_mode_status(int id);
#endif /* defined(CONFIG_ACCEL_SPOOF_MODE) */

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

size_t hid_fill_buffer(uint8_t *buffer, uint8_t report_id, const void *data,
			  size_t data_len)
{
	size_t response_len = I2C_HID_HEADER_SIZE + data_len;

	//cprintf("data len inside fill buffer %s\n",data_len);
	buffer[0] = response_len & 0xFF;
	buffer[1] = (response_len >> 8) & 0xFF;
	buffer[2] = report_id;
	memcpy(buffer + I2C_HID_HEADER_SIZE, data, data_len);
	return response_len;
}

#ifdef BOARD_MODE
/* Fill input report struct with data so that fill_buffer can use it*/
static int hid_compile_input(int report_id)
{
	struct motion_sensor_t *sensor;
	struct hid_accel_input_report *input;

	sensor = hid_host_sensor_id_to_real_sensor(report_id);
	if (sensor == NULL)
		return EC_RES_INVALID_PARAM;
	mutex_lock(&g_sensor_mutex);

	input->x = sensor->xyz[X];
	input->y = sensor->xyz[Y];
	input->z = sensor->xyz[Z];

	input_reports[0] = *input;
	return 0;

}
#endif

/* Fill input report struct with data so that fill_buffer can use it*/
void hid_compile_dummy_input(struct motion_sensor_t *sensor)
{
	//hid_accel_input_report *input;
	ccprintf("In hid compile dummy input");

	input.x = sensor->xyz[X];
	input.y = sensor->xyz[Y];
	input.z = sensor->xyz[Z];

	input_reports[0] = input;
	ccprintf("Check value set: input_reports[0] x %d\n", input.x);
	ccprintf("Check value set: input_reports[0] y %d\n", input.y);
	ccprintf("Check value set: input_reports[0] z %d\n", input.z);

}

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
	/* As we can't use report ID of 0, we map to motion_sensors[] by
	 * subtracting 1 from the report ID
	 */
	report_id_mapped = report_id - 1;
	ccprintf("Report ID afer mapping %d\n", report_id_mapped);
	sensor = &motion_sensors[report_id_mapped];

	/* if sensor is powered and initialized, return match */
	if (SENSOR_ACTIVE(sensor) && (sensor->state == SENSOR_INITIALIZED))
		return sensor;

	/* If no match then the EC currently doesn't support ID received. */
	return sensor;
}

// check that response in i2c buffer matches report_descr exactly
int compareReportDescResponse(uint8_t *buffer, const uint8_t *report_desc)
{
	int i, j;

	ccprintf("Inside compare ReportDesc Response:\n");
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

int i2c_hid_process(int data_len, uint8_t *buffer,
			void (*send_response)(int len))
{
	int reg;
	size_t response_len;
	uint32_t data;

	if (data_len == 0)
		reg = INPUT_REPORT_REGISTER;
	else
		reg = buffer[1] << 8 | buffer[0];

	switch (reg) {
	/* Return HID descr to incompatible types when assigning to typehost */
	case HID_DESC_REGISTER:
		memcpy(buffer, &hid_desc, sizeof(hid_desc));
#ifdef BOARD_MODE
		send_response(sizeof(hid_desc));
#endif
		return 0;
	/* Return Report descr to host */
	case REPORT_DESC_REGISTER:
		ccprintf("Retrieve REPORT_DESC_REGISTER\n");
		memcpy(buffer, &report_desc, sizeof(report_desc));
#ifdef BOARD_MODE
		send_response(sizeof(report_desc));
#endif
		return compareReportDescResponse(buffer, report_desc);
	/* Return input report to host */
	case INPUT_REPORT_REGISTER:
	// Need to add code to check if reset is pending. Not sure of GPIO used
#ifdef BOARD_MODE
		hid_compile_input(REPORT_ID_BASE_ACCEL);
#endif
		response_len = hid_fill_buffer(buffer, REPORT_ID_BASE_ACCEL,
				&input_reports[0],
				sizeof(struct hid_accel_input_report));
		ccprintf("&input_reports[0] x:%d\n", input_reports[0].x);
		ccprintf("&input_reports[0] y:%d\n", input_reports[0].y);
		ccprintf("&input_reports[0] z:%d\n", input_reports[0].z);
#ifdef BOARD_MODE
		send_response(response_len);
#endif
		//gpio_set_level(GPIO_INT_L, 1);
		return response_len;
	/* Process cmd from host */
	case COMMAND_REGISTER:
		ccprintf("Inside i2c hid process fn: data set is:%d\n", data);
		data = i2c_hid_command_process(data_len, buffer, send_response);
		return data;
	default:
		// Ignore invalid register.
		return 0;
	}
}

int i2c_hid_command_process(int len, uint8_t *buffer,
				   void (*send_response)(int len))
{
	uint8_t command = buffer[3] & 0x0F;
	uint8_t report_type_id = buffer[2];
	uint8_t rpt_id = report_type_id & 0x0F;
	size_t response_len;
	uint8_t sensor_rpt_id = 0;
	uint32_t data_set;
	struct motion_sensor_t *sensor;
	int ret = EC_RES_INVALID_PARAM;

	switch (command) {
	case I2C_HID_CMD_RESET:
		ccprintf("I2C-HID: command reset\n");
		//hid_compile_input(rpt_id);
		// Need to implement this

		return 0;
	/* For both input and feature reports */
	case I2C_HID_CMD_GET_REPORT:
		ccprintf("I2C-HID: command get_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL:
#ifdef BOARD_MODE
		hid_compile_input(REPORT_ID_BASE_ACCEL);
#endif
		response_len = hid_fill_buffer(buffer, REPORT_ID_BASE_ACCEL,
				&input_reports[0],
				sizeof(struct hid_accel_input_report));
		ccprintf("&input_reports[0] x:%d\n", input_reports[0].x);
		ccprintf("&input_reports[0] y:%d\n", input_reports[0].y);
		ccprintf("&input_reports[0] z:%d\n", input_reports[0].z);
		return response_len;
		default:
			response_len = 2;
			buffer[0] = response_len;
			buffer[1] = 0;
			return 0;
		}
#ifdef BOARD_MODE
		send_response(response_len);
#endif
		return 0;
	case I2C_HID_CMD_SET_REPORT:
		ccprintf("I2C-HID: command set_report (%04x)\n", rpt_id);
		switch (rpt_id) {
		case REPORT_ID_BASE_ACCEL_SAMPLING_RATE:
			data_set = extract_report(len, buffer, &base_accel_odr,
				       sizeof(base_accel_odr));
			sensor_rpt_id = hid_get_sensorid_from_featureid(rpt_id);
			sensor =
			hid_host_sensor_id_to_real_sensor(sensor_rpt_id);
			ccprintf("data_set %d\n", data_set);
			ccprintf("base_accel_odr %d\n", base_accel_odr);
			ccprintf("sensor_report_id: %d\n", sensor_rpt_id);


			if (sensor == NULL)
				return EC_RES_INVALID_PARAM;

			/* Set new data rate */
			if (data_set == base_accel_odr) {
				/* To be sure timestamps are calculated
				 * properly, send an event to have a
				 * timestamp inserted in the FIFO.
				 */
#ifdef CONFIG_ACCEL_FIFO
				motion_sense_insert_timestamp();
#endif
				sensor->config[SENSOR_CONFIG_AP].odr =
				base_accel_odr;

				ret = motion_sense_set_data_rate(sensor);
				if (ret != EC_SUCCESS)
					return EC_RES_INVALID_PARAM;

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
				return base_accel_odr;
		}
			return 0;
		default:
			return 0;
		}
		return 0;

	case I2C_HID_CMD_SET_POWER:
		// Dummy call to avoid compile errors
		i2c_hid_process(sizeof(buffer), buffer, send_response);
		return 0;

	}
	return 0;
}


/*****************************************************************************/
/* Console commands */
#ifdef CONFIG_CMD_ACCELS
static int command_accelrange_hid(int argc, char **argv)
{
	char *e;
	int id, data, round = 1;
	struct motion_sensor_t *sensor;

	if (argc < 2 || argc > 4)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);
	if (*e || id < 0 || id >= motion_sensor_count)
		return EC_ERROR_PARAM1;

	sensor = &motion_sensors[id];

	if (argc >= 3) {
		/* Second argument is data to write. */
		data = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (argc == 4) {
			/* Third argument is rounding flag. */
			round = strtoi(argv[3], &e, 0);
			if (*e)
				return EC_ERROR_PARAM3;
		}

		/*
		 * Write new range, if it returns invalid arg, then return
		 * a parameter error.
		 */
		if (sensor->drv->set_range(sensor,
					   data,
					   round) == EC_ERROR_INVAL)
			return EC_ERROR_PARAM2;
	} else {
		ccprintf("Range for sensor %d: %d\n", id,
			 sensor->drv->get_range(sensor));
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelrangehid, command_accelrange_hid,
	"id [data [roundup]]",
	"Read or write accelerometer range");

static int command_accelresolution_hid(int argc, char **argv)
{
	char *e;
	int id, data, round = 1;
	struct motion_sensor_t *sensor;

	if (argc < 2 || argc > 4)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);
	if (*e || id < 0 || id >= motion_sensor_count)
		return EC_ERROR_PARAM1;

	sensor = &motion_sensors[id];

	if (argc >= 3) {
		/* Second argument is data to write. */
		data = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (argc == 4) {
			/* Third argument is rounding flag. */
			round = strtoi(argv[3], &e, 0);
			if (*e)
				return EC_ERROR_PARAM3;
		}

		/*
		 * Write new resolution, if it returns invalid arg, then
		 * return a parameter error.
		 */
		if (sensor->drv->set_resolution &&
		    sensor->drv->set_resolution(sensor, data, round)
			== EC_ERROR_INVAL)
			return EC_ERROR_PARAM2;
	} else {
		ccprintf("Resolution for sensor %d: %d\n", id,
			 sensor->drv->get_resolution(sensor));
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelreshid, command_accelresolution_hid,
	"id [data [roundup]]",
	"Read or write accelerometer resolution");

static int command_accel_data_rate_hid(int argc, char **argv)
{
	char *e;
	int id, data, round = 1, ret;
	struct motion_sensor_t *sensor;
	enum sensor_config config_id;

	if (argc < 2 || argc > 4)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);
	if (*e || id < 0 || id >= motion_sensor_count)
		return EC_ERROR_PARAM1;

	sensor = &motion_sensors[id];

	if (argc >= 3) {
		/* Second argument is data to write. */
		data = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		if (argc == 4) {
			/* Third argument is rounding flag. */
			round = strtoi(argv[3], &e, 0);
			if (*e)
				return EC_ERROR_PARAM3;
		}

		/*
		 * Take ownership of the sensor and
		 * Write new data rate, if it returns invalid arg, then
		 * return a parameter error.
		 */
		config_id = motion_sense_get_ec_config();
		sensor->config[SENSOR_CONFIG_AP].odr = 0;
		sensor->config[config_id].odr =
			data | (round ? ROUND_UP_FLAG : 0);
		ret = motion_sense_set_data_rate(sensor);
		if (ret)
			return EC_ERROR_PARAM2;
		/* Sensor might be out of suspend, check the ec_rate */
		motion_sense_set_motion_intervals();
	} else {
		ccprintf("Data rate for sensor %d: %d\n", id,
			 sensor->drv->get_data_rate(sensor));
		ccprintf("EC rate for sensor %d: %d\n", id,
			 motion_sense_ec_rate(sensor));
		ccprintf("Current EC rate: %d\n", motion_interval_hid);
		ccprintf("Current Interrupt rate: %d\n", motion_int_interval);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelratehid, command_accel_data_rate_hid,
	"id [data [roundup]]",
	"Read or write accelerometer ODR");

static int command_accel_read_xyz_hid(int argc, char **argv)
{
	char *e;
	int id, n = 1, ret;
	struct motion_sensor_t *sensor;
	vector_3_t v;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);

	if (*e || id < 0 || id >= motion_sensor_count)
		return EC_ERROR_PARAM1;

	if (argc >= 3)
		n = strtoi(argv[2], &e, 0);

	sensor = &motion_sensors[id];

	while ((n == -1) || (n-- > 0)) {
		ret = sensor->drv->read(sensor, v);
		if (ret == 0)
			ccprintf("Current data %d: %-5d %-5d %-5d\n",
				 id, v[X], v[Y], v[Z]);
		else
			ccprintf("vector not ready\n");
		ccprintf("Last calib. data %d: %-5d %-5d %-5d\n",
			 id, sensor->xyz[X], sensor->xyz[Y], sensor->xyz[Z]);
		task_wait_event(motion_min_interval);
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(accelreadhid, command_accel_read_xyz_hid,
	"id [n]",
	"Read sensor x/y/z");

static int command_accel_init_hid(int argc, char **argv)
{
	char *e;
	int id, ret;
	struct motion_sensor_t *sensor;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);

	if (*e || id < 0 || id >= motion_sensor_count)
		return EC_ERROR_PARAM1;

	sensor = &motion_sensors[id];
	ret = motion_sense_init(sensor);

	ccprintf("%s: state %d - %d\n", sensor->name, sensor->state, ret);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelinithid, command_accel_init_hid,
	"id",
	"Init sensor");

#ifdef CONFIG_CMD_ACCEL_INFO
static int command_display_accel_info_hid(int argc, char **argv)
{
	char *e;
	int val;

	if (argc > 3)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is on/off whether to display accel data. */
	if (argc > 1) {
		if (!parse_bool(argv[1], &val))
			return EC_ERROR_PARAM1;

		accel_disp = val;
	}

	/*
	 * Second arg changes the accel task time interval. Note accel
	 * sampling interval will be clobbered when chipset suspends or
	 * resumes.
	 */
	if (argc > 2) {
		val = strtoi(argv[2], &e, 0);
		if (*e)
			return EC_ERROR_PARAM2;

		motion_interval_hid = val * MSEC;
		task_wake(TASK_ID_MOTIONSENSE);

	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelinfohid, command_display_accel_info_hid,
	"on/off [interval]",
	"Print motion sensor info, lid angle calculations"
	" and set calculation frequency.");
#endif /* CONFIG_CMD_ACCEL_INFO */

#ifdef CONFIG_CMD_ACCEL_FIFO
static int motion_sense_read_fifo_hid(int argc, char **argv)
{
	int count, i;
	struct ec_response_motion_sensor_data v;

	if (argc < 1)
		return EC_ERROR_PARAM_COUNT;

	/* Limit the amount of data to avoid saturating the UART buffer */
	count = MIN(queue_count(&motion_sense_fifo), 16);
	for (i = 0; i < count; i++) {
		queue_peek_units(&motion_sense_fifo, &v, i, 1);
		if (v.flags & (MOTIONSENSE_SENSOR_FLAG_TIMESTAMP |
			       MOTIONSENSE_SENSOR_FLAG_FLUSH)) {
			uint64_t timestamp;

			memcpy(&timestamp, v.data, sizeof(v.data));
			ccprintf("Timestamp: 0x%016lx%s\n", timestamp,
				 (v.flags & MOTIONSENSE_SENSOR_FLAG_FLUSH ?
				  " - Flush" : ""));
		} else {
			ccprintf("%d %d: %-5d %-5d %-5d\n", i, v.sensor_num,
				 v.data[X], v.data[Y], v.data[Z]);
		}
	}
	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(fiforeadhid, motion_sense_read_fifo_hid,
	"id",
	"Read Fifo sensor");
#endif /* defined(CONFIG_CMD_ACCEL_FIFO) */
#endif /* CONFIG_CMD_ACCELS */

#ifdef CONFIG_ACCEL_SPOOF_MODE
static void print_spoof_mode_status(int id)
{
	CPRINTS("Sensor %d spoof mode is %s. <%d, %d, %d>", id,
		motion_sensors[id].in_spoof_mode ? "enabled" : "disabled",
		motion_sensors[id].spoof_xyz[X],
		motion_sensors[id].spoof_xyz[Y],
		motion_sensors[id].spoof_xyz[Z]);
}

#ifdef CONFIG_CMD_ACCELSPOOF
static int command_accelspoof_hid(int argc, char **argv)
{
	char *e;
	int id, enable, i;
	struct motion_sensor_t *s;

	/* There must be at least 1 parameter, the sensor id. */
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* First argument is sensor id. */
	id = strtoi(argv[1], &e, 0);
	if (id >= motion_sensor_count || id < 0)
		return EC_ERROR_PARAM1;

	s = &motion_sensors[id];

	/* Print the sensor's current spoof status. */
	if (argc == 2)
		print_spoof_mode_status(id);

	/* Enable/Disable spoof mode. */
	if (argc >= 3) {
		if (!parse_bool(argv[2], &enable))
			return EC_ERROR_PARAM2;

		if (enable) {
			/*
			 * If no components are provided, we'll just use the
			 * current values as the spoofed values.  But if the
			 * components are provided, use the provided ones as the
			 * spoofed ones.
			 */
			if (argc == 6) {
				for (i = 0; i < 3; i++)
					s->spoof_xyz[i] = strtoi(argv[3 + i],
								 &e, 0);
			} else if (argc == 3) {
				for (i = X; i <= Z; i++)
					s->spoof_xyz[i] = s->raw_xyz[i];
			} else {
				/* It's either all or nothing. */
				return EC_ERROR_PARAM_COUNT;
			}
		}
		s->in_spoof_mode = enable;
		print_spoof_mode_status(id);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(accelspoofhid, command_accelspoof_hid,
			"id [on/off] [X] [Y] [Z]",
			"Enable/Disable spoofing of sensor readings.");
#endif /* defined(CONIFG_CMD_ACCELSPOOF) */
#endif /* defined(CONFIG_ACCEL_SPOOF_MODE) */
