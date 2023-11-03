#ifndef _HYPERDEBUG_BOARD_UTIL__H_
#define _HYPERDEBUG_BOARD_UTIL__H_

#include "stddef.h"
#include "stdint.h"

void find_best_divisor(
	uint32_t desired_freq, const uint32_t base_frequencies[], size_t num_base_frequencies,
	uint8_t *best_divisor, size_t *best_base_frequency_index);

#endif
