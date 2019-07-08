/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB State Machine Framework */

#ifndef __CROS_EC_USB_SM_H
#define __CROS_EC_USB_SM_H

#define DECLARE_SM_FUNC_(prefix, name, exit) \
				DECLARE_SM_FUNC_##exit(prefix, name)
#define DECLARE_SM_FUNC_WITH_EXIT(prefix, name)   static unsigned int \
				prefix##_##name##_exit(int port)
#define DECLARE_SM_FUNC_NOOP_EXIT(prefix, name)

#define DECLARE_SM_SIG_(prefix, name, exit)  DECLARE_SM_##exit(prefix, name)
#define DECLARE_SM_WITH_EXIT(prefix, name)   prefix##_##name##_exit
#define DECLARE_SM_NOOP_EXIT(prefix, name)  do_nothing_exit


/*
 * Helper macro for the declaration of states.
 *
 * @param prefix - prefix of state function name
 * @param name   - name of state
 * @param exit   - if WITH_EXIT, generates an exit state function name
 *                 if NOOP_EXIT, generates do_nothing_exit function name
 *
 * EXAMPLE:
 *
 * DECLARE_STATE(tc, test, WITH_EXIT); generates the following:
 *
 * static unsigned int tc_test(int port, enum signal sig);
 * static unsigned int tc_test_entry(int port);
 * static unsigned int tc_test_run(int port);
 * static unsigned int tc_test_exit(int port);
 * static const state_sig tc_test_sig[] = {
 *	tc_test_entry,
 *	tc_test_run,
 *	tc_test_exit,
 *	get_super_state };
 *
 * DECLARE_STATE(tc, test, NOOP_EXIT); generates the following:
 *
 * static unsigned int tc_test(int port, enum signal sig);
 * static unsigned int tc_test_entry(int port);
 * static unsigned int tc_test_run(int port);
 * static const state_sig tc_test_sig[] = {
 *      tc_test_entry,
 *      tc_test_run,
 *      do_nothing_exit,
 *      get_super_state };
 */
#define DECLARE_STATE(prefix, name, exit) \
static unsigned int prefix##_##name(int port, enum signal sig); \
static unsigned int prefix##_##name##_entry(int port); \
static unsigned int prefix##_##name##_run(int port); \
DECLARE_SM_FUNC_(prefix, name, exit); \
static const state_sig prefix##_##name##_sig[] = { \
prefix##_##name##_entry, \
prefix##_##name##_run, \
DECLARE_SM_SIG_(prefix, name, exit), \
get_super_state \
}


typedef void (*state_execution)(int port);


struct usb_state {
	const state_execution entry;
	const state_execution run;
	const state_execution exit;
	const usb_state *parent;
};

/* in C file */


struct sm_ctx {
	struct usb_state *current;
	struct usb_state *previous;
};


#define SM_CTX(smo)    ((struct sm_ctx *)&smo)

/* Local state machine states */
enum sm_local_state {
	SM_INIT,
	SM_RUN,
	SM_PAUSED
};

/**
 * Changes a state machines state
 *
 * @param port   USB-C port number
 * @param ctx    State machine context
 * @param target State to transition to
 * @return 0
 */
int set_state(int port, struct sm_ctx *ctx, sm_state target);

/**
 * Executes a state machine
 *
 * @param port USB-C port number
 * @param ctx  State machine context
 * @param sig  State machine signal
 */
void exe_state(int port, struct sm_ctx *ctx, enum signal sig);

#endif /* __CROS_EC_USB_SM_H */
