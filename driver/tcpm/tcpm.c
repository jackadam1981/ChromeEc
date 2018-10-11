/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TCPM implementation for CONFIG_USB_PD_TCPC_MANAGER. This implementation
 * will be used by devices that manage a TCPC device, e.g. chromebooks (opposed
 * to devices where the EC itself implements the TCPC, e.g. Zinger).
 */

#include "atomic.h"
#include "console.h"
#include "tcpm.h"
#include "task.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
int tcpc_write(int port, int reg, int val)
{
	int rv = i2c_write8(tcpc_config[port].i2c_host_port,
			    tcpc_config[port].i2c_slave_addr, reg, val);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_write8(tcpc_config[port].i2c_host_port,
				tcpc_config[port].i2c_slave_addr, reg, val);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_write16(int port, int reg, int val)
{
	int rv = i2c_write16(tcpc_config[port].i2c_host_port,
			     tcpc_config[port].i2c_slave_addr, reg, val);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_write16(tcpc_config[port].i2c_host_port,
				 tcpc_config[port].i2c_slave_addr, reg, val);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_read(int port, int reg, int *val)
{
	int rv = i2c_read8(tcpc_config[port].i2c_host_port,
			   tcpc_config[port].i2c_slave_addr, reg, val);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_read8(tcpc_config[port].i2c_host_port,
			       tcpc_config[port].i2c_slave_addr, reg, val);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_read16(int port, int reg, int *val)
{
	int rv = i2c_read16(tcpc_config[port].i2c_host_port,
			    tcpc_config[port].i2c_slave_addr, reg, val);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_read16(tcpc_config[port].i2c_host_port,
				tcpc_config[port].i2c_slave_addr, reg, val);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_read_block(int port, int reg, uint8_t *in, int size)
{
	int rv = i2c_read_block(tcpc_config[port].i2c_host_port,
			    tcpc_config[port].i2c_slave_addr, reg, in, size);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_read_block(tcpc_config[port].i2c_host_port,
				tcpc_config[port].i2c_slave_addr, reg,
				in, size);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_write_block(int port, int reg, const uint8_t *out, int size)
{
	int rv = i2c_write_block(tcpc_config[port].i2c_host_port,
			    tcpc_config[port].i2c_slave_addr, reg, out, size);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_write_block(tcpc_config[port].i2c_host_port,
				tcpc_config[port].i2c_slave_addr, reg,
				out, size);
	}
	pd_device_accessed(port);
	return rv;
}

int tcpc_xfer(int port, const uint8_t *out, int out_size,
			uint8_t *in, int in_size)
{
	int rv;
	/* Dispatching to tcpc_xfer_unlocked reduces code size growth. */
	tcpc_lock(port, 1);
	rv = tcpc_xfer_unlocked(port, out, out_size, in, in_size,
				I2C_XFER_SINGLE);
	tcpc_lock(port, 0);
	return rv;
}

int tcpc_xfer_unlocked(int port, const uint8_t *out, int out_size,
			    uint8_t *in, int in_size, int flags)
{
	int rv = i2c_xfer_unlocked(tcpc_config[port].i2c_host_port,
			  tcpc_config[port].i2c_slave_addr, out, out_size,
			  in, in_size, flags);
	if (rv && pd_device_in_low_power(port)) {
		pd_wait_for_wakeup(port);
		rv = i2c_xfer_unlocked(tcpc_config[port].i2c_host_port,
			      tcpc_config[port].i2c_slave_addr, out, out_size,
			      in, in_size, flags);
	}
	pd_device_accessed(port);
	return rv;
}
#endif /* CONFIG_USB_PD_TCPC_LOW_POWER */

/* TCPM driver wrapper function */
int tcpm_init(int port)
{
	int rv;

	rv = tcpc_config[port].drv->init(port);
	if (rv)
		return rv;

	/* Board specific post TCPC init */
	if (board_tcpc_post_init)
		rv = board_tcpc_post_init(port);

	return rv;
}

int tcpm_get_cc(int port, int *cc1, int *cc2)
{
	static int prev_cc1;
	static int prev_cc2;

	const int rv = tcpc_config[port].drv->get_cc(port, cc1, cc2);

	if (pd_debug_level() >= 3 && rv == EC_SUCCESS) {
		if (prev_cc1 != *cc1 || prev_cc2 != *cc2)
			CPRINTF("C%d CC lines status changed: cc1 %d->%d, cc2 "
				"%d->%d\n",
				port, prev_cc1, *cc1, prev_cc2, *cc2);

		prev_cc1 = *cc1;
		prev_cc2 = *cc2;
	}

	return rv;
}

struct cached_tcpm_message {
	uint32_t header;
	uint32_t payload[7];
};

/* Cache depth needs to be power of 2 */
#define CACHE_DEPTH (1 << 2)
#define CACHE_DEPTH_MASK (CACHE_DEPTH - 1)

struct queue {
	/*
	 * Head points to the index of the first empty slot to put a new RX
	 * message. Must be masked before used in lookup.
	 */
	uint32_t head;
	/*
	 * Tail points to the index of the first message for the PD task to
	 * consume. Must be masked before used in lookup.
	 */
	uint32_t tail;
	struct cached_tcpm_message buffer[CACHE_DEPTH];
};
static struct queue cached_messages[CONFIG_USB_PD_PORT_COUNT];

/* Note this method can be called from an interrupt context. */
int tcpm_enqueue_message(const int port)
{
	int rv;
	struct queue *const q = &cached_messages[port];
	struct cached_tcpm_message *const head =
		&q->buffer[q->head & CACHE_DEPTH_MASK];

	if (q->head - q->tail == CACHE_DEPTH) {
		CPRINTS("C%d RX EC Buffer full!", port);
		return EC_ERROR_OVERFLOW;
	}

	/* Call the raw driver without caching */
	rv = tcpc_config[port].drv->get_message_raw(port, head->payload,
						    &head->header);
	if (rv) {
		CPRINTS("C%d: Could not retrieve RX message (%d)", port, rv);
		return rv;
	}

	/* Increment atomically to ensure get_message_raw happens-before */
	atomic_add(&q->head, 1);

	/* Wake PD task up so it can process incoming RX messages */
	task_set_event(PD_PORT_TO_TASK_ID(port), TASK_EVENT_WAKE, 0);

	return EC_SUCCESS;
}

int tcpm_has_pending_message(const int port)
{
	const struct queue *const q = &cached_messages[port];

	return q->head != q->tail;
}

void tcpm_clear_pending_messages(int port)
{
	struct queue *const q = &cached_messages[port];

	q->tail = q->head;
}

int tcpm_dequeue_message(const int port, uint32_t *const payload,
			 int *const header)
{
	struct queue *const q = &cached_messages[port];
	struct cached_tcpm_message *const tail =
		&q->buffer[q->tail & CACHE_DEPTH_MASK];

	if (!tcpm_has_pending_message(port)) {
		CPRINTS("C%d No message in RX buffer!");
		return EC_ERROR_BUSY;
	}

	/* Copy cache data in to parameters */
	*header = tail->header;
	memcpy(payload, tail->payload, sizeof(tail->payload));

	/* Increment atomically to ensure memcpy happens-before */
	atomic_add(&q->tail, 1);

	return EC_SUCCESS;
}
