.. Copyright 2025 The ChromiumOS Authors
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.

.. _ish:

===
ISH
===

----------
Bus layout
----------

.. mermaid::
   :align: center

   block-beta
      columns 4

      ISH:1
      space:2
      EC:1

      arrow1<["&nbsp;&nbsp;"]>(up, down):1
      space:2
      arrow2<["&nbsp;&nbsp;"]>(up, down):1
  
      block:bus:4
         i2c["i2c bus"]
      end
  
      arrow3<["&nbsp;&nbsp;"]>(up, down):1
      arrow4<["&nbsp;&nbsp;"]>(up, down):1
      arrow5<["&nbsp;&nbsp;"]>(up, down):1
      space
  
      block:peripherals:4
         Lid["Lid<br/>Sensor"]
         Base["Base<br/>Sensor"]
         Other["Light Sensors<br/>-or-<br/>Proximity Sensors<br/>-or-<br/>Magnetometer"]
         Hall["Hall Effect"]
      end

-----------------
Interrupt routing
-----------------

.. mermaid::
   :align: center

   block-beta
      columns 4

      ISH:1
      space:2
      EC:1

      space:4

      block:peripherals:4
         Lid["Lid<br/>Sensor"]
         Base["Base<br/>Sensor"]
         Other["Light Sensors<br/>-or-<br/>Proximity Sensors<br/>-or-<br/>Magnetometer"]
         Hall["Hall Effect"]
      end
  
      EC-- "EC_ISH_INT" -->ISH
      Hall-- "LID_OPEN<br/>TABLET_MODE" -->EC
      Other-- "INT" -->ISH
      Lid-- "INT" -->ISH
      Base-- "INT" -->ISH

**Are all interrupts needed?** Not really. Interrupts are used to get the right
timestamp to Android, but if we have a convertible device Android doesn't use
both lid and base sensors. The rule is:

* If two accelerometers are used (no IMU): The LID accelerometers interrupt is
  needed.
* If one accelerometer and one IMU are used: The IMU interrupt is used
  regardless of placement (lid vs base).

---------------------------
Implementation Requirements
---------------------------

Types of sensors which connect to the ISH
=========================================
The following sensor types SHALL be connected to the ISH:

* Accelerometers
* IMUs
* Light sensors
* Proximity sensors
* Magnetometers
* Hall effect sensors

EC Interrupt to ISH
===================
.. warning::

   There SHALL be a dedicated GPIO routed between the EC and the ISH to act as
   an EC interrupt to the ISH.

Why?

* When using the ISH sensor stack, the ISH is the controller and the EC is the
  target on the ``SENSOR_I2C`` bus.
* The ISH queries the ChromeOS Board Information (CBI) to determine the expected
  sesor targets attached to the ``SENSOR_I2C`` bus. CBI is stored inside the EC
  Nonvolatile memory. After the request is sent to the EC, the ISH needs to know
  when the response is ready. This signal is carried by the interrupt.
* ISH uses EC as a GPIO expander for ``LID_OPEN`` and ``TABLET_MODE`` signals.

Sensor I2C bus routes to ISH
============================
* The ``SENSOR_I2C`` bus SHALL route to the ISH.
* There SHALL be an I2C bus connection between the EC and the ISH. The bus
  connection can be a dedicated bus, OR, the EC MAY be placed on the same bus as
  the ``SENSOR_I2C`` bus. No peripheral other than the EC and SENSORs should be
  placed on the ``SENSOR_I2C`` bus.

Why?

* ISH collects the sensor data for the OS and thus needs to be the controller of
  the ``SENSOR_I2C`` bus.
* The ISH needs to communicate with the EC and thus needs a bus between itself
  and the EC.

Hall effect GPIOs to EC
=======================
* Interrupts from the hall effect sensors MUST be routed to the EC.
* They may optionally be dual routed to the ISH, but it's not necessary.
