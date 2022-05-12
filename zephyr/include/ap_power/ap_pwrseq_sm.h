/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AP_PWRSEQ_SM_H_
#define _AP_PWRSEQ_SM_H_
#include <device.h>
#include <ap_power/ap_pwrseq_sm_defs.h>

#define EXIT_IF_NOT_READY(data)                             do {   \
		if (data->status != AP_PWRSEQ_SM_STATUS_READY) {return;}}while(0)

#define SET_NOT_READY(data)                                     \
		data->status = AP_PWRSEQ_SM_STATUS_NOT_READY

#define IS_READY(data)                                     \
		(data->status == AP_PWRSEQ_SM_STATUS_READY)

#define IS_EVENT_SET(data, event)                                 \
		(data->events & BIT(event))

#define AP_POWER_ARCH_STATE_DEFINE(state, entry, run, exit)                   \
	struct smf_state arch_##state =                                       \
		SMF_CREATE_STATE(entry, run, exit, NULL);

#define AP_POWER_CHIPSET_STATE_DEFINE(state, entry, run, exit)                \
	struct smf_state chipset_##state =                                       \
		SMF_CREATE_STATE(entry, run, exit, &arch_##state);

#define AP_POWER_STATE_DEFINE(_state, entry, run, exit)       \
	struct ap_pwrseq_smf state_##_state = {                              \
		.actions = SMF_CREATE_STATE(entry, run, exit, &chipset_##_state), \
		.state = _state};

#define AP_POWER_SUB_STATE_DEFINE(_state, entry, run, exit, parent_state)       \
	struct ap_pwrseq_smf sub_state_##_state = {                              \
		.actions = SMF_CREATE_STATE(entry, run, exit, &chipset_##parent_state), \
		.state = _state};

enum ap_pwrseq_sm_status {
	AP_PWRSEQ_SM_STATUS_READY,
	AP_PWRSEQ_SM_STATUS_NOT_READY,
};

struct ap_pwrseq_sm_data {
	struct smf_ctx smf;
	const struct ap_pwrseq_smf** states;
	uint32_t events;
	enum ap_pwrseq_sm_status status;
};

struct ap_pwrseq_sm_data * ap_pwrseq_sm_get_instance(void);

int ap_pwrseq_sm_init(struct ap_pwrseq_sm_data *const data,
		      enum ap_pwrseq_state state);

int ap_pwrseq_sm_set_events(struct ap_pwrseq_sm_data *const data, uint32_t events);

int ap_pwrseq_sm_set_state(struct ap_pwrseq_sm_data *const data,
			   enum ap_pwrseq_state state);

int ap_pwrseq_sm_run_state(struct ap_pwrseq_sm_data *const data);

enum ap_pwrseq_state ap_pwrseq_sm_get_prev_state(struct ap_pwrseq_sm_data
						 *const data);

enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(struct ap_pwrseq_sm_data
						*const data);

const char *const ap_pwrseq_get_state_str(enum ap_pwrseq_state state);

#endif //_AP_PWRSEQ_SM_H_
