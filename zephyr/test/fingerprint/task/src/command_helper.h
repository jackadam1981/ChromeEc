// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <drivers/fingerprint.h>
#include <ec_commands.h>
#include <host_command.h>

uint16_t fpinfo_cmd_helper(struct ec_response_fp_info_v2 *response)
{
	static const size_t test_info_buffer_size =
		sizeof(struct ec_response_fp_info_v2) +
		sizeof(struct fp_image_frame_params) * NUM_IMAGE_CAPTURE_TYPES;

	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_FP_INFO, 2, *response);
	args.response_max = test_info_buffer_size;

	return host_command_process(&args);
}
