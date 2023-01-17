/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_stub_device.h"
#include "util.h"

#include <zephyr/device.h>
#include <zephyr/drivers/i2c_emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/sys/byteorder.h>

#define DT_DRV_COMPAT elan_ekth3000

struct ekth3000_data {
	int cached_cmd;
};

static int ekth3000_fake_report(uint8_t *buf, int len)
{
	const static uint8_t report[34] = {
		[2] = 0x5D, /* report ID */
		[3] = 0x08, /* touch_info */
	};

	memcpy(buf, report, MIN(len, sizeof(report)));

	return 0;
}

static int ekth3000_transfer(const struct emul *target, struct i2c_msg *msgs,
			   int num_msgs, int addr)
{
	int cmd = -1;

	for (int i = 0; i < num_msgs; i++) {
		struct i2c_msg *msg = &msgs[i];
		bool read = msg->flags & I2C_MSG_READ;
		int len = msg->len;

		if (read) {
			switch (cmd) {
			case -1:
				return ekth3000_fake_report(msg->buf, len);
			case 0x0105:
				if (len != 2) {
					return -1;
				}
				msg->buf[0] = msg->buf[1] = 1;
				break;
			default:
				break;
			}
		} else {
			if (len < 2) {
				return -1;
			}
			cmd = sys_le16_to_cpu(*(uint16_t*)msg->buf);
		}
	}

	return 0;
}

static int ekth3000_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	return 0;
}

#define INIT_EKTH3000_EMUL(n)                                        \
	static struct ekth3000_data ekth3000_data_##n = {              \
	};                                                         \
	EMUL_DT_INST_DEFINE(n, ekth3000_emul_init, &ekth3000_data_##n, \
			    NULL, &elan_ekth3000_api)

const static struct i2c_emul_api elan_ekth3000_api = {
	.transfer = ekth3000_transfer,
};

DT_INST_FOREACH_STATUS_OKAY(INIT_EKTH3000_EMUL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
