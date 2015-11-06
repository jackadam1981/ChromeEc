/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "debug_printf.h"
#include "link_defs.h"
#include "registers.h"
#include "setup.h"

/* Is there a system wide function for this? */
void halt(void)
{
	while(1)
		;
}

void checkBuildVersion (void) {
	uint32_t last_sync = GREG32(SWDP, P4_LAST_SYNC);

	if ( last_sync == GC_SWDP_P4_LAST_SYNC_DEFAULT)
		return;

	debug_printf("compiled for %u, not willing to run on %u\n",
		     GC_SWDP_P4_LAST_SYNC_DEFAULT, last_sync);
	halt();
}

void unlockFlashForRW (void)
{
	uint32_t text_end = ((uint32_t)(&__ro_end) + CONFIG_FLASH_BANK_SIZE)
		& ~(CONFIG_FLASH_BANK_SIZE - 1);

	GREG32(GLOBALSEC, FLASH_REGION1_BASE_ADDR) = text_end;
	GREG32(GLOBALSEC, FLASH_REGION1_SIZE) = CONFIG_FLASH_SIZE - text_end - 1;
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION1_CTRL, EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION1_CTRL, RD_EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION1_CTRL, WR_EN, 0);
}

void disarmRAMGuards (void)
{
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION0_CTRL, EN, 1);
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION0_CTRL, RD_EN, 1);
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION0_CTRL, WR_EN, 1);
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION1_CTRL, EN, 1);
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION1_CTRL, RD_EN, 1);
	GWRITE_FIELD(GLOBALSEC, CPU0_D_REGION1_CTRL, WR_EN, 1);
}

#if 0

void _purgatory (void) __attribute__((section(".tarpit")));
void _purgatory(void) {
  while (1) {
    __asm__("wfe");
  }
}

void setupRAMGuards (void) {
  extern struct { uint8_t nothing[32]; } _guard0, _guard1;

  // Setup couple of memory fences in RAM by making two untouchable regions.
  // Helps defend critical .guarded_data section and stack beyond it.
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION0_BASE_ADDR_OFFSET,
            (uint32_t)(&_guard0));
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION0_SIZE_OFFSET,
            sizeof(_guard0) - 1);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION0_CTRL_OFFSET,
            GLOBALSEC_CPU0_D_REGION0_CTRL_EN_MASK);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION1_BASE_ADDR_OFFSET,
            (uint32_t)(&_guard1));
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION1_SIZE_OFFSET,
            sizeof(_guard1) - 1);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION1_CTRL_OFFSET,
            GLOBALSEC_CPU0_D_REGION1_CTRL_EN_MASK);
}

void reboot (void) {
  // Enable the timerls 0
  write_reg(TIMELS0_BASE_ADDR+TIMELS_TIMER1_CONTROL_OFFSET, 0x1);
  // Load the counter, ~1s
  write_reg(TIMELS0_BASE_ADDR+TIMELS_TIMER1_LOAD_OFFSET, 2400*100);  // 10000x~400uS
  // TODO will come back to this once timerls1 wakeup signal is connected
  write_reg(PMU_BASE_ADDR+PMU_EXITPD_MASK_OFFSET, PMU_EXITPD_MASK_TIMELS0_PD_EXIT_TIMER1_MASK);

  write_reg(PMU_BASE_ADDR+PMU_LOW_POWER_DIS_OFFSET,
            PMU_LOW_POWER_DIS_VDDL_MASK |
            PMU_LOW_POWER_DIS_VDDIOF_MASK |
            PMU_LOW_POWER_DIS_VDDXO_MASK |
            PMU_LOW_POWER_DIS_JTR_RC_MASK);

  read_write_reg(PMU_BASE_ADDR+PMU_LOW_POWER_DIS_OFFSET,
                 PMU_LOW_POWER_DIS_START_MASK,
                 1,
                 PMU_LOW_POWER_DIS_START_LSB);

  _purgatory();
}
#endif
