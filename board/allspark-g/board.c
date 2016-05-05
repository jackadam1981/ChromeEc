/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* cube board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "rtk_types.h"
#include "rtk_error.h"
#include "rtk_switch.h"
#include "vlan.h"

#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

void board_config_pre_init(void)
{
	/* enable SYSCFG clock */
	STM32_RCC_APB2ENR |= 1 << 0;
}

#include "gpio_list.h"

const struct adc_t adc_channels[] = {
	/* PA1: STM32_AIN 1 */
	[ADC_C0_CC1_PD] = {"CC1",  3300, 4096, 0, STM32_AIN(1)},
	/* 1/2 VBUS voltage */
	[ADC_VBUS]      = {"VBUS", 36300, 4096, 0, STM32_AIN(8)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 1000, GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);


#ifdef CONFIG_RTK_SWITCH
static void board_rtk_switch_init(void)
{
#define VLAN1 1
#define VLAN2 2
  int ret;
  rtk_vlan_cfg_t vlan1, vlan2;

  ccprintf("RTK switch init start\n");

  ret = rtk_switch_init();
  if (ret != RT_ERR_OK){
    ccprintf("RTK switch init failed (%d)\n", ret);
    return;
  }

  /* Initialize VLAN */
  ret = rtk_vlan_init();
  if (ret != RT_ERR_OK){
    ccprintf("RTK switch VLAN init failed (%d)\n", ret);
    return;
  }

  /* VLAN1 member: UTP0, UTP1 */
  /* VLAN2 member: UTP0, UTP2 */
  memset(&vlan1, 0, sizeof(vlan1));
  memset(&vlan2, 0, sizeof(vlan2));
  RTK_PORTMASK_PORT_SET(vlan1.mbr, UTP_PORT0);
  RTK_PORTMASK_PORT_SET(vlan1.mbr, UTP_PORT3);
  RTK_PORTMASK_PORT_SET(vlan1.untag, UTP_PORT0);
  ret = rtk_vlan_set(VLAN1, &vlan1);
  if (ret != RT_ERR_OK){
    ccprintf("RTK VLAN1 init failed (%d)\n", ret);
    return;
  }
  RTK_PORTMASK_PORT_SET(vlan2.mbr, UTP_PORT1);
  RTK_PORTMASK_PORT_SET(vlan2.mbr, UTP_PORT3);
  RTK_PORTMASK_PORT_SET(vlan2.untag, UTP_PORT1);
  ret = rtk_vlan_set(VLAN2, &vlan2);
  if (ret != RT_ERR_OK){
    ccprintf("RTK VLAN2 init failed (%d)\n", ret);
    return;
  }

  /* Set PVID for each port */
  rtk_vlan_portPvid_set(UTP_PORT0, VLAN1, 0);
  rtk_vlan_portPvid_set(UTP_PORT1, VLAN2, 0);
  rtk_vlan_portPvid_set(UTP_PORT3, VLAN1, 0);

  ccprintf("RTK switch init end\n");
}
DECLARE_DEFERRED(board_rtk_switch_init);

static int init_rtk_switch(int argc, char **argv)
{
  gpio_set_level(GPIO_RTK_SW_VDDH_EN, 1);
  gpio_set_level(GPIO_RTK_SW_VDDL_EN, 1);

  hook_call_deferred(&board_rtk_switch_init_data, 1000 * MSEC);
  return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(initrtk, init_rtk_switch,
    NULL,
    "Init RTK switch",
    NULL);
#endif //CONFIG_RTK_SWITCH

static void board_init(void)
{
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
