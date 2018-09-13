/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB Type-C VPD and CTVPD module.
 */
#include "common.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_sm.h"
#include "usb_tc_sm.h"
#include "util.h"
#include "usb_pd_test_util.h"
#include "vpd_api.h"

/*
 * Test State Hierarchy
 *   SM_TEST_A transitions to SM_TEST_B
 *   SM_TEST_B transitions to SM_TEST_C
 *   SM_TEST_C transitions to SM_TEST_A
 *
 * ---------------------------     ---------------------------
 * | SM_TEST_SUPER_A1        |     | SM_TEST_SUPER_B1        |
 * | ----------------------- |     | ----------------------- |
 * | | SM_TEST_SUPER_A2    | |     | | SM_TEST_SUPER_B2    | |
 * | | ------------------- | |     | | ------------------- | |
 * | | |SM_TEST_SUPER_A3 | | |     | | |SM_TEST_SUPER_B3 | | |
 * | | |                 | | |     | | |                 | | |
 * | | |  -------------  | | |     | | |  -------------  | | |
 * | | |  | SM_TEST_A |------------------>| SM_TEST_B |  | | |
 * | | |  -------------  | | |     | | |  -------------  | | |
 * | | |--------^--------| | |     | | |--------/--------| | |
 * | |-----------\---------| |     | |---------/-----------| |
 * |--------------\----------|     |----------/--------------|
 *                 \                         /
 *                  \                       /
 *                   \                     /
 *                    \                   /
 *                     \                 /
 *                      \              \/
 *                        -------------
 *                        | SM_TEST_C |
 *                        -------------
 *
 * test_hierarchy_0: Tests a flat state machine without super states
 * test_hierarchy_1: Tests a hierarchical state machine with 1 super state
 * test_hierarchy_2: Tests a hierarchical state machine with 2 super state
 * test_hierarchy_3: Tests a hierarchical state machine with 3 super state
 *
 */


#define PORT0   0
#define TSM_OBJ(port)   (SM_OBJ(sm[port]))

struct sm_ {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	int sv_tmp;
	int sv_a_idx;
	int sv_b_idx;
	int sv_a[4];
	int sv_b[4];
	int sv_c;
} sm[1];

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static unsigned int sm_test_super_A1(int port, enum signal sig);
static unsigned int sm_test_super_A1_entry(int port);
static unsigned int sm_test_super_A1_run(int port);
static unsigned int sm_test_super_A1_exit(int port);

static unsigned int sm_test_super_B1(int port, enum signal sig);
static unsigned int sm_test_super_B1_entry(int port);
static unsigned int sm_test_super_B1_run(int port);
static unsigned int sm_test_super_B1_exit(int port);
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
static unsigned int sm_test_super_A2(int port, enum signal sig);
static unsigned int sm_test_super_A2_entry(int port);
static unsigned int sm_test_super_A2_run(int port);
static unsigned int sm_test_super_A2_exit(int port);

static unsigned int sm_test_super_B2(int port, enum signal sig);
static unsigned int sm_test_super_B2_entry(int port);
static unsigned int sm_test_super_B2_run(int port);
static unsigned int sm_test_super_B2_exit(int port);
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
	defined(TEST_USB_SM_FRAMEWORK_H1)
static unsigned int sm_test_super_A3(int port, enum signal sig);
static unsigned int sm_test_super_A3_entry(int port);
static unsigned int sm_test_super_A3_run(int port);
static unsigned int sm_test_super_A3_exit(int port);

static unsigned int sm_test_super_B3(int port, enum signal sig);
static unsigned int sm_test_super_B3_entry(int port);
static unsigned int sm_test_super_B3_run(int port);
static unsigned int sm_test_super_B3_exit(int port);
#endif

static unsigned int sm_test_A(int port, enum signal sig);
static unsigned int sm_test_A_entry(int port);
static unsigned int sm_test_A_run(int port);
static unsigned int sm_test_A_exit(int port);

static unsigned int sm_test_B(int port, enum signal sig);
static unsigned int sm_test_B_entry(int port);
static unsigned int sm_test_B_run(int port);
static unsigned int sm_test_B_exit(int port);

static unsigned int sm_test_C(int port, enum signal sig);
static unsigned int sm_test_C_entry(int port);
static unsigned int sm_test_C_run(int port);
static unsigned int sm_test_C_exit(int port);

