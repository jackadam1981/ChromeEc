#include "console.h"
#include "hooks.h"
#include "ipi_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "link_defs.h"
#include "memmap.h"

static uint32_t v;

static uint32_t sram_v;

__SECTION(dram.bss)
static uint32_t dram_v;

static void demo_ipi_handler(int id, void *data, uint32_t len)
{
	if (!len) {
		ccprintf("len is zero.");
		return;
	}

	v = *(uint32_t *)data;
	ccprintf("ipi recv data %d len %d\n", v, len);

	task_set_event(TASK_ID_DEMO, 0x1);
}
DECLARE_IPI(SCP_IPI_DEMO, demo_ipi_handler, 0);


void demo_task(void *u)
{
	while (1) {
		int ret;
		uintptr_t addr;

		task_wait_event_mask(0x1, -1);

		if (v == 1) {
			memmap_scp_to_ap((uintptr_t)&sram_v, &addr);
			ccprintf("&sram %x addr %x\n", (uintptr_t)&sram_v, addr);
		} else {
			memmap_scp_to_ap((uintptr_t)&dram_v, &addr);
			ccprintf("&sram %x addr %x\n", (uintptr_t)&dram_v, addr);
		}
		v = addr;

		ret = ipi_send(SCP_IPI_DEMO, (void *)&v, sizeof(v), 1000);
		ccprintf("ipi reply %x ret %d\n", v, ret);
	}
}
