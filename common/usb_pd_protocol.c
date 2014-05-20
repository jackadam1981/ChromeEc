/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "common.h"
#include "console.h"
#include "crc.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_config.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

/* dump full packet on RX error */
static int debug_dump;
#else
#define CPRINTF(format, args...)
const int debug_dump;
#endif

/* Control Message type */
enum {
	/* 0 Reserved */
	PD_CTRL_GOOD_CRC = 1,
	PD_CTRL_GOTO_MIN = 2,
	PD_CTRL_ACCEPT = 3,
	PD_CTRL_REJECT = 4,
	PD_CTRL_PING = 5,
	PD_CTRL_PS_RDY = 6,
	PD_CTRL_GET_SOURCE_CAP = 7,
	PD_CTRL_GET_SINK_CAP = 8,
	PD_CTRL_PROTOCOL_ERR = 9,
	PD_CTRL_SWAP = 10,
	/* 11 Reserved */
	PD_CTRL_WAIT = 12,
	PD_CTRL_SOFT_RESET = 13,
	/* 14-15 Reserved */
};

/* Data message type */
enum {
	/* 0 Reserved */
	PD_DATA_SOURCE_CAP = 1,
	PD_DATA_REQUEST = 2,
	PD_DATA_BIST = 3,
	PD_DATA_SINK_CAP = 4,
	/* 5-14 Reserved */
	PD_DATA_VENDOR_DEF = 15,
};

/* Protocol revision */
#define PD_REV10 0

/* Port role */
#define PD_ROLE_SINK   0
#define PD_ROLE_SOURCE 1

/* build message header */
#define PD_HEADER(type, role, id, cnt) \
	((type) | (PD_REV10 << 6) | \
	 ((role) << 8) | ((id) << 9) | ((cnt) << 12))

#define PD_HEADER_CNT(header)  (((header) >> 12) & 7)
#define PD_HEADER_TYPE(header) ((header) & 0xF)
#define PD_HEADER_ID(header)   (((header) >> 9) & 7)

/* Encode 5 bits using Biphase Mark Coding */
#define BMC(x)   ((x &  1 ? 0x001 : 0x3FF) \
		^ (x &  2 ? 0x004 : 0x3FC) \
		^ (x &  4 ? 0x010 : 0x3F0) \
		^ (x &  8 ? 0x040 : 0x3C0) \
		^ (x & 16 ? 0x100 : 0x300))

/* 4b/5b + Bimark Phase encoding */
static const uint16_t bmc4b5b[] = {
/* 0 = 0000 */ BMC(0x1E) /* 11110 */,
/* 1 = 0001 */ BMC(0x09) /* 01001 */,
/* 2 = 0010 */ BMC(0x14) /* 10100 */,
/* 3 = 0011 */ BMC(0x15) /* 10101 */,
/* 4 = 0100 */ BMC(0x0A) /* 01010 */,
/* 5 = 0101 */ BMC(0x0B) /* 01011 */,
/* 6 = 0110 */ BMC(0x0E) /* 01110 */,
/* 7 = 0111 */ BMC(0x0F) /* 01111 */,
/* 8 = 1000 */ BMC(0x12) /* 10010 */,
/* 9 = 1001 */ BMC(0x13) /* 10011 */,
/* A = 1010 */ BMC(0x16) /* 10110 */,
/* B = 1011 */ BMC(0x17) /* 10111 */,
/* C = 1100 */ BMC(0x1A) /* 11010 */,
/* D = 1101 */ BMC(0x1B) /* 11011 */,
/* E = 1110 */ BMC(0x1C) /* 11100 */,
/* F = 1111 */ BMC(0x1D) /* 11101 */,
/* Sync-1      K-code       11000 Startsynch #1 */
/* Sync-2      K-code       10001 Startsynch #2 */
/* RST-1       K-code       00111 Hard Reset #1 */
/* RST-2       K-code       11001 Hard Reset #2 */
/* EOP         K-code       01101 EOP End Of Packet */
/* Reserved    Error        00000 */
/* Reserved    Error        00001 */
/* Reserved    Error        00010 */
/* Reserved    Error        00011 */
/* Reserved    Error        00100 */
/* Reserved    Error        00101 */
/* Reserved    Error        00110 */
/* Reserved    Error        01000 */
/* Reserved    Error        01100 */
/* Reserved    Error        10000 */
/* Reserved    Error        11111 */
};
#define PD_SYNC1 0x18
#define PD_SYNC2 0x11
#define PD_RST1  0x07
#define PD_RST2  0x19
#define PD_EOP   0x0D

