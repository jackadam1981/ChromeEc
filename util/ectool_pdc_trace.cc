/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "comm-host.h"
#include "ectool.h"
#include "ectool_pdc_net.h"
#include "ectool_pdc_pcap.h"
#include "misc_util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <endian.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/* clang-format off */
const char cmd_pdc_trace_usage[] =
	"\n\tCollect USB PDC messages\n"
	"\t-p <port>  collect on USB-C port <port> (default all)\n"
	"\t-d <host>  send to <host> (UDP port "
		      STRINGIFY(USB_PDC_UDP_PORT) ")\n"
	"\t-s         send to stdout (default if no other destination)\n"
	"\t-v         send to crostini VM (UDP port "
		      STRINGIFY(USB_PDC_UDP_PORT) ", on-device only)\n"
	"\t-w <file>  write to <file>";
/* clang-format on */

static void pl_entries(const uint8_t *data, size_t data_size, bool with_stdout);

static FILE *pcap;

int cmd_pdc_trace(int argc, char *argv[])
{
	struct ec_params_pdc_trace_msg_enable ep = {
		.port = EC_PDC_TRACE_MSG_PORT_ALL,
	};
	struct ec_response_pdc_trace_msg_enable er;

	struct ec_response_pdc_trace_msg_get_entries *gr =
		(ec_response_pdc_trace_msg_get_entries *)ec_inbuf;

	int rv;

	bool s_flag = false;
	bool vm_flag = false;
	const char *d_flag = NULL;
	const char *w_flag = NULL;

	/*
	 * output traces to stdout unless another output destination
	 * has been requested.
	 */
	bool with_stdout = true;

	int c;
	optind = 0; /* reset previous getopt */

	while ((c = getopt(argc, argv, "d:p:svw:")) != -1) {
		char *s;
		int port;

		switch (c) {
		case 'd':
			d_flag = optarg;
			with_stdout = false;
			break;

		case 'p':
			port = strtol(optarg, &s, 0);
			if ((s && *s != '\0')) {
				fprintf(stderr, "Bad port number: %s\n",
					optarg);
				return -1;
			}
			ep.port = port;
			break;

		case 's':
			s_flag = true;
			break;

		case 'v':
			vm_flag = true;
			with_stdout = false;
			break;

		case 'w':
			w_flag = optarg;
			with_stdout = false;
			break;

		default:
			/* unexpected option */
			return -1;
		}
	}

	if (vm_flag && d_flag != NULL) {
		fprintf(stderr,
			"VM and host destination are mutually exclusive\n");
		return -1;
	}

	if (optind + 1 == argc) {
		const char *cmd = argv[optind];

		if (strcmp(cmd, "off") == 0) {
			++optind;
			ep.port = EC_PDC_TRACE_MSG_PORT_NONE;
			rv = ec_command(EC_CMD_PDC_TRACE_MSG_ENABLE, 0, &ep,
					sizeof(ep), &er, sizeof(er));
			if (rv < 0)
				return rv;
			return 0;
		}
	}

	if (optind != argc) {
		fprintf(stderr, "Usage: %s\n", cmd_pdc_trace_usage);
		return -1;
	}

	rv = ec_command(EC_CMD_PDC_TRACE_MSG_ENABLE, 0, &ep, sizeof(ep), &er,
			sizeof(er));
	if (rv < 0)
		return rv;

	if (w_flag != NULL) {
		pcap = pdc_pcap_open(w_flag);
		if (pcap == NULL)
			return -1;
	}

	const char *hn = NULL;

	if (vm_flag) {
		hn = pdc_net_get_vm_ip();
		if (hn == NULL) {
			pdc_pcap_close(pcap);
			return -1;
		}
	} else if (d_flag) {
		hn = d_flag;
	}

	if (hn != NULL) {
		int net_status = pdc_net_open(hn);
		if (net_status < 0) {
			fprintf(stderr,
				"could not set up network destination %s\n",
				hn);
			pdc_pcap_close(pcap);
			return -1;
		}
	}

	if (ep.port == EC_PDC_TRACE_MSG_PORT_ALL) {
		printf("tracing all ports\n");
	} else {
		printf("tracing port C%u\n", ep.port);
	}

	if (hn != NULL) {
		const char *d;

		if (vm_flag) {
			d = "VM";
		} else {
			d = "network";
		}

		printf("sending traces to %s destination %s:%u\n", d, hn,
		       USB_PDC_UDP_PORT);
	}

	if (s_flag)
		with_stdout = true;

	while (1) {
		size_t payload_size;

		rv = ec_command(EC_CMD_PDC_TRACE_MSG_GET_ENTRIES, 0, NULL, 0,
				gr, ec_max_insize);
		if (rv < 0)
			break;

		payload_size = gr->pl_size;

		if (payload_size == 0) {
			if (pcap != NULL)
				fflush(pcap);

			usleep(100 * 1000); /* 100 ms */
			continue;
		}

		pl_entries(gr->payload, payload_size, with_stdout);
	}

	pdc_pcap_close(pcap);
	pdc_net_close();

	/*
	 * Turn off tracing.
	 */
	ep.port = EC_PDC_TRACE_MSG_PORT_NONE;
	rv = ec_command(EC_CMD_PDC_TRACE_MSG_ENABLE, 0, &ep, sizeof(ep), &er,
			sizeof(er));
	if (rv < 0)
		return rv;

	return rv;
}

