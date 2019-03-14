/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "crc8.h"
#include "flash_log.h"
#include "flash.h"
#include "hooks.h"
#include "shared_mem.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/*
 * A few assumptions this log facility design is based on are:
 *
 * - the log is stored in a flash space configured per board type, chip level
 *   functions are used for writing and erasing, flash space access control is
 *   transparent for the log facility.
 *
 * - log events are rare, attempts to log concurrent events could fail.
 *
 * - log events are retrieved by the host periodically, much sooner than log
 *   overflows
 *
 * With the above in mind, here is a basic design:
 *
 * The log is kept in one flash page. Entries are of variable size, as defined
 * by the header. On each startup, if the log is more than three quarters
 * full, the log flash space is erased and the top quarter of the log is
 * written back.
 *
 * If an entry would not fit into the log it is silently dropped.
 *
 * Log entries can not be written or read from within interrupt processing
 * routines.
 *
 * Only one log access can be processed at a time. Attempts to log new events
 * while a log entry is being saved will be silently ignored.
 *
 * At run time log compaction is attempted if it is more than 90% full. If
 * compaction is not possible and the new entry does not fit, it would be
 * silently dropped.
 *
 * because memory allocation fails), log entries larger than 8 bytes are being
 * added truncated to 8 bytes (aligned flash write size), with LOG_TRUNCATED
 * bit set in the .size field.
 */

static const void *log_offset_to_addr(uint16_t log_offset)
{
	return (const void *)(CONFIG_FLASH_LOG_BASE + log_offset);
}

struct log_read_context {
	uint16_t read_cursor;
	uint32_t prev_timestamp;
};

static uint16_t log_write_cursor;
static uint32_t log_stamp;
static uint8_t log_event_in_progress;
static uint8_t log_read_in_progress;
static struct log_read_context read_context;
static uint8_t compaction_count;
static void (*platform_flash_control)(int enable);

static void flash_log_erase(void)
{
	flash_physical_erase(CONFIG_FLASH_LOG_BASE - CONFIG_PROGRAM_MEMORY_BASE,
			     CONFIG_FLASH_LOG_SPACE);
}

static void flash_log_write(uint16_t log_offset, const void *data,
			    size_t data_size)
{
	flash_physical_write(log_offset + CONFIG_FLASH_LOG_BASE -
				     CONFIG_PROGRAM_MEMORY_BASE,
			     data_size, data);
}

static void flash_log_write_enable(void)
{
	if (platform_flash_control)
		platform_flash_control(1);
}

static void flash_log_write_disable(void)
{
	if (platform_flash_control)
		platform_flash_control(0);
}

/* Wrapper to avoid excessive typecasting throughout the rest of the file. */
static uint8_t calc_crc8(const void *buf, size_t size, uint8_t prev)
{
	return crc8_arg((const uint8_t *)buf, size, prev);
}

static int entry_is_valid(const struct flash_log_entry *r)
{
	size_t entry_size;
	uint32_t entry_offset;
	struct flash_log_entry copy;

	entry_size = FLASH_LOG_ENTRY_SIZE(r->size);
	entry_offset = (uintptr_t)r - CONFIG_FLASH_LOG_BASE;

	if ((entry_offset + entry_size) > CONFIG_FLASH_LOG_SPACE)
		return 0;

	copy = *r;
	copy.crc = 0;
	copy.crc = calc_crc8(&copy, sizeof(copy), 0);
	copy.crc = calc_crc8(r + 1, FLASH_LOG_PAYLOAD_SIZE(r->size), copy.crc);
	return (copy.crc == r->crc);
}

static void try_compacting(void)
{
	char *buf;
	uint16_t read_cursor = 0;
	uint16_t compac_cursor = 0;

	/* Try rewriting the top 25 of the log into its bottom. */
	/*
	 * Fist allocate a buffer large enough to keep a quarter of the
	 * log.
	 */
	if (shared_mem_acquire(COMPACTION_SPACE_PRESERVE, &buf) != EC_SUCCESS)
		return;

	while (read_cursor < log_write_cursor) {
		const struct flash_log_entry *r;
		size_t entry_space;

		r = log_offset_to_addr(read_cursor);
		if (!entry_is_valid(r))
			break;

		entry_space = FLASH_LOG_ENTRY_SIZE(r->size);

		if ((log_write_cursor - read_cursor) <=
		    COMPACTION_SPACE_PRESERVE) {
			memcpy(buf + compac_cursor, r, entry_space);
			compac_cursor += entry_space;
		}

		read_cursor += entry_space;
	}

	flash_log_write_enable();
	flash_log_erase();
	flash_log_write(0, buf, compac_cursor);
	log_write_cursor = compac_cursor;
	flash_log_write_disable();

	shared_mem_release(buf);

	compaction_count++;

	read_context.read_cursor = 0;
	read_context.prev_timestamp = 0;
}

