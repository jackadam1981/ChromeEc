/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Functions required for UFP_D operation
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"
#include "task.h"
#include "usb_pd.h"
#include "usb_pd_dp_ufp.h"


#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

enum hpd_state {
	LOW_WAIT,
	HIGH_CHECK,
	HIGH_WAIT,
	LOW_CHECK,
	IRQ_CHECK,
};

#define EDGE_QUEUE_DEPTH BIT(3)
#define EDGE_QUEUE_MASK (EDGE_QUEUE_DEPTH - 1)
#define HPD_QUEUE_DEPTH BIT(2)
#define HPD_QUEUE_MASK (HPD_QUEUE_DEPTH - 1)
#define HPD_T_IRQ_MIN_PULSE 250
#define HPD_T_IRQ_MAX_PULSE (2 * MSEC)
#define HPD_T_MIN_DP_ATTEN (10 * MSEC)

struct hpd_mark {
	int level;
	uint64_t ts;
};

struct hpd_edge {
	int overflow;
	uint32_t head;
	uint32_t tail;
	struct hpd_mark buffer[EDGE_QUEUE_DEPTH];
};

struct hpd_info {
	enum hpd_state state;
	int count;
	uint64_t timer;
	uint64_t last_send_ts;
	enum hpd_event queue[HPD_QUEUE_DEPTH];
	struct hpd_edge edges;
};

static struct hpd_info hpd;
static struct mutex hpd_mutex;


