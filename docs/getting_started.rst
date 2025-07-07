.. Copyright 2025 The ChromiumOS Authors
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.

.. _getting_started:

========================================
Get Started Building EC Images (Quickly)
========================================
The `Chromium OS Developer Guide`_ and `README`_ walk through the steps needed
to fetch and build Chromium OS source. These steps can be followed to retrieve
and build EC source as well. On the other hand, if your sole interest is
building an EC image, the general developer guide contains some extra unneeded
steps.

.. note::

   This page includes some bash commands that need to be run in different
   environments. The following convension is used:

   * ``(host) $`` when the command should be run from the host machine.
   * ``(cr) $`` when the command should be run from the chroot.
   * ``(dut) $`` when the command should be run from the AP console of the DUT.

----------------------
Setup your environment
----------------------
Here is a set of steps to set up a development environment to build EC images
inside the Chromium OS chroot:


1.  Create a folder for your chroot:

    .. code-block:: bash

        (host)$ mkdir chromiumos; cd chromiumos

2.  Initialize the checkout in the current directory:

    .. code-block:: bash

        (host)$ repo init -u https://chromium.googlesource.com/chromiumos/manifest

3.  Update the working tree to the latest version:

    .. code-block:: bash

        (host)$ repo sync -j <number of cores on your workstatsion>

4.  Enter the chroot (type your password for ``sudo`` if prompted):

    .. code-block:: bash

        (host)$ cros_sdk --no-ns-pid

---------------
Build the image
---------------
.. code-block:: bash

   (cr)$ cd ~/chromiumos/src/platform/ec; zmake build ${TARGET}

--------
Flashing
--------
Once you have an image, you can flash it to your board.

Servo
=====
If you have a servo (see :ref:`servod`_), you can flash the image with:

.. code-block:: bash

    (cr)$ cd ~/chromiumos/src/platform/ec; sudo ./util/flash_ec --board=${BOARD}

flashrom
========
If you don't have a servo, you can flash the image with ``flashrom``. First,
copy the image to the device. Then, run the following command on the device:

.. code-block:: bash

    (cr)$ flashrom -p ec -w <path to image>

-------
Testing
-------
Once you have flashed the image, you can test it.

ectool
======
``ectool`` is a command-line utility that can be used to interact with the EC.
It can be used to read and write registers, send commands, and more.

To build ``ectool``, run the following command:

.. code-block:: bash

    (cr)$ cd ~/chromiumos/src/platform/ec; make BOARD=host -j

Then, you can run ``ectool`` on the device:

.. code-block:: bash

    (cr)$ ectool --help

Unit Tests
==========
To run the legacy unit tests, run the following command:

.. code-block:: bash

    cd ~/chromiumos/src/platform/ec; make runtests -j

Twister
=======
Twister is the Zephyr testing framework. To run the Twister tests, run the
following command:

.. code-block:: bash

    cd ~/chromiumos/src/platform/ec; ./twister -p <platform> -s <test suite>

For more information, see the `Twister documentation`_.

.. _Chromium OS Developer Guide: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/
.. _README: https://chromium.googlesource.com/chromiumos/platform/ec/+/refs/heads/main/README.md
.. _Twister documentation: https://docs.zephyrproject.org/latest/develop/test/twister.html
