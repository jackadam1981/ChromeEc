/*
 * Copyright (c) 2018 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by Fingerprint Cards AB.
 */

#ifndef FPC_MALLOC_H
#define FPC_MALLOC_H

/**
 * @file    fpc_malloc.h
 * @brief   External malloc and free APIs.
 *
 * The BEP library includes a default weak implementation that uses stdlib malloc
 * and free for convenience and backward compatibility.
 * It is recommended to override these with custom implementations.
 */

#include <stdlib.h>

/**
 * @brief Malloc wrapper.
 *
 * @param[in] size Size of allocation.
 *
 * @return Pointer to data or NULL if unsuccessful.
 *
 */
void *fpc_malloc(size_t size);

/**
 * @brief Free wrapper.
 *
 * @param[in] data Pointer to data.
 *
 */
void fpc_free(void *data);

#endif /* FPC_MALLOC_H */
