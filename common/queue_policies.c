/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Queue policies
 */
#include "queue_policies.h"
#include "util.h"

#include <stddef.h>

/*****************************************************************************/
static void queue_add_null(struct queue_policy const *policy, size_t count)
{
}

static void queue_remove_null(struct queue_policy const *policy, size_t count)
{
}

static struct queue_policy_ops const queue_policy_null_ops = {
	.add    = queue_add_null,
	.remove = queue_remove_null,
};

struct queue_policy const queue_policy_null = {
	.ops = &queue_policy_null_ops,
};
/*****************************************************************************/
static void queue_add_direct(struct queue_policy const *policy, size_t count)
{
	struct queue_policy_direct const *direct =
		DOWNCAST(policy, struct queue_policy_direct, policy);

	if (count && direct->producer->ops->read)
		direct->producer->ops->read(direct->producer, count);
}

static void queue_remove_direct(struct queue_policy const *policy, size_t count)
{
	struct queue_policy_direct const *direct =
		DOWNCAST(policy, struct queue_policy_direct, policy);

	if (count && direct->consumer->ops->written)
		direct->consumer->ops->written(direct->consumer, count);
}

struct queue_policy_ops const queue_policy_direct_ops = {
	.add    = queue_add_direct,
	.remove = queue_remove_direct,
};
/*****************************************************************************/
