/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host commands shared across multiple USB-PD implementations
 */

#include "atomic.h"
#include "ec_commands.h"
#include "host_command.h"
#include "usb_pd.h"
#include "usb_mux.h"

#include <string.h>

__overridable enum ec_pd_port_location board_get_pd_port_location(int port)
{
	(void)port;
	return EC_PD_PORT_LOCATION_UNKNOWN;
}

static enum ec_status hc_get_pd_port_caps(struct host_cmd_handler_args *args)
{
	const struct ec_params_get_pd_port_caps *p = args->params;
	struct ec_response_get_pd_port_caps *r = args->response;

	if (p->port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	/* Power Role */
	if (IS_ENABLED(CONFIG_USB_PD_DUAL_ROLE))
		r->pd_power_role_cap = EC_PD_POWER_ROLE_DUAL;
	else
		r->pd_power_role_cap = EC_PD_POWER_ROLE_SINK;

	/* Try-Power Role */
	if (IS_ENABLED(CONFIG_USB_PD_TRY_SRC))
		r->pd_try_power_role_cap = EC_PD_TRY_POWER_ROLE_SOURCE;
	else
		r->pd_try_power_role_cap = EC_PD_TRY_POWER_ROLE_NONE;

	if (IS_ENABLED(CONFIG_USB_VPD) || IS_ENABLED(CONFIG_USB_CTVPD))
		r->pd_data_role_cap = EC_PD_DATA_ROLE_UFP;
	else
		r->pd_data_role_cap = EC_PD_DATA_ROLE_DUAL;

	/* Allow boards to override the locations from UNKNOWN if desired */
	r->pd_port_location = board_get_pd_port_location(p->port);

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_PD_PORT_CAPS, hc_get_pd_port_caps,
		     EC_VER_MASK(0));

#ifdef CONFIG_HOSTCMD_TYPEC_STATUS
/*
 * Validate ec_response_typec_status_v0's binary compatibility with
 * ec_response_typec_status, which is being deprecated.
 */
BUILD_ASSERT(offsetof(struct ec_response_typec_status_v0,
		      typec_status.sop_prime_revision) ==
	     offsetof(struct ec_response_typec_status, sop_prime_revision));
BUILD_ASSERT(offsetof(struct ec_response_typec_status_v0, source_cap_pdos) ==
	     offsetof(struct ec_response_typec_status, source_cap_pdos));
BUILD_ASSERT(sizeof(struct ec_response_typec_status_v0) ==
	     sizeof(struct ec_response_typec_status));

/*
 * Validate ec_response_typec_status_v0's binary compatibility with
 * ec_response_typec_status_v1 with respect to typec_status.
 */
BUILD_ASSERT(offsetof(struct ec_response_typec_status_v0,
		      typec_status.pd_enabled) ==
	     offsetof(struct ec_response_typec_status_v1,
		      typec_status.pd_enabled));
BUILD_ASSERT(offsetof(struct ec_response_typec_status_v0,
		      typec_status.sop_prime_revision) ==
	     offsetof(struct ec_response_typec_status_v1,
		      typec_status.sop_prime_revision));

static enum ec_status hc_typec_status(struct host_cmd_handler_args *args)
{
	const struct ec_params_typec_status *p = args->params;
	struct ec_response_typec_status_v1 *r1 = args->response;
	struct ec_response_typec_status_v0 *r0 = args->response;
	struct cros_ec_typec_status *cs = &r1->typec_status;
	const char *tc_state_name;

	if (p->port >= board_get_usb_pd_port_count())
		return EC_RES_INVALID_PARAM;

	args->response_size = args->version == 0 ? sizeof(*r0) : sizeof(*r1);

	if (args->response_max < args->response_size)
		return EC_RES_RESPONSE_TOO_BIG;

	cs->pd_enabled = pd_comm_is_enabled(p->port);
	cs->dev_connected = pd_is_connected(p->port);
	cs->sop_connected = pd_capable(p->port);

