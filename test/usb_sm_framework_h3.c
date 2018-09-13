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
 *   SM_TEST_C transitions to SM_TEST_AA
 *   SM_TEST_AA transitions to SM_TEST_A
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
 * | | |        ^        | | |     | | |---------/-------| | |
 * | | |        |        | | |     | |----------/----------| |
 * | | |  -------------- | | |     |-----------/-------------|
 * | | |  | SM_TEST_AA | | | |                /
 * | | |  -------------- | | |               /
 * | | |--------^--------| | |              /
 * | |-----------\---------| |             /
 * |--------------\----------|            /
 *                 \                     /
 *                  \                   /
 *                   \                 /
 *                    \               /
 *                     \             /
 *                      \           \/
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

enum state_id {
	ENTER_A1 = 1,
	RUN_A1,
	EXIT_A1,
	ENTER_A2,
	RUN_A2,
	EXIT_A2,
	ENTER_A3,
	RUN_A3,
	EXIT_A3,
	ENTER_A,
	RUN_A,
	EXIT_A,
	ENTER_AA,
	RUN_AA,
	EXIT_AA,
	ENTER_B1,
	RUN_B1,
	EXIT_B1,
	ENTER_B2,
	RUN_B2,
	EXIT_B2,
	ENTER_B3,
	RUN_B3,
	EXIT_B3,
	ENTER_B,
	RUN_B,
	EXIT_B,
	ENTER_C,
	RUN_C,
	EXIT_C,
};

#define PORT0   0
#define TSM_OBJ(port)   (SM_OBJ(sm[port]))

struct sm_ {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	int sv_tmp;
	int idx;
	int seq[37];
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

static unsigned int sm_test_AA(int port, enum signal sig);
static unsigned int sm_test_AA_entry(int port);
static unsigned int sm_test_AA_run(int port);
static unsigned int sm_test_AA_exit(int port);

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

static const state_sig sm_test_AA_sig[] = {
	sm_test_AA_entry,
	sm_test_AA_run,
	sm_test_AA_exit,
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

static void clear_seq(int port)
{
	int i;

	sm[port].idx = 0;

	for (i = 0; i < 8; i++)
		sm[port].seq[i] = 0;
}

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static unsigned int sm_test_super_A1(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_super_A1_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int sm_test_super_A1_entry(int port)
{
	sm[port].seq[sm[port].idx++] = ENTER_A1;
	return 0;
}

static unsigned int sm_test_super_A1_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_A1;
	return 0;
}

static unsigned int sm_test_super_A1_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_A1;
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
	sm[port].seq[sm[port].idx++] = ENTER_B1;
	return 0;
}

static unsigned int sm_test_super_B1_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_B1;
	return 0;
}

static unsigned int sm_test_super_B1_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_B1;
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
	sm[port].seq[sm[port].idx++] = ENTER_A2;
	return 0;
}

static unsigned int sm_test_super_A2_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_A2;
	return RUN_SUPER;
}

static unsigned int sm_test_super_A2_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_A2;
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
	sm[port].seq[sm[port].idx++] = ENTER_B2;
	return 0;
}

static unsigned int sm_test_super_B2_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_B2;
	return RUN_SUPER;
}

static unsigned int sm_test_super_B2_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_B2;
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
	sm[port].seq[sm[port].idx++] = ENTER_A3;
	return 0;
}

static unsigned int sm_test_super_A3_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_A3;
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return RUN_SUPER;
#else
	return 0;
#endif
}

static unsigned int sm_test_super_A3_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_A3;
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
	sm[port].seq[sm[port].idx++] = ENTER_B3;
	return 0;
}

static unsigned int sm_test_super_B3_run(int port)
{
	sm[port].seq[sm[port].idx++] = RUN_B3;
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2)
	return RUN_SUPER;
#else
	return 0;
#endif
}

static unsigned int sm_test_super_B3_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_B3;
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
	sm[port].seq[sm[port].idx++] = ENTER_A;
	return 0;
}

