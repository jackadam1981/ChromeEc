.. Copyright 2025 The ChromiumOS Authors
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.

.. _chromium_ec:

===========
Chromium EC
===========
The Chromium EC repository contains the open-source firmware for a variety of
embedded microcontrollers used in Chromebooks and other ChromeOS devices.

The primary objective of this codebase is to provide robust and efficient
firmware for several types of embedded chips:

*   **Embedded Controller (EC)**: The main EC is a microcontroller present in
    every Chromebook. It is responsible for low-level tasks that need to happen
    when the main processor is off or sleeping. These tasks include power
    sequencing (turning components on and off in the right order), battery
    charging, keyboard and touchpad scanning, thermal management, and other
    system-level functions.

*   **Integrated Sensor Hub (ISH)**: On some devices, an ISH is used to manage
    sensors like accelerometers and gyroscopes, offloading this work from the
    main application processor to save power.

*   **Fingerprint Sensor (FPS)**: For devices with fingerprint readers, this
    firmware manages the sensor, processes fingerprint data, and handles
    secure communication.

Modern Chromium EC firmware is built upon the `Zephyr RTOS <https://www.zephyrproject.org/>`__,
a scalable real-time operating system designed for resource-constrained devices.
Legacy devices use an older, custom-built OS.

This documentation is intended for developers working on the Chromium EC
firmware. It provides information on how to get started with the EC codebase,
how to debug and test the firmware, and how to use the various subsystems and
utilities.

.. mermaid::
    :align: center

    flowchart TB
       subgraph GitHub
         Zephyr[(Zephyr)]
       end
       subgraph Chromium
         cZephyr[(third_party/zephyr)]
         Ec[(platform/ec)]
       end
       subgraph "Portage/chroot"
         ebuild(chromeos-zephyr<br/>ebuild)
         zmake
       end
       Zephyr --> cZephyr
       cZephyr --> Portage/chroot
       Ec --> Portage/chroot
       Portage/chroot --> zephyr.bin


.. toctree::
   :maxdepth: 1
   :hidden:

   Getting started <getting_started>
   Testing & Debugging <testing_debugging>
   ish
   modules
   projects