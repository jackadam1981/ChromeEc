/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "power/icelake.h"
#include "util.h"
#include <device.h>
#include <drivers/espi.h>

LOG_MODULE_DECLARE(espi, CONFIG_ESPI_LOG_LEVEL);
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

struct oob_header {
	uint8_t cycle_type;
	uint8_t tag_and_len_bit_11_8;
	uint8_t len_bit_7_0;
	uint8_t msg_code;
};

/* eSPI host entity address  */
#define PCH_DEST_SLV_ADDR     0x02u
#define SRC_SLV_ADDR          0x21u

#define OOB_RESPONSE_SENDER_INDEX   0x02u
#define OOB_RESPONSE_DATA_INDEX     0x04u


/* Temperature command opcode */
#define OOB_CMDCODE           0x01u
#define OOB_RESPONSE_LEN            0x05u

/* Maximum bytes for OOB transactions */
#define MAX_ESPI_BUF_LEN      80u
#define MIN_GET_TEMP_CYCLES   5u

/* 100ms */
#define MAX_OOB_TIMEOUT       100ul

static struct espi_oob_packet resp_pckt;
static uint8_t buf[MAX_ESPI_BUF_LEN];

static int request_temp(const struct device *dev)
{
	int ret;
	struct oob_header oob_hdr;
	struct espi_oob_packet req_pckt;

	LOG_WRN("%s", __func__);

	oob_hdr.msg_code = PCH_DEST_SLV_ADDR;
	oob_hdr.tag_and_len_bit_11_8 = 0;
	oob_hdr.len_bit_7_0 = 1;
	oob_hdr.cycle_type = SRC_SLV_ADDR;

	/* Packetize OOB request */
	req_pckt.buf = (uint8_t *)&oob_hdr;
	req_pckt.len = sizeof(struct oob_header);

	ret = espi_send_oob(dev, &req_pckt);
	if (ret) {
		LOG_ERR("OOB Tx failed %d", ret);
		return ret;
	}

	return 0;
}

static int retrieve_packet(const struct device *dev, uint8_t *sender)
{
	int ret;

	resp_pckt.buf = (uint8_t *)&buf;
	resp_pckt.len = MAX_ESPI_BUF_LEN;

	ret = espi_receive_oob(dev, &resp_pckt);
	if (ret) {
		LOG_ERR("OOB Rx failed %d", ret);
		return ret;
	}

	LOG_INF("OOB transaction completed rcvd: %d bytes", resp_pckt.len);
	for (int i = 0; i < resp_pckt.len; i++) {
		LOG_INF("%x ", buf[i]);
	}

	if (sender) {
		*sender = buf[OOB_RESPONSE_SENDER_INDEX];
	}

	return 0;
}

int get_pch_temp_sync(const struct device *dev)
{
	int ret;

	for (int i = 0; i < MIN_GET_TEMP_CYCLES; i++) {
		ret = request_temp(dev);
		if (ret) {
			LOG_ERR("OOB req failed %d", ret);
			return ret;
		}

		ret = retrieve_packet(dev, NULL);
		if (ret) {
			LOG_ERR("OOB retrieve failed %d", ret);
			return ret;
		}
	}

	return 0;
}

