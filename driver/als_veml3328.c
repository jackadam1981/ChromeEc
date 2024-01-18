/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Vishay VEML3328 light sensor driver
 */
#include "accelgyro.h"
#include "common.h"
#include "driver/als_veml3328.h"
#include "i2c.h"
#include "math_util.h"
#include "console.h"
#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ##args)
uint16_t veml3328_conf = VEML3328_CONF_DEFAULT;
#define VEML3328_GET_DATA(_s) ((struct veml3328_drv_data_t *)(_s)->drv_data)

/*
 * Read VEML3328 light sensor data.
 */
static int veml3328_read_lux_xyz(VEML3328_CALIB *calib, int *lux, float *x, float *y, float *z)
{
    int ret;
    int raw_data[5];
    for (int i = 0; i < 5; i++) {
        ret = i2c_read16(I2C_PORT_SENSOR, VEML3328_I2C_ADDR,
                         VEML3328_REG_C_DATA + i, &(raw_data[i]));
        if (ret)
            return ret;
        //CPRINTF("RAW[%d] = 0x%x\n", i, raw_data[i]);
    }

    float c = (float)raw_data[0];
    float r = (float)raw_data[1];
    float g = (float)raw_data[2];
    float b = (float)raw_data[3];
    float ir = (float)raw_data[4];

    if (c < 1.0)
        c = 1.0;
    if (r < 1.0)
        r = 1.0;
    if (g < 1.0)
        g = 1.0;
    if (b < 1.0)
        b = 1.0;

    float CCTi, CCTi0;
    CCTi0 = calib->per_model.Lccti0 * (float)(r + g - b) / (float)c +
            calib->per_model.Lccti1 * (float)(r + g) / (float)b;
    if ((calib->per_model.Lccti0 > 0) && ((r + g - b) <= 0)) {
        CCTi = 0.1;
    } else if (CCTi0 > calib->per_model.X1) {
        CCTi = CCTi0 * calib->per_model.Y1;
    } else if (CCTi0 < calib->per_model.X2) {
        CCTi = CCTi0 * calib->per_system.C_ccti;
    } else {
        CCTi = CCTi0 * (calib->per_system.C_ccti + (CCTi0 - calib->per_model.X2) *
                       ((calib->per_model.Y1 - calib->per_system.C_ccti) /
                        (calib->per_model.X1 - calib->per_model.X2)));
    }

    // Calculate x, y and z
    if ((r < 10) || (g < 10) || (b < 10) || (c < 10)) // Low lux
    {
        *x = 0.362;
        *y = 0.366;
    }
    else if ((r == 65535) || (g == 65535) || (b == 65535) || (c == 65535)) // High lux
    {
        *x = 0.362;
        *y = 0.366;
    }
    else
    {
        *x = (calib->per_system.Cx2 + calib->per_model.A2) * CCTi * CCTi +
            (calib->per_system.Cx1 + calib->per_model.A1) * CCTi +
            (calib->per_system.Cx0 + calib->per_model.A0);
        *y = (calib->per_system.Cy2 + calib->per_model.B2) * CCTi * CCTi +
            (calib->per_system.Cy1 + calib->per_model.B1) * CCTi +
            (calib->per_system.Cy0 + calib->per_model.B0);

        if (*x < calib->per_model.Dx_min)
            *x = calib->per_model.Dx_min;
        if (*x > calib->per_model.Dx_max)
            *x = calib->per_model.Dx_max;
        if (*y < calib->per_model.Dy_min)
            *y = calib->per_model.Dy_min;
        if (*y > calib->per_model.Dy_max)
            *y = calib->per_model.Dy_max;
    }
    *z = 1.0 - *x - *y;

    // Calculate lux
    float gain1[] = {0.5, 1.0, 2.0, 4.0};   // IT: 50ms, 100ms, 200ms, 400ms
    float gain2[] = {1.0, 2.0, 4.0};        // Gain1: x1, x2, x4
    float gain3[] = {1.0, 2.0, 4.0, 0.5};   // Gain2: x1, x2, x4, 1/2
    float gain4[] = {1.0, 1/3.0f};          // HD: x1, 1/3
    int IT_gain = (veml3328_conf & VEML3328_IT_MASK) >> VEML3328_IT_SHIFT;
    int Gain1_gain = (veml3328_conf & VEML3328_GAIN_1_MASK) >> VEML3328_GAIN_1_SHIFT;
    int Gain2_gain = (veml3328_conf & VEML3328_GAIN_2_MASK) >> VEML3328_GAIN_2_SHIFT;
    int HD_gain = (veml3328_conf & VEML3328_HD_MASK) >> VEML3328_HD_SHIFT;
    float Lux_gain = gain1[IT_gain] * gain2[Gain1_gain] * gain3[Gain2_gain] * gain4[HD_gain];
    float Ch, Cl;
    if ((r == 65535) || (g == 65535) || (b == 65535) || (c == 65535) || (ir == 65535))
    {
        Ch = 1;
        Cl = 1;
    } else {
        if ((r < 10) || (g < 10) || (c < 10))
        {
            Ch = 1;
        } else {
            Ch = calib->per_model.Lh1 * ir / c + calib->per_model.Lh0;
            if ((ir / c) <= calib->per_model.Jh)
                Ch = 1;
            if (Ch >= calib->per_model.Ch_max)
                Ch = calib->per_model.Ch_max;
            if (Ch <= calib->per_model.Ch_min)
                Ch = calib->per_model.Ch_min;
        }
        if ((r < 10) || (g < 10) || (b < 10) || (c < 10)) {
            Cl = 1;
        } else {
            Cl = calib->per_model.Ll1 * CCTi + calib->per_model.Ll0;
            if ((ir / c) >= calib->per_model.Jl)
                Cl = 1;
            if (Cl >= calib->per_model.Cl_max)
                Cl = calib->per_model.Cl_max;
            if (Cl <= calib->per_model.Cl_min)
                Cl = calib->per_model.Cl_min;
        }
    }
    *lux = (int)(calib->per_system.C_lux * Ch * Cl *
                 (calib->per_model.LG * g + calib->per_model.LC * c) / Lux_gain);
    return EC_SUCCESS;
}
/*
 * Read data from VEML3328 light sensor, and transfer unit into lux.
 */
