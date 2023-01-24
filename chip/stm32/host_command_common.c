/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "fpsensor_detect.h"
#include "host_command.h"
#include "spi.h"
#include "usart_host_command.h"

#ifndef CONFIG_I2C_PERIPHERAL

/* Store current transport type */
static enum fp_transport_type curr_transport_type = FP_TRANSPORT_TYPE_UNKNOWN;

const uint32_t host_command_protocol_info_flags(void)
{
	return EC_PROTOCOL_INFO_IN_PROGRESS_SUPPORTED;
}

const uint16_t host_command_max_request_size(void)
{
	enum ec_status ret_status = EC_RES_INVALID_COMMAND;

	/*
	 * Read transport type from TRANSPORT_SEL bootstrap pin the first
	 * time this function is called.
	 */
	if (IS_ENABLED(CONFIG_FINGERPRINT_MCU) &&
	    (curr_transport_type == FP_TRANSPORT_TYPE_UNKNOWN))
		curr_transport_type = get_fp_transport_type();

	if (IS_ENABLED(CONFIG_USART_HOST_COMMAND) &&
	    curr_transport_type == FP_TRANSPORT_TYPE_UART)
		return USART_MAX_REQUEST_SIZE;
	else if (IS_ENABLED(CONFIG_SPI) &&
		 curr_transport_type == FP_TRANSPORT_TYPE_SPI)
		return SPI_MAX_REQUEST_SIZE;

	return 0;
}

const uint16_t host_command_max_response_size(void)
{
	enum ec_status ret_status = EC_RES_INVALID_COMMAND;

	/*
	 * Read transport type from TRANSPORT_SEL bootstrap pin the first
	 * time this function is called.
	 */
	if (IS_ENABLED(CONFIG_FINGERPRINT_MCU) &&
	    (curr_transport_type == FP_TRANSPORT_TYPE_UNKNOWN))
		curr_transport_type = get_fp_transport_type();

	if (IS_ENABLED(CONFIG_USART_HOST_COMMAND) &&
	    curr_transport_type == FP_TRANSPORT_TYPE_UART)
		return USART_MAX_RESPONSE_SIZE;
	else if (IS_ENABLED(CONFIG_SPI) &&
		 curr_transport_type == FP_TRANSPORT_TYPE_SPI)
		return SPI_MAX_RESPONSE_SIZE;

	return 0;
}

#endif /* CONFIG_I2C_PERIPHERAL */
