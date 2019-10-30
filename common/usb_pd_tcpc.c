/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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
#include "tcpm.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "usb_pd_tcpm.h"

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
#else
#define CPRINTF(format, args...)
static const int debug_level;
#endif

/* Start of Packet sequence : three Sync-1 K-codes, then one Sync-2 K-code */
#define PD_SOP (PD_SYNC1 | (PD_SYNC1<<5) | (PD_SYNC1<<10) | (PD_SYNC2<<15))
#define PD_SOP_PRIME	(PD_SYNC1 | (PD_SYNC1<<5) | \
			(PD_SYNC3<<10) | (PD_SYNC3<<15))
#define PD_SOP_PRIME_PRIME	(PD_SYNC1 | (PD_SYNC3<<5) | \
				(PD_SYNC1<<10) | (PD_SYNC3<<15))

/* Hard Reset sequence : three RST-1 K-codes, then one RST-2 K-code */
#define PD_HARD_RESET (PD_RST1 | (PD_RST1 << 5) |\
		      (PD_RST1 << 10) | (PD_RST2 << 15))

/*
 * Polarity based on 'DFP Perspective' (see table USB Type-C Cable and Connector
 * Specification)
 *
 * CC1    CC2    STATE             POSITION
 * ----------------------------------------
 * open   open   NC                N/A
 * Rd     open   UFP attached      1
 * open   Rd     UFP attached      2
 * open   Ra     pwr cable no UFP  N/A
 * Ra     open   pwr cable no UFP  N/A
 * Rd     Ra     pwr cable & UFP   1
 * Ra     Rd     pwr cable & UFP   2
 * Rd     Rd     dbg accessory     N/A
 * Ra     Ra     audio accessory   N/A
 *
 * Note, V(Rd) > V(Ra)
 */
#ifndef PD_SRC_RD_THRESHOLD
#define PD_SRC_RD_THRESHOLD PD_SRC_DEF_RD_THRESH_MV
#endif
#ifndef PD_SRC_VNC
#define PD_SRC_VNC PD_SRC_DEF_VNC_MV
#endif

#ifndef CC_RA
#define CC_RA(port, cc, sel)  (cc < PD_SRC_RD_THRESHOLD)
#endif
#define CC_RD(cc) ((cc >= PD_SRC_RD_THRESHOLD) && (cc < PD_SRC_VNC))
#ifndef CC_NC
#define CC_NC(port, cc, sel)  (cc >= PD_SRC_VNC)
#endif

/*
 * Polarity based on 'UFP Perspective'.
 *
 * CC1    CC2    STATE             POSITION
 * ----------------------------------------
 * open   open   NC                N/A
 * Rp     open   DFP attached      1
 * open   Rp     DFP attached      2
 * Rp     Rp     Accessory attached N/A
 */
#ifndef PD_SNK_VA
#define PD_SNK_VA PD_SNK_VA_MV
#endif

#define CC_RP(cc)  (cc >= PD_SNK_VA)

/*
 * Type C power source charge current limits are identified by their cc
 * voltage (set by selecting the proper Rd resistor). Any voltage below
 * TYPE_C_SRC_500_THRESHOLD will not be identified as a type C charger.
 */
#define TYPE_C_SRC_500_THRESHOLD	PD_SRC_RD_THRESHOLD
#define TYPE_C_SRC_1500_THRESHOLD	660  /* mV */
#define TYPE_C_SRC_3000_THRESHOLD	1230 /* mV */

/* Convert TCPC Alert register to index into pd.alert[] */
#define ALERT_REG_TO_INDEX(reg) (reg - TCPC_REG_ALERT)

/* PD transmit errors */
enum pd_tx_errors {
	PD_TX_ERR_GOODCRC = -1, /* Failed to receive goodCRC */
	PD_TX_ERR_DISABLED = -2, /* Attempted transmit even though disabled */
	PD_TX_ERR_INV_ACK = -4, /* Received different packet instead of gCRC */
	PD_TX_ERR_COLLISION = -5 /* Collision detected during transmit */
};

/* PD Header with SOP* encoded in bits 31 - 28 */
union pd_header_sop {
	uint16_t pd_header;
	uint32_t head;
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
	uint8_t msg_id_last;

	/* Next transmit */
	enum tcpm_transmit_type tx_type;
	uint16_t tx_head;
	uint32_t tx_payload[7];
	const uint32_t *tx_data;
} pd[CONFIG_USB_PD_PORT_COUNT];

