/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* MT SCP RV32i configuration */

#include "cache.h"
#include "csr.h"
#include "hooks.h"
#include "registers.h"

#define SCP_SRAM_END (CONFIG_IPC_SHARED_OBJ_ADDR & (~(0x400 - 1)))

struct mpu_entry mpu_entries[NR_MPU_ENTRIES] = {
	/* SRAM (for most code, data) */
	{0, SCP_SRAM_END, MPU_ATTR_C | MPU_ATTR_W | MPU_ATTR_R},
	/* SRAM (for IPI shared buffer) */
	{SCP_SRAM_END, SCP_FW_END, MPU_ATTR_W | MPU_ATTR_R},
	/* For AP domain */
#ifdef CHIP_VARIANT_MT8195
	{0x60000000, 0x70000000, MPU_ATTR_W | MPU_ATTR_R | MPU_ATTR_P},
#else
	{0x60000000, 0x70000000, MPU_ATTR_W | MPU_ATTR_R},
#endif
	/* For SCP sys */
	{0x70000000, 0x80000000, MPU_ATTR_W | MPU_ATTR_R},
#ifdef CHIP_VARIANT_MT8195
#ifdef CHIP_VARIANT_MT8195_CORE1
	{0x10A00000, 0x113FF000, MPU_ATTR_C | MPU_ATTR_W | MPU_ATTR_R},
	{0x113FF000, 0x11400000, MPU_ATTR_W | MPU_ATTR_R},
#elif defined(BOARD_DRAGONFRUIT_SCP_CORE0)
	{0x10000000, 0x109FF000, MPU_ATTR_C | MPU_ATTR_W | MPU_ATTR_R},
	{0x109FF000, 0x10A00000, MPU_ATTR_W | MPU_ATTR_R},
#else
	{0x10000000, 0x113FF000, MPU_ATTR_C | MPU_ATTR_W | MPU_ATTR_R},
	{0x113FF000, 0x11400000, MPU_ATTR_W | MPU_ATTR_R},
#endif
#else
	{0x10000000, 0x11400000, MPU_ATTR_W | MPU_ATTR_R},
#endif
};

#include "gpio_list.h"

#ifdef CHIP_VARIANT_MT8195
static void core0_boot_done(void)
{
	SCP_CORE0_GPR(0) = SCP_CORE0_INIT_DONE;
}
#endif

#ifdef CONFIG_PANIC_CONSOLE_OUTPUT
static void report_previous_panic(void)
{
	struct panic_data * panic = panic_get_data();

	if (panic == NULL && SCP_CORE0_MON_PC_LATCH == 0)
		return;

	ccprintf("[Previous Panic]\n");
	if (panic) {
		panic_data_ccprint(panic);
	} else {
		ccprintf("No panic data\n");
	}
	ccprintf("Latch PC:%x LR:%x SP:%x\n",
		SCP_CORE0_MON_PC_LATCH,
		SCP_CORE0_MON_LR_LATCH,
		SCP_CORE0_MON_SP_LATCH);

}
#endif

static void scp_init_hook(void)
{
#ifdef CONFIG_PANIC_CONSOLE_OUTPUT
	report_previous_panic();
#endif

#ifdef CHIP_VARIANT_MT8195
	core0_boot_done();
#endif
}
DECLARE_HOOK(HOOK_INIT, scp_init_hook, HOOK_PRIO_DEFAULT);