void flash_log_add_event(uint8_t type, uint8_t size, void *payload)
{
	union {
		struct flash_log_entry r;
		uint8_t buf[FLASH_LOG_ENTRY_SIZE(0)];
	} header;

	size_t total_size;
	const size_t payload_offset =
		MIN(sizeof(header) - sizeof(header.r), size);

	if (size > MAX_FLASH_LOG_PAYLOAD_SIZE)
		return;

	if (in_interrupt_context())
		return;

	/*
	 * --- critical section ---
	 *
	 * try to grab the resource. If not avialable, try at least posting a
	 * truncated event.
	 */
	interrupt_disable();
	if (log_event_in_progress) {
		/* What a coincidence! */
		interrupt_enable();
		return;
	}
	log_event_in_progress = 1;
	interrupt_enable();
	/* --- end of critical section --- */

	/* The entry will take this much space in the flash. */
	total_size = FLASH_LOG_ENTRY_SIZE(size);

	if ((log_write_cursor > RUN_TIME_LOG_FULL_WATERMARK) &&
	    !log_read_in_progress)
		try_compacting();

	if (total_size > (CONFIG_FLASH_LOG_SPACE - log_write_cursor))
		/*
		 * Compaction must have failed or was not allowed, and no room
		 * to log.
		 */
		goto log_add_exit;

	/* Ok, there is room to save the entry in, let's do it. */
	header.r.timestamp = ++log_stamp;
	header.r.size = size;
	header.r.type = type;
	header.r.crc = 0;

	/* Calculate CRC in two shots. */
	header.r.crc = calc_crc8(&header.r, sizeof(header.r), 0);
	if (size) {
		header.r.crc = calc_crc8(payload, size, header.r.crc);

		/* Now fill the aligned structure to capacity. */
		memcpy(&header.r + 1, payload, payload_offset);
	}

	/* Call platform provided function to enable flash access. */
	flash_log_write_enable();
	flash_log_write(log_write_cursor, &header, sizeof(header));
	log_write_cursor += sizeof(header);
	total_size -= sizeof(header);
	if (size > payload_offset)
		flash_log_write(log_write_cursor,
				(const uint8_t *)payload + payload_offset,
				total_size);
	flash_log_write_disable();

	log_write_cursor += total_size;

log_add_exit:
	/* No need for critical section in this case. */
	log_event_in_progress = 0;
}

int flash_log_dequeue_event(uint32_t event_after, void *buffer,
			    size_t buffer_size)
{
	const struct flash_log_entry *r;
	int rv = 0;
	size_t copy_size;

	/*
	 * It is OK to read during while a new event is added, but we sure
	 * want to prevent compaction during log read.
	 */
	/*
	 * --- critical section ---
	 *
	 * Do not try reading if log event is in progress, let the caller try
	 * again.
	 */
	interrupt_disable();
	if (log_event_in_progress) {
		/*
		 * Just in case there is a compaction, let's tell the caller
		 * try again.
		 */
		interrupt_enable();
		return -EC_ERROR_BUSY;
	}
	log_read_in_progress = 1;
	interrupt_enable();
	/* --- end of critical section --- */

	if (!event_after | (event_after != read_context.prev_timestamp)) {
		/* Will have to start over. */
		read_context.read_cursor = 0;
		read_context.prev_timestamp = 0;
	}

	if (read_context.read_cursor > (CONFIG_FLASH_LOG_SPACE - sizeof(*r)))
		/* No more room in the log, should never happen. */
		goto log_read_exit;

	do {
		r = log_offset_to_addr(read_context.read_cursor);
		if (r->timestamp == ~0)
			/* Points at erased space, no more entries. */
			goto log_read_exit;

		if (!entry_is_valid(r)) {
			rv = -EC_ERROR_INVAL;
			goto log_read_exit;
		}

		read_context.read_cursor += FLASH_LOG_ENTRY_SIZE(r->size);

	} while (r->timestamp <= event_after);

	/*
	 * If we are here, we found the next event, let's see if it fits into
	 * the buffer.
	 */
	copy_size = FLASH_LOG_PAYLOAD_SIZE(r->size) + sizeof(*r);
	if (copy_size > buffer_size) {
		rv = -EC_ERROR_MEMORY_ALLOCATION;
		/* To be on the safe side will start over next time. */
		read_context.read_cursor = 0;
		read_context.prev_timestamp = 0;
		goto log_read_exit;
	}

	read_context.prev_timestamp = r->timestamp;
	memcpy(buffer, r, copy_size);
	rv = copy_size;

log_read_exit:
	log_read_in_progress = 0;
	return rv;
}

