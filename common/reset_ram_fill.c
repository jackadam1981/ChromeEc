
#include "reset_ram_fill.h"

volatile uint32_t *const reset_ram_fill_cond1_reg __attribute__((weak)) = (uint32_t *)0xFFFFFFFF;
const uint32_t reset_ram_fill_cond0_mask __attribute__((weak)) = (uint32_t)0xFFFFFFFF;
const uint32_t reset_ram_fill_cond1_mask __attribute__((weak)) = (uint32_t)0xFFFFFFFF;
