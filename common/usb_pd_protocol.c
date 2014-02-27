/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

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

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

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
#define PD_HARD_RESET (PD_RST1 | (PD_RST1 << 5) | (PD_RST1 << 10) | (PD_RST2 << 15))

/* PDO : Power Data Object */
/*
 * 1. The vSafe5V Fixed Supply Object shall always be the first object.
 * 2. The remaining Fixed Supply Objects,
 *    if present, shall be sent in voltage order; lowest to highest.
 * 3. The Battery Supply Objects,
 *    if present shall be sent in Minimum Voltage order; lowest to highest.
 * 4. The Variable Supply (non battery) Objects,
 *    if present, shall be sent in Minimum Voltage order; lowest to highest.
 */
#define PDO_TYPE_FIXED    (0 << 30)
#define PDO_TYPE_BATTERY  (1 << 30)
#define PDO_TYPE_VARIABLE (2 << 30)

#define PDO_FIXED_DUAL_ROLE (1 << 29) /* Dual role device */
#define PDO_FIXED_SUSPEND   (1 << 28) /* USB Suspend supported */
#define PDO_FIXED_EXTERNAL  (1 << 27) /* Externally powered */
#define PDO_FIXED_COMM_CAP  (1 << 26) /* USB Communications Capable */
#define PDO_FIXED_PEAK_CURR () /* [21..20] Peak current */
#define PDO_FIXED_VOLT(mv)  (((mv)/50) << 20) /* Voltage in 50mV units */
#define PDO_FIXED_CURR(ma)  (((ma)/10) << 0)  /* Maximum current in 10mV units */

#define PDO_FIXED(mv, ma, flags) (PDO_FIXED_VOLT(mv) |\
				  PDO_FIXED_CURR(ma) | (flags))

#define PDO_VAR_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_VAR_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 0)

#define PDO_VAR(min_mv, max_mv, op_ma) \
				(PDO_VAR_MIN_VOLT(min_mv) | \
				 PDO_VAR_MAX_VOLT(max_mv) | \
				 PDO_VAR_OP_CURR(op_ma))

#define PDO_BATT_MAX_VOLT(mv) ((((mv) / 50) & 0x3FF) << 20)
#define PDO_BATT_MIN_VOLT(mv) ((((mv) / 50) & 0x3FF) << 10)
#define PDO_BATT_OP_POWER(mw) ((((mw) / 10) & 0x3FF) << 0)

#define PDO_BATT(min_mv, max_mv, op_mw) \
				(PDO_BATT_MIN_VOLT(min_mv) | \
				 PDO_BATT_MAX_VOLT(max_mv) | \
				 PDO_BATT_OP_POWER(op_mw))

/* RDO : Request Data Object */
#define RDO_OBJ_POS(n)             (((n) & 0x7) << 28)
#define RDO_GIVE_BACK              (1 << 27)
#define RDO_CAP_MISMATCH           (1 << 26)
#define RDO_COMM_CAP               (1 << 25)
#define RDO_NO_SUSPEND             (1 << 24)
#define RDO_FIXED_VAR_OP_CURR(ma)  ((((ma) / 10) & 0x3FF) << 10)
#define RDO_FIXED_VAR_MAX_CURR(ma) ((((ma) / 10) & 0x3FF) << 0)

#define RDO_BATT_OP_POWER(mw)      ((((mw) / 250) & 0x3FF) << 10)
#define RDO_BATT_MAX_POWER(mw)     ((((mw) / 250) & 0x3FF) << 10)

#define RDO_FIXED(n, op_ma, max_ma, flags) \
				(RDO_OBJ_POS(n) | (flags) | \
				RDO_FIXED_VAR_OP_CURR(op_ma) | \
				RDO_FIXED_VAR_MAX_CURR(max_ma))


#define RDO_BATT(n, op_mw, max_mw, flags) \
				(RDO_OBJ_POS(n) | (flags) | \
				RDO_BATT_OP_POWERCURR(op_mw) | \
				RDO_BATT_MAX_POWER(max_mw))

