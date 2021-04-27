/*
 * Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO expander for CCG6XF. */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "ioexpander.h"
#include "tcpm/ccgxxf.h"
#include "tcpm/tcpci.h"

#define CPRINTF(format, args...) cprintf(CC_GPIO, format, ## args)
#define CPRINTS(format, args...) cprints(CC_GPIO, format, ## args)

#define CCGXXF_GPIO_HIZ_ANALOG   (0)
#define CCGXXF_GPIO_HIZ_DIGITAL  (1)
#define CCGXXF_GPIO_RES_UP       (2)
#define CCGXXF_GPIO_RES_DWN      (3)
#define CCGXXF_GPIO_OD_LOW       (4)
#define CCGXXF_GPIO_OD_HIGH      (5)
#define CCGXXF_GPIO_STRONG       (6)
#define CCGXXF_GPIO_RES_UPDOWN   (7)

static int ccgxxf_ioex_init(int ioex)
{
  int rv, val;

  // Check the Device ID can be read
  rv = i2c_read8(ioex_config[ioex].i2c_host_port, ioex_config[ioex].i2c_addr_flags,
                 TCPC_REG_BCD_DEV, &val);

  if (rv != EC_SUCCESS) 
  {
    CPRINTF("Failed to read CCGXXF DEV ID for IOexpander %d", ioex);
    return rv;
  }

  return EC_SUCCESS;
}

static int ccgxxf_ioex_check_is_valid(int port, int mask)
{
	if (port >= CCGXXF_IOEXP_MAX_PORT_CNT)
			return EC_ERROR_INVAL;

  if (mask & ~CCGXXF_IOEXP_VALID_GPIO_MASK) {
		CPRINTF("GPIO%02d is not support in CCGXXF", __fls(mask));
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static int ccgxxf_ioex_get_level(int ioex, int port, int mask, int *val)
{
  int rv;

	rv = ccgxxf_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;
    
  rv = i2c_read8(ioex_config[ioex].i2c_host_port, ioex_config[ioex].i2c_addr_flags,
                 TCPC_REG_CCGXXF_GPIO_P0_DATA_IN + port, val);
	if (rv != EC_SUCCESS)
		return rv;

	*val = !!(*val & mask);

	return EC_SUCCESS;
}

static int ccgxxf_ioex_set_level(int ioex, int port, int mask, int value)
{
  int rv; 
  int val;
   
	rv = ccgxxf_ioex_check_is_valid(port, mask);
	if (rv != EC_SUCCESS)
		return rv;

	rv = i2c_read8(ioex_config[ioex].i2c_host_port, ioex_config[ioex].i2c_addr_flags,
                 TCPC_REG_CCGXXF_GPIO_P0_DATA_OUT + port, &val);
	if (rv != EC_SUCCESS)
		return rv;

	if (value)
		val |= mask;
	else
		val &= ~mask;

	return i2c_write8(ioex_config[ioex].i2c_host_port, ioex_config[ioex].i2c_addr_flags, 
                    TCPC_REG_CCGXXF_GPIO_P0_DATA_OUT + port, val);
}

static int ccgxxf_get_flags_by_mask(int ioex, int port, int mask, int *flags)
{
  /*
  * CCGXXF doesn't have register to report the flags by mask.
  * If needed, store the flags locally and return them.
  */
  return EC_ERROR_UNIMPLEMENTED;
}

static int ccgxxf_ioex_set_flags(int ioex, int port, int mask, int flags)
{
  int rv; 
  int pinMode;
  int mode;

  rv = ccgxxf_ioex_check_is_valid(port, mask);
  if (rv != EC_SUCCESS)
    return rv;

  if (flags & GPIO_ANALOG)
  {
      pinMode = CCGXXF_GPIO_HIZ_ANALOG;    
      /* For GPIO_ANALOG other flags are ignored */ 
  }
  else
  {
    if (flags & GPIO_OUTPUT)  /* If INPUT and OUTPUT set together OUTPUT has priority */
    {
       if (flags & GPIO_OPEN_DRAIN)
       {
          if (flags & GPIO_PULL_UP)
          {
             pinMode = CCGXXF_GPIO_RES_UP;    
          }
          else
          {
             pinMode = CCGXXF_GPIO_OD_LOW;
          }
       }
       else /* no ODR */
       {
           pinMode = CCGXXF_GPIO_STRONG;
       }
    } 
    else if (flags & GPIO_INPUT)
    {
        if (flags & GPIO_PULL_UP)
        {
           pinMode = CCGXXF_GPIO_RES_UP;
           flags |= GPIO_HIGH;
        }
        else if (flags & GPIO_PULL_DOWN)
        {
           pinMode = CCGXXF_GPIO_RES_DWN;    
           flags |= GPIO_LOW;
        }
        else /* no pull-up or pull-down */
        {
           pinMode = CCGXXF_GPIO_HIZ_DIGITAL;    
        }
        /* for GPIO_INPUT other flags are ignored */ 
    } 
    else 
    {
      /* Either ANALOG, INPUT or OUTPUT shall be set */
   		return EC_ERROR_INVAL;
    }
  }
  
  mode = (mask << CCGXXF_IOEXP_PORT_MASK_OFFSET) | (pinMode << CCGXXF_IOEXP_PIN_MODE_OFFSET) | port;

  rv = i2c_write16(ioex_config[ioex].i2c_host_port, ioex_config[ioex].i2c_addr_flags, 
                   TCPC_REG_CCGXXF_GPIO_CONFIG_REG, mode);
  if (rv != EC_SUCCESS)
    return rv;
 
  if ((flags & GPIO_ANALOG) == 0)
  {
    if (flags & GPIO_HIGH)
    {
      return ccgxxf_ioex_set_level(ioex, port, mask, 1);
    }
    else if (flags & GPIO_LOW)
    {
      return ccgxxf_ioex_set_level(ioex, port, mask, 0);
    }
  }
  
  return rv;
}

const struct ioexpander_drv ccgxxf_ioexpander_drv = {
	.init              = &ccgxxf_ioex_init,
	.get_level         = &ccgxxf_ioex_get_level,
	.set_level         = &ccgxxf_ioex_set_level,
  .get_flags_by_mask = &ccgxxf_get_flags_by_mask,
	.set_flags_by_mask = &ccgxxf_ioex_set_flags,
};