void invalidate_last_message_id(int port)
{
	/*
	 * Message id starts from 0 to 7. If msg_id_last is initialized to 0,
	 * it will lead to repetitive message id with first received packet,
	 * so initialize it with an invalid value 0xff.
	 */
	pd[port].msg_id_last = 0xff;
}

int consume_repeat_message(int port, uint16_t msg_header)
{
	uint8_t msg_id = PD_HEADER_ID(msg_header);

	/* If repeat message ignore, except softreset control request. */
	if (PD_HEADER_TYPE(msg_header) == PD_CTRL_SOFT_RESET &&
	    PD_HEADER_CNT(msg_header) == 0) {
		invalidate_last_message_id(port);
	} else if (pd[port].msg_id_last != msg_id) {
		pd[port].msg_id_last = msg_id;
	} else if (pd[port].msg_id_last == msg_id) {
		CPRINTF("Repeat msg_id[%d] port[%d]\n", msg_id, port);
		return 1;
	}

	return 0;
}

static int rx_buf_is_full(int port)
{
	/*
	 * TODO: Refactor these to use the incrementing-counter idiom instead of
	 * the wrapping-counter idiom to reclaim the last buffer entry.
	 *
	 * Buffer is full if the tail is 1 ahead of head.
	 */
	int diff = pd[port].rx_buf_tail - pd[port].rx_buf_head;
	return (diff == 1) || (diff == -RX_BUFFER_SIZE);
}

int rx_buf_is_empty(int port)
{
	/* Buffer is empty if the head and tail are the same */
	return pd[port].rx_buf_tail == pd[port].rx_buf_head;
}

void rx_buf_clear(int port)
{
	pd[port].rx_buf_tail = pd[port].rx_buf_head;
}

static void rx_buf_increment(int port, int *buf_ptr)
{
	*buf_ptr = *buf_ptr == RX_BUFFER_SIZE ? 0 : *buf_ptr + 1;
}

int prepare_message(int port, uint16_t header, uint8_t cnt,
		    const uint32_t *data);

int send_hard_reset(int port);

int send_validate_message(int port, uint16_t header,
			  const uint32_t *data);

void send_goodcrc(int port, int id);

#if 0
/* TODO: when/how do we trigger this ? */
static int analyze_rx_bist(int port);

void bist_mode_2_rx(int port)
{
	int analyze_bist = 0;
	int num_bits;
	timestamp_t start_time;

	/* monitor for incoming packet */
	pd_rx_enable_monitoring(port);

	/* loop until we start receiving data */
	start_time.val = get_time().val;
	while ((get_time().val - start_time.val) < (500*MSEC)) {
		task_wait_event(10*MSEC);
		/* incoming packet ? */
		if (pd_rx_started(port)) {
			analyze_bist = 1;
			break;
		}
	}

	if (analyze_bist) {
		/*
		 * once we start receiving bist data, analyze 40 bytes
		 * every 10 msec. Continue analyzing until BIST data
		 * is no longer received. The standard limits the max
		 * BIST length to 60 msec.
		 */
		start_time.val = get_time().val;
		while ((get_time().val - start_time.val)
			< (PD_T_BIST_RECEIVE)) {
			num_bits = analyze_rx_bist(port);
			pd_rx_complete(port);
			/*
			 * If no data was received, then analyze_rx_bist()
			 * will return a -1 and there is no need to stay
			 * in this mode
			 */
			if (num_bits == -1)
				break;
			msleep(10);
			pd_rx_enable_monitoring(port);
		}
	} else {
		CPRINTF("BIST RX TO\n");
	}
}
#endif

void bist_mode_2_tx(int port);
int decode_short(int port, int off, uint16_t *val16);
int decode_word(int port, int off, uint32_t *val32);

#ifdef CONFIG_COMMON_RUNTIME
#if 0
/*
 * TODO: when/how do we trigger this ? Could add custom vendor command
 * to TCPCI to enter bist verification? Is there an easier way?
 */
static int count_set_bits(int n)
{
	int count = 0;
	while (n) {
		n &= (n - 1);
		count++;
	}
	return count;
}