/* BDO : BIST Data Object */
#define BDO_MODE_RECV       (0 << 28)
#define BDO_MODE_TRANSMIT   (1 << 28)
#define BDO_MODE_COUNTERS   (2 << 28)
#define BDO_MODE_CARRIER0   (3 << 28)
#define BDO_MODE_CARRIER1   (4 << 28)
#define BDO_MODE_CARRIER2   (5 << 28)
#define BDO_MODE_CARRIER3   (6 << 28)
#define BDO_MODE_EYE        (7 << 28)

#define BDO(mode,cnt)       ((mode) | ((cnt) & 0xFFFF))

/* VDO : Vendor Defined Message Object */
#define VDO(vid, custom) (((vid) << 16) | ((custom) & 0xFFFF))

/* current port role */
static uint8_t pd_role;
/* 3-bit rolling message ID counter */
static uint8_t pd_message_id;

static enum {
	PD_STATE_IDLE,
	PD_STATE_TX,
	PD_STATE_RX,
} pd_task_state;

/* increment message ID counter */
static void inc_id(void)
{
	pd_message_id = (pd_message_id + 1) & 0x7;
}

static inline int encode_short(void *ctxt, int off, uint16_t val16)
{
	off = pd_write_sym(ctxt, off, bmc4b5b[(val16 >> 0) & 0xF]);
	off = pd_write_sym(ctxt, off, bmc4b5b[(val16 >> 4) & 0xF]);
	off = pd_write_sym(ctxt, off, bmc4b5b[(val16 >> 8) & 0xF]);
	return pd_write_sym(ctxt, off, bmc4b5b[(val16 >> 12) & 0xF]);
}

static inline int encode_word(void *ctxt, int off, uint32_t val32)
{
	off = encode_short(ctxt, off, (val32 >> 0) & 0xFFFF);
	return encode_short(ctxt, off, (val32 >> 16) & 0xFFFF);
}

/* prepare a 4b/5b-encoded PD message to send */
static int prepare_message(void *ctxt, uint16_t header, uint8_t cnt,
			   uint32_t *data)
{
	int off, i;
	crc32_init();
	/* 64-bit preamble */
	off = pd_write_preamble(ctxt);
	/* Start Of Packet: 3x Sync-1 + 1x Sync-2 */
	off = pd_write_sym(ctxt, off, BMC(PD_SYNC1));
	off = pd_write_sym(ctxt, off, BMC(PD_SYNC1));
	off = pd_write_sym(ctxt, off, BMC(PD_SYNC1));
	off = pd_write_sym(ctxt, off, BMC(PD_SYNC2));
	/* header */
	off = encode_short(ctxt, off, header);
	crc32_hash16(header);
	/* data payload */
	for (i = 0; i < cnt; i++) {
		off = encode_word(ctxt, off, data[i]);
		crc32_hash32(data[i]);
	}
	/* CRC */
	off = encode_word(ctxt, off, crc32_result());
	/* End Of Packet */
	return pd_write_sym(ctxt, off, BMC(PD_EOP));
}

static void send_control(void *ctxt, int type)
{
	int bit_len;
	uint16_t header = PD_HEADER(type, pd_role, pd_message_id, 0);

	bit_len = prepare_message(ctxt, header, 0, NULL);

	pd_start_tx(ctxt, bit_len);
	CPRINTF("CTRL[%d]>%d",type,bit_len);
	pd_tx_done();
	CPRINTF("Done\n");
}

static void send_goodcrc(void *ctxt, int id)
{
	uint16_t header = PD_HEADER(PD_CTRL_GOOD_CRC, pd_role, id, 0);
	int bit_len = prepare_message(ctxt, header, 0, NULL);

	pd_start_tx(ctxt, bit_len);
	pd_tx_done();
}

