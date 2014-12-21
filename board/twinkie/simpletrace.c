/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "injector.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "usb_pd_config.h"
#include "util.h"

int trace_mode;

static const char * const ctrl_msg_name[] = {
	[0]                      = "RSVD-C0",
	[PD_CTRL_GOOD_CRC]       = "GOODCRC",
	[PD_CTRL_GOTO_MIN]       = "GOTOMIN",
	[PD_CTRL_ACCEPT]         = "ACCEPT",
	[PD_CTRL_REJECT]         = "REJECT",
	[PD_CTRL_PING]           = "PING",
	[PD_CTRL_PS_RDY]         = "PSRDY",
	[PD_CTRL_GET_SOURCE_CAP] = "GSRCCAP",
	[PD_CTRL_GET_SINK_CAP]   = "GSNKCAP",
	[PD_CTRL_DR_SWAP]        = "DRSWAP",
	[PD_CTRL_PR_SWAP]        = "PRSWAP",
	[PD_CTRL_VCONN_SWAP]     = "VCONNSW",
	[PD_CTRL_WAIT]           = "WAIT",
	[PD_CTRL_SOFT_RESET]     = "SFT-RST",
	[14]                     = "RSVD-C14",
	[15]                     = "RSVD-C15",
};

static const char * const data_msg_name[] = {
	[0]                      = "RSVD-D0",
	[PD_DATA_SOURCE_CAP]     = "SRCCAP",
	[PD_DATA_REQUEST]        = "REQUEST",
	[PD_DATA_BIST]           = "BIST",
	[PD_DATA_SINK_CAP]       = "SNKCAP",
	/* 5-14 Reserved */
	[PD_DATA_VENDOR_DEF]     = "VDM",
};

static const char * const svdm_cmd_name[] = {
	[CMD_DISCOVER_IDENT]     = "DISCID",
	[CMD_DISCOVER_SVID]	 = "DISCSVID",
	[CMD_DISCOVER_MODES]	 = "DISCMODE",
	[CMD_ENTER_MODE]	 = "ENTER",
	[CMD_EXIT_MODE]		 = "EXIT",
	[CMD_ATTENTION]		 = "ATTN",
	[CMD_DP_STATUS]          = "DPSTAT",
	[CMD_DP_CONFIG]          = "DPCFG",
};

static const char * const svdm_cmdt_name[] = {
	[CMDT_INIT]     = "INI",
	[CMDT_RSP_ACK]  = "ACK",
	[CMDT_RSP_NAK]  = "NAK",
	[CMDT_RSP_BUSY] = "BSY",
};

static void print_pdo(uint32_t word)
{
	if ((word & PDO_TYPE_MASK) == PDO_TYPE_BATTERY)
		ccprintf(" %dmV/%dmW:%08x", ((word>>10)&0x3ff)*50,
			 (word&0x3ff)*250, word);
	else
		ccprintf(" %dmV/%dmA:%08x", ((word>>10)&0x3ff)*50,
			 (word&0x3ff)*10, word);
}

static void print_rdo(uint32_t word)
{
	int pos = (word >> 28) & 7;
	ccprintf("[%d] %08x", pos, word);
}

static void print_vdo(int idx, uint32_t word)
{
	if (idx == 0 && (word & VDO_SVDM_TYPE)) {
		const char *cmd = svdm_cmd_name[PD_VDO_CMD(word)];
		const char *cmdt = svdm_cmdt_name[PD_VDO_CMDT(word)];
		uint16_t vid = PD_VDO_VID(word);
		if (!cmd)
			cmd = "????";
		ccprintf(" V%04x:%s,%s:%08x", vid, cmd, cmdt, word);
	} else {
		ccprintf(" %08x", word);
	}
}

