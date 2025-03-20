/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UCSI PPM altnernate mode policy handler */

#include "config.h"
#include "ppm_utils.h"
#include "usb_pd.h"
#include "zephyr/sys/util.h"

#include <errno.h>

#include <zephyr/devicetree.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <usbc/ppm.h>

LOG_MODULE_REGISTER(ppm_am_policy);

#define PPM_MAX_ALT_MODES 8

enum mode_entry_state {
	GET_ALTERNATE_MODES_CONN,
	GET_ALTERNATE_MODES_SOP,
	GET_ALTERNATE_MODES_SOP_PRIME,
	GET_CAM_SUPPORTED,
	SET_NEW_CAM_EXIT_DP,
	SET_NEW_CAM_ENTER_TBT,
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
	[SET_NEW_CAM_EXIT_DP] = "Set new CAM - exit DP mode",
	[SET_NEW_CAM_ENTER_TBT] = "Set new CAM - enable TBT mode",
};

static int ppm_get_alternate_modes_helper(struct ppm_am_policy_data *am_data,
					  enum alt_modes_recipient recipient)
{
	struct ucsi_altmode_field ucsi_alt_modes[8];
	int num_alt_modes = 0;
	int rv;

	rv = ppm_get_alternate_modes(am_data->ppm_dev, am_data->connector_num,
				     recipient, ARRAY_SIZE(ucsi_alt_modes),
				     ucsi_alt_modes, &num_alt_modes);

	for (int i = 0; i < num_alt_modes; i++) {
		if (ucsi_alt_modes[i].svid == USB_VID_INTEL) {
			am_data->tbt_supported[recipient] = true;
		}
		am_data->svids[recipient][i] = ucsi_alt_modes[i].svid;
	}

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

	rv = ppm_set_new_cam(am_data->ppm_dev, am_data->connector_num, true,
			     new_cam, 0);

	return rv;
}

static int ppm_exit_dp(struct ppm_am_policy_data *am_data)
{
	uint8_t new_cam = 0xFF;
	int rv;

	/* Find the CAM index corresponding to the DP alt mode
	 * by the partner (SOP).
	 *
	 * TODO: The UCSI spec implies that the New CAM field is an index
	 * into the alternate modes supported by the PPM (connector).
	 * Testing shows these indexes correspond to the partner (SOP) alternate
	 * mode list.
	 */
	for (int i = 0; i < PPM_MAX_ALT_MODES; i++) {
		if (am_data->svids[RECIPIENT_SOP][i] == USB_SID_DISPLAYPORT) {
			new_cam = i;
			break;
		}
	}

	if (new_cam == 0xFF) {
		/* Couldn't find the DP SVID.  This shouldn't happen. */
		return -EINVAL;
	}

	rv = ppm_set_new_cam(am_data->ppm_dev, am_data->connector_num, false,
			     new_cam, 0);

	return rv;
}

static void ppm_am_policy_work_handler(struct k_work *work)
{
	struct k_work_delayable *d_work = k_work_delayable_from_work(work);
	struct ppm_am_policy_data *am_data =
		CONTAINER_OF(d_work, struct ppm_am_policy_data, work);
	bool am_complete = false;
	bool tbt_supported;
	int rv;
	size_t resp_size;
	k_timeout_t work_delay = K_MSEC(25);

	LOG_INF("PPM%d: mode entry state %s", am_data->connector_num,
		mode_entry_state_names[am_data->mode_entry_state]);

	/* Note - the PPM command path is synchronous. We exit after each
	 * command to ensure we service other policy requests.
	 */

	switch (am_data->mode_entry_state) {
	case GET_ALTERNATE_MODES_CONN:
		rv = ppm_get_alternate_modes_helper(am_data,
						    RECIPIENT_CONNECTOR);
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
		rv = ppm_get_cam_supported(am_data->ppm_dev,
					   am_data->connector_num, 1,
					   &am_data->cam_supported_mask,
					   &resp_size);
		if (rv != 0 || (resp_size != 1)) {
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
		rv = ppm_get_alternate_modes_helper(am_data, RECIPIENT_SOP);
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
		rv = ppm_get_alternate_modes_helper(am_data,
						    RECIPIENT_SOP_PRIME);
		if (rv != 0) {
			/* Failed to read alternate modes - continue. */
			am_complete = true;
			break;
		}
		if (am_data->tbt_supported[RECIPIENT_SOP_PRIME]) {
			am_data->mode_entry_state = SET_NEW_CAM_EXIT_DP;
			work_delay = K_MSEC(2000);
		} else {
			am_complete = true;
		}
		break;

	case SET_NEW_CAM_EXIT_DP:
		rv = ppm_exit_dp(am_data);
		if (rv != 0) {
			am_complete = true;
			break;
		}
		am_data->mode_entry_state = SET_NEW_CAM_ENTER_TBT;
		break;
	case SET_NEW_CAM_ENTER_TBT:
#if 0
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
#endif
		ppm_enter_tbt_usb4(am_data);

		am_complete = true;
		break;
	}

	if (!am_complete) {
		k_work_schedule(&am_data->work, work_delay);
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

	k_work_schedule(&am_data->work, K_MSEC(1000));

	return 0;
}

int ppm_am_policy_handler_init(void)
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
