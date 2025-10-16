#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(rts5453, CONFIG_USBC_LOG_LEVEL);
#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "third_party/incbin/incbin.h"
/* TPS6699X_FW_ROOT is defined in this directory's CMakeLists.txt and points to()
 * ${PLATFORM_EC}/zephyr/drivers/usbc
 */
INCBIN(rts54xx_fw, STRINGIFY(RTS54XX_FW_ROOT) "/rts54xx.bin");
#define RTS_I2C_WINDOW_SPEED_KHZ 400
#define FW_MAJOR_VERSION_SHIFT 16
#define FW_MINOR_VERSION_SHIFT 8
#define FW_PATCH_VERSION_SHIFT 0
#define FW_HASH_SIZE 3
#define PING_STATUS_DELAY_MS 10
#define PING_STATUS_TIMEOUT_US 10000000 /* 10s */
#define PING_STATUS_MASK 0x3
#define PING_STATUS_COMPLETE 0x1
#define PING_STATUS_INVALID_FMT 0x3
#define RTS_RESTART_DELAY_US 7000000 /* 7s */
#define MAX_COMMAND_SIZE 32
#define FW_CHUNK_SIZE 29
#define FLASH_SEGMENT_SIZE (64 * 1024)
#define RTS545X_VENDOR_CMD 0x01
#define RTS545X_FLASH_ERASE_CMD 0x03
#define RTS545X_FLASH_WRITE_0_64K_CMD 0x04
#define RTS545X_RESET_TO_FLASH_CMD 0x05
#define RTS545X_FLASH_WRITE_64K_128K_CMD 0x06
#define RTS545X_FLASH_WRITE_128K_192K_CMD 0x13
#define RTS545X_FLASH_WRITE_192K_256K_CMD 0x14
#define RTS545X_VALIDATE_ISP_CMD 0x16
#define RTS545X_GET_IC_STATUS_CMD 0x3A
#define RTS54XX_BLOCK_READ_CMD 0x80
struct i2c_dt_spec *i2c;
enum flash_write_cmd_off {
	ADDR_L_OFF,
	ADDR_H_OFF,
	DATA_COUNT_OFF,
	DATA_OFF,
};
struct rts5453_ic_status {
	uint8_t byte_count;
	uint8_t code_location;
	uint16_t reserved_0;
	uint8_t major_version;
	uint8_t minor_version;
	uint8_t patch_version;
	uint16_t reserved_1;
	uint8_t pd_typec_status;
	uint8_t vid_pid[4];
	uint8_t reserved_2;
	uint8_t flash_bank;
	uint8_t reserved_3[16];
} __attribute__((__packed__));
static struct rts5453_ic_status ic_status;
static int get_ping_status(uint8_t *status_byte)
{
	struct i2c_msg msg;
	msg.buf = status_byte;
	msg.len = 1;
	msg.flags = I2C_MSG_READ | I2C_MSG_STOP;
	return i2c_transfer_dt(i2c, &msg, 1);
}
static int rts54xx_i2c_read(uint8_t *buf, uint8_t len)
{
	struct i2c_msg msg[2];
	uint8_t cmd = RTS54XX_BLOCK_READ_CMD;
	int rv;
	msg[0].buf = &cmd;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;
	msg[1].buf = buf;
	msg[1].len = len;
	msg[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;
	rv = i2c_transfer_dt(i2c, msg, 2);
	
	if(rv < 0)
	    LOG_INF(" rv: 0x%x\n", rv);
	return rv;
}
static int rts54xx_i2c_write(uint8_t *buf, uint8_t len)
{
	struct i2c_msg msg;
	msg.buf = buf;
	msg.len = len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;
	return i2c_transfer_dt(i2c, &msg, 1);
}
static int rts545x_ping_status(uint8_t *status_byte)
{
	int ret;
        int retry = 3;
	while (retry) {
		ret = get_ping_status(status_byte);
		if (ret < 0) {
			LOG_INF("%s: Error %d reading ping_status\n", __func__, ret);
			return ret;
		}
		/* Command execution is complete */
		if ((*status_byte & PING_STATUS_MASK) == PING_STATUS_COMPLETE)
			return 0;
		/* Invalid command format */
		if ((*status_byte & PING_STATUS_MASK) == PING_STATUS_INVALID_FMT)
			return -EINVAL;
		k_msleep(PING_STATUS_DELAY_MS);
	}
	return -ETIMEDOUT;
}
static int rts545x_block_out_transfer(uint8_t cmd_code, size_t len,
				      const uint8_t *write_data, uint8_t *status_byte)
{
	int ret;
	/* Command byte + Byte Count + Data[0..31] */
	uint8_t write_buf[MAX_COMMAND_SIZE + 2];
	if ((len + 2) > ARRAY_SIZE(write_buf))
		return -EINVAL;
	write_buf[0] = cmd_code;
	write_buf[1] = len;
	memcpy(&write_buf[2], write_data, len);
	ret = rts54xx_i2c_write(write_buf, len + 2);
	if (ret < 0) {
		LOG_INF("%s: Error %d sending command %#02x\n", __func__, ret, cmd_code);
		return ret;
	}
	if (cmd_code == RTS545X_RESET_TO_FLASH_CMD)
		return 0;
	return rts545x_ping_status(status_byte);
}
static int rts545x_block_in_transfer(size_t len, uint8_t *read_data)
{
	return rts54xx_i2c_read(read_data, len);
}
static int rts545x_vendor_cmd_enable()
{
	uint8_t vendor_cmd_enable[] = {0xDA, 0x0B, 0x01};
	uint8_t ping_status;
	int ret;
	ret = rts545x_block_out_transfer(RTS545X_VENDOR_CMD, ARRAY_SIZE(vendor_cmd_enable),
					 vendor_cmd_enable, &ping_status);
	if (ret)
		LOG_INF("%s failed: %d\n", __func__, ret);
	return ret;
}
static int rts545x_get_ic_status(struct rts5453_ic_status *ic_sts)
{
	uint8_t get_ic_status[] = {0x00, 0x00, sizeof(*ic_sts) - 1};
	uint8_t ping_status;
	int ret;
	ret = rts545x_block_out_transfer(RTS545X_GET_IC_STATUS_CMD,
					 ARRAY_SIZE(get_ic_status), get_ic_status,
					 &ping_status);
	if (ret) {
		LOG_INF("%s failed: %d\n", __func__, ret);
		return ret;
	}
	ret = rts545x_block_in_transfer(sizeof(*ic_sts), (uint8_t *)ic_sts);
	if (ret) {
		LOG_INF("%s failed: %d reading IC_STATUS\n", __func__, ret);
		return ret;
	}
	LOG_INF("%s: VID:PID %x%x:%x%x, FW_Ver %u.%u.%u, %s Bank:%d\n", __func__,
	       ic_sts->vid_pid[1], ic_sts->vid_pid[0], ic_sts->vid_pid[3], ic_sts->vid_pid[2],
	       ic_sts->major_version, ic_sts->minor_version, ic_sts->patch_version,
	       ic_sts->code_location ? "Flash" : "ROM", ic_sts->flash_bank ? 1 : 0);
	return 0;
}
static int rts545x_flash_access_enable()
{
	uint8_t flash_access_enable[] = {0xDA, 0x0B, 0x03};
	uint8_t ping_status;
	int ret;
	ret = rts545x_block_out_transfer(RTS545X_VENDOR_CMD,
					 ARRAY_SIZE(flash_access_enable), flash_access_enable,
					 &ping_status);
	if (ret)
		LOG_INF("%s failed: %d\n", __func__, ret);
	return ret;
}
static int rts545x_flash_write(const uint8_t *image, size_t image_size)
{
	uint8_t flash_write[MAX_COMMAND_SIZE] = {0};
	uint8_t bank0_write_cmds[] = {RTS545X_FLASH_WRITE_0_64K_CMD,
				      RTS545X_FLASH_WRITE_64K_128K_CMD};
	uint8_t bank1_write_cmds[] = {RTS545X_FLASH_WRITE_128K_192K_CMD,
				      RTS545X_FLASH_WRITE_192K_256K_CMD};
	uint8_t *flash_write_cmds;
	uint32_t seg_boundary = FLASH_SEGMENT_SIZE;
	uint8_t cmd, ping_status, segment, size = 0;
	uint32_t offset = 0;
	uint32_t progress_counter = 0;
	int ret = 0;
	/* If running ROM code or flash_bank 1, program flash_bank 0 */
	if (!ic_status.code_location || ic_status.flash_bank)
		flash_write_cmds = bank0_write_cmds;
	else
		flash_write_cmds = bank1_write_cmds;
	while (offset < image_size) {
		segment = offset < FLASH_SEGMENT_SIZE ? 0 : 1;
		seg_boundary = (segment + 1) * FLASH_SEGMENT_SIZE;
		cmd = flash_write_cmds[segment];
		size = MIN(FW_CHUNK_SIZE, MIN(seg_boundary, image_size) - offset);
		flash_write[ADDR_L_OFF] = (uint8_t)(offset & 0xff);
		flash_write[ADDR_H_OFF] = (uint8_t)((offset >> 8) & 0xff);
		flash_write[DATA_COUNT_OFF] = size;
		memcpy(&flash_write[DATA_OFF], &image[offset], size);
		/* Account for ADDR_L, ADDR_H, Write Data Count */
		ret = rts545x_block_out_transfer(cmd, size + 3, flash_write, &ping_status);
		if (ret) {
			LOG_INF("%s: failed(%d) @off:0x%x\n", __func__, ret, offset);
			break;
		}
		offset += size;
		progress_counter += size;
		if (progress_counter >= 4000) {
			/* Prints an update every 4000 bytes transferred */
			LOG_INF("%s: Progress: %u / %zu\n", __func__, offset,
			       image_size);
			progress_counter = 0;
		}
	}
	return ret;
}
static int rts545x_flash_access_disable()
{
	return rts545x_vendor_cmd_enable();
}
static int rts545x_validate_firmware()
{
	uint8_t validate_isp[] = {0x01};
	uint8_t ping_status;
	int ret;
	/* ROM Code does not support validate_isp command */
	if (!ic_status.code_location)
		return 0;
	ret = rts545x_block_out_transfer(RTS545X_VALIDATE_ISP_CMD, ARRAY_SIZE(validate_isp),
					 validate_isp, &ping_status);
	if (ret)
		LOG_INF("%s: failed: %d", __func__, ret);
	return ret;
}
static int rts545x_reset_to_flash()
{
	uint8_t reset_to_flash[] = {0xDA, 0x0B, 0x01};
	uint8_t ping_status;
	int ret;
	ret = rts545x_block_out_transfer(RTS545X_RESET_TO_FLASH_CMD,
					 ARRAY_SIZE(reset_to_flash), reset_to_flash,
					 &ping_status);
	if (ret)
		LOG_INF("%s failed: %d\n", __func__, ret);
	/* Programming sequence recommends 5 second delay for reset */
	k_msleep(5000);
	return ret;
}
static int rts545x_confirm_flash_update()
{
	int ret;
	struct rts5453_ic_status new_ic_status;
	uint8_t exp_flash_bank =
		ic_status.code_location == 0 ? 0x00 : (ic_status.flash_bank ^ 0x10);
	ret = rts545x_vendor_cmd_enable();
	if (ret)
		return ret;
	ret = rts545x_get_ic_status(&new_ic_status);
	if (ret)
		return ret;
	if (new_ic_status.flash_bank != exp_flash_bank) {
		LOG_INF("%s: failed. Exp %#02x != Actual %#02x\n", __func__, exp_flash_bank,
		       new_ic_status.flash_bank);
		return -1;
	}
	return 0;
}
static int rts545x_update_flash()
{
	int ret;
	/* Follow RTS545x ISP Flow procedure. Note that the ISP flow for ROM to
	 * MC is not currently supported.
	 */
	LOG_INF("%s: Vendor command enable... ", __func__);
	ret = rts545x_vendor_cmd_enable();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	ret = rts545x_get_ic_status(&ic_status);
	if (ret) {
		LOG_INF("%s: IC status failed (%d)\n", __func__, ret);
		return ret;
	}
	LOG_INF("%s: Got IC status\n", __func__);
	LOG_INF("%s: Flash access enable... ", __func__);
	ret = rts545x_flash_access_enable();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	/* TODO(b/323608798): Add flash unlock step support */
	LOG_INF("%s: Starting flash write\n", __func__);
	ret = rts545x_flash_write(g_rts54xx_fw_data, g_rts54xx_fw_size);
	if (ret) {
		LOG_INF("%s: Failed during flash write", __func__);
		rts545x_flash_access_disable();
		return ret;
	}
	LOG_INF("%s: Completed flash write\n", __func__);
	LOG_INF("%s: Flash access disable... ", __func__);
	ret = rts545x_flash_access_disable();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	LOG_INF("%s: Validate FW... ", __func__);
	ret = rts545x_validate_firmware();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	LOG_INF("%s: Reset to flash... ", __func__);
	ret = rts545x_reset_to_flash();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	LOG_INF("%s: Confirm update... ", __func__);
	ret = rts545x_confirm_flash_update();
	if (ret) {
		LOG_INF("fail (%d)\n", ret);
		return ret;
	}
	LOG_INF("success\n");
	return 0;
}
/**
 * @brief Temporary EC-based FW update routine
 *
 * @param dev Device pointer for the PDC to update (needed only once per chip)
 */
int rts54xx_do_firmware_update_internal(const struct i2c_dt_spec *_i2c)
{
    LOG_INF("rts54xx_do_firmware_update_internal\n");
    i2c = (struct i2c_dt_spec *)_i2c;
    
    return rts545x_update_flash();
}