static void hpd_to_dp_attention(void)
{
	int evt_index = hpd.count - 1;

	hpd.last_send_ts = get_time().val;
	pd_send_hpd(hpd_config.port, hpd.queue[evt_index]);

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
	/*
	 * HPD edges are marked in the irq routine. The converter state machine
	 * runs in the hooks task and so there will be some delay between when
	 * the edge was captured and when that edge is processed here in the
	 * state machine. This means that the delitch timer (250 uSec) may have
	 * already expired or is about to expire.
	 *
	 * If transitioning to timing dependent state, need to ensure the state
	 * machine is executed again. All timers are relative to the ts value
	 * passed into this routine. The timestamps passed into this routine
	 * are either the values latched in the irq routine, or the current
	 * time latched by the calling function. From the perspective of the
	 * state machine, ts represents the current time.
	 */
	switch (hpd.state) {
	case LOW_WAIT:
		/*
		 * In this state only expected event is a level change from low
		 * to high.
		 */
		if (level) {
			hpd.state = HIGH_CHECK;
			hpd.timer = ts + HPD_T_IRQ_MIN_PULSE;
		}
		break;
	case HIGH_CHECK:
		/*
		 * In this state if level is high and deglitch timer is
		 * exceeded, then state advances to HIGH_WAIT, otherwise return
		 * to LOW_WAIT state.
		 */
		if (!level || (ts <= hpd.timer)) {
			hpd.state = LOW_WAIT;
		} else {
			hpd.state = HIGH_WAIT;
			hpd_queue_event(hpd_high);
		}
		break;
	case HIGH_WAIT:
		/*
		 * In this state, only expected event is a level change from
		 * high to low. If current level is low, then advance to
		 * LOW_CHECK for deglitch checking.
		 */
		if (!level) {
			hpd.state = LOW_CHECK;
			hpd.timer = ts + HPD_T_IRQ_MIN_PULSE;
		}
		break;
	case LOW_CHECK:
		/*
		 * This state is used to deglitch high->low level
		 * change. However, due to processing latency, it's possible to
		 * detect hpd_irq event if level is high and low pulse width was
		 * valid.
		 */
		if (!level) {
			/* Still low, now wait for IRQ or LOW determination */
			hpd.timer = ts + (HPD_T_IRQ_MAX_PULSE -
					  HPD_T_IRQ_MIN_PULSE);
			hpd.state = IRQ_CHECK;

		} else {
			uint64_t irq_ts = hpd.timer + HPD_T_IRQ_MAX_PULSE -
				HPD_T_IRQ_MIN_PULSE;
			/*
			 * If hpd is high now, this must have been an edge
			 * event, but still need to determine if the pulse width
			 * is longer than hpd_irq min pulse width. State will
			 * advance to HIGH_WAIT, but if pulse width is < 2 msec,
			 * must send hpd_irq event.
			 */
			if ((ts >= hpd.timer) && (ts <= irq_ts)) {
				/* hpd irq detected */
				hpd_queue_event(hpd_irq);
			}
			hpd.state = HIGH_WAIT;
		}
		break;
	case IRQ_CHECK:
		/*
		 * In this state deglitch time has already passed. If current
		 * level is low and hpd_irq timer has expired, then go to
		 * LOW_WAIT as hpd_low event has been detected. If level is high
		 * and low pulse is < hpd_irq, hpd_irq event has been detected.
		 */
		if (level) {
			hpd.state = HIGH_WAIT;
			if (ts <= hpd.timer) {
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
	uint64_t ts = get_time().val;
	uint32_t num_hpd_events = (hpd.edges.head - hpd.edges.tail) &
		EDGE_QUEUE_MASK;

	/*
	 * HPD edges are detected via GPIO interrupts. The ISR routine adds edge
	 * info to a queue and scheudles this routine. If this routine is called
	 * without a new edge detected, then it is being called due to a timer
	 * event.
	 */

	/* First check to see overflow condition has occurred */
	if (hpd.edges.overflow) {
		/* Disable hpd interrupts */
		usb_pd_hpd_converter_enable(0);
		/* Re-enable hpd converter */
		usb_pd_hpd_converter_enable(1);
	}

	if (num_hpd_events) {
		while(num_hpd_events-- > 0) {
			int idx = hpd.edges.tail;

			level = hpd.edges.buffer[idx].level;
			ts = hpd.edges.buffer[idx].ts;

			hpd_to_pd_converter(level, ts);
			hpd.edges.tail = (hpd.edges.tail + 1) & EDGE_QUEUE_MASK;
		}
	} else {
		/* no new edge event, so get current time and level */
		level = gpio_get_level(hpd_config.signal);
		ts = get_time().val;
		hpd_to_pd_converter(level, ts);
	}

	/*
	 * If min time spacing requirement is exceeded and a hpd_event is
	 * queued, then send DP_ATTENTION message.
	 */
	if (hpd.count > 0) {
		if ((get_time().val - hpd.last_send_ts) > HPD_T_MIN_DP_ATTEN) {
			/* Generate DP_ATTENTION event pending in queue */
			hpd_to_dp_attention();
		} else {
			/*
			 * Need to wait until until min spacing requirement of
			 * DP attention messages. Set callback time to the min
			 * value required. This callback time could be changed
			 * based on hpd interrupts.
			 */
			hook_call_deferred(&manage_hpd_data,
					   HPD_T_MIN_DP_ATTEN -
					   (get_time().val - hpd.last_send_ts));
		}
	}

	/*
	 * Because of the delay between gpio edge irq, and when those edge
	 * events are processed here, all timers must be done relative to the
	 * timing marker stored in the hpd edge queue. If the state machine
	 * required a new timer, then hpd.timer will be advanced relative to the
	 * ts that was passed into the state machine.
	 *
	 * If the deglitch timer is active, then it can likely already have been
	 * expired when the edge gets processed. So if the timer is active the
	 * deferred callback must be requested.
	 *.
	 */
	if (hpd.timer > ts) {
		uint64_t callback_us = 0;
		uint64_t now = get_time().val;

		/* If timer is in the future, adjust the callback timer */
		if (now < hpd.timer)
			callback_us = (hpd.timer - now) & 0xffffffff;

		hook_call_deferred(&manage_hpd_data, callback_us);
	}
}

void usb_pd_hpd_converter_enable(int enable)
{
	/*
	 * This can be called from svdm response function (PD task) or hooks
	 * task when an overflow condition was reported.
	 */
	mutex_lock(&hpd_mutex);

	if (enable) {
		gpio_disable_interrupt(hpd_config.signal);
		/* Reset HPD event queue */
		hpd.state = LOW_WAIT;
		hpd.count = 0;
		hpd.timer = 0;
	        hpd.last_send_ts = 0;

		/* Reset hpd signal edges queue */
		hpd.edges.head = 0;
		hpd.edges.tail = 0;
		hpd.edges.overflow = 0;

		/* If signal is high, need to ensure state machine executes */
		if (gpio_get_level(hpd_config.signal))
			hook_call_deferred(&manage_hpd_data, 0);

		/* Enable hpd edge detection */
		gpio_enable_interrupt(hpd_config.signal);
	} else {
		gpio_disable_interrupt(hpd_config.signal);
		hook_call_deferred(&manage_hpd_data, -1);
	}

	mutex_unlock(&hpd_mutex);
}

void usb_pd_hpd_edge_event(int signal)
{
	int  next_head = (hpd.edges.head + 1) & EDGE_QUEUE_MASK;
	struct hpd_mark mark;

	/* Get current timestamp and level */
	mark.ts = get_time().val;
	mark.level = gpio_get_level(hpd_config.signal);

	/* Add this edge to the buffer if there is space */
	if (next_head != hpd.edges.tail) {
		hpd.edges.buffer[hpd.edges.head].ts = mark.ts;
		hpd.edges.buffer[hpd.edges.head].level = mark.level;
		hpd.edges.head = next_head;
	} else {
		/* Edge queue is overflowing, need to reset the converter */
		hpd.edges.overflow = 1;
	}
	/* Schedule HPD state machine to run ASAP */
	hook_call_deferred(&manage_hpd_data, 0);
}
