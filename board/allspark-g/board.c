/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* allspark-g board configuration */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "registers.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"
#include "gpio_list.h"

#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* GPIO level setting helpers through BSRR register */
#define GPIO_SET(n)   (1 << (n))
#define GPIO_RESET(n) (1 << ((n) + 16))

const struct adc_t adc_channels[] = {
  /* PA0: STM32_AIN 0 */
  [ADC_TEMP_DETECT] = {"TEMP",  3300, 4096, 0, STM32_AIN(0)},
	/* PA1: STM32_AIN 1 */
	[ADC_C0_CC1_PD]   = {"CC1",   3300, 4096, 0, STM32_AIN(1)},
	/* 1/2 VBUS voltage */
	[ADC_VBUS]        = {"VBUS", 36300, 4096, 0, STM32_AIN(3)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct i2c_port_t i2c_ports[] = {
	{"master", I2C_PORT_MASTER, 1000, GPIO_MASTER_I2C_SCL, GPIO_MASTER_I2C_SDA}
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

/* I2C slave address of Realtek switch */
#define RTK_SWITCH_I2C_ADDR   0xB8

/* Register addresses and values for switch configuration */
const uint16_t enable_vlan_reg_data[][2] = {
  {0x13c2, 0x0000},
  {0x0018, 0x0f00},
  {0x1d15, 0x0a69},
  {0x1f02, 0x2014},
  {0x1f00, 0x0001},
  {0x1d15, 0x0a69},
  {0x1f03, 0x0000},
  {0x1f02, 0x2014},
  {0x1f00, 0x0003},
  {0x0038, 0x0f00},
  {0x1d15, 0x0a69},
  {0x1f02, 0x2034},
  {0x1f00, 0x0001},
  {0x1d15, 0x0a69},
  {0x1f03, 0x0000},
  {0x1f02, 0x2034},
  {0x1f00, 0x0003},
  {0x0058, 0x0f00},
  {0x1d15, 0x0a69},
  {0x1f02, 0x2054},
  {0x1f00, 0x0001},
  {0x1d15, 0x0a69},
  {0x1f03, 0x0000},
  {0x1f02, 0x2054},
  {0x1f00, 0x0003},
  {0x0078, 0x0f00},
  {0x1d15, 0x0a69},
  {0x1f02, 0x2074},
  {0x1f00, 0x0001},
  {0x1d15, 0x0a69},
  {0x1f03, 0x0000},
  {0x1f02, 0x2074},
  {0x1f00, 0x0003},
  {0x0098, 0x0f00},
  {0x1d15, 0x0a69},
  {0x1f02, 0x2094},
  {0x1f00, 0x0001},
  {0x1d15, 0x0a69},
  {0x1f03, 0x0000},
  {0x1f02, 0x2094},
  {0x1f00, 0x0003},
  {0x13eb, 0x15bb},
  {0x1303, 0x06d6},
  {0x1304, 0x0700},
  {0x13e2, 0x003f},
  {0x13f9, 0x0090},
  {0x121e, 0x03ca},
  {0x1233, 0x0352},
  {0x1237, 0x00a0},
  {0x123a, 0x0030},
  {0x1239, 0x0084},
  {0x0301, 0x1000},
  {0x1349, 0x001f},
  {0x18e0, 0x4004},
  {0x122b, 0x641c},
  {0x1305, 0xc000},
  {0x1200, 0x7fcb},
  {0x0884, 0x0003},
  {0x06eb, 0x0001},
  {0x00cf, 0xffff},
  {0x00d0, 0x0007},
  {0x00ce, 0x48b0},
  {0x0398, 0xffff},
  {0x0399, 0x0007},
  {0x0300, 0x0001},
  {0x03fa, 0x0007},
  {0x08c8, 0x00c0},
  {0x0a30, 0x020e},
  {0x0800, 0x0000},
  {0x0802, 0x0000},
  {0x09da, 0x0017},
  {0x1d32, 0x0002},
  {0x0728, 0x0000},
  {0x0729, 0x0000},
  {0x072a, 0x0000},
  {0x072b, 0x0000},
  {0x072c, 0x0000},
  {0x072d, 0x0000},
  {0x072e, 0x0000},
  {0x072f, 0x0000},
  {0x0730, 0x0000},
  {0x0731, 0x0000},
  {0x0732, 0x0000},
  {0x0733, 0x0000},
  {0x0734, 0x0000},
  {0x0735, 0x0000},
  {0x0736, 0x0000},
  {0x0737, 0x0000},
  {0x0738, 0x0000},
  {0x0739, 0x0000},
  {0x073a, 0x0000},
  {0x073b, 0x0000},
  {0x073c, 0x0000},
  {0x073d, 0x0000},
  {0x073e, 0x0000},
  {0x073f, 0x0000},
  {0x0740, 0x0000},
  {0x0741, 0x0000},
  {0x0742, 0x0000},
  {0x0743, 0x0000},
  {0x0744, 0x0000},
  {0x0745, 0x0000},
  {0x0746, 0x0000},
  {0x0747, 0x0000},
  {0x0748, 0x0000},
  {0x0749, 0x0000},
  {0x074a, 0x0000},
  {0x074b, 0x0000},
  {0x074c, 0x0000},
  {0x074d, 0x0000},
  {0x074e, 0x0000},
  {0x074f, 0x0000},
  {0x0750, 0x0000},
  {0x0751, 0x0000},
  {0x0752, 0x0000},
  {0x0753, 0x0000},
  {0x0754, 0x0000},
  {0x0755, 0x0000},
  {0x0756, 0x0000},
  {0x0757, 0x0000},
  {0x0758, 0x0000},
  {0x0759, 0x0000},
  {0x075a, 0x0000},
  {0x075b, 0x0000},
  {0x075c, 0x0000},
  {0x075d, 0x0000},
  {0x075e, 0x0000},
  {0x075f, 0x0000},
  {0x0760, 0x0000},
  {0x0761, 0x0000},
  {0x0762, 0x0000},
  {0x0763, 0x0000},
  {0x0764, 0x0000},
  {0x0765, 0x0000},
  {0x0766, 0x0000},
  {0x0767, 0x0000},
  {0x0768, 0x0000},
  {0x0769, 0x0000},
  {0x076a, 0x0000},
  {0x076b, 0x0000},
  {0x076c, 0x0000},
  {0x076d, 0x0000},
  {0x076e, 0x0000},
  {0x076f, 0x0000},
  {0x0770, 0x0000},
  {0x0771, 0x0000},
  {0x0772, 0x0000},
  {0x0773, 0x0000},
  {0x0774, 0x0000},
  {0x0775, 0x0000},
  {0x0776, 0x0000},
  {0x0777, 0x0000},
  {0x0778, 0x0000},
  {0x0779, 0x0000},
  {0x077a, 0x0000},
  {0x077b, 0x0000},
  {0x077c, 0x0000},
  {0x077d, 0x0000},
  {0x077e, 0x0000},
  {0x077f, 0x0000},
  {0x0780, 0x0000},
  {0x0781, 0x0000},
  {0x0782, 0x0000},
  {0x0783, 0x0000},
  {0x0784, 0x0000},
  {0x0785, 0x0000},
  {0x0786, 0x0000},
  {0x0787, 0x0000},
  {0x0788, 0x0000},
  {0x0789, 0x0000},
  {0x078a, 0x0000},
  {0x078b, 0x0000},
  {0x078c, 0x0000},
  {0x078d, 0x0000},
  {0x078e, 0x0000},
  {0x078f, 0x0000},
  {0x0790, 0x0000},
  {0x0791, 0x0000},
  {0x0792, 0x0000},
  {0x0793, 0x0000},
  {0x0794, 0x0000},
  {0x0795, 0x0000},
  {0x0796, 0x0000},
  {0x0797, 0x0000},
  {0x0798, 0x0000},
  {0x0799, 0x0000},
  {0x079a, 0x0000},
  {0x079b, 0x0000},
  {0x079c, 0x0000},
  {0x079d, 0x0000},
  {0x079e, 0x0000},
  {0x079f, 0x0000},
  {0x07a0, 0x0000},
  {0x07a1, 0x0000},
  {0x07a2, 0x0000},
  {0x07a3, 0x0000},
  {0x07a4, 0x0000},
  {0x07a5, 0x0000},
  {0x07a6, 0x0000},
  {0x07a7, 0x0000},
  {0x0510, 0xdfdf},
  {0x0511, 0x0000},
  {0x0512, 0x0000},
  {0x0501, 0x0001},
  {0x0500, 0x000b},
  {0x0728, 0x00df},
  {0x0729, 0x0000},
  {0x072a, 0x0000},
  {0x072b, 0x0001},
  {0x0700, 0x0000},
  {0x0851, 0x0000},
  {0x000e, 0x4880},
  {0x0700, 0x0000},
  {0x0851, 0x0000},
  {0x002e, 0x4880},
  {0x0701, 0x0000},
  {0x0851, 0x0000},
  {0x004e, 0x4880},
  {0x0701, 0x0000},
  {0x0851, 0x0000},
  {0x006e, 0x4880},
  {0x0702, 0x0000},
  {0x0852, 0x0000},
  {0x008e, 0x4880},
  {0x0703, 0x0000},
  {0x0852, 0x0000},
  {0x00ce, 0x4880},
  {0x0703, 0x0000},
  {0x0852, 0x0000},
  {0x00ee, 0x4880},
  {0x07a9, 0x0001},
  {0x07a9, 0x0003},
  {0x07a9, 0x0007},
  {0x07a9, 0x000f},
  {0x07a9, 0x001f},
  {0x07a9, 0x005f},
  {0x07a9, 0x00df},
  {0x07a8, 0x0001},
  {0x0510, 0x0109},
  {0x0511, 0x0000},
  {0x0512, 0x0000},
  {0x0501, 0x0001},
  {0x0500, 0x000b},
  {0x0728, 0x0009},
  {0x0729, 0x0000},
  {0x072a, 0x0000},
  {0x072b, 0x0001},
  {0x0510, 0x020a},
  {0x0511, 0x0000},
  {0x0512, 0x0000},
  {0x0501, 0x0002},
  {0x0500, 0x000b},
  {0x0501, 0x0001},
  {0x0500, 0x0003},
  {0x0700, 0x0000},
  {0x0851, 0x0000},
  {0x0501, 0x0002},
  {0x0500, 0x0003},
  {0x072c, 0x000a},
  {0x072d, 0x0000},
  {0x072e, 0x0000},
  {0x072f, 0x0002},
  {0x0700, 0x0100},
  {0x0851, 0x0000},
  {0x0501, 0x0001},
  {0x0500, 0x0003},
  {0x0701, 0x0000},
  {0x0851, 0x0000}};

const uint16_t disable_vlan_reg_data[][2] = {
  {0x000e, 0x48b0},
  {0x002e, 0x48b0},
  {0x006e, 0x48b0},
  {0x07a9, 0x00de},
  {0x07a9, 0x00dc},
  {0x07a9, 0x00d4},
  {0x07a8, 0x0000}};

/* Switch port link status */
static int rtk_vlan_status = -1;
static int rtk_link_status[2];

static int rtk_smi_read(int mAddrs, int *rData)
{
  int rv;
  uint8_t txbuf[sizeof(uint16_t)], rxbuf[sizeof(uint16_t)];

  txbuf[0] = (mAddrs >> 8) & 0xff;
  txbuf[1] = mAddrs & 0xff;

  /* I2C read 16-bit word: transmit 16-bit offset, and read 16bits */
  i2c_lock(I2C_PORT_MASTER, 1);
  rv = i2c_xfer(
      I2C_PORT_MASTER,
      RTK_SWITCH_I2C_ADDR,
      txbuf, sizeof(uint16_t),
      rxbuf, sizeof(uint16_t),
      I2C_XFER_SINGLE);
  i2c_lock(I2C_PORT_MASTER, 0);

  if (rv)
    return rv;

  *rData = ((uint16_t)rxbuf[0] << 8) | rxbuf[1];

  return EC_SUCCESS;
}

static int rtk_smi_write(int mAddrs, int rData)
{
  int rv;
  uint8_t buf[2 * sizeof(uint16_t)];

  buf[0] = (mAddrs >> 8) & 0xff;
  buf[1] = mAddrs & 0xff;
  buf[2] = (rData >> 8) & 0xff;
  buf[3] = rData & 0xff;

  /* I2C write 16-bit word: transmit 16-bit offset, and write 16bits */
  i2c_lock(I2C_PORT_MASTER, 1);
  rv = i2c_xfer(
      I2C_PORT_MASTER,
      RTK_SWITCH_I2C_ADDR,
      buf, 2 * sizeof(uint16_t),
      NULL, 0,
      I2C_XFER_SINGLE);
  i2c_lock(I2C_PORT_MASTER, 0);

  return rv;
}

static int _rtk_switch_probe(void)
{
  int retVal;
  int data;

  /* Write 0x2002 to register 0x130D to enable big-endian standard
   * I2C protocol. However, this first write must be little endian
   */
  if((retVal = rtk_smi_write(0x0D13, 0x0220)) != EC_SUCCESS)
    return retVal;

  if((retVal = rtk_smi_write(0x13C2, 0x0249)) != EC_SUCCESS)
    return retVal;

  if((retVal = rtk_smi_read(0x1300, &data)) != EC_SUCCESS)
    return retVal;

  if((retVal = rtk_smi_write(0x13C2, 0x0000)) != EC_SUCCESS)
    return retVal;

  if(data != 0x6367){
    CPRINTS("_rtk_switch_probe failed (%x)", data);
    return EC_ERROR_UNKNOWN;
  }

  return EC_SUCCESS;
}

/* 0 for Fail; 1 for Success */
int board_enable_vlan(int enable)
{
  size_t i;
  int ret;

  if (rtk_vlan_status == -1)
    return 0;

  if (rtk_vlan_status == 0 && enable){
    /* Initialize VLAN
     * VLAN1 members: P0, P3
     * VLAN2 members: P1, P3
     * P0 PVID: VLAN1
     * P1 PVID: VLAN2
     */
    /* Equivalent code of register writings */
    //#define VLAN1 1
    //#define VLAN2 2
    //rtk_vlan_cfg_t vlan1, vlan2;
    //rtk_switch_init();
    //rtk_vlan_init();
    //memset(&vlan1, 0, sizeof(vlan1));
    //memset(&vlan2, 0, sizeof(vlan2));
    //RTK_PORTMASK_PORT_SET(vlan1.mbr, UTP_PORT0);
    //RTK_PORTMASK_PORT_SET(vlan1.mbr, UTP_PORT3);
    //RTK_PORTMASK_PORT_SET(vlan1.untag, UTP_PORT0);
    //rtk_vlan_set(VLAN1, &vlan1);
    //RTK_PORTMASK_PORT_SET(vlan2.mbr, UTP_PORT1);
    //RTK_PORTMASK_PORT_SET(vlan2.mbr, UTP_PORT3);
    //RTK_PORTMASK_PORT_SET(vlan2.untag, UTP_PORT1);
    //rtk_vlan_set(VLAN2, &vlan2);
    //rtk_vlan_portPvid_set(UTP_PORT0, VLAN1, 0);
    //rtk_vlan_portPvid_set(UTP_PORT1, VLAN2, 0);
    //rtk_vlan_portPvid_set(UTP_PORT3, VLAN1, 0);
    for (i = 0;
        i < sizeof(enable_vlan_reg_data) / (sizeof(uint16_t) * 2);
        i++){
      if((ret = rtk_smi_write(
              enable_vlan_reg_data[i][0],
              enable_vlan_reg_data[i][1])) != EC_SUCCESS){
        CPRINTF("Enable VLAN failed @%x\n", enable_vlan_reg_data[i][0]);
        return 0;
      }
    }
    CPRINTF("RTK VLAN enabled\n");
  }
  else{
    /* Equivalent code of register writings */
    //rtk_vlan_tagMode_set(UTP_PORT0, VLAN_TAG_MODE_REAL_KEEP_FORMAT);
    //rtk_vlan_tagMode_set(UTP_PORT1, VLAN_TAG_MODE_REAL_KEEP_FORMAT);
    //rtk_vlan_tagMode_set(UTP_PORT3, VLAN_TAG_MODE_REAL_KEEP_FORMAT);
    //rtk_vlan_portIgrFilterEnable_set(UTP_PORT0, DISABLED);
    //rtk_vlan_portIgrFilterEnable_set(UTP_PORT1, DISABLED);
    //rtk_vlan_portIgrFilterEnable_set(UTP_PORT3, DISABLED);
    //rtk_vlan_egrFilterEnable_set(DISABLED);
    for (i = 0;
      i < sizeof(disable_vlan_reg_data) / (sizeof(uint16_t) * 2);
      i++){
      if((ret = rtk_smi_write(
              disable_vlan_reg_data[i][0],
              disable_vlan_reg_data[i][1])) != EC_SUCCESS){
        CPRINTF("Disable VLAN failed @%x\n", disable_vlan_reg_data[i][0]);
        return 0;
      }
    }
    CPRINTF("RTK VLAN disabled\n");
  }
  rtk_vlan_status = enable;

  return 1;
}

static void rtk_switch_scan_port(void);
DECLARE_DEFERRED(rtk_switch_scan_port);

static void rtk_switch_scan_port(void)
{
  int ret;
  int port;
  int data;
  int change = 0;

  for (port = 0; port < 2; port++){
    ret = rtk_smi_read(0x1352 + port, &data);
    if (ret != EC_SUCCESS){
      CPRINTF("smi_read failed (%d)\n", ret);
      break;
    }
    if (rtk_link_status[port] != ((data >> 4) & 1)){
      rtk_link_status[port] = (data >> 4) & 1;
      CPRINTF("port %d link %s\n", port, rtk_link_status[port]? "up": "down");
      change = 1;
    }
  }

  if (change){
    pd_log_event(
        PD_EVENT_ACC_ETH_LINK,
        0,
        (rtk_link_status[1] << 1) | rtk_link_status[0],
        NULL);
    if (pd_is_connected(0))
      pd_send_eth_link_change(0);
  }

  hook_call_deferred(&rtk_switch_scan_port_data, 100 * MSEC);
}

static void board_rtk_switch_init(void)
{
  int ret;

  ret = _rtk_switch_probe();
  if (ret != EC_SUCCESS){
    CPRINTF("RTK switch probe failed (%d)\n", ret);
    return;
  }
  rtk_vlan_status = 0;

  board_enable_vlan(1); /* TODO: default enable? */

  CPRINTF("RTK switch inited\n");

  memset(rtk_link_status, sizeof(rtk_link_status), 0);
  rtk_switch_scan_port();
}
DECLARE_DEFERRED(board_rtk_switch_init);

void board_config_pre_init(void)
{
  /* enable SYSCFG clock */
  STM32_RCC_APB2ENR |= 1 << 0;
}

static void board_init(void)
{
  /* Power on Ethernet adapter and switch */
  gpio_set_level(GPIO_ETHERNET_POWER_EN, 1);
  gpio_set_level(GPIO_RTK_SW_VDDH_EN, 1);
  gpio_set_level(GPIO_RTK_SW_VDDL_EN, 1);

  hook_call_deferred(&board_rtk_switch_init_data, 1000 * MSEC);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
