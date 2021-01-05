/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault coredump implementation
 */


#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "memfault/core/compiler.h"
#include "memfault/core/math.h"
#include "memfault/core/debug_log.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/assert.h"
#include "memfault/panics/platform/coredump.h"
#include "link_defs.h"
#include "task.h"

#define MEMFAULT_STACK_COLLECTION_SIZE (256)
#define MEMFAULT_MEMORY_COLLECTION_SIZE (1024*3)
#define MEMFAULT_EXTRA_COLLECTION_SIZE (256*3)

#if 0
extern uint32_t __bss_start[];
extern uint32_t __bss_after_system_stack[];
extern uint32_t __bss_end[];
extern uint32_t __data_start[];
extern uint32_t __data_end[];

#define BSS_START (__bss_after_system_stack)
#define DATA_START (__data_start)

#define BSS_END (__bss_end)
#define DATA_END (__data_end)

#define BSS_LENGTH (BSS_END-BSS_START)
#define DATA_LENGTH (DATA_END-DATA_START)
#endif

#define MEMFAULT_COREDUMP_SIZE ( \
  MEMFAULT_RAM_BACKED_LOG_SIZE + \
  MEMFAULT_EXTRA_COLLECTION_SIZE + \
  MEMFAULT_STACK_COLLECTION_SIZE)

MEMFAULT_PUT_IN_SECTION(".noinit.mflt_coredump") MEMFAULT_ALIGNED(8)
static uint8_t s_coredump_region[MEMFAULT_COREDUMP_SIZE];


const sMfltCoredumpRegion *memfault_platform_coredump_get_static_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {

  *num_regions = (__memfault_capture_end - __memfault_capture);
  MEMFAULT_LOG_INFO("num_static_regions = %d", *num_regions);
  return __memfault_capture;
}

#define DYNAMIC_COREDUMP_REGION_MAX 64

const sMfltCoredumpRegion *memfault_platform_coredump_get_dynamic_regions(
    const sCoredumpCrashInfo *crash_info, size_t *num_regions) {
  static sMfltCoredumpRegion s_coredump_dynamic_regions[DYNAMIC_COREDUMP_REGION_MAX];

  int region_index = 0;

  s_coredump_dynamic_regions[region_index++] = MEMFAULT_COREDUMP_MEMORY_REGION_INIT(crash_info->stack_address, MEMFAULT_STACK_COLLECTION_SIZE);

  *num_regions = region_index;
  MEMFAULT_LOG_INFO("num_dynamic_regions = %d", *num_regions);
  return s_coredump_dynamic_regions;
}

void memfault_platform_coredump_storage_get_info(sMfltCoredumpStorageInfo *info) {
  *info = (sMfltCoredumpStorageInfo) {
    .size = sizeof(s_coredump_region),
    .sector_size = sizeof(s_coredump_region),
  };
}


bool memfault_platform_coredump_storage_read(uint32_t offset, void *data,
                                             size_t read_len) {
  if ((offset + read_len) > sizeof(s_coredump_region)) {
    return false;
  }

  const uint8_t *read_ptr = &s_coredump_region[offset];
  memcpy(data, read_ptr, read_len);
  return true;
}


bool memfault_platform_coredump_storage_erase(uint32_t offset, size_t erase_size) {
  if ((offset + erase_size) > sizeof(s_coredump_region)) {
    return false;
  }

  uint8_t *erase_ptr = &s_coredump_region[offset];
  memset(erase_ptr, 0x0, erase_size);
  return true;
}


bool memfault_platform_coredump_storage_write(uint32_t offset, const void *data,
                                              size_t data_len) {
  if ((offset + data_len) > sizeof(s_coredump_region)) {
    return false;
  }

  uint8_t *write_ptr = &s_coredump_region[offset];
  memcpy(write_ptr, data, data_len);
  return true;
}

void memfault_platform_coredump_storage_clear(void) {
  const uint8_t clear_byte = 0x0;
  memfault_platform_coredump_storage_write(0, &clear_byte, sizeof(clear_byte));
}

int memfault_platform_coredump_init(void) {

  sMfltCoredumpStorageInfo storage_info = { 0 };
  memfault_platform_coredump_storage_get_info(&storage_info);
  const size_t size_needed = memfault_coredump_storage_compute_size_required();
  MEMFAULT_LOG_INFO("Coredump storage required: %d, actual: %d",
    size_needed, storage_info.size);

  return 0;
}
