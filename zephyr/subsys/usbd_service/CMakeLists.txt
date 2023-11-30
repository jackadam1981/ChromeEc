# SPDX-License-Identifier: Apache-2.0

zephyr_library_sources(usbd_hid_init.c)
zephyr_library_sources(usbd_hid_test.c)
zephyr_library_sources_ifdef(CONFIG_USBD_HID_KEYBOARD usbd_kb_hid.c)
zephyr_library_sources(usbd_tp_hid.c)
zephyr_library_sources_ifdef(CONFIG_USBD_HID_VIVALDI hid_vivaldi.c)