	cs->power_role = pd_get_power_role(p->port);
	cs->data_role = pd_get_data_role(p->port);
	cs->vconn_role = pd_get_vconn_state(p->port) ? PD_ROLE_VCONN_SRC :
						       PD_ROLE_VCONN_OFF;
	cs->polarity = pd_get_polarity(p->port);
	cs->cc_state = pd_get_task_cc_state(p->port);
	cs->dp_pin = get_dp_pin_mode(p->port);
	cs->mux_state = usb_mux_get(p->port);

	tc_state_name = pd_get_task_state_name(p->port);
	strzcpy(cs->tc_state, tc_state_name, sizeof(cs->tc_state));

	cs->events = pd_get_events(p->port);

	if (pd_get_partner_rmdo(p->port).major_rev != 0) {
		cs->sop_revision =
			PD_STATUS_RMDO_REV_SET_MAJOR(
				pd_get_partner_rmdo(p->port).major_rev) |
			PD_STATUS_RMDO_REV_SET_MINOR(
				pd_get_partner_rmdo(p->port).minor_rev) |
			PD_STATUS_RMDO_VER_SET_MAJOR(
				pd_get_partner_rmdo(p->port).major_ver) |
			PD_STATUS_RMDO_VER_SET_MINOR(
				pd_get_partner_rmdo(p->port).minor_ver);
	} else if (cs->sop_connected) {
		cs->sop_revision = PD_STATUS_REV_SET_MAJOR(
			pd_get_rev(p->port, TCPCI_MSG_SOP));
	} else {
		cs->sop_revision = 0;
	}

	cs->sop_prime_revision =
		pd_get_identity_discovery(p->port, TCPCI_MSG_SOP_PRIME) ==
				PD_DISC_COMPLETE ?
			PD_STATUS_REV_SET_MAJOR(
				pd_get_rev(p->port, TCPCI_MSG_SOP_PRIME)) :
			0;

	if (args->version == 0) {
		cs->source_cap_count = MIN(pd_get_src_cap_cnt(p->port),
					   ARRAY_SIZE(r0->source_cap_pdos));
		memcpy(r0->source_cap_pdos, pd_get_src_caps(p->port),
		       cs->source_cap_count * sizeof(uint32_t));
		cs->sink_cap_count = MIN(pd_get_snk_cap_cnt(p->port),
					 ARRAY_SIZE(r0->sink_cap_pdos));
		memcpy(r0->sink_cap_pdos, pd_get_snk_caps(p->port),
		       cs->sink_cap_count * sizeof(uint32_t));
	} else {
		cs->source_cap_count = MIN(pd_get_src_cap_cnt(p->port),
					   ARRAY_SIZE(r1->source_cap_pdos));
		memcpy(r1->source_cap_pdos, pd_get_src_caps(p->port),
		       cs->source_cap_count * sizeof(uint32_t));
		cs->sink_cap_count = MIN(pd_get_snk_cap_cnt(p->port),
					 ARRAY_SIZE(r1->sink_cap_pdos));
		memcpy(r1->sink_cap_pdos, pd_get_snk_caps(p->port),
		       cs->sink_cap_count * sizeof(uint32_t));
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_TYPEC_STATUS, hc_typec_status,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif /* CONFIG_HOSTCMD_TYPEC_STATUS */

#if !defined(CONFIG_USB_PD_TCPM_STUB)
/*
 * PD host event status for host command
 * Note: this variable must be aligned on 4-byte boundary because we pass the
 * address to atomic_ functions which use assembly to access them.
 */
static atomic_t pd_host_event_status __aligned(4);

test_mockable void pd_send_host_event(int mask)
{
	/* mask must be set */
	if (!mask)
		return;

	atomic_or(&pd_host_event_status, mask);
	/* interrupt the AP */
	host_set_single_event(EC_HOST_EVENT_PD_MCU);
}

static enum ec_status
hc_pd_host_event_status(struct host_cmd_handler_args *args)
{
	struct ec_response_host_event_status *r = args->response;

	/* Read and clear the host event status to return to AP */
	r->status = atomic_clear(&pd_host_event_status);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_HOST_EVENT_STATUS, hc_pd_host_event_status,
		     EC_VER_MASK(0));
#endif /* ! CONFIG_USB_PD_TCPM_STUB */