static const uint8_t dec4b5b[] = {
/* Error    */ 0x10 /* 00000 */,
/* Error    */ 0x10 /* 00001 */,
/* Error    */ 0x10 /* 00010 */,
/* Error    */ 0x10 /* 00011 */,
/* Error    */ 0x10 /* 00100 */,
/* Error    */ 0x10 /* 00101 */,
/* Error    */ 0x10 /* 00110 */,
/* RST-1    */ 0x13 /* 00111 K-code: Hard Reset #1 */,
/* Error    */ 0x10 /* 01000 */,
/* 1 = 0001 */ 0x01 /* 01001 */,
/* 4 = 0100 */ 0x04 /* 01010 */,
/* 5 = 0101 */ 0x05 /* 01011 */,
/* Error    */ 0x10 /* 01100 */,
/* EOP      */ 0x15 /* 01101 K-code: EOP End Of Packet */,
/* 6 = 0110 */ 0x06 /* 01110 */,
/* 7 = 0111 */ 0x07 /* 01111 */,
/* Error    */ 0x10 /* 10000 */,
/* Sync-2   */ 0x12 /* 10001 K-code: Startsynch #2 */,
/* 8 = 1000 */ 0x08 /* 10010 */,
/* 9 = 1001 */ 0x09 /* 10011 */,
/* 2 = 0010 */ 0x02 /* 10100 */,
/* 3 = 0011 */ 0x03 /* 10101 */,
/* A = 1010 */ 0x0A /* 10110 */,
/* B = 1011 */ 0x0B /* 10111 */,
/* Sync-1   */ 0x11 /* 11000 K-code: Startsynch #1 */,
/* RST-2    */ 0x14 /* 11001 K-code: Hard Reset #2 */,
/* C = 1100 */ 0x0C /* 11010 */,
/* D = 1101 */ 0x0D /* 11011 */,
/* E = 1110 */ 0x0E /* 11100 */,
/* F = 1111 */ 0x0F /* 11101 */,
/* 0 = 0000 */ 0x00 /* 11110 */,
/* Error    */ 0x10 /* 11111 */,
};

/* Start of Packet sequence : three Sync-1 K-codes, then one Sync-2 K-code */
#define PD_SOP (PD_SYNC1 | (PD_SYNC1<<5) | (PD_SYNC1<<10) | (PD_SYNC2<<15))

/* Hard Reset sequence : three RST-1 K-codes, then one RST-2 K-code */
#define PD_HARD_RESET (PD_RST1 | (PD_RST1 << 5) |\
		      (PD_RST1 << 10) | (PD_RST2 << 15))

/* PD counter definitions */
#define PD_MESSAGE_ID_COUNT 7
#define PD_RETRY_COUNT 2
#define PD_HARD_RESET_COUNT 2
#define PD_CAPS_COUNT 50

/* Timers */
#define PD_T_SEND_SOURCE_CAP (1500*MSEC) /* between 1s and 2s */
#define PD_T_GET_SOURCE_CAP  (1500*MSEC) /* between 1s and 2s */
#define PD_T_SOURCE_ACTIVITY   (45*MSEC) /* between 40ms and 50ms */
#define PD_T_SENDER_RESPONSE   (30*MSEC) /* between 24ms and 30ms */
#define PD_T_PS_TRANSITION    (220*MSEC) /* between 200ms and 220ms */

/* Port role at startup */
#ifdef CONFIG_USB_PD_DUAL_ROLE
#define PD_ROLE_DEFAULT PD_ROLE_SINK
#else
#define PD_ROLE_DEFAULT PD_ROLE_SOURCE
#endif

static struct pd_protocol {
	/* current port role */
	uint8_t role;
	/* 3-bit rolling message ID counter */
	uint8_t msg_id;
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t polarity;

