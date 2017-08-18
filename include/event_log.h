/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_EVENT_LOG_H
#define __CROS_EC_EVENT_LOG_H

struct event_log_entry {
	uint32_t timestamp; /* relative timestamp in milliseconds */
	uint8_t type;       /* event type, caller-defined */
	uint8_t size;       /* [7:5] caller-def'd [4:0] payload size in bytes */
	uint16_t data;      /* type-defined data payload */
	uint8_t payload[0]; /* optional additional data payload: 0..16 bytes */
} __packed;

#define EVENT_LOG_SIZE_MASK  0x1f
#define EVENT_LOG_SIZE(size) ((size) & EVENT_LOG_SIZE_MASK)

/* The timestamp is the microsecond counter shifted to get about a ms. */
#define EVENT_LOG_TIMESTAMP_SHIFT 10 /* 1 LSB = 1024us */
/* Returned in the "type" field, when there is no entry available */
#define EVENT_LOG_NO_ENTRY 0xff

/* Add an entry to the event log. */
void log_add_event(uint8_t type, uint8_t size, uint16_t data,
		   void *payload, uint32_t timestamp);

/*
 * Remove and return an entry from the event log, if available.
 * Returns size of log entry *r.
 */
int log_dequeue_event(struct event_log_entry *r);

/*
 * Peek into the log events withoug changing the log contents.
 *
 * The passed in pointer needs to be large enough to store the largest
 * possible event entry, which is 24 bytes for this implementation.
 *
 * If the passed in entry has the timestamp field set to 0, the function
 * returns the newest available log entry. Each following invocation with
 * timestamp field set to a non-zero value would return the entry one before
 * the current one, until all available entries in the log have been returned.
 *
 * Returns the size of the retrieved entry, or zero if no entry was found.
 *
 * Returning entries in the reverse chronological order allows to smoothly
 * traverse the log.
 *
 * If traversal in the chronological order were attempted, there always would
 * be a chance of the older log entries disappearing between
 * log_peek_prev_event() invocations, resulting in inaccurate representation
 * of the log entries sequence.
 *
 * Note that this function KEEPS A SINGLE CONTEXT, so if more than one thread
 * attempt to traverse the log concurrently the result would be a corrupted
 * sequence of log events.
 */
int log_peek_prev_event(struct event_log_entry *r);
#endif /* __CROS_EC_EVENT_LOG_H */