static unsigned int get_super_state(int port);

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static const state_sig sm_test_super_A1_sig[] = {
	sm_test_super_A1_entry,
	sm_test_super_A1_run,
	sm_test_super_A1_exit,
	get_super_state
};

static const state_sig sm_test_super_B1_sig[] = {
	sm_test_super_B1_entry,
	sm_test_super_B1_run,
	sm_test_super_B1_exit,
	get_super_state
};
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
static const state_sig sm_test_super_A2_sig[] = {
	sm_test_super_A2_entry,
	sm_test_super_A2_run,
	sm_test_super_A2_exit,
	get_super_state
};

static const state_sig sm_test_super_B2_sig[] = {
	sm_test_super_B2_entry,
	sm_test_super_B2_run,
	sm_test_super_B2_exit,
	get_super_state
};
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
		defined(TEST_USB_SM_FRAMEWORK_H1)
static const state_sig sm_test_super_A3_sig[] = {
	sm_test_super_A3_entry,
	sm_test_super_A3_run,
	sm_test_super_A3_exit,
	get_super_state
};

static const state_sig sm_test_super_B3_sig[] = {
	sm_test_super_B3_entry,
	sm_test_super_B3_run,
	sm_test_super_B3_exit,
	get_super_state
};
#endif

static const state_sig sm_test_A_sig[] = {
	sm_test_A_entry,
	sm_test_A_run,
	sm_test_A_exit,
	get_super_state
};

static const state_sig sm_test_B_sig[] = {
	sm_test_B_entry,
	sm_test_B_run,
	sm_test_B_exit,
	get_super_state
};