	enum {
		PD_STATE_DISABLED,
#ifdef CONFIG_USB_PD_DUAL_ROLE
		PD_STATE_SNK_DISCONNECTED,
		PD_STATE_SNK_DISCOVERY,
		PD_STATE_SNK_REQUESTED,
		PD_STATE_SNK_TRANSITION,
		PD_STATE_SNK_READY,
#endif /* CONFIG_USB_PD_DUAL_ROLE */

		PD_STATE_SRC_DISCONNECTED,
		PD_STATE_SRC_DISCOVERY,
		PD_STATE_SRC_NEGOCIATE,
		PD_STATE_SRC_ACCEPTED,
		PD_STATE_SRC_TRANSITION,
		PD_STATE_SRC_READY,

		PD_STATE_HARD_RESET,
		PD_STATE_BIST,
	} task_state;
} pd[PD_PORT_COUNT];

static void pd_protocol_init(void)
{
	int i;

	/* Initialize PD protocol state variables for each port. */
	for (i = 0; i < PD_PORT_COUNT; i++) {
		pd[i].role = PD_ROLE_DEFAULT;
		pd[i].task_state = PD_DEFAULT_STATE;
	}

	/* Initialize TX pins and put them in Hi-Z */
	pd_tx_init();
}
DECLARE_HOOK(HOOK_INIT, pd_protocol_init, HOOK_PRIO_DEFAULT);

/* increment message ID counter */
static void inc_id(int port)
{
	pd[port].msg_id = (pd[port].msg_id + 1) & PD_MESSAGE_ID_COUNT;
}

static inline int encode_short(int port, int off, uint16_t val16)
{
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 0) & 0xF]);
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 4) & 0xF]);
	off = pd_write_sym(port, off, bmc4b5b[(val16 >> 8) & 0xF]);
	return pd_write_sym(port, off, bmc4b5b[(val16 >> 12) & 0xF]);
}

static inline int encode_word(int port, int off, uint32_t val32)
{
	off = encode_short(port, off, (val32 >> 0) & 0xFFFF);
	return encode_short(port, off, (val32 >> 16) & 0xFFFF);
}

/* prepare a 4b/5b-encoded PD message to send */
static int prepare_message(int port, uint16_t header, uint8_t cnt,
			   const uint32_t *data)
{
	int off, i;
	crc32_init();
	/* 64-bit preamble */
	off = pd_write_preamble(port);
	/* Start Of Packet: 3x Sync-1 + 1x Sync-2 */
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC1));
	off = pd_write_sym(port, off, BMC(PD_SYNC2));
	/* header */
	off = encode_short(port, off, header);
	crc32_hash16(header);
	/* data payload */
	for (i = 0; i < cnt; i++) {
		off = encode_word(port, off, data[i]);
		crc32_hash32(data[i]);
	}
	/* CRC */
	off = encode_word(port, off, crc32_result());
	/* End Of Packet */
	off = pd_write_sym(port, off, BMC(PD_EOP));
	/* Ensure that we have a final edge */
	return pd_write_last_edge(port, off);
}

static int analyze_rx(int port, uint32_t *payload);

static void send_hard_reset(int port)
{
	int off;

	/* 64-bit preamble */
	off = pd_write_preamble(port);
	/* Hard-Reset: 3x RST-1 + 1x RST-2 */
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST1));
	off = pd_write_sym(port, off, BMC(PD_RST2));
	/* Ensure that we have a final edge */
	off = pd_write_last_edge(port, off);
	/* Transmit the packet */
	pd_start_tx(port, pd[port].polarity, off);
	pd_tx_done(port, pd[port].polarity);
}