/*
 * PDC messages get a 3 byte header to provide additional context when
 * decoding:
 *
 *   byte 0: the Type-C port number
 *   byte 1: the direction of message (EC-RX vs. EC-TX)
 *   byte 2: message type for PDC chip type specific decoding
 */

#define TRACE_HDR_SIZE sizeof(struct pdc_trace_msg_header)

struct pdc_trace_msg_header {
	uint16_t seq_num;
	uint8_t port_num;
	uint8_t direction;
	uint8_t msg_type;
} __packed;

BUILD_ASSERT(sizeof(struct pdc_trace_msg_header) == 5);

static void pl_entries(const uint8_t *const data, size_t data_size,
		       bool with_stdout)
{
	uint8_t pcap_buf[500];
	const struct pdc_trace_msg_entry *e;
	size_t consumed_bytes = 0;

	for (;;) {
		size_t e_size;

		if (consumed_bytes >= data_size)
			break;

		e = (struct pdc_trace_msg_entry *)(data + consumed_bytes);
		e_size = sizeof(*e);
		if (consumed_bytes + e_size > data_size) {
			fprintf(stderr,
				"entry header out of bounds (%zu+%zu) > %zu\n",
				consumed_bytes, e_size, data_size);
			break;
		}

		e_size += e->pdc_data_size;
		if (consumed_bytes + e_size > data_size) {
			fprintf(stderr, "entry out of bounds (%zu+%zu) > %zu\n",
				consumed_bytes, e_size, data_size);
			break;
		}

		if (with_stdout) {
			int rxtx;

			if (e->direction)
				rxtx = 'T';
			else
				rxtx = 'R';
			printf("%3zu byte entry at %3zu/%zu: seq %5u C%u %cX T%02x",
			       e_size, consumed_bytes, data_size, e->seq_num,
			       e->port_num, rxtx, e->msg_type);
			for (int i = 0; i < e->pdc_data_size; ++i)
				printf(" %02x", e->pdc_data[i]);
			printf("\n");
		}

		size_t count = MIN(e->pdc_data_size,
				   sizeof(pcap_buf) - TRACE_HDR_SIZE);

		const struct pdc_trace_msg_header th = {
			.seq_num = e->seq_num,
			.port_num = e->port_num,
			.direction = e->direction,
			.msg_type = e->msg_type,
		};

		memcpy(pcap_buf, &th, sizeof(th));
		memcpy(&pcap_buf[sizeof(th)], e->pdc_data, count);

		if (pcap != NULL) {
			struct timeval tv;

			tv.tv_sec = e->time32_us / 1000000;
			tv.tv_usec = e->time32_us % 1000000;

			pdc_pcap_append(pcap, tv, pcap_buf,
					TRACE_HDR_SIZE + count);
		}

		pdc_net_out(pcap_buf, TRACE_HDR_SIZE + count);

		consumed_bytes += e_size;
	}
}
