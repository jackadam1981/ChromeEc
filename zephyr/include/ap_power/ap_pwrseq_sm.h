/*
 * Copyright 2022 Intel Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _AP_PWRSEQ_SM_H_
#define _AP_PWRSEQ_SM_H_
#include <zephyr/device.h>

#include "ap_power/ap_pwrseq_sm_defs.h"

/*
 * This is required to ensure macro AP_POWER_SM_DEF_STATE_HANDLER handles
 * passing `NULL` properly.
 */
#ifdef NULL
#undef NULL
#define NULL 0
#endif

/**
 * @brief Check if event is set.
 *
 * @param data State machine data pointer.
 *
 * @param event Event to be check if set.
 *
 * @retval True Event has been set, return False otherwise.
 **/
#define IS_EVENT_SET(data, event)                                             \
	(((struct ap_pwrseq_sm_data *)(data))->events & BIT(event))

/* User define action handler, each action handler must follow this type. */
typedef int (*ap_pwr_state_action_handler)(void *data);

/**
 * @brief Macro to define action handler wrapper function.
 *
 * @param name Valid enumaration value of state.
 *
 * @param level One of the three AP power sequence levels: arch, chipset or app.
 *
 * @param action One of the three SMF action handlers: entry, run or exit.
 *
 * @param handler Action handler function of type `ap_pwr_state_action_handler`.
 *
 * @retval Defines static wrapper function of handler to be called by AP power
 * sequence state machine.
 **/
