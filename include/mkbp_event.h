/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Event handling in MKBP keyboard protocol
 */

#ifndef __CROS_EC_MKBP_EVENT_H
#define __CROS_EC_MKBP_EVENT_H

/*
 * Last time the host received an interrupt.
 *
 * Retrieved via __hw_clock_source_read() as close as possible
 * to the interrupt source. Intended to be virtually the same time the
 * first line of the AP hard irq for the EC interrupt.
 */
extern uint32_t mkbp_last_event_time;

/*
 * Sends an event to the AP.
 *
 * When this is called, the event data must be ready for query.  Otherwise,
 * when the AP queries the event, an error is returned and the event is lost.
 *
 * @param event_type  One of EC_MKBP_EVENT_*.
 * @return   True if event succeeded to generate host interrupt.
 */
int mkbp_send_event(uint8_t event_type);

/**
 * Enables dynamic decision on how to route MKBP events to the AP.
 *
 * Some boards may need different mechanisms to report a new event to
 * the AP, depending on HW / SW configuration that can only be dynamically
 * discovered at runtime. The host_mkbp_event_helper is called in
 * mkbp_event.c:set_host_interrupt in order to enable this dynamic reporting.
 *
 * If no helper is configured, the decision is predicated upon whether
 * CONFIG_MKBP_USE_HOST_EVENT is defined.
 */
typedef void(*mkbp_event_notifier)(int active);
void set_mkbp_event_notifier(mkbp_event_notifier f);

void mkbp_event_notifier_gpio(int active);
void mkbp_event_notifier_host(int active);

/*
 * The struct to store the event source definition.  The get_data routine is
 * responsible for returning the event data when queried by the AP.  The
 * parameter 'data' points to where the event data needs to be stored, and
 * the size of the event data should be returned.
 */
struct mkbp_event_source {
	uint8_t event_type;
	int (*get_data)(uint8_t *data);
};

#define DECLARE_EVENT_SOURCE(type, func)                       \
	const struct mkbp_event_source __keep		       \
	__no_sanitize_address _evt_src_##type		       \
	__attribute__((section(".rodata.evtsrcs")))            \
		 = {type, func}

#endif  /* __CROS_EC_MKBP_EVENT_H */
