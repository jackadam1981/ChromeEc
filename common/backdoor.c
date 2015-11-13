/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "backdoor.h"
#include "byteorder.h"
#include "task.h"
#include "tpm_registers.h"

static struct subcommand_entry {
	uint16_t subcommand_code;
	backdoor_handler subcommand_handler;
} handlers[20];  /* This should be fine tuned. */

static struct mutex backdoor_mutex;
static unsigned handler_count;

static uint8_t response_buffer[2048];
static const uint16_t header_size = sizeof(struct tpm_cmd_header);

struct subcommand_entry *find_entry(uint16_t subcommand)
{
	unsigned i;

	for (i = 0; i < handler_count; i++)
		if (handlers[i].subcommand_code == subcommand)
				return handlers + i;
	return NULL;
}
void backdoor_route_command(struct tpm_cmd_header *tpmh,
			    unsigned *out_size,
			    uint8_t **out_buffer)
{
	unsigned command_size = be32toh(tpmh->size);
	struct tpm_cmd_header *tch;



	/* This covers the case of the handler not found. */
	tch = (struct tpm_cmd_header *)response_buffer;
	memcpy(tch, tpmh, sizeof(*tch));
	*out_buffer = (uint8_t *)tch;
	*out_size = htobe32(sizeof(*tch));

	if (command_size >= header_size) {
		/*
		 * This is a valid size packet, let's see if there is a
		 * handler for this subcommand.
		 */
		uint16_t subcommand_code;
		struct subcommand_entry *entry;
		backdoor_handler handler;

		subcommand_code = be16toh(tpmh->subcommand_code);
		mutex_lock(&backdoor_mutex);

		/* Is there a registered subcommand? */
		entry = find_entry(subcommand_code);
		if (entry)
			handler = entry->subcommand_handler;
		else
			handler = NULL;

		mutex_unlock(&backdoor_mutex);

		/*
		 * If subcommand is found, execute it and prepare the
		 * response.
		 */
		if (handler) {
			size_t response_size;

			response_size = sizeof(response_buffer) - header_size;

			handler(tpmh + 1,
				be32toh(tpmh->size) - header_size,
				tch + 1,
				&response_size);
			/*
			 * Now response_size is the size of the data returned
			 * by the invoked command.
			 */
			response_size += header_size;
			*out_size = response_size;

			/* Plug in proper header fields. */
			tch->tag = tpmh->tag;
			tch->command_code = tpmh->command_code;
			tch->size = htobe32(response_size);
			tch->subcommand_code = tpmh->subcommand_code;
		}
	}
}

int backdoor_register_handler(uint16_t subcommand_code,
			      backdoor_handler handler)
{
	int rv = 0; /* This means failure. */

	mutex_lock(&backdoor_mutex);

	if (handler_count < ARRAY_SIZE(handlers)) {
		struct subcommand_entry *entry;

		entry = find_entry(subcommand_code);
		if (!entry) {
			handlers[handler_count].subcommand_code
				= subcommand_code;
			handlers[handler_count].subcommand_handler = handler;
			handler_count++;
			rv = 1;
		}
	}
	mutex_unlock(&backdoor_mutex);

	return rv;
}

int backdoor_unregister_handler(uint16_t subcommand_code)
{
	struct subcommand_entry *entry;
	int rv = 0; /* This means failure */

	mutex_lock(&backdoor_mutex);

	entry = find_entry(subcommand_code);
	if (entry) {
		int entry_index = entry - handlers;

		rv = 1;
		handler_count--;

		if (entry_index != handler_count)
			handlers[entry_index] = handlers[handler_count];
	}

	mutex_unlock(&backdoor_mutex);

	return rv;
}
