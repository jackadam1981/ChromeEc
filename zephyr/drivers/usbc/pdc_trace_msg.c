/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB PDC message tracing.
 */

#include "atomic.h"
#include "builtin/assert.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "task.h"
#include "timer.h"
#include "usb_pd.h"
#include "util.h"

#include <drivers/pdc.h>

#define MSG(format, args...) printk("%s: " format, __func__, ##args)

#define MSG_FIFO_SIZE_LOG2 CONFIG_USBC_PDC_TRACE_MSG_FIFO_SIZE
#define MSG_FIFO_SIZE (1 << MSG_FIFO_SIZE_LOG2)
#define MSG_FIFO_SIZE_MASK (MSG_FIFO_SIZE - 1)
#define MSG_FIFO_MOD(offset) ((offset) & MSG_FIFO_SIZE_MASK)

#define MSG_ENTRY_SEQ_NUM_BITS \
	(8 * member_size(struct pdc_trace_msg_entry, seq_num))
#define MSG_ENTRY_SEQ_NUM_MOD(n) ((n) & ((1 << MSG_ENTRY_SEQ_NUM_BITS) - 1))

BUILD_ASSERT(MSG_ENTRY_SEQ_NUM_BITS == 16);

#define e_offset(_f) offsetof(struct pdc_trace_msg_entry, _f)

static struct {
	mutex_t lock;
	uint32_t dropped;
	uint32_t seq_num;
	size_t next_rd;
	size_t next_wr;
	size_t wr_available;
	uint8_t buf[MSG_FIFO_SIZE];
} msg_fifo = {
	.wr_available = MSG_FIFO_SIZE,
};

__test_only void pdc_trace_msg_fifo_reset(void)
{
	memset(&msg_fifo, 0, sizeof(msg_fifo));
	msg_fifo.wr_available = MSG_FIFO_SIZE;
}

/*
 * Note: protected by msg_fifo.lock.
 */
static uint8_t pdc_trace_port = EC_PDC_TRACE_MSG_PORT_NONE;

__maybe_unused static bool is_port_present(int port)
{
	return (port >= 0) && (port < board_get_usb_pd_port_count());
}

static uint8_t msg_fifo_get8(size_t offset)
{
	return msg_fifo.buf[MSG_FIFO_MOD(offset)];
}

static uint16_t msg_fifo_get16(size_t offset)
{
	uint8_t hi, lo;

	lo = msg_fifo.buf[MSG_FIFO_MOD(offset)];
	hi = msg_fifo.buf[MSG_FIFO_MOD(offset + 1)];
	return (hi << 8) | lo;
}

/*
 * @brief Append data to the msg FIFO. This is essentially a memcpy with
 *        wrap-around support.
 *
 * @param offset FIFO offset (index) to add new data
 * @param data   pointer to data to add
 * @param count  number of data bytes to add
 *               adding 0 bytes is a no-op
 *               overflow is not checked
 *
 * @return new FIFO offset for next invocation
 */
static size_t msg_fifo_append(size_t offset, const void *data, size_t count)
{
	size_t start1;
	size_t count1;

	start1 = MSG_FIFO_MOD(offset);

	if (count == 0)
		return start1;

	count1 = MIN(count, MSG_FIFO_SIZE - start1);
	memcpy(msg_fifo.buf + start1, data, count1);
	if (count > count1) {
		memcpy(msg_fifo.buf, (const uint8_t *)data + count1,
		       count - count1);
	}
	return MSG_FIFO_MOD(offset + count);
}

/*
 * @brief return a reference to the oldest message in the FIFO
 *        if peek succeeds, callers can access the message data
 *        without locks
 *        note:
 *   	    peek/skip only works for the single consumer case
 *   	    parallel consumers may see the same entry
 *
 * @param offsetp *offsetp is set to the offset of available message
 *        data
 *
 * @return true IFF message data is available with *offsetp referring to it
 */
