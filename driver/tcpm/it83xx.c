/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* TCPM for MCU also running TCPC */

/*usb_pd_tcpc.c define*/
#include "adc.h"
#include "common.h"
#include "config.h"
#include "console.h"
#include "crc.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "tcpci.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "usb_pd_tcpm.h"
#include "it83xx.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*
 * Debug log level - higher number == more log
 *   Level 0: Log state transitions
 *   Level 1: Level 0, plus packet info
 *   Level 2: Level 1, plus ping packet and packet dump on error
 *
 * Note that higher log level causes timing changes and thus may affect
 * performance.
 */
static int debug_level;

#ifndef CONFIG_USB_PD_TCPM_ITE8320
static struct mutex pd_crc_lock;
#endif

#else
#define CPRINTF(format, args...)
static const int debug_level;
#endif

/* PD transmit errors */
enum pd_tx_errors {
	PD_TX_ERR_GOODCRC = -1, /* Failed to receive goodCRC */
	PD_TX_ERR_DISABLED = -2, /* Attempted transmit even though disabled */
	PD_TX_ERR_INV_ACK = -4, /* Received different packet instead of gCRC */
	PD_TX_ERR_COLLISION = -5 /* Collision detected during transmit */
};

/*
 * If TCPM is not on this chip, and PD low power is defined, then use low
 * power task delay logic.
 */
#if !defined(CONFIG_USB_POWER_DELIVERY) && defined(CONFIG_USB_PD_LOW_POWER)
#define TCPC_LOW_POWER
#endif

/*
 * Receive message buffer size. Buffer physical size is RX_BUFFER_SIZE + 1,
 * but only RX_BUFFER_SIZE of that memory is used to store messages that can
 * be retrieved from TCPM. The last slot is a temporary buffer for collecting
 * a message before deciding whether or not to keep it.
 */
#ifdef CONFIG_USB_POWER_DELIVERY
#define RX_BUFFER_SIZE 1
#else
#define RX_BUFFER_SIZE 2
#endif

static struct pd_port_controller {
	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t polarity;
	/* Our CC pull resistor setting */
	uint8_t cc_pull;
	/* CC status */
	uint8_t cc_status[2];
	/* TCPC alert status */
	uint16_t alert;
	uint16_t alert_mask;
	/* RX enabled */
	uint8_t rx_enabled;
	/* Power status */
	uint8_t power_status;
	uint8_t power_status_mask;

#ifdef TCPC_LOW_POWER
	/* Timestamp beyond which we allow low power task sampling */
	timestamp_t low_power_ts;
#endif

	/* Last received */
	int rx_head[RX_BUFFER_SIZE+1];
	uint32_t rx_payload[RX_BUFFER_SIZE+1][7];
	int rx_buf_head, rx_buf_tail;

	/* Next transmit */
	enum tcpm_transmit_type tx_type;
	uint16_t tx_head;
	uint32_t tx_payload[7];
	const uint32_t *tx_data;
} pd[CONFIG_USB_PD_PORT_COUNT];

/*usb_pd_tcpc.c func*/
static int rx_buf_is_full(int port)
{
	/* Buffer is full if the tail is 1 ahead of head */
	int diff = pd[port].rx_buf_tail - pd[port].rx_buf_head;

	return (diff == 1) || (diff == -RX_BUFFER_SIZE);
}

static int rx_buf_is_empty(int port)
{
	/* Buffer is empty if the head and tail are the same */
	return pd[port].rx_buf_tail == pd[port].rx_buf_head;
}

static void rx_buf_increment(int port, int *buf_ptr)
{
	*buf_ptr = *buf_ptr == RX_BUFFER_SIZE ? 0 : *buf_ptr + 1;
}

static int send_hard_reset(int port)
{
	ite8320_pd_hw_reset(port, USBPD_RESET_TYPE_HARD);
	return 0;
}

static int send_validate_message(int port, uint16_t header,
				 const uint32_t *data)
{
	uint8_t cnt = PD_HEADER_CNT(header);
	enum usbpd_sop_type enumSopType = USBPD_SOP_TYPE_SOP;
	enum usbpd_result result = USBPD_RESULT_FAIL;
	/*chrome code return bit_len (result >= 0) if tx success.*/
	result = ite8320_pd_tx_data(port, enumSopType,
					(header & 0x0f), cnt, data);

	/*check TX status*/
	if (result == USBPD_RESULT_SUCC)
		return 1;
	else if (result == USBPD_RESULT_TX_DISCARD)
		return PD_TX_ERR_DISABLED;
	else
		return PD_TX_ERR_GOODCRC;
}

