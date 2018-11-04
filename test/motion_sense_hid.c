#include <math.h>
#include <stdio.h>
#include <string.h>

#include "accelgyro.h"
#include "common.h"
#include "config.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c_hid.h"
//#include "i2c_over_lpc.h"
#include "motion_sense.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

/* HID-specific headers */
#include "i2c_hid.h"

//#define CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN 64
//static uint8_t buffer[CONFIG_NPCX_I2C_OVER_LPC_MSG_LEN];
//static uint8_t *buffer_ptr = &buffer;

#define TEST_LID_EC_RATE (10 * MSEC)

/* Reports (double buffered) */
struct hid_accel_input_report input_reports[2];
struct hid_accel_feature_report feature_reports[2];

/* Current active report buffer index */
int report_active_index;

/* Sensor odr */
uint32_t base_accel_odr;


/* Usage ID, Usage page. Implementing sensors as separate TLCs. Max size 64kb */
static const uint8_t report_desc[] = {
// input reports (transmit)
        0x05, 0x20,                    /* Usage Page (Sensors) */
        0x09, 0x73,                    /* Usage Sensor Type (3D Accel) */
        // Report ID for accel
        0x85, REPORT_ID_BASE_ACCEL,      /* Report ID (3DAccel) */
        0x19, 0x01,                    /* HID_USAGE_MIN_8 */
        0x29, 0x02,                    /* HID_USAGE_MAX_8 */
        0xA1, 0x01,                    /* Collection (Application: Accel TLC) */
        // 1. Sensor state
        0x0A, 0x01, 0x02,                 /* HID_USAGE_SENSOR_STATE */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x25, 6,                          /* HID_LOGICAL_MAX_8*/
        0x075, 8,                         /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT */
        0xA1, 0x02,                       /* HID_COLLECTION, (Logical) */
        0x0A, 0x00, 0x08,                      /* SENSOR_STATE_UNKNOWN*/
        0x0A, 0x01, 0x08,                      /* SENSOR_STATE_READY*/
        0x0A, 0x02, 0x08,                      /* SENSOR_STATE_NOT_AVAILABLE*/
        0x0A, 0x03, 0x08,                      /* SENSOR_STATE_NO_DATA */
        0x0A, 0x04, 0x08,                      /* SENSOR_STATE_INITIALIZING*/
        0x0A, 0x05, 0x08,                      /* SENSOR_STATE_ACCESS_DENIED,*/
        0x0A, 0x06, 0x08,                      /* SENSOR_STATE_ERROR*/
        0x81, 0x03,                            /* HID_INPUT(Const_Arr_Abs) */
        0xC0,                             /* HID_END_COLLECTION*/
        // 2. Sensor event
        0x0A,0x02,0x02,                   /* HID_USAGE_SENSOR_EVENT */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x25, 16,                         /* HID_LOGICAL_MAX_8*/
        0x075, 8,                         /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT */
        0xA1, 0x02,                       /* HID_COLLECTION, (Logical) */
        0x0A,0x10,0x08,                        /* SENSOR_EV_UNKNOWN */
        0x0A,0x11,0x08,                        /* SENSOR_EV_STATE_CHANGED */
        0x0A,0x12,0x08,                        /* SENSOR_EV_PROPERTY_CHANGED */
        0x0A,0x13,0x08,                        /* SENSOR_EV_DATA_UPDATED */
        0x0A,0x14,0x08,                        /* SENSOR_EV_POLL_RESPONSE */
        0x0A,0x15,0x08,                        /* SENSOR_EV_CHANGE_SENSITIVITY*/
        0x0A,0x16,0x08,                        /* SENSOR_EV_MAX_REACHED */
        0x0A,0x17,0x08,                        /* SENSOR_EV_MIN_REACHED */
        0x0A,0x18,0x08,                        /* SENSOR_EV_HIGH_THRESHOLD_CROSS_UPWARD*/
        0x0A,0x19,0x08,                        /* SENSOR_EV_HIGH_THRESHOLD_CROSS_DOWNWARD*/
        0x0A,0x1A,0x08,                        /* SENSOR_EV_LOW_THRESHOLD_CROSS_UPWARD*/
        0x0A,0x1B,0x08,                        /* SENSOR_EV_LOW_THRESHOLD_CROSS_DOWNWARD*/
        0x0A,0x1C,0x08,                        /* SENSOR_EV_ZERO_THRESHOLD_CROSS_UPWARD*/
        0x0A,0x1D,0x08,                        /* SENSOR_EV_ZERO_THRESHOLD_CROSS_DOWNWARD*/
        0x0A,0x1E,0x08,                         /* SENSOR_EV_PERIOD_EXCEEDED */
        0x0A,0x1F,0x08,                         /* SENSOR_EV_FREQUENCY_EXCEEDED*/
        0x0A,0x20,0x08,                         /* SENSOR_EV_COMPLEX_TRIGGER */
        0x81, 0x03,                            /* HID_INPUT(Const_Arr_Abs) */
        0xC0,                             /* HID_END_COLLECTION*/
        // 3. X, Y, Z axis accel readings
        0x0A, 0x53, 0x04,                 /* MOTION_ACCELERATION_X_AXIS*/
        0x0A,0x54,0x04,                   /* MOTION_ACCELERATION_Y_AXIS */
        0x0A,0x55,0x04,                   /* MOTION_ACCELERATION_Z_AXIS*/
        0x16, 0x01, 0x80,                 /* LOGICAL_MINIMUM (-32767)*/
        0x2A,0xFF,0x7F,                   /* LOGICAL_MAXIMUM (32767)*/
        0x075,16,                         /* HID_REPORT_SIZE */
        0x95, 3,                          /* HID_REPORT_COUNT */
        0x55,0x0E,                        /* HID_UNIT_EXPONENT*/
        0x81, 0x02,                       /* HID_INPUT(Const_Arr_Abs) */

// feature reports (xmit/receive)
        // 1. Reporting state
        0x0A, 0x16, 0x03,                 /*SENSOR_PROPERTY_REPORTING_STATE*/
        0x85, REPORT_ID_BASE_ACCEL_REPORTING_STATE, /* Report ID */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x25, 5,                          /* HID_LOGICAL_MAX_8*/
        0x075, 8,                         /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT */
        0xA1, 0x02,                       /* HID_COLLECTION, (Logical) */
        0x0A,0x40,0x08,                        /* REPORTING_STATE_NO_EVENTS */
        0x0A,0x41,0x08,                        /* REPORTING_STATE_ALL_EVENTS*/
        0x0A,0x42,0x08,                        /* REPORTING_STATE_THRESHOLD_EVENTS*/
        0x0A,0x43,0x08,                        /* REPORTING_STATE_NO_EVENTS_WAKE*/
        0x0A,0x44,0x08,                        /* REPORTING_STATE_ALL_EVENTS_WAKE*/
        0x0A,0x45,0x08,                        /* REPORTING_STATE_THRESHOLD_EVENTS_WAKE*/
        0xB1,0x02,                             /* HID_FEATURE(Data_Arr_Abs)*/
        0xC0,                             /* HID_END_COLLECTION*/
        // 2. Power state
        0x0A, 0x19, 0x03,                 /*SENSOR_PROPERTY_POWER_STATE*/
        0x85, REPORT_ID_BASE_ACCEL_POWER_STATE, /* Report ID */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x25, 5,                          /* HID_LOGICAL_MAX_8*/
        0x075, 8,                         /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT */
        0xA1, 0x02,                       /* HID_COLLECTION, (Logical) */
        0x0A,0x50,0x08,                        /* POWER_STATE_UNDEFINED  */
        0x0A,0x51,0x08,                        /* POWER_STATE_D0_FULL_POWER */
        0x0A,0x52,0x08,                        /* POWER_STATE_D1_LOW_POWER*/
        0x0A,0x53,0x08,                        /* POWER_STATE_D2_STANDBY_WITH_WAKE*/
        0x0A,0x54,0x08,                        /* POWER_STATE_D3_SLEEP_WITH_WAKE */
        0x0A,0x55,0x08,                        /* POWER_STATE_D4_POWER_OFF */
        0xB1,0x02,                             /* HID_FEATURE(Data_Arr_Abs)*/
        0xC0,                             /* HID_END_COLLECTION*/
        // 3. Change sensitivity
        0x0A,0x0F,0x03,                   /* SENSOR_PROPERTY_CHANGE_SENSITIVITY_ABS*/
        0x85, REPORT_ID_BASE_ACCEL_CHANGE_SENSITIVITY, /* Report ID */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x26,0xFF,0xFF,                   /* LOGICAL_MAX_16*/
        0x75,16,                          /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT*/
        0x55,0x0E,                        /* HID_UNIT_EXPONENT*/
        0xB1,0x02,                        /* HID_FEATURE(Data_Arr_Abs)*/
        // 4. Sensor status
        0x0A,0x03,0x03,                   /* SENSOR_PROPERTY_SENSOR_STATUS */
        0x85, REPORT_ID_BASE_ACCEL_SENSOR_STATUS, /* Report ID */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x55, 0xFF,0xFF,0xFF,0xFF,        /* HID_LOGICAL_MAX_32 */
        0x75,32,                          /* HID_REPORT_SIZE */
        0xB1,0x02,                        /* HID_FEATURE(Data_Arr_Abs)*/
        // 5. Sampling rate/ odr
        0x0A,0x17,0x03,                   /* SENSOR_PROPERTY_REPORT_INTERVAL*/
        0x85, REPORT_ID_BASE_ACCEL_SAMPLING_RATE, /* Report ID */
        0x15, 0,                          /* HID_LOGICAL_MIN_8 */
        0x55, 0xFF,0xFF,0xFF,0xFF,        /* HID_LOGICAL_MAX_32 */
        0x75,32,                          /* HID_REPORT_SIZE */
        0x95, 1,                          /* HID_REPORT_COUNT*/
        0x55, 0,                          /* HID_UNIT_EXPONENT*/
        0xB1,0x02,                        /* HID_FEATURE(Data_Arr_Abs)*/

};

