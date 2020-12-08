/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Honeybuns family-specific configuration */
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "usb_pd.h"
#include "util.h"
#include "system.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

enum hpd_state {
	LOW_WAIT,
	HIGH_CHECK,
	HIGH_WAIT,
	IRQ_CHECK,
};

#define HPD_QUEUE_DEPTH BIT(2)
#define HPD_QUEUE_MASK (HPD_QUEUE_DEPTH - 1)
#define HPD_T_IRQ_MAX_PULSE (2 * MSEC)
#define HPD_T_MIN_DP_ATTEN (10 * MSEC)
#define HPD_GPIO_SIGNAL GPIO_DDI_MST_IN_HPD

struct hpd_mark {
	int level;
	uint64_t ts;
};

struct hpd_edge {
	uint32_t head;
	uint32_t tail;
	struct hpd_mark buffer[HPD_QUEUE_DEPTH];
};

struct hpd_info {
	enum hpd_state state;
	enum gpio_signal signal;
	int enable;
	int count;
	uint64_t timer;
	uint64_t last_send_ts;
	enum hpd_event queue[HPD_QUEUE_DEPTH];
	struct hpd_edge edges;
};

#ifdef SECTION_IS_RW
static struct hpd_info hpd;
#endif
/******************************************************************************/

static void board_power_sequence(void)
{
	int i;

	for(i = 0; i < board_power_seq_count; i++) {
		gpio_set_level(board_power_seq[i].signal,
			       board_power_seq[i].level);
		if (board_power_seq[i].delay_ms)
			msleep(board_power_seq[i].delay_ms);
	}
}