static int send_validate_message(int port, uint16_t header,
				 uint8_t cnt, const uint32_t *data)
{
	int r;
	static uint32_t payload[7];

	/* retry 3 times if we are not getting a valid answer */
	for (r = 0; r <= PD_RETRY_COUNT; r++) {
		int bit_len;
		uint16_t head;
		/* write the encoded packet in the transmission buffer */
		bit_len = prepare_message(port, header, cnt, data);
		/* Transmit the packet */
		pd_start_tx(port, pd[port].polarity, bit_len);
		pd_tx_done(port, pd[port].polarity);
		/* starting waiting for GoodCrc */
		pd_rx_start(port);
		/* read the incoming packet if any */
		head = analyze_rx(port, payload);
		pd_rx_complete(port);
		if (head > 0) { /* we got a good packet, analyze it */
			int type = PD_HEADER_TYPE(head);
			int nb = PD_HEADER_CNT(head);
			uint8_t id = PD_HEADER_ID(head);
			if (type == PD_CTRL_GOOD_CRC && nb == 0 &&
			   id == pd[port].msg_id) {
				/* got the GoodCRC we were expecting */
				inc_id(port);
				/* do not catch last edges as a new packet */
				udelay(10);
				return bit_len;
			} else {
				/* CPRINTF("ERR ACK/%d %04x\n", id, head); */
			}
		}
	}
	/* we failed all the re-transmissions */
	/* TODO: try HardReset */
	CPRINTF("TX NO ACK %04x/%d\n", header, cnt);
	return -1;
}

static int send_control(int port, int type)
{
	int bit_len;
	uint16_t header = PD_HEADER(type, pd[port].role,
			pd[port].msg_id, 0);

	bit_len = send_validate_message(port, header, 0, NULL);

	CPRINTF("CTRL[%d]>%d\n", type, bit_len);

	return bit_len;
}

static void send_goodcrc(int port, int id)
{
	uint16_t header = PD_HEADER(PD_CTRL_GOOD_CRC, pd[port].role, id, 0);
	int bit_len = prepare_message(port, header, 0, NULL);

	pd_start_tx(port, pd[port].polarity, bit_len);
	pd_tx_done(port, pd[port].polarity);
}

static int send_source_cap(int port)
{
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_SOURCE_CAP, pd[port].role,
			pd[port].msg_id, pd_src_pdo_cnt);

	bit_len = send_validate_message(port, header, pd_src_pdo_cnt,
					pd_src_pdo);
	CPRINTF("srcCAP>%d\n", bit_len);

	return bit_len;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
static void send_sink_cap(int port)
{
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_SINK_CAP, pd[port].role,
			pd[port].msg_id, pd_snk_pdo_cnt);

	bit_len = send_validate_message(port, header, pd_snk_pdo_cnt,
					pd_snk_pdo);
	CPRINTF("snkCAP>%d\n", bit_len);
}

static int send_request(int port, uint32_t rdo)
{
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_REQUEST, pd[port].role,
			pd[port].msg_id, 1);

	bit_len = send_validate_message(port, header, 1, &rdo);
	CPRINTF("REQ%d>\n", bit_len);

	return bit_len;
}
#endif /* CONFIG_USB_PD_DUAL_ROLE */

static int send_bist(int port)
{
	uint32_t bdo = BDO(BDO_MODE_TRANSMIT, 0);
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_BIST, pd[port].role,
			pd[port].msg_id, 1);

	bit_len = send_validate_message(port, header, 1, &bdo);
	CPRINTF("BIST>%d\n", bit_len);

	return bit_len;
}

static void handle_vdm_request(int port, int cnt, uint32_t *payload)
{
	uint16_t vid = PD_VDO_VID(payload[0]);
#ifdef CONFIG_USB_PD_CUSTOM_VDM
	int rlen;
	uint32_t *rdata;

	if (vid == USB_VID_GOOGLE) {
		rlen = pd_custom_vdm(port, cnt, payload, &rdata);
		if (rlen > 0) {
			uint16_t header = PD_HEADER(PD_DATA_VENDOR_DEF,
						pd[port].role, pd[port].msg_id,
						rlen);
			send_validate_message(port, header, rlen, rdata);
		}
		return;
	}
#endif
	CPRINTF("Unhandled VDM VID %04x CMD %04x\n",
		vid, payload[0] & 0xFFFF);
}

