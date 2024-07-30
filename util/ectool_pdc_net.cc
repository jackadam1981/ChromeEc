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

static const char id_hash_query[] =
	"ls -l /home/root | grep daemon-store | awk '{print $NF}'";
static const char ip_query_template[] =
	"vsh --vm_name=termina --owner_id='%s' --target_container=penguin -- "
	"ip -4 -br -j addr show eth0 | "
	"sed -n -e 's/.*local\":\"\\([0-9.]\\+\\)\",.*/\\1/p'";

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

	dst_want = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = IPPROTO_UDP,
	};

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

/*
 * get the IP addr of the "linux" VM where wireshark can run
 */
const char *pdc_net_get_vm_ip(void)
{
	char id_hash[500];
	char ip_query[500];
	static char ip_buf[500];
	FILE *fp;
	int sh_status;
	size_t sz;

	fp = popen(id_hash_query, "r");
	if (fp == NULL) {
		fprintf(stderr, "popen \"%s\" failed\n", id_hash_query);
		return NULL;
	}

	sz = fread(id_hash, 1, sizeof(id_hash), fp);

	sh_status = pclose(fp);
	if (sz == 0 || sh_status != 0) {
		fprintf(stderr,
			"popen \"%s\" returned %zu bytes and exit status %d\n",
			id_hash_query, sz, sh_status);
		fprintf(stderr, "is anyone logged into chromeos?\n");
		return NULL;
	}

	if (sz > 0)
		id_hash[sz - 1] = '\0';

	printf("found CROS_USER_ID_HASH=\"%s\"\n", id_hash);

	snprintf(ip_query, sizeof(ip_query), ip_query_template, id_hash);

	fp = popen(ip_query, "r");
	if (fp == NULL) {
		fprintf(stderr, "popen \"%s\" failed\n", ip_query);
		return NULL;
	}

	sz = fread(ip_buf, 1, sizeof(ip_buf), fp);

	sh_status = pclose(fp);
	if (sz == 0 || sh_status != 0) {
		fprintf(stderr,
			"popen \"%s\" returned %zu bytes and exit status %d\n",
			ip_query, sz, sh_status);
		fprintf(stderr, "is the linux VM running?\n");
		return NULL;
	}

	if (sz > 0)
		ip_buf[sz - 1] = '\0';

	printf("found crostini IP=\"%s\"\n", ip_buf);

	return ip_buf;
}
