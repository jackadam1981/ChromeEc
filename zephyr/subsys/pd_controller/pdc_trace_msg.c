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

#include <stdio.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include <drivers/pdc.h>

#define MSG_FIFO_SIZE CONFIG_USBC_PDC_TRACE_MSG_FIFO_SIZE
#define MSG_FIFO_SIZE_MASK (MSG_FIFO_SIZE - 1)
#define MSG_FIFO_MOD(offset) ((offset) & MSG_FIFO_SIZE_MASK)

#define MSG_ENTRY_SEQ_NUM_BITS \
	(8 * member_size(struct pdc_trace_msg_entry, seq_num))
#define MSG_ENTRY_SEQ_NUM_MOD(n) ((n) & ((1 << MSG_ENTRY_SEQ_NUM_BITS) - 1))

BUILD_ASSERT(MSG_ENTRY_SEQ_NUM_BITS == 16);
BUILD_ASSERT(POWER_OF_TWO(MSG_FIFO_SIZE));

LOG_MODULE_REGISTER(pdc_trace);

#define e_offset(_f) offsetof(struct pdc_trace_msg_entry, _f)

static struct {
	struct k_mutex lock;
	uint32_t dropped;
	uint32_t seq_num;
	size_t next_rd;
	size_t next_wr;
	size_t wr_available;
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
static uint8_t msg_fifo_buf[MSG_FIFO_SIZE];

__maybe_unused static bool is_port_present(int port)
{
	return (port >= 0) && (port < board_get_usb_pd_port_count());
}

static uint8_t msg_fifo_get8(size_t offset)
{
	return msg_fifo_buf[MSG_FIFO_MOD(offset)];
}

static uint16_t msg_fifo_get16(size_t offset)
{
	uint8_t hi, lo;

	lo = msg_fifo_buf[MSG_FIFO_MOD(offset)];
	hi = msg_fifo_buf[MSG_FIFO_MOD(offset + 1)];
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
	memcpy(msg_fifo_buf + start1, data, count1);
	if (count > count1) {
		memcpy(msg_fifo_buf, (const uint8_t *)data + count1,
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
	k_mutex_lock(&msg_fifo.lock, K_FOREVER);

	if (msg_fifo.wr_available >= sizeof(msg_fifo_buf)) {
		/* FIFO is empty */
		mutex_unlock(&msg_fifo.lock);
		return false;
	}

	*offsetp = msg_fifo.next_rd;

	k_mutex_unlock(&msg_fifo.lock);

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

	k_mutex_lock(&msg_fifo.lock, K_FOREVER);

	if (msg_fifo.wr_available >= sizeof(msg_fifo_buf)) {
		/* FIFO is already empty */
		k_mutex_unlock(&msg_fifo.lock);
		return false;
	}

	fifo_offset = msg_fifo.next_rd;
	cap_entry_bytes = sizeof(struct pdc_trace_msg_entry) +
			  msg_fifo_get8(fifo_offset + e_offset(pdc_data_size));
	msg_fifo.next_rd = MSG_FIFO_MOD(fifo_offset + cap_entry_bytes);
	msg_fifo.wr_available += cap_entry_bytes;

	k_mutex_unlock(&msg_fifo.lock);

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

	k_mutex_lock(&msg_fifo.lock, K_FOREVER);

	if (pdc_trace_port == EC_PDC_TRACE_MSG_PORT_NONE) {
		k_mutex_unlock(&msg_fifo.lock);
		return false;
	}

	if ((pdc_trace_port != EC_PDC_TRACE_MSG_PORT_ALL) &&
	    (pdc_trace_port != port)) {
		k_mutex_unlock(&msg_fifo.lock);
		return false;
	}

	struct pdc_trace_msg_entry e_header;

	cap_entry_bytes = sizeof(e_header) + msg_bytes;

	if (cap_entry_bytes > msg_fifo.wr_available) {
		/* FIFO overflow */
		if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG_VERBOSE)) {
			LOG_DBG("%zu bytes > max %d bytes\n", cap_entry_bytes,
				(int)msg_fifo.wr_available);
		}
		++msg_fifo.dropped;
		k_mutex_unlock(&msg_fifo.lock);
		return false;
	}

	fifo_offset = msg_fifo.next_wr;
	msg_fifo.next_wr = MSG_FIFO_MOD(fifo_offset + cap_entry_bytes);
	msg_fifo.wr_available -= cap_entry_bytes;

	e_header.time32_us = get_time().le.lo;
	e_header.seq_num = msg_fifo.seq_num;
	e_header.port_num = port;
	e_header.direction = dir;
	e_header.msg_type = msg_type;
	e_header.pdc_data_size = msg_bytes;
	fifo_offset = msg_fifo_append(fifo_offset, &e_header, sizeof(e_header));
	fifo_offset =
		msg_fifo_append(fifo_offset, payload, e_header.pdc_data_size);
	msg_fifo.seq_num = MSG_ENTRY_SEQ_NUM_MOD(msg_fifo.seq_num + 1);

	k_mutex_unlock(&msg_fifo.lock);
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

	k_mutex_lock(&msg_fifo.lock, K_FOREVER);

	prev_port = pdc_trace_port;
	pdc_trace_port = new_port;

	k_mutex_unlock(&msg_fifo.lock);

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
 * @brief Convert payload bytes to string notation in a buffer.
 *
 * @param str        Buffer for string notation.
 * @param str_len    Size of buffer. The provided buffer must be
 *                   large enough for some useful data to be returned.
 * @param pdc_offset Offset info message FIFO for PDC message.
 * @pl_size          Number of bytes to process starting at pdc_offset.
 */
static void fifo_pl_str(char *str, const size_t str_len, const int pdc_offset,
			const uint8_t pl_size)
{
	int str_index;

	if (str_len < 20) {
		/* string buffer too small, give up */
		if (str_len > 0)
			str[0] = '\0';
		return;
	}

	str_index = sprintf(str, "bytes %u:", pl_size);

	/*
	 * figure out number of entries buffer can handle
	 */
	const int entry_str_len = 3;
	int entries;
	entries = (str_len - 1 - str_index) / entry_str_len;
	entries = MIN(entries, pl_size);

	for (int i = 0; i < entries; ++i) {
		sprintf(&str[str_index], " %02x",
			msg_fifo_get8(pdc_offset + i));
		str_index += entry_str_len;
	}
	str[str_index] = '\0';
}

#define ENTRY_FMT "SEQ:%04x PORT:%u %s {\n%s\n}\n"
#define STR_BUF_SIZE 100

/*
 * @brief Print FIFO entry to shell console or debug log
 *
 * @param sh         Shell handle for output.
 *                   If NULL, a debug log entry is written.
 * @param pdc_offset Offset info message FIFO for PDC message.
 */
__maybe_unused static void fifo_entry_print(const struct shell *sh,
					    const int pdc_offset)
{
	char str_buf[STR_BUF_SIZE];
	uint16_t sn;
	uint8_t pn, dir, sz;

	sn = msg_fifo_get16(pdc_offset + e_offset(seq_num));
	pn = msg_fifo_get8(pdc_offset + e_offset(port_num));
	dir = msg_fifo_get8(pdc_offset + e_offset(direction));
	sz = msg_fifo_get8(pdc_offset + e_offset(pdc_data_size));

	fifo_pl_str(str_buf, sizeof(str_buf), pdc_offset + e_offset(pdc_data),
		    sz);

	if (sh != NULL) {
		shell_fprintf(sh, SHELL_NORMAL, ENTRY_FMT, sn, pn,
			      dir ? "OUT" : "IN", str_buf);
	} else if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG_VERBOSE)) {
		LOG_DBG(ENTRY_FMT, sn, pn, dir ? "OUT" : "IN", str_buf);
	}
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
	memcpy(data, msg_fifo_buf + start1, count1);
	if (count > count1) {
		memcpy((uint8_t *)data + count1, msg_fifo_buf, count - count1);
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
			fifo_entry_print(NULL, fifo_offset);

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

/*
 * @brief convert port number to string,
 *        handles PORT_NONE and PORT_ALL.
 *
 * @param port port number
 *
 * @return pointer to static string buffer
 */
static const char *port_num_str(uint8_t port)
{
	static char buf[4];

	switch (port) {
	case EC_PDC_TRACE_MSG_PORT_NONE:
		return "NONE";
	case EC_PDC_TRACE_MSG_PORT_ALL:
		return "ALL";
	default:
		sprintf(buf, "%u", port);
		return buf;
	}
}

int cmd_pdc_trace(const struct shell *sh, int argc, const char **argv)
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
		if (strcasecmp(argv[1], "off") == 0 ||
		    strcasecmp(argv[1], "none") == 0) {
			port = EC_PDC_TRACE_MSG_PORT_NONE;
			break;
		}
		port = strtoi(argv[1], &rest, 0);
		if (*rest != '\0') {
			shell_error(sh, "Invalid port number: %s", argv[1]);
			return -ENOEXEC;
		}
		if (port < 0 || port == EC_PDC_TRACE_MSG_PORT_ALL ||
		    port == EC_PDC_TRACE_MSG_PORT_NONE) {
			shell_error(sh, "Port number out of range: %d", port);
			return -ENOEXEC;
		}
		break;
	default:
		return -ENOEXEC;
	}

	if (port == PORT_NO_CHANGE) {
		shell_fprintf(sh, SHELL_NORMAL, "PDC trace port is: %s\n",
			      port_num_str(pdc_trace_port));
	} else {
		int prev_port;

		prev_port = pdc_trace_msg_enable(port);

		shell_fprintf(sh, SHELL_NORMAL,
			      "PDC trace port changed from %s ",
			      port_num_str(prev_port));
		shell_fprintf(sh, SHELL_NORMAL, " to %s\n", port_num_str(port));
	}

	/*
	 * "off" (PDC_TRACE_MSG_PORT_NONE) only stops new entries.
	 * drain remaining messages.
	 */

	size_t offset;

	while (msg_fifo_peek(&offset)) {
		fifo_entry_print(sh, offset);
		msg_fifo_skip();
	}

	shell_fprintf(sh, SHELL_NORMAL,
		      "msg_fifo: rd %d wr %d wr_available %d, dropped %u\n",
		      (int)msg_fifo.next_rd, (int)msg_fifo.next_wr,
		      (int)msg_fifo.wr_available, (int)msg_fifo.dropped);

	return 0;
}

#endif /* CONFIG_USBC_PDC_TRACE_MSG_CONSOLE_CMD */