static void bist_mode_2_tx(int port)
{
	ite8320_pd_bist_mode_2_tx(port);
}

int pd_analyze_rx(int port, uint32_t *payload)
{
	enum usbpd_result     enumResult = USBPD_RESULT_SUCC;
	enum usbpd_sop_type   enumSopType = USBPD_SOP_TYPE_SOP;
	struct usbpd_header   stHeader;
	int header = 0;

	enumResult = ite8320_pd_rx_data(port, &enumSopType, &stHeader, payload);
	memcpy(&header, &stHeader, sizeof(struct usbpd_header));

	if (enumResult != USBPD_RESULT_SUCC)
		return -1;
	return header;
}

static void handle_request(int port, uint16_t head)
{
	int cnt = PD_HEADER_CNT(head);

	if (PD_HEADER_TYPE(head) != PD_CTRL_GOOD_CRC || cnt) {
		/*send_goodcrc(port, PD_HEADER_ID(head));*/
	} else {
		/* keep RX monitoring on to avoid collisions */
		pd_rx_enable_monitoring(port);
	}
}

static void alert(int port, int mask)
{
	/* Always update the Alert status register */
	pd[port].alert |= mask;
	/*
	 * Only send interrupt to TCPM if corresponding
	 * bit in the alert_enable register is set.
	 */
	if (pd[port].alert_mask & mask)
		tcpc_alert(port);
}

int tcpc_run(int port, int evt)
{
	int cc, i, res;

	/* incoming packet ? */
	if ((evt & PD_EVENT_RX) && pd[port].rx_enabled) {
		/* Get message and place at RX buffer head */
		res = pd[port].rx_head[pd[port].rx_buf_head] =
			pd_analyze_rx(port,
				pd[port].rx_payload[pd[port].rx_buf_head]);
		/*pd_rx_complete is accomplished in pd_analyze_rx*/
		/*pd_rx_complete(port);*/
		/*
		 * If there is space in buffer, then increment head to keep
		 * the message and send goodCRC. If this is a hard reset,
		 * send alert regardless of rx buffer status. Else if there is
		 * no space in buffer, then do not send goodCRC and drop
		 * message.
		 */
	 /*CPRINTF("res 0x%x, rx full:0x%x\n", res, rx_buf_is_full(port));*/
		if (res > 0 && !rx_buf_is_full(port)) {
			rx_buf_increment(port, &pd[port].rx_buf_head);
			handle_request(port, res);
			alert(port, TCPC_REG_ALERT_RX_STATUS);
		} else if (USBPD_IS_HARD_RESET_DETECT(port)) {
			alert(port, TCPC_REG_ALERT_RX_HARD_RST);
		}
	}
	/* outgoing packet ? */
	if ((evt & PD_EVENT_TX) && pd[port].rx_enabled) {
		switch (pd[port].tx_type) {
		case TCPC_TX_SOP:
			res = send_validate_message(port,
					pd[port].tx_head,
					pd[port].tx_data);
			break;
		case TCPC_TX_BIST_MODE_2:
			bist_mode_2_tx(port);
			res = 0;
			break;
		case TCPC_TX_HARD_RESET:
			res = send_hard_reset(port);
			break;
		default:
			res = PD_TX_ERR_DISABLED;
			break;
		}
		/* send appropriate alert for tx completion */
		if (res >= 0)
			alert(port, TCPC_REG_ALERT_TX_SUCCESS);
		else if (res == PD_TX_ERR_GOODCRC)
			alert(port, TCPC_REG_ALERT_TX_FAILED);
		else
			alert(port, TCPC_REG_ALERT_TX_DISCARDED);
	} else {
		/* If we have nothing to transmit, then sample CC lines */
		/* CC pull changed, wait 1ms for CC voltage to stabilize */
		if (evt & PD_EVENT_CC)
			usleep(MSEC);
		/* check CC lines */
		for (i = 0; i < 2; i++) {
			cc = ite8320_pd_get_cc(port, i);
			if (pd[port].cc_status[i] != cc) {
				pd[port].cc_status[i] = cc;
				alert(port, TCPC_REG_ALERT_CC_STATUS);
			}
		}
	}
	/* make sure PD monitoring is enabled to wake on PD RX */
	if (pd[port].rx_enabled)
		pd_rx_enable_monitoring(port);

#ifdef TCPC_LOW_POWER
	/*
	 * If we are presenting Rd with no connection, and timestamp is
	 * past the low power timestamp, then we don't need to sample
	 * CC lines as often. In this case, our connection delay should not
	 * actually increased because we will get an interrupt on VBUS detect.
	 */
	return (get_time().val >= pd[port].low_power_ts.val &&
		pd[port].cc_pull == TYPEC_CC_RD &&
		pd[port].cc_status[0] == TYPEC_CC_VOLT_OPEN &&
		pd[port].cc_status[1] == TYPEC_CC_VOLT_OPEN) ? 200 * MSEC :
							       10 * MSEC;
#else
	return 10*MSEC;
#endif
}

