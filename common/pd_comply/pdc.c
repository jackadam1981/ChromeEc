/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define PDO_FIXED_FLAGS (PDO_FIXED_DUAL_ROLE | PDO_FIXED_DATA_SWAP |\
			 PDO_FIXED_COMM_CAP)

#define PDC_FLAGS_CONNECTED	BIT(0)
#define PDC_FLAGS_RA_RD		BIT(1)

enum pdc_uut_role {
	PD_UFP,		/* UUT is a UFP */
	PD_DFP,		/* UUT is a DFP */
};

enum pdc_return {
	PDC_NO_RESPONSE = -2,
	PDC_FAIL,
	PDC_DONE,
	PDC_CONTINUE
};

enum pdc_failure {
	PDC_FAILURE_NO_RESPONSE = 1,
	PDC_FAILURE_BRINGUP,
};

/* Case statements that match the spec. */
#define a 0
#define b 1
#define c 2
#define d 3
#define e 4
#define f 5
#define g 6
#define h 7
#define i 8
#define j 9
#define z 25

enum pdc_current {
	PDC_1A5 = 1500,
	PDC_3A0 = 3000,
};

typedef bool (*pdc_procedure)(void);

struct pdc_tests {
	pdc_procedure proc;
	char *name;
	char *desc;
};

static struct pdc {
	/* loop timeout */
	uint64_t evt_timeout;
	/* uut role */
	enum pdc_uut_role uut_role;
	/* current test step */
	uint32_t step;
	/* PDC_FLAGS_x flags used during test */
	uint32_t flags;
	/* stop the test at this step */
	uint32_t stop_at;
	/* message xmit state */
	uint32_t msg_state;
	/* a sub state within a step */
	uint32_t state;
	/* sourc current */
	enum pdc_current current;
	/* cc1 status */
	enum tcpc_cc_voltage_status cc1;
	/* cc2 status */
	enum tcpc_cc_voltage_status cc2;
	/* cc polarity */
	enum tcpc_cc_polarity polarity;
	/* testers PD rev */
	enum pd_rev_type rev;
	/* message response timer */
	uint64_t resp_timer;
	/* generic timer */
	uint64_t timer;
	/* second generic timer */
	uint64_t timer2;
	/* timer used to measure a delta */
	uint64_t delta_time;
	/* start time of delta measurement */
	uint64_t delta_start;
	/* compliance port */
	int port;
	/* PD message */
	uint32_t payload[7];
	/* PD message header */
	uint32_t header;
	/* controls if test procedure is running */
	bool run_proc;
	/* test procedure to run */
	pdc_procedure proc;
} pdc;

/* goto next step */
static inline bool pdc_next_step(void)
{
	if (pdc.stop_at == pdc.step) {
		/* Termination step reached. Stop the test. */
		return true;
	}

	/* Reset message sending state machine */
	pdc.msg_state = 0;
	/* Reset generic state machine */
	pdc.state = 0;
	/* Advance to next state */
	pdc.step++;
	/* Continue the test */
	return false;
}

static void reset_msg_state(void)
{
	pdc.msg_state = 0;
}

/* Report failure EC Console or back to Host via host command */
static void pdc_report_failure(enum pdc_failure failure, int arg)
{
	switch (failure) {
	case PDC_FAILURE_NO_RESPONSE:
		ccprintf("NO RESPONSE at step %d\n", arg);
		break;
	case PDC_FAILURE_BRINGUP:
		ccprintf("Bring up FAILED at step %d\n", arg);
		break;
	}
}

/*
 * Data Messages
 */

static void pdc_send_source_cap(void)
{
	pdc.payload[0] = PDO_FIXED(5000,
			pdc.current,
			PDO_FIXED_FLAGS);

	pdc.header = PD_HEADER(PD_DATA_SOURCE_CAP,
			pd[pdc.port].power_role,
			pd[pdc.port].data_role,
			pd[pdc.port].msg_id,
			1, /* 1 SRC PDO */
			pdc.rev,
			0); /* non-extended message */
}

static void pdc_send_vdm(int vdm_cmd, uint16_t sid, uint8_t vdm_vdo_num,
				enum tcpm_transmit_type tx_type)
{
	pdc.payload[0] = VDO(sid,
		1, /* Structured VDM */
		VDO_SVDM_VERS(pd_get_vdo_ver(pdc.port, tx_type)) | vdm_cmd);

	pdc.header = PD_HEADER(PD_DATA_VENDOR_DEF,
			pd[pdc.port].power_role,
			pd[pdc.port].data_role,
			pd[pdc.port].msg_id,
			1 + vdm_vdo_num,
			pdc.rev,
			0);
}

/*
 * send data message
 */