/* Mock functions */
static int accel_init(const struct motion_sensor_t *s)
{
	return EC_SUCCESS;
}

static int accel_read(const struct motion_sensor_t *s, intv3_t v)
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


void (*send_response)(int len);

const struct accelgyro_drv test_motion_sense = {
	.init = accel_init,
	.read = accel_read,
	.set_range = accel_set_range,
	.get_range = accel_get_range,
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
	.wHIDDescLength = 0x001e, // 0x1e
	.bcdVersion = 0x0100,
	.wReportDescLength = sizeof(report_desc),
	.wReportDescRegister = REPORT_DESC_REGISTER,
	.wInputRegister = INPUT_REPORT_REGISTER,
	.wMaxInputLength = I2C_HID_HEADER_SIZE + sizeof(struct hid_accel_input_report),
	.wOutputRegister = 0,
	.wMaxOutputLength = 0,
	.wCommandRegister = COMMAND_REGISTER,
	.wDataRegister = DATA_REGISTER
};

// check that response in i2c buffer matches report_descr exactly
static int compareReportDescResponse(uint8_t *buffer, const uint8_t *report_desc) {
	int i, j;
	for (i=0;i<sizeof(report_desc);i++) {
	 	for (j=0;j<sizeof(buffer);j++) {
	 		if (report_desc[i] != buffer[j]) {
	 			return 0;
	 		} else return 1;
	 	}
	}
	return -1;
}
/* Emulate host by sending set sampling frequency report */
static int test_set_accel_sampling_freq(void) {
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
 *   xx... - report data
 *
 * Note that command register and data register have the same address. Also,
 * any report ID >= 15 requires an extra byte after the SET_REPORT byte, which
 * is not supported here as we don't have any report ID >= 15.
 *
 * In summary, we expect |buffer| contains at least 10 bytes where the report
 * data starts at buffer[9]. If |buffer| contains the incorrect number bytes,
 * we ignore the report.*/
 
// host buffer received with set_report cmd and odr to set
	uint8_t buffer[13];
	int odr;
 	buffer[0] = 0x00; // cmd reg lsb
 	buffer[1] = 0x30; // cmd reg msb
 	buffer[2] = 0x31; // report type & report ID
 	buffer[3] = 0x03; // set_report cmd opcode
 	buffer[4] = 0x00; // data reg same as cmd reg. lsb
 	buffer[5] = 0x30; // data reg msb
 	buffer[6] = 0x00; // report len lsb
 	buffer[7] = 0x04; // report len msb. len=4 as odr is stored as int32_t
 	buffer[8] = REPORT_ID_BASE_ACCEL_SAMPLING_RATE; // report ID
 	buffer[9] = 0x05; // odr data starts here
 	buffer[10] = 0x0;
 	buffer[11] = 0x0;
 	buffer[12] = 0x0;

	odr = i2c_hid_process(sizeof(buffer), buffer, send_response);
 	printf("ODR set is: %d\n", odr);
 	TEST_ASSERT(odr == (buffer[9] & 0x0F));
 	return EC_SUCCESS;
 
}

