/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

enum hw_dbg_bp_type {
	HW_DBG_BP_TYPE_INSTRUCTION,
	HW_DBG_BP_TYPE_MEMORY,
	HW_DBG_DP_TYPE_COMBINED,
}

enum hw_dbg_bp_flags {
	HW_DBG_BP_FLAGS_NONE,
	HW_DBG_BP_FLAGS_LOAD = 0x1,
	HW_DBG_BP_FLAGS_STORE = 0x2,
	HW_DBP_BP_FLAGS_LEN_1,
	HW_DBP_BP_FLAGS_LEN_2,
	HW_DBP_BP_FLAGS_LEN_4,
	HW_DBP_BP_FLAGS_LEN_8,
}

typedef hw_dbg_handle_t int;

typedef bool (*hw_dbg_bp_callback_t)(const device *dev, hw_dbg_handle_t bp);

typedef int (*hw_dbg_init_t)(const struct device *dev);
typedef int (*hw_dbg_bp_num_available_t)(const struct device *dev, enum hw_dbg_bp_type type);
typedef hw_dbg_handle_t (*hw_dbg_bp_set_t)(const struct device *dev, uintptr_t addr, enum hw_dbg_bp_type type, enum hw_dbg_bp_flags, hw_dbg_bp_callback_t cb);
typedef int (*hw_dbg_bp_enable_t)(const device struct *dev, bool enable);
typedef int (*hw_dbg_bp_clear_t)(const device struct *dev, hw_dbg_handle_t bp);

__subsystem struct hw_dbg_api_t {
    hw_dbg_init_t init;
    hw_dbg_bp_num_available_t bp_num_available;
    hw_dbg_bp_set_t dbg_bp_set;
    hw_dbg_bp_enable_t enable_bp;
    hw_dbg_bp_clear_t clear_bp;
}

__syscall int hw_dbg_init(const struct device *dev);

static inline int z_impl_hw_dbg_init(const struct device *dev)
{
	const struct hw_dbg_api_t *api =
		(const struct hw_dbg_api_t *)dev->api;

	if (api->init == NULL) {
		return -ENOTSUP;
	}

	return api->init(dev);
}

__syscall int hw_dbg_set_bp_instruction(const struct device *dev, uintptr_t addr);

static inline int z_impl_hw_dbg_set_bp_instruction(const struct device *dev, uintptr_t addr)
{
	const struct hw_dbg_api_t *api =
		(const struct hw_dbg_api_t *)dev->api;

	if (api->set_bp_instruction == NULL) {
		return -ENOTSUP;
	}

	return api->set_bp_instruction(dev, addr);
}

__syscall int hw_dbg_set_bp_memory(const struct device *dev, uintptr_t addr, bool read, bool write);

static inline int z_impl_hw_dbg_set_bp_memory(const struct device *dev, uintptr_t addr, bool read, bool write)
{
	const struct hw_dbg_api_t *api =
		(const struct hw_dbg_api_t *)dev->api;

	if (api->set_bp_memory == NULL) {
		return -ENOTSUP;
	}

	return api->set_bp_memory(dev, addr, read, write);
}

__syscall int hw_dbg_enable_bp(const device struct *dev, hw_dbg_handle_t bp, bool enable);

static inline int z_impl_hw_dbg_enable_bp(const device struct *dev, hw_dbg_handle_t bp, bool enable)
{
	const struct hw_dbg_api_t *api =
		(const struct hw_dbg_api_t *)dev->api;

	if (api->enable_bp == NULL) {
		return -ENOTSUP;
	}

	return api->enable_bp(dev, bp, enable);
}

__syscall int hw_dbg_clear_bp(const device struct *dev, hw_dbg_handle_t bp);

static inline int z_impl_hw_dbg_clear_bp(const device struct *dev, hw_dbg_handle_t bp)
{
	const struct hw_dbg_api_t *api =
		(const struct hw_dbg_api_t *)dev->api;

	if (api->clear_bp == NULL) {
		return -ENOTSUP;
	}

	return api->clear_bp(dev, dp);
}