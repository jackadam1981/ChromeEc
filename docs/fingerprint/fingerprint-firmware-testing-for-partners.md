# Fingerprint Firmware Testing Instructions for Partners

This document is intended to help partners (sensor vendors, MCU vendors, etc) run the ChromeOS fingerprint team's firmware tests, as part of the AVL process. The document assumes that you‘re using Linux to do the development; preferably a recent version of Ubuntu or Debian.

[TOC]

## Hardware Requirements

You will need a Chromebook with the fingerprint sensor and fingerprint MCU (FPMCU), and a servo debugger.

### Chromebook with fingerprint sensor

The Chromebook needs to be in developer mode so that the test can ssh into it. The fingerprint firmware tests will run a series of bash commands, including flashing the FPMCU firmware and rebooting the Chromebook.

### Servo

Servo is a general purpose debug board used in many automated tests in Chromium OS. Among other things, servo enables the tests to toggle hardware write protect.

While there are multiple versions of servo, for firmware tests we strongly recommend Servo V4 as that's the simplest and most often used in autotests. This document will assume you are using Servo V4.

### Hardware Setup

Connect the "Host" side of Servo V4 to your host machine (which should have a Chromium OS chroot). Connect the other side of Servo V4 to a USB port on the Chromebook with fingerprint sensor. The Chromebook can be powered via the "DUT power" port on Servo V4 or separately. Make sure the you can ssh into the Chromebook from the chroot on the host machine.

## Software Setup

### Get the Chromium OS source code.

*   First, make sure you [have the prerequisites].
*   Then [get the source].
*   Create and [enter the `chroot`].
    *   You can stop after the `enter the chroot` step.

### Build the autotest codebase

```bash
# from a terminal on your machine
(outside chroot) $ cd ~/chromiumos/src

# enter the chroot (the flag is important)
(outside chroot) $ cros_sdk --no-ns-pid

# build autotest
(chroot) $ sudo emerge autotest
```

### Start servod

```bash
(chroot) $ sudo servod --board=<BOARD>
```

## Run a Single Fingerprint Firmware Test

To run a single test, use this command in your chroot:

```bash
test_that --board=<board to be tested> <IP> <test name>
```

For example:

```bash
test_that --board=nocturne <IP> firmware_Fingerprint.ReadFlash
```

## Run the Entire Fingerprint Firmware Test Suite

To run the entire suite, use this command in your chroot:

```bash
test_that --board=<board to be tested> <IP> suite:fingerprint
```

<!-- Links -->

[have the prerequisites]: https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md#Prerequisites
[get the source]: https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md#get-the-source
[enter the `chroot`]: https://chromium.googlesource.com/chromiumos/docs/+/master/developer_guide.md#building-chromium-os

