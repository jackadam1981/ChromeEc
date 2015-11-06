#include "boot_loader.h"

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

void disarmRAMGuards (void) {
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION0_CTRL_OFFSET,
            GLOBALSEC_CPU0_D_REGION0_CTRL_EN_MASK |
            GLOBALSEC_CPU0_D_REGION0_CTRL_RD_EN_MASK |
            GLOBALSEC_CPU0_D_REGION0_CTRL_WR_EN_MASK);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_CPU0_D_REGION1_CTRL_OFFSET,
            GLOBALSEC_CPU0_D_REGION1_CTRL_EN_MASK |
            GLOBALSEC_CPU0_D_REGION1_CTRL_RD_EN_MASK |
            GLOBALSEC_CPU0_D_REGION1_CTRL_WR_EN_MASK);
}

void unlockFlashForRW (void) {
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_BASE_ADDR_OFFSET, FLASH0_BASE + LOADER_SIZE);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_SIZE_OFFSET, FLASH_SIZE - LOADER_SIZE - 1);
  write_reg(GLOBALSEC_BASE_ADDR+GLOBALSEC_FLASH_REGION1_CTRL_OFFSET,
            GLOBALSEC_FLASH_REGION1_CTRL_EN_MASK |
            GLOBALSEC_FLASH_REGION1_CTRL_WR_EN_MASK |
            GLOBALSEC_FLASH_REGION1_CTRL_RD_EN_MASK);
}

void checkBuildVersion (void) {
  if (read_reg(SWDP0_BASE_ADDR+SWDP_P4_LAST_SYNC_OFFSET) != SWDP_P4_LAST_SYNC_DEFAULT) {
    INFO("compiled for %u, not willing to run on %u\n",
         SWDP_P4_LAST_SYNC_DEFAULT,
         read_reg(SWDP0_BASE_ADDR+SWDP_P4_LAST_SYNC_OFFSET));
    _purgatory();
  }
}

void _purgatory (void) __attribute__((section(".tarpit")));
void _purgatory(void) {
  while (1) {
    __asm__("wfe");
  }
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
