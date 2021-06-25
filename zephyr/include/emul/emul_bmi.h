/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @file
 *
 * @brief Backend API for BMI emulator
 */

#ifndef __EMUL_BMI_H
#define __EMUL_BMI_H

#include <emul.h>
#include <drivers/i2c.h>
#include <drivers/i2c_emul.h>

/**
 * @brief BMA255 emulator backend API
 * @defgroup bmi_emul BMA255 emulator
 * @{
 *
 * BMA255 emulator supports responses to all write and read I2C messages.
 * Accelerometer registers are obtained from internal emulator state, range
 * register and offset. Only fast compensation is supported by default handler.
 * Registers backed in NVM are fully supported (GP0, GP1, offset). For proper
 * support for interrupts and FIFO, user needs to use custom handlers.
 * Application may alter emulator state:
 *
 * - define a Device Tree overlay file to set default NVM content, default
 *   static accelerometer value and which inadvisable driver behaviour should
 *   be treated as errors
 * - call @ref bmi_emul_set_read_func and @ref bmi_emul_set_write_func to setup
 *   custom handlers for I2C messages
 * - call @ref bmi_emul_set_reg and @ref bmi_emul_get_reg to set and get value
 *   of BMA255 registers
 * - call @ref bmi_emul_set_off and @ref bmi_emul_set_off to set and get
 *   internal offset value
 * - call @ref bmi_emul_set_acc and @ref bmi_emul_set_acc to set and get
 *   accelerometer value
 * - call bmi_emul_set_err_* to change emulator behaviour on inadvisable driver
 *   behaviour
 * - call @ref bmi_emul_set_read_fail_reg and @ref bmi_emul_set_write_fail_reg
 *   to configure emulator to fail on given register read or write
 */

/**
 * Axis argument used in @ref bmi_emul_set_acc @ref bmi_emul_get_acc
 * @ref bmi_emul_set_off and @ref bmi_emul_get_off
 */
#define BMI_EMUL_ACC_X		0
#define BMI_EMUL_ACC_Y		1
#define BMI_EMUL_ACC_Z		2
#define BMI_EMUL_GYR_X		3
#define BMI_EMUL_GYR_Y		4
#define BMI_EMUL_GYR_Z		5

#define BMI_EMUL_160		1
#define BMI_EMUL_260		2

/**
 * Acceleration 1g in internal emulator units. It is helpful for using
 * functions @ref bmi_emul_set_acc @ref bmi_emul_get_acc
 * @ref bmi_emul_set_off and @ref bmi_emul_get_off
 */
#define BMI_EMUL_1G		BIT(14)
#define BMI_EMUL_125_DEG_S	BIT(15)

#define BMI_EMUL_FRAME_CONFIG	BIT(0)
#define BMI_EMUL_FRAME_ACC	BIT(1)
#define BMI_EMUL_FRAME_MAG	BIT(2)
#define BMI_EMUL_FRAME_GYR	BIT(3)

#define BMI_EMUL_ACCESS_E	1

/**
 * Special register values used in @ref bmi_emul_set_read_fail_reg and
 * @ref bmi_emul_set_write_fail_reg
 */
#define BMI_EMUL_FAIL_ALL_REG	(-1)
#define BMI_EMUL_NO_FAIL_REG	(-2)

struct bmi_emul_frame {
	uint8_t type;
	uint8_t tag;
	uint8_t config;
	int32_t acc_x;
	int32_t acc_y;
	int32_t acc_z;
	int32_t gyr_x;
	int32_t gyr_y;
	int32_t gyr_z;
	int32_t mag_x;
	int32_t mag_y;
	int32_t mag_z;
	int32_t rhall;

	struct bmi_emul_frame *next;
};

struct bmi_emul_type_data {
	bool sensortime_follow_config_frame;

	int (*handle_write)(uint8_t *regs, struct i2c_emul *emul, int *reg,
			    int byte, uint8_t val);
	int (*handle_read)(uint8_t *regs, struct i2c_emul *emul, int *reg,
			   int byte, char *buf);
	void (*reset)(uint8_t *regs, struct i2c_emul *emul);
	const uint8_t *rsvd_mask;