static void send_source_cap(void *ctxt)
{
	uint32_t pdo[] = {
		PDO_FIXED( 5000,  500, PDO_FIXED_EXTERNAL),
		PDO_FIXED( 5000, 3000, 0),
		PDO_FIXED(12000, 3000, 0),
		PDO_FIXED(20000, 2000, 0),
	};
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_SOURCE_CAP, pd_role, pd_message_id,
				    ARRAY_SIZE(pdo));

	bit_len = prepare_message(ctxt, header, ARRAY_SIZE(pdo), pdo);

	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 1);

	/* Kick off the DMA to send the data */
	CPRINTF("srcCAP%d>",bit_len);
	pd_start_tx(ctxt, bit_len);
	pd_tx_done();
	CPRINTF("+OK\n");
	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 0);
}

static void send_sink_cap(void *ctxt)
{
	uint32_t pdo[] = {
		PDO_BATT( 4500,  5500, 15000),
		PDO_BATT(11500, 12500, 36000),
	};
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_SINK_CAP, pd_role, pd_message_id,
				    ARRAY_SIZE(pdo));

	bit_len = prepare_message(ctxt, header, ARRAY_SIZE(pdo), pdo);

	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 1);

	/* Kick off the DMA to send the data */
	CPRINTF("snkCAP%d>",bit_len);
	pd_start_tx(ctxt, bit_len);
	pd_tx_done();
	CPRINTF("+OK\n");
	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 0);
}

static void send_request(void *ctxt)
{
	uint32_t rdo = RDO_FIXED(1, 500, 500, 0);
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_REQUEST, pd_role, pd_message_id, 1);

	bit_len = prepare_message(ctxt, header, 1, &rdo);

	/* Kick off the DMA to send the data */
	CPRINTF("REQ%d>",bit_len);
	pd_start_tx(ctxt, bit_len);
	pd_tx_done();
	CPRINTF("+OK\n");
}

static void send_bist(void *ctxt)
{
	uint32_t bdo = BDO(BDO_MODE_TRANSMIT, 0);
	int bit_len;
	uint16_t header = PD_HEADER(PD_DATA_BIST, pd_role, pd_message_id, 1);

	bit_len = prepare_message(ctxt, header, 1, &bdo);

	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 1);

	/* Kick off the DMA to send the data */
	CPRINTF("BIST%d>",bit_len);
	pd_start_tx(ctxt, bit_len);
	pd_tx_done();
	CPRINTF("Done\n");
	/* DEBUG */gpio_set_level(GPIO_LED_GREEN, 0);
}
extern void enable_rx_monitoring(void);

static void handle_request(void *ctxt, uint16_t head, uint32_t *payload)
{
	int type = PD_HEADER_TYPE(head);
	int cnt = PD_HEADER_CNT(head);
	int p;

	if (type != 1 || cnt)
		send_goodcrc(ctxt, PD_HEADER_ID(head));

	/* dump received packet content */
	CPRINTF("RECV %04x/%d ", head, cnt);
	for (p = 0; p < cnt; p++)
		CPRINTF("[%d]%08x ", p, payload[p]);
	CPRINTF("\n");

	switch(type)
	{
	case PD_CTRL_GOOD_CRC:
		break;
	case 0:
	case 11:
	case 14:
	case PD_DATA_VENDOR_DEF:
		CPRINTF("Invalid message type %d\n", type);
		break;
	case PD_CTRL_PING:
		/* Nothing else to do */
		break;
	case PD_CTRL_GET_SOURCE_CAP:
		send_source_cap(ctxt);
		break;
	case PD_CTRL_GET_SINK_CAP:
		send_sink_cap(ctxt);
		break;
	case PD_CTRL_GOTO_MIN:
	case PD_CTRL_ACCEPT:
	case PD_CTRL_REJECT:
#if 0
	PD_DATA_SOURCE_CAP = 1,
	PD_DATA_REQUEST = 2,
	PD_DATA_BIST = 3,
	PD_DATA_SINK_CAP = 4,
#endif
	case PD_CTRL_PS_RDY:
	case PD_CTRL_PROTOCOL_ERR:
	case PD_CTRL_SWAP:
	case PD_CTRL_WAIT:
	case PD_CTRL_SOFT_RESET:
		CPRINTF("Unhandled message type %d\n", type);
		break;
	}
	if (type == 1 && !cnt) { /* Got a good CRC */
		inc_id();
		enable_rx_monitoring();
	}
}

