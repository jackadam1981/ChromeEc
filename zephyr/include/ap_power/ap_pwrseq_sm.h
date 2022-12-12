/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef _AP_PWRSEQ_SM_H_
#define _AP_PWRSEQ_SM_H_
#include "ap_power/ap_pwrseq_sm_defs.h"
#include "common.h"

#include <zephyr/device.h>

/*
 * This is required to ensure macro AP_POWER_SM_DEF_STATE_HANDLER handles
 * passing `NULL` properly.
 */
#ifdef NULL
#undef NULL
#define NULL 0
#endif

/* User define action handler, each action handler must follow this type. */
typedef int (*ap_pwr_state_action_handler)(void *data);

#define AP_POWER_SM_HANDLER_DECL(action)           \
	void ap_pwrseq_sm_exec_##action##_handler( \
		void *const data, ap_pwr_state_action_handler handler)

AP_POWER_SM_HANDLER_DECL(entry);
AP_POWER_SM_HANDLER_DECL(run);
AP_POWER_SM_HANDLER_DECL(exit);

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
#define AP_POWER_SM_DEF_STATE_HANDLER(name, level, action, handler)            \
	static void ap_pwr_##name##_##level##_##action##_##handler(void *data) \
	{                                                                      \
		ap_pwrseq_sm_exec_##action##_handler(data, handler);           \
	}

/**
 * @brief Macro to define action handler wrapper function for a single level.
 *
 * @param name Valid enumaration value of state.
 *
 * @param level One of the three AP power sequence levels: arch, chipset or app.
 *
 * @param _entry Function called when entering into this state.
 *
 * @param _run Action handler function called when run operation is invoked.
 *
 * @param _exit Function called when exiting this state.
 *
 * @param handler Action handler function of type `ap_pwr_state_action_handler`.
 *
 * @retval Defines static wrapper function of handler to be called by AP power
 * sequence state machine.
 **/
