/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * USB Power delivery port management For Cypress EZ-PD CCG6DF, CCG6SF
 * CCGXXF FW is designed to adapt standard TCPM driver procedures.
 */
#include "ccgxxf.h"
#include "console.h"
#include "tcpm/tcpci.h"
#include "tcpm/tcpm.h"
#include "timer.h"
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
/*
 * TODO: F/W upgrade module. move it to depthcharge.
 * Caution: Do not upload binary if it's propritery
 */
extern const uint8_t image1[];
extern const uint8_t image2[];
extern const uint8_t image1_metadata[];
extern const uint8_t image2_metadata[];
extern const int image1_rows;
extern const int image2_rows;
static int ccgxxf_get_fwu_status_reg(int port, int delay_ms, int *status)
{
	int rv, i;
	for (i = 0; i < CCGXXF_FWU_RES_RETRY; i++) {
		msleep(delay_ms);
		rv = tcpc_read16(port, CCGXXF_REG_FWU_RESPONSE, status);
		if (rv)
			return rv;
		if (*status == CCGXXF_FWU_RES_CMD_FAILED)
			return EC_ERROR_UNKNOWN;
		if (*status == CCGXXF_FWU_RES_CMD_IN_PROGRESS)
			continue;
		/* Got the status */
		return EC_SUCCESS;
	}
	return EC_ERROR_UNKNOWN;
}
static int ccgxxf_update_fw_region(int port, int cur_image)
{
  int rv, i;
  //int status;
  int region_rows;
  const uint8_t* fw_bin;
  /* select image to be written */
  if (cur_image == 2)
  {
      fw_bin = image1;
      region_rows = image1_rows;  
  }
  else if (cur_image == 1)
  {
      fw_bin = image2;
      region_rows = image2_rows;  
  }
  else
  {
      return EC_ERROR_UNKNOWN;;
  }
  /* prepare data buffer */
  for (i = 0; i < region_rows; i++) {
    rv = tcpc_write_block(port, CCGXXF_REG_FWU_BUFFER,
        &fw_bin[i * CCGXXF_FWU_BUFFER_ROW_SIZE],
        CCGXXF_FWU_BUFFER_ROW_SIZE);
    if (rv)
      return rv;
    CPRINTS("******%d", i);
    cflush();
    msleep(20);
    /* initiate flash write */
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
        CCGXXF_FWU_CMD_WRITE_FLASH_ROW);
    if (rv)
      return rv;
    /* wait for flash write executed before initiate another I2C ransaction */
    msleep(20);
  }
  return EC_SUCCESS;
}
static int ccgxxf_fw_update(int port)
{
    int rv, read_val, fw_build, fwu_status;
    uint8_t fw_ver_major, fw_ver_minor;
    /* Make sure its correct vendor ID */
    rv = tcpc_read16(port, TCPC_REG_VENDOR_ID, &read_val);
    if (rv)
      return rv;
    if (read_val != CCGXXF_VENDOR_ID) {
      CPRINTS("Invalid vendor ID");
      return EC_ERROR_UNKNOWN;
    }
    /* Make sure it has correct product ID */
    rv = tcpc_read16(port, TCPC_REG_PRODUCT_ID, &read_val);
    if (rv)
      return rv;
    if (read_val != CCGXXF_PRODUCT_ID_CCG6DF &&
      read_val != CCGXXF_PRODUCT_ID_CCG6SF) {
      CPRINTS("Invalid product ID");
      return EC_ERROR_UNKNOWN;
    }
    /* Get the F/W version info from the device */
    rv = tcpc_read16(port, CCGXXF_REG_FW_VERSION, &read_val);
    if (rv)
      return rv;
    fw_ver_major = (read_val >> 8) & 0x00FF;
    fw_ver_minor = read_val & 0x00FF;
    rv = tcpc_read16(port, CCGXXF_REG_FW_VERSION_BUILD, &fw_build);
    if (rv)
      return rv;
    cflush();
    CPRINTS("FW Version from the device: %d.%d.%d",
        fw_build, fw_ver_major, fw_ver_minor);
    CPRINTS("FW Version from the binary: %d.%d.%d",
        image1[CCGXXF_BUILD_NUM_OFFSET],
        image1[CCGXXF_BUILD_VER_MAJOR_OFFSET],
        image1[CCGXXF_BUILD_VER_MINOR_OFFSET]);
    /* Nothing to do if the FW version of the binary is same as in device */
    if (fw_ver_major == image1[CCGXXF_BUILD_VER_MAJOR_OFFSET] &&
      fw_ver_minor == image1[CCGXXF_BUILD_VER_MINOR_OFFSET] &&
      fw_build == image1[CCGXXF_BUILD_NUM_OFFSET])
      return EC_SUCCESS;
    /* Enable Firmware Upgrade Mode */
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND, CCGXXF_FWU_CMD_ENABLE);
    if (rv)
      return rv;
    rv = ccgxxf_get_fwu_status_reg(port, 5, &fwu_status);
    if (rv)
      return rv;
    if (fwu_status != CCGXXF_FWU_RES_SUCCESS)
      return EC_ERROR_UNKNOWN;
    /* Get the F/W region to be updated */
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
        CCGXXF_FWU_CMD_GET_FW_MODE);
    if (rv)
      return rv;
    rv = ccgxxf_get_fwu_status_reg(port, 5, &fwu_status);
    if (rv)
      return rv;
    /* Update the F/W region */
    if (fwu_status == CCGXXF_FWU_RES_FW_REGION_1 ||
      fwu_status == CCGXXF_FWU_RES_FW_REGION_2)
      CPRINTS("Updating F/W region %d", (fwu_status >> 8) & 0xFF);
    else
      return EC_ERROR_UNKNOWN;
    rv = ccgxxf_update_fw_region(port, fwu_status >> 8);
    if (rv)
      return rv;
    /* Prepare metadata */
    if ( (fwu_status >> 8) == 2 )
      rv = tcpc_write_block(port, CCGXXF_REG_FWU_BUFFER, image1_metadata, CCGXXF_FWU_BUFFER_ROW_SIZE);
    else
      rv = tcpc_write_block(port, CCGXXF_REG_FWU_BUFFER, image2_metadata, CCGXXF_FWU_BUFFER_ROW_SIZE);
    if (rv)
      return rv;
    msleep(5); 
    /* Validate f/w image */
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND,
        CCGXXF_FWU_CMD_VALIDATE_FW_IMG);
    if (rv)
      return rv;
    //msleep(50);
    CPRINTS("*********here %d", __LINE__);
    /* 50 ms delay to validate image & 5 ms delay to get status */
    rv = ccgxxf_get_fwu_status_reg(port, 55, &fwu_status);
    CPRINTS("****here %d 0x%x", __LINE__, fwu_status);
    if (rv)
      return rv;
    if (fwu_status != CCGXXF_FWU_RES_SUCCESS)
      return EC_ERROR_UNKNOWN;
    CPRINTS("********here %d", __LINE__);
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND, CCGXXF_FWU_CMD_DISABLE);
    if (rv)
      return rv;
    ;
    CPRINTS("*********here %d", __LINE__);
    rv = tcpc_write16(port, CCGXXF_REG_FWU_COMMAND, CCGXXF_FWU_CMD_RESET);
    CPRINTS("*********here %d, %d", __LINE__, rv);
    if (rv)
      return rv;
    CPRINTS("F/W update success");
    cflush();
    return EC_SUCCESS;
}
static int console_command_ccgxxf_fw_update(int argc, char **argv)
{
	char *e;
	int port;
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;
	/* get & vaidate port number */
	port = strtoi(argv[1], &e, 0);
	if (*e || !board_is_usb_pd_port_present(port))
		return EC_ERROR_PARAM1;
	return ccgxxf_fw_update(port);
}
DECLARE_CONSOLE_COMMAND(ccg_fw, console_command_ccgxxf_fw_update,
			"port", "Update ccgxxf firmware");
