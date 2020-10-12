#include "common.h"
#include "i2c.h"
#include "usb_mux.h"
#include "tcpci.h"

#define TCPC_REG_FLIPPED_MUX_SWITCH 0x20
#define TCPC_REG_USB_MUX_SWITCH 0x40
#define TCPC_REG_DP_MUX_SWITCH 0x80

static int ps8755_read_firmcmd(const struct usb_mux *me, int *val)
{
	int rv = i2c_read8(me->i2c_port, 0x12, 0x20, val);
	if (rv == EC_SUCCESS)
		ccprints("i2c read port 0x%x addr 0x12 reg 0x20 SUCCESS: 0x%x", me->i2c_port, *val);
	else
		ccprints("i2c read port 0x%x addr 0x12 reg 0x20 FAILED", me->i2c_port);
	return rv;
}

static int ps8755_write_firmcmd(const struct usb_mux *me, int val)
{
	int rv = i2c_write8(me->i2c_port, 0x12, 0x20, val);
	if (rv == EC_SUCCESS)
		ccprints("i2c write port 0x%x addr 0x12 reg 0x20 val 0x%x SUCCESS", me->i2c_port, val);
	else
		ccprints("i2c write port 0x%x addr 0x12 reg 0x20 FAILED", me->i2c_port);
	return rv;
}

int ps8755_mux_set(const struct usb_mux *me, mux_state_t mux_state)
{
	int rv;
	int reg = 0;
	int firmcmd = 0;

	ccprints("ps8755_mux_set");

	/* Parameter is port only */
	rv = mux_read(me, TCPC_REG_CONFIG_STD_OUTPUT, &reg);
	if (rv != EC_SUCCESS) {
		ccprints("read TCPC_REG_CONFIG_STD_OUTPUT error");
		return rv;
	}
	reg &= ~(TCPC_REG_CONFIG_STD_OUTPUT_MUX_MASK |
		 TCPC_REG_CONFIG_STD_OUTPUT_CONNECTOR_FLIPPED);

	rv = ps8755_read_firmcmd(me, &firmcmd);
	if (rv != EC_SUCCESS) {
		ccprints("read ps8755_read_firmcmd error");
		return rv;
	}
	firmcmd &= 0x1f;

	if (mux_state & USB_PD_MUX_USB_ENABLED) {
		ccprints("ps8755_mux_set: USB");
		reg |= TCPC_REG_CONFIG_STD_OUTPUT_MUX_USB;
		firmcmd |= TCPC_REG_USB_MUX_SWITCH;
	}
	if (mux_state & USB_PD_MUX_DP_ENABLED) {
		ccprints("ps8755_mux_set: DP");
		reg |= TCPC_REG_CONFIG_STD_OUTPUT_MUX_DP;
		firmcmd |= TCPC_REG_DP_MUX_SWITCH;
	}
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED) {
		ccprints("ps8755_mux_set: FLIP");
		reg |= TCPC_REG_CONFIG_STD_OUTPUT_CONNECTOR_FLIPPED;
		firmcmd |= TCPC_REG_FLIPPED_MUX_SWITCH;
	}

	/* Parameter is port only */
	rv = mux_write(me, TCPC_REG_CONFIG_STD_OUTPUT, reg);
	if (rv != EC_SUCCESS) {
		ccprints("write TCPC_REG_CONFIG_STD_OUTPUT error");
		return rv;
	} else {
		ccprints("write TCPC_REG_CONFIG_STD_OUTPUT value 0x%x success", reg);
	}
	return ps8755_write_firmcmd(me, firmcmd);
}

const struct usb_mux_driver ps8755_usb_mux_driver = {
	.init = &tcpci_tcpm_mux_init,
	.set = &ps8755_mux_set,
	.get = &tcpci_tcpm_mux_get,
};
