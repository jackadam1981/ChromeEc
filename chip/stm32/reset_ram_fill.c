
#include "registers.h"

/* Primary trigger */
// volatile uint32_t *const reset_ram_fill_cond0_reg = ((volatile uint32_t *)(0x58024400 + 0xd0));
volatile uint32_t *const reset_ram_fill_cond0_reg = &STM32_RCC_RESET_CAUSE;
const uint32_t reset_ram_fill_cond0_mask = RESET_CAUSE_POR;