static unsigned int sm_test_A_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].sv_tmp = 1;
		sm[port].seq[sm[port].idx++] = RUN_A;
	} else {
		set_state(port, TSM_OBJ(port), sm_test_B);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int sm_test_A_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_A;
	return 0;
}

static unsigned int sm_test_AA(int port, enum signal sig)
{
	int ret;

	ret = (*sm_test_AA_sig[sig])(port);
#if defined(TEST_USB_SM_FRAMEWORK_H3) || defined(TEST_USB_SM_FRAMEWORK_H2) || \
		defined(TEST_USB_SM_FRAMEWORK_H1)
	return SUPER(ret, sig, sm_test_super_A3);
#else
	return SUPER(ret, sig, 0);
#endif
}

static unsigned int sm_test_AA_entry(int port)
{
	sm[port].sv_tmp = 0;
	sm[port].seq[sm[port].idx++] = ENTER_AA;
	return 0;
}

static unsigned int sm_test_AA_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].sv_tmp = 1;
		sm[port].seq[sm[port].idx++] = RUN_AA;
	} else {
		set_state(port, TSM_OBJ(port), sm_test_A);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int sm_test_AA_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_AA;
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
	sm[port].seq[sm[port].idx++] = ENTER_B;
	return 0;
}

static unsigned int sm_test_B_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].seq[sm[port].idx++] = RUN_B;
		sm[port].sv_tmp = 1;
	} else {
		set_state(port, TSM_OBJ(port), sm_test_C);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int sm_test_B_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_B;
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
	sm[port].seq[sm[port].idx++] = ENTER_C;
	return 0;
}

static unsigned int sm_test_C_run(int port)
{
	if (sm[port].sv_tmp == 0) {
		sm[port].seq[sm[port].idx++] = RUN_C;
		sm[port].sv_tmp = 1;
	} else {
		set_state(port, TSM_OBJ(port), sm_test_AA);
	}

	return 0;
}

static unsigned int sm_test_C_exit(int port)
{
	sm[port].seq[sm[port].idx++] = EXIT_C;
	return 0;
}

#if defined(TEST_USB_SM_FRAMEWORK_H0)
static int test_hierarchy_0(void)
{
	int port = PORT0;
	int i;

	clear_seq(port);
	init_state(port, TSM_OBJ(port), sm_test_A);

	for (i = 0; i < 9; i++) {
		task_wake(TASK_ID_TEST);
		task_wait_event(5 * MSEC);
	}

	/* i == 0 */
	TEST_ASSERT(sm[port].seq[0] == ENTER_A);

	/* i == 1 */
	TEST_ASSERT(sm[port].seq[1] == RUN_A);

	/* i == 2 */
	TEST_ASSERT(sm[port].seq[2] == EXIT_A);
	TEST_ASSERT(sm[port].seq[3] == ENTER_B);

	/* i == 3 */
	TEST_ASSERT(sm[port].seq[4] == RUN_B);

	/* i == 4 */
	TEST_ASSERT(sm[port].seq[5] == EXIT_B);
	TEST_ASSERT(sm[port].seq[6] == ENTER_C);

	/* i == 5 */
	TEST_ASSERT(sm[port].seq[7] == RUN_C);

	/* i == 6 */
	TEST_ASSERT(sm[port].seq[8] == EXIT_C);
	TEST_ASSERT(sm[port].seq[9] == ENTER_AA);

	/* i == 7 */
	TEST_ASSERT(sm[port].seq[10] == RUN_AA);

	/* i == 8 */
	TEST_ASSERT(sm[port].seq[11] == EXIT_AA);
	TEST_ASSERT(sm[port].seq[12] == ENTER_A);

	for (i = 13; i < 37; i++)
		TEST_ASSERT(sm[port].seq[i] == 0);

	return EC_SUCCESS;
}
#endif