void flash_log_register_flash_control_callback(
	void (*flash_control)(int enable))
{
	platform_flash_control = flash_control;
}

test_export_static void flash_log_init(void)
{
	uint16_t read_cursor = 0;
	const struct flash_log_entry *r;

	r = log_offset_to_addr(read_cursor);
	while (entry_is_valid(r)) {
		log_stamp = r->timestamp + 1;
		read_cursor += FLASH_LOG_ENTRY_SIZE(r->size);
		r = log_offset_to_addr(read_cursor);
	}

	log_write_cursor = read_cursor;

	flash_log_write_enable();
	if (r->timestamp != ~0) {
		/* Log space must be corrupted, compact it. */
		try_compacting();
		flash_log_add_event(FE_LOG_CORRUPTED, 0, NULL);
		flash_log_write_disable();
		return;
	}

	/*
	 * Timestamp field is set to all ones, presumably this points to free
	 * space in the log.
	 *
	 * Is there anything at all in the log?
	 */
	if (read_cursor) {
		/*
		 * Next write will have to come here unless compacting changes
		 * that.
		 */
		if (read_cursor > STARTUP_LOG_FULL_WATERMARK)
			try_compacting();
	} else {
		flash_log_add_event(FE_LOG_START, 0, NULL);
	}
	flash_log_write_disable();
}
DECLARE_HOOK(HOOK_INIT, flash_log_init, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CMD_FLASH_LOG
/*
 * Display Flash event log.
 */
static int command_flash_log(int argc, char **argv)
{
	uint32_t stamp = 0;
	union entry_u e;
	int rv;
	uint32_t type;
	size_t size;
	size_t i;

	if (argc > 1) {
		if (!strcasecmp(argv[1], "-e")) {
			ccprintf("Erasing flash log\n");
			flash_log_write_enable();
			flash_log_erase();
			flash_log_write_disable();
			argc--;
			argv++;
		}
	}
	if (argc < 3) {
		if (argc == 2)
			stamp = atoi(argv[1]);

		/* Retrieve entries newer than 'stamp'. */
		while ((rv = flash_log_dequeue_event(stamp, e.entry,
						     sizeof(e))) > 0) {
			size_t i;

			ccprintf("%08x:%02x", e.r.timestamp, e.r.type);
			for (i = 0; i < FLASH_LOG_PAYLOAD_SIZE(e.r.size); i++) {
				if (i && !(i % 16)) {
					ccprintf("\n           ");
					cflush();
				}
				ccprintf(" %02x", e.r.payload[i]);
			}
			ccprintf("\n");
			stamp = e.r.timestamp;
		}
		if (rv)
			ccprintf("Warning: Last attempt to dequeue returned "
				 "%d\n",
				 rv);
		return EC_SUCCESS;
	}

	if (argc != 3) {
		ccprintf("type and size of the entry are required\n");
		return EC_ERROR_PARAM_COUNT;
	}

	type = atoi(argv[1]);
	size = atoi(argv[2]);

	if (type >= FLASH_LOG_NO_ENTRY) {
		ccprintf("type must not exceed %d\n", FLASH_LOG_NO_ENTRY - 1);
		return EC_ERROR_PARAM2;
	}

	if (size > MAX_FLASH_LOG_PAYLOAD_SIZE) {
		ccprintf("size must not exceed %d\n",
			 MAX_FLASH_LOG_PAYLOAD_SIZE);
		return EC_ERROR_PARAM3;
	}

	for (i = 0; i < size; i++)
		e.r.payload[i] = type + i;
	flash_log_add_event(type, size, e.r.payload);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(flog, command_flash_log,
			"[-e] ][[stamp]|[<type> <size>]]",
			"Dump on the console the flash log contents,"
			"optionally erasing it\n"
			"or add a new entry of <type> and <size> bytes");
#endif
