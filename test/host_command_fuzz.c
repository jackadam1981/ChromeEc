/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Fuzz host command.
 */

#include <pthread.h>
#include <sys/time.h>

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "host_test.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#define TASK_EVENT_FUZZ TASK_EVENT_CUSTOM(1)
#define TASK_EVENT_HOSTCMD_DONE TASK_EVENT_CUSTOM(2)

/* Request/response buffer size (and maximum command length) */
#define BUFFER_SIZE 128

struct host_packet pkt;
static uint8_t resp_buf[BUFFER_SIZE];
static uint8_t req_buf[BUFFER_SIZE];
static struct ec_host_request *req = (struct ec_host_request *)req_buf;

static void hostcmd_respond(struct host_packet *pkt)
{
	task_set_event(TASK_ID_TEST_RUNNER, TASK_EVENT_HOSTCMD_DONE, 0);
}

static char calculate_checksum(const char *buf, int size)
{
	int c = 0;
	int i;

	for (i = 0; i < size; ++i)
		c += buf[i];

	return -c;
}

static int hostcmd_fill(const uint8_t *data, size_t size)
{
	const int checksum_offset = offsetof(struct ec_host_request, checksum);
	const int checksum_size = sizeof(req->checksum);

	/* Not enough space in buffer. */
	if (size > (BUFFER_SIZE - checksum_size))
		return -1;

	/*
	 * TODO(chromium:854975): We should probably malloc req_buf with the
	 * correct size, to make we do not read uninitialized req_buf data.
	 */
	memset(req_buf, 0, sizeof(req_buf));

	/*
	 * Skip checksum when filling in data, as we only try commands with
	 * valid checksum.
	 */
	memcpy(req_buf, data, MIN(checksum_offset, size));
	if (size > checksum_offset)
		memcpy(req_buf + checksum_offset + checksum_size,
			data + checksum_offset, size - checksum_offset);

	req->checksum = calculate_checksum(req_buf,
					MIN(sizeof(req_buf), pkt.request_size));

	pkt.request_size = 0;
	pkt.send_response = hostcmd_respond;
	pkt.request = (const void *)req_buf;
	pkt.request_max = BUFFER_SIZE;
	pkt.request_size = size;
	pkt.response = (void *)resp_buf;
	pkt.response_max = BUFFER_SIZE;
	pkt.driver_result = 0;

	return 0;
}

static pthread_cond_t done_cond;
static pthread_mutex_t lock;

void run_test(void)
{
	ccprints("Fuzzing task started");
	wait_for_task_started();

	while (1) {
		task_wait_event_mask(TASK_EVENT_FUZZ, -1);

		/* Send the host command (pkt prepared by main thread). */
		host_packet_receive(&pkt);
		task_wait_event_mask(TASK_EVENT_HOSTCMD_DONE, -1);
		pthread_cond_signal(&done_cond);
	}
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	/* Fill in req_buf. */
	if (hostcmd_fill(data, size) < 0)
		return 0;

	task_set_event(TASK_ID_TEST_RUNNER, TASK_EVENT_FUZZ, 0);
	pthread_cond_wait(&done_cond, &lock);

	return 0;
}

