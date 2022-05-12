/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <ap_power/ap_pwrseq_sm.h>
#include <logging/log.h>

#define AP_PWRSEQ_ARCH_STATE_UNDEFINE(state)                                    \
	struct smf_state arch_##state =                                       \
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL);

#define AP_PWRSEQ_ARCH_STATE_DEFINE_WITH_COMMA(state, defined)                \
	COND_CODE_1(defined, (),            \
		(AP_PWRSEQ_ARCH_STATE_UNDEFINE(state)))

#define AP_PWRSEQ_EACH_ARCH_STATE_NODE_DEFINE(node_id)                        \
	AP_PWRSEQ_ARCH_STATE_DEFINE_WITH_COMMA(                               \
		DT_STRING_TOKEN(node_id, state),                              \
		DT_PROP(node_id, arch_action_enabled))

#define AP_PWRSEQ_EACH_ARCH_STATE_NODE_CHILD_DEFINE(node_id)                  \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_ARCH_STATE_NODE_DEFINE)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
		AP_PWRSEQ_EACH_ARCH_STATE_NODE_CHILD_DEFINE)

#define AP_PWRSEQ_CHIPSET_STATE_UNDEFINE(state)                                    \
	struct smf_state chipset_##state =                                       \
		SMF_CREATE_STATE(NULL, NULL, NULL, &arch_##state);

#define AP_PWRSEQ_CHIPSET_STATE_DEFINE_WITH_COMMA(state, defined)                \
	COND_CODE_1(defined, (),            \
		(AP_PWRSEQ_CHIPSET_STATE_UNDEFINE(state)))

#define AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_DEFINE(node_id)                        \
	AP_PWRSEQ_CHIPSET_STATE_DEFINE_WITH_COMMA(                               \
		DT_STRING_TOKEN(node_id, state),                              \
		DT_PROP(node_id, chipset_action_enabled))

#define AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_CHILD_DEFINE(node_id)                  \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_DEFINE)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
		AP_PWRSEQ_EACH_CHIPSET_STATE_NODE_CHILD_DEFINE)

#define AP_PWRSEQ_STATE_UNDEFINE(_state)                                    \
	struct ap_pwrseq_smf state_##_state = {                              \
		.actions = SMF_CREATE_STATE(NULL, NULL, NULL, &chipset_##_state), \
		.state = _state};

#define AP_PWRSEQ_STATE_EMPTY_DEFINE(state, defined)                \
	COND_CODE_1(defined, (),            \
		(AP_PWRSEQ_STATE_UNDEFINE(state)))

#define AP_PWRSEQ_EACH_EMPTY_STATE_NODE_DEFINE(node_id)                        \
	AP_PWRSEQ_STATE_EMPTY_DEFINE(                               \
		DT_STRING_TOKEN(node_id, state),                              \
		DT_PROP(node_id, action_enabled))

#define AP_PWRSEQ_EACH_STATE_EMPTY_NODE_CHILD_DEFINE(node_id)                  \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_EMPTY_STATE_NODE_DEFINE)

DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
		AP_PWRSEQ_EACH_STATE_EMPTY_NODE_CHILD_DEFINE)

/* Sub States defines */
#define AP_PWRSEQ_SUB_STATE_DEFINE(state)                                 \
	[state] = &sub_state_##state,

#define AP_PWRSEQ_SUB_STATE_DEFINE_(state)             \
	AP_PWRSEQ_SUB_STATE_DEFINE(state)

#define AP_PWRSEQ_EACH_SUB_STATE_NODE_DEFINE(node_id)                     \
		AP_PWRSEQ_SUB_STATE_DEFINE_(DT_STRING_TOKEN(node_id, state))

#define AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DEFINE(node_id)               \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_SUB_STATE_NODE_DEFINE)

/* States defines */
#define AP_PWRSEQ_STATE_DEFINE(state)             \
	[state] = &state_##state,

#define AP_PWRSEQ_STATE_DEFINE_(state)             \
	AP_PWRSEQ_STATE_DEFINE(state)