static int analyze_rx_bist(int port)
{
	int i = 0, bit = -1;
	uint32_t w, match;
	int invalid_bits = 0;
	int bits_analyzed = 0;
	static int total_invalid_bits;

	/* dequeue bits until we see a full byte of alternating 1's and 0's */
	while (i < 10 && (bit < 0 || (w != 0xaa && w != 0x55)))
		bit = pd_dequeue_bits(port, i++, 8, &w);

	/* if we didn't find any bytes that match criteria, display error */
	if (i == 10) {
		CPRINTF("invalid pattern\n");
		return -1;
	}
	/*
	 * now we know what matching byte we are looking for, dequeue a bunch
	 * more data and count how many bits differ from expectations.
	 */
	match = w;
	bit = i - 1;
	for (i = 0; i < 40; i++) {
		bit = pd_dequeue_bits(port, bit, 8, &w);
		if (i && (i % 20 == 0))
			CPRINTF("\n");
		CPRINTF("%02x ", w);
		bits_analyzed += 8;
		invalid_bits += count_set_bits(w ^ match);
	}

	total_invalid_bits += invalid_bits;

	CPRINTF("\nInvalid: %d/%d\n",
		invalid_bits, total_invalid_bits);
	return bits_analyzed;
}
#endif
#endif

int pd_analyze_rx(int port, uint32_t *payload);

static void handle_request(int port, uint16_t head)
{
	int cnt = PD_HEADER_CNT(head);

	if (PD_HEADER_TYPE(head) != PD_CTRL_GOOD_CRC || cnt)
		send_goodcrc(port, PD_HEADER_ID(head));
	else
		/* keep RX monitoring on to avoid collisions */
		pd_rx_enable_monitoring(port);
}

