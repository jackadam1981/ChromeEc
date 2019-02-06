/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Header file for the EC command handler
 */

#include "consumer.h"
#include "producer.h"
#include "stdint.h"

struct ec_comm_config {
	struct producer const producer;
	struct consumer const consumer;
};

extern struct producer_ops const ec_comm_producer_ops;
extern struct consumer_ops const ec_comm_consumer_ops;

#define EC_COMM_PACKET_SIZE	32
typedef uint8_t ec_comm_packet[32];
