/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* wrapper from FreeRTOS to ChromeOS EC */

#ifndef __CROS_EC_WRAPPER_H
#define __CROS_EC_WRAPPER_H

#include "console.h"
#include "util.h"

#define DEBUGLEVEL 3
/* DEBUG LEVEL definition*/
#define LOG_DEBUG  3
#define LOG_INFO   2
#define LOG_WARN   1
#define LOG_ERROR  0

#define PRINTF_D(x...)            \
({                                \
    if (LOG_DEBUG <= DEBUGLEVEL)  \
        ccprintf(x);              \
})

#define PRINTF_I(x...)            \
({                                \
    if (LOG_INFO <= DEBUGLEVEL)   \
        ccprintf(x);              \
})

#define PRINTF_W(x...)            \
({                                \
    if (LOG_WARN <= DEBUGLEVEL)   \
        ccprintf(x);              \
})


#define PRINTF_E(x...)            \
({                                \
    if (LOG_ERROR <= DEBUGLEVEL)  \
        ccprintf(x);              \
})

#define configASSERT(x) \
	ASSERT(x)

extern uint32_t dma_scp_to_ap(uint32_t scp_addr);
extern uint32_t dma_ap_to_scp(uint32_t ap_addr);
extern uint32_t dma_scp_cache_to_ap(uint32_t scp_cache_addr);
extern uint32_t dma_ap_to_scp_cache(uint32_t ap_addr);

#define scp_to_ap(scp_addr) \
	dma_scp_to_ap(scp_addr)

#define ap_to_scp(ap_addr) \
	dma_ap_to_scp(ap_addr)

#define scp_cache_to_ap(scp_cache_addr) \
	dma_scp_cache_to_ap(scp_cache_addr)

#define ap_to_scp_cache(ap_addr) \
	dma_ap_to_scp_cache(ap_addr)

#define scp_ipi_send(id, buf, len, wait, dir) \
	ipi_send(id, buf, len, wait)

#define scp_ipi_wakeup_ap_registration(id) \
	ipi_wakeup_ap_registration(id)

#define scp_ipi_registration(id, handler, name) \
	ipi_register(id, handler)

#define scp_ipi_unregistration(id) \
	ipi_unregister(id)

#define is_in_isr(void) \
	in_interrupt_context(void)

#define taskENTER_CRITICAL(void) \
	interrupt_disable(void)

#define taskEXIT_CRITICAL(void) \
	interrupt_enable(void)

#define xTaskGetTickCountFromISR  // TODO

#endif  /* __CROS_EC_WRAPPER_H */