void pd_rx_event(int port)
{
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_RX, 0);
}

int tcpm_alert_status_clear(int port, uint16_t mask)
{
	/*
	 * If the RX status alert is attempting to be cleared, then increment
	 * rx buffer tail pointer. if the RX buffer is not empty, then keep
	 * the RX status alert active.
	 */
	if (mask & TCPC_REG_ALERT_RX_STATUS) {
		if (!rx_buf_is_empty(port)) {
			rx_buf_increment(port, &pd[port].rx_buf_tail);
			if (!rx_buf_is_empty(port))
				/* buffer is not empty, keep alert active */
				mask &= ~TCPC_REG_ALERT_RX_STATUS;
		}
	}

	/* clear only the bits specified by the TCPM */
	pd[port].alert &= ~mask;
#ifndef CONFIG_USB_POWER_DELIVERY
	/* Set Alert# inactive if all alert bits clear
	 * 8320 have no tcpc_alert register
	 */
#endif
	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
static int tcpc_set_power_status(int port, int vbus_present)
{
	/* Update VBUS present bit */
	if (vbus_present)
		pd[port].power_status |= TCPC_REG_POWER_STATUS_VBUS_PRES;
	else
		pd[port].power_status &= ~TCPC_REG_POWER_STATUS_VBUS_PRES;

	/* Set bit Port Power Status bit in Alert register */
	if (pd[port].power_status_mask & TCPC_REG_POWER_STATUS_VBUS_PRES)
		alert(port, TCPC_REG_ALERT_POWER_STATUS);

	return EC_SUCCESS;
}
#endif /* CONFIG_USB_PD_TCPM_VBUS */

void tcpc_pre_init(void)
{
	int i;

	/* Mark as uninitialized */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		pd[i].power_status |= TCPC_REG_POWER_STATUS_UNINIT |
				      TCPC_REG_POWER_STATUS_VBUS_DET;
}
/* Must be prioritized above i2c init */
DECLARE_HOOK(HOOK_INIT, tcpc_pre_init, HOOK_PRIO_INIT_I2C - 1);

void tcpc_init(int port)
{
	int i;

	/* Initialize physical layer */
	pd_hw_init(port, PD_ROLE_DEFAULT);
	pd[port].cc_pull = PD_ROLE_DEFAULT == PD_ROLE_SOURCE ? TYPEC_CC_RP :
							       TYPEC_CC_RD;
#ifdef TCPC_LOW_POWER
	/* Don't use low power immediately after boot */
	pd[port].low_power_ts.val = get_time().val + SECOND;
#endif

	/* make sure PD monitoring is disabled initially */
	pd[port].rx_enabled = 0;
	/* make initial readings of CC voltages */
	for (i = 0; i < 2; i++)
		pd[port].cc_status[i] = ite8320_pd_get_cc(port, i);
#ifdef CONFIG_USB_PD_TCPM_VBUS
	tcpc_set_power_status(port, ite8320_pd_detect_vbus(port));
#endif /* CONFIG_USB_PD_TCPM_VBUS */

	/* set default alert and power mask register values */
	pd[port].alert_mask = TCPC_REG_ALERT_MASK_ALL;
	pd[port].power_status_mask = TCPC_REG_POWER_STATUS_MASK_ALL;

	/* set power status alert since the UNINIT bit has been set */
	alert(port, TCPC_REG_ALERT_POWER_STATUS);
}

#ifdef CONFIG_USB_PD_TCPM_VBUS
void pd_vbus_evt_p0(enum gpio_signal signal)
{
	tcpc_set_power_status(TASK_ID_TO_PD_PORT(TASK_ID_PD_C0),
				ite8320_pd_detect_vbus(0));
	task_wake(TASK_ID_PD_C0);
}

#if CONFIG_USB_PD_PORT_COUNT >= 2
void pd_vbus_evt_p1(enum gpio_signal signal)
{
	tcpc_set_power_status(TASK_ID_TO_PD_PORT(TASK_ID_PD_C1),
				ite8320_pd_detect_vbus(1));
	task_wake(TASK_ID_PD_C1);
}
#endif /* PD_PORT_COUNT >= 2 */
#endif /* CONFIG_USB_PD_TCPM_VBUS */

#ifdef CONFIG_COMMON_RUNTIME
static int command_tcpc(int argc, char **argv)
{
	int port;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!strcasecmp(argv[1], "dump")) {
		int level;

		if (argc < 3)
			ccprintf("lvl: %d\n", debug_level);
		else {
			level = strtoi(argv[2], &e, 10);
			if (*e)
				return EC_ERROR_PARAM2;
			debug_level = level;
		}
		return EC_SUCCESS;
	}

	/* command: pd <port> <subcmd> [args] */
	port = strtoi(argv[1], &e, 10);
	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;
	if (*e || port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_ERROR_PARAM2;

	if (!strcasecmp(argv[2], "clock")) {
		int freq;

		if (argc < 4)
			return EC_ERROR_PARAM2;

		freq = strtoi(argv[3], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;
		pd_set_clock(port, freq);
		ccprintf("set TX frequency to %d Hz\n", freq);
		return EC_SUCCESS;
	} else if (!strncasecmp(argv[2], "state", 5)) {
		ccprintf("Port C%d, %s - CC:%d, CC0:%d, CC1:%d\n"
			 "Alert: 0x%02x Mask: 0x%04x\n"
			 "Power Status: 0x%02x Mask: 0x%02x\n", port,
			 pd[port].rx_enabled ? "Ena" : "Dis",
			 pd[port].cc_pull,
			 pd[port].cc_status[0], pd[port].cc_status[1],
			 pd[port].alert, pd[port].alert_mask,
			 pd[port].power_status, pd[port].power_status_mask);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tcpc, command_tcpc,
			"dump [0|1]\n\t<port> [clock|state]",
			"Type-C Port Controller",
			NULL);
#endif

/*TCPCI function define*/
static int init_alert_mask(int port)
{
	uint16_t mask;
	int rv;

	/*
	 * Create mask of alert events that will cause the TCPC to
	 * signal the TCPM via the Alert# gpio line.
	 */
	mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED |
		TCPC_REG_ALERT_TX_DISCARDED | TCPC_REG_ALERT_RX_STATUS |
		TCPC_REG_ALERT_RX_HARD_RST | TCPC_REG_ALERT_CC_STATUS;
	/* Set the alert mask in TCPC */
	rv = tcpm_alert_mask_set(port, mask);

	return rv;
}

static int init_power_status_mask(int port)
{
	return tcpm_set_power_status_mask(port, 0);
}

int tcpm_init(int port)
{
	int rv;

	tcpc_init(port);
	rv = init_alert_mask(port);
	if (rv)
		return rv;

	return init_power_status_mask(port);
}

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	int cc, i;
	/* check CC lines */
	for (i = 0; i < 2; i++) {
		cc = ite8320_pd_get_cc(port, i);
		if (pd[port].cc_status[i] != cc) {
			pd[port].cc_status[i] = cc;
			alert(port, TCPC_REG_ALERT_CC_STATUS);
		}
	}

	*cc2 = pd[port].cc_status[1];
	*cc1 = pd[port].cc_status[0];

	return EC_SUCCESS;
}

int tcpm_set_cc(int port, int pull)
{
	/* If CC pull resistor not changing, then nothing to do */
	if (pd[port].cc_pull == pull)
		return EC_SUCCESS;

	/* Change CC pull resistor */
	pd[port].cc_pull = pull;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	 pd_set_host_mode(port, pull);
#endif

#ifdef TCPC_LOW_POWER
	/*
	 * Reset the low power timestamp every time CC termination toggles,
	 * because we only want to go into low power mode when we are not
	 * dual-role toggling.
	 */
	pd[port].low_power_ts.val = get_time().val +
					2*(PD_T_DRP_SRC + PD_T_DRP_SNK);
#endif

	/*
	 * Before CC pull can be changed and the task can read the new
	 * status, we should set the CC status to open, in case TCPM
	 * asks before it is known for sure.
	 */
	pd[port].cc_status[0] = TYPEC_CC_VOLT_OPEN;
	pd[port].cc_status[1] = pd[port].cc_status[0];

	/* Wake the PD phy task with special CC event mask */
	/* TODO: use top case if no TCPM on same CPU */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpc_run(port, PD_EVENT_CC);
#else
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
#endif
	return EC_SUCCESS;
}

int tcpm_set_polarity(int port, int polarity)
{
	pd[port].polarity = polarity;
	pd_select_polarity(port, pd[port].polarity);

	return EC_SUCCESS;
}

int tcpm_set_power_status_mask(int port, uint8_t mask)
{
	pd[port].power_status_mask = mask;
	return EC_SUCCESS;
}

int tcpm_set_vconn(int port, int enable)
{
#ifdef CONFIG_USBC_VCONN
	ite8320_pd_enable_vconn(port, enable);
#endif
	return EC_SUCCESS;
}

int tcpm_set_msg_header(int port, int power_role, int data_role)
{
	pd[port].power_role = power_role;
	pd[port].data_role = data_role;

	return EC_SUCCESS;
}

int tcpm_alert_status(int port, int *alert)
{
	/* Read Alert register */
	uint16_t ret = pd[port].alert;
	*alert = ret;
	return EC_SUCCESS;
}

int tcpm_set_rx_enable(int port, int enable)
{
#if defined(CONFIG_LOW_POWER_IDLE) && !defined(CONFIG_USB_POWER_DELIVERY)
		int i;
#endif
	pd[port].rx_enabled = enable;

	if (enable)
		IMR(port) &= ~USBPD_MSG_RX_DONE;
	else
		IMR(port) |= USBPD_MSG_RX_DONE;

	if (!enable)
		pd_rx_disable_monitoring(port);

#if defined(CONFIG_LOW_POWER_IDLE) && !defined(CONFIG_USB_POWER_DELIVERY)
	/* If any PD port is connected, then disable deep sleep */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; ++i)
		if (pd[port].rx_enabled)
			break;

	if (i == CONFIG_USB_PD_PORT_COUNT)
		enable_sleep(SLEEP_MASK_USB_PD);
	else
		disable_sleep(SLEEP_MASK_USB_PD);
#endif
	return EC_SUCCESS;
}

int tcpm_alert_mask_set(int port, uint16_t mask)
{
	/* Update the alert mask as specificied by the TCPM */
	pd[port].alert_mask = mask;
	return EC_SUCCESS;
}

int tcpm_get_message(int port, uint32_t *payload, int *head)
{
	/* Get message at tail of RX buffer */
	int idx = pd[port].rx_buf_tail;

	memcpy(payload, pd[port].rx_payload[idx],
	       sizeof(pd[port].rx_payload[idx]));
	*head = pd[port].rx_head[idx];

	/* Read complete, clear RX status alert bit */
	tcpm_alert_status_clear(port, TCPC_REG_ALERT_RX_STATUS);

	return EC_SUCCESS;
}

int tcpm_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
		  const uint32_t *data)
{
	/* Store data to transmit and wake task to send it */
	pd[port].tx_type = type;
	pd[port].tx_head = header;
	pd[port].tx_data = data;
	/* TODO: use top case if no TCPM on same CPU */
#ifdef CONFIG_USB_POWER_DELIVERY
	tcpc_run(port, PD_EVENT_TX);
#else
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_TX, 0);
#endif
	return EC_SUCCESS;
}