static int command_oob(int argc, char **argv)
{
	get_pch_temp_sync(DEVICE_DT_GET(DT_NODELABEL(espi0)));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(oobt, command_oob, "", "");

/* OOB channel SMBus command */
enum espi_oob_smbus_command {
	OOB_SMBUS_GET_TEMP = 1,
	OOB_SMBUS_GET_RTC,
};

/* OOB channel receive length */
#define OOB_SMBUS_REC_TEMP_LENGTH	0x05
#define OOB_SMBUS_REC_RTC_LENTGH	0x0C

/* OOB channel SMBus receive data byte count */
#define OOB_SMBUS_REC_TEMP_BYTE_COUNT	0x02
#define OOB_SMBUS_REC_RTC_BYTE_COUNT	0x09

/* eSPI cycle type field */
#define ESPI_FLASH_READ_CYCLE_TYPE	0x00
#define ESPI_FLASH_WRITE_CYCLE_TYPE	0x01
#define ESPI_FLASH_ERASE_CYCLE_TYPE	0x02
#define ESPI_OOB_CYCLE_TYPE		0x21

/* eSPI tag + len[11:8] field */
#define ESPI_TAG_LEN_FIELD(tag, len) \
		   ((((tag) & 0xF) << 4) | (((len) >> 8) & 0xF))

/* OOB channel SMBus address field */

/* PCH/SoC eSPI-MC (hardware) Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_SOC_MC		(0x01 << 1)

/* PCH/SoC Intel ME FW SMBus Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_SOC_ME		(0x10 << 1)

/* Intel PCH PECI SMBus MCTP Request Handler Address */
#define OOB_SMBUS_DEST_ADDR_PCH_PECI_MCTP	(0x20 << 1)

/* eSPI slave address to master */
#define OOB_SMBUS_SRC_ADDR_EC	0x07

/**
 * eSPI OOB channel data buffer size maximum 80 bytes.
 * Actually length field can reach 4096 bytes defined in intel espi spec.
 */
#define ESPI_OOB_MAX_LENGTH	80

/* Minimum eSPI OOB channel data receive length */
#define ESPI_OOB_MIN_LENGTH	5

/**
 * Receive messages from eSPI OOB channel
 * @param oob_data point to the oob data array buffer
 * @return EC_SUCCESS, or non-zero if error
 */
int espi_oob_receive(uint8_t *oob_data);

/**
 * Send messages via eSPI OOB channel
 * @param oob_data point to the oob data array buffer
 * @return EC_SUCCESS, or non-zero if error
 */
int espi_oob_send(uint8_t *oob_data);

/* TODO: (need a queue?) ESPI OOB data */
static uint8_t espi_oob_data[ESPI_OOB_MAX_LENGTH];

int process_espi_oob_cycle_data(void)
{
	int oob_len, i;

	/* Byte 3 - slave address */
	if (espi_oob_data[3] != (OOB_SMBUS_SRC_ADDR_EC << 1))
		return EC_ERROR_INVAL;

	/*
	 * Byte 1[3:0] - length [11:8]
	 * Byte 2      - length [7:0]
	 */
	oob_len = (espi_oob_data[1] & 0x0F) << 8 | espi_oob_data[2];

	/* Byte 4 - eSPI OOB command */
	switch (espi_oob_data[4]) {
	case OOB_SMBUS_GET_TEMP:
		/*
		 * Byte 5 - Byte count 2
		 * Byte 6 - Master address
		 * Byte 7 - Temperature
		 */
		if (oob_len != OOB_SMBUS_REC_TEMP_LENGTH ||
			espi_oob_data[5] != OOB_SMBUS_REC_TEMP_BYTE_COUNT ||
			espi_oob_data[6] !=
				(OOB_SMBUS_DEST_ADDR_PCH_SOC_MC | 0x1) ||
			espi_oob_data[7] == 0xFF)
			return EC_ERROR_INVAL;

		CPRINTS("temp = %d deg C", espi_oob_data[7]);
		break;

	case OOB_SMBUS_GET_RTC:
		/*
		 * Byte 5 - Byte count 9
		 * Byte 6 - Master address
		 */
		if (oob_len != OOB_SMBUS_REC_RTC_LENTGH ||
			espi_oob_data[5] != OOB_SMBUS_REC_RTC_BYTE_COUNT ||
			espi_oob_data[6] !=
				(OOB_SMBUS_DEST_ADDR_PCH_SOC_MC | 0x1))
			return EC_RES_INVALID_RESPONSE;

		/*
		 * Byte 7[0]-DS - Daylight saving (1-enable, 0-disabled)
		 * Byte 7[1]-HF - Hour format (1-24hr format, 0-12hr format)
		 * Byte 7[2]-DM - Data mode (1-Binay, 0-BCD)
		 * Byte 7[7]-MD - Meridium (when HF=0; 1-PM, 0-AM)
		 */
		CPRINTF("DS:HF:DM:MD::%d:%d:%d:%d\n", espi_oob_data[7] & 0x1,
			!!(espi_oob_data[7] & 0x2), !!(espi_oob_data[7] & 0x4),
			!!(espi_oob_data[7] & 0x80));

		/*
		 * Byte  8 - PCH RTC Time: Seconds
		 * Byte  9 - PCH RTC Time: Minutes
		 * Byte 10 - PCH RTC Time: Hours
		 * Byte 11 - PCH RTC Time: Day of Week
		 * Byte 12 - PCH RTC Time: Day of Month
		 * Byte 13 - PCH RTC Time: Month
		 * Byte 14 - PCH RTC Time: Year
		 */
		CPRINTF("SS:MN:HH:DW:DD:MM:YY:");
		for (i = 8; i < oob_len + 2; i++)
			CPRINTF(":%02x", espi_oob_data[i]);
		CPRINTF("\n");
		break;

	default:
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int command_espi_oob(int argc, char **argv)
{
	char *e;
	uint8_t cmd, byte_count, dst_addr;
	uint16_t data_len;
	int ret;
	struct espi_oob_packet req_pckt;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	cmd = (uint8_t) strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	/* Prepare eSPI OOB data for commands */
	switch (cmd) {
	case OOB_SMBUS_GET_TEMP:
	case OOB_SMBUS_GET_RTC:
		data_len = 0x4;
		byte_count = 0x1;
		dst_addr = OOB_SMBUS_DEST_ADDR_PCH_SOC_MC;
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	/* cycle type */
	espi_oob_data[0] = ESPI_OOB_CYCLE_TYPE;
	/* tag + len[11:8] */
	espi_oob_data[1] = ESPI_TAG_LEN_FIELD(0, data_len);
	/* len[7:0] */
	espi_oob_data[2] = data_len;
	/* SMBus destination addr */
	espi_oob_data[3] = dst_addr;
	/* SMBus cmd */
	espi_oob_data[4] = cmd;
	/* SMBus byte count */
	espi_oob_data[5] = byte_count;
	/* SMBus source addr (slave request) */
	espi_oob_data[6] = (OOB_SMBUS_SRC_ADDR_EC << 1) | 0x1;

	//return espi_oob_send(espi_oob_data);

	/* Packetize OOB request */
	req_pckt.buf = (uint8_t *)&espi_oob_data;
	req_pckt.len = 7;

	ret = espi_send_oob(DEVICE_DT_GET(DT_NODELABEL(espi0)), &req_pckt);
	if (ret) {
		LOG_ERR("OOB Tx failed %d", ret);
		return ret;
	}

	resp_pckt.buf = (uint8_t *)&espi_oob_data;
	resp_pckt.len = MAX_ESPI_BUF_LEN;

	ret = espi_receive_oob(DEVICE_DT_GET(DT_NODELABEL(espi0)), &resp_pckt);
	if (ret) {
		LOG_ERR("OOB Rx failed %d", ret);
		return ret;
	}
#if 0
	for (int i = 0; i < 20; i++) {
		printk("OOB Rx [%d] = %x\n", i, espi_oob_data[i]);
	}
#endif
	process_espi_oob_cycle_data();

#if 0
	LOG_INF("OOB transaction completed rcvd: %d bytes", resp_pckt.len);
	for (int i = 0; i < resp_pckt.len; i++) {
		LOG_INF("%x ", buf[i]);
	}
#endif

	return 0;
}
DECLARE_CONSOLE_COMMAND(oob, command_espi_oob,
			"cmd",
			"Send ESPI OOB command");

static uint8_t espi_flash_data[64];
static int command_espi_read_flash(int argc, char **argv)
{
	char *e;
	uint16_t data_len;
	uint32_t addr;
	int ret;
	struct espi_flash_packet pckt;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	addr = (uint32_t) strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	data_len = (uint16_t) strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	pckt.buf = espi_flash_data;
	pckt.flash_addr = addr;
	pckt.len = data_len;

	ret = espi_read_flash(DEVICE_DT_GET(DT_NODELABEL(espi0)), &pckt);
	if (ret) {
		LOG_ERR("espi_read_flash failed: %d", ret);
		return ret;
	}

	for (int i =0; i < pckt.len; i++) {
		CPRINTF("%s [0x%x]=0x%x\n", __func__, (pckt.flash_addr+i),
			espi_flash_data[i]);
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(espi_rf, command_espi_read_flash,
			"addr len",
			"espi read flash");

static int command_espi_write_flash(int argc, char **argv)
{
	char *e;
	uint16_t data_len;
	uint32_t addr;
	int ret;
	struct espi_flash_packet pckt;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	addr = (uint32_t) strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	data_len = (uint16_t) strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	pckt.buf = espi_flash_data;
	pckt.flash_addr = addr;
	pckt.len = data_len;

	for (int i =0; i < pckt.len; i++) {
		espi_flash_data[i] = i;
	}

	ret = espi_write_flash(DEVICE_DT_GET(DT_NODELABEL(espi0)), &pckt);
	if (ret) {
		LOG_ERR("%s failed: %d", __func__, ret);
		return ret;
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(espi_wf, command_espi_write_flash,
			"addr len",
			"espi write flash");

static int command_espi_erase_flash(int argc, char **argv)
{
	char *e;
	uint16_t data_len;
	uint32_t addr;
	int ret;
	struct espi_flash_packet pckt;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	addr = (uint32_t) strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	data_len = (uint16_t) strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	pckt.buf = espi_flash_data;
	pckt.flash_addr = addr;
	pckt.len = data_len;

	ret = espi_flash_erase(DEVICE_DT_GET(DT_NODELABEL(espi0)), &pckt);
	if (ret) {
		LOG_ERR("%s failed: %d", __func__, ret);
		return ret;
	}

	return 0;
}
DECLARE_CONSOLE_COMMAND(espi_ef, command_espi_erase_flash,
			"addr len",
			"espi erase flash");

/******************************************************************************/
/* PWROK signal configuration */
/*
 * On ADLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_assert_list);