static void handle_data_request(int port, uint16_t head,
		uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);
	int cnt = PD_HEADER_CNT(head);

	switch (type) {
#ifdef CONFIG_USB_PD_DUAL_ROLE
	case PD_DATA_SOURCE_CAP:
		if ((pd[port].task_state == PD_STATE_SNK_DISCOVERY)
			|| (pd[port].task_state == PD_STATE_SNK_TRANSITION)) {
			uint32_t rdo;
			int res;
			/* we were waiting for them, let's process them */
			res = pd_choose_voltage(cnt, payload, &rdo);
			if (res >= 0) {
				res = send_request(port, rdo);
				if (res >= 0)
					pd[port].task_state =
							PD_STATE_SNK_REQUESTED;
				else
					/*
					 * for now: ignore failure here,
					 * we will retry ...
					 * TODO(crosbug.com/p/28332)
					 */
					pd[port].task_state =
							PD_STATE_SNK_REQUESTED;
			}
		}
		break;
#endif /* CONFIG_USB_PD_DUAL_ROLE */
	case PD_DATA_REQUEST:
		if ((pd[port].role == PD_ROLE_SOURCE) && (cnt == 1))
			if (!pd_request_voltage(payload[0])) {
				send_control(port, PD_CTRL_ACCEPT);
				pd[port].task_state = PD_STATE_SRC_ACCEPTED;
				return;
			}
		/* the message was incorrect or cannot be satisfied */
		send_control(port, PD_CTRL_REJECT);
		break;
	case PD_DATA_BIST:
		CPRINTF("BIST not supported\n");
		break;
	case PD_DATA_SINK_CAP:
		break;
	case PD_DATA_VENDOR_DEF:
		handle_vdm_request(port, cnt, payload);
		break;
	default:
		CPRINTF("Unhandled data message type %d\n", type);
	}
}

static void handle_ctrl_request(int port, uint16_t head,
		uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);

	switch (type) {
	case PD_CTRL_GOOD_CRC:
		/* should not get it */
		break;
	case PD_CTRL_PING:
		/* Nothing else to do */
		break;
	case PD_CTRL_GET_SOURCE_CAP:
		send_source_cap(port);
		break;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	case PD_CTRL_GET_SINK_CAP:
		send_sink_cap(port);
		break;
	case PD_CTRL_GOTO_MIN:
		break;
	case PD_CTRL_PS_RDY:
		if (pd[port].role == PD_ROLE_SINK)
			pd[port].task_state = PD_STATE_SNK_READY;
		break;
	case PD_CTRL_REJECT:
		pd[port].task_state = PD_STATE_SNK_DISCOVERY;
		break;
#endif /* CONFIG_USB_PD_DUAL_ROLE */
	case PD_CTRL_ACCEPT:
		break;
	case PD_CTRL_PROTOCOL_ERR:
	case PD_CTRL_SWAP:
	case PD_CTRL_WAIT:
	case PD_CTRL_SOFT_RESET:
	default:
		CPRINTF("Unhandled ctrl message type %d\n", type);
	}
}

static void handle_request(int port, uint16_t head,
		uint32_t *payload)
{
	int cnt = PD_HEADER_CNT(head);
	int p;

	if (PD_HEADER_TYPE(head) != 1 || cnt)
		send_goodcrc(port, PD_HEADER_ID(head));

	/* dump received packet content */
	CPRINTF("RECV %04x/%d ", head, cnt);
	for (p = 0; p < cnt; p++)
		CPRINTF("[%d]%08x ", p, payload[p]);
	CPRINTF("\n");

	if (cnt)
		handle_data_request(port, head, payload);
	else
		handle_ctrl_request(port, head, payload);
}

static inline int decode_short(int port, int off, uint16_t *val16)
{
	uint32_t w;
	int end;

	end = pd_dequeue_bits(port, off, 20, &w);

#if 0 /* DEBUG */
	CPRINTS("%d-%d: %05x %x:%x:%x:%x\n",
		off, end, w,
		dec4b5b[(w >> 15) & 0x1f], dec4b5b[(w >> 10) & 0x1f],
		dec4b5b[(w >>  5) & 0x1f], dec4b5b[(w >>  0) & 0x1f]);
#endif
	*val16 = dec4b5b[w & 0x1f] |
		(dec4b5b[(w >>  5) & 0x1f] << 4) |
		(dec4b5b[(w >> 10) & 0x1f] << 8) |
		(dec4b5b[(w >> 15) & 0x1f] << 12);
	return end;
}

static inline int decode_word(int port, int off, uint32_t *val32)
{
	off = decode_short(port, off, (uint16_t *)val32);
	return decode_short(port, off, ((uint16_t *)val32 + 1));
}