static int pdc_send_data_msg(enum pd_data_msg_type msg,
				int vdm_cmd,
				uint16_t sid,
				uint8_t vdm_vdo_num,
				enum tcpm_transmit_type tx_type,
				uint64_t response_wait_time)
{
	switch (pdc.msg_state) {
	case 0:
		switch (msg) {
		case PD_DATA_SOURCE_CAP:
			pdc_send_source_cap();
			break;
		case PD_DATA_REQUEST:
			return PDC_FAIL;
		case PD_DATA_BIST:
			return PDC_FAIL;
		case PD_DATA_SINK_CAP:
			return PDC_FAIL;
		case PD_DATA_BATTERY_STATUS:
			return PDC_FAIL;
		case PD_DATA_ALERT:
			return PDC_FAIL;
		case PD_DATA_GET_COUNTRY_INFO:
			return PDC_FAIL;
		case PD_DATA_ENTER_USB:
			return PDC_FAIL;
		case PD_DATA_VENDOR_DEF:
			pdc_send_vdm(vdm_cmd, sid, vdm_vdo_num, tx_type);
			break;
		}

		pd_transmit(pdc.port, tx_type, pdc.header, pdc.payload, 0);

		if (response_wait_time == 0)
			return PDC_DONE;

		pdc.msg_state++;
		pdc.resp_timer = get_time().val + response_wait_time;
		pdc.delta_start = get_time().val;
		/* fall through */
	case 1:
		if (tcpm_has_pending_message(pdc.port)) {
			pdc.delta_time = (get_time().val - pdc.delta_start);
			/* Dequeue and consume duplicate message ID. */
			if (tcpm_dequeue_message(pdc.port,
						 pdc.payload,
						 &pdc.header)
							== EC_SUCCESS) {
				return PDC_DONE;
			}
			/* dequeue error */
			return PDC_FAIL;
		} else if (get_time().val > pdc.resp_timer) {
			/* No Response */
			return PDC_NO_RESPONSE;
		}
	}

	return PDC_CONTINUE;
}

/*
 * send control message
 */
static int pdc_send_control_msg(enum pd_ctrl_msg_type  msg,
				enum tcpm_transmit_type tx_type)
{
	pdc.header = PD_HEADER(msg,
			pd[pdc.port].power_role,
			pd[pdc.port].data_role,
			pd[pdc.port].msg_id,
			0, /* 0 PDOs */
			pdc.rev,
			0); /* non-extended message */

	pd_transmit(pdc.port, tx_type, pdc.header, NULL, 0);

	return PDC_DONE;
}

/* disable vbus */
static void pdc_src_power_off(void)
{
	/* Remove VBUS */
	pd_power_supply_reset(pdc.port);
}

/* enable vbus */
static int pdc_src_power_on(void)
{
	int ret;

	ret = pd_set_power_supply_ready(pdc.port);
	tcpm_enable_auto_discharge_disconnect(pdc.port, 1);
	return ret;
}

static void pdc_update_role_and_polarity(void)
{
	/* Detect polarity */
	if (pdc.uut_role == PD_UFP) {
		pd[pdc.port].power_role = PD_ROLE_SOURCE;
		pd[pdc.port].data_role = PD_ROLE_DFP;
		pdc.polarity =
			get_src_polarity(pdc.cc1, pdc.cc2);
	} else {
		pd[pdc.port].power_role = PD_ROLE_SINK;
		pd[pdc.port].data_role = PD_ROLE_UFP;
		pdc.polarity =
			get_snk_polarity(pdc.cc1, pdc.cc2);
	}

	tcpm_set_polarity(pdc.port, pdc.polarity);
	tcpm_set_msg_header(pdc.port,
				pd[pdc.port].power_role,
				pd[pdc.port].data_role);
}

/* stops and running test and disconnects the uut */
static void pdc_uut_disconnect(void)
{
	/* stop running the test */
	pdc.run_proc = false;

	tcpm_init(pdc.port);

	/* Stop listening for pd messages */
	tcpm_set_rx_enable(pdc.port, 0);

	/* Ensure we are not sourcing Vbus */
	pdc_src_power_off();

	/* Disable VCONN */
	tcpm_set_vconn(pdc.port, 0);

	/* Remove terminations from CC */
	tcpm_set_cc(pdc.port, TYPEC_CC_OPEN);
}

/* gets the compliance port of the tester */
static int pdc_get_port(void)
{
	return pdc.port;
}

/* sets the PD rev of the tester */
static bool pdc_set_rev(uint8_t rev)
{
	switch (rev) {
	case 2:
		pdc.rev = PD_REV20;
		break;
	case 3:
		pdc.rev = PD_REV30;
		break;
	default:
		return false;
	}

	return true;
}

/* gets the PD rev of the tester */
static enum pd_rev_type pdc_get_rev(void)
{
	return pdc.rev;
}

/* sets the role of the uut */
static void pdc_set_uut_role(enum pdc_uut_role role)
{
	pdc.uut_role = role;
}

/* gets the role of the uut */
static enum pdc_uut_role pdc_get_uut_role(void)
{
	return pdc.uut_role;
}

/* sets the loop timeout */
static void pdc_set_evt_timeout(uint64_t timeout)
{
	pdc.evt_timeout = timeout;
}

/* Include PD2.0 tests */
#include "pdc_rev2.c"
/* Include PD3.0 tests */
#include "pdc_rev3.c"