static void alert(int port, int mask)
{
	CPRINTF("alert(%d, 0x%04x) alert_mask=0x%04x\n", port, mask, pd[port].alert_mask);
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
	//CPRINTF("tcpc_run(%d, %d) rx_enable=%d tx_type=%d\n", port, evt, pd[port].rx_enabled, pd[port].tx_type);

	/* incoming packet ? */
	if (pd_rx_started(port) && pd[port].rx_enabled) {
		/* Get message and place at RX buffer head */
		res = pd[port].rx_head[pd[port].rx_buf_head] =
			pd_analyze_rx(port,
				pd[port].rx_payload[pd[port].rx_buf_head]);
		pd_rx_complete(port);

		/*
		 * If there is space in buffer, then increment head to keep
		 * the message and send goodCRC. If this is a hard reset,
		 * send alert regardless of rx buffer status. Else if there is
		 * no space in buffer, then do not send goodCRC and drop
		 * message.
		 */
		if (res > 0 && !rx_buf_is_full(port)) {
			rx_buf_increment(port, &pd[port].rx_buf_head);
			handle_request(port, res);
			alert(port, TCPC_REG_ALERT_RX_STATUS);
		} else if (res == PD_RX_ERR_HARD_RESET) {
			alert(port, TCPC_REG_ALERT_RX_HARD_RST);
		}
	}

	/* outgoing packet ? */
	if ((evt & PD_EVENT_TX) && pd[port].rx_enabled) {
		switch (pd[port].tx_type) {
#if defined(CONFIG_USB_TYPEC_VPD) || defined(CONFIG_USB_TYPEC_CTVPD)
		case TCPC_TX_SOP_PRIME:
#else
		case TCPC_TX_SOP:
#endif
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
			/* Read CC voltage, convert to status */
			cc = pd_read_cc_status(port, i);
			/* Check for status change */
			if (pd[port].cc_status[i] != cc) {
				CPRINTF("New state for CC%d line: %d\n", i + 1, cc);
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
		cc_is_open(pd[port].cc_status[0], pd[port].cc_status[1]))
		       ? 200 * MSEC
		       : 10 * MSEC;
#else
	return 10*MSEC;
#endif
}

#if !defined(CONFIG_USB_POWER_DELIVERY) && !defined(CONFIG_USB_SM_FRAMEWORK)
void pd_task(void *u)
{
	int port = TASK_ID_TO_PD_PORT(task_get_current());
	int timeout = 10*MSEC;
	int evt;

	/* initialize phy task */
	tcpc_init(port);

	/* we are now initialized */
	pd[port].power_status &= ~TCPC_REG_POWER_STATUS_UNINIT;

	while (1) {
		/* wait for next event/packet or timeout expiration */
		evt = task_wait_event(timeout);

		/* run phy task once */
		timeout = tcpc_run(port, evt);
	}
}
#endif

void pd_rx_event(int port)
{
	task_set_event(PD_PORT_TO_TASK_ID(port), TASK_EVENT_WAKE, 0);
}

int tcpc_alert_status(int port, int *alert)
{
	/* return the value of the TCPC Alert register */
	uint16_t ret = pd[port].alert;
	*alert = ret;
	return EC_SUCCESS;
}

int tcpc_alert_status_clear(int port, uint16_t mask)
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
	/* Set Alert# inactive if all alert bits clear */
	if (!pd[port].alert)
		tcpc_alert_clear(port);
#endif
	return EC_SUCCESS;
}

int tcpc_alert_mask_set(int port, uint16_t mask)
{
	/* Update the alert mask as specificied by the TCPM */
	pd[port].alert_mask = mask;
	return EC_SUCCESS;
}

int tcpc_set_cc(int port, int pull)
{
	/* If CC pull resistor not changing, then nothing to do */
	if (pd[port].cc_pull == pull)
		return EC_SUCCESS;

	/* Change CC pull resistor */
	pd[port].cc_pull = pull;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	pd_set_host_mode(port, pull == TYPEC_CC_RP);
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

int tcpc_get_cc(int port, int *cc1, int *cc2)
{
	*cc2 = pd[port].cc_status[1];
	*cc1 = pd[port].cc_status[0];

	return EC_SUCCESS;
}

int board_select_rp_value(int port, int rp) __attribute__((weak));

int tcpc_select_rp_value(int port, int rp)
{
	if (board_select_rp_value)
		return board_select_rp_value(port, rp);
	else
		return EC_ERROR_UNIMPLEMENTED;
}

int tcpc_set_polarity(int port, int polarity)
{
	pd[port].polarity = polarity;
	pd_select_polarity(port, pd[port].polarity);

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_TCPC_TRACK_VBUS
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
#endif /* CONFIG_USB_PD_TCPC_TRACK_VBUS */

int tcpc_set_power_status_mask(int port, uint8_t mask)
{
	pd[port].power_status_mask = mask;
	return EC_SUCCESS;
}

int tcpc_set_vconn(int port, int enable)
{
#ifdef CONFIG_USBC_VCONN
	pd_set_vconn(port, pd[port].polarity, enable);
#endif
	return EC_SUCCESS;
}

int tcpc_set_rx_enable(int port, int enable)
{
#if defined(CONFIG_LOW_POWER_IDLE) && !defined(CONFIG_USB_POWER_DELIVERY)
	int i;
#endif
	pd[port].rx_enabled = enable;

	if (!enable)
		pd_rx_disable_monitoring(port);

#if defined(CONFIG_LOW_POWER_IDLE) && !defined(CONFIG_USB_POWER_DELIVERY)
	/* If any PD port is connected, then disable deep sleep */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; ++i)
		if (pd[i].rx_enabled)
			break;

	if (i == CONFIG_USB_PD_PORT_COUNT)
		enable_sleep(SLEEP_MASK_USB_PD);
	else
		disable_sleep(SLEEP_MASK_USB_PD);
#endif
	return EC_SUCCESS;
}

int tcpc_transmit(int port, enum tcpm_transmit_type type, uint16_t header,
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

int tcpc_set_msg_header(int port, int power_role, int data_role)
{
	pd[port].power_role = power_role;
	pd[port].data_role = data_role;

	return EC_SUCCESS;
}

int tcpc_get_message(int port, uint32_t *payload, int *head)
{
	/* Get message at tail of RX buffer */
	int idx = pd[port].rx_buf_tail;

	memcpy(payload, pd[port].rx_payload[idx],
	       sizeof(pd[port].rx_payload[idx]));
	*head = pd[port].rx_head[idx];
	return EC_SUCCESS;
}

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

	CPRINTF("tcpc_init(%d)\n", port);

	/* Initialize physical layer */
	pd_hw_init(port, PD_ROLE_DEFAULT(port));
	pd[port].cc_pull = PD_ROLE_DEFAULT(port) ==
		PD_ROLE_SOURCE ? TYPEC_CC_RP : TYPEC_CC_RD;
#ifdef TCPC_LOW_POWER
	/* Don't use low power immediately after boot */
	pd[port].low_power_ts.val = get_time().val + SECOND;
#endif

	/* make sure PD monitoring is disabled initially */
	pd[port].rx_enabled = 0;
	invalidate_last_message_id(port);

	/* make initial readings of CC voltages */
	for (i = 0; i < 2; i++) {
		pd[port].cc_status[i] = pd_read_cc_status(port, i);
	}

#ifdef CONFIG_USB_PD_TCPC_TRACK_VBUS
#if CONFIG_USB_PD_PORT_COUNT >= 2
	tcpc_set_power_status(port, !gpio_get_level(port ?
			      GPIO_USB_C1_VBUS_WAKE_L :
			      GPIO_USB_C0_VBUS_WAKE_L));
#else
	tcpc_set_power_status(port, !gpio_get_level(GPIO_USB_C0_VBUS_WAKE_L));
#endif /* CONFIG_USB_PD_PORT_COUNT >= 2 */
#endif /* CONFIG_USB_PD_TCPC_TRACK_VBUS */

	/* set default alert and power mask register values */
	pd[port].alert_mask = TCPC_REG_ALERT_MASK_ALL;
	pd[port].power_status_mask = TCPC_REG_POWER_STATUS_MASK_ALL;

	/* set power status alert since the UNINIT bit has been set */
	alert(port, TCPC_REG_ALERT_POWER_STATUS);
}

#ifdef CONFIG_USB_PD_TCPC_TRACK_VBUS
void pd_vbus_evt_p0(enum gpio_signal signal)
{
	tcpc_set_power_status(TASK_ID_TO_PD_PORT(TASK_ID_PD_C0),
				 !gpio_get_level(GPIO_USB_C0_VBUS_WAKE_L));
	task_wake(TASK_ID_PD_C0);
}

#if CONFIG_USB_PD_PORT_COUNT >= 2
void pd_vbus_evt_p1(enum gpio_signal signal)
{
	tcpc_set_power_status(TASK_ID_TO_PD_PORT(TASK_ID_PD_C1),
				 !gpio_get_level(GPIO_USB_C1_VBUS_WAKE_L));
	task_wake(TASK_ID_PD_C1);
}
#endif /* PD_PORT_COUNT >= 2 */
#endif /* CONFIG_USB_PD_TCPC_TRACK_VBUS */

#ifndef CONFIG_USB_POWER_DELIVERY
static void tcpc_i2c_write(int port, int reg, int len, uint8_t *payload)
{
	uint16_t alert;

	/* If we are not yet initialized, ignore any write command */
	if (pd[port].power_status & TCPC_REG_POWER_STATUS_UNINIT)
		return;

	switch (reg) {
	case TCPC_REG_ROLE_CTRL:
		tcpc_set_cc(port, TCPC_REG_ROLE_CTRL_CC1(payload[1]));
		break;
	case TCPC_REG_POWER_CTRL:
		tcpc_set_vconn(port, TCPC_REG_POWER_CTRL_VCONN(payload[1]));
		break;
	case TCPC_REG_TCPC_CTRL:
		tcpc_set_polarity(port,
				  TCPC_REG_TCPC_CTRL_POLARITY(payload[1]));
		break;
	case TCPC_REG_MSG_HDR_INFO:
		tcpc_set_msg_header(port,
				    TCPC_REG_MSG_HDR_INFO_PROLE(payload[1]),
				    TCPC_REG_MSG_HDR_INFO_DROLE(payload[1]));
		break;
	case TCPC_REG_ALERT:
		alert = payload[1];
		alert |= (payload[2] << 8);
		/* clear alert bits specified by the TCPM */
		tcpc_alert_status_clear(port, alert);
		break;
	case TCPC_REG_ALERT_MASK:
		alert = payload[1];
		alert |= (payload[2] << 8);
		tcpc_alert_mask_set(port, alert);
		break;
	case TCPC_REG_RX_DETECT:
		tcpc_set_rx_enable(port, payload[1] &
					 TCPC_REG_RX_DETECT_SOP_HRST_MASK);
		break;
	case TCPC_REG_POWER_STATUS_MASK:
		tcpc_set_power_status_mask(port, payload[1]);
		break;
	case TCPC_REG_TX_HDR:
		pd[port].tx_head = (payload[2] << 8) | payload[1];
		break;
	case TCPC_REG_TX_DATA:
		memcpy(pd[port].tx_payload, &payload[1], len - 1);
		break;
	case TCPC_REG_TRANSMIT:
		tcpc_transmit(port, TCPC_REG_TRANSMIT_TYPE(payload[1]),
			      pd[port].tx_head, pd[port].tx_payload);
		break;
	}
}

static int tcpc_i2c_read(int port, int reg, uint8_t *payload)
{
	int cc1, cc2;
	int alert;

	switch (reg) {
	case TCPC_REG_VENDOR_ID:
		*(uint16_t *)payload = USB_VID_GOOGLE;
		return 2;
	case TCPC_REG_CC_STATUS:
		tcpc_get_cc(port, &cc1, &cc2);
		payload[0] = TCPC_REG_CC_STATUS_SET(
				pd[port].cc_pull == TYPEC_CC_RD,
				pd[port].cc_status[0], pd[port].cc_status[1]);
		return 1;
	case TCPC_REG_ROLE_CTRL:
		payload[0] = TCPC_REG_ROLE_CTRL_SET(0, 0,
						    pd[port].cc_pull,
						    pd[port].cc_pull);
		return 1;
	case TCPC_REG_TCPC_CTRL:
		payload[0] = TCPC_REG_TCPC_CTRL_SET(pd[port].polarity);
		return 1;
	case TCPC_REG_MSG_HDR_INFO:
		payload[0] = TCPC_REG_MSG_HDR_INFO_SET(pd[port].data_role,
						       pd[port].power_role);
		return 1;
	case TCPC_REG_RX_DETECT:
		payload[0] = pd[port].rx_enabled ?
				TCPC_REG_RX_DETECT_SOP_HRST_MASK : 0;
		return 1;
	case TCPC_REG_ALERT:
		tcpc_alert_status(port, &alert);
		payload[0] = alert & 0xff;
		payload[1] = (alert >> 8) & 0xff;
		return 2;
	case TCPC_REG_ALERT_MASK:
		payload[0] = pd[port].alert_mask & 0xff;
		payload[1] = (pd[port].alert_mask >> 8) & 0xff;
		return 2;
	case TCPC_REG_RX_BYTE_CNT:
		payload[0] = 3 + 4 *
			PD_HEADER_CNT(pd[port].rx_head[pd[port].rx_buf_tail]);
		return 1;
	case TCPC_REG_RX_HDR:
		payload[0] = pd[port].rx_head[pd[port].rx_buf_tail] & 0xff;
		payload[1] =
			(pd[port].rx_head[pd[port].rx_buf_tail] >> 8) & 0xff;
		return 2;
	case TCPC_REG_RX_DATA:
		memcpy(payload, pd[port].rx_payload[pd[port].rx_buf_tail],
		       sizeof(pd[port].rx_payload[pd[port].rx_buf_tail]));
		return sizeof(pd[port].rx_payload[pd[port].rx_buf_tail]);
	case TCPC_REG_POWER_STATUS:
		payload[0] = pd[port].power_status;
		return 1;
	case TCPC_REG_POWER_STATUS_MASK:
		payload[0] = pd[port].power_status_mask;
		return 1;
	case TCPC_REG_TX_HDR:
		payload[0] = pd[port].tx_head & 0xff;
		payload[1] = (pd[port].tx_head >> 8) & 0xff;
		return 2;
	case TCPC_REG_TX_DATA:
		memcpy(payload, pd[port].tx_payload,
		       sizeof(pd[port].tx_payload));
		return sizeof(pd[port].tx_payload);
	default:
		return 0;
	}
}

void tcpc_i2c_process(int read, int port, int len, uint8_t *payload,
		      void (*send_response)(int))
{
	int i, reg;

	if (debug_level >= 1) {
		CPRINTF("tcpci p%d: ", port);
		for (i = 0; i < len; i++)
			CPRINTF("0x%02x ", payload[i]);
		CPRINTF("\n");
	}

	/* length must always be at least 1 */
	if (len == 0) {
		/*
		 * if this is a read, we must call send_response() for
		 * i2c transaction to finishe properly
		 */
		if (read)
			(*send_response)(0);
	}

	/* if this is a write, length must be at least 2 */
	if (!read && len < 2)
		return;

	/* register is always first byte */
	reg = payload[0];

	/* perform read or write */
	if (read) {
		len = tcpc_i2c_read(port, reg, payload);
		(*send_response)(len);
	} else {
		tcpc_i2c_write(port, reg, len, payload);
	}
}
#endif

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
			"Type-C Port Controller");
#endif
