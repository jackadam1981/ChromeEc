/*
* Copyright (c) 2019 Fingerprint Cards AB <tech@fingerprints.com>
*
* All rights are reserved.
* Proprietary and confidential.
* Unauthorized copying of this file, via any medium is strictly prohibited.
* Any use is subject to an appropriate license granted by Fingerprint Cards AB.
*/

/**
 * @file    fpc_static_malloc.c
 * @brief   Static Malloc implementation
 *
 * If many small allocations are used, static pre allocated blocks can be used.
 * One area with 0-32 Small blocks, and one area with 0-32 Large blocks. These
 * areas are located before dynamic space. 1 bit of overhead per block.
 *
 */

#define FPC_STATIC_MALLOC_SIZE

#ifdef FPC_STATIC_MALLOC_SIZE

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

//#include "fpc_bep_types.h"
//#include "fpc_log.h"
//#include "fpc_util.h"

#include "fpc_malloc.h"
#include "fpc_static_malloc.h"

/* Memory alignment. malloc will return addresses aligned with ADDR_ALIGN
 * Can be 1, 2, 4, 8, 16, 32, ... */
#define ADDR_ALIGN                          8

/* Enables logging and memory tracking. Do not include if not explicitly
 * specified */
//#define ALLOC_DEBUG                         0

/* Private */

#define ya_min(a,b)  ((a) < (b) ? (a) : (b))

#define ALIGN(p) ((uintptr_t)((uint8_t*)(p) + ADDR_ALIGN-1) & ~((uintptr_t)ADDR_ALIGN-1))

typedef struct _s_mem_hdr {
    size_t allocated;
    struct _s_mem_hdr *next;
    uint8_t mem[0];
} s_mem_hdr;

static uint8_t *s_mem_start = 0;
static uint8_t *s_mem_end = 0;

#ifdef ALLOC_DEBUG
static size_t s_peak_a = 0;
static size_t s_end_a = 0;
static size_t s_tot_a = 0;
#endif


/**
 *
 */
int fpc_static_malloc_setup(void *buffer, size_t size)
{
    s_mem_start = (uint8_t*)ALIGN((uint8_t*)buffer);
    s_mem_end = (uint8_t*)buffer + size;
    s_mem_hdr *hdr = (s_mem_hdr*)(void*)s_mem_start;
    hdr->allocated = 0;
    hdr->next = 0;
    return 0;
}


/**
*
*/
void *fpc_malloc(size_t bytes)
{
    s_mem_hdr *hdr_this;

    if (!bytes)
        return (void*)0;

    hdr_this = (s_mem_hdr*)(void*)s_mem_start;

    while (hdr_this != 0)
    {
        if (hdr_this->allocated == 0) {
            if (hdr_this->next == 0) {
                if (hdr_this->mem + bytes > s_mem_end) {
#ifdef ALLOC_DEBUG
                    fpc_log_printf("Alloc %lu bytes: # OUT OF MEM #", (unsigned long)bytes);
#endif
                    return (void*)0;
                }
            }
            else {
                if (hdr_this->mem + bytes > (uint8_t*)hdr_this->next) {
                    hdr_this = hdr_this->next;
                    continue;
                }
            }
            hdr_this->allocated = bytes;
#ifdef ALLOC_DEBUG
            s_tot_a += bytes;
            if (!hdr_this->next) {
                s_end_a = (size_t)(hdr_this->mem + bytes - s_mem_start);
                if (s_end_a > s_peak_a)
                    s_peak_a = s_end_a;
            }
            fpc_log_printf("%s %lu bytes @ %p (%lu / %lu : %lu)",
                    hdr_this->next ? "Insert" : "Append",
                    (unsigned long)bytes,
                    hdr_this->mem,
                    (unsigned long)s_tot_a,
                    (unsigned long)s_end_a,
                    (unsigned long)s_peak_a);
#endif
            return (void*)hdr_this->mem;
        }
        else {
            s_mem_hdr *next_prel = (s_mem_hdr*)ALIGN((uint8_t*)hdr_this->mem + hdr_this->allocated);

            if (hdr_this->next == 0) {
                if (next_prel->mem + bytes > s_mem_end) {
#ifdef ALLOC_DEBUG
                    fpc_log_printf("Alloc %lu bytes: # OUT OF MEM #", (unsigned long)bytes);
#endif
                    return (void*)0;
                }
            }
            else if (next_prel->mem + bytes > (uint8_t*)hdr_this->next) {
                hdr_this = hdr_this->next;
                continue;
            }
            next_prel->next = hdr_this->next;
            next_prel->allocated = bytes;
            hdr_this->next = next_prel;

#ifdef ALLOC_DEBUG
            s_tot_a += bytes;
            if (!next_prel->next) {
                s_end_a = (size_t)(next_prel->mem + bytes - s_mem_start);
                if (s_end_a > s_peak_a)
                    s_peak_a = s_end_a;

            }
            fpc_log_printf("%s %lu bytes @ %p (%lu / %lu : %lu)",
                    next_prel->next ? "Insert" : "Append",
                    (unsigned long)bytes,
                    next_prel->mem,
                    (unsigned long)s_tot_a,
                    (unsigned long)s_end_a,
                    (unsigned long)s_peak_a);
#endif
            return (void*)next_prel->mem;
        }
    }
    return (void*)0;
}


/**
*
*/
void fpc_free(void *p)
{
    s_mem_hdr *hdr_this;
    s_mem_hdr *hdr_prev;

    if (!p)
        return;

    hdr_this = (s_mem_hdr*)(void*)s_mem_start;
    hdr_prev = (s_mem_hdr*)(void*)s_mem_start;

    while (hdr_this) {
        if (hdr_this->mem == p) {

#ifdef ALLOC_DEBUG
            s_tot_a -= hdr_this->allocated;
            if (!hdr_this->next) {
                s_end_a = (size_t)(hdr_prev->mem + hdr_prev->allocated
                        - s_mem_start);
            }
            fpc_log_printf("Free %lu bytes @ %p (%lu / %lu)",
                    (unsigned long)hdr_this->allocated,
                    hdr_this->mem,
                    (unsigned long)s_tot_a,
                    (unsigned long)s_end_a);
#endif
            hdr_this->allocated = 0;
            hdr_prev->next = hdr_this->next;
            return;
        }
        hdr_prev = hdr_this;
        hdr_this = hdr_this->next;
    }
}

#endif

