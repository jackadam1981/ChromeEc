/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stddef.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <usbc/ppm.h>

LOG_MODULE_REGISTER(ppm_utils);

int ppm_get_alternate_modes(const struct device *ppm_dev, uint8_t connector_num,
			    enum alt_modes_recipient recipient,
			    size_t max_modes,
			    struct ucsi_altmode_field *ucsi_alt_modes,
			    size_t *num_alt_modes)
{
	const struct ucsi_pd_driver *ppm_api = ppm_dev->api;
	uint8_t resp[PDC_MAX_DATA_LENGTH] = { 0 };
	size_t found_alt_modes;
	int offset;
	int rv;

	if (connector_num > ppm_api->get_active_port_count(ppm_dev)) {
		return -EINVAL;
	}

	struct ucsi_control_t get_am_cmd = {
		.command = UCSI_GET_ALTERNATE_MODES,
		.data_length = 0,
		.command_specific = {
			recipient & 0x07,
			connector_num & 0x7F,
			0, /* Starting offset of 0 */
			1, /* Read 2 (1+1) alternate modes fields */
		},
	};

	found_alt_modes = 0;
	for (int i = 0; i < max_modes; i += 2) {
		/* We receive two fields per command call. Set the offset */
		get_am_cmd.command_specific[2] = i;

		offset = i * sizeof(struct ucsi_altmode_field);

		if ((i + 1) > max_modes) {
			/* Caller's buffer not a multiple of 2.  Truncate
			 * the last read. */
			get_am_cmd.command_specific[3] = 0;
		}

		rv = ppm_api->execute_cmd(ppm_dev, &get_am_cmd, &resp[offset]);
		if (rv < 0) {
			if (i != 0) {
				/* The PDC completed UCSI_GET_ALTERNATE_MODES
				 * at lower offset numbers. Assume the
				 * PDC has reached the end of its alternate
				 * mode list and return the successfully read
				 * alt modes to the caller.
				 */
				break;
			}

			LOG_ERR("PPM%d: Failed to execute UCSI_GET_ALTERNATE_MODES command: %d",
				connector_num, rv);
			return rv;
		}

		/* Non-negative return values indicate the number of bytes in
		 * in the response data.  There are 3 expected response data
		 * sizes:
		 *  0 - End of alternate mode list
		 *  6 - one alternate mode and implied end of alternate mode
		 *      list
		 * 12 - two alternate modes, PPM must issue additional commands
		 *      to get the additional alternate modes
		 */
		if (rv == 0) {
			break;
		}

		if (rv < sizeof(struct ucsi_get_alternate_modes_t)) {
			found_alt_modes++;
			break;
		}

		found_alt_modes += 2;
	}

	/* Note - num_alt_modes already bound by caller's max_modes. */
	memcpy(ucsi_alt_modes, resp,
	       found_alt_modes * sizeof(struct ucsi_altmode_field));

	*num_alt_modes = found_alt_modes;

	return 0;
}

int ppm_get_cam_supported(const struct device *ppm_dev, uint8_t connector_num,
			  size_t mask_size, uint8_t *altmode_supported_mask,
			  size_t *resp_size)
{
	const struct ucsi_pd_driver *ppm_api = ppm_dev->api;
	uint8_t resp[PDC_MAX_DATA_LENGTH] = { 0 };
	int rv;

	if (connector_num > ppm_api->get_active_port_count(ppm_dev)) {
		return -EINVAL;
	}

	struct ucsi_control_t get_cam_supported = {
                .command = UCSI_GET_CAM_SUPPORTED,
                .data_length = 0,
                .command_specific = {
                        connector_num & 0x7F,
                },
        };

	rv = ppm_api->execute_cmd(ppm_dev, &get_cam_supported,
				  (uint8_t *)&resp);
	if (rv < 0) {
		LOG_ERR("PPM%d: failed to execute UCSI_GET_CAM_SUPPORTED command: %d",
			connector_num, rv);
		return rv;
	}

	*resp_size = MIN(rv, mask_size);

	memcpy(altmode_supported_mask, resp, *resp_size);

	return 0;
}

int ppm_get_current_cam(const struct device *ppm_dev, uint8_t connector_num,
			size_t altmodes_size, uint8_t *altmodes,
			size_t *num_altmodes)
{
	const struct ucsi_pd_driver *ppm_api = ppm_dev->api;
	uint8_t resp[PDC_MAX_DATA_LENGTH] = { 0 };
	int rv;

	if (connector_num > ppm_api->get_active_port_count(ppm_dev)) {
		return -EINVAL;
	}

	struct ucsi_control_t get_current_cam = {
                .command = UCSI_GET_CURRENT_CAM,
                .data_length = 0,
                .command_specific = {
                        connector_num & 0x7F,
                },
        };

	/* Note - the UCSI 3.0 spec is inconsistent as to behavior when there
	 * are no active alternate modes.
	 *
	 * The Data Length field says "set to the number of Alternate Modes
	 * that the connector is currently operating in. Else set to 0x00".
	 *
	 * The GET_CURRENT_CAM data description says for the  Current Alternate
	 * Mode[0] "If the connector is not operating in an alternate mode, the
	 * PPM shall set this field to 0xFF."
	 *
	 * So it's not clear if the PPM will return 1 or 0 in the data length
	 * field when there are no active alternate modes.
	 */
	rv = ppm_api->execute_cmd(ppm_dev, &get_current_cam, (uint8_t *)&resp);
	if (rv < 0) {
		LOG_INF("PPM%d: Failed to execute UCSI_GET_CURRENT_CAM: %d",
			connector_num, rv);
		return rv;
	}

	*num_altmodes = MIN(rv, altmodes_size);

	memcpy(altmodes, resp, *num_altmodes);

	return 0;
}
