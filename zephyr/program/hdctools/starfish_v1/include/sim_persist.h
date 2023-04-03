/* Copyright 2023 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_STARFISH_SIM_PERSIST_H__
#define __CROS_EC_STARFISH_SIM_PERSIST_H__

struct storage_result {
    void *buffer;
    size_t bufferSize;
    size_t minSize;
    size_t maxSize;
    size_t size;
};

#define INIT_STORAGE_RESULT(data)                            \
    {                                                    \
        .buffer = &data,    \
        .bufferSize = sizeof(data), \
        .minSize = sizeof(data),    \
        .maxSize = sizeof(data),    \
    }

#define SIM_ERASE_FIELD(name)   sim_persist_save(name, NULL, 0)

#define SIM_SAVE_FIELD(name, data)      sim_persist_save(name, &data, sizeof(data))

bool sim_persist_load(const char *key, struct storage_result *result);

bool sim_persist_save(const char *key, const void *src, size_t len);

bool sim_persist_erase();

void sim_persist_status();

#endif /* __CROS_EC_STARFISH_SIM_PERSIST_H__ */
