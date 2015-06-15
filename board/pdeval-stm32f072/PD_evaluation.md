USB PD chip evaluation configuration
====================================

This board configuration implements a USB Power Delivery TCPM
in order to evaluate various TCPC chips.
The code tries to follow the preliminary USB PD interface standard but for TCPC chip implementing proprietary I2C protocol, a new TCPM file can be implementedas explained in the [Updating the code](#Updating) section below.

Building
--------

### ChromiumOS chroot
XxX

### build the TCPM code
make BOARD=pdeval-stm32f072


Updating the code
-----------------

### TCPC Communication code

Please duplicate the [common/usb_pd_tcpm.c](../../common/usb_pd_tcpm.c) into `common/usb_pd_tcpm_<vendor>.c`.
Then update the control logic through I2C there.

### Board configuration

In [board/pdeval-stm32f072/board.h](board.h), you can update CONFIG_USB_PD_PORT_COUNT to the actual number of ports on your board.
You also need to create/delete the corresponding PD_Cx tasks in [board/pdeval-stm32f072/ec.tasklist](ec.tasklist).

Flashing and Running
--------------------

Use openocd or the wrapper through flash_ec
XxX

Testing
-------

TODO EC command line commands
list ...

Known Issues
------------

1. This doc is not written yet ...

2. You might need a ChromeOS chroot ...

Troubleshooting
---------------

1. OpenOCD is not finding the device.

	1. Kernel module may not be loaded.
	2. Udev rules file might not be installed correctly.
	3. PD firmware version may be too old.
	4. Type-C cable from Suzy-Q to the DUT may be upside down.  The SBU lines
	used for case closed debugging are not orientation invariant.

2. You got black smoke

	1. Time to buy a new one.