static int analyze_rx(int port, uint32_t *payload)
{
	int bit;
	char *msg = "---";
	uint32_t val = 0;
	uint16_t header;
	uint32_t pcrc, ccrc;
	int p, cnt;
	/* uint32_t eop; */

	crc32_init();
	pd_init_dequeue(port);

	/* Detect preamble */
	bit = pd_find_preamble(port);
	if (bit < 0) {
		msg = "Preamble";
		goto packet_err;
	}

	/* Find the Start Of Packet sequence */
	while (bit > 0) {
		bit = pd_dequeue_bits(port, bit, 20, &val);
		if (val == PD_SOP)
			break;
		/* TODO: detect SOP with 1 error code */
		/* TODO: detect Hard reset */
	}
	if (bit < 0) {
		msg = "SOP";
		goto packet_err;
	}

	/* read header */
	bit = decode_short(port, bit, &header);
	crc32_hash16(header);
	cnt = PD_HEADER_CNT(header);

	/* read payload data */
	for (p = 0; p < cnt && bit > 0; p++) {
		bit = decode_word(port, bit, payload+p);
		crc32_hash32(payload[p]);
	}
	if (bit < 0) {
		msg = "len";
		goto packet_err;
	}

	/* check transmitted CRC */
	bit = decode_word(port, bit, &pcrc);
	ccrc = crc32_result();
	if (bit < 0 || pcrc != ccrc) {
		msg = "CRC";
		if (pcrc != ccrc)
			bit = PD_ERR_CRC;
		/* DEBUG */CPRINTF("CRC %08x <> %08x\n", pcrc, crc32_result());
		goto packet_err;
	}

	/* check End Of Packet */
	/* SKIP EOP for now
	bit = pd_dequeue_bits(port, bit, 5, &eop);
	if (bit < 0 || eop != PD_EOP) {
		msg = "EOP";
		goto packet_err;
	}
	*/

	return header;
packet_err:
	if (debug_dump)
		pd_dump_packet(port, msg);
	else
		CPRINTF("RX ERR (%d)\n", bit);
	return bit;
}

static void execute_hard_reset(int port)
{
	pd[port].msg_id = 0;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	pd[port].task_state = pd[port].role == PD_ROLE_SINK ?
			PD_STATE_SNK_DISCONNECTED : PD_STATE_SRC_DISCONNECTED;
#else
	pd[port].task_state = PD_STATE_SRC_DISCONNECTED;
#endif
	pd_power_supply_reset(port);
	CPRINTF("HARD RESET!\n");
}

