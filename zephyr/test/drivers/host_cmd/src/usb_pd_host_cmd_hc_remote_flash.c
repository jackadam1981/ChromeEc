/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_mocks.h"
#include "test/drivers/test_state.h"
#include "test/drivers/stubs.h"
#include "test/drivers/utils.h"
#include "test_usb_pd_host_cmd.h"
#include "usb_pd.h"

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__fw_reboot_subcmd)
{
	const struct ec_params_usb_pd_fw_update p = {
		.cmd = USB_PD_FW_REBOOT,
		.port = TEST_PORT,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);
	zassert_ok(host_command_process(&args));

	zassert_equal(pd_send_vdm_fake.call_count, 1);
	zassert_equal(pd_send_vdm_fake.arg0_val, TEST_PORT);
	zassert_equal(pd_send_vdm_fake.arg1_val, USB_VID_GOOGLE);
	zassert_equal(pd_send_vdm_fake.arg2_val, VDO_CMD_REBOOT);
	zassert_equal(pd_send_vdm_fake.arg3_val, NULL);
	zassert_equal(pd_send_vdm_fake.arg4_val, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__fw_flash_erase_subcmd)
{
	const struct ec_params_usb_pd_fw_update p = {
		.cmd = USB_PD_FW_FLASH_ERASE,
		.port = TEST_PORT,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);
	zassert_ok(host_command_process(&args));

	zassert_equal(pd_send_vdm_fake.call_count, 1);
	zassert_equal(pd_send_vdm_fake.arg0_val, TEST_PORT);
	zassert_equal(pd_send_vdm_fake.arg1_val, USB_VID_GOOGLE);
	zassert_equal(pd_send_vdm_fake.arg2_val, VDO_CMD_FLASH_ERASE);
	zassert_equal(pd_send_vdm_fake.arg3_val, NULL);
	zassert_equal(pd_send_vdm_fake.arg4_val, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__fw_sig_subcmd)
{
	const struct ec_params_usb_pd_fw_update p = {
		.cmd = USB_PD_FW_ERASE_SIG,
		.port = TEST_PORT,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);
	zassert_ok(host_command_process(&args));

	zassert_equal(pd_send_vdm_fake.call_count, 1);
	zassert_equal(pd_send_vdm_fake.arg0_val, TEST_PORT);
	zassert_equal(pd_send_vdm_fake.arg1_val, USB_VID_GOOGLE);
	zassert_equal(pd_send_vdm_fake.arg2_val, VDO_CMD_ERASE_SIG);
	zassert_equal(pd_send_vdm_fake.arg3_val, NULL);
	zassert_equal(pd_send_vdm_fake.arg4_val, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__fw_flash_write_subcmd)
{
	struct params_wrapper {
		struct {
			struct ec_params_usb_pd_fw_update p;
			uint32_t payload;
		};
	};

	struct params_wrapper params = {
		.p =  {
			.cmd = USB_PD_FW_FLASH_WRITE,
			.port = TEST_PORT,
			/* Arbitrary data size (must be multiple of 4) */
			.size = 4,
		},
		/* Arbitrary payload */
		.payload = 0x1,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, params);
	zassert_ok(host_command_process(&args));

	zassert_equal(pd_send_vdm_fake.call_count, 1);
	zassert_equal(pd_send_vdm_fake.arg0_val, TEST_PORT);
	zassert_equal(pd_send_vdm_fake.arg1_val, USB_VID_GOOGLE);
	zassert_equal(pd_send_vdm_fake.arg2_val, VDO_CMD_FLASH_WRITE);

	/* Sent one 32-bit word */
	zassert_equal(pd_send_vdm_fake.arg4_val, 1);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__bad_port)
{
	const struct ec_params_usb_pd_fw_update p = {
		/* Arbitrary subcmd */
		.cmd = USB_PD_FW_ERASE_SIG,
		.port = -1,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);

	zassert_equal(host_command_process(&args), EC_RES_INVALID_PARAM);
	zassert_equal(pd_send_vdm_fake.call_count, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__size_too_big)
{
	const struct ec_params_usb_pd_fw_update p = {
		/* Arbitrary subcmd */
		.cmd = USB_PD_FW_ERASE_SIG,
		.port = TEST_PORT,
		.size = INT_MAX,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);

	zassert_equal(host_command_process(&args), EC_RES_INVALID_PARAM);
	zassert_equal(pd_send_vdm_fake.call_count, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__active_charging_port_no_batt)
{
	const struct ec_params_usb_pd_fw_update p = {
		/* Arbitrary subcmd */
		.cmd = USB_PD_FW_ERASE_SIG,
		.port = TEST_PORT,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);

	test_set_battery_present(false);
	charge_manager_get_active_charge_port_fake.return_val = TEST_PORT;

	zassert_equal(host_command_process(&args), EC_RES_UNAVAILABLE);
	zassert_equal(charge_manager_get_active_charge_port_fake.call_count, 1);
	zassert_equal(pd_send_vdm_fake.call_count, 0);
}

ZTEST_USER(usb_pd_host_cmd, test_hc_remote_flash__bad_cmd)
{
	const struct ec_params_usb_pd_fw_update p = {
		.cmd = -1,
		.port = TEST_PORT,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, p);

	zassert_equal(host_command_process(&args), EC_RES_INVALID_PARAM);
	zassert_equal(pd_send_vdm_fake.call_count, 0);
}

ZTEST_USER(usb_pd_host_cmd,
	   test_hc_remote_flash__fw_flash_write_subcmd_bad_data_size)
{
	struct params_wrapper {
		struct {
			struct ec_params_usb_pd_fw_update p;
			uint32_t payload;
		};
	};

	struct params_wrapper params = {
		.p =  {
			.cmd = USB_PD_FW_FLASH_WRITE,
			.port = TEST_PORT,
			/* Make size not multiple of 4 */
			.size = 3,
		},
		/* Arbitrary payload */
		.payload = 0x1,
	};

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_PARAMS(EC_CMD_USB_PD_FW_UPDATE, 0, params);
	zassert_equal(host_command_process(&args), EC_RES_INVALID_PARAM);

	zassert_equal(pd_send_vdm_fake.call_count, 0);
}