/******************************************************************************/
/* I2C port map configuration */
const struct i2c_port_t i2c_ports[] = {
	{"i2c1",  I2C_PORT_I2C1,  400, GPIO_EC_I2C1_SCL, GPIO_EC_I2C1_SDA},
#ifndef BOARD_P1
	{"i2c2",  I2C_PORT_I2C2,  400, GPIO_EC_I2C2_SCL, GPIO_EC_I2C2_SDA},
#endif
	{"i2c3",  I2C_PORT_I2C3,  400, GPIO_EC_I2C3_SCL, GPIO_EC_I2C3_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifndef SECTION_IS_RW
static void baseboard_set_usbc_sink_mode(void)
{
	uint32_t cr;

	/*
	 * Bare minimum code required to enable UCPD peripheral and apply Rd to
	 * the CC lines. This is only applied in RO.
	 */
	/* Ensure that clock to UCPD is enabled */
	STM32_RCC_APB1ENR2 |= STM32_RCC_APB1ENR2_UPCD1EN;
	/* enable the peripheral */
	STM32_UCPD_CFGR1(0) |= STM32_UCPD_CFGR1_UCPDEN;

	cr = STM32_UCPD_CR(0);
	/* Apply Rd to both CC lines */
	cr |= STM32_UCPD_CR_ANAMODE | STM32_UCPD_CR_CCENABLE_MASK;
	STM32_UCPD_CR(0) = cr;

	CPRINTS("usbc: CR = 0x%x", STM32_UCPD_CR(0));
}
#endif

static void baseboard_init(void)
{
	/* Turn on power rails */
	board_power_sequence();
	CPRINTS("board: Power rails enabled");
#ifdef SECTION_IS_RW
	system_clear_reset_flags(EC_RESET_FLAG_POWER_ON);
	system_set_reset_flags(EC_RESET_FLAG_EFS);
#else
	baseboard_set_usbc_sink_mode();
#endif
}
DECLARE_HOOK(HOOK_INIT, baseboard_init, HOOK_PRIO_INIT_I2C + 1);

#ifdef SECTION_IS_RW

static void hpd_to_dp_attention(void)
{
	int evt_index = hpd.count - 1;

	hpd.last_send_ts = get_time().val;
	pd_send_hpd(0, hpd.queue[evt_index]);

	/* If there are still events, need to shift the buffer */
	if (--hpd.count) {
		int i;

		for (i = 0; i < hpd.count; i++)
			hpd.queue[i] = hpd.queue[i + 1];
	}
}

static void hpd_queue_event(enum hpd_event evt)
{
	/*
	 * Can always add hpd_low and hpd_high events, but for hpd_irq need to
	 * ensure that no more than 2 hpd_irq events are queued.
	 */
	if (evt == hpd_irq) {
		if ((hpd.count >= HPD_QUEUE_DEPTH) || ((hpd.count  >= 2) &&
			(hpd.queue[hpd.count - 2] == hpd_irq))) {
			CPRINTS("hpd: discard hpd: count - %d",
				hpd.count);
			return;
		}
	}

	if (evt == hpd_low) {
		hpd.count = 0;
	}

	/* Add event to the queue */
	hpd.queue[hpd.count++] = evt;
}


static void hpd_to_pd_converter(int level, uint64_t ts)
{

	switch (hpd.state) {
	case LOW_WAIT:
		if (level) {
			hpd.state = HIGH_CHECK;
			hpd.timer = ts + HPD_T_IRQ_MAX_PULSE;
		}
		break;
	case HIGH_CHECK:
		if (!level) {
			hpd.state = LOW_WAIT;
		} else if (get_time().val > hpd.timer) {
			hpd.state = HIGH_WAIT;
			hpd_queue_event(hpd_high);
		}
		break;
	case HIGH_WAIT:
		if (!level) {
			hpd.state = IRQ_CHECK;
			hpd.timer = ts + HPD_T_IRQ_MAX_PULSE;
		}
		break;
	case IRQ_CHECK:
		if (level) {
			if (ts <= hpd.timer) {
				hpd.state = HIGH_WAIT;
				hpd_queue_event(hpd_irq);
			}
		} else if (ts >  hpd.timer) {
			hpd.state = LOW_WAIT;
			hpd_queue_event(hpd_low);
		}
		break;
	}
}

static void manage_hpd(void);
DECLARE_DEFERRED(manage_hpd);

static void manage_hpd(void)
{
	int level;
	uint64_t ts;
	uint32_t num_hpd_events = (hpd.edges.head - hpd.edges.tail) &
		HPD_QUEUE_MASK;

	/*
	 * HPD edges are detected via GPIO interrupts. The ISR routine adds edge
	 * info to a queue and scheudles this routine. If this routine is called
	 * without a new edge detected, then it is being called due to a timer
	 * event.
	 */
	if (num_hpd_events) {
		while(num_hpd_events-- > 0) {
			int idx = hpd.edges.tail;

			level = hpd.edges.buffer[idx].level;
			ts = hpd.edges.buffer[idx].ts;

			hpd_to_pd_converter(level, ts);

			hpd.edges.tail = (hpd.edges.tail + 1) & HPD_QUEUE_MASK;
		}
	} else {
		/* no new edge event, so get current time and level */
		level = gpio_get_level(hpd.signal);
		ts = get_time().val;
		hpd_to_pd_converter(level, ts);
	}

	/*
	 * If min time spacing requirement is exceeded and a hpd_event is
	 * queued, then send DP_ATTENTION message.
	 */
	if (hpd.count > 0) {
		if ((get_time().val - hpd.last_send_ts) > HPD_T_MIN_DP_ATTEN) {
			/* Send hpd event via DP_ATTEN message and adjust queue */
			hpd_to_dp_attention();
		} else {
			hook_call_deferred(&manage_hpd_data, 2 * MSEC);
		}
	}

	if (hpd.timer > get_time().val)
		hook_call_deferred(&manage_hpd_data, hpd.timer -
				   get_time().val);
}

void baseboard_hpd_converter_enable(int enable)
{
	if (enable) {
		gpio_disable_interrupt(HPD_GPIO_SIGNAL);
		hpd.state = LOW_WAIT;
		hpd.count = 0;
		hpd.timer = 0;
	        hpd.last_send_ts = 0;
		hpd.signal = HPD_GPIO_SIGNAL;

		/* Reset hpd signal edges queue */
		hpd.edges.head = 0;
		hpd.edges.tail = 0;

		/* If low -> high edge already present, need to advace state */
		hpd_to_pd_converter(gpio_get_level(hpd.signal), get_time().val);
		if (hpd.timer > get_time().val)
			hook_call_deferred(&manage_hpd_data, hpd.timer -
					   get_time().val);

		gpio_enable_interrupt(HPD_GPIO_SIGNAL);
	} else {
		gpio_disable_interrupt(HPD_GPIO_SIGNAL);
		hook_call_deferred(&manage_hpd_data, -1);
	}

	hpd.enable = !!enable;
}

void baseboard_manage_hpd_event(int signal)
{
	hpd.edges.buffer[hpd.edges.head].ts = get_time().val;
	hpd.edges.buffer[hpd.edges.head].level = gpio_get_level(signal);

	hpd.edges.head = (hpd.edges.head + 1) & HPD_QUEUE_MASK;
	hook_call_deferred(&manage_hpd_data, 0);
}

#endif
