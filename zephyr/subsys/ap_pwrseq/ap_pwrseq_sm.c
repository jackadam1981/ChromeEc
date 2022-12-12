/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/logging/log.h>

#include "ap_power/ap_pwrseq_sm.h"

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

struct ap_pwrseq_sm_data {
	/* Zephyr SMF context */
	struct smf_ctx smf;
	/* Pointer to array of states structures */
	const struct ap_pwrseq_smf** states;
	/* Bitfiled of events */
	uint32_t events;
	/* Id of current thread executing state machine */
	k_tid_t tid;
	/* Flag to inform if current `run` action has been handled */
	bool run_handled;
	/* Flag to inform if current `entry` action has been handled */
	bool entry_handled;
	/* Flag to inform if current `exit` action has been handled */
	bool exit_handled;
};

/**
 * Declare weak `struct smf_state` definitions of all ACPI states for
 * architecture and chipset level. These are used as placeholder to keep AP
 * power sequence state machine hierarchy in case corresponding state action
 * handlers are not provided by implementation.
 **/
#define AP_POWER_ARCH_STATE_WEAK_DEFINE(name)                                 \
	const struct smf_state __weak arch_##name##_actions =                 \
		SMF_CREATE_STATE(NULL, NULL, NULL, NULL);

#define AP_POWER_CHIPSET_STATE_WEAK_DEFINE(name)                              \
	const struct smf_state __weak chipset_##name##_actions =              \
		SMF_CREATE_STATE(NULL, NULL, NULL, &arch_##name##_actions);

/**
 * Declare weak `struct ap_pwrseq_smf` definitions of all ACPI states for
 * application level. These are used as placeholder to keep AP
 * power sequence state machine hierarchy in case corresponding state action
 * handlers are not provided by implementation.
 **/
#define AP_POWER_APP_STATE_WEAK_DEFINE(name)                                  \
	const struct ap_pwrseq_smf __weak app_state_##name = {                \
		.actions = SMF_CREATE_STATE(NULL, NULL, NULL,                 \
					    &chipset_##name##_actions),       \
		.state = name};

#define AP_POWER_STATE_WEAK_DEFINE(name)                                      \
	AP_POWER_ARCH_STATE_WEAK_DEFINE(name)                                 \
	AP_POWER_CHIPSET_STATE_WEAK_DEFINE(name)                              \
	AP_POWER_APP_STATE_WEAK_DEFINE(name)

AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_G3)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S5)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S4)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S3)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S2)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S1)
AP_POWER_STATE_WEAK_DEFINE(AP_POWER_STATE_S0)

#define AP_PWRSEQ_STATE_DEFINE(name)                                          \
	[name] = &app_state_##name

/* Sub States defines */
#define AP_PWRSEQ_APP_SUB_STATE_DEFINE(state)                                 \
	[state] = &app_state_##state,

#define AP_PWRSEQ_APP_SUB_STATE_DEFINE_(state)                                \
	AP_PWRSEQ_APP_SUB_STATE_DEFINE(state)

#define AP_PWRSEQ_EACH_APP_SUB_STATE_NODE_DEFINE__(node_id, prop, idx)        \
	AP_PWRSEQ_APP_SUB_STATE_DEFINE_(DT_CAT6(node_id, _P_, prop, _IDX_,    \
						idx, _STRING_UPPER_TOKEN))

#define AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE(state)                   \
	[state] = &chipset_##state##_actions,

#define AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE_(state)                  \
	AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE(state)

#define AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE__(node_id, prop, idx)    \
	AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE_(                        \
			DT_CAT6(node_id, _P_, prop, _IDX_, idx,               \
				_STRING_UPPER_TOKEN))

#define AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DEFINE(node_id)                   \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, chipset),                       \
		(DT_FOREACH_PROP_ELEM(node_id, chipset,                       \
			AP_PWRSEQ_EACH_CHIPSET_SUB_STATE_NODE_DEFINE__)),     \
		(COND_CODE_1(DT_NODE_HAS_PROP(node_id, application),          \
			(DT_FOREACH_PROP_ELEM(node_id, application,           \
			AP_PWRSEQ_EACH_APP_SUB_STATE_NODE_DEFINE__)),         \
			())))

/**
 * @brief Array containing power state action handlers for all state and
 * and substates available for AP power sequence state machine, these items
 * correspond to `enum ap_pwrseq_state`.
 **/
static const struct ap_pwrseq_smf *ap_pwrseq_states[AP_POWER_STATE_COUNT] = {
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_G3),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S5),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S4),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S3),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S2),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S1),
	AP_PWRSEQ_STATE_DEFINE(AP_POWER_STATE_S0),
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_states,
			AP_PWRSEQ_EACH_SUB_STATE_NODE_CHILD_DEFINE)
};

#define AP_PWRSEQ_SUB_STATE_STR_DEFINE_WITH_COMA(state)                       \
	state,

#define AP_PWRSEQ_EACH_SUB_STATE_STR_DEFINE(node_id, prop, idx)               \
	AP_PWRSEQ_SUB_STATE_STR_DEFINE_WITH_COMA(DT_CAT5(node_id, _P_,        \
							 prop, _IDX_, idx))

