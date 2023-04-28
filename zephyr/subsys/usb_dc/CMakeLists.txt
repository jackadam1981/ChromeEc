# SPDX-License-Identifier: Apache-2.0

if(CONFIG_USB_DEVICE_STACK)

zephyr_library()
zephyr_library_include_directories(include)
zephyr_library_sources(usb_dc_trans.c)

endif()
