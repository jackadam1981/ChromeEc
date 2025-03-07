/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI PPM altnernate mode policy handler */

#include "config.h"
#include "usb_pd.h"

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <usbc/ppm.h>

LOG_MODULE_REGISTER(ppm_am_policy);

#define PPM_MAX_ALT_MODES 8

enum alt_modes_recipient {
	RECIPIENT_CONNECTOR,
	RECIPIENT_SOP,
	RECIPIENT_SOP_PRIME,
	RECIPIENT_MAX,
};

enum mode_entry_state {
	GET_ALTERNATE_MODES_CONN,
	GET_ALTERNATE_MODES_SOP,
	GET_ALTERNATE_MODES_SOP_PRIME,
	GET_CAM_SUPPORTED,
	SET_NEW_CAM,
};

struct ppm_am_policy_data {
	uint8_t connector_num;
	const struct device *ppm_dev;
	const struct ucsi_pd_driver *ppm_api;

	enum mode_entry_state mode_entry_state;

	uint8_t cam_supported_mask;
	uint16_t svids[RECIPIENT_MAX][PPM_MAX_ALT_MODES];
	bool tbt_supported[RECIPIENT_MAX];
	struct k_work_delayable work;
};

static struct ppm_am_policy_data ppm_am_data[CONFIG_USB_PD_PORT_MAX_COUNT];

static const char *const mode_entry_state_names[] = {
	[GET_ALTERNATE_MODES_CONN] = "Get Alt Modes Connector",
	[GET_ALTERNATE_MODES_SOP] = "Get Alt Modes SOP",
	[GET_ALTERNATE_MODES_SOP_PRIME] = "Get Alt Modes SOP Prime",
	[GET_CAM_SUPPORTED] = "Get CAM Supported",
	[SET_NEW_CAM] = "Set new CAM - enable TBT mode",
};

static int ppm_get_alternate_modes(struct ppm_am_policy_data *am_data,
				   enum alt_modes_recipient recipient)
{
	struct ucsi_get_alternate_modes_t
		ucsi_response[PPM_MAX_ALT_MODES / 2] = { 0 };
	int num_alt_modes = 0;
	int rv;

	struct ucsi_control_t get_am_cmd = {
		.command = UCSI_GET_ALTERNATE_MODES,
		.data_length = 0,
		.command_specific = {
			recipient & 0x07,
			am_data->connector_num & 0x7F,
			0, /* Starting offset of 0 */
			1, /* Read 2 (1+1) alternate modes fields */
		},
	};

	for (int i = 0; i < ARRAY_SIZE(ucsi_response); i++) {
		/* We receive two fields per command call. Set the offset */
		get_am_cmd.command_specific[2] = i * 2;

		rv = am_data->ppm_api->execute_cmd(
			am_data->ppm_dev, &get_am_cmd,
			(uint8_t *)&ucsi_response[i]);
		if (rv < 0) {
			LOG_ERR("PPM%d: Failed to execute UCSI_GET_ALTERNATE_MODES command: %d (i=%d)",
				am_data->connector_num, rv, i);
			return rv;
		}

		if (ucsi_response[i].altmode_fields[0].svid == 0) {
			num_alt_modes = 2 * i;
			break;
		}
		if (ucsi_response[i].altmode_fields[1].svid == 0) {
			num_alt_modes = 2 * i + 1;
			break;
		}
	}

	for (int i = 0; i < num_alt_modes; i++) {
		if (ucsi_response[i / 2].altmode_fields[i % 2].svid ==
		    USB_VID_INTEL) {
			am_data->tbt_supported[recipient] = true;
		}
		am_data->svids[recipient][i] =
			ucsi_response[i / 2].altmode_fields[i % 2].svid;
	}

	return 0;
}

static int ppm_get_cam_supported(struct ppm_am_policy_data *am_data)
{
	uint8_t resp[8] = { 0 };
	int rv;

	struct ucsi_control_t get_cam_supported = {
                .command = UCSI_GET_CAM_SUPPORTED,
                .data_length = 0,
                .command_specific = {
                        am_data->connector_num & 0x7F,
                },
        };

	rv = am_data->ppm_api->execute_cmd(am_data->ppm_dev, &get_cam_supported,
					   (uint8_t *)&resp);
	if (rv < 0) {
		LOG_ERR("PPM%d: failed to execute UCSI_GET_CAM_SUPPORTED command: %d",
			am_data->connector_num, rv);
		return rv;
	}

	/* We only check the first 8 SVIDs. */
	am_data->cam_supported_mask = resp[0];

	return 0;
}

static int ppm_enter_tbt_usb4(struct ppm_am_policy_data *am_data)
{
	uint8_t new_cam = 0xFF;
	int rv;

	/* Find the CAM index corresponding to the Intel SVID as reported
	 * by the partner (SOP).
	 *
	 * TODO: The UCSI spec implies that the New CAM field is an index
	 * into the alternate modes supported by the PPM (connector).
	 * Testing shows these indexes correspond to the partner (SOP) alternate
	 * mode list.
	 */
	for (int i = 0; i < PPM_MAX_ALT_MODES; i++) {
		if (am_data->svids[RECIPIENT_SOP][i] == USB_VID_INTEL) {
			new_cam = i;
			break;
		}
	}

	if (new_cam == 0xFF) {
		/* Couldn't find the Intel SVID.  This shouldn't happen. */
		return -EINVAL;
	}

	struct ucsi_control_t set_new_cam = {
                .command = UCSI_SET_NEW_CAM,
                .data_length = 0,
                .command_specific = {
                        (am_data->connector_num & 0x7F) | BIT(7),
			new_cam,
			/* TODO: AM Specific field not well documented. */
			0, 0, 0, 0,
                },
        };

	rv = am_data->ppm_api->execute_cmd(am_data->ppm_dev, &set_new_cam,
					   NULL);
	if (rv < 0) {
		LOG_ERR("PPM%d: Failed to execute UCSI_SET_NEW_CAM: %d",
			am_data->connector_num, rv);
		return rv;
	}

	return 0;
}

