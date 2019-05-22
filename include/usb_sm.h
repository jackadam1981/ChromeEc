/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB State Machine Framework */

#ifndef __CROS_EC_USB_SM_H
#define __CROS_EC_USB_SM_H

#define ADD_EXIT(prefix, name, exit)  ADD_EXIT_##exit(prefix, name)
#define ADD_EXIT_true(prefix, name)   static unsigned int \
				prefix##_state_##name##_exit(int port)
#define ADD_EXIT_false(prefix, name)

#define IMP_EXIT(prefix, name, exit)  IMP_EXIT_##exit(prefix, name)
#define IMP_EXIT_true(prefix, name)   prefix##_state_##name##_exit
#define IMP_EXIT_false(prefix, name)  do_nothing_exit

/*
 * Helper macro for the declaration of states.
 *
 * @param prefix - prefix of state function name
 * @param name   - name of state
 * @param exit   - if true, generates an exit state function name
 *                 if false, generates do_nothing_exit function name
 *
 * EXAMPLE:
 *
 * DECLARE_STATE(tc, test, true); generates the following:
 *
 * static unsigned int tc_state_test(int port, enum signal sig);
 * static unsigned int tc_state_test_entry(int port);
 * static unsigned int tc_state_test_run(int port);
 * static unsigned int tc_state_test_exit(int port);
 * static const state_sig tc_state_test_sig[] = {
 *	tc_state_test_entry,
 *	tc_state_test_run,
 *	tc_state_test_exit,
 *	get_super_state };
 *
 * DECLARE_STATE(tc, test, false); generates the following:
 *
 * static unsigned int tc_state_test(int port, enum signal sig);
 * static unsigned int tc_state_test_entry(int port);
 * static unsigned int tc_state_test_run(int port);
 * static const state_sig tc_state_test_sig[] = {
 *      tc_state_test_entry,
 *      tc_state_test_run,
 *      do_nothing_exit,
 *      get_super_state };
 */
#define DECLARE_STATE(prefix, name, exit) \
static unsigned int prefix##_state_##name(int port, enum signal sig); \
static unsigned int prefix##_state_##name##_entry(int port); \
static unsigned int prefix##_state_##name##_run(int port); \
ADD_EXIT(prefix, name, exit); \
static const state_sig prefix##_state_##name##_sig[] = { \
prefix##_state_##name##_entry, \
prefix##_state_##name##_run, \
IMP_EXIT(prefix, name, exit), \
get_super_state \
}

#define SM_OBJ(smo)    ((struct sm_obj *)&smo)
#define SUPER(r, sig, s)  ((((r) == 0) || ((sig) == ENTRY_SIG) || \
			((sig) == EXIT_SIG)) ? 0 : ((uintptr_t)(s)))
#define RUN_SUPER	1

/* Local state machine states */
enum sm_local_state {
	SM_INIT,
	SM_RUN,
	SM_PAUSED
};

/* State Machine signals */
enum signal {
	ENTRY_SIG = 0,
	RUN_SIG,
	EXIT_SIG,
	SUPER_SIG,
};

typedef unsigned int (*state_sig)(int port);
typedef unsigned int (*sm_state)(int port, enum signal sig);

struct sm_obj {
	sm_state task_state;
	sm_state last_state;
};

/**
 * Initialize a State Machine
 *
 * @param port   USB-C port number
 * @param obj    State machine object
 * @param target Initial state of state machine
 */
void init_state(int port, struct sm_obj *obj, sm_state target);

/**
 * Changes a state machines state
 *
 * @param port   USB-C port number
 * @param obj    State machine object
 * @param target State to transition to
 * @return 0
 */
int set_state(int port, struct sm_obj *obj, sm_state target);

/**
 * Executes a state machine
 *
 * @param port USB-C port number
 * @param obj  State machine object
 * @param sig  State machine signal
 */
void exe_state(int port, struct sm_obj *obj, enum signal sig);

#endif /* __CROS_EC_USB_SM_H */
