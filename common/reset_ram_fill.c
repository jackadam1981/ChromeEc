
#include "common.h"
#include "reset_ram_fill.h"

/* reset_ram_fill_cond0_reg must be provided */
const uint32_t reset_ram_fill_cond0_mask __overridable = (uint32_t)0xFFFFFFFF;
/* reset_ram_fill_cond0_test must be provided */

volatile uint32_t *const reset_ram_fill_cond1_reg __overridable = (uint32_t *)COND_REG_PTR_UNUSED;
const uint32_t reset_ram_fill_cond1_mask __overridable = (uint32_t)0xFFFFFFFF;
const uint32_t reset_ram_fill_cond1_test __overridable = (uint32_t)0xFFFFFFFF;

volatile uint32_t *const reset_ram_fill_cond2_reg __overridable = (uint32_t *)COND_REG_PTR_UNUSED;
const uint32_t reset_ram_fill_cond2_mask __overridable = (uint32_t)0xFFFFFFFF;
const uint32_t reset_ram_fill_cond2_test __overridable = (uint32_t)0xFFFFFFFF;