#if defined(TEST_USB_SM_FRAMEWORK_H1)
static int test_hierarchy_1(void)
{
	int port = PORT0;
	int i;

	clear_seq(port);
	init_state(port, TSM_OBJ(port), sm_test_A);

	for (i = 0; i < 9; i++) {
		task_wake(TASK_ID_TEST);
		task_wait_event(5 * MSEC);
	}

	/* i == 0 */
	TEST_ASSERT(sm[port].seq[0] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[1] == ENTER_A);

	/* i == 1 */
	TEST_ASSERT(sm[port].seq[2] == RUN_A);
	TEST_ASSERT(sm[port].seq[3] == RUN_A3);

	/* i == 2 */
	TEST_ASSERT(sm[port].seq[4] == EXIT_A);
	TEST_ASSERT(sm[port].seq[5] == EXIT_A3);
	TEST_ASSERT(sm[port].seq[6] == ENTER_B3);
	TEST_ASSERT(sm[port].seq[7] == ENTER_B);

	/* i == 3 */
	TEST_ASSERT(sm[port].seq[8] == RUN_B);
	TEST_ASSERT(sm[port].seq[9] == RUN_B3);

	/* i == 4 */
	TEST_ASSERT(sm[port].seq[10] == EXIT_B);
	TEST_ASSERT(sm[port].seq[11] == EXIT_B3);
	TEST_ASSERT(sm[port].seq[12] == ENTER_C);

	/* i == 5 */
	TEST_ASSERT(sm[port].seq[13] == RUN_C);

	/* i == 6 */
	TEST_ASSERT(sm[port].seq[14] == EXIT_C);
	TEST_ASSERT(sm[port].seq[15] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[16] == ENTER_AA);

	/* i == 7 */
	TEST_ASSERT(sm[port].seq[17] == RUN_AA);
	TEST_ASSERT(sm[port].seq[18] == RUN_A3);

	/* i == 8 */
	TEST_ASSERT(sm[port].seq[19] == EXIT_AA);
	TEST_ASSERT(sm[port].seq[20] == ENTER_A);

	for (i = 21; i < 37; i++)
		TEST_ASSERT(sm[port].seq[i] == 0);

	return EC_SUCCESS;
}
#endif


#if defined(TEST_USB_SM_FRAMEWORK_H2)
static int test_hierarchy_2(void)
{
	int port = PORT0;
	int i;

	clear_seq(port);
	init_state(port, TSM_OBJ(port), sm_test_A);

	for (i = 0; i < 9; i++) {
		task_wake(TASK_ID_TEST);
		task_wait_event(5 * MSEC);
	}

	/* i == 0 */
	TEST_ASSERT(sm[port].seq[0] == ENTER_A2);
	TEST_ASSERT(sm[port].seq[1] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[2] == ENTER_A);

	/* i == 1 */
	TEST_ASSERT(sm[port].seq[3] == RUN_A);
	TEST_ASSERT(sm[port].seq[4] == RUN_A3);
	TEST_ASSERT(sm[port].seq[5] == RUN_A2);

	/* i == 2 */
	TEST_ASSERT(sm[port].seq[6] == EXIT_A);
	TEST_ASSERT(sm[port].seq[7] == EXIT_A3);
	TEST_ASSERT(sm[port].seq[8] == EXIT_A2);
	TEST_ASSERT(sm[port].seq[9] == ENTER_B2);
	TEST_ASSERT(sm[port].seq[10] == ENTER_B3);
	TEST_ASSERT(sm[port].seq[11] == ENTER_B);

	/* i == 3 */
	TEST_ASSERT(sm[port].seq[12] == RUN_B);
	TEST_ASSERT(sm[port].seq[13] == RUN_B3);
	TEST_ASSERT(sm[port].seq[14] == RUN_B2);

	/* i == 4 */
	TEST_ASSERT(sm[port].seq[15] == EXIT_B);
	TEST_ASSERT(sm[port].seq[16] == EXIT_B3);
	TEST_ASSERT(sm[port].seq[17] == EXIT_B2);
	TEST_ASSERT(sm[port].seq[18] == ENTER_C);

	/* i == 5 */
	TEST_ASSERT(sm[port].seq[19] == RUN_C);

	/* i == 6 */
	TEST_ASSERT(sm[port].seq[20] == EXIT_C);
	TEST_ASSERT(sm[port].seq[21] == ENTER_A2);
	TEST_ASSERT(sm[port].seq[22] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[23] == ENTER_AA);

	/* i == 7 */
	TEST_ASSERT(sm[port].seq[24] == RUN_AA);
	TEST_ASSERT(sm[port].seq[25] == RUN_A3);
	TEST_ASSERT(sm[port].seq[26] == RUN_A2);

	/* i == 8 */
	TEST_ASSERT(sm[port].seq[27] == EXIT_AA);
	TEST_ASSERT(sm[port].seq[28] == ENTER_A);

	for (i = 29; i < 37; i++)
		TEST_ASSERT(sm[port].seq[i] == 0);

	return EC_SUCCESS;
}
#endif

