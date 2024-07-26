/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "ectool.h"
#include "ectool_pdc_net.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <endian.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

/*
 * "connection refused" errors go away when there's a listener:
 *
 * for example, using "netcat -l -u 2012"
 *
 */
int pdc_net_open(const char *hostname)
{
	struct addrinfo dst_want;
	struct addrinfo *dst_info, *d;
	int so;

	memset(&dst_want, 0, sizeof(dst_want));
	dst_want.ai_flags = AI_NUMERICSERV;
	dst_want.ai_family = AF_UNSPEC;
	dst_want.ai_socktype = SOCK_DGRAM;
	dst_want.ai_protocol = IPPROTO_UDP;

	if (getaddrinfo(hostname, STRINGIFY(USB_PDC_UDP_PORT), &dst_want,
			&dst_info) != 0) {
		fprintf(stderr, "getaddrinfo %s:%s failed\n", hostname,
			STRINGIFY(USB_PDC_UDP_PORT));
		return -1;
	}

	d = dst_info;
	if (d == NULL)
		return -1;

	so = socket(d->ai_family, d->ai_socktype, d->ai_protocol);
	if (so < 0) {
		fprintf(stderr, "could not create socket: %s\n",
			strerror(errno));
		return -1;
	}

	if (connect(so, d->ai_addr, d->ai_addrlen) < 0) {
		fprintf(stderr, "could not connect socket: %s\n",
			strerror(errno));
		close(so);
		return -1;
	}

	return so;
}

void pdc_net_out(int udp_tx_fd, const void *buf, size_t buf_len)
{
	if (udp_tx_fd < 0)
		return;

	/*
	 * the 1st send to crostini/VM spuriously fails with
	 * "connection refused" when there has been no traffic
	 * for a while.
	 */

	int retries = 1;

	do {
		ssize_t cc = send(udp_tx_fd, buf, buf_len, 0);
		if (cc > 0)
			break;
		if (cc < 0 && retries == 0)
			perror("send");
	} while (retries-- > 0);
}

void pdc_net_close(int udp_tx_fd)
{
	if (udp_tx_fd < 0)
		return;

	close(udp_tx_fd);
}
