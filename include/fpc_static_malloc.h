/*
 * Copyright (c) 2019-2023 Fingerprint Cards AB <tech@fingerprints.com>
 *
 * All rights are reserved.
 * Proprietary and confidential.
 * Unauthorized copying of this file, via any medium is strictly prohibited.
 * Any use is subject to an appropriate license granted by Fingerprint Cards AB.
 */

/**
 * @file    fpc_static_malloc.h
 * @brief   Static Malloc implementation
 *
 * Static in the sense that user provides address space for memory allocations.
 *
 * The dynamic allocations are built as a pure linked list without logic.
 * First available area that fits the requested bytes will be used. 8 bytes
 * of overhead + alignment overhead per allocation.
 *
 * fpc_static_malloc_setup() must be called before usage.
 */

/**
 * @brief Initiates memory block to be used for memory allocations.
 * @param buffer Pointer to memory buffer.
 * @param size Size of memory buffer.
 * @return result
 */
int fpc_static_malloc_setup(void *buffer, size_t size);
