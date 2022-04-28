/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_state.h"
#include "lpc.h"
#include "espi.h"
#include "zephyr_espi_shim.h"
#include "drivers/espi_emul.h"

#define PORT 0
#define ESPI_DEVICE DEVICE_DT_GET(DT_CHOSEN(cros_ec_espi))

static int hostcmd_resp_recv;
static int hostcmd_resp_code;

static uint8_t calculate_hostcmd_checksum(const char *buf, int size)
{
	int c = 0;
	int i;

	for (i = 0; i < size; ++i)
		c += buf[i];

	return -c;
}

static void espi_hostcmd_resp_cb(const struct device *dev,
				 struct espi_callback *cb,
				 struct espi_event espi_evt)
{
	hostcmd_resp_recv++;
	hostcmd_resp_code = espi_evt.evt_data;
}

ZTEST_USER(espi, test_host_command_get_protocol_info)
{
	struct ec_response_get_protocol_info response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_GET_PROTOCOL_INFO, 0,
					    response);

	zassert_ok(host_command_process(&args), NULL);
	zassert_ok(args.result, NULL);
	zassert_equal(args.response_size, sizeof(response), NULL);
	zassert_equal(response.protocol_versions, BIT(3), NULL);
	zassert_equal(response.max_request_packet_size, EC_LPC_HOST_PACKET_SIZE,
		      NULL);
	zassert_equal(response.max_response_packet_size,
		      EC_LPC_HOST_PACKET_SIZE, NULL);
	zassert_equal(response.flags, 0, NULL);
}

ZTEST_USER(espi, test_host_command_usb_pd_power_info)
{
	/* Only test we've enabled the command */
	struct ec_response_usb_pd_power_info response;
	struct ec_params_usb_pd_power_info params = { .port = PORT };
	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_USB_PD_POWER_INFO, 0, response, params);

	args.params = &params;
	zassert_ok(host_command_process(&args), NULL);
	zassert_ok(args.result, NULL);
	zassert_equal(args.response_size, sizeof(response), NULL);
}

ZTEST_USER(espi, test_host_command_typec_status)
{
	/* Only test we've enabled the command */
	struct ec_params_typec_status params = { .port = PORT };
	struct ec_response_typec_status response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND(EC_CMD_TYPEC_STATUS, 0, response, params);

	zassert_ok(host_command_process(&args), NULL);
	zassert_ok(args.result, NULL);
	zassert_equal(args.response_size, sizeof(response), NULL);
}

ZTEST_USER(espi, espi_host_cmd)
{
	struct ec_host_request *request = NULL;
	uint32_t *data_ptr = NULL;
	struct espi_callback espi_cb_obj;
	struct espi_event evt;

	espi_init_callback(&espi_cb_obj, espi_hostcmd_resp_cb,
		ESPI_BUS_PERIPHERAL_NOTIFICATION);
	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 1);

	evt.evt_type = ESPI_BUS_PERIPHERAL_NOTIFICATION;
	evt.evt_details = ESPI_PERIPHERAL_EC_HOST_CMD;
	evt.evt_data = EC_COMMAND_PROTOCOL_3;

	espi_read_lpc_request(ESPI_DEVICE, ECUSTOM_HOST_CMD_GET_PARAM_MEMORY,
		(uint32_t *)&request);

	data_ptr = (uint32_t *)(request + 1);
	request->struct_version = EC_HOST_REQUEST_VERSION;

	request->checksum = 0;
	request->command = EC_CMD_HELLO;
	request->command_version = 0;
	request->reserved = 0;
	request->data_len = 4;
	*data_ptr = 0x10203040;
	request->checksum = calculate_hostcmd_checksum((char *)request,
		sizeof(*request) + sizeof(*data_ptr));

	espi_emul_raise_event(ESPI_DEVICE, evt);

	k_msleep(1000);

	zassert_equal(hostcmd_resp_recv, 1, "callback should be called");
	zassert_equal(hostcmd_resp_code, 0, "result code should be 0");
	zassert_equal(*data_ptr, 0x11223344, "hostcmd should be executed");

	espi_emul_manage_host_callback(ESPI_DEVICE, &espi_cb_obj, 0);
}

ZTEST_SUITE(espi, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