void pd_comply_task(void *u)
{
	/* set the compliance port */
	pdc.port = TASK_ID_TO_PD_PORT(task_get_current());
	/* default to testing at PD Revision 2.0 */
	pdc.rev = PD_REV20;
	/* default to 3A */
	pdc.current = PDC_3A0;
	/* default to testing UUT as UFP */
	pdc.uut_role = PD_UFP;
	/* no procedures to run */
	pdc.run_proc = false;
	/* disable polling of CC lines */
	pdc.evt_timeout = -1;
	/*
	 * trick the pd interrupt handler into thinking
	 * that we are not disabled
	 */
	pd[pdc.port].task_state = 100;

	/* start out disconnected */
	pdc_uut_disconnect();

	while (1) {
		/* wait for next event or timeout expiration */
		task_wait_event(pdc.evt_timeout);

		if (pdc.run_proc && (pdc.proc != NULL)) {
			/* Sample CC lines */
			tcpm_get_cc(pdc.port, &pdc.cc1, &pdc.cc2);

			/* Run the procedure */
			pdc.run_proc = pdc.proc();
		} else {
			/* The procedure has ended. So stop polling CC lines */
			pdc.evt_timeout = (10 * MSEC);

			/*
			 * Reject any messages received when not running a test
			 */
			if (tcpm_has_pending_message(pdc.port)) {
				/* Dequeue and consume duplicate message ID. */
				if (tcpm_dequeue_message(pdc.port,
							pdc.payload,
							&pdc.header)
							== EC_SUCCESS) {
					pdc_send_control_msg(PD_CTRL_REJECT,
						TCPC_TX_SOP);
				}
			}
		}
	}
}

/* start the given test */
static void start_procedure(pdc_procedure proc)
{
	/* start test at step a */
	pdc.step = a;
	/* run the entire test */
	pdc.stop_at = z;
	/* clear all flags */
	pdc.flags = 0;
	/* reset steps sub state to 0 */
	pdc.state = 0;
	/* set procedure to run */
	pdc.proc = proc;
	/* start the test */
	pdc.run_proc = true;
	/* default event loop is 10ms */
	pdc.evt_timeout = (10 * MSEC);
	/* trigger the task to run */
	task_wake(PD_PORT_TO_TASK_ID(pdc.port));
}

/* EC console commands */
static int cmd_pdc(int argc, char *argv[])
{
	char *err;

	if (argc == 2) {
		/* pdc port - returns compliance port */
		if (!strcasecmp(argv[1], "port"))
			ccprintf("%d\n", pdc_get_port());
		/* pdc rev - returns current rev of tester */
		else if (!strcasecmp(argv[1], "rev"))
			ccprintf("%d\n", pdc_get_rev() + 1);
		/*
		 * pdc disconnect - stops and disconnect the currently
		 * running test
		 */
		else if (!strcasecmp(argv[1], "disconnect"))
			pdc_uut_disconnect();

		/*
		 * pdc list - list all available tests that are able
		 * to run.
		 */
		else if (!strcasecmp(argv[1], "list")) {
			struct pdc_tests *pdc_test;

			if (pdc_get_rev() == PD_REV20)
				pdc_test = rev2_tests;
			else
				pdc_test = rev3_tests;

			while (pdc_test->name != NULL) {
				ccprintf("\t%s - %s\n", pdc_test->name,
							pdc_test->desc);
				pdc_test++;
			}
		} else {
			struct pdc_tests *pdc_test;

			if (pdc_get_rev() == PD_REV20)
				pdc_test = rev2_tests;
			else
				pdc_test = rev3_tests;

			/* pdc <test_name> - runs the given test */
			while (pdc_test->name != NULL) {
				if (!strcasecmp(argv[1],
					pdc_test->name)) {
					start_procedure(pdc_test->proc);
					return EC_SUCCESS;
				}
				pdc_test++;
			}
			return EC_ERROR_INVAL;
		}
	} else if (argc == 3) {
		/* pdc rev <2 | 3> - sets the rev of the tester */
		if (!strcasecmp(argv[1], "rev")) {
			int rev = strtoi(argv[2], &err, 10);

			if (*err || !pdc_set_rev(rev)) {
				ccprintf(
				"Unknown revision %d. Only 2 and 3 are valid\n"
				, rev);
				return EC_ERROR_PARAM2;
			}
		}
		/* pdc uut <ufp | dfp > - sets the role of th uut */
		else if (!strcasecmp(argv[1], "uut")) {
			if (!strcasecmp(argv[2], "ufp")) {
				pdc_set_uut_role(PD_UFP);
			} else if (!strcasecmp(argv[2], "dfp")) {
				pdc_set_uut_role(PD_DFP);
			} else {
				ccprintf(
				"Unknown role %s. Only ufp and dfp are valid\n"
				, argv[2]);
				return EC_ERROR_PARAM2;
			}
		}
	}
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pdc, cmd_pdc, "\n\tport |\n\trev <\"2\"|\"3\">" \
" |\n\tuut <\"ufp\"|\"dfp\"> |\n\tdisconnect |\n\tlist |\n\t<test name>",
			"Run PD Compliance Test");