#define AP_POWER_SM_DEF_STATE_HANDLER(name, level, action, handler)           \
	static void ap_pwr_##name##_##level##_##action##_##handler(void *data)\
	{                                                                     \
		ap_pwr_state_action_handler action_handler = handler;         \
		struct ap_pwrseq_sm_data *sm_data = data;                     \
		if (action_handler && !sm_data->action##_handled)             \
			sm_data->action##_handled = !!action_handler(data);   \
	}

/**
 * @brief Macro to assemble action handler wrapper function name.
 *
 * @param name Valid enumaration value of state.
 *
 * @param level One of the three AP power sequence levels: arch, chipset or app.
 *
 * @param action One of the three SMF action handlers: entry, run or exit.
 *
 * @param handler Action handler function of type `ap_pwr_state_action_handler`.
 *
 * @retval Constructs static name of handler wrapper function to be called by
 * AP power sequence state machine.
 **/
#define AP_POWER_SM_ACTION(name, level, action, handler)                      \
		ap_pwr_##name##_##level##_##action##_##handler

/**
 * @brief Macro to create SMF state following AP power sequence.
 *
 * @param name Valid enumaration value of state.
 *
 * @param level One of the three AP power sequence levels: arch, chipset or app.
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_SM_CREATE_STATE(name, level, _entry, _run, _exit, parent)    \
	SMF_CREATE_STATE(AP_POWER_SM_ACTION(name, level, entry, _entry),      \
			 AP_POWER_SM_ACTION(name, level, run, _run),          \
			 AP_POWER_SM_ACTION(name, level, exit, _exit),        \
			 parent)

/**
 * @brief Define architecture level state action handlers.
 *
 * @param name Valid enumaration value of state.
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_ARCH_STATE_DEFINE(name, _entry, _run, _exit)                 \
	AP_POWER_SM_DEF_STATE_HANDLER(name, arch, entry, _entry)              \
	AP_POWER_SM_DEF_STATE_HANDLER(name, arch, run, _run)                  \
	AP_POWER_SM_DEF_STATE_HANDLER(name, arch, exit, _exit)                \
	const struct smf_state arch_##name##_actions =                        \
		AP_POWER_SM_CREATE_STATE(name, arch, _entry, _run, _exit, NULL);

/**
 * @brief Define chipset level state action handlers.
 *
 * @param name Valid enumaration value of state.
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_CHIPSET_STATE_DEFINE(name, _entry, _run, _exit)              \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, entry, _entry)           \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, run, _run)               \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, exit, _exit)             \
	const struct smf_state chipset_##name##_actions =                     \
		AP_POWER_SM_CREATE_STATE(name, chipset, _entry, _run, _exit,  \
					 &arch_##name##_actions);

/**
 * @brief Define application level state action handlers.
 *
 * @param name Valid enumaration value of state.
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_APP_STATE_DEFINE(name, _entry, _run, _exit)                  \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, entry, _entry)               \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, run, _run)                   \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, exit, _exit)                 \
const struct ap_pwrseq_smf app_state_##name = {                               \
		.actions = AP_POWER_SM_CREATE_STATE(name, app, _entry, _run,  \
			 _exit,	&chipset_##name##_actions),                   \
		.state = name};

/**
 * @brief Define chipset level substate action handlers.
 *
 * @param name Valid enumaration value of state, as provided by devicetree
 * compatible with "ap-pwrseq-sub-states".
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @param parent Valid enumaration value of parent state,
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_CHIPSET_SUB_STATE_DEFINE(name, _entry, _run, _exit, parent)  \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, entry, _entry)           \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, run, _run)               \
	AP_POWER_SM_DEF_STATE_HANDLER(name, chipset, exit, _exit)             \
	const struct ap_pwrseq_smf chipset_##name##_actions = {               \
		.actions = AP_POWER_SM_CREATE_STATE(name, chipset, _entry,    \
		 _run, _exit, &arch_##parent##_actions),                      \
		.state = name};

/**
 * @brief Define application level substate action handlers.
 *
 * @param name Valid enumaration value of state, as provided by devicetree
 * compatible with "ap-pwrseq-sub-states".
 *
 * @param _entry Function to be called when entrying state.
 *
 * @param _run Function to be called when executing `run` operation.
 *
 * @param _exit Function to be called when exiting state.
 *
 * @param parent Valid enumaration value of parent state,
 *
 * @retval Defines global structure with action handlers to be used by AP
 * power sequence state machine.
 **/
#define AP_POWER_APP_SUB_STATE_DEFINE(name, _entry, _run, _exit, parent)      \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, entry, _entry)               \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, run, _run)                   \
	AP_POWER_SM_DEF_STATE_HANDLER(name, app, exit, _exit)                 \
	const struct ap_pwrseq_smf app_state_##name = {                       \
		.actions = AP_POWER_SM_CREATE_STATE(name, app, _entry, _run,  \
			 _exit, &chipset_##parent##_actions),                 \
		.state = name};

typedef int (*ap_pwrseq_sm_init_func)(void *dev);

typedef enum ap_pwrseq_state (*ap_pwrseq_sm_get_init_state)(void *dev);

#define AP_POWER_INIT_FUNC(func) ap_pwrseq_sm_init_func init_func = func;

#define AP_POWER_INIT_STATE_FUNC(func) ap_pwrseq_sm_get_init_state      \
						get_init_state_func = func;

struct ap_pwrseq_sm_data {
	/* Zephyr SMF context */
	struct smf_ctx smf;
	/* Pointer to array of states structures */
	const struct ap_pwrseq_smf** states;
	/* Bitfiled of events */
	uint32_t events;
	/* Flag to inform if current `run` action has been handled */
	bool run_handled;
	/* Flag to inform if current `entry` action has been handled */
	bool entry_handled;
	/* Flag to inform if current `exit` action has been handled */
	bool exit_handled;
};

/**
 * @brief Obtain AP power sequence state machine instance.
 *
 * @param None.
 *
 * @retval Return instance data of the state machine, only one instance is
 * allowed per application.
 **/
struct ap_pwrseq_sm_data * ap_pwrseq_sm_get_instance(void);

/**
 * @brief Sets AP power sequence state machine initial state.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval SUCCESS Upon success, state ‘entry’ action handlers on all
 * implemented levels will be invoked.
 * @retval -EINVAL State provided is invalid.
 * @retval -EPERM  State machine is already initialized.
 **/
int ap_pwrseq_sm_init(struct ap_pwrseq_sm_data *const data);

/**
 * @brief Sets AP power sequence state machine to provided state.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param state Enum value of next state to be executed.
 *
 * @retval SUCCESS Upon success, current state `exit` action handler and next
 * state `entry` action handler will be executed.
 * @retval -EINVAL State provided is invalid.
 **/
int ap_pwrseq_sm_set_state(struct ap_pwrseq_sm_data *const data,
			   enum ap_pwrseq_state state);

/**
 * @brief Sets events to be processed by AP power sequence state machine.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param events Bitfield of events bein triggrered to be processed.
 *
 * @retval SUCCESS Events have been set.
 **/
int ap_pwrseq_sm_set_events(struct ap_pwrseq_sm_data *const data,
			    uint32_t events);

/**
 * @brief Execute current state `run` action handlers.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval SUCCESS Upon success, provided `run` action handlers will be executed
 * for all levels in current state.
 * @retval -EINVAL State machine has not been initialized.
 **/
int ap_pwrseq_sm_run_state(struct ap_pwrseq_sm_data *const data);

/**
 * @brief Get current state enumeration value.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval Enum value Upon success
 * @retval AP_POWER_STATE_UNDEF If state machine has not been initialized.
 **/
enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(struct ap_pwrseq_sm_data
						*const data);
/**
 * @brief Get previous state enumeration value.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval Enum value Upon success
 * @retval AP_POWER_STATE_UNDEF If state machine is at initial state and no
 * previous state is available.
 **/
enum ap_pwrseq_state ap_pwrseq_sm_get_prev_state(struct ap_pwrseq_sm_data
						 *const data);

/**
 * @brief Get enumeration state value corresponding string value.
 *
 * @param state Enumeration value of state.
 *
 * @retval String value containing the state name.
 * @retval NULL If state provided is invalid.
 **/
const char *const ap_pwrseq_sm_get_state_str(enum ap_pwrseq_state state);

#endif //_AP_PWRSEQ_SM_H_
