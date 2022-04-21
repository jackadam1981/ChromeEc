#include "console.h"
#include "ipi_chip.h"
#include "registers.h"
#include "task.h"
#include "util.h"
#include "compile_time_macros.h"
#include "memmap.h"
#include "link_defs.h"

struct echo_test {
	uint32_t v;
	uint32_t p;
	uint32_t dram_pa;
	uint32_t dram_sz;
};

struct echo_test rx;

static void echo_ipi_handler(int id, void *data, unsigned int len)
{
	if (!len) {
		ccprintf("len is zero.");
		return;
	}

	memcpy(&rx, data, MIN(len, sizeof(rx)));

	ccprintf("id %d rx v %u p %x\n", id, rx.v, (uint32_t)rx.p);

	task_wake(TASK_ID_IPI_TEST);
}
DECLARE_IPI(SCP_IPI_DEBUG, echo_ipi_handler, 0);

__SECTION(dramnc) static int dramnc_test[256 / 4];

__SECTION(dram.bss) static int dram_test[256 / 4];

void ipi_test_task(void *u)
{
	int ret;

	while (1) {
		task_wait_event(-1);
		if (rx.dram_pa != 0) {
			int max = 256 / 4;
			int i;
			uintptr_t ap;

			if (rx.dram_pa == 1) {
				for (i = 0; i < max; i++) {
					dramnc_test[i] = rx.v + i;
				}
				memmap_scp_to_ap((uintptr_t)dramnc_test, &ap);
				rx.dram_pa = ap;
				rx.dram_sz = sizeof(dramnc_test);
			} else {
				for (i = 0; i < max; i++) {
					dram_test[i] = rx.v + i;
				}
				memmap_scp_to_ap((uintptr_t)dram_test, &ap);
				rx.dram_pa = ap;
				rx.dram_sz = sizeof(dram_test);
			}
			ccprintf("ap %x\n", ap);
		}

		/* ACK */
		ret = ipi_send(SCP_IPI_DEBUG, &rx, sizeof(rx), 1);
		ccprintf("task ack ret %d\n", ret);
	}
}