/* Emulate host by requesting for hid descriptor */
static int test_read_hid_descr(void) {
	int HIDDescLength, bcdVersion, ReportDescLength;
	int ReportDescRegister, InputRegister, MaxInputLength, OutputRegister;
	int MaxOutputLength, CommandRegister, DataRegister;

	uint8_t buffer[30]; // max hid_descr len=30 bytes
 	buffer[0] = 0x01; // hid_descr cmd reg lsb
 	buffer[1] = 0x00; // hid_descr cmd reg msb

	i2c_hid_process(sizeof(buffer), buffer, send_response);
	printf("HID Descr len: %d \n", sizeof(hid_desc));
	printf("Report Descr len: %x \n", sizeof(report_desc));
	printf("wMaxInputLength: %x \n", hid_desc.wMaxInputLength);
	
	// check that response in i2c buffer matches hid_descr exactly
	HIDDescLength = buffer[0] & 0xFF;
	TEST_ASSERT(HIDDescLength == hid_desc.wHIDDescLength);

	bcdVersion = buffer[3] << 8 | buffer[2];
	TEST_ASSERT(bcdVersion == hid_desc.bcdVersion);

	ReportDescLength = buffer[5] << 8 | buffer[4];
	TEST_ASSERT(ReportDescLength == hid_desc.wReportDescLength);

	ReportDescRegister = buffer[7] << 8 | buffer[6];
	TEST_ASSERT(ReportDescRegister == hid_desc.wReportDescRegister);

	InputRegister = buffer[9] << 8 | buffer[8];
	TEST_ASSERT(InputRegister == hid_desc.wInputRegister);

	MaxInputLength = buffer[11] << 8 | buffer[10];
	TEST_ASSERT(MaxInputLength == hid_desc.wMaxInputLength);

	OutputRegister = buffer[13] << 8 | buffer[12];
	TEST_ASSERT(OutputRegister == hid_desc.wOutputRegister);

	MaxOutputLength = buffer[15] << 8 | buffer[14];
	TEST_ASSERT(MaxOutputLength == hid_desc.wMaxOutputLength);

	CommandRegister = buffer[17] << 8 | buffer[16];
	TEST_ASSERT(CommandRegister == hid_desc.wCommandRegister);

	DataRegister = buffer[19] << 8 | buffer[18];
	TEST_ASSERT(DataRegister == hid_desc.wDataRegister);
	return EC_SUCCESS;

}

/* Emulate host by requesting for report descriptor */
static int test_read_report_descr(void) {
	uint8_t buffer[sizeof(report_desc)];
	int same;
 	buffer[0] = 0x00; // report_descr cmd reg lsb
 	buffer[1] = 0x10; // report_descr cmd reg msb
	i2c_hid_process(sizeof(buffer), buffer, send_response);
	// check that response in i2c buffer matches report_descr exactly
	same = compareReportDescResponse(buffer, report_desc);
	TEST_ASSERT(same == 1);
	return EC_SUCCESS;

}

void run_test(void)
{
	test_reset();
	RUN_TEST(test_set_accel_sampling_freq);
	RUN_TEST(test_read_hid_descr);
	RUN_TEST(test_read_report_descr);

	test_print_result();
}