#define AP_POWER_SM_DEF_STATE_HANDLERS(name, level, _entry, _run, _exit) \
	AP_POWER_SM_DEF_STATE_HANDLER(name, level, entry, _entry)        \
	AP_POWER_SM_DEF_STATE_HANDLER(name, level, run, _run)            \
	AP_POWER_SM_DEF_STATE_HANDLER(name, level, exit, _exit)

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
#define AP_POWER_SM_ACTION(name, level, action, handler) \
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
#define AP_POWER_SM_CREATE_STATE(name, level, _entry, _run, _exit, parent) \
	SMF_CREATE_STATE(AP_POWER_SM_ACTION(name, level, entry, _entry),   \
			 AP_POWER_SM_ACTION(name, level, run, _run),       \
			 AP_POWER_SM_ACTION(name, level, exit, _exit), parent)

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
#define AP_POWER_ARCH_STATE_DEFINE(name, entry, run, exit)           \
	AP_POWER_SM_DEF_STATE_HANDLERS(name, arch, entry, run, exit) \
	const struct smf_state arch_##name##_actions =               \
		AP_POWER_SM_CREATE_STATE(name, arch, entry, run, exit, NULL)

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
#define AP_POWER_CHIPSET_STATE_DEFINE(name, entry, run, exit)             \
	AP_POWER_SM_DEF_STATE_HANDLERS(name, chipset, entry, run, exit)   \
	const struct smf_state chipset_##name##_actions =                 \
		AP_POWER_SM_CREATE_STATE(name, chipset, entry, run, exit, \
					 &arch_##name##_actions)

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
#define AP_POWER_APP_STATE_DEFINE(name, entry, run, exit)                     \
	AP_POWER_SM_DEF_STATE_HANDLERS(name, app, entry, run, exit)           \
	const struct ap_pwrseq_smf app_state_##name = {                       \
		.actions =                                                    \
			AP_POWER_SM_CREATE_STATE(name, app, entry, run, exit, \
						 &chipset_##name##_actions),  \
		.state = name                                                 \
	}

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
#define AP_POWER_CHIPSET_SUB_STATE_DEFINE(name, entry, run, exit, parent)      \
	AP_POWER_SM_DEF_STATE_HANDLERS(name, chipset, entry, run, exit)        \
	const struct ap_pwrseq_smf chipset_##name##_actions = {                \
		.actions = AP_POWER_SM_CREATE_STATE(name, chipset, entry, run, \
						    exit,                      \
						    &arch_##parent##_actions), \
		.state = name                                                  \
	}

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
#define AP_POWER_APP_SUB_STATE_DEFINE(name, entry, run, exit, parent)          \
	AP_POWER_SM_DEF_STATE_HANDLERS(name, app, entry, run, exit)            \
	const struct ap_pwrseq_smf app_state_##name = {                        \
		.actions =                                                     \
			AP_POWER_SM_CREATE_STATE(name, app, entry, run, exit,  \
						 &chipset_##parent##_actions), \
		.state = name                                                  \
	}

__override_proto enum ap_pwrseq_state ap_pwrseq_board_sm_init(void *const data);

/**
 * @brief Obtain AP power sequence state machine instance.
 *
 * @param None.
 *
 * @retval Return instance data of the state machine, only one instance is
 * allowed per application.
 **/
void *ap_pwrseq_sm_get_instance(void);

/**
 * @brief Sets AP power sequence state machine initial state.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param tid AP power sequence instance thread associated to this state
 * machine. Functions `ap_pwrseq_sm_set_state` and `ap_pwrseq_sm_run_state` are
 * meant to be executed only within this thread context.
 *
 * @retval SUCCESS Upon success, state ‘entry’ action handlers on all
 * implemented levels will be invoked.
 * @retval -EINVAL State provided is invalid.
 * @retval -EPERM  State machine is already initialized.
 **/
int ap_pwrseq_sm_init(void *const data, k_tid_t tid);

/**
 * @brief Sets AP power sequence state machine to provided state.
 *
 * This function is meant to be executed only within AP power sequence driver
 * thread context. `tid` was given in `ap_pwrseq_sm_init`.
 *
 * Only one state transition is permited within `run` iterations.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param state Enum value of next state to be executed.
 *
 * @retval SUCCESS Upon success, current state `exit` action handler and next
 * state `entry` action handler will be executed.
 * @retval -EINVAL State provided is invalid.
 **/
int ap_pwrseq_sm_set_state(void *const data, enum ap_pwrseq_state state);

/**
 * @brief Check if events is set for current AP power sequence state machine
 * `run` iteration.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param event Enum of test to be tested.
 *
 * @retval True If event is set, False otherwise.
 **/
bool ap_pwrseq_sm_is_event_set(void *const data, enum ap_pwrseq_event event);

/**
 * @brief Execute current state `run` action handlers.
 *
 * This function is meant to be executed only within AP power sequence driver
 * thread context. `tid` was given in `ap_pwrseq_sm_init`.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @param events Events to be processed in current `run` iteration.
 *
 * @retval SUCCESS Upon success, provided `run` action handlers will be executed
 * for all levels in current state.
 * @retval -EINVAL State machine has not been initialized.
 **/
int ap_pwrseq_sm_run_state(void *const data, uint32_t events);

/**
 * @brief Get current state enumeration value.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval Enum value Upon success.
 * @retval AP_POWER_STATE_UNDEF If state machine has not been initialized.
 **/
enum ap_pwrseq_state ap_pwrseq_sm_get_cur_state(void *const data);

/**
 * @brief Get state machine is entering.
 *
 * This function is meant to be executed only within AP power sequence driver
 * thread context. `tid` was given in `ap_pwrseq_sm_init`.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval Enum value Upon success.
 * @retval AP_POWER_STATE_UNDEF If state machine is not doing state transition.
 **/
enum ap_pwrseq_state ap_pwrseq_sm_get_entry_state(void *const data);

/**
 * @brief Get state machine is exiting.
 *
 * This function is meant to be executed only within AP power sequence driver
 * thread context. `tid` was given in `ap_pwrseq_sm_init`.
 *
 * @param data Pointer to AP power sequence state machine instance data.
 *
 * @retval Enum value Upon success.
 * @retval AP_POWER_STATE_UNDEF If state machine is not doing state transition.
 **/
enum ap_pwrseq_state ap_pwrseq_sm_get_exit_state(void *const data);
#endif /* _AP_PWRSEQ_SM_H_ */
