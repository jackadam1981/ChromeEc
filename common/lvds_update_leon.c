#include "timer.h"
#include "console.h"
#include "i2c.h"

void lvds_i2c_update(void)
{
	int rv;
	/* Stop FW. */
	rv = i2c_write8(0x01, 0x6A, 0x80, 0x01);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	/* Make sure FW stop for first command. */
	msleep(60);

	/* Update power sequence setting. */
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x32);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x14);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x33);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x37);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x34);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x0E);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x35);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x02);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x36);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x37);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x37);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x14);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x38);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x82);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}

	/* Update SSCG setting. */
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x39);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0xAB);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3A);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x08);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}

	/* Update LVDS swap setting. */
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3B);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x04);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}

	/* Default setting. */
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3C);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x06);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3D);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x38);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3E);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x73);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x3F);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x33);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x06);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x90);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x06);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0xB0);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x01, 0x06);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}
	rv = i2c_write8(0x01, 0x6A, 0x00, 0x80);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}

	/* Finish update, FW start running. */
	rv = i2c_write8(0x01, 0x6A, 0x80, 0x00);
	if (rv) {
		ccprintf("LVDS Write Fail");
		return;
	}

	ccprintf("LVDS Write Finish");

	return;
}