static const state_sig sm_test_C_sig[] = {
	sm_test_C_entry,
	sm_test_C_run,
	sm_test_C_exit,
	get_super_state
};

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static unsigned int sm_test_super_A1(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_A1_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int sm_test_super_A1_entry(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 1;
	return 0;
}

static unsigned int sm_test_super_A1_run(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 8;
	return 0;
}

static unsigned int sm_test_super_A1_exit(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 1;
	return 0;
}

static unsigned int sm_test_super_B1(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_B1_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int sm_test_super_B1_entry(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 10;
	return 0;
}

static unsigned int sm_test_super_B1_run(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 500;
	return 0;
}

static unsigned int sm_test_super_B1_exit(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 10;
	return 0;
}

#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
static unsigned int sm_test_super_A2(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_A2_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3)
	return SUPER(ret, sig, sm_test_super_A1);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_super_A2_entry(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 2;
	return 0;
}

static unsigned int sm_test_super_A2_run(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 6;
	return RUN_SUPER;
}

static unsigned int sm_test_super_A2_exit(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 2;
	return 0;
}

static unsigned int sm_test_super_B2(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_B2_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3)
	return SUPER(ret, sig, sm_test_super_B1);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_super_B2_entry(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 20;
	return 0;
}

static unsigned int sm_test_super_B2_run(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 400;
	return RUN_SUPER;
}

static unsigned int sm_test_super_B2_exit(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 20;
	return 0;
}

#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
		defined(TEST_USB_SM_FRAMEWORK_H1)
static unsigned int sm_test_super_A3(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_A3_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return SUPER(ret, sig, sm_test_super_A2);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_super_A3_entry(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 3;
	return 0;
}

static unsigned int sm_test_super_A3_run(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 4;
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return RUN_SUPER;
#else
	return 0;
#endif
}

static unsigned int sm_test_super_A3_exit(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 3;
	return 0;
}

static unsigned int sm_test_super_B3(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_B3_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return SUPER(ret, sig, sm_test_super_B2);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_super_B3_entry(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 30;
	return 0;
}

static unsigned int sm_test_super_B3_run(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 300;
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return RUN_SUPER;
#else
	return 0;
#endif
}

static unsigned int sm_test_super_B3_exit(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 30;
	return 0;
}
#endif


static unsigned int sm_test_A(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_A_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
		defined(TEST_USB_SM_FRAMEWORK_H1)
	return SUPER(ret, sig, sm_test_super_A3);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_A_entry(int port)
{
	sm[port].sv_tmp = 0;
	sm[port].sv_a[sm[port].sv_a_idx++] = 4;
	return 0;
}

static unsigned int sm_test_A_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].sv_a[sm[port].sv_a_idx++] = 2;
		sm[port].sv_tmp = 1;
	} else {
		sm[port].sv_b_idx = 0;
		set_state(port, TSM_OBJ(port), sm_test_B);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int sm_test_A_exit(int port)
{
	sm[port].sv_a[sm[port].sv_a_idx++] = 4;
	return 0;
}

static unsigned int sm_test_B(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_B_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
		defined(TEST_USB_SM_FRAMEWORK_H1)
	return SUPER(ret, sig, sm_test_super_B3);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_B_entry(int port)
{
	sm[port].sv_tmp = 0;
	sm[port].sv_b[sm[port].sv_b_idx++] = 40;

	return 0;
}

static unsigned int sm_test_B_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].sv_b[sm[port].sv_b_idx++] = 200;
		sm[port].sv_tmp = 1;
	} else {
		set_state(port, TSM_OBJ(port), sm_test_C);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int sm_test_B_exit(int port)
{
	sm[port].sv_b[sm[port].sv_b_idx++] = 40;
	return 0;
}

static unsigned int get_super_state(int port)
{
	return RUN_SUPER;
}

static unsigned int sm_test_C(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_C_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int sm_test_C_entry(int port)
{
	sm[port].sv_tmp = 0;
	sm[port].sv_c = 1000;

	return 0;
}

static unsigned int sm_test_C_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].sv_c = 2000;
		sm[port].sv_tmp = 1;
	} else {
		sm[port].sv_a_idx = 0;
		set_state(port, TSM_OBJ(port), sm_test_A);
	}

	return 0;
}

static unsigned int sm_test_C_exit(int port)
{
	sm[port].sv_c = 3000;
	return 0;
}

static void init_sm(int port)
{
	int i;

	sm[port].sv_a_idx = 0;
	for (i = 0; i < 4; i++) {
		sm[port].sv_a[i] = 0;
		sm[port].sv_b[i] = 0;
	}

	init_state(port, TSM_OBJ(port), sm_test_A);
}

#if defined(TEST_USB_SM_FRAMEWORK_H0)
static int test_hierarchy_0(void)
{
	int port = PORT0;

	/* Test State A enter sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 4);

	/* Test State A run sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 2);

	/* Test State A exit sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 4);

	/* Test State B enter sequence */
	TEST_ASSERT(sm[port].sv_b[0] == 40);

	/* Test State B run sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 200);

	/* Test State B exit sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 40);

	/* Test State C enter sequence */
	TEST_ASSERT(sm[port].sv_c == 1000);
	/* Test run sequence */
	task_wake(TASK_ID_TEST);

	/* Test State C run sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 2000);

	/* Test State C exit sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 3000);

	/* Test State A enter sequence */
	TEST_ASSERT(sm[port].sv_a[0] == 4);

	return EC_SUCCESS;
}
#endif


#if defined(TEST_USB_SM_FRAMEWORK_H1)
static int test_hierarchy_1(void)
{
	int port = PORT0;

	/* Test State A enter sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 3);
	TEST_ASSERT(sm[port].sv_a[1] == 4);

	/* Test State A run sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 2);
	TEST_ASSERT(sm[port].sv_a[1] == 4);

	/* Test State A exit sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 4);
	TEST_ASSERT(sm[port].sv_a[1] == 3);

	/* Test State B enter sequence */
	TEST_ASSERT(sm[port].sv_b[0] == 30);
	TEST_ASSERT(sm[port].sv_b[1] == 40);

	/* Test State B run sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 200);
	TEST_ASSERT(sm[port].sv_b[1] == 300);

	/* Test State B exit sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 40);
	TEST_ASSERT(sm[port].sv_b[1] == 30);

	/* Test State C enter sequence */
	TEST_ASSERT(sm[port].sv_c == 1000);
	/* Test run sequence */
	task_wake(TASK_ID_TEST);

	/* Test State C run sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 2000);

	/* Test State C exit sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 3000);

	/* Test State A enter sequence */
	TEST_ASSERT(sm[port].sv_a[0] == 3);
	TEST_ASSERT(sm[port].sv_a[1] == 4);

	return EC_SUCCESS;
}
#endif


#if defined(TEST_USB_SM_FRAMEWORK_H2)
static int test_hierarchy_2(void)
{
	int port = PORT0;

	/* Test State A enter sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 2);
	TEST_ASSERT(sm[port].sv_a[1] == 3);
	TEST_ASSERT(sm[port].sv_a[2] == 4);

	/* Test State A run sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 2);
	TEST_ASSERT(sm[port].sv_a[1] == 4);
	TEST_ASSERT(sm[port].sv_a[2] == 6);

	/* Test State A exit sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 4);
	TEST_ASSERT(sm[port].sv_a[1] == 3);
	TEST_ASSERT(sm[port].sv_a[2] == 2);

	/* Test State B enter sequence */
	TEST_ASSERT(sm[port].sv_b[0] == 20);
	TEST_ASSERT(sm[port].sv_b[1] == 30);
	TEST_ASSERT(sm[port].sv_b[2] == 40);

	/* Test State B run sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 200);
	TEST_ASSERT(sm[port].sv_b[1] == 300);
	TEST_ASSERT(sm[port].sv_b[2] == 400);

	/* Test State B exit sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 40);
	TEST_ASSERT(sm[port].sv_b[1] == 30);
	TEST_ASSERT(sm[port].sv_b[2] == 20);

	/* Test State C enter sequence */
	TEST_ASSERT(sm[port].sv_c == 1000);

	/* Test State C run sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 2000);

	/* Test State C exit sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 3000);

	/* Test State A enter sequence */
	TEST_ASSERT(sm[port].sv_a[0] == 2);
	TEST_ASSERT(sm[port].sv_a[1] == 3);
	TEST_ASSERT(sm[port].sv_a[2] == 4);

return EC_SUCCESS;
}
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static int test_hierarchy_3(void)
{
	int port = PORT0;

	/* Test State A enter sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 1);
	TEST_ASSERT(sm[port].sv_a[1] == 2);
	TEST_ASSERT(sm[port].sv_a[2] == 3);
	TEST_ASSERT(sm[port].sv_a[3] == 4);

	/* Test State A run sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 2);
	TEST_ASSERT(sm[port].sv_a[1] == 4);
	TEST_ASSERT(sm[port].sv_a[2] == 6);
	TEST_ASSERT(sm[port].sv_a[3] == 8);

	/* Test State A exit sequence */
	sm[port].sv_a_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_a[0] == 4);
	TEST_ASSERT(sm[port].sv_a[1] == 3);
	TEST_ASSERT(sm[port].sv_a[2] == 2);
	TEST_ASSERT(sm[port].sv_a[3] == 1);

	/* Test State B enter sequence */
	TEST_ASSERT(sm[port].sv_b[0] == 10);
	TEST_ASSERT(sm[port].sv_b[1] == 20);
	TEST_ASSERT(sm[port].sv_b[2] == 30);
	TEST_ASSERT(sm[port].sv_b[3] == 40);

	/* Test State B enter sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 200);
	TEST_ASSERT(sm[port].sv_b[1] == 300);
	TEST_ASSERT(sm[port].sv_b[2] == 400);
	TEST_ASSERT(sm[port].sv_b[3] == 500);

	/* Test State B exit sequence */
	sm[port].sv_b_idx = 0;
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_b[0] == 40);
	TEST_ASSERT(sm[port].sv_b[1] == 30);
	TEST_ASSERT(sm[port].sv_b[2] == 20);
	TEST_ASSERT(sm[port].sv_b[3] == 10);

	/* Test State C enter sequence */
	TEST_ASSERT(sm[port].sv_c == 1000);

	/* Test State C run sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 2000);

	/* Test State C exit sequence */
	task_wake(TASK_ID_TEST);
	task_wait_event(5 * MSEC);
	TEST_ASSERT(sm[port].sv_c == 3000);

	/* Test State A enter sequence */
	TEST_ASSERT(sm[port].sv_a[0] == 1);
	TEST_ASSERT(sm[port].sv_a[1] == 2);
	TEST_ASSERT(sm[port].sv_a[2] == 3);
	TEST_ASSERT(sm[port].sv_a[3] == 4);

	return EC_SUCCESS;
}
#endif

int test_task(void *u)
{
	int port = PORT0;

	while (1) {
		/* wait for next event/packet or timeout expiration */
		task_wait_event(-1);
		/* run state machine */
		exe_state(port, TSM_OBJ(port), RUN_SIG);
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();
	init_sm(PORT0);
#if defined(TEST_USB_SM_FRAMEWORK_H3)
	RUN_TEST(test_hierarchy_3);
#elif defined(TEST_USB_SM_FRAMEWORK_H2)
	RUN_TEST(test_hierarchy_2);
#elif defined(TEST_USB_SM_FRAMEWORK_H1)
	RUN_TEST(test_hierarchy_1);
#else
	RUN_TEST(test_hierarchy_0);
#endif
	test_print_result();
}