static void ppm_am_policy_work_handler(struct k_work *work)
{
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct ppm_am_policy_data *am_data =
		CONTAINER_OF(d_work, struct ppm_am_policy_data, work);
	bool am_complete = false;
	bool tbt_supported;
	int rv;

	LOG_INF("PPM%d: mode entry state %s", am_data->connector_num,
		mode_entry_state_names[am_data->mode_entry_state]);

	/* Note - the PPM command path is synchronous. We exit after each
	 * command to ensure we service other policy requests.
	 */

	switch (am_data->mode_entry_state) {
	case GET_ALTERNATE_MODES_CONN:
		rv = ppm_get_alternate_modes(am_data, RECIPIENT_CONNECTOR);
		if (rv != 0) {
			/* Failed to read alternate modes - stop enter mode
			 * processing. */
			am_complete = true;
			break;
		}

		if (am_data->tbt_supported[RECIPIENT_CONNECTOR]) {
			am_data->mode_entry_state = GET_CAM_SUPPORTED;
		} else {
			am_complete = true;
		}
		break;

	case GET_CAM_SUPPORTED:
		rv = ppm_get_cam_supported(am_data);
		if (rv != 0) {
			/* Failed to read supported modes - stop enter mode
			 * processing. */
			am_complete = true;
			break;
		}

		tbt_supported = false;
		for (int i = 0; i < PPM_MAX_ALT_MODES; i++) {
			if ((am_data->svids[RECIPIENT_CONNECTOR][i] ==
			     USB_VID_INTEL) &&
			    (am_data->cam_supported_mask & BIT(i))) {
				tbt_supported = true;
				break;
			}
		}

		if (tbt_supported) {
			am_data->mode_entry_state = GET_ALTERNATE_MODES_SOP;
		} else {
			am_complete = true;
		}
		break;

	case GET_ALTERNATE_MODES_SOP:
		rv = ppm_get_alternate_modes(am_data, RECIPIENT_SOP);
		if (rv != 0) {
			/* Failed to read alternate modes - continue. */
			am_complete = true;
			break;
		}
		if (am_data->tbt_supported[RECIPIENT_SOP]) {
			am_data->mode_entry_state =
				GET_ALTERNATE_MODES_SOP_PRIME;
		} else {
			am_complete = true;
		}
		break;

	case GET_ALTERNATE_MODES_SOP_PRIME:
		rv = ppm_get_alternate_modes(am_data, RECIPIENT_SOP_PRIME);
		if (rv != 0) {
			/* Failed to read alternate modes - continue. */
			am_complete = true;
			break;
		}
		if (am_data->tbt_supported[RECIPIENT_SOP_PRIME]) {
			am_data->mode_entry_state = SET_NEW_CAM;
		} else {
			am_complete = true;
		}
		break;

	case SET_NEW_CAM:
		/* If we get to this state, the connector, partner, and cable
		 * all support the TBT VID.
		 */
		LOG_INF("PPM%d: Alt Mode info", am_data->connector_num);
		LOG_INF("    svids = 0x%04x 0x%04x",
			am_data->svids[RECIPIENT_CONNECTOR][0],
			am_data->svids[RECIPIENT_CONNECTOR][1]);
		LOG_INF("    CAM mask   = 0x%08x", am_data->cam_supported_mask);
		LOG_INF("    TBT: conn %d, SOP %d, SOP' %d",
			am_data->tbt_supported[RECIPIENT_CONNECTOR],
			am_data->tbt_supported[RECIPIENT_SOP],
			am_data->tbt_supported[RECIPIENT_SOP_PRIME]);

		ppm_enter_tbt_usb4(am_data);

		am_complete = true;
		break;
	}

	if (!am_complete) {
		k_work_schedule(&am_data->work, K_MSEC(25));
	}
}

int ppm_am_policy_run(unsigned int port)
{
	struct ppm_am_policy_data *am_data;

	if (port >= CONFIG_USB_PD_PORT_MAX_COUNT) {
		return -ERANGE;
	}

	am_data = &ppm_am_data[port];

	/* Always clear out all cached data when the PDC reports an altnernate
	 * mode change.
	 */
	am_data->mode_entry_state = GET_ALTERNATE_MODES_CONN;
	memset(&am_data->svids, 0, sizeof(am_data->svids));
	memset(&am_data->tbt_supported, 0, sizeof(am_data->tbt_supported));
	am_data->cam_supported_mask = 0;

	k_work_schedule(&am_data->work, K_NO_WAIT);

	return 0;
}

static int ppm_am_policy_handler_init(void)
{
	const struct device *ppm_dev = DEVICE_DT_GET(DT_INST(0, ucsi_ppm));
	struct ppm_am_policy_data *am_data;

	if (!device_is_ready(ppm_dev)) {
		LOG_ERR("device %s not ready", ppm_dev->name);
		return -ENODEV;
	}

	am_data = ppm_am_data;
	for (int i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++, am_data++) {
		am_data->connector_num = i + 1;
		am_data->ppm_dev = ppm_dev;
		am_data->ppm_api = ppm_dev->api;
		am_data->mode_entry_state = GET_ALTERNATE_MODES_CONN;

		k_work_init_delayable(&am_data->work,
				      ppm_am_policy_work_handler);
	}

	LOG_INF("PPM Alternate Mode Policy driver started");

	return 0;
}
SYS_INIT(ppm_am_policy_handler_init, APPLICATION, 99);
