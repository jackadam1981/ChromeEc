
#include "registers.h"

/* Trigger on Power-On-Reset */
volatile uint32_t *const reset_ram_fill_cond0_reg = &STM32_RCC_RESET_CAUSE;
const uint32_t reset_ram_fill_cond0_mask = RESET_CAUSE_POR;
const uint32_t reset_ram_fill_cond0_test = RESET_CAUSE_POR;

/* Trigger on Pin-Reset */
volatile uint32_t *const reset_ram_fill_cond1_reg = &STM32_RCC_RESET_CAUSE;
const uint32_t reset_ram_fill_cond1_mask = RESET_CAUSE_OTHER & ~BIT(18);
const uint32_t reset_ram_fill_cond1_test = RESET_CAUSE_PIN | RESET_CAUSE_CPU;