void pd_task(void)
{
	int head;
	int port = TASK_ID_TO_PORT(task_get_current());
	uint32_t payload[7];
	int timeout = 10*MSEC;
	int cc1_volt, cc2_volt;
	int res;

	/* Ensure the power supply is in the default state */
	pd_power_supply_reset(port);

	pd_hw_init(port);

	while (1) {
		/* monitor for incoming packet */
		pd_rx_enable_monitoring(port);
		/* Verify board specific health status : current, voltages... */
		res = pd_board_checks();
		if (res != EC_SUCCESS) {
			/* cut the power */
			execute_hard_reset(port);
			/* notify the other side of the issue */
			/* send_hard_reset(port); */
		}
		/* wait for next event/packet or timeout expiration */
		task_wait_event(timeout);
		/* incoming packet ? */
		if (pd_rx_started(port)) {
			head = analyze_rx(port, payload);
			pd_rx_complete(port);
			if (head > 0)
				handle_request(port,  head, payload);
			else if (head == PD_ERR_HARD_RESET)
				execute_hard_reset(port);
		}
		/* if nothing to do, verify the state of the world in 500ms */
		timeout = 500*MSEC;
		switch (pd[port].task_state) {
		case PD_STATE_DISABLED:
			/* Nothing to do */
			break;
		case PD_STATE_SRC_DISCONNECTED:
			/* Vnc monitoring */
			cc1_volt = pd_adc_read(port, 0);
			cc2_volt = pd_adc_read(port, 1);
			if ((cc1_volt < PD_SRC_VNC) ||
			    (cc2_volt < PD_SRC_VNC)) {
				pd[port].polarity = !(cc1_volt < PD_SRC_VNC);
				pd_select_polarity(port, pd[port].polarity);
				/* Enable VBUS */
				pd_set_power_supply_ready(port);
				pd[port].task_state = PD_STATE_SRC_DISCOVERY;
			}
			timeout = 10*MSEC;
			break;
		case PD_STATE_SRC_DISCOVERY:
			/* Query capabilites of the other side */
			res = send_source_cap(port);
			/* packet was acked => PD capable device) */
			if (res >= 0) {
				pd[port].task_state = PD_STATE_SRC_NEGOCIATE;
			} else { /* failed, retry later */
				timeout = PD_T_SEND_SOURCE_CAP;
			}
			break;
		case PD_STATE_SRC_NEGOCIATE:
			/* wait for a "Request" message */
			break;
		case PD_STATE_SRC_ACCEPTED:
			/* Accept sent, wait for the end of transition */
			timeout = PD_POWER_SUPPLY_TRANSITION_DELAY;
			pd[port].task_state = PD_STATE_SRC_TRANSITION;
			break;
		case PD_STATE_SRC_TRANSITION:
			res = pd_set_power_supply_ready(port);
			/* TODO error fallback */
			/* the voltage output is good, notify the source */
			res = send_control(port, PD_CTRL_PS_RDY);
			if (res >= 0) {
				timeout =  PD_T_SEND_SOURCE_CAP;
				/* it'a time to ping regularly the sink */
				pd[port].task_state = PD_STATE_SRC_READY;
			}
			/* TODO error fallback */
			break;
		case PD_STATE_SRC_READY:
			/* Verify that the sink is alive */
			res = send_control(port, PD_CTRL_PING);
			if (res < 0) {
				/* The sink died ... */
				pd_power_supply_reset(port);
				pd[port].task_state = PD_STATE_SRC_DISCOVERY;
				timeout = PD_T_SEND_SOURCE_CAP;
			} else { /* schedule next keep-alive */
				timeout = PD_T_SOURCE_ACTIVITY;
			}
			break;
#ifdef CONFIG_USB_PD_DUAL_ROLE
		case PD_STATE_SNK_DISCONNECTED:
			/* Source connection monitoring */
			cc1_volt = pd_adc_read(port, 0);
			cc2_volt = pd_adc_read(port, 1);
			if ((cc1_volt > PD_SNK_VA) ||
			    (cc2_volt > PD_SNK_VA)) {
				pd[port].polarity = !(cc1_volt > PD_SNK_VA);
				pd_select_polarity(port, pd[port].polarity);
				pd[port].task_state = PD_STATE_SNK_DISCOVERY;
			}
			timeout = 10*MSEC;
			break;
		case PD_STATE_SNK_DISCOVERY:
			/* For non-PD aware source, detect source disconnect */
			cc1_volt = pd_adc_read(port, pd[port].polarity);
			if (cc1_volt < PD_SNK_VA) {
				/* The source disappeared ... */
				pd[port].task_state = PD_STATE_SNK_DISCONNECTED;
				/* Debouncing */
				timeout = 50*MSEC;
				break;
			}

			res = send_control(port, PD_CTRL_GET_SOURCE_CAP);
			/* packet was acked => PD capable device) */
			if (res >= 0) {
				/*
				 * we should a SOURCE_CAP package which will
				 * switch to the PD_STATE_SNK_REQUESTED state,
				 * else retry after the response timeout.
				 */
				timeout = PD_T_SENDER_RESPONSE;
			} else { /* failed, retry later */
				timeout = PD_T_GET_SOURCE_CAP;
			}
			break;
		case PD_STATE_SNK_REQUESTED:
			/* Ensure the power supply actually becomes ready */
			pd[port].task_state = PD_STATE_SNK_TRANSITION;
			timeout = PD_T_PS_TRANSITION;
			break;
		case PD_STATE_SNK_TRANSITION:
			/*
			 * did not get the PS_READY,
			 * try again to whole request cycle.
			 */
			pd[port].task_state = PD_STATE_SNK_DISCOVERY;
			timeout = 10*MSEC;
			break;
		case PD_STATE_SNK_READY:
			/* we have power and we are happy */

			/* if we have lost vbus, go back to disconnected */
			if (!pd_snk_is_vbus_provided(port)) {
				pd[port].task_state = PD_STATE_SNK_DISCONNECTED;
				/* set timeout small to reconnect fast */
				timeout = 5*MSEC;
				break;
			}

			/* check vital parameters from time to time */
			timeout = 100*MSEC;
			break;
#endif /* CONFIG_USB_PD_DUAL_ROLE */
		case PD_STATE_HARD_RESET:
			send_hard_reset(port);
			/* reset our own state machine */
			execute_hard_reset(port);
			break;
		case PD_STATE_BIST:
			send_bist(port);
			pd[port].task_state = PD_STATE_DISABLED;
			break;
		}
	}
}

