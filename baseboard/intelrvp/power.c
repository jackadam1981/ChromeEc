/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "power/icelake.h"
#include <device.h>
#include <drivers/espi.h>

LOG_MODULE_DECLARE(espi, CONFIG_ESPI_LOG_LEVEL);

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
DECLARE_CONSOLE_COMMAND(oob, command_oob, "", "");

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
