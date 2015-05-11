/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Queue policies
 *
 * Queue policies describe how a queue behaves (who it notifies, in what
 * contexts) when units are added or removed from the queue.
 */
#ifndef INCLUDE_QUEUE_POLICIES_H
#define INCLUDE_QUEUE_POLICIES_H

#include "queue.h"
#include "consumer.h"
#include "producer.h"

/*
 * The NULL policy does no notification when units are added or removed from
 * the queue.
 */
extern struct queue_policy const queue_policy_null;

/*
 * The direct notification policy manages a 1-to-1 producer consumer model.
 * When new units are added to the queue the consumer is notified directly, in
 * whatever context (interrupt, deferred, task...) that the queue addition
 * happened.  Similarly, queue removals directly notify the producer.
 */
struct queue_policy_direct {
	struct queue_policy policy;

	struct producer const *producer;
	struct consumer const *consumer;
};

extern struct queue_policy_ops const queue_policy_direct_ops;

#define QUEUE_POLICY_DIRECT(NAME, PRODUCER, CONSUMER)		\
	static struct queue_policy_direct const NAME = {	\
		.policy = {					\
			.ops = &queue_policy_direct_ops,	\
		},						\
		.producer = &PRODUCER,				\
		.consumer = &CONSUMER,				\
	};
	

#endif /* INCLUDE_QUEUE_POLICIES_H */
