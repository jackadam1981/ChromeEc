// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __CROS_EC_PMP_H
#define __CROS_EC_PMP_H

#ifdef CONFIG_PLATFORM_EC_ROLLBACK_PMP_PROTECT

/**
 * @brief Sets the write permission for a specific PMP entry.
 *
 * Searches for the PMP entry matching CUSTOM_PMP_ENTRY_START.
 * Modifies the Write (W) bit in this entry's PMP configuration.
 *
 * @note This function currently supports up to 8 PMP slots (CONFIG_PMP_SLOTS <=
 * 8). Extending beyond 8 slots in C requires more switch cases or an assembly
 * helper.
 *
 * @param write_enable If true, enables writes to the region (sets W bit).
 *                     If false, disables writes (clears W bit).
 *
 * @return 0 on success.
 *         -ENOENT if the PMP entry for CUSTOM_PMP_ENTRY_START is not found.
 *         -ENOTSUP if CONFIG_PMP_SLOTS > 8.
 *         -EINVAL for unexpected internal errors.
 */
int riscv_pmp_set_write_permission(bool write_enable);

#endif

#endif /* __CROS_EC_PMP_H */