#define AP_PWRSEQ_EACH_STATE_NODE_DEFINE(node_id)                     \
	AP_PWRSEQ_STATE_DEFINE_(                            \
		DT_STRING_TOKEN(node_id, state))

#define AP_PWRSEQ_EACH_STATE_NODE_CHILD_DEFINE(node_id)               \
	DT_FOREACH_CHILD(node_id, AP_PWRSEQ_EACH_STATE_NODE_DEFINE)

static const struct ap_pwrseq_smf *ap_pwrseq_states[AP_POWER_STATE_COUNT] = {
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_state,
			AP_PWRSEQ_EACH_STATE_NODE_CHILD_DEFINE)
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_state,
			AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DEFINE)
};

#define AP_POWER_SUB_STATE_ENUM_STR_DEF_WITH_COMMA(node_id)                           \
	DT_PROP(node_id, state),

#define AP_POWER_SUB_STATE_ENUM_STR_DEF(node_id)                                      \
	DT_FOREACH_CHILD(node_id, AP_POWER_SUB_STATE_ENUM_STR_DEF_WITH_COMMA)

static const char * const ap_pwrseq_state_str[AP_POWER_STATE_COUNT] = {
	"AP_POWER_STATE_G3",
	"AP_POWER_STATE_S5",
	"AP_POWER_STATE_S4",
	"AP_POWER_STATE_S3",
	"AP_POWER_STATE_S2",
	"AP_POWER_STATE_S1",
	"AP_POWER_STATE_S0",
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_state, AP_POWER_SUB_STATE_ENUM_STR_DEF)
};

static struct ap_pwrseq_sm_data sm_data = {
	.states = ap_pwrseq_states,
};

struct ap_pwrseq_sm_data * ap_pwrseq_sm_get_instance(void)
{
	return &sm_data;
}

int ap_pwrseq_sm_init(struct ap_pwrseq_sm_data *const data,
		      enum ap_pwrseq_state state)
{
	if (state >= AP_POWER_STATE_COUNT) {
		return -EINVAL;
	}

	if (data->smf.current == NULL) {
		smf_set_initial(&data->smf,
				(const struct smf_state*)data->states[state]);
		return 0;
	}

	return -EPERM;
}

int ap_pwrseq_sm_set_state(struct ap_pwrseq_sm_data *const data,
			   enum ap_pwrseq_state state)
{
	if ((state >= AP_POWER_STATE_COUNT) ||
	     data->status != AP_PWRSEQ_SM_STATUS_READY) {
		return -EINVAL;
	}
	smf_set_state((struct smf_ctx *const)&data->smf,
			(const struct smf_state*)data->states[state]);
	return 0;
}

int ap_pwrseq_sm_set_events(struct ap_pwrseq_sm_data *const data, uint32_t events)
{
	data->events = events;
	return 0;
}

int ap_pwrseq_sm_run_state(struct ap_pwrseq_sm_data *const data)
{
	if (data->smf.current == NULL) {
		return -ENOMEM;
	}
	data->status = AP_PWRSEQ_SM_STATUS_READY;
	return smf_run_state((struct smf_ctx *const)data);
}

enum ap_pwrseq_state ap_pwrseq_sm_get_prev_state(struct ap_pwrseq_sm_data
					         *const data)
{
	if (data->smf.previous == NULL) {
		/* Undefined state since we are at the begining */
		return AP_POWER_STATE_UNDEF;
	}
	return ((struct ap_pwrseq_smf *)data->smf.previous)->state;
}

enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(struct ap_pwrseq_sm_data
						*const data)
{
	if (data->smf.current == NULL) {
		/* Undefined state since we have not started */
		return AP_POWER_STATE_UNDEF;
	}
	return ((struct ap_pwrseq_smf *)data->smf.current)->state;
}

const char *const ap_pwrseq_get_state_str(enum ap_pwrseq_state state)
{
	if (state >=AP_POWER_STATE_COUNT) {
		return NULL;
	}
	return ap_pwrseq_state_str[state];
}