	const int *nvm_reg;
	int nvm_len;

	int gyr_off_reg;
	int acc_off_reg;
	int gyr98_off_reg;
};

const struct bmi_emul_type_data *get_bmi160_emul_type_data(void);
const struct bmi_emul_type_data *get_bmi260_emul_type_data(void);

void bmi_emul_flush_fifo(struct i2c_emul *emul, bool tag_time, bool header);

void bmi_emul_reset_common(struct i2c_emul *emul, bool tag_time, bool header);

void bmi_emul_set_cmd_end_time(struct i2c_emul *emul, int time);

bool bmi_emul_is_cmd_end(struct i2c_emul *emul);

int bmi_emul_append_frame(struct i2c_emul *emul, struct bmi_emul_frame *frame);

uint16_t bmi_emul_fifo_len(struct i2c_emul *emul, bool tag_time, bool header);

uint8_t bmi_emul_get_fifo_data(struct i2c_emul *emul, int byte,
			       bool tag_time, bool header, int acc_shift,
			       int gyr_shift);

int32_t bmi_emul_get_value(struct i2c_emul *emul, int axis);

void bmi_emul_state_to_reg(struct i2c_emul *emul, int acc_shift,
			   int gyr_shift, int acc_reg, int gyr_reg,
			   int sensortime_reg, bool acc_off_en,
			   bool gyr_off_en);

/**
 * @brief Get pointer to BMA255 emulator using device tree order number.
 *
 * @param ord Device tree order number obtained from DT_DEP_ORD macro
 *
 * @return Pointer to BMA255 emulator
 */
struct i2c_emul *bmi_emul_get(int ord);

/**
 * @brief Custom function type that is used as user-defined callback in read
 *        I2C messages handling.
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Address which is now accessed by read command
 * @param data Pointer to custom user data
 *
 * @return 0 on success. Value of @p reg should be set by @ref bmi_emul_set_reg
 * @return 1 continue with normal BMA255 emulator handler
 * @return negative on error
 */
typedef int (*bmi_emul_read_func)(struct i2c_emul *emul, int reg, int byte,
				  void *data);

/**
 * @brief Custom function type that is used as user-defined callback in write
 *        I2C messages handling.
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Address which is now accessed by write command
 * @param val Value which is being written to @p reg
 * @param data Pointer to custom user data
 *
 * @return 0 on success
 * @return 1 continue with normal BMA255 emulator handler
 * @return negative on error
 */
typedef int (*bmi_emul_write_func)(struct i2c_emul *emul, int reg, int byte,
				   uint8_t val, void *data);

/**
 * @brief Lock access to BMA255 properties. After acquiring lock, user
 *        may change emulator behaviour in multi-thread setup.
 *
 * @param emul Pointer to BMA255 emulator
 * @param timeout Timeout in getting lock
 *
 * @return k_mutex_lock return code
 */
int bmi_emul_lock_data(struct i2c_emul *emul, k_timeout_t timeout);

/**
 * @brief Unlock access to BMA255 properties.
 *
 * @param emul Pointer to BMA255 emulator
 *
 * @return k_mutex_unlock return code
 */
int bmi_emul_unlock_data(struct i2c_emul *emul);

/**
 * @brief Set write handler for I2C messages. This function is called before
 *        generic handler.
 *
 * @param emul Pointer to BMA255 emulator
 * @param func Pointer to custom function
 * @param data User data passed on call of custom function
 */
void bmi_emul_set_write_func(struct i2c_emul *emul, bmi_emul_write_func func,
			     void *data);

/**
 * @brief Set read handler for I2C messages. This function is called before
 *        generic handler.
 *
 * @param emul Pointer to BMA255 emulator
 * @param func Pointer to custom function
 * @param data User data passed on call of custom function
 */
void bmi_emul_set_read_func(struct i2c_emul *emul, bmi_emul_read_func func,
			    void *data);

