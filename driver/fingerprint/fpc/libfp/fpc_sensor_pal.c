/* Copyright 2017 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* FPC Platform Abstraction Layer callbacks */

#include "common.h"
#include "console.h"
#include "fpc_sensor_pal.h"
#include "fpsensor.h"
#include "fpsensor_utils.h"
#include "shared_mem.h"
#include "spi.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

//#define FPC_MEM_CHECK /* uncomment to add fpc memory check for malloc/free */

#ifdef FPC_MEM_CHECK
#define FPC_MEM_DEBUG /* uncomment to track fpc bep memory using */

#pragma pack(push, mem_hdr_t, 1)

typedef struct _mem_chain_t {
    struct _mem_chain_t *next;
#ifdef FPC_MEM_DEBUG
	size_t alloc_size;
#endif
	uint8_t mem[];
} mem_chain_t;

#pragma pack(pop, mem_hdr_t)

static mem_chain_t *mem_chain_start = NULL;
static mem_chain_t *mem_chain_end = NULL;
#ifdef FPC_MEM_DEBUG

#include "ec_commands.h"

static uint32_t mem_alloc_total = 0;
static uint32_t mem_member_total = 0;
#endif

#endif

void fpc_pal_log_entry(const char *tag, int log_level, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	uart_puts(tag);
	uart_vprintf(format, args);
	va_end(args);
}

int fpc_pal_delay_us(uint64_t us)
{
	if (us > 250)
		usleep(us);
	else
		udelay(us);
	return 0;
}

int fpc_pal_spi_writeread(fpc_device_t device, uint8_t *tx_buf, uint8_t *rx_buf,
			  uint32_t size)
{
	return spi_transaction(SPI_FP_DEVICE, tx_buf, size, rx_buf,
			       SPI_READBACK_ALL);
}

int fpc_pal_wait_irq(fpc_device_t device, fpc_pal_irq_t irq_type)
{
	/* TODO: b/72360575 */
	return EC_SUCCESS; /* just lie about it, libfpsensor prefers... */
}

int32_t FpcMalloc(void **data, size_t size)
{
#ifdef FPC_MEM_CHECK
#ifdef FPC_MEM_DEBUG
	ccprintf("fpc malloc %d bytes at ", size);
#endif
	if (!size) {
		*data = NULL;
		return -1;
	}

	mem_chain_t *mem_member;
	int rc = shared_mem_acquire(sizeof(mem_chain_t) + size, (char **)&mem_member);
	if (rc != EC_SUCCESS) {
		ccprintf("shared_mem_acquire %d bytes failed\n", sizeof(mem_chain_t) + size);
		return -1;
	}
	*data = mem_member->mem;

#ifdef FPC_MEM_DEBUG
	mem_member->alloc_size = size;
	mem_alloc_total += mem_member->alloc_size;
	mem_member_total ++;
	ccprintf("0x%p, member_total: %u, alloc_total: %u\n", *data, mem_member_total, mem_alloc_total);
#endif
	mem_member->next = NULL;

	if (!mem_chain_start) {
		mem_chain_start = mem_member;
	}

	if (mem_chain_end) {
		mem_chain_end->next = mem_member;
	}

	mem_chain_end = mem_member;

	return 0;
#else
	return shared_mem_acquire(size, (char **)data);
#endif
}

void FpcFree(void **data)
{
#ifdef FPC_MEM_CHECK

#ifdef FPC_MEM_DEBUG
	ccprintf("fpc free at 0x%p ", *data);
#endif

	if (*data == NULL)
		return;

	mem_chain_t *mem_this = mem_chain_start;
	mem_chain_t *mem_prev = NULL;

	while (mem_this) {
		if (mem_this->mem == *data) {
			if (mem_this == mem_chain_start) {
				mem_chain_start = mem_this->next;
			} else if (mem_this == mem_chain_end) {
				mem_chain_end = mem_prev;
				if (mem_prev)
					mem_prev->next = NULL;
			} else {
				mem_prev->next = mem_this->next;
			}
#ifdef FPC_MEM_DEBUG
			ccprintf("%u bytes, ", mem_this->alloc_size);
			mem_alloc_total -= mem_this->alloc_size;
			mem_member_total --;
			ccprintf("member_total = %u, alloc_total = %u\n", mem_member_total, mem_alloc_total);
#endif
			shared_mem_release(mem_this);
			*data = NULL;
			return;
		}
		mem_prev = mem_this;
		mem_this = mem_this->next;
	}

#ifdef FPC_MEM_DEBUG
	ccprintf("\n0x%p is already freed\n", *data);
#endif

	*data = NULL;

#else
	shared_mem_release(*data);
#endif
}

#if defined(FPC_MEM_CHECK) && defined(FPC_MEM_DEBUG)
static int command_fpcheck(int argc, const char **argv)
{
	mem_chain_t *member = mem_chain_start;

	ccprintf("member_total = %u, alloc_total = %u\n", mem_member_total, mem_alloc_total);
	while (member) {
		ccprintf("0x%p has %u bytes\n", member->mem, member->alloc_size); 
		member = member->next;
	}
	return 0;
}

DECLARE_CONSOLE_COMMAND(fpcheck, command_fpcheck, NULL,
			"Run fpc memory check");
#endif
