#include "emul/emul_common_i2c.h"
#include "emul/emul_stub_device.h"
#include "emul/tcpc/emul_it8xxx2.h"
#include "emul/tcpc/emul_tcpci.h"
#include "tcpm/tcpci.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

#define DT_DRV_COMPAT cros_it8xxx2_tcpc_emul

LOG_MODULE_REGISTER(it8xxx2_emul, CONFIG_TCPCI_EMUL_LOG_LEVEL);

static bool it8xxx2_is_vendor_reg(int reg)
{
	switch (reg) {
	case it8xxx2_REG_BMCIO_RXDZEN:
	case it8xxx2_REG_BMCIO_RXDZSEL:
	case it8xxx2_REG_DRP_TOGGLE_CYCLE:
	case it8xxx2_REG_I2CRST_CTRL:
	case it8xxx2_REG_PHY_CTRL1:
	case it8xxx2_REG_PHY_CTRL2:
	case it8xxx2_REG_PWR:
	case it8xxx2_REG_RT_MASK:
	case it8xxx2_REG_TTCPC_FILTER:
	case it8xxx2_REG_VENDOR_5:
	case it8xxx2_REG_VENDOR_7:
		return true;

	default:
		return false;
	}
}

static int it8xxx2_emul_read_byte(const struct emul *emul, int reg, uint8_t *val,
				 int bytes)
{
	if (it8xxx2_is_vendor_reg(reg)) {
		uint16_t val16;
		int err;

		if (bytes != 0)
			return -EIO;

		err = tcpci_emul_get_reg(emul, reg, &val16);
		if (err != 0)
			return err;

		*val = val16;
		return 0;
	}

	return tcpci_emul_read_byte(emul, reg, val, bytes);
}

static int it8xxx2_emul_write_byte(const struct emul *emul, int reg, uint8_t val,
				  int bytes)
{
	if (it8xxx2_is_vendor_reg(reg)) {
		if (bytes != 1)
			return -EIO;

		return tcpci_emul_set_reg(emul, reg, val);
	}

	return tcpci_emul_write_byte(emul, reg, val, bytes);
}

static int it8xxx2_emul_handle_write(const struct emul *emul, int reg,
				    int msg_len)
{
	if (it8xxx2_is_vendor_reg(reg)) {
		return 0;
	}

	return tcpci_emul_handle_write(emul, reg, msg_len);
}

void it8xxx2_emul_reset(const struct emul *emul)
{
	tcpci_emul_reset(emul);
}

static int it8xxx2_emul_tcpc_access_reg(const struct emul *emul, int reg,
				       int bytes, bool read)
{
	return reg;
}

static const struct {
	uint16_t reg;
	uint16_t val;
} it8xxx2_rv[] = {
	{ TCPC_REG_VENDOR_ID, it8xxx2_VENDOR_ID },
	{ TCPC_REG_PRODUCT_ID, 0x1711 },
	{ TCPC_REG_BCD_DEV, 0x2173 },
	{ TCPC_REG_TC_REV, 0x0011 },
	{ TCPC_REG_PD_REV, 0x2011 },
	/*
	 * TCPC_REG_PD_INT_REV is set automatically by
	 * tcpci_emul_set_rev() called as a ZTEST_RULE.
	 */
};

static int it8xxx2_emul_init(const struct emul *emul,
			    const struct device *parent)
{
	struct tcpc_emul_data *tcpc_data = emul->data;
	struct tcpci_ctx *tcpci_ctx = tcpc_data->tcpci_ctx;
	const struct device *i2c_dev = parent;

	tcpci_ctx->common.write_byte = it8xxx2_emul_write_byte;
	tcpci_ctx->common.finish_write = it8xxx2_emul_handle_write;
	tcpci_ctx->common.read_byte = it8xxx2_emul_read_byte;
	tcpci_ctx->common.access_reg = it8xxx2_emul_tcpc_access_reg;

	tcpci_emul_i2c_init(emul, i2c_dev);

	it8xxx2_emul_reset(emul);

	zassert_ok(
		tcpci_emul_set_reg(emul, TCPC_REG_VENDOR_ID, it8xxx2_VENDOR_ID));
	zassert_ok(tcpci_emul_set_reg(emul, TCPC_REG_PRODUCT_ID, 0x1234));

	for (int i = 0; i < ARRAY_SIZE(it8xxx2_rv); ++i) {
		zassert_ok(tcpci_emul_set_reg(emul, it8xxx2_rv[i].reg,
					      it8xxx2_rv[i].val));
	}

	return 0;
}

static int i2c_it8xxx2_emul_transfer(const struct emul *target,
				    struct i2c_msg *msgs, int num_msgs,
				    int addr)
{
	struct tcpc_emul_data *tcpc_data = target->data;
	struct tcpci_ctx *tcpci_ctx = tcpc_data->tcpci_ctx;

	return i2c_common_emul_transfer_workhorse(target, &tcpci_ctx->common,
						  &tcpc_data->i2c_cfg, msgs,
						  num_msgs, addr);
}

static struct i2c_emul_api i2c_it8xxx2_emul_api = {
	.transfer = i2c_it8xxx2_emul_transfer,
};

#define it8xxx2_EMUL(n)                                     \
	TCPCI_EMUL_DEFINE(n, it8xxx2_emul_init, NULL, NULL, \
			  &i2c_it8xxx2_emul_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(it8xxx2_EMUL)

DT_INST_FOREACH_STATUS_OKAY(EMUL_STUB_DEVICE);
