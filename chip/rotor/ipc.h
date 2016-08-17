/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Inter-Processor Communication module for Rotor MCU */

#include "common.h"

/**
 * Get a buffer to use to write for an IPC channel.
 *
 * The first buffer for the channel will be used by the other core, so we use
 * the second buffer.
 *
 * @param channel	IPC channel which you wish to use.
 *
 * @return A pointer to a buffer for your IPC channel.
 */
uint32_t *get_ipc_buffer(uint8_t channel);

/**
 * Send a message to a partner core.
 *
 * NOTE: The caller MUST have their message already prepared before calling this
 * function.
 *
 * @param channel	IPC channel to send the message through.
 * @param src		Pointer to message to be sent.
 * @param bit		Which bit to mark in interrupt set register.
 *
 * @return EC_SUCCESS on success, EC_ERROR_INVAL if channel is wrong,
 *		EC_ERROR_TIMEOUT if TX is currently not allowed.
 */
int send_message(uint8_t channel, uint32_t *src, uint8_t bit);
