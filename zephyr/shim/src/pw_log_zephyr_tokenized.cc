/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log_core.h>
#include <zephyr/spinlock.h>

#include <pw_log_tokenized/base64.h>
#include <pw_log_tokenized/config.h>
#include <pw_log_tokenized/handler.h>
#include <pw_log_tokenized/metadata.h>

extern "C" {
#include "zephyr_console_shim.h"
/* including console.h will chain other files to be included.
 * extern function of interest only */
enum console_channel {
#define CONSOLE_CHANNEL(enumeration, string) enumeration,
#include "console_channel.inc"
#undef CONSOLE_CHANNEL

	/* Channel count; not itself a channel */
	CC_CHANNEL_COUNT
};
bool console_channel_is_disabled(enum console_channel channel);
}

namespace pw::log_zephyr
{
namespace
{
	// The Zephyr console may output raw text along with Base64 tokenized
	// messages, which could interfere with detokenization. Output a
	// character to mark the end of a Base64 message.
	// If delimiter changes, make sure to update the following files to
	// match
	//  -- src/third_party/hdctools/servo/ec3po/console.py
	constexpr char kEndDelimiter = '~';

	struct k_spinlock lock;
	k_spinlock_key_t key;
} // namespace

extern "C" void pw_log_tokenized_HandleLog(uint32_t metadata,
					   const uint8_t log_buffer[],
					   size_t size_bytes)
{
	pw::log_tokenized::Metadata meta(metadata);

	if (IS_ENABLED(CONFIG_CONSOLE_CHANNEL)) {
		if (console_channel_is_disabled(
			    (enum console_channel)meta.flags())) {
			return;
		}
	}

	// Encode the tokenized message as Base64.
	InlineBasicString base64_string =
		log_tokenized::PrefixedBase64Encode(log_buffer, size_bytes);

	if (base64_string.empty()) {
		return;
	}

	// On DUT, timberslide doesn't receive console raw text, okay to send
	// base64 message without end delimiter
	if (IS_ENABLED(CONFIG_PLATFORM_EC_HOSTCMD_CONSOLE)) {
		console_buf_notify_chars(base64_string.c_str(),
					 base64_string.size());
	}
	base64_string += kEndDelimiter;

	// TODO(asemjonovs):
	// https://github.com/zephyrproject-rtos/zephyr/issues/59454 Zephyr
	// frontend should protect messages from getting corrupted from multiple
	// threads.
	key = k_spin_lock(&lock);
	LOG_PRINTK("%s", base64_string.c_str());
	k_spin_unlock(&lock, key);
}

} // namespace pw::log_zephyr