/**
 * @brief Set value of given register of BMA255
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Register address which value will be changed
 * @param val New value of the register
 */
void bmi_emul_set_reg(struct i2c_emul *emul, int reg, uint8_t val);

/**
 * @brief Get value of given register of BMA255
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Register address
 *
 * @return Value of the register
 */
uint8_t bmi_emul_get_reg(struct i2c_emul *emul, int reg);

/**
 * @brief Setup fail on read of given register of BMA255
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Register address or one of special values (bmi_EMUL_FAIL_ALL_REG,
 *            bmi_EMUL_NO_FAIL_REG)
 */
void bmi_emul_set_read_fail_reg(struct i2c_emul *emul, int reg);

/**
 * @brief Setup fail on write of given register of BMA255
 *
 * @param emul Pointer to BMA255 emulator
 * @param reg Register address or one of special values (bmi_EMUL_FAIL_ALL_REG,
 *            bmi_EMUL_NO_FAIL_REG)
 */
void bmi_emul_set_write_fail_reg(struct i2c_emul *emul, int reg);

/**
 * @brief Get internal value of offset for given axis
 *
 * @param emul Pointer to BMA255 emulator
 * @param axis Axis to access: 0 - X, 1 - Y, 2 - Z
 *
 * @return Offset of given axis. LSB is 0.97mg
 */
int16_t bmi_emul_get_off(struct i2c_emul *emul, int axis);

/**
 * @brief Set internal value of offset for given axis
 *
 * @param emul Pointer to BMA255 emulator
 * @param axis Axis to access: 0 - X, 1 - Y, 2 - Z
 * @param val New value of offset. LSB is 0.97mg
 */
void bmi_emul_set_off(struct i2c_emul *emul, int axis, int16_t val);

/**
 * @brief Get internal value of accelerometer for given axis
 *
 * @param emul Pointer to BMA255 emulator
 * @param axis Axis to access: 0 - X, 1 - Y, 2 - Z
 *
 * @return Acceleration of given axis. LSB is 0.97mg
 */
int16_t bmi_emul_get_acc(struct i2c_emul *emul, int axis);

/**
 * @brief Set internal value of accelerometr for given axis
 *
 * @param emul Pointer to BMA255 emulator
 * @param axis Axis to access: 0 - X, 1 - Y, 2 - Z
 * @param val New value of accelerometer axis. LSB is 0.97mg
 */
void bmi_emul_set_value(struct i2c_emul *emul, int axis, int32_t val);

/**
 * @brief Set if error should be generated when fast compensation is triggered
 *        when not ready flag is set
 *
 * @param emul Pointer to BMA255 emulator
 * @param set Check for this error
 */
void bmi_emul_set_err_on_cal_nrdy(struct i2c_emul *emul, bool set);

/**
 * @brief Set if error should be generated when fast compensation is triggered
 *        when range is not 2G
 *
 * @param emul Pointer to BMA255 emulator
 * @param set Check for this error
 */
void bmi_emul_set_err_on_cal_bad_range(struct i2c_emul *emul, bool set);

/**
 * @brief Set if error should be generated when read only register is being
 *        written
 *
 * @param emul Pointer to BMA255 emulator
 * @param set Check for this error
 */
void bmi_emul_set_err_on_ro_write(struct i2c_emul *emul, bool set);

/**
 * @brief Set if error should be generated when reserved bits of register are
 *        not set to 0 on write I2C message
 *
 * @param emul Pointer to BMA255 emulator
 * @param set Check for this error
 */
void bmi_emul_set_err_on_rsvd_write(struct i2c_emul *emul, bool set);

/**
 * @brief Set if error should be generated when MSB register is accessed before
 *        LSB register
 *
 * @param emul Pointer to BMA255 emulator
 * @param set Check for this error
 */
void bmi_emul_set_err_on_msb_first(struct i2c_emul *emul, bool set);

/**
 * @}
 */

#endif /* __EMUL_BMI_H */