void pd_rx_event(int port)
{
	task_set_event(PORT_TO_TASK_ID(port), PD_EVENT_RX, 0);
}

#ifdef CONFIG_COMMON_RUNTIME
void pd_request_source_voltage(int port, int mv)
{
	pd_set_max_voltage(mv);
	pd[port].role = PD_ROLE_SINK;
	pd_set_host_mode(port, 0);
	pd[port].task_state = PD_STATE_SNK_DISCONNECTED;
	task_wake(PORT_TO_TASK_ID(port));
}

static int command_pd(int argc, char **argv)
{
	int port;
	char *e;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	port = strtoi(argv[2], &e, 10);
	if (*e || port >= PD_PORT_COUNT)
		return EC_ERROR_PARAM2;

	if (!strcasecmp(argv[1], "tx")) {
		pd[port].task_state = PD_STATE_SNK_DISCOVERY;
		task_wake(PORT_TO_TASK_ID(port));
	} else if (!strcasecmp(argv[1], "bist")) {
		pd[port].task_state = PD_STATE_BIST;
		task_wake(PORT_TO_TASK_ID(port));
	} else if (!strcasecmp(argv[1], "charger")) {
		pd[port].role = PD_ROLE_SOURCE;
		pd_set_host_mode(port, 1);
		pd[port].task_state = PD_STATE_SRC_DISCONNECTED;
		task_wake(PORT_TO_TASK_ID(port));
	} else if (!strncasecmp(argv[1], "dev", 3)) {
		int max_volt = -1;
		if (argc >= 3)
			max_volt = strtoi(argv[3], &e, 10) * 1000;

		pd_request_source_voltage(port, max_volt);
	} else if (!strcasecmp(argv[1], "clock")) {
		int freq;

		if (argc < 3)
			return EC_ERROR_PARAM2;

		freq = strtoi(argv[3], &e, 10);
		if (*e)
			return EC_ERROR_PARAM2;
		pd_set_clock(port, freq);
		ccprintf("set TX frequency to %d Hz\n", freq);
	} else if (!strcasecmp(argv[1], "dump")) {
		debug_dump = !debug_dump;
	} else if (!strncasecmp(argv[1], "hard", 4)) {
		pd[port].task_state = PD_STATE_HARD_RESET;
		task_wake(PORT_TO_TASK_ID(port));
	} else if (!strncasecmp(argv[1], "ping", 4)) {
		pd[port].role = PD_ROLE_SOURCE;
		pd_set_host_mode(port, 1);
		pd[port].task_state = PD_STATE_SRC_READY;
		task_wake(PORT_TO_TASK_ID(port));
	} else if (!strncasecmp(argv[1], "state", 5)) {
		const char * const state_names[] = {
			"DISABLED",
			"SNK_DISCONNECTED", "SNK_DISCOVERY", "SNK_REQUESTED",
			"SNK_TRANSITION", "SNK_READY",
			"SRC_DISCONNECTED", "SRC_DISCOVERY", "SRC_NEGOCIATE",
			"SRC_ACCEPTED", "SRC_TRANSITION", "SRC_READY",
			"HARD_RESET", "BIST",
		};
		ccprintf("Port C%d - Role: %s Polarity: CC%d State: %s\n",
			port, pd[port].role == PD_ROLE_SOURCE ? "SRC" : "SNK",
			pd[port].polarity + 1,
			state_names[pd[port].task_state]);
	} else {
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pd, command_pd,
			"[tx|bist|charger|dev|dump|hard|clock|ping|state] port",
			"USB PD",
			NULL);
#endif /* CONFIG_COMMON_RUNTIME */
