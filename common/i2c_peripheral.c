/* Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* I2C peripheral cross-platform code for Chrome EC */

#include "host_command.h"
#include "i2c.h"
#include "util.h"

const uint16_t host_command_max_request_size(void)
{
	return I2C_MAX_HOST_PACKET_SIZE;
}

const uint16_t host_command_max_response_size(void)
{
	return I2C_MAX_HOST_PACKET_SIZE;
}