#if defined(TEST_USB_SM_FRAMEWORK_H3)
static int test_hierarchy_3(void)
{
	int port = PORT0;
	int i;

	clear_seq(port);
	init_state(port, TSM_OBJ(port), sm_test_A);

	for (i = 0; i < 9; i++) {
		task_wake(TASK_ID_TEST);
		task_wait_event(5 * MSEC);
	}

	/* i == 0 */
	TEST_ASSERT(sm[port].seq[0] == ENTER_A1);
	TEST_ASSERT(sm[port].seq[1] == ENTER_A2);
	TEST_ASSERT(sm[port].seq[2] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[3] == ENTER_A);

	/* i == 1 */
	TEST_ASSERT(sm[port].seq[4] == RUN_A);
	TEST_ASSERT(sm[port].seq[5] == RUN_A3);
	TEST_ASSERT(sm[port].seq[6] == RUN_A2);
	TEST_ASSERT(sm[port].seq[7] == RUN_A1);

	/* i == 2 */
	TEST_ASSERT(sm[port].seq[8] == EXIT_A);
	TEST_ASSERT(sm[port].seq[9] == EXIT_A3);
	TEST_ASSERT(sm[port].seq[10] == EXIT_A2);
	TEST_ASSERT(sm[port].seq[11] == EXIT_A1);
	TEST_ASSERT(sm[port].seq[12] == ENTER_B1);
	TEST_ASSERT(sm[port].seq[13] == ENTER_B2);
	TEST_ASSERT(sm[port].seq[14] == ENTER_B3);
	TEST_ASSERT(sm[port].seq[15] == ENTER_B);

	/* i == 3 */
	TEST_ASSERT(sm[port].seq[16] == RUN_B);
	TEST_ASSERT(sm[port].seq[17] == RUN_B3);
	TEST_ASSERT(sm[port].seq[18] == RUN_B2);
	TEST_ASSERT(sm[port].seq[19] == RUN_B1);

	/* i == 4 */
	TEST_ASSERT(sm[port].seq[20] == EXIT_B);
	TEST_ASSERT(sm[port].seq[21] == EXIT_B3);
	TEST_ASSERT(sm[port].seq[22] == EXIT_B2);
	TEST_ASSERT(sm[port].seq[23] == EXIT_B1);
	TEST_ASSERT(sm[port].seq[24] == ENTER_C);

	/* i == 5 */
	TEST_ASSERT(sm[port].seq[25] == RUN_C);

	/* i == 6 */
	TEST_ASSERT(sm[port].seq[26] == EXIT_C);
	TEST_ASSERT(sm[port].seq[27] == ENTER_A1);
	TEST_ASSERT(sm[port].seq[28] == ENTER_A2);
	TEST_ASSERT(sm[port].seq[29] == ENTER_A3);
	TEST_ASSERT(sm[port].seq[30] == ENTER_AA);

	/* i == 7 */
	TEST_ASSERT(sm[port].seq[31] == RUN_AA);
	TEST_ASSERT(sm[port].seq[32] == RUN_A3);
	TEST_ASSERT(sm[port].seq[33] == RUN_A2);
	TEST_ASSERT(sm[port].seq[34] == RUN_A1);

	/* i == 8 */
	TEST_ASSERT(sm[port].seq[35] == EXIT_AA);
	TEST_ASSERT(sm[port].seq[36] == ENTER_A);

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