void tcpc_alert(int port)
{
	int status;

	/* Read the Alert register from the TCPC */
	tcpm_alert_status(port, &status);

	/*
	 * Clear alert status for everything except RX_STATUS, which shouldn't
	 * be cleared until we have successfully retrieved message.
	 */
	if (status & ~TCPC_REG_ALERT_RX_STATUS)
		tcpm_alert_status_clear(port,
					status & ~TCPC_REG_ALERT_RX_STATUS);

	if (status & TCPC_REG_ALERT_CC_STATUS) {
		/* CC status changed, wake task */
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_CC, 0);
	}
	if (status & TCPC_REG_ALERT_RX_STATUS) {
		/*
		 * message received. since TCPC is compiled in, we
		 * already received PD_EVENT_RX from phy layer in
		 * pd_rx_event(), so we don't need to set another
		 * event.
		 */
	}
	if (status & TCPC_REG_ALERT_RX_HARD_RST) {
		/* hard reset received */
		pd_execute_hard_reset(port);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
	if (status & TCPC_REG_ALERT_TX_COMPLETE) {
		/* transmit complete */
		pd_transmit_complete(port, status & TCPC_REG_ALERT_TX_SUCCESS ?
					   TCPC_TX_COMPLETE_SUCCESS :
					   TCPC_TX_COMPLETE_FAILED);
	}
}