static int veml3328_read(const struct motion_sensor_t *s, intv3_t v)
{
    struct veml3328_drv_data_t *drv_data = VEML3328_GET_DATA(s);
    int ret;
    int lux;
    float x, y, z;
    ret = veml3328_read_lux_xyz(&drv_data->calib, &lux, &x, &y, &z);
    if (ret)
        return ret;

    v[0] = lux;
    v[1] = 0;
    v[2] = 0;
    /*
     * Return an error when nothing change to prevent filling the
     * fifo with useless data.
     */
    if (v[0] == drv_data->last_value)
        return EC_ERROR_UNCHANGED;
    //CPRINTF("Lux = %d\n", lux);
    drv_data->last_value = v[0];
    return EC_SUCCESS;
}
static int veml3328_set_range(struct motion_sensor_t *s, int range, int rnd)
{
    return EC_SUCCESS;
}
static int veml3328_set_data_rate(const struct motion_sensor_t *s, int rate,
                                  int roundup)
{
    VEML3328_GET_DATA(s)->rate = rate;
    return EC_SUCCESS;
}
static int veml3328_get_data_rate(const struct motion_sensor_t *s)
{
    return VEML3328_GET_DATA(s)->rate;
}
static int veml3328_set_offset(const struct motion_sensor_t *s,
                               const int16_t *offset, int16_t temp)
{
    /* TODO: check calibration method */
    return EC_SUCCESS;
}
static int veml3328_get_offset(const struct motion_sensor_t *s, int16_t *offset,
                               int16_t *temp)
{
    /* TODO: check calibration method */
    return EC_SUCCESS;
}
/**
 * Initialise VEML3328 light sensor.
 */
static int veml3328_init(struct motion_sensor_t *s)
{
    int ret;
    ret = i2c_write16(s->port, s->i2c_spi_addr_flags,
                      VEML3328_REG_CONF, VEML3328_SD);
    if (ret)
        return ret;
    ret = i2c_write16(s->port, s->i2c_spi_addr_flags,
                      VEML3328_REG_CONF, veml3328_conf);
    if (ret)
        return ret;
    CPRINTS("veml3328_init-------------------------\n");
    return sensor_init_done(s);
}
const struct accelgyro_drv veml3328_drv = {
    .init = veml3328_init,
    .read = veml3328_read,
    .set_range = veml3328_set_range,
    .set_offset = veml3328_set_offset,
    .get_offset = veml3328_get_offset,
    .set_data_rate = veml3328_set_data_rate,
    .get_data_rate = veml3328_get_data_rate,
};
