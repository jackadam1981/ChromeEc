/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <system.h>

// static uint64_t get_time;
// extern int cb_calls;
// extern char tmp_buf[1024];
// extern int tmp_len;
// extern int tx_cb_calls;
// extern int tx_end_calls;
// extern uint32_t ier;
// extern uint32_t fcr;
// extern uint32_t lcr;
// extern uint32_t mcr;
#include <zephyr/drivers/syscon.h>

// static const struct device *const syscon_dev =
// 			DEVICE_DT_GET(DT_NODELABEL(syscon));

// static void dn_thread_entry(void *arg1, void *arg2, void *arg3)
// {
// 	uint64_t get_time_diff;
// 	uint64_t get_time_now;
// 	uint64_t get_time_old = 0;
// 	// uint32_t ptr;
// 	while(1) {
// 		// int cb_calls_tmp = cb_calls;
// 		get_time_now = k_uptime_get();
// 		get_time_diff = k_uptime_get() - get_time_old;
// 		get_time_old = get_time_now;
// 		// printk("DN diff: %llu, cb calls: %d\n", get_time_diff, cb_calls_tmp);
// 		printk("DN diff: %llu\n", get_time_diff);
//                 uint32_t reg;
// 		syscon_read_reg(syscon_dev, 0x20, &reg);
// 		printk("DN retention ram: %08x\n", reg & 0x0fff0000);

// 		// ptr = 0x0;
// 		// printk("DN h_f:\n");
// 		// for (int i = 0; i < 32; i++) {
// 		// 	printk("%02x", *((volatile uint8_t*)(ptr + i)));
// 		// }
// 		// printk("\n");

// 		// ptr = 0x20;
// 		// printk("DN s_d:\n");
// 		// for (int i = 0; i < 64; i++) {
// 		// 	printk("%02x", *((volatile uint8_t*)(ptr + i)));
// 		// }
// 		// printk("\n");

// 		// ptr = 0x60;
// 		// printk("DN pk_f:\n");
// 		// for (int i = 0; i < 65; i++) {
// 		// 	printk("%02x", *((volatile uint8_t*)(ptr + i)));
// 		// }
// 		// printk("\n");

// 		// ptr = 0xa0;
// 		// printk("DN sk_f:\n");
// 		// for (int i = 0; i < 32; i++) {
// 		// 	printk("%02x", *((volatile uint8_t*)(ptr + i)));
// 		// }
// 		// printk("\n");

// 		// printk("DN buff len: %d\n", tmp_len);
// 		// for (int i = 0; i < tmp_len; i++) {
// 		// 	printk("%c", ((char*)tmp_buf)[i]);
// 		// }
// 		// printk("\n");
// 		// printk("DN tx_cb_calls: %d\n", tx_cb_calls);
// 		// printk("DN tx_end_calls: %d\n", tx_end_calls);
// 		// printk("DN ier: 0x%08x\n", ier);
// 		// printk("DN fcr: 0x%08x\n", fcr);
// 		// printk("DN lcr: 0x%08x\n", lcr);
// 		// printk("DN mcr: 0x%08x\n", mcr);
// 		// ier = 0;
// 		// fcr = 0;
// 		// lcr = 0;
// 		// mcr = 0;
// 		// tx_cb_calls = 0;
// 		// tx_end_calls = 0;
// 		// cb_calls = 0;
// 		k_msleep(3000);
// 		// printk("DN mtime diff: %llu\n", mtime_diff);

// 		// printk("DN 0xf0100008 0x%08x\n", *((volatile uint32_t*)0xf0100008));
// 		// printk("DN 0xf0100010 0x%08x\n", *((volatile uint32_t*)0xf0100010));
// 		// printk("DN 0xf0100014 0x%08x\n", *((volatile uint32_t*)0xf0100014));
// 		// printk("DN 0xf010001C 0x%08x\n", *((volatile uint32_t*)0xf010001C));
// 	}
// }

// K_THREAD_DEFINE(dn_thread, 1024, dn_thread_entry, NULL, NULL, NULL, 80, 0, 0);

// TODO replace with entropy driver - set zephyr,entropy chosen node
void trng_rand_bytes(void *buffer, size_t len)
{
	for (int i = 0; i < len; i++) {
		((uint8_t*)buffer)[i] = (uint8_t)sys_clock_cycle_get_32();
	}
}
