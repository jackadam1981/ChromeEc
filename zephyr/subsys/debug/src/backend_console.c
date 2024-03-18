/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include <zephyr/debug/gdbstub.h>
#include <zephyr/shell/shell.h>

#define BUF_LEN CONFIG_PLATFORM_EC_GDBSTUB_BACKEND_CONSOLE_BUFFER_LEN

LOG_MODULE_REGISTER(gdbstub_backend_console, LOG_LEVEL_DBG);

BUILD_ASSERT(CONFIG_SHELL_CMD_BUFF_SIZE >= BUF_LEN);

/* TODO: remove when these are visible upstream. */
int z_gdb_backend_init(void);
void z_gdb_putchar(unsigned char ch);
char z_gdb_getchar(void);
int z_gdb_main_loop(struct gdb_ctx *ctx);

struct gdb_packet{
	/* +1 to allow for NULL termination. */
	char buf[BUF_LEN + 1];
	int size;
	int index;
};

enum tx_state {
	TX_IDLE,
	TX_WRITE,
	TX_CHECKSUM_A,
	TX_CHECKSUM_B,
};


static struct k_sem sem;
static k_tid_t gdb_thread;
static struct k_thread gdb_thread_data;
static struct gdb_packet rx_buf;

static struct gdb_packet tx_buf;
static enum tx_state tx_state;
static bool tx_skip;

K_THREAD_STACK_DEFINE(gdb_thread_stack, 1024);

struct k_thread *gdb_nonstop_get_thread(void) {
	return &gdb_thread_data;
}

static void gdbstub_thread(void *unused1, void *unused2, void *unused3)
{
	struct gdb_ctx *ctx = NULL;

	while (1) {
		if(!gdb_nonstop_select_stopped(&ctx, NULL)) {
			k_msleep(100);
			continue;
		}
		z_gdb_main_loop(ctx);
	}
}

int z_gdb_backend_init(void)
{
	LOG_DBG("GDB backend init");
	k_sem_init(&sem, 0, 1);
	gdb_thread = k_thread_create(
			&gdb_thread_data, gdb_thread_stack,
			K_THREAD_STACK_SIZEOF(gdb_thread_stack),
			gdbstub_thread, NULL, NULL, NULL,
			CONFIG_PLATFORM_EC_GDBSTUB_BACKEND_PRIORITY, 0,
			K_MSEC(1000));
	k_thread_name_set(gdb_thread, "GDBSTUB");
	return 0;
}

void z_gdb_putchar(unsigned char ch)
{
	//LOG_INF("GDBCH:%c", ch);
	if (!tx_skip) {
		tx_buf.buf[tx_buf.index] = ch;
		tx_buf.index++;
		tx_skip = tx_buf.index >= BUF_LEN;
	} else {
		LOG_ERR("TX buffer overrun");
		return;
	}

	switch (tx_state) {
		case TX_IDLE:
			/*
			* +/- are essentially a special type of packet, convert them
			* to a form that is easier to pick out from the console.
			*/
			if (ch == '-') {
				LOG_INF("gdbresponse$-#2d");
				tx_buf.index = 0;
				break;
			} else if(ch == '+') {
				LOG_INF("gdbresponse$+#2b");
				tx_buf.index = 0;
				break;
			} else if (ch != '$' && ch != '%') {
				LOG_ERR("Failed to find packet start, got %c", ch);
				break;
			}
			tx_state = TX_WRITE;
			break;

		case TX_WRITE:
			if (ch == '#') {
				/* End of packet, wait for checksum. */
				tx_state = TX_CHECKSUM_A;
			}
			break;

		case TX_CHECKSUM_A:
			/* Wait for second checksum character. */
			tx_state = TX_CHECKSUM_B;
			break;

		case TX_CHECKSUM_B:
			tx_buf.buf[tx_buf.index + 1] = '\0';
			LOG_INF("gdbresponse%s", tx_buf.buf);
			tx_state = TX_IDLE;
			tx_buf.index = 0;
			tx_skip = false;
			break;
	}
}

char z_gdb_getchar(void)
{
	char c;
	static bool has_packet;

	if (!has_packet) {
		k_sem_take(&sem, K_FOREVER);
		has_packet = true;
	}
	
	c = rx_buf.buf[rx_buf.index];
	rx_buf.index++;
	if (rx_buf.index >= rx_buf.size) {
		has_packet = false;
	}

	return c;
}

static int command_gdbpacket(const struct shell *sh, size_t argc, char **argv)
{
	int len;

	if (argc != 2) {
		shell_error(sh, "Invalid number of arguments");
		return -EINVAL;
	}

	len = strlen(argv[1]);
	if (len >= BUF_LEN) {
		shell_error(sh, "Packet buffer overrun");
		return -EINVAL;
	}

	rx_buf.size = len;
	rx_buf.index = 0;
	memcpy(rx_buf.buf, argv[1], len);
	k_sem_give(&sem);
	return 0;
}

SHELL_CMD_REGISTER(gdbpacket, NULL, "Send packet to GDB stub",
		   command_gdbpacket);