static bool msg_fifo_peek(size_t *offsetp)
{
	mutex_lock(&msg_fifo.lock);

	if (msg_fifo.wr_available >= sizeof(msg_fifo.buf)) {
		/* FIFO is empty */
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	*offsetp = msg_fifo.next_rd;

	mutex_unlock(&msg_fifo.lock);

	return true;
}

/*
 * @brief skip/consume the oldest trace entry in the FIFO
 *
 * @return true IFF if an entry was skipped/consumed
 */
static bool msg_fifo_skip(void)
{
	int fifo_offset;
	size_t cap_entry_bytes;

	mutex_lock(&msg_fifo.lock);

	if (msg_fifo.wr_available >= sizeof(msg_fifo.buf)) {
		/* FIFO is already empty */
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	fifo_offset = msg_fifo.next_rd;
	cap_entry_bytes = sizeof(struct pdc_trace_msg_entry) +
			  msg_fifo_get8(fifo_offset + e_offset(pdc_data_size));
	msg_fifo.next_rd = MSG_FIFO_MOD(fifo_offset + cap_entry_bytes);
	msg_fifo.wr_available += cap_entry_bytes;

	mutex_unlock(&msg_fifo.lock);

	return true;
}

/*
 * @brief push a PDC message into the FIFO
 *        a new trace entry is crated for the PDC message
 *        increment drop count if FIFO is full
 *
 * @param port      Port associated with the message
 * @param dir       Message is from transmit vs. receive path
 * @param msg_type  Token identifying message type
 * @param payload   Pointer to PDC message
 * @param msg_bytes Size of PDC message
 *
 * @return true IFF an entry was added to the FIFO
 */
static bool msg_fifo_push_entry(uint8_t port, uint8_t dir, uint8_t msg_type,
				const void *payload, int msg_bytes)
{
	size_t cap_entry_bytes;
	size_t fifo_offset;

	mutex_lock(&msg_fifo.lock);

	if (pdc_trace_port == EC_PDC_TRACE_MSG_PORT_NONE) {
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	if ((pdc_trace_port != EC_PDC_TRACE_MSG_PORT_ALL) &&
	    (pdc_trace_port != port)) {
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	struct pdc_trace_msg_entry e_header;

	cap_entry_bytes = sizeof(e_header) + msg_bytes;

	if (cap_entry_bytes > msg_fifo.wr_available) {
		/* FIFO overflow */
		if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG_VERBOSE)) {
			MSG("%zu bytes > max %d bytes\n", cap_entry_bytes,
			    (int)msg_fifo.wr_available);
		}
		++msg_fifo.dropped;
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	fifo_offset = msg_fifo.next_wr;
	msg_fifo.next_wr = MSG_FIFO_MOD(fifo_offset + cap_entry_bytes);
	msg_fifo.wr_available -= cap_entry_bytes;

	e_header.time_us = get_time().val;
	e_header.seq_num = msg_fifo.seq_num;
	e_header.port_num = port;
	e_header.direction = dir;
	e_header.msg_type = msg_type;
	e_header.pdc_data_size = msg_bytes;
	fifo_offset = msg_fifo_append(fifo_offset, &e_header, sizeof(e_header));
	fifo_offset =
		msg_fifo_append(fifo_offset, payload, e_header.pdc_data_size);
	msg_fifo.seq_num = MSG_ENTRY_SEQ_NUM_MOD(msg_fifo.seq_num + 1);

	mutex_unlock(&msg_fifo.lock);
	return true;
}

/*
 * @brief Control PDC message tracing on specified port.
 *
 * @param port Port number to change.
 *             Use EC_PDC_TRACE_MSG_PORT_NONE to disable.
 *             Use EC_PDC_TRACE_MSG_PORT_ALL to enable on all ports.
 *             Use valid port number to enable on a single port.
 *
 * @return previous port tracing value.
 */
test_export_static int pdc_trace_msg_enable(int new_port)
{
	int prev_port;

	mutex_lock(&msg_fifo.lock);

	prev_port = pdc_trace_port;
	pdc_trace_port = new_port;

	mutex_unlock(&msg_fifo.lock);

	return prev_port;
}

test_mockable bool pdc_trace_msg_req(int port,
				     enum pdc_trace_chip_type msg_type,
				     const uint8_t *buf, const int count)
{
	return msg_fifo_push_entry(port, PDC_TRACE_MSG_DIR_OUT, msg_type, buf,
				   count);
}

test_mockable bool pdc_trace_msg_resp(int port,
				      enum pdc_trace_chip_type msg_type,
				      const uint8_t *buf, const int count)
{
	return msg_fifo_push_entry(port, PDC_TRACE_MSG_DIR_IN, msg_type, buf,
				   count);
}

/*
 * @brief Print PDC message payload to console
 */
static void fifo_pl_print(const int pdc_offset, const int pl_size)
{
	printk("  bytes %d:", pl_size);
	for (int i = 0; i < pl_size; ++i) {
		printk(" %02x", msg_fifo_get8(pdc_offset + i));
	}
}

/*
 * @brief Print FIFO entry to console
 */
__maybe_unused static void fifo_entry_print(const int pdc_offset)
{
	uint16_t sn;
	uint8_t pn, dir, sz;

	sn = msg_fifo_get16(pdc_offset + e_offset(seq_num));
	pn = msg_fifo_get8(pdc_offset + e_offset(port_num));
	dir = msg_fifo_get8(pdc_offset + e_offset(direction));
	sz = msg_fifo_get8(pdc_offset + e_offset(pdc_data_size));

	printk("SEQ:%04x PORT:%u %s {\n", sn, pn, dir ? "OUT" : "IN");
	fifo_pl_print(pdc_offset + e_offset(pdc_data), sz);
	printk("\n}\n");
}

#ifdef CONFIG_USBC_PDC_TRACE_MSG_HOST_CMD

static enum ec_status
hc_pdc_trace_msg_enable(struct host_cmd_handler_args *args)
{
	const struct ec_params_pdc_trace_msg_enable *p = args->params;
	struct ec_response_pdc_trace_msg_enable *r = args->response;
	int req_port;

	req_port = p->port;

	switch (req_port) {
	case EC_PDC_TRACE_MSG_PORT_NONE:
	case EC_PDC_TRACE_MSG_PORT_ALL:
		break;
	default:
		if (!is_port_present(req_port))
			req_port = EC_PDC_TRACE_MSG_PORT_NONE;
	}

	memset(r, 0, sizeof(*r));
	r->port = pdc_trace_msg_enable(req_port);
	r->fifo_free = msg_fifo.wr_available;
	r->dropped_count = msg_fifo.dropped;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_PDC_TRACE_MSG_ENABLE, hc_pdc_trace_msg_enable,
		     EC_VER_MASK(0));

/*
 * @brief Get data from the msg FIFO. This is essentially a memcpy with
 *        wrap-around support.
 *
 * @param data   Destination of data.
 * @param offset FIFO offset (index) to get data from.
 * @param count  Number of data bytes to get.
 *               Getting 0 bytes is a no-op
 */
static void msg_fifo_get_data(void *data, size_t offset, size_t count)
{
	size_t start1;
	size_t count1;

	if (count == 0)
		return;

	start1 = MSG_FIFO_MOD(offset);
	count1 = MIN(count, MSG_FIFO_SIZE - start1);
	memcpy(data, msg_fifo.buf + start1, count1);
	if (count > count1) {
		memcpy((uint8_t *)data + count1, msg_fifo.buf, count - count1);
	}
}

static enum ec_status
hc_pdc_trace_msg_get_entries(struct host_cmd_handler_args *args)
{
	struct ec_response_pdc_trace_msg_get_entries *r = args->response;

	memset(r, 0, sizeof(*r));

	size_t fifo_offset;

	while (msg_fifo_peek(&fifo_offset)) {
		size_t cap_entry_bytes;

		if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG_VERBOSE))
			fifo_entry_print(fifo_offset);

		cap_entry_bytes =
			sizeof(struct pdc_trace_msg_entry) +
			msg_fifo_get8(fifo_offset + e_offset(pdc_data_size));

		if (cap_entry_bytes > MAX_HC_PDC_TRACE_MSG_GET_PAYLOAD) {
			/* this will never fit, skip it */
			msg_fifo_skip();
			continue;
		}
		if (r->pl_size + cap_entry_bytes >
		    MAX_HC_PDC_TRACE_MSG_GET_PAYLOAD) {
			/* not enough room, return next time */
			break;
		}

		msg_fifo_get_data(&r->payload[r->pl_size], fifo_offset,
				  cap_entry_bytes);

		r->pl_size += cap_entry_bytes;

		msg_fifo_skip();
	}

	args->response_size = sizeof(*r) + r->pl_size;

	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_PDC_TRACE_MSG_GET_ENTRIES,
		     hc_pdc_trace_msg_get_entries, EC_VER_MASK(0));

#endif /* CONFIG_USBC_PDC_TRACE_MSG_HOST_CMD */

#ifdef CONFIG_USBC_PDC_TRACE_MSG_CONSOLE_CMD

#define PORT_NO_CHANGE -1

static void printk_port_num(int port)
{
	switch (port) {
	case EC_PDC_TRACE_MSG_PORT_NONE:
		printk("NONE");
		break;
	case EC_PDC_TRACE_MSG_PORT_ALL:
		printk("ALL");
		break;
	default:
		printk("%u", port);
	}
}

static int command_pdc_trace(int argc, const char **argv)
{
	int port;
	char *rest;

	switch (argc) {
	case 1:
		port = PORT_NO_CHANGE;
		break;
	case 2:
		if (strcasecmp(argv[1], "on") == 0 ||
		    strcasecmp(argv[1], "all") == 0) {
			port = EC_PDC_TRACE_MSG_PORT_ALL;
			break;
		}
		if (strcasecmp(argv[1], "off") == 0) {
			port = EC_PDC_TRACE_MSG_PORT_NONE;
			break;
		}
		port = strtoi(argv[1], &rest, 0);
		if (*rest != '\0')
			return EC_ERROR_PARAM1;
		if (port < 0 || port == EC_PDC_TRACE_MSG_PORT_NONE)
			return EC_ERROR_PARAM1;
		break;
	default:
		return EC_ERROR_PARAM_COUNT;
	}

	if (port == PORT_NO_CHANGE) {
		printk("PDC trace port is: ");
		printk_port_num(pdc_trace_port);
		printk("\n");
	} else {
		int prev_port;

		prev_port = pdc_trace_msg_enable(port);

		printk("PDC trace port changed from ");
		printk_port_num(prev_port);
		printk(" to ");
		printk_port_num(port);
		printk("\n");
	}

	/*
	 * "off" (PDC_TRACE_MSG_PORT_NONE) only stops new entries.
	 * drain remaining messages.
	 */

	size_t offset;

	while (msg_fifo_peek(&offset)) {
		fifo_entry_print(offset);
		msg_fifo_skip();
	}

	printk("msg_fifo: rd %d wr %d wr_available %d, dropped %u\n",
	       (int)msg_fifo.next_rd, (int)msg_fifo.next_wr,
	       (int)msg_fifo.wr_available, (int)msg_fifo.dropped);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(pdctrace, command_pdc_trace, "<Type-C port>|all|on|off",
			"Trace PDC messages");

#endif /* CONFIG_USBC_PDC_TRACE_MSG_CONSOLE_CMD */