#define AP_PWRSEQ_EACH_SUB_STATE_STR_DEF_NODE_CHILD_DEFINE(node_id)           \
	COND_CODE_1(DT_NODE_HAS_PROP(node_id, application),                   \
		(DT_FOREACH_PROP_ELEM(node_id, application,                   \
				      AP_PWRSEQ_EACH_SUB_STATE_STR_DEFINE)),  \
		(COND_CODE_1(DT_NODE_HAS_PROP(node_id, chipset),              \
			(DT_FOREACH_PROP_ELEM(node_id, chipset,               \
				      AP_PWRSEQ_EACH_SUB_STATE_STR_DEFINE)),  \
			())))

/**
 * @brief String of all states and substates availabel for AP power seqence
 * state machine.
 **/
static const char * const ap_pwrseq_state_str[AP_POWER_STATE_COUNT] = {
	"AP_POWER_STATE_PG3",
	"AP_POWER_STATE_G3",
	"AP_POWER_STATE_S5",
	"AP_POWER_STATE_S4",
	"AP_POWER_STATE_S3",
	"AP_POWER_STATE_S2",
	"AP_POWER_STATE_S1",
	"AP_POWER_STATE_S0",
	DT_FOREACH_STATUS_OKAY(ap_pwrseq_sub_states,
		AP_PWRSEQ_EACH_SUB_STATE_STR_DEF_NODE_CHILD_DEFINE)
};

static struct ap_pwrseq_sm_data sm_data_0 = {
	.states = ap_pwrseq_states,
};

__overridable enum ap_pwrseq_state ap_pwrseq_board_sm_init(void *const data)
{
	return AP_POWER_STATE_G3;
}

void * ap_pwrseq_sm_get_instance(void)
{
	return &sm_data_0;
}

int ap_pwrseq_sm_init(void *const data, k_tid_t tid)
{
	struct ap_pwrseq_sm_data *sm_data = data;
	enum ap_pwrseq_state state = AP_POWER_STATE_UNDEF;

	if (sm_data->smf.current || sm_data->tid) {
		return -EPERM;
	}

	state = ap_pwrseq_board_sm_init(data);
	if (state >= AP_POWER_STATE_COUNT) {
		return -EINVAL;
	}

	smf_set_initial(&sm_data->smf,
		        (const struct smf_state*)sm_data->states[state]);
	sm_data->tid = tid;
	return 0;
}

int ap_pwrseq_sm_set_state(void *const data, enum ap_pwrseq_state state)
{
	struct ap_pwrseq_sm_data *sm_data = data;

	if (state >= AP_POWER_STATE_COUNT) {
		return -EINVAL;
	}

	if (sm_data->tid != k_current_get()) {
		/**
		 * This function is meant to be executed only within AP power
		 * sequence driver thread context. `tid` was given on
		 * `ap_pwrseq_sm_init` function.
		 **/
		return -EPERM;
	}
	sm_data->entry_handled = sm_data->exit_handled = false;
	smf_set_state((struct smf_ctx *const)&sm_data->smf,
		(const struct smf_state*)sm_data->states[state]);
	return 0;
}

bool ap_pwrseq_sm_is_event_set(void *const data, enum ap_pwrseq_event event)
{
	struct ap_pwrseq_sm_data *sm_data = data;

	return !!((sm_data->events & BIT(event)) == BIT(event));
}

int ap_pwrseq_sm_run_state(void *const data, uint32_t events)
{
	struct ap_pwrseq_sm_data *sm_data = data;

	if (sm_data->smf.current == NULL) {
		return -EINVAL;
	}

	if (sm_data->tid != k_current_get()) {
		/**
		 * This function is meant to be executed only within AP power
		 * sequence driver thread context. `tid` was given on
		 * `ap_pwrseq_sm_init` function.
		 **/
		return -EPERM;
	}

	sm_data->run_handled = false;
	sm_data->events = events;
	return smf_run_state((struct smf_ctx *const)sm_data);
}

enum ap_pwrseq_state ap_pwrseq_sm_get_prev_state(void *const data)
{
	struct ap_pwrseq_sm_data *sm_data = data;

	if (sm_data->smf.previous == NULL) {
		/* Undefined state since we are at the begining */
		return AP_POWER_STATE_UNDEF;
	}
	return ((struct ap_pwrseq_smf *)sm_data->smf.previous)->state;
}

enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(void *const data)
{
	struct ap_pwrseq_sm_data *sm_data = data;

	if (sm_data->smf.current == NULL) {
		/* Undefined state since we have not started */
		return AP_POWER_STATE_UNDEF;
	}
	return ((struct ap_pwrseq_smf *)sm_data->smf.current)->state;
}

const char *const ap_pwrseq_sm_get_state_str(enum ap_pwrseq_state state)
{
	if (state >=AP_POWER_STATE_COUNT) {
		return NULL;
	}
	return ap_pwrseq_state_str[state];
}

#define AP_POWER_SM_HANDLER_DEF(action)                                       \
	void ap_pwrseq_sm_exec_##action##_handler(void *const data,           \
						  ap_pwr_state_action_handler \
						  handler)                    \
	{                                                                     \
		struct ap_pwrseq_sm_data *sm_data = data;                     \
		if (handler && !sm_data->action##_handled)                    \
			sm_data->action##_handled = !!handler(data);          \
	}

AP_POWER_SM_HANDLER_DEF(entry)
AP_POWER_SM_HANDLER_DEF(run)
AP_POWER_SM_HANDLER_DEF(exit)