static inline int decode_short(void *ctxt, int off, uint16_t *val16)
{
	uint32_t w;
	int end;

	end = pd_dequeue_bits(ctxt, off, 20, &w);

#if 0 /* DEBUG */
	CPRINTF("%d-%d: %05x %x:%x:%x:%x\n",
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

static inline int decode_word(void *ctxt, int off, uint32_t *val32)
{
	off = decode_short(ctxt, off, (uint16_t *)val32);
	return decode_short(ctxt, off, ((uint16_t *)val32 + 1));
}

static int analyze_rx(uint32_t *payload)
{
	int bit;
	char *msg = "---";
	uint32_t val = 0;
	uint16_t header;
	uint32_t pcrc;
	int p, cnt;
	//uint32_t eop;
	void *ctxt;

	crc32_init();
	ctxt = pd_init_dequeue();

	/* Detect preamble */
	bit = pd_find_preamble(ctxt);
	if (bit < 0) {
		msg = "Preamble";
		goto packet_err;
	}

	/* Find the Start Of Packet sequence */
	while (bit > 0) {
		bit = pd_dequeue_bits(ctxt, bit, 20, &val);
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
	bit = decode_short(ctxt, bit, &header);
	crc32_hash16(header);
	cnt = PD_HEADER_CNT(header);

	/* read payload data */
	for (p = 0; p < cnt && bit > 0; p++) {
		bit = decode_word(ctxt, bit, payload+p);
		crc32_hash32(payload[p]);
	}
	if (bit < 0) {
		msg = "len";
		goto packet_err;
	}

	/* check transmitted CRC */
	bit = decode_word(ctxt, bit, &pcrc);
	if (bit < 0 || pcrc != crc32_result()) {
		msg = "CRC";
		/* DEBUG */CPRINTF("CRC %08x <> %08x\n", pcrc, crc32_result());
		goto packet_err;
	}

	/* check End Of Packet */
	/* SKIP EOP for now
	bit = pd_dequeue_bits(ctxt, bit, 5, &eop);
	if (bit < 0 || eop != PD_EOP) {
		msg = "EOP";
		goto packet_err;
	}
	*/

	return header;
packet_err:
	pd_dump_packet(ctxt, msg);
	return -1;
}

void pd_task(void)
{
	int head;
	void *ctxt = pd_hw_init();
	uint32_t payload[7];

	while(1) {
		task_wait_event(-1);
		switch (pd_task_state)
		{
		case PD_STATE_TX:
			send_control(ctxt, PD_CTRL_GET_SOURCE_CAP);
			if (0) {
			send_source_cap(ctxt);
			send_bist(ctxt);
			send_request(ctxt);
			send_sink_cap(ctxt);
			}
			pd_task_state = PD_STATE_IDLE;
			break;
		case PD_STATE_RX:
			head = analyze_rx(payload);
			pd_task_state = PD_STATE_IDLE;
			pd_rx_complete();
			if (head > 0)
				handle_request(ctxt, head, payload);
			break;
		case PD_STATE_IDLE:
			break;
		}
	}
}

void pd_rx_event(void)
{
	pd_task_state = PD_STATE_RX;
	task_wake(TASK_ID_PD);
}

void button_event(enum gpio_signal signal)
{
	int level = gpio_get_level(GPIO_USER_BUTTON);

	gpio_set_level(GPIO_LED_BLUE, level);
	if (level) {
		pd_task_state = PD_STATE_TX;
		task_wake(TASK_ID_PD);
	}
}

static int command_pd(int argc, char **argv)
{
	pd_task_state = PD_STATE_TX;
	task_wake(TASK_ID_PD);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pd, command_pd,
                        "none",
                        "USB PD",
                        NULL);

static int command_tx(int argc, char **argv)
{
	inc_id();

	pd_task_state = PD_STATE_TX;
	task_wake(TASK_ID_PD);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(tx, command_tx,
                        "none",
                        "send fake packet",
                        NULL);

static int command_rx(int argc, char **argv)
{
	pd_rx_event();

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rx, command_rx,
                        "none",
                        "fake RX",
                        NULL);