static void print_packet(int head, uint32_t *payload)
{
	int i;
	int cnt = PD_HEADER_CNT(head);
	int typ = PD_HEADER_TYPE(head);
	int id = PD_HEADER_ID(head);
	const char *name;
	const char *prole;

	if (trace_mode == TRACE_MODE_RAW) {
		ccprintf("%T %04x", head);
		for (i = 0; i < cnt; i++)
			ccprintf(" %08x", payload[i]);
		ccputs("\n");
		return;
	}
	name = cnt ? data_msg_name[typ] : ctrl_msg_name[typ];
	prole = head & (PD_ROLE_SOURCE << 8) ? "SRC" : "SNK";
	ccprintf("%T %s/%d %s[%04x]", prole, id, name);
	if (!cnt) { /* Control message : we are done */
		ccputs("\n");
		return;
	}
	/* Print payload for data message */
	for (i = 0; i < cnt; i++)
		switch (typ) {
		case PD_DATA_SOURCE_CAP:
			print_pdo(payload[i]);
			break;
		case PD_DATA_REQUEST:
			print_rdo(payload[i]);
			break;
		case PD_DATA_BIST:
			ccprintf("mode %d cnt %04x", payload[i] >> 28,
				 payload[i] & 0xffff);
			break;
		case PD_DATA_SINK_CAP:
			print_pdo(payload[i]);
			break;
		case PD_DATA_VENDOR_DEF:
			print_vdo(i, payload[i]);
			break;
		default:
			ccprintf(" %08x", payload[i]);
	}
	ccputs("\n");
}

static void print_error(int err)
{
	if (err == -1)
		ccprintf("%T TMOUT\n");
	else if (err == -2)
		ccprintf("%T HARD-RST\n");
	else if (err == -5)
		ccprintf("%T SOP*\n");
	else
		ccprintf("ERR %d\n", err);
}

/* keep track of RX edge timing in order to trigger receive */
static timestamp_t rx_edge_ts[2][PD_RX_TRANSITION_COUNT];
static int rx_edge_ts_idx[2];

void rx_event(void)
{
	int pending, i;
	int next_idx;
	pending = STM32_EXTI_PR;

	for (i = 0; i < PD_PORT_COUNT; i++) {
		if (pending & EXTI_COMP_MASK(i)) {
			rx_edge_ts[i][rx_edge_ts_idx[i]].val = get_time().val;
			next_idx = (rx_edge_ts_idx[i] ==
					PD_RX_TRANSITION_COUNT - 1) ?
						0 : rx_edge_ts_idx[i] + 1;

			/*
			 * If we have seen enough edges in a certain amount of
			 * time, then trigger RX start.
			 */
			if ((rx_edge_ts[i][rx_edge_ts_idx[i]].val -
			     rx_edge_ts[i][next_idx].val)
			     < PD_RX_TRANSITION_WINDOW) {
				/* start sampling */
				pd_rx_start(i);
				/*
				 * ignore the comparator IRQ until we are done
				 * with current message
				 */
				pd_rx_disable_monitoring(i);
				/* trigger the analysis in the task */
				task_set_event(TASK_ID_SNIFFER, i, 0);
			} else {
				/* do not trigger RX start, just clear int */
				STM32_EXTI_PR = EXTI_COMP_MASK(i);
			}
			rx_edge_ts_idx[i] = next_idx;
		}
	}
}
DECLARE_IRQ(STM32_IRQ_COMP, rx_event, 1);

int analyze_rx(int port, uint32_t *payload);

void trace_packets(void)
{
	int head;
	uint32_t payload[7];

	/* Disable sniffer DMA configuration */
	dma_disable(STM32_DMAC_CH6);
	dma_disable(STM32_DMAC_CH7);
	task_disable_irq(STM32_IRQ_DMA_CHANNEL_4_7);
	/* remove TIM1 CH1/2/3 DMA remapping */
	STM32_SYSCFG_CFGR1 &= ~(1 << 28);

	/* "classical" PD RX configuration */
	pd_hw_init_rx(0);
	/* Enable the RX interrupts */
	pd_rx_enable_monitoring(0);

	while (1) {
		task_wait_event(-1);
		if (trace_mode == TRACE_MODE_OFF)
			break;
		/* incoming packet processing */
		head = pd_analyze_rx(0, payload);
		pd_rx_complete(0);
		pd_rx_enable_monitoring(0);
		if (head > 0)
			print_packet(head, payload);
		else
			print_error(head);
	}

	task_disable_irq(STM32_IRQ_COMP);
	/* Disable tracer DMA configuration */
	dma_disable(STM32_DMAC_CH2);
	/* Put back : sniffer RX hardware configuration */
	sniffer_init();
}

void set_trace_mode(int mode)
{
	/* No change */
	if (mode == trace_mode)
		return;

	trace_mode = mode;
	/* kick the task to take into account the new value */
	task_wake(TASK_ID_SNIFFER);